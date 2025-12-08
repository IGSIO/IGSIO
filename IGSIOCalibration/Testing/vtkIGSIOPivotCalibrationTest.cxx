#include "vtkIGSIOPivotCalibrationAlgo.h"
#include "Utils.h"

#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkTransform.h>
#include <vtkVectorOperators.h>

#include <cmath>
#include <iostream>
#include <random>

struct GroundTruth
{
  vtkVector3d TipPosition_World;  // Fixed pivot point in world coordinates
  vtkVector3d TipPosition_Marker; // Fixed pivot point in marker coordinates
  vtkVector3d TipDir_Marker;      // The unit vector direction from the stylus marker to the tip in marker coordinates
  double StylusLength;            // Length from tracker origin to tip
};

static GroundTruth GenerateGroundTruth(double stylusLength, std::mt19937& rng)
{
  std::uniform_real_distribution<double> dist(-500.0, 500.0);

  const vtkVector3d dir = GenerateRandomDirection(rng);

  GroundTruth gt;
  gt.TipPosition_World = vtkVector3d(dist(rng), dist(rng), dist(rng));
  gt.TipPosition_Marker = stylusLength * dir;
  gt.TipDir_Marker = dir;
  gt.StylusLength = stylusLength;
  return gt;
}

// Generates a random MarkerToWorld transform: maps points in marker coordinates to world coordinates.
static vtkNew<vtkMatrix4x4> GenerateRandomMarkerTransform(const GroundTruth& gt, std::mt19937& rng)
{
  vtkNew<vtkMatrix4x4> markerToWorld = GenerateRandomRotationMatrix(rng);

  const vtkVector4d tipDir_Marker(gt.TipDir_Marker[0], gt.TipDir_Marker[1], gt.TipDir_Marker[2], 0);
  double tipDirData_World[4];

  markerToWorld->MultiplyPoint(tipDir_Marker.GetData(), tipDirData_World);

  const vtkVector3d tipDir_World(tipDirData_World[0], tipDirData_World[1], tipDirData_World[2]);
  const vtkVector3d tipToMarker_World = -tipDir_World;
  const vtkVector3d markerPos_World = gt.TipPosition_World + gt.StylusLength * tipToMarker_World;

  markerToWorld->SetElement(0, 3, markerPos_World[0]);
  markerToWorld->SetElement(1, 3, markerPos_World[1]);
  markerToWorld->SetElement(2, 3, markerPos_World[2]);

  return markerToWorld;
}

// A sanity check test to make sure that the generated marker transforms work as expected.
// Specifically: the generated marker transforms should leave the tip position fixed.
static int TestGenerateRandomMarkerTransform(std::mt19937& rng)
{
  LOG_INFO("Running TestGenerateRandomMarkerTransform...");

  const double stylusLength = 150.0;
  const GroundTruth gt = GenerateGroundTruth(stylusLength, rng);
  const vtkNew<vtkMatrix4x4> markerToWorld = GenerateRandomMarkerTransform(gt, rng);

  // The stylus tip in marker coordinates should map to the ground truth tip position in world coordinates
  const vtkVector4d tip_Marker(gt.TipPosition_Marker[0], gt.TipPosition_Marker[1], gt.TipPosition_Marker[2], 1.0);
  vtkVector4d tip_World;
  markerToWorld->MultiplyPoint(tip_Marker.GetData(), tip_World.GetData());

  const double tolerance = 1e-9;
  const double distance = (vtkVector3d(tip_World.GetData()) - gt.TipPosition_World).Norm();

  if (distance > tolerance)
  {
    LOG_ERROR("ERROR: Tip position mismatch! Distance between expected tip and computed tip = " << distance);
    return EXIT_FAILURE;
  }

  LOG_INFO("  PASSED");
  return EXIT_SUCCESS;
}

static int TestPivotCalibration(vtkIGSIOPivotCalibrationAlgo* pivotAlgo, double stylusLength, double stylusTipSigma, double errorTolerance, std::mt19937& rng)
{
  LOG_INFO("Running TestPivotCalibration (stylusLength=" << stylusLength << ", tipSigma=" << stylusTipSigma << ", errorTolerance=" << errorTolerance << ")...");

  // Generate ground truth
  const GroundTruth gt = GenerateGroundTruth(stylusLength, rng);

  // Target number of points
  const int targetNumberOfPoints = pivotAlgo->GetMaximumNumberOfPoseBuckets() * pivotAlgo->GetPoseBucketSize();

  // Feed marker transforms
  pivotAlgo->RemoveAllCalibrationPoints();
  const int maxIterations = 5 * targetNumberOfPoints; // pivotAlgo can reject points, so we keep trying at most maxIterations times
  for (int iteration = 0; iteration < maxIterations && pivotAlgo->GetNumberOfCalibrationPoints() < targetNumberOfPoints; ++iteration)
  {
    const vtkNew<vtkMatrix4x4> markerToWorld = GenerateRandomMarkerTransform(gt, rng);
    const vtkNew<vtkMatrix4x4> markerToWorldPlusNoise = ApplyRandomPerturbation(markerToWorld, 0, stylusTipSigma, rng);

    pivotAlgo->InsertNextCalibrationPoint(markerToWorldPlusNoise);
  }

  // Check if we collected enough points
  const int numPoses = pivotAlgo->GetNumberOfCalibrationPoints();
  if (numPoses < targetNumberOfPoints)
  {
    LOG_ERROR("ERROR: Only collected " << numPoses << " poses out of " << targetNumberOfPoints << " required");
    return EXIT_FAILURE;
  }

  // Perform calibration
  const igsioStatus status = pivotAlgo->DoPivotCalibration(nullptr, true);
  if (status != IGSIO_SUCCESS)
  {
    LOG_ERROR("ERROR: Pivot calibration failed");
    return EXIT_FAILURE;
  }

  // Get the result
  vtkMatrix4x4* tipToMarker = pivotAlgo->GetPivotPointToMarkerTransformMatrix();
  if (!tipToMarker)
  {
    LOG_ERROR("ERROR: Failed to get calibration result matrix");
    return EXIT_FAILURE;
  }

  // Compute the tip location in marker coordinates by transforming the origin
  const double origin[4] = { 0, 0, 0, 1 };
  double computedTip[4];
  tipToMarker->MultiplyPoint(origin, computedTip);

  const double tipError = std::sqrt(vtkMath::Distance2BetweenPoints(computedTip, gt.TipPosition_Marker.GetData()));
  const double rmse = pivotAlgo->GetPivotCalibrationErrorMm();

  LOG_INFO("  RMSE: " << rmse << " mm");
  LOG_INFO("  Tip error: " << tipError << " mm");
  LOG_INFO("  Ground truth tip: (" << gt.TipPosition_Marker[0] << ", " << gt.TipPosition_Marker[1] << ", " << gt.TipPosition_Marker[2] << ")");
  LOG_INFO("  Computed tip: (" << computedTip[0] << ", " << computedTip[1] << ", " << computedTip[2] << ")");

  if (tipError >= errorTolerance)
  {
    LOG_ERROR("ERROR: Tip error " << tipError << " exceeds tolerance " << errorTolerance);
    return EXIT_FAILURE;
  }

  LOG_INFO("  PASSED");
  return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
  std::mt19937 rng(42);
  int numberOfErrors = 0;

  vtkNew<vtkIGSIOPivotCalibrationAlgo> pivotAlgo;
  pivotAlgo->SetValidateInputBufferEnabled(true);
  pivotAlgo->SetPoseBucketSize(10);
  pivotAlgo->SetMaximumNumberOfPoseBuckets(10);

  // Test the transform generation helper
  if (TestGenerateRandomMarkerTransform(rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }

  // Exact (no noise)
  if (TestPivotCalibration(pivotAlgo, 50.0, 0.0, 1e-6, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestPivotCalibration(pivotAlgo, 150.0, 0.0, 1e-6, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestPivotCalibration(pivotAlgo, 300.0, 0.0, 1e-6, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }

  // Small noise
  if (TestPivotCalibration(pivotAlgo, 50.0, 0.1, 0.05, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestPivotCalibration(pivotAlgo, 150.0, 0.1, 0.05, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestPivotCalibration(pivotAlgo, 300.0, 0.1, 0.05, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }

  // Medium noise
  if (TestPivotCalibration(pivotAlgo, 50.0, 0.5, 0.1, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestPivotCalibration(pivotAlgo, 150.0, 0.5, 0.1, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestPivotCalibration(pivotAlgo, 300.0, 0.5, 0.1, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }

  // Higher noise
  if (TestPivotCalibration(pivotAlgo, 50.0, 3.0, 1.0, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestPivotCalibration(pivotAlgo, 150.0, 3.0, 1.0, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestPivotCalibration(pivotAlgo, 300.0, 3.0, 1.0, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }

  LOG_INFO("=== Summary ===");
  if (numberOfErrors > 0)
  {
    LOG_INFO("FAILED: " << numberOfErrors << " test(s) failed");
    return EXIT_FAILURE;
  }

  LOG_INFO("All tests PASSED");
  return EXIT_SUCCESS;
}

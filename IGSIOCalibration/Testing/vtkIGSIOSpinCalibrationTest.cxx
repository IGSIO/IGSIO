#include "vtkIGSIOSpinCalibrationAlgo.h"
#include "Utils.h"

#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkTransform.h>
#include <vtkVectorOperators.h>

#include <cmath>
#include <random>

struct GroundTruth
{
  vtkVector3d TipPosition_World;  // Fixed tip position in world coordinates
  vtkVector3d TipPosition_Marker; // Tip position in marker coordinates (on spin axis)
  vtkVector3d SpinAxis_Marker;    // Unit spin axis in marker coordinates
  double MarkerRadius;            // Perpendicular distance from marker origin to spin axis
};

static GroundTruth GenerateGroundTruth(double stylusLength, double markerRadius, std::mt19937& rng)
{
  std::uniform_real_distribution<double> dist(-500.0, 500.0);

  // Random spin axis in marker coordinates
  const vtkVector3d spinAxis = GenerateRandomDirection(rng);

  // Compute tip position in marker coordinates
  // The marker origin is at perpendicular distance markerRadius from the spin axis
  const double h = std::sqrt(stylusLength * stylusLength - markerRadius * markerRadius);
  const vtkVector3d perpDir = GenerateRandomPerpendicularDirection(spinAxis, rng);
  const vtkVector3d tipPosition_Marker = h * spinAxis + markerRadius * perpDir;

  GroundTruth gt;
  gt.TipPosition_World = vtkVector3d(dist(rng), dist(rng), dist(rng));
  gt.TipPosition_Marker = tipPosition_Marker;
  gt.SpinAxis_Marker = spinAxis;
  gt.MarkerRadius = markerRadius;
  return gt;
}

static vtkVector3d ComputeSpinAxisWorld(vtkMatrix4x4* markerToWorld, const vtkVector3d& spinAxis_Marker)
{
  const vtkVector4d axis_Marker(spinAxis_Marker[0], spinAxis_Marker[1], spinAxis_Marker[2], 0);
  double axis_World[4];
  markerToWorld->MultiplyPoint(axis_Marker.GetData(), axis_World);

  return vtkVector3d(axis_World);
}

static vtkNew<vtkMatrix4x4> GenerateInitialMarkerTransform(const GroundTruth& gt, std::mt19937& rng)
{
  vtkNew<vtkMatrix4x4> markerToWorld = GenerateRandomRotationMatrix(rng);

  // Transform tip from marker to world and compute marker origin position
  const vtkVector4d tip_Marker(gt.TipPosition_Marker[0], gt.TipPosition_Marker[1], gt.TipPosition_Marker[2], 0);
  double tipOffset_World[4];
  markerToWorld->MultiplyPoint(tip_Marker.GetData(), tipOffset_World);

  // markerOrigin_World + tipOffset_World = tipPosition_World
  // markerOrigin_World = tipPosition_World - tipOffset_World
  const vtkVector3d markerOrigin_World = gt.TipPosition_World - vtkVector3d(tipOffset_World);

  markerToWorld->SetElement(0, 3, markerOrigin_World[0]);
  markerToWorld->SetElement(1, 3, markerOrigin_World[1]);
  markerToWorld->SetElement(2, 3, markerOrigin_World[2]);

  return markerToWorld;
}

// A sanity check test to make sure that the generated initial marker transform works as expected.
static int TestGenerateInitialMarkerTransform(std::mt19937& rng)
{
  LOG_INFO("Running TestGenerateInitialMarkerTransform...");

  const double stylusLength = 150.0;
  const double markerRadius = 30.0;

  const GroundTruth gt = GenerateGroundTruth(stylusLength, markerRadius, rng);
  const vtkNew<vtkMatrix4x4> markerToWorld = GenerateInitialMarkerTransform(gt, rng);

  // Verify tip in marker coordinates maps to tip in world coordinates
  const vtkVector4d tip_Marker(gt.TipPosition_Marker[0], gt.TipPosition_Marker[1], gt.TipPosition_Marker[2], 1.0);
  vtkVector4d tip_World;
  markerToWorld->MultiplyPoint(tip_Marker.GetData(), tip_World.GetData());

  const double tolerance = 1e-9;
  const double distance = (vtkVector3d(tip_World.GetData()) - gt.TipPosition_World).Norm();

  if (distance > tolerance)
  {
    LOG_ERROR("ERROR: Tip position mismatch! Distance = " << distance);
    return EXIT_FAILURE;
  }

  LOG_INFO("  PASSED");
  return EXIT_SUCCESS;
}

static int TestSpinCalibration(vtkIGSIOSpinCalibrationAlgo* spinAlgo,
                               double stylusLength,
                               double markerRadius,
                               double rotationNoise,
                               double errorToleranceDegrees,
                               std::mt19937& rng)
{
  LOG_INFO("Running TestSpinCalibration (stylusLength=" << stylusLength << ", markerRadius=" << markerRadius << ", noise=" << rotationNoise << "°)...");

  // Generate ground truth
  const GroundTruth gt = GenerateGroundTruth(stylusLength, markerRadius, rng);

  // Generate initial marker transform
  const vtkNew<vtkMatrix4x4> initialMarkerToWorld = GenerateInitialMarkerTransform(gt, rng);
  const vtkVector3d spinAxis_World = ComputeSpinAxisWorld(initialMarkerToWorld, gt.SpinAxis_Marker);

  // Clear previous calibration points
  spinAlgo->RemoveAllCalibrationPoints();

  const int targetNumberOfPoints = spinAlgo->GetMaximumNumberOfPoseBuckets() * spinAlgo->GetPoseBucketSize();
  const double angleIncrementDegrees = 2.0 * 360.0 / targetNumberOfPoints; // Two revolutions
  const int maxIterations = targetNumberOfPoints * 5;

  // Feed continuous marker transforms (spinning around axis)
  for (int i = 0; i < maxIterations && spinAlgo->GetNumberOfCalibrationPoints() < targetNumberOfPoints; ++i)
  {
    const double angle = i * angleIncrementDegrees;
    const vtkNew<vtkMatrix4x4> markerToWorld = RotateAroundAxisAtPoint(initialMarkerToWorld, spinAxis_World, gt.TipPosition_World, angle);
    const vtkNew<vtkMatrix4x4> markerToWorldPlusNoise = ApplyRandomPerturbation(markerToWorld, rotationNoise, 0, rng);

    spinAlgo->InsertNextCalibrationPoint(markerToWorldPlusNoise);
  }

  // Check if we collected enough points
  const int numPoses = spinAlgo->GetNumberOfCalibrationPoints();
  if (numPoses < targetNumberOfPoints)
  {
    LOG_ERROR("ERROR: Only collected " << numPoses << " poses out of " << targetNumberOfPoints << " required");
    return EXIT_FAILURE;
  }

  // Perform calibration
  const igsioStatus status = spinAlgo->DoSpinCalibration(nullptr, false, true);
  if (status != IGSIO_SUCCESS)
  {
    LOG_ERROR("ERROR: Spin calibration failed");
    return EXIT_FAILURE;
  }

  // Get the result
  vtkMatrix4x4* tipToMarker = spinAlgo->GetPivotPointToMarkerTransformMatrix();

  // Extract Z-axis from tipToMarker by transforming (0, 0, 1, 0) - this is the spin axis in marker coordinates
  const double zAxis[4] = { 0, 0, 1, 0 };
  double computedSpinAxis_MarkerData[4];
  tipToMarker->MultiplyPoint(zAxis, computedSpinAxis_MarkerData);
  const vtkVector3d computedSpinAxis_Marker(computedSpinAxis_MarkerData);

  // Compare with ground truth: compute angle between axes in degrees
  // Axes should be parallel (either same or opposite direction)
  const double dotProduct = computedSpinAxis_Marker.Dot(gt.SpinAxis_Marker);
  const double axisErrorDegrees = vtkMath::DegreesFromRadians(std::acos(std::min(1.0, std::abs(dotProduct))));

  const double rmse = spinAlgo->GetSpinCalibrationErrorMm();

  LOG_INFO("  RMSE: " << rmse << " mm");
  LOG_INFO("  Axis error: " << axisErrorDegrees << " degrees");

  if (axisErrorDegrees >= errorToleranceDegrees)
  {
    LOG_ERROR("ERROR: Axis error " << axisErrorDegrees << " degrees exceeds tolerance " << errorToleranceDegrees << " degrees");
    return EXIT_FAILURE;
  }

  LOG_INFO("  PASSED");
  return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
  std::mt19937 rng(42);
  int numberOfErrors = 0;

  vtkNew<vtkIGSIOSpinCalibrationAlgo> spinAlgo;
  spinAlgo->SetValidateInputBufferEnabled(true);
  spinAlgo->SetPoseBucketSize(10);
  spinAlgo->SetMaximumNumberOfPoseBuckets(10);

  // Test helper functions
  if (TestGenerateInitialMarkerTransform(rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }

  // Exact tests (no noise) - various stylus lengths and marker radii
  // stylusLength, markerRadius, rotationNoiseDegrees, errorToleranceDegrees
  if (TestSpinCalibration(spinAlgo, 50.0, 10.0, 0.0, 1e-6, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestSpinCalibration(spinAlgo, 150.0, 20.0, 0.0, 1e-6, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestSpinCalibration(spinAlgo, 300.0, 30.0, 0.0, 1e-6, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }

  // Small noise tests (0.5° tracking noise) - simulating good tracking conditions
  if (TestSpinCalibration(spinAlgo, 50.0, 10.0, 0.5, 0.5, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestSpinCalibration(spinAlgo, 150.0, 20.0, 0.5, 0.5, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestSpinCalibration(spinAlgo, 300.0, 30.0, 0.5, 0.5, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }

  // Medium noise tests (5.0° tracking noise) - simulating typical tracking conditions
  if (TestSpinCalibration(spinAlgo, 50.0, 10.0, 5.0, 5.0, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestSpinCalibration(spinAlgo, 150.0, 20.0, 5.0, 5.0, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestSpinCalibration(spinAlgo, 300.0, 30.0, 5.0, 5.0, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }

  // Higher noise tests (10.0° tracking noise) - simulating challenging conditions
  if (TestSpinCalibration(spinAlgo, 50.0, 10.0, 10.0, 10.0, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestSpinCalibration(spinAlgo, 150.0, 20.0, 10.0, 10.0, rng) != EXIT_SUCCESS)
  {
    numberOfErrors++;
  }
  if (TestSpinCalibration(spinAlgo, 300.0, 30.0, 10.0, 10.0, rng) != EXIT_SUCCESS)
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

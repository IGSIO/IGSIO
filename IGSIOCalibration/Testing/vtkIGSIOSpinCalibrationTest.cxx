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
  vtkVector3d TipPositionWorld;  // Fixed tip position in world coordinates
  vtkVector3d TipPositionMarker; // Tip position in marker coordinates (on spin axis)
  vtkVector3d SpinAxisMarker;    // Unit spin axis in marker coordinates
  double MarkerRadius;           // Perpendicular distance from marker origin to spin axis
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
  const vtkVector3d tipPositionMarker = h * spinAxis + markerRadius * perpDir;

  GroundTruth gt;
  gt.TipPositionWorld = vtkVector3d(dist(rng), dist(rng), dist(rng));
  gt.TipPositionMarker = tipPositionMarker;
  gt.SpinAxisMarker = spinAxis;
  gt.MarkerRadius = markerRadius;
  return gt;
}

static vtkVector3d ComputeSpinAxisWorld(vtkMatrix4x4* markerToWorld, const vtkVector3d& spinAxisMarker)
{
  const vtkVector4d axisMarker(spinAxisMarker[0], spinAxisMarker[1], spinAxisMarker[2], 0);
  double axisWorld[4];
  markerToWorld->MultiplyPoint(axisMarker.GetData(), axisWorld);

  return vtkVector3d(axisWorld);
}

static vtkNew<vtkMatrix4x4> GenerateInitialMarkerTransform(const GroundTruth& gt, std::mt19937& rng)
{
  vtkNew<vtkMatrix4x4> markerToWorld = GenerateRandomRotationMatrix(rng);

  // Transform tip from marker to world and compute marker origin position
  const vtkVector4d tipMarker(gt.TipPositionMarker[0], gt.TipPositionMarker[1], gt.TipPositionMarker[2], 0);
  double tipWorldOffset[4];
  markerToWorld->MultiplyPoint(tipMarker.GetData(), tipWorldOffset);

  // markerOriginWorld + tipWorldOffset = tipPositionWorld
  // markerOriginWorld = tipPositionWorld - tipWorldOffset
  const vtkVector3d markerOriginWorld = gt.TipPositionWorld - vtkVector3d(tipWorldOffset);

  markerToWorld->SetElement(0, 3, markerOriginWorld[0]);
  markerToWorld->SetElement(1, 3, markerOriginWorld[1]);
  markerToWorld->SetElement(2, 3, markerOriginWorld[2]);

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
  const vtkVector4d tipMarker(gt.TipPositionMarker[0], gt.TipPositionMarker[1], gt.TipPositionMarker[2], 1.0);
  vtkVector4d tipWorld;
  markerToWorld->MultiplyPoint(tipMarker.GetData(), tipWorld.GetData());

  const double tolerance = 1e-9;
  const double distance = (vtkVector3d(tipWorld.GetData()) - gt.TipPositionWorld).Norm();

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
  const vtkVector3d spinAxisWorld = ComputeSpinAxisWorld(initialMarkerToWorld, gt.SpinAxisMarker);

  // Clear previous calibration points
  spinAlgo->RemoveAllCalibrationPoints();

  const int targetNumberOfPoints = spinAlgo->GetMaximumNumberOfPoseBuckets() * spinAlgo->GetPoseBucketSize();
  const double angleIncrementDegrees = 2.0 * 360.0 / targetNumberOfPoints; // Two revolutions
  const int maxIterations = targetNumberOfPoints * 5;

  // Feed continuous marker transforms (spinning around axis)
  for (int i = 0; i < maxIterations && spinAlgo->GetNumberOfCalibrationPoints() < targetNumberOfPoints; ++i)
  {
    const double angle = i * angleIncrementDegrees;
    const vtkNew<vtkMatrix4x4> markerToWorld = RotateAroundAxisAtPoint(initialMarkerToWorld, spinAxisWorld, gt.TipPositionWorld, angle);
    const vtkNew<vtkMatrix4x4> markerToWorldPlusNoise = ApplyRandomPerturbation(markerToWorld, rotationNoise, rng);

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
  double computedSpinAxisMarkerData[4];
  tipToMarker->MultiplyPoint(zAxis, computedSpinAxisMarkerData);
  const vtkVector3d computedSpinAxisMarker(computedSpinAxisMarkerData);

  // Compare with ground truth: compute angle between axes in degrees
  // Axes should be parallel (either same or opposite direction)
  const double dotProduct = computedSpinAxisMarker.Dot(gt.SpinAxisMarker);
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

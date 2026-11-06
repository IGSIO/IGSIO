#ifndef __vtkIGSIOCalibrationTestingUtils
#define __vtkIGSIOCalibrationTestingUtils

#include <vtkVector.h>
#include <vtkVectorOperators.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkMath.h>

#include <random>

static vtkVector3d GenerateRandomDirection(std::mt19937& rng)
{
  std::uniform_real_distribution<double> distTheta(0.0, 2 * vtkMath::Pi());
  std::uniform_real_distribution<double> distZ(-1.0, 1.0);

  const double theta = distTheta(rng);
  const double z = distZ(rng);
  const double r = std::sqrt(1.0 - z * z);

  return vtkVector3d(r * std::cos(theta), r * std::sin(theta), z);
}

static vtkVector3d GenerateRandomPerpendicularDirection(const vtkVector3d& dir, std::mt19937& rng)
{
  std::uniform_real_distribution<double> distAngle(0.0, 360.0);

  const vtkVector3d arbitrary = (std::abs(dir[0]) < 0.9) ? vtkVector3d(1, 0, 0) : vtkVector3d(0, 1, 0);
  const vtkVector3d perp = dir.Cross(arbitrary).Normalized();

  // Rotate by random angle around dir to get a random perpendicular direction
  const double angle = distAngle(rng);

  vtkNew<vtkTransform> transform;
  transform->RotateWXYZ(angle, dir.GetData());

  double perpRotated[3];
  transform->TransformPoint(perp.GetData(), perpRotated);

  return vtkVector3d(perpRotated);
}

static vtkNew<vtkMatrix4x4> GenerateRandomRotationMatrix(std::mt19937& rng)
{
  std::uniform_real_distribution<double> distAngle(0.0, 360.0);

  const vtkVector3d axis = GenerateRandomDirection(rng);
  const double angleDegrees = distAngle(rng);

  vtkNew<vtkTransform> transform;
  transform->RotateWXYZ(angleDegrees, axis.GetData());

  vtkNew<vtkMatrix4x4> rotation;
  transform->GetMatrix(rotation);

  return rotation;
}

// Applies a small random rotation to a matrix to simulate tracking noise
static vtkNew<vtkMatrix4x4> ApplyRandomPerturbation(vtkMatrix4x4* matrix, double maxAngle, double translationSigma, std::mt19937& rng)
{
  std::uniform_real_distribution<double> angleDist(0.0, maxAngle);
  std::normal_distribution<double> translationDist(0.0, translationSigma);

  const vtkVector3d perturbAxis = GenerateRandomDirection(rng);
  const double perturbAngle = angleDist(rng);
  const vtkVector3d perturbTranslation = translationDist(rng) * GenerateRandomDirection(rng);

  vtkNew<vtkTransform> transform;
  vtkNew<vtkMatrix4x4> result;

  transform->SetMatrix(matrix);
  transform->RotateWXYZ(perturbAngle, perturbAxis.GetData());
  transform->Translate(perturbTranslation.GetData());
  transform->GetMatrix(result);

  return result;
}

// Craetes a new matrix that applies the original matrix, then rotates its result around `axis` at `point` by `angleDegrees`.
static vtkNew<vtkMatrix4x4> RotateAroundAxisAtPoint(vtkMatrix4x4* matrix, const vtkVector3d& axis, const vtkVector3d& point, double angleDegrees)
{
  vtkNew<vtkTransform> transform;

  // Apply the original matrix, translate to origin, rotate, then translate back
  // Note that vtkTransform is PreMultiply by default, so the last transform is applied first
  transform->Translate(point[0], point[1], point[2]);
  transform->RotateWXYZ(angleDegrees, axis.GetData());
  transform->Translate(-point[0], -point[1], -point[2]);
  transform->Concatenate(matrix);

  vtkNew<vtkMatrix4x4> result;
  transform->GetMatrix(result);

  return result;
}

#endif

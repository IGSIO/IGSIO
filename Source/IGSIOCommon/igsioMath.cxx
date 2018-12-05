/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

// IGSIO includes
#include "igsioMath.h"

// VTK includes
#include <vtkMath.h>
#include <vtkTransform.h>

// STD includes
#include <algorithm>

#define MINIMUM_NUMBER_OF_CALIBRATION_EQUATIONS 8

//----------------------------------------------------------------------------
igsioMath::igsioMath()
{

}

//----------------------------------------------------------------------------
igsioMath::~igsioMath()
{

}

//----------------------------------------------------------------------------
// Spherical linear interpolation between two rotation quaternions.
// t is a value between 0 and 1 that interpolates between from and to (t=0 means the results is the same as "from").
// Precondition: no aliasing problems to worry about ("result" can be "from" or "to" param).
// Parameters: adjustSign - If true, then slerp will operate by adjusting the sign of the slerp to take shortest path. True is recommended, otherwise the interpolation sometimes give unexpected results.
// References: From Adv Anim and Rendering Tech. Pg 364
void igsioMath::Slerp(double *result, double t, double *from, double *to, bool adjustSign /*= true*/)
{
  const double* p = from; // just an alias to match q

                          // calc cosine theta
  double cosom = from[0] * to[0] + from[1] * to[1] + from[2] * to[2] + from[3] * to[3]; // dot( from, to )

                                                                                        // adjust signs (if necessary)
  double q[4];
  if (adjustSign && (cosom < (double)0.0))
  {
    cosom = -cosom;
    q[0] = -to[0];   // Reverse all signs
    q[1] = -to[1];
    q[2] = -to[2];
    q[3] = -to[3];
  }
  else
  {
    q[0] = to[0];
    q[1] = to[1];
    q[2] = to[2];
    q[3] = to[3];
  }

  // Calculate coefficients
  double sclp, sclq;
  if (((double)1.0 - cosom) > (double)0.0001) // 0.0001 -> some epsillon
  {
    // Standard case (slerp)
    double omega, sinom;
    omega = acos(cosom); // extract theta from dot product's cos theta
    sinom = sin(omega);
    sclp = sin(((double)1.0 - t) * omega) / sinom;
    sclq = sin(t * omega) / sinom;
  }
  else
  {
    // Very close, do linear interp (because it's faster)
    sclp = (double)1.0 - t;
    sclq = t;
  }

  for (int i = 0; i<4; i++)
  {
    result[i] = sclp * p[i] + sclq * q[i];
  }
}

//----------------------------------------------------------------------------
igsioStatus igsioMath::ConstrainRotationToTwoAxes(double downVector_Sensor[3], int notRotatingAxisIndex, vtkMatrix4x4* sensorToSouthWestDownTransform)
{
  if (notRotatingAxisIndex<0 || notRotatingAxisIndex >= 3)
  {
    LOG_ERROR("Invalid notRotatingAxisIndex is specified (valid values are 0, 1, 2). Use default: 1.");
    notRotatingAxisIndex = 1;
  }

  // Sensor axis vector that is assumed to always point to West. This is chosen so that cross(westVector_Sensor, downVector_Sensor) = southVector_Sensor.
  double westVector_Sensor[4] = { 0,0,0,0 };
  double southVector_Sensor[4] = { 0,0,0,0 };

  westVector_Sensor[notRotatingAxisIndex] = 1;

  vtkMath::Cross(westVector_Sensor, downVector_Sensor, southVector_Sensor); // compute South
  vtkMath::Normalize(southVector_Sensor);
  vtkMath::Cross(downVector_Sensor, southVector_Sensor, westVector_Sensor); // compute West
  vtkMath::Normalize(westVector_Sensor);

  // row 0
  sensorToSouthWestDownTransform->SetElement(0, 0, southVector_Sensor[0]);
  sensorToSouthWestDownTransform->SetElement(0, 1, southVector_Sensor[1]);
  sensorToSouthWestDownTransform->SetElement(0, 2, southVector_Sensor[2]);
  // row 1
  sensorToSouthWestDownTransform->SetElement(1, 0, westVector_Sensor[0]);
  sensorToSouthWestDownTransform->SetElement(1, 1, westVector_Sensor[1]);
  sensorToSouthWestDownTransform->SetElement(1, 2, westVector_Sensor[2]);
  // row 2
  sensorToSouthWestDownTransform->SetElement(2, 0, downVector_Sensor[0]);
  sensorToSouthWestDownTransform->SetElement(2, 1, downVector_Sensor[1]);
  sensorToSouthWestDownTransform->SetElement(2, 2, downVector_Sensor[2]);

  if (notRotatingAxisIndex<0 || notRotatingAxisIndex >= 3)
  {
    return IGSIO_FAIL;
  }
  else
  {
    return IGSIO_SUCCESS;
  }

}

//----------------------------------------------------------------------------
std::string igsioMath::GetTransformParametersString(vtkTransform* transform)
{
  double rotation[3];
  double translation[3];
  double scale[3];
  transform->GetOrientation(rotation);
  transform->GetPosition(translation);
  transform->GetScale(scale);

  std::ostringstream result;
  result << std::setprecision(4) << "Rotation: (" << rotation[0] << ", " << rotation[1] << ", " << rotation[2]
    << ")  Translation: (" << translation[0] << ", " << translation[1] << ", " << translation[2]
    << ")  Scale: (" << scale[0] << ", " << scale[1] << ", " << scale[2] << ")";

  return result.str();
}

//----------------------------------------------------------------------------
std::string igsioMath::GetTransformParametersString(vtkMatrix4x4* matrix)
{
  vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
  transform->SetMatrix(matrix);

  return igsioMath::GetTransformParametersString(transform);
}

//----------------------------------------------------------------------------
void igsioMath::PrintVtkMatrix(vtkMatrix4x4* matrix, std::ostringstream &stream, int precision/* = 3*/)
{
  LOG_TRACE("igsioMath::PrintVtkMatrix");

  for (int i = 0; i < 4; i++)
  {
    if (i>0)
    {
      stream << std::endl;
    }
    for (int j = 0; j < 4; j++)
    {
      stream << std::fixed << std::setprecision(precision) << std::setw(precision + 3) << std::right << matrix->GetElement(i, j) << " ";
    }
  }
}

//----------------------------------------------------------------------------
void igsioMath::LogVtkMatrix(vtkMatrix4x4* matrix, int precision/* = 3*/)
{
  LOG_TRACE("igsioMath::LogVtkMatrix");

  std::ostringstream matrixStream;
  igsioMath::PrintVtkMatrix(matrix, matrixStream, precision);
  LOG_INFO(matrixStream.str());
}

//----------------------------------------------------------------------------
// Description
// Calculate distance between a line (defined by two points) and a point
double igsioMath::ComputeDistanceLinePoint(const double x[3], // linepoint 1
  const double y[3], // linepoint 2
  const double z[3] // target point
)
{
  double u[3];
  double v[3];
  double w[3];

  u[0] = y[0] - x[0];
  u[1] = y[1] - x[1];
  u[2] = y[2] - x[2];

  vtkMath::Normalize(u);

  v[0] = z[0] - x[0];
  v[1] = z[1] - x[1];
  v[2] = z[2] - x[2];

  double dot = vtkMath::Dot(u, v);

  w[0] = v[0] - dot*u[0];
  w[1] = v[1] - dot*u[1];
  w[2] = v[2] - dot*u[2];

  return sqrt(vtkMath::Dot(w, w));
}

//----------------------------------------------------------------------------
igsioStatus igsioMath::ComputeMeanAndStdev(const std::vector<double> &values, double &mean, double &stdev)
{
  if (values.empty())
  {
    LOG_ERROR("igsioMath::ComputeMeanAndStdev failed, the input vector is empty");
    return IGSIO_FAIL;
  }
  double sum = 0;
  for (std::vector<double>::const_iterator it = values.begin(); it != values.end(); ++it)
  {
    sum += *it;
  }
  mean = sum / double(values.size());
  double variance = 0;
  for (std::vector<double>::const_iterator it = values.begin(); it != values.end(); ++it)
  {
    variance += (*it - mean)*(*it - mean);
  }
  stdev = sqrt(variance / double(values.size()));
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioMath::ComputeRms(const std::vector<double> &values, double &rms)
{
  if (values.empty())
  {
    LOG_ERROR("igsioMath::ComputeRms failed, the input vector is empty");
    return IGSIO_FAIL;
  }
  double sumSquares = 0;
  for (std::vector<double>::const_iterator it = values.begin(); it != values.end(); ++it)
  {
    sumSquares += (*it) * (*it);
  }
  double meanSquares = sumSquares / double(values.size());
  rms = sqrt(meanSquares);
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioMath::ComputePercentile(const std::vector<double> &values, double percentileToKeep, double &valueMax, double &valueMean, double &valueStdev)
{
  std::vector<double> sortedValues = values;
  std::sort(sortedValues.begin(), sortedValues.end());

  int numberOfKeptValues = igsioMath::Round((double)sortedValues.size() * percentileToKeep);
  sortedValues.erase(sortedValues.begin() + numberOfKeptValues, sortedValues.end());

  valueMax = sortedValues.back();
  ComputeMeanAndStdev(sortedValues, valueMean, valueStdev);

  return IGSIO_SUCCESS;
}

#if _MSC_VER == 1600 // VS 2010
namespace std
{
  double round(double arg)
  {
    return igsioMath::Round(arg);
  }
}
#endif

//----------------------------------------------------------------------------
double igsioMath::GetPositionDifference(vtkMatrix4x4* aMatrix, vtkMatrix4x4* bMatrix)
{
  LOG_TRACE("igsioMath::GetPositionDifference");
  vtkSmartPointer<vtkTransform> aTransform = vtkSmartPointer<vtkTransform>::New();
  aTransform->SetMatrix(aMatrix);

  vtkSmartPointer<vtkTransform> bTransform = vtkSmartPointer<vtkTransform>::New();
  bTransform->SetMatrix(bMatrix);

  double ax = aTransform->GetPosition()[0];
  double ay = aTransform->GetPosition()[1];
  double az = aTransform->GetPosition()[2];

  double bx = bTransform->GetPosition()[0];
  double by = bTransform->GetPosition()[1];
  double bz = bTransform->GetPosition()[2];

  // Euclidean distance
  double distance = sqrt( pow(ax-bx,2) + pow(ay-by,2) + pow(az-bz,2) );

  return distance;
}

//----------------------------------------------------------------------------
double igsioMath::GetOrientationDifference(vtkMatrix4x4* aMatrix, vtkMatrix4x4* bMatrix)
{
  vtkSmartPointer<vtkMatrix4x4> diffMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  vtkSmartPointer<vtkMatrix4x4> invBmatrix = vtkSmartPointer<vtkMatrix4x4>::New();

  vtkMatrix4x4::Invert(bMatrix, invBmatrix);

  vtkMatrix4x4::Multiply4x4(aMatrix, invBmatrix, diffMatrix);

  vtkSmartPointer<vtkTransform> diffTransform = vtkSmartPointer<vtkTransform>::New();
  diffTransform->SetMatrix(diffMatrix);

  double angleDiff_rad= vtkMath::RadiansFromDegrees(diffTransform->GetOrientationWXYZ()[0]);

  double normalizedAngleDiff_rad = atan2( sin(angleDiff_rad), cos(angleDiff_rad) ); // normalize angle to domain -pi, pi

  return vtkMath::DegreesFromRadians(normalizedAngleDiff_rad);
}

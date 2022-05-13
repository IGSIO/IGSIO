/*=IGSIO=header=begin======================================================
  Program: IGSIO
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================IGSIO=header=end*/

// IGSIO includes
#include "igsioConfigure.h"
#include "igsioMath.h"
#include "vtkIGSIOPivotCalibrationAlgo.h"
#include "vtkIGSIOTransformRepository.h"
#include <vtkIGSIOAccurateTimer.h>

// VTK includes
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkTransform.h"
#include "vtkXMLUtilities.h"
#include "vtksys/SystemTools.hxx"

// ITK includes
#include <vnl/algo/vnl_determinant.h>
#include <vnl/algo/vnl_svd.h>
#include <vnl/algo/vnl_symmetric_eigensystem.h>
#include <vnl/vnl_vector.h>

vtkStandardNewMacro(vtkIGSIOPivotCalibrationAlgo);

//-----------------------------------------------------------------------------
vtkIGSIOPivotCalibrationAlgo::vtkIGSIOPivotCalibrationAlgo()
{
  this->PivotPointToMarkerTransformMatrix = NULL;

  // Unspecified error value is -1.0.
  this->PivotCalibrationErrorMm = -1.0;

  this->PivotPointPosition_Reference[0] = 0.0;
  this->PivotPointPosition_Reference[1] = 0.0;
  this->PivotPointPosition_Reference[2] = 0.0;
  this->PivotPointPosition_Reference[3] = 1.0;
}

//-----------------------------------------------------------------------------
vtkIGSIOPivotCalibrationAlgo::~vtkIGSIOPivotCalibrationAlgo()
{
  this->SetPivotPointToMarkerTransformMatrix(NULL);
}

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOPivotCalibrationAlgo::ReadConfiguration(vtkXMLDataElement* aConfig)
{
  XML_FIND_NESTED_ELEMENT_REQUIRED(pivotCalibrationElement, aConfig, "vtkIGSIOPivotCalibrationAlgo");
  XML_READ_CSTRING_ATTRIBUTE_REQUIRED(ObjectMarkerCoordinateFrame, pivotCalibrationElement);
  XML_READ_CSTRING_ATTRIBUTE_REQUIRED(ReferenceCoordinateFrame, pivotCalibrationElement);
  XML_READ_CSTRING_ATTRIBUTE_REQUIRED(ObjectPivotPointCoordinateFrame, pivotCalibrationElement);
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
/*
In homogeneous coordinates:
 PivotPoint_Reference = MarkerToReferenceTransformMatrix * PivotPoint_Marker

MarkerToReferenceTransformMatrix decomosed to rotation matrix and translation vector:
 PivotPoint_Reference = MarkerToReferenceTransformRotationMatrix * PivotPoint_Marker + MarkerToReferenceTransformTranslationVector
rearranged:
 MarkerToReferenceTransformRotationMatrix * PivotPoint_Marker - PivotPoint_Reference = -MarkerToReferenceTransformTranslationVector
in a matrix form:
 [ MarkerToReferenceTransformRotationMatrix | -Identity3x3 ] * [ PivotPoint_Marker    ] = [ -MarkerToReferenceTransformTranslationVector ]
                                                               [ PivotPoint_Reference ]

It's an Ax=b linear problem that can be solved with robust LSQR:
 Ai = [ MarkerToReferenceTransformRotationMatrix | -Identity3x3 ]
 xi = [ PivotPoint_Marker    ]
      [ PivotPoint_Reference ]
 bi = [ -MarkerToReferenceTransformTranslationVector ]
*/
igsioStatus vtkIGSIOPivotCalibrationAlgo::GetPivotPointPosition(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, std::set<unsigned int>& outlierIndices, double* pivotPoint_Marker, double* pivotPoint_Reference)
{
  std::vector<vnl_vector<double> > aMatrix;
  std::vector<double> bVector;
  vnl_vector<double> xVector(6, 0);   // result vector

  vnl_vector<double> aMatrixRow(6);
  for (std::vector<vtkMatrix4x4*>::const_iterator markerToReferenceTransformIt = markerToTransformMatrixArray.begin();
    markerToReferenceTransformIt != markerToTransformMatrixArray.end(); ++markerToReferenceTransformIt)
  {
    for (int i = 0; i < 3; i++)
    {
      aMatrixRow(0) = (*markerToReferenceTransformIt)->Element[i][0];
      aMatrixRow(1) = (*markerToReferenceTransformIt)->Element[i][1];
      aMatrixRow(2) = (*markerToReferenceTransformIt)->Element[i][2];
      aMatrixRow(3) = (i == 0 ? -1 : 0);
      aMatrixRow(4) = (i == 1 ? -1 : 0);
      aMatrixRow(5) = (i == 2 ? -1 : 0);
      aMatrix.push_back(aMatrixRow);
    }
    bVector.push_back(-(*markerToReferenceTransformIt)->Element[0][3]);
    bVector.push_back(-(*markerToReferenceTransformIt)->Element[1][3]);
    bVector.push_back(-(*markerToReferenceTransformIt)->Element[2][3]);
  }

  double mean = 0;
  double stdev = 0;
  vnl_vector<unsigned int> notOutliersIndices;
  notOutliersIndices.clear();
  notOutliersIndices.set_size(bVector.size());
  for (unsigned int i = 0; i < bVector.size(); ++i)
  {
    notOutliersIndices.put(i, i);
  }
  if (igsioMath::LSQRMinimize(aMatrix, bVector, xVector, &mean, &stdev, &notOutliersIndices) != IGSIO_SUCCESS)
  {
    LOG_ERROR("vtkIGSIOPivotCalibrationAlgo failed: LSQRMinimize error");
    return IGSIO_FAIL;
  }

  // Note: Outliers are detected and rejected for each row (each coordinate axis). Although most frequently
  // an outlier sample's every component is an outlier, it may be possible that only certain components of an
  // outlier sample are removed, which may be desirable for some cases (e.g., when the point is an outlier because
  // temporarily it was flipped around one axis) and not desirable for others (when the point is completely randomly
  // corrupted), but there would be no measurable difference anyway if the only a few percent of the points are
  // outliers.

  outlierIndices.clear();
  unsigned int processFromRowIndex = 0;
  for (unsigned int i = 0; i < notOutliersIndices.size(); i++)
  {
    unsigned int nextNotOutlierRowIndex = notOutliersIndices[i];
    if (nextNotOutlierRowIndex > processFromRowIndex)
    {
      // samples were missed, so they are outliers
      for (unsigned int outlierRowIndex = processFromRowIndex; outlierRowIndex < nextNotOutlierRowIndex; outlierRowIndex++)
      {
        int sampleIndex = outlierRowIndex / 3; // 3 rows are generated per sample
        outlierIndices.insert(sampleIndex);
      }
    }
    processFromRowIndex = nextNotOutlierRowIndex + 1;
  }

  pivotPoint_Marker[0] = xVector[0];
  pivotPoint_Marker[1] = xVector[1];
  pivotPoint_Marker[2] = xVector[2];

  pivotPoint_Reference[0] = xVector[3];
  pivotPoint_Reference[1] = xVector[4];
  pivotPoint_Reference[2] = xVector[5];

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOPivotCalibrationAlgo::DoCalibrationInternal(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, double& error)
{
  double pivotPoint_Marker[4] = { 0.0, 0.0, 0.0, 0.0 };
  double pivotPoint_Reference[4] = { 0.0, 0.0, 0.0, 0.0 };
  vtkNew<vtkMatrix4x4> pivotPointToTool;
  std::set<unsigned int> outlierIndices;
  if (this->DoPivotCalibrationInternal(markerToTransformMatrixArray, true, outlierIndices, pivotPoint_Marker, pivotPoint_Reference, pivotPointToTool) != IGSIO_SUCCESS)
  {
    return IGSIO_FAIL;
  }

  error = this->ComputePivotCalibrationError(markerToTransformMatrixArray, outlierIndices, pivotPoint_Reference, pivotPointToTool);
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOPivotCalibrationAlgo::DoPivotCalibration(vtkIGSIOTransformRepository* aTransformRepository/* = NULL*/, bool autoOrient)
{
  if (this->MarkerToReferenceTransformMatrixBuckets.empty())
  {
    LOG_ERROR("No points are available for pivot calibration");
    this->SetErrorCode(CALIBRATION_NOT_ENOUGH_POINTS);
    return IGSIO_FAIL;
  }

  if (this->GetMaximumToolOrientationDifferenceDegrees() < this->MinimumOrientationDifferenceDegrees)
  {
    LOG_ERROR("Not enough variation in input poses");
    this->SetErrorCode(CALIBRATION_NOT_ENOUGH_VARIATION);
    return IGSIO_FAIL;
  }

  if (!this->PivotPointToMarkerTransformMatrix)
  {
    vtkNew<vtkMatrix4x4> pivotPointToMarkerTransformMatrix;
    this->SetPivotPointToMarkerTransformMatrix(pivotPointToMarkerTransformMatrix);
  }

  double pivotPoint_Marker[4] = { 0.0, 0.0, 0.0, 1.0 };
  double pivotPoint_Reference[4] = { 0.0, 0.0, 0.0, 1.0 };
  std::vector<vtkMatrix4x4*> markerToTransformMatrixArray = this->GetAllMarkerToReferenceMatrices();
  // DoPivotCalibrationInternal will set the error code if something went wrong
  igsioStatus status = this->DoPivotCalibrationInternal(markerToTransformMatrixArray, autoOrient, this->OutlierIndices, pivotPoint_Marker, pivotPoint_Reference, this->PivotPointToMarkerTransformMatrix);
  this->PivotPointPosition_Reference[0] = pivotPoint_Reference[0];
  this->PivotPointPosition_Reference[1] = pivotPoint_Reference[1];
  this->PivotPointPosition_Reference[2] = pivotPoint_Reference[2];

  this->ComputePivotCalibrationError();

  if (this->MaximumCalibrationErrorMm >= 0.0 && this->PivotCalibrationErrorMm > this->MaximumCalibrationErrorMm)
  {
    this->SetErrorCode(CALIBRATION_HIGH_ERROR);
    status = IGSIO_FAIL;
  }

  // Save result
  if (aTransformRepository)
  {
    igsioTransformName pivotPointToMarkerTransformName(this->ObjectPivotPointCoordinateFrame, this->ObjectMarkerCoordinateFrame);
    aTransformRepository->SetTransform(pivotPointToMarkerTransformName, this->PivotPointToMarkerTransformMatrix);
    aTransformRepository->SetTransformPersistent(pivotPointToMarkerTransformName, true);
    aTransformRepository->SetTransformDate(pivotPointToMarkerTransformName, vtkIGSIOAccurateTimer::GetInstance()->GetDateAndTimeString().c_str());
    aTransformRepository->SetTransformError(pivotPointToMarkerTransformName, this->PivotCalibrationErrorMm);
  }
  else
  {
    LOG_DEBUG("Transform repository object is NULL, cannot save results into it");
  }

  return status;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOPivotCalibrationAlgo::DoPivotCalibrationInternal(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, bool autoOrient, std::set<unsigned int>& outlierIndices, double pivotPoint_Marker[4], double pivotPoint_Reference[4], vtkMatrix4x4* pivotPointToMarkerTransformMatrix)
{
  if (markerToTransformMatrixArray.empty())
  {
    LOG_ERROR("No points are available for pivot calibration");
    this->SetErrorCode(CALIBRATION_NOT_ENOUGH_POINTS);
    return IGSIO_FAIL;
  }

  pivotPoint_Marker[0] = 0.0;
  pivotPoint_Marker[1] = 0.0;
  pivotPoint_Marker[2] = 0.0;
  pivotPoint_Marker[3] = 1.0;
  pivotPoint_Reference[0] = 0.0;
  pivotPoint_Reference[1] = 0.0;
  pivotPoint_Reference[2] = 0.0;
  pivotPoint_Reference[3] = 1.0;
  if (this->GetPivotPointPosition(markerToTransformMatrixArray, outlierIndices, pivotPoint_Marker, pivotPoint_Reference) != IGSIO_SUCCESS)
  {
    LOG_ERROR("DoPivotCalibrationInternal: Could not determine pivot point position");
    this->SetErrorCode(CALIBRATION_FAIL);
    return IGSIO_FAIL;
  }

  if (!pivotPointToMarkerTransformMatrix)
  {
    // If the pivot point matrix isn't provided, then we don't need to generate the full matrix.
    this->SetErrorCode(CALIBRATION_NO_ERROR);
    return IGSIO_SUCCESS;
  }

  // Get the result (tooltip to tool transform)
  double x = pivotPoint_Marker[0];
  double y = pivotPoint_Marker[1];
  double z = pivotPoint_Marker[2];

  pivotPointToMarkerTransformMatrix->SetElement(0, 3, x);
  pivotPointToMarkerTransformMatrix->SetElement(1, 3, y);
  pivotPointToMarkerTransformMatrix->SetElement(2, 3, z);

  // Compute tool orientation
  // Z axis: from the pivot point to the marker is on the -Z axis of the tool
  double pivotPointToMarkerTransformZ[3] = { x, y, z };
  vtkMath::Normalize(pivotPointToMarkerTransformZ);
  pivotPointToMarkerTransformMatrix->SetElement(0, 2, pivotPointToMarkerTransformZ[0]);
  pivotPointToMarkerTransformMatrix->SetElement(1, 2, pivotPointToMarkerTransformZ[1]);
  pivotPointToMarkerTransformMatrix->SetElement(2, 2, pivotPointToMarkerTransformZ[2]);

  // Y axis: orthogonal to tool's Z axis and the marker's X axis
  double pivotPointToMarkerTransformY[3] = { 0, 0, 0 };
  // Use the unitX vector as pivotPointToMarkerTransformX vector, unless unitX is parallel to pivotPointToMarkerTransformZ.
  // If unitX is parallel to pivotPointToMarkerTransformZ then use the unitY vector as pivotPointToMarkerTransformX.

  double unitX[3] = { 1, 0, 0 };
  double angle = acos(vtkMath::Dot(pivotPointToMarkerTransformZ, unitX));
  // Normalize between -pi/2 .. +pi/2
  if (angle > vtkMath::Pi() / 2)
  {
    angle -= vtkMath::Pi();
  }
  else if (angle < -vtkMath::Pi() / 2)
  {
    angle += vtkMath::Pi();
  }
  if (fabs(angle) * 180.0 / vtkMath::Pi() > 20.0)
  {
    // unitX is not parallel to pivotPointToMarkerTransformZ
    vtkMath::Cross(pivotPointToMarkerTransformZ, unitX, pivotPointToMarkerTransformY);
    LOG_DEBUG("Use unitX");
  }
  else
  {
    // unitX is parallel to pivotPointToMarkerTransformZ
    // use the unitY instead
    double unitY[3] = { 0, 1, 0 };
    vtkMath::Cross(pivotPointToMarkerTransformZ, unitY, pivotPointToMarkerTransformY);
    LOG_DEBUG("Use unitY");
  }
  vtkMath::Normalize(pivotPointToMarkerTransformY);
  pivotPointToMarkerTransformMatrix->SetElement(0, 1, pivotPointToMarkerTransformY[0]);
  pivotPointToMarkerTransformMatrix->SetElement(1, 1, pivotPointToMarkerTransformY[1]);
  pivotPointToMarkerTransformMatrix->SetElement(2, 1, pivotPointToMarkerTransformY[2]);

  // X axis: orthogonal to tool's Y axis and Z axis
  double pivotPointToMarkerTransformX[3] = { 0, 0, 0 };
  vtkMath::Cross(pivotPointToMarkerTransformY, pivotPointToMarkerTransformZ, pivotPointToMarkerTransformX);
  vtkMath::Normalize(pivotPointToMarkerTransformX);
  pivotPointToMarkerTransformMatrix->SetElement(0, 0, pivotPointToMarkerTransformX[0]);
  pivotPointToMarkerTransformMatrix->SetElement(1, 0, pivotPointToMarkerTransformX[1]);
  pivotPointToMarkerTransformMatrix->SetElement(2, 0, pivotPointToMarkerTransformX[2]);

  if (autoOrient)
  {
    this->UpdateShaftDirection(pivotPointToMarkerTransformMatrix); // Flip it if necessary
  }

  this->SetErrorCode(CALIBRATION_NO_ERROR);
  return IGSIO_SUCCESS;
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOPivotCalibrationAlgo::GetPivotPointToMarkerTranslationString(double aPrecision/*=3*/)
{
  if (this->PivotPointToMarkerTransformMatrix == NULL)
  {
    LOG_ERROR("Tooltip to tool transform is not initialized!");
    return "";
  }

  std::ostringstream s;
  s << std::fixed << std::setprecision(aPrecision)
    << this->PivotPointToMarkerTransformMatrix->GetElement(0, 3)
    << " x " << this->PivotPointToMarkerTransformMatrix->GetElement(1, 3)
    << " x " << this->PivotPointToMarkerTransformMatrix->GetElement(2, 3)
    << std::ends;

  return s.str();
}


//-----------------------------------------------------------------------------
void vtkIGSIOPivotCalibrationAlgo::ComputePivotCalibrationError()
{
  const std::vector<vtkMatrix4x4*> markerToTransformMatrixArray = this->GetAllMarkerToReferenceMatrices();
  this->PivotCalibrationErrorMm = this->ComputePivotCalibrationError(markerToTransformMatrixArray, this->OutlierIndices, this->PivotPointPosition_Reference, this->PivotPointToMarkerTransformMatrix);
}

//-----------------------------------------------------------------------------
double vtkIGSIOPivotCalibrationAlgo::ComputePivotCalibrationError(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, std::set<unsigned int>& outlierIndices, double* pivotPoint_Reference, vtkMatrix4x4* pivotPointToMarkerTransformMatrix)
{
  vtkSmartPointer<vtkMatrix4x4> pivotPointToReferenceMatrix = vtkSmartPointer<vtkMatrix4x4>::New();

  // Compute the error for each sample as distance between the mean pivot point position and the pivot point position computed from each sample
  std::vector<double> errorValues;
  double currentPivotPoint_Reference[4] = { 0, 0, 0, 1 };
  unsigned int sampleIndex = 0;
  for (std::vector< vtkMatrix4x4* >::const_iterator markerToReferenceTransformIt = markerToTransformMatrixArray.begin();
    markerToReferenceTransformIt != markerToTransformMatrixArray.end(); ++markerToReferenceTransformIt, ++sampleIndex)
  {
    if (outlierIndices.find(sampleIndex) != outlierIndices.end())
    {
      // outlier, so skip from the error computation
      continue;
    }

    vtkMatrix4x4::Multiply4x4((*markerToReferenceTransformIt), pivotPointToMarkerTransformMatrix, pivotPointToReferenceMatrix);
    for (int i = 0; i < 3; i++)
    {
      currentPivotPoint_Reference[i] = pivotPointToReferenceMatrix->Element[i][3];
    }
    double errorValue = sqrt(vtkMath::Distance2BetweenPoints(currentPivotPoint_Reference, pivotPoint_Reference));
    errorValues.push_back(errorValue);
  }

  double mean = 0;
  double stdev = 0;
  igsioMath::ComputeMeanAndStdev(errorValues, mean, stdev);
  return mean;
}

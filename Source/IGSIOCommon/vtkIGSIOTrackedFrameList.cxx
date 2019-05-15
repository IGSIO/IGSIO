/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

// IGSIO includes
#include "igsioMath.h"
#include "igsioTrackedFrame.h"
#include "vtkIGSIOTrackedFrameList.h"
#include "vtkIGSIOTransformRepository.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkXMLUtilities.h>
#include <vtksys/SystemTools.hxx>

// STD includes
#include <algorithm>
#include <math.h>

//----------------------------------------------------------------------------
// ************************* vtkIGSIOTrackedFrameList *****************************
//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkIGSIOTrackedFrameList);

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::vtkIGSIOTrackedFrameList()
{
  this->SetNumberOfUniqueFrames(5);

  this->MinRequiredTranslationDifferenceMm = 0.0;
  this->MinRequiredAngleDifferenceDeg = 0.0;
  this->MaxAllowedTranslationSpeedMmPerSec = 0.0;
  this->MaxAllowedRotationSpeedDegPerSec = 0.0;
  this->ValidationRequirements = 0;

}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::~vtkIGSIOTrackedFrameList()
{
  this->Clear();
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::RemoveTrackedFrame(int frameNumber)
{
  if (frameNumber < 0 || (unsigned int)frameNumber >= this->GetNumberOfTrackedFrames())
  {
    LOG_WARNING("Failed to remove tracked frame from list - invalid frame number: " << frameNumber);
    return IGSIO_FAIL;
  }

  delete this->TrackedFrameList[frameNumber];
  this->TrackedFrameList.erase(this->TrackedFrameList.begin() + frameNumber);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::RemoveTrackedFrameRange(unsigned int frameNumberFrom, unsigned int frameNumberTo)
{
  if (frameNumberTo >= this->GetNumberOfTrackedFrames() || frameNumberFrom > frameNumberTo)
  {
    LOG_WARNING("Failed to remove tracked frame from list - invalid frame number range: (" << frameNumberFrom << ", " << frameNumberTo << ")");
    return IGSIO_FAIL;
  }

  for (unsigned int i = frameNumberFrom; i <= frameNumberTo; ++i)
  {
    delete this->TrackedFrameList[i];
  }

  this->TrackedFrameList.erase(this->TrackedFrameList.begin() + frameNumberFrom, this->TrackedFrameList.begin() + frameNumberTo + 1);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
void vtkIGSIOTrackedFrameList::Clear()
{
  for (unsigned int i = 0; i < this->TrackedFrameList.size(); i++)
  {
    if (this->TrackedFrameList[i] != NULL)
    {
      delete this->TrackedFrameList[i];
      this->TrackedFrameList[i] = NULL;
    }
  }
  this->TrackedFrameList.clear();
}

//----------------------------------------------------------------------------
void vtkIGSIOTrackedFrameList::PrintSelf(std::ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Number of frames = " << GetNumberOfTrackedFrames();
  for (FieldMapType::const_iterator it = this->CustomFields.begin(); it != this->CustomFields.end(); it++)
  {
    os << indent << it->first << " = " << it->second.second << std::endl;
  }
}

//----------------------------------------------------------------------------
igsioTrackedFrame* vtkIGSIOTrackedFrameList::GetTrackedFrame(int frameNumber)
{
  if ((unsigned int)frameNumber >= this->GetNumberOfTrackedFrames())
  {
    LOG_ERROR("vtkIGSIOTrackedFrameList::GetTrackedFrame requested a non-existing frame (framenumber=" << frameNumber);
    return NULL;
  }
  return this->TrackedFrameList[frameNumber];
}


//----------------------------------------------------------------------------
igsioTrackedFrame* vtkIGSIOTrackedFrameList::GetTrackedFrame(unsigned int frameNumber)
{
  if (frameNumber >= this->GetNumberOfTrackedFrames())
  {
    LOG_ERROR("vtkIGSIOTrackedFrameList::GetTrackedFrame requested a non-existing frame (framenumber=" << frameNumber);
    return NULL;
  }
  return this->TrackedFrameList[frameNumber];
}

//----------------------------------------------------------------------------
unsigned int vtkIGSIOTrackedFrameList::GetNumberOfTrackedFrames()
{
  return this->Size();
}

//----------------------------------------------------------------------------
unsigned int vtkIGSIOTrackedFrameList::Size()
{
  return this->TrackedFrameList.size();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType vtkIGSIOTrackedFrameList::GetTrackedFrameList()
{
  return this->TrackedFrameList;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::AddTrackedFrameList(vtkIGSIOTrackedFrameList* inTrackedFrameList, InvalidFrameAction action /*=ADD_INVALID_FRAME_AND_REPORT_ERROR*/)
{
  igsioStatus status = IGSIO_SUCCESS;
  for (unsigned int i = 0; i < inTrackedFrameList->GetNumberOfTrackedFrames(); ++i)
  {
    if (this->AddTrackedFrame(inTrackedFrameList->GetTrackedFrame(i), action) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to add tracked frame to the list!");
      status = IGSIO_FAIL;
      continue;
    }
  }

  return status;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::AddTrackedFrame(igsioTrackedFrame* trackedFrame, InvalidFrameAction action /*=ADD_INVALID_FRAME_AND_REPORT_ERROR*/)
{
  bool isFrameValid = true;
  if (action != ADD_INVALID_FRAME)
  {
    isFrameValid = this->ValidateData(trackedFrame);
  }

  if (!isFrameValid)
  {
    switch (action)
    {
      case ADD_INVALID_FRAME_AND_REPORT_ERROR:
        LOG_ERROR("Validation failed on frame, the frame is added to the list anyway");
        break;
      case ADD_INVALID_FRAME:
        LOG_DEBUG("Validation failed on frame, the frame is added to the list anyway");
        break;
      case SKIP_INVALID_FRAME_AND_REPORT_ERROR:
        LOG_ERROR("Validation failed on frame, the frame is ignored");
        return IGSIO_FAIL;
      case SKIP_INVALID_FRAME:
        LOG_DEBUG("Validation failed on frame, the frame is ignored");
        return IGSIO_SUCCESS;
    }
  }

  // Make a copy and add frame to the list
  igsioTrackedFrame* pTrackedFrame = new igsioTrackedFrame(*trackedFrame);
  this->TrackedFrameList.push_back(pTrackedFrame);
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::TakeTrackedFrame(igsioTrackedFrame* trackedFrame, InvalidFrameAction action /*=ADD_INVALID_FRAME_AND_REPORT_ERROR*/)
{
  bool isFrameValid = true;
  if (action != ADD_INVALID_FRAME)
  {
    isFrameValid = this->ValidateData(trackedFrame);
  }
  if (!isFrameValid)
  {
    switch (action)
    {
      case ADD_INVALID_FRAME_AND_REPORT_ERROR:
        LOG_ERROR("Validation failed on frame, the frame is added to the list anyway");
        break;
      case ADD_INVALID_FRAME:
        LOG_DEBUG("Validation failed on frame, the frame is added to the list anyway");
        break;
      case SKIP_INVALID_FRAME_AND_REPORT_ERROR:
        LOG_ERROR("Validation failed on frame, the frame is ignored");
        delete trackedFrame;
        return IGSIO_FAIL;
      case SKIP_INVALID_FRAME:
        LOG_DEBUG("Validation failed on frame, the frame is ignored");
        delete trackedFrame;
        return IGSIO_SUCCESS;
    }
  }

  // Make a copy and add frame to the list
  this->TrackedFrameList.push_back(trackedFrame);
  return IGSIO_SUCCESS;
}


//----------------------------------------------------------------------------
bool vtkIGSIOTrackedFrameList::ValidateData(igsioTrackedFrame* trackedFrame)
{
  if (this->ValidationRequirements == 0)
  {
    // If we don't want to validate return immediately
    return true;
  }

  if (this->ValidationRequirements & REQUIRE_UNIQUE_TIMESTAMP)
  {
    if (! this->ValidateTimestamp(trackedFrame))
    {
      LOG_DEBUG("Validation failed - timestamp is not unique: " << trackedFrame->GetTimestamp());
      return false;
    }
  }

  if (this->ValidationRequirements & REQUIRE_TRACKING_OK)
  {
    if (! this->ValidateStatus(trackedFrame))
    {
      LOG_DEBUG("Validation failed - tracking status in not OK");
      return false;
    }
  }

  if (this->ValidationRequirements & REQUIRE_CHANGED_TRANSFORM)
  {
    if (! this->ValidateTransform(trackedFrame))
    {
      LOG_DEBUG("Validation failed - transform is not changed");
      return false;
    }
  }

  if (this->ValidationRequirements & REQUIRE_CHANGED_ENCODER_POSITION)
  {
    if (! this->ValidateEncoderPosition(trackedFrame))
    {
      LOG_DEBUG("Validation failed - encoder position is not changed");
      return false;
    }
  }


  if (this->ValidationRequirements & REQUIRE_SPEED_BELOW_THRESHOLD)
  {
    if (! this->ValidateSpeed(trackedFrame))
    {
      LOG_DEBUG("Validation failed - speed is higher than threshold");
      return false;
    }
  }

  return true;
}

//----------------------------------------------------------------------------
bool vtkIGSIOTrackedFrameList::ValidateTimestamp(igsioTrackedFrame* trackedFrame)
{
  if (this->TrackedFrameList.size() == 0)
  {
    // the existing list is empty, so any frame has unique timestamp and therefore valid
    return true;
  }
  const bool isTimestampUnique = std::find_if(this->TrackedFrameList.begin(), this->TrackedFrameList.end(), TrackedFrameTimestampFinder(trackedFrame)) == this->TrackedFrameList.end();
  // validation passed if the timestamp is unique
  return isTimestampUnique;
}

//----------------------------------------------------------------------------
bool vtkIGSIOTrackedFrameList::ValidateEncoderPosition(igsioTrackedFrame* trackedFrame)
{
  TrackedFrameListType::iterator searchIndex;
  const int containerSize = this->TrackedFrameList.size();
  if (containerSize < this->NumberOfUniqueFrames)
  {
    searchIndex = this->TrackedFrameList.begin();
  }
  else
  {
    searchIndex = this->TrackedFrameList.end() - this->NumberOfUniqueFrames;
  }

  if (std::find_if(searchIndex, this->TrackedFrameList.end(), igsioTrackedFrameEncoderPositionFinder(trackedFrame,
                   this->MinRequiredTranslationDifferenceMm, this->MinRequiredAngleDifferenceDeg)) != this->TrackedFrameList.end())
  {
    // We've already inserted this frame
    LOG_DEBUG("Tracked frame encoder position validation result: we've already inserted this frame to container!");
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
bool vtkIGSIOTrackedFrameList::ValidateTransform(igsioTrackedFrame* trackedFrame)
{
  TrackedFrameListType::iterator searchIndex;
  const int containerSize = this->TrackedFrameList.size();
  if (containerSize < this->NumberOfUniqueFrames)
  {
    searchIndex = this->TrackedFrameList.begin();
  }
  else
  {
    searchIndex = this->TrackedFrameList.end() - this->NumberOfUniqueFrames;
  }

  if (std::find_if(searchIndex, this->TrackedFrameList.end(), TrackedFrameTransformFinder(trackedFrame, this->FrameTransformNameForValidation,
                   this->MinRequiredTranslationDifferenceMm, this->MinRequiredAngleDifferenceDeg)) != this->TrackedFrameList.end())
  {
    // We've already inserted this frame
    LOG_DEBUG("Tracked frame transform validation result: we've already inserted this frame to container!");
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------
bool vtkIGSIOTrackedFrameList::ValidateStatus(igsioTrackedFrame* trackedFrame)
{
  vtkIGSIOTransformRepository* repo = vtkIGSIOTransformRepository::New();
  repo->SetTransforms(*trackedFrame);
  bool isValid(false);
  if (repo->GetTransformValid(this->FrameTransformNameForValidation, isValid) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Unable to retrieve transform \'" << this->FrameTransformNameForValidation.GetTransformName() << "\'.");
  }
  repo->Delete();

  return isValid;
}

//----------------------------------------------------------------------------
bool vtkIGSIOTrackedFrameList::ValidateSpeed(igsioTrackedFrame* trackedFrame)
{
  if (this->TrackedFrameList.size() < 1)
  {
    return true;
  }

  TrackedFrameListType::iterator latestFrameInList = this->TrackedFrameList.end() - 1;

  // Compute difference between the last two timestamps
  double diffTimeSec = fabs(trackedFrame->GetTimestamp() - (*latestFrameInList)->GetTimestamp());
  if (diffTimeSec < 0.0001)
  {
    // the frames are almost acquired at the same time, cannot compute speed reliably
    // better to invalidate the frame
    return false;
  }

  vtkSmartPointer<vtkMatrix4x4> inputTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  double inputTransformVector[16] = {0};
  if (trackedFrame->GetFrameTransform(this->FrameTransformNameForValidation, inputTransformVector))
  {
    inputTransformMatrix->DeepCopy(inputTransformVector);
  }
  else
  {
    std::string strFrameTransformName;
    this->FrameTransformNameForValidation.GetTransformName(strFrameTransformName);
    LOG_ERROR("Unable to get frame transform '" << strFrameTransformName << "' from input tracked frame!");
    return false;
  }

  vtkSmartPointer<vtkMatrix4x4> latestTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  double latestTransformVector[16] = {0};
  if ((*latestFrameInList)->GetFrameTransform(this->FrameTransformNameForValidation, latestTransformVector))
  {
    latestTransformMatrix->DeepCopy(latestTransformVector);
  }
  else
  {
    LOG_ERROR("Unable to get default frame transform for latest frame!");
    return false;
  }

  if (this->MaxAllowedTranslationSpeedMmPerSec > 0)
  {
    // Compute difference between the last two positions
    double diffPosition = igsioMath::GetPositionDifference(inputTransformMatrix, latestTransformMatrix);
    double velocityPositionMmPerSec = fabs(diffPosition / diffTimeSec);
    if (velocityPositionMmPerSec > this->MaxAllowedTranslationSpeedMmPerSec)
    {
      LOG_DEBUG("Tracked frame speed validation result: tracked frame position change too fast (VelocityPosition = " << velocityPositionMmPerSec << ">" << this->MaxAllowedTranslationSpeedMmPerSec << " mm/sec)");
      return false;
    }
  }

  if (this->MaxAllowedRotationSpeedDegPerSec > 0)
  {
    // Compute difference between the last two orientations
    double diffOrientationDeg = igsioMath::GetOrientationDifference(inputTransformMatrix, latestTransformMatrix);
    double velocityOrientationDegPerSec = fabs(diffOrientationDeg / diffTimeSec);
    if (velocityOrientationDegPerSec > this->MaxAllowedRotationSpeedDegPerSec)
    {
      LOG_DEBUG("Tracked frame speed validation result: tracked frame angle change too fast VelocityOrientation = " << velocityOrientationDegPerSec << ">" << this->MaxAllowedRotationSpeedDegPerSec << " deg/sec)");
      return false;
    }
  }

  return true;
}

//----------------------------------------------------------------------------
igsioTransformName vtkIGSIOTrackedFrameList::GetFrameTransformNameForValidation()
{
  return this->FrameTransformNameForValidation;
}

//----------------------------------------------------------------------------
int vtkIGSIOTrackedFrameList::GetNumberOfBitsPerScalar()
{
  int numberOfBitsPerScalar = 0;
  if (this->GetNumberOfTrackedFrames() > 0)
  {
    numberOfBitsPerScalar = this->GetTrackedFrame(0)->GetNumberOfBitsPerScalar();
  }
  else
  {
    LOG_WARNING("Unable to get bits per scalar: there is no frame in the tracked frame list!");
  }

  return numberOfBitsPerScalar;
}

//----------------------------------------------------------------------------
int vtkIGSIOTrackedFrameList::GetNumberOfBitsPerPixel()
{
  int numberOfBitsPerPixel = 0;
  if (this->GetNumberOfTrackedFrames() > 0)
  {
    numberOfBitsPerPixel = this->GetTrackedFrame(0)->GetNumberOfBitsPerPixel();
  }
  else
  {
    LOG_WARNING("Unable to get bits per pixel: there is no frame in the tracked frame list!");
  }

  return numberOfBitsPerPixel;
}

//-----------------------------------------------------------------------------
igsioCommon::VTKScalarPixelType vtkIGSIOTrackedFrameList::GetPixelType()
{
  if (this->GetNumberOfTrackedFrames() < 1)
  {
    LOG_ERROR("Unable to get pixel type size: there is no frame in the tracked frame list!");
    return VTK_VOID;
  }

  for (unsigned int i = 0; i < this->GetNumberOfTrackedFrames(); ++i)
  {
    if (this->GetTrackedFrame(i)->GetImageData()->IsImageValid())
    {
      return this->GetTrackedFrame(i)->GetImageData()->GetVTKScalarPixelType();
    }
  }

  LOG_WARNING("There are no valid images in the tracked frame list.");
  return VTK_VOID;
}

//-----------------------------------------------------------------------------
int vtkIGSIOTrackedFrameList::GetNumberOfScalarComponents()
{
  if (this->GetNumberOfTrackedFrames() < 1)
  {
    LOG_ERROR("Unable to get number of scalar components: there is no frame in the tracked frame list!");
    return 1;
  }

  for (unsigned int i = 0; i < this->GetNumberOfTrackedFrames(); ++i)
  {
    if (this->GetTrackedFrame(i)->GetImageData()->IsImageValid())
    {
      return this->GetTrackedFrame(i)->GetImageData()->GetImage()->GetNumberOfScalarComponents();
    }
  }

  LOG_WARNING("There are no valid images in the tracked frame list.");
  return 1;
}

//-----------------------------------------------------------------------------
US_IMAGE_ORIENTATION vtkIGSIOTrackedFrameList::GetImageOrientation()
{
  if (this->GetNumberOfTrackedFrames() < 1)
  {
    LOG_ERROR("Unable to get image orientation: there is no frame in the tracked frame list!");
    return US_IMG_ORIENT_XX;
  }

  for (unsigned int i = 0; i < this->GetNumberOfTrackedFrames(); ++i)
  {
    if (this->GetTrackedFrame(i)->GetImageData()->IsImageValid())
    {
      return this->GetTrackedFrame(i)->GetImageData()->GetImageOrientation();
    }
  }

  LOG_WARNING("There are no valid images in the tracked frame list.");
  return US_IMG_ORIENT_XX;
}

//-----------------------------------------------------------------------------
US_IMAGE_TYPE vtkIGSIOTrackedFrameList::GetImageType()
{
  if (this->GetNumberOfTrackedFrames() < 1)
  {
    LOG_ERROR("Unable to get image type: there is no frame in the tracked frame list!");
    return US_IMG_TYPE_XX;
  }

  for (unsigned int i = 0; i < this->GetNumberOfTrackedFrames(); ++i)
  {
    if (this->GetTrackedFrame(i)->GetImageData()->IsImageValid())
    {
      return this->GetTrackedFrame(i)->GetImageData()->GetImageType();
    }
  }

  LOG_WARNING("There are no valid images in the tracked frame list.");
  return US_IMG_TYPE_XX;
}


//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::GetFrameSize(FrameSizeType& outFrameSize)
{
  if (this->GetNumberOfTrackedFrames() < 1)
  {
    LOG_ERROR("Unable to get image type: there are no frames in the tracked frame list!");
    return IGSIO_FAIL;
  }

  for (unsigned int i = 0; i < this->GetNumberOfTrackedFrames(); ++i)
  {
    if (this->GetTrackedFrame(i)->GetImageData() && this->GetTrackedFrame(i)->GetImageData()->IsImageValid())
    {
      outFrameSize = this->GetTrackedFrame(i)->GetFrameSize();
      return IGSIO_SUCCESS;
    }
  }

  LOG_WARNING("There are no valid images in the tracked frame list.");
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::GetEncodingFourCC(std::string& encoding)
{
  if (this->GetNumberOfTrackedFrames() < 1)
  {
    LOG_ERROR("Unable to get image type: there are no frames in the tracked frame list!");
    return IGSIO_FAIL;
  }

  for (unsigned int i = 0; i < this->GetNumberOfTrackedFrames(); ++i)
  {
    if (this->GetTrackedFrame(i)->GetImageData() && this->GetTrackedFrame(i)->GetImageData()->IsImageValid())
    {
      encoding = this->GetTrackedFrame(i)->GetEncodingFourCC();
      return IGSIO_SUCCESS;
    }
  }

  LOG_WARNING("There are no valid images in the tracked frame list.");
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
const char* vtkIGSIOTrackedFrameList::GetCustomString(const char* fieldName)
{
  FieldMapType::iterator fieldIterator;
  fieldIterator = this->CustomFields.find(fieldName);
  if (fieldIterator != this->CustomFields.end())
  {
    return fieldIterator->second.second.c_str();
  }
  // Can't use/return this->GetFrameField(fieldName).c_str(), since the string object would be deleted,
  // leaving a dangling char* pointer. 
  return NULL;
}


//----------------------------------------------------------------------------
std::string vtkIGSIOTrackedFrameList::GetFrameField(const std::string& fieldName) const
{
  igsioFieldMapType::const_iterator fieldIterator = this->CustomFields.find(fieldName);
  if (fieldIterator != this->CustomFields.end())
  {
    return fieldIterator->second.second;
  }
  return std::string("");
}

//----------------------------------------------------------------------------
std::string vtkIGSIOTrackedFrameList::GetCustomString(const std::string& fieldName) const
{
  return this->GetFrameField(fieldName);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::GetCustomTransform(const char* frameTransformName, vtkMatrix4x4* outMatrix) const
{
  if (frameTransformName == NULL)
  {
    LOG_ERROR("Invalid frame transform name");
    return IGSIO_FAIL;
  }
  if (outMatrix == nullptr)
  {
    LOG_ERROR("Invalid call to vtkIGSIOTrackedFrameList::GetCustomTransform. Cannot continue.");
    return IGSIO_FAIL;
  }

  return this->GetTransform(igsioTransformName(frameTransformName), *outMatrix);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::GetCustomTransform(const char* frameTransformName, double* transformMatrix) const
{
  if (frameTransformName == NULL)
  {
    LOG_ERROR("Invalid frame transform name");
    return IGSIO_FAIL;
  }
  if (transformMatrix == nullptr)
  {
    LOG_ERROR("Invalid call to vtkIGSIOTrackedFrameList::GetCustomTransform. Cannot continue.");
    return IGSIO_FAIL;
  }

  return this->GetTransform(igsioTransformName(frameTransformName), transformMatrix);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::GetTransform(const igsioTransformName& name, vtkMatrix4x4& outMatrix) const
{
  if (name.GetTransformName().empty())
  {
    LOG_ERROR("Invalid transform name.");
    return IGSIO_FAIL;
  }

  std::string customString = this->GetFrameField(name.GetTransformName() + "Transform");
  if (customString.empty())
  {
    LOG_ERROR("Cannot find frame transform " << name.GetTransformName());
    return IGSIO_FAIL;
  }

  std::vector<std::string> entries = igsioCommon::SplitStringIntoTokens(customString, ' ', false);
  if (entries.size() != 16)
  {
    LOG_ERROR("Transform " << name.GetTransformName() << " is ill-defined. Aborting.");
    return IGSIO_FAIL;
  }

  std::istringstream transformFieldValue(customString);
  double item;
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      transformFieldValue >> item;
      if (transformFieldValue.fail())
      {
        LOG_ERROR("Unable to parse number in " << name.GetTransformName());
        return IGSIO_FAIL;
      }
      outMatrix.SetElement(i, j, item);
    }
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::GetTransform(const igsioTransformName& name, double* outMatrix) const
{
  vtkNew<vtkMatrix4x4> mat;
  if (this->GetTransform(name, *mat) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Unable to retrieve transform " << name.GetTransformName());
    return IGSIO_FAIL;
  }
  if (outMatrix == nullptr)
  {
    LOG_ERROR("Invalid call to vtkIGSIOTrackedFrameList::GetCustomTransform. Cannot continue.");
    return IGSIO_FAIL;
  }

  memcpy(outMatrix, mat->GetData(), sizeof(double) * 16);
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::SetCustomTransform(const char* frameTransformName, vtkMatrix4x4* transformMatrix)
{
  if (frameTransformName == NULL)
  {
    LOG_ERROR("Invalid frame transform name");
    return IGSIO_FAIL;
  }
  if (transformMatrix == nullptr)
  {
    LOG_ERROR("Invalid call to vtkIGSIOTrackedFrameList::GetCustomTransform. Cannot continue.");
    return IGSIO_FAIL;
  }

  return this->SetTransform(igsioTransformName(frameTransformName), transformMatrix->GetData());
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::SetCustomTransform(const char* frameTransformName, double* transformMatrix)
{
  if (frameTransformName == NULL)
  {
    LOG_ERROR("Invalid frame transform name");
    return IGSIO_FAIL;
  }
  if (transformMatrix == nullptr)
  {
    LOG_ERROR("Invalid call to vtkIGSIOTrackedFrameList::GetCustomTransform. Cannot continue.");
    return IGSIO_FAIL;
  }

  return this->SetTransform(igsioTransformName(frameTransformName), transformMatrix);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::SetTransform(const igsioTransformName& frameTransformName, const vtkMatrix4x4& transformMatrix)
{
  if (frameTransformName.GetTransformName().empty())
  {
    LOG_ERROR("Invalid transform name.");
    return IGSIO_FAIL;
  }
  double elements[16];
  memcpy(elements, const_cast<vtkMatrix4x4*>(&transformMatrix)->GetData(), sizeof(double) * 16); // ew, but memcpy will preserve const concept of incoming argument
  return this->SetTransform(igsioTransformName(frameTransformName), elements);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::SetTransform(const igsioTransformName& frameTransformName, double* transformMatrix)
{
  if (frameTransformName.GetTransformName().empty())
  {
    LOG_ERROR("Invalid transform name.");
    return IGSIO_FAIL;
  }
  if (transformMatrix == nullptr)
  {
    LOG_ERROR("Invalid call to vtkIGSIOTrackedFrameList::GetCustomTransform. Cannot continue.");
    return IGSIO_FAIL;
  }

  std::ostringstream transform;
  transform << transformMatrix[0] << " " << transformMatrix[1] << " " << transformMatrix[2] << " " << transformMatrix[3] << " "
            << transformMatrix[4] << " " << transformMatrix[5] << " " << transformMatrix[6] << " " << transformMatrix[7] << " "
            << transformMatrix[8] << " " << transformMatrix[9] << " " << transformMatrix[10] << " " << transformMatrix[11] << " "
            << transformMatrix[12] << " " << transformMatrix[13] << " " << transformMatrix[14] << " " << transformMatrix[15] << " ";
  return this->SetFrameField(frameTransformName.GetTransformName(), transform.str(), FRAMEFIELD_NONE);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::SetCustomString(const char* fieldName, const char* fieldValue, igsioFrameFieldFlags flags)
{
  if (fieldName == NULL)
  {
    LOG_ERROR("Field name is invalid");
    return IGSIO_FAIL;
  }
  if (fieldValue == NULL)
  {
    this->CustomFields.erase(fieldName);
    return IGSIO_SUCCESS;
  }
  return this->SetFrameField(fieldName, fieldValue, flags);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::SetCustomString(const std::string& fieldName, const std::string& fieldValue, igsioFrameFieldFlags flags)
{
  return this->SetFrameField(fieldName, fieldValue, flags);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::SetFrameField(const std::string& fieldName, const std::string& fieldValue, igsioFrameFieldFlags flags)
{
  if (fieldName.empty())
  {
    LOG_ERROR("Field name is invalid");
    return IGSIO_FAIL;
  }
  if (fieldValue.empty())
  {
    this->CustomFields.erase(fieldName);
    return IGSIO_SUCCESS;
  }
  this->CustomFields[fieldName].first = flags;
  this->CustomFields[fieldName].second = fieldValue;
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
void vtkIGSIOTrackedFrameList::GetCustomFieldNameList(std::vector<std::string>& fieldNames)
{
  fieldNames.clear();
  for (FieldMapType::const_iterator it = this->CustomFields.begin(); it != this->CustomFields.end(); it++)
  {
    fieldNames.push_back(it->first);
  }
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::GetGlobalTransform(vtkMatrix4x4* globalTransform)
{
  const char* offsetStr = GetCustomString("Offset");
  if (offsetStr == NULL)
  {
    LOG_ERROR("Cannot determine global transform, Offset is undefined");
    return IGSIO_FAIL;
  }
  const char* transformStr = GetCustomString("TransformMatrix");
  if (transformStr == NULL)
  {
    LOG_ERROR("Cannot determine global transform, TransformMatrix is undefined");
    return IGSIO_FAIL;
  }

  double item;
  int i = 0;

  const int offsetLen = 3;
  double offset[offsetLen];
  std::istringstream offsetFieldValue(offsetStr);
  for (i = 0; offsetFieldValue >> item && i < offsetLen; i++)
  {
    offset[i] = item;
  }
  if (i < offsetLen)
  {
    LOG_ERROR("Not enough elements in the Offset field (expected " << offsetLen << ", found " << i << ")");
    return IGSIO_FAIL;
  }

  const int transformLen = 9;
  double transform[transformLen];
  std::istringstream transformFieldValue(transformStr);
  for (i = 0; transformFieldValue >> item && i < transformLen; i++)
  {
    transform[i] = item;
  }
  if (i < transformLen)
  {
    LOG_ERROR("Not enough elements in the TransformMatrix field (expected " << transformLen << ", found " << i << ")");
    return IGSIO_FAIL;
  }

  globalTransform->Identity();

  globalTransform->SetElement(0, 3, offset[0]);
  globalTransform->SetElement(1, 3, offset[1]);
  globalTransform->SetElement(2, 3, offset[2]);

  globalTransform->SetElement(0, 0, transform[0]);
  globalTransform->SetElement(0, 1, transform[1]);
  globalTransform->SetElement(0, 2, transform[2]);
  globalTransform->SetElement(1, 0, transform[3]);
  globalTransform->SetElement(1, 1, transform[4]);
  globalTransform->SetElement(1, 2, transform[5]);
  globalTransform->SetElement(2, 0, transform[6]);
  globalTransform->SetElement(2, 1, transform[7]);
  globalTransform->SetElement(2, 2, transform[8]);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::SetGlobalTransform(vtkMatrix4x4* globalTransform)
{
  std::ostringstream strOffset;
  strOffset << globalTransform->GetElement(0, 3)
            << " " << globalTransform->GetElement(1, 3)
            << " " << globalTransform->GetElement(2, 3);
  this->SetFrameField("Offset", strOffset.str(), igsioFrameFieldFlags::FRAMEFIELD_NONE);

  std::ostringstream strTransform;
  strTransform << globalTransform->GetElement(0, 0) << " " << globalTransform->GetElement(0, 1) << " " << globalTransform->GetElement(0, 2) << " ";
  strTransform << globalTransform->GetElement(1, 0) << " " << globalTransform->GetElement(1, 1) << " " << globalTransform->GetElement(1, 2) << " ";
  strTransform << globalTransform->GetElement(2, 0) << " " << globalTransform->GetElement(2, 1) << " " << globalTransform->GetElement(2, 2);
  this->SetFrameField("TransformMatrix", strTransform.str(), igsioFrameFieldFlags::FRAMEFIELD_NONE);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOTrackedFrameList::VerifyProperties(vtkIGSIOTrackedFrameList* trackedFrameList, US_IMAGE_ORIENTATION expectedOrientation, US_IMAGE_TYPE expectedType)
{
  if (trackedFrameList == NULL)
  {
    LOG_ERROR("vtkIGSIOTrackedFrameList::VerifyProperties failed: tracked frame list is NULL!");
    return IGSIO_FAIL;
  }
  if (trackedFrameList->GetImageOrientation() != expectedOrientation)
  {
    LOG_ERROR("vtkIGSIOTrackedFrameList::VerifyProperties failed: expected image orientation is "
              << igsioVideoFrame::GetStringFromUsImageOrientation(expectedOrientation)
              << ", actual orientation is "
              << igsioVideoFrame::GetStringFromUsImageOrientation(trackedFrameList->GetImageOrientation()));
    return IGSIO_FAIL;
  }
  if (trackedFrameList->GetImageType() != expectedType)
  {
    LOG_ERROR("vtkIGSIOTrackedFrameList::VerifyProperties failed: expected image type is "
              << igsioVideoFrame::GetStringFromUsImageType(expectedType)
              << ", actual type is "
              << igsioVideoFrame::GetStringFromUsImageType(trackedFrameList->GetImageType()));
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
double vtkIGSIOTrackedFrameList::GetMostRecentTimestamp()
{
  double mostRecentTimestamp = 0.0;

  for (unsigned int i = 0; i < this->GetNumberOfTrackedFrames(); ++i)
  {
    if (this->GetTrackedFrame(i)->GetTimestamp() > mostRecentTimestamp)
    {
      mostRecentTimestamp = this->GetTrackedFrame(i)->GetTimestamp();
    }
  }

  return mostRecentTimestamp;
}

//-----------------------------------------------------------------------------
bool vtkIGSIOTrackedFrameList::IsContainingValidImageData()
{
  for (unsigned int i = 0; i < this->GetNumberOfTrackedFrames(); ++i)
  {
    if (this->GetTrackedFrame(i)->GetImageData()->IsImageValid())
    {
      // found a valid image
      return true;
    }
  }
  // no valid images found
  return false;
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::iterator vtkIGSIOTrackedFrameList::begin()
{
  return TrackedFrameList.begin();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::const_iterator vtkIGSIOTrackedFrameList::begin() const
{
  return TrackedFrameList.begin();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::iterator vtkIGSIOTrackedFrameList::end()
{
  return TrackedFrameList.end();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::const_iterator vtkIGSIOTrackedFrameList::end() const
{
  return TrackedFrameList.end();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::iterator begin(vtkIGSIOTrackedFrameList& list)
{
  return list.begin();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::const_iterator begin(const vtkIGSIOTrackedFrameList& list)
{
  return list.begin();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::iterator end(vtkIGSIOTrackedFrameList& list)
{
  return list.end();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::const_iterator end(const vtkIGSIOTrackedFrameList& list)
{
  return list.end();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::reverse_iterator vtkIGSIOTrackedFrameList::rbegin()
{
  return TrackedFrameList.rbegin();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::const_reverse_iterator vtkIGSIOTrackedFrameList::rbegin() const
{
  return TrackedFrameList.rbegin();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::reverse_iterator vtkIGSIOTrackedFrameList::rend()
{
  return TrackedFrameList.rend();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::const_reverse_iterator vtkIGSIOTrackedFrameList::rend() const
{
  return TrackedFrameList.rend();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::reverse_iterator rbegin(vtkIGSIOTrackedFrameList& list)
{
  return list.rbegin();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::const_reverse_iterator rbegin(const vtkIGSIOTrackedFrameList& list)
{
  return list.rbegin();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::reverse_iterator rend(vtkIGSIOTrackedFrameList& list)
{
  return list.rend();
}

//----------------------------------------------------------------------------
vtkIGSIOTrackedFrameList::TrackedFrameListType::const_reverse_iterator rend(const vtkIGSIOTrackedFrameList& list)
{
  return list.rend();
}
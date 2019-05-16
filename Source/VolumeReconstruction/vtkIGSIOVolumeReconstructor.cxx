/*=IGSIO=header=begin======================================================
  Program: IGSIO
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================IGSIO=header=end*/

// Local includes
#include "igsioConfigure.h"
#include "igsioTrackedFrame.h"
#include "vtkIGSIOFanAngleDetectorAlgo.h"
#include "vtkIGSIOFillHolesInVolume.h"
#include "igsioXmlUtils.h"
#include "vtkIGSIOSequenceIO.h"
#include "vtkIGSIOTrackedFrameList.h"
#include "vtkIGSIOTransformRepository.h"
#include "vtkIGSIOVolumeReconstructor.h"
#include "igsioCommon.h"

// STL includes
#include <limits>

// VTK includes
#include <vtkDataSetWriter.h>
#include <vtkImageData.h>
#include <vtkImageExtractComponents.h>
#include <vtkImageFlip.h>
#include <vtkImageImport.h>
#include <vtkImageImport.h>
#include <vtkImageViewer.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPNGReader.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkXMLUtilities.h>

vtkStandardNewMacro(vtkIGSIOVolumeReconstructor);

//----------------------------------------------------------------------------
vtkIGSIOVolumeReconstructor::vtkIGSIOVolumeReconstructor()
  : ReconstructedVolume(vtkSmartPointer<vtkImageData>::New())
  , Reconstructor(vtkIGSIOPasteSliceIntoVolume::New())
  , HoleFiller(vtkIGSIOFillHolesInVolume::New())
  , FanAngleDetector(vtkIGSIOFanAngleDetectorAlgo::New())
  , FillHoles(false)
  , EnableFanAnglesAutoDetect(false)
  , SkipInterval(1)
  , ReconstructedVolumeUpdatedTime(0)
{
  this->FanAnglesDeg[0] = 0.0;
  this->FanAnglesDeg[1] = 0.0;
}

//----------------------------------------------------------------------------
vtkIGSIOVolumeReconstructor::~vtkIGSIOVolumeReconstructor()
{
  if (this->Reconstructor)
  {
    this->Reconstructor->Delete();
    this->Reconstructor = NULL;
  }
  if (this->HoleFiller)
  {
    this->HoleFiller->Delete();
    this->HoleFiller = NULL;
  }
  if (this->FanAngleDetector)
  {
    this->FanAngleDetector->Delete();
    this->FanAngleDetector = NULL;
  }
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::ReadConfiguration(vtkXMLDataElement* config)
{
  XML_FIND_NESTED_ELEMENT_REQUIRED(reconConfig, config, "VolumeReconstruction");

  XML_READ_STRING_ATTRIBUTE_OPTIONAL(ReferenceCoordinateFrame, reconConfig);
  XML_READ_STRING_ATTRIBUTE_OPTIONAL(ImageCoordinateFrame, reconConfig);

  XML_READ_VECTOR_ATTRIBUTE_REQUIRED(double, 3, OutputSpacing, reconConfig);
  XML_READ_VECTOR_ATTRIBUTE_OPTIONAL(double, 3, OutputOrigin, reconConfig);
  XML_READ_VECTOR_ATTRIBUTE_OPTIONAL(int, 6, OutputExtent, reconConfig);

  XML_READ_VECTOR_ATTRIBUTE_OPTIONAL(int, 2, ClipRectangleOrigin, reconConfig);
  XML_READ_VECTOR_ATTRIBUTE_OPTIONAL(int, 2, ClipRectangleSize, reconConfig);

  XML_READ_VECTOR_ATTRIBUTE_OPTIONAL(double, 2, FanAngles, reconConfig);  // DEPRECATED (replaced by FanAnglesDeg)
  XML_READ_VECTOR_ATTRIBUTE_OPTIONAL(double, 2, FanAnglesDeg, reconConfig);

  XML_READ_VECTOR_ATTRIBUTE_OPTIONAL(double, 2, FanOrigin, reconConfig);  // DEPRECATED (replaced by FanOriginPixel)
  XML_READ_VECTOR_ATTRIBUTE_OPTIONAL(double, 2, FanOriginPixel, reconConfig);

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, FanDepth, reconConfig);   // DEPRECATED (replaced by FanRadiusStopPixel)
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, FanRadiusStartPixel, reconConfig);
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, FanRadiusStopPixel, reconConfig);

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, FanAnglesAutoDetectBrightnessThreshold, reconConfig);
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, FanAnglesAutoDetectFilterRadiusPixel, reconConfig);

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, PixelRejectionThreshold, reconConfig);

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, SkipInterval, reconConfig);
  if (this->SkipInterval < 1)
  {
    LOG_WARNING("SkipInterval in the config file must be greater or equal to 1. Resetting to 1");
    SkipInterval = 1;
  }

  // reconstruction options
  XML_READ_ENUM2_ATTRIBUTE_OPTIONAL(Interpolation, reconConfig,
                                    this->Reconstructor->GetInterpolationModeAsString(vtkIGSIOPasteSliceIntoVolume::LINEAR_INTERPOLATION), vtkIGSIOPasteSliceIntoVolume::LINEAR_INTERPOLATION,
                                    this->Reconstructor->GetInterpolationModeAsString(vtkIGSIOPasteSliceIntoVolume::NEAREST_NEIGHBOR_INTERPOLATION), vtkIGSIOPasteSliceIntoVolume::NEAREST_NEIGHBOR_INTERPOLATION);

  XML_READ_ENUM3_ATTRIBUTE_OPTIONAL(Optimization, reconConfig,
                                    this->Reconstructor->GetOptimizationModeAsString(vtkIGSIOPasteSliceIntoVolume::FULL_OPTIMIZATION), vtkIGSIOPasteSliceIntoVolume::FULL_OPTIMIZATION,
                                    this->Reconstructor->GetOptimizationModeAsString(vtkIGSIOPasteSliceIntoVolume::PARTIAL_OPTIMIZATION), vtkIGSIOPasteSliceIntoVolume::PARTIAL_OPTIMIZATION,
                                    this->Reconstructor->GetOptimizationModeAsString(vtkIGSIOPasteSliceIntoVolume::NO_OPTIMIZATION), vtkIGSIOPasteSliceIntoVolume::NO_OPTIMIZATION);

  XML_READ_ENUM4_ATTRIBUTE_OPTIONAL(CompoundingMode, reconConfig,
                                    this->Reconstructor->GetCompoundingModeAsString(vtkIGSIOPasteSliceIntoVolume::LATEST_COMPOUNDING_MODE), vtkIGSIOPasteSliceIntoVolume::LATEST_COMPOUNDING_MODE,
                                    this->Reconstructor->GetCompoundingModeAsString(vtkIGSIOPasteSliceIntoVolume::MEAN_COMPOUNDING_MODE), vtkIGSIOPasteSliceIntoVolume::MEAN_COMPOUNDING_MODE,
                                    this->Reconstructor->GetCompoundingModeAsString(vtkIGSIOPasteSliceIntoVolume::IMPORTANCE_MASK_COMPOUNDING_MODE), vtkIGSIOPasteSliceIntoVolume::IMPORTANCE_MASK_COMPOUNDING_MODE,
                                    this->Reconstructor->GetCompoundingModeAsString(vtkIGSIOPasteSliceIntoVolume::MAXIMUM_COMPOUNDING_MODE), vtkIGSIOPasteSliceIntoVolume::MAXIMUM_COMPOUNDING_MODE);

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(int, NumberOfThreads, reconConfig);

  XML_READ_ENUM2_ATTRIBUTE_OPTIONAL(FillHoles, reconConfig, "ON", true, "OFF", false);
  XML_READ_BOOL_ATTRIBUTE_OPTIONAL(EnableFanAnglesAutoDetect, reconConfig);

  // Find and read kernels. First for loop counts the number of kernels to allocate, second for loop stores them
  if (this->FillHoles)
  {
    // load input for kernel size, stdev, etc...
    XML_FIND_NESTED_ELEMENT_REQUIRED(holeFilling, reconConfig, "HoleFilling");
    if (this->HoleFiller->ReadConfiguration(holeFilling) != IGSIO_SUCCESS)
    {
      return IGSIO_FAIL;
    }
  }

  // ==== Warn if using DEPRECATED XML tags (2014-08-15, #923) ====
  XML_READ_WARNING_DEPRECATED_CSTRING_REPLACED(Compounding, reconConfig, CompoundingMode);
  XML_READ_ENUM2_ATTRIBUTE_OPTIONAL(Compounding, reconConfig, "ON", 1, "OFF", 0);

  XML_READ_WARNING_DEPRECATED_CSTRING_REPLACED(Calculation, reconConfig, CompoundingMode);
  XML_READ_ENUM2_ATTRIBUTE_OPTIONAL(Calculation, reconConfig,
                                    this->Reconstructor->GetCalculationAsString(vtkIGSIOPasteSliceIntoVolume::WEIGHTED_AVERAGE_CALCULATION), vtkIGSIOPasteSliceIntoVolume::WEIGHTED_AVERAGE_CALCULATION,
                                    this->Reconstructor->GetCalculationAsString(vtkIGSIOPasteSliceIntoVolume::MAXIMUM_CALCULATION), vtkIGSIOPasteSliceIntoVolume::MAXIMUM_CALCULATION);

  if (this->Reconstructor->GetCompoundingMode() == vtkIGSIOPasteSliceIntoVolume::UNDEFINED_COMPOUNDING_MODE)
  {
    if (this->Reconstructor->GetCalculation() == vtkIGSIOPasteSliceIntoVolume::MAXIMUM_CALCULATION)
    {
      SetCompoundingMode(vtkIGSIOPasteSliceIntoVolume::MAXIMUM_COMPOUNDING_MODE);
    }
    else if (this->Reconstructor->GetCompounding() == 0)
    {
      SetCompoundingMode(vtkIGSIOPasteSliceIntoVolume::LATEST_COMPOUNDING_MODE);
    }
    else
    {
      SetCompoundingMode(vtkIGSIOPasteSliceIntoVolume::MEAN_COMPOUNDING_MODE);
    }
    LOG_WARNING("CompoundingMode has not been set. Will assume " << this->Reconstructor->GetCompoundingModeAsString(this->Reconstructor->GetCompoundingMode()) << ".");
  }

  if (this->Reconstructor->GetCompoundingMode() == vtkIGSIOPasteSliceIntoVolume::IMPORTANCE_MASK_COMPOUNDING_MODE)
  {
    if (this->ImportanceMaskFilename.empty())
    {
      XML_READ_STRING_ATTRIBUTE_REQUIRED(ImportanceMaskFilename, reconConfig);
    }
    else
    {
      // Importance mask has already been specified by other means, it is no longer required
      XML_READ_STRING_ATTRIBUTE_OPTIONAL(ImportanceMaskFilename, reconConfig);
    }

    if (UpdateImportanceMask() == IGSIO_FAIL)
    {
      LOG_ERROR("Failed to set up importance mask");
      return IGSIO_FAIL;
    }
  }
  else
  {
    XML_READ_STRING_ATTRIBUTE_OPTIONAL(ImportanceMaskFilename, reconConfig);
  }

  this->Modified();

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
// Get the XML element describing the freehand object
igsioStatus vtkIGSIOVolumeReconstructor::WriteConfiguration(vtkXMLDataElement* config)
{
  XML_FIND_NESTED_ELEMENT_CREATE_IF_MISSING(reconConfig, config, "VolumeReconstruction");

  XML_WRITE_STRING_ATTRIBUTE_IF_NOT_EMPTY(ImageCoordinateFrame, config);
  XML_WRITE_STRING_ATTRIBUTE_IF_NOT_EMPTY(ReferenceCoordinateFrame, config);

  // output parameters
  reconConfig->SetVectorAttribute("OutputSpacing", 3, this->Reconstructor->GetOutputSpacing());
  reconConfig->SetVectorAttribute("OutputOrigin", 3, this->Reconstructor->GetOutputOrigin());
  reconConfig->SetVectorAttribute("OutputExtent", 6, this->Reconstructor->GetOutputExtent());

  // clipping parameters
  reconConfig->SetVectorAttribute("ClipRectangleOrigin", 2, this->Reconstructor->GetClipRectangleOrigin());
  reconConfig->SetVectorAttribute("ClipRectangleSize", 2, this->Reconstructor->GetClipRectangleSize());

  // Fan parameters
  // remove deprecated attributes
  XML_REMOVE_ATTRIBUTE("FanDepth", reconConfig);
  XML_REMOVE_ATTRIBUTE("FanOrigin", reconConfig);
  XML_REMOVE_ATTRIBUTE("FanAngles", reconConfig);
  if (this->Reconstructor->FanClippingApplied())
  {
    reconConfig->SetVectorAttribute("FanAnglesDeg", 2, this->Reconstructor->GetFanAnglesDeg());
    // Image spacing is 1.0, so reconstructor's fan origin and radius values are in pixels
    reconConfig->SetVectorAttribute("FanOriginPixel", 2, this->Reconstructor->GetFanOrigin());
    reconConfig->SetDoubleAttribute("FanRadiusStartPixel", this->Reconstructor->GetFanRadiusStart());
    reconConfig->SetDoubleAttribute("FanRadiusStopPixel", this->Reconstructor->GetFanRadiusStop());
    XML_WRITE_BOOL_ATTRIBUTE(EnableFanAnglesAutoDetect, reconConfig);
    if (this->EnableFanAnglesAutoDetect)
    {
      reconConfig->SetDoubleAttribute("FanAnglesAutoDetectBrightnessThreshold", this->FanAngleDetector->GetBrightnessThreshold());
      reconConfig->SetIntAttribute("FanAnglesAutoDetectFilterRadiusPixel", this->FanAngleDetector->GetFilterRadiusPixel());
    }
  }
  else
  {
    XML_REMOVE_ATTRIBUTE("FanAnglesDeg", reconConfig);
    XML_REMOVE_ATTRIBUTE("EnableFanAnglesAutoDetect", reconConfig);
    XML_REMOVE_ATTRIBUTE("FanOriginPixel", reconConfig);
    XML_REMOVE_ATTRIBUTE("FanRadiusStartPixel", reconConfig);
    XML_REMOVE_ATTRIBUTE("FanRadiusStopPixel", reconConfig);
  }

  // reconstruction options
  reconConfig->SetAttribute("Interpolation", this->Reconstructor->GetInterpolationModeAsString(this->Reconstructor->GetInterpolationMode()));
  reconConfig->SetAttribute("Optimization", this->Reconstructor->GetOptimizationModeAsString(this->Reconstructor->GetOptimization()));
  reconConfig->SetAttribute("CompoundingMode", this->Reconstructor->GetCompoundingModeAsString(this->Reconstructor->GetCompoundingMode()));

  if (this->Reconstructor->GetNumberOfThreads() > 0)
  {
    reconConfig->SetIntAttribute("NumberOfThreads", this->Reconstructor->GetNumberOfThreads());
  }
  else
  {
    XML_REMOVE_ATTRIBUTE("NumberOfThreads", reconConfig);
  }

  XML_WRITE_STRING_ATTRIBUTE_REMOVE_IF_EMPTY(ImportanceMaskFilename, reconConfig);

  if (this->Reconstructor->IsPixelRejectionEnabled())
  {
    reconConfig->SetDoubleAttribute("PixelRejectionThreshold", this->GetPixelRejectionThreshold());
  }
  else
  {
    XML_REMOVE_ATTRIBUTE("PixelRejectionThreshold", reconConfig);
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::AddImageToExtent(vtkImageData* image, vtkMatrix4x4* imageToReference, double* extent_Ref)
{
  // Output volume is in the Reference coordinate system.

  // Prepare the four corner points of the input US image.
  int* frameExtent = image->GetExtent();
  std::vector< double* > corners_ImagePix;
  double minX = frameExtent[0];
  double maxX = frameExtent[1];
  double minY = frameExtent[2];
  double maxY = frameExtent[3];
  if (this->Reconstructor->GetClipRectangleSize()[0] > 0 && this->Reconstructor->GetClipRectangleSize()[1] > 0)
  {
    // Clipping rectangle is specified
    minX = std::max<double>(minX, this->Reconstructor->GetClipRectangleOrigin()[0]);
    maxX = std::min<double>(maxX, this->Reconstructor->GetClipRectangleOrigin()[0] + this->Reconstructor->GetClipRectangleSize()[0]);
    minY = std::max<double>(minY, this->Reconstructor->GetClipRectangleOrigin()[1]);
    maxY = std::min<double>(maxY, this->Reconstructor->GetClipRectangleOrigin()[1] + this->Reconstructor->GetClipRectangleSize()[1]);
  }
  double c0[ 4 ] = { minX, minY, 0,  1 };
  double c1[ 4 ] = { minX, maxY, 0,  1 };
  double c2[ 4 ] = { maxX, minY, 0,  1 };
  double c3[ 4 ] = { maxX, maxY, 0,  1 };
  corners_ImagePix.push_back(c0);
  corners_ImagePix.push_back(c1);
  corners_ImagePix.push_back(c2);
  corners_ImagePix.push_back(c3);

  // Transform the corners to Reference and expand the extent if needed
  for (unsigned int corner = 0; corner < corners_ImagePix.size(); ++corner)
  {
    double corner_Ref[ 4 ] = { 0, 0, 0, 1 }; // position of the corner in the Reference coordinate system
    imageToReference->MultiplyPoint(corners_ImagePix[corner], corner_Ref);

    for (int axis = 0; axis < 3; axis ++)
    {
      if (corner_Ref[axis] < extent_Ref[axis * 2])
      {
        // min extent along this coord axis has to be decreased
        extent_Ref[axis * 2] = corner_Ref[axis];
      }
      if (corner_Ref[axis] > extent_Ref[axis * 2 + 1])
      {
        // max extent along this coord axis has to be increased
        extent_Ref[axis * 2 + 1] = corner_Ref[axis];
      }
    }
  }
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::GetImageToReferenceTransformName(igsioTransformName& imageToReferenceTransformName)
{
  // image to reference transform is specified in the XML tree
  imageToReferenceTransformName = igsioTransformName(this->ImageCoordinateFrame, this->ReferenceCoordinateFrame);
  if (!imageToReferenceTransformName.IsValid())
  {
    LOG_ERROR("Failed to set ImageToReference transform name from '" << this->ImageCoordinateFrame << "' to '" << this->ReferenceCoordinateFrame << "'");
    return IGSIO_FAIL;
  }
  return IGSIO_SUCCESS;

  if (this->ImageCoordinateFrame.empty())
  {
    LOG_ERROR("Image coordinate frame name is undefined");
  }
  if (this->ReferenceCoordinateFrame.empty())
  {
    LOG_ERROR("Reference coordinate frame name is undefined");
  }
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::SetOutputExtentFromFrameList(vtkIGSIOTrackedFrameList* trackedFrameList, vtkIGSIOTransformRepository* transformRepository, std::string& errorDescription)
{
  igsioTransformName imageToReferenceTransformName;
  if (GetImageToReferenceTransformName(imageToReferenceTransformName) != IGSIO_SUCCESS)
  {
    errorDescription = "Invalid ImageToReference transform name";
    LOG_ERROR(errorDescription);
    return IGSIO_FAIL;
  }

  if (trackedFrameList == NULL)
  {
    errorDescription = "No valid frames are available";
    LOG_ERROR("Failed to set output extent from tracked frame list - input frame list is NULL");
    return IGSIO_FAIL;
  }
  if (trackedFrameList->GetNumberOfTrackedFrames() == 0)
  {
    errorDescription = "No valid frames are available";
    LOG_ERROR("Failed to set output extent from tracked frame list - input frame list is empty");
    return IGSIO_FAIL;
  }

  if (transformRepository == NULL)
  {
    errorDescription = "Error in transform repository";
    LOG_ERROR("Failed to set output extent from tracked frame list - input transform repository is NULL");
    return IGSIO_FAIL;
  }

  double extent_Ref[6] =
  {
    VTK_DOUBLE_MAX, VTK_DOUBLE_MIN,
    VTK_DOUBLE_MAX, VTK_DOUBLE_MIN,
    VTK_DOUBLE_MAX, VTK_DOUBLE_MIN
  };

  const int numberOfFrames = trackedFrameList->GetNumberOfTrackedFrames();
  int numberOfValidFrames = 0;
  for (int frameIndex = 0; frameIndex < numberOfFrames; ++frameIndex)
  {
    igsioTrackedFrame* frame = trackedFrameList->GetTrackedFrame(frameIndex);

    if (transformRepository->SetTransforms(*frame) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to update transform repository with tracked frame!");
      continue;
    }

    // Get transform
    ToolStatus status(TOOL_INVALID);
    vtkSmartPointer<vtkMatrix4x4> imageToReferenceTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    if (transformRepository->GetTransform(imageToReferenceTransformName, imageToReferenceTransformMatrix, &status) != IGSIO_SUCCESS)
    {
      std::string strImageToReferenceTransformName;
      imageToReferenceTransformName.GetTransformName(strImageToReferenceTransformName);
      LOG_WARNING("Failed to get transform '" << strImageToReferenceTransformName << "' from transform repository!");
      continue;
    }

    if (status == TOOL_OK)
    {
      numberOfValidFrames++;

      // Get image (only the frame extents will be used)
      vtkImageData* frameImage = trackedFrameList->GetTrackedFrame(frameIndex)->GetImageData()->GetImage();

      // Expand the extent_Ref to include this frame
      AddImageToExtent(frameImage, imageToReferenceTransformMatrix, extent_Ref);
    }
  }

  LOG_DEBUG("Automatic volume extent computation from frames used " << numberOfValidFrames << " out of " << numberOfFrames);
  if (numberOfValidFrames == 0)
  {
    std::string strImageToReferenceTransformName;
    imageToReferenceTransformName.GetTransformName(strImageToReferenceTransformName);
    errorDescription = "Automatic volume extent computation failed, there were no valid " + strImageToReferenceTransformName + " transform available in the whole sequence"
                       + " (probably wrong image or reference coordinate system was defined or all transforms were invalid)";
    LOG_ERROR(errorDescription);
    return IGSIO_FAIL;
  }

  // Adjust the output origin  so it falls on a multiple of voxel spacing.
  // This ensures the same voxel lattice when multiple sweeps are volume-reconstructed independently.
  // If the multi-sweep volume reconstructions are later merged, no resampling will be necessary.
  double* outputSpacing = this->Reconstructor->GetOutputSpacing();
  double outputOrigin_Ref[3] = { 0.0, 0.0, 0.0 };
  for (int d = 0; d < 3; d++)
  {
    outputOrigin_Ref[d] = std::floor(extent_Ref[d * 2] / outputSpacing[d]) * outputSpacing[d];
  }

  // Set the output extent from the current min and max values, using the user-defined image resolution.
  int outputExtent[ 6 ] = { 0, 0, 0, 0, 0, 0 };
  for (int d = 0; d < 3; d++)
  {
    // in general, this would be computed as int(std::floor((extent_Ref[d * 2] - outputOrigin_Ref[d]) / outputSpacing[d]));
    // we set outputExtent so that this will be always 0
    outputExtent[d * 2 + 1] = int(std::ceil((extent_Ref[d * 2 + 1] - outputOrigin_Ref[d]) / outputSpacing[d]));
  }

  this->Reconstructor->SetOutputScalarMode(trackedFrameList->GetTrackedFrame(0)->GetImageData()->GetImage()->GetScalarType());
  this->Reconstructor->SetOutputExtent(outputExtent);
  this->Reconstructor->SetOutputOrigin(outputOrigin_Ref);
  try
  {
    if (this->Reconstructor->ResetOutput() != IGSIO_SUCCESS) // :TODO: call this automatically
    {
      errorDescription = "Failed to initialize output volume of the reconstructor";
      LOG_ERROR(errorDescription);
      return IGSIO_FAIL;
    }
  }
  catch (std::bad_alloc& e)
  {
    cerr << e.what() << endl;
    errorDescription = "StartReconstruction failed due to out of memory. Try to reduce the size or increase spacing of the output volume.";
    LOG_ERROR(errorDescription);
    return IGSIO_FAIL;
  }

  this->Modified();

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::AddTrackedFrame(igsioTrackedFrame* frame, vtkIGSIOTransformRepository* transformRepository, bool* insertedIntoVolume/*=NULL*/)
{
  igsioTransformName imageToReferenceTransformName;
  if (GetImageToReferenceTransformName(imageToReferenceTransformName) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Invalid ImageToReference transform name");
    return IGSIO_FAIL;
  }

  if (frame == NULL)
  {
    LOG_ERROR("Failed to add tracked frame to volume - input frame is NULL");
    return IGSIO_FAIL;
  }

  if (transformRepository == NULL)
  {
    LOG_ERROR("Failed to add tracked frame to volume - input transform repository is NULL");
    return IGSIO_FAIL;
  }

  if (this->Reconstructor->GetCompoundingMode() == vtkIGSIOPasteSliceIntoVolume::IMPORTANCE_MASK_COMPOUNDING_MODE)
  {
    if (UpdateImportanceMask() == IGSIO_FAIL)
    {
      LOG_ERROR("Failed to get importance mask");
      return IGSIO_FAIL;
    }
  }

  ToolStatus status(TOOL_INVALID);
  vtkSmartPointer<vtkMatrix4x4> imageToReferenceTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  if (transformRepository->GetTransform(imageToReferenceTransformName, imageToReferenceTransformMatrix, &status) != IGSIO_SUCCESS)
  {
    std::string strImageToReferenceTransformName;
    imageToReferenceTransformName.GetTransformName(strImageToReferenceTransformName);
    LOG_ERROR("Failed to get transform '" << strImageToReferenceTransformName << "' from transform repository");
    return IGSIO_FAIL;
  }

  if (insertedIntoVolume != NULL)
  {
    *insertedIntoVolume = (status == TOOL_OK);
  }

  if (status != TOOL_OK)
  {
    // Insert only valid frame into volume
    std::string strImageToReferenceTransformName;
    imageToReferenceTransformName.GetTransformName(strImageToReferenceTransformName);
    LOG_DEBUG("Transform '" << strImageToReferenceTransformName << "' is invalid for the current frame, therefore this frame is not be inserted into the volume");
    return IGSIO_SUCCESS;
  }

  vtkImageData* frameImage = frame->GetImageData()->GetImage();
  bool isImageEmpty = false;
  UpdateFanAnglesFromImage(frameImage, isImageEmpty);
  if (isImageEmpty)
  {
    // nothing to insert, image is empty
    return IGSIO_SUCCESS;
  }

  igsioStatus insertSliceStatus = this->Reconstructor->InsertSlice(frameImage, imageToReferenceTransformMatrix);
  this->Modified();
  return insertSliceStatus;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::UpdateReconstructedVolume()
{
  // Load reconstructed volume if the algorithm configuration was modified since the last load
  //   MTime is updated whenever a new frame is added or configuration modified
  //   ReconstructedVolumeUpdatedTime is updated whenever a reconstruction was completed
  if (this->ReconstructedVolumeUpdatedTime >= this->GetMTime())
  {
    // reconstruction is already up-to-date
    return IGSIO_SUCCESS;
  }

  if (this->FillHoles)
  {
    if (this->GenerateHoleFilledVolume() != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to generate hole filled volume!");
      return IGSIO_FAIL;
    }
  }
  else
  {
    this->ReconstructedVolume->DeepCopy(this->Reconstructor->GetReconstructedVolume());
  }

  this->ReconstructedVolumeUpdatedTime = this->GetMTime();

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::GetReconstructedVolume(vtkImageData* volume)
{
  if (this->UpdateReconstructedVolume() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to load reconstructed volume");
    return IGSIO_FAIL;
  }

  volume->DeepCopy(this->ReconstructedVolume);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::GenerateHoleFilledVolume()
{
  LOG_INFO("Hole Filling has begun");
  this->HoleFiller->SetReconstructedVolume(this->Reconstructor->GetReconstructedVolume());
  this->HoleFiller->SetAccumulationBuffer(this->Reconstructor->GetAccumulationBuffer());
  this->HoleFiller->Update();
  LOG_INFO("Hole Filling has finished");

  this->ReconstructedVolume->DeepCopy(HoleFiller->GetOutput());

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::ExtractGrayLevels(vtkImageData* reconstructedVolume)
{
  if (this->UpdateReconstructedVolume() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to load reconstructed volume");
    return IGSIO_FAIL;
  }

  vtkSmartPointer<vtkImageExtractComponents> extract = vtkSmartPointer<vtkImageExtractComponents>::New();

  extract->SetComponents(0);
  extract->SetInputData(this->ReconstructedVolume);
  extract->Update();

  reconstructedVolume->DeepCopy(extract->GetOutput());

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::ExtractAccumulation(vtkImageData* accumulationBuffer)
{
  if (this->UpdateReconstructedVolume() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to load reconstructed volume");
    return IGSIO_FAIL;
  }

  vtkSmartPointer<vtkImageExtractComponents> extract = vtkSmartPointer<vtkImageExtractComponents>::New();

  extract->SetComponents(0);
  extract->SetInputData(this->Reconstructor->GetAccumulationBuffer());
  extract->Update();

  accumulationBuffer->DeepCopy(extract->GetOutput());

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::SaveReconstructedVolumeToFile(const std::string& filename, bool accumulation/*=false*/, bool useCompression/*=true*/)
{
  vtkSmartPointer<vtkImageData> volumeToSave = vtkSmartPointer<vtkImageData>::New();
  if (accumulation)
  {
    if (this->ExtractAccumulation(volumeToSave) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Extracting accumulation buffer failed!");
      return IGSIO_FAIL;
    }
  }
  else
  {
    if (this->ExtractGrayLevels(volumeToSave) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Extracting gray channel failed!");
      return IGSIO_FAIL;
    }
  }
  return vtkIGSIOVolumeReconstructor::SaveReconstructedVolumeToFile(volumeToSave, filename, useCompression);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::SaveReconstructedVolumeToFile(vtkImageData* volumeToSave, const std::string& filename, bool useCompression/*=true*/)
{
  if (volumeToSave == NULL)
  {
    LOG_ERROR("vtkIGSIOVolumeReconstructor::SaveReconstructedVolumeToMetafile: invalid input image");
    return IGSIO_FAIL;
  }

  int dims[3];
  volumeToSave->GetDimensions(dims);
  FrameSizeType frameSize = { static_cast<unsigned int>(dims[0]), static_cast<unsigned int>(dims[1]), static_cast<unsigned int>(dims[2]) };

  vtkNew<vtkIGSIOTrackedFrameList> list;
  igsioTrackedFrame frame;
  igsioVideoFrame image;
  image.AllocateFrame(frameSize, volumeToSave->GetScalarType(), volumeToSave->GetNumberOfScalarComponents());
  image.GetImage()->DeepCopy(volumeToSave);
  image.SetImageOrientation(US_IMG_ORIENT_MFA);
  image.SetImageType(US_IMG_BRIGHTNESS);
  frame.SetImageData(image);
  list->AddTrackedFrame(&frame);
  if (vtkIGSIOSequenceIO::Write(filename, list.GetPointer(), US_IMG_ORIENT_MF, useCompression) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to save reconstructed volume in sequence metafile!");
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
int* vtkIGSIOVolumeReconstructor::GetClipRectangleOrigin()
{
  return this->Reconstructor->GetClipRectangleOrigin();
}

//----------------------------------------------------------------------------
int* vtkIGSIOVolumeReconstructor::GetClipRectangleSize()
{
  return this->Reconstructor->GetClipRectangleSize();
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::Reset()
{
  this->Reconstructor->ResetOutput();
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::UpdateFanAnglesFromImage(vtkImageData* frameImage, bool& isImageEmpty)
{
  isImageEmpty = false;
  if (this->EnableFanAnglesAutoDetect)
  {
    this->FanAngleDetector->SetMaxFanAnglesDeg(this->FanAnglesDeg);
    this->FanAngleDetector->SetImage(frameImage);
    this->FanAngleDetector->Update();
    if (this->FanAngleDetector->GetIsFrameEmpty())
    {
      // no image content is found
      LOG_TRACE("No image data is detected in the current frame, the frame is skipped");
      this->Reconstructor->SetFanAnglesDeg(this->FanAnglesDeg); // to make sure fan enable/disable is computed correctly
      isImageEmpty = true;
      return;
    }
    this->Reconstructor->SetFanAnglesDeg(this->FanAngleDetector->GetDetectedFanAnglesDeg());
  }
  else
  {
    this->Reconstructor->SetFanAnglesDeg(this->FanAnglesDeg);
  }
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetOutputOrigin(double* origin)
{
  this->Reconstructor->SetOutputOrigin(origin);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetOutputSpacing(double* spacing)
{
  this->Reconstructor->SetOutputSpacing(spacing);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetOutputExtent(int* extent)
{
  this->Reconstructor->SetOutputExtent(extent);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetNumberOfThreads(int numberOfThreads)
{
  this->Reconstructor->SetNumberOfThreads(numberOfThreads);
  this->HoleFiller->SetNumberOfThreads(numberOfThreads);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetClipRectangleOrigin(int* origin)
{
  this->Reconstructor->SetClipRectangleOrigin(origin);
  this->FanAngleDetector->SetClipRectangleOrigin(origin);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetClipRectangleSize(int* size)
{
  this->Reconstructor->SetClipRectangleSize(size);
  this->FanAngleDetector->SetClipRectangleSize(size);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetFanAnglesDeg(double* anglesDeg)
{
  this->FanAnglesDeg[0] = anglesDeg[0];
  this->FanAnglesDeg[1] = anglesDeg[1];
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetFanAngles(double* anglesDeg)
{
  LOG_WARNING("FanAngles volume reconstructor parameter is deprecated. Use FanAnglesDeg instead (with the same value).");
  this->Reconstructor->SetFanAnglesDeg(anglesDeg);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetFanOriginPixel(double* originPixel)
{
  // Image coordinate system has unit spacing, so we can set the pixel values directly
  this->Reconstructor->SetFanOrigin(originPixel);
  this->FanAngleDetector->SetFanOrigin(originPixel);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetFanOrigin(double* originPixel)
{
  LOG_WARNING("FanOrigin volume reconstructor parameter is deprecated. Use FanOriginPixels instead (with the same value).");
  SetFanOriginPixel(originPixel);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetFanRadiusStartPixel(double radiusPixel)
{
  // Image coordinate system has unit spacing, so we can set the pixel values directly
  this->Reconstructor->SetFanRadiusStart(radiusPixel);
  this->FanAngleDetector->SetFanRadiusStart(radiusPixel);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetFanRadiusStopPixel(double radiusPixel)
{
  // Image coordinate system has unit spacing, so we can set the pixel values directly
  this->Reconstructor->SetFanRadiusStop(radiusPixel);
  this->FanAngleDetector->SetFanRadiusStop(radiusPixel);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetFanDepth(double fanDepthPixel)
{
  LOG_WARNING("FanDepth volume reconstructor parameter is deprecated. Use FanRadiusStopPixels instead (with the same value).");
  SetFanRadiusStopPixel(fanDepthPixel);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetInterpolation(vtkIGSIOPasteSliceIntoVolume::InterpolationType interpolation)
{
  this->Reconstructor->SetInterpolationMode(interpolation);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetCompoundingMode(vtkIGSIOPasteSliceIntoVolume::CompoundingType compoundingMode)
{
  this->Reconstructor->SetCompoundingMode(compoundingMode);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetOptimization(vtkIGSIOPasteSliceIntoVolume::OptimizationType optimization)
{
  this->Reconstructor->SetOptimization(optimization);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetCalculation(vtkIGSIOPasteSliceIntoVolume::CalculationTypeDeprecated type)
{
  this->Reconstructor->SetCalculation(type);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetCompounding(int comp)
{
  this->Reconstructor->SetCompounding(comp);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetPixelRejectionThreshold(double threshold)
{
  this->Reconstructor->SetPixelRejectionThreshold(threshold);
}

//----------------------------------------------------------------------------
double vtkIGSIOVolumeReconstructor::GetPixelRejectionThreshold()
{
  return this->Reconstructor->GetPixelRejectionThreshold();
}

//----------------------------------------------------------------------------
bool vtkIGSIOVolumeReconstructor::FanClippingApplied()
{
  return this->Reconstructor->FanClippingApplied();
}

//----------------------------------------------------------------------------
double* vtkIGSIOVolumeReconstructor::GetFanOrigin()
{
  return this->Reconstructor->GetFanOrigin();
}

//----------------------------------------------------------------------------
double* vtkIGSIOVolumeReconstructor::GetFanAnglesDeg()
{
  return this->FanAnglesDeg;
}

//----------------------------------------------------------------------------
double* vtkIGSIOVolumeReconstructor::GetDetectedFanAnglesDeg()
{
  return this->Reconstructor->GetFanAnglesDeg();
}

//----------------------------------------------------------------------------
double vtkIGSIOVolumeReconstructor::GetFanRadiusStartPixel()
{
  return this->Reconstructor->GetFanRadiusStart();
}

//----------------------------------------------------------------------------
double vtkIGSIOVolumeReconstructor::GetFanRadiusStopPixel()
{
  return this->Reconstructor->GetFanRadiusStop();
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetFanAnglesAutoDetectBrightnessThreshold(double threshold)
{
  this->FanAngleDetector->SetBrightnessThreshold(threshold);
}

//----------------------------------------------------------------------------
void vtkIGSIOVolumeReconstructor::SetFanAnglesAutoDetectFilterRadiusPixel(int radiusPixel)
{
  this->FanAngleDetector->SetFilterRadiusPixel(radiusPixel);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOVolumeReconstructor::UpdateImportanceMask()
{
  if (this->ImportanceMaskFilename == this->ImportanceMaskFilenameInReconstructor)
  {
    // no change
    return IGSIO_SUCCESS;
  }
  this->ImportanceMaskFilenameInReconstructor = this->ImportanceMaskFilename;

  if (!this->ImportanceMaskFilename.empty())
  {
    std::string importanceMaskFilePath;
    if (igsioCommon::FindImagePath(this->ImportanceMaskFilename, importanceMaskFilePath) == IGSIO_FAIL) //TODO
    {
      LOG_ERROR("Cannot get importance mask from file: " << this->ImportanceMaskFilename);
      return IGSIO_FAIL;
    }
    vtkSmartPointer<vtkPNGReader> reader = vtkSmartPointer<vtkPNGReader>::New();
    reader->SetFileName(importanceMaskFilePath.c_str());
    reader->Update();
    if (reader->GetOutput() == NULL)
    {
      LOG_ERROR("Failed to read importance image from file: " << importanceMaskFilePath);
      return IGSIO_FAIL;
    }
    vtkSmartPointer<vtkImageFlip> flipYFilter = vtkSmartPointer<vtkImageFlip>::New();
    flipYFilter->SetFilteredAxis(1); // flip y axis
    flipYFilter->SetInputConnection(reader->GetOutputPort());
    flipYFilter->Update();
    this->Reconstructor->SetImportanceMask(flipYFilter->GetOutput());
  }
  else
  {
    this->Reconstructor->SetImportanceMask(NULL);
  }
  return IGSIO_SUCCESS;
}

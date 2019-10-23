/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

// Local includes
//#include "PlusConfigure.h"
#include "igsioVideoFrame.h"

// VTK includes
#include <vtkBMPReader.h>
#include <vtkExtractVOI.h>
#include <vtkImageData.h>
#include <vtkImageImport.h>
#include <vtkImageReader.h>
#include <vtkObjectFactory.h>
#include <vtkPNMReader.h>
#include <vtkSmartPointer.h>
#include <vtkTIFFReader.h>
#include <vtkTrivialProducer.h>
#include <vtkUnsignedCharArray.h>

// STL includes
#include <algorithm>
#include <string>

// vtkAddon includes
#include <vtkStreamingVolumeCodec.h>
#include <vtkStreamingVolumeFrame.h>
#include <vtkStreamingVolumeCodecFactory.h>

//----------------------------------------------------------------------------

namespace
{
  //----------------------------------------------------------------------------
  template<class ScalarType>
  igsioStatus FlipClipImageGeneric(vtkImageData* inputImage, const igsioVideoFrame::FlipInfoType& flipInfo, const std::array<int, 3>& clipRectangleOrigin, const std::array<int, 3>& clipRectangleSize, vtkImageData* outputImage)
  {
    const int numberOfScalarComponents = inputImage->GetNumberOfScalarComponents();

    int dims[3] = {0, 0, 0};
    inputImage->GetDimensions(dims);

    int localClipRectangleSize[3] = {clipRectangleSize[0], clipRectangleSize[1], clipRectangleSize[2]};

    int inputWidth(dims[0]);
    int inputHeight(dims[1]);
    // TODO : when flipping of 3D data is implemented, inputDepth will be needed
    //int inputDepth( dims[2] );

    int outputDims[3] = {0, 0, 0};
    outputImage->GetDimensions(outputDims);
    int outputWidth(outputDims[0]);
    int outputHeight(outputDims[1]);
    int outputDepth(outputDims[2]);

    void* inBuff = inputImage->GetScalarPointer();
    void* outBuff = outputImage->GetScalarPointer();

    vtkIdType pixelIncrement(0);
    vtkIdType inputRowIncrement(0);
    vtkIdType inputImageIncrement(0);
    inputImage->GetIncrements(pixelIncrement, inputRowIncrement, inputImageIncrement);

    vtkIdType outputRowIncrement(0);
    vtkIdType outputImageIncrement(0);
    outputImage->GetIncrements(pixelIncrement, outputRowIncrement, outputImageIncrement);

    if (flipInfo.doubleRow)
    {
      if (inputHeight % 2 != 0)
      {
        LOG_ERROR("Cannot flip image with pairs of rows kept together, as number of rows is odd (" << inputHeight << ")");
        return IGSIO_FAIL;
      }
      // TODO : I don't think this is correct if transposition is happening... double check
      inputWidth *= 2;
      outputWidth *= 2;

      inputHeight /= 2;
      outputHeight /= 2;

      localClipRectangleSize[0] *= 2;
      localClipRectangleSize[1] /= 2;

      inputRowIncrement *= 2;
      outputRowIncrement *= 2;
    }
    if (flipInfo.doubleColumn)
    {
      if (inputWidth % 2 != 0)
      {
        LOG_ERROR("Cannot flip image with pairs of columns kept together, as number of columns is odd (" << inputWidth << ")");
        return IGSIO_FAIL;
      }
    }

    if (!flipInfo.hFlip && flipInfo.vFlip && !flipInfo.eFlip && flipInfo.tranpose == igsioVideoFrame::TRANSPOSE_NONE)
    {
      // flip Y
      for (int z = 0; z < outputDepth; z++)
      {
        // Set the input position to be the first unclipped pixel in the input image
        ScalarType* inputPixel = (ScalarType*)inBuff + (clipRectangleOrigin[2] + z) * inputImageIncrement + clipRectangleOrigin[1] * inputRowIncrement + clipRectangleOrigin[0] * pixelIncrement;

        // Set the target position pointer to the first pixel of the last row of each output image
        ScalarType* outputPixel = (ScalarType*)outBuff + outputImageIncrement * z + (outputHeight - 1) * outputRowIncrement;
        // Copy the image row-by-row, reversing the row order
        for (int y = 0; y < outputHeight; y++)
        {
          memcpy(outputPixel, inputPixel, outputWidth * pixelIncrement);
          inputPixel += inputRowIncrement;
          outputPixel -= outputRowIncrement;
        }
      }
    }
    else if (flipInfo.hFlip && !flipInfo.vFlip && !flipInfo.eFlip && flipInfo.tranpose == igsioVideoFrame::TRANSPOSE_NONE)
    {
      // flip X
      if (flipInfo.doubleColumn)
      {
        // flip X double column
        for (int z = 0; z < outputDepth; z++)
        {
          // Set the input position to be the first unclipped pixel in the input image
          ScalarType* inputPixel = (ScalarType*)inBuff + (clipRectangleOrigin[2] + z) * inputImageIncrement + clipRectangleOrigin[1] * inputRowIncrement + clipRectangleOrigin[0] * 2 * pixelIncrement;

          // Set the target position pointer to the last pixel of the first row of each output image
          ScalarType* outputPixel = (ScalarType*)outBuff + outputImageIncrement * z + outputWidth * 2 * pixelIncrement - 2 * pixelIncrement;
          // Copy the image row-by-row, reversing the pixel order in each row
          for (int y = 0; y < outputHeight; y++)
          {
            for (int x = 0; x < outputWidth; x++)
            {
              // For each scalar, copy it
              for (int s = 0; s < numberOfScalarComponents; ++s)
              {
                *(outputPixel - 1 * numberOfScalarComponents + s) = *(inputPixel + s);
                *(outputPixel + s) = *(inputPixel + 1 * numberOfScalarComponents + s);
              }
              inputPixel += 2 * pixelIncrement;
              outputPixel -= 2 * pixelIncrement;
            }
            // bring the output back to the end of the next row
            outputPixel += 2 * outputRowIncrement;
            // wrap the input to the beginning of the next clipped row
            inputPixel += (inputWidth - localClipRectangleSize[0]) * 2 * pixelIncrement;
          }
        }
      }
      else
      {
        // flip X single column
        for (int z = 0; z < outputDepth; z++)
        {
          // Set the input position to be the first unclipped pixel in the input image
          ScalarType* inputPixel = (ScalarType*)inBuff + (clipRectangleOrigin[2] + z) * inputImageIncrement + clipRectangleOrigin[1] * inputRowIncrement + clipRectangleOrigin[0] * pixelIncrement;

          // Set the target position pointer to the last pixel of the first row of each output image
          ScalarType* outputPixel = (ScalarType*)outBuff + z * outputImageIncrement + (outputWidth - 1) * pixelIncrement;
          // Copy the image row-by-row, reversing the pixel order in each row
          for (int y = 0; y < outputHeight; y++)
          {
            for (int x = 0; x < outputWidth; x++)
            {
              // For each scalar, copy it
              for (int s = 0; s < numberOfScalarComponents; ++s)
              {
                *(outputPixel + s) = *(inputPixel + s);
              }
              inputPixel += pixelIncrement;
              outputPixel -= pixelIncrement;
            }
            // bring the output back to the end of the next row
            outputPixel += 2 * outputRowIncrement;
            // wrap the input to the beginning of the next clipped row
            inputPixel += (inputWidth - localClipRectangleSize[0]) * pixelIncrement;
          }
        }
      }
    }
    else if (flipInfo.hFlip && flipInfo.vFlip && !flipInfo.eFlip && flipInfo.tranpose == igsioVideoFrame::TRANSPOSE_NONE)
    {
      // flip X and Y
      if (flipInfo.doubleColumn)
      {
        // flip X and Y double column
        for (int z = 0; z < outputDepth; z++)
        {
          // Set the input position to be the first unclipped pixel in the input image
          ScalarType* inputPixel = (ScalarType*)inBuff + (clipRectangleOrigin[2] + z) * inputImageIncrement + clipRectangleOrigin[1] * inputRowIncrement + clipRectangleOrigin[0] * 2 * pixelIncrement;

          // Set the target position pointer to the last pixel of each output image
          ScalarType* outputPixel = (ScalarType*)outBuff + outputImageIncrement * (z + 1) - 2 * pixelIncrement;
          // Copy the image pixel-by-pixel, reversing the pixel order
          for (int y = 0; y < outputHeight; y++)
          {
            for (int x = 0; x < outputWidth; x++)
            {
              // For each scalar, copy it
              for (int s = 0; s < numberOfScalarComponents; ++s)
              {
                *(outputPixel + s) = *(inputPixel + s);
              }
              inputPixel += 2 * pixelIncrement;
              outputPixel -= 2 * pixelIncrement;
            }
            // wrap the input to the beginning of the next clipped row
            inputPixel += (inputWidth - localClipRectangleSize[0]) * 2 * pixelIncrement;
          }
        }
      }
      else
      {
        // flip X and Y single column
        for (int z = 0; z < outputDepth; z++)
        {
          // Set the input position to be the first unclipped pixel in the input image
          ScalarType* inputPixel = (ScalarType*)inBuff + (clipRectangleOrigin[2] + z) * inputImageIncrement + clipRectangleOrigin[1] * inputRowIncrement + clipRectangleOrigin[0] * pixelIncrement;

          // Set the target position pointer to the last pixel of each output image
          ScalarType* outputPixel = (ScalarType*)outBuff + outputImageIncrement * (z + 1) - 1 * pixelIncrement;
          // Copy the image pixel-by-pixel, reversing the pixel order
          for (int y = 0; y < outputHeight; y++)
          {
            for (int x = 0; x < outputWidth; x++)
            {
              // For each scalar, copy it
              for (int s = 0; s < numberOfScalarComponents; ++s)
              {
                *(outputPixel + s) = *(inputPixel + s);
              }
              inputPixel += pixelIncrement;
              outputPixel -= pixelIncrement;
            }
            // wrap the input to the beginning of the next clipped row
            inputPixel += (inputWidth - localClipRectangleSize[0]) * pixelIncrement;
          }
        }
      }
    }
    else if (!flipInfo.hFlip && !flipInfo.vFlip && flipInfo.eFlip && flipInfo.tranpose == igsioVideoFrame::TRANSPOSE_NONE)
    {
      // flip Z

      // Set the input position to the first unclipped pixel
      ScalarType* inputPixel = (ScalarType*)inBuff + clipRectangleOrigin[2] * inputImageIncrement + clipRectangleOrigin[1] * inputRowIncrement + clipRectangleOrigin[0] * pixelIncrement;

      for (int z = outputDepth - 1; z >= 0; z--)
      {
        // Set the target position pointer to the first pixel of the each image
        ScalarType* outputPixel = (ScalarType*)outBuff + z * outputImageIncrement;

        // Copy the image row-by-row
        for (int y = 0; y < outputHeight; y++)
        {
          memcpy(outputPixel, inputPixel, outputRowIncrement);
          inputPixel += inputRowIncrement;
          outputPixel += outputRowIncrement;
        }
      }
    }
    else if (!flipInfo.hFlip && !flipInfo.vFlip && !flipInfo.eFlip && flipInfo.tranpose == igsioVideoFrame::TRANSPOSE_IJKtoKIJ)
    {
      // Transpose an image in KIJ layout to IJK layout
      if (flipInfo.doubleColumn)
      {
        // Transpose an image in KIJ layout to IJK layout double column

        // Set the input position to the first unclipped pixel
        ScalarType* inputPixel = (ScalarType*)inBuff + clipRectangleOrigin[2] * inputImageIncrement + clipRectangleOrigin[1] * inputRowIncrement + clipRectangleOrigin[0] * 2 * pixelIncrement;

        // Copy the image column->row, each column from the next image
        for (int z = 0; z < outputWidth; ++z)
        {
          for (int y = 0; y < outputDepth; ++y)
          {
            for (int x = 0; x < outputHeight; ++x)
            {
              // Set the target position pointer to the correct output pixel offset
              // ZXY -> XYZ
              ScalarType* outputPixel = (ScalarType*)(outBuff) + + z * 2 * pixelIncrement + x * outputRowIncrement + y * outputImageIncrement;

              // For each scalar, copy it
              for (int s = 0; s < numberOfScalarComponents; ++s)
              {
                *(outputPixel + s) = *(inputPixel + s);
              }
              inputPixel += 2 * pixelIncrement;
            }
            // wrap the input to the beginning of the next row's unclipped pixel
            inputPixel += (inputWidth - localClipRectangleSize[0]) * 2 * pixelIncrement;
          }
          // wrap the input to the beginning of the next image's unclipped row
          inputPixel += (inputHeight - localClipRectangleSize[1]) * inputRowIncrement;
        }
      }
      else
      {
        // Transpose an image IJK to KIJ
        // Set the input position to the first unclipped pixel
        ScalarType* inputPixel = (ScalarType*)inBuff + clipRectangleOrigin[2] * inputImageIncrement + clipRectangleOrigin[1] * inputRowIncrement + clipRectangleOrigin[0] * pixelIncrement;

        for (int z = 0; z < outputWidth; ++z)
        {
          for (int y = 0; y < outputDepth; ++y)
          {
            for (int x = 0; x < outputHeight; ++x)
            {
              // Set the target position pointer to the correct output pixel offset
              // X,Y,Z -> Z,X,Y
              ScalarType* outputPixel = (ScalarType*)(outBuff) + z * pixelIncrement + x * outputRowIncrement + y * outputImageIncrement;

              // For each scalar, copy it
              for (int s = 0; s < numberOfScalarComponents; ++s)
              {
                *(outputPixel + s) = *(inputPixel + s);
              }
              inputPixel += pixelIncrement;
            }
            // wrap the input to the beginning of the next row's unclipped pixel
            inputPixel += (inputWidth - localClipRectangleSize[0]) * pixelIncrement;
          }
          // wrap the input to the beginning of the next image's unclipped row
          inputPixel += (inputHeight - localClipRectangleSize[1]) * inputRowIncrement;
        }
      }
    }
    else
    {
      LOG_ERROR("Operation not permitted. " << std::endl << "flipInfo.hFlip: " << (flipInfo.hFlip ? "TRUE" : "FALSE") << std::endl <<
                "flipInfo.vFlip: " << (flipInfo.vFlip ? "TRUE" : "FALSE") << std::endl <<
                "flipInfo.eFlip: " << (flipInfo.eFlip ? "TRUE" : "FALSE") << std::endl <<
                "flipInfo.tranpose: " << igsioVideoFrame::TransposeToString(flipInfo.tranpose) << std::endl <<
                "flipInfo.doubleColumn: " << (flipInfo.doubleColumn ? "TRUE" : "FALSE") << std::endl <<
                "flipInfo.doubleRow: " << (flipInfo.doubleRow ? "TRUE" : "FALSE"));
    }

    return IGSIO_SUCCESS;
  }
}

//----------------------------------------------------------------------------
igsioVideoFrame::igsioVideoFrame()
  : Image(NULL)
  , EncodedFrame(NULL)
  , ImageType(US_IMG_BRIGHTNESS)
  , ImageOrientation(US_IMG_ORIENT_MF)
{
}

//----------------------------------------------------------------------------
igsioVideoFrame::igsioVideoFrame(const igsioVideoFrame& videoItem)
  : Image(NULL)
  , EncodedFrame(NULL)
  , ImageType(US_IMG_BRIGHTNESS)
  , ImageOrientation(US_IMG_ORIENT_MF)
{
  *this = videoItem;
}

//----------------------------------------------------------------------------
igsioVideoFrame::~igsioVideoFrame()
{
  DELETE_IF_NOT_NULL(this->Image);
}

//----------------------------------------------------------------------------
igsioVideoFrame& igsioVideoFrame::operator=(igsioVideoFrame const& videoItem)
{
  // Handle self-assignment
  if (this == &videoItem)
  {
    return *this;
  }

  this->ImageType = videoItem.ImageType;
  this->ImageOrientation = videoItem.ImageOrientation;

  // Copy the pixels. Don't use image duplicator, because that wouldn't reuse the existing buffer
  if (videoItem.GetFrameSizeInBytes() > 0)
  {
    FrameSizeType frameSize = {0, 0, 0};
    videoItem.GetFrameSize(frameSize);

    unsigned int numberOfScalarComponents(1);
    if (videoItem.GetNumberOfScalarComponents(numberOfScalarComponents) == IGSIO_FAIL)
    {
      LOG_ERROR("Unable to retrieve number of scalar components.");
    }

    if (!videoItem.IsFrameEncoded())
    {
      if (this->AllocateFrame(frameSize, videoItem.GetVTKScalarPixelType(), numberOfScalarComponents) != IGSIO_SUCCESS)
      {
        LOG_ERROR("Failed to allocate memory for the new frame in the buffer!");
      }
      else
      {
        this->Image->DeepCopy(videoItem.Image);
      }
    }
  }
  this->SetEncodedFrame(videoItem.GetEncodedFrame());

  return *this;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::DeepCopy(igsioVideoFrame* videoItem)
{
  if (videoItem == NULL)
  {
    LOG_ERROR("Failed to deep copy video buffer item - buffer item NULL!");
    return IGSIO_FAIL;
  }

  (*this) = (*videoItem);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::FillBlank()
{
  if (!this->IsImageValid())
  {
    LOG_ERROR("Unable to fill image to blank, image data is NULL.");
    return IGSIO_FAIL;
  }

  memset(this->GetScalarPointer(), 0, this->GetFrameSizeInBytes());

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::AllocateFrame(vtkImageData* image, const FrameSizeType& imageSize, igsioCommon::VTKScalarPixelType pixType, unsigned int numberOfScalarComponents)
{
  if (imageSize[0] > 0 && imageSize[1] > 0 && imageSize[2] == 0)
  {
    LOG_WARNING("Single slice images should have a dimension of z=1");
  }

  if (image != NULL)
  {
    int imageExtents[6] = { 0, 0, 0, 0, 0, 0 };
    image->GetExtent(imageExtents);
    if (imageSize[0] == imageExtents[1] - imageExtents[0] + 1 &&
        imageSize[1] == imageExtents[3] - imageExtents[2] + 1 &&
        imageSize[2] == imageExtents[5] - imageExtents[4] + 1 &&
        image->GetScalarType() == pixType &&
        image->GetNumberOfScalarComponents() == numberOfScalarComponents)
    {
      // already allocated, no change
      return IGSIO_SUCCESS;
    }
  }

  image->SetExtent(0, imageSize[0] - 1, 0, imageSize[1] - 1, 0, imageSize[2] - 1);
  image->AllocateScalars(pixType, numberOfScalarComponents);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::AllocateFrame(const FrameSizeType& imageSize, igsioCommon::VTKScalarPixelType pixType, unsigned int numberOfScalarComponents)
{
  if (this->GetImage() == NULL)
  {
    this->SetImageData(vtkImageData::New());
  }
  igsioStatus allocStatus = igsioVideoFrame::AllocateFrame(this->GetImage(), imageSize, pixType, numberOfScalarComponents);
  return allocStatus;
}

//----------------------------------------------------------------------------
unsigned long igsioVideoFrame::GetFrameSizeInBytes() const
{
  if (!this->IsImageValid())
  {
    return 0;
  }
  FrameSizeType frameSize = {0, 0, 0};
  this->GetFrameSize(frameSize);

  int bytesPerScalar = GetNumberOfBytesPerScalar();
  if (bytesPerScalar != 1 && bytesPerScalar != 2 && bytesPerScalar != 4 && bytesPerScalar != 8)
  {
    LOG_ERROR("Unsupported scalar size: " << bytesPerScalar << " bytes/scalar component");
  }
  unsigned int numberOfScalarComponents(1);
  if (this->GetNumberOfScalarComponents(numberOfScalarComponents) == IGSIO_FAIL)
  {
    LOG_ERROR("Unable to retrieve number of scalar components.");
    return 0;
  }
  unsigned long frameSizeInBytes = frameSize[0] * frameSize[1] * frameSize[2] * bytesPerScalar * numberOfScalarComponents;
  return frameSizeInBytes;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::DeepCopyFrom(vtkImageData* frame)
{
  if (frame == NULL)
  {
    LOG_ERROR("Failed to deep copy from vtk image data - input frame is NULL!");
    return IGSIO_FAIL;
  }

  int* frameExtent = frame->GetExtent();
  if ((frameExtent[1] - frameExtent[0] + 1) < 0 || (frameExtent[3] - frameExtent[2] + 1) < 0 || (frameExtent[5] - frameExtent[4] + 1) < 0)
  {
    LOG_ERROR("Negative frame extents. Cannot DeepCopy frame.");
    return IGSIO_FAIL;
  }
  FrameSizeType frameSize = {static_cast<unsigned int>(frameExtent[1] - frameExtent[0] + 1), static_cast<unsigned int>(frameExtent[3] - frameExtent[2] + 1), static_cast<unsigned int>(frameExtent[5] - frameExtent[4] + 1) };

  if (this->AllocateFrame(frameSize, frame->GetScalarType(), frame->GetNumberOfScalarComponents()) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to allocate memory for plus video frame!");
    return IGSIO_FAIL;
  }

  memcpy(this->Image->GetScalarPointer(), frame->GetScalarPointer(), this->GetFrameSizeInBytes());

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::ShallowCopyFrom(vtkImageData* frame)
{
  if (frame == NULL)
  {
    LOG_ERROR("Failed to shallow copy from vtk image data - input frame is NULL!");
    return IGSIO_FAIL;
  }
  this->Image->ShallowCopy(frame);
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
int igsioVideoFrame::GetNumberOfBytesPerScalar() const
{
  return igsioVideoFrame::GetNumberOfBytesPerScalar(GetVTKScalarPixelType());
}

//----------------------------------------------------------------------------
int igsioVideoFrame::GetNumberOfBytesPerPixel() const
{
  unsigned int numberOfScalarComponents(1);
  if (this->GetNumberOfScalarComponents(numberOfScalarComponents) == IGSIO_FAIL)
  {
    LOG_ERROR("Unable to retrieve number of scalar components.");
    return -1;
  }
  return this->GetNumberOfBytesPerScalar() * numberOfScalarComponents;
}

//----------------------------------------------------------------------------
US_IMAGE_ORIENTATION igsioVideoFrame::GetImageOrientation() const
{
  return this->ImageOrientation;
}

//----------------------------------------------------------------------------
void igsioVideoFrame::SetImageOrientation(US_IMAGE_ORIENTATION imgOrientation)
{
  this->ImageOrientation = imgOrientation;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::GetNumberOfScalarComponents(unsigned int& scalarComponents) const
{
  if (IsImageValid())
  {
    if (this->Image)
    {
      scalarComponents = static_cast<unsigned int>(this->Image->GetNumberOfScalarComponents());
      return IGSIO_SUCCESS;
    }
    else if (this->EncodedFrame)
    {
      scalarComponents = static_cast<unsigned int>(this->EncodedFrame->GetNumberOfComponents());
      return IGSIO_SUCCESS;
    }
  }

  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
US_IMAGE_TYPE igsioVideoFrame::GetImageType() const
{
  return this->ImageType;
}

//----------------------------------------------------------------------------
void igsioVideoFrame::SetImageType(US_IMAGE_TYPE imgType)
{
  this->ImageType = imgType;
}

//----------------------------------------------------------------------------
void* igsioVideoFrame::GetScalarPointer() const
{
  if (!this->IsImageValid())
  {
    LOG_ERROR("Cannot get buffer pointer, the buffer hasn't been created yet");
    return NULL;
  }

  return this->Image->GetScalarPointer();
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::GetFrameSize(FrameSizeType& frameSize) const
{
  if (!this->IsImageValid())
  {
    frameSize[0] = frameSize[1] = frameSize[2] = 0;
    return IGSIO_FAIL;
  }

  int frameSizeSigned[3];
  if (this->Image)
  {
    this->Image->GetDimensions(frameSizeSigned);
  }
  else if (this->EncodedFrame)
  {
    this->EncodedFrame->GetDimensions(frameSizeSigned);
  }
  else
  {
    return IGSIO_FAIL;
  }

  if (frameSizeSigned[0] < 0)
  {
    frameSizeSigned[0] = 0;
  }
  if (frameSizeSigned[1] < 0)
  {
    frameSizeSigned[1] = 0;
  }
  if (frameSizeSigned[2] < 0)
  {
    frameSizeSigned[2] = 0;
  }
  frameSize[0] = static_cast<unsigned int>(frameSizeSigned[0]);
  frameSize[1] = static_cast<unsigned int>(frameSizeSigned[1]);
  frameSize[2] = static_cast<unsigned int>(frameSizeSigned[2]);
  return IGSIO_SUCCESS;
}


//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::GetEncodingFourCC(std::string& encoding) const
{
  if (!this->IsImageValid())
  {
    encoding = "";
    return IGSIO_FAIL;
  }

  if (this->Image)
  {
    encoding = "";
    return IGSIO_SUCCESS;
  }

  if (this->EncodedFrame)
  {
    encoding = this->EncodedFrame->GetCodecFourCC();
    return IGSIO_SUCCESS;
  }

  encoding = "";
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
bool igsioVideoFrame::IsFrameEncoded() const
{
  return this->EncodedFrame != NULL;
}

//----------------------------------------------------------------------------
igsioCommon::VTKScalarPixelType igsioVideoFrame::GetVTKScalarPixelType() const
{
  if (this->Image == NULL || !this->IsImageValid())
  {
    return VTK_UNSIGNED_CHAR;
  }
  return this->Image->GetScalarType();
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::GetFlipAxes(US_IMAGE_ORIENTATION usImageOrientation1, US_IMAGE_TYPE usImageType1, US_IMAGE_ORIENTATION usImageOrientation2, FlipInfoType& flipInfo)
{
  flipInfo.doubleRow = false;
  flipInfo.doubleColumn = false;
  if (usImageType1 == US_IMG_RF_I_LINE_Q_LINE)
  {
    if (usImageOrientation1 == US_IMG_ORIENT_FM
        || usImageOrientation1 == US_IMG_ORIENT_FU
        || usImageOrientation1 == US_IMG_ORIENT_NM
        || usImageOrientation1 == US_IMG_ORIENT_NU)
    {
      // keep line pairs together
      flipInfo.doubleRow = true;
    }
    else
    {
      LOG_ERROR("RF scanlines are expected to be in image rows");
      return IGSIO_FAIL;
    }
  }
  else if (usImageType1 == US_IMG_RF_IQ_LINE)
  {
    if (usImageOrientation1 == US_IMG_ORIENT_FM
        || usImageOrientation1 == US_IMG_ORIENT_FU
        || usImageOrientation1 == US_IMG_ORIENT_NM
        || usImageOrientation1 == US_IMG_ORIENT_NU)
    {
      // keep IQ value pairs together
      flipInfo.doubleColumn = true;
    }
    else
    {
      LOG_ERROR("RF scanlines are expected to be in image rows");
      return IGSIO_FAIL;
    }
  }

  flipInfo.hFlip = false; // horizontal
  flipInfo.vFlip = false; // vertical
  flipInfo.eFlip = false; // elevational
  flipInfo.tranpose = TRANSPOSE_NONE; // no transpose
  if (usImageOrientation1 == US_IMG_ORIENT_XX)
  {
    LOG_ERROR("Failed to determine the necessary image flip - unknown input image orientation 1");
    return IGSIO_FAIL;
  }

  if (usImageOrientation2 == US_IMG_ORIENT_XX)
  {
    LOG_ERROR("Failed to determine the necessary image flip - unknown input image orientation 2");
    return IGSIO_SUCCESS;
  }
  if (usImageOrientation1 == usImageOrientation2)
  {
    // no flip
    return IGSIO_SUCCESS;
  }

  if ((usImageOrientation1 == US_IMG_ORIENT_UF && usImageOrientation2 == US_IMG_ORIENT_MF) ||
      (usImageOrientation1 == US_IMG_ORIENT_MF && usImageOrientation2 == US_IMG_ORIENT_UF) ||
      (usImageOrientation1 == US_IMG_ORIENT_UN && usImageOrientation2 == US_IMG_ORIENT_MN) ||
      (usImageOrientation1 == US_IMG_ORIENT_MN && usImageOrientation2 == US_IMG_ORIENT_UN) ||
      (usImageOrientation1 == US_IMG_ORIENT_FU && usImageOrientation2 == US_IMG_ORIENT_NU) ||
      (usImageOrientation1 == US_IMG_ORIENT_NU && usImageOrientation2 == US_IMG_ORIENT_FU) ||
      (usImageOrientation1 == US_IMG_ORIENT_FM && usImageOrientation2 == US_IMG_ORIENT_NM) ||
      (usImageOrientation1 == US_IMG_ORIENT_NM && usImageOrientation2 == US_IMG_ORIENT_FM)
     )
  {
    // flip x
    flipInfo.hFlip = true;
    return IGSIO_SUCCESS;
  }
  if ((usImageOrientation1 == US_IMG_ORIENT_UF && usImageOrientation2 == US_IMG_ORIENT_UN) ||
      (usImageOrientation1 == US_IMG_ORIENT_MF && usImageOrientation2 == US_IMG_ORIENT_MN) ||
      (usImageOrientation1 == US_IMG_ORIENT_UN && usImageOrientation2 == US_IMG_ORIENT_UF) ||
      (usImageOrientation1 == US_IMG_ORIENT_MN && usImageOrientation2 == US_IMG_ORIENT_MF) ||
      (usImageOrientation1 == US_IMG_ORIENT_FU && usImageOrientation2 == US_IMG_ORIENT_FM) ||
      (usImageOrientation1 == US_IMG_ORIENT_NU && usImageOrientation2 == US_IMG_ORIENT_NM) ||
      (usImageOrientation1 == US_IMG_ORIENT_FM && usImageOrientation2 == US_IMG_ORIENT_FU) ||
      (usImageOrientation1 == US_IMG_ORIENT_NM && usImageOrientation2 == US_IMG_ORIENT_NU)
     )
  {
    // flip y
    flipInfo.vFlip = true;
    return IGSIO_SUCCESS;
  }
  if ((usImageOrientation1 == US_IMG_ORIENT_UFA && usImageOrientation2 == US_IMG_ORIENT_UFD) ||
      (usImageOrientation1 == US_IMG_ORIENT_UFD && usImageOrientation2 == US_IMG_ORIENT_UFA) ||
      (usImageOrientation1 == US_IMG_ORIENT_MFA && usImageOrientation2 == US_IMG_ORIENT_MFD) ||
      (usImageOrientation1 == US_IMG_ORIENT_MFD && usImageOrientation2 == US_IMG_ORIENT_MFA) ||
      (usImageOrientation1 == US_IMG_ORIENT_UNA && usImageOrientation2 == US_IMG_ORIENT_UND) ||
      (usImageOrientation1 == US_IMG_ORIENT_UND && usImageOrientation2 == US_IMG_ORIENT_UNA) ||
      (usImageOrientation1 == US_IMG_ORIENT_MNA && usImageOrientation2 == US_IMG_ORIENT_MND) ||
      (usImageOrientation1 == US_IMG_ORIENT_MND && usImageOrientation2 == US_IMG_ORIENT_MNA)
     )
  {
    // flip z
    flipInfo.eFlip = true;
    return IGSIO_SUCCESS;
  }
  if ((usImageOrientation1 == US_IMG_ORIENT_UF && usImageOrientation2 == US_IMG_ORIENT_MN) ||
      (usImageOrientation1 == US_IMG_ORIENT_MF && usImageOrientation2 == US_IMG_ORIENT_UN) ||
      (usImageOrientation1 == US_IMG_ORIENT_UN && usImageOrientation2 == US_IMG_ORIENT_MF) ||
      (usImageOrientation1 == US_IMG_ORIENT_MN && usImageOrientation2 == US_IMG_ORIENT_UF) ||
      (usImageOrientation1 == US_IMG_ORIENT_FU && usImageOrientation2 == US_IMG_ORIENT_NM) ||
      (usImageOrientation1 == US_IMG_ORIENT_NU && usImageOrientation2 == US_IMG_ORIENT_FM) ||
      (usImageOrientation1 == US_IMG_ORIENT_FM && usImageOrientation2 == US_IMG_ORIENT_NU) ||
      (usImageOrientation1 == US_IMG_ORIENT_NM && usImageOrientation2 == US_IMG_ORIENT_FU)
     )
  {
    // flip xy
    flipInfo.hFlip = true;
    flipInfo.vFlip = true;
    return IGSIO_SUCCESS;
  }
  if ((usImageOrientation1 == US_IMG_ORIENT_UFA && usImageOrientation2 == US_IMG_ORIENT_MFD) ||
      (usImageOrientation1 == US_IMG_ORIENT_MFD && usImageOrientation2 == US_IMG_ORIENT_UFA) ||
      (usImageOrientation1 == US_IMG_ORIENT_UNA && usImageOrientation2 == US_IMG_ORIENT_MND) ||
      (usImageOrientation1 == US_IMG_ORIENT_MND && usImageOrientation2 == US_IMG_ORIENT_UNA)
     )
  {
    // flip xz
    flipInfo.hFlip = true;
    flipInfo.eFlip = true;
    return IGSIO_SUCCESS;
  }
  if (
    (usImageOrientation1 == US_IMG_ORIENT_UFA && usImageOrientation2 == US_IMG_ORIENT_UND) ||
    (usImageOrientation1 == US_IMG_ORIENT_UND && usImageOrientation2 == US_IMG_ORIENT_UFA) ||
    (usImageOrientation1 == US_IMG_ORIENT_MFA && usImageOrientation2 == US_IMG_ORIENT_MND) ||
    (usImageOrientation1 == US_IMG_ORIENT_MND && usImageOrientation2 == US_IMG_ORIENT_MFA)
  )
  {
    // flip yz
    flipInfo.vFlip = true;
    flipInfo.eFlip = true;
    return IGSIO_SUCCESS;
  }
  if (
    (usImageOrientation1 == US_IMG_ORIENT_UFA && usImageOrientation2 == US_IMG_ORIENT_MND) ||
    (usImageOrientation1 == US_IMG_ORIENT_MND && usImageOrientation2 == US_IMG_ORIENT_UFA)
  )
  {
    // flip xyz
    flipInfo.hFlip = true;
    flipInfo.vFlip = true;
    flipInfo.eFlip = true;
    return IGSIO_SUCCESS;
  }
  if (
    (usImageOrientation1 == US_IMG_ORIENT_AMF && usImageOrientation2 == US_IMG_ORIENT_MFA) ||
    (usImageOrientation1 == US_IMG_ORIENT_MFA && usImageOrientation2 == US_IMG_ORIENT_AMF)
  )
  {
    // Transpose J KI images to K IJ images
    flipInfo.tranpose = TRANSPOSE_IJKtoKIJ;
    return IGSIO_SUCCESS;
  }

  assert(0);
  LOG_ERROR("Image orientation conversion between orientations " << igsioCommon::GetStringFromUsImageOrientation(usImageOrientation1)
            << " and " << igsioCommon::GetStringFromUsImageOrientation(usImageOrientation2)
            << " is not supported. Only reordering of rows, columns and slices.");
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::GetOrientedClippedImage(vtkImageData* inUsImage,
    FlipInfoType flipInfo,
    US_IMAGE_TYPE inUsImageType,
    vtkImageData* outUsOrientedImage,
    const std::array<int, 3>& clipRectangleOrigin,
    const std::array<int, 3>& clipRectangleSize)
{
  if (inUsImage == NULL)
  {
    LOG_ERROR("Failed to convert image data to the requested orientation - input image is null!");
    return IGSIO_FAIL;
  }

  if (outUsOrientedImage == NULL)
  {
    LOG_ERROR("Failed to convert image data to the requested orientation - output image is null!");
    return IGSIO_FAIL;
  }

  return igsioVideoFrame::FlipClipImage(inUsImage, flipInfo, clipRectangleOrigin, clipRectangleSize, outUsOrientedImage);
}

//----------------------------------------------------------------------------
// vtkImageImport logs a warning if the image size is (1,1,1) because it thinks
// the whole extent is not set. Avoid the warning by specifying the extent using
// a callback
int* WholeExtentCallback_1_1_1(void*)
{
  static int defaultextent[6] = {0, 0, 0, 0, 0, 0};
  return defaultextent;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::GetOrientedClippedImage(unsigned char* imageDataPtr,
    FlipInfoType flipInfo, US_IMAGE_TYPE inUsImageType,
    igsioCommon::VTKScalarPixelType inUsImagePixelType,
    unsigned int numberOfScalarComponents,
    const FrameSizeType& inputFrameSizeInPx,
    igsioVideoFrame& outBufferItem,
    const std::array<int, 3>& clipRectangleOrigin,
    const std::array<int, 3>& clipRectangleSize)
{
  return igsioVideoFrame::GetOrientedClippedImage(imageDataPtr, flipInfo, inUsImageType, inUsImagePixelType,
         numberOfScalarComponents, inputFrameSizeInPx, outBufferItem.GetImage(), clipRectangleOrigin, clipRectangleSize);
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::GetOrientedClippedImage(unsigned char* imageDataPtr,
    FlipInfoType flipInfo,
    US_IMAGE_TYPE inUsImageType,
    igsioCommon::VTKScalarPixelType inUsImagePixelType,
    unsigned int numberOfScalarComponents,
    const FrameSizeType& inputFrameSizeInPx,
    vtkImageData* outUsOrientedImage,
    const std::array<int, 3>& clipRectangleOrigin,
    const std::array<int, 3>& clipRectangleSize)
{
  if (imageDataPtr == NULL)
  {
    LOG_ERROR("Failed to convert image data to MF orientation - input image is null!");
    return IGSIO_FAIL;
  }

  if (outUsOrientedImage == NULL)
  {
    LOG_ERROR("Failed to convert image data to MF orientation - output image is null!");
    return IGSIO_FAIL;
  }

  // Create a VTK image out of a buffer without copying the pixel data
  vtkSmartPointer<vtkImageImport> inUsImage = vtkSmartPointer<vtkImageImport>::New();
  inUsImage->SetImportVoidPointer(imageDataPtr);

  // Avoid warning if image size is (1,1,1)
  if (inputFrameSizeInPx[0] == 1 && inputFrameSizeInPx[1] == 1 && inputFrameSizeInPx[2] == 1)
  {
    inUsImage->SetWholeExtentCallback(WholeExtentCallback_1_1_1);
  }
  else
  {
    inUsImage->SetWholeExtent(0, inputFrameSizeInPx[0] - 1, 0, inputFrameSizeInPx[1] - 1, 0, inputFrameSizeInPx[2] - 1);
  }

  inUsImage->SetDataExtent(0, inputFrameSizeInPx[0] - 1, 0, inputFrameSizeInPx[1] - 1, 0, inputFrameSizeInPx[2] - 1);
  inUsImage->SetDataScalarType(outUsOrientedImage->GetScalarType());
  inUsImage->SetNumberOfScalarComponents(outUsOrientedImage->GetNumberOfScalarComponents());
  inUsImage->Update();

  igsioStatus result = igsioVideoFrame::GetOrientedClippedImage(inUsImage->GetOutput(), flipInfo, inUsImageType,
                       outUsOrientedImage, clipRectangleOrigin, clipRectangleSize);

  return result;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::FlipClipImage(vtkImageData* inUsImage,
    const igsioVideoFrame::FlipInfoType& flipInfo,
    const std::array<int, 3>& clipRectangleOrigin,
    const std::array<int, 3>& clipRectangleSize,
    vtkImageData* outUsOrientedImage)
{
  if (inUsImage == NULL)
  {
    LOG_ERROR("Failed to convert image data to the requested orientation - input image is null!");
    return IGSIO_FAIL;
  }

  if (outUsOrientedImage == NULL)
  {
    LOG_ERROR("Failed to convert image data to the requested orientation - output image is null!");
    return IGSIO_FAIL;
  }

  if (!flipInfo.hFlip && !flipInfo.vFlip && !flipInfo.eFlip && flipInfo.tranpose == TRANSPOSE_NONE)
  {
    if (igsioCommon::IsClippingRequested(clipRectangleOrigin, clipRectangleSize))
    {
      // No flip or transpose, but clipping requested, let vtk do the heavy lifting
      vtkSmartPointer<vtkExtractVOI> extract = vtkSmartPointer<vtkExtractVOI>::New();
      vtkSmartPointer<vtkTrivialProducer> tp = vtkSmartPointer<vtkTrivialProducer>::New();
      tp->SetOutput(inUsImage);
      extract->SetInputConnection(tp->GetOutputPort());
      extract->SetVOI(
        clipRectangleOrigin[0], clipRectangleOrigin[0] + clipRectangleSize[0] - 1,
        clipRectangleOrigin[1], clipRectangleOrigin[1] + clipRectangleSize[1] - 1,
        clipRectangleOrigin[2], clipRectangleOrigin[2] + clipRectangleSize[2] - 1
      );
      extract->SetOutput(outUsOrientedImage);
      extract->Update();
      return IGSIO_SUCCESS;
    }
    else
    {
      // no flip, clip or transpose
      outUsOrientedImage->DeepCopy(inUsImage);
      return IGSIO_SUCCESS;
    }
  }

  // Validate output image is correct dimensions to receive final oriented and/or clipped result
  int inputDimensions[3] = {0, 0, 0};
  inUsImage->GetDimensions(inputDimensions);
  std::array<int, 3> finalClipOrigin = {clipRectangleOrigin[0], clipRectangleOrigin[1], clipRectangleOrigin[2]};
  std::array<int, 3> finalClipSize = {clipRectangleSize[0], clipRectangleSize[1], clipRectangleSize[2]};
  int finalOutputSize[3] = {inputDimensions[0], inputDimensions[1], inputDimensions[2]};
  if (igsioCommon::IsClippingRequested(clipRectangleOrigin, clipRectangleSize))
  {
    // Clipping requested, validate that source image is bigger than requested clip size
    int inExtents[6] = {0, 0, 0, 0, 0, 0};
    inUsImage->GetExtent(inExtents);

    if (!igsioCommon::IsClippingWithinExtents(clipRectangleOrigin, clipRectangleSize, inExtents))
    {
      LOG_WARNING("Clipping information cannot fit within the original image. No clipping will be performed. Origin=[" << clipRectangleOrigin[0] << "," << clipRectangleOrigin[1] << "," << clipRectangleOrigin[2] <<
                  "]. Size=[" << clipRectangleSize[0] << "," << clipRectangleSize[1] << "," << clipRectangleSize[2] << "].");

      finalClipOrigin[0] = 0;
      finalClipOrigin[1] = 0;
      finalClipOrigin[2] = 0;
      finalClipSize[0] = inputDimensions[0];
      finalClipSize[1] = inputDimensions[1];
      finalClipSize[2] = inputDimensions[2];
      finalOutputSize[0] = inputDimensions[0];
      finalOutputSize[1] = inputDimensions[1];
      finalOutputSize[2] = inputDimensions[2];
    }
    else
    {
      // Clip parameters are good, set the final output size to be the clipped size
      finalOutputSize[0] = clipRectangleSize[0];
      finalOutputSize[1] = clipRectangleSize[1];
      finalOutputSize[2] = clipRectangleSize[2];
    }
  }
  else
  {
    finalClipOrigin[0] = 0;
    finalClipOrigin[1] = 0;
    finalClipOrigin[2] = 0;
    finalClipSize[0] = inputDimensions[0];
    finalClipSize[1] = inputDimensions[1];
    finalClipSize[2] = inputDimensions[2];
  }

  // Adjust output image dimensions to account for transposition of axes
  if (flipInfo.tranpose == TRANSPOSE_IJKtoKIJ)
  {
    int temp = finalOutputSize[0];
    finalOutputSize[0] = finalOutputSize[2];
    finalOutputSize[2] = finalOutputSize[1];
    finalOutputSize[1] = temp;
  }

  int outDimensions[3] = {0, 0, 0};
  outUsOrientedImage->GetDimensions(outDimensions);

  // Update the output image if the dimensions don't match the final clip size (which might be the same as the input image)
  if (outDimensions[0] != finalOutputSize[0] || outDimensions[1] != finalOutputSize[1] || outDimensions[2] != finalOutputSize[2] || outUsOrientedImage->GetScalarType() != inUsImage->GetScalarType() || outUsOrientedImage->GetNumberOfScalarComponents() != inUsImage->GetNumberOfScalarComponents())
  {
    // Allocate the output image, adjust for 1 based sizes to 0 based extents
    outUsOrientedImage->SetExtent(0, finalOutputSize[0] - 1, 0, finalOutputSize[1] - 1, 0, finalOutputSize[2] - 1);
    outUsOrientedImage->AllocateScalars(inUsImage->GetScalarType(), inUsImage->GetNumberOfScalarComponents());
  }

  int numberOfBytesPerScalar = igsioVideoFrame::GetNumberOfBytesPerScalar(inUsImage->GetScalarType());

  igsioStatus status(IGSIO_FAIL);
  switch (numberOfBytesPerScalar)
  {
    case 1:
      status = FlipClipImageGeneric<vtkTypeUInt8>(inUsImage, flipInfo, finalClipOrigin, finalClipSize, outUsOrientedImage);
      break;
    case 2:
      status = FlipClipImageGeneric<vtkTypeUInt16>(inUsImage, flipInfo, finalClipOrigin, finalClipSize, outUsOrientedImage);
      break;
    case 4:
      status = FlipClipImageGeneric<vtkTypeUInt32>(inUsImage, flipInfo, finalClipOrigin, finalClipSize, outUsOrientedImage);
      break;
    case 8:
      status = FlipClipImageGeneric<vtkTypeUInt64>(inUsImage, flipInfo, finalClipOrigin, finalClipSize, outUsOrientedImage);
      break;
    default:
      LOG_ERROR("Unsupported bit depth: " << numberOfBytesPerScalar << " bytes per scalar");
      break;
  }
  return status;
}

//----------------------------------------------------------------------------
igsioStatus igsioVideoFrame::ReadImageFromFile(igsioVideoFrame& frame, const char* fileName)
{
  vtkImageReader2* reader(NULL);

  std::string extension = vtksys::SystemTools::GetFilenameExtension(std::string(fileName));
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  std::vector<std::string> readerExtensions;

  if (reader == NULL)
  {
    reader = vtkTIFFReader::New();
    igsioCommon::SplitStringIntoTokens(reader->GetFileExtensions(), ' ', readerExtensions);

    if (std::find(readerExtensions.begin(), readerExtensions.end(), extension) == readerExtensions.end())
    {
      DELETE_IF_NOT_NULL(reader);
    }
  }

  if (reader == NULL)
  {
    reader = vtkBMPReader::New();
    igsioCommon::SplitStringIntoTokens(reader->GetFileExtensions(), ' ', readerExtensions);

    if (std::find(readerExtensions.begin(), readerExtensions.end(), extension) == readerExtensions.end())
    {
      DELETE_IF_NOT_NULL(reader);
    }
  }

  if (reader == NULL)
  {
    reader = vtkPNMReader::New();
    igsioCommon::SplitStringIntoTokens(reader->GetFileExtensions(), ' ', readerExtensions);

    if (std::find(readerExtensions.begin(), readerExtensions.end(), extension) == readerExtensions.end())
    {
      DELETE_IF_NOT_NULL(reader);
    }
  }

  reader->SetFileName(fileName);
  reader->Update();

  igsioStatus result = frame.DeepCopyFrom(reader->GetOutput());
  reader->Delete();
  return result;
}


//----------------------------------------------------------------------------
int igsioVideoFrame::GetNumberOfBytesPerScalar(igsioCommon::VTKScalarPixelType pixelType)
{
  switch (pixelType)
  {
    case VTK_UNSIGNED_CHAR:
      return sizeof(vtkTypeUInt8);
    case VTK_CHAR:
      return sizeof(vtkTypeInt8);
    case VTK_UNSIGNED_SHORT:
      return sizeof(vtkTypeUInt16);
    case VTK_SHORT:
      return sizeof(vtkTypeInt16);
    case VTK_UNSIGNED_INT:
      return sizeof(vtkTypeUInt32);
    case VTK_INT:
      return sizeof(vtkTypeInt32);
    case VTK_UNSIGNED_LONG:
      return sizeof(unsigned long);
    case VTK_LONG:
      return sizeof(long);
    case VTK_FLOAT:
      return sizeof(vtkTypeFloat32);
    case VTK_DOUBLE:
      return sizeof(vtkTypeFloat64);
    default:
      LOG_ERROR("GetNumberOfBytesPerPixel: unknown pixel type " << pixelType);
      return VTK_VOID;
  }
}

//----------------------------------------------------------------------------
vtkImageData* igsioVideoFrame::GetImage() const
{
  return this->Image;
}

//----------------------------------------------------------------------------
vtkStreamingVolumeFrame* igsioVideoFrame::GetEncodedFrame() const
{
  return this->EncodedFrame;
}

//----------------------------------------------------------------------------
void igsioVideoFrame::SetImageData(vtkImageData* imageData)
{
  this->Image = imageData;
}

//----------------------------------------------------------------------------
void igsioVideoFrame::SetEncodedFrame(vtkStreamingVolumeFrame* encodedFrame)
{
  this->EncodedFrame = encodedFrame;
}

//----------------------------------------------------------------------------
#define VTK_TO_STRING(pixType) case pixType: return "##pixType"
std::string igsioVideoFrame::GetStringFromVTKPixelType(igsioCommon::VTKScalarPixelType vtkScalarPixelType)
{
  switch (vtkScalarPixelType)
  {
      VTK_TO_STRING(VTK_CHAR);
      VTK_TO_STRING(VTK_UNSIGNED_CHAR);
      VTK_TO_STRING(VTK_SHORT);
      VTK_TO_STRING(VTK_UNSIGNED_SHORT);
      VTK_TO_STRING(VTK_INT);
      VTK_TO_STRING(VTK_UNSIGNED_INT);
      VTK_TO_STRING(VTK_FLOAT);
      VTK_TO_STRING(VTK_DOUBLE);
    default:
      return "Unknown";
  }
}

#undef VTK_TO_STRING

//----------------------------------------------------------------------------
std::string igsioVideoFrame::TransposeToString(TransposeType type)
{
  switch (type)
  {
    case TRANSPOSE_NONE:
      return "TRANPOSE_NONE";
    case TRANSPOSE_IJKtoKIJ:
      return "TRANSPOSE_KIJtoIJK";
    default:
      return "ERROR";
  }
}

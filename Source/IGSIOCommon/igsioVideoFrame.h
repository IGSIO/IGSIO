/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/


#ifndef __igsioVideoFrame_h
#define __igsioVideoFrame_h

// VTK includes
#include <vtkImageData.h>
#include <vtkImageExport.h>
#include <vtkSmartPointer.h>

// IGSIO includes
#include "igsioCommon.h"
#include "vtkigsiocommon_export.h"

// vtkAddon includes
#include <vtkStreamingVolumeFrame.h>

class vtkStreamingVolumeCodec;

/*!
\enum US_IMAGE_ORIENTATION
\brief Defines constant values for ultrasound image orientation
The ultrasound image axes are defined as follows:
\li x axis: points towards the x coordinate increase direction
\li y axis: points towards the y coordinate increase direction
\li z axis: points towards the z coordinate increase direction
The image orientation can be defined by specifying which transducer axis corresponds to the x, y and z image axes, respectively.
\ingroup PlusLibCommon
*/
enum US_IMAGE_ORIENTATION
{
  US_IMG_ORIENT_XX,  /*!< undefined */
  US_IMG_ORIENT_UF, /*!< image x axis = unmarked transducer axis, image y axis = far transducer axis */
  US_IMG_ORIENT_UFD = US_IMG_ORIENT_UF, /*!< image x axis = unmarked transducer axis, image y axis = far transducer axis, image z axis = descending transducer axis */
  US_IMG_ORIENT_UFA, /*!< image x axis = unmarked transducer axis, image y axis = far transducer axis, image z axis = ascending transducer axis */
  US_IMG_ORIENT_UN, /*!< image x axis = unmarked transducer axis, image y axis = near transducer axis */
  US_IMG_ORIENT_UNA = US_IMG_ORIENT_UN, /*!< image x axis = unmarked transducer axis, image y axis = near transducer axis, image z axis = ascending transducer axis */
  US_IMG_ORIENT_UND, /*!< image x axis = unmarked transducer axis, image y axis = near transducer axis, image z axis = descending transducer axis */
  US_IMG_ORIENT_MF, /*!< image x axis = marked transducer axis, image y axis = far transducer axis */
  US_IMG_ORIENT_MFA = US_IMG_ORIENT_MF, /*!< image x axis = marked transducer axis, image y axis = far transducer axis, image z axis = ascending transducer axis */
  US_IMG_ORIENT_MFD, /*!< image x axis = marked transducer axis, image y axis = far transducer axis, image z axis = descending transducer axis */
  US_IMG_ORIENT_AMF,
  US_IMG_ORIENT_MN, /*!< image x axis = marked transducer axis, image y axis = near transducer axis */
  US_IMG_ORIENT_MND = US_IMG_ORIENT_MN, /*!< image x axis = marked transducer axis, image y axis = near transducer axis, image z axis = descending transducer axis */
  US_IMG_ORIENT_MNA, /*!< image x axis = marked transducer axis, image y axis = near transducer axis, image z axis = ascending transducer axis */
  US_IMG_ORIENT_FU, /*!< image x axis = far transducer axis, image y axis = unmarked transducer axis (usually for RF frames)*/
  US_IMG_ORIENT_NU, /*!< image x axis = near transducer axis, image y axis = unmarked transducer axis (usually for RF frames)*/
  US_IMG_ORIENT_FM, /*!< image x axis = far transducer axis, image y axis = marked transducer axis (usually for RF frames)*/
  US_IMG_ORIENT_NM, /*!< image x axis = near transducer axis, image y axis = marked transducer axis (usually for RF frames)*/
  US_IMG_ORIENT_LAST   /*!< just a placeholder for range checking, this must be the last defined orientation item */
};

/*!
\enum US_IMAGE_TYPE
\brief Defines constant values for ultrasound image type
\ingroup PlusLibCommon
*/
enum US_IMAGE_TYPE
{
  US_IMG_TYPE_XX,    /*!< undefined */
  US_IMG_BRIGHTNESS, /*!< B-mode image */
  US_IMG_RF_REAL,    /*!< RF-mode image, signal is stored as a series of real values */
  US_IMG_RF_IQ_LINE, /*!< RF-mode image, signal is stored as a series of I and Q samples in a line (I1, Q1, I2, Q2, ...) */
  US_IMG_RF_I_LINE_Q_LINE, /*!< RF-mode image, signal is stored as a series of I samples in a line, then Q samples in the next line (I1, I2, ..., Q1, Q2, ...) */
  US_IMG_RGB_COLOR, /*!< RGB24 color image */
  US_IMG_TYPE_LAST   /*!< just a placeholder for range checking, this must be the last defined image type */
};

/*!
\class igsioVideoFrame
\brief Store images in a variety of pixel formats
A VTK image can only store a certain pixel type.
This class can store a VTK image with any pixel type and has convenient
functions to get/set its content from ITK and VTK images and byte arrays.
\ingroup PlusLibigsioCommon
\sa vtkPlusDataBufferItem
*/
class VTKIGSIOCOMMON_EXPORT igsioVideoFrame
{
public:
  enum TransposeType
  {
    TRANSPOSE_NONE,
    TRANSPOSE_IJKtoKIJ
  };
  static std::string TransposeToString(TransposeType type);

  struct FlipInfoType
  {
    FlipInfoType() : hFlip(false), vFlip(false), eFlip(false), tranpose(TRANSPOSE_NONE), doubleColumn(false), doubleRow(false) {};
    bool hFlip; // flip the image horizontally (pixel columns are reordered)
    bool vFlip; // flip the image vertically (pixel rows are reordered)
    bool eFlip; // flip the image elevationally (pixel slices are reordered)
    TransposeType tranpose; // transpose images
    bool doubleColumn; // keep pairs of pixel columns together (for RF_IQ_LINE encoded images)
    bool doubleRow; // keep pairs of pixel rows together (for RF_I_LINE_Q_LINE encoded images)
  };

  /*! Constructor */
  igsioVideoFrame();

  /*! Destructor */
  virtual ~igsioVideoFrame();

  /*! Copy constructor */
  igsioVideoFrame(const igsioVideoFrame& DataBufferItem);

  /*! Equality operator */
  igsioVideoFrame& operator=(igsioVideoFrame const& videoItem);

  /*! Allocate memory for the image. The image object must be already created. */
  static igsioStatus AllocateFrame(vtkImageData* image, const FrameSizeType& imageSize, igsioCommon::VTKScalarPixelType vtkScalarPixelType, unsigned int numberOfScalarComponents);
  /*! Allocate memory for the image. */
  igsioStatus AllocateFrame(const FrameSizeType& imageSize, igsioCommon::VTKScalarPixelType vtkScalarPixelType, unsigned int numberOfScalarComponents);

  /*! Return the pixel type using VTK enums. */
  igsioCommon::VTKScalarPixelType GetVTKScalarPixelType() const;

  /*! Convert between ITK and VTK scalar pixel types */
  //static igsioCommon::VTKScalarPixelType GetVTKScalarPixelType(igsioCommon::ITKScalarPixelType pixelType);

  /* Return a string version of the VTK type */
  static std::string GetStringFromVTKPixelType(igsioCommon::VTKScalarPixelType vtkScalarPixelType);

  /*! Return the image orientation */
  US_IMAGE_ORIENTATION GetImageOrientation() const;

  /*! Set the image orientation. Does not reorder the pixels. */
  void SetImageOrientation(US_IMAGE_ORIENTATION imgOrientation);

  /*! Return the number of components */
  igsioStatus GetNumberOfScalarComponents(unsigned int& scalarComponents) const;

  /*! Return the image type */
  US_IMAGE_TYPE GetImageType() const;

  /*! Set the image orientation. Does not do any conversion. */
  void SetImageType(US_IMAGE_TYPE imgType);

  /*!
    Get the size of scalar component in bytes.
    Returns 1 for 8-bit images.
    Returns 1 for 24-bit RGB images, as it is stored in three 1-byte scalar components.
  */
  int GetNumberOfBytesPerScalar() const;

  /*!
    Get the size of the pixel in bytes.
    Returns 1 for 8-bit images.
    Returns 3 for 24-bit RGB images.
  */
  int GetNumberOfBytesPerPixel() const;

  /*! Get the size of a scalar pixel in bytes */
  static int GetNumberOfBytesPerScalar(igsioCommon::VTKScalarPixelType pixelType);

  /*! Get the dimensions of the frame in pixels */
  igsioStatus GetFrameSize(FrameSizeType& frameSize) const;

  /*! Get the fourCC of the frame encoding */
  igsioStatus GetEncodingFourCC(std::string& encodingFourCC) const;

  bool IsFrameEncoded() const;

  /*! Get the pointer to the pixel buffer */
  void* GetScalarPointer() const;

  /*! Get the pixel buffer size in bytes */
  unsigned long GetFrameSizeInBytes() const;

  /*! Get the VTK image, does not copy the pixel buffer */
  vtkImageData* GetImage() const;

  /*! Get the encoded frame data */
  vtkStreamingVolumeFrame* GetEncodedFrame() const;

  /*! Set the encoded frame data */
  void SetEncodedFrame(vtkStreamingVolumeFrame* encodedFrame);

  /*! Copy pixel data from another igsioVideoFrame object, same as operator= */
  igsioStatus DeepCopy(igsioVideoFrame* DataBufferItem);

  /*! Sets the pixel buffer content by copying pixel data from a vtkImageData object.*/
  igsioStatus DeepCopyFrom(vtkImageData* frame);

  /*! Sets the pixel buffer content by copying pixel data from a vtkImageData object.*/
  igsioStatus ShallowCopyFrom(vtkImageData* frame);

  /*! Get US_IMAGE_ORIENTATION enum value from string */
  static US_IMAGE_ORIENTATION GetUsImageOrientationFromString(const char* imgOrientationStr);
  static US_IMAGE_ORIENTATION GetUsImageOrientationFromString(const std::string& imgOrientationStr);

  /*! Get US_IMAGE_ORIENTATION string value from enum */
  static std::string GetStringFromUsImageOrientation(US_IMAGE_ORIENTATION imgOrientation);

  /*! Get US_IMAGE_TYPE enum value from string */
  static US_IMAGE_TYPE GetUsImageTypeFromString(const char* imgTypeStr);
  static US_IMAGE_TYPE GetUsImageTypeFromString(const std::string& imgTypeStr);

  /*! Get US_IMAGE_TYPE string value from enum */
  static std::string GetStringFromUsImageType(US_IMAGE_TYPE imgType);

  /*! Read unsigned char type image file to igsioVideoFrame */
  static igsioStatus ReadImageFromFile(igsioVideoFrame& frame, const char* fileName);

  /*! Calculate the flip parameters for two given orientation types
  \param usImageOrientation1 orientation of the first image
  \param usImageType1 type of the first image, used to determine if images are RF or B-mode
  \param usImageOrientation2 orientation of the second image
  \param flipInfo the result structure to populate
  */
  static igsioStatus GetFlipAxes(US_IMAGE_ORIENTATION usImageOrientation1, US_IMAGE_TYPE usImageType1, US_IMAGE_ORIENTATION usImageOrientation2, FlipInfoType& flipInfo);

  /*! Convert oriented image to MF oriented ultrasound image and perform any requested clipping
  \param imageDataPtr the source data to analyze for possible clipping and reorienting
  \param flipInfo the operations to perform
  \param inUsImageType the image type of the source image
  \param inUsImagePixelType the pixel type of the source image
  \param numberOfScalarComponents the number of scalar components of the source image (1=bw, 3=rgb)
  \param inputFrameSizeInPx the frame size of the source image
  \param outUsOrientedImage the output image to populate with clipped and oriented data
  \param clipRectangleOrigin the clipping origin relative to the inUsImage data origin
  \param clipRectangleSize the size of the clipping space, a value of NO_CLIP in either [0],[1] or [2] indicates no clipping performed, in inputImage space
  */
  static igsioStatus GetOrientedClippedImage(unsigned char* imageDataPtr,
      FlipInfoType flipInfo,
      US_IMAGE_TYPE inUsImageType,
      igsioCommon::VTKScalarPixelType inUsImagePixelType,
      unsigned int numberOfScalarComponents,
      const FrameSizeType& inputFrameSizeInPx,
      vtkImageData* outUsOrientedImage,
      const std::array<int, 3>& clipRectangleOrigin,
      const std::array<int, 3>& clipRectangleSize);

  /*! Convert oriented image to MF oriented ultrasound image and perform any requested clipping
  \param imageDataPtr the source data to analyze for possible clipping and reorienting
  \param flipInfo the operations to perform
  \param inUsImageType the image type of the source image
  \param inUsImagePixelType the pixel type of the source image
  \param numberOfScalarComponents the number of scalar components of the source image (1=bw, 3=rgb)
  \param inputFrameSizeInPx the frame size of the source image
  \param outBufferItem the output video frame to populate with clipped and oriented data
  \param clipRectangleOrigin the clipping origin relative to the inUsImage data origin
  \param clipRectangleSize the size of the clipping space, a value of NO_CLIP in either [0],[1] or [2] indicates no clipping performed, in inputImage space
  */
  static igsioStatus GetOrientedClippedImage(unsigned char* imageDataPtr,
      FlipInfoType flipInfo,
      US_IMAGE_TYPE inUsImageType,
      igsioCommon::VTKScalarPixelType inUsImagePixelType,
      unsigned int numberOfScalarComponents,
      const FrameSizeType& inputFrameSizeInPx,
      igsioVideoFrame& outBufferItem,
      const std::array<int, 3>& clipRectangleOrigin,
      const std::array<int, 3>& clipRectangleSize);

  /*! Convert oriented image to MF oriented ultrasound image and perform any requested clipping
  \param inUsImage the source image to analyze for possible clipping and reorienting
  \param flipInfo the operations to perform
  \param inUsImageType the image type of the source image
  \param outUsOrientedImage the output image to populate with clipped and oriented data
  \param clipRectangleOrigin the clipping origin relative to the inUsImage data origin
  \param clipRectangleSize the size of the clipping space, a value of NO_CLIP in either [0],[1] or [2] indicates no clipping performed, in inputImage space
  */
  static igsioStatus GetOrientedClippedImage(vtkImageData* inUsImage,
      FlipInfoType flipInfo,
      US_IMAGE_TYPE inUsImageType,
      vtkImageData* outUsOrientedImage,
      const std::array<int, 3>& clipRectangleOrigin,
      const std::array<int, 3>& clipRectangleSize);

  /*!
  Flip a 2D image along one or two axes. This is a performance optimized version of flipping that does not use ITK filters
  \param clipRectangleOrigin the clipping origin relative to the inUsImage data origin
  \param clipRectangleSize the size of the clipping space, a value of NO_CLIP in either [0],[1] or [2] indicates no clipping performed
  */
  static igsioStatus FlipClipImage(vtkImageData* inUsImage,
                                   const FlipInfoType& flipInfo,
                                   const std::array<int, 3>& clipRectangleOrigin,
                                   const std::array<int, 3>& clipRectangleSize,
                                   vtkImageData* outUsOrientedImage);

  /*! Return true if the image data is valid (e.g. not NULL) */
  bool IsImageValid() const
  {
    if (this->Image == NULL && this->EncodedFrame == NULL)
    {
      return false;
    }

    if (!this->Image && this->EncodedFrame)
    {
      return true;
    }

    const int* extent = this->Image->GetExtent();
    if (extent[0] > extent[1] || extent[2] > extent[3] || extent[4] > extent[5])
    {
      return false;
    }
    return true;
  }

  /*! Fill the actual image data with black pixels (0) */
  igsioStatus FillBlank();

protected:
  void SetImageData(vtkImageData* imageData);

  vtkImageData* Image;
  vtkSmartPointer<vtkStreamingVolumeFrame> EncodedFrame;
  vtkSmartPointer<vtkImageData> DecodedFrame;
  vtkSmartPointer<vtkStreamingVolumeCodec> Codec;

  US_IMAGE_TYPE ImageType;
  US_IMAGE_ORIENTATION ImageOrientation;
};

#endif

/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkIGSIOMetaImageSequenceIO_h
#define __vtkIGSIOMetaImageSequenceIO_h

#include "vtksequenceio_export.h"

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#endif

#include "vtkIGSIOSequenceIOBase.h"
#include "vtk_zlib.h"

/*!
  \class vtkIGSIOMetaImageSequenceIO
  \brief Read and write MetaImage file with a sequence of frames, with additional information for each frame
  \ingroup PlusLibCommon
*/
class VTKSEQUENCEIO_EXPORT vtkIGSIOMetaImageSequenceIO : public vtkIGSIOSequenceIOBase
{
public:
  static vtkIGSIOMetaImageSequenceIO* New();
  vtkTypeMacro(vtkIGSIOMetaImageSequenceIO, vtkIGSIOSequenceIOBase);
  virtual void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /*! Accessors to control 2D Dims output */
  vtkSetMacro(Output2DDataWithZDimensionIncluded, bool);
  vtkGetMacro(Output2DDataWithZDimensionIncluded, bool);

  /*! Update the number of frames in the header
      This is used primarily by vtkPlusVirtualCapture to update the final tally of frames, as it continually appends new frames to the file
      /param numberOfFrames the new number of frames to write
      /param isData3D is the data 3D or 2D?
      */
  virtual igsioStatus UpdateDimensionsCustomStrings(int numberOfFrames, bool isData3D) VTK_OVERRIDE;

  /*!
    Append the frames in tracked frame list to the header, if the onlyTrackerData flag is true it will not save
    in the header the image data related fields.
  */
  virtual igsioStatus AppendImagesToHeader() VTK_OVERRIDE;

  /*! Finalize the header */
  virtual igsioStatus FinalizeHeader() VTK_OVERRIDE;

  /*! Close the sequence */
  virtual igsioStatus Close() VTK_OVERRIDE;

  /*! Check if this class can read the specified file */
  static bool CanReadFile(const std::string& filename);

  /*! Check if this class can write the specified file */
  static bool CanWriteFile(const std::string& filename);

  /*! Update a field in the image header with its current value */
  virtual igsioStatus UpdateFieldInImageHeader(const char* fieldName) VTK_OVERRIDE;

  /*! Return the string that represents the dimensional sizes */
  virtual const char* GetDimensionSizeString() VTK_OVERRIDE;

  /*! Return the string that represents the dimensional kinds */
  virtual const char* GetDimensionKindsString() VTK_OVERRIDE;

  /*!
    Set input/output file name. The file contains only the image header in case of
    MHD images and the full image (including pixel data) in case of MHA images.
  */
  virtual igsioStatus SetFileName(const std::string& aFilename) VTK_OVERRIDE;

protected:
  vtkIGSIOMetaImageSequenceIO();
  virtual ~vtkIGSIOMetaImageSequenceIO();

  /*! Read all the fields in the metaimage file header */
  virtual igsioStatus ReadImageHeader() VTK_OVERRIDE;

  /*! Read pixel data from the metaimage */
  virtual igsioStatus ReadImagePixels() VTK_OVERRIDE;

  /*! Prepare the image file for writing */
  virtual igsioStatus PrepareImageFile() VTK_OVERRIDE;

  /*! Write all the fields to the metaimage file header */
  virtual igsioStatus WriteInitialImageHeader() VTK_OVERRIDE;

  /*!
    Writes the compressed pixel data directly into file.
    The compression is performed in chunks, so no excessive memory is used for the compression.
    \param aFilename the file where the compressed pixel data will be written to
    \param compressedDataSize returns the size of the total compressed data that is written to the file.
  */
  virtual igsioStatus WriteCompressedImagePixelsToFile(int& compressedDataSize) VTK_OVERRIDE;

  /*! Conversion between ITK and METAIO pixel types */
  igsioStatus ConvertMetaElementTypeToVtkPixelType(const std::string& elementTypeStr, igsioCommon::VTKScalarPixelType& vtkPixelType);
  /*! Conversion between ITK and METAIO pixel types */
  igsioStatus ConvertVtkPixelTypeToMetaElementType(igsioCommon::VTKScalarPixelType vtkPixelType, std::string& elementTypeStr);

private:
  /*! ASCII or binary */
  bool IsPixelDataBinary;
  /*! If 2D data, boolean to determine if we should write out in the form X Y Nfr (false) or X Y 1 Nfr (true) */
  bool Output2DDataWithZDimensionIncluded;
  /*! compression stream handle for compression streaming */
  z_stream CompressionStream;

protected:
  vtkIGSIOMetaImageSequenceIO(const vtkIGSIOMetaImageSequenceIO&); //purposely not implemented
  void operator=(const vtkIGSIOMetaImageSequenceIO&); //purposely not implemented
};

#endif // __vtkIGSIOMetaImageSequenceIO_h

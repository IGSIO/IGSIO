/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkIGSIONrrdSequenceIO_h
#define __vtkIGSIONrrdSequenceIO_h

#include "vtksequenceio_export.h"

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#endif

#include "vtkIGSIOSequenceIOBase.h"
#include "vtk_zlib.h"

//class vtkIGSIOTrackedFrameList;

/*!
  \class vtkIGSIONrrdSequenceIO
  \brief Read and write Nrrd file with a sequence of frames, with additional information for each frame. File definition can be found at http://teem.sourceforge.net/nrrd/format.html
  \ingroup PlusLibCommon
*/
class VTKSEQUENCEIO_EXPORT vtkIGSIONrrdSequenceIO : public vtkIGSIOSequenceIOBase
{
  enum NrrdEncoding
  {
    NRRD_ENCODING_RAW = 0,
    NRRD_ENCODING_TXT,
    NRRD_ENCODING_TEXT = NRRD_ENCODING_TXT,
    NRRD_ENCODING_ASCII = NRRD_ENCODING_TXT,
    NRRD_ENCODING_HEX,
    NRRD_ENCODING_GZ,
    NRRD_ENCODING_GZIP = NRRD_ENCODING_GZ,
    NRRD_ENCODING_BZ2,
    NRRD_ENCODING_BZIP2 = NRRD_ENCODING_BZ2
  };

  static std::string EncodingToString( NrrdEncoding encoding );

public:
  static vtkIGSIONrrdSequenceIO* New();
  vtkTypeMacro( vtkIGSIONrrdSequenceIO, vtkIGSIOSequenceIOBase );
  virtual void PrintSelf( ostream& os, vtkIndent indent ) VTK_OVERRIDE;

  /*! Update the number of frames in the header
      This is used primarily by vtkPlusVirtualCapture to update the final tally of frames, as it continually appends new frames to the file
      /param numberOfFrames the new number of frames to write
      /param dimensions number of dimensions in the data
      /param isData3D is the data 3D or 2D?
  */
  virtual igsioStatus UpdateDimensionsCustomStrings( int numberOfFrames, bool isData3D ) VTK_OVERRIDE;

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
  static bool CanReadFile( const std::string& filename );

  /*! Check if this class can write the specified file */
  static bool CanWriteFile( const std::string& filename );

  /*! Update a field in the image header with its current value */
  virtual igsioStatus UpdateFieldInImageHeader( const char* fieldName ) VTK_OVERRIDE;

  /*! Return the string that represents the dimensional sizes */
  virtual const char* GetDimensionSizeString() VTK_OVERRIDE;

  /*! Return the string that represents the dimensional sizes */
  virtual const char* GetDimensionKindsString() VTK_OVERRIDE;

  /*!
    Set input/output file name. The file contains only the image header in case of
    nhdr images and the full image (including pixel data) in case of nrrd images.
  */
  virtual igsioStatus SetFileName( const std::string& aFilename ) VTK_OVERRIDE;

protected:
  vtkIGSIONrrdSequenceIO();
  virtual ~vtkIGSIONrrdSequenceIO();

  /*! Read all the fields in the image file header */
  virtual igsioStatus ReadImageHeader() VTK_OVERRIDE;

  /*! Read pixel data from the image */
  virtual igsioStatus ReadImagePixels() VTK_OVERRIDE;

  /*! Prepare the image file for writing */
  virtual igsioStatus PrepareImageFile() VTK_OVERRIDE;

  /*! Write all the fields to the image file header */
  virtual igsioStatus WriteInitialImageHeader() VTK_OVERRIDE;

  /*!
    Writes the compressed pixel data directly into file.
    The compression is performed in chunks, so no excessive memory is used for the compression.
    \param compressedDataSize returns the size of the total compressed data that is written to the file.
  */
  virtual igsioStatus WriteCompressedImagePixelsToFile( int& compressedDataSize ) VTK_OVERRIDE;

  /*! Conversion between ITK and METAIO pixel types */
  igsioStatus ConvertNrrdTypeToVtkPixelType( const std::string& elementTypeStr, igsioCommon::VTKScalarPixelType& vtkPixelType );
  /*! Conversion between ITK and METAIO pixel types */
  igsioStatus ConvertVtkPixelTypeToNrrdType( igsioCommon::VTKScalarPixelType vtkPixelType, std::string& elementTypeStr );

  /*! Return the size of a file */
  static FilePositionOffsetType GetFileSize( const std::string& filename );

  /*! Convert a string to an encoding */
  static NrrdEncoding StringToNrrdEncoding( const std::string& encoding );

  /*! Convert an encoding to a string*/
  static std::string NrrdEncodingToString( NrrdEncoding encoding );

protected:
  /*! Nrrd encoding type */
  NrrdEncoding Encoding;

  /*! file handle for the compression stream */
  gzFile CompressionStream;

private:
  vtkIGSIONrrdSequenceIO( const vtkIGSIONrrdSequenceIO& ); //purposely not implemented
  void operator=( const vtkIGSIONrrdSequenceIO& ); //purposely not implemented
};

#endif // __vtkIGSIONrrdSequenceIO_h

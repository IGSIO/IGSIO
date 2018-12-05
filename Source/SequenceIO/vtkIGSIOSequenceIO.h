/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkIGSIOSequenceIO_h
#define __vtkIGSIOSequenceIO_h

//#include "igsioCommon.h"
#include "igsioVideoFrame.h"
#include "vtksequenceio_export.h"
#include "vtkIGSIOSequenceIOBase.h"

//class vtkIGSIOTrackedFrameList;

/*!
  \class vtkIGSIOSequenceIO
  \brief Class to abstract away specific sequence file read/write details
  \ingroup PlusLibCommon
*/
class VTKSEQUENCEIO_EXPORT vtkIGSIOSequenceIO : public vtkObject
{
public:
  /*! Write object contents into file */
  static igsioStatus Write(const std::string& filename, const std::string& path, igsioTrackedFrame* frame, US_IMAGE_ORIENTATION orientationInFile = US_IMG_ORIENT_MF, bool useCompression = true, bool enableImageDataWrite = true);
  static igsioStatus Write(const std::string& filename, igsioTrackedFrame* frame, US_IMAGE_ORIENTATION orientationInFile = US_IMG_ORIENT_MF, bool useCompression = true, bool enableImageDataWrite = true);

  /*! Write object contents into file */
  static igsioStatus Write(const std::string& filename, const std::string& path, vtkIGSIOTrackedFrameList* frameList, US_IMAGE_ORIENTATION orientationInFile = US_IMG_ORIENT_MF, bool useCompression = true, bool enableImageDataWrite = true);
  static igsioStatus Write(const std::string& filename, vtkIGSIOTrackedFrameList* frameList, US_IMAGE_ORIENTATION orientationInFile = US_IMG_ORIENT_MF, bool useCompression = true, bool enableImageDataWrite = true);

  /*! Read file contents into the object */
  static igsioStatus Read(const std::string& filename, vtkIGSIOTrackedFrameList* frameList);

  /*! Create a handler for a given filetype */
  static vtkIGSIOSequenceIOBase* CreateSequenceHandlerForFile(const std::string& filename);

protected:
  vtkIGSIOSequenceIO();
  virtual ~vtkIGSIOSequenceIO();
};

#endif // __vtkIGSIOSequenceIO_h

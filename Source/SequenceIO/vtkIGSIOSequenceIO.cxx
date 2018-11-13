/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

//#include "PlusConfigure.h"

#include "vtkIGSIOMetaImageSequenceIO.h"
#include "vtkIGSIONrrdSequenceIO.h"
#include "vtkIGSIOSequenceIO.h"
#include "vtkIGSIOTrackedFrameList.h"

/// VTK includes
#include <vtkNew.h>

#ifdef VTKSEQUENCEIO_USE_MKV
  #include "vtkIGSIOMkvSequenceIO.h"
#endif

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIO::Write(const std::string& filename, const std::string& path, vtkIGSIOTrackedFrameList* frameList, US_IMAGE_ORIENTATION orientationInFile/*=US_IMG_ORIENT_MF*/, bool useCompression/*=true*/, bool enableImageDataWrite/*=true*/)
{
  // Convert local filename to plus output filename
  if (vtksys::SystemTools::FileExists(filename.c_str()))
  {
    // Remove the file before replacing it
    vtksys::SystemTools::RemoveFile(filename.c_str());
  }

  // Parse sequence filename to determine if it's metafile or NRRD
  if (vtkIGSIOMetaImageSequenceIO::CanWriteFile(filename))
  {
    vtkNew<vtkIGSIOMetaImageSequenceIO> writer;
    writer->SetUseCompression(useCompression);
    writer->SetFileName(filename);
    writer->SetOutputFilePath(path);
    writer->SetImageOrientationInFile(orientationInFile);
    writer->SetTrackedFrameList(frameList);
    writer->SetEnableImageDataWrite(enableImageDataWrite);
    if (frameList->GetNumberOfTrackedFrames() == 1)
    {
      writer->IsDataTimeSeriesOff();
    }
    if (writer->Write() != IGSIO_SUCCESS)
    {
      LOG_ERROR(frameList, "Couldn't write sequence metafile: " <<  filename);
      return IGSIO_FAIL;
    }
    return IGSIO_SUCCESS;
  }
  else if (vtkIGSIONrrdSequenceIO::CanWriteFile(filename))
  {
    vtkNew<vtkIGSIONrrdSequenceIO> writer;
    writer->SetUseCompression(useCompression);
    writer->SetFileName(filename);
    writer->SetOutputFilePath(path);
    writer->SetImageOrientationInFile(orientationInFile);
    writer->SetTrackedFrameList(frameList);
    writer->SetEnableImageDataWrite(enableImageDataWrite);
    if (frameList->GetNumberOfTrackedFrames() == 1)
    {
      writer->IsDataTimeSeriesOff();
    }
    if (writer->Write() != IGSIO_SUCCESS)
    {
      LOG_ERROR(frameList, "Couldn't write Nrrd file: " <<  filename);
      return IGSIO_FAIL;
    }
    return IGSIO_SUCCESS;
  }
#ifdef VTKSEQUENCEIO_USE_MKV
  else if (vtkIGSIOMkvSequenceIO::CanWriteFile(filename))
  {
    vtkNew<vtkIGSIOMkvSequenceIO> writer;
    writer->SetUseCompression(useCompression);
    writer->SetFileName(filename);
    writer->SetOutputFilePath(path);
    writer->SetImageOrientationInFile(orientationInFile);
    writer->SetTrackedFrameList(frameList);
    writer->SetEnableImageDataWrite(enableImageDataWrite);
    if (frameList->GetNumberOfTrackedFrames() == 1)
    {
      writer->IsDataTimeSeriesOff();
    }
    if (writer->Write() != IGSIO_SUCCESS)
    {
      LOG_ERROR(frameList, "Couldn't write Nrrd file: " << filename);
      return IGSIO_FAIL;
    }
  }
#endif

  LOG_ERROR(frameList, "No writer for file: " << filename);
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIO::Write(const std::string& filename, const std::string& path, igsioTrackedFrame* frame, US_IMAGE_ORIENTATION orientationInFile /*= US_IMG_ORIENT_MF*/, bool useCompression /*= true*/, bool EnableImageDataWrite /*= true*/)
{
  vtkNew<vtkIGSIOTrackedFrameList> list;
  list->AddTrackedFrame(frame);
  return vtkIGSIOSequenceIO::Write(filename, path, list.GetPointer(), orientationInFile, useCompression, EnableImageDataWrite);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIO::Read(const std::string& filename, vtkIGSIOTrackedFrameList* frameList)
{
  if (!vtksys::SystemTools::FileExists(filename.c_str()))
  {
    LOG_ERROR(frameList, "File: " << filename << " does not exist.");
    return IGSIO_FAIL;
  }

  if (vtkIGSIOMetaImageSequenceIO::CanReadFile(filename))
  {
    // Attempt metafile read
    vtkNew<vtkIGSIOMetaImageSequenceIO> reader;
    reader->SetFileName(filename);
    reader->SetTrackedFrameList(frameList);
    if (reader->Read() != IGSIO_SUCCESS)
    {
      LOG_ERROR(frameList, "Couldn't read sequence metafile: " << filename);
      return IGSIO_FAIL;
    }
    return IGSIO_SUCCESS;
  }
  // Parse sequence filename to determine if it's metafile or NRRD
  else if (vtkIGSIONrrdSequenceIO::CanReadFile(filename))
  {
    vtkNew<vtkIGSIONrrdSequenceIO> reader;
    reader->SetFileName(filename);
    reader->SetTrackedFrameList(frameList);
    if (reader->Read() != IGSIO_SUCCESS)
    {
      LOG_ERROR(frameList, "Couldn't read Nrrd file: " << filename);
      return IGSIO_FAIL;
    }

    return IGSIO_SUCCESS;
  }
#ifdef VTKSEQUENCEIO_ENABLE_MKV
  else if (vtkPlusMkvSequenceIO::CanReadFile(filename))
  {
    // Attempt MKV read
    if (frameList->ReadFromMatroskaFile(filename.c_str()) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to read video buffer from MKV file: " << filename);
      return IGSIO_FAIL;
    }

    return IGSIO_SUCCESS;
  }
#endif

  LOG_ERROR(frameList, "No reader for file: " << filename);
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
vtkIGSIOSequenceIOBase* vtkIGSIOSequenceIO::CreateSequenceHandlerForFile(const std::string& filename)
{
  // Parse sequence filename to determine if it's metafile or NRRD
  if (vtkIGSIOMetaImageSequenceIO::CanWriteFile(filename))
  {
    return vtkIGSIOMetaImageSequenceIO::New();
  }
  else if (vtkIGSIONrrdSequenceIO::CanWriteFile(filename))
  {
    return vtkIGSIONrrdSequenceIO::New();
  }
#ifdef PLUS_USE_VTKSEQUENCEIO_MKV
  else if (vtkPlusMkvSequenceIO::CanReadFile(filename))
  {
    return vtkPlusMkvSequenceIO::New();
  }
#endif

  LOG_ERROR("No writer for file: " << filename);
  return NULL;
}

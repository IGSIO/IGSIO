/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

//#include "PlusConfigure.h"
#include "vtkObjectFactory.h"
#include "vtkIGSIOSequenceIOBase.h"
#include "vtkIGSIOTrackedFrameList.h"
#include "vtksys/SystemTools.hxx"
#include "igsioTrackedFrame.h"

#if _WIN32
  #include <errno.h>

  #if defined(_MSC_PLATFORM_TOOLSET_v120) || defined(_MSC_PLATFORM_TOOLSET_v140)
    // Version helpers is only available in Windows SDK 8.1 (v120) or newer
    #include <VersionHelpers.h>
  #endif
  #include "windows.h"
#endif

//----------------------------------------------------------------------------
vtkCxxSetObjectMacro(vtkIGSIOSequenceIOBase, TrackedFrameList, vtkIGSIOTrackedFrameList);

//----------------------------------------------------------------------------
vtkIGSIOSequenceIOBase::vtkIGSIOSequenceIOBase()
  : TrackedFrameList(vtkIGSIOTrackedFrameList::New())
  , UseCompression(false)
  , CompressedBytesWritten(0)
  , EnableImageDataWrite(true)
  , PixelType(VTK_VOID)
  , NumberOfScalarComponents(1)
  , IsDataTimeSeries(true)
  , NumberOfDimensions(4)
  , CurrentFrameOffset(0)
  , TotalBytesWritten(0)
  , ImageOrientationInFile(US_IMG_ORIENT_XX)
  , ImageOrientationInMemory(US_IMG_ORIENT_XX)
  , ImageType(US_IMG_TYPE_XX)
  , PixelDataFileOffset(0)
  , PixelDataFileName("")
  , OutputImageFileHandle(NULL)
{
  this->Dimensions[0] = 1;
  this->Dimensions[1] = 1;
  this->Dimensions[2] = 1;
  this->Dimensions[3] = 1;
}

//----------------------------------------------------------------------------
vtkIGSIOSequenceIOBase::~vtkIGSIOSequenceIOBase()
{
  if (this->TrackedFrameList != NULL)
  {
    this->SetTrackedFrameList(NULL);
  }
}

//----------------------------------------------------------------------------
void vtkIGSIOSequenceIOBase::PrintSelf(ostream& os, vtkIndent indent)
{
  os << indent << "Frame List User Fields:" << std::endl;
  this->TrackedFrameList->PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::Read()
{
  this->TrackedFrameList->Clear();

  if (this->ReadImageHeader() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Could not load header from file: " << this->FileName);
    return IGSIO_FAIL;
  }

  if (this->ReadImagePixels() != IGSIO_SUCCESS)
  {
    return IGSIO_FAIL;
  }
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::DeleteFrameString(int frameNumber, const char* fieldName)
{
  if (fieldName == nullptr)
  {
    LOG_ERROR("No field name specified.");
    return IGSIO_FAIL;
  }

  igsioTrackedFrame* trackedFrame = this->TrackedFrameList->GetTrackedFrame(frameNumber);
  if (trackedFrame == NULL)
  {
    LOG_ERROR("Cannot access frame " << frameNumber);
    return IGSIO_FAIL;
  }

  return trackedFrame->DeleteFrameField(std::string(fieldName));
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::DeleteFrameString(int frameNumber, const std::string& fieldName)
{
  igsioTrackedFrame* trackedFrame = this->TrackedFrameList->GetTrackedFrame(frameNumber);
  if (trackedFrame == NULL)
  {
    LOG_ERROR("Cannot access frame " << frameNumber);
    return IGSIO_FAIL;
  }

  return trackedFrame->DeleteFrameField(fieldName);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::SetFrameString(int frameNumber, const char* fieldName,  const char* fieldValue)
{
  if (fieldName == NULL || fieldValue == NULL)
  {
    LOG_ERROR("Invalid field name or value");
    return IGSIO_FAIL;
  }
  return this->SetFrameString(frameNumber, std::string(fieldName), std::string(fieldValue));
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::SetFrameString(int frameNumber, const std::string& fieldName, const std::string& fieldValue)
{
  this->CreateTrackedFrameIfNonExisting(frameNumber);
  igsioTrackedFrame* trackedFrame = this->TrackedFrameList->GetTrackedFrame(frameNumber);
  if (trackedFrame == NULL)
  {
    LOG_ERROR("Cannot access frame " << frameNumber);
    return IGSIO_FAIL;
  }
  trackedFrame->SetFrameField(fieldName, fieldValue);
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::SetCustomString(const char* fieldName, const char* fieldValue, igsioFrameFieldFlags flags)
{
  if (fieldName == NULL)
  {
    LOG_ERROR("Invalid field name");
    return IGSIO_FAIL;
  }
  return this->SetFrameField(fieldName, fieldValue, flags);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::SetCustomString(const std::string& fieldName, const std::string& fieldValue, igsioFrameFieldFlags flags)
{
  if (fieldName.empty())
  {
    LOG_ERROR("Invalid field name");
    return IGSIO_FAIL;
  }
  return this->SetFrameField(fieldName, fieldValue, flags);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::SetCustomString(const std::string& fieldName, int fieldValue, igsioFrameFieldFlags flags)
{
  if (fieldName.empty())
  {
    LOG_ERROR("Invalid field name");
    return IGSIO_FAIL;
  }
  std::stringstream ss;
  ss << fieldValue;
  return this->SetFrameField(fieldName, ss.str(), flags);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::SetFrameField(const std::string& fieldName, const std::string& fieldValue, igsioFrameFieldFlags flags)
{
  if (fieldName.empty())
  {
    LOG_ERROR("Invalid field name");
    return IGSIO_FAIL;
  }
  return this->TrackedFrameList->SetFrameField(fieldName, fieldValue, flags);
}

//----------------------------------------------------------------------------
const char* vtkIGSIOSequenceIOBase::GetCustomString(const char* fieldName)
{
  if (fieldName == NULL)
  {
    LOG_ERROR("Invalid field name or value");
    return NULL;
  }
  // Can't return this->GetFrameField(fieldName).c_str(), since the string object would be deleted,
  // leaving a dangling char* pointer. 
  return this->TrackedFrameList->GetCustomString(fieldName);
}

//----------------------------------------------------------------------------
std::string vtkIGSIOSequenceIOBase::GetCustomString(const std::string& fieldName)
{
  return this->GetFrameField(fieldName);
}

//----------------------------------------------------------------------------
std::string vtkIGSIOSequenceIOBase::GetFrameField(const std::string& fieldName)
{
  return this->TrackedFrameList->GetFrameField(fieldName);
}

//----------------------------------------------------------------------------
void vtkIGSIOSequenceIOBase::CreateTrackedFrameIfNonExisting(unsigned int frameNumber)
{
  if (frameNumber < this->TrackedFrameList->GetNumberOfTrackedFrames())
  {
    // frame is already created
    return;
  }
  igsioTrackedFrame emptyFrame;
  for (unsigned int i = this->TrackedFrameList->GetNumberOfTrackedFrames(); i < frameNumber + 1; i++)
  {
    this->TrackedFrameList->AddTrackedFrame(&emptyFrame, vtkIGSIOTrackedFrameList::ADD_INVALID_FRAME);
  }
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::PrepareHeader()
{
  if (this->EnableImageDataWrite && this->TrackedFrameList->IsContainingValidImageData())
  {
    if (this->ImageOrientationInFile == US_IMG_ORIENT_XX)
    {
      // No specific orientation is requested, so just use the same as in the memory
      this->ImageOrientationInFile = this->TrackedFrameList->GetImageOrientation();
    }
    if (this->ImageOrientationInFile != this->TrackedFrameList->GetImageOrientation())
    {
      // Reordering of the frames is not implemented, so just save the images as they are in the memory
      LOG_WARNING("Saving of images is supported only in the same orientation as currently in the memory");
      this->ImageOrientationInFile = this->TrackedFrameList->GetImageOrientation();
    }

    if (this->ImageType == US_IMG_TYPE_XX)
    {
      // No specific type is requested, so just use the same as in the memory
      this->ImageType = this->TrackedFrameList->GetImageType();
    }
    if (this->ImageType != this->TrackedFrameList->GetImageType())
    {
      // Reordering of the frames is not implemented, so just save the images as they are in the memory
      LOG_WARNING("Saving of images is supported only in the same type as currently in the memory");
      this->ImageType = this->TrackedFrameList->GetImageType();
    }
  }

  if (this->TempHeaderFileName.empty())
  {
    std::string tempFilename;
    if (igsioCommon::CreateTemporaryFilename(tempFilename, this->OutputFilePath) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Unable to create temporary header file. Check write access.");
      return IGSIO_FAIL;
    }
    this->TempHeaderFileName = tempFilename;
  }

  if (this->TempImageFileName.empty())
  {
    std::string tempFilename;
    if (igsioCommon::CreateTemporaryFilename(tempFilename, this->OutputFilePath) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Unable to create temporary image file. Check write access.");
      return IGSIO_FAIL;
    }
    this->TempImageFileName = tempFilename;
  }

  if (this->WriteInitialImageHeader() != IGSIO_SUCCESS)
  {
    return IGSIO_FAIL;
  }

  return this->PrepareImageFile();
}

//----------------------------------------------------------------------------
std::array<unsigned int, 4> vtkIGSIOSequenceIOBase::GetDimensions() const
{
  return this->Dimensions;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::Write()
{
  if (this->PrepareHeader() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Unable to prepare the header.");
    return IGSIO_FAIL;
  }
  if (this->AppendImagesToHeader() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Unable to append images to the header.");
    return IGSIO_FAIL;
  }
  if (this->FinalizeHeader() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Unable to finalize the header.");
    return IGSIO_FAIL;
  }

  if (this->WriteImages() != IGSIO_SUCCESS)
  {
    return IGSIO_FAIL;
  }

  this->Close();

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::Close()
{
  std::string headerFullPath = igsioCommon::GetAbsolutePath(this->OutputFilePath, this->FileName);

  // Rename header to final filename
  MoveFileInternal(this->TempHeaderFileName.c_str(), headerFullPath.c_str());

  LOG_DEBUG("Moved file from: " << this->TempHeaderFileName << " to " << headerFullPath);

  if (this->PixelDataFileName.empty())
  {
    // Append image to final file (single file)
    AppendFile(this->TempImageFileName, headerFullPath.c_str());
  }
  else
  {
    // Rename image to final filename (header+data file)

    // Use the same path as the header but replace the filename
    std::vector<std::string> pathElements;
    vtksys::SystemTools::SplitPath(headerFullPath.c_str(), pathElements);
    pathElements.erase(pathElements.end() - 1);
    std::string pixelDataFileNameOnly = vtksys::SystemTools::GetFilenameName(this->PixelDataFileName);
    pathElements.push_back(pixelDataFileNameOnly);
    std::string pixFullPath = vtksys::SystemTools::JoinPath(pathElements);

    MoveFileInternal(this->TempImageFileName.c_str(), pixFullPath.c_str());
  }

  this->TempHeaderFileName.clear();
  this->TempImageFileName.clear();

  this->CurrentFrameOffset = 0;
  this->TotalBytesWritten = 0;
  this->CompressedBytesWritten = 0;

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::WriteImages()
{
  if (this->EnableImageDataWrite && this->TrackedFrameList->IsContainingValidImageData() && this->ImageOrientationInFile != this->TrackedFrameList->GetImageOrientation())
  {
    // Reordering of the frames is not implemented, so return with an error
    LOG_ERROR("Saving of images is supported only in the same orientation as currently in the memory");
    return IGSIO_FAIL;
  }

  bool imageDataAvailable = (this->Dimensions[0] > 0 && this->Dimensions[1] > 0 && this->Dimensions[2] > 0);

  if (this->PixelType == VTK_VOID)
  {
    // If the pixel type was not defined, define it to UCHAR
    this->PixelType = VTK_UNSIGNED_CHAR;
  }

  igsioStatus result = IGSIO_SUCCESS;
  if (!GetUseCompression())
  {
    if (imageDataAvailable)
    {
      // Create a blank frame if we have to write an invalid frame to sequence file
      igsioVideoFrame blankFrame;
      FrameSizeType frameSize = { this->Dimensions[0], this->Dimensions[1], this->Dimensions[2] };
      if (blankFrame.AllocateFrame(frameSize, this->PixelType, this->NumberOfScalarComponents) != IGSIO_SUCCESS)
      {
        LOG_ERROR("Failed to allocate space for blank image.");
        return IGSIO_FAIL;
      }
      blankFrame.FillBlank();

      // not compressed
      for (unsigned int frameNumber = 0; frameNumber < this->TrackedFrameList->GetNumberOfTrackedFrames(); frameNumber++)
      {
        igsioTrackedFrame* trackedFrame = this->TrackedFrameList->GetTrackedFrame(frameNumber);

        igsioVideoFrame* videoFrame = &blankFrame;
        if (this->EnableImageDataWrite && trackedFrame->GetImageData()->IsImageValid())
        {
          videoFrame = trackedFrame->GetImageData();
        }

        size_t writtenSize = 0;
        igsioStatus status = igsioCommon::RobustFwrite(this->OutputImageFileHandle, videoFrame->GetScalarPointer(),
                             videoFrame->GetFrameSizeInBytes(), writtenSize);
        if (status == IGSIO_FAIL)
        {
          LOG_ERROR("Unable to write entire frame to file. Frame size: " << videoFrame->GetFrameSizeInBytes()
                    << ", successfully written: " << writtenSize << " bytes");
        }
        this->TotalBytesWritten += writtenSize;
      }
    }
  }
  else
  {
    // compressed
    int compressedDataSize = 0;
    if (imageDataAvailable)
    {
      result = WriteCompressedImagePixelsToFile(compressedDataSize);
      if (result == IGSIO_SUCCESS)
      {
        TotalBytesWritten += compressedDataSize;
        this->CompressedBytesWritten += compressedDataSize;
      }
    }
  }

  if (result == IGSIO_SUCCESS)
  {
    this->CurrentFrameOffset += this->TrackedFrameList->GetNumberOfTrackedFrames();
  }
  return result;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::MoveFileInternal(const char* oldname, const char* newname)
{
  // Adopted from CMake's cmSystemTools.cxx
  bool success = false;
#ifdef _WIN32
  // On Windows the move functions will not replace existing files. Check if the destination exists.
  if (vtksys::SystemTools::FileExists(newname, true))
  {
    // The destination exists.  We have to replace it carefully.  The
    // MoveFileEx function does what we need but is not available on
    // Win9x.
    DWORD attrs;

    // Make sure the destination is not read only.
    attrs = GetFileAttributes(newname);
    if (attrs & FILE_ATTRIBUTE_READONLY)
    {
      SetFileAttributes(newname, attrs & ~FILE_ATTRIBUTE_READONLY);
    }

#if defined(_MSC_PLATFORM_TOOLSET_v120) || defined(_MSC_PLATFORM_TOOLSET_v140)
    if (!IsWindowsVistaOrGreater() && IsWindowsXPOrGreater())
#else
    // Check the windows version number.
    OSVERSIONINFO osv;
    osv.dwOSVersionInfoSize = sizeof(osv);
    GetVersionEx(&osv);
    if (osv.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
#endif
    {
      // This is Win9x.  There is no MoveFileEx implementation.  We
      // cannot quite rename the file atomically.  Just delete the
      // destination and then move the file.
      DeleteFile(newname);
      success = (MoveFile(oldname, newname) != 0);
    }
    else
    {
      // This is not Win9x.  Use the MoveFileEx implementation.
      success = (MoveFileEx(oldname, newname, MOVEFILE_REPLACE_EXISTING) != 0);
    }
  }
  else
  {
    // The destination does not exist.  Just move the file.
    success = (MoveFile(oldname, newname) != 0);
  }
#else
  if (!vtksys::SystemTools::CopyFileAlways(oldname, newname))
  {
    return IGSIO_FAIL;
  }
  vtksys::SystemTools::RemoveFile(oldname);
#endif
  return success ? IGSIO_SUCCESS : IGSIO_FAIL;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::Discard()
{
  vtksys::SystemTools::RemoveFile(this->TempHeaderFileName.c_str());
  vtksys::SystemTools::RemoveFile(this->TempImageFileName.c_str());

  this->TempHeaderFileName.clear();
  this->TempImageFileName.clear();

  this->CurrentFrameOffset = 0;
  this->TotalBytesWritten = 0;

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioTrackedFrame* vtkIGSIOSequenceIOBase::GetTrackedFrame(int frameNumber)
{
  igsioTrackedFrame* trackedFrame = this->TrackedFrameList->GetTrackedFrame(frameNumber);
  return trackedFrame;
}

//----------------------------------------------------------------------------
FrameSizeType vtkIGSIOSequenceIOBase::GetMaximumImageDimensions()
{
  FrameSizeType maxFrameSize;
  maxFrameSize[0] = 0;
  maxFrameSize[1] = 0;
  maxFrameSize[2] = 0;

  for (unsigned int frameNumber = 0; frameNumber < this->TrackedFrameList->GetNumberOfTrackedFrames(); frameNumber++)
  {
    FrameSizeType currFrameSize = this->TrackedFrameList->GetTrackedFrame(frameNumber)->GetFrameSize();
    if (maxFrameSize[0] < currFrameSize[0])
    {
      maxFrameSize[0] = currFrameSize[0];
    }

    if (maxFrameSize[1] < currFrameSize[1])
    {
      maxFrameSize[1] = currFrameSize[1];
    }

    if (maxFrameSize[2] < currFrameSize[2])
    {
      maxFrameSize[2] = currFrameSize[2];
    }
  }

  return maxFrameSize;
}

//----------------------------------------------------------------------------
std::string vtkIGSIOSequenceIOBase::GetPixelDataFilePath()
{
  if (this->PixelDataFileName.empty())
  {
    // LOCAL => data is stored in one file
    return this->FileName;
  }

  std::string dir = vtksys::SystemTools::GetFilenamePath(this->FileName);
  if (!dir.empty())
  {
    dir += "/";
  }
  std::string path = dir + this->PixelDataFileName;
  return path;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::FileOpen(FILE** stream, const char* filename, const char* flags)
{
#ifdef _WIN32
  if (fopen_s(stream, filename, flags) != 0)
  {
    (*stream) = NULL;
  }
#else
  (*stream) = fopen(filename, flags);
#endif
  if ((*stream) == 0)
  {
    return IGSIO_FAIL;
  }
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOSequenceIOBase::AppendFile(const std::string& sourceFilename, const std::string& destFilename)
{
#if _WIN32
  FILE* in;
  errno_t inErr = fopen_s(&in, sourceFilename.c_str(), "rb") ;
#else
  FILE* in = fopen(sourceFilename.c_str(), "rb") ;
#endif

#if _WIN32
  FILE* out;
  errno_t outErr = fopen_s(&out, destFilename.c_str(), "ab+");
#else
  FILE* out = fopen(destFilename.c_str(), "ab+");
#endif

#if _WIN32
  if (inErr != 0 || outErr != 0)
  {
    char inErrStr[3000];
    char outErrStr[3000];
    strerror_s(inErrStr, inErr);
    strerror_s(outErrStr, outErr);
    LOG_ERROR("An error occurred while appending data from " << sourceFilename << " to " << destFilename << ": " << inErrStr << "::" << outErrStr) ;
#else
  if (in == NULL || out == NULL)
  {
    LOG_ERROR("An error occurred while appending data from " << sourceFilename << " to " << destFilename) ;
#endif
    return IGSIO_FAIL;
  }
  else
  {
    const int BUFFER_SIZE = 32000;
    char* buffer = new char[BUFFER_SIZE];
    size_t len = 0 ;
    while ((len = fread(buffer, 1, BUFFER_SIZE, in)) > 0)
    {
      fwrite(buffer, 1, len, out) ;
      memset(buffer, 0, BUFFER_SIZE);
    }
    fclose(in);
    fclose(out);
    if (!vtksys::SystemTools::RemoveFile(sourceFilename.c_str()))
    {
      LOG_WARNING("Unable to remove the file " << sourceFilename << " after append is completed");
    }
    delete[] buffer;
  }
  return IGSIO_SUCCESS;
}

/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

//#include "itksys/SystemTools.hxx"
#include "vtkIGSIOMetaImageSequenceIO.h"
#include <iomanip>
#include <iostream>
#include <vector>

#ifdef _WIN32
  #define FSEEK _fseeki64
  #define FTELL _ftelli64
#else
  #define FSEEK fseek
  #define FTELL ftell
#endif

#include "vtksys/SystemTools.hxx"
#include "vtkObjectFactory.h"
#include "vtkIGSIOTrackedFrameList.h"
#include "igsioTrackedFrame.h"

namespace
{
  // Size of MetaIO fields, in bytes (adopted from metaTypes.h)
  enum
  {
    MET_NONE, MET_ASCII_CHAR, MET_CHAR, MET_UCHAR, MET_SHORT,
    MET_USHORT, MET_INT, MET_UINT, MET_LONG, MET_ULONG,
    MET_LONG_LONG, MET_ULONG_LONG, MET_FLOAT, MET_DOUBLE, MET_STRING,
    MET_CHAR_ARRAY, MET_UCHAR_ARRAY, MET_SHORT_ARRAY, MET_USHORT_ARRAY, MET_INT_ARRAY,
    MET_UINT_ARRAY, MET_LONG_ARRAY, MET_ULONG_ARRAY, MET_LONG_LONG_ARRAY, MET_ULONG_LONG_ARRAY,
    MET_FLOAT_ARRAY, MET_DOUBLE_ARRAY, MET_FLOAT_MATRIX, MET_OTHER,
    // insert values before this line
    MET_NUM_VALUE_TYPES
  };
  static const unsigned char MET_ValueTypeSize[MET_NUM_VALUE_TYPES] =
  {
    0, 1, 1, 1, 2,
    2, 4, 4, 4, 4,
    8, 8, 4, 8, 1,
    1, 1, 2, 2, 4,
    4, 4, 4, 8, 8,
    4, 8, 4, 0
  };

  static const int MAX_LINE_LENGTH = 1000;

  static const int SEQMETA_FIELD_PADDED_LINE_LENGTH = 40;
  static const char* SEQMETA_FIELD_US_IMG_ORIENT = "UltrasoundImageOrientation";
  static const char* SEQMETA_FIELD_US_IMG_TYPE = "UltrasoundImageType";
  static const char* SEQMETA_FIELD_ELEMENT_DATA_FILE = "ElementDataFile";
  static const char* SEQMETA_FIELD_VALUE_ELEMENT_DATA_FILE_LOCAL = "LOCAL";
  static const char* SEQMETA_FIELD_DIMSIZE = "DimSize";
  static const char* SEQMETA_FIELD_KINDS = "Kinds";
  static const char* SEQMETA_FIELD_COMPRESSED_DATA_SIZE = "CompressedDataSize";

  static std::string SEQMETA_FIELD_FRAME_FIELD_PREFIX = "Seq_Frame";
  static std::string SEQMETA_FIELD_IMG_STATUS = "ImageStatus";
}

//----------------------------------------------------------------------------

vtkStandardNewMacro(vtkIGSIOMetaImageSequenceIO);

//----------------------------------------------------------------------------
vtkIGSIOMetaImageSequenceIO::vtkIGSIOMetaImageSequenceIO()
  : vtkIGSIOSequenceIOBase()
  , IsPixelDataBinary(true)
  , Output2DDataWithZDimensionIncluded(false)
{
}

//----------------------------------------------------------------------------
vtkIGSIOMetaImageSequenceIO::~vtkIGSIOMetaImageSequenceIO()
{
}

//----------------------------------------------------------------------------
void vtkIGSIOMetaImageSequenceIO::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);

  // TODO : anything specific to print
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::ReadImageHeader()
{
  FILE* stream = NULL;
  // open in binary mode because we determine the start of the image buffer also during this read
  if (FileOpen(&stream, this->FileName.c_str(), "rb") != IGSIO_SUCCESS)
  {
    LOG_ERROR("The file " << this->FileName << " could not be opened for reading");
    return IGSIO_FAIL;
  }

  char line[MAX_LINE_LENGTH + 1] = {0};
  while (fgets(line, MAX_LINE_LENGTH, stream))
  {

    std::string lineStr = line;

    // Split line into name and value
    size_t equalSignFound;
    equalSignFound = lineStr.find_first_of("=");
    if (equalSignFound == std::string::npos)
    {
      LOG_WARNING("Parsing line failed, equal sign is missing (" << lineStr << ")");
      continue;
    }
    // trim spaces from the left and right
    std::string name = igsioCommon::Trim(lineStr.substr(0, equalSignFound));
    std::string value = igsioCommon::Trim(lineStr.substr(equalSignFound + 1));

    if (!igsioCommon::HasSubstrInsensitive(name, SEQMETA_FIELD_FRAME_FIELD_PREFIX))
    {
      // field
      this->SetFrameField(name, value);

      // Arrived to ElementDataFile, this is the last element
      if (igsioCommon::IsEqualInsensitive(name, SEQMETA_FIELD_ELEMENT_DATA_FILE))
      {
        if (igsioCommon::IsEqualInsensitive(value, SEQMETA_FIELD_VALUE_ELEMENT_DATA_FILE_LOCAL))
        {
          // pixel data stored locally
          this->PixelDataFileOffset = FTELL(stream);
        }
        else
        {
          // pixel data stored in separate file
          this->PixelDataFileName = value;
          this->PixelDataFileOffset = 0;
        }
        // this is the last element of the header
        break;
      }
    }
    else
    {
      // frame field
      // name: Seq_Frame0000_CustomTransform
      name.erase(0, SEQMETA_FIELD_FRAME_FIELD_PREFIX.size());   // 0000_CustomTransform

      // Split line into name and value
      size_t underscoreFound;
      underscoreFound = name.find_first_of("_");
      if (underscoreFound == std::string::npos)
      {
        LOG_WARNING("Parsing line failed, underscore is missing from frame field name (" << lineStr << ")");
        continue;
      }
      std::string frameNumberStr = name.substr(0, underscoreFound);   // 0000
      std::string frameFieldName = name.substr(underscoreFound + 1);   // CustomTransform

      int frameNumber = 0;
      if (igsioCommon::StringToInt(frameNumberStr.c_str(), frameNumber) != IGSIO_SUCCESS)
      {
        LOG_WARNING("Parsing line failed, cannot get frame number from frame field (" << lineStr << ")");
        continue;
      }
      SetFrameString(frameNumber, frameFieldName.c_str(), value.c_str());

      if (ferror(stream))
      {
        LOG_ERROR("Error reading the file " << this->FileName);
        break;
      }
      if (feof(stream))
      {
        break;
      }
    }
  }

  fclose(stream);

  int nDims = 3;
  if (igsioCommon::StringToInt(this->TrackedFrameList->GetCustomString("NDims"), nDims) == IGSIO_SUCCESS)
  {
    if (nDims != 2 && nDims != 3 && nDims != 4)
    {
      LOG_ERROR("Invalid NDims value: " << nDims << ". Valid range is [2,4].");
      return IGSIO_FAIL;
    }
  }
  this->NumberOfDimensions = nDims;

  std::vector<std::string> kinds;
  if (this->TrackedFrameList->GetCustomString(SEQMETA_FIELD_KINDS) != NULL)
  {
    igsioCommon::SplitStringIntoTokens(std::string(this->TrackedFrameList->GetCustomString(SEQMETA_FIELD_KINDS)), ' ', kinds);
  }
  else
  {
    LOG_WARNING(SEQMETA_FIELD_KINDS << " not found in file: " << this->FileName << ". Treating the last dimension as time.");

    for (int i = 0; i < this->NumberOfDimensions - 1; i++)
    {
      kinds.push_back("domain");
    }
    kinds.push_back("list");
  }

  std::istringstream issDimSize(this->TrackedFrameList->GetCustomString(SEQMETA_FIELD_DIMSIZE));
  int dimSize(0);
  int spatialDomainCount(0);
  for (unsigned int i = 0; i < kinds.size(); i++)
  {
    issDimSize >> dimSize;
    if (igsioCommon::IsEqualInsensitive(kinds[i], "domain"))
    {
      if (spatialDomainCount == 3 && dimSize > 1)   // 0-indexed, this is the 4th spatial domain
      {
        LOG_ERROR("PLUS supports up to 3 spatial domains. File: " << this->FileName << " contains more than 3.");
        return IGSIO_FAIL;
      }
      this->Dimensions[spatialDomainCount] = dimSize;
      spatialDomainCount++;
    }
    else if (igsioCommon::IsEqualInsensitive(kinds[i], "time") || igsioCommon::IsEqualInsensitive(kinds[i], "list")) // time = resampling ok, list = resampling not ok
    {
      this->Dimensions[3] = dimSize;
    }
  }

  // Post process to handle setting 3rd dimension to 0 if there is no image data in the file
  if (this->Dimensions[0] == 0 || this->Dimensions[1] == 0)
  {
    this->Dimensions[2] = 0;
  }

  // Only check image related settings if dimensions are not 0 0 0
  if (this->Dimensions[0] != 0 && this->Dimensions[1] != 0 && this->Dimensions[2] != 0)
  {
    this->ImageOrientationInFile = igsioVideoFrame::GetUsImageOrientationFromString(this->GetCustomString(SEQMETA_FIELD_US_IMG_ORIENT));

    const char* imgTypeStr = this->GetCustomString(SEQMETA_FIELD_US_IMG_TYPE);
    if (imgTypeStr == NULL)
    {
      // if the image type is not defined then assume that it is B-mode image
      this->ImageType = US_IMG_BRIGHTNESS;
    }
    else
    {
      this->ImageType = igsioVideoFrame::GetUsImageTypeFromString(imgTypeStr);
    }

    const char* binaryDataFieldValue = this->TrackedFrameList->GetCustomString("BinaryData");
    if (binaryDataFieldValue != NULL)
    {
      if (STRCASECMP(binaryDataFieldValue, "true") == 0)
      {
        this->IsPixelDataBinary = true;
      }
      else
      {
        this->IsPixelDataBinary = false;
      }
    }
    else
    {
      LOG_WARNING("BinaryData field has not been found in " << this->FileName << ". Assume binary data.");
      this->IsPixelDataBinary = true;
    }
    if (!this->IsPixelDataBinary)
    {
      LOG_ERROR("Failed to read " << this->FileName << ". Only binary pixel data (BinaryData=true) reading is supported.");
      return IGSIO_FAIL;
    }

    if (this->TrackedFrameList->GetCustomString("CompressedData") != NULL
        && STRCASECMP(this->TrackedFrameList->GetCustomString("CompressedData"), "true") == 0)
    {
      this->SetUseCompression(true);
    }
    else
    {
      this->SetUseCompression(false);
    }

    if (this->TrackedFrameList->GetCustomString("ElementNumberOfChannels") != NULL)
    {
      // this field is optional
      igsioCommon::StringToInt(this->TrackedFrameList->GetCustomString("ElementNumberOfChannels"), this->NumberOfScalarComponents);
    }

    std::string elementTypeStr = this->TrackedFrameList->GetCustomString("ElementType");
    if (ConvertMetaElementTypeToVtkPixelType(elementTypeStr.c_str(), this->PixelType) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Unknown component type: " << elementTypeStr);
      return IGSIO_FAIL;
    }

    // If no specific image orientation is requested then determine it automatically from the image type
    // B-mode: MF
    // RF-mode: FM
    if (this->ImageOrientationInMemory == US_IMG_ORIENT_XX)
    {
      switch (this->ImageType)
      {
        case US_IMG_BRIGHTNESS:
        case US_IMG_RGB_COLOR:
          this->SetImageOrientationInMemory(US_IMG_ORIENT_MF);
          break;
        case US_IMG_RF_I_LINE_Q_LINE:
        case US_IMG_RF_IQ_LINE:
        case US_IMG_RF_REAL:
          this->SetImageOrientationInMemory(US_IMG_ORIENT_FM);
          break;
        default:
          if (this->Dimensions[0] == 0 && this->Dimensions[1] == 0 && this->Dimensions[2] == 1)
          {
            LOG_DEBUG("Only tracking data is available in the metafile");
          }
          else
          {
            LOG_WARNING("Cannot determine image orientation automatically, unknown image type " <<
                        (imgTypeStr ? imgTypeStr : "(undefined)") << ", use the same orientation in memory as in the file");
          }
          this->SetImageOrientationInMemory(this->ImageOrientationInFile);
      }
    }
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
// Read the spacing and dimensions of the image.
igsioStatus vtkIGSIOMetaImageSequenceIO::ReadImagePixels()
{
  int frameCount = this->Dimensions[3];
  unsigned int frameSizeInBytes = 0;
  if (this->Dimensions[0] > 0 && this->Dimensions[1] > 0 && this->Dimensions[2] > 0)
  {
    frameSizeInBytes = this->Dimensions[0] * this->Dimensions[1] * this->Dimensions[2] * igsioVideoFrame::GetNumberOfBytesPerScalar(this->PixelType) * this->NumberOfScalarComponents;
  }

  if (frameSizeInBytes == 0)
  {
    LOG_DEBUG("No image data in the metafile");
    return IGSIO_SUCCESS;
  }

  int numberOfErrors = 0;

  FILE* stream = NULL;

  if (FileOpen(&stream, GetPixelDataFilePath().c_str(), "rb") != IGSIO_SUCCESS)
  {
    LOG_ERROR("The file " << GetPixelDataFilePath() << " could not be opened for reading");
    return IGSIO_FAIL;
  }

  std::vector<unsigned char> allFramesPixelBuffer;
  if (this->UseCompression)
  {
    unsigned int allFramesPixelBufferSize = frameCount * frameSizeInBytes;

    try
    {
      allFramesPixelBuffer.resize(allFramesPixelBufferSize);
    }
    catch (std::bad_alloc& e)
    {
      cerr << e.what() << endl;
      LOG_ERROR("vtkIGSIOMetaImageSequenceIO::ReadImagePixels failed due to out of memory. Try to reduce image buffer sizes or use a 64-bit build of Plus.");
      return IGSIO_FAIL;
    }

    unsigned int allFramesCompressedPixelBufferSize = 0;
    igsioCommon::StringToInt(this->TrackedFrameList->GetCustomString(SEQMETA_FIELD_COMPRESSED_DATA_SIZE), allFramesCompressedPixelBufferSize);
    std::vector<unsigned char> allFramesCompressedPixelBuffer;
    allFramesCompressedPixelBuffer.resize(allFramesCompressedPixelBufferSize);

    FSEEK(stream, this->PixelDataFileOffset, SEEK_SET);
    if (fread(&(allFramesCompressedPixelBuffer[0]), 1, allFramesCompressedPixelBufferSize, stream) != allFramesCompressedPixelBufferSize)
    {
      LOG_ERROR("Could not read " << allFramesCompressedPixelBufferSize << " bytes from " << GetPixelDataFilePath());
      fclose(stream);
      return IGSIO_FAIL;
    }

    uLongf unCompSize = allFramesPixelBufferSize;
    if (uncompress((Bytef*) & (allFramesPixelBuffer[0]), &unCompSize, (const Bytef*) & (allFramesCompressedPixelBuffer[0]), allFramesCompressedPixelBufferSize) != Z_OK)
    {
      LOG_ERROR("Cannot uncompress the pixel data");
      fclose(stream);
      return IGSIO_FAIL;
    }
    if (unCompSize != allFramesPixelBufferSize)
    {
      LOG_ERROR("Cannot uncompress the pixel data: uncompressed data is less than expected");
      fclose(stream);
      return IGSIO_FAIL;
    }

  }

  std::vector<unsigned char> pixelBuffer;
  pixelBuffer.resize(frameSizeInBytes);
  for (int frameNumber = 0; frameNumber < frameCount; frameNumber++)
  {
    CreateTrackedFrameIfNonExisting(frameNumber);
    igsioTrackedFrame* trackedFrame = this->TrackedFrameList->GetTrackedFrame(frameNumber);

    // Allocate frame only if it is valid
    std::string imgStatus = trackedFrame->GetFrameField(SEQMETA_FIELD_IMG_STATUS);
    if (!imgStatus.empty())    // Found the image status field
    {
      // Save status field
      std::string strImgStatus(imgStatus);

      // Delete image status field from tracked frame
      // Image status can be determine by trackedFrame->GetImageData()->IsImageValid()
      trackedFrame->DeleteFrameField(SEQMETA_FIELD_IMG_STATUS.c_str());

      if (STRCASECMP(strImgStatus.c_str(), "OK") != 0)     // Image status _not_ OK
      {
        LOG_DEBUG("Frame #" << frameNumber << " image data is invalid, no need to allocate data in the tracked frame list.");
        continue;
      }
    }

    trackedFrame->GetImageData()->SetImageOrientation(this->ImageOrientationInMemory);
    trackedFrame->GetImageData()->SetImageType(this->ImageType);

    FrameSizeType frameSize = { this->Dimensions[0], this->Dimensions[1], this->Dimensions[2] };
    if (trackedFrame->GetImageData()->AllocateFrame(frameSize, this->PixelType, this->NumberOfScalarComponents) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Cannot allocate memory for frame " << frameNumber);
      numberOfErrors++;
      continue;
    }

    std::array<int, 3> clipRectOrigin = {igsioCommon::NO_CLIP, igsioCommon::NO_CLIP, igsioCommon::NO_CLIP};
    std::array<int, 3> clipRectSize = {igsioCommon::NO_CLIP, igsioCommon::NO_CLIP, igsioCommon::NO_CLIP};

    igsioVideoFrame::FlipInfoType flipInfo;
    if (igsioVideoFrame::GetFlipAxes(this->ImageOrientationInFile, this->ImageType, this->ImageOrientationInMemory, flipInfo) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to convert image data to the requested orientation, from " << igsioVideoFrame::GetStringFromUsImageOrientation(this->ImageOrientationInFile) <<
                " to " << igsioVideoFrame::GetStringFromUsImageOrientation(this->ImageOrientationInMemory));
      return IGSIO_FAIL;
    }

    if (!this->UseCompression)
    {
      FilePositionOffsetType offset = PixelDataFileOffset + frameNumber * frameSizeInBytes;
      FSEEK(stream, offset, SEEK_SET);
      if (fread(&(pixelBuffer[0]), 1, frameSizeInBytes, stream) != frameSizeInBytes)
      {
        LOG_ERROR("Could not read " << frameSizeInBytes << " bytes from " << GetPixelDataFilePath());
        numberOfErrors++;
      }
      FrameSizeType frameSize = { this->Dimensions[0], this->Dimensions[1], this->Dimensions[2] };
      if (igsioVideoFrame::GetOrientedClippedImage(&(pixelBuffer[0]), flipInfo, this->ImageType, this->PixelType, this->NumberOfScalarComponents, frameSize, *trackedFrame->GetImageData(), clipRectOrigin, clipRectSize) != IGSIO_SUCCESS)
      {
        LOG_ERROR("Failed to get oriented image from sequence metafile (frame number: " << frameNumber << ")!");
        numberOfErrors++;
        continue;
      }
    }
    else
    {
      FrameSizeType frameSize = { this->Dimensions[0], this->Dimensions[1], this->Dimensions[2] };
      if (igsioVideoFrame::GetOrientedClippedImage(&(allFramesPixelBuffer[0]) + frameNumber * frameSizeInBytes, flipInfo, this->ImageType, this->PixelType, this->NumberOfScalarComponents, frameSize, *trackedFrame->GetImageData(), clipRectOrigin, clipRectSize) != IGSIO_SUCCESS)
      {
        LOG_ERROR("Failed to get oriented image from sequence metafile (frame number: " << frameNumber << ")!");
        numberOfErrors++;
        continue;
      }
    }
  }

  fclose(stream);

  if (numberOfErrors > 0)
  {
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::PrepareImageFile()
{
  if (this->GetUseCompression())
  {
    // use the default memory allocation routines
    this->CompressionStream.zalloc = Z_NULL;
    this->CompressionStream.zfree = Z_NULL;
    this->CompressionStream.opaque = Z_NULL;
    int ret = deflateInit(&this->CompressionStream, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK)
    {
      LOG_ERROR("Image compression initialization failed (errorCode=" << ret << ")");
      return IGSIO_FAIL;
    }
  }
  if (FileOpen(&this->OutputImageFileHandle, this->TempImageFileName.c_str(), "ab+") != IGSIO_SUCCESS)
  {
    LOG_ERROR("Unable to open output stream for writing.");
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
bool vtkIGSIOMetaImageSequenceIO::CanReadFile(const std::string& filename)
{
  FILE* stream = NULL;
  // open in binary mode because we determine the start of the image buffer also during this read
  if (vtkIGSIOMetaImageSequenceIO::FileOpen(&stream, filename.c_str(), "rb") != IGSIO_SUCCESS)
  {
    LOG_DEBUG("The file " << filename << " could not be opened for reading");
    return false;
  }
  char line[MAX_LINE_LENGTH + 1] = {0};
  if (fgets(line, MAX_LINE_LENGTH, stream) != line)
  {
    LOG_DEBUG("The file " << filename << " could not be opened for reading. Failed to retrieve first line.");
    return false;
  }
  fclose(stream);

  // the first line in the file should be:
  // ObjectType = Image

  std::string lineStr = line;

  // Split line into name and value
  size_t equalSignFound;
  equalSignFound = lineStr.find_first_of("=");
  if (equalSignFound == std::string::npos)
  {
    LOG_DEBUG("Parsing line failed, equal sign is missing (" << lineStr << ")");
    return false;
  }
  // trim spaces from the left and right
  std::string name = igsioCommon::Trim(lineStr.substr(0, equalSignFound));
  std::string value = igsioCommon::Trim(lineStr.substr(equalSignFound + 1));

  if (!igsioCommon::IsEqualInsensitive(name, "ObjectType"))
  {
    LOG_DEBUG("Expect ObjectType field name in the first field");
    return false;
  }
  if (!igsioCommon::IsEqualInsensitive(value, "Image"))
  {
    LOG_DEBUG("Expect Image value name in the first field");
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------
bool vtkIGSIOMetaImageSequenceIO::CanWriteFile(const std::string& filename)
{
  if (igsioCommon::IsEqualInsensitive(vtksys::SystemTools::GetFilenameLastExtension(filename), ".mha")  ||
      igsioCommon::IsEqualInsensitive(vtksys::SystemTools::GetFilenameLastExtension(filename), ".mhd"))
  {
    return true;
  }

  return false;
}

//----------------------------------------------------------------------------
/** Writes the spacing and dimensions of the image.
* Assumes SetFileName has been called with a valid file name. */
igsioStatus vtkIGSIOMetaImageSequenceIO::WriteInitialImageHeader()
{
  if (this->TrackedFrameList->GetNumberOfTrackedFrames() == 0)
  {
    LOG_ERROR("No frames in frame list, unable to query a frame for meta information.");
    return IGSIO_FAIL;
  }

  // First, is this 2D or 3D?
  bool isData3D = (this->TrackedFrameList->GetTrackedFrame(0)->GetFrameSize()[2] > 1);
  bool isDataTimeSeries = this->IsDataTimeSeries; // don't compute it from the number of frames because we may still have only one frame but acquire more frames later

  // Override fields
  this->NumberOfDimensions = isData3D ? 3 : 2;
  if (this->Output2DDataWithZDimensionIncluded)
  {
    this->NumberOfDimensions++;
  }
  if (isDataTimeSeries)
  {
    this->NumberOfDimensions++;
  }
  this->SetFrameField("NDims", igsioCommon::ToString<int>(this->NumberOfDimensions));

  this->SetFrameField("BinaryData", "True");
  this->SetFrameField("BinaryDataByteOrderMSB", "False");

  // CompressedData
  if (GetUseCompression())
  {
    this->SetFrameField("CompressedData", "True");
    int paddingCharacters = SEQMETA_FIELD_PADDED_LINE_LENGTH - strlen(SEQMETA_FIELD_COMPRESSED_DATA_SIZE) - 4;
    std::string compDataSize("0");
    for (int i = 0; i < paddingCharacters; ++i)
    {
      compDataSize += " ";
    }
    this->SetFrameField(SEQMETA_FIELD_COMPRESSED_DATA_SIZE, compDataSize);   // add spaces so that later the field can be updated with larger values
  }
  else
  {
    this->SetFrameField("CompressedData", "False");
    this->SetFrameField(SEQMETA_FIELD_COMPRESSED_DATA_SIZE, "");
  }

  FrameSizeType frameSize = {0, 0, 0};
  if (this->EnableImageDataWrite)
  {
    frameSize = this->GetMaximumImageDimensions();
  }
  else
  {
    frameSize[0] = 1;
    frameSize[1] = 1;
    frameSize[2] = 1;
  }

  // Set the dimensions of the data to be written
  this->Dimensions[0] = frameSize[0];
  this->Dimensions[1] = frameSize[1];
  this->Dimensions[2] = frameSize[2];
  this->Dimensions[3] = this->TrackedFrameList->GetNumberOfTrackedFrames();

  if (this->EnableImageDataWrite)
  {
    // Make sure the frame size is the same for each valid image
    // If it's needed, we can use the largest frame size for each frame and copy the image data row by row
    // but then, we need to save the original frame size for each frame and crop the image when we read it
    for (unsigned int frameNumber = 0; frameNumber < this->TrackedFrameList->GetNumberOfTrackedFrames(); frameNumber++)
    {
      FrameSizeType currFrameSize = this->TrackedFrameList->GetTrackedFrame(frameNumber)->GetFrameSize();
      if (this->TrackedFrameList->GetTrackedFrame(frameNumber)->GetImageData()->IsImageValid()
          && (frameSize[0] != currFrameSize[0] || frameSize[1] != currFrameSize[1] || frameSize[2] != currFrameSize[2]))
      {
        LOG_ERROR("Frame size mismatch: expected size (" << frameSize[0] << "x" << frameSize[1] << "x" << frameSize[2]
                  << ") differ from actual size (" << currFrameSize[0] << "x" << currFrameSize[1] << "x" << currFrameSize[2] << ") for frame #" << frameNumber);
        return IGSIO_FAIL;
      }
    }
  }

  // Update NDims and Dims fields in header
  this->UpdateDimensionsCustomStrings(this->TrackedFrameList->GetNumberOfTrackedFrames(), isData3D);

  // PixelType
  if (this->TrackedFrameList->IsContainingValidImageData())
  {
    this->PixelType = this->TrackedFrameList->GetPixelType();
    if (this->PixelType == VTK_VOID)
    {
      // If the pixel type was not defined, define it to UCHAR
      this->PixelType = VTK_UNSIGNED_CHAR;
    }
  }
  std::string pixelTypeStr;
  vtkIGSIOMetaImageSequenceIO::ConvertVtkPixelTypeToMetaElementType(this->PixelType, pixelTypeStr);
  this->SetFrameField("ElementType", pixelTypeStr);   // pixel type (a.k.a component type) is stored in the ElementType element

  // ElementNumberOfChannels
  if (this->EnableImageDataWrite)
  {
    if (this->TrackedFrameList->IsContainingValidImageData())
    {
      this->NumberOfScalarComponents = this->TrackedFrameList->GetNumberOfScalarComponents();
    }
    this->SetFrameField("ElementNumberOfChannels", igsioCommon::ToString<int>(this->NumberOfScalarComponents));
  }

  this->SetFrameField(SEQMETA_FIELD_US_IMG_ORIENT, igsioVideoFrame::GetStringFromUsImageOrientation(US_IMG_ORIENT_MF));
  // Image orientation
  if (this->EnableImageDataWrite)
  {
    std::string orientationStr = igsioVideoFrame::GetStringFromUsImageOrientation(this->ImageOrientationInFile);
    this->SetFrameField(SEQMETA_FIELD_US_IMG_ORIENT, orientationStr);
  }

  // Image type
  if (this->EnableImageDataWrite)
  {
    std::string typeStr = igsioVideoFrame::GetStringFromUsImageType(this->ImageType);
    this->SetFrameField(SEQMETA_FIELD_US_IMG_TYPE, typeStr);
  }

  // Add fields with default values if they are not present already
  int numSpaceDimensions = isData3D ? 3 : 2;
  if (this->Output2DDataWithZDimensionIncluded)
  {
    numSpaceDimensions++;
  }
  if (isDataTimeSeries)
  {
    numSpaceDimensions++;
  }

  if (GetCustomString("TransformMatrix") == NULL
      || igsioCommon::SplitStringIntoTokens(GetCustomString("TransformMatrix"), ' ', false).size() / numSpaceDimensions != numSpaceDimensions)
  {
    // Dynamically calculate identity matrix for given spatial dimensions
    std::ostringstream transformMatrixStream;
    for (int i = 0; i < numSpaceDimensions; ++i)
    {
      for (int j = 0; j < numSpaceDimensions; ++j)
      {
        if (i == j)
        {
          transformMatrixStream << "1";
        }
        else
        {
          transformMatrixStream << "0";
        }
        if (j != numSpaceDimensions - 1)
        {
          transformMatrixStream << " ";
        }
      }
      if (i != numSpaceDimensions - 1)
      {
        transformMatrixStream << " ";
      }
    }
    this->SetFrameField("TransformMatrix", transformMatrixStream.str());
  }

  if (GetCustomString("Offset") == NULL
      || igsioCommon::SplitStringIntoTokens(GetCustomString("Offset"), ' ', false).size() != numSpaceDimensions)
  {
    // Dynamically calculate offset dimensions
    std::ostringstream offsetStream;
    for (int i = 0; i < numSpaceDimensions; ++i)
    {
      // Code assumes all images have the same origin
      vtkImageData* image = this->TrackedFrameList->GetTrackedFrame(0)->GetImageData()->GetImage();
      if (image)
      {
        offsetStream << image->GetOrigin()[i];
      }
      else
      {
        offsetStream << "0";
      }

      if (i != numSpaceDimensions - 1)
      {
        offsetStream << " ";
      }
    }
    this->SetFrameField("Offset", offsetStream.str());
  }
  if (GetCustomString("CenterOfRotation") == NULL
      || igsioCommon::SplitStringIntoTokens(GetCustomString("CenterOfRotation"), ' ', false).size() != numSpaceDimensions)
  {
    // Dynamically calculate center of rotation position
    std::ostringstream rotationStream;
    for (int i = 0; i < numSpaceDimensions; ++i)
    {
      rotationStream << "0";
      if (i != numSpaceDimensions - 1)
      {
        rotationStream << " ";
      }
    }
    this->SetFrameField("CenterOfRotation", rotationStream.str());
  }

  if (GetCustomString("ElementSpacing") == NULL
      || igsioCommon::SplitStringIntoTokens(GetCustomString("ElementSpacing"), ' ', false).size() != numSpaceDimensions)
  {
    // Dynamically calculate the spacing values
    std::ostringstream spacingStream;
    for (int i = 0; i < numSpaceDimensions; ++i)
    {
      // Code assumes all images have the same spacing
      vtkImageData* image = this->TrackedFrameList->GetTrackedFrame(0)->GetImageData()->GetImage();
      if (image)
      {
        spacingStream << image->GetSpacing()[i];
      }
      else
      {
        spacingStream << "1";
      }

      if (i != numSpaceDimensions - 1)
      {
        spacingStream << " ";
      }
    }
    this->SetFrameField("ElementSpacing", spacingStream.str());
  }

  if (GetCustomString("AnatomicalOrientation") == NULL)
  {
    this->SetFrameField("AnatomicalOrientation", "RAI");
  }

  FILE* stream = NULL;
  // open in binary mode because we determine the start of the image buffer also during this read
  if (FileOpen(&stream, this->TempHeaderFileName.c_str(), "wb") != IGSIO_SUCCESS)
  {
    LOG_ERROR("The file " << this->TempHeaderFileName << " could not be opened for writing");
    return IGSIO_FAIL;
  }

  // The header shall start with these two fields
  const char* objType = "ObjectType = Image\n";
  fputs(objType, stream);
  this->TotalBytesWritten += strlen(objType);

  std::stringstream nDimsFieldStream;
  nDimsFieldStream << "NDims = " << this->NumberOfDimensions << std::endl;
  fputs(nDimsFieldStream.str().c_str(), stream);
  this->TotalBytesWritten += nDimsFieldStream.str().length();

  std::vector<std::string> fieldNames;
  this->TrackedFrameList->GetCustomFieldNameList(fieldNames);
  for (std::vector<std::string>::iterator it = fieldNames.begin(); it != fieldNames.end(); it++)
  {
    if (igsioCommon::IsEqualInsensitive(*it, "ObjectType"))
    {
      continue;  // this must be the first element
    }
    if (igsioCommon::IsEqualInsensitive(*it, "NDims"))
    {
      continue;  // this must be the second element
    }
    if (igsioCommon::IsEqualInsensitive(*it, "ElementDataFile"))
    {
      continue;  // this must be the last element
    }
    std::string field = (*it) + " = " + GetCustomString(it->c_str()) + "\n";
    fputs(field.c_str(), stream);
    this->TotalBytesWritten += field.length();
  }

  fclose(stream);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::AppendImagesToHeader()
{
  FILE* stream = NULL;
  // open in binary mode because we determine the start of the image buffer also during this read
  if (FileOpen(&stream, this->TempHeaderFileName.c_str(), "ab+") != IGSIO_SUCCESS)
  {
    LOG_ERROR("The file " << this->TempHeaderFileName << " could not be opened for appending.");
    return IGSIO_FAIL;
  }

  // Write frame fields (Seq_Frame0000_... = ...)
  for (unsigned int frameNumber = this->CurrentFrameOffset; frameNumber < this->TrackedFrameList->GetNumberOfTrackedFrames() + this->CurrentFrameOffset; frameNumber++)
  {
    LOG_DEBUG("Writing frame " << frameNumber);
    unsigned int adjustedFrameNumber = frameNumber - this->CurrentFrameOffset;
    igsioTrackedFrame* trackedFrame = this->TrackedFrameList->GetTrackedFrame(adjustedFrameNumber);

    std::ostringstream frameIndexStr;
    frameIndexStr << std::setfill('0') << std::setw(4) << frameNumber;

    std::vector<std::string> fieldNames;
    trackedFrame->GetFrameFieldNameList(fieldNames);

    for (std::vector<std::string>::iterator it = fieldNames.begin(); it != fieldNames.end(); it++)
    {
      std::string field = SEQMETA_FIELD_FRAME_FIELD_PREFIX + frameIndexStr.str() + "_" + (*it) + " = " + trackedFrame->GetFrameField(it->c_str()) + "\n";
      fputs(field.c_str(), stream);
      TotalBytesWritten += field.length();
    }
    //Only write this field if the image is saved. If only the tracking pose is kept do not save this field to the header
    if (this->EnableImageDataWrite)
    {
      // Add image status field
      std::string imageStatus("OK");
      if (!trackedFrame->GetImageData()->IsImageValid())
      {
        imageStatus = "INVALID";
      }
      std::string imgStatusField = SEQMETA_FIELD_FRAME_FIELD_PREFIX + frameIndexStr.str() + "_" + SEQMETA_FIELD_IMG_STATUS + " = " + imageStatus + "\n";
      fputs(imgStatusField.c_str(), stream);
      TotalBytesWritten += imgStatusField.length();
    }
  }

  fclose(stream);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::FinalizeHeader()
{
  FILE* stream = NULL;
  // open in binary mode because we determine the start of the image buffer also during this read
  if (FileOpen(&stream, this->TempHeaderFileName.c_str(), "ab+") != IGSIO_SUCCESS)
  {
    LOG_ERROR("The file " << this->TempHeaderFileName << " could not be opened for appending.");
    return IGSIO_FAIL;
  }

  std::string dataFileStr;
  if (this->PixelDataFileName.empty())
  {
    dataFileStr = std::string(SEQMETA_FIELD_VALUE_ELEMENT_DATA_FILE_LOCAL);
  }
  else
  {
    dataFileStr = this->PixelDataFileName;
  }

  std::string elem = "ElementDataFile = " + dataFileStr + "\n";
  fputs(elem.c_str(), stream);
  TotalBytesWritten += elem.size();

  fclose(stream);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::WriteCompressedImagePixelsToFile(int& compressedDataSize)
{
  LOG_DEBUG("Writing compressed pixel data into file started");

  compressedDataSize = 0;

  const int outputBufferSize = 16384; // can be any number, just picked a value from a zlib example
  unsigned char outputBuffer[outputBufferSize];

  z_stream strm; // stream describing the compression state

  // use the default memory allocation routines
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  int ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK)
  {
    LOG_ERROR("Image compression initialization failed (errorCode=" << ret << ")");
    return IGSIO_FAIL;
  }

  // Create a blank frame if we have to write an invalid frame to metafile
  igsioVideoFrame blankFrame;
  FrameSizeType frameSize = { this->Dimensions[0], this->Dimensions[1], this->Dimensions[2] };
  if (blankFrame.AllocateFrame(frameSize, this->PixelType, this->NumberOfScalarComponents) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to allocate space for blank image.");
    return IGSIO_FAIL;
  }
  blankFrame.FillBlank();

  for (unsigned int frameNumber = 0; frameNumber < this->TrackedFrameList->GetNumberOfTrackedFrames(); frameNumber++)
  {
    igsioTrackedFrame* trackedFrame(NULL);

    if (this->EnableImageDataWrite)
    {
      trackedFrame = this->TrackedFrameList->GetTrackedFrame(frameNumber);
      if (trackedFrame == NULL)
      {
        LOG_ERROR("Cannot access frame " << frameNumber << " while trying to writing compress data into file");
        deflateEnd(&strm);
        return IGSIO_FAIL;
      }
    }

    igsioVideoFrame* videoFrame = &blankFrame;
    if (this->EnableImageDataWrite)
    {
      if (trackedFrame->GetImageData()->IsImageValid())
      {
        videoFrame = trackedFrame->GetImageData();
      }
    }

    strm.next_in = (Bytef*)videoFrame->GetScalarPointer();
    strm.avail_in = videoFrame->GetFrameSizeInBytes();

    // Note: it's possible to request to consume all inputs and delete all history after each frame writing to allow random access
    int flush = (frameNumber < this->TrackedFrameList->GetNumberOfTrackedFrames() - 1) ? Z_NO_FLUSH : Z_FINISH;

    // run deflate() on input until output buffer not full, finish
    // compression if all of source has been read in
    do
    {
      strm.avail_out = outputBufferSize;
      strm.next_out = outputBuffer;

      ret = deflate(&strm, flush);    /* no bad return value */
      if (ret == Z_STREAM_ERROR)
      {
        // state clobbered
        LOG_ERROR("Zlib state became invalid during the compression process (errorCode=" << ret << ")");
        deflateEnd(&strm);   // clean up
        return IGSIO_FAIL;
      }

      size_t numberOfBytesReadyForWriting = outputBufferSize - strm.avail_out;
      size_t numberOfBytesWritten = 0;
      if (igsioCommon::RobustFwrite(this->OutputImageFileHandle, outputBuffer, numberOfBytesReadyForWriting, numberOfBytesWritten) != IGSIO_SUCCESS)
      {
        LOG_ERROR("Error writing compressed data into file");
        deflateEnd(&strm);   // clean up
        return IGSIO_FAIL;
      }
      compressedDataSize += numberOfBytesWritten;

    }
    while (strm.avail_out == 0);

    if (strm.avail_in != 0)
    {
      // state clobbered (by now all input should have been consumed)
      LOG_ERROR("Zlib state became invalid during the compression process");
      deflateEnd(&strm);   // clean up
      return IGSIO_FAIL;
    }
  }

  deflateEnd(&strm);   // clean up

  LOG_DEBUG("Writing compressed pixel data into file completed");

  if (ret != Z_STREAM_END)
  {
    LOG_ERROR("Error occurred during compressing image data into file");
    return IGSIO_FAIL;
  }
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::ConvertMetaElementTypeToVtkPixelType(const std::string& elementTypeStr, igsioCommon::VTKScalarPixelType& vtkPixelType)
{
  if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_OTHER")
      || igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_NONE")
      || elementTypeStr.empty())
  {
    vtkPixelType = VTK_VOID;
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_CHAR"))
  {
    vtkPixelType = VTK_CHAR;
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_ASCII_CHAR"))
  {
    vtkPixelType = VTK_CHAR;
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_UCHAR"))
  {
    vtkPixelType = VTK_UNSIGNED_CHAR;
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_SHORT"))
  {
    vtkPixelType = VTK_SHORT;
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_USHORT"))
  {
    vtkPixelType = VTK_UNSIGNED_SHORT;
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_INT"))
  {
    vtkPixelType = VTK_INT;
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_UINT"))
  {
    if (sizeof(unsigned int) == MET_ValueTypeSize[MET_UINT])
    {
      vtkPixelType = VTK_UNSIGNED_INT;
    }
    else if (sizeof(unsigned long) == MET_ValueTypeSize[MET_UINT])
    {
      vtkPixelType = VTK_UNSIGNED_LONG;
    }
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_LONG"))
  {
    if (sizeof(unsigned int) == MET_ValueTypeSize[MET_LONG])
    {
      vtkPixelType = VTK_LONG;
    }
    else if (sizeof(unsigned long) == MET_ValueTypeSize[MET_UINT])
    {
      vtkPixelType = VTK_INT;
    }
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_ULONG"))
  {
    if (sizeof(unsigned long) == MET_ValueTypeSize[MET_ULONG])
    {
      vtkPixelType = VTK_UNSIGNED_LONG;
    }
    else if (sizeof(unsigned int) == MET_ValueTypeSize[MET_ULONG])
    {
      vtkPixelType = VTK_UNSIGNED_INT;
    }
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_LONG_LONG"))
  {
    if (sizeof(long) == MET_ValueTypeSize[MET_LONG_LONG])
    {
      vtkPixelType = VTK_LONG;
    }
    else if (sizeof(int) == MET_ValueTypeSize[MET_LONG_LONG])
    {
      vtkPixelType = VTK_INT;
    }
    else
    {
      vtkPixelType = VTK_VOID;
    }
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_ULONG_LONG"))
  {
    if (sizeof(unsigned long) == MET_ValueTypeSize[MET_ULONG_LONG])
    {
      vtkPixelType = VTK_UNSIGNED_LONG;
    }
    else if (sizeof(unsigned int) == MET_ValueTypeSize[MET_ULONG_LONG])
    {
      vtkPixelType = VTK_UNSIGNED_INT;
    }
    else
    {
      vtkPixelType = VTK_VOID;
    }
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_FLOAT"))
  {
    if (sizeof(float) == MET_ValueTypeSize[MET_FLOAT])
    {
      vtkPixelType = VTK_FLOAT;
    }
    else if (sizeof(double) == MET_ValueTypeSize[MET_FLOAT])
    {
      vtkPixelType = VTK_DOUBLE;
    }
  }
  else if (igsioCommon::IsEqualInsensitive(elementTypeStr, "MET_DOUBLE"))
  {
    vtkPixelType = VTK_DOUBLE;
    if (sizeof(double) == MET_ValueTypeSize[MET_DOUBLE])
    {
      vtkPixelType = VTK_DOUBLE;
    }
    else if (sizeof(float) == MET_ValueTypeSize[MET_DOUBLE])
    {
      vtkPixelType = VTK_FLOAT;
    }
  }
  else
  {
    LOG_ERROR("Unknown component type: " << elementTypeStr);
    vtkPixelType = VTK_VOID;
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::ConvertVtkPixelTypeToMetaElementType(igsioCommon::VTKScalarPixelType vtkPixelType, std::string& elementTypeStr)
{
  if (vtkPixelType == VTK_VOID)
  {
    elementTypeStr = "MET_OTHER";
    return IGSIO_SUCCESS;
  }
  const char* metaElementTypes[] =
  {
    "MET_CHAR",
    "MET_UCHAR",
    "MET_SHORT",
    "MET_USHORT",
    "MET_INT",
    "MET_UINT",
    "MET_LONG",
    "MET_ULONG",
    "MET_LONG_LONG",
    "MET_ULONG_LONG",
    "MET_FLOAT",
    "MET_DOUBLE",
  };

  igsioCommon::VTKScalarPixelType testedPixelType = VTK_VOID;
  for (unsigned int i = 0; i < sizeof(metaElementTypes); i++)
  {
    if (ConvertMetaElementTypeToVtkPixelType(metaElementTypes[i], testedPixelType) != IGSIO_SUCCESS)
    {
      continue;
    }
    if (testedPixelType == vtkPixelType)
    {
      elementTypeStr = metaElementTypes[i];
      return IGSIO_SUCCESS;
    }
  }
  elementTypeStr = "MET_OTHER";
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::UpdateFieldInImageHeader(const char* fieldName)
{
  if (fieldName == NULL)
  {
    LOG_ERROR("NULL fieldname sent to vtkIGSIOMetaImageSequenceIO::UpdateFieldInImageHeader");
    return IGSIO_FAIL;
  }

  if (this->TempHeaderFileName.empty())
  {
    LOG_ERROR("Cannot update file header, filename is invalid");
    return IGSIO_FAIL;
  }

  // open in read+write
#if _MSC_VER <= 1500
  std::fstream stream(this->TempHeaderFileName.c_str(), std::ios::in | std::ios::out | std::ios::binary);
#else
  std::fstream stream(this->TempHeaderFileName, std::ios::in | std::ios::out | std::ios::binary);
#endif

  if (!stream)
  {
    LOG_ERROR("The file " << this->TempHeaderFileName << " could not be opened for reading and writing");
    return IGSIO_FAIL;
  }

  std::string line;
  while (std::getline(stream, line))
  {
    // Split line into name and value
    size_t equalSignFound;
    equalSignFound = line.find_first_of("=");
    if (equalSignFound == std::string::npos)
    {
      LOG_WARNING("Parsing line failed, equal sign is missing (" << line << ")");
      continue;
    }
    std::string name = igsioCommon::Trim(line.substr(0, equalSignFound));

    if (igsioCommon::IsEqualInsensitive(name, fieldName))
    {
      // found the field that has to be updated

      // construct a new line with the updated value
      std::ostringstream newLineStr;
      newLineStr << name << " = " << GetCustomString(name.c_str());

      // need to add padding whitespace characters to fully replace the old line
      int paddingCharactersNeeded = line.length() - newLineStr.str().size();
      if (paddingCharactersNeeded < 0)
      {
        LOG_ERROR("Cannot update line in image header (the new string '" << newLineStr.str() << "' is longer than the current string '" << line << "')");
        return IGSIO_FAIL;
      }
      for (int i = 0; i < paddingCharactersNeeded; i++)
      {
        newLineStr << " ";
      }

      // rewind to file pointer the first character of the line
      stream.seekp(-std::ios::off_type(line.size()) - 1, std::ios_base::cur);

      // overwrite the old line
      if (!(stream << newLineStr.str()))
      {
        LOG_ERROR("Cannot update line in image header (writing the updated line into the file failed)");
        return IGSIO_FAIL;
      }

      return IGSIO_SUCCESS;
    }
    else if (igsioCommon::IsEqualInsensitive(name, SEQMETA_FIELD_ELEMENT_DATA_FILE))
    {
      // this is guaranteed to be the last line in the header
      break;
    }
  }

  LOG_ERROR("Field " << fieldName << " is not found in the header file, update with new value is failed:");
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
const char* vtkIGSIOMetaImageSequenceIO::GetDimensionSizeString()
{
  return SEQMETA_FIELD_DIMSIZE;
}

//----------------------------------------------------------------------------
const char* vtkIGSIOMetaImageSequenceIO::GetDimensionKindsString()
{
  return SEQMETA_FIELD_KINDS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::Close()
{
  // Update fields that are known only at the end of the processing
  if (this->GetUseCompression())
  {
    std::stringstream ss;
    ss << this->CompressedBytesWritten;
    this->SetFrameField(SEQMETA_FIELD_COMPRESSED_DATA_SIZE, ss.str());
    if (UpdateFieldInImageHeader(SEQMETA_FIELD_COMPRESSED_DATA_SIZE) != IGSIO_SUCCESS)
    {
      return IGSIO_FAIL;
    }
    deflateEnd(&this->CompressionStream);   // clean up
  }

  fclose(this->OutputImageFileHandle);

  return Superclass::Close();
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::SetFileName(const std::string& aFilename)
{
  this->FileName.clear();
  this->PixelDataFileName.clear();

  if (aFilename.empty())
  {
    LOG_ERROR("Invalid metaimage file name");
  }

  this->FileName = aFilename;
  // Trim whitespace and " characters from the beginning and end of the filename
  this->FileName = igsioCommon::Trim(aFilename);

  // Set pixel data filename at the same time
  std::string fileExt = vtksys::SystemTools::GetFilenameLastExtension(this->FileName);
  if (STRCASECMP(fileExt.c_str(), ".mha") == 0)
  {
    this->PixelDataFileName = "";
  }
  else if (STRCASECMP(fileExt.c_str(), ".mhd") == 0)
  {
    std::string pixFileName = vtksys::SystemTools::GetFilenameWithoutExtension(this->FileName);
    if (this->UseCompression)
    {
      pixFileName += ".zraw";
    }
    else
    {
      pixFileName += ".raw";
    }

    // Use only the file name of pixFileName for PixelDataFile.
    // To support compatibility of a saved *.mhd file on Linux or Mac OSX machine.
    this->PixelDataFileName = pixFileName;
  }
  else
  {
    LOG_WARNING("Writing sequence metafile with '" << fileExt << "' extension is not supported. Using mha extension instead.");
    std::string fileNameWithMhaExt = vtksys::SystemTools::GetFilenameWithoutExtension(this->FileName) + ".mha";

    // Use the same path as the header but replace the filename
    std::vector<std::string> pathElements;
    vtksys::SystemTools::SplitPath(this->FileName.c_str(), pathElements);
    pathElements.erase(pathElements.end() - 1);
    pathElements.push_back(fileNameWithMhaExt);
    this->FileName = vtksys::SystemTools::JoinPath(pathElements);

    this->PixelDataFileName = "";
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMetaImageSequenceIO::UpdateDimensionsCustomStrings(int numberOfFrames, bool isData3D)
{
  if (this->EnableImageDataWrite && this->TrackedFrameList->IsContainingValidImageData())
  {
    this->NumberOfScalarComponents = this->TrackedFrameList->GetNumberOfScalarComponents();
  }

  std::stringstream sizesStr;
  std::stringstream kindStr;

  if (this->NumberOfScalarComponents > 1)
  {
    sizesStr << this->NumberOfScalarComponents << " ";
    kindStr << "vector" << " ";
  }

  int entries = (isData3D ? 3 : 2) + (numberOfFrames > 1 ? 1 : 0);

  this->Dimensions[3] = numberOfFrames;
  // Write out all but the last entry
  for (int i = 0; i < entries - 1; ++i)
  {
    kindStr << "domain" << " ";
    sizesStr << this->Dimensions[i] << " ";
  }

  // pad kind string with spaces then append last entry
  int lastDimension = (numberOfFrames > 1 ? 3 : (isData3D ? 2 : 1));
  std::string lastKind;
  if (numberOfFrames > 1)
  {
    lastKind = "list";
  }
  else
  {
    lastKind = "domain";
  }

  {
    std::stringstream ss;
    ss << this->Dimensions[lastDimension];
    // strlen(SEQUENCE_FIELD_SIZES) + 2 for "DimSize = "
    int numchars = sizesStr.str().length() + ss.str().length() + strlen(SEQMETA_FIELD_DIMSIZE) + 3;
    for (int i = 0; i < SEQMETA_FIELD_PADDED_LINE_LENGTH - numchars; ++i)
    {
      sizesStr << " ";
    }
    sizesStr << this->Dimensions[lastDimension];
  }

  // strlen(SEQUENCE_FIELD_SIZES) + 3 for "Kinds = "
  int numchars = kindStr.str().length() + lastKind.length() + strlen(SEQMETA_FIELD_KINDS) + 3;
  for (int i = 0; i < SEQMETA_FIELD_PADDED_LINE_LENGTH - numchars; ++i)
  {
    kindStr << " ";
  }
  kindStr << lastKind;

  this->SetFrameField(SEQMETA_FIELD_DIMSIZE, sizesStr.str());
  this->SetFrameField(SEQMETA_FIELD_KINDS, kindStr.str());

  return IGSIO_SUCCESS;
}
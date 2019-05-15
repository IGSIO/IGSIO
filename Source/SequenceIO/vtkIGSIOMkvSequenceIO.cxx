/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#include "vtkIGSIOMkvSequenceIO.h"

// IGSIO includes
#include "igsioTrackedFrame.h"
#include "vtkIGSIOTrackedFrameList.h"

// VTK includes
#include <vtkImageMapToColors.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>

// libwebm includes
#include <mkvwriter.h>
#include <mkvparser.h>
#include <mkvreader.h>

// std includes
#include <errno.h>

#include <vtkStreamingVolumeFrame.h>

#define NANOSECONDS_IN_SECOND 1000000000.0
#define SECONDS_IN_NANOSECOND 1.0/NANOSECONDS_IN_SECOND

#define VTKSEQUENCEIO_MKV_UNCOMPRESSED_CODECID "V_UNCOMPRESSED"
#define VTKSEQUENCEIO_MKV_VP8_CODECID "V_VP8"
#define VTKSEQUENCEIO_MKV_VP9_CODECID "V_VP9"
#define VTKSEQUENCEIO_MKV_H264_CODECID "V_H264"

#define VTKSEQUENCEIO_RGB2_FOURCC "RGB2"
#define VTKSEQUENCEIO_RV24_FOURCC "RV24"
#define VTKSEQUENCEIO_RGB24_FOURCC "RGB"
#define VTKSEQUENCEIO_GRAYSCALE8_FOURCC "Y800"
#define VTKSEQUENCEIO_VP8_FOURCC "VP80"
#define VTKSEQUENCEIO_VP9_FOURCC "VP90"
#define VTKSEQUENCEIO_H264_FOURCC "H264"

struct MetaData
{
  std::string Name;
  std::string Value;
  MetaData(std::string name, std::string value)
    : Name(name)
    , Value(value)
  {};
};
typedef std::map<double, std::vector<MetaData> > MetaDataInfo;

struct FrameInfo
{
  vtkSmartPointer<vtkUnsignedCharArray> Data;
  double         TimestampSeconds;
  bool           IsKey;
  FrameInfo()
    : Data(NULL)
    , TimestampSeconds(0.0)
    , IsKey(false)
  {};
};
typedef std::vector<FrameInfo> FrameInfoList;

struct VideoTrackInfo
{
  VideoTrackInfo() {}
  VideoTrackInfo(std::string name, std::string encoding, std::string colourSpace, unsigned long width, unsigned long height, double frameRate, bool isGreyScale)
    : Name(name)
    , Encoding(encoding)
    , ColourSpace(colourSpace)
    , Width(width)
    , Height(height)
    , FrameRate(frameRate)
    , IsGreyScale(isGreyScale)
  {}
  ~VideoTrackInfo() {}
  std::string Name;
  std::string Encoding;
  std::string ColourSpace;
  unsigned long Width;
  unsigned long Height;
  double FrameRate;
  bool IsGreyScale;
};
typedef std::map <int, VideoTrackInfo> VideoTrackMap;


struct MetadataTrackInfo
{
  MetadataTrackInfo() {};
  MetadataTrackInfo(std::string name, std::string encoding)
    : Name(name)
    , Encoding(encoding)
  {}
  std::string Name;
  std::string Encoding;
};
typedef std::map <int, MetadataTrackInfo> MetadataTrackMap;

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkIGSIOMkvSequenceIO);

class vtkIGSIOMkvSequenceIO::vtkInternal
{
public:
  //---------------------------------------------------------------------------
  vtkInternal(vtkIGSIOMkvSequenceIO* external)
    : External(external)
    , ReaderInitialized(false)
    , WriterInitialized(false)
    , InitialTimestamp(-1)
    , EBMLHeader(NULL)
    , MKVWriter(NULL)
    , MKVWriteSegment(NULL)
    , MKVReader(NULL)
    , MKVReadSegment(NULL)
  {
  };

  //---------------------------------------------------------------------------
  ~vtkInternal()
  {
  }

  //
  virtual bool FourCCRequiresEncoding(std::string fourCC)
  {
    if (fourCC == "RV24")
    {
      return false;
    }
    return true;
  }

  //
  static std::string FourCCToCodecId(std::string fourCC)
  {
    if (fourCC == VTKSEQUENCEIO_RGB2_FOURCC)
    {
      return VTKSEQUENCEIO_MKV_UNCOMPRESSED_CODECID;
    }
    else if (fourCC == VTKSEQUENCEIO_RV24_FOURCC)
    {
      return VTKSEQUENCEIO_MKV_UNCOMPRESSED_CODECID;
    }
    else if (fourCC == VTKSEQUENCEIO_RGB24_FOURCC)
    {
      return VTKSEQUENCEIO_MKV_UNCOMPRESSED_CODECID;
    }
    else if (fourCC == VTKSEQUENCEIO_VP8_FOURCC)
    {
      return VTKSEQUENCEIO_MKV_VP8_CODECID;
    }
    else if (fourCC == VTKSEQUENCEIO_VP9_FOURCC)
    {
      return VTKSEQUENCEIO_MKV_VP9_CODECID;
    }
    else if (fourCC == VTKSEQUENCEIO_H264_FOURCC)
    {
      return VTKSEQUENCEIO_MKV_H264_CODECID;
    }

    return "";
  };

  //
  static std::string CodecIdToFourCC(std::string codecId)
  {
    if (codecId == VTKSEQUENCEIO_MKV_UNCOMPRESSED_CODECID)
    {
      return VTKSEQUENCEIO_RV24_FOURCC;
    }
    else if (codecId == VTKSEQUENCEIO_MKV_VP9_CODECID)
    {
      return VTKSEQUENCEIO_VP9_FOURCC;
    }
    else if (codecId == VTKSEQUENCEIO_MKV_VP8_CODECID)
    {
      return VTKSEQUENCEIO_VP8_FOURCC;
    }
    else if (codecId == VTKSEQUENCEIO_MKV_H264_CODECID)
    {
      return VTKSEQUENCEIO_H264_FOURCC;
    }

    return "";
  };

  //
  virtual void Close();

  //--------------------------------------
  // Writing methods
  //
  virtual int AddVideoTrack(std::string name, std::string encodingFourCC, int width, int height, std::string language = "und");
  //
  virtual int AddMetadataTrack(std::string name, std::string language = "und");
  //
  virtual bool WriteHeader();
  //
  virtual bool WriteFrame(unsigned char* frameData, uint64_t size, bool frameType, int trackNumber, double timestampSeconds);
  //
  virtual bool WriteMetadata(std::string metadata, int trackNumber, double timestampSeconds = 0.0, double durationSeconds = SECONDS_IN_NANOSECOND);

  //--------------------------------------
  // Reading methods
  //
  virtual bool ReadHeader();
  //
  virtual bool ReadVideoData();
  //
  virtual bool ReadMetadata();

  vtkIGSIOMkvSequenceIO* External;

  bool ReaderInitialized;
  bool WriterInitialized;
  double FrameRate;
  int VideoTrackNumber;

  double InitialTimestamp;
  std::string EncodingFourCC;
  double InitialTimestampSeconds;
  std::map<std::string, uint64_t> FrameFieldTracks;

  VideoTrackMap    VideoTracks;
  MetadataTrackMap MetadataTracks;
  std::map<std::string, int> VideoNameToTrackMap;

  mkvparser::EBMLHeader* EBMLHeader;
  mkvmuxer::MkvWriter* MKVWriter;
  mkvmuxer::Segment* MKVWriteSegment;
  mkvparser::MkvReader* MKVReader;
  mkvparser::Segment* MKVReadSegment;
};

//----------------------------------------------------------------------------
vtkIGSIOMkvSequenceIO::vtkIGSIOMkvSequenceIO()
  : Internal(new vtkInternal(this))
{
}

//----------------------------------------------------------------------------
vtkIGSIOMkvSequenceIO::~vtkIGSIOMkvSequenceIO()
{
  this->Close();

  delete Internal;
  this->Internal = NULL;
}

//----------------------------------------------------------------------------
void vtkIGSIOMkvSequenceIO::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMkvSequenceIO::ReadImageHeader()
{
  if (!this->Internal->ReadHeader())
  {
    LOG_ERROR("Could not read mkv header!");
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
// Read the spacing and dimensions of the image.
igsioStatus vtkIGSIOMkvSequenceIO::ReadImagePixels()
{
  if (!this->Internal->ReadVideoData())
  {
    LOG_ERROR("Could not read mkv video!");
    return IGSIO_FAIL;
  }

  if (!this->Internal->ReadMetadata())
  {
    LOG_ERROR("Could not read mkv metadata!");
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMkvSequenceIO::PrepareImageFile()
{
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
bool vtkIGSIOMkvSequenceIO::CanReadFile(const std::string& filename)
{
  if (igsioCommon::IsEqualInsensitive(vtksys::SystemTools::GetFilenameLastExtension(filename), ".mkv"))
  {
    return true;
  }

  if (igsioCommon::IsEqualInsensitive(vtksys::SystemTools::GetFilenameLastExtension(filename), ".webm"))
  {
    return true;
  }

  return false;
}

//----------------------------------------------------------------------------
bool vtkIGSIOMkvSequenceIO::CanWriteFile(const std::string& filename)
{
  if (igsioCommon::IsEqualInsensitive(vtksys::SystemTools::GetFilenameLastExtension(filename), ".mkv"))
  {
    return true;
  }
  if (igsioCommon::IsEqualInsensitive(vtksys::SystemTools::GetFilenameLastExtension(filename), ".webm"))
  {
    return true;
  }

  return false;
}

//----------------------------------------------------------------------------
/** Assumes SetFileName has been called with a valid file name. */
igsioStatus vtkIGSIOMkvSequenceIO::WriteInitialImageHeader()
{
  if (this->TrackedFrameList->Size() == 0)
  {
    LOG_ERROR("Could not write MKV header, no tracked frames");
    return IGSIO_FAIL;
  }

  std::string trackName = this->TrackedFrameList->GetFrameField("TrackName");
  if (trackName.empty())
  {
    trackName = "Video";
  }

  igsioTrackedFrame* frame = this->TrackedFrameList->GetTrackedFrame(0);
  if (!frame)
  {
    LOG_ERROR("Could not write MKV header, frame is missing");
    return IGSIO_FAIL;
  }

  igsioVideoFrame* videoFrame = frame->GetImageData();
  if (!videoFrame)
  {
    LOG_ERROR("Could not write MKV header, video frame is missing");
    return IGSIO_FAIL;
  }

  FrameSizeType frameSize = frame->GetFrameSize();
  std::string encodingFourCC = "";
  if (!videoFrame->IsFrameEncoded())
  {
    vtkImageData* image = videoFrame->GetImage();
    if (image->GetScalarType() != VTK_UNSIGNED_CHAR && image->GetScalarType() != VTK_CHAR)
    {
      LOG_ERROR("Only supported image type is unsigned char");
      return IGSIO_FAIL;
    }

    int numberOfComponents = image->GetNumberOfScalarComponents();


    if (numberOfComponents == 1)
    {
      encodingFourCC = VTKSEQUENCEIO_GRAYSCALE8_FOURCC;
    }
    else if (numberOfComponents == 3)
    {
      encodingFourCC = VTKSEQUENCEIO_RV24_FOURCC;
    }
    else
    {
      LOG_ERROR("Number of components in image must be 1 or 2");
    }
  }
  else
  {
    encodingFourCC = frame->GetEncodingFourCC();
  }

  if (!this->Internal->WriteHeader())
  {
    LOG_ERROR("Could not write MKV header!");
  }

  this->Internal->VideoTrackNumber = this->Internal->AddVideoTrack(trackName, encodingFourCC, frameSize[0], frameSize[1]);
  if (this->Internal->VideoTrackNumber < 1)
  {
    LOG_ERROR("Could not create video track!");
    return IGSIO_FAIL;
  }

  // All tracks have to be initialized before starting
  // All desired custom frame field names must be availiable starting from frame 0
  for (unsigned int frameNumber = 0; frameNumber < this->TrackedFrameList->GetNumberOfTrackedFrames(); frameNumber++)
  {
    igsioTrackedFrame* trackedFrame(NULL);
    trackedFrame = this->TrackedFrameList->GetTrackedFrame(frameNumber);
    std::vector<std::string> fieldNames;
    trackedFrame->GetFrameFieldNameList(fieldNames);
    for (std::vector<std::string>::iterator it = fieldNames.begin(); it != fieldNames.end(); it++)
    {
      uint64_t trackNumber = this->Internal->FrameFieldTracks[*it];
      if (trackNumber <= 0)
      {
        int metaDataTrackNumber = this->Internal->AddMetadataTrack((*it));
        if (metaDataTrackNumber > 0)
        {
          this->Internal->FrameFieldTracks[*it] = metaDataTrackNumber;
        }
      }
    }
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMkvSequenceIO::AppendImagesToHeader()
{
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMkvSequenceIO::WriteImages()
{
  // Do the actual writing
  if (!this->Internal->WriterInitialized)
  {
    if (this->WriteInitialImageHeader() != IGSIO_SUCCESS)
    {
      return IGSIO_FAIL;
    }
    this->Internal->WriterInitialized = true;
  }

  FrameSizeType frameSize = { 0, 0, 0 };
  this->TrackedFrameList->GetFrameSize(frameSize);

  for (unsigned int frameNumber = 0; frameNumber < this->TrackedFrameList->GetNumberOfTrackedFrames(); frameNumber++)
  {
    igsioTrackedFrame* trackedFrame(NULL);
    igsioVideoFrame* videoFrame(NULL);

    if (this->EnableImageDataWrite)
    {
      trackedFrame = this->TrackedFrameList->GetTrackedFrame(frameNumber);
      if (!trackedFrame)
      {
        LOG_ERROR("Cannot access frame " << frameNumber << " while trying to writing compress data into file");
        continue;
      }

      videoFrame = trackedFrame->GetImageData();
      if (!videoFrame)
      {
        LOG_ERROR("Cannot find image in frame " << frameNumber << " while trying to writing compress data into file");
        continue;
      }

      if (this->Internal->InitialTimestamp == -1)
      {
        this->Internal->InitialTimestamp = trackedFrame->GetTimestamp();
      }

      double timestamp = trackedFrame->GetTimestamp() - this->Internal->InitialTimestamp;

      if (!videoFrame->IsFrameEncoded())
      {
        vtkImageData* image = videoFrame->GetImage();
        int numberOfComponents = image->GetNumberOfScalarComponents();
        uint64_t size = frameSize[0] * frameSize[1] * frameSize[2] * numberOfComponents * image->GetScalarSize();
        this->Internal->WriteFrame((unsigned char*)image->GetScalarPointer(), size, true, this->Internal->VideoTrackNumber, timestamp);
      }
      else
      {
        vtkStreamingVolumeFrame* encodedFrame = videoFrame->GetEncodedFrame();
        if (!encodedFrame || !encodedFrame->GetFrameData())
        {
          LOG_ERROR("Error writing to file: " << this->FileName << ". Encoded frame " << frameNumber << " missing!");
          return IGSIO_FAIL;
        }

        vtkUnsignedCharArray* frameData = encodedFrame->GetFrameData();
        unsigned long size = frameData->GetSize() * frameData->GetElementComponentSize();
        if (!this->Internal->WriteFrame(frameData->GetPointer(0), size, encodedFrame->IsKeyFrame(), this->Internal->VideoTrackNumber, timestamp))
        {
          LOG_ERROR("Could not write frame to file: " << this->FileName);
          return IGSIO_FAIL;
        }
      }

      igsioFieldMapType customFields = trackedFrame->GetCustomFields();
      for (igsioFieldMapType::const_iterator customFieldIt = customFields.begin(); customFieldIt != customFields.end(); ++customFieldIt)
      {
        uint64_t trackID = this->Internal->FrameFieldTracks[customFieldIt->first];
        if (trackID == 0)
        {
          LOG_ERROR("Could not find metadata track for: " << customFieldIt->first);
          continue;
        }

        this->Internal->WriteMetadata(customFieldIt->second.second, trackID, trackedFrame->GetTimestamp() - this->Internal->InitialTimestamp);
      }
    }
  }
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMkvSequenceIO::Close()
{
  this->Internal->Close();
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMkvSequenceIO::WriteCompressedImagePixelsToFile(int& compressedDataSize)
{
  return this->WriteImages();
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMkvSequenceIO::SetFileName(const std::string& aFilename)
{
  this->FileName = aFilename;
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOMkvSequenceIO::UpdateDimensionsCustomStrings(int numberOfFrames, bool isData3D)
{
  return IGSIO_SUCCESS;
}

//---------------------------------------------------------------------------
bool vtkIGSIOMkvSequenceIO::vtkInternal::WriteHeader()
{
  this->Close();

  this->MKVWriter = new mkvmuxer::MkvWriter();

  std::string fileFullPath = this->External->FileName;
  if (!this->External->OutputFilePath.empty() && !vtksys::SystemTools::FileIsFullPath(fileFullPath))
  {
    fileFullPath = this->External->OutputFilePath + "/" + fileFullPath;
  }

  if (!this->MKVWriter->Open(fileFullPath.c_str()))
  {
    LOG_ERROR("Could not open file " << this->External->FileName << " for writing: " << strerror(errno));
    return false;
  }

  if (!this->EBMLHeader)
  {
    this->EBMLHeader = new mkvparser::EBMLHeader();
  }

  if (!this->MKVWriteSegment)
  {
    this->MKVWriteSegment = new mkvmuxer::Segment();
  }

  if (!this->MKVWriteSegment->Init(this->MKVWriter))
  {
    LOG_ERROR("Could not initialize MKV file segment!");
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------
int vtkIGSIOMkvSequenceIO::vtkInternal::AddVideoTrack(std::string name, std::string encodingFourCC, int width, int height, std::string language/*="und"*/)
{
  if (!this->MKVWriteSegment)
  {
    LOG_ERROR("Could not create metadata track! MKVWriteSegment does not exist!");
    return -1;
  }

  int trackNumber = this->MKVWriteSegment->AddVideoTrack(width, height, 0);
  if (trackNumber <= 0)
  {
    LOG_ERROR("Could not create video track!");
    return -1;
  }

  mkvmuxer::VideoTrack* videoTrack = (mkvmuxer::VideoTrack*)this->MKVWriteSegment->GetTrackByNumber(trackNumber);
  if (!videoTrack)
  {
    LOG_ERROR("Could not find video track: " << trackNumber << "!");
    return -1;
  }

  std::string codecId = FourCCToCodecId(encodingFourCC);
  videoTrack->set_codec_id(codecId.c_str());
  videoTrack->set_name(name.c_str());
  videoTrack->set_language(language.c_str());

  if (codecId == VTKSEQUENCEIO_MKV_UNCOMPRESSED_CODECID)
  {
    videoTrack->set_colour_space(encodingFourCC.c_str());
  }

  this->MKVWriteSegment->CuesTrack(trackNumber);

  return trackNumber;
}

//----------------------------------------------------------------------------
int vtkIGSIOMkvSequenceIO::vtkInternal::AddMetadataTrack(std::string name, std::string language/*="und"*/)
{
  if (!this->MKVWriteSegment)
  {
    LOG_ERROR("Could not create metadata track! MKVWriteSegment does not exist!");
  }

  mkvmuxer::Track* const track = this->MKVWriteSegment->AddTrack(0);
  if (!track)
  {
    LOG_ERROR("Could not create metadata track!");
    return false;
  }

  track->set_name(name.c_str());
  track->set_type(mkvparser::Track::kSubtitle);
  track->set_codec_id("S_TEXT/UTF8");
  track->set_language(language.c_str());
  return track->number();
}

//-----------------------------------------------------------------------------
bool vtkIGSIOMkvSequenceIO::vtkInternal::WriteFrame(unsigned char* frameData, uint64_t size, bool isKeyFrame, int trackNumber, double timestampSeconds)
{
  if (!this->MKVWriter)
  {
    LOG_ERROR("MKV writer not initialized.");
    return false;
  }

  uint64_t timestampNanoSeconds = std::floor(NANOSECONDS_IN_SECOND * timestampSeconds);
  if (!this->MKVWriteSegment->AddFrame(frameData, size, trackNumber, timestampNanoSeconds, isKeyFrame))
  {
    LOG_ERROR("Error writing frame to file!");
    return false;
  }
  this->MKVWriteSegment->AddCuePoint(timestampNanoSeconds, trackNumber);
  return true;
}

//---------------------------------------------------------------------------
bool vtkIGSIOMkvSequenceIO::vtkInternal::WriteMetadata(std::string metadata, int trackNumber, double timestampSeconds, double durationSeconds/*=1.0/NANOSECONDS_IN_SECOND*/)
{
  uint64_t timestampNanoSeconds = std::floor(NANOSECONDS_IN_SECOND * timestampSeconds);
  uint64_t durationNanoSeconds = std::floor(NANOSECONDS_IN_SECOND * durationSeconds);
  if (!this->MKVWriteSegment->AddMetadata((uint8_t*)metadata.c_str(), sizeof(char) * (metadata.length() + 1), trackNumber, timestampNanoSeconds, durationNanoSeconds))
  {
    LOG_ERROR("Error writing metadata to file!");
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
bool vtkIGSIOMkvSequenceIO::vtkInternal::ReadHeader()
{
  this->Close();

  this->MKVReader = new mkvparser::MkvReader();
  this->MKVReader->Open(this->External->FileName.c_str());

  if (!this->EBMLHeader)
  {
    this->EBMLHeader = new mkvparser::EBMLHeader();
  }
  long long readerPosition;
  this->EBMLHeader->Parse(this->MKVReader, readerPosition);

  mkvparser::Segment::CreateInstance(this->MKVReader, readerPosition, this->MKVReadSegment);
  if (!this->MKVReadSegment)
  {
    LOG_ERROR("Could not read mkv segment!");
    this->Close();
    return false;
  }

  this->MKVReadSegment->Load();
  this->MKVReadSegment->ParseHeaders();

  const mkvparser::Tracks* tracks = this->MKVReadSegment->GetTracks();
  if (!tracks)
  {
    LOG_ERROR("Could not retrieve tracks!");
    this->Close();
    return false;
  }

  unsigned long numberOfTracks = tracks->GetTracksCount();
  for (unsigned long trackIndex = 0; trackIndex < numberOfTracks; ++trackIndex)
  {
    const mkvparser::Track* track = tracks->GetTrackByIndex(trackIndex);
    if (!track)
    {
      continue;
    }

    const long trackType = track->GetType();
    const long trackNumber = track->GetNumber();
    if (trackType == mkvparser::Track::kVideo)
    {
      const mkvparser::VideoTrack* const videoTrack = static_cast<const mkvparser::VideoTrack*>(track);

      std::string trackName;
      if (videoTrack->GetNameAsUTF8())
      {
        trackName = videoTrack->GetNameAsUTF8();
      }
      else
      {
        std::stringstream ss;
        ss << "Video_" << trackNumber;
        trackName = ss.str();
      }

      std::string encoding;
      if (videoTrack->GetCodecId())
      {
        encoding = videoTrack->GetCodecId();
      }
      else
      {
        encoding = "Unknown";
      }

      std::string encodingFourCC = this->CodecIdToFourCC(encoding);
      std::string colourSpace;
      if (videoTrack->GetColourSpace())
      {
        colourSpace = videoTrack->GetColourSpace();
      }

      unsigned long width = videoTrack->GetWidth();
      unsigned long height = videoTrack->GetHeight();
      double frameRate = videoTrack->GetFrameRate();
      bool isGreyScale = false;
      VideoTrackInfo videoTrackInfo(trackName, encodingFourCC, colourSpace, width, height, frameRate, isGreyScale);
      this->VideoTracks[trackNumber] = videoTrackInfo;
      this->External->TrackedFrameList->SetFrameField("TrackName", trackName);
    }
    else if (trackType == mkvparser::Track::kMetadata || trackType == mkvparser::Track::kSubtitle)
    {
      std::string trackName = track->GetNameAsUTF8();
      std::string encoding;
      if (track->GetCodecId())
      {
        encoding = track->GetCodecId();
      }
      else
      {
        encoding = "Unknown";
      }
      MetadataTrackInfo metadataTrackInfo(trackName, encoding);
      this->MetadataTracks[trackNumber] = metadataTrackInfo;
    }
  }

  return true;
}

//----------------------------------------------------------------------------
bool vtkIGSIOMkvSequenceIO::vtkInternal::ReadVideoData()
{
  const mkvparser::Tracks* tracks = this->MKVReadSegment->GetTracks();

  double lastTimestamp = -1;

  vtkSmartPointer<vtkStreamingVolumeFrame> previousFrame = NULL;

  unsigned int frameNumber = 0;
  const mkvparser::Cluster* cluster = this->MKVReadSegment->GetFirst();
  while (cluster != NULL && !cluster->EOS())
  {
    const mkvparser::BlockEntry* blockEntry;
    long status = cluster->GetFirst(blockEntry);
    if (status < 0)
    {
      LOG_ERROR("Could not find block!");
      return false;
    }

    while (blockEntry != NULL && !blockEntry->EOS())
    {
      const mkvparser::Block* const block = blockEntry->GetBlock();

      const long long timestampNanoSeconds = block->GetTime(cluster);
      // Convert nanoseconds to seconds
      double timestampSeconds = timestampNanoSeconds / NANOSECONDS_IN_SECOND; // Timestamp of the current cluster in seconds

      unsigned long trackNumber = static_cast<unsigned long>(block->GetTrackNumber());
      const mkvparser::Track* const track = tracks->GetTrackByNumber(trackNumber);

      if (!track)
      {
        LOG_ERROR("Could not find track!");
        return false;
      }

      const int trackType = track->GetType();
      if (trackType == mkvparser::Track::kVideo)
      {
        const int frameCount = block->GetFrameCount();
        for (int i = 0; i < frameCount; ++i)
        {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);
          const long size = frame.len;
          const long long offset = frame.pos;

          // Handle video frame
          VideoTrackInfo* videoTrack = &this->VideoTracks[trackNumber];
          if (lastTimestamp != -1 && lastTimestamp == timestampSeconds)
          {
            if (videoTrack->FrameRate > 0.0)
            {
              timestampSeconds = lastTimestamp + (1. / videoTrack->FrameRate);
            }
            else
            {
              // TODO: Not all files seem to be encoded with either timestamp or framerate
              // Timestamp hasn't changed and there is no framerate tag, assuming 25 fps
              timestampSeconds = lastTimestamp + (1. / 25.);
            }
          }

          this->External->CreateTrackedFrameIfNonExisting(frameNumber);
          igsioTrackedFrame* trackedFrame = this->External->TrackedFrameList->GetTrackedFrame(frameNumber);
          trackedFrame->GetImageData()->SetImageOrientation(US_IMG_ORIENT_MF); // TODO: save orientation and type
          trackedFrame->GetImageData()->SetImageType(US_IMG_RGB_COLOR);
          trackedFrame->SetTimestamp(timestampSeconds);
          FrameSizeType frameSize = { static_cast<unsigned int>(videoTrack->Width), static_cast<unsigned int>(videoTrack->Height), 1 };
          if (videoTrack->Encoding.empty())
          {
            trackedFrame->GetImageData()->AllocateFrame(frameSize, VTK_UNSIGNED_CHAR, 3);
            frame.Read(this->MKVReader, (unsigned char*)trackedFrame->GetImageData()->GetImage()->GetScalarPointer());
          }
          else
          {
            vtkSmartPointer<vtkStreamingVolumeFrame> encodedFrame = vtkSmartPointer<vtkStreamingVolumeFrame>::New();
            encodedFrame->SetFrameType(block->IsKey() ? vtkStreamingVolumeFrame::IFrame : vtkStreamingVolumeFrame::PFrame);
            encodedFrame->SetCodecFourCC(videoTrack->Encoding);
            encodedFrame->SetDimensions(videoTrack->Width, videoTrack->Height, 1);

            if (!encodedFrame->IsKeyFrame())
            {
              encodedFrame->SetPreviousFrame(previousFrame);
            }
            previousFrame = encodedFrame;

            vtkSmartPointer<vtkUnsignedCharArray> encodedFrameData = vtkSmartPointer<vtkUnsignedCharArray>::New();
            encodedFrameData->Allocate(size);
            encodedFrame->SetFrameData(encodedFrameData);

            frame.Read(this->MKVReader, encodedFrameData->GetPointer(0));
            trackedFrame->GetImageData()->SetEncodedFrame(encodedFrame);
          }

          ++frameNumber;
          lastTimestamp = timestampSeconds;
        }
      }
      status = cluster->GetNext(blockEntry, blockEntry);
      if (status < 0)  // error
      {
        this->Close();
        LOG_ERROR("Error reading MKV: " << status << "!");
        return false;
      }
    }
    cluster = this->MKVReadSegment->GetNext(cluster);
  }

  return true;
}

//----------------------------------------------------------------------------
bool vtkIGSIOMkvSequenceIO::vtkInternal::ReadMetadata()
{
  const mkvparser::Tracks* tracks = this->MKVReadSegment->GetTracks();
  MetaDataInfo metaDataInfo;

  double lastTimestamp = -1;
  const mkvparser::Cluster* cluster = this->MKVReadSegment->GetFirst();
  while (cluster != NULL && !cluster->EOS())
  {
    const mkvparser::BlockEntry* blockEntry;
    long status = cluster->GetFirst(blockEntry);
    if (status < 0)
    {
      LOG_ERROR("Could not find block!");
      return false;
    }

    while (blockEntry != NULL && !blockEntry->EOS())
    {
      const mkvparser::Block* block = blockEntry->GetBlock();
      const long long timestampNanoSeconds = block->GetTime(cluster);
      // Convert nanoseconds to seconds
      double timestampSeconds = timestampNanoSeconds / NANOSECONDS_IN_SECOND; // Timestamp of the current cluster in seconds

      unsigned long trackNumber = static_cast<unsigned long>(block->GetTrackNumber());
      const mkvparser::Track* track = tracks->GetTrackByNumber(trackNumber);
      if (!track)
      {
        LOG_ERROR("Could not find track!");
        return false;
      }

      const int trackType = track->GetType();
      if (trackType == mkvparser::Track::kMetadata || trackType == mkvparser::Track::kSubtitle)
      {


        const int frameCount = block->GetFrameCount();
        for (int i = 0; i < frameCount; ++i)
        {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);
          const long size = frame.len;
          const long long offset = frame.pos;

          MetadataTrackInfo* metaDataTrack = &this->MetadataTracks[trackNumber];

          std::string frameField = std::string(frame.len, ' ');
          frame.Read(this->MKVReader, (unsigned char*)frameField.c_str());
          metaDataInfo[timestampSeconds].push_back(MetaData(metaDataTrack->Name, frameField));
        }
      }
      status = cluster->GetNext(blockEntry, blockEntry);
      if (status < 0)  // error
      {
        this->Close();
        LOG_ERROR("Error reading MKV: " << status << "!");
        return false;
      }
    }
    cluster = this->MKVReadSegment->GetNext(cluster);
  }

  for (unsigned int i = 0; i < this->External->TrackedFrameList->GetNumberOfTrackedFrames(); ++i)
  {
    igsioTrackedFrame* trackedFrame = this->External->TrackedFrameList->GetTrackedFrame(i);
    for (MetaDataInfo::iterator metaDataInfoIt = metaDataInfo.begin(); metaDataInfoIt != metaDataInfo.end(); ++metaDataInfoIt)
    {
      double timestampSeconds = metaDataInfoIt->first;
      if (trackedFrame->GetTimestamp() != timestampSeconds)
      {
        continue;
      }

      std::vector<MetaData> metaDataList = metaDataInfoIt->second;
      for (std::vector<MetaData>::iterator metaDataIt = metaDataList.begin(); metaDataIt != metaDataList.end(); ++metaDataIt)
      {
        trackedFrame->SetFrameField(metaDataIt->Name, metaDataIt->Value);
      }
    }
  }

  return true;
}

//---------------------------------------------------------------------------
void vtkIGSIOMkvSequenceIO::vtkInternal::Close()
{
  if (this->MKVWriteSegment)
  {
    this->MKVWriteSegment->Finalize();
  }

  if (this->MKVWriter)
  {
    this->MKVWriter->Close();
    delete this->MKVWriter;
    this->MKVWriter = NULL;
  }

  if (this->MKVWriteSegment)
  {
    delete this->MKVWriteSegment;
    this->MKVWriteSegment = NULL;
  }
  this->WriterInitialized = false;

  if (this->MKVReadSegment)
  {
    this->MKVReader->Close();
    delete this->MKVReadSegment;
    this->MKVReadSegment = NULL;
  }

  if (this->EBMLHeader)
  {
    delete this->EBMLHeader;
    this->EBMLHeader = NULL;
  }
  this->ReaderInitialized = false;
}

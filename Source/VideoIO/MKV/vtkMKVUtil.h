/*==============================================================================

Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
Queen's University, Kingston, ON, Canada. All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

This file was originally developed by Kyle Sunderland, PerkLab, Queen's University

==============================================================================*/

#ifndef __vtkMKVUtil_h
#define __vtkMKVUtil_h

#define NANOSECONDS_IN_SECOND 1000000000.0
#define SECONDS_IN_NANOSECOND 1.0/NANOSECONDS_IN_SECOND

#define VTKVIDEOIO_MKV_UNCOMPRESSED_CODECID "V_UNCOMPRESSED"
#define VTKVIDEOIO_MKV_VP9_CODECID "V_VP9"
#define VTKVIDEOIO_MKV_H264_CODECID "V_H264"

#define VTKVIDEOIO_RGB2_FOURCC "RGB2"
#define VTKVIDEOIO_RV24_FOURCC "RV24"
#define VTKVIDEOIO_RGB24_FOURCC "RGB"
#define VTKVIDEOIO_VP9_FOURCC "VP90"
#define VTKVIDEOIO_H264_FOURCC "H264"

class VTKVIDEOIO_EXPORT vtkMKVUtil
{
public:

  static std::string CodecIdToFourCC(std::string codecId)
  {
    if (codecId == VTKVIDEOIO_MKV_VP9_CODECID)
    {
      return VTKVIDEOIO_VP9_FOURCC;
    }
    else if (codecId == VTKVIDEOIO_MKV_H264_CODECID)
    {
      return VTKVIDEOIO_H264_FOURCC;
    }
  
    return "";
  };

  static std::string FourCCToCodecId(std::string fourCC)
  {
    if (fourCC == VTKVIDEOIO_RGB2_FOURCC)
    {
      return VTKVIDEOIO_MKV_UNCOMPRESSED_CODECID;
    }
    else if (fourCC == VTKVIDEOIO_RV24_FOURCC)
    {
      return VTKVIDEOIO_MKV_UNCOMPRESSED_CODECID;
    }
    else if (fourCC == VTKVIDEOIO_RGB24_FOURCC)
    {
      return VTKVIDEOIO_MKV_UNCOMPRESSED_CODECID;
    }
    else if (fourCC == VTKVIDEOIO_VP9_FOURCC)
    {
      return VTKVIDEOIO_MKV_VP9_CODECID;
    }
    else if (fourCC == VTKVIDEOIO_H264_FOURCC)
    {
      return VTKVIDEOIO_MKV_H264_CODECID;
    }

    return "";
  };
  
  struct FrameInfo
  {
    vtkSmartPointer<vtkUnsignedCharArray> Data;
    double         TimestampSeconds;
    bool           IsKey;
  };
  typedef std::vector<FrameInfo> FrameInfoList;
  
  struct VideoTrackInfo
  {
    VideoTrackInfo() {}
    VideoTrackInfo(std::string name, std::string encoding, unsigned long width, unsigned long height, double frameRate, bool isGreyScale)
      : Name(name)
      , Encoding(encoding)
      , Width(width)
      , Height(height)
      , FrameRate(frameRate)
      , IsGreyScale(isGreyScale)
    {}
    ~VideoTrackInfo() {}
    std::string Name;
    std::string Encoding;
    unsigned long Width;
    unsigned long Height;
    double FrameRate;
    bool IsGreyScale;
    FrameInfoList Frames;
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
    FrameInfoList Frames;
  };
  typedef std::map <int, MetadataTrackInfo> MetadataTrackMap;
};

#endif

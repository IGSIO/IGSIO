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

// libwebm includes
#include "mkvwriter.h"
#include "mkvparser.h"

// VTK includes
#include <vtkObjectFactory.h>

// vtkVideoIO includes
#include "vtkMKVWriter.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMKVWriter);

class vtkMKVWriter::vtkInternal
{
public:
  vtkMKVWriter* External;

  double FrameRate;
  mkvmuxer::MkvWriter* MKVWriter;
  mkvparser::EBMLHeader* EBMLHeader;
  mkvmuxer::Segment* MKVWriteSegment;

  //----------------------------------------------------------------------------
  vtkInternal(vtkMKVWriter* external)
    : External(external)
    , MKVWriter(new mkvmuxer::MkvWriter())
    , EBMLHeader(NULL)
    , MKVWriteSegment(NULL)
  {
  }

  //----------------------------------------------------------------------------
  virtual ~vtkInternal()
  {
    delete this->MKVWriter;
    this->MKVWriter = NULL;
  }

};

//---------------------------------------------------------------------------
vtkMKVWriter::vtkMKVWriter()
  : Internal(new vtkInternal(this))
{
}

//---------------------------------------------------------------------------
vtkMKVWriter::~vtkMKVWriter()
{
  this->Close();
  delete this->Internal;
  this->Internal = NULL;
}

//---------------------------------------------------------------------------
bool vtkMKVWriter::WriteHeader()
{
  this->Close();
  
  this->Internal->MKVWriter = new mkvmuxer::MkvWriter();
  if (!this->Internal->MKVWriter->Open(this->Filename.c_str()))
  {
    vtkErrorMacro("Could not open file " << this->Filename << "!");
    return false;
  }

  if (!this->Internal->EBMLHeader)
  {
    this->Internal->EBMLHeader = new mkvparser::EBMLHeader();
  }

  if (!this->Internal->MKVWriteSegment)
  {
    this->Internal->MKVWriteSegment = new mkvmuxer::Segment();
  }

  if (!this->Internal->MKVWriteSegment->Init(this->Internal->MKVWriter))
  {
    vtkErrorMacro("Could not initialize MKV file segment!");
    return false;
  }
  
  return true;
}

//----------------------------------------------------------------------------
int vtkMKVWriter::AddVideoTrack(std::string name, std::string encodingFourCC, int width, int height, std::string language/*="und"*/)
{
  int trackNumber = this->Internal->MKVWriteSegment->AddVideoTrack(width, height, 0);
  if (trackNumber <= 0)
  {
    vtkErrorMacro("Could not create video track!");
    return false;
  }

  mkvmuxer::VideoTrack* videoTrack = (mkvmuxer::VideoTrack*)this->Internal->MKVWriteSegment->GetTrackByNumber(trackNumber);
  if (!videoTrack)
  {
    vtkErrorMacro("Could not find video track: " << trackNumber << "!");
    return false;
  }

  std::string codecId = vtkMKVUtil::FourCCToCodecId(encodingFourCC);
  videoTrack->set_codec_id(codecId.c_str());
  videoTrack->set_name(name.c_str());
  videoTrack->set_language(language.c_str());

  if (codecId == VTKVIDEOIO_MKV_UNCOMPRESSED_CODECID)
  {
    videoTrack->set_colour_space(encodingFourCC.c_str());
  }

  this->Internal->MKVWriteSegment->CuesTrack(trackNumber);

  vtkMKVUtil::VideoTrackInfo videoInfo(name, encodingFourCC, width, height, 0.0, false);
  return trackNumber;
}

//----------------------------------------------------------------------------
int vtkMKVWriter::AddMetadataTrack(std::string name, std::string language/*="und"*/)
{
  mkvmuxer::Track* const track = this->Internal->MKVWriteSegment->AddTrack(0);
  if (!track)
  {
    vtkErrorMacro("Could not create metadata track!");
    return false;
  }

  track->set_name(name.c_str());
  track->set_type(mkvparser::Track::kSubtitle);
  track->set_codec_id("S_TEXT/UTF8");
  track->set_language(language.c_str());
  return track->number();
}

//-----------------------------------------------------------------------------
bool vtkMKVWriter::WriteEncodedVideoFrame(unsigned char* encodedFrame, uint64_t size, bool isKeyFrame, int trackNumber, double timestampSeconds)
{
  if (!this->Internal->MKVWriter)
  {
    vtkErrorMacro("Header not initialized.")
    return false;
  }

  uint64_t timestampNanoSeconds = std::floor(NANOSECONDS_IN_SECOND * timestampSeconds);
  if (!this->Internal->MKVWriteSegment->AddFrame(encodedFrame, size, trackNumber, timestampNanoSeconds, isKeyFrame))
  {
    vtkErrorMacro("Error writing frame to file!");
    return false;
  }
  this->Internal->MKVWriteSegment->AddCuePoint(timestampNanoSeconds, trackNumber);
  return true;
}

//---------------------------------------------------------------------------
bool vtkMKVWriter::WriteMetadata(std::string metadata, int trackNumber, double timestampSeconds, double durationSeconds/*=1.0/NANOSECONDS_IN_SECOND*/)
{
  uint64_t timestampNanoSeconds = std::floor(NANOSECONDS_IN_SECOND * timestampSeconds);
  uint64_t durationNanoSeconds = std::floor(NANOSECONDS_IN_SECOND * durationSeconds);
  if (!this->Internal->MKVWriteSegment->AddMetadata((uint8_t*)metadata.c_str(), sizeof(char) * (metadata.length() + 1), trackNumber, timestampNanoSeconds, durationNanoSeconds))
  {
    vtkErrorMacro("Error writing metadata to file!");
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------
void vtkMKVWriter::Close()
{
  if (this->Internal->MKVWriteSegment)
  {
    this->Internal->MKVWriteSegment->Finalize();
  }

  if (this->Internal->MKVWriter)
  {
    this->Internal->MKVWriter->Close();
    delete this->Internal->MKVWriter;
    this->Internal->MKVWriter = NULL;
  }

  if (this->Internal->MKVWriteSegment)
  {
    delete this->Internal->MKVWriteSegment;
    this->Internal->MKVWriteSegment = NULL;
  }
}

//---------------------------------------------------------------------------
void vtkMKVWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}

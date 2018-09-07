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

#ifndef __vtkMKVWriter_h
#define __vtkMKVWriter_h

// std includes
#include <map>

// VTK includes
#include <vtkImageData.h>

// vtkVideoIO includes
#include "vtkGenericVideoWriter.h"
#include "vtkMKVUtil.h"

class VTKVIDEOIO_EXPORT vtkMKVWriter : public vtkGenericVideoWriter
{
  vtkTypeMacro(vtkMKVWriter, vtkGenericVideoWriter);

public:
  static vtkMKVWriter *New();
  void PrintSelf(ostream& os, vtkIndent indent);

protected:
  vtkMKVWriter();
  virtual ~vtkMKVWriter();

public:
  virtual bool WriteHeader();

  virtual int AddVideoTrack(std::string name, std::string encodingFourCC, int width, int height, std::string language="und");
  virtual int AddMetadataTrack(std::string name, std::string language="und");

  virtual bool WriteEncodedVideoFrame(unsigned char* encodedFrame, uint64_t size, bool frameType, int trackNumber, double timestampSeconds);
  virtual bool WriteMetadata(std::string metadata, int trackNumber, double timestampSeconds = 0.0, double durationSeconds = SECONDS_IN_NANOSECOND);

  virtual void Close();

  static std::string FourCCToCodecId(std::string);

protected:
  class vtkInternal;
  vtkInternal* Internal;
};

#endif

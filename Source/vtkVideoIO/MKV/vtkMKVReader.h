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

#ifndef __vtkMKVReader_h
#define __vtkMKVReader_h

// VTK includes
#include <vtkImageData.h>

// vtkVideoIO includes
#include "vtkGenericVideoReader.h"
#include "vtkMKVUtil.h"

// std includes
#include <map>
#include <vector>

class VTKVIDEOIO_EXPORT vtkMKVReader : public vtkGenericVideoReader
{
  vtkTypeMacro(vtkMKVReader, vtkGenericVideoReader);

public:
  static vtkMKVReader *New();
  void PrintSelf(ostream& os, vtkIndent indent);

protected:
  vtkMKVReader();
  virtual ~vtkMKVReader();

public:

  virtual bool ReadHeader();
  virtual bool ReadContents();

  virtual vtkSmartPointer<vtkUnsignedCharArray> GetRawVideoFrame(int videoTrackNumber, unsigned int frameNumber, double &timestampSec, bool &isKeyFrame);

  virtual void Close();

  ///
  static std::string CodecIdToFourCC(std::string);

  ///
  vtkMKVUtil::VideoTrackMap    GetVideoTracks();

  ///
  vtkMKVUtil::MetadataTrackMap GetMetadataTracks();

protected:
  class vtkInternal;
  vtkInternal* Internal;
};

#endif

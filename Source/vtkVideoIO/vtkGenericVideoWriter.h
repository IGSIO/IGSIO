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

#ifndef __vtkGenericVideoWriter_h
#define __vtkGenericVideoWriter_h

// std  includes
#include <map>

// VTK includes
#include <vtkImageData.h>
#include <vtkObject.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>

#include "vtkvideoio_export.h"
class VTKVIDEOIO_EXPORT vtkGenericVideoWriter : public vtkObject
{
  vtkTypeMacro(vtkGenericVideoWriter, vtkObject);

public:
  void PrintSelf(ostream& os, vtkIndent indent);

protected:
  vtkGenericVideoWriter();
  virtual ~vtkGenericVideoWriter();

private:
  vtkGenericVideoWriter(const vtkGenericVideoWriter&); // Not implemented
  void operator=(const vtkGenericVideoWriter&);        // Not implemented

public:
  vtkSetMacro(Filename, std::string);
  vtkGetMacro(Filename, std::string);

protected:
  std::string Filename;
};

#endif

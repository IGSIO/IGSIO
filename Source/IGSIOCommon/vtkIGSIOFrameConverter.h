/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/


#ifndef __vtkIGSIOFrameConverter_h
#define __vtkIGSIOFrameConverter_h

#include "vtkigsiocommon_export.h"
#include "igsioCommon.h"
#include "igsioVideoFrame.h"

#include "vtkImageData.h"
#include <vtkSmartPointer.h>

#include <vtkStreamingVolumeFrame.h>
class vtkStreamingVolumeCodec;

/*!
\class vtkIGSIOFrameConverter
*/
class VTKIGSIOCOMMON_EXPORT vtkIGSIOFrameConverter : public vtkObject
{
public:

  static vtkIGSIOFrameConverter* New();
  vtkTypeMacro(vtkIGSIOFrameConverter, vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  vtkSmartPointer<vtkImageData> GetUncompressedImage(igsioVideoFrame* frame);
  vtkSmartPointer<vtkStreamingVolumeFrame> GetCompressedFrame(igsioVideoFrame* frame, std::string codecFourCC, std::map<std::string, std::string> parameters);

protected:
  typedef std::map<std::string, vtkSmartPointer<vtkStreamingVolumeCodec> > CodecList;
  CodecList Codecs;

protected:
  vtkIGSIOFrameConverter();
  virtual ~vtkIGSIOFrameConverter();
private:
  vtkIGSIOFrameConverter(const vtkIGSIOFrameConverter&);
  void operator=(const vtkIGSIOFrameConverter&);
};

#endif

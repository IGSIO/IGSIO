/*=IGSIO=header=begin======================================================
Program: IGSIO
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================IGSIO=header=end*/

#ifndef __vtkIGSIOFrameConverter_h
#define __vtkIGSIOFrameConverter_h

// IGSIO includes
#include "vtkigsiocommon_export.h"
#include "igsioCommon.h"
#include "igsioVideoFrame.h"

// VTK includes
#include "vtkImageData.h"
#include <vtkSmartPointer.h>

// vtkAddon includes
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

  vtkSmartPointer<vtkImageData> GetImageData(igsioVideoFrame* frame);
  vtkSmartPointer<vtkStreamingVolumeFrame> GetEncodedFrame(igsioVideoFrame* frame, std::string codecFourCC, std::map<std::string, std::string> parameters);

  /*!
  If EnableCache is set to true, then a pointer to the previously encoded/decoded frame/image will be stored.
  Repeated GetEncodedFrame/GetImageData calls that are made with the same input will be return the cached value rather than encoding/decoding the input again
  */
  vtkSetMacro(EnableCache, bool);
  vtkGetMacro(EnableCache, bool);
  vtkBooleanMacro(EnableCache, bool);

  /*!
  If RequestKeyFrame is set to true, the codec will request a keyframe at every GetEncodedFrame call until a keyframe is returned.
  Once a keyframe has been encoded, RequestKeyFrame will be set to false.
  */
  vtkSetMacro(RequestKeyFrame, bool);
  vtkGetMacro(RequestKeyFrame, bool);
  vtkBooleanMacro(RequestKeyFrame, bool);

protected:
  typedef std::map<std::string, vtkSmartPointer<vtkStreamingVolumeCodec> > CodecList;
  CodecList Codecs;
  bool EnableCache;
  bool RequestKeyFrame;

  vtkSmartPointer<vtkStreamingVolumeFrame> CacheLastInputFrame;
  vtkSmartPointer<vtkStreamingVolumeFrame> CacheLastOutputFrame;
  vtkSmartPointer<vtkImageData> CacheLastInputImage;
  vtkSmartPointer<vtkImageData> CacheLastOutputImage;

protected:
  vtkIGSIOFrameConverter();
  virtual ~vtkIGSIOFrameConverter();
private:
  vtkIGSIOFrameConverter(const vtkIGSIOFrameConverter&);
  void operator=(const vtkIGSIOFrameConverter&);
};

#endif

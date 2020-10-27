/*=auto=========================================================================

Portions (c) Copyright 2009 Brigham and Women's Hospital (BWH) All Rights Reserved.

See Doc/copyright/copyright.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Program:   3D Slicer
Module:    $RCSfile: vtkVP9VolumeCodec.h,v $
Date:      $Date: 2006/03/19 17:12:29 $
Version:   $Revision: 1.3 $

=========================================================================auto=*/

#ifndef __vtkVP9VolumeCodec_h
#define __vtkVP9VolumeCodec_h

// vtkAddon includes
#include "vtkStreamingVolumeCodec.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkObject.h>
#include <vtkUnsignedCharArray.h>

// Codec includes
#include "vtkigsiocodecs_export.h"

#ifndef VTK_OVERRIDE
#define VTK_OVERRIDE override
#endif

class VTKIGSIOCODECS_EXPORT vtkVP9VolumeCodec : public vtkStreamingVolumeCodec
{
public:
  static vtkVP9VolumeCodec* New();
  virtual vtkStreamingVolumeCodec* CreateCodecInstance() VTK_OVERRIDE;
  vtkTypeMacro(vtkVP9VolumeCodec, vtkStreamingVolumeCodec);

  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  std::string GetLosslessEncodingParameter() { return "losslessEncoding"; };
  std::string GetMinimumKeyFrameDistanceParameter() { return "minimumKeyFrameDistance"; };
  std::string GetMaximumKeyFrameDistanceParameter() { return "maximumKeyFrameDistance"; };
  std::string GetSpeedParameter() { return "encodingSpeed"; };
  std::string GetBitRateParameter() { return "bitRate"; };
  std::string GetRateControlParameter() { return "rateControl"; };
  std::string GetDeadlineModeParameter() { return "deadlineMode"; };

  virtual std::string GetFourCC() VTK_OVERRIDE;

  virtual std::string GetParameterDescription(std::string parameterName) VTK_OVERRIDE;

  virtual bool SetParametersFromPresetValue(const std::string& presetValue) VTK_OVERRIDE;

protected:
  vtkVP9VolumeCodec();
  ~vtkVP9VolumeCodec();

  virtual bool DecodeFrameInternal(vtkStreamingVolumeFrame* inputFrame, vtkImageData* outputImageData, bool saveDecodedImage = true) VTK_OVERRIDE;
  virtual bool EncodeImageDataInternal(vtkImageData* outputImageData, vtkStreamingVolumeFrame* inputFrame, bool forceKeyFrame) VTK_OVERRIDE;
  virtual bool UpdateParameterInternal(std::string parameterValue, std::string parameterName) VTK_OVERRIDE;

private:
  vtkVP9VolumeCodec(const vtkVP9VolumeCodec&);
  void operator=(const vtkVP9VolumeCodec&);

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

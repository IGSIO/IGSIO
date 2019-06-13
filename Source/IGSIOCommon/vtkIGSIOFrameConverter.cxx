/*=IGSIO=header=begin======================================================
Program: IGSIO
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================IGSIO=header=end*/

// IGSIO includes
#include "vtkIGSIOFrameConverter.h"

// vtkAddon includes
#include <vtkStreamingVolumeCodec.h>
#include <vtkStreamingVolumeCodecFactory.h>

vtkStandardNewMacro(vtkIGSIOFrameConverter);

//----------------------------------------------------------------------------
vtkIGSIOFrameConverter::vtkIGSIOFrameConverter()
  : EnableCache(false)
  , RequestKeyFrame(false)
  , CacheLastInputFrame(nullptr)
  , CacheLastOutputFrame(nullptr)
  , CacheLastInputImage(nullptr)
  , CacheLastOutputImage(nullptr)
{
}

//----------------------------------------------------------------------------
vtkIGSIOFrameConverter::~vtkIGSIOFrameConverter()
{
}
//----------------------------------------------------------------------------
void vtkIGSIOFrameConverter::PrintSelf(std::ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkImageData> vtkIGSIOFrameConverter::GetImageData(igsioVideoFrame* frame)
{
  vtkSmartPointer<vtkImageData> uncompressedImage = NULL;
  if (!frame->IsFrameEncoded())
  {
    uncompressedImage = frame->GetImage();
  }
  else
  {
    vtkStreamingVolumeFrame* compressedFrame = frame->GetEncodedFrame();
    vtkSmartPointer<vtkStreamingVolumeCodec> codec = nullptr;
    if (compressedFrame)
    {
      std::string fourCC = compressedFrame->GetCodecFourCC();
      CodecList::iterator codecIt = this->Codecs.find(fourCC);
      if (codecIt != this->Codecs.end())
      {
        codec = codecIt->second;
      }
      else
      {
        vtkStreamingVolumeCodecFactory* factory = vtkStreamingVolumeCodecFactory::GetInstance();
        codec = factory->CreateCodecByFourCC(fourCC);
        if (codec)
        {
          this->Codecs[fourCC] = codec;
        }
      }
    }

    if (codec == nullptr)
    {
      return nullptr;
    }

    if (compressedFrame && codec)
    {
      uncompressedImage = vtkSmartPointer<vtkImageData>::New();
      uncompressedImage->SetDimensions(compressedFrame->GetDimensions());
      uncompressedImage->AllocateScalars(compressedFrame->GetVTKScalarType(), compressedFrame->GetNumberOfComponents());
      codec->DecodeFrame(compressedFrame, uncompressedImage);
    }
  }
  return uncompressedImage;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkStreamingVolumeFrame> vtkIGSIOFrameConverter::GetEncodedFrame(igsioVideoFrame* frame, std::string codecFourCC, std::map<std::string, std::string> parameters)
{
  vtkSmartPointer<vtkStreamingVolumeFrame> encodedFrame = nullptr;
  vtkSmartPointer<vtkStreamingVolumeCodec> codec = nullptr;
  if (!frame->IsFrameEncoded())
  {
    vtkSmartPointer<vtkImageData> image = frame->GetImage();
    if (!image)
    {
      return nullptr;
    }

    if (this->EnableCache)
    {
      if (this->CacheLastInputImage == image)
      {
        return this->CacheLastOutputFrame;
      }
      this->CacheLastInputImage = image;
    }

    CodecList::iterator codecIt = this->Codecs.find(codecFourCC);
    if (codecIt != this->Codecs.end())
    {
      codec = codecIt->second;
    }
    else
    {
      vtkStreamingVolumeCodecFactory* factory = vtkStreamingVolumeCodecFactory::GetInstance();
      codec = factory->CreateCodecByFourCC(codecFourCC);
      if (codec)
      {
        this->Codecs[codecFourCC] = codec;
      }
    }

    if (codec == nullptr)
    {
      return nullptr;
    }

    encodedFrame = vtkSmartPointer<vtkStreamingVolumeFrame>::New();
    codec->SetParameters(parameters);
    codec->EncodeImageData(image, encodedFrame, this->GetRequestKeyFrame());
    if (encodedFrame->IsKeyFrame())
    {
      this->SetRequestKeyFrame(false);
    }
  }
  else
  {
    vtkStreamingVolumeFrame* encodedFrame = frame->GetEncodedFrame();
    if (encodedFrame != nullptr)
    {
      if (this->GetEnableCache())
      {
        if (this->CacheLastInputFrame == encodedFrame)
        {
          return this->CacheLastOutputFrame;
        }
        this->CacheLastInputFrame = encodedFrame;
      }

      std::string encodingFourCC = encodedFrame->GetCodecFourCC();
      if (encodingFourCC == codecFourCC)
      {
        encodedFrame = encodedFrame;
      }
      else
      {
        // TODO: re-encode
      }
    }
  }

  if (this->GetEnableCache())
  {
    this->CacheLastOutputFrame = encodedFrame;
  }
  return encodedFrame;
}

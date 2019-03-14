
#include <vtkIGSIOFrameConverter.h>
#include <vtkStreamingVolumeCodec.h>
#include <vtkStreamingVolumeCodecFactory.h>

vtkStandardNewMacro(vtkIGSIOFrameConverter);

//----------------------------------------------------------------------------
vtkIGSIOFrameConverter::vtkIGSIOFrameConverter()
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

vtkSmartPointer<vtkImageData> vtkIGSIOFrameConverter::GetUncompressedImage(igsioVideoFrame* frame)
{
  vtkSmartPointer<vtkImageData> uncompressedImage = NULL;
  if (!frame->IsFrameEncoded())
  {
    uncompressedImage = frame->GetImage();
  }
  else
  {
    vtkStreamingVolumeFrame* compressedFrame = frame->GetEncodedFrame();
    vtkSmartPointer<vtkStreamingVolumeCodec> codec = NULL;
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
      return NULL;
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
vtkSmartPointer<vtkStreamingVolumeFrame> vtkIGSIOFrameConverter::GetCompressedFrame(igsioVideoFrame* frame, std::string codecFourCC, std::map<std::string, std::string> parameters)
{
  vtkSmartPointer<vtkStreamingVolumeFrame> compressedFrame = NULL;
  vtkSmartPointer<vtkStreamingVolumeCodec> codec = NULL;
  if (!frame->IsFrameEncoded())
  {
    vtkSmartPointer<vtkImageData> image = frame->GetImage();
    if (!image)
    {
      return NULL;
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
      return NULL;
    }

    compressedFrame = vtkSmartPointer<vtkStreamingVolumeFrame>::New();
    codec->SetParameters(parameters);
    codec->EncodeImageData(image, compressedFrame);
  }
  else
  {
    vtkStreamingVolumeFrame* encodedFrame = frame->GetEncodedFrame();
    if (encodedFrame != NULL)
    {
      std::string encodingFourCC = encodedFrame->GetCodecFourCC();
      if (encodingFourCC == codecFourCC)
      {
        compressedFrame = encodedFrame;
      }
      else
      {
        // TODO: re-encode
      }
    }
  }

  return compressedFrame;
}

/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

// std includes
#include <iomanip>

// VTK includes
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtksys/CommandLineArguments.hxx>

// IGSIO includes
#include "igsioTrackedFrame.h"
#include "vtkIGSIOMkvSequenceIO.h"
#include "vtkIGSIOTrackedFrameList.h"

///////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
  std::string inputImageSequenceFileName;
  std::string outputImageSequenceFileName;

  int numberOfFailures(0);

  vtksys::CommandLineArguments args;
  args.Initialize(argc, argv);

  args.AddArgument("--input-filename", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputImageSequenceFileName, "Filename of the input image sequence.");
  args.AddArgument("--output-filename", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &outputImageSequenceFileName, "Filename of the output image sequence.");

  if (!args.Parse())
  {
    std::cerr << "Problem parsing arguments" << std::endl;
    std::cout << "Help: " << args.GetHelp() << std::endl;
    exit(EXIT_FAILURE);
  }

  if (inputImageSequenceFileName.empty() && outputImageSequenceFileName.empty())
  {
    std::cerr << "--input-filename or --output-filename is required" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (!inputImageSequenceFileName.empty())
  {
    // ******************************************************************************
    // Test reading

    vtkNew<vtkIGSIOMkvSequenceIO> reader;
    reader->SetFileName(inputImageSequenceFileName.c_str());
    if (reader->Read() != IGSIO_SUCCESS)
    {
      LOG_ERROR("Couldn't read sequence MKV: " << inputImageSequenceFileName);
      return EXIT_FAILURE;
    }
    vtkIGSIOTrackedFrameList* trackedFrameList = reader->GetTrackedFrameList();

    if (trackedFrameList == NULL)
    {
      LOG_ERROR("Unable to get trackedFrameList!");
      return EXIT_FAILURE;
    }

    if (trackedFrameList->GetNumberOfTrackedFrames() < 0)
    {
      LOG_ERROR("No frames in trackedFrameList!");
      return EXIT_FAILURE;
    }
  }

  if (!outputImageSequenceFileName.empty())
  {
    // ******************************************************************************
    // Test writing

    unsigned int width = 10;
    unsigned int height = 10;
    unsigned int numFrames = 100;
    float fps = 10.0;
    double timeBetweenFrames = 1.0 / fps;
    double timestamp = 0.0;

    FrameSizeType frameSize = { width, height, 1 };

    vtkNew<vtkIGSIOTrackedFrameList> outputTrackedFrameList;

    igsioTrackedFrame emptyFrame;
    for (unsigned int i = outputTrackedFrameList->GetNumberOfTrackedFrames(); i < numFrames; i++)
    {
      outputTrackedFrameList->AddTrackedFrame(&emptyFrame, vtkIGSIOTrackedFrameList::ADD_INVALID_FRAME);
    }

    for (unsigned int i = 0; i < numFrames; ++i)
    {
      igsioTrackedFrame* trackedFrame = outputTrackedFrameList->GetTrackedFrame(i);
      igsioVideoFrame* videoFrame = trackedFrame->GetImageData();
      videoFrame->AllocateFrame(frameSize, VTK_UNSIGNED_CHAR, 3);

      vtkImageData* imageData = videoFrame->GetImage();
      unsigned char* imageDataScalars = (unsigned char*)imageData->GetScalarPointer();
      unsigned char blue = 255 * (i / (double)numFrames);
      for (unsigned int y = 0; y < height; ++y)
      {
        unsigned char green = 255 * (y / (double)height);
        for (unsigned int x = 0; x < width; ++x)
        {
          unsigned char red = 255 * (x / (double)width);
          imageDataScalars[0] = red;
          imageDataScalars[1] = green;
          imageDataScalars[2] = blue;
          imageDataScalars += 3;
        }
      }
      imageDataScalars = (unsigned char*)imageData->GetScalarPointer();
      timestamp += timeBetweenFrames;
      trackedFrame->SetTimestamp(timestamp);
    }

    vtkNew<vtkIGSIOMkvSequenceIO> writer;
    writer->SetFileName(outputImageSequenceFileName.c_str());
    writer->SetTrackedFrameList(outputTrackedFrameList.GetPointer());
    if (!writer->Write())
    {
      LOG_ERROR("Could not write trackedFrameList!");
      return EXIT_FAILURE;
    }
    writer->Close();
  }

  LOG_DEBUG("vtkIGSIOMkvSequenceIOTest completed successfully!");
  return EXIT_SUCCESS;
}

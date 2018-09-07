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

// std includes
#include <iostream>

// VTK includes
#include <vtkNew.h>
#include <vtksys/CommandLineArguments.hxx>

// vtkVideoIO includes
#include "vtkMKVWriter.h"

//----------------------------------------------------------------------------
int main(int argc, char* argv[])
{

  bool printHelp(false);
  std::string filename = "mkvWriterTest1.mkv";
  int width = 10;
  int height = 10;
  int numFrames = 25;
  double fps = 10;

  vtksys::CommandLineArguments args;
  args.Initialize(argc, argv);
  args.AddArgument("--help", vtksys::CommandLineArguments::NO_ARGUMENT, &printHelp, "Print this help.");
  args.AddArgument("--filename", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &filename, "Input filename.");
  args.AddArgument("--width", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &width, "Image width.");
  args.AddArgument("--height", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &height, "Image height.");
  args.AddArgument("--numFrames", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &numFrames, "Number of frames.");
  args.AddArgument("--fps", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &fps, "Frames per second.");

  if (!args.Parse())
  {
    std::cerr << "Problem parsing arguments" << std::endl;
    std::cout << "\n\nvtkMKVWriterTest1 help:" << args.GetHelp() << std::endl;
    return EXIT_FAILURE;
  }

  vtkNew<vtkMKVWriter> mkvWriter;
  mkvWriter->SetFilename(filename);

  if (!mkvWriter->WriteHeader())
  {
    std::cerr << "Could not write header!" << std::endl;
    return EXIT_FAILURE;
  }

  
  int videoTrackNumber = mkvWriter->AddVideoTrack("Test", "RV24", width, height);
  if (videoTrackNumber < 1)
  {
    std::cerr << "Could not create video track!" << std::endl;
    return EXIT_FAILURE;
  }

  int metadataTrackNumber = mkvWriter->AddMetadataTrack("Framenumber");
  if (metadataTrackNumber < 1)
  {
    std::cerr << "Could not create metadata track!" << std::endl;
  }

  vtkNew<vtkImageData> imageData;
  imageData->SetDimensions(width, height, 1);
  imageData->AllocateScalars(VTK_UNSIGNED_CHAR, 3);
  unsigned char* imageDataScalars = (unsigned char*)imageData->GetScalarPointer();
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      unsigned char red = 255 * (x / (double)width);
      unsigned char green = 255 * (y / (double)height);
      imageDataScalars[0] = red;
      imageDataScalars[1] = green;
      imageDataScalars += 3;
    }
  }

  double timeBetweenFrames = 1.0 / fps;
  double timestamp = 0.0;
  for (int i = 0; i < numFrames; ++i)
  {
    imageDataScalars = (unsigned char*)imageData->GetScalarPointer();
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        unsigned char blue = 255 * (i / (double)numFrames);
        imageDataScalars[2] = blue;
        imageDataScalars += 3;
      }
    }
    unsigned long size = imageData->GetScalarSize() * imageData->GetNumberOfScalarComponents() * width * height;
    mkvWriter->WriteEncodedVideoFrame(
      (unsigned char*)imageData->GetScalarPointer(),
      imageData->GetScalarSize() * imageData->GetNumberOfScalarComponents() * width * height,
      true, videoTrackNumber, timestamp);

    mkvWriter->WriteMetadata(std::to_string(i), metadataTrackNumber, timestamp);

    timestamp += timeBetweenFrames;
  }
  
  mkvWriter->Close();
  
  return EXIT_SUCCESS;
}

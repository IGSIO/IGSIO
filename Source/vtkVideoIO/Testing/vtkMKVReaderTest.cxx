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
#include <vtkPNGWriter.h>

// vtkVideoIO includes
#include "vtkMKVReader.h"

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

  if (!args.Parse())
  {
    std::cerr << "Problem parsing arguments" << std::endl;
    std::cout << "\n\nvtkMKVWriterTest1 help:" << args.GetHelp() << std::endl;
    return EXIT_FAILURE;
  }

  vtkNew<vtkMKVReader> mkvReader;
  mkvReader->SetFilename(filename);
  mkvReader->ReadHeader();
  mkvReader->ReadContents();
  mkvReader->Close();

  vtkMKVUtil::VideoTrackMap videoTracks = mkvReader->GetVideoTracks();
  if (videoTracks.size() < 1)
  {
    return EXIT_FAILURE;
  }

  for (vtkMKVUtil::VideoTrackMap::iterator trackIt = videoTracks.begin(); trackIt != videoTracks.end(); ++trackIt)
  {
    vtkMKVUtil::VideoTrackInfo* videoTrack = &trackIt->second;
    if (width != videoTrack->Width ||
        height != videoTrack->Height ||
        numFrames != videoTrack->Frames.size())
    {
      return EXIT_FAILURE;
    }
  }
  
  
  return EXIT_SUCCESS;
}

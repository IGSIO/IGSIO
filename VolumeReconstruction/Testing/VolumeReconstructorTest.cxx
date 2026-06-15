/*=IGSIO=header=begin======================================================
  Program: IGSIO
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================IGSIO=header=end*/

#include "igsioConfigure.h"
#include <iostream>
#include "igsioTrackedFrame.h"
#include "igsioXmlUtils.h"
#include "vtkImageData.h"
#include "vtkMatrix4x4.h"
#include "vtkIGSIOSequenceIO.h"
#include "vtkIGSIOTrackedFrameList.h"
#include "vtkIGSIOTransformRepository.h"
#include "vtkIGSIOVolumeReconstructor.h"
#include "vtkXMLUtilities.h"
#include "vtksys/CommandLineArguments.hxx"

#include <cmath>

//----------------------------------------------------------------------------
// Reads a reconstructed volume file (written as a single-frame IGSIO sequence
// file by vtkIGSIOVolumeReconstructor::SaveReconstructedVolumeToFile) into a
// vtkImageData.
static igsioStatus ReadVolumeFromFile(const std::string& filename, vtkImageData* volume)
{
  vtkSmartPointer<vtkIGSIOTrackedFrameList> list = vtkSmartPointer<vtkIGSIOTrackedFrameList>::New();
  if (vtkIGSIOSequenceIO::Read(filename, list) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to read volume file: " << filename);
    return IGSIO_FAIL;
  }
  if (list->GetNumberOfTrackedFrames() < 1)
  {
    LOG_ERROR("No frames found in volume file: " << filename);
    return IGSIO_FAIL;
  }
  vtkImageData* image = list->GetTrackedFrame(0)->GetImageData()->GetImage();
  if (image == NULL)
  {
    LOG_ERROR("No image data in volume file: " << filename);
    return IGSIO_FAIL;
  }
  volume->DeepCopy(image);
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
// Compares a reconstructed volume against a reference volume, allowing for
// small per-voxel differences (e.g. last-bit floating-point rounding that
// varies between CPU architectures). Returns IGSIO_SUCCESS if the geometry
// matches and the maximum absolute voxel difference is within threshold.
static igsioStatus CompareVolumeToReference(const std::string& volumeFileName, const std::string& referenceFileName, double threshold)
{
  vtkSmartPointer<vtkImageData> volume = vtkSmartPointer<vtkImageData>::New();
  vtkSmartPointer<vtkImageData> reference = vtkSmartPointer<vtkImageData>::New();
  if (ReadVolumeFromFile(volumeFileName, volume) != IGSIO_SUCCESS
    || ReadVolumeFromFile(referenceFileName, reference) != IGSIO_SUCCESS)
  {
    return IGSIO_FAIL;
  }

  int volumeExtent[6] = { 0 };
  int referenceExtent[6] = { 0 };
  volume->GetExtent(volumeExtent);
  reference->GetExtent(referenceExtent);
  for (int i = 0; i < 6; ++i)
  {
    if (volumeExtent[i] != referenceExtent[i])
    {
      LOG_ERROR("Volume extent [" << volumeExtent[0] << " " << volumeExtent[1] << " " << volumeExtent[2] << " "
        << volumeExtent[3] << " " << volumeExtent[4] << " " << volumeExtent[5] << "] does not match reference extent ["
        << referenceExtent[0] << " " << referenceExtent[1] << " " << referenceExtent[2] << " "
        << referenceExtent[3] << " " << referenceExtent[4] << " " << referenceExtent[5] << "]");
      return IGSIO_FAIL;
    }
  }

  int numberOfComponents = volume->GetNumberOfScalarComponents();
  if (numberOfComponents != reference->GetNumberOfScalarComponents())
  {
    LOG_ERROR("Volume has " << numberOfComponents << " scalar component(s) but reference has "
      << reference->GetNumberOfScalarComponents());
    return IGSIO_FAIL;
  }

  double maxDifference = 0.0;
  for (int z = volumeExtent[4]; z <= volumeExtent[5]; ++z)
  {
    for (int y = volumeExtent[2]; y <= volumeExtent[3]; ++y)
    {
      for (int x = volumeExtent[0]; x <= volumeExtent[1]; ++x)
      {
        for (int c = 0; c < numberOfComponents; ++c)
        {
          double difference = std::fabs(volume->GetScalarComponentAsDouble(x, y, z, c)
            - reference->GetScalarComponentAsDouble(x, y, z, c));
          if (difference > maxDifference)
          {
            maxDifference = difference;
          }
        }
      }
    }
  }

  if (maxDifference > threshold)
  {
    LOG_ERROR("Reconstructed volume differs from reference: maximum voxel difference " << maxDifference
      << " exceeds threshold " << threshold);
    return IGSIO_FAIL;
  }

  LOG_INFO("Reconstructed volume matches reference (maximum voxel difference " << maxDifference
    << ", threshold " << threshold << ")");
  return IGSIO_SUCCESS;
}

int main(int argc, char* argv[])
{
  bool printHelp(false);
  // Parse command line arguments.

  std::string inputImgSeqFileName;
  std::string inputConfigFileName;
  std::string outputVolumeFileName;
  std::string outputVolumeAlphaFileNameDeprecated;
  std::string outputVolumeAccumulationFileName;
  std::string outputFrameFileName;
  std::string importanceMaskFileName;
  std::string inputImageToReferenceTransformName;
  std::string referenceVolumeFileName;
  double comparisonThreshold = 0.0;

  // Deprecated arguments (2013-07-29, #800)
  std::string inputImageToReferenceTransformNameDeprecated;
  std::string inputImgSeqFileNameDeprecated;

  int verboseLevel = vtkIGSIOLogger::LOG_LEVEL_UNDEFINED;

  bool disableCompression = false;

  vtksys::CommandLineArguments cmdargs;
  cmdargs.Initialize(argc, argv);

  cmdargs.AddArgument("--image-to-reference-transform", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputImageToReferenceTransformName, "Name of the transform to define the image slice pose relative to the reference coordinate system (e.g., ImageToReference). Note that this parameter is optional, if it is defined then it overrides the ImageCoordinateFrame and ReferenceCoordinateFrame attribute values in the configuration file.");
  cmdargs.AddArgument("--source-seq-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputImgSeqFileName, "Input sequence file filename (.mha/.nrrd)");
  cmdargs.AddArgument("--config-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputConfigFileName, "Input configuration file name (.xml)");
  cmdargs.AddArgument("--output-volume-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &outputVolumeFileName, "Output file name of the reconstructed volume (must have .mha, .mhd, .nrrd or .nhdr extension)");
  cmdargs.AddArgument("--output-volume-accumulation-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &outputVolumeAccumulationFileName, "Output file name of the accumulation of the reconstructed volume (.mha/.nrrd)");
  cmdargs.AddArgument("--output-frame-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &outputFrameFileName, "A filename that will be used for storing the tracked image frames. Each frame will be exported individually, with the proper position and orientation in the reference coordinate system");
  cmdargs.AddArgument("--help", vtksys::CommandLineArguments::NO_ARGUMENT, &printHelp, "Print this help.");
  cmdargs.AddArgument("--disable-compression", vtksys::CommandLineArguments::NO_ARGUMENT, &disableCompression, "Do not compress output image files.");
  cmdargs.AddArgument("--verbose", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &verboseLevel, "Verbose level (1=error only, 2=warning, 3=info, 4=debug, 5=trace)");
  cmdargs.AddArgument("--importance-mask-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &importanceMaskFileName, "The file to use as the importance mask.");
  cmdargs.AddArgument("--reference-volume-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &referenceVolumeFileName, "Reference volume file name (.mha/.nrrd) to compare against. When specified, the test runs in comparison mode: it reads --output-volume-file and this reference file and verifies that the maximum absolute voxel difference is within --comparison-threshold, instead of performing a reconstruction.");
  cmdargs.AddArgument("--comparison-threshold", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &comparisonThreshold, "Maximum allowed absolute per-voxel difference when comparing against a reference volume (default: 0). Used only with --reference-volume-file.");

  // Deprecated arguments (2013-07-29, #800)
  cmdargs.AddArgument("--transform", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputImageToReferenceTransformNameDeprecated, "Image to reference transform name used for the reconstruction. DEPRECATED, use --image-to-reference-transform argument instead");
  cmdargs.AddArgument("--img-seq-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputImgSeqFileNameDeprecated, "Input sequence file filename (.mha/.nrrd). DEPRECATED: use --source-seq-file argument instead");
  // Deprecated argument (2014-08-15, #923)
  cmdargs.AddArgument("--output-volume-alpha-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &outputVolumeAlphaFileNameDeprecated, "Output file name of the alpha channel of the reconstructed volume (.mha/.nrrd). DEPRECATED: use --output-volume-accumulation-file argument instead");

  if (!cmdargs.Parse())
  {
    std::cerr << "Problem parsing arguments" << std::endl;
    std::cout << "Help: " << cmdargs.GetHelp() << std::endl;
    exit(EXIT_FAILURE);
  }

  if (printHelp)
  {
    std::cout << cmdargs.GetHelp() << std::endl;
    exit(EXIT_SUCCESS);
  }

  // Set the log level
  vtkIGSIOLogger::Instance()->SetLogLevel(verboseLevel);

  // Deprecated arguments (2013-07-29, #800)
  if (!inputImageToReferenceTransformNameDeprecated.empty())
  {
    LOG_WARNING("The --transform argument is deprecated. Use --image-to-reference-transform instead.");
    if (inputImageToReferenceTransformName.empty())
    {
      inputImageToReferenceTransformName = inputImageToReferenceTransformNameDeprecated;
    }
  }
  if (!inputImgSeqFileNameDeprecated.empty())
  {
    LOG_WARNING("The --img-seq-file argument is deprecated. Use --source-seq-file instead.");
    if (inputImgSeqFileName.empty())
    {
      inputImgSeqFileName = inputImgSeqFileNameDeprecated;
    }
  }
  // Deprecated argument (2014-08-15, #923)
  if (!outputVolumeAlphaFileNameDeprecated.empty())
  {
    LOG_WARNING("The --output-volume-alpha-file argument is deprecated. Use --output-volume-accumulation-file instead.");
    if (outputVolumeAccumulationFileName.empty())
    {
      outputVolumeAccumulationFileName = outputVolumeAlphaFileNameDeprecated;
    }
  }

  // Comparison mode: compare a previously reconstructed volume against a reference volume.
  if (!referenceVolumeFileName.empty())
  {
    if (outputVolumeFileName.empty())
    {
      std::cout << "ERROR: --output-volume-file must be specified to compare against --reference-volume-file!" << std::endl;
      std::cout << "Help: " << cmdargs.GetHelp() << std::endl;
      exit(EXIT_FAILURE);
    }
    if (CompareVolumeToReference(outputVolumeFileName, referenceVolumeFileName, comparisonThreshold) != IGSIO_SUCCESS)
    {
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  if (inputConfigFileName.empty())
  {
    std::cout << "ERROR: Input config file name is missing!" << std::endl;
    std::cout << "Help: " << cmdargs.GetHelp() << std::endl;
    exit(EXIT_FAILURE);
  }

  vtkSmartPointer<vtkIGSIOVolumeReconstructor> reconstructor = vtkSmartPointer<vtkIGSIOVolumeReconstructor>::New();

  if (!importanceMaskFileName.empty())
  {
    reconstructor->SetImportanceMaskFilename(importanceMaskFileName);
  }


  LOG_INFO("Reading configuration file:" << inputConfigFileName);
  vtkSmartPointer<vtkXMLDataElement> configRootElement = vtkSmartPointer<vtkXMLDataElement>::New();
  if (igsioXmlUtils::ReadDeviceSetConfigurationFromFile(configRootElement, inputConfigFileName.c_str()) == IGSIO_FAIL)
  {
    LOG_ERROR("Unable to read configuration from file " << inputConfigFileName.c_str());
    return EXIT_FAILURE;
  }

  if (reconstructor->ReadConfiguration(configRootElement) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to read configuration from " << inputConfigFileName.c_str());
    return EXIT_FAILURE;
  }

  vtkSmartPointer<vtkIGSIOTransformRepository> transformRepository = vtkSmartPointer<vtkIGSIOTransformRepository>::New();
  if (configRootElement->FindNestedElementWithName("CoordinateDefinitions") != NULL)
  {
    if (transformRepository->ReadConfiguration(configRootElement) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to read transforms from CoordinateDefinitions");
      return EXIT_FAILURE;
    }
  }
  else
  {
    LOG_DEBUG("No transforms were found in CoordinateDefinitions. Only the transforms defined in the input image will be available.");
  }

  // Print calibration transform
  std::ostringstream osTransformRepo;
  transformRepository->Print(osTransformRepo);
  LOG_DEBUG("Transform repository: \n" << osTransformRepo.str());

  // Read image sequence
  LOG_INFO("Reading image sequence " << inputImgSeqFileName);
  vtkSmartPointer<vtkIGSIOTrackedFrameList> trackedFrameList = vtkSmartPointer<vtkIGSIOTrackedFrameList>::New();
  if (vtkIGSIOSequenceIO::Read(inputImgSeqFileName, trackedFrameList) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Unable to load input sequences file.");
    exit(EXIT_FAILURE);
  }

  // Reconstruct volume
  igsioTransformName imageToReferenceTransformName;
  if (!inputImageToReferenceTransformName.empty())
  {
    // image to reference transform is specified at the command-line
    if (imageToReferenceTransformName.SetTransformName(inputImageToReferenceTransformName.c_str()) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Invalid image to reference transform name: " << inputImageToReferenceTransformName);
      return EXIT_FAILURE;
    }
    reconstructor->SetImageCoordinateFrame(imageToReferenceTransformName.From());
    reconstructor->SetReferenceCoordinateFrame(imageToReferenceTransformName.To());
  }

  LOG_INFO("Set volume output extent...");
  std::string errorDetail;
  if (reconstructor->SetOutputExtentFromFrameList(trackedFrameList, transformRepository, errorDetail) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to set output extent of volume!");
    return EXIT_FAILURE;
  }

  LOG_INFO("Reconstruct volume...");
  const int numberOfFrames = trackedFrameList->GetNumberOfTrackedFrames();
  int numberOfFramesAddedToVolume = 0;

  for (int frameIndex = 0; frameIndex < numberOfFrames; frameIndex += reconstructor->GetSkipInterval())
  {
    LOG_DEBUG("Frame: " << frameIndex);
    vtkIGSIOLogger::PrintProgressbar((100.0 * frameIndex) / numberOfFrames);

    igsioTrackedFrame* frame = trackedFrameList->GetTrackedFrame(frameIndex);

    if (transformRepository->SetTransforms(*frame) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to update transform repository with frame #" << frameIndex);
      continue;
    }

    // Insert slice for reconstruction
    bool insertedIntoVolume = false;
	bool isFirst = frameIndex == 0;
	bool isLast = frameIndex + reconstructor->GetSkipInterval() >= numberOfFrames;
    if (reconstructor->AddTrackedFrame(frame, transformRepository, isFirst, isLast, &insertedIntoVolume) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to add tracked frame to volume with frame #" << frameIndex);
      continue;
    }

    if (insertedIntoVolume)
    {
      numberOfFramesAddedToVolume++;
    }

    // Write an ITK image with the image pose in the reference coordinate system
    if (!outputFrameFileName.empty())
    {
      vtkSmartPointer<vtkMatrix4x4> imageToReferenceTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
      if (transformRepository->GetTransform(imageToReferenceTransformName, imageToReferenceTransformMatrix) != IGSIO_SUCCESS)
      {
        std::string strImageToReferenceTransformName;
        imageToReferenceTransformName.GetTransformName(strImageToReferenceTransformName);
        LOG_ERROR("Failed to get transform '" << strImageToReferenceTransformName << "' from transform repository!");
        continue;
      }

      // Print the image to reference transform
      std::ostringstream os;
      imageToReferenceTransformMatrix->Print(os);
      LOG_TRACE("Image to reference transform: \n" << os.str());

      // Insert frame index before the file extension (image.mha => image001.mha)
      std::ostringstream ss;
      size_t found;
      found = outputFrameFileName.find_last_of(".");
      ss << outputFrameFileName.substr(0, found);
      ss.width(3);
      ss.fill('0');
      ss << frameIndex;
      ss << outputFrameFileName.substr(found);

      //igsioCommon::WriteToFile(frame, ss.str(), imageToReferenceTransformMatrix);
    }
  }

  vtkIGSIOLogger::PrintProgressbar(100);

  trackedFrameList->Clear();

  LOG_INFO("Number of frames added to the volume: " << numberOfFramesAddedToVolume << " out of " << numberOfFrames);

  LOG_INFO("Saving volume to file...");
  reconstructor->SaveReconstructedVolumeToFile(outputVolumeFileName, false, !disableCompression);

  if (!outputVolumeAccumulationFileName.empty())
  {
    reconstructor->SaveReconstructedVolumeToFile(outputVolumeAccumulationFileName, true, !disableCompression);
  }

  return EXIT_SUCCESS;
}

/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

//#include "PlusConfigure.h"
#include "vtksys/CommandLineArguments.hxx"
#include <iomanip>

#include "vtkSmartPointer.h"
#include "vtkMatrix4x4.h"

#include "vtkIGSIOMetaImageSequenceIO.h"

#include "vtkIGSIOTrackedFrameList.h"
#include "igsioTrackedFrame.h"


///////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
  std::string inputImageSequenceFileName;
  std::string outputImageSequenceFileName;

  int numberOfFailures(0);

  vtksys::CommandLineArguments args;
  args.Initialize(argc, argv);

  args.AddArgument("--img-seq-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputImageSequenceFileName, "Filename of the input image sequence.");
  args.AddArgument("--output-img-seq-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &outputImageSequenceFileName, "Filename of the output image sequence.");

  if (!args.Parse())
  {
    std::cerr << "Problem parsing arguments" << std::endl;
    std::cout << "Help: " << args.GetHelp() << std::endl;
    exit(EXIT_FAILURE);
  }

  if (inputImageSequenceFileName.empty())
  {
    std::cerr << "--img-seq-file is required" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (outputImageSequenceFileName.empty())
  {
    std::cerr << "output-img-seq-file-name is required" << std::endl;
    exit(EXIT_FAILURE);
  }

  ///////////////

  vtkSmartPointer<vtkIGSIOMetaImageSequenceIO> reader = vtkSmartPointer<vtkIGSIOMetaImageSequenceIO>::New();
  reader->SetFileName(inputImageSequenceFileName.c_str());
  if (reader->Read() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Couldn't read sequence metafile: " <<  inputImageSequenceFileName);
    return EXIT_FAILURE;
  }

  vtkIGSIOTrackedFrameList* trackedFrameList = reader->GetTrackedFrameList();

  if (trackedFrameList == NULL)
  {
    LOG_ERROR("Unable to get trackedFrameList!");
    return EXIT_FAILURE;
  }

  LOG_DEBUG("Test GetCustomString method ...");
  const char* imgOrientation = trackedFrameList->GetCustomString("UltrasoundImageOrientation");
  if (imgOrientation == NULL)
  {
    LOG_ERROR("Unable to get custom string!");
    numberOfFailures++;
  }

  LOG_DEBUG("Test GetCustomTransform method ...");
  double tImageToTool[16];
  if (!trackedFrameList->GetCustomTransform("ImageToToolTransform", tImageToTool))
  {
    LOG_ERROR("Unable to get custom transform!");
    numberOfFailures++;
  }

  // Create an absolute path to the output image sequence, in the output directory
  //outputImageSequenceFileName = vtkPlusConfig::GetInstance()->GetOutputPath(outputImageSequenceFileName);

  // ******************************************************************************
  // Test writing

  vtkSmartPointer<vtkIGSIOMetaImageSequenceIO> writer = vtkSmartPointer<vtkIGSIOMetaImageSequenceIO>::New();
  writer->UseCompressionOn();
  writer->SetFileName(outputImageSequenceFileName.c_str());
  writer->SetTrackedFrameList(trackedFrameList);

  LOG_DEBUG("Test SetFrameTransform method ...");
  // Add the transformation matrix to metafile
  int numberOfFrames = trackedFrameList->GetNumberOfTrackedFrames();
  for (int i = 0 ; i < numberOfFrames; i++)
  {
    vtkSmartPointer<vtkMatrix4x4> transMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    transMatrix->SetElement(0, 0, i);
    writer->GetTrackedFrame(i)->SetFrameTransform(
      igsioTransformName("Tool", "Tracker"),
      transMatrix);
  }

  vtkSmartPointer<vtkMatrix4x4> highPrecMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  highPrecMatrix->SetElement(0, 0, 0.12345678901234567890);
  highPrecMatrix->SetElement(0, 1, 1.12345678901234567890);
  highPrecMatrix->SetElement(0, 2, 2.12345678901234567890);
  highPrecMatrix->SetElement(1, 0, 10.12345678901234567890);
  highPrecMatrix->SetElement(1, 1, 11.12345678901234567890);
  highPrecMatrix->SetElement(1, 2, 12.12345678901234567890);
  highPrecMatrix->SetElement(2, 0, 20.12345678901234567890);
  highPrecMatrix->SetElement(2, 1, 21.12345678901234567890);
  highPrecMatrix->SetElement(2, 2, 22.12345678901234567890);
  highPrecMatrix->SetElement(0, 3, 12345.12345678901234567890);
  highPrecMatrix->SetElement(0, 3, 23456.12345678901234567890);
  highPrecMatrix->SetElement(0, 3, 34567.12345678901234567890);

  const double highPrecTimeOffset = 0.123456789012345678901234567890;

  igsioTransformName highPrecTransformName("HighPrecTool", "Tracker");
  for (int i = 0 ; i < numberOfFrames; i++)
  {
    vtkSmartPointer<vtkMatrix4x4> transMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    transMatrix->DeepCopy(highPrecMatrix);
    writer->GetTrackedFrame(i)->SetFrameTransform(highPrecTransformName, transMatrix);
    writer->GetTrackedFrame(i)->SetTimestamp(double(i) + highPrecTimeOffset);
  }

  LOG_DEBUG("Test SetCustomTransform method ...");
  vtkSmartPointer<vtkMatrix4x4> calibMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  calibMatrix->Identity();
  writer->GetTrackedFrameList()->SetCustomTransform("ImageToToolTransform", calibMatrix);

  if (writer->Write() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Couldn't write sequence metafile: " << outputImageSequenceFileName);
    return EXIT_FAILURE;
  }

  igsioTransformName tnToolToTracker("Tool", "Tracker");
  LOG_DEBUG("Test GetFrameTransform method ...");
  for (int i = 0; i < numberOfFrames; i++)
  {
    vtkSmartPointer<vtkMatrix4x4> writerMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkSmartPointer<vtkMatrix4x4> readerMatrix = vtkSmartPointer<vtkMatrix4x4>::New();

    if (!reader->GetTrackedFrameList()->GetTrackedFrame(i)->GetFrameTransform(tnToolToTracker, readerMatrix))
    {
      LOG_ERROR("Unable to get ToolToTracker frame transform to frame #" << i);
      numberOfFailures++;
    }

    if (!writer->GetTrackedFrame(i)->GetFrameTransform(tnToolToTracker, writerMatrix))
    {
      LOG_ERROR("Unable to get ToolToTracker frame transform to frame #" << i);
      numberOfFailures++;
    }

    for (int row = 0; row < 4; row++)
    {
      for (int col = 0; col < 4; col++)
      {
        if (readerMatrix->GetElement(row, col) != writerMatrix->GetElement(row, col))
        {
          LOG_ERROR("The input and output matrices are not the same at element: (" << row << ", " << col << "). ");
          numberOfFailures++;
        }
      }
    }

    if (!reader->GetTrackedFrameList()->GetTrackedFrame(i)->GetFrameTransform(highPrecTransformName, readerMatrix))
    {
      LOG_ERROR("Unable to get high precision frame transform for frame #" << i);
      numberOfFailures++;
    }
    const double FLOAT_COMPARISON_TOLERANCE = 1e-12;
    for (int row = 0; row < 4; row++)
    {
      for (int col = 0; col < 4; col++)
      {

        if (fabs(readerMatrix->GetElement(row, col) - highPrecMatrix->GetElement(row, col)) > FLOAT_COMPARISON_TOLERANCE)
        {
          LOG_ERROR("The input and output matrices are not the same at element: (" << row << ", " << col << "). ");
          numberOfFailures++;
        }
      }
    }
    double readerTimestamp = reader->GetTrackedFrameList()->GetTrackedFrame(i)->GetTimestamp();
    double expectedTimestamp = double(i) + highPrecTimeOffset;
    if (fabs(expectedTimestamp - readerTimestamp) > FLOAT_COMPARISON_TOLERANCE)
    {
      LOG_ERROR("The timestamp is not precise enough at frame " << i);
      numberOfFailures++;
    }

  }

  // ******************************************************************************
  // Test image status

  vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
  matrix->SetElement(0, 3, -50);
  matrix->SetElement(0, 3, 150);

  vtkSmartPointer<vtkIGSIOTrackedFrameList> dummyTrackedFrame = vtkSmartPointer<vtkIGSIOTrackedFrameList>::New();
  igsioTrackedFrame validFrame;
  FrameSizeType frameSize = {200, 200, 1};
  validFrame.GetImageData()->AllocateFrame(frameSize, VTK_UNSIGNED_CHAR, 1);
  validFrame.GetImageData()->FillBlank();
  validFrame.SetFrameTransform(igsioTransformName("Image", "Probe"), matrix);
  validFrame.SetFrameField("FrameNumber", "0");
  validFrame.SetTimestamp(1.0);

  igsioTrackedFrame invalidFrame;
  invalidFrame.SetFrameTransform(igsioTransformName("Image", "Probe"), matrix);
  invalidFrame.SetFrameField("FrameNumber", "1");
  invalidFrame.SetTimestamp(2.0);

  igsioTrackedFrame validFrame_copy(validFrame);
  validFrame_copy.SetTimestamp(3.0);
  validFrame_copy.SetFrameField("FrameNumber", "3");

  dummyTrackedFrame->AddTrackedFrame(&validFrame);
  dummyTrackedFrame->AddTrackedFrame(&invalidFrame);
  dummyTrackedFrame->AddTrackedFrame(&validFrame_copy);

  vtkSmartPointer<vtkIGSIOMetaImageSequenceIO> writerImageStatus = vtkSmartPointer<vtkIGSIOMetaImageSequenceIO>::New();
  writerImageStatus->SetFileName(outputImageSequenceFileName.c_str());
  writerImageStatus->SetTrackedFrameList(dummyTrackedFrame);
  writerImageStatus->UseCompressionOn();

  if (writerImageStatus->Write() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Couldn't write sequence metafile: " << outputImageSequenceFileName);
    return EXIT_FAILURE;
  }

  vtkSmartPointer<vtkIGSIOMetaImageSequenceIO> readerImageStatus = vtkSmartPointer<vtkIGSIOMetaImageSequenceIO>::New();
  readerImageStatus->SetFileName(outputImageSequenceFileName.c_str());
  if (readerImageStatus->Read() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Couldn't read sequence metafile: " <<  outputImageSequenceFileName);
    return EXIT_FAILURE;
  }
  vtkIGSIOTrackedFrameList* trackedFrameListImageStatus = readerImageStatus->GetTrackedFrameList();
  if (trackedFrameListImageStatus == NULL)
  {
    LOG_ERROR("Unable to get trackedFrameList!");
    return EXIT_FAILURE;
  }

  if (trackedFrameListImageStatus->GetNumberOfTrackedFrames() != 3
      && !trackedFrameListImageStatus->GetTrackedFrame(1)->GetImageData()->IsImageValid())
  {
    LOG_ERROR("Image status read/write failed!");
    return EXIT_FAILURE;
  }

  // Test metafile writting with different sized images
  igsioTrackedFrame differentSizeFrame;
  FrameSizeType frameSizeSmaller = {150, 150, 1};
  differentSizeFrame.GetImageData()->AllocateFrame(frameSizeSmaller, VTK_UNSIGNED_CHAR, 1);
  differentSizeFrame.GetImageData()->FillBlank();
  differentSizeFrame.SetFrameTransform(igsioTransformName("Image", "Probe"), matrix);
  differentSizeFrame.SetFrameField("FrameNumber", "6");
  differentSizeFrame.SetTimestamp(6.0);
  dummyTrackedFrame->AddTrackedFrame(&differentSizeFrame);

  vtkSmartPointer<vtkIGSIOMetaImageSequenceIO> writerDiffSize = vtkSmartPointer<vtkIGSIOMetaImageSequenceIO>::New();
  writerDiffSize->SetFileName(outputImageSequenceFileName.c_str());
  writerDiffSize->SetTrackedFrameList(dummyTrackedFrame);
  writerDiffSize->UseCompressionOff();

  // We should get an error when trying to write a sequence with different frame sizes into file
  int oldVerboseLevel = vtkIGSIOLogger::Instance()->GetLogLevel();
  vtkIGSIOLogger::Instance()->SetLogLevel(vtkIGSIOLogger::LOG_LEVEL_ERROR - 1); // temporarily disable error logging (as we are expecting an error)
  if (writerDiffSize->Write() == IGSIO_SUCCESS)
  {
    vtkIGSIOLogger::Instance()->SetLogLevel(oldVerboseLevel);
    LOG_ERROR("Expect a 'Frame size mismatch' error in vtkPlusMetaImageSequenceIO but the operation has been reported to be successful.");
    return EXIT_FAILURE;
  }
  vtkIGSIOLogger::Instance()->SetLogLevel(oldVerboseLevel);

  if (numberOfFailures > 0)
  {
    LOG_ERROR("Total number of failures: " << numberOfFailures);
    LOG_ERROR("vtkIGSIOMetaImageSequenceIOTest1 failed!");
    return EXIT_FAILURE;
  }

  LOG_INFO("vtkIGSIOMetaImageSequenceIOTest1 completed successfully!");
  return EXIT_SUCCESS;
}

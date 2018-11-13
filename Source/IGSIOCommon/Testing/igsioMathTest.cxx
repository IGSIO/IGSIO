/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/


#include <deque>

#include "vtkDebugLeaksManager.h"
#include "vtkSystemIncludes.h"
#include "vtkDebugLeaksManager.h"

#include "vtksys/CommandLineArguments.hxx"
#include "igsioMath.h"
#include "vtkXMLDataElement.h"
#include "vtkXMLUtilities.h"
#include "vtkMath.h"
#include "vtkIGSIOAccurateTimer.h"

const double DOUBLE_DIFF = 0.0001; // used for comparing double numbers

template<class floatType> int TestFloor(const char* floatName);

//----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  int numberOfErrors(0); 

  bool printHelp(false);

  int verboseLevel = vtkIGSIOLogger::LOG_LEVEL_UNDEFINED;

  vtksys::CommandLineArguments args;
  args.Initialize(argc, argv);
  std::string inputDataFileName(""); 

  args.AddArgument("--help", vtksys::CommandLineArguments::NO_ARGUMENT, &printHelp, "Print this help.");  
  args.AddArgument("--xml-file", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &inputDataFileName, "Input XML data file name");  
  args.AddArgument("--verbose", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &verboseLevel, "Verbose level (1=error only, 2=warning, 3=info, 4=debug, 5=trace)");  

  if ( !args.Parse() )
  {
    std::cerr << "Problem parsing arguments" << std::endl;
    std::cout << "Help: " << args.GetHelp() << std::endl;
    exit(EXIT_FAILURE);
  }
  
  if ( printHelp ) 
  {
    std::cout << args.GetHelp() << std::endl;
    exit(EXIT_SUCCESS); 
  }

  vtkIGSIOLogger::Instance()->SetLogLevel(verboseLevel);

  if ( inputDataFileName.empty() )
  {
    std::cerr << "input-data-file argument required!" << std::endl;
    std::cout << "Help: " << args.GetHelp() << std::endl;
    exit(EXIT_FAILURE);
  }  

  vtkSmartPointer<vtkXMLDataElement> xmligsioMathTest = vtkSmartPointer<vtkXMLDataElement>::Take(
    vtkXMLUtilities::ReadElementFromFile(inputDataFileName.c_str()));

  if ( xmligsioMathTest == NULL )
  {
    LOG_ERROR("Failed to read input xml configuration file from file: " << inputDataFileName );
    exit(EXIT_FAILURE);
  }
  
  // Test igsioMath::LSQRMinimize 
  numberOfErrors += TestFloor<float>("float");
  numberOfErrors += TestFloor<double>("double");

  if ( numberOfErrors > 0 ) 
  {
    LOG_INFO("Test failed!"); 
    return EXIT_FAILURE; 
  }

  //vtkXMLUtilities::WriteElementToFile(xmligsioMathTest, "d:/igsioMathTestData.xml", &vtkIndent(2)); 

  LOG_INFO("Test finished successfully!"); 
  return EXIT_SUCCESS; 
}

template<class floatType> int TestFloor(const char* floatName)
{
  const int repeatOperations=500;
  const int numberOfOperations=100000;

  // initialize random number generation with the sub-millisecond part of the current time
  srand((unsigned int)(vtkIGSIOAccurateTimer::GetSystemTime()-floor(vtkIGSIOAccurateTimer::GetSystemTime()))*1e6); 

  //typedef double floatType;

  std::deque<floatType> testFloatNumbers(numberOfOperations);
  for (int i=0; i<numberOfOperations; i++)
  {
    testFloatNumbers[i]=1000 * floatType(rand())/floatType(RAND_MAX) - 500;
  }
  
  std::deque<floatType> testResultsPlusFloor(numberOfOperations);
  double timestampDiffPlusFloor=0;
  {
    double timestampBefore=vtkIGSIOAccurateTimer::GetSystemTime();
    for (int rep=0; rep<repeatOperations; rep++)
    {
      for (int i=0; i<numberOfOperations; i++)
      {
        testResultsPlusFloor[i]=igsioMath::Floor(testFloatNumbers[i]);
      }
    }
    double timestampAfter=vtkIGSIOAccurateTimer::GetSystemTime(); 
    timestampDiffPlusFloor=timestampAfter-timestampBefore;
  }

  std::deque<floatType> testResultsFloor(numberOfOperations);
  double timestampDiffFloor=0;
  {
    double timestampBefore=vtkIGSIOAccurateTimer::GetSystemTime();
    for (int rep=0; rep<repeatOperations; rep++)
    {
      for (int i=0; i<numberOfOperations; i++)
      {
        testResultsFloor[i]=floor(testFloatNumbers[i]);
      }
    }
    double timestampAfter=vtkIGSIOAccurateTimer::GetSystemTime(); 
    timestampDiffFloor=timestampAfter-timestampBefore;
  }

  double timestampDiffVtkFloor=0;
  {
    double timestampBefore=vtkIGSIOAccurateTimer::GetSystemTime();
    for (int rep=0; rep<repeatOperations; rep++)
    {
      for (int i=0; i<numberOfOperations; i++)
      {
        testResultsFloor[i]=vtkMath::Floor(testFloatNumbers[i]);
      }
    }
    double timestampAfter=vtkIGSIOAccurateTimer::GetSystemTime(); 
    timestampDiffVtkFloor=timestampAfter-timestampBefore;
  }

  int numberOfErrors=0;
  int numberOfigsioMathFloorMismatches=0;
  for (int i=0; i<numberOfOperations; i++)
  {
    if ( testResultsFloor[i] != testResultsPlusFloor[i] )
    {
      LOG_INFO("igsioMath::Floor computation mismatch for input "
  <<std::scientific<<std::setprecision(std::numeric_limits<double>::digits10+1)
  <<testFloatNumbers[i]<<": "<<testResultsFloor[i]<<" (using vtkMath::Floor) != "<<testResultsPlusFloor[i]<<" (using igsioMath::Floor)");
      numberOfigsioMathFloorMismatches++;      
    }    
  }
  
  double percentageigsioMathFloorMismatches=double(numberOfigsioMathFloorMismatches)/numberOfOperations*100.0;
  const double percentageigsioMathFloorMismatchesAlowed=0.01; // 1.0 means 1 percent, so we allow 1/10000 error rate
  if (percentageigsioMathFloorMismatches>percentageigsioMathFloorMismatchesAlowed)
  {
    LOG_ERROR("Percentage of floor computation errors with igsioMath::Floor() is "<<percentageigsioMathFloorMismatches<<"%, which is out of the acceptable range (maximum "<<percentageigsioMathFloorMismatchesAlowed<<"%)");
    numberOfErrors++;
  }
  else
  {
    LOG_INFO("Percentage of floor computation errors with igsioMath::Floor() is "<<percentageigsioMathFloorMismatches<<"%, which is within the acceptable range (maximum "<<percentageigsioMathFloorMismatchesAlowed<<"%)");
  }
  

  LOG_INFO("Time required for "<<numberOfOperations*repeatOperations<<" floor operations on type "<<floatName<<": "
    <<"using igsioMath::Floor: "<<timestampDiffPlusFloor<<"sec, "
    <<"using vtkMath::Floor: "<<timestampDiffVtkFloor<<"sec, "    
    <<"using floor:"<<timestampDiffFloor<<"sec");

  if (timestampDiffPlusFloor>timestampDiffFloor*1.10)
  {
    LOG_ERROR("The optimized floor implementation is more than 10% slower than the unoptimized version.");
    numberOfErrors++;
  }

  // Testing a special value, see http://web.archiveorange.com/archive/v/aysypwArfEx6YnyPN3OM
  floatType specialValueKnownBad=16.999993; // known bad
  int plusMathFloored=igsioMath::Floor(specialValueKnownBad);
  int vtkMathFloored=vtkMath::Floor(specialValueKnownBad);
  int floored=floor(specialValueKnownBad);
  if (plusMathFloored!=floored)
  {
    LOG_INFO(std::fixed<<"The "<<specialValueKnownBad<<" value was incorrectly floored by igsioMath::Floor to "<<plusMathFloored<<" (instead of "<<floored<<"). This is a known limitation of the fast floor implementation.");
  }
  if (vtkMathFloored!=floored)
  {
    LOG_ERROR(std::fixed<<"The "<<specialValueKnownBad<<" value was incorrectly floored by vtkMath::Floor to "<<plusMathFloored<<" (instead of "<<floored<<")");
    numberOfErrors++;
  }

  floatType specialValueGood=16.999991; // good
  plusMathFloored=igsioMath::Floor(specialValueGood);
  vtkMathFloored=vtkMath::Floor(specialValueGood);
  floored=floor(specialValueGood);
  if (plusMathFloored!=floored)
  {
    LOG_ERROR(std::fixed<<"The "<<specialValueGood<<" value was incorrectly floored by igsioMath::Floor to "<<plusMathFloored<<" (instead of "<<floored<<"). This is a known limitation of the fast floor implementation.");
  numberOfErrors++;
  }
  else
  {
    LOG_INFO(std::fixed<<"The "<<specialValueGood<<" value was correctly floored by vtkMath::Floor to "<<plusMathFloored);
  }
  if (vtkMathFloored!=floored)
  {
    LOG_ERROR(std::fixed<<"The "<<specialValueGood<<" value was incorrectly floored by vtkMath::Floor to "<<plusMathFloored<<" (instead of "<<floored<<")");
    numberOfErrors++;
  }
  

  return numberOfErrors;
}

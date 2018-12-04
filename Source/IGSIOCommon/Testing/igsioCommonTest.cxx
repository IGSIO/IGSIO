/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

// Local includes
#include "vtkIGSIORecursiveCriticalSection.h"
#include "igsioCommon.h"
#include "igsioXmlUtils.h"

// VTK includes
#include <vtkSmartPointer.h>
#include <vtksys/CommandLineArguments.hxx>

namespace
{
  static const double DOUBLE_THRESHOLD = 0.0001;

  igsioStatus TestStringFunctions()
  {
    // re #1182
    std::string baseline("Testing PLUS is fun!");
    std::string lowercase("Testing plus is fun!");
    if (!igsioCommon::IsEqualInsensitive(baseline, lowercase))
    {
      return IGSIO_FAIL;
    }

    std::wstring w_baseline(L"Testing PLUS is fun!");
    std::wstring w_lowercase(L"Testing plus is fun!");
    if (!igsioCommon::IsEqualInsensitive(w_baseline, w_lowercase))
    {
      return IGSIO_FAIL;
    }

    if (!igsioCommon::HasSubstrInsensitive(baseline, "PLUS") || !igsioCommon::HasSubstrInsensitive(baseline, "plus"))
    {
      return IGSIO_FAIL;
    }
    if (!igsioCommon::HasSubstrInsensitive(w_baseline, L"PLUS") || !igsioCommon::HasSubstrInsensitive(w_baseline, L"plus"))
    {
      return IGSIO_FAIL;
    }

    std::string tokenBaseline("Testing;PLUS;is;fun!;;");
    std::vector<std::string> tokens;
    igsioCommon::SplitStringIntoTokens(tokenBaseline, ';', tokens, false);
    if (tokens.size() != 4)
    {
      return IGSIO_FAIL;
    }
    if (!igsioCommon::IsEqualInsensitive(tokens[0], "Testing") ||
        !igsioCommon::IsEqualInsensitive(tokens[1], "PLUS") ||
        !igsioCommon::IsEqualInsensitive(tokens[2], "is") ||
        !igsioCommon::IsEqualInsensitive(tokens[3], "fun!"))
    {
      return IGSIO_FAIL;
    }
    tokens.clear();
    igsioCommon::SplitStringIntoTokens(tokenBaseline, ';', tokens, true);
    if (tokens.size() != 5)
    {
      return IGSIO_FAIL;
    }
    if (!tokens[4].empty())
    {
      return IGSIO_FAIL;
    }

    std::string reconstructed;
    igsioCommon::JoinTokensIntoString(tokens, reconstructed, ';');
    if (!igsioCommon::IsEqualInsensitive(reconstructed, "Testing;PLUS;is;fun!;"))
    {
      return IGSIO_FAIL;
    }

    std::string spaces(" Testing PLUS is fun!");
    std::string trimmed = igsioCommon::Trim(spaces);
    if (igsioCommon::IsEqualInsensitive(spaces, trimmed))
    {
      return IGSIO_FAIL;
    }

    spaces = "Testing PLUS is fun!";
    trimmed = igsioCommon::Trim(spaces);
    if (!igsioCommon::IsEqualInsensitive(spaces, trimmed))
    {
      return IGSIO_FAIL;
    }

    return IGSIO_SUCCESS;
  }

  igsioStatus TestXMLFunctions()
  {
    std::string theXML = "<PlusConfiguration version=\"2.1\"><DataCollection StartupDelaySec = \"1.0\"><DeviceSet></DeviceSet></DataCollection></PlusConfiguration>";
    vtkSmartPointer<vtkXMLDataElement> rootElement = vtkSmartPointer<vtkXMLDataElement>::Take(vtkXMLUtilities::ReadElementFromString(theXML.c_str()));
    if (rootElement == NULL)
    {
      return IGSIO_FAIL;
    }

    bool isEqual(false);
    if (igsioCommon::XML::SafeCheckAttributeValueInsensitive(*rootElement, "version", "2.1", isEqual) != IGSIO_SUCCESS || !isEqual)
    {
      return IGSIO_FAIL;
    }
    double version(0.0);
    if (igsioCommon::XML::SafeGetAttributeValueInsensitive<double>(*rootElement, "version", version) != IGSIO_SUCCESS || version != 2.1)
    {
      return IGSIO_FAIL;
    }

    return IGSIO_SUCCESS;
  }

  igsioStatus TestValidTransformName(std::string from, std::string to)
  {
    igsioTransformName transformName;
    std::string strName = std::string(from) + std::string("To") + std::string(to);
    if (transformName.SetTransformName(strName.c_str()) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to create transform name from " << strName);
      return IGSIO_FAIL;
    }

    if (STRCASECMP(transformName.From().c_str(), from.c_str()) != 0)
    {
      LOG_ERROR("Expected From coordinate frame name '" << from << "' differ from matched coordinate frame name '" << transformName.From() << "'.");
      return IGSIO_FAIL;
    }

    if (STRCASECMP(transformName.To().c_str(), to.c_str()) != 0)
    {
      LOG_ERROR("Expected To coordinate frame name '" << to << "' differ from matched coordinate frame name '" << transformName.To() << "'.");
      return IGSIO_FAIL;
    }

    std::string outTransformName;
    if (transformName.GetTransformName(outTransformName) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Failed to get transform name from igsioTransformName!");
      return IGSIO_FAIL;
    }

    if (STRCASECMP(strName.c_str(), outTransformName.c_str()) != 0)
    {
      LOG_ERROR("Expected transform name '" << strName << "' differ from generated transform name '" << outTransformName << "'.");
      return IGSIO_FAIL;
    }

    LOG_INFO("Input: " << strName << "  <From> coordinate frame: " << transformName.From() << "  <To> coordinate frame: " << transformName.To() << "  Output: " << outTransformName);

    return IGSIO_SUCCESS;
  }

  igsioStatus TestInvalidTransformName(std::string from, std::string to)
  {
    igsioTransformName transformName;
    std::string strName = std::string(from) + std::string("To") + std::string(to);
    transformName.SetTransformName(strName.c_str());

    if (!transformName.IsValid())
    {
      LOG_INFO("Invalid transform input found: " << strName);
    }
    else
    {
      LOG_ERROR("Invalid transform name expected!");
      return IGSIO_FAIL;
    }

    return IGSIO_SUCCESS;
  }
}

int main(int argc, char** argv)
{
  bool printHelp(false);
  int verboseLevel = vtkIGSIOLogger::LOG_LEVEL_UNDEFINED;

  vtksys::CommandLineArguments args;
  args.Initialize(argc, argv);

  args.AddArgument("--help", vtksys::CommandLineArguments::NO_ARGUMENT, &printHelp, "Print this help.");
  args.AddArgument("--verbose", vtksys::CommandLineArguments::EQUAL_ARGUMENT, &verboseLevel, "Verbose level (1=error only, 2=warning, 3=info, 4=debug, 5=trace)");

  if (!args.Parse())
  {
    std::cerr << "Problem parsing arguments" << std::endl;
    std::cout << "Help: " << args.GetHelp() << std::endl;
    exit(EXIT_FAILURE);
  }

  if (printHelp)
  {
    std::cout << args.GetHelp() << std::endl;
    exit(EXIT_SUCCESS);

  }

  vtkIGSIOLogger::Instance()->SetLogLevel(verboseLevel);

  // ***********************************************
  // Test string to number conversion
  // ***********************************************
  double doubleResult(0);
  if (igsioCommon::StringToDouble("8.12", doubleResult) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to convert '8.12' string to double");
    return EXIT_FAILURE;
  }
  if (fabs(doubleResult - 8.12) > DOUBLE_THRESHOLD)
  {
    LOG_ERROR("Failed to convert '8.12' string to double - difference is larger then threshold: " << std::fixed << DOUBLE_THRESHOLD);
    return EXIT_FAILURE;
  }

  if (igsioCommon::StringToDouble("", doubleResult) != IGSIO_FAIL)
  {
    LOG_ERROR("Error expected on empty string to double conversion!");
    return EXIT_FAILURE;
  }

  int intResult(0);
  if (igsioCommon::StringToInt("8", intResult) != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to convert '8' string to integer!");
    return EXIT_FAILURE;
  }
  if (intResult != 8)
  {
    LOG_ERROR("Failed to convert '8' string to integer - difference is: " << intResult - 8);
    return EXIT_FAILURE;
  }

  if (igsioCommon::StringToInt("", intResult) != IGSIO_FAIL)
  {
    LOG_ERROR("Error expected on empty string to integer conversion!");
    return EXIT_FAILURE;
  }

  // ***********************************************
  // Test igsioTransformName
  // ***********************************************

  igsioTransformName trName("tr1", "tr2");
  if (trName.From().compare("Tr1") != 0)
  {
    LOG_ERROR("Capitalization test failed: " << trName.From() << " != Tr1");
    exit(EXIT_FAILURE);
  }
  if (trName.To().compare("Tr2") != 0)
  {
    LOG_ERROR("Capitalization test failed: " << trName.To() << " != Tr2");
    exit(EXIT_FAILURE);
  }

  if (TestValidTransformName("Image", "Probe") != IGSIO_SUCCESS)
  {
    exit(EXIT_FAILURE);
  }
  if (TestValidTransformName("Tool", "Tool") != IGSIO_SUCCESS)
  {
    exit(EXIT_FAILURE);
  }
  if (TestValidTransformName("TestTool", "OtherTool") != IGSIO_SUCCESS)
  {
    exit(EXIT_FAILURE);
  }


  if (TestInvalidTransformName("To", "To") != IGSIO_SUCCESS)
  {
    exit(EXIT_FAILURE);
  }
  if (TestInvalidTransformName("ToTol", "TolTo") != IGSIO_SUCCESS)
  {
    exit(EXIT_FAILURE);
  }
  if (TestInvalidTransformName("TolTo", "ToTol") != IGSIO_SUCCESS)
  {
    exit(EXIT_FAILURE);
  }
  if (TestInvalidTransformName("to", "to") != IGSIO_SUCCESS)
  {
    exit(EXIT_FAILURE);
  }

  LOG_INFO("Test recursive critical section");
  vtkIGSIORecursiveCriticalSection* critSec = vtkIGSIORecursiveCriticalSection::New();
  LOG_INFO(" Lock");
  critSec->Lock();
  LOG_INFO(" Lock again");
  critSec->Lock();
  LOG_INFO(" Unlock");
  critSec->Unlock();
  LOG_INFO(" Unlock again");
  critSec->Unlock();
  LOG_INFO(" Delete");
  critSec->Delete();
  LOG_INFO(" Done");

  if (TestStringFunctions() != IGSIO_SUCCESS)
  {
    exit(EXIT_FAILURE);
  }

  if (TestXMLFunctions() != IGSIO_SUCCESS)
  {
    exit(EXIT_FAILURE);
  }

  LOG_INFO("Test finished successfully!");
  return EXIT_SUCCESS;
}


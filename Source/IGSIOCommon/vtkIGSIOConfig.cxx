/*=IGSIO=header=begin======================================================
  Program: IGSIO
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================IGSIO=header=end*/

#include "igsioConfigure.h"
#include "vtkIGSIOConfig.h"
#include "vtkDirectory.h"
#include "vtkMatrix4x4.h"
#include "vtkIGSIORecursiveCriticalSection.h"
#include "vtkXMLUtilities.h"
#include "vtksys/SystemTools.hxx"
#include "vtkIGSIOAccurateTimer.h"

#include <vtkSmartPointer.h>

// Needed for proper singleton initialization
// The vtkDebugLeaks singleton must be initialized before and
// destroyed after the vtkIGSIOConfig singleton.
#include "vtkDebugLeaksManager.h"
#include "vtkObjectFactory.h"

#if _WIN32
#include <stdlib.h>
#include <shlobj.h>
#elif defined(unix) || defined(__unix__) || defined(__unix)
#include "unistd.h"
#elif defined(__APPLE__)
#include <unistd.h>
#include <mach-o/dyld.h>
#endif

static const char APPLICATION_CONFIGURATION_FILE_NAME[] = "IGSIOConfig.xml";


vtkIGSIOConfig* vtkIGSIOConfig::Instance = NULL;

//-----------------------------------------------------------------------------
class vtkIGSIOConfigCleanup
{
public:
  inline void Use()
  {
  }

  ~vtkIGSIOConfigCleanup()
  {
    if (vtkIGSIOConfig::GetInstance())
    {
      vtkIGSIOConfig::SetInstance(NULL);
    }
  }
};
//------ File local content ---------------------------------------------------
namespace
{
  // Ensure creation order to prevent crashing during ~vtkIGSIOConfigCleanup()
  // ConfigCreationCriticalSection must be destroyed AFTER vtkIGSIOConfigCleanupGlobal
  struct StaticVariables
  {
    vtkIGSIOSimpleRecursiveCriticalSection ConfigCreationCriticalSection;
    vtkIGSIOConfigCleanup vtkIGSIOConfigCleanupGlobal;
  };
  StaticVariables staticVariables;
}

//-----------------------------------------------------------------------------
vtkIGSIOConfig* vtkIGSIOConfig::New()
{
  return vtkIGSIOConfig::GetInstance();
}

//-----------------------------------------------------------------------------
vtkIGSIOConfig* vtkIGSIOConfig::GetInstance()
{
  if (!vtkIGSIOConfig::Instance)
  {
    igsioLockGuard<vtkIGSIOSimpleRecursiveCriticalSection> criticalSectionGuardedLock(&(staticVariables.ConfigCreationCriticalSection));

    if (vtkIGSIOConfig::Instance != NULL)
    {
      return vtkIGSIOConfig::Instance;
    }

    staticVariables.vtkIGSIOConfigCleanupGlobal.Use();

    // Need to call vtkObjectFactory::CreateInstance method because this
    // registers the class in the vtkDebugLeaks::MemoryTable.
    // Without this we would get a "Deleting unknown object" VTK warning on application exit
    // (in debug mode, with debug leak checking enabled).
    vtkObject* ret = vtkObjectFactory::CreateInstance("vtkIGSIOConfig");
    if (ret)
    {
      vtkIGSIOConfig::Instance = static_cast<vtkIGSIOConfig*>(ret);
    }
    else
    {
      vtkIGSIOConfig::Instance = new vtkIGSIOConfig();
    }
  }

  // return the instance
  return vtkIGSIOConfig::Instance;
}

//-----------------------------------------------------------------------------
void vtkIGSIOConfig::SetInstance(vtkIGSIOConfig* instance)
{
  if (vtkIGSIOConfig::Instance == instance)
  {
    return;
  }
  // preferably this will be NULL
  if (vtkIGSIOConfig::Instance)
  {
    vtkIGSIOConfig::Instance->Delete();
  }
  vtkIGSIOConfig::Instance = instance;

  // user will call ->Delete() after setting instance
  if (instance != NULL)
  {
    instance->Register(NULL);
  }
}

//-----------------------------------------------------------------------------
vtkIGSIOConfig::vtkIGSIOConfig()
  : DeviceSetConfigurationData(NULL)
  , ApplicationConfigurationData(NULL)
{
  // vtkIGSIOAccurateTimer will instantiate the logger singleton to a vtkIGSIOLogger
  // Need to instantiate the singleton as a vtkIGSIOLogger
  vtkIGSIOLogger::Instance();

  this->ApplicationStartTimestamp = vtkIGSIOAccurateTimer::GetInstance()->GetDateAndTimeString();

  // Retrieve the program directory (where the exe file is located)
  SetProgramDirectory();

  // Load settings from IGSIOConfig.xml
  LoadApplicationConfiguration();
}

//-----------------------------------------------------------------------------
vtkIGSIOConfig::~vtkIGSIOConfig()
{
  this->SetDeviceSetConfigurationData(NULL);
  this->SetApplicationConfigurationData(NULL);
}

//-----------------------------------------------------------------------------
void vtkIGSIOConfig::SetProgramDirectory()
{
#ifdef _WIN32
  char cProgramPath[2048] = {'\0'};
  GetModuleFileName(NULL, cProgramPath, 2048);
  this->ProgramDirectory = vtksys::SystemTools::GetProgramPath(cProgramPath);
#elif defined(__linux__)
  const unsigned int cProgramPathSize = 2048;
  char cProgramPath[cProgramPathSize] = {'\0'};

  // linux
  if (readlink("/proc/self/exe", cProgramPath, cProgramPathSize) != -1)
  {
    // linux
    std::string path = vtksys::SystemTools::CollapseFullPath(cProgramPath);
    std::string dirName;
    std::string fileName;
    vtksys::SystemTools::SplitProgramPath(path.c_str(), dirName, fileName);
    this->ProgramDirectory = dirName;
  }
  else
  {
    // non-linux systems
    // currently simply the working directory is used instead of the executable path
    // see http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
    std::string path = vtksys::SystemTools::CollapseFullPath("./");
    LOG_WARNING("Cannot get program path. IGSIOConfig.xml will be read from  " << path);
    this->ProgramDirectory = path;
  }
#elif defined(__APPLE__)
  char cProgramPath[2048];
  uint32_t size = sizeof(cProgramPath);
  if (_NSGetExecutablePath(cProgramPath, &size) == 0)
  {
    std::string path = vtksys::SystemTools::CollapseFullPath(cProgramPath);
    std::string dirName;
    std::string fileName;
    vtksys::SystemTools::SplitProgramPath(path.c_str(), dirName, fileName);
    this->ProgramDirectory = dirName;
  }
  else
  {
    std::string path = vtksys::SystemTools::CollapseFullPath("./");
    LOG_WARNING("Cannot get program path. IGSIOConfig.xml will be read from  " << path);
    this->ProgramDirectory = path;
  }
#endif
}

//-----------------------------------------------------------------------------
void vtkIGSIOConfig::SetOutputDirectory(const std::string& aDir)
{
  this->OutputDirectory = aDir;

  // Set log file name and path to the output directory
  std::string logfilename = std::string(this->ApplicationStartTimestamp) + "_IGSIOLog.txt";
  vtkIGSIOLogger::Instance()->SetLogFileName(GetOutputPath(logfilename).c_str());
}

//-----------------------------------------------------------------------------
void vtkIGSIOConfig::SetImageDirectory(const std::string& aDir)
{
  this->ImageDirectory = aDir;
}

//-----------------------------------------------------------------------------
void vtkIGSIOConfig::SetDeviceSetConfigurationDirectory(const std::string& aDir)
{
  this->DeviceSetConfigurationDirectory = aDir;
}

//-----------------------------------------------------------------------------
void vtkIGSIOConfig::SetDeviceSetConfigurationFileName(const std::string& aFilePath)
{
  this->DeviceSetConfigurationFileName = aFilePath;
}

//----------------------------------------------------------------------------
vtkXMLDataElement* vtkIGSIOConfig::CreateDeviceSetConfigurationFromFile(const std::string& aConfigFile)
{
  // Read main configuration file
  std::string configFilePath = aConfigFile;
  if (!vtksys::SystemTools::FileExists(configFilePath, true))
  {
    configFilePath = vtkIGSIOConfig::GetInstance()->GetDeviceSetConfigurationPath(aConfigFile);
    if (!vtksys::SystemTools::FileExists(configFilePath, true))
    {
      LOG_ERROR("Reading device set configuration file failed: " << aConfigFile << " does not exist in the current directory or in " << vtkIGSIOConfig::GetInstance()->GetDeviceSetConfigurationDirectory());
      return NULL;
    }
  }
  vtkXMLDataElement* configRootElement = vtkXMLUtilities::ReadElementFromFile(configFilePath.c_str());
  if (configRootElement == NULL)
  {
    LOG_ERROR("Reading device set configuration file failed: syntax error in " << aConfigFile);
    return NULL;
  }

  // Print configuration file contents for debugging purposes
  LOG_DEBUG("Device set configuration is read from file: " << aConfigFile);
  std::ostringstream xmlFileContents;
  igsioCommon::XML::PrintXML(xmlFileContents, vtkIndent(1), configRootElement);
  LOG_DEBUG("Device set configuration file contents: " << std::endl << xmlFileContents.str());

  return configRootElement;
}

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOConfig::LoadApplicationConfiguration()
{
  LOG_TRACE("vtkIGSIOConfig::LoadApplicationConfiguration");

  if (this->ProgramDirectory.empty())
  {
    LOG_ERROR("Unable to read configuration - program directory has to be set first!");
    return IGSIO_FAIL;
  }
  std::string applicationConfigurationFilePath = vtksys::SystemTools::CollapseFullPath(APPLICATION_CONFIGURATION_FILE_NAME, this->ProgramDirectory.c_str());

  bool saveNeeded = false;

  // Read configuration
  vtkSmartPointer<vtkXMLDataElement> applicationConfigurationRoot;
  if (vtksys::SystemTools::FileExists(applicationConfigurationFilePath.c_str(), true))
  {
    applicationConfigurationRoot.TakeReference(vtkXMLUtilities::ReadElementFromFile(applicationConfigurationFilePath.c_str()));
  }
  if (applicationConfigurationRoot == NULL)
  {
    LOG_DEBUG("Application configuration file is not found at '" << applicationConfigurationFilePath << "' - file will be created with default values");
    applicationConfigurationRoot = vtkSmartPointer<vtkXMLDataElement>::New();
    applicationConfigurationRoot->SetName("IGSIOConfig");
    saveNeeded = true;
  }

  this->SetApplicationConfigurationData(applicationConfigurationRoot);

  // Verify root element name
  if (!igsioCommon::IsEqualInsensitive(applicationConfigurationRoot->GetName(), "IGSIOConfig"))
  {
    LOG_ERROR("Invalid application configuration file (root XML element of the file '" << applicationConfigurationFilePath << "' should be 'IGSIOConfig' instead of '" << applicationConfigurationRoot->GetName() << "')");
    return IGSIO_FAIL;
  }

  // Read log level
  int logLevel = 0;
  if (applicationConfigurationRoot->GetScalarAttribute("LogLevel", logLevel))
  {
    vtkIGSIOLogger::Instance()->SetLogLevel(logLevel);
  }
  else
  {
    LOG_INFO("LogLevel attribute is not found - default 'Info' log level will be used");
    vtkIGSIOLogger::Instance()->SetLogLevel(vtkIGSIOLogger::LOG_LEVEL_INFO);
    saveNeeded = true;
  }

  // Read last device set config file
  const char* lastDeviceSetConfigFile = applicationConfigurationRoot->GetAttribute("LastDeviceSetConfigurationFileName");
  if ((lastDeviceSetConfigFile != NULL) && (STRCASECMP(lastDeviceSetConfigFile, "") != 0))
  {
    this->SetDeviceSetConfigurationFileName(lastDeviceSetConfigFile);
  }
  else
  {
    LOG_DEBUG("Cannot read last used device set config file until you connect to one first");
  }

  // Read device set configuration directory
  const char* deviceSetDirectory = applicationConfigurationRoot->GetAttribute("DeviceSetConfigurationDirectory");
  if ((deviceSetDirectory != NULL) && (STRCASECMP(deviceSetDirectory, "") != 0))
  {
    this->SetDeviceSetConfigurationDirectory(deviceSetDirectory);
  }
  else
  {
    LOG_INFO("Device set configuration directory is not set - default '../config' will be used");
    this->SetDeviceSetConfigurationDirectory("../config");
    saveNeeded = true;
  }
  // Make device set configuration directory
  if (! vtksys::SystemTools::MakeDirectory(GetDeviceSetConfigurationDirectory().c_str()))
  {
    LOG_ERROR("Unable to create device set configuration directory '" << GetDeviceSetConfigurationDirectory() << "'");
    return IGSIO_FAIL;
  }

  // Read editor application
  const char* editorApplicationExecutable = applicationConfigurationRoot->GetAttribute("EditorApplicationExecutable");
  if ((editorApplicationExecutable != NULL) && (STRCASECMP(editorApplicationExecutable, "") != 0))
  {
    this->SetEditorApplicationExecutable(editorApplicationExecutable);
  }
  else
  {
    LOG_DEBUG("Editor application executable is not set - system default will be used");
#if _WIN32
    // Try to find Notepad++ in Program Files, if not found then use notepad.
    char pf[MAX_PATH];
    SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, 0, pf);
    std::string fullPath = std::string(pf) + "\\Notepad++\\notepad++.exe";
    std::string application = "notepad.exe";
    if (vtksys::SystemTools::FileExists(fullPath.c_str()))
    {
      application = fullPath;
    }
    this->SetEditorApplicationExecutable(application.c_str());
#elif defined(unix) || defined(__unix__) || defined(__unix)
    this->SetEditorApplicationExecutable("gedit");
#elif __APPLE__
    this->SetEditorApplicationExecutable("TextEdit");
#endif
    saveNeeded = true;
  }

  // Read output directory
  const char* outputDirectory = applicationConfigurationRoot->GetAttribute("OutputDirectory");
  if ((outputDirectory != NULL) && (STRCASECMP(outputDirectory, "") != 0))
  {
    this->SetOutputDirectory(outputDirectory);
  }
  else
  {
    LOG_DEBUG("Output directory is not set - default './Output' will be used");
    this->SetOutputDirectory("./Output");
    saveNeeded = true;
  }

  // Make device set configuration directory
  if (! vtksys::SystemTools::MakeDirectory(GetOutputDirectory().c_str()))
  {
    LOG_ERROR("Unable to create device set configuration directory '" << GetOutputDirectory() << "'");
    return IGSIO_FAIL;
  }

  // Read image directory
  const char* imageDirectory = applicationConfigurationRoot->GetAttribute("ImageDirectory");
  if ((imageDirectory != NULL) && (STRCASECMP(imageDirectory, "") != 0))
  {
    this->SetImageDirectory(imageDirectory);
  }
  else
  {
    LOG_INFO("Image directory is not set - default '../data' will be used");
    this->SetImageDirectory("../data");
    saveNeeded = true;
  }

  // Read model directory
  const char* modelDirectory = applicationConfigurationRoot->GetAttribute("ModelDirectory");
  if ((modelDirectory != NULL) && (STRCASECMP(modelDirectory, "") != 0))
  {
    this->ModelDirectory = modelDirectory;
  }
  else
  {
    LOG_INFO("Model directory is not set - default '../config' will be used");
    this->ModelDirectory = "../config";
    saveNeeded = true;
  }

  // Read scripts directory
  const char* scriptsDirectory = applicationConfigurationRoot->GetAttribute("ScriptsDirectory");
  if ((scriptsDirectory != NULL) && (STRCASECMP(scriptsDirectory, "") != 0))
  {
    this->ScriptsDirectory = scriptsDirectory;
  }
  else
  {
    LOG_INFO("Scripts directory is not set - default '../scripts' will be used");
    this->ScriptsDirectory = "../scripts";
    saveNeeded = true;
  }

  if (saveNeeded)
  {
    return SaveApplicationConfigurationToFile();
  }

  return IGSIO_SUCCESS;
}

//------------------------------------------------------------------------------
igsioStatus vtkIGSIOConfig::WriteApplicationConfiguration()
{
  LOG_TRACE("vtkIGSIOConfig::WriteApplicationConfiguration");

  vtkXMLDataElement* applicationConfigurationRoot = this->ApplicationConfigurationData;
  if (applicationConfigurationRoot == NULL)
  {
    // Configuration root does not exist yet, create it now
    vtkSmartPointer<vtkXMLDataElement> newApplicationConfigurationRoot = vtkSmartPointer<vtkXMLDataElement>::New();
    newApplicationConfigurationRoot->SetName("IGSIOConfig");
    this->SetApplicationConfigurationData(newApplicationConfigurationRoot);
    applicationConfigurationRoot = newApplicationConfigurationRoot;
  }

  // Verify root element name
  if (STRCASECMP(applicationConfigurationRoot->GetName(), "IGSIOConfig") != 0)
  {
    LOG_ERROR("Invalid application configuration file (root XML element should be 'IGSIOConfig' found '" << applicationConfigurationRoot->GetName() << "' instead)");
    return IGSIO_FAIL;
  }

  // Save date
  applicationConfigurationRoot->SetAttribute("Date", vtksys::SystemTools::GetCurrentDateTime("%Y.%m.%d %X").c_str());

  // Save log level
  applicationConfigurationRoot->SetIntAttribute("LogLevel", vtkIGSIOLogger::Instance()->GetLogLevel());

  // Save device set directory
  applicationConfigurationRoot->SetAttribute("DeviceSetConfigurationDirectory", this->DeviceSetConfigurationDirectory.c_str());

  // Save last device set config file
  applicationConfigurationRoot->SetAttribute("LastDeviceSetConfigurationFileName", this->DeviceSetConfigurationFileName.c_str());

  // Save editor application
  applicationConfigurationRoot->SetAttribute("EditorApplicationExecutable", this->EditorApplicationExecutable.c_str());

  // Save output path
  applicationConfigurationRoot->SetAttribute("OutputDirectory", this->OutputDirectory.c_str());

  // Save image directory path
  applicationConfigurationRoot->SetAttribute("ImageDirectory", this->ImageDirectory.c_str());

  // Save model directory path
  applicationConfigurationRoot->SetAttribute("ModelDirectory", this->ModelDirectory.c_str());

  // Save scripts directory path
  applicationConfigurationRoot->SetAttribute("ScriptsDirectory", this->ScriptsDirectory.c_str());

  return IGSIO_SUCCESS;
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetNewDeviceSetConfigurationFileName()
{
  LOG_TRACE("vtkIGSIOConfig::GetNewDeviceSetConfigurationFileName");

  std::string resultFileName;
  if (this->DeviceSetConfigurationFileName.empty())
  {
    LOG_WARNING("New configuration file name cannot be assembled due to absence of input configuration file name");
    resultFileName = "IGSIOConfiguration";
  }
  else
  {
    resultFileName = this->DeviceSetConfigurationFileName;
    resultFileName = resultFileName.substr(0, resultFileName.find(".xml"));
    resultFileName = resultFileName.substr(resultFileName.find_last_of("/\\") + 1);
  }

  // Detect if date is already in the filename and remove it if it is there
  if (resultFileName.length() > 16)
  {
    std::string possibleDate = resultFileName.substr(resultFileName.length() - 15, 15);
    std::string possibleDay = possibleDate.substr(0, 8);
    std::string possibleTime = possibleDate.substr(9, 6);
    if (atoi(possibleDay.c_str()) && atoi(possibleTime.c_str()))
    {
      resultFileName = resultFileName.substr(0, resultFileName.length() - 16);
    }
  }

  // Construct new file name with date and time
  resultFileName.append("_");
  resultFileName.append(vtksys::SystemTools::GetCurrentDateTime("%Y%m%d_%H%M%S"));
  resultFileName.append(".xml");

  return resultFileName;
}

//------------------------------------------------------------------------------
igsioStatus vtkIGSIOConfig::SaveApplicationConfigurationToFile()
{
  LOG_TRACE("vtkIGSIOConfig::SaveApplicationConfigurationToFile");

  if (WriteApplicationConfiguration() != IGSIO_SUCCESS)
  {
    LOG_ERROR("Failed to save application configuration to XML data!");
    return IGSIO_FAIL;
  }

  std::string applicationConfigurationFilePath = vtksys::SystemTools::CollapseFullPath(APPLICATION_CONFIGURATION_FILE_NAME, this->ProgramDirectory.c_str());
  igsioCommon::XML::PrintXML(applicationConfigurationFilePath.c_str(), this->ApplicationConfigurationData);

  LOG_DEBUG("Application configuration file '" << applicationConfigurationFilePath << "' saved");

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetApplicationConfigurationFilePath() const
{
  return vtksys::SystemTools::CollapseFullPath(APPLICATION_CONFIGURATION_FILE_NAME, this->ProgramDirectory.c_str());
}

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOConfig::WriteTransformToCoordinateDefinition(const char* aFromCoordinateFrame, const char* aToCoordinateFrame, vtkMatrix4x4* aMatrix, double aError/*=-1*/, const char* aDate/*=NULL*/)
{
  LOG_TRACE("vtkIGSIOConfig::WriteTransformToCoordinateDefinition(" << aFromCoordinateFrame << ", " << aToCoordinateFrame << ")");

  if (aFromCoordinateFrame == NULL)
  {
    LOG_ERROR("Failed to write transform to CoordinateDefinitions - 'From' coordinate frame name is NULL");
    return IGSIO_FAIL;
  }

  if (aToCoordinateFrame == NULL)
  {
    LOG_ERROR("Failed to write transform to CoordinateDefinitions - 'To' coordinate frame name is NULL");
    return IGSIO_FAIL;
  }

  if (aMatrix == NULL)
  {
    LOG_ERROR("Failed to write transform to CoordinateDefinitions - matrix is NULL");
    return IGSIO_FAIL;
  }


  vtkXMLDataElement* deviceSetConfigRootElement = GetDeviceSetConfigurationData();
  if (deviceSetConfigRootElement == NULL)
  {
    LOG_ERROR("Failed to write transform to CoordinateDefinitions - config root element is NULL");
    return IGSIO_FAIL;
  }

  vtkXMLDataElement* coordinateDefinitions = deviceSetConfigRootElement->FindNestedElementWithName("CoordinateDefinitions");
  if (coordinateDefinitions == NULL)
  {
    vtkSmartPointer<vtkXMLDataElement> newCoordinateDefinitions = vtkSmartPointer<vtkXMLDataElement>::New();
    newCoordinateDefinitions->SetName("CoordinateDefinitions");
    deviceSetConfigRootElement->AddNestedElement(newCoordinateDefinitions);
    coordinateDefinitions = newCoordinateDefinitions;
  }

  // Check if we already have this entry in the config file
  vtkXMLDataElement* transformElement = NULL;
  int nestedElementIndex(0);
  while (nestedElementIndex < coordinateDefinitions->GetNumberOfNestedElements() && transformElement == NULL)
  {
    vtkXMLDataElement* nestedElement = coordinateDefinitions->GetNestedElement(nestedElementIndex);
    const char* fromAttribute = nestedElement->GetAttribute("From");
    const char* toAttribute = nestedElement->GetAttribute("To");

    if (fromAttribute && toAttribute
        && STRCASECMP(fromAttribute, aFromCoordinateFrame) == 0
        && STRCASECMP(toAttribute, aToCoordinateFrame) == 0)
    {
      transformElement = nestedElement;
    }
  }

  if (transformElement == NULL)
  {
    vtkSmartPointer<vtkXMLDataElement> newElement = vtkSmartPointer<vtkXMLDataElement>::New();
    newElement->SetName("Transform");
    newElement->SetAttribute("From", aFromCoordinateFrame);
    newElement->SetAttribute("To", aToCoordinateFrame);
    coordinateDefinitions->AddNestedElement(newElement);
    transformElement = newElement;
  }

  double vectorMatrix[16] = {0};
  vtkMatrix4x4::DeepCopy(vectorMatrix, aMatrix);
  transformElement->SetVectorAttribute("Matrix", 16, vectorMatrix);

  if (aError > 0)
  {
    transformElement->SetDoubleAttribute("Error", aError);
  }

  if (aDate != NULL)
  {
    transformElement->SetAttribute("Date", aDate);
  }
  else // Add current date if it was not explicitly specified
  {
    transformElement->SetAttribute("Date", vtksys::SystemTools::GetCurrentDateTime("%Y.%m.%d %X").c_str());
  }

  return IGSIO_SUCCESS;
}

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOConfig::ReadTransformToCoordinateDefinition(const char* aFromCoordinateFrame, const char* aToCoordinateFrame, vtkMatrix4x4* aMatrix, double* aError/*=NULL*/, std::string* aDate/*=NULL*/)
{
  LOG_TRACE("vtkIGSIOConfig::ReadTransformToCoordinateDefinition(" << aFromCoordinateFrame << ", " << aToCoordinateFrame << ")");

  vtkXMLDataElement* configRootElement = GetDeviceSetConfigurationData();
  return ReadTransformToCoordinateDefinition(configRootElement, aFromCoordinateFrame, aToCoordinateFrame, aMatrix, aError, aDate);
}

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOConfig::ReadTransformToCoordinateDefinition(vtkXMLDataElement* aDeviceSetConfigRootElement, const char* aFromCoordinateFrame, const char* aToCoordinateFrame, vtkMatrix4x4* aMatrix, double* aError/*=NULL*/, std::string* aDate/*=NULL*/)
{
  LOG_TRACE("vtkIGSIOConfig::ReadTransformToCoordinateDefinition(" << aFromCoordinateFrame << ", " << aToCoordinateFrame << ")");

  if (aDeviceSetConfigRootElement == NULL)
  {
    LOG_ERROR("Failed read transform from CoordinateDefinitions - config root element is NULL");
    return IGSIO_FAIL;
  }

  if (aFromCoordinateFrame == NULL)
  {
    LOG_ERROR("Failed to read transform from CoordinateDefinitions - 'From' coordinate frame name is NULL");
    return IGSIO_FAIL;
  }

  if (aToCoordinateFrame == NULL)
  {
    LOG_ERROR("Failed to read transform from CoordinateDefinitions - 'To' coordinate frame name is NULL");
    return IGSIO_FAIL;
  }

  if (aMatrix == NULL)
  {
    LOG_ERROR("Failed to read transform from CoordinateDefinitions - matrix is NULL");
    return IGSIO_FAIL;
  }

  vtkXMLDataElement* coordinateDefinitions = aDeviceSetConfigRootElement->FindNestedElementWithName("CoordinateDefinitions");
  if (coordinateDefinitions == NULL)
  {
    LOG_ERROR("Failed read transform from CoordinateDefinitions - CoordinateDefinitions element not found");
    return IGSIO_FAIL;
  }

  for (int nestedElementIndex = 0; nestedElementIndex < coordinateDefinitions->GetNumberOfNestedElements(); ++nestedElementIndex)
  {
    vtkXMLDataElement* nestedElement = coordinateDefinitions->GetNestedElement(nestedElementIndex);
    if (STRCASECMP(nestedElement->GetName(), "Transform") != 0)
    {
      // Not a transform element, skip it
      continue;
    }

    const char* fromAttribute = nestedElement->GetAttribute("From");
    const char* toAttribute = nestedElement->GetAttribute("To");

    if (fromAttribute && toAttribute
        && STRCASECMP(fromAttribute, aFromCoordinateFrame) == 0
        && STRCASECMP(toAttribute, aToCoordinateFrame) == 0)
    {
      double vectorMatrix[16] = {0};
      if (nestedElement->GetVectorAttribute("Matrix", 16, vectorMatrix))
      {
        aMatrix->DeepCopy(vectorMatrix);
      }
      else
      {
        LOG_ERROR("Unable to find 'Matrix' attribute of '" << aFromCoordinateFrame << "' to '" << aToCoordinateFrame << "' transform among the CoordinateDefinitions in the configuration file");
        return IGSIO_FAIL;
      }

      if (aError != NULL)
      {
        double error(0);
        if (nestedElement->GetScalarAttribute("Error", error))
        {
          *aError = error;
        }
        else
        {
          LOG_WARNING("Unable to find 'Error' attribute of '" << aFromCoordinateFrame << "' to '" << aToCoordinateFrame << "' transform among the CoordinateDefinitions in the configuration file");
        }
      }

      if (aDate != NULL)
      {
        const char* date =  nestedElement->GetAttribute("Date");
        if (date != NULL)
        {
          *aDate = date;
        }
        else
        {
          LOG_WARNING("Unable to find 'Date' attribute of '" << aFromCoordinateFrame << "' to '" << aToCoordinateFrame << "' transform among the CoordinateDefinitions in the configuration file");
        }
      }

      return IGSIO_SUCCESS;
    }
  }

  LOG_DEBUG("Unable to find from '" << aFromCoordinateFrame << "' to '" << aToCoordinateFrame << "' transform among the CoordinateDefinitions in the configuration file");

  return IGSIO_FAIL;
}

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOConfig::ReplaceElementInDeviceSetConfiguration(const char* aElementName, vtkXMLDataElement* aNewRootElement)
{
  LOG_TRACE("vtkIGSIOConfig::ReplaceElementInDeviceSetConfiguration(" << aElementName << ")");

  vtkXMLDataElement* deviceSetConfigRootElement = GetDeviceSetConfigurationData();
  vtkXMLDataElement* orginalElement = deviceSetConfigRootElement->FindNestedElementWithName(aElementName);
  if (orginalElement != NULL)
  {
    deviceSetConfigRootElement->RemoveNestedElement(orginalElement);
  }

  vtkXMLDataElement* newElement = aNewRootElement->FindNestedElementWithName(aElementName);
  if (newElement != NULL)
  {
    deviceSetConfigRootElement->AddNestedElement(newElement);
  }
  else
  {
    // Re-add deleted element if there was one
    if (orginalElement != NULL)
    {
      deviceSetConfigRootElement->AddNestedElement(orginalElement);
    }
    LOG_ERROR("No element with the name '" << aElementName << "' can be found in the given XML data element!");
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//-----------------------------------------------------------------------------
vtkXMLDataElement* vtkIGSIOConfig::LookupElementWithNameContainingChildWithNameAndAttribute(vtkXMLDataElement* aConfig, const char* aElementName, const char* aChildName, const char* aChildAttributeName, const char* aChildAttributeValue)
{
  LOG_TRACE("vtkIGSIOConfig::LookupElementWithNameContainingChildWithNameAndAttribute(" << aElementName << ", " << aChildName << ", " << (aChildAttributeName == NULL ? "" : aChildAttributeName) << ", " << (aChildAttributeValue == NULL ? "" : aChildAttributeValue) << ")");

  if (aConfig == NULL)
  {
    LOG_ERROR("No input XML data element is specified!");
    return NULL;
  }

  vtkXMLDataElement* firstElement = aConfig->LookupElementWithName(aElementName);
  if (firstElement == NULL)
  {
    return NULL;
  }
  else
  {
    if (aChildAttributeName && aChildAttributeValue)
    {
      return firstElement->FindNestedElementWithNameAndAttribute(aChildName, aChildAttributeName, aChildAttributeValue);
    }
    else
    {
      return firstElement->FindNestedElementWithName(aChildName);
    }
  }
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetFirstFileFoundInConfigurationDirectory(const char* aFileName)
{
  LOG_TRACE("vtkIGSIOConfig::GetFirstFileFoundInConfigurationDirectory(" << aFileName << ")");

  if (this->DeviceSetConfigurationDirectory.empty())
  {
    std::string configurationDirectory = vtksys::SystemTools::GetFilenamePath(GetDeviceSetConfigurationFileName());
    return vtkIGSIOConfig::GetFirstFileFoundInDirectory(aFileName, configurationDirectory.c_str());
  }
  else
  {
    return GetFirstFileFoundInDirectory(aFileName, this->DeviceSetConfigurationDirectory.c_str());
  }
};

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetFirstFileFoundInDirectory(const char* aFileName, const char* aDirectory)
{
  LOG_TRACE("vtkIGSIOConfig::GetFirstFileFoundInDirectory(" << aFileName << ", " << aDirectory << ")");

  std::string result = FindFileRecursivelyInDirectory(aFileName, aDirectory);
  if (STRCASECMP("", result.c_str()) == 0)
  {
    LOG_WARNING("File " << aFileName << " was not found in directory " << aDirectory);
  }

  return result;
};

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::FindFileRecursivelyInDirectory(const char* aFileName, const char* aDirectory)
{
  LOG_TRACE("vtkIGSIOConfig::FindFileRecursivelyInDirectory(" << aFileName << ", " << aDirectory << ")");

  std::vector<std::string> directoryList;
  directoryList.push_back(aDirectory);

  // Search for the file in the input directory first (no system paths allowed)
  std::string result = vtksys::SystemTools::FindFile(aFileName, directoryList, true);
  if (STRCASECMP("", result.c_str()))
  {
    return result;
  }
  else // If not found then call this function recursively for the subdirectories of the input directory
  {
    vtkSmartPointer<vtkDirectory> dir = vtkSmartPointer<vtkDirectory>::New();
    if (dir->Open(aDirectory))
    {
      for (int i = 0; i < dir->GetNumberOfFiles(); ++i)
      {
        const char* fileOrDirectory = dir->GetFile(i);
        if ((! dir->FileIsDirectory(fileOrDirectory)) || (STRCASECMP(".", fileOrDirectory) == 0) || (STRCASECMP("..", fileOrDirectory) == 0))
        {
          continue;
        }

        std::string fullPath(aDirectory);
        fullPath.append("/");
        fullPath.append(fileOrDirectory);

        result = FindFileRecursivelyInDirectory(aFileName, fullPath.c_str());
        if (STRCASECMP("", result.c_str()))
        {
          return result;
        }
      }
    }
    else
    {
      LOG_DEBUG("Directory " << aDirectory << " cannot be opened");
      return "";
    }
  }

  return "";
};

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOConfig::FindImagePath(const std::string& aImagePath, std::string& aFoundAbsolutePath)
{
  LOG_TRACE("vtkIGSIOConfig::FindImagePath(" << aImagePath << ")");

  // Check file relative to the image directory
  aFoundAbsolutePath = GetImagePath(aImagePath);
  if (vtksys::SystemTools::FileExists(aFoundAbsolutePath.c_str()))
  {
    return IGSIO_SUCCESS;
  }
  LOG_DEBUG("Absolute path not found at: " << aFoundAbsolutePath);

  // Check file relative to the device set configuration file
  std::string configurationFileDirectory = vtksys::SystemTools::GetFilenamePath(GetDeviceSetConfigurationFileName());
  aFoundAbsolutePath = GetAbsolutePath(aImagePath, GetAbsolutePath(configurationFileDirectory, this->ProgramDirectory));
  if (vtksys::SystemTools::FileExists(aFoundAbsolutePath.c_str()))
  {
    return IGSIO_SUCCESS;
  }
  LOG_DEBUG("Absolute path not found at: " << aFoundAbsolutePath);

  // Check file relative to the device set configuration directory
  aFoundAbsolutePath = GetDeviceSetConfigurationPath(aImagePath);
  if (vtksys::SystemTools::FileExists(aFoundAbsolutePath.c_str()))
  {
    return IGSIO_SUCCESS;
  }
  LOG_DEBUG("Absolute path not found at: " << aFoundAbsolutePath);

  // Check file relative to the current working directory
  aFoundAbsolutePath = vtksys::SystemTools::CollapseFullPath(aImagePath.c_str(), vtksys::SystemTools::GetCurrentWorkingDirectory().c_str());
  if (vtksys::SystemTools::FileExists(aFoundAbsolutePath.c_str()))
  {
    return IGSIO_SUCCESS;
  }

  aFoundAbsolutePath = "";
  LOG_ERROR("Image with relative path '" << aImagePath << "' cannot be found neither relative to image directory (" << GetImageDirectory() << ")"
            << " nor relative to device set configuration file directory (" << configurationFileDirectory << ")" << ")"
            << " nor to device set configuration directory (" << GetDeviceSetConfigurationDirectory() << ")"
            << " nor the current working directory directory (" << vtksys::SystemTools::GetCurrentWorkingDirectory() << ")");
  return IGSIO_FAIL;
}

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOConfig::FindModelPath(const std::string& aModelPath, std::string& aFoundAbsolutePath)
{
  LOG_TRACE("vtkIGSIOConfig::FindModelPath(" << aModelPath << ")");

  // Check if the file exists in the specified absolute path
  if (vtksys::SystemTools::FileExists(aModelPath.c_str()))
  {
    // found
    aFoundAbsolutePath = aModelPath;
    return IGSIO_SUCCESS;
  }
  LOG_DEBUG("Absolute path not found at: " << aModelPath);

  // Check file relative to the device set configuration file
  std::string configurationFileDirectory = vtksys::SystemTools::GetFilenamePath(GetDeviceSetConfigurationFileName());
  aFoundAbsolutePath = GetAbsolutePath(aModelPath, GetAbsolutePath(configurationFileDirectory, this->ProgramDirectory));
  if (vtksys::SystemTools::FileExists(aFoundAbsolutePath.c_str()))
  {
    return IGSIO_SUCCESS;
  }
  LOG_DEBUG("Absolute path not found at: " << aFoundAbsolutePath);

  // Check recursively in the model directory
  std::string absoluteModelDirectoryPath = GetModelPath(".");
  aFoundAbsolutePath = FindFileRecursivelyInDirectory(aModelPath.c_str(), absoluteModelDirectoryPath.c_str());
  if (!aFoundAbsolutePath.empty())
  {
    // found
    LOG_DEBUG("Absolute path found at: " << aFoundAbsolutePath);
    return IGSIO_SUCCESS;
  }
  LOG_DEBUG("Absolute path not found in subdirectories: " << absoluteModelDirectoryPath);

  // Check file relative to the device set configuration directory
  aFoundAbsolutePath = GetDeviceSetConfigurationPath(aModelPath);
  if (vtksys::SystemTools::FileExists(aFoundAbsolutePath.c_str()))
  {
    // found
    LOG_DEBUG("Absolute path found at: " << aFoundAbsolutePath);
    return IGSIO_SUCCESS;
  }
  LOG_DEBUG("Absolute path not found at: " << aFoundAbsolutePath);

  // Check file relative to the current working directory
  aFoundAbsolutePath = vtksys::SystemTools::CollapseFullPath(aModelPath.c_str(), vtksys::SystemTools::GetCurrentWorkingDirectory().c_str());
  if (vtksys::SystemTools::FileExists(aFoundAbsolutePath.c_str()))
  {
    return IGSIO_SUCCESS;
  }

  aFoundAbsolutePath = "";
  LOG_ERROR("Model with relative path '" << aModelPath << "' cannot be found neither within the model directory (" << absoluteModelDirectoryPath << ")"
            << " nor relative to device set configuration file directory (" << configurationFileDirectory << ")" << ")"
            << " nor to device set configuration directory (" << GetDeviceSetConfigurationDirectory() << ")"
            << " nor the current working directory directory (" << vtksys::SystemTools::GetCurrentWorkingDirectory() << ")");
  return IGSIO_FAIL;
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetAbsolutePath(const std::string& aPath, const std::string& aBasePath)
{
  if (aPath.empty())
  {
    // empty
    return aBasePath;
  }
  if (vtksys::SystemTools::FileIsFullPath(aPath.c_str()))
  {
    // already absolute
    return aPath;
  }

  // relative to the ProgramDirectory
  std::string absolutePath = vtksys::SystemTools::CollapseFullPath(aPath.c_str(), aBasePath.c_str());
  return absolutePath;
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetModelPath(const std::string& subPath)
{
  return GetAbsolutePath(subPath, GetAbsolutePath(this->ModelDirectory, this->ProgramDirectory));
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetDeviceSetConfigurationPath(const std::string& subPath)
{
  return GetAbsolutePath(subPath, GetAbsolutePath(this->DeviceSetConfigurationDirectory, this->ProgramDirectory));
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetOutputPath(const std::string& subPath)
{
  return GetAbsolutePath(subPath, GetAbsolutePath(this->OutputDirectory, this->ProgramDirectory));
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetOutputDirectory()
{
  return GetOutputPath(".");
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetDeviceSetConfigurationDirectory()
{
  return GetDeviceSetConfigurationPath(".");
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetImageDirectory()
{
  return GetImagePath(".");
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetImagePath(const std::string& subPath)
{
  return GetAbsolutePath(subPath, GetAbsolutePath(this->ImageDirectory, this->ProgramDirectory));
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetScriptPath(const std::string& subPath)
{
  return GetAbsolutePath(subPath, GetAbsolutePath(this->ScriptsDirectory, this->ProgramDirectory));
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetDeviceSetConfigurationFileName()
{
  return GetDeviceSetConfigurationPath(this->DeviceSetConfigurationFileName);
}

//-----------------------------------------------------------------------------
void vtkIGSIOConfig::SetDeviceSetConfigurationData(vtkXMLDataElement* deviceSetConfigurationData)
{
  vtkSetObjectBodyMacro(DeviceSetConfigurationData, vtkXMLDataElement, deviceSetConfigurationData);
  if (this->DeviceSetConfigurationData != NULL)
  {
    std::string IGSIOLibVersion = igsioCommon::GetIGSIOVersionString();
    this->DeviceSetConfigurationData->SetAttribute("IGSIORevision", IGSIOLibVersion.c_str());
  }
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOConfig::GetIGSIOExecutablePath(const std::string& executableName)
{
  std::string processNameWithExtension = executableName;
#ifdef _WIN32
  processNameWithExtension += ".exe";
#endif
  return GetAbsolutePath(processNameWithExtension, this->ProgramDirectory);
}

/*=IGSIO=header=begin======================================================
  Program: IGSIO
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================IGSIO=header=end*/

#ifndef __vtkIGSIOConfig_h
#define __vtkIGSIOConfig_h

#include "igsioConfigure.h"
#include "igsioCommon.h"
#include "vtkigsiocommon_export.h"

#include "vtkObject.h"
#include "vtkXMLDataElement.h"

class vtkMatrix4x4;
class vtkIGSIORecursiveCriticalSection;

/*!
  \class vtkIGSIOConfig
  \brief Singleton class providing tools needed for handling the configuration - finding files, assembling file names, holding and saving the configuration data
  \ingroup IGSIOLibCommon
*/
class VTKIGSIOCOMMON_EXPORT vtkIGSIOConfig : public vtkObject
{
public:
  vtkTypeMacro(vtkIGSIOConfig, vtkObject);

  /*! New */
  static vtkIGSIOConfig* New();

  /*!
    Instance getter for the singleton class
    \return Instance object
  */
  static vtkIGSIOConfig* GetInstance();

  /*! Allows cleanup of the singleton at application exit */
  static void SetInstance(vtkIGSIOConfig* instance);

  /*!
    Saves application configuration data to file (into the same file where it was read from)
    \return Success flag
  */
  igsioStatus SaveApplicationConfigurationToFile();

  /*!
    Returns the path of the application configuration file
  */
  std::string GetApplicationConfigurationFilePath() const;

  /*!
    Assembles a filename for the new device set configuration that is the same as the input file name, only with the current date and time in the end (for saving to a new file)
    \return New device set configuration file name
   */
  std::string GetNewDeviceSetConfigurationFileName();

  /*!
    Write coordinate frame definition transforms to the configuration file (if already exists update the values)
    \param aFromCoordinateFrame Name of the 'from' coordinate frame used to define the transformation
    \param aToCoordinateFrame Name of the 'to' coordinate frame used to define the transformation
    \param aMatrix Matrix value of the FromFrameToFrame transformation
    \param aError Error value of the computed transform (if -1, we don't write it)
    \param aDate Date in string format (if NULL, we don't write it)
  */
  igsioStatus WriteTransformToCoordinateDefinition(const char* aFromCoordinateFrame, const char* aToCoordinateFrame, vtkMatrix4x4* aMatrix, double aError = -1, const char* aDate = NULL);

  /*!
    Read coordinate frame definition transfoms from the actual configuration data in the memory (vtkIGSIOConfig::GetInstance()->GetDeviceSetConfigurationData())
    \param aFromCoordinateFrame Input name of the 'from' coordinate frame used to define the transformation
    \param aToCoordinateFrame Input name of the 'to' coordinate frame used to define the transformation
    \param aMatrix Output matrix value of the FromFrameToFrame transformation
    \param aError Output error value of the computed transform (if NULL, we don't read it)
    \param aDate Output date in string format (if NULL, we don't read it)
  */
  igsioStatus ReadTransformToCoordinateDefinition(const char* aFromCoordinateFrame, const char* aToCoordinateFrame, vtkMatrix4x4* aMatrix, double* aError = NULL, std::string* aDate = NULL);

  /*! Read coordinate frame definition transfoms from specified configuration xml data
    \param aDeviceSetConfigRootElement Device set configuration root element
    \param aFromCoordinateFrame Input name of the 'from' coordinate frame used to define the transformation
    \param aToCoordinateFrame Input name of the 'to' coordinate frame used to define the transformation
    \param aMatrix Output matrix value of the FromFrameToFrame transformation
    \param aError Output error value of the computed transform (if NULL, we don't read it)
    \param aDate Output date in string format (if NULL, we don't read it)
  */
  igsioStatus ReadTransformToCoordinateDefinition(vtkXMLDataElement* aDeviceSetConfigRootElement, const char* aFromCoordinateFrame, const char* aToCoordinateFrame, vtkMatrix4x4* aMatrix, double* aError = NULL, std::string* aDate = NULL);

  /*!
    Replaces an element with a name on the level under the top level in device set configuration with the element in the parameter root XML data element
    \param aElementName Element name to replace
    \param aNewRootElement Root element containing the element to replace with
    \return Success flag
  */
  igsioStatus ReplaceElementInDeviceSetConfiguration(const char* aElementName, vtkXMLDataElement* aNewRootElement);

  /*!
    Searches a data element in an XML tree: the child of aElementName that has the name aChildName and has an attribute aChildAttributeName with the value aChildAttributeValue
    \param aConfig Root XML element in that the search is conducted
    \param aElementName Name of the parent of the searched element
    \param aChildName Name of the searched element
    \param aChildAttributeName Name of the attribute based on which we want the element to be found
    \param aChildAttributeValue Value of the attribute based on which we want the element to be found
    \return Found XML data element
  */
  vtkXMLDataElement* LookupElementWithNameContainingChildWithNameAndAttribute(vtkXMLDataElement* aConfig, const char* aElementName, const char* aChildName, const char* aChildAttributeName, const char* aChildAttributeValue);

  /*!
    Search recursively for a file in the configuration directory
    \param aFileName Name of the file to be searched
    \return The first file that is found (with full path)
  */
  std::string GetFirstFileFoundInConfigurationDirectory(const char* aFileName);

  /*!
    Search recursively for a file in a specified directory
    \param aFileName Name of the file to be searched
    \param aDirectory Directory in which search is performed
    \return The first file that is found (with full path)
  */
  std::string GetFirstFileFoundInDirectory(const char* aFileName, const char* aDirectory);

  /*!
    Set output directory (also changes log file location)
  */
  void SetOutputDirectory(const std::string& aDir);

  /*! Set image directory path (if relative then the base is ProgramDirectory) */
  void SetImageDirectory(const std::string& aDir);

  /*!
    Get absolute image path from input image path
    \param aImagePath Image path read from configuration file
    \param aOutputAbsolutePath Absolute image path if found (output parameter)
  */
  igsioStatus FindImagePath(const std::string& aImagePath, std::string& aFoundAbsolutePath);

  /*!
    Get absolute model path from input image model path
    \param aModelPath Model path read from configuration file
    \param aOutputAbsolutePath Absolute model path if found (output parameter)
  */
  igsioStatus FindModelPath(const std::string& aModelPath, std::string& aFoundAbsolutePath);

  /*!
    Get absolute path for a model file specified by a path relative to the model directory.
    If subPath is not defined then the absolute path of the model directory will be returned.
  */
  std::string GetModelPath(const std::string& subPath);

  /*!
    Get absolute path for a device set configuration file specified by a path relative to the device set configuration directory.
    If subPath is not defined then the absolute path of the device set configuration directory will be returned.
  */
  std::string GetDeviceSetConfigurationPath(const std::string& subPath);

  /*! Get absolute path of the current device set configuration file */
  std::string GetDeviceSetConfigurationFileName();

  /*! Set device set configuration directory path (if relative then the base is ProgramDirectory) */
  void SetDeviceSetConfigurationDirectory(const std::string& aDir);

  /*! Set device set configuration file name (if relative then the base is ProgramDirectory) */
  void SetDeviceSetConfigurationFileName(const std::string& aFilePath);

  /*!
  * Convenience function to create a device set configuration from a file
  * Returned pointer must be managed by the caller
  */
  vtkXMLDataElement* CreateDeviceSetConfigurationFromFile(const std::string& aConfigFile);

  /*!
    Get absolute path for an output file specified by a path relative to the output directory.
  */
  std::string GetOutputPath(const std::string& subPath);

  /*!
    Get absolute path of the output directory.
  */
  std::string GetOutputDirectory();

  /*!
    Get absolute path of the device set configuration directory.
  */
  std::string GetDeviceSetConfigurationDirectory();

  /*!
    Get absolute path of the image directory.
  */
  std::string GetImageDirectory();

  /*!
    Get absolute path for an image file specified by a path relative to the image directory.
    If subPath is not defined then the absolute path of the image directory will be returned.
  */
  std::string GetImagePath(const std::string& subPath);

  /*!
    Get absolute path for a script file specified by a path relative to the scripts directory.
    If subPath is not defined then the absolute path of the scripts directory will be returned.
  */
  std::string GetScriptPath(const std::string& subPath);

  /*! Get device set configuration data */
  vtkGetObjectMacro(DeviceSetConfigurationData, vtkXMLDataElement);
  /*! Set device set configuration data */
  void SetDeviceSetConfigurationData(vtkXMLDataElement* deviceSetConfigurationData);

  /*! Get editor application executable path and file name */
  vtkGetStdStringMacro(EditorApplicationExecutable);
  /*! Set editor application executable path and file name */
  vtkSetStdStringMacro(EditorApplicationExecutable);

  /*! Get application start timestamp */
  vtkGetStdStringMacro(ApplicationStartTimestamp);

  /*!
    Gets the full path of a IGSIO executable file.
    executableName should not contain file extension (.exe is added automatically on Windows platforms)
  */
  std::string GetIGSIOExecutablePath(const std::string& executableName);

protected:
  /*! Set program directory path */
  void SetProgramDirectory();

  /*!
    Get absolute path from a relative or absolute path
    \param aPath Relative or absolute path of a directory.
    \param aBasePath If aPath is a relative path then this aBasePath will be prepended to the input aPath
  */
  std::string GetAbsolutePath(const std::string& aPath, const std::string& aBasePath);

  /*! Get application configuration data */
  vtkGetObjectMacro(ApplicationConfigurationData, vtkXMLDataElement);
  /*! Set application configuration data */
  vtkSetObjectMacro(ApplicationConfigurationData, vtkXMLDataElement);

protected:
  /*!
    Save application configuration file
    \return Success flag
  */
  igsioStatus WriteApplicationConfiguration();

  /*!
    Read application configuration from file (IGSIOConfig.xml in the application's directory)
    \return Success flag
  */
  igsioStatus LoadApplicationConfiguration();

  /*!
    Search recursively for a file in a specified directory (core, protected function)
    \param aFileName Name of the file to be searched
    \param aDirectory Directory in which search is performed
    \return The first file that is found with full path
  */
  std::string FindFileRecursivelyInDirectory(const char* aFileName, const char* aDirectory);

protected:
  vtkIGSIOConfig();
  virtual  ~vtkIGSIOConfig();

protected:
  /*! Device set configuration directory path. If relative then the base is ProgramDirectory. */
  std::string DeviceSetConfigurationDirectory;

  /*! Used device set configuration file name (for assembling the new file name with the date) */
  std::string DeviceSetConfigurationFileName;

  /*! Session device set configuration data */
  vtkXMLDataElement*  DeviceSetConfigurationData;

  /*! Application configuration data */
  vtkXMLDataElement*  ApplicationConfigurationData;

  /*! Path and filename of the editor application executable to be used */
  std::string EditorApplicationExecutable;

  /*! Output directory path. If relative then the base is ProgramDirectory. */
  std::string OutputDirectory;

  /*! Program directory of the executable. Absolute path. */
  std::string ProgramDirectory;

  /*! Image directory path. It is used as home directory for images when the image path for a saved dataset is not an absolute path. If relative then the base is ProgramDirectory. */
  std::string ImageDirectory;

  /*! Model directory path. It is used as home directory for models when the model path for a saved dataset is not an absolute path. If relative then the base is ProgramDirectory. */
  std::string ModelDirectory;

  /*! Scripts directory path. If relative then the base is ProgramDirectory. */
  std::string ScriptsDirectory;

  /*! Formatted string timestamp of the application start time - used as a prefix for most outputs */
  std::string ApplicationStartTimestamp;

private:
  /*! Instance of the singleton */
  static vtkIGSIOConfig* Instance;
};


#endif

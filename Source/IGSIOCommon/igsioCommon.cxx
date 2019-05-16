/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

// Local includes
#include "igsioCommon.h"
#include "igsioTrackedFrame.h"
#include "vtkIGSIOTrackedFrameList.h"
#include "vtkIGSIOAccurateTimer.h"

// VTK includes
#include <vtkXMLDataElement.h>
#include <vtksys/SystemTools.hxx>

// STL includes
#include <algorithm>
#include <string>
#include <limits>

#ifdef _WIN32
  #include <Windows.h>
#elif __linux__
  #include <linux/limits.h>
#endif

//-------------------------------------------------------
bool vtkIGSIOLogHelper::ShouldWeLog(bool errorPresent)
{
  if (errorPresent)
  {
    ++m_Count;
    double timeStamp = vtkIGSIOAccurateTimer::GetSystemTime();
    if (timeStamp - m_LastError > m_MinimumTimeBetweenLoggingSec
        || m_Count > m_MinimumCountBetweenLogging)
    {
      //log the error this time
      m_LastError = timeStamp;
      m_Count = 0;
      return true;
    }
    else
    {
      return false;
    }
  }
  else //error has just been removed, reset to initial state
  {
    m_LastError = -std::numeric_limits<double>::max() / 2;
    m_Count = -2;
    return false;
  }
}

//-------------------------------------------------------
igsioTransformName::igsioTransformName()
{
}

//-------------------------------------------------------
igsioTransformName::~igsioTransformName()
{
}

//-------------------------------------------------------
igsioTransformName::igsioTransformName(std::string aFrom, std::string aTo)
{
  this->Capitalize(aFrom);
  this->m_From = aFrom;

  this->Capitalize(aTo);
  this->m_To = aTo;
}

//-------------------------------------------------------
igsioTransformName::igsioTransformName(const std::string& transformName)
{
  this->SetTransformName(transformName.c_str());
}

//-------------------------------------------------------
bool igsioTransformName::IsValid() const
{
  if (this->m_From.empty())
  {
    return false;
  }

  if (this->m_To.empty())
  {
    return false;
  }

  return true;
}

//-------------------------------------------------------
igsioStatus igsioTransformName::SetTransformName(const char* aTransformName)
{
  if (aTransformName == NULL)
  {
    LOG_ERROR("Failed to set transform name if it's NULL");
    return IGSIO_FAIL;
  }

  return this->SetTransformName(std::string(aTransformName));
}

//----------------------------------------------------------------------------
igsioStatus igsioTransformName::SetTransformName(const std::string& aTransformName)
{
  if (aTransformName.empty())
  {
    LOG_ERROR("Failed to set transform name if it's empty");
    return IGSIO_FAIL;
  }

  this->m_From.clear();
  this->m_To.clear();

  size_t posTo = std::string::npos;

  // Check if the string has only one valid 'To' phrase
  int numOfMatch = 0;
  std::string subString = aTransformName;
  size_t posToTested = std::string::npos;
  size_t numberOfRemovedChars = 0;
  while (((posToTested = subString.find("To")) != std::string::npos) && (subString.length() > posToTested + 2))
  {
    if (toupper(subString[posToTested + 2]) == subString[posToTested + 2])
    {
      // there is a "To", and after that the next letter is uppercase, so it's really a match (e.g., the first To in TestToolToTracker would not be a real match)
      numOfMatch++;
      posTo = numberOfRemovedChars + posToTested;
    }
    // search in the rest of the string
    subString = subString.substr(posToTested + 2);
    numberOfRemovedChars += posToTested + 2;
  }

  if (numOfMatch != 1)
  {
    LOG_ERROR("Unable to parse transform name, there are " << numOfMatch
              << " matching 'To' phrases in the transform name '" << aTransformName << "', while exactly one allowed.");
    return IGSIO_FAIL;
  }

  // Find <FrameFrom>To<FrameTo> matches
  if (posTo == std::string::npos)
  {
    LOG_ERROR("Failed to set transform name - unable to find 'To' in '" << aTransformName << "'!");
    return IGSIO_FAIL;
  }
  else if (posTo == 0)
  {
    LOG_ERROR("Failed to set transform name - no coordinate frame name before 'To' in '" << aTransformName << "'!");
    return IGSIO_FAIL;
  }
  else if (posTo == aTransformName.length() - 2)
  {
    LOG_ERROR("Failed to set transform name - no coordinate frame name after 'To' in '" << aTransformName << "'!");
    return IGSIO_FAIL;
  }

  // Set From coordinate frame name
  this->m_From = aTransformName.substr(0, posTo);

  // Allow handling of To coordinate frame containing "Transform"
  std::string postFrom(aTransformName.substr(posTo + 2));
  if (postFrom.find("Transform") != std::string::npos)
  {
    postFrom = postFrom.substr(0, postFrom.find("Transform"));
  }

  this->m_To = postFrom;
  this->Capitalize(this->m_From);
  this->Capitalize(this->m_To);

  return IGSIO_SUCCESS;
}

//-------------------------------------------------------
igsioStatus igsioTransformName::GetTransformName(std::string& aTransformName) const
{
  if (this->m_From.empty())
  {
    LOG_ERROR("Failed to get transform name - 'From' transform name is empty");
    return IGSIO_FAIL;
  }

  if (this->m_To.empty())
  {
    LOG_ERROR("Failed to get transform name - 'To' transform name is empty");
    return IGSIO_FAIL;
  }

  aTransformName = (this->m_From + std::string("To") + this->m_To);
  return IGSIO_SUCCESS;
}

//-------------------------------------------------------
std::string igsioTransformName::GetTransformName() const
{
  return (this->m_From + std::string("To") + this->m_To);
}

//-------------------------------------------------------
std::string igsioTransformName::From() const
{
  return this->m_From;
}

//-------------------------------------------------------
std::string igsioTransformName::To() const
{
  return this->m_To;
}

//-------------------------------------------------------
void igsioTransformName::Capitalize(std::string& aString)
{
  // Change first character to uppercase
  if (aString.length() < 1)
  {
    return;
  }
  aString[0] = toupper(aString[0]);
}

//-------------------------------------------------------
void igsioTransformName::Clear()
{
  this->m_From = "";
  this->m_To = "";
}

//----------------------------------------------------------------------------
igsioStatus igsioCommon::CreateTemporaryFilename(std::string& aString, const std::string& anOutputDirectory)
{
  aString = "";
  int maxRetryCount = 50;
  int tryCount = 0;
  while (tryCount < maxRetryCount)
  {
    tryCount++;

#ifdef _WIN32
    // Get output directory
    char candidateFilename[MAX_PATH] = "";
    std::string path;
    if (!anOutputDirectory.empty())
    {
      path = vtksys::SystemTools::GetRealPath(anOutputDirectory.c_str());
    }
    else
    {
      char tempPath[MAX_PATH] = "";
      if (GetTempPath(MAX_PATH, tempPath) == 0)
      {
        LOG_ERROR("Unable to retrieve temp path: " << GetLastError());
        return IGSIO_FAIL;
      }
      path = tempPath;
    }

    // Get full output file path
    UINT uRetVal = GetTempFileName(path.c_str(), "tmp", 0, candidateFilename);   // buffer for name
    if (uRetVal == ERROR_BUFFER_OVERFLOW)
    {
      if (vtksys::SystemTools::FileExists(candidateFilename))
      {
        vtksys::SystemTools::RemoveFile(candidateFilename);
      }
      LOG_ERROR("Path too long to generate temporary filename (" << path << "). Consider moving output directory to shorter path.");
      continue;
    }
    else if (uRetVal == 0)
    {
      LOG_ERROR("Failed to generate temporary filename. Error code:" << GetLastError());
      continue;
    }

    aString = candidateFilename;
    return IGSIO_SUCCESS;
#else
    std::string path;
    if (!anOutputDirectory.empty())
    {
      path = vtksys::SystemTools::GetRealPath(anOutputDirectory.c_str());
    }
    else
    {
      path = std::string(P_tmpdir);
    }

    char candidateFilenameBuffer[PATH_MAX];
    memset(candidateFilenameBuffer, 0, PATH_MAX);
    strncpy(candidateFilenameBuffer, path.c_str(), path.length());
    strcat(candidateFilenameBuffer, "/plusTmpFile-XXXXXX");
    char* candidateFilename = mktemp(candidateFilenameBuffer);

    if (vtksys::SystemTools::FileExists(candidateFilename))
    {
      continue;
    }
    ofstream aFile(candidateFilename);
    if (!aFile.is_open())
    {
      LOG_WARNING("Cannot write to temp file " << candidateFilename << " check write permissions of output directory.");
      continue;
    }

    aFile.close();
    vtksys::SystemTools::RemoveFile(candidateFilename);
    aString = candidateFilename;
    return IGSIO_SUCCESS;
#endif
  }

  LOG_ERROR("igsioCommon::CreateTemporaryFilename failed to generate a temporary file name");
  return IGSIO_FAIL;
}

//----------------------------------------------------------------------------
std::vector<std::string> igsioCommon::GetSequenceExtensions()
{
  // Return list of supported sequence file extensions in lower case
  std::vector<std::string> sequenceExtensions;
  sequenceExtensions.push_back(".igs.mha");
  sequenceExtensions.push_back(".igs.mhd");
  sequenceExtensions.push_back(".igs.nrrd");
  sequenceExtensions.push_back(".igs.nhdr");
  sequenceExtensions.push_back(".seq.mha");
  sequenceExtensions.push_back(".seq.mhd");
  sequenceExtensions.push_back(".seq.nrrd");
  sequenceExtensions.push_back(".seq.nhdr");
  sequenceExtensions.push_back(".mha");
  sequenceExtensions.push_back(".mhd");
  sequenceExtensions.push_back(".nrrd");
  sequenceExtensions.push_back(".nhdr");
  return sequenceExtensions;
}

//----------------------------------------------------------------------------
std::string igsioCommon::GetSequenceFilenameWithoutExtension(std::string name)
{
  std::string extension = igsioCommon::GetSequenceFilenameExtension(name);
  return name.substr(0, name.size() - extension.size());;
}

//----------------------------------------------------------------------------
std::string igsioCommon::GetSequenceFilenameExtension(std::string name)
{
  std::string lowerName(name);
  std::transform(begin(lowerName), end(lowerName), lowerName.begin(), ::tolower);

  std::vector<std::string> extensions = igsioCommon::GetSequenceExtensions();
  std::vector<std::string>::iterator extensionIt;
  for (extensionIt = extensions.begin(); extensionIt != extensions.end(); ++extensionIt)
  {
    if (extensionIt->size() > lowerName.size())
    {
      continue;
    }

    std::string tail = lowerName.substr(lowerName.size() - extensionIt->size());
    if (tail == *extensionIt)
    {
      // Return original extension (including original capitalization)
      return name.substr(lowerName.size() - extensionIt->size());
    }
  }
  return "";
}

//----------------------------------------------------------------------------
std::string igsioCommon::Tail(const std::string& source, const std::string::size_type length)
{
  if (length >= source.size())
  {
    return source;
  }
  return source.substr(source.size() - length);
}


//-------------------------------------------------------
std::string igsioCommon::Trim(const std::string& str)
{
  std::string copyStr(str);
  copyStr.erase(str.find_last_not_of(" \t\r\n") + 1);
  copyStr.erase(0, str.find_first_not_of(" \t\r\n"));

  return copyStr;
}

//----------------------------------------------------------------------------
// print out data while replacing XML special characters <, >, &, ", ' with
// &lt;, &gt;, &amp;, &quot;, &apos;, respectively.
void PrintWithEscapedData(ostream& os, const char* data)
{
  for (size_t i = 0; i < strlen(data); i++)
  {
    switch (data[i])
    {
      case '&':
        os << "&amp;";
        break;
      case '<':
        os << "&lt;";
        break;
      case '>':
        os << "&gt;";
        break;
      case '"':
        os << "&quot;";
        break;
      case '\'':
        os << "&apos;";
        break;
      default:
        os << data[i];
    }
  }
}

//----------------------------------------------------------------------------
igsioStatus igsioCommon::XML::PrintXML(ostream& os, vtkIndent indent, vtkXMLDataElement* elem)
{
  if (elem == NULL)
  {
    LOG_ERROR("igsioCommon::XML::PrintXML failed, input element is invalid");
    return IGSIO_FAIL;
  }
  vtkIndent nextIndent = indent.GetNextIndent();

  os << indent << "<" << elem->GetName();

  // If there are many attributes then print each of them in separate lines to improve readability
  bool printEachAttributeInNewLine = elem->GetNumberOfAttributes() > 5;

  for (int i = 0; i < elem->GetNumberOfAttributes(); ++i)
  {
    std::string attName = elem->GetAttributeName(i);

    // Find out if it's a matrix element, because we format them somewhat differently
    bool matrixElement = false;
    const int MATRIX_ELEM_COUNT = 16;
    double matrixValues[MATRIX_ELEM_COUNT] = {0};
    if (attName.find("Matrix") != std::string::npos || attName.find("Transform") != std::string::npos)
    {
      if (elem->GetVectorAttribute(attName.c_str(), MATRIX_ELEM_COUNT, matrixValues) == MATRIX_ELEM_COUNT)
      {
        // This seems to be a vector
        matrixElement = true;
      }
    }

    if (matrixElement)
    {
      os << std::endl;
      os << nextIndent << attName << "=\"" << std::endl;
      vtkIndent matrixIndent = nextIndent.GetNextIndent();
      os << matrixIndent << matrixValues[0] << "\t" << matrixValues[1] << "\t" << matrixValues[2] << "\t" << matrixValues[3] << std::endl;
      os << matrixIndent << matrixValues[4] << "\t" << matrixValues[5] << "\t" << matrixValues[6] << "\t" << matrixValues[7] << std::endl;
      os << matrixIndent << matrixValues[8] << "\t" << matrixValues[9] << "\t" << matrixValues[10] << "\t" << matrixValues[11] << std::endl;
      os << matrixIndent << matrixValues[12] << "\t" << matrixValues[13] << "\t" << matrixValues[14] << "\t" << matrixValues[15] << "\"";
      // Prevent writing additional attributes right after the last line of the matrix
      // If this is the last attribute then we don't have to start a new line, just append the closing to the matrix line.
      // If each attribute is written to a separate line anyway then we don't have to start a new line, it'll be added before adding the next element.
      bool isLastAttribute = (i + 1 == elem->GetNumberOfAttributes());
      if (!isLastAttribute && !printEachAttributeInNewLine)
      {
        os << std::endl << nextIndent;
      }
    }
    else
    {
      if (printEachAttributeInNewLine)
      {
        os << std::endl << nextIndent;
      }
      else
      {
        os << " ";
      }
      os << attName << "=\"";
      PrintWithEscapedData(os, elem->GetAttributeValue(i));
      os << "\"";
    }
  }
  // Long format tag is needed if either or both
  // nested elements or inline data are present.
  std::string charData;
  char* charDataPtr = elem->GetCharacterData();
  if (charDataPtr != NULL)
  {
    charData = Trim(charDataPtr);
  }

  if (elem->GetNumberOfNestedElements() > 0 || !charData.empty())
  {
    os << ">\n";
    // nested elements
    for (int i = 0; i < elem->GetNumberOfNestedElements(); ++i)
    {
      PrintXML(os, nextIndent, elem->GetNestedElement(i));
    }
    // inline data
    if (!charData.empty())
    {
      os << nextIndent;
      PrintWithEscapedData(os, charData.c_str());
      os << endl;
    }
    // close tag
    os << indent << "</" << elem->GetName() << ">\n";
  }
  // We can get away with short format tag.
  else
  {
    os << " />\n";
  }
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioCommon::XML::PrintXML(const std::string& fname, vtkXMLDataElement* elem)
{
  ofstream of(fname);
  if (!of.is_open())
  {
    LOG_ERROR("Failed to open " << fname << " for writing");
    return IGSIO_FAIL;
  }
  of.imbue(std::locale::classic());
  return igsioCommon::XML::PrintXML(of, vtkIndent(), elem);
}

//----------------------------------------------------------------------------
std::string igsioCommon::GetIGSIOVersionString()
{
  std::string plusLibVersion = std::string("IGSIO-") + std::string(IGSIO_VERSION);
#ifdef _DEBUG
  plusLibVersion += " (debug build)";
#endif
#if defined _WIN64
  plusLibVersion += " - Win64";
#elif defined _WIN32
  plusLibVersion += " - Win32";
#elif defined __APPLE__
  plusLibVersion += " - Mac";
#else
  plusLibVersion += " - Linux";
#endif
  return plusLibVersion;
}

//-------------------------------------------------------
void igsioCommon::SplitStringIntoTokens(const std::string& s, char delim, std::vector<std::string>& elems, bool keepEmptyParts)
{
  std::istringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim))
  {
    if (keepEmptyParts || !item.empty())
    {
      elems.push_back(item);
    }
  }
}

//----------------------------------------------------------------------------
VTKIGSIOCOMMON_EXPORT std::vector<std::string> igsioCommon::SplitStringIntoTokens(const std::string& s, char delim, bool keepEmptyParts/*=true*/)
{
  std::vector<std::string> tokens;
  igsioCommon::SplitStringIntoTokens(s, delim, tokens, keepEmptyParts);
  return tokens;
}

//----------------------------------------------------------------------------
igsioStatus igsioCommon::DrawLine(vtkImageData& imageData, const std::array<float, 3>& colour, LINE_STYLE style,
                                  const std::array<int, 3>& startPixel, const std::array<int, 3>& endPixel,
                                  unsigned int numberOfPoints, ALPHA_BEHAVIOR alphaBehavior /*= ALPHA_BEHAVIOR_OPAQUE */)
{
  auto checkRange([](int startPixel, int endPixel) -> bool
  {
    if (endPixel < startPixel)
    {
      auto temp = startPixel;
      startPixel = endPixel;
      endPixel = temp;
    }

    if (endPixel < 0 && endPixel - startPixel > 0)
    {
      // negative overflow
      return false;
    }

    return true;
  });

  if (!checkRange(startPixel[0], endPixel[0]))
  {
    LOG_ERROR("Cannot express horizontal slope. Value exceeds int limits.");
    return IGSIO_FAIL;
  }

  if (!checkRange(startPixel[1], endPixel[1]))
  {
    LOG_ERROR("Cannot express vertical slope. Value exceeds int limits.");
    return IGSIO_FAIL;
  }

  if (!checkRange(startPixel[2], endPixel[2]))
  {
    LOG_ERROR("Cannot express depth slope. Value exceeds int limits.");
    return IGSIO_FAIL;
  }

  if (style == LINE_STYLE_SOLID)
  {
    numberOfPoints = std::max(endPixel[0] - startPixel[0], endPixel[1] - startPixel[1]) + 1;
  }
  else if (numberOfPoints < 2)
  {
    LOG_ERROR("Unable to draw a line with less than 1 point!");
    return IGSIO_FAIL;
  }

  int* extent = imageData.GetExtent();
  double directionVectorX = static_cast<double>(endPixel[0] - startPixel[0]) / (numberOfPoints - 1);
  double directionVectorY = static_cast<double>(endPixel[1] - startPixel[1]) / (numberOfPoints - 1);
  double directionVectorZ = static_cast<double>(endPixel[2] - startPixel[2]) / (numberOfPoints - 1);
  for (unsigned int point = 0; point < numberOfPoints; ++point)
  {
    unsigned int pixelCoordX = startPixel[0] + directionVectorX * point;
    unsigned int pixelCoordY = startPixel[1] + directionVectorY * point;
    unsigned int pixelCoordZ = startPixel[2] + directionVectorZ * point;

    for (int component = 0; component < std::min(imageData.GetNumberOfScalarComponents(), 3); ++component)
    {
      if (pixelCoordX < static_cast<unsigned int>(extent[0]) || pixelCoordX > static_cast<unsigned int>(extent[1]) || pixelCoordY < static_cast<unsigned int>(extent[2]) || pixelCoordY > static_cast<unsigned int>(extent[3]))
      {
        // outside of the specified extent
        continue;
      }
      imageData.SetScalarComponentFromFloat(pixelCoordX, pixelCoordY, pixelCoordZ, component, colour[component]);
    }
    if (imageData.GetNumberOfScalarComponents() > 3 && alphaBehavior == ALPHA_BEHAVIOR_OPAQUE)
    {
      if (pixelCoordX < static_cast<unsigned int>(extent[0]) || pixelCoordX > static_cast<unsigned int>(extent[1]) || pixelCoordY < static_cast<unsigned int>(extent[2]) || pixelCoordY > static_cast<unsigned int>(extent[3]))
      {
        // outside of the specified extent
        continue;
      }
      // TODO : is a component value from [0,1]?
      imageData.SetScalarComponentFromFloat(pixelCoordX, pixelCoordY, pixelCoordZ, 3, 1.f);
    }
  }

  return IGSIO_SUCCESS;
}

namespace
{
  //----------------------------------------------------------------------------
  bool icompare_pred(unsigned char a, unsigned char b)
  {
    return ::tolower(a) == ::tolower(b);
  }

  //----------------------------------------------------------------------------
  bool icompare_pred_w(wchar_t a, wchar_t b)
  {
    return ::towlower(a) == ::towlower(b);
  }
}

//----------------------------------------------------------------------------
TrackedFrameFieldStatus igsioCommon::ConvertToolStatusToTrackedFrameFieldStatus(const ToolStatus& status)
{
  TrackedFrameFieldStatus fieldStatus = FIELD_INVALID;
  if (status == TOOL_OK)
  {
    fieldStatus = FIELD_OK;
  }

  return fieldStatus;
}

//----------------------------------------------------------------------------
ToolStatus igsioCommon::ConvertTrackedFrameFieldStatusToToolStatus(TrackedFrameFieldStatus fieldStatus)
{
  ToolStatus status = TOOL_MISSING;
  if (fieldStatus == FIELD_OK)
  {
    status = TOOL_OK;
  }

  return status;
}

//----------------------------------------------------------------------------
ToolStatus igsioCommon::ConvertStringToToolStatus(const std::string& status)
{
  if (igsioCommon::IsEqualInsensitive("OK", status))
  {
    return TOOL_OK;
  }
  else if (igsioCommon::IsEqualInsensitive("MISSING", status))
  {
    return TOOL_MISSING;
  }
  else if (igsioCommon::IsEqualInsensitive("OUT_OF_VIEW", status))
  {
    return TOOL_OUT_OF_VIEW;
  }
  else if (igsioCommon::IsEqualInsensitive("OUT_OF_VOLUME", status))
  {
    return TOOL_OUT_OF_VOLUME;
  }
  else if (igsioCommon::IsEqualInsensitive("SWITCH1_IS_ON", status))
  {
    return TOOL_SWITCH1_IS_ON;
  }
  else if (igsioCommon::IsEqualInsensitive("SWITCH2_IS_ON", status))
  {
    return TOOL_SWITCH2_IS_ON;
  }
  else if (igsioCommon::IsEqualInsensitive("SWITCH3_IS_ON", status))
  {
    return TOOL_SWITCH3_IS_ON;
  }
  else if (igsioCommon::IsEqualInsensitive("REQ_TIMEOUT", status))
  {
    return TOOL_REQ_TIMEOUT;
  }
  else if (igsioCommon::IsEqualInsensitive("INVALID", status))
  {
    return TOOL_INVALID;
  }
  else if (igsioCommon::IsEqualInsensitive("PATH_NOT_FOUND", status))
  {
    return TOOL_PATH_NOT_FOUND;
  }
  else
  {
    LOG_ERROR("Unknown tool status string received. Defaulting to TOOL_UNKNOWN.");
    return TOOL_UNKNOWN;
  }
}

//----------------------------------------------------------------------------
std::string igsioCommon::ConvertToolStatusToString(const ToolStatus& status)
{
  std::string flagFieldValue;
  if (status == TOOL_OK)
  {
    flagFieldValue = "OK";
  }
  else if (status == TOOL_MISSING)
  {
    flagFieldValue = "MISSING";
  }
  else if (status == TOOL_OUT_OF_VIEW)
  {
    flagFieldValue = "OUT_OF_VIEW";
  }
  else if (status == TOOL_OUT_OF_VOLUME)
  {
    flagFieldValue = "OUT_OF_VOLUME";
  }
  else if (status == TOOL_SWITCH1_IS_ON)
  {
    flagFieldValue = "SWITCH1_IS_ON";
  }
  else if (status == TOOL_SWITCH2_IS_ON)
  {
    flagFieldValue = "SWITCH2_IS_ON";
  }
  else if (status == TOOL_SWITCH3_IS_ON)
  {
    flagFieldValue = "SWITCH3_IS_ON";
  }
  else if (status == TOOL_REQ_TIMEOUT)
  {
    flagFieldValue = "REQ_TIMEOUT";
  }
  else if (status == TOOL_INVALID)
  {
    flagFieldValue = "INVALID";
  }
  else if (status == TOOL_PATH_NOT_FOUND)
  {
    flagFieldValue = "PATH_NOT_FOUND";
  }
  else
  {
    LOG_ERROR("Unknown tracker status received - set \"UNKNOWN\" by default!");
    flagFieldValue = "UNKNOWN";
  }

  return flagFieldValue;
}

//----------------------------------------------------------------------------
bool VTKIGSIOCOMMON_EXPORT igsioCommon::IsEqualInsensitive(std::string const& a, std::string const& b)
{
  if (a.length() == b.length())
  {
    return std::equal(b.begin(), b.end(), a.begin(), icompare_pred);
  }
  else
  {
    return false;
  }
}

//----------------------------------------------------------------------------
bool VTKIGSIOCOMMON_EXPORT igsioCommon::IsEqualInsensitive(std::wstring const& a, std::wstring const& b)
{
  if (a.length() == b.length())
  {
    return std::equal(b.begin(), b.end(), a.begin(), icompare_pred_w);
  }
  else
  {
    return false;
  }
}

//----------------------------------------------------------------------------
bool VTKIGSIOCOMMON_EXPORT igsioCommon::HasSubstrInsensitive(std::string const& a, std::string const& b)
{
  std::string lowerA(a);
  std::transform(begin(lowerA), end(lowerA), lowerA.begin(), ::tolower);
  std::string lowerB(b);
  std::transform(begin(lowerB), end(lowerB), lowerB.begin(), ::tolower);

  return lowerA.find(lowerB) != std::string::npos;
}

//----------------------------------------------------------------------------
bool VTKIGSIOCOMMON_EXPORT igsioCommon::HasSubstrInsensitive(std::wstring const& a, std::wstring const& b)
{
  std::wstring lowerA(a);
  std::transform(begin(lowerA), end(lowerA), begin(lowerA), ::towlower);
  std::wstring lowerB(b);
  std::transform(begin(lowerB), end(lowerB), begin(lowerB), ::towlower);

  return lowerA.find(lowerB) != std::wstring::npos;
}

//----------------------------------------------------------------------------
VTKIGSIOCOMMON_EXPORT igsioStatus igsioCommon::DrawScanLines(int* inputImageExtent, float greyValue, const PixelLineList& scanLineEndPoints, vtkIGSIOTrackedFrameList* trackedFrameList)
{
  std::array<float, 3> colour;
  colour[0] = colour[1] = colour[2] = greyValue;

  return igsioCommon::DrawScanLines(inputImageExtent, colour, scanLineEndPoints, trackedFrameList);
}

//----------------------------------------------------------------------------
VTKIGSIOCOMMON_EXPORT igsioStatus igsioCommon::DrawScanLines(int* inputImageExtent, const std::array<float, 3>& colour, const PixelLineList& scanLineEndPoints, vtkIGSIOTrackedFrameList* trackedFrameList)
{
  LOG_DEBUG("Processing " << trackedFrameList->GetNumberOfTrackedFrames() << " frames...");

  igsioStatus result(IGSIO_SUCCESS);
  for (unsigned int frameIndex = 0; frameIndex < trackedFrameList->GetNumberOfTrackedFrames(); frameIndex++)
  {
    LOG_DEBUG("Processing frame " << frameIndex);
    igsioTrackedFrame* frame = trackedFrameList->GetTrackedFrame(frameIndex);
    igsioCommon::DrawScanLines(inputImageExtent, colour, scanLineEndPoints, frame->GetImageData()->GetImage());
  }

  return result;
}

//----------------------------------------------------------------------------
VTKIGSIOCOMMON_EXPORT igsioStatus igsioCommon::DrawScanLines(int* inputImageExtent, float greyValue, const PixelLineList& scanLineEndPoints, vtkImageData* imageData)
{
  std::array<float, 3> colour;
  colour[0] = colour[1] = colour[2] = greyValue;

  return igsioCommon::DrawScanLines(inputImageExtent, colour, scanLineEndPoints, imageData);
}

//----------------------------------------------------------------------------
VTKIGSIOCOMMON_EXPORT igsioStatus igsioCommon::DrawScanLines(int* inputImageExtent, const std::array<float, 3>& colour, const PixelLineList& scanLineEndPoints, vtkImageData* imageData)
{
  int numOfSamplesPerScanline = inputImageExtent[1] - inputImageExtent[0] + 1;

  igsioStatus result(IGSIO_SUCCESS);
  for (auto iter = scanLineEndPoints.begin(); iter != scanLineEndPoints.end(); iter++)
  {
    if (igsioCommon::DrawLine(*imageData, colour, igsioCommon::LINE_STYLE_SOLID, iter->first, iter->second, numOfSamplesPerScanline) != IGSIO_SUCCESS)
    {
      LOG_ERROR("Scaline failed. Unable to draw line.");
      result = IGSIO_FAIL;
      continue;
    }
  }

  return result;
}

//----------------------------------------------------------------------------
igsioStatus igsioCommon::DrawLine(vtkImageData& imageData, float greyValue, LINE_STYLE style, const std::array<int, 3>& startPixel,
                                  const std::array<int, 3>& endPixel, unsigned int numberOfPoints, ALPHA_BEHAVIOR alphaBehavior /*= ALPHA_BEHAVIOR_OPAQUE */)
{
  std::array<float, 3> colour;
  colour[0] = colour[1] = colour[2] = greyValue;

  return igsioCommon::DrawLine(imageData, colour, style, startPixel, endPixel, numberOfPoints, alphaBehavior);
}

//-------------------------------------------------------
bool igsioCommon::IsClippingRequested(const std::array<int, 3>& clipOrigin, const std::array<int, 3>& clipSize)
{
  return (
           clipOrigin[0] != igsioCommon::NO_CLIP &&
           clipOrigin[1] != igsioCommon::NO_CLIP &&
           clipOrigin[2] != igsioCommon::NO_CLIP &&
           clipSize[0] != igsioCommon::NO_CLIP &&
           clipSize[1] != igsioCommon::NO_CLIP &&
           clipSize[2] != igsioCommon::NO_CLIP
         );
}

//-------------------------------------------------------
bool igsioCommon::IsClippingWithinExtents(const std::array<int, 3>& clipOrigin, const std::array<int, 3>& clipSize, const int extents[6])
{
  return (clipOrigin[0] >= extents[0] && clipOrigin[0] <= extents[1]) &&
         (clipOrigin[1] >= extents[2] && clipOrigin[1] <= extents[3]) &&   // Verify that the origin is within the image
         (clipOrigin[2] >= extents[4] && clipOrigin[2] <= extents[5]) &&

         (clipOrigin[0] + clipSize[0] - 1 <= extents[1]) &&
         (clipOrigin[1] + clipSize[1] - 1 <= extents[3]) &&   // Verify that the extent of the clipping falls within the image
         (clipOrigin[2] + clipSize[2] - 1 <= extents[5]);
}

//-------------------------------------------------------
igsioStatus igsioCommon::RobustFwrite(FILE* fileHandle, void* data, size_t dataSize, size_t& writtenSize)
{
  // on some systems fwrite cannot write all data in one chunk, therefore we have to write
  // chunks until all bytes are written or failed to write any bytes
  unsigned char* writePointer = static_cast<unsigned char*>(data);
  size_t remainingBytes = dataSize;
  size_t writtenBytesForCurrentBlock = 0;
  while ((writtenBytesForCurrentBlock = fwrite(writePointer, 1, remainingBytes, fileHandle)) > 0 && !ferror(fileHandle))
  {
    remainingBytes -= writtenBytesForCurrentBlock;
    writePointer += writtenBytesForCurrentBlock;
    if (remainingBytes == 0)
    {
      // completed
      break;
    }
  }

  writtenSize = dataSize - remainingBytes;
  if (remainingBytes > 0)
  {
    LOG_ERROR("Filed to write data to file. Data size: " << dataSize << ", successfully written: " << writtenSize << " bytes");
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioCommon::XML::SafeCheckAttributeValueInsensitive(vtkXMLDataElement& element, const std::string& attributeName, const std::string& value, bool& isEqual)
{
  if (attributeName.empty())
  {
    return IGSIO_FAIL;
  }
  auto attr = element.GetAttribute(attributeName.c_str());
  if (attr == NULL)
  {
    return IGSIO_FAIL;
  }
  isEqual = igsioCommon::IsEqualInsensitive(attr, value);
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioCommon::XML::SafeGetAttributeValueInsensitive(vtkXMLDataElement& element, const std::string& attributeName, std::string& value)
{
  if (attributeName.empty())
  {
    return IGSIO_FAIL;
  }
  auto attr = element.GetAttribute(attributeName.c_str());
  if (attr == NULL)
  {
    return IGSIO_FAIL;
  }
  value = attr;
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
VTKIGSIOCOMMON_EXPORT igsioStatus igsioCommon::XML::SafeGetAttributeValueInsensitive(vtkXMLDataElement& element, const std::wstring& attributeName, std::string& value)
{
  return igsioCommon::XML::SafeGetAttributeValueInsensitive(element, std::string(begin(attributeName), end(attributeName)), value);
}

//----------------------------------------------------------------------------
VTKIGSIOCOMMON_EXPORT igsioStatus igsioCommon::XML::SafeCheckAttributeValueInsensitive(vtkXMLDataElement& element, const std::wstring& attributeName, const std::wstring& value, bool& isEqual)
{
  return igsioCommon::XML::SafeCheckAttributeValueInsensitive(element, std::string(begin(attributeName), end(attributeName)), std::string(begin(value), end(value)), isEqual);
}

//-----------------------------------------------------------------------------
std::string igsioCommon::GetAbsolutePath(const std::string& path, const std::string& filename)
{
  if (path.empty())
  {
    // empty
    return filename;
  }
  if (vtksys::SystemTools::FileIsFullPath(filename.c_str()))
  {
    // already absolute
    return filename;
  }

  // relative to the ProgramDirectory
  return  vtksys::SystemTools::CollapseFullPath(filename.c_str(), path.c_str());
}

//-----------------------------------------------------------------------------
igsioStatus igsioCommon::FindImagePath(const std::string& aImagePath, std::string& aFoundAbsolutePath)
{
  aFoundAbsolutePath = aImagePath;
  if (vtksys::SystemTools::FileExists(aImagePath.c_str()))
  {
    return IGSIO_SUCCESS;
  }

  // Check file relative to the current working directory
  aFoundAbsolutePath = vtksys::SystemTools::CollapseFullPath(aImagePath.c_str(), vtksys::SystemTools::GetCurrentWorkingDirectory().c_str());
  if (vtksys::SystemTools::FileExists(aFoundAbsolutePath.c_str()))
  {
    return IGSIO_SUCCESS;
  }

  aFoundAbsolutePath = "";
  return IGSIO_FAIL;
}

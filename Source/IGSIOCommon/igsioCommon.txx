/*=IGSIO=header=begin======================================================
Program: IGSIO
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================IGSIO=header=end*/

namespace igsioCommon
{
  namespace XML
  {
    //----------------------------------------------------------------------------
    template<typename T>
    igsioStatus SafeGetAttributeValueInsensitive(vtkXMLDataElement& element, const std::wstring& attributeName, T& value)
    {
      return igsioCommon::XML::SafeGetAttributeValueInsensitive(element, std::string(begin(attributeName), end(attributeName)), value);
    }

    //----------------------------------------------------------------------------
    template<typename T>
    igsioStatus SafeGetAttributeValueInsensitive(vtkXMLDataElement& element, const std::string& attributeName, T& value)
    {
      if (element.GetAttribute(attributeName.c_str()) == NULL)
      {
        return IGSIO_FAIL;
      }
      element.GetScalarAttribute(attributeName.c_str(), value);
      return IGSIO_SUCCESS;
    }
  }

  //----------------------------------------------------------------------------
  template<typename ElemType>
  void JoinTokensIntoString(const std::vector<ElemType>& elems, std::string& output, char separator)
  {
    output = "";
    std::stringstream ss;
    typedef std::vector<ElemType> List;

    for (typename List::const_iterator it = elems.begin(); it != elems.end(); ++it)
    {
      ss << *it;
      if (it != elems.end() - 1)
      {
        ss << separator;
      }
    }

    output = ss.str();
  }
}
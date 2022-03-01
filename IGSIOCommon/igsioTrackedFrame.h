/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __TRACKEDFRAME_H
#define __TRACKEDFRAME_H

#include "vtkigsiocommon_export.h"

#include "igsioCommon.h"
#include "igsioVideoFrame.h"

class vtkMatrix4x4;
class vtkPoints;

/*!
  \class TrackedFrame
  \brief Stores tracked frame (image + pose information + field data)
  \ingroup PlusLibCommon
*/
class VTKIGSIOCOMMON_EXPORT igsioTrackedFrame
{
public:
  static const char* FIELD_FRIENDLY_DEVICE_NAME;

public:
  static const std::string TransformPostfix;
  static const std::string TransformStatusPostfix;

public:
  igsioTrackedFrame();
  ~igsioTrackedFrame();
  igsioTrackedFrame(const igsioTrackedFrame& frame);
  igsioTrackedFrame& operator=(igsioTrackedFrame const& trackedFrame);

public:
  /*! Set image data */
  void SetImageData(const igsioVideoFrame& value);

  /*! Get image data */
  igsioVideoFrame* GetImageData();

  /*! Set timestamp */
  void SetTimestamp(double value);

  /*! Get timestamp */
  double GetTimestamp();

  /*! Set frame field */
  void SetFrameField(std::string name, std::string value, igsioFrameFieldFlags flags = FRAMEFIELD_NONE);

  /*! Get frame field value */
  std::string GetFrameField(const char* fieldName) const;
  std::string GetFrameField(const std::string& fieldName) const;

  /*! Delete frame field */
  igsioStatus DeleteFrameField(const char* fieldName);
  igsioStatus DeleteFrameField(const std::string& fieldName);

  /*!
    Check if a frame field is defined or not
    \return true, if the field is defined; false, if the field is not defined
  */
  bool IsFrameFieldDefined(const char* fieldName);

  /*!
    Check if a frame transform name field is defined or not
    \return true, if the field is defined; false, if the field is not defined
  */
  bool IsFrameTransformNameDefined(const igsioTransformName& transformName);

  /*! Get frame transform */
  igsioStatus GetFrameTransform(const igsioTransformName& frameTransformName, double transform[16]) const;
  /*! Get frame transform */
  igsioStatus GetFrameTransform(const igsioTransformName& frameTransformName, vtkMatrix4x4* transformMatrix) const;

  /*! Get frame status */
  igsioStatus GetFrameTransformStatus(const igsioTransformName& frameTransformName, ToolStatus& status) const;
  /*! Set frame status */
  igsioStatus SetFrameTransformStatus(const igsioTransformName& frameTransformName, ToolStatus status);

  /*! Set frame transform */
  igsioStatus SetFrameTransform(const igsioTransformName& frameTransformName, double transform[16]);

  /*! Set frame transform */
  igsioStatus SetFrameTransform(const igsioTransformName& frameTransformName, vtkMatrix4x4* transform);

  /*! Get the list of the name of all frame fields */
  void GetFrameFieldNameList(std::vector<std::string>& fieldNames) const;

  /*! Get the list of the transform name of all frame transforms*/
  void GetFrameTransformNameList(std::vector<igsioTransformName>& transformNames) const;

  /*! Get tracked frame size in pixel. Returns: FrameSizeType.  */
  FrameSizeType GetFrameSize() const;

  /* Get the fourCC of the encoded frame */
  std::string GetEncodingFourCC() const;

  /*! Get tracked frame pixel size in bits (scalar size * number of scalar components) */
  int GetNumberOfBitsPerScalar() const;

  /*! Get number of scalar components in a pixel */
  igsioStatus GetNumberOfScalarComponents(unsigned int& numberOfScalarComponents) const;

  /*! Get number of bits in a pixel */
  int GetNumberOfBitsPerPixel() const;

  /*! Set Segmented fiducial point pixel coordinates */
  void SetFiducialPointsCoordinatePx(vtkPoints* fiducialPoints);

  /*! Get Segmented fiducial point pixel coordinates */
  vtkPoints* GetFiducialPointsCoordinatePx();

  /*! Print tracked frame human readable serialization data to XML data
      If requestedTransforms is empty, all stored FrameFields are sent
  */
  igsioStatus PrintToXML(vtkXMLDataElement* xmlData, const std::vector<igsioTransformName>& requestedTransforms) const;

  /*! Serialize Tracked frame human readable data to xml data and return in string */
  igsioStatus GetTrackedFrameInXmlData(std::string& strXmlData, const std::vector<igsioTransformName>& requestedTransforms) const;

  /*! Deserialize TrackedFrame human readable data from xml data string */
  igsioStatus SetTrackedFrameFromXmlData(const char* strXmlData);
  /*! Deserialize TrackedFrame human readable data from xml data string */
  igsioStatus SetTrackedFrameFromXmlData(const std::string& xmlData);

  /*! Convert from field status string to field status enum */
  static TrackedFrameFieldStatus ConvertFieldStatusFromString(const char* statusStr);

  /*! Convert from field status enum to field status string */
  static std::string ConvertFieldStatusToString(TrackedFrameFieldStatus status);

  /*! Return all custom fields in a map */
  const igsioFieldMapType& GetCustomFields();
  igsioFieldMapType GetFrameFields() const;

  /*! Returns true if the input string ends with "Transform", else false */
  static bool IsTransform(std::string str);

  /*! Returns true if the input string ends with "TransformStatus", else false */
  static bool IsTransformStatus(std::string str);

public:
  bool operator< (igsioTrackedFrame data)
  {
    return Timestamp < data.Timestamp;
  }
  bool operator== (const igsioTrackedFrame& data) const
  {
    return (Timestamp == data.Timestamp);
  }

protected:
  igsioVideoFrame ImageData;
  double Timestamp;

  igsioFieldMapType FrameFields;

  mutable FrameSizeType FrameSize;      // Cached value is updated from underlying image data when accessed
  mutable std::string   EncodingFourCC; // Cached value is updated from underlying image data when accessed

  /*! Stores segmented fiducial point pixel coordinates */
  vtkPoints* FiducialPointsCoordinatePx;
};

//----------------------------------------------------------------------------

/*!
  \enum TrackedFrameValidationRequirements
  \brief If any of the requested requirement is not fulfilled then the validation fails.
  \ingroup PlusLibCommon
*/
enum TrackedFrameValidationRequirements
{
  REQUIRE_UNIQUE_TIMESTAMP = 0x0001, /*!< the timestamp shall be unique */
  REQUIRE_TRACKING_OK = 0x0002, /*!<  the tracking flags shall be valid (TOOL_OK) */
  REQUIRE_CHANGED_ENCODER_POSITION = 0x0004, /*!<  the stepper encoder position shall be different from the previous ones  */
  REQUIRE_SPEED_BELOW_THRESHOLD = 0x0008, /*!<  the frame acquisition speed shall be less than a threshold */
  REQUIRE_CHANGED_TRANSFORM = 0x0010, /*!<  the transform defined by name shall be different from the previous ones  */
};

/*!
  \class TrackedFrameTimestampFinder
  \brief Helper class used for validating timestamps in a tracked frame list
  \ingroup PlusLibCommon
*/
class TrackedFrameTimestampFinder
{
public:
  TrackedFrameTimestampFinder(igsioTrackedFrame* frame): mTrackedFrame(frame) {};
  bool operator()(igsioTrackedFrame* newFrame)
  {
    return newFrame->GetTimestamp() == mTrackedFrame->GetTimestamp();
  }
  igsioTrackedFrame* mTrackedFrame;
};

//----------------------------------------------------------------------------

/*!
  \class TrackedFrameEncoderPositionFinder
  \brief Helper class used for validating encoder position in a tracked frame list
  \ingroup PlusLibCommon
*/
class VTKIGSIOCOMMON_EXPORT igsioTrackedFrameEncoderPositionFinder
{
public:
  igsioTrackedFrameEncoderPositionFinder(igsioTrackedFrame* frame, double minRequiredTranslationDifferenceMm, double minRequiredAngleDifferenceDeg);
  ~igsioTrackedFrameEncoderPositionFinder();

  /*! Get stepper encoder values from the tracked frame */
  static igsioStatus GetStepperEncoderValues(igsioTrackedFrame* trackedFrame, double& probePosition, double& probeRotation, double& templatePosition);

  /*!
    Predicate unary function for std::find_if to validate encoder position
    \return Returning true if the encoder position difference is less than required
  */
  bool operator()(igsioTrackedFrame* newFrame);

protected:
  igsioTrackedFrame* mTrackedFrame;
  double mMinRequiredTranslationDifferenceMm;
  double mMinRequiredAngleDifferenceDeg;
};

//----------------------------------------------------------------------------

/*!
  \class TrackedFrameTransformFinder
  \brief Helper class used for validating frame transform in a tracked frame list
  \ingroup PlusLibCommon
*/
class TrackedFrameTransformFinder
{
public:
  TrackedFrameTransformFinder(igsioTrackedFrame* frame, const igsioTransformName& frameTransformName, double minRequiredTranslationDifferenceMm, double minRequiredAngleDifferenceDeg);
  ~TrackedFrameTransformFinder();

  /*!
    Predicate unary function for std::find_if to validate transform
    \return Returning true if the transform difference is less than required
  */
  bool operator()(igsioTrackedFrame* newFrame);

protected:
  igsioTrackedFrame* mTrackedFrame;
  double mMinRequiredTranslationDifferenceMm;
  double mMinRequiredAngleDifferenceDeg;
  igsioTransformName mFrameTransformName;
};

#endif

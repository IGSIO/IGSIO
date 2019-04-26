/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkIGSIOTrackedFrameList_h
#define __vtkIGSIOTrackedFrameList_h

#include "vtkigsiocommon_export.h"

#include "igsioVideoFrame.h" // for US_IMAGE_ORIENTATION
#include "vtkObject.h"

#include <deque>

class vtkXMLDataElement;
class igsioTrackedFrame;
class vtkMatrix4x4;

/*!
  \class vtkIGSIOTrackedFrameList
  \brief Stores a list of tracked frames (image + pose information)

  Validation threshold values: If the threshold==0 it means that no
  checking is needed (the frame is always accepted). If the threshold>0
  then the frame is considered valid only if the position/angle
  difference compared to all previously acquired frames is larger than
  the position/angle minimum value and the translation/rotation speed is lower
  than the maximum allowed translation/rotation.

  \ingroup PlusLibCommon
*/
class VTKIGSIOCOMMON_EXPORT vtkIGSIOTrackedFrameList : public vtkObject
{
public:
  typedef std::deque<igsioTrackedFrame*> TrackedFrameListType;

  static vtkIGSIOTrackedFrameList* New();
  vtkTypeMacro(vtkIGSIOTrackedFrameList, vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /*!
    Action performed after AddTrackedFrame got invalid frame.
    Invalid frame can be a TrackedFrame if the validation requirement didn't meet the expectation.
  */
  enum InvalidFrameAction
  {
    ADD_INVALID_FRAME_AND_REPORT_ERROR = 0, /*!< Add invalid frame to the list and report an error */
    ADD_INVALID_FRAME, /*!< Add invalid frame to the list wihout notification */
    SKIP_INVALID_FRAME_AND_REPORT_ERROR, /*!< Skip invalid frame and report an error */
    SKIP_INVALID_FRAME /*!< Skip invalid frame wihout notification */
  };

  /*! Add tracked frame to container. If the frame is invalid then it may not actually add it to the list. */
  virtual igsioStatus AddTrackedFrame(igsioTrackedFrame* trackedFrame, InvalidFrameAction action = ADD_INVALID_FRAME_AND_REPORT_ERROR);

  /*! Add tracked frame to container by taking ownership of the passed pointer. If the frame is invalid then it may not actually add it to the list (it will be deleted immediately). */
  virtual igsioStatus TakeTrackedFrame(igsioTrackedFrame* trackedFrame, InvalidFrameAction action = ADD_INVALID_FRAME_AND_REPORT_ERROR);

  /*! Add all frames from a tracked frame list to the container. It adds all invalid frames as well, but an error is reported. */
  virtual igsioStatus AddTrackedFrameList(vtkIGSIOTrackedFrameList* inTrackedFrameList, InvalidFrameAction action = ADD_INVALID_FRAME_AND_REPORT_ERROR);

  /*! Get tracked frame from container */
  virtual igsioTrackedFrame* GetTrackedFrame(int frameNumber);
  virtual igsioTrackedFrame* GetTrackedFrame(unsigned int frameNumber);

  /*! Get number of tracked frames */
  virtual unsigned int GetNumberOfTrackedFrames();
  virtual unsigned int Size();

  /*! Get the tracked frame list */
  TrackedFrameListType GetTrackedFrameList();

  /* Retrieve the latest timestamp in the tracked frame list */
  double GetMostRecentTimestamp();

  /*! Remove a tracked frame from the list and free up memory
    \param frameNumber Index of tracked frame to remove (from 0 to NumberOfFrames-1)
  */
  virtual igsioStatus RemoveTrackedFrame(int frameNumber);

  /*! Remove a range of tracked frames from the list and free up memory
    \param frameNumberFrom First frame to be removed (inclusive)
    \param frameNumberTo Last frame to be removed (inclusive)
  */
  virtual igsioStatus RemoveTrackedFrameRange(unsigned int frameNumberFrom, unsigned int frameNumberTo);

  /*! Clear tracked frame list and free memory */
  virtual void Clear();

  /*! Set the number of following unique frames needed in the tracked frame list */
  vtkSetMacro(NumberOfUniqueFrames, int);

  /*! Get the number of following unique frames needed in the tracked frame list */
  vtkGetMacro(NumberOfUniqueFrames, int);

  /*! Set the threshold of acceptable speed of position change */
  vtkSetMacro(MinRequiredTranslationDifferenceMm, double);

  /*!Get the threshold of acceptable speed of position change */
  vtkGetMacro(MinRequiredTranslationDifferenceMm, double);

  /*! Set the threshold of acceptable speed of orientation change in degrees */
  vtkSetMacro(MinRequiredAngleDifferenceDeg, double);

  /*! Get the threshold of acceptable speed of orientation change in degrees */
  vtkGetMacro(MinRequiredAngleDifferenceDeg, double);

  /*! Set the maximum allowed translation speed in mm/sec */
  vtkSetMacro(MaxAllowedTranslationSpeedMmPerSec, double);

  /*! Get the maximum allowed translation speed in mm/sec */
  vtkGetMacro(MaxAllowedTranslationSpeedMmPerSec, double);

  /*! Set the maximum allowed rotation speed in degree/sec */
  vtkSetMacro(MaxAllowedRotationSpeedDegPerSec, double);

  /*! Get the maximum allowed rotation speed in degree/sec */
  vtkGetMacro(MaxAllowedRotationSpeedDegPerSec, double);

  /*! Set validation requirements
  \sa TrackedFrameValidationRequirements
  */
  vtkSetMacro(ValidationRequirements, long);

  /*! Get validation requirements
  \sa TrackedFrameValidationRequirements
  */
  vtkGetMacro(ValidationRequirements, long);

  /*! Set frame transform name used for transform validation */
  void SetFrameTransformNameForValidation(const igsioTransformName& aTransformName)
  {
    this->FrameTransformNameForValidation = aTransformName;
  }

  /*! Get frame transform name used for transform validation */
  igsioTransformName GetFrameTransformNameForValidation();

  /*! Get tracked frame scalar size in bits */
  virtual int GetNumberOfBitsPerScalar();

  /*! Get tracked frame pixel size in bits (scalar size * number of scalar components) */
  virtual int GetNumberOfBitsPerPixel();

  /*! Get tracked frame pixel type */
  igsioCommon::VTKScalarPixelType GetPixelType();

  /*! Get number of components */
  int GetNumberOfScalarComponents();

  /*! Get tracked frame image orientation */
  US_IMAGE_ORIENTATION GetImageOrientation();

  /*! Get tracked frame image type */
  US_IMAGE_TYPE GetImageType();

  /*! Get tracked frame image size*/
  igsioStatus GetFrameSize(FrameSizeType& outFrameSize);

  /* Get tracked frame encoding */
  igsioStatus GetEncodingFourCC(std::string& encoding);

  /*! Get the value of the custom field. If we couldn't find it, return NULL */
  virtual const char* GetCustomString(const char* fieldName);
  virtual std::string GetCustomString(const std::string& fieldName) const;
  virtual std::string GetFrameField(const std::string& fieldName) const;

  /*! Set custom string value to \c fieldValue. If \c fieldValue is NULL then the field is deleted. */
  virtual igsioStatus SetCustomString(const char* fieldName, const char* fieldValue, igsioFrameFieldFlags flags = FRAMEFIELD_NONE);
  virtual igsioStatus SetCustomString(const std::string& fieldName, const std::string& fieldValue, igsioFrameFieldFlags flags = FRAMEFIELD_NONE);
  virtual igsioStatus SetFrameField(const std::string& fieldName, const std::string& fieldValue, igsioFrameFieldFlags flags = FRAMEFIELD_NONE);

  /*! Get the custom transformation matrix from metafile by frame transform name
  * It will search for a field like: Seq_Frame[frameNumber]_[frameTransformName]
  * Return false if the the field is missing */
  virtual igsioStatus GetCustomTransform(const char* frameTransformName, vtkMatrix4x4* transformMatrix) const;
  virtual igsioStatus GetTransform(const igsioTransformName& name, vtkMatrix4x4& outMatrix) const;

  /*! Get the custom transformation matrix from metafile by frame transform name
  * It will search for a field like: Seq_Frame[frameNumber]_[frameTransformName]
  * Return false if the the field is missing */
  virtual igsioStatus GetCustomTransform(const char* frameTransformName, double* transformMatrix) const;
  virtual igsioStatus GetTransform(const igsioTransformName& name, double* outMatrix) const;

  /*! Set the custom transformation matrix from metafile by frame transform name
  * It will search for a field like: Seq_Frame[frameNumber]_[frameTransformName] */
  virtual igsioStatus SetCustomTransform(const char* frameTransformName, vtkMatrix4x4* transformMatrix);
  virtual igsioStatus SetTransform(const igsioTransformName& frameTransformName, const vtkMatrix4x4& transformMatrix);

  /*! Set the custom transformation matrix from metafile by frame transform name
  * It will search for a field like: Seq_Frame[frameNumber]_[frameTransformName] */
  virtual igsioStatus SetCustomTransform(const char* frameTransformName, double* transformMatrix);
  virtual igsioStatus SetTransform(const igsioTransformName& frameTransformName, double* transformMatrix);

  /*! Get custom field name list */
  void GetCustomFieldNameList(std::vector<std::string>& fieldNames);

  /*! Get global transform (stored in the Offset and TransformMatrix fields) */
  igsioStatus GetGlobalTransform(vtkMatrix4x4* globalTransform);

  /*! Set global transform (stored in the Offset and TransformMatrix fields) */
  igsioStatus SetGlobalTransform(vtkMatrix4x4* globalTransform);

  /*!
    Verify properties of a tracked frame list. If the tracked frame list pointer is invalid or the expected properties
    (image orientation, type) are different from the actual values then the method returns with failure.
    It is a static method so that the validity of the pointer can be easily checked as well.
  */
  static igsioStatus VerifyProperties(vtkIGSIOTrackedFrameList* trackedFrameList, US_IMAGE_ORIENTATION expectedOrientation, US_IMAGE_TYPE expectedType);

  /*! Return true if the list contains at least one valid image frame */
  bool IsContainingValidImageData();

  /*! Implement support for C++11 ranged for loops */
  TrackedFrameListType::iterator begin();
  TrackedFrameListType::iterator end();
  TrackedFrameListType::const_iterator begin() const;
  TrackedFrameListType::const_iterator end() const;

  TrackedFrameListType::reverse_iterator rbegin();
  TrackedFrameListType::reverse_iterator rend();
  TrackedFrameListType::const_reverse_iterator rbegin() const;
  TrackedFrameListType::const_reverse_iterator rend() const;

protected:
  vtkIGSIOTrackedFrameList();
  virtual ~vtkIGSIOTrackedFrameList();

  /*!
    Perform validation on a tracked frame . If any of the requested requirement is not fulfilled then the validation fails.
    \param trackedFrame Input tracked frame
    \return True if the frame is valid
    \sa TrackedFrameValidationRequirements
  */
  virtual bool ValidateData(igsioTrackedFrame* trackedFrame);

  bool ValidateTimestamp(igsioTrackedFrame* trackedFrame);
  bool ValidateTransform(igsioTrackedFrame* trackedFrame);
  bool ValidateStatus(igsioTrackedFrame* trackedFrame);
  bool ValidateEncoderPosition(igsioTrackedFrame* trackedFrame);
  bool ValidateSpeed(igsioTrackedFrame* trackedFrame);

  TrackedFrameListType TrackedFrameList;
  igsioFieldMapType CustomFields;

  int NumberOfUniqueFrames;

  /*! Validation threshold value */
  double MinRequiredTranslationDifferenceMm;
  /*! Validation threshold value */
  double MinRequiredAngleDifferenceDeg;
  /*! Validation threshold value */
  double MaxAllowedTranslationSpeedMmPerSec;
  /*! Validation threshold value */
  double MaxAllowedRotationSpeedDegPerSec;

  long ValidationRequirements;
  igsioTransformName FrameTransformNameForValidation;

private:
  vtkIGSIOTrackedFrameList(const vtkIGSIOTrackedFrameList&);
  void operator=(const vtkIGSIOTrackedFrameList&);
};

/// Implement support for C++11 ranged for loops
VTKIGSIOCOMMON_EXPORT vtkIGSIOTrackedFrameList::TrackedFrameListType::iterator begin(vtkIGSIOTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkIGSIOTrackedFrameList::TrackedFrameListType::iterator end(vtkIGSIOTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkIGSIOTrackedFrameList::TrackedFrameListType::const_iterator begin(const vtkIGSIOTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkIGSIOTrackedFrameList::TrackedFrameListType::const_iterator end(const vtkIGSIOTrackedFrameList& list);

VTKIGSIOCOMMON_EXPORT vtkIGSIOTrackedFrameList::TrackedFrameListType::reverse_iterator rbegin(vtkIGSIOTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkIGSIOTrackedFrameList::TrackedFrameListType::reverse_iterator rend(vtkIGSIOTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkIGSIOTrackedFrameList::TrackedFrameListType::const_reverse_iterator rbegin(const vtkIGSIOTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkIGSIOTrackedFrameList::TrackedFrameListType::const_reverse_iterator rend(const vtkIGSIOTrackedFrameList& list);

#endif
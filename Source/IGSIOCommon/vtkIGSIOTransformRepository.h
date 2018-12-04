/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkIGSIOTransformRepository_h
#define __vtkIGSIOTransformRepository_h

// Local includes
////#include "PlusConfigure.h"
#include "igsioCommon.h"
#include "vtkigsiocommon_export.h"


// VTK includes
#include <vtkObject.h>

// STL includes
#include <list>
#include <map>

class igsioTrackedFrame;
class vtkMatrix4x4;
class vtkIGSIORecursiveCriticalSection;
class vtkTransform;

/*!
\class vtkIGSIOTransformRepository
\brief Combine multiple transforms to get a transform between arbitrary coordinate frames

The vtkIGSIOTransformRepository stores a number of transforms between coordinate frames and
it can multiply these transforms (or the inverse of these transforms) to
compute the transform between any two coordinate frames.

Example usage:
\code
  transformRepository->SetTransform("Probe", "Tracker", mxProbeToTracker);
  transformRepository->SetTransform("Image", "Probe", mxImageToProbe);
  ...
  vtkSmartPointer<vtkMatrix4x4> mxImageToTracker=vtkSmartPointer<vtkMatrix4x4>::New();
  vtkIGSIOTransformRepository::TransformStatus status=vtkIGSIOTransformRepository::TRANSFORM_INVALID;
  transformRepository->GetTransform("Image", "Tracker", mxImageToTracker, &status);
\endcode

The following coordinate frames are used commonly:
  \li Image: image frame coordinate system, origin is the bottom-left corner, unit is pixel
  \li Tool: coordinate system of the DRB attached to the probe, unit is mm
  \li Reference: coordinate system of the DRB attached to the reference body, unit is mm
  \li Tracker: coordinate system of the tracker, unit is mm
  \li World: world coordinate system, orientation is usually patient RAS, unit is mm

\ingroup PlusLibCommon
*/
class VTKIGSIOCOMMON_EXPORT vtkIGSIOTransformRepository : public vtkObject
{
public:
  static vtkIGSIOTransformRepository* New();
  vtkTypeMacro(vtkIGSIOTransformRepository, vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /*!
    Set a transform matrix between two coordinate frames. The method fails if the transform
    can be already constructed by concatenating/inverting already stored transforms. Changing an already
    set transform is allowed. The transform is computed even if one or more of the used transforms
    have non valid status.
  */
  virtual igsioStatus SetTransform(const igsioTransformName& aTransformName, vtkMatrix4x4* matrix, ToolStatus toolStatus = TOOL_OK);

  /*!
    Set all transform matrices between two coordinate frames stored in TrackedFrame. The method fails if any of the transforms
    can be already constructed by concatenating/inverting already stored transforms. Changing an already
    set transform is allowed. The transform is computed even if one or more of the used transforms
    have non valid statuses.
  */
  virtual igsioStatus SetTransforms(igsioTrackedFrame& trackedFrame);

  /*!
    Set the valid status of a transform matrix between two coordinate frames. A transform is normally valid,
    but temporarily it can be set to non valid (e.g., when a tracked tool gets out of view).
  */
  virtual igsioStatus SetTransformStatus(const igsioTransformName& aTransformName, ToolStatus toolStatus);

  /*!
    Set the persistent status of a transform matrix between two coordinate frames. A transform is non persistent by default.
    Transforms with status persistent will be written into config file on WriteConfiguration call.
  */
  virtual igsioStatus SetTransformPersistent(const igsioTransformName& aTransformName, bool isPersistent);

  /*! Set the computation error of the transform matrix between two coordinate frames. */
  virtual igsioStatus SetTransformError(const igsioTransformName& aTransformName, double aError);

  /*! Get the computation error of the transform matrix between two coordinate frames. */
  virtual igsioStatus GetTransformError(const igsioTransformName& aTransformName, double& aError, bool quiet = false);

  /*! Set the computation date of the transform matrix between two coordinate frames. */
  virtual igsioStatus SetTransformDate(const igsioTransformName& aTransformName, const std::string& aDate);

  /*! Get the computation date of the transform matrix between two coordinate frames. */
  virtual igsioStatus GetTransformDate(const igsioTransformName& aTransformName, std::string& aDate, bool quiet = false);

  /*!
    Read all transformations from XML data CoordinateDefinitions element and add them to the transforms with
    persistent and valid status. The method fails if any of the transforms
    can be already constructed by concatenating/inverting already stored transforms. Changing an already
    set transform is allowed.
  */
  virtual igsioStatus ReadConfiguration(vtkXMLDataElement* configRootElement);

  /*!
    Delete all transforms from XML data CoordinateDefinitions element then write all transform matrices with persistent status
    into the xml data CoordinateDefinitions element. The function will give a warning message in case of any non valid persistent transform.
  */
  virtual igsioStatus WriteConfiguration(vtkXMLDataElement* configRootElement);

  /*!
    Delete all transforms from XML data CoordinateDefinitions element then write all transform matrices that are persistent and non-persistent if boolean
    is true, only persistent if the boolean is false into the xml data CoordinateDefinitions element. The function will give a warning message
    in case of any non valid persistent transform.
  */
  virtual igsioStatus WriteConfigurationGeneric(vtkXMLDataElement* configRootElement, bool copyAllTransforms);

  /*!
    Get a transform matrix between two coordinate frames. The method fails if the transform
    cannot be already constructed by combining/inverting already stored transforms.
    \param aTransformName name of the transform to retrieve from the repository
    \param matrix the retrieved transform is copied into this matrix
    \param toolStatus if this parameter is not NULL then the transforms' status is returned at that memory address
  */
  virtual igsioStatus GetTransform(const igsioTransformName& aTransformName, vtkMatrix4x4* matrix, ToolStatus* toolStatus = NULL) const;

  /*!
    Get the valid status of a transform matrix between two coordinate frames.
    The status is typically invalid when a tracked tool is out of view.
  */
  virtual igsioStatus GetTransformValid(const igsioTransformName& aTransformName, bool& isValid);

  /*! Get the persistent status of a transform matrix between two coordinate frames. */
  virtual igsioStatus GetTransformPersistent(const igsioTransformName& aTransformName, bool& isPersistent);

  /*! Removes a transform from the repository */
  virtual igsioStatus DeleteTransform(const igsioTransformName& aTransformName);

  /*! Removes all the transforms from the repository */
  void Clear();

  /*! Checks if a transform exist */
  virtual igsioStatus IsExistingTransform(const igsioTransformName aTransformName, bool aSilent = true);

  /*! Copies the persistent and non-persistent contents if boolean is true, only persistent contents if fase */
  virtual igsioStatus DeepCopy(vtkIGSIOTransformRepository* sourceRepositoryName, bool copyAllTransforms);

protected:
  vtkIGSIOTransformRepository();
  ~vtkIGSIOTransformRepository();

  /*!
    \struct TransformInfo
    \brief Stores a transformation matrix and some additional information (valid or not, computed or not)
    \ingroup PlusLibCommon
  */
  class TransformInfo
  {
  public:
    TransformInfo();
    virtual ~TransformInfo();
    TransformInfo(const TransformInfo& obj);
    TransformInfo& operator=(const TransformInfo& obj);

    bool IsValid() { return m_ToolStatus == TOOL_OK; }

    /*! TransformInfo storing the transformation matrix between two coordinate frames */
    vtkTransform* m_Transform;
    /*! Describes the state of the tool status */
    ToolStatus m_ToolStatus;
    /*!
      If the value is true then it means that the transform is computed from
      another transform (by inverting that). If the value is false it means
      that it is an original transform (set by the user by a SetTransform() method call)
    */
    bool m_IsComputed;
    /*!
      If the value is true then it means that the transform is persistent
      and won't change, so we can save it to config file as a coordinate definition;
    */
    bool m_IsPersistent;
    /*! Persistent transform creation date, saved to configuration file */
    std::string m_Date;
    /*! Persistent transform calculation error (e.g calibration error) */
    double m_Error;
  };

  /*! For each "to" coordinate frame name (first) stores a transform (second)*/
  typedef std::map<std::string, TransformInfo> CoordFrameToTransformMapType;
  /*! For each "from" coordinate frame (first) stores an array of transforms (second) */
  typedef std::map<std::string, CoordFrameToTransformMapType> CoordFrameToCoordFrameToTransformMapType;

  /*! List of transforms */
  typedef std::list<TransformInfo*> TransformInfoListType;

  /*! Get a user-defined original input transform (or its inverse). Does not combine user-defined input transforms. */
  TransformInfo* GetOriginalTransform(const igsioTransformName& aTransformName) const;

  /*!
    Find a transform path between the specified coordinate frames.
    \param aTransformName name of the transform to find
    \param transformInfoList Stores the list of transforms to get from the fromCoordFrameName to toCoordFrameName
    \param skipCoordFrameName This is the name of a coordinate system that should be ignored (e.g., because it was checked previously already)
    \param silent Don't log an error if path cannot be found (it's normal while searching in branches of the graph)
    \return returns IGSIO_SUCCESS if a path can be found, IGSIO_FAIL otherwise
  */
  igsioStatus FindPath(const igsioTransformName& aTransformName, TransformInfoListType& transformInfoList, const char* skipCoordFrameName = NULL, bool silent = false) const;

  mutable CoordFrameToCoordFrameToTransformMapType CoordinateFrames;

  vtkIGSIORecursiveCriticalSection* CriticalSection;

  TransformInfo TransformToSelf;

private:
  vtkIGSIOTransformRepository(const vtkIGSIOTransformRepository&);
  void operator=(const vtkIGSIOTransformRepository&);
};

#endif


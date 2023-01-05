/*=IGSIO=header=begin======================================================
Program: IGSIO
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================IGSIO=header=end*/

#ifndef __vtkIGSIOAbstractStylusCalibrationAlgo_h
#define __vtkIGSIOAbstractStylusCalibrationAlgo_h

// Local includes
#include "igsioConfigure.h"
#include "igsioCommon.h"
#include "vtkigsiocalibration_export.h"

// VTK includes
#include <vtkCommand.h>
#include <vtkObject.h>
#include <vtkMatrix4x4.h>

// STL includes
#include <set>
#include <deque>

class vtkXMLDataElement;

//-----------------------------------------------------------------------------

/*!
  \class vtkIGSIOAbstractStylusCalibrationAlgo
  \brief Abstract calibration algorithm

  Abstract class for performing calibration algorithms.

  Input transforms are collected and stored in an input buffer that inserts each transform into a bucket.
  New buckets are added as each are filled, up to a specified maximum. If a bucket is filled, then it is checked
  to ensure it contains valid calibration poses, and the error within the bucket is below a specified threshold.
  If the poses are invalid or the error is too high, then the entire buffer is discarded.

  \ingroup igsioCalibrationAlgorithm
*/
class VTKIGSIOCALIBRATION_EXPORT vtkIGSIOAbstractStylusCalibrationAlgo : public vtkObject
{
public:
  vtkTypeMacro(vtkIGSIOAbstractStylusCalibrationAlgo, vtkObject);

  enum Events
  {
    InputTransformAddedEvent = vtkCommand::UserEvent + 173,
  };

  enum CalibrationErrorCode
  {
    CALIBRATION_NO_ERROR,
    CALIBRATION_FAIL,
    CALIBRATION_NOT_STARTED,
    CALIBRATION_NOT_ENOUGH_POINTS,
    CALIBRATION_NOT_ENOUGH_VARIATION,
    CALIBRATION_HIGH_ERROR,
  };

  /// Read configuration
  /// \param aConfig Root element of the device set configuration
  virtual igsioStatus ReadConfiguration(vtkXMLDataElement* aConfig) = 0;

  /// Remove all previously inserted calibration points.
  /// Call this method to get rid of previously added calibration points
  /// before starting a new calibration.
  virtual void RemoveAllCalibrationPoints();

  /// Insert acquired point to calibration point list
  /// \param aMarkerToReferenceTransformMatrix New calibration point (tool to reference transform)
  virtual igsioStatus InsertNextCalibrationPoint(vtkMatrix4x4* aMarkerToReferenceTransformMatrix);

  /// Get the number of outlier points. It is recommended to display a warning to the user
  /// if the percentage of outliers vs total number of points is larger than a few percent.
  virtual int GetNumberOfDetectedOutliers();

  /// Get the total number of collected calibration points
  virtual int GetNumberOfCalibrationPoints();

  // Computes the maximum orientation difference in degrees between the first tool transformation
  // and all the others. Used for determining if there was enough variation in the input data.
  virtual double GetMaximumToolOrientationDifferenceDegrees();

  /// Name of the object marker coordinate frame (eg. Stylus)
  vtkGetStdStringMacro(ObjectMarkerCoordinateFrame);
  vtkSetStdStringMacro(ObjectMarkerCoordinateFrame);

  /// Name of the reference coordinate frame (eg. Reference)
  vtkGetStdStringMacro(ReferenceCoordinateFrame);
  vtkSetStdStringMacro(ReferenceCoordinateFrame);

  /// Name of the object pivot point coordinate frame (eg. StylusTip)
  vtkGetStdStringMacro(ObjectPivotPointCoordinateFrame);
  vtkSetStdStringMacro(ObjectPivotPointCoordinateFrame);

  /// Error code indicating what went wrong with the calibration.
  vtkGetMacro(ErrorCode, int);

  //@{
  /// Required minimum amount of variation within the recorded poses
  vtkSetMacro(MinimumOrientationDifferenceDegrees, double);
  vtkGetMacro(MinimumOrientationDifferenceDegrees, double);
  //@}

  //@{
  /// Required minimum amount of variation in position from the previous position in order for a transform to be accepted (0 degrees by default).
  vtkGetMacro(PositionDifferenceThresholdMm, double);
  vtkSetMacro(PositionDifferenceThresholdMm, double);
  //@}

  //@{
  /// Required minimum amount of variation in position from the previous position in order for a transform to be accepted (0 degrees by default).
  vtkGetMacro(OrientationDifferenceThresholdDegrees, double);
  vtkSetMacro(OrientationDifferenceThresholdDegrees, double);
  //@}

  //@{
  /// The number of input points to be stored in each bucket
  vtkGetMacro(PoseBucketSize, int);
  vtkSetMacro(PoseBucketSize, int);
  //@}

  //@{
  /// The maximum number of buckets to be stored. If the number of buckets is exceeded, the oldest will be discarded.
  vtkGetMacro(MaximumNumberOfPoseBuckets, int);
  vtkSetMacro(MaximumNumberOfPoseBuckets, int);
  //@}

  //@{
  /// The accepted amount of error within a single bucket. If the error in the latest bucket exceeds this amount, then all of the buckets will be discarded.
  vtkGetMacro(MaximumPoseBucketError, double);
  vtkSetMacro(MaximumPoseBucketError, double);
  //@}

  //@{
  /// Output calibration tip to marker position
  vtkGetObjectMacro(PivotPointToMarkerTransformMatrix, vtkMatrix4x4);
  vtkSetObjectMacro(PivotPointToMarkerTransformMatrix, vtkMatrix4x4);
  vtkGetVector3Macro(PivotPointPosition_Reference, double);
  //@}

  /// Flip the direction of the shaft (Z-axis).
  static void FlipShaftDirection(vtkMatrix4x4* toolTipToToolMatrix);

  /// Get only the rotation component from toolTipToToolMatrix.
  static void GetToolTipToToolRotation(vtkMatrix4x4* toolTipToToolMatrix, vtkMatrix4x4* rotationMatrix);

  //@{
  /// The maximum amount of error that is allowed for a calibration result to be considered valid.
  /// If this value is >= 0 and the calibration error is greater than the maximum, then the calibration method will return with failure.
  vtkSetMacro(MaximumCalibrationErrorMm, double);
  vtkGetMacro(MaximumCalibrationErrorMm, double);
  //@}

  //@{
  /// Flag that will enable/disable buffer validation.
  /// Disabled by default.
  vtkSetMacro(ValidateInputBufferEnabled, bool);
  vtkGetMacro(ValidateInputBufferEnabled, bool);
  vtkBooleanMacro(ValidateInputBufferEnabled, bool);
  //@}

protected:
  vtkSetMacro(ErrorCode, CalibrationErrorCode);

  /// Get a vector containing all MarkerToReference matrices.
  virtual std::vector<vtkMatrix4x4*> GetAllMarkerToReferenceMatrices();
  virtual void GetAllMarkerToReferenceMatrices(std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray);

  /// Get a vector containing MarkerToReference in the specified bucket.
  virtual std::vector<vtkMatrix4x4*> GetBucketMarkerToReferenceMatrices(int bucket);
  virtual void GetBucketMarkerToReferenceMatrices(int bucket, std::vector<vtkMatrix4x4*>& matrixArray);

  /// Get a vector containing MarkerToReference in the latest bucket.
  virtual std::vector<vtkMatrix4x4*> GetLatestBucketMarkerToReferenceMatrices();
  virtual void GetLatestBucketMarkerToReferenceMatrices(std::vector<vtkMatrix4x4*>& matrixArray);

  /// Removes all data if the latest bucket is not within the acceptable error threshold.
  virtual void ValidateInputBuffer();

  /// Perform the internal calibration and calculate the error on the input vector of MarkerToReference matrices.
  /// This method is used to perform calibration on each bucket as it is filled.
  /// If the calibration fails, or if the error is higher than MaximumPoseBucketError, then the input buffer is discarded.
  virtual igsioStatus DoCalibrationInternal(const std::vector<vtkMatrix4x4*>& markerToReferenceMatrixArray, double& error) = 0;

  // Returns the orientation difference in degrees between two 4x4 homogeneous transformation matrix, in degrees.
  virtual double GetOrientationDifferenceDegrees(vtkMatrix4x4* aMatrix, vtkMatrix4x4* bMatrix);

  // Verify whether the tool's shaft is in the same direction as the ToolTip to Tool vector.
  // Rotate the ToolTip coordinate frame by 180 degrees about the secondary axis to make the
  // shaft in the same direction as the ToolTip to Tool vector, if this is not already the case.
  void UpdateShaftDirection(vtkMatrix4x4* toolTipToToolMatrix);

protected:
  vtkIGSIOAbstractStylusCalibrationAlgo();
  virtual ~vtkIGSIOAbstractStylusCalibrationAlgo();

protected:
  vtkMatrix4x4* PivotPointToMarkerTransformMatrix;
  double        PivotPointPosition_Reference[4];

  vtkMatrix4x4* PreviousMarkerToReferenceTransformMatrix;

  std::string ObjectMarkerCoordinateFrame;
  std::string ReferenceCoordinateFrame;
  std::string ObjectPivotPointCoordinateFrame;

  std::set<unsigned int>    OutlierIndices;

  CalibrationErrorCode     ErrorCode;

  double                    MinimumOrientationDifferenceDegrees;

  double                    PositionDifferenceThresholdMm;
  double                    OrientationDifferenceThresholdDegrees;

  int                       PoseBucketSize;
  int                       MaximumNumberOfPoseBuckets;
  double                    MaximumPoseBucketError;

  double                    MaximumCalibrationErrorMm;

  bool                      ValidateInputBufferEnabled;

  struct MarkerToReferenceTransformMatrixBucket
  {
    MarkerToReferenceTransformMatrixBucket()
    {
      this->MarkerToReferenceCalibrationPoints = std::vector< vtkSmartPointer<vtkMatrix4x4> >();
    }
    std::vector< vtkSmartPointer<vtkMatrix4x4> > MarkerToReferenceCalibrationPoints;
  };
  std::deque<MarkerToReferenceTransformMatrixBucket>  MarkerToReferenceTransformMatrixBuckets;

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

/*=IGSIO=header=begin======================================================
Program: IGSIO
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================IGSIO=header=end*/

#ifndef __vtkIGSIOPivotCalibrationAlgo_h
#define __vtkIGSIOPivotCalibrationAlgo_h

// Local includes
#include "igsioConfigure.h"
#include "igsioCommon.h"
#include "vtkIGSIOAbstractStylusCalibrationAlgo.h"

// VTK includes
#include <vtkCommand.h>
#include <vtkObject.h>
#include <vtkMatrix4x4.h>

// STL includes
#include <set>
#include <deque>

class vtkIGSIOTransformRepository;
class vtkXMLDataElement;

//-----------------------------------------------------------------------------
/*!
  \class vtkIGSIOPivotCalibrationAlgo
  \brief Pivot calibration algorithm to calibrate a stylus. It determines the pose of the stylus tip relative to the marker attached to the stylus.

  The stylus tip position is computed by robust LSQR method, which detects and ignores outliers (that have much larger reprojection error than other points).

  The stylus pose is computed assuming that the marker is attached on the center of one of the stylus axes, which is often a good approximation.
  The axis that points towards the marker is the PivotPoint coordinate system's -Z axis (so that points in front of the stylus have positive Z coordinates
  in the PivotPoint coordinate system). The X axis of the PivotPoint coordinate system is
  aligned with the marker coordinate system's X axis (unless the Z axis of the PivotPoint coordinate system is parallel with the marker coordinate
  system's X axis; in this case the X axis of the PivotPoint coordinate system is aligned with the marker coordinate system's Y axis). The Y axis
  of the PivotPoint coordinate system is chosen to be the cross product of the Z and X axes.

  The method detects outlier points (points that have larger than 3x error than the standard deviation) and ignores them when computing the pivot point
  coordinates and the calibration error.

  \ingroup igsioCalibrationAlgorithm
*/
class VTKIGSIOCALIBRATION_EXPORT vtkIGSIOPivotCalibrationAlgo : public vtkIGSIOAbstractStylusCalibrationAlgo
{
public:
  vtkTypeMacro(vtkIGSIOPivotCalibrationAlgo, vtkIGSIOAbstractStylusCalibrationAlgo);
  static vtkIGSIOPivotCalibrationAlgo* New();

  /*!
  * Read configuration
  * \param aConfig Root element of the device set configuration
  */
  igsioStatus ReadConfiguration(vtkXMLDataElement* aConfig) override;

  /*!
    Calibrate (call the minimizer and set the result)
    \param aTransformRepository Transform repository to save the results into
  */
  igsioStatus DoPivotCalibration(vtkIGSIOTransformRepository* aTransformRepository = NULL, bool autoOrient = true);

  /*!
    Get calibration result string to display
    \param aPrecision Number of decimals shown
    \return Calibration result (e.g. stylus tip to stylus translation) string
  */
  std::string GetPivotPointToMarkerTranslationString(double aPrecision = 3);

  //@{
  /// Mean error of the pivot calibration result in mm.
  /// A value of -1.0 means that the error has not been calculated.
  vtkGetMacro(PivotCalibrationErrorMm, double);
  //@}

protected:
  vtkIGSIOPivotCalibrationAlgo();
  virtual ~vtkIGSIOPivotCalibrationAlgo();

protected:

  ////@{
  /// Compute the mean position error of the pivot point (in mm)
  void ComputePivotCalibrationError();
  double ComputePivotCalibrationError(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, std::set<unsigned int>& outlierIndices, double* pivotPoint_Reference, vtkMatrix4x4* pivotPointToMarkerTransformMatrix);
  ////@}

  igsioStatus GetPivotPointPosition(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, std::set<unsigned int>& outlierIndices, double* pivotPoint_Marker, double* pivotPoint_Reference);

  igsioStatus DoCalibrationInternal(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, double& error) override;
  igsioStatus DoPivotCalibrationInternal(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, bool autoOrient, std::set<unsigned int>& outlierIndices, double pivotPoint_Marker[4], double pivotPoint_Reference[4], vtkMatrix4x4* pivotPointToMarkerTransformMatrix);

protected:
  double PivotCalibrationErrorMm;

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

/*=IGSIO=header=begin======================================================
Program: IGSIO
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================IGSIO=header=end*/

#ifndef __vtkIGSIOSpinCalibrationAlgo_h
#define __vtkIGSIOSpinCalibrationAlgo_h

// Local includes
#include "igsioConfigure.h"
#include "igsioCommon.h"
#include "vtkIGSIOAbstractStylusCalibrationAlgo.h"

// VTK includes
#include <vtkCommand.h>
#include <vtkObject.h>
#include <vtkMatrix4x4.h>

class vtkIGSIOTransformRepository;
class vtkXMLDataElement;

//-----------------------------------------------------------------------------

/*!
  \class vtkIGSIOSpinCalibrationAlgo
  \brief Spin calibration algorithm to calibrate a stylus. It determines the direction of the stylus shaft.

  While pivot calibration can find the position of the stylus tip relative to the stylus tracker, it doesn't find the rotation component of the stylus shaft.
  Spin calibration calculates the rotation neccisary so that the stylus shaft points along the -Z axis direction in tye StylusTip coordinate system.

  The stylus shaft is found by minimizing the distance from the ideal axis of rotation to the axis of rotation for each instantaneous rotation.

  \ingroup igsioCalibrationAlgorithm
*/
class VTKIGSIOCALIBRATION_EXPORT vtkIGSIOSpinCalibrationAlgo : public vtkIGSIOAbstractStylusCalibrationAlgo
{
public:
  vtkTypeMacro(vtkIGSIOSpinCalibrationAlgo, vtkIGSIOAbstractStylusCalibrationAlgo);
  static vtkIGSIOSpinCalibrationAlgo* New();

  /// Read configuration
  /// \param aConfig Root element of the device set configuration
  igsioStatus ReadConfiguration(vtkXMLDataElement* aConfig) override;

  /// Calibrate (call the minimizer and set the result)
  /// \param aTransformRepository Transform repository to save the results into
  igsioStatus DoSpinCalibration(vtkIGSIOTransformRepository* aTransformRepository = NULL, bool snapRotation = false, bool autoOrient = true);

  /// Mean error of the pivot calibration result in mm
  /// A value of -1.0 means that the error has not been calculated.
  vtkGetMacro(SpinCalibrationErrorMm, double);

protected:
  vtkIGSIOSpinCalibrationAlgo();
  virtual ~vtkIGSIOSpinCalibrationAlgo();

protected:
  igsioStatus DoCalibrationInternal(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, double& error) override;
  igsioStatus DoSpinCalibrationInternal(const std::vector<vtkMatrix4x4*>& markerToTransformMatrixArray, bool snapRotation, bool autoOrient, vtkMatrix4x4* pivotPointToMarkerTransformMatrix, double& error);

protected:
  double SpinCalibrationErrorMm;

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

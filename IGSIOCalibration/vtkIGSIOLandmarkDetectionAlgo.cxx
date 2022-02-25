/*=IGSIO=header=begin======================================================
Program: IGSIO
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================IGSIO=header=end*/

#include "igsioConfigure.h"
#include "igsioXmlUtils.h"
#include "igsioMath.h"

#include "vtkIGSIOLandmarkDetectionAlgo.h"

// VTK includes
#include "vtkMatrix4x4.h"
#include <vtkObjectFactory.h>

vtkStandardNewMacro( vtkIGSIOLandmarkDetectionAlgo );

//-----------------------------------------------------------------------------
// Default algorithm parameters
static const double PERCENTAGE_WINDOWS_SKIP = 0.1;//The first windows detected as pivoting wont be used for averaging.

//-----------------------------------------------------------------------------
vtkIGSIOLandmarkDetectionAlgo::vtkIGSIOLandmarkDetectionAlgo()
{
  this->DetectedLandmarkPoints_Reference = NULL;
  this->SetDetectedLandmarkPoints_Reference( vtkSmartPointer<vtkPoints>::New() );

  this->AcquisitionRate = 20.0;
  this->FilterWindowTimeSec = 0.2;
  this->DetectionTimeSec = 1.0;
  this->StylusShaftMinimumDisplacementThresholdMm = 30;
  this->StylusTipMaximumDisplacementThresholdMm = 1.5;
  this->MinimumDistanceBetweenLandmarksMm = 15.0;
}

//-----------------------------------------------------------------------------
vtkIGSIOLandmarkDetectionAlgo::~vtkIGSIOLandmarkDetectionAlgo()
{
  this->StylusTipToReferenceTransformsDeque.clear();
  this->SetDetectedLandmarkPoints_Reference( NULL );
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::SetAcquisitionRate( double aquisitionRateSamplePerSec )
{
  if( aquisitionRateSamplePerSec <= 0 )
  {
    LOG_ERROR( "Specified acquisition rate " << aquisitionRateSamplePerSec << "[SamplePerSec]is not positive" );
    return IGSIO_FAIL;
  }
  this->AcquisitionRate = aquisitionRateSamplePerSec;
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::SetDetectionTimeSec( double detectionTimeSec )
{
  if( detectionTimeSec <= 0 )
  {
    LOG_ERROR( "Specified detection time (" << detectionTimeSec << " [s]) is not positive" );
    return IGSIO_FAIL;
  }
  this->DetectionTimeSec = detectionTimeSec;
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::SetFilterWindowTimeSec( double filterWindowTimeSec )
{
  if( filterWindowTimeSec <= 0 )
  {
    LOG_ERROR( "Specified window time (" << filterWindowTimeSec << " [s]) is not positive" );
    return IGSIO_FAIL;
  }
  this->FilterWindowTimeSec = filterWindowTimeSec;
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::SetStylusShaftMinimumDisplacementThresholdMm( double stylusShaftMinimumDisplacementThresholdMm )
{
  if( stylusShaftMinimumDisplacementThresholdMm < 0 )
  {
    LOG_ERROR( "Specified stylusShaftMinimumDisplacementThreshold (" << stylusShaftMinimumDisplacementThresholdMm << " [mm]) is negative" );
    return IGSIO_FAIL;
  }
  this->StylusShaftMinimumDisplacementThresholdMm = stylusShaftMinimumDisplacementThresholdMm;
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::SetStylusTipMaximumDisplacementThresholdMm( double stylusTipMaximumMotionThresholdMm )
{
  if( stylusTipMaximumMotionThresholdMm < 0 )
  {
    LOG_ERROR( "Specified StylusTipMaximumDisplacementThreshold (" << stylusTipMaximumMotionThresholdMm << " [mm]) is negative" );
    return IGSIO_FAIL;
  }
  this->StylusTipMaximumDisplacementThresholdMm = stylusTipMaximumMotionThresholdMm;
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::ComputeFilterWindowSize( unsigned int& filterWindowSize )
{
  if( this->AcquisitionRate <= 0 )
  {
    LOG_ERROR( "Specified acquisition rate is not positive" );
    return IGSIO_FAIL;
  }
  filterWindowSize = static_cast<unsigned int>( igsioMath::Round( this->AcquisitionRate * this->FilterWindowTimeSec ) );
  if( filterWindowSize < 1 )
  {
    LOG_WARNING( "The smallest window size is set to 1" );
    filterWindowSize = 1;
  }
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::ComputeNumberOfWindows( unsigned int& numberOfWindows )
{
  if( this->FilterWindowTimeSec <= 0 || this->DetectionTimeSec <= this->FilterWindowTimeSec )
  {
    LOG_ERROR( "Detection (" << this->DetectionTimeSec << " [s]) or filter window (" << this->FilterWindowTimeSec << " [s]) times are not correct" );
    return IGSIO_FAIL;
  }

  numberOfWindows = this->DetectionTimeSec / this->FilterWindowTimeSec;
  return IGSIO_SUCCESS;
}

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::ResetDetection()
{
  LOG_DEBUG( "Reset" );
  this->StylusTipToReferenceTransformsDeque.clear();
  this->DetectedLandmarkPoints_Reference->Reset();
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::DeleteLastLandmark()
{
  if( this->DetectedLandmarkPoints_Reference->GetNumberOfPoints() <= 0 )
  {
    LOG_ERROR( "There were no landmark detected" );
    return IGSIO_FAIL;
  }
  this->DetectedLandmarkPoints_Reference->GetData()->RemoveTuple( this->DetectedLandmarkPoints_Reference->GetNumberOfPoints() - 1 );
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::InsertLandmark_Reference( double stylusTipPosition_Reference[4] )
{
  int existingLandmarkId = GetNearExistingLandmarkId( stylusTipPosition_Reference );
  if( existingLandmarkId != -1 )
  {
    LOG_INFO( "No landmark inserted, landmark #" << existingLandmarkId << " was already detected" );
    return IGSIO_SUCCESS;
  }
  this->DetectedLandmarkPoints_Reference->InsertNextPoint( stylusTipPosition_Reference );
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::FilterStylusTipPositionsWindow( double stylusTipFiltered_Reference[4] )
{
  unsigned int filterWindowSize = 0;
  if ( ComputeFilterWindowSize( filterWindowSize ) == IGSIO_FAIL )
  {
    return IGSIO_FAIL;
  }

  if( this->StylusTipToReferenceTransformsDeque.size() < filterWindowSize )
  {
    LOG_ERROR( "There are not enough stylus tip positions acquired yet" );
    return IGSIO_FAIL;
  }

  std::deque< vtkSmartPointer<vtkMatrix4x4> >::iterator stylusTipToReferenceTransformIt = this->StylusTipToReferenceTransformsDeque.end();
  double stylusTipPositionSum_Reference[4] = {0, 0, 0, 1};
  for( unsigned int i = 0; i < filterWindowSize; i++ )
  {
    stylusTipToReferenceTransformIt--;
    stylusTipPositionSum_Reference[0] += ( *stylusTipToReferenceTransformIt )->Element[0][3];
    stylusTipPositionSum_Reference[1] += ( *stylusTipToReferenceTransformIt )->Element[1][3];
    stylusTipPositionSum_Reference[2] += ( *stylusTipToReferenceTransformIt )->Element[2][3];
  }
  //make sure we don't divide by zero
  if( filterWindowSize == 0 )
  {
    LOG_ERROR( "Filter window size is zero can not be used, how does it happened!" );
    return IGSIO_FAIL;
  }
  stylusTipFiltered_Reference[0] = stylusTipPositionSum_Reference[0] / filterWindowSize;
  stylusTipFiltered_Reference[1] = stylusTipPositionSum_Reference[1] / filterWindowSize;
  stylusTipFiltered_Reference[2] = stylusTipPositionSum_Reference[2] / filterWindowSize;
  stylusTipFiltered_Reference[3] = 1.0;
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::InsertNextStylusTipToReferenceTransform( vtkMatrix4x4* stylusTipToReferenceTransform, int& newLandmarkDetected )
{
  newLandmarkDetected = -1;
  if ( stylusTipToReferenceTransform == NULL )
  {
    LOG_ERROR( "stylusTipToReferenceTransform is NULL" );
    return IGSIO_FAIL;
  }

  this->StylusTipToReferenceTransformsDeque.push_back( stylusTipToReferenceTransform );

  unsigned int filterWindowSize = 0;
  unsigned int numberOfRequiredWindows = 0;
  if ( ComputeFilterWindowSize( filterWindowSize ) == IGSIO_FAIL || ComputeNumberOfWindows( numberOfRequiredWindows ) == IGSIO_FAIL )
  {
    return IGSIO_FAIL;
  }

  bool windowCompleted = ( this->StylusTipToReferenceTransformsDeque.size() % filterWindowSize ) == 0;
  if ( !windowCompleted )
  {
    // just keep collecting more data
    return IGSIO_SUCCESS;
  }

  // We've just completed a new window

  // Update StylusTipPathBoundingBox
  double stylusTipFiltered_Reference[4] = {0, 0, 0, 1}; // stylus tip position in the last window
  if ( FilterStylusTipPositionsWindow( stylusTipFiltered_Reference ) != IGSIO_SUCCESS )
  {
    return IGSIO_FAIL;
  }
  this->StylusTipPathBoundingBox.AddPoint( stylusTipFiltered_Reference );

  // Update StylusShaftPathBoundingBox
  //Point 10 cm above the stylus tip, if it moves(window change bigger than AboveLandmarkThresholdMm) while the tip is static (window change smaller than LandmarkThresholdMm then it is landmark point.
  double StylusShaftPoint_StylusTip[4] = {100, 0, 0, 1};
  double StylusShaftPoint_Reference[4] = {0, 0, 0, 1};
  stylusTipToReferenceTransform->MultiplyPoint( StylusShaftPoint_StylusTip, StylusShaftPoint_Reference );
  this->StylusShaftPathBoundingBox.AddPoint( StylusShaftPoint_Reference );

  unsigned int numberOfAcquiredWindows = igsioMath::Floor( this->StylusTipToReferenceTransformsDeque.size() / filterWindowSize );

  // Print all available StylusTipToReferenceTransform positions if debug level is TRACE
  bool debugOutput = vtkIGSIOLogger::Instance()->GetLogLevel() >= vtkIGSIOLogger::LOG_LEVEL_TRACE;
  if ( debugOutput )
  {
    LOG_TRACE( "Window Landmark (" << stylusTipFiltered_Reference[0] << ", " << stylusTipFiltered_Reference[1] << ", " << stylusTipFiltered_Reference[2] << ") found keep going" );
    int i = 0;
    for ( std::deque< vtkSmartPointer<vtkMatrix4x4> >::iterator markerToReferenceTransformIt = this->StylusTipToReferenceTransformsDeque.begin();
          markerToReferenceTransformIt != this->StylusTipToReferenceTransformsDeque.end(); ++markerToReferenceTransformIt )
    {
      if( i % filterWindowSize == 0 )
      {
        LOG_TRACE( "Window " << ( i / filterWindowSize ) << ":" );
      }
      LOG_TRACE( "P( " << -( *markerToReferenceTransformIt )->Element[0][3] << ", " << -( *markerToReferenceTransformIt )->Element[1][3] << ", " << -( *markerToReferenceTransformIt )->Element[2][3] << ")" );
      i++;
    }
  }

  // If tip is moved then clear transforms and start detection of the latest landmark from scratch
  double stylusTipPathBoundingBoxSize[3] = {0};
  this->StylusTipPathBoundingBox.GetLengths( stylusTipPathBoundingBoxSize );
  if( vtkMath::Norm( stylusTipPathBoundingBoxSize ) > this->StylusTipMaximumDisplacementThresholdMm )
  {
    LOG_TRACE( "StylusTip has moved: StylusTipBoundingBox norm = " << vtkMath::Norm( stylusTipPathBoundingBoxSize ) );
    this->StylusTipToReferenceTransformsDeque.clear();
    this->StylusShaftPathBoundingBox.Reset();
    this->StylusTipPathBoundingBox.Reset();
    return IGSIO_SUCCESS;
  }

  // If enough rotation range has been covered and we collected enough windows then we accept this as a new landmark point
  if( numberOfAcquiredWindows < numberOfRequiredWindows )
  {
    // not enough windows yet
    // just keep collecting more data
    return IGSIO_SUCCESS;
  }

  double stylusShaftPathBoundingBoxSize[3] = {0};
  this->StylusShaftPathBoundingBox.GetLengths( stylusShaftPathBoundingBoxSize );
  if( vtkMath::Norm( stylusShaftPathBoundingBoxSize ) < this->StylusShaftMinimumDisplacementThresholdMm )
  {
    // not enough pivoting yet
    // just keep collecting more data
    return IGSIO_SUCCESS;
  }

  // We've detected a new landmark point!
  int numberOfLandmarksBefore = this->DetectedLandmarkPoints_Reference->GetNumberOfPoints();

  EstimateLandmarkPosition();
  this->StylusTipPathBoundingBox.Reset();
  this->StylusShaftPathBoundingBox.Reset();
  this->StylusTipToReferenceTransformsDeque.clear();

  if( numberOfLandmarksBefore != this->DetectedLandmarkPoints_Reference->GetNumberOfPoints() )
  {
    newLandmarkDetected = this->DetectedLandmarkPoints_Reference->GetNumberOfPoints();
  }
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
int vtkIGSIOLandmarkDetectionAlgo::GetNearExistingLandmarkId( double stylusTipPosition_Reference[4] )
{
  double detectedLandmark_Reference[4] = {0, 0, 0, 1};
  double landmarkDifference_Reference[4] = {0, 0, 0, 0};
  for( int id = 0; id < this->DetectedLandmarkPoints_Reference->GetNumberOfPoints(); id++ )
  {
    this->DetectedLandmarkPoints_Reference->GetPoint( id, detectedLandmark_Reference );
    landmarkDifference_Reference[0] = detectedLandmark_Reference[0] - stylusTipPosition_Reference[0];
    landmarkDifference_Reference[1] = detectedLandmark_Reference[1] - stylusTipPosition_Reference[1];
    landmarkDifference_Reference[2] = detectedLandmark_Reference[2] - stylusTipPosition_Reference[2];
    landmarkDifference_Reference[3] = detectedLandmark_Reference[3] - stylusTipPosition_Reference[3];

    // If the distance between landmarks is X then if we are X / 3 distance from a landmark we can be quite confident
    // that we are actually at that landmark (other landmarks should be farther, at about X * 2 / 3 distance).
    // If we used X / 2 as distance threshold then we may accept a point that is halfway between two landmarks.
    if( vtkMath::Norm( landmarkDifference_Reference ) < this->MinimumDistanceBetweenLandmarksMm / 3 )
    {
      return id;
    }
  }
  return -1;
}

//----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::EstimateLandmarkPosition()
{
  unsigned int filterWindowSize = 0;
  unsigned int numberOfRequiredWindows = 0;
  if ( ComputeFilterWindowSize( filterWindowSize ) == IGSIO_FAIL || ComputeNumberOfWindows( numberOfRequiredWindows ) == IGSIO_FAIL )
  {
    return IGSIO_FAIL;
  }

  int numberOfWindowsSkip = filterWindowSize = igsioMath::Round( numberOfRequiredWindows * PERCENTAGE_WINDOWS_SKIP );
  if( filterWindowSize < 1 )
  {
    LOG_WARNING( "The smallest number of windows to skip is set to 1" );
    filterWindowSize = 1;
  }

  unsigned int i = 0;
  std::vector<double> values[4];
  for ( std::deque< vtkSmartPointer<vtkMatrix4x4> >::iterator stylusTipToReferenceTransformIt = this->StylusTipToReferenceTransformsDeque.begin();
        stylusTipToReferenceTransformIt != this->StylusTipToReferenceTransformsDeque.end(); ++stylusTipToReferenceTransformIt )
  {
    //Don't use first numberOfWindowsSkip windows
    if( i >= ( filterWindowSize * numberOfWindowsSkip ) && i < ( numberOfRequiredWindows )*filterWindowSize )
    {
      double sylusTip_Reference[3] = {( *stylusTipToReferenceTransformIt )->Element[0][3], ( *stylusTipToReferenceTransformIt )->Element[1][3], ( *stylusTipToReferenceTransformIt )->Element[2][3]};
      values[0].push_back( sylusTip_Reference[0] );
      values[1].push_back( sylusTip_Reference[1] );
      values[2].push_back( sylusTip_Reference[2] );
      values[3].push_back( vtkMath::Norm( sylusTip_Reference ) );
    }
    i++;
  }

  if( values[0].size() != ( numberOfRequiredWindows - numberOfWindowsSkip )*filterWindowSize )
  {
    LOG_ERROR( "Not right number of points to estimate landmark position" ); //LOG error and do something CLEAR?
    return IGSIO_FAIL;
  }

  double stylusTipMean_Reference[4] = {0, 0, 0, 1};
  double stylusTipStdev_Reference[4] = {0, 0, 0, 0};
  for ( int j = 0; j < 4; ++j )
  {
    igsioMath::ComputeMeanAndStdev( values[j], stylusTipMean_Reference[j], stylusTipStdev_Reference[j] );
  }
  int existingLandmarkId = GetNearExistingLandmarkId( stylusTipMean_Reference );
  if ( existingLandmarkId != -1 )
  {
    LOG_INFO( "Landmark #" << existingLandmarkId << " was already detected" );
    return IGSIO_SUCCESS;
  }
  this->DetectedLandmarkPoints_Reference->InsertNextPoint( stylusTipMean_Reference );

  LOG_DEBUG( "Stylus tip positions used for detection" );
  LOG_DEBUG( "Stylus tips STD deviation ( " << stylusTipStdev_Reference[0] << ", " << stylusTipStdev_Reference[1] << ", " << stylusTipStdev_Reference[2] << ") Norm = " << vtkMath::Norm( stylusTipStdev_Reference ) );
  LOG_DEBUG( "Stylus tips Magnitude STD deviation " << stylusTipStdev_Reference[3] );

  //compute the mean vector, compute the magnitude of error vector, get stats for this magnitude
  std::vector<double> sylusTipsToMeanVectorsMagnitude;
  for( unsigned int j = 0; j < values[0].size(); j++ )
  {
    double sylusTipToMeanVector[3] = {stylusTipMean_Reference[0] - values[0][j], stylusTipMean_Reference[1] - values[1][j], stylusTipMean_Reference[2] - values[2][j]};
    sylusTipsToMeanVectorsMagnitude.push_back( vtkMath::Norm( sylusTipToMeanVector ) );
  }

  double stylusVectorsMagnitudeMean = 0;
  double stylusVectorsMagnitudeStdev = 0;
  igsioMath::ComputeMeanAndStdev( sylusTipsToMeanVectorsMagnitude, stylusVectorsMagnitudeMean, stylusVectorsMagnitudeStdev );
  LOG_DEBUG( "Error magnitude = ||StylusTipsMean-StylusTip||" );
  LOG_DEBUG( "Error magnitude Mean = " << stylusVectorsMagnitudeMean );
  LOG_DEBUG( "Error magnitude STD deviation = " << stylusVectorsMagnitudeStdev );

  return IGSIO_SUCCESS;
}

//-----------------------------------------------------------------------------
std::string vtkIGSIOLandmarkDetectionAlgo::GetDetectedLandmarksString( double aPrecision/*=3*/ )
{
  if ( this->DetectedLandmarkPoints_Reference->GetNumberOfPoints() > 0 )
  {
    std::ostringstream s;
    double landmarkDetected_Reference[4] = {0, 0, 0, 1};
    s << std::fixed << std::setprecision( aPrecision );
    for ( int id = 0; id < this->DetectedLandmarkPoints_Reference->GetNumberOfPoints(); id++ )
    {
      this->DetectedLandmarkPoints_Reference->GetPoint( id, landmarkDetected_Reference );
      s << "Landmark " << id + 1 << " (" << landmarkDetected_Reference[0] << ", " << landmarkDetected_Reference[1] << ", " << landmarkDetected_Reference[2] << ")";
    }
    s << std::ends;
    return s.str();
  }
  else
  {
    return "\nNo landmarks found";
  }
}

//-----------------------------------------------------------------------------
igsioStatus vtkIGSIOLandmarkDetectionAlgo::ReadConfiguration( vtkXMLDataElement* aConfig )
{
  XML_FIND_NESTED_ELEMENT_REQUIRED( PhantomLandmarkLandmarkDetectionElement, aConfig, "vtkIGSIOPhantomLandmarkRegistrationAlgo" );
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL( double, AcquisitionRate, PhantomLandmarkLandmarkDetectionElement );

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL( double, FilterWindowTimeSec, PhantomLandmarkLandmarkDetectionElement );
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL( double, DetectionTimeSec, PhantomLandmarkLandmarkDetectionElement );
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL( double, StylusShaftMinimumDisplacementThresholdMm, PhantomLandmarkLandmarkDetectionElement );
  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL( double, StylusTipMaximumDisplacementThresholdMm, PhantomLandmarkLandmarkDetectionElement );

  unsigned int filterWindowSize = 0;
  unsigned int numberOfWindows = 0;
  if ( ComputeFilterWindowSize( filterWindowSize ) == IGSIO_FAIL || ComputeNumberOfWindows( numberOfWindows ) == IGSIO_FAIL )
  {
    return IGSIO_FAIL;
  }

  LOG_DEBUG( "AcquisitionRate = " << AcquisitionRate << "[fps] WindowTimeSec = " << FilterWindowTimeSec << "[s] DetectionTimeSec = " << DetectionTimeSec << "[s]" );
  LOG_DEBUG( "NumberOfWindows = " << numberOfWindows << " WindowSize = " << filterWindowSize << " MinimumDistanceBetweenLandmarksMm = " << MinimumDistanceBetweenLandmarksMm << "[mm] LandmarkThreshold " << StylusTipMaximumDisplacementThresholdMm << "[mm]" );

  return IGSIO_SUCCESS;
}

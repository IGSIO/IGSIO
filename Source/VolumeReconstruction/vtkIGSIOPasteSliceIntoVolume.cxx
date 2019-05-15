/*=IGSIO=header=begin======================================================
Program: IGSIO
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================IGSIO=header=end*/

/*=========================================================================
The following copyright notice is applicable to parts of this file:

Copyright (c) 2000-2007 Atamai, Inc.
Copyright (c) 2008-2009 Danielle Pace

Use, modification and redistribution of the software, in source or
binary forms, are permitted provided that the following terms and
conditions are met:

1) Redistribution of the source code, in verbatim or modified
form, must retain the above copyright notice, this license,
the following disclaimer, and any notices that refer to this
license and/or the following disclaimer.

2) Redistribution in binary form must include the above copyright
notice, a copy of this license and the following disclaimer
in the documentation or with other materials provided with the
distribution.

3) Modified copies of the source code must be clearly marked as such,
and must not be misrepresented as verbatim copies of the source code.

THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES PROVIDE THE SOFTWARE "AS IS"
WITHOUT EXPRESSED OR IMPLIED WARRANTY INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE.  IN NO EVENT SHALL ANY COPYRIGHT HOLDER OR OTHER PARTY WHO MAY
MODIFY AND/OR REDISTRIBUTE THE SOFTWARE UNDER THE TERMS OF THIS LICENSE
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, LOSS OF DATA OR DATA BECOMING INACCURATE
OR LOSS OF PROFIT OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF
THE USE OR INABILITY TO USE THE SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGES.

=========================================================================*/

#include "igsioConfigure.h"

#include <vtkObjectFactory.h>
#include "vtkImageData.h"
#include "vtkIndent.h"
#include "vtkMath.h"
#include "vtkMultiThreader.h"
#include "vtkTransform.h"
#include "vtkXMLUtilities.h"
#include "vtkXMLDataElement.h"

#include "vtkIGSIOPasteSliceIntoVolume.h"
#include "vtkIGSIOPasteSliceIntoVolumeHelperCommon.h"
#include "vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized.h"
#include "vtkIGSIOPasteSliceIntoVolumeHelperOptimized.h"

vtkStandardNewMacro( vtkIGSIOPasteSliceIntoVolume );

struct InsertSliceThreadFunctionInfoStruct
{
  vtkImageData* InputFrameImage;
  vtkMatrix4x4* TransformImageToReference;
  vtkImageData* OutputVolume;
  vtkImageData* Accumulator;
  vtkImageData* ImportanceImage;
  vtkIGSIOPasteSliceIntoVolume::OptimizationType Optimization;
  vtkIGSIOPasteSliceIntoVolume::InterpolationType InterpolationMode;
  vtkIGSIOPasteSliceIntoVolume::CompoundingType CompoundingMode;
  double PixelRejectionThreshold;

  double ClipRectangleOrigin[2];
  double ClipRectangleSize[2];
  double FanAnglesDeg[2];
  double FanOrigin[2];
  double FanRadiusStart;
  double FanRadiusStop;
  std::vector<unsigned int> AccumulationBufferSaturationErrors;
};

//----------------------------------------------------------------------------
vtkIGSIOPasteSliceIntoVolume::vtkIGSIOPasteSliceIntoVolume()
{
  this->ReconstructedVolume = vtkImageData::New();
  this->AccumulationBuffer = vtkImageData::New();
  this->ImportanceMask = NULL;
  this->Threader = vtkMultiThreader::New();

  this->OutputOrigin[0] = 0.0;
  this->OutputOrigin[1] = 0.0;
  this->OutputOrigin[2] = 0.0;

  this->OutputSpacing[0] = 1.0;
  this->OutputSpacing[1] = 1.0;
  this->OutputSpacing[2] = 1.0;

  // Set to invalid values by default
  // If the user doesn't set the correct values then inserting the slice will be refused
  this->OutputExtent[0] = 0;
  this->OutputExtent[1] = 0;
  this->OutputExtent[2] = 0;
  this->OutputExtent[3] = 0;
  this->OutputExtent[4] = 0;
  this->OutputExtent[5] = 0;

  this->ClipRectangleOrigin[0] = 0;
  this->ClipRectangleOrigin[1] = 0;
  this->ClipRectangleSize[0] = 0;
  this->ClipRectangleSize[1] = 0;

  this->FanAnglesDeg[0] = 0.0;
  this->FanAnglesDeg[1] = 0.0;
  this->FanOrigin[0] = 0.0;
  this->FanOrigin[1] = 0.0;
  this->FanRadiusStart = 0.0;
  this->FanRadiusStop = 500.0;

  // scalar type for input and output
  this->OutputScalarMode = VTK_UNSIGNED_CHAR;

  // reconstruction options
  this->InterpolationMode = NEAREST_NEIGHBOR_INTERPOLATION;
  this->Optimization = FULL_OPTIMIZATION;
  this->CompoundingMode = UNDEFINED_COMPOUNDING_MODE;

  this->NumberOfThreads = 0; // 0 means not set, the default number of threads will be used

  this->EnableAccumulationBufferOverflowWarning = true;

  // deprecated reconstruction options
  this->Compounding = -1;
  this->Calculation = UNDEFINED_CALCULATION;

  SetPixelRejectionDisabled();
}

//----------------------------------------------------------------------------
vtkIGSIOPasteSliceIntoVolume::~vtkIGSIOPasteSliceIntoVolume()
{
  if ( this->ReconstructedVolume )
  {
    this->ReconstructedVolume->Delete();
    this->ReconstructedVolume = NULL;
  }
  if ( this->AccumulationBuffer )
  {
    this->AccumulationBuffer->Delete();
    this->AccumulationBuffer = NULL;
  }
  this->SetImportanceMask(NULL);
  if ( this->Threader )
  {
    this->Threader->Delete();
    this->Threader = NULL;
  }
}

//----------------------------------------------------------------------------
void vtkIGSIOPasteSliceIntoVolume::PrintSelf( ostream& os, vtkIndent indent )
{
  this->Superclass::PrintSelf( os, indent );
  if ( this->ReconstructedVolume )
  {
    os << indent << "ReconstructedVolume:\n";
    this->ReconstructedVolume->PrintSelf( os, indent.GetNextIndent() );
  }
  if ( this->AccumulationBuffer )
  {
    os << indent << "AccumulationBuffer:\n";
    this->AccumulationBuffer->PrintSelf( os, indent.GetNextIndent() );
  }
  if (this->ImportanceMask)
  {
    os << indent << "ImportanceMask:\n";
    this->ImportanceMask->PrintSelf(os, indent.GetNextIndent());
  }
  os << indent << "OutputOrigin: " << this->OutputOrigin[0] << " " <<
     this->OutputOrigin[1] << " " << this->OutputOrigin[2] << "\n";
  os << indent << "OutputSpacing: " << this->OutputSpacing[0] << " " <<
     this->OutputSpacing[1] << " " << this->OutputSpacing[2] << "\n";
  os << indent << "OutputExtent: " << this->OutputExtent[0] << " " <<
     this->OutputExtent[1] << " " << this->OutputExtent[2] << " " <<
     this->OutputExtent[3] << " " << this->OutputExtent[4] << " " <<
     this->OutputExtent[5] << "\n";
  os << indent << "ClipRectangleOrigin: " << this->ClipRectangleOrigin[0] << " " <<
     this->ClipRectangleOrigin[1] << "\n";
  os << indent << "ClipRectangleSize: " << this->ClipRectangleSize[0] << " " <<
     this->ClipRectangleSize[1] << "\n";
  os << indent << "FanAnglesDeg: " << this->FanAnglesDeg[0] << " " <<
     this->FanAnglesDeg[1] << "\n";
  os << indent << "FanOrigin: " << this->FanOrigin[0] << " " <<
     this->FanOrigin[1] << "\n";
  os << indent << "FanRadiusStart: " << this->FanRadiusStart << "\n";
  os << indent << "FanRadiusStop: " << this->FanRadiusStop << "\n";
  os << indent << "InterpolationMode: " << this->GetInterpolationModeAsString( this->InterpolationMode ) << "\n";
  os << indent << "CompoundingMode: " << this->GetCompoundingModeAsString( this->CompoundingMode ) << "\n";
  os << indent << "Optimization: " << this->GetOptimizationModeAsString( this->Optimization ) << "\n";
  os << indent << "NumberOfThreads: ";
  if ( this->NumberOfThreads > 0 )
  {
    os << this->NumberOfThreads << "\n";
  }
  else
  {
    os << "default\n";
  }
}


//----------------------------------------------------------------------------
vtkImageData* vtkIGSIOPasteSliceIntoVolume::GetReconstructedVolume()
{
  return this->ReconstructedVolume;
}

//----------------------------------------------------------------------------
vtkImageData* vtkIGSIOPasteSliceIntoVolume::GetAccumulationBuffer()
{
  return this->AccumulationBuffer;
}

//----------------------------------------------------------------------------
// Clear the output volume and the accumulation buffer
igsioStatus vtkIGSIOPasteSliceIntoVolume::ResetOutput()
{
  // Allocate memory for accumulation buffer and set all pixels to 0
  // Start with this buffer because if no compunding is needed then we release memory before allocating memory for the reconstructed image.

  vtkImageData* accData = this->GetAccumulationBuffer();
  if ( accData == NULL )
  {
    LOG_ERROR( "Accumulation buffer object is not created" );
    return IGSIO_FAIL;
  }
  int accExtent[6];
  // we do compunding, so we need to have an accumulation buffer with the same size as the output image
  for ( int i = 0; i < 6; i++ )
  {
    accExtent[i] = this->OutputExtent[i];
  }

  accData->SetExtent( accExtent );
  accData->SetOrigin( this->OutputOrigin );
  accData->SetSpacing( this->OutputSpacing );
  accData->AllocateScalars( VTK_UNSIGNED_SHORT, 1 );

  void* accPtr = accData->GetScalarPointerForExtent( accExtent );
  if ( accPtr == NULL )
  {
    LOG_ERROR( "Cannot allocate memory for accumulation image extent: " << accExtent[1] - accExtent[0] << "x" << accExtent[3] - accExtent[2] << " x " << accExtent[5] - accExtent[4] );
  }
  else
  {
    memset( accPtr, 0, ( size_t( accExtent[1] - accExtent[0] + 1 ) *
                         size_t( accExtent[3] - accExtent[2] + 1 ) *
                         size_t( accExtent[5] - accExtent[4] + 1 ) *
                         accData->GetScalarSize()*accData->GetNumberOfScalarComponents() ) );
  }
  // Allocate memory for the reconstructed image and set all pixels to 0

  vtkImageData* outData = this->ReconstructedVolume;
  if ( outData == NULL )
  {
    LOG_ERROR( "Output image object is not created" );
    return IGSIO_FAIL;
  }

  int* outExtent = this->OutputExtent;
  outData->SetExtent( outExtent );
  outData->SetOrigin( this->OutputOrigin );
  outData->SetSpacing( this->OutputSpacing );
  outData->AllocateScalars( this->OutputScalarMode, 1 );

  void* outPtr = outData->GetScalarPointerForExtent( outExtent );
  if ( outPtr == NULL )
  {
    LOG_ERROR( "Cannot allocate memory for output image extent: " << outExtent[1] - outExtent[0] << "x" << outExtent[3] - outExtent[2] << " x " << outExtent[5] - outExtent[4] );
    return IGSIO_FAIL;
  }
  else
  {
    memset( outPtr, 0, ( size_t( outExtent[1] - outExtent[0] + 1 ) *
                         size_t( outExtent[3] - outExtent[2] + 1 ) *
                         size_t( outExtent[5] - outExtent[4] + 1 ) *
                         outData->GetScalarSize()*outData->GetNumberOfScalarComponents() ) );
  }

  return IGSIO_SUCCESS;
}

//****************************************************************************
// RECONSTRUCTION - OPTIMIZED
//****************************************************************************

//----------------------------------------------------------------------------
// Does the actual work of optimally inserting a slice, with optimization
// Basically, just calls Multithread()
igsioStatus vtkIGSIOPasteSliceIntoVolume::InsertSlice( vtkImageData* image, vtkMatrix4x4* transformImageToReference )
{
  if ( this->OutputExtent[0] >= this->OutputExtent[1]
       && this->OutputExtent[2] >= this->OutputExtent[3]
       && this->OutputExtent[4] >= this->OutputExtent[5] )
  {
    LOG_ERROR( "Invalid output volume extent [" << this->OutputExtent[0] << "," << this->OutputExtent[1] << ","
               << this->OutputExtent[2] << "," << this->OutputExtent[3] << "," << this->OutputExtent[4] << "," << this->OutputExtent[5] << "]."
               << " Cannot insert slice into the volume. Set the correct output volume origin, spacing, and extent before inserting slices." );
    return IGSIO_FAIL;
  }

  InsertSliceThreadFunctionInfoStruct str;
  str.InputFrameImage = image;
  str.TransformImageToReference = transformImageToReference;
  str.OutputVolume = this->ReconstructedVolume;
  str.Accumulator = this->AccumulationBuffer;
  str.ImportanceImage = this->ImportanceMask;
  str.InterpolationMode = this->InterpolationMode;
  str.CompoundingMode = this->CompoundingMode;
  str.Optimization = this->Optimization;
  if ( this->ClipRectangleSize[0] > 0 && this->ClipRectangleSize[1] > 0 )
  {
    // ClipRectangle specified
    str.ClipRectangleOrigin[0] = this->ClipRectangleOrigin[0];
    str.ClipRectangleOrigin[1] = this->ClipRectangleOrigin[1];
    str.ClipRectangleSize[0] = this->ClipRectangleSize[0];
    str.ClipRectangleSize[1] = this->ClipRectangleSize[1];
  }
  else
  {
    // ClipRectangle not specified, use full image slice
    str.ClipRectangleOrigin[0] = image->GetExtent()[0];
    str.ClipRectangleOrigin[1] = image->GetExtent()[2];
    str.ClipRectangleSize[0] = image->GetExtent()[1];
    str.ClipRectangleSize[1] = image->GetExtent()[3];
  }
  str.FanAnglesDeg[0] = this->FanAnglesDeg[0];
  str.FanAnglesDeg[1] = this->FanAnglesDeg[1];
  str.FanOrigin[0] = this->FanOrigin[0];
  str.FanOrigin[1] = this->FanOrigin[1];
  str.FanRadiusStart = this->FanRadiusStart;
  str.FanRadiusStop = this->FanRadiusStop;

  str.PixelRejectionThreshold = this->PixelRejectionThreshold;

  if ( this->NumberOfThreads > 0 )
  {
    this->Threader->SetNumberOfThreads( this->NumberOfThreads );
  }

  // initialize array that counts the number of insertion errors due to overflow in the accumulation buffer
  int numThreads( this->Threader->GetNumberOfThreads() );
  str.AccumulationBufferSaturationErrors.resize( numThreads );
  str.AccumulationBufferSaturationErrors.clear();
  for ( int i = 0; i < numThreads; i++ )
  {
    str.AccumulationBufferSaturationErrors.push_back( 0 );
  }

  this->Threader->SetSingleMethod( InsertSliceThreadFunction, &str );
  this->Threader->SingleMethodExecute();

  // sum up str.AccumulationBufferSaturationErrors
  unsigned int sumAccOverflowErrors( 0 );
  for ( int i = 0; i < numThreads; i++ )
  {
    sumAccOverflowErrors += str.AccumulationBufferSaturationErrors[i];
  }
  if ( sumAccOverflowErrors && !EnableAccumulationBufferOverflowWarning )
  {
    LOG_WARNING( sumAccOverflowErrors << " voxels have had too many pixels inserted. This can result in errors in the final volume. It is recommended that the output volume resolution be increased." );
  }

  this->ReconstructedVolume->Modified();
  this->AccumulationBuffer->Modified();
  this->Modified();

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
VTK_THREAD_RETURN_TYPE vtkIGSIOPasteSliceIntoVolume::InsertSliceThreadFunction( void* arg )
{
  vtkMultiThreader::ThreadInfo* threadInfo = static_cast<vtkMultiThreader::ThreadInfo*>( arg );
  InsertSliceThreadFunctionInfoStruct* str = static_cast<InsertSliceThreadFunctionInfoStruct*>( threadInfo->UserData );

  // Compute what extent of the input image will be processed by this thread
  int threadId = threadInfo->ThreadID;
  int threadCount = threadInfo->NumberOfThreads;
  int inputFrameExtent[6];
  str->InputFrameImage->GetExtent( inputFrameExtent );
  unsigned char *importancePtr = NULL;
  int inputFrameExtentForCurrentThread[6] = { 0, -1, 0, -1, 0, -1 };

  int totalUsedThreads = vtkIGSIOPasteSliceIntoVolume::SplitSliceExtent(inputFrameExtentForCurrentThread, inputFrameExtent, threadId, threadCount);

  if (threadId >= totalUsedThreads)
  {
    // don't use this thread. Sometimes the threads dont
    // break up very well and it is just as efficient to leave a
    // few threads idle.
    return VTK_THREAD_RETURN_VALUE;
  }

  if (str->CompoundingMode == IMPORTANCE_MASK_COMPOUNDING_MODE)
  {
    if (!str->ImportanceImage)
    {
      LOG_ERROR( "OptimizedInsertSlice: IMPORTANCE_MASK_COMPOUNDING_MODE was selected but importance mask has not been defined" );
      return VTK_THREAD_RETURN_VALUE;
    }
    int importanceMaskExtent[6];
    str->ImportanceImage->GetExtent( importanceMaskExtent );
    for (int i = 0; i < 6; i++)
    {
      if (inputFrameExtent[i]!=importanceMaskExtent[i])
      {
        LOG_ERROR("OptimizedInsertSlice: input frame extent ["
        << inputFrameExtent[0] << ", " << inputFrameExtent[1] << ", " << inputFrameExtent[2]<<", "
        << inputFrameExtent[3] << ", " << inputFrameExtent[4] << ", " << inputFrameExtent[5]<<"]"
        " does not match importance mask extent ["
        << importanceMaskExtent[0] << ", " << importanceMaskExtent[1] << ", " << importanceMaskExtent[2]<<", "
        << importanceMaskExtent[3] << ", " << importanceMaskExtent[4] << ", " << importanceMaskExtent[5]<<"]");
        return VTK_THREAD_RETURN_VALUE;
      }
    }
    if (str->ImportanceImage->GetNumberOfScalarComponents() != 1)
    {
      LOG_ERROR("OptimizedInsertSlice: number of scalar components in importance mask is invalid (1 expected, actual value is "
        << str->ImportanceImage->GetNumberOfScalarComponents() << ")");
      return VTK_THREAD_RETURN_VALUE;
    }
    if (str->ImportanceImage->GetScalarType() != VTK_UNSIGNED_CHAR)
    {
      LOG_ERROR( "OptimizedInsertSlice: importance mask extent must have unsigned char scalar type");
      return VTK_THREAD_RETURN_VALUE;
    }
    importancePtr = static_cast<unsigned char*>(str->ImportanceImage->GetScalarPointerForExtent(inputFrameExtentForCurrentThread));
  }

  // this filter expects that input is the same type as output.
  if ( str->InputFrameImage->GetScalarType() != str->OutputVolume->GetScalarType() )
  {
    LOG_ERROR( "OptimizedInsertSlice: input ScalarType (" << str->InputFrameImage->GetScalarType() << ") "
               << " must match out ScalarType (" << str->OutputVolume->GetScalarType() << ")" );
    return VTK_THREAD_RETURN_VALUE;
  }

  // Get input frame extent and pointer
  vtkImageData* inData = str->InputFrameImage;
  void* inPtr = inData->GetScalarPointerForExtent( inputFrameExtentForCurrentThread );

  // Get output volume extent and pointer
  vtkImageData* outData = str->OutputVolume;
  int* outExt = outData->GetExtent();
  void* outPtr = outData->GetScalarPointerForExtent( outExt );

  if (str->Accumulator->GetScalarType() != VTK_UNSIGNED_SHORT || str->Accumulator->GetNumberOfScalarComponents() != 1)
  {
    LOG_ERROR( "OptimizedInsertSlice: accumulator must have unsigned short scalar type and 1 component");
    return VTK_THREAD_RETURN_VALUE;
  }
  unsigned short* accPtr = static_cast<unsigned short*>(str->Accumulator->GetScalarPointerForExtent(outExt));

  // count the number of accumulation buffer overflow instances in the memory address here:
  unsigned int* accumulationBufferSaturationErrorsThread = &( str->AccumulationBufferSaturationErrors[threadId] );

  // Transform chain:
  // ImagePixToVolumePix =
  //  = VolumePixFromImagePix
  //  = VolumePixFromRef * RefFromImage * ImageFromImagePix

  vtkSmartPointer<vtkTransform> tVolumePixFromRef = vtkSmartPointer<vtkTransform>::New();
  tVolumePixFromRef->Translate( str->OutputVolume->GetOrigin() );
  tVolumePixFromRef->Scale( str->OutputVolume->GetSpacing() );
  tVolumePixFromRef->Inverse();

  vtkSmartPointer<vtkTransform> tRefFromImage = vtkSmartPointer<vtkTransform>::New();
  tRefFromImage->SetMatrix( str->TransformImageToReference );

  vtkSmartPointer<vtkTransform> tImageFromImagePix = vtkSmartPointer<vtkTransform>::New();
  tImageFromImagePix->Scale( str->InputFrameImage->GetSpacing() );

  vtkSmartPointer<vtkTransform> tImagePixToVolumePix = vtkSmartPointer<vtkTransform>::New();
  tImagePixToVolumePix->Concatenate( tVolumePixFromRef );
  tImagePixToVolumePix->Concatenate( tRefFromImage );
  tImagePixToVolumePix->Concatenate( tImageFromImagePix );

  vtkSmartPointer<vtkMatrix4x4> mImagePixToVolumePix = vtkSmartPointer<vtkMatrix4x4>::New();
  tImagePixToVolumePix->GetMatrix( mImagePixToVolumePix );


  // set up all the info for passing into the appropriate insertSlice function
  vtkIGSIOPasteSliceIntoVolumeInsertSliceParams insertionParams;
  insertionParams.accOverflowCount = accumulationBufferSaturationErrorsThread;
  insertionParams.accPtr = accPtr;
  insertionParams.importanceMask = str->ImportanceImage;
  insertionParams.importancePtr = importancePtr;
  insertionParams.compoundingMode = str->CompoundingMode;
  insertionParams.clipRectangleOrigin = str->ClipRectangleOrigin;
  insertionParams.clipRectangleSize = str->ClipRectangleSize;
  insertionParams.fanAnglesDeg = str->FanAnglesDeg;
  insertionParams.fanRadiusStart = str->FanRadiusStart;
  insertionParams.fanRadiusStop = str->FanRadiusStop;
  insertionParams.fanOrigin = str->FanOrigin;
  insertionParams.inData = str->InputFrameImage;
  insertionParams.inExt = inputFrameExtentForCurrentThread;
  insertionParams.inPtr = inPtr;
  insertionParams.interpolationMode = str->InterpolationMode;
  insertionParams.outData = outData;
  insertionParams.outPtr = outPtr;
  insertionParams.pixelRejectionThreshold = str->PixelRejectionThreshold;
  // the matrix will be set once we know more about the optimization level

  if ( str->Optimization == FULL_OPTIMIZATION )
  {
    // use fixed-point math
    // change transform matrix so that instead of taking
    // input coords -> output coords it takes output indices -> input indices
    fixed newmatrix[16]; // fixed because optimization = 2
    for ( int i = 0; i < 4; i++ )
    {
      int rowindex = ( i << 2 );
      newmatrix[rowindex  ] = mImagePixToVolumePix->GetElement( i, 0 );
      newmatrix[rowindex + 1] = mImagePixToVolumePix->GetElement( i, 1 );
      newmatrix[rowindex + 2] = mImagePixToVolumePix->GetElement( i, 2 );
      newmatrix[rowindex + 3] = mImagePixToVolumePix->GetElement( i, 3 );
    }
    insertionParams.matrix = newmatrix;

    switch ( str->InputFrameImage->GetScalarType() )
    {
    case VTK_SHORT:
      vtkOptimizedInsertSlice<fixed, short>( &insertionParams );
      break;
    case VTK_UNSIGNED_SHORT:
      vtkOptimizedInsertSlice<fixed, unsigned short>( &insertionParams );
      break;
    case VTK_CHAR:
      vtkOptimizedInsertSlice<fixed, char>( &insertionParams );
      break;
    case VTK_UNSIGNED_CHAR:
      vtkOptimizedInsertSlice<fixed, unsigned char>( &insertionParams );
      break;
    case VTK_FLOAT:
      vtkOptimizedInsertSlice<fixed, float>( &insertionParams );
      break;
    case VTK_DOUBLE:
      vtkOptimizedInsertSlice<fixed, double>( &insertionParams );
      break;
    case VTK_INT:
      vtkOptimizedInsertSlice<fixed, int>( &insertionParams );
      break;
    case VTK_UNSIGNED_INT:
      vtkOptimizedInsertSlice<fixed, unsigned int>( &insertionParams );
      break;
    case VTK_LONG:
      vtkOptimizedInsertSlice<fixed, long>( &insertionParams );
      break;
    case VTK_UNSIGNED_LONG:
      vtkOptimizedInsertSlice<fixed, unsigned long>( &insertionParams );
      break;
    default:
      LOG_ERROR( "OptimizedInsertSlice: Unknown input ScalarType" );
      return VTK_THREAD_RETURN_VALUE;
    }
  }
  else
  {
    // if we are not using fixed point math for optimization = 2, we are either:
    // doing no optimization (0) OR
    // breaking into x, y, z components with no bounds checking for nearest neighbor (1)

    // change transform matrix so that instead of taking
    // input coords -> output coords it takes output indices -> input indices
    double newmatrix[16];
    for ( int i = 0; i < 4; i++ )
    {
      int rowindex = ( i << 2 );
      newmatrix[rowindex  ] = mImagePixToVolumePix->GetElement( i, 0 );
      newmatrix[rowindex + 1] = mImagePixToVolumePix->GetElement( i, 1 );
      newmatrix[rowindex + 2] = mImagePixToVolumePix->GetElement( i, 2 );
      newmatrix[rowindex + 3] = mImagePixToVolumePix->GetElement( i, 3 );
    }
    insertionParams.matrix = newmatrix;


    if ( str->Optimization == PARTIAL_OPTIMIZATION )
    {
      switch ( inData->GetScalarType() )
      {
      case VTK_SHORT:
        vtkOptimizedInsertSlice<double, short>( &insertionParams );
        break;
      case VTK_UNSIGNED_SHORT:
        vtkOptimizedInsertSlice<double, unsigned short>( &insertionParams );
        break;
      case VTK_CHAR:
        vtkOptimizedInsertSlice<double, char>( &insertionParams );
        break;
      case VTK_UNSIGNED_CHAR:
        vtkOptimizedInsertSlice<double, unsigned char>( &insertionParams );
        break;
      case VTK_FLOAT:
        vtkOptimizedInsertSlice<double, float>( &insertionParams );
        break;
      case VTK_DOUBLE:
        vtkOptimizedInsertSlice<double, double>( &insertionParams );
        break;
      case VTK_INT:
        vtkOptimizedInsertSlice<double, int>( &insertionParams );
        break;
      case VTK_UNSIGNED_INT:
        vtkOptimizedInsertSlice<double, unsigned int>( &insertionParams );
        break;
      case VTK_LONG:
        vtkOptimizedInsertSlice<double, long>( &insertionParams );
        break;
      case VTK_UNSIGNED_LONG:
        vtkOptimizedInsertSlice<double, unsigned long>( &insertionParams );
        break;
      default:
        LOG_ERROR( "OptimizedInsertSlice: Unknown input ScalarType" );
      }
    }
    else
    {
      // no optimization
      switch ( inData->GetScalarType() )
      {
      case VTK_SHORT:
        vtkUnoptimizedInsertSlice<double, short>( &insertionParams );
        break;
      case VTK_UNSIGNED_SHORT:
        vtkUnoptimizedInsertSlice<double, unsigned short>( &insertionParams );
        break;
      case VTK_CHAR:
        vtkUnoptimizedInsertSlice<double, char>( &insertionParams );
        break;
      case VTK_UNSIGNED_CHAR:
        vtkUnoptimizedInsertSlice<double, unsigned char>( &insertionParams );
        break;
      case VTK_FLOAT:
        vtkUnoptimizedInsertSlice<double, float>( &insertionParams );
        break;
      case VTK_DOUBLE:
        vtkUnoptimizedInsertSlice<double, double>( &insertionParams );
        break;
      case VTK_INT:
        vtkUnoptimizedInsertSlice<double, int>( &insertionParams );
        break;
      case VTK_UNSIGNED_INT:
        vtkUnoptimizedInsertSlice<double, unsigned int>( &insertionParams );
        break;
      case VTK_LONG:
        vtkUnoptimizedInsertSlice<double, long>( &insertionParams );
        break;
      case VTK_UNSIGNED_LONG:
        vtkUnoptimizedInsertSlice<double, unsigned long>( &insertionParams );
        break;
      default:
        LOG_ERROR( "UnoptimizedInsertSlice: Unknown input ScalarType" );
      }
    }
  }

  return VTK_THREAD_RETURN_VALUE;
}

//----------------------------------------------------------------------------
// For streaming and threads.  Splits output update extent into num pieces.
// This method needs to be called num times.  Results must not overlap for
// consistent starting extent.  Subclass can override this method.
// This method returns the number of pieces resulting from a successful split.
// This can be from 1 to "requestedNumberOfThreads".
// If 1 is returned, the extent cannot be split.
int vtkIGSIOPasteSliceIntoVolume::SplitSliceExtent( int splitExt[6], int fullExt[6], int threadId, int requestedNumberOfThreads )
{

  int min, max; // the min and max indices of the axis of interest

  LOG_TRACE( "SplitSliceExtent: ( " << fullExt[0] << ", "
             << fullExt[1] << ", "
             << fullExt[2] << ", " << fullExt[3] << ", "
             << fullExt[4] << ", " << fullExt[5] << "), "
             << threadId << " of " << requestedNumberOfThreads );

  // start with same extent
  memcpy( splitExt, fullExt, 6 * sizeof( int ) );

  // determine which axis we should split along - preference is z, then y, then x
  // as long as we can possibly split along that axis (i.e. as long as z0 != z1)
  int splitAxis = 2; // the axis we should split along, try with z first
  min = fullExt[4];
  max = fullExt[5];
  while ( min == max )
  {
    --splitAxis;
    // we cannot split if the input extent is something like [50, 50, 100, 100, 0, 0]!
    if ( splitAxis < 0 )
    {
      LOG_DEBUG( "Cannot split the extent to multiple threads" );
      return 1;
    }
    min = fullExt[splitAxis * 2];
    max = fullExt[splitAxis * 2 + 1];
  }

  // determine the actual number of pieces that will be generated
  int range = max - min + 1;
  // split the range over the maximum number of threads
  int valuesPerThread = ( int )ceil( range / ( double )requestedNumberOfThreads );
  // figure out the largest thread id used
  int maxThreadIdUsed = ( int )ceil( range / ( double )valuesPerThread ) - 1;
  // if we are in a thread that will work on part of the extent, then figure
  // out the range that this thread should work on
  if ( threadId < maxThreadIdUsed )
  {
    splitExt[splitAxis * 2] = splitExt[splitAxis * 2] + threadId * valuesPerThread;
    splitExt[splitAxis * 2 + 1] = splitExt[splitAxis * 2] + valuesPerThread - 1;
  }
  if ( threadId == maxThreadIdUsed )
  {
    splitExt[splitAxis * 2] = splitExt[splitAxis * 2] + threadId * valuesPerThread;
  }

  // return the number of threads used
  return maxThreadIdUsed + 1;
}

//****************************************************************************

const char* vtkIGSIOPasteSliceIntoVolume::GetInterpolationModeAsString( InterpolationType type )
{
  switch ( type )
  {
  case NEAREST_NEIGHBOR_INTERPOLATION:
    return "NEAREST_NEIGHBOR";
  case LINEAR_INTERPOLATION:
    return "LINEAR";
  default:
    LOG_ERROR( "Unknown interpolation option: " << type );
    return "unknown";
  }
}

const char* vtkIGSIOPasteSliceIntoVolume::GetOutputScalarModeAsString( int type )
{
  switch ( type )
  {
  // list obtained from: http://www.vtk.org/doc/nightly/html/vtkType_8h_source.html
  //case VTK_VOID:              return "VTK_VOID";
  //case VTK_BIT:               return "VTK_BIT";
  case VTK_CHAR:
    return "VTK_CHAR";
  case VTK_UNSIGNED_CHAR:
    return "VTK_UNSIGNED_CHAR";
  case VTK_SHORT:
    return "VTK_SHORT";
  case VTK_UNSIGNED_SHORT:
    return "VTK_UNSIGNED_SHORT";
  case VTK_INT:
    return "VTK_INT";
  case VTK_UNSIGNED_INT:
    return "VTK_UNSIGNED_INT";
  case VTK_LONG:
    return "VTK_LONG";
  case VTK_UNSIGNED_LONG:
    return "VTK_UNSIGNED_LONG";
  case VTK_FLOAT:
    return "VTK_FLOAT";
  case VTK_DOUBLE:
    return "VTK_DOUBLE";
  //case VTK_SIGNED_CHAR:       return "VTK_SIGNED_CHAR";
  //case VTK_ID_TYPE:           return "VTK_ID_TYPE";
  default:
    LOG_ERROR( "Unknown output scalar mode option: " << type );
    return "unknown";
  }
}

const char* vtkIGSIOPasteSliceIntoVolume::GetCompoundingModeAsString( CompoundingType type )
{
  switch ( type )
  {
  case UNDEFINED_COMPOUNDING_MODE:
    return "UNDEFINED";
  case LATEST_COMPOUNDING_MODE:
    return "LATEST";
  case MEAN_COMPOUNDING_MODE:
    return "MEAN";
  case IMPORTANCE_MASK_COMPOUNDING_MODE:
    return "IMPORTANCE_MASK";
  case MAXIMUM_COMPOUNDING_MODE:
    return "MAXIMUM";
  default:
    LOG_ERROR( "Unknown compounding option: " << type );
    return "unknown";
  }
}

const char* vtkIGSIOPasteSliceIntoVolume::GetCalculationAsString( CalculationTypeDeprecated type )
{
  switch ( type )
  {
  case UNDEFINED_CALCULATION:
    return "UNDEFINED";
  case WEIGHTED_AVERAGE_CALCULATION:
    return "WEIGHTED_AVERAGE";
  case MAXIMUM_CALCULATION:
    return "MAXIMUM";
  default:
    LOG_ERROR( "Unknown compounding option: " << type );
    return "unknown";
  }
}

const char* vtkIGSIOPasteSliceIntoVolume::GetOptimizationModeAsString( OptimizationType type )
{
  switch ( type )
  {
  case FULL_OPTIMIZATION:
    return "FULL";
  case PARTIAL_OPTIMIZATION:
    return "PARTIAL";
  case NO_OPTIMIZATION:
    return "NONE";
  default:
    LOG_ERROR( "Unknown optimization option: " << type );
    return "unknown";
  }
}

//----------------------------------------------------------------------------
bool vtkIGSIOPasteSliceIntoVolume::FanClippingApplied()
{
  return this->FanAnglesDeg[0] != 0.0 || this->FanAnglesDeg[1] != 0.0;
}

//----------------------------------------------------------------------------
bool vtkIGSIOPasteSliceIntoVolume::IsPixelRejectionEnabled()
{
  return PixelRejectionEnabled( this->PixelRejectionThreshold );
}

//----------------------------------------------------------------------------
void vtkIGSIOPasteSliceIntoVolume::SetPixelRejectionDisabled()
{
  this->PixelRejectionThreshold = PIXEL_REJECTION_DISABLED;
}

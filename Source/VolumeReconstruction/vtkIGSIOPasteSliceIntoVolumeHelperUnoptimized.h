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


/*!
  \file vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized.h
  \brief Unoptimized helper functions for pasting slice into volume

  Contains unoptimized interpolation and slice insertion functions for vtkIGSIOPasteSliceIntoVolume

  \sa vtkIGSIOPasteSliceIntoVolume, vtkIGSIOPasteSliceIntoVolumeHelperCommon, vtkIGSIOPasteSliceIntoVolumeHelperOptimized, vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized
  \ingroup IGSIOLibVolumeReconstruction
*/

#ifndef __vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized_h
#define __vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized_h

#include "vtkIGSIOPasteSliceIntoVolumeHelperCommon.h"

/*!
  Non-optimized nearest neighbor interpolation.

  In the un-optimized version, each output voxel
  is converted into a set of look-up indices for the input data;
  then, the indices are checked to ensure they lie within the
  input data extent.

  In the optimized versions, the check is done in reverse:
  it is first determined which output voxels map to look-up indices
  within the input data extent.  Then, further calculations are
  done only for those voxels.  This means that 1) minimal work
  is done for voxels which map to regions outside of the input
  extent (they are just set to the background color) and 2)
  the inner loops of the look-up and interpolation are
  tightened relative to the un-uptimized version.

  Do nearest-neighbor interpolation of the input data 'inPtr' of extent 
  'inExt' at the 'point'.  The result is placed at 'outPtr'.  
  If the lookup data is beyond the extent 'inExt', set 'outPtr' to
  the background color 'background'.  
  The number of scalar components in the data is 'numscalars'
*/
template <class F, class T>
static int vtkNearestNeighborInterpolation(F *point,
                                           T *inPtr,
                                           T *outPtr,
                                           unsigned short *accPtr,
                                           unsigned char *importancePtr,
                                           int numscalars,
                                           vtkIGSIOPasteSliceIntoVolume::CompoundingType compoundingMode,
                                           int outExt[6],
                                           vtkIdType outInc[3],
                                           unsigned int* accOverflowCount)
{
  int i;
  // The nearest neighbor interpolation occurs here
  // The output point is the closest point to the input point - rounding
  // to get closest point
  int outIdX = igsioMath::Round(point[0])-outExt[0];
  int outIdY = igsioMath::Round(point[1])-outExt[2];
  int outIdZ = igsioMath::Round(point[2])-outExt[4];

  // fancy way of checking bounds
  if ((outIdX | (outExt[1]-outExt[0] - outIdX) |
       outIdY | (outExt[3]-outExt[2] - outIdY) |
       outIdZ | (outExt[5]-outExt[4] - outIdZ)) >= 0)
  {
    int inc = outIdX*outInc[0]+outIdY*outInc[1]+outIdZ*outInc[2];
    outPtr += inc;
    switch (compoundingMode)
    {
    case (vtkIGSIOPasteSliceIntoVolume::MAXIMUM_COMPOUNDING_MODE):
      {
        accPtr += inc/outInc[0];

        int newa = *accPtr + ACCUMULATION_MULTIPLIER;
        if (newa > ACCUMULATION_THRESHOLD)
          (*accOverflowCount) += 1;

        for (i = 0; i < numscalars; i++)
        {
          if (*inPtr > *outPtr)
            *outPtr = *inPtr;
          inPtr++;
          outPtr++;
        }

        *accPtr = ACCUMULATION_MAXIMUM; // set to 0xFFFF by default for overflow protection
        if (newa < ACCUMULATION_MAXIMUM)
        {
          *accPtr = newa;
        }

        break;
      }
    case (vtkIGSIOPasteSliceIntoVolume::MEAN_COMPOUNDING_MODE):
      {
        accPtr += inc/outInc[0];
        if (*accPtr <= ACCUMULATION_THRESHOLD) { // no overflow, act normally

          int newa = *accPtr + ACCUMULATION_MULTIPLIER;
          if (newa > ACCUMULATION_THRESHOLD)
            (*accOverflowCount) += 1;

          for (i = 0; i < numscalars; i++)
          {
            *outPtr = ((*inPtr++)*ACCUMULATION_MULTIPLIER + (*outPtr)*(*accPtr))/newa;
            outPtr++;
          }

          *accPtr = ACCUMULATION_MAXIMUM; // set to 0xFFFF by default for overflow protection
          if (newa < ACCUMULATION_MAXIMUM)
          {
            *accPtr = newa;
          }
        } else { // overflow, use recursive filtering with 255/256 and 1/256 as the weights, since 255 voxels have been inserted so far
          // TODO: Should do this for all the scalars, and accumulation?
          *outPtr = (T)(fraction1_256 * (*inPtr++) + fraction255_256 * (*outPtr));
        }
        break;
      }
    case (vtkIGSIOPasteSliceIntoVolume::IMPORTANCE_MASK_COMPOUNDING_MODE):
      {
        accPtr += inc/outInc[0];
        if (*accPtr <= ACCUMULATION_THRESHOLD) { // no overflow, act normally

          if (*importancePtr == 0)
          {
            //nothing to do
            break;
          }

          int newa = *accPtr + *importancePtr;
          if (newa > ACCUMULATION_THRESHOLD)
          {
            (*accOverflowCount) += 1;
          }
          
          for (i = 0; i < numscalars; i++)
          {
            *outPtr = ((*inPtr++)*(*importancePtr) + (*outPtr)*(*accPtr))/newa;
            outPtr++;
          }

          *accPtr = ACCUMULATION_MAXIMUM; // set to 0xFFFF by default for overflow protection
          if (newa < ACCUMULATION_MAXIMUM)
          {
            *accPtr = newa;
          }
        } 
        else 
        { 
          // overflow, use recursive filtering with 255/256 and 1/256 as the weights, since 255 voxels have been inserted so far
          // TODO: Should do this for all the scalars, and accumulation?
          *outPtr = (T)(fraction1_256 * (*inPtr++) + fraction255_256 * (*outPtr));
        }
        break;
      }
    case (vtkIGSIOPasteSliceIntoVolume::LATEST_COMPOUNDING_MODE):
      {
        accPtr += inc/outInc[0];

        int newa = *accPtr + ACCUMULATION_MULTIPLIER;
        if (newa > ACCUMULATION_THRESHOLD)
          (*accOverflowCount) += 1;

        for (i = 0; i < numscalars; i++)
        {
          *outPtr = *inPtr;
          inPtr++;
          outPtr++;
        }

        *accPtr = ACCUMULATION_MAXIMUM; // set to 0xFFFF by default for overflow protection
        if (newa < ACCUMULATION_MAXIMUM)
        {
          *accPtr = newa;
        }

        break;
      }
    default:
      LOG_ERROR("Unknown Compounding operator detected, value " << compoundingMode << ". Leaving value as-is.");
      break;
    }
    return 1;
  }
  return 0;
} 

//----------------------------------------------------------------------------
/*!
  Actually inserts the slice - executes the filter for any type of data, without optimization
  Given an input and output region, execute the filter algorithm to fill the
  output from the input - no optimization.
  (this one function is pretty much the be-all and end-all of the filter)
*/
template <class F, class T>
static void vtkUnoptimizedInsertSlice(vtkIGSIOPasteSliceIntoVolumeInsertSliceParams* insertionParams)
{
  // information on the volume
  vtkImageData* outData = insertionParams->outData;
  T* outPtr = reinterpret_cast<T*>(insertionParams->outPtr);
  unsigned short* accPtr = insertionParams->accPtr;
  unsigned char* importancePtr = insertionParams->importancePtr;
  vtkImageData* inData = insertionParams->inData;
  T* inPtr = reinterpret_cast<T*>(insertionParams->inPtr);
  int* inExt = insertionParams->inExt;
  unsigned int* accOverflowCount = insertionParams->accOverflowCount;

  // transform matrix for image -> volume
  F* matrix = reinterpret_cast<F*>(insertionParams->matrix);
  LOG_TRACE("sliceToOutputVolumeMatrix="<<matrix[0]<<" "<<matrix[1]<<" "<<matrix[2]<<" "<<matrix[3]<<"; "
    <<matrix[4]<<" "<<matrix[5]<<" "<<matrix[6]<<" "<<matrix[7]<<"; "
    <<matrix[8]<<" "<<matrix[9]<<" "<<matrix[10]<<" "<<matrix[11]<<"; "
    <<matrix[12]<<" "<<matrix[13]<<" "<<matrix[14]<<" "<<matrix[15]
    );

  // details specified by the user RE: how the voxels should be computed
  vtkIGSIOPasteSliceIntoVolume::InterpolationType interpolationMode = insertionParams->interpolationMode;   // linear or nearest neighbor
  vtkIGSIOPasteSliceIntoVolume::CompoundingType compoundingMode = insertionParams->compoundingMode;         // weighted average or maximum

  // parameters for clipping
  double* clipRectangleOrigin = insertionParams->clipRectangleOrigin; // array size 2
  double* clipRectangleSize = insertionParams->clipRectangleSize; // array size 2
  double* fanAnglesDeg = insertionParams->fanAnglesDeg; // array size 2, for transrectal/curvilinear transducers
  double* fanOrigin = insertionParams->fanOrigin; // array size 2
  double fanRadiusStart = insertionParams->fanRadiusStart;
  double fanRadiusStop = insertionParams->fanRadiusStop;

  // slice spacing and origin
  double inSpacing[3];
  inData->GetSpacing(inSpacing);
  double inOrigin[3];
  inData->GetOrigin(inOrigin);

  // number of pixels in the x and y directions between the fan origin and the slice origin  
  double fanOriginInPixels[2] =
  {
    (fanOrigin[0]-inOrigin[0])/inSpacing[0],
    (fanOrigin[1]-inOrigin[1])/inSpacing[1]
  };
  // fan depth squared 
  double squaredFanRadiusStart = fanRadiusStart*fanRadiusStart;
  double squaredFanRadiusStop = fanRadiusStop*fanRadiusStop;

  // absolute value of slice spacing
  double inSpacingSquare[2]=
  {
    inSpacing[0]*inSpacing[0],
    inSpacing[1]*inSpacing[1]
  };

  double pixelAspectRatio=fabs(inSpacing[1]/inSpacing[0]);
  // tan of the left and right fan angles
  double fanLinePixelRatioLeft = tan(vtkMath::RadiansFromDegrees(fanAnglesDeg[0]))*pixelAspectRatio;
  double fanLinePixelRatioRight = tan(vtkMath::RadiansFromDegrees(fanAnglesDeg[1]))*pixelAspectRatio;
  // the tan of the right fan angle is always greater than the left one
  if (fanLinePixelRatioLeft > fanLinePixelRatioRight)
  {
    // swap left and right fan lines
    double tmp = fanLinePixelRatioLeft; 
    fanLinePixelRatioLeft = fanLinePixelRatioRight; 
    fanLinePixelRatioRight = tmp;
  }
  // get the clip rectangle as an extent
  int clipExt[6];
  GetClipExtent(clipExt, inOrigin, inSpacing, inExt, clipRectangleOrigin, clipRectangleSize);

  // find maximum output range = output extent
  int outExt[6];
  outData->GetExtent(outExt);

  // Get increments to march through data - ex move from the end of one x scanline of data to the
  // start of the next line
  vtkIdType outInc[3] ={0};
  outData->GetIncrements(outInc);
  vtkIdType inIncX=0, inIncY=0, inIncZ=0;
  inData->GetContinuousIncrements(inExt, inIncX, inIncY, inIncZ);
  int numscalars = inData->GetNumberOfScalarComponents();
  vtkIdType impIncX = 0;
  vtkIdType impIncY = 0;
  vtkIdType impIncZ = 0;
  if (insertionParams->importanceMask)
  {
    insertionParams->importanceMask->GetContinuousIncrements(inExt, impIncX, impIncY, impIncZ);
  }

  // Set interpolation method - nearest neighbor or trilinear  
  int (*interpolate)(F *, T *, T *, unsigned short *, unsigned char *, int, vtkIGSIOPasteSliceIntoVolume::CompoundingType, int a[6], vtkIdType b[3], unsigned int *)=NULL; // pointer to the nearest neighbor or trilinear interpolation function  
  switch (interpolationMode)
  {
  case vtkIGSIOPasteSliceIntoVolume::NEAREST_NEIGHBOR_INTERPOLATION:
    interpolate = &vtkNearestNeighborInterpolation;
    break;
  case vtkIGSIOPasteSliceIntoVolume::LINEAR_INTERPOLATION:
    interpolate = &vtkTrilinearInterpolation;
    break;
  default:
    LOG_ERROR("Unkown interpolation mode: "<<interpolationMode);
    return;
  }

  bool pixelRejectionEnabled = PixelRejectionEnabled(insertionParams->pixelRejectionThreshold);
  double pixelRejectionThresholdSumAllComponents = 0;
  if (pixelRejectionEnabled)
  {
    pixelRejectionThresholdSumAllComponents = insertionParams->pixelRejectionThreshold * numscalars;
  }

  // Loop through  slice pixels in the input extent and put them into the output volume
  // the resulting point in the output volume (outPoint) from a point in the input slice
  // (inpoint)
  double outPoint[4];
  double inPoint[4]; 
  inPoint[3] = 1;
  bool fanClippingEnabled = (fanLinePixelRatioLeft != 0 || fanLinePixelRatioRight != 0);
  for (int idZ = inExt[4]; idZ <= inExt[5]; idZ++, inPtr += inIncZ, importancePtr += impIncZ)
  {
    for (int idY = inExt[2]; idY <= inExt[3]; idY++, inPtr += inIncY, importancePtr += impIncY)
    {
      for (int idX = inExt[0]; idX <= inExt[1]; idX++, inPtr += numscalars, importancePtr += 1)
      {
        // check if we are within the current clip extent
        if (idX < clipExt[0] || idX > clipExt[1] || idY < clipExt[2] || idY > clipExt[3])
        {
          // outside the clipping rectangle
          continue;
        }

        if (pixelRejectionEnabled)
        {
          double inPixelSumAllComponents = 0;
          for (int i = numscalars-1; i>=0; i--)
          {
            inPixelSumAllComponents+=inPtr[i];
          }
          if (inPixelSumAllComponents<pixelRejectionThresholdSumAllComponents)
          {
            // too dark, skip this pixel
            continue;
          }
        }        

        // check if we are within the clipping fan
        if ( fanClippingEnabled )
        {
          // x and y are the current pixel coordinates in fan coordinate system (in pixels)
          double x = (idX-fanOriginInPixels[0]);
          double y = (idY-fanOriginInPixels[1]);
          if (y<0 || (x/y<fanLinePixelRatioLeft) || (x/y>fanLinePixelRatioRight))
          {
            // outside the fan triangle
            continue;
          }
          double squaredDistanceFromFanOrigin = x*x*inSpacingSquare[0]+y*y*inSpacingSquare[1];
          if (squaredDistanceFromFanOrigin<squaredFanRadiusStart || squaredDistanceFromFanOrigin>squaredFanRadiusStop)
          {
            // too close or too far from the fan origin
            continue;
          }
        }

        inPoint[0] = idX;
        inPoint[1] = idY;
        inPoint[2] = idZ;

        // matrix multiplication - input -> output
        for (int i = 0; i < 4; i++)
        {
          int rowindex = i << 2;
          outPoint[i] = matrix[rowindex  ] * inPoint[0] + 
                        matrix[rowindex+1] * inPoint[1] + 
                        matrix[rowindex+2] * inPoint[2] + 
                        matrix[rowindex+3] * inPoint[3] ;
        }

        // deal with w (homogeneous transform) if the transform was a perspective transform
        outPoint[0] /= outPoint[3]; 
        outPoint[1] /= outPoint[3]; 
        outPoint[2] /= outPoint[3];
        outPoint[3] = 1;

        // interpolation functions return 1 if the interpolation was successful, 0 otherwise
        interpolate(outPoint, inPtr, outPtr, accPtr, importancePtr, numscalars, compoundingMode, outExt, outInc, accOverflowCount);
      }
    }
  }
}


#endif

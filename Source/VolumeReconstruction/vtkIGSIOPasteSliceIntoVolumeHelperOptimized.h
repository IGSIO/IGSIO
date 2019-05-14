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
  \file vtkIGSIOPasteSliceIntoVolumeHelperOptimized.h
  \brief Optimized helper functions for pasting slice into volume

  Contains optimized interpolation and slice insertion functions for vtkIGSIOPasteSliceIntoVolume

  \sa vtkIGSIOPasteSliceIntoVolume, vtkIGSIOPasteSliceIntoVolumeHelperCommon, vtkIGSIOPasteSliceIntoVolumeHelperOptimized, vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized
  \ingroup IGSIOLibVolumeReconstruction
*/

#ifndef __vtkIGSIOPasteSliceIntoVolumeHelperOptimized_h
#define __vtkIGSIOPasteSliceIntoVolumeHelperOptimized_h

#include "vtkIGSIOPasteSliceIntoVolumeHelperCommon.h"
#include "fixed.h"

//----------------------------------------------------------------------------
/*! 
  Find approximate intersection of line with the plane
  x = x_min, y = y_min, or z = z_min (lower limit of data extent) 
*/
template<class F>
static inline
int intersectionHelper(F *point, F *axis, int *limit, int ai, int *inExt)
{
  F rd = limit[ai]*point[3]-point[ai]  + 0.5; 

  if (rd < inExt[0])
  { 
    return inExt[0];
  }
  else if (rd > inExt[1])
  {
    return inExt[1];
  }
  else
  {
    return int(rd);
  }
}

//----------------------------------------------------------------------------
/*! Find the point just inside the extent */
template <class F>
static int intersectionLow(F *point, F *axis, int *sign,
                           int *limit, int ai, int *inExt)
{
  // approximate value of r
  int r = intersectionHelper(point,axis,limit,ai,inExt);

  // move back and forth to find the point just inside the extent
  for (;;)
  {
    F p = point[ai]+r*axis[ai];

    if (((sign[ai] < 0 && r > inExt[0]) ||
      (sign[ai] > 0 && r < inExt[1])) &&
      igsioMath::Round(p) < limit[ai])
    {
      r += sign[ai];
    }
    else
    {
      break;
    }
  }

  for (;;)
  {
    F p = point[ai]+(r-sign[ai])*axis[ai];

    if (((sign[ai] > 0 && r > inExt[0]) ||
      (sign[ai] < 0 && r < inExt[1])) &&
      igsioMath::Round(p) >= limit[ai])
    {
      r -= sign[ai];
    }
    else
    {
      break;
    }
  }

  return r;
}

//----------------------------------------------------------------------------
/*! Find the point just inside the extent, for x = x_max */
template <class F>
static int intersectionHigh(F *point, F *axis, int *sign, 
                            int *limit, int ai, int *inExt)
{
  // approximate value of r
  int r = intersectionHelper(point,axis,limit,ai,inExt);

  // move back and forth to find the point just inside the extent
  for (;;)
  {
    F p = point[ai]+r*axis[ai];

    if (((sign[ai] > 0 && r > inExt[0]) ||
      (sign[ai] < 0 && r < inExt[1])) &&
      igsioMath::Round(p) > limit[ai])
    {
      r -= sign[ai];
    }
    else
    {
      break;
    }
  }

  for (;;)
  {
    F p = point[ai]+(r+sign[ai])*axis[ai];

    if (((sign[ai] < 0 && r > inExt[0]) ||
      (sign[ai] > 0 && r < inExt[1])) &&
      igsioMath::Round(p) <= limit[ai])
    {
      r += sign[ai];
    }
    else
    {
      break;
    }
  }

  return r;
}

//----------------------------------------------------------------------------
template <class F>
static int isBounded(F *point, F *xAxis, int *inMin, int *inMax, int ai, int r)
{
  int bi = ai+1; 
  int ci = ai+2;
  if (bi > 2) 
  { 
    bi -= 3; // coordinate index must be 0, 1 or 2 
  } 
  if (ci > 2)
  { 
    ci -= 3;
  }

  F fbp = point[bi]+r*xAxis[bi];
  F fcp = point[ci]+r*xAxis[ci];

  int bp = igsioMath::Round(fbp);
  int cp = igsioMath::Round(fcp);

  return (bp >= inMin[bi] && bp <= inMax[bi] &&
    cp >= inMin[ci] && cp <= inMax[ci]);
}

//----------------------------------------------------------------------------
/*!
  This huge mess finds out where the current output raster
  line intersects the input volume
*/
static void vtkUltraFindExtentHelper(int &xIntersectionPixStart, int &xIntersectionPixEnd, int sign, int *inExt)
{
  if (sign < 0)
  {
    int i = xIntersectionPixStart;
    xIntersectionPixStart = xIntersectionPixEnd;
    xIntersectionPixEnd = i;
  }

  // bound xIntersectionPixStart,xIntersectionPixEnd within reasonable limits
  if (xIntersectionPixStart < inExt[0]) 
  {
    xIntersectionPixStart = inExt[0];
  }
  if (xIntersectionPixEnd > inExt[1]) 
  {
    xIntersectionPixEnd = inExt[1];
  }
  if (xIntersectionPixStart > xIntersectionPixEnd) 
  {
    xIntersectionPixStart = inExt[0];
    xIntersectionPixEnd = inExt[0]-1;
  }
}  

//----------------------------------------------------------------------------
template <class F>
static void vtkUltraFindExtent(int& xIntersectionPixStart, int& xIntersectionPixEnd, F *point, F *xAxis, 
                               int *inMin, int *inMax, int *inExt)
{
  int i, ix, iy, iz;
  int sign[3];
  int indx1[4],indx2[4];
  F p1,p2;

  // find signs of components of x axis 
  // (this is complicated due to the homogeneous coordinate)
  for (i = 0; i < 3; i++)
  {
    p1 = point[i];

    p2 = point[i]+xAxis[i];

    if (p1 <= p2)
    {
      sign[i] = 1;
    }
    else 
    {
      sign[i] = -1;
    }
  } 

  // order components of xAxis from largest to smallest
  ix = 0;
  for (i = 1; i < 3; i++)
  {
    if (((xAxis[i] < 0) ? (-xAxis[i]) : (xAxis[i])) >
      ((xAxis[ix] < 0) ? (-xAxis[ix]) : (xAxis[ix])))
    {
      ix = i;
    }
  }

  iy = ((ix > 1) ? ix-2 : ix+1);
  iz = ((ix > 0) ? ix-1 : ix+2);

  if (((xAxis[iy] < 0) ? (-xAxis[iy]) : (xAxis[iy])) >
    ((xAxis[iz] < 0) ? (-xAxis[iz]) : (xAxis[iz])))
  {
    i = iy;
    iy = iz;
    iz = i;
  }

  xIntersectionPixStart = intersectionLow(point,xAxis,sign,inMin,ix,inExt);
  xIntersectionPixEnd = intersectionHigh(point,xAxis,sign,inMax,ix,inExt);

  // find points of intersections
  // first, find w-value for perspective (will usually be 1)
  for (i = 0; i < 3; i++)
  {
    p1 = point[i]+xIntersectionPixStart*xAxis[i];
    p2 = point[i]+xIntersectionPixEnd*xAxis[i];

    indx1[i] = igsioMath::Round(p1);
    indx2[i] = igsioMath::Round(p2);
  }

  // passed through x face, check opposing face
  if (isBounded(point,xAxis,inMin,inMax,ix,xIntersectionPixStart))
  {
    if (isBounded(point,xAxis,inMin,inMax,ix,xIntersectionPixEnd))
    {
      vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[ix],inExt);
      return;
    }

    // check y face
    if (indx2[iy] < inMin[iy])
    {
      xIntersectionPixEnd = intersectionLow(point,xAxis,sign,inMin,iy,inExt);
      if (isBounded(point,xAxis,inMin,inMax,iy,xIntersectionPixEnd))
      {
        vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[ix],inExt);
        return;
      }
    }

    // check other y face
    else if (indx2[iy] > inMax[iy])
    {
      xIntersectionPixEnd = intersectionHigh(point,xAxis,sign,inMax,iy,inExt);
      if (isBounded(point,xAxis,inMin,inMax,iy,xIntersectionPixEnd))
      {
        vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[ix],inExt);
        return;
      }
    }

    // check z face
    if (indx2[iz] < inMin[iz])
    {
      xIntersectionPixEnd = intersectionLow(point,xAxis,sign,inMin,iz,inExt);
      if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixEnd))
      {
        vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[ix],inExt);
        return;
      }
    }

    // check other z face
    else if (indx2[iz] > inMax[iz])
    {
      xIntersectionPixEnd = intersectionHigh(point,xAxis,sign,inMax,iz,inExt);
      if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixEnd))
      {
        vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[ix],inExt);
        return;
      }
    }
  }

  // passed through the opposite x face
  if (isBounded(point,xAxis,inMin,inMax,ix,xIntersectionPixEnd))
  {
    // check y face
    if (indx1[iy] < inMin[iy])
    {
      xIntersectionPixStart = intersectionLow(point,xAxis,sign,inMin,iy,inExt);
      if (isBounded(point,xAxis,inMin,inMax,iy,xIntersectionPixStart))
      {
        vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[ix],inExt);
        return;
      }
    }
    // check other y face
    else if (indx1[iy] > inMax[iy])
    {
      xIntersectionPixStart = intersectionHigh(point,xAxis,sign,inMax,iy,inExt);
      if (isBounded(point,xAxis,inMin,inMax,iy,xIntersectionPixStart))
      {
        vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[ix],inExt);
        return;
      }
    }

    // check other y face
    if (indx1[iz] < inMin[iz])
    {
      xIntersectionPixStart = intersectionLow(point,xAxis,sign,inMin,iz,inExt);
      if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixStart))
      {
        vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[ix],inExt);
        return;
      }
    }
    // check other z face
    else if (indx1[iz] > inMax[iz])
    {
      xIntersectionPixStart = intersectionHigh(point,xAxis,sign,inMax,iz,inExt);
      if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixStart))
      {
        vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[ix],inExt);
        return;
      }
    }
  }

  // line might pass through bottom face
  if ((indx1[iy] >= inMin[iy] && indx2[iy] < inMin[iy]) ||
    (indx1[iy] < inMin[iy] && indx2[iy] >= inMin[iy]))
  {
    xIntersectionPixStart = intersectionLow(point,xAxis,sign,inMin,iy,inExt);
    if (isBounded(point,xAxis,inMin,inMax,iy,xIntersectionPixStart))
    {
      // line might pass through top face
      if ((indx1[iy] <= inMax[iy] && indx2[iy] > inMax[iy]) ||
        (indx1[iy] > inMax[iy] && indx2[iy] <= inMax[iy]))
      { 
        xIntersectionPixEnd = intersectionHigh(point,xAxis,sign,inMax,iy,inExt);
        if (isBounded(point,xAxis,inMin,inMax,iy,xIntersectionPixEnd))
        {
          vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[iy],inExt);
          return;
        }
      }

      // line might pass through in-to-screen face
      if ((indx1[iz] < inMin[iz] && indx2[iy] < inMin[iy]) ||
        (indx2[iz] < inMin[iz] && indx1[iy] < inMin[iy]))
      { 
        xIntersectionPixEnd = intersectionLow(point,xAxis,sign,inMin,iz,inExt);
        if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixEnd))
        {
          vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[iy],inExt);
          return;
        }
      }
      // line might pass through out-of-screen face
      else if ((indx1[iz] > inMax[iz] && indx2[iy] < inMin[iy]) ||
        (indx2[iz] > inMax[iz] && indx1[iy] < inMin[iy]))
      {
        xIntersectionPixEnd = intersectionHigh(point,xAxis,sign,inMax,iz,inExt);
        if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixEnd))
        {
          vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[iy],inExt);
          return;
        }
      } 
    }
  }

  // line might pass through top face
  if ((indx1[iy] <= inMax[iy] && indx2[iy] > inMax[iy]) ||
    (indx1[iy] > inMax[iy] && indx2[iy] <= inMax[iy]))
  {
    xIntersectionPixEnd = intersectionHigh(point,xAxis,sign,inMax,iy,inExt);
    if (isBounded(point,xAxis,inMin,inMax,iy,xIntersectionPixEnd))
    {
      // line might pass through in-to-screen face
      if ((indx1[iz] < inMin[iz] && indx2[iy] > inMax[iy]) ||
        (indx2[iz] < inMin[iz] && indx1[iy] > inMax[iy]))
      {
        xIntersectionPixStart = intersectionLow(point,xAxis,sign,inMin,iz,inExt);
        if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixStart))
        {
          vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[iy],inExt);
          return;
        }
      }
      // line might pass through out-of-screen face
      else if ((indx1[iz] > inMax[iz] && indx2[iy] > inMax[iy]) ||
        (indx2[iz] > inMax[iz] && indx1[iy] > inMax[iy]))
      {
        xIntersectionPixStart = intersectionHigh(point,xAxis,sign,inMax,iz,inExt);
        if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixStart))
        {
          vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[iy],inExt);
          return;
        }
      }
    } 
  }

  // line might pass through in-to-screen face
  if ((indx1[iz] >= inMin[iz] && indx2[iz] < inMin[iz]) ||
    (indx1[iz] < inMin[iz] && indx2[iz] >= inMin[iz]))
  {
    xIntersectionPixStart = intersectionLow(point,xAxis,sign,inMin,iz,inExt);
    if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixStart))
    {
      // line might pass through out-of-screen face
      if (indx1[iz] > inMax[iz] || indx2[iz] > inMax[iz])
      {
        xIntersectionPixEnd = intersectionHigh(point,xAxis,sign,inMax,iz,inExt);
        if (isBounded(point,xAxis,inMin,inMax,iz,xIntersectionPixEnd))
        {
          vtkUltraFindExtentHelper(xIntersectionPixStart,xIntersectionPixEnd,sign[iz],inExt);
          return;
        }
      }
    }
  }

  xIntersectionPixStart = inExt[0];
  xIntersectionPixEnd = inExt[0] - 1;
}



//----------------------------------------------------------------------------
/*! Optimized nearest neighbor interpolation, without integer mathematics */
template<class T>
static inline void vtkFreehand2OptimizedNNHelper(int xIntersectionPixStart,
                                                 int xIntersectionPixEnd,
                                                 double *outPoint,
                                                 double *outPoint1,
                                                 double *xAxis,
                                                 T *&inPtr,
                                                 T *outPtr,
                                                 int *outExt,
                                                 vtkIdType *outInc,
                                                 int numscalars,
                                                 vtkIGSIOPasteSliceIntoVolume::CompoundingType compoundingMode, 
                                                 unsigned short *accPtr,
                                                 unsigned char *&importancePtr,
                                                 unsigned int *accOverflowCount,
                                                 double pixelRejectionThreshold)
{
  bool pixelRejectionEnabled = PixelRejectionEnabled(pixelRejectionThreshold);
  double pixelRejectionThresholdSumAllComponents = 0;
  if (pixelRejectionEnabled)
  {
    pixelRejectionThresholdSumAllComponents = pixelRejectionThreshold * numscalars;
  }
  switch (compoundingMode) {
  case  vtkIGSIOPasteSliceIntoVolume::MEAN_COMPOUNDING_MODE :
    for (int idX = xIntersectionPixStart; idX <= xIntersectionPixEnd; idX++)
    {
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
          inPtr += numscalars;
          continue;
        }
      }

      outPoint[0] = outPoint1[0] + idX*xAxis[0]; 
      outPoint[1] = outPoint1[1] + idX*xAxis[1];
      outPoint[2] = outPoint1[2] + idX*xAxis[2];

      int outIdX = igsioMath::Round(outPoint[0]) - outExt[0];
      int outIdY = igsioMath::Round(outPoint[1]) - outExt[2];
      int outIdZ = igsioMath::Round(outPoint[2]) - outExt[4];

      int inc = outIdX*outInc[0] + outIdY*outInc[1] + outIdZ*outInc[2];
      T *outPtr1 = outPtr + inc;
      // divide by outInc[0] to accomodate for the difference
      // in the number of scalar pointers between the output
      // and the accumulation buffer
      unsigned short *accPtr1 = accPtr + (inc/outInc[0]); // removed cast to unsigned short because it might cause loss in larger numbers

      if (*accPtr1 <= ACCUMULATION_THRESHOLD) { // no overflow, act normally

        unsigned short newa = *accPtr1 + ((unsigned short)(ACCUMULATION_MULTIPLIER)); 

        if (newa > ACCUMULATION_THRESHOLD)
          (*accOverflowCount) += 1;

        int i = numscalars;
        do 
        {
          i--;
          *outPtr1 = ((*inPtr++)*ACCUMULATION_MULTIPLIER + (*outPtr1)*(*accPtr1))/newa;
          outPtr1++;
        }
        while (i);

        *accPtr1 = ACCUMULATION_MAXIMUM;
        if (newa < ACCUMULATION_MAXIMUM)
        {
          *accPtr1 = newa;
        } 
      } else { // overflow, use recursive filtering with 255/256 and 1/256 as the weights, since 255 voxels have been inserted so far
        // TODO : This doesn't iterate through the scalars, this could be a problem
        *outPtr1 = (T)( (*inPtr++)*fraction1_256 + (*outPtr1)*fraction255_256 );
      }
    }
    break;
  case  vtkIGSIOPasteSliceIntoVolume::IMPORTANCE_MASK_COMPOUNDING_MODE :
    for (int idX = xIntersectionPixStart; idX <= xIntersectionPixEnd; idX++)
    {
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
          inPtr += numscalars;
          importancePtr++;
          continue;
        }
      }

      outPoint[0] = outPoint1[0] + idX*xAxis[0]; 
      outPoint[1] = outPoint1[1] + idX*xAxis[1];
      outPoint[2] = outPoint1[2] + idX*xAxis[2];

      int outIdX = igsioMath::Round(outPoint[0]) - outExt[0];
      int outIdY = igsioMath::Round(outPoint[1]) - outExt[2];
      int outIdZ = igsioMath::Round(outPoint[2]) - outExt[4];

      int inc = outIdX*outInc[0] + outIdY*outInc[1] + outIdZ*outInc[2];
      T *outPtr1 = outPtr + inc;
      // divide by outInc[0] to accomodate for the difference
      // in the number of scalar pointers between the output
      // and the accumulation buffer
      unsigned short *accPtr1 = accPtr + (inc/outInc[0]); // removed cast to unsigned short because it might cause loss in larger numbers

      if (*accPtr1 <= ACCUMULATION_THRESHOLD)
      {
        // no overflow, act normally
        if (*importancePtr == 0)
        {
          //nothing to do
          break;
        }
        unsigned short newa = *accPtr1 + *importancePtr;

        if (newa > ACCUMULATION_THRESHOLD)
        {
          (*accOverflowCount) += 1;
        }

        int i = numscalars;
        do 
        {
          i--;
          *outPtr1 = ((*inPtr++)*(*importancePtr) + (*outPtr1)*(*accPtr1))/newa;
          outPtr1++;
        }
        while (i);
        importancePtr++;

        *accPtr1 = ACCUMULATION_MAXIMUM;
        if (newa < ACCUMULATION_MAXIMUM)
        {
          *accPtr1 = newa;
        } 
      }
      else
      {
        // overflow, use recursive filtering with 255/256 and 1/256 as the weights, since 255 voxels have been inserted so far
        // TODO : This doesn't iterate through the scalars, this could be a problem
        *outPtr1 = (T)( (*inPtr++)*fraction1_256 + (*outPtr1)*fraction255_256 );
      }
    }
    break;
  case  vtkIGSIOPasteSliceIntoVolume::MAXIMUM_COMPOUNDING_MODE :
    for (int idX = xIntersectionPixStart; idX <= xIntersectionPixEnd; idX++)
    {
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
          inPtr += numscalars;
          continue;
        }
      }

      outPoint[0] = outPoint1[0] + idX*xAxis[0]; 
      outPoint[1] = outPoint1[1] + idX*xAxis[1];
      outPoint[2] = outPoint1[2] + idX*xAxis[2];

      int outIdX = igsioMath::Round(outPoint[0]) - outExt[0];
      int outIdY = igsioMath::Round(outPoint[1]) - outExt[2];
      int outIdZ = igsioMath::Round(outPoint[2]) - outExt[4];

      int inc = outIdX*outInc[0] + outIdY*outInc[1] + outIdZ*outInc[2];
      T *outPtr1 = outPtr + inc;
      // divide by outInc[0] to accomodate for the difference
      // in the number of scalar pointers between the output
      // and the accumulation buffer
      unsigned short *accPtr1 = accPtr + (inc/outInc[0]); // removed cast to unsigned short because it might cause loss in larger numbers
      int i = numscalars;
      do 
      {
        i--;
        if (*outPtr1 < *inPtr)
          *outPtr1 = *inPtr;
        outPtr1++;
        inPtr++;
      }
      while (i);

      *accPtr1 = (unsigned short)ACCUMULATION_MULTIPLIER;
    }
    break;
  case  vtkIGSIOPasteSliceIntoVolume::LATEST_COMPOUNDING_MODE :
    for (int idX = xIntersectionPixStart; idX <= xIntersectionPixEnd; idX++)
    {
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
          inPtr += numscalars;
          continue;
        }
      }

      outPoint[0] = outPoint1[0] + idX*xAxis[0]; 
      outPoint[1] = outPoint1[1] + idX*xAxis[1];
      outPoint[2] = outPoint1[2] + idX*xAxis[2];

      int outIdX = igsioMath::Round(outPoint[0]) - outExt[0];
      int outIdY = igsioMath::Round(outPoint[1]) - outExt[2];
      int outIdZ = igsioMath::Round(outPoint[2]) - outExt[4];

      int inc = outIdX*outInc[0] + outIdY*outInc[1] + outIdZ*outInc[2];
      T *outPtr1 = outPtr + inc;
      // divide by outInc[0] to accomodate for the difference
      // in the number of scalar pointers between the output
      // and the accumulation buffer
      unsigned short *accPtr1 = accPtr + (inc/outInc[0]); // removed cast to unsigned short because it might cause loss in larger numbers
      int i = numscalars;
      do 
      {
        i--;
        *outPtr1 = *inPtr;
        outPtr1++;
        inPtr++;
      }
      while (i);

      *accPtr1 = (unsigned short)ACCUMULATION_MULTIPLIER;
    }
    break;
  default:
    LOG_ERROR("Unknown Compounding operator detected, value " << compoundingMode << ". Leaving value as-is.");
    break;
  }
}

//----------------------------------------------------------------------------
/*! 
  Optimized nearest neighbor interpolation, specifically optimized for fixed
  point (i.e. integer) mathematics
*/
template <class T>
static inline void vtkFreehand2OptimizedNNHelper(int xIntersectionPixStart,
                                                 int xIntersectionPixEnd,
                                                 fixed *outPoint,
                                                 fixed *outPoint1,
                                                 fixed *xAxis,
                                                 T *&inPtr,
                                                 T *outPtr,
                                                 int *outExt,
                                                 vtkIdType *outInc,
                                                 int numscalars,
                                                 vtkIGSIOPasteSliceIntoVolume::CompoundingType compoundingMode,
                                                 unsigned short *accPtr,
                                                 unsigned char *&importancePtr,
                                                 unsigned int *accOverflowCount,
                                                 double pixelRejectionThreshold)
{
  bool pixelRejectionEnabled = PixelRejectionEnabled(pixelRejectionThreshold);
  double pixelRejectionThresholdSumAllComponents = 0;
  if (pixelRejectionEnabled)
  {
    pixelRejectionThresholdSumAllComponents = pixelRejectionThreshold * numscalars;
  }

  outPoint[0] = outPoint1[0] + xIntersectionPixStart*xAxis[0] - outExt[0];
  outPoint[1] = outPoint1[1] + xIntersectionPixStart*xAxis[1] - outExt[2];
  outPoint[2] = outPoint1[2] + xIntersectionPixStart*xAxis[2] - outExt[4];

  switch (compoundingMode) {
  case  vtkIGSIOPasteSliceIntoVolume::MEAN_COMPOUNDING_MODE :
    // Nearest-Neighbor, no extent checks, with accumulation
    for (int idX = xIntersectionPixStart; idX <= xIntersectionPixEnd; idX++)
    {
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
          inPtr += numscalars;
          outPoint[0] += xAxis[0];
          outPoint[1] += xAxis[1];
          outPoint[2] += xAxis[2];
          continue;
        }
      }

      int outIdX = igsioMath::Round(outPoint[0]);
      int outIdY = igsioMath::Round(outPoint[1]);
      int outIdZ = igsioMath::Round(outPoint[2]);

      int inc = outIdX*outInc[0] + outIdY*outInc[1] + outIdZ*outInc[2];
      T *outPtr1 = outPtr + inc;
      // divide by outInc[0] to accomodate for the difference
      // in the number of scalar pointers between the output
      // and the accumulation buffer
      unsigned short *accPtr1 = accPtr + (inc/outInc[0]);

      if (*accPtr1 <= ACCUMULATION_THRESHOLD) { // no overflow, act normally

        unsigned short newa = *accPtr1 + ((unsigned short)(ACCUMULATION_MULTIPLIER));

        if (newa > ACCUMULATION_THRESHOLD)
          (*accOverflowCount) += 1;

        int i = numscalars;
        do 
        {
          i--;
          *outPtr1 = ((*inPtr++)*ACCUMULATION_MULTIPLIER + (*outPtr1)*(*accPtr1))/newa;
          outPtr1++;
        }
        while (i);

        *accPtr1 = ACCUMULATION_MAXIMUM;
        if (newa < ACCUMULATION_MAXIMUM)
        {
          *accPtr1 = newa;
        }
      } else { // overflow, use recursive filtering with 255/256 and 1/256 as the weights, since 255 voxels have been inserted so far
        // TODO : This doesn't iterate through the scalars, this could be a problem
        *outPtr1 = (T)( (*inPtr++)*fraction1_256 + (*outPtr1)*fraction255_256 );
      }

      outPoint[0] += xAxis[0];
      outPoint[1] += xAxis[1];
      outPoint[2] += xAxis[2];
    }
    break;
  case  vtkIGSIOPasteSliceIntoVolume::IMPORTANCE_MASK_COMPOUNDING_MODE :
    // Nearest-Neighbor, no extent checks, with accumulation
    for (int idX = xIntersectionPixStart; idX <= xIntersectionPixEnd; idX++)
    {
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
          inPtr += numscalars;
          outPoint[0] += xAxis[0];
          outPoint[1] += xAxis[1];
          outPoint[2] += xAxis[2];
          importancePtr++;
          continue;
        }
      }

      int outIdX = igsioMath::Round(outPoint[0]);
      int outIdY = igsioMath::Round(outPoint[1]);
      int outIdZ = igsioMath::Round(outPoint[2]);

      int inc = outIdX*outInc[0] + outIdY*outInc[1] + outIdZ*outInc[2];
      T *outPtr1 = outPtr + inc;
      // divide by outInc[0] to accomodate for the difference
      // in the number of scalar pointers between the output
      // and the accumulation buffer
      unsigned short *accPtr1 = accPtr + (inc/outInc[0]);

      if (*accPtr1 <= ACCUMULATION_THRESHOLD) { // no overflow, act normally

        if (*importancePtr == 0)
        {
          //nothing to do
          break;
        }
        unsigned short newa = *accPtr1 + *importancePtr;

        if (newa > ACCUMULATION_THRESHOLD)
        {
          (*accOverflowCount) += 1;
        }

        int i = numscalars;
        do 
        {
          i--;
          *outPtr1 = ((*inPtr++)*(*importancePtr) + (*outPtr1)*(*accPtr1))/newa;
          outPtr1++;
        }
        while (i);
        importancePtr++;

        *accPtr1 = ACCUMULATION_MAXIMUM;
        if (newa < ACCUMULATION_MAXIMUM)
        {
          *accPtr1 = newa;
        }
      }
      else
      {
        // overflow, use recursive filtering with 255/256 and 1/256 as the weights, since 255 voxels have been inserted so far
        // TODO : This doesn't iterate through the scalars, this could be a problem
        *outPtr1 = (T)( (*inPtr++)*fraction1_256 + (*outPtr1)*fraction255_256 );
      }

      outPoint[0] += xAxis[0];
      outPoint[1] += xAxis[1];
      outPoint[2] += xAxis[2];
    }
    break;
  case  vtkIGSIOPasteSliceIntoVolume::MAXIMUM_COMPOUNDING_MODE :
    for (int idX = xIntersectionPixStart; idX <= xIntersectionPixEnd; idX++)
    {
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
          inPtr += numscalars;
          outPoint[0] += xAxis[0];
          outPoint[1] += xAxis[1];
          outPoint[2] += xAxis[2];
          continue;
        }
      }

      int outIdX = igsioMath::Round(outPoint[0]);
      int outIdY = igsioMath::Round(outPoint[1]);
      int outIdZ = igsioMath::Round(outPoint[2]);

      int inc = outIdX*outInc[0] + outIdY*outInc[1] + outIdZ*outInc[2];
      T *outPtr1 = outPtr + inc;
      // divide by outInc[0] to accomodate for the difference
      // in the number of scalar pointers between the output
      // and the accumulation buffer
      unsigned short *accPtr1 = accPtr + (inc/outInc[0]);
      int i = numscalars;
      do 
      {
        i--;
        if (*outPtr1 < *inPtr)
          *outPtr1 = *inPtr;
        outPtr1++;
        inPtr++;
      }
      while (i);

      *accPtr1 = (unsigned short)ACCUMULATION_MULTIPLIER;

      outPoint[0] += xAxis[0];
      outPoint[1] += xAxis[1];
      outPoint[2] += xAxis[2];
    }
    break;
  case  vtkIGSIOPasteSliceIntoVolume::LATEST_COMPOUNDING_MODE :
    for (int idX = xIntersectionPixStart; idX <= xIntersectionPixEnd; idX++)
    {
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
          inPtr += numscalars;
          outPoint[0] += xAxis[0];
          outPoint[1] += xAxis[1];
          outPoint[2] += xAxis[2];
          continue;
        }
      }

      int outIdX = igsioMath::Round(outPoint[0]);
      int outIdY = igsioMath::Round(outPoint[1]);
      int outIdZ = igsioMath::Round(outPoint[2]);

      int inc = outIdX*outInc[0] + outIdY*outInc[1] + outIdZ*outInc[2];
      T *outPtr1 = outPtr + inc;
      // divide by outInc[0] to accomodate for the difference
      // in the number of scalar pointers between the output
      // and the accumulation buffer
      unsigned short *accPtr1 = accPtr + (inc/outInc[0]);
      int i = numscalars;
      do 
      {
        i--;
        *outPtr1 = *inPtr;
        outPtr1++;
        inPtr++;
      }
      while (i);

      *accPtr1 = (unsigned short)ACCUMULATION_MULTIPLIER;

      outPoint[0] += xAxis[0];
      outPoint[1] += xAxis[1];
      outPoint[2] += xAxis[2];
    }
    break;
  default:
    LOG_ERROR("Unknown Compounding operator detected, value " << compoundingMode << ". Leaving value as-is.");
    break;
  }
}

//----------------------------------------------------------------------------
/*! Actually inserts the slice, with optimization */
template <class F, class T>
static void vtkOptimizedInsertSlice(vtkIGSIOPasteSliceIntoVolumeInsertSliceParams* insertionParams)
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
  LOG_TRACE("sliceToOutputVolumeMatrix="<<(float)matrix[0]<<" "<<(float)matrix[1]<<" "<<(float)matrix[2]<<" "<<(float)matrix[3]<<"; "
    <<(float)matrix[4]<<" "<<(float)matrix[5]<<" "<<(float)matrix[6]<<" "<<(float)matrix[7]<<"; "
    <<(float)matrix[8]<<" "<<(float)matrix[9]<<" "<<(float)matrix[10]<<" "<<(float)matrix[11]<<"; "
    <<(float)matrix[12]<<" "<<(float)matrix[13]<<" "<<(float)matrix[14]<<" "<<(float)matrix[15]
    );

  // details specified by the user RE: how the voxels should be computed
  vtkIGSIOPasteSliceIntoVolume::InterpolationType interpolationMode = insertionParams->interpolationMode;   // linear or nearest neighbor
  vtkIGSIOPasteSliceIntoVolume::CompoundingType compoundingMode = insertionParams->compoundingMode;         // weighted average or maximum

  // parameters for clipping
  double* clipRectangleOrigin = insertionParams->clipRectangleOrigin; // array size 2
  double* clipRectangleSize = insertionParams->clipRectangleSize; // array size 2
  double* fanAnglesDeg = insertionParams->fanAnglesDeg; // array size 2
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
  int clipExt[6]={0};
  GetClipExtent(clipExt, inOrigin, inSpacing, inExt, clipRectangleOrigin, clipRectangleSize);

  // find maximum output range = output extent
  int outExt[6]={0};
  outData->GetExtent(outExt);

  // Get increments to march through data - ex move from the end of one x scanline of data to the
  // start of the next line
  vtkIdType outInc[3]={0};
  outData->GetIncrements(outInc);
  vtkIdType inIncX=0, inIncY=0, inIncZ=0;
  inData->GetContinuousIncrements(inExt, inIncX, inIncY, inIncZ);
  int numscalars = inData->GetNumberOfScalarComponents();
  vtkIdType imIncX = 0, imIncY = 0, imIncZ = 0;
  if (compoundingMode == vtkIGSIOPasteSliceIntoVolume::IMPORTANCE_MASK_COMPOUNDING_MODE)
  {
    insertionParams->importanceMask->GetContinuousIncrements(inExt, imIncX, imIncY, imIncZ);
  }

  int outMax[3];
  int outMin[3]; // the max and min values of the output extents -
  // if outextent = (x0, x1, y0, y1, z0, z1), then
  // outMax = (x1, y1, z1) and outMin = (x0, y0, z0)
  for (int i = 0; i < 3; i++)
  {
    outMin[i] = outExt[2*i];
    outMax[i] = outExt[2*i+1];
  }

  // outPoint0, outPoint1, outPoint is a fancy way of incremetally multiplying the input point by
  // the index matrix to get the output point...  Outpoint is the result
  F outPoint0[3]; // temp, see above
  F outPoint1[3]; // temp, see above
  F outPoint[3]; // this is the final output point, created using Output0 and Output1
  F xAxis[3], yAxis[3], zAxis[3], origin[3]; // the index matrix (transform), broken up into axes and an origin

  // break matrix into a set of axes IGSIO an origin
  // (this allows us to calculate the transform Incrementally)
  for (int i = 0; i < 3; i++)
  {
    int rowindex = (i<<2);
    xAxis[i]  = matrix[rowindex  ]; // remember that the matrix is the indexMatrix, and transforms
    yAxis[i]  = matrix[rowindex+1];  // output pixels to input pixels
    zAxis[i]  = matrix[rowindex+2];
    origin[i] = matrix[rowindex+3];
  }

  bool fanClippingEnabled = (fanLinePixelRatioLeft != 0 || fanLinePixelRatioRight != 0);

  bool pixelRejectionEnabled = PixelRejectionEnabled(insertionParams->pixelRejectionThreshold);
  double pixelRejectionThresholdSumAllComponents = 0;
  if (pixelRejectionEnabled)
  {
    pixelRejectionThresholdSumAllComponents = insertionParams->pixelRejectionThreshold * numscalars;
  }

  int xIntersectionPixStart,xIntersectionPixEnd;

  // Loop through INPUT pixels - remember this is a 3D cube represented by the input extent
  for (int idZ = inExt[4]; idZ <= inExt[5]; idZ++) // for each image...
  {
    outPoint0[0] = origin[0]+idZ*zAxis[0]; // incremental transform
    outPoint0[1] = origin[1]+idZ*zAxis[1];
    outPoint0[2] = origin[2]+idZ*zAxis[2];

    for (int idY = inExt[2]; idY <= inExt[3]; idY++) // for each horizontal line in the image...
    {
      outPoint1[0] = outPoint0[0]+idY*yAxis[0]; // incremental transform
      outPoint1[1] = outPoint0[1]+idY*yAxis[1];
      outPoint1[2] = outPoint0[2]+idY*yAxis[2];

      // find intersections of x raster line with the output extent

      // this only changes xIntersectionPixStart and xIntersectionPixEnd
      vtkUltraFindExtent(xIntersectionPixStart,xIntersectionPixEnd,outPoint1,xAxis,outMin,outMax,inExt);

      // next, handle the 'fan' shape of the input
      double y = idY - fanOriginInPixels[1];

      // first, check the angle range of the fan - choose xIntersectionPixStart and xIntersectionPixEnd based
      // on the triangle that the fan makes from the fan origin to the bottom
      // line of the video image
      bool skipMiddleSegment = false;
      int xSkipMiddleSegmentPixStart; // first pixel that should be skipped in the middle
      int xSkipMiddleSegmentPixEnd; // last pixel that should be skipped in the middle
      if (fanClippingEnabled && xIntersectionPixStart <= xIntersectionPixEnd)
      {
        // equivalent to: xIntersectionPixStart < igsioMath::Ceil(fanLinePixelRatioLeft*y + fanOriginInPixels[0] + 1)
        // this is what the radius would be based on tan(fanAngle)
        if (xIntersectionPixStart < -igsioMath::Floor(-(fanLinePixelRatioLeft*y + fanOriginInPixels[0] + 1)))
        {
          xIntersectionPixStart = -igsioMath::Floor(-(fanLinePixelRatioLeft*y + fanOriginInPixels[0] + 1));
        }
        if (xIntersectionPixEnd > igsioMath::Floor(fanLinePixelRatioRight*y + fanOriginInPixels[0] - 1))
        {
          xIntersectionPixEnd = igsioMath::Floor(fanLinePixelRatioRight*y + fanOriginInPixels[0] - 1);
        }

        // check if we are not too close or too far from the fan origin
        double squaredDepth = (y*y)*inSpacingSquare[1];
        double dxRadiusStop = (squaredFanRadiusStop - squaredDepth);
        if (dxRadiusStop < 0)
        {
          // we are outside the fan's stop radius, ex at the bottom lines
          // we should not paste any pixels into this line
          xIntersectionPixStart = inExt[0];
          xIntersectionPixEnd = inExt[0]-1;
        }
        else
        {
          // if we are within the fan's radius, we have to adjust if we are in
          // the "ellipsoidal" (bottom) part of the fan instead of the top
          // "triangular" part
          dxRadiusStop = sqrt(dxRadiusStop/inSpacingSquare[0]);
          // this is what xIntersectionPixStart would be if we calculated it based on the
          // pythagorean theorem
          if (xIntersectionPixStart < -igsioMath::Floor(-(fanOriginInPixels[0] - dxRadiusStop + 1)))
          {
            xIntersectionPixStart = -igsioMath::Floor(-(fanOriginInPixels[0] - dxRadiusStop + 1));
          }
          if (xIntersectionPixEnd > igsioMath::Floor(fanOriginInPixels[0] + dxRadiusStop - 1))
          {
            xIntersectionPixEnd = igsioMath::Floor(fanOriginInPixels[0] + dxRadiusStop - 1);
          }
          double dxRadiusStart = (squaredFanRadiusStart - squaredDepth);
          if (dxRadiusStart > 0)
          {
            // we are inside the fan't start radius (near the transducer surface)
            // we have to skip some center pixels
            dxRadiusStart = sqrt(dxRadiusStart/inSpacingSquare[0]);
            xSkipMiddleSegmentPixStart = -igsioMath::Floor(-(fanOriginInPixels[0] - dxRadiusStart + 1));
            xSkipMiddleSegmentPixEnd = igsioMath::Floor(fanOriginInPixels[0] + dxRadiusStart - 1);
            skipMiddleSegment = true;
          }

        }
      }

      // bound to the ultrasound clip rectangle
      if (xIntersectionPixStart < clipExt[0])
      {
        xIntersectionPixStart = clipExt[0];
      }
      if (xIntersectionPixEnd > clipExt[1])
      {
        xIntersectionPixEnd = clipExt[1];
      }

      if (xIntersectionPixStart > xIntersectionPixEnd || idY < clipExt[2] || idY > clipExt[3])
      {
        xIntersectionPixStart = inExt[0];
        xIntersectionPixEnd = inExt[0]-1;
      }

      // If the middle segment is on either side then we don't have to remove the middle segment
      // we can just reduce the intersection on one side
      if (skipMiddleSegment)
      {
        if (xSkipMiddleSegmentPixStart<xIntersectionPixStart)
        {
          if (xIntersectionPixStart < xSkipMiddleSegmentPixEnd+1)
          {
            xIntersectionPixStart = xSkipMiddleSegmentPixEnd+1; // without this it's OK
            if (xIntersectionPixEnd<xIntersectionPixStart)
            {
              // there is no section to fill
              xIntersectionPixEnd=xIntersectionPixStart-1;
            }
          }
          skipMiddleSegment = false;
        }
        else if (xSkipMiddleSegmentPixEnd>xIntersectionPixEnd)
        {
          if (xIntersectionPixEnd > xSkipMiddleSegmentPixStart-1)
          {
            xIntersectionPixEnd = xSkipMiddleSegmentPixStart-1;
            if (xIntersectionPixEnd<xIntersectionPixStart)
            {
              // there is no section to fill
              xIntersectionPixEnd=xIntersectionPixStart-1;
            }
          }
          skipMiddleSegment = false;
        }
      }

      // skip the portion of the slice to the left of the fan
      for (int idX = inExt[0]; idX < xIntersectionPixStart; idX++)
      {
        inPtr += numscalars;
        importancePtr++;
      }
      // multiplying the input point by the transform will give you fractional pixels,
      // so we need interpolation
      
      if (interpolationMode == vtkIGSIOPasteSliceIntoVolume::LINEAR_INTERPOLATION)
      { 
        if (skipMiddleSegment)
        {
          for (int idX = xIntersectionPixStart; idX < xSkipMiddleSegmentPixStart; idX++) // for all of the x pixels within the fan before the skipped middle section
          {
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
                inPtr += numscalars; // go to the next x pixel
                importancePtr++;
                continue;
              }
            }

            outPoint[0] = outPoint1[0] + idX*xAxis[0];
            outPoint[1] = outPoint1[1] + idX*xAxis[1];
            outPoint[2] = outPoint1[2] + idX*xAxis[2];
            vtkTrilinearInterpolation(outPoint, inPtr, outPtr, accPtr, importancePtr, numscalars, compoundingMode, outExt, outInc, accOverflowCount); // hit is either 1 or 0
            inPtr += numscalars; // go to the next x pixel
            importancePtr++;
          }
          inPtr += numscalars * (xSkipMiddleSegmentPixEnd-xSkipMiddleSegmentPixStart+1);
          importancePtr += xSkipMiddleSegmentPixEnd - xSkipMiddleSegmentPixStart + 1;
          for (int idX = xSkipMiddleSegmentPixEnd+1; idX <= xIntersectionPixEnd; idX++) // for all of the x pixels within the fan after the skipped middle section
          {
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
                inPtr += numscalars; // go to the next x pixel
                importancePtr++;
                continue;
              }
            }

            outPoint[0] = outPoint1[0] + idX*xAxis[0];
            outPoint[1] = outPoint1[1] + idX*xAxis[1];
            outPoint[2] = outPoint1[2] + idX*xAxis[2];
            vtkTrilinearInterpolation(outPoint, inPtr, outPtr, accPtr, importancePtr, numscalars, compoundingMode, outExt, outInc, accOverflowCount); // hit is either 1 or 0
            inPtr += numscalars; // go to the next x pixel
            importancePtr++;
          }
        }
        else
        {
          for (int idX = xIntersectionPixStart; idX <= xIntersectionPixEnd; idX++) // for all of the x pixels within the fan
          {
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
                inPtr += numscalars; // go to the next x pixel
                importancePtr++;
                continue;
              }
            }

            outPoint[0] = outPoint1[0] + idX*xAxis[0];
            outPoint[1] = outPoint1[1] + idX*xAxis[1];
            outPoint[2] = outPoint1[2] + idX*xAxis[2];
            vtkTrilinearInterpolation(outPoint, inPtr, outPtr, accPtr, importancePtr, numscalars, compoundingMode, outExt, outInc, accOverflowCount); // hit is either 1 or 0
            inPtr += numscalars; // go to the next x pixel
            importancePtr++;
          }
        }
      }      
      else 
      {
        // interpolating with nearest neighbor
        if (skipMiddleSegment)
        {
          vtkFreehand2OptimizedNNHelper(xIntersectionPixStart, xSkipMiddleSegmentPixStart-1, outPoint, outPoint1, xAxis, 
            inPtr, outPtr, outExt, outInc,
            numscalars, compoundingMode, accPtr, importancePtr, accOverflowCount, insertionParams->pixelRejectionThreshold);
          inPtr += numscalars * (xSkipMiddleSegmentPixEnd-xSkipMiddleSegmentPixStart+1);
          importancePtr += (xSkipMiddleSegmentPixEnd - xSkipMiddleSegmentPixStart + 1);;
          vtkFreehand2OptimizedNNHelper(xSkipMiddleSegmentPixEnd+1, xIntersectionPixEnd, outPoint, outPoint1, xAxis, 
            inPtr, outPtr, outExt, outInc,
            numscalars, compoundingMode, accPtr, importancePtr, accOverflowCount, insertionParams->pixelRejectionThreshold);
        }
        else
        {
          vtkFreehand2OptimizedNNHelper(xIntersectionPixStart, xIntersectionPixEnd, outPoint, outPoint1, xAxis, 
            inPtr, outPtr, outExt, outInc,
            numscalars, compoundingMode, accPtr, importancePtr, accOverflowCount, insertionParams->pixelRejectionThreshold);
        }
      }

      // skip the portion of the slice to the right of the fan
      for (int idX = xIntersectionPixEnd+1; idX <= inExt[1]; idX++)
      {
        inPtr += numscalars;
        importancePtr++;
      }

      inPtr += inIncY; // move to the next line
      importancePtr += imIncY;
    }
    inPtr += inIncZ; // move to the next image
    importancePtr += imIncZ;
  }
}

#endif

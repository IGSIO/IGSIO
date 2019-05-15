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

/* MODIFIED JUNE 2012 by Thomas Vaughan:
A few notes about the code:

It is unclear why the variable 255 was used in many places. Following the
logic, it is the equivalent to the accumulation buffer value for inserting
a whole pixel into a single voxel. In this case it would make more sense if
the value was 256 (which is consistent with the fixed point representation).
However, as mentioned above, it is unclear what the original intent with this
number was. I changed it to 256. For readability and to make it easier to
change  later, I defined it as a preprocessor statement in this header file -
see ACCUMULATION_MULTIPLIER.

Depending on the data type used to store the accumulation buffer, there will
be a different maximum value that can be stored for any single voxel. At the
time of writing, the data type is unsigned short, meaning the maximum value
in the accumulation buffer is exactly 65535. ACCUMULATION_MAXIMUM is this
value.

The two values are used to determine a threshold for the accumulation buffer
as to when we should stop taking in pixel values (to prevent overflow). The
calculation will be correct as long as inserting the pixel does not cause
the accumulation buffer to overflow. The highest value that can be inserted
into the accumulation value is ACCUMULATION_MULTIPLIER. As long as the current
accumulation buffer value IGSIO ACCUMULATION_MULTIPLIER is less than or equal
to the maximm value, we are safe. Therefore, the threshold should be defined
as:
ACCUMULATION_THRESHOLD = (ACCUMULATION_MAXIMUM - ACCUMULATION_MULTIPLIER)

Note that, in order to deal with overflow, the code previously would take in
the additional pixel value and  calculate a weighted average, as normally done
in compounding (MEAN), but then reset the  accumulation value for the voxel to 65535
afterward. This resulted in unwanted and clearly-wrong artifacts.
*/

/*!
  \file vtkIGSIOPasteSliceIntoVolumeHelperCommon.h
  \brief Helper functions for pasting slice into volume

  Contains common interpolation and clipping functions for vtkIGSIOPasteSliceIntoVolume

  \sa vtkIGSIOPasteSliceIntoVolume, vtkIGSIOPasteSliceIntoVolumeHelperOptimized, vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized
  \ingroup IGSIOLibVolumeReconstruction
*/
#ifndef __vtkIGSIOPasteSliceIntoVolumeHelperCommon_h
#define __vtkIGSIOPasteSliceIntoVolumeHelperCommon_h

#include "igsioConfigure.h"

#include "igsioMath.h"
#include "fixed.h"
#include "float.h" // for DBL_MAX
#include <typeinfo>

class vtkImageData;

namespace
{
  static const double fraction1_256 = 1.0 / 256;
  static const double fraction255_256 = 255.0 / 256;
}

// regarding these values, see comments at the top of this file by Thomas Vaughan
#define ACCUMULATION_MULTIPLIER 256
#define ACCUMULATION_MAXIMUM 65535
#define ACCUMULATION_THRESHOLD 65279 // calculate manually to possibly save on computation time

#define PIXEL_REJECTION_DISABLED (-DBL_MAX)

bool PixelRejectionEnabled(double threshold) { return threshold > PIXEL_REJECTION_DISABLED + DBL_MIN * 200; }

/*!
  These are the parameters that are supplied to any given "InsertSlice" function, whether it be
  optimized or unoptimized.
 */
struct vtkIGSIOPasteSliceIntoVolumeInsertSliceParams
{
  // information on the volume
  vtkImageData* outData;            // the output volume
  void* outPtr;                     // scalar pointer to the output volume over the output extent
  unsigned short* accPtr;           // scalar pointer to the accumulation buffer over the output extent
  vtkImageData* importanceMask;
  unsigned char* importancePtr;     // scalar pointer to the importance mask over the output extent
  vtkImageData* inData;             // input slice
  void* inPtr;                      // scalar pointer to the input volume over the input slice extent
  int* inExt;                       // array size 6, input slice extent (could have been split for threading)
  unsigned int* accOverflowCount;   // the number of voxels that may have error due to accumulation overflow

  // transform matrix for images -> volume
  void* matrix;

  // details specified by the user RE: how the voxels should be computed
  vtkIGSIOPasteSliceIntoVolume::InterpolationType interpolationMode;   // linear or nearest neighbor
  vtkIGSIOPasteSliceIntoVolume::CompoundingType compoundingMode;

  // parameters for clipping
  double* clipRectangleOrigin; // array size 2
  double* clipRectangleSize; // array size 2
  // parameters for cliipping for curvilinear transducers
  double* fanAnglesDeg; // array size 2
  double* fanOrigin; // array size 2, in the input image physical coordinate system
  double fanRadiusStart; // in the input image physical coordinate system
  double fanRadiusStop; // in the input image physical coordinate system

  double pixelRejectionThreshold;
};


/*!
  Implements trilinear interpolation

  Does reverse trilinear interpolation. Trilinear interpolation would use
  the pixel values to interpolate something in the middle we have the
  something in the middle and want to spread it to the discrete pixel
  values around it, in an interpolated way

  Do trilinear interpolation of the input data 'inPtr' of extent 'inExt'
  at the 'point'.  The result is placed at 'outPtr'.
  If the lookup data is beyond the extent 'inExt', set 'outPtr' to
  the background color 'background'.
  The number of scalar components in the data is 'numscalars'
*/
template <class F, class T>
static int vtkTrilinearInterpolation(F* point,
                                     T* inPtr,
                                     T* outPtr,
                                     unsigned short* accPtr,
                                     unsigned char* importancePtr,
                                     int numscalars,
                                     vtkIGSIOPasteSliceIntoVolume::CompoundingType compoundingMode,
                                     int outExt[6],
                                     vtkIdType outInc[3],
                                     unsigned int* accOverflowCount)
{
  // Determine if the output is a floating point or integer type. If floating point type then we don't round
  // the interpolated value.
  bool roundOutput = true; // assume integer output by default
  T floatValueInOutputType = 0.3;
  if (floatValueInOutputType > 0)
  {
    // output is a floating point number
    roundOutput = false;
  }

  F fx, fy, fz;

  // convert point[0] into integer component and a fraction
  int outIdX0 = igsioMath::Floor(point[0], fx);
  // point[0] is unchanged, outIdX0 is the integer (floor), fx is the float
  int outIdY0 = igsioMath::Floor(point[1], fy);
  int outIdZ0 = igsioMath::Floor(point[2], fz);

  int outIdX1 = outIdX0 + (fx != 0); // ceiling
  int outIdY1 = outIdY0 + (fy != 0);
  int outIdZ1 = outIdZ0 + (fz != 0);

  // at this point in time we have the floor (outIdX0), the ceiling (outIdX1)
  // and the fractional component (fx) for x, y and z

  // bounds check
  if ((outIdX0 | (outExt[1] - outExt[0] - outIdX1) |
       outIdY0 | (outExt[3] - outExt[2] - outIdY1) |
       outIdZ0 | (outExt[5] - outExt[4] - outIdZ1)) >= 0)
  {
    // do reverse trilinear interpolation
    vtkIdType factX0 = outIdX0 * outInc[0];
    vtkIdType factY0 = outIdY0 * outInc[1];
    vtkIdType factZ0 = outIdZ0 * outInc[2];
    vtkIdType factX1 = outIdX1 * outInc[0];
    vtkIdType factY1 = outIdY1 * outInc[1];
    vtkIdType factZ1 = outIdZ1 * outInc[2];

    vtkIdType factY0Z0 = factY0 + factZ0;
    vtkIdType factY0Z1 = factY0 + factZ1;
    vtkIdType factY1Z0 = factY1 + factZ0;
    vtkIdType factY1Z1 = factY1 + factZ1;

    // increment between the output pointer and the 8 pixels to work on
    vtkIdType idx[8];
    idx[0] = factX0 + factY0Z0;
    idx[1] = factX0 + factY0Z1;
    idx[2] = factX0 + factY1Z0;
    idx[3] = factX0 + factY1Z1;
    idx[4] = factX1 + factY0Z0;
    idx[5] = factX1 + factY0Z1;
    idx[6] = factX1 + factY1Z0;
    idx[7] = factX1 + factY1Z1;

    // remainders from the fractional components - difference between the fractional value and the ceiling
    F rx = 1 - fx;
    F ry = 1 - fy;
    F rz = 1 - fz;

    F ryrz = ry * rz;
    F ryfz = ry * fz;
    F fyrz = fy * rz;
    F fyfz = fy * fz;

    F fdx[8]; // fdx is the weight towards the corner
    fdx[0] = rx * ryrz;
    fdx[1] = rx * ryfz;
    fdx[2] = rx * fyrz;
    fdx[3] = rx * fyfz;
    fdx[4] = fx * ryrz;
    fdx[5] = fx * ryfz;
    fdx[6] = fx * fyrz;
    fdx[7] = fx * fyfz;

    F f, r, a;
    T* inPtrTmp, *outPtrTmp;

    unsigned short* accPtrTmp;

    // loop over the eight voxels
    int j = 8;
    do
    {
      j--;
      if (fdx[j] == 0)
      {
        continue;
      }
      inPtrTmp = inPtr;
      outPtrTmp = outPtr + idx[j];
      accPtrTmp = accPtr + ((idx[j] / outInc[0]));
      a = *accPtrTmp;

      int i = numscalars;
      do
      {
        i--;
        switch (compoundingMode)
        {
          case vtkIGSIOPasteSliceIntoVolume::MAXIMUM_COMPOUNDING_MODE:
          {
            const F minWeight(0.125); // If a pixel is right in the middle of the eight surrounding voxels
            // (trilinear weight = 0.125 for each), then it the compounding operator
            // should be applied for each. Else, it should only be considered
            // for the other nearest voxels.
            if (fdx[j] >= minWeight && *inPtrTmp > *outPtrTmp)
            {
              *outPtrTmp = (*inPtrTmp);
              f = fdx[j];
              a = f * ACCUMULATION_MULTIPLIER;;
            }
            break;
          }
          case vtkIGSIOPasteSliceIntoVolume::LATEST_COMPOUNDING_MODE:
          {
            const F minWeight(0.125); // If a pixel is right in the middle of the eight surrounding voxels
            // (trilinear weight = 0.125 for each), then it the compounding operator
            // should be applied for each. Else, it should only be considered
            // for the other nearest voxels.
            if (fdx[j] >= minWeight)
            {
              *outPtrTmp = (*inPtrTmp);
              f = fdx[j];
              a = f * ACCUMULATION_MULTIPLIER;;
            }
            break;
          }
          case vtkIGSIOPasteSliceIntoVolume::MEAN_COMPOUNDING_MODE:
            f = fdx[j];
            r = F((*accPtrTmp) / (double)ACCUMULATION_MULTIPLIER); // added division by double, since this always returned 0 otherwise
            a = f + r;
            if (roundOutput)
            {
              igsioMath::Round((f * (*inPtrTmp) + r * (*outPtrTmp)) / a, *outPtrTmp);
            }
            else
            {
              *outPtrTmp = (f * (*inPtrTmp) + r * (*outPtrTmp)) / a;
            }
            a *= ACCUMULATION_MULTIPLIER; // needs to be done for proper conversion to unsigned short for accumulation buffer
            break;
          case vtkIGSIOPasteSliceIntoVolume::IMPORTANCE_MASK_COMPOUNDING_MODE:
            f = fdx[j];
            if (*importancePtr == 0)
            {
              break;
            }
            a = F((*importancePtr) * f + *accPtrTmp);
            if (typeid(F) == typeid(fixed))
            {
              //multiplying (*accPtrTmp)*(*outPtrTmp) tends to overflow fixed point type, so divide in-between
              //splitting like this incurs two divisions, but avoids overflow
              r = (*inPtrTmp) * (*importancePtr) * f / a + ((*accPtrTmp) / a) * (*outPtrTmp);
            }
            else // with float just one division is used
            {
              r = F((*inPtrTmp) * (*importancePtr) * f + (*outPtrTmp) * (*accPtrTmp)) / a;
            }
            if (roundOutput)
            {
              igsioMath::Round(r, *outPtrTmp);
            }
            else
            {
              *outPtrTmp = r;
            }
            break;
          default:
            LOG_ERROR("Unknown Compounding operator detected, value " << compoundingMode << ". Leaving value as-is.");
            break;
        }
        inPtrTmp++;
        outPtrTmp++;
      }
      while (i); // number of scalars

      F newa = a;
      if (newa > ACCUMULATION_THRESHOLD && *accPtrTmp <= ACCUMULATION_THRESHOLD)
      {
        (*accOverflowCount) += 1;
      }

      // don't allow accumulation buffer overflow
      *accPtrTmp = ACCUMULATION_MAXIMUM;
      if (newa < ACCUMULATION_MAXIMUM)
      {
        // round the fixed point to the nearest whole unit, and save the result as an unsigned short into the accumulation buffer
        igsioMath::Round(newa, *accPtrTmp);
      }
    }
    while (j);
    return 1;
  }
  // if bounds check fails
  return 0;
}


//----------------------------------------------------------------------------
/*!
  Convert the ClipRectangle into a clip extent that can be applied to the
  input data - number of pixels (+ or -) from the origin (the z component
  is copied from the inExt parameter)

  \param clipExt {x0, x1, y0, y1, z0, z1} the "output" of this function is to change this array
  \param inOrigin = {x, y, z} the origin in mm
  \param inSpacing = {x, y, z} the spacing in mm
  \param inExt = {x0, x1, y0, y1, z0, z1} min/max possible extent, in pixels
  \param clipRectangleOrigin = {x, y} origin of the clipping rectangle in the image, in pixels
  \param clipRectangleSize = {x, y} size of the clipping rectangle in the image, in pixels
*/
void GetClipExtent(int clipExt[6],
                   double inOrigin[3],
                   double inSpacing[3],
                   const int inExt[6],
                   double clipRectangleOrigin[2],
                   double clipRectangleSize[2])
{
  // Map the clip rectangle (millimetres) to pixels
  // --> number of pixels (+ or -) from the origin

  int x0 = (int)ceil((clipRectangleOrigin[0] - inOrigin[0]) / inSpacing[0]);
  int x1 = (int)floor((clipRectangleOrigin[0] - inOrigin[0] + clipRectangleSize[0]) / inSpacing[0]);
  int y0 = (int)ceil((clipRectangleOrigin[1] - inOrigin[1]) / inSpacing[1]);
  int y1 = (int)floor((clipRectangleOrigin[1] - inOrigin[1] + clipRectangleSize[1]) / inSpacing[1]);

  // Make sure that x0 <= x1 and y0 <= y1
  if (x0 > x1)
  {
    int tmp = x0;
    x0 = x1;
    x1 = tmp;
  }
  if (y0 > y1)
  {
    int tmp = y0;
    y0 = y1;
    y1 = tmp;
  }

  // make sure the clip extent lies within the input extent
  if (x0 < inExt[0])
  {
    x0 = inExt[0];
  }
  if (x1 > inExt[1])
  {
    x1 = inExt[1];
  }
  // clip extent was outside of range of input extent
  if (x0 > x1)
  {
    x0 = inExt[0];
    x1 = inExt[0] - 1;
  }

  if (y0 < inExt[2])
  {
    y0 = inExt[2];
  }
  if (y1 > inExt[3])
  {
    y1 = inExt[3];
  }
  // clip extent was outside of range of input extent
  if (y0 > y1)
  {
    y0 = inExt[2];
    y1 = inExt[2] - 1;
  }

  // Set the clip extent
  clipExt[0] = x0;
  clipExt[1] = x1;
  clipExt[2] = y0;
  clipExt[3] = y1;
  clipExt[4] = inExt[4];
  clipExt[5] = inExt[5];
}

#endif
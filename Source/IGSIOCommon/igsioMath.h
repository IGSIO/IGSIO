/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __IGSIOMATH_H
#define __IGSIOMATH_H

//#include "PlusConfigure.h"
#include "vtkigsiocommon_export.h"
#include "igsioCommon.h"

#include <vector>

#include "vtkMath.h"

class vtkMatrix4x4;
class vtkTransform;

#if _MSC_VER == 1600 // VS 2010
namespace std
{
  double VTKIGSIOCOMMON_EXPORT round(double arg);
}
#endif

/*!
  \class igsioMath
  \brief A utility class that contains static functions for various useful commonly used computations
  \ingroup PlusLibCommon
*/
class VTKIGSIOCOMMON_EXPORT igsioMath
{
public:

  /*! Returns the distance between a line, defined by two point (x and y) and a point (z) */
  static double ComputeDistanceLinePoint(const double x[3], const double y[3], const double z[3]);

  /*!
  Spherical linear interpolation between two rotation quaternions.
  Precondition: no aliasing problems to worry about ("result" can be "from" or "to" param).
  \param result Interpolated quaternion
  \param from Input quaternion
  \param to Input quaternion
  \param t Value between 0 and 1 that interpolates between from and to (t=0 means the results is the same as "from").
  \param adjustSign If true, then slerp will operate by adjusting the sign of the slerp to take shortest path
  References: From Adv Anim and Rendering Tech. Pg 364
  */
  static void Slerp(double *result, double t, double *from, double *to, bool adjustSign = true);

  /*
  This function constrain an orientation in 3DOF to 2DOF (rotation around two axes)
  This function is given a rotation axis vector ("down" vector, in the sensor coordinate system;
  the down vector equals the acceleration direction if the sensor is not moving).
  The updated orientation will be rotated along this rotation axis so that the specified (notRotatingAxisIndex-th) axis
  points to the (0,1,0) orientation in the SouthWestDown coordinate system.
  The input sensorToSouthWestDownTransform is updated with the constrained transformation matrix.
  This can be used for setting the West direction explicitly when a compass is not used for getting the rotation around the Down axis.
  The notRotatingAxisIndex is the index of the sensor axis that will be constrained to always point to the West direction.
  */
  static igsioStatus ConstrainRotationToTwoAxes(double downVector_Sensor[3], int notRotatingAxisIndex, vtkMatrix4x4* sensorToSouthWestDownTransform);

  /*! Returns a string containing the parameters (rotation, translation, scaling) from a transformation */
  static std::string GetTransformParametersString(vtkTransform* transform);

  /*! Returns a string containing the parameters (rotation, translation, scaling) from a vtkMatrix */
  static std::string GetTransformParametersString(vtkMatrix4x4* matrix);

  /*! Print VTK matrix into STL stream */
  static void PrintVtkMatrix(vtkMatrix4x4* matrix, std::ostringstream &stream, int precision = 3);

  /*! Print VTK matrix into log as info */
  static void LogVtkMatrix(vtkMatrix4x4* matrix, int precision = 3);

  // Fast floor implementation. Adopted from vtkImageReslice.

  // The 'floor' function is slow, so we want to do an integer
  // cast but keep the "floor" behavior of always rounding down,
  // rather than truncating, i.e. we want -0.6 to become -1.
  // The easiest way to do this is to add a large value in
  // order to make the value "unsigned", then cast to int, and
  // then subtract off the large value.

  // On the old i386 architecture, even a cast to int is very
  // expensive because it requires changing the rounding mode
  // on the FPU.  So we use a bit-trick similar to the one
  // described at http://www.stereopsis.com/FPU.html

  // Note that these floor/ceil/round functions do not always return the
  // same value as floor(), when it is slightly less than an integer.
  // e.g.,
  // double x = 16.999995;
  // PlusMath::Floor(x)) => 17
  // vtkMath::Floor(x))  => 16
  // floor(x)            => 16

#if defined ia64 || defined __ia64__ || defined _M_IA64
#define VTK_RESLICE_64BIT_FLOOR
#elif defined __ppc64__ || defined __x86_64__ || defined _M_X64
#define VTK_RESLICE_64BIT_FLOOR
#elif defined __ppc__ || defined sparc || defined mips
#define VTK_RESLICE_32BIT_FLOOR
#elif defined i386 || defined _M_IX86
#define VTK_RESLICE_I386_FLOOR
#endif

  // We add a tolerance of 2^-17 (around 7.6e-6) so that float
  // values that are just less than the closest integer are
  // rounded up.  This adds robustness against rounding errors.

#define VTK_RESLICE_FLOOR_TOL 7.62939453125e-06

  /*! Performance-optimized Floor computation. May return slightly incorrect value (when x is slightly smaller than an integer, it may return the ceil). Adopted from vtkImageReslice. */
  static inline int Floor(double x)
  {
#if defined VTK_RESLICE_64BIT_FLOOR
    x += (103079215104.0 + VTK_RESLICE_FLOOR_TOL);
#ifdef VTK_TYPE_USE___INT64
    __int64 i = static_cast<__int64>(x);
    return static_cast<int>(i - 103079215104i64);
#else
    long long i = static_cast<long long>(x);
    return static_cast<int>(i - 103079215104LL);
#endif
#elif defined VTK_RESLICE_32BIT_FLOOR
    x += (2147483648.0 + VTK_RESLICE_FLOOR_TOL);
    unsigned int i = static_cast<unsigned int>(x);
    return static_cast<int>(i - 2147483648U);
#elif defined VTK_RESLICE_I386_FLOOR
    union { double d; unsigned short s[4]; unsigned int i[2]; } dual;
    dual.d = x + 103079215104.0;  // (2**(52-16))*1.5
    return static_cast<int>((dual.i[1] << 16) | ((dual.i[0]) >> 16));
#else
    int i = vtkMath::Floor(x + VTK_RESLICE_FLOOR_TOL);
    return i;
#endif
  }

  /*! Performance-optimized Round computation. May return slightly incorrect value (when x is slightly smaller than ???.5, it may round to the ceil). Adopted from vtkImageReslice. */
  static inline int Round(double x)
  {
#if defined VTK_RESLICE_64BIT_FLOOR
    x += (103079215104.5 + VTK_RESLICE_FLOOR_TOL);
#ifdef VTK_TYPE_USE___INT64
    __int64 i = static_cast<__int64>(x);
    return static_cast<int>(i - 103079215104i64);
#else
    long long i = static_cast<long long>(x);
    return static_cast<int>(i - 103079215104LL);
#endif
#elif defined VTK_RESLICE_32BIT_FLOOR
    x += (2147483648.5 + VTK_RESLICE_FLOOR_TOL);
    unsigned int i = static_cast<unsigned int>(x);
    return static_cast<int>(i - 2147483648U);
#elif defined VTK_RESLICE_I386_FLOOR
    union { double d; unsigned int i[2]; } dual;
    dual.d = x + 103079215104.5;  // (2**(52-16))*1.5
    return static_cast<int>((dual.i[1] << 16) | ((dual.i[0]) >> 16));
#else
    return vtkMath::Floor(x + (0.5 + VTK_RESLICE_FLOOR_TOL));
#endif
  }

  /*! Ceiling operation optimized for speed, may return slightly incorrect value*/
  static inline int Ceil(double x)
  {
    return -Floor(-x - 1.0) - 1;
  }

  /*!
  Convert a float into an integer plus a fraction, optimized for speed, may return slightly incorrect value
  \param x Input floating point value
  \param f Output fraction part
  \return Output integer part
  */
  template <class F>
  static inline int Floor(F x, F &f)
  {
    int ix = Floor(x);
    f = x - ix;
    return ix;
  }

  /*!
  Round operation that copies the result to a specified memory address, optimized for speed, may return slightly incorrect value
  \param val Input floating point value
  \param rnd Output rounded value
  */
  template <class F, class T>
  static inline void Round(F val, T& rnd)
  {
    rnd = Round(val);
  }

  /*!
  Convenience function to compute mean and standard deviation for a vector of doubles
  \param values Input values
  \param mean Computed mean
  \param stdev Computed standard deviation
  */
  static igsioStatus ComputeMeanAndStdev(const std::vector<double> &values, double &mean, double &stdev);

  /*!
  Convenience function to compute mean and standard deviation for a vector of doubles
  \param values Input values
  \param rms Root mean square
  */
  static igsioStatus ComputeRms(const std::vector<double> &values, double &rms);

  /*!
  Convenience function to compute a percentile (percentile % of the values are smaller than the computed value)
  \param values Input values
  \param percentile Percents of values to keep (between 0.0 and 1.0)
  */
  static igsioStatus ComputePercentile(const std::vector<double> &values, double percentileToKeep, double &valueMax, double &valueMean, double &valueStdev);

  /*! Returns the Euclidean distance between two 4x4 homogeneous transformation matrix */
  static double GetPositionDifference(vtkMatrix4x4* aMatrix, vtkMatrix4x4* bMatrix); 

  /*! Returns the orientation difference in degrees between two 4x4 homogeneous transformation matrix, in degrees. */
  static double GetOrientationDifference(vtkMatrix4x4* aMatrix, vtkMatrix4x4* bMatrix); 

protected:
  igsioMath(); 
  ~igsioMath();

private: 
  igsioMath(igsioMath const&);
  igsioMath& operator=(igsioMath const&);
};

#endif 
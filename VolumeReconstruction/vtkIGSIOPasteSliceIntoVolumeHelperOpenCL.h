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
  \file vtkIGSIOPasteSliceIntoVolumeHelperOpenCL.h
  \brief GPU-accelerated helper functions for pasting slice into volume


  \sa vtkIGSIOPasteSliceIntoVolume, vtkIGSIOPasteSliceIntoVolumeHelperCommon, vtkIGSIOPasteSliceIntoVolumeHelperOptimized, vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized
  \ingroup IGSIOLibVolumeReconstruction
*/

#ifndef __vtkIGSIOPasteSliceIntoVolumeHelperOpenCL_h
#define __vtkIGSIOPasteSliceIntoVolumeHelperOpenCL_h

#include "vtkIGSIOPasteSliceIntoVolumeHelperCommon.h"

#ifdef __APPLE__
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#include <mutex>
#include <sstream>

struct PasteSliceArgs
{
  cl_int numscalars;
  cl_int outExt[6];
  cl_double matrix[16];
};

bool vtkOpenCLHasGPU()
{
  // OpenCL initialization
  std::vector<cl::Platform> all_platforms;
  cl::Platform::get(&all_platforms);

  std::vector<cl::Device> all_devices;
  for (const auto& platform : all_platforms)
  {
    std::vector<cl::Device> platform_devices;
    platform.getDevices(CL_DEVICE_TYPE_GPU, &platform_devices);
    if (!platform_devices.empty())
    {
      return true;
    }
  }

  return false;
}

struct vtkOpenCLContextParameters
{
  int inExt[6];
  int outExt[6];
  size_t scalar_size;
  std::string matrix_type_name;
  std::string data_type_name;
  vtkIGSIOPasteSliceIntoVolume::CompoundingType compounding_mode;
  vtkIGSIOPasteSliceIntoVolume::InterpolationType interpolation_mode;
};

bool operator==(const vtkOpenCLContextParameters& one, const vtkOpenCLContextParameters& two)
{
  return
    memcmp(one.inExt, two.inExt, sizeof(one.inExt)) == 0
    && memcmp(one.outExt, two.outExt, sizeof(one.outExt)) == 0
    && one.scalar_size == two.scalar_size
    && one.matrix_type_name == two.matrix_type_name
    && one.data_type_name == two.data_type_name
    && one.compounding_mode == two.compounding_mode
    && one.interpolation_mode == two.interpolation_mode;
}

bool operator!=(const vtkOpenCLContextParameters& one, const vtkOpenCLContextParameters& two)
{
  return !(one == two);
}

/*!
  Stores the OpenCL resources that do not change between slices.

  This class is only used internally inside IGSIO, so it does not have to be exported.
*/
class vtkOpenCLContext
{
public:
  vtkOpenCLContext(vtkOpenCLContextParameters parameters);
  ~vtkOpenCLContext();

public:
  cl::Device Device;
  cl::Context Context;
  cl::Buffer SliceBuffer;
  cl::Buffer VolumeBuffer;
  cl::Buffer AccumulationBuffer;
  cl::Buffer OverflowBuffer;
  cl::Program Program;
  cl::CommandQueue Queue;

  vtkOpenCLContextParameters Parameters;
  bool OutputVolumeChanged;
};

vtkOpenCLContext::vtkOpenCLContext(vtkOpenCLContextParameters parameters) : Parameters(parameters)
{
  // OpenCL initialization
  std::vector<cl::Platform> all_platforms;
  cl::Platform::get(&all_platforms);

  std::vector<cl::Device> all_devices;
  for (const auto& platform : all_platforms)
  {
    std::vector<cl::Device> platform_devices;
    platform.getDevices(CL_DEVICE_TYPE_GPU, &platform_devices);
    all_devices.insert(all_devices.end(), platform_devices.begin(), platform_devices.end());
  }

  this->Device = all_devices[0];

  LOG_INFO("Using OpenCL platform device " << this->Device.getInfo<CL_DEVICE_NAME>());

  this->Context = cl::Context({ this->Device });

  cl::Program::Sources sources;

  std::stringstream kernel_source;
  kernel_source <<
                "struct PasteSliceArgs {\n"
                " int numscalars;\n"
                " int outExt[6];\n";
  kernel_source << " " << parameters.matrix_type_name << " matrix[16];\n";
  kernel_source << "};\n"
                "\n";
  kernel_source <<
                "kernel void paste_slice(const global " << parameters.data_type_name << "* Slice, "
                "    global " << parameters.data_type_name << "* Volume,"
                "    global unsigned short* Accumulation, "
                "    global unsigned int* OverflowCount, "
                "    struct PasteSliceArgs args)\n";
  kernel_source <<
                "{\n"
                "    size_t idX = get_global_id(0) - get_global_offset(0);\n"
                "    size_t idY = get_global_id(1) - get_global_offset(1);\n"
                "    size_t idZ = get_global_id(2) - get_global_offset(2);\n";
  kernel_source <<
                "    const global " << parameters.data_type_name << "* inPtr = Slice + args.numscalars * (idZ * get_global_size(0) * get_global_size(1) + idY * get_global_size(0) + idX);\n"
                "    // matrix multiplication - input -> output\n"
                "    " << parameters.matrix_type_name << " inPoint[4] = {\n"
                "        idX + get_global_offset(0),\n"
                "        idY + get_global_offset(1),\n"
                "        idZ + get_global_offset(2),\n"
                "        1.0\n"
                "    };\n"
                "    " << parameters.matrix_type_name << " outPoint[4];\n"
                "    for (int i = 0; i < 4; i++) {\n"
                "        int rowindex = 4 * i;\n"
                "        outPoint[i] = args.matrix[rowindex] * inPoint[0] + \n"
                "		   args.matrix[rowindex + 1] * inPoint[1] +\n"
                "          args.matrix[rowindex + 2] * inPoint[2] +\n"
                "          args.matrix[rowindex + 3] * inPoint[3];\n"
                "    }\n"

                "    // deal with w (homogeneous transform) if the transform was a perspective transform\n"
                "    for (int i = 0; i < 3; ++i) {\n"
                "        outPoint[i] = outPoint[i] / outPoint[3] - 0.02;\n"
                "    }\n"
                "    outPoint[3] = 1;\n";

  if (parameters.interpolation_mode == vtkIGSIOPasteSliceIntoVolume::LINEAR_INTERPOLATION)
  {
    // Linear interpolation
    kernel_source <<
                  " // convert point components into integer component and a fraction\n"
                  << parameters.matrix_type_name << " f[2][3], foutId[3];\n"
                  << "int outId0[3], outId[2][3];\n"
                  "for (int i = 0; i < 3; ++i) {\n"
                  "    f[1][i] = fract(outPoint[i], &foutId[i]);\n"
                  "    f[0][i] = 1.0 - f[1][i];\n"
                  "    outId[0][i] = foutId[i];\n"
                  "    outId[1][i] = ceil(outPoint[i]);\n"
                  "}\n"
                  "if ((outId[0][0] | (args.outExt[1] - args.outExt[0] - outId[1][0]) |\n"
                  "outId[0][1] | (args.outExt[3] - args.outExt[2] - outId[1][1]) |\n"
                  "outId[0][2] | (args.outExt[5] - args.outExt[4] - outId[1][2])) >= 0)\n"
                  "{\n"
                  "// do reverse trilinear interpolation\n"
                  "for (int dx = 0; dx < 2; ++dx) {\n"
                  "for (int dy = 0; dy < 2; ++dy) {\n"
                  "for (int dz = 0; dz < 2; ++dz) {\n"
                  << parameters.matrix_type_name << " weight = f[dx][0] * f[dy][1] * f[dy][2];\n"
                  "// Ignore the voxel where weight is less that one-eight\n"
                  "if (weight < 0.125) {\n"
                  "    continue;\n"
                  "}\n"
                  "int outIdX = outId[dx][0];\n"
                  "int outIdY = outId[dy][1];\n"
                  "int outIdZ = outId[dz][2];\n";
  }
  else
  {
    kernel_source <<
                  "    \n"
                  "    // The nearest neighbor interpolation occurs here\n"
                  "    // The output point is the closest point to the input point - rounding\n"
                  "    // to get closest point\n"
                  "\n"
                  "    int outIdX = round(outPoint[0]) - args.outExt[0];\n"
                  "    int outIdY = round(outPoint[1]) - args.outExt[2];\n"
                  "    int outIdZ = round(outPoint[2]) - args.outExt[4];\n"
                  "    // fancy way of checking bounds\n"
                  "    if ((outIdX | (args.outExt[1] - args.outExt[0] - outIdX) |\n"
                  "    outIdY | (args.outExt[3] - args.outExt[2] - outIdY) |\n"
                  "    outIdZ | (args.outExt[5] - args.outExt[4] - outIdZ)) >= 0) {\n"
                  "        const " << parameters.matrix_type_name << " weight = 1.0;\n";

  }
  kernel_source <<
                "        global " << parameters.data_type_name << "* outPtr = Volume + args.numscalars * (outIdX + outIdY * (1 + args.outExt[1] - args.outExt[0]) + outIdZ * (1 + args.outExt[1] - args.outExt[0]) * (1 + args.outExt[3] - args.outExt[2]));\n"
                "        for (int i = 0; i < args.numscalars; i++) {\n";
  switch (parameters.compounding_mode)
  {
    case vtkIGSIOPasteSliceIntoVolume::LATEST_COMPOUNDING_MODE:
      kernel_source <<
                    "            outPtr[i] = inPtr[i];\n";
      break;
    case vtkIGSIOPasteSliceIntoVolume::MAXIMUM_COMPOUNDING_MODE:
    default:
      kernel_source <<
                    "            if (inPtr[i] > outPtr[i]) {\n"
                    "                outPtr[i] = inPtr[i];\n"
                    "            }\n";
      break;
  }
  kernel_source <<
                "        }\n"
                "        global unsigned short* accPtr = Accumulation + (outIdX + outIdY * (1 + args.outExt[1] - args.outExt[0]) + outIdZ * (1 + args.outExt[1] - args.outExt[0]) * (1 + args.outExt[3] - args.outExt[2]));\n"
                "        int newa = *accPtr + weight * " << ACCUMULATION_MULTIPLIER << ";\n"
                "        if (newa > " << ACCUMULATION_THRESHOLD << ") {\n"
                "            *OverflowCount += 1;\n"
                "        }\n"
                "        *accPtr = min(newa, " << ACCUMULATION_MAXIMUM << ");\n";

  if (parameters.interpolation_mode == vtkIGSIOPasteSliceIntoVolume::LINEAR_INTERPOLATION)
  {
    kernel_source <<
                  "    }\n"
                  "    }\n"
                  "    }\n";
  }
  kernel_source <<
                "    }\n"
                "}\n";

  std::string kernel_source_str = kernel_source.str();

  sources.push_back({ kernel_source_str.c_str(), kernel_source_str.length() });

  cl::Program program(this->Context, sources);
  if (program.build({ this->Device }) != CL_SUCCESS)
  {
    LOG_ERROR(" Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(this->Device));
  }
  else
  {
    this->Program = program;
  }

  int* inExt = parameters.inExt;
  int* outExt = parameters.outExt;

  cl_int err = CL_SUCCESS;
  this->SliceBuffer = cl::Buffer(this->Context, CL_MEM_READ_ONLY, parameters.scalar_size * (1 + inExt[5] - inExt[4]) * (1 + inExt[3] - inExt[2]) * (1 + inExt[1] - inExt[0]), NULL, &err);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Create input slice buffer: " << err);
  }

  this->VolumeBuffer = cl::Buffer(this->Context, CL_MEM_READ_WRITE, parameters.scalar_size * (1 + outExt[5] - outExt[4]) * (1 + outExt[3] - outExt[2]) * (1 + outExt[1] - outExt[0]), NULL, &err);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Create output volume buffer: " << err);
  }

  this->AccumulationBuffer = cl::Buffer(this->Context, CL_MEM_READ_WRITE, sizeof(unsigned short) * (1 + outExt[5] - outExt[4]) * (1 + outExt[3] - outExt[2]) * (1 + outExt[1] - outExt[0]), NULL, &err);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Create accumulation buffer: " << err);
  }

  this->OverflowBuffer = cl::Buffer(this->Context, CL_MEM_READ_WRITE, sizeof(unsigned int), NULL, &err);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Create overflow buffer: " << err);
  }

  this->Queue = cl::CommandQueue(this->Context, this->Device);

  this->OutputVolumeChanged = false;
}

vtkOpenCLContext::~vtkOpenCLContext()
{
}

void vtkOpenCLComputePitch(int ext[6], int incY, int incZ, int scalar_size, int* pitchY, int* pitchZ)
{
  *pitchY = incY + (1 + ext[1] - ext[0]);
  *pitchZ = incZ + (1 + ext[3] - ext[2]) * (*pitchY);

  *pitchY *= scalar_size;
  *pitchZ *= scalar_size;
}

static void vtkOpenCLReadOutput(vtkIGSIOPasteSliceIntoVolumeInsertSliceParams* insertionParams, vtkOpenCLContext* context);

//----------------------------------------------------------------------------
/*!
  Actually inserts the slice - executes the filter for any type of data, without optimization
  Given an input and output region, execute the filter algorithm to fill the
  output from the input - no optimization.
  (this one function is pretty much the be-all and end-all of the filter)
*/
static void vtkOpenCLInsertSlice(vtkIGSIOPasteSliceIntoVolumeInsertSliceParams* insertionParams, int scalar_type, vtkOpenCLContext** opencl_context)
{
  // information on the volume
  vtkImageData* outData = insertionParams->outData;
  void* outPtr = reinterpret_cast<void*>(insertionParams->outPtr);
  unsigned short* accPtr = insertionParams->accPtr;
  unsigned char* importancePtr = insertionParams->importancePtr;
  vtkImageData* inData = insertionParams->inData;
  void* inPtr = reinterpret_cast<void*>(insertionParams->inPtr);
  int* inExt = insertionParams->inExt;
  unsigned int* accOverflowCount = insertionParams->accOverflowCount;

  // transform matrix for image -> volume
  double* matrix = reinterpret_cast<double*>(insertionParams->matrix);
  LOG_TRACE("sliceToOutputVolumeMatrix=" << matrix[0] << " " << matrix[1] << " " << matrix[2] << " " << matrix[3] << "; "
            << matrix[4] << " " << matrix[5] << " " << matrix[6] << " " << matrix[7] << "; "
            << matrix[8] << " " << matrix[9] << " " << matrix[10] << " " << matrix[11] << "; "
            << matrix[12] << " " << matrix[13] << " " << matrix[14] << " " << matrix[15]
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
    (fanOrigin[0] - inOrigin[0]) / inSpacing[0],
    (fanOrigin[1] - inOrigin[1]) / inSpacing[1]
  };
  // fan depth squared
  double squaredFanRadiusStart = fanRadiusStart * fanRadiusStart;
  double squaredFanRadiusStop = fanRadiusStop * fanRadiusStop;

  // absolute value of slice spacing
  double inSpacingSquare[2] =
  {
    inSpacing[0]* inSpacing[0],
    inSpacing[1]* inSpacing[1]
  };

  double pixelAspectRatio = fabs(inSpacing[1] / inSpacing[0]);
  // tan of the left and right fan angles
  double fanLinePixelRatioLeft = tan(vtkMath::RadiansFromDegrees(fanAnglesDeg[0])) * pixelAspectRatio;
  double fanLinePixelRatioRight = tan(vtkMath::RadiansFromDegrees(fanAnglesDeg[1])) * pixelAspectRatio;
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
  vtkIdType outInc[3] = {0};
  outData->GetIncrements(outInc);
  vtkIdType inIncX = 0, inIncY = 0, inIncZ = 0;
  inData->GetContinuousIncrements(inExt, inIncX, inIncY, inIncZ);
  int numscalars = inData->GetNumberOfScalarComponents();
  vtkIdType impIncX = 0;
  vtkIdType impIncY = 0;
  vtkIdType impIncZ = 0;
  if (insertionParams->importanceMask)
  {
    insertionParams->importanceMask->GetContinuousIncrements(inExt, impIncX, impIncY, impIncZ);
  }

  bool pixelRejectionEnabled = PixelRejectionEnabled(insertionParams->pixelRejectionThreshold);
  double pixelRejectionThresholdSumAllComponents = 0;
  if (pixelRejectionEnabled)
  {
    pixelRejectionThresholdSumAllComponents = insertionParams->pixelRejectionThreshold * numscalars;
  }

  vtkOpenCLContextParameters p;
  for (int i = 0; i < 6; ++i)
  {
    p.inExt[i] = inExt[i];
    p.outExt[i] = outExt[i];
  }
  p.matrix_type_name = "double";
  p.compounding_mode = compoundingMode;
  p.interpolation_mode = interpolationMode;

  switch (scalar_type)
  {
    case VTK_CHAR:
      p.scalar_size = sizeof(char);
      p.data_type_name = "char";
      break;
    case VTK_UNSIGNED_CHAR:
      p.scalar_size = sizeof(unsigned char);
      p.data_type_name = "unsigned char";
      break;
    case VTK_SHORT:
      p.scalar_size = sizeof(short);
      p.data_type_name = "short";
      break;
    case VTK_UNSIGNED_SHORT:
      p.scalar_size = sizeof(unsigned short);
      p.data_type_name = "unsigned short";
      break;
    case VTK_INT:
      p.scalar_size = sizeof(int);
      p.data_type_name = "int";
      break;
    case VTK_UNSIGNED_INT:
      p.scalar_size = sizeof(unsigned int);
      p.data_type_name = "unsigned int";
      break;
    case VTK_LONG:
      p.scalar_size = sizeof(long);
      p.data_type_name = "long";
      break;
    case VTK_UNSIGNED_LONG:
      p.scalar_size = sizeof(unsigned long);
      p.data_type_name = "unsigned long";
      break;
    case VTK_FLOAT:
      p.scalar_size = sizeof(float);
      p.data_type_name = "float";
      break;
    case VTK_DOUBLE:
      p.scalar_size = sizeof(double);
      p.data_type_name = "double";
      break;
    default:
      LOG_ERROR("OpenCLInsertSlice: Unknown input ScalarType");
  }

  p.scalar_size *= numscalars;

  if (*opencl_context == NULL || (*opencl_context)->Parameters != p)
  {
    static std::mutex contextInitMutex;
    std::lock_guard<std::mutex> lock(contextInitMutex);

    if (*opencl_context == NULL || (*opencl_context)->Parameters != p)
    {
      delete *opencl_context;
      *opencl_context = new vtkOpenCLContext(p);
    }
  }
  vtkOpenCLContext* context = *opencl_context;

  cl::size_t<3> buffer_origin;
  cl::size_t<3> in_origin;
  cl::size_t<3> in_region;
  cl::size_t<3> out_origin;
  cl::size_t<3> out_region;
  cl::size_t<3> acc_origin;
  cl::size_t<3> acc_region;
  for (int i = 0; i < 3; ++i)
  {
    size_t size_factor = (i == 0) ? p.scalar_size : 1;

    buffer_origin[i] = 0;

    in_origin[i] = inExt[2 * i] * size_factor;
    in_region[i] = (1 + inExt[2 * i + 1] - inExt[2 * i]) * size_factor;

    out_origin[i] = outExt[2 * i] * size_factor;
    out_region[i] = (1 + outExt[2 * i + 1] - outExt[2 * i]) * size_factor;

    size_t acc_size_factor = (i == 0) ? sizeof(unsigned short) : 1;
    acc_origin[i] = outExt[2 * i] * acc_size_factor;
    acc_region[i] = (1 + outExt[2 * i + 1] - outExt[2 * i]) * acc_size_factor;
  }

  cl_int err = CL_SUCCESS;

  int inPitchY, inPitchZ;
  vtkOpenCLComputePitch(inExt, inIncY, inIncZ, p.scalar_size, &inPitchY, &inPitchZ);

  int outPitchY = outInc[1] * p.scalar_size;
  int outPitchZ = outInc[2] * p.scalar_size;

  int accPitchY = outInc[1] * sizeof(unsigned short);
  int accPitchZ = outInc[2] * sizeof(unsigned short);

  cl::Event write_event;

  if (insertionParams->isFirst)
  {
    err = context->Queue.enqueueWriteBufferRect(context->VolumeBuffer, CL_NON_BLOCKING, buffer_origin, out_origin, out_region,
          0, 0, outPitchY, outPitchZ, outPtr);
    if (err != CL_SUCCESS)
    {
      LOG_ERROR("Write output volume buffer: " << err);
    }

    err = context->Queue.enqueueWriteBufferRect(context->AccumulationBuffer, CL_NON_BLOCKING, buffer_origin, acc_origin, acc_region,
          0, 0, accPitchY, accPitchZ, accPtr);
    if (err != CL_SUCCESS)
    {
      LOG_ERROR("Write accumulation buffer: " << err);
    }

    err = context->Queue.enqueueWriteBuffer(context->OverflowBuffer, CL_NON_BLOCKING, 0, sizeof(unsigned int), accOverflowCount);
    if (err != CL_SUCCESS)
    {
      LOG_ERROR("Write overflow buffer: " << err);
    }
  }

  err = context->Queue.enqueueWriteBufferRect(context->SliceBuffer, CL_NON_BLOCKING, buffer_origin, in_origin, in_region,
        0, 0, inPitchY, inPitchZ, inPtr, NULL, &write_event);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Write input slice buffer: " << err);
  }

  err = context->Queue.flush();
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Flush OpenCL queue: " << err);
  }

  cl::NDRange slice_range(1 + inExt[1] - inExt[0], 1 + inExt[3] - inExt[2], 1 + inExt[5] - inExt[4]);
  cl::NDRange slice_offset(inExt[0], inExt[2], inExt[4]);
  cl::Kernel paste_slice_kernel(context->Program, "paste_slice");

  err = paste_slice_kernel.setArg(0, context->SliceBuffer);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Kernel set arg 0 (Slice): " << err);
  }

  err = paste_slice_kernel.setArg(1, context->VolumeBuffer);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Kernel set arg 1 (Volume): " << err);
  }

  err = paste_slice_kernel.setArg(2, context->AccumulationBuffer);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Kernel set arg 2 (Accumulation): " << err);
  }

  err = paste_slice_kernel.setArg(3, context->OverflowBuffer);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Kernel set arg 3 (Overflow): " << err);
  }

  PasteSliceArgs args;
  args.numscalars = numscalars;
  memcpy(args.outExt, outExt, sizeof(args.outExt));
  for (int i = 0; i < 16; ++i)
  {
    args.matrix[i] = matrix[i];
  }

  err = paste_slice_kernel.setArg(4, args);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Kernel set arg 4 (Args): " << err);
  }

  err = context->Queue.enqueueNDRangeKernel(paste_slice_kernel, slice_offset, slice_range);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Launch OpenCL kernel: " << err);
  }

  context->OutputVolumeChanged = true;

  if (insertionParams->isLast)
  {
    vtkOpenCLReadOutput(insertionParams, context);
  }
  else
  {
    /* We have to wait for writes to finish so the host memory pointed to by inPtr can be reused
     * On the other hand, no need to wait for kernel to finish as that is not (yet) visible to the CPU.
     */
    err = write_event.wait();
    if (err != CL_SUCCESS)
    {
      LOG_ERROR("Finish OpenCL queue: " << err);
    }
  }
}

static void vtkOpenCLReadOutput(vtkIGSIOPasteSliceIntoVolumeInsertSliceParams* insertionParams, vtkOpenCLContext* context)
{
  if (context == NULL || !context->OutputVolumeChanged)
  {
    return;
  }

  int outExt[6];
  insertionParams->outData->GetExtent(outExt);
  vtkIdType outInc[3] = { 0 };
  insertionParams->outData->GetIncrements(outInc);

  insertionParams->outData->GetScalarSize();

  int scalar_size = insertionParams->outData->GetScalarSize() * insertionParams->outData->GetNumberOfScalarComponents();

  cl::size_t<3> buffer_origin;
  cl::size_t<3> out_origin;
  cl::size_t<3> out_region;
  cl::size_t<3> acc_origin;
  cl::size_t<3> acc_region;
  for (int i = 0; i < 3; ++i)
  {
    size_t size_factor = (i == 0) ? scalar_size : 1;

    buffer_origin[i] = 0;

    out_origin[i] = outExt[2 * i] * size_factor;
    out_region[i] = (1 + outExt[2 * i + 1] - outExt[2 * i]) * size_factor;

    size_t acc_size_factor = (i == 0) ? sizeof(unsigned short) : 1;
    acc_origin[i] = outExt[2 * i] * acc_size_factor;
    acc_region[i] = (1 + outExt[2 * i + 1] - outExt[2 * i]) * acc_size_factor;
  }

  cl_int err = CL_SUCCESS;

  int outPitchY = outInc[1] * scalar_size;
  int outPitchZ = outInc[2] * scalar_size;

  int accPitchY = outInc[1] * sizeof(unsigned short);
  int accPitchZ = outInc[2] * sizeof(unsigned short);

  err = context->Queue.enqueueReadBufferRect(context->VolumeBuffer, CL_NON_BLOCKING, buffer_origin, out_origin, out_region,
        0, 0, outPitchY, outPitchZ, insertionParams->outPtr);
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Read output volume buffer: " << err);
  }

  if (insertionParams->accPtr)
  {
    err = context->Queue.enqueueReadBufferRect(context->AccumulationBuffer, CL_NON_BLOCKING, buffer_origin, acc_origin, acc_region,
          0, 0, accPitchY, accPitchZ, insertionParams->accPtr);
    if (err != CL_SUCCESS)
    {
      LOG_ERROR("Read accumulation buffer: " << err);
    }
  }

  if (insertionParams->accOverflowCount)
  {
    err = context->Queue.enqueueReadBuffer(context->OverflowBuffer, CL_NON_BLOCKING, 0, sizeof(unsigned int), insertionParams->accOverflowCount);
    if (err != CL_SUCCESS)
    {
      LOG_ERROR("Read accumulation overflow buffer: " << err);
    }
  }

  err = context->Queue.finish();
  if (err != CL_SUCCESS)
  {
    LOG_ERROR("Finish OpenCL queue: " << err);
  }

  context->OutputVolumeChanged = false;
}

#endif

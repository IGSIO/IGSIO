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

#include <CL/cl.hpp>

#include <mutex>
#include <sstream>

struct PasteSliceArgs {
	cl_int numscalars;
	cl_int inIncX;
	cl_int inIncY;
	cl_int inIncZ;
	cl_int outExt[6];
	cl_int outInc[3];
	cl_float matrix[16];
};

bool vtkOpenCLHasGPU()
{
	// OpenCL initialization
	std::vector<cl::Platform> all_platforms;
	cl::Platform::get(&all_platforms);

	std::vector<cl::Device> all_devices;
	for (const auto& platform : all_platforms) {
		std::vector<cl::Device> platform_devices;
		platform.getDevices(CL_DEVICE_TYPE_GPU, &platform_devices);
		if (!platform_devices.empty()) {
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
};

bool operator==(const vtkOpenCLContextParameters& one, const vtkOpenCLContextParameters& two)
{
	return
		memcmp(one.inExt, two.inExt, sizeof(one.inExt)) == 0
		&& memcmp(one.outExt, two.outExt, sizeof(one.outExt)) == 0
		&& one.scalar_size == two.scalar_size
		&& one.matrix_type_name == two.matrix_type_name
		&& one.data_type_name == two.data_type_name;

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
	cl::Program Program;
	cl::CommandQueue Queue;

	vtkOpenCLContextParameters Parameters;
};

vtkOpenCLContext::vtkOpenCLContext(vtkOpenCLContextParameters parameters) : Parameters(parameters)
{
	// OpenCL initialization
	std::vector<cl::Platform> all_platforms;
	cl::Platform::get(&all_platforms);

	std::vector<cl::Device> all_devices;
	for (const auto& platform : all_platforms) {
		std::vector<cl::Device> platform_devices;
		platform.getDevices(CL_DEVICE_TYPE_GPU, &platform_devices);
		all_devices.insert(all_devices.end(), platform_devices.begin(), platform_devices.end());
	}

	this->Device = all_devices[0];

	LOG_ERROR("Using OpenCL platform device " << this->Device.getInfo<CL_DEVICE_NAME>());

	this->Context = cl::Context({ this->Device });

	cl::Program::Sources sources;

	std::stringstream kernel_source;
	kernel_source <<
		"struct PasteSliceArgs {\n"
		" int numscalars;\n"
		" int inIncX;\n"
		" int inIncY;\n"
		" int inIncZ;\n"
		" int outExt[6];\n"
		" int outInc[3];\n";
	kernel_source << " " << parameters.matrix_type_name << " matrix[16];\n";
    kernel_source << "};\n"
		"\n;";
	kernel_source <<
		"kernel void paste_slice(const global " << parameters.data_type_name << "* Slice, global " << parameters.data_type_name << "* Volume, struct PasteSliceArgs args)\n";
	kernel_source <<
		"{\n"
		"    size_t idX = get_global_id(0);\n"
		"    size_t idY = get_global_id(1);\n"
		"    size_t idZ = get_global_id(2);\n";
	kernel_source <<
		"    const global " << parameters.data_type_name << "* inPtr = Slice + args.numscalars * (idZ * args.inIncZ + idY * args.inIncY + idX * args.inIncX);\n"
		"    // matrix multiplication - input -> output\n"
		"    " << parameters.matrix_type_name << " inPoint[4] = {idX, idY, idZ, 1.0};\n"
		"    " << parameters.matrix_type_name << " outPoint[4];"
		"    for (int i = 0; i < 4; i++) {\n"
		"        int rowindex = i << 2;\n"
		"        outPoint[i] = args.matrix[rowindex] * inPoint[0] + \n"
		"		   args.matrix[rowindex + 1] * inPoint[1] +\n"
		"        args.matrix[rowindex + 2] * inPoint[2] +\n"
		"        args.matrix[rowindex + 3] * inPoint[3];\n"
		"    }\n"

		"    // deal with w (homogeneous transform) if the transform was a perspective transform\n"
		"    outPoint[0] /= outPoint[3];\n"
		"    outPoint[1] /= outPoint[3];\n"
		"    outPoint[2] /= outPoint[3];\n"
		"    outPoint[3] = 1;\n"
		"    \n"
		"    // The nearest neighbor interpolation occurs here\n"
		"    // The output point is the closest point to the input point - rounding\n"
		"    // to get closest point\n"
		"    int outIdX = round(outPoint[0]) - args.outExt[0];\n"
		"    int outIdY = round(outPoint[1]) - args.outExt[2];\n"
		"    int outIdZ = round(outPoint[2]) - args.outExt[4];\n"

		"    // fancy way of checking bounds\n"
		"    if ((outIdX | (args.outExt[1] - args.outExt[0] - outIdX) |\n"
		"    outIdY | (args.outExt[3] - args.outExt[2] - outIdY) |\n"
		"    outIdZ | (args.outExt[5] - args.outExt[4] - outIdZ)) >= 0) {\n"
		"        global " << parameters.data_type_name << "* outPtr = Volume + args.numscalars * (outIdX * args.outInc[0] + outIdY * args.outInc[1] + outIdZ * args.outInc[2]);\n"
		"        for (int i = 0; i < args.numscalars; i++) {\n"
		"            if (*inPtr > *outPtr) {\n"
		"                *outPtr = *inPtr;\n"
		"            }\n"
		"        }\n"
		"    }\n"
		"}\n";

	std::string kernel_source_str = kernel_source.str();
	sources.push_back({ kernel_source_str.c_str(), kernel_source_str.length() });

	cl::Program program(this->Context, sources);
	if (program.build({ this->Device }) != CL_SUCCESS) {
		LOG_ERROR(" Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(this->Device));
	}
	else {
		LOG_ERROR(" Success building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(this->Device));
		this->Program = program;
	}

	int* inExt = parameters.inExt;
	int* outExt = parameters.outExt;
	this->SliceBuffer = cl::Buffer(this->Context, CL_MEM_READ_ONLY, parameters.scalar_size * (inExt[5] - inExt[4]) * (inExt[3] - inExt[2]) * (inExt[1] - inExt[0]));
	this->VolumeBuffer = cl::Buffer(this->Context, CL_MEM_READ_WRITE, parameters.scalar_size * (outExt[5] - outExt[4]) * (outExt[3] - outExt[2]) * (outExt[1] - outExt[0]));

	this->Queue = cl::CommandQueue(this->Context, this->Device);
}

vtkOpenCLContext::~vtkOpenCLContext()
{
}

//----------------------------------------------------------------------------
/*!
  Actually inserts the slice - executes the filter for any type of data, without optimization
  Given an input and output region, execute the filter algorithm to fill the
  output from the input - no optimization.
  (this one function is pretty much the be-all and end-all of the filter)
*/
template <class F, class T>
static void vtkOpenCLInsertSlice(vtkIGSIOPasteSliceIntoVolumeInsertSliceParams* insertionParams, vtkOpenCLContext **opencl_context)
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
    LOG_ERROR("Unknown interpolation mode: "<<interpolationMode);
    return;
  }

  bool pixelRejectionEnabled = PixelRejectionEnabled(insertionParams->pixelRejectionThreshold);
  double pixelRejectionThresholdSumAllComponents = 0;
  if (pixelRejectionEnabled)
  {
    pixelRejectionThresholdSumAllComponents = insertionParams->pixelRejectionThreshold * numscalars;
  }

  if (*opencl_context == NULL) {
	  static std::mutex contextInitMutex;
	  std::lock_guard<std::mutex> lock(contextInitMutex);

	  vtkOpenCLContextParameters p;
	  memcpy(p.inExt, inExt, sizeof(p.inExt));
	  memcpy(p.outExt, outExt, sizeof(p.outExt));
	  p.scalar_size = sizeof(T);
	  p.matrix_type_name = "double";
	  p.data_type_name = "double";

	  if (*opencl_context == NULL || (*opencl_context)->Parameters != p) {
		  delete *opencl_context;
		  *opencl_context = new vtkOpenCLContext(p);
	  }
  }
  vtkOpenCLContext* context = *opencl_context;

  cl::size_t<3> origin;
  cl::size_t<3> region;
  for (int i = 0; i < 3; ++i) {
	  region[i] = (inExt[2 * i + 1] - inExt[2 * i]) * sizeof(T);
  }

  cl_int err = CL_SUCCESS;

  err = context->Queue.enqueueWriteBufferRect(context->SliceBuffer, CL_NON_BLOCKING, origin, origin, region,
	  (inExt[1] - inExt[0]) * sizeof(T), (inExt[1] - inExt[0]) * (inExt[3] - inExt[2]) * sizeof(T),
	  inIncY * sizeof(T), inIncZ * sizeof(T), inPtr);
  if (err != CL_SUCCESS) {
	  LOG_ERROR("Write input slice buffer: " << err);
  }

  cl::size_t<3> out_region;
  for (int i = 0; i < 3; ++i) {
	  region[i] = (outExt[2 * i + 1] - outExt[2 * i]) * sizeof(T);
  }

  err = context->Queue.enqueueWriteBufferRect(context->VolumeBuffer, CL_NON_BLOCKING, origin, origin, out_region,
	  (outExt[1] - outExt[0]) * sizeof(T), (outExt[1] - outExt[0]) * (outExt[3] - outExt[2]) * sizeof(T),
	  outInc[1] * sizeof(T), outInc[2] * sizeof(T), outPtr);
  if (err != CL_SUCCESS) {
	  LOG_ERROR("Write output volume buffer: " << err);
  }


  cl::NDRange slice_range(inExt[1] - inExt[0], inExt[3] - inExt[2], inExt[5] - inExt[4]);
  cl::Kernel paste_slice_kernel(context->Program, "paste_slice");
  paste_slice_kernel.setArg(0, context->SliceBuffer);
  paste_slice_kernel.setArg(1, context->VolumeBuffer);
  PasteSliceArgs args;
  args.numscalars = numscalars;
  args.inIncX = inIncX;
  args.inIncY = inIncY;
  args.inIncZ = inIncZ;
  memcpy(args.outExt, outExt, sizeof(args.outExt));
  memcpy(args.outInc, outInc, sizeof(args.outInc));
  for (int i = 0; i < 16; ++i) {
	  args.matrix[i] = matrix[i];
  }
  paste_slice_kernel.setArg(2, args);
  err = context->Queue.enqueueNDRangeKernel(paste_slice_kernel, cl::NullRange, slice_range, cl::NullRange);
  if (err != CL_SUCCESS) {
	  LOG_ERROR("Launch OpenCL kernel: " << err);
  }


  // Wait for the execution to finish by issuing a BLOCKING read of the output buffer
  err = context->Queue.enqueueReadBufferRect(context->VolumeBuffer, CL_BLOCKING, origin, origin, out_region,
	  (outExt[1] - outExt[0]) * sizeof(T), (outExt[1] - outExt[0]) * (outExt[3] - outExt[2]) * sizeof(T),
	  outInc[1] * sizeof(T), outInc[2] * sizeof(T), outPtr);
  if (err != CL_SUCCESS) {
	  LOG_ERROR("Read output volume buffer: " << err);
  }
}


#endif

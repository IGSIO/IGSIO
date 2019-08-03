# IGSIO

A collection of tools and algorithms for image guided systems

## Prerequisites

- C++ compiler
  - Windows
    - Visual Studio >= 2015
  - Linux
  - MacOSX

- VTK >= 8

## Build instructions
- Clone IGSIO into a directory of your choice
- Configure the project with CMake
  - Specify the generator
  - Enter VTK directory (VTK_DIR) pointing to the location of VTKConfig.cmake
  - Enable desired components
    - IGSIO_BUILD_SEQUENCEIO: Read/write sequence files to vtkIGSIOTrackedFrameList
      - IGSIO_SEQUENCEIO_ENABLE_MKV: Read/write sequence files using Matroska video format (MKV)
    - IGSIO_BUILD_VOLUMERECONSTRUCTION: Reconstruct volumes using images and transforms in vtkIGSIOTrackedFrameList
    - IGSIO_BUILD_CODECS: Codecs used for video compression
      - IGSIO_USE_VP9: VP9 codec

## Support

If you encounter any issues or have any questions, feel free to submit an issue [here](https://github.com/IGSIO/IGSIO/issues/new).

## Projects using IGSIO

- Plus (https://plustoolkit.github.io/): Free, open-source library and applications for data acquisition, pre-processing, calibration, and real-time streaming of imaging, position tracking, and other sensor data.
- SlicerIGSIO (https://github.com/IGSIO/SlicerIGSIO): Extension for 3D Slicer that provides access to IGSIO algorithms

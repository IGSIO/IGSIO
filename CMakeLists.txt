cmake_minimum_required(VERSION 3.3.0)
project(IGSIO)

set(IGSIO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(IGSIO_BINARY_DIR "${CMAKE_BINARY_DIR}")

# ------------------------------------------------------------------------
# Set project version number
set(IGSIO_VERSION_MAJOR "1")
set(IGSIO_VERSION_MINOR "0")
set(IGSIO_VERSION_PATCH "0")
set(IGSIO_VERSION ${IGSIO_VERSION_MAJOR}.${IGSIO_VERSION_MINOR}.${IGSIO_VERSION_PATCH})

set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/Modules
  ${CMAKE_MODULE_PATH}
  )

# --------------------------------------------------------------------------
# System Settings
if(WIN32)
  # This method adds the necessary compiler flag
  set(RUNTIME_MINIMUM_WINDOWS_VERSION "0x0602")
  add_definitions(-D_MSC_PLATFORM_TOOLSET_$(PlatformToolset))
  if((NOT CMAKE_GENERATOR_TOOLSET OR NOT ${CMAKE_GENERATOR_TOOLSET} MATCHES .*xp) AND
     (NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION OR CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_GREATER_EQUAL 8.1))
    add_definitions(-DIGSIO_USE_VERSION_HELPER)
  endif()
endif()

# --------------------------------------------------------------------------
# Configure output paths for libraries and executables.
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(NOT IGSIO_INSTALL_BIN_DIR)
  set(IGSIO_INSTALL_BIN_DIR "bin")
endif()

if(NOT IGSIO_INSTALL_LIB_DIR)
  set(IGSIO_INSTALL_LIB_DIR "lib")
endif()

if(NOT IGSIO_INSTALL_DATA_DIR)
  set(IGSIO_INSTALL_DATA_DIR "share")
endif()

if(NOT IGSIO_INSTALL_CMAKE_DIR)
  set(IGSIO_INSTALL_CMAKE_DIR "${IGSIO_INSTALL_LIB_DIR}/cmake")
endif()

if(NOT IGSIOLIB_INCLUDE_INSTALL)
  set(IGSIOLIB_INCLUDE_INSTALL "include")
  set(IGSIO_INCLUDE_INSTALL "include")
endif()

if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif()
option(BUILD_SHARED_LIBS "Build shared libraries" ${BUILD_SHARED_LIBS})

set(IGSIO_EXECUTABLE_OUTPUT_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
if(MSVC OR ${CMAKE_GENERATOR} MATCHES "Xcode")
  set(TEST_OUTPUT_PATH "${IGSIO_EXECUTABLE_OUTPUT_PATH}/Release/Output")
else()
  set(TEST_OUTPUT_PATH "${IGSIO_EXECUTABLE_OUTPUT_PATH}/Output")
endif()

option(COPY_EXTERNAL_LIBS "Copy external libraries to output directory." OFF)

#-----------------------------------------------------------------------------
# Specify common external project properties
#-----------------------------------------------------------------------------
include(${CMAKE_ROOT}/Modules/GenerateExportHeader.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/IGSIOMacros.cmake)

# --------------------------------------------------------------------------
option(IGSIO_USE_3DSlicer "" OFF)
if (IGSIO_USE_3DSlicer)
  find_package(Slicer REQUIRED)
  set(IGSIO_Slicer_DIR ${Slicer_DIR} CACHE STRING "")
endif()

# --------------------------------------------------------------------------
# Try to find VTK and include its settings (otherwise complain)
find_package(VTK NO_MODULE REQUIRED PATHS ${VTK_DIR} NO_DEFAULT_PATH)
if(VTK_FOUND AND VTK_VERSION VERSION_LESS 8.9.0)
  include(${VTK_USE_FILE})
endif()

if(TARGET VTK::CommonCore)
  set(IGSIO_VTK_PREFIX VTK::)
else()
  set(IGSIO_VTK_PREFIX vtk)
endif()

find_package(ITK REQUIRED NO_MODULE)
include(${ITK_USE_FILE})

if (COPY_EXTERNAL_LIBS)
  CopyLibrariesToDirectory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${VTK_LIBRARIES})
endif()

option(IGSIO_USE_SYSTEM_ZLIB "Use system zlib" OFF)
option(BUILD_TESTING "Enable tests" ON)
if(BUILD_TESTING)
  include(CTest)
endif()

option(IGSIO_BUILD_SEQUENCEIO "Build classes for reading/writing sequence files" ON)
option(IGSIO_BUILD_VOLUMERECONSTRUCTION "Build classes for volume reconstruction" OFF)
option(IGSIO_SEQUENCEIO_ENABLE_MKV "Enable MKV reading/writing" OFF)
option(IGSIO_BUILD_CODECS "Build classes for Video codecs" OFF)
option(IGSIO_USE_VP9 "Enable VP9 codec" OFF)
if(IGSIO_USE_VP9)
  set(IGSIO_BUILD_CODECS ON CACHE BOOL "Build classes for reading/writing sequence files" FORCE)
endif()

if(IGSIO_SEQUENCEIO_ENABLE_MKV)
  set(IGSIO_BUILD_SEQUENCEIO ON CACHE BOOL "Build classes for reading/writing sequence files" FORCE)
endif()

set(IGSIO_ZLIB_LIBRARY ${ZLIB_LIBRARY} CACHE STRING "")
set(IGSIO_ZLIB_INCLUDE_DIR CACHE STRING "")
if(IGSIO_USE_SYSTEM_ZLIB) # setting IGSIOZLib needs to come before Superbuild
  set(IGSIO_ZLIB_LIBRARY ${ZLIB_LIBRARY})
  set(IGSIO_ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIR})
else()
  set(IGSIO_ZLIB_LIBRARY vtkzlib)
  set(IGSIO_ZLIB_INCLUDE_DIR "")
endif()

# Enable GPU support. Requires OpenCL to be installed
option(IGSIO_USE_GPU "GPU acceleration via OpenCL" OFF)
mark_as_advanced(IGSIO_USE_GPU)

if(IGSIO_USE_GPU)
  find_package(OpenCL REQUIRED)
endif()

#-----------------------------------------------------------------------------
# Run superbuild script instead of library script
if (${CMAKE_VERSION} VERSION_GREATER 3.4)
  option(IGSIO_SUPERBUILD "Build IGSIO as Superbuild." OFF)
endif()

if(IGSIO_SUPERBUILD)
  include(SuperBuild/Superbuild.cmake)
else()

  # --------------------------------------------------------------------------
  # Configure include file
  set(IGSIO_CONFIGURE_DIR ${CMAKE_CURRENT_BINARY_DIR})
  configure_file(
     ${CMAKE_CURRENT_SOURCE_DIR}/igsioConfigure.h.in
     ${IGSIO_CONFIGURE_DIR}/igsioConfigure.h
  )

  add_subdirectory(Codecs)
  add_subdirectory(IGSIOCommon)
  if(IGSIO_BUILD_SEQUENCEIO)
    add_subdirectory(SequenceIO)
  endif()
  if (IGSIO_BUILD_VOLUMERECONSTRUCTION)
    add_subdirectory(VolumeReconstruction)
  endif()
  add_subdirectory(IGSIOCalibration)

  #-----------------------------------------------------------------------------
  # Export all targets at once from the build tree in their final configuration.
  get_property(_igsio_targets GLOBAL PROPERTY IGSIO_TARGETS)
  if (_igsio_targets)
    list(REMOVE_DUPLICATES _igsio_targets)
    export(TARGETS ${_igsio_targets} FILE ${IGSIO_BINARY_DIR}/IGSIOTargets.cmake)
  endif()
  unset(_igsio_targets)

  # Create an IGSIOConfig.cmake and IGSIOConfig Version.cmake file for the use from the build tree
  #-----------------------------------------------------------------------------
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/IGSIOConfigVersion.cmake"
    VERSION ${IGSIO_VERSION}
    COMPATIBILITY ExactVersion)
  list(APPEND IGSIO_INSTALL_CMAKE_FILES} "${CMAKE_CURRENT_BINARY_DIR}/IGSIOConfigVersion.cmake")
  include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/GenerateIGSIOConfig.cmake)

endif()

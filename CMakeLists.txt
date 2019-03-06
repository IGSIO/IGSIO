CMAKE_MINIMUM_REQUIRED(VERSION 3.3.0)
PROJECT(IGSIO)

SET(IGSIO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
SET(IGSIO_BINARY_DIR "${CMAKE_BINARY_DIR}")

# ------------------------------------------------------------------------
# Set project version number
SET(IGSIO_VERSION_MAJOR "1")
SET(IGSIO_VERSION_MINOR "0")
SET(IGSIO_VERSION_PATCH "0")
SET(IGSIO_VERSION ${IGSIO_VERSION_MAJOR}.${IGSIO_VERSION_MINOR}.${IGSIO_VERSION_PATCH})

# --------------------------------------------------------------------------
# Configure include file
CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/igsioConfigure.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/igsioConfigure.h
  )

SET(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/Modules
  ${CMAKE_MODULE_PATH}
  )

# --------------------------------------------------------------------------
# Configure output paths for libraries and executables.
IF(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
ENDIF()

IF(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
ENDIF()

IF(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
ENDIF()

IF(NOT IGSIO_INSTALL_BIN_DIR)
  SET(IGSIO_INSTALL_BIN_DIR "bin")
ENDIF()

IF(NOT IGSIO_INSTALL_LIB_DIR)
  SET(IGSIO_INSTALL_LIB_DIR "lib")
ENDIF()

IF(NOT IGSIO_INSTALL_DATA_DIR)
  SET(IGSIO_INSTALL_DATA_DIR "share")
ENDIF()

IF(NOT DEFINED BUILD_SHARED_LIBS)
  SET(BUILD_SHARED_LIBS ON)
ENDIF()
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ${BUILD_SHARED_LIBS})

OPTION(COPY_EXTERNAL_LIBS "Copy external libraries to output directory." OFF)

#-----------------------------------------------------------------------------
# Specify common external project properties
#-----------------------------------------------------------------------------
INCLUDE(${CMAKE_ROOT}/Modules/GenerateExportHeader.cmake)
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/CMake/IGSIOMacros.cmake)

# Create an IGSIOConfig.cmake and IGSIOConfig Version.cmake file for the use from the build tree
#-----------------------------------------------------------------------------
INCLUDE(CMakePackageConfigHelpers)
# Re: #1075 eventually remove this section after people are used to the new location
CONFIGURE_FILE(IGSIOConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/IGSIOConfig.cmake" @ONLY)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/IGSIOConfigVersion.cmake"
  VERSION ${IGSIO_VERSION}
  COMPATIBILITY ExactVersion)
LIST(APPEND IGSIO_INSTALL_CMAKE_FILES} "${CMAKE_CURRENT_BINARY_DIR}/IGSIOConfigVersion.cmake")

# --------------------------------------------------------------------------
# Try to find VTK and include its settings (otherwise complain)
FIND_PACKAGE(VTK NO_MODULE REQUIRED PATHS ${VTK_DIR} NO_DEFAULT_PATH)
IF(VTK_FOUND)
  INCLUDE(${VTK_USE_FILE})
ELSE()
  MESSAGE(FATAL_ERROR "This application requires VTK. One of these components is missing. Please verify configuration")
ENDIF()

IF (COPY_EXTERNAL_LIBS)
  CopyLibrariesToDirectory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${VTK_LIBRARIES})
ENDIF()

OPTION(IGSIO_USE_SYSTEM_ZLIB "Use system zlib" OFF)
OPTION(BUILD_TESTING "Enable tests" ON)
IF(BUILD_TESTING)
  INCLUDE(CTest)
ENDIF()

OPTION(IGSIO_BUILD_SEQUENCEIO "Build classes for reading/writing sequence files" ON)
OPTION(IGSIO_SEQUENCEIO_ENABLE_MKV "Enable MKV reading/writing" OFF)
OPTION(IGSIO_BUILD_CODECS "Build classes for Video codecs" OFF)
OPTION(IGSIO_USE_VP9 "Enable VP9 codec" OFF)
IF(IGSIO_USE_VP9)
  SET(IGSIO_BUILD_CODECS ON CACHE BOOL "Build classes for reading/writing sequence files" FORCE)
ENDIF()

IF(IGSIO_SEQUENCEIO_ENABLE_MKV)
  SET(IGSIO_BUILD_SEQUENCEIO ON CACHE BOOL "Build classes for reading/writing sequence files" FORCE)
ENDIF()

OPTION(IGSIO_USE_3DSlicer "" OFF)
IF (IGSIO_USE_3DSlicer)
  FIND_PACKAGE(Slicer REQUIRED)
  SET(IGSIO_Slicer_DIR ${Slicer_DIR} CACHE STRING "")
ENDIF()

SET(IGSIO_ZLIB_LIBRARY ${ZLIB_LIBRARY} CACHE STRING "")
SET(IGSIO_ZLIB_INCLUDE_DIR CACHE STRING "")
IF(IGSIO_USE_SYSTEM_ZLIB) # setting IGSIOZLib needs to come before Superbuild
  SET(IGSIO_ZLIB_LIBRARY ${ZLIB_LIBRARY})
  SET(IGSIO_ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIR})
ELSE()
  SET(IGSIO_ZLIB_LIBRARY vtkzlib)
  SET(IGSIO_ZLIB_INCLUDE_DIR "")
ENDIF()

#-----------------------------------------------------------------------------
# Run superbuild script instead of library script
IF (${CMAKE_VERSION} VERSION_GREATER 3.4)
  OPTION(IGSIO_SUPERBUILD "Build IGSIO as Superbuild." OFF)
ENDIF()

IF(IGSIO_SUPERBUILD)
  INCLUDE(SuperBuild/Superbuild.cmake)
ELSE()
  ADD_SUBDIRECTORY(Source/Codecs)
  ADD_SUBDIRECTORY(Source/IGSIOCommon)
  IF(IGSIO_BUILD_SEQUENCEIO)
    ADD_SUBDIRECTORY(Source/SequenceIO)
  ENDIF()

  #-----------------------------------------------------------------------------
  # Export all targets at once from the build tree in their final configuration.
  get_property(_igsio_targets GLOBAL PROPERTY IGSIO_TARGETS)
  if (_igsio_targets)
    list(REMOVE_DUPLICATES _igsio_targets)
    EXPORT(TARGETS ${_igsio_targets} FILE ${IGSIO_BINARY_DIR}/IGSIOTargets.cmake)
  endif()
  unset(_igsio_targets)

  INCLUDE(${CMAKE_SOURCE_DIR}/CMake/GenerateIGSIOConfig.cmake)

ENDIF()

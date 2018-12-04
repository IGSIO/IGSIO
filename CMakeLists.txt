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

# --------------------------------------------------------------------------
# Configure output paths for libraries and executables.
IF(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
ENDIF()

IF(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
ENDIF()

IF(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/share")
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

OPTION(USE_SYSTEM_ZLIB "Use system zlib" OFF)
SET(IGSIOZLIB_INCLUDE_DIR "")
IF(USE_SYSTEM_ZLIB) # setting IGSIOZLib needs to come before processing subdirectories
  SET(IGSIOZLIB_LIBRARY ${ZLIB_LIBRARY})
  INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
  LIST(APPEND IGSIOZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIR})
ELSE()
  SET(IGSIOZLIB_LIBRARY vtkzlib)
ENDIF()

OPTION(BUILD_TESTING "Enable tests" ON)
IF(BUILD_TESTING)
  INCLUDE(CTest)
ENDIF()

ADD_SUBDIRECTORY(Source/IGSIOCommon)

OPTION(BUILD_SEQUENCEIO "Build classes for reading/writing" ON)
IF(BUILD_SEQUENCEIO)
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

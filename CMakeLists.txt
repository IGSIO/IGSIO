CMAKE_MINIMUM_REQUIRED(VERSION 3.3.0)
PROJECT(IGSIO)

SET(IGSIO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
SET(IGSIO_BINARY_DIR "${CMAKE_BINARY_DIR}")

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
INCLUDE(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/GenerateExportHeader.cmake)
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/CMake/IGSIOMacros.cmake)

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

OPTION(BUILD_TESTING "Enable tests" ON)
IF(BUILD_TESTING)
  INCLUDE(CTest)
ENDIF()

OPTION(BUILD_VTKVIDEOIO "Enable video reading/writing" ON)
IF(BUILD_VTKVIDEOIO)
  ADD_SUBDIRECTORY(Source/VideoIO)
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

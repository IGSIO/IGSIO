project(vtkAddon)

#-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.5)
#-----------------------------------------------------------------------------

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
if(NOT DEFINED BUILD_SHARED_LIBS)
  option(BUILD_SHARED_LIBS "Build with shared libraries." ON)
endif()

IF(NOT vtkAddon_INSTALL_BIN_DIR)
  SET(vtkAddon_INSTALL_BIN_DIR "bin")
ENDIF()

IF(NOT vtkAddon_INSTALL_LIB_DIR)
  SET(vtkAddon_INSTALL_LIB_DIR "lib")
ENDIF()

IF(NOT vtkAddon_INSTALL_DATA_DIR)
  SET(vtkAddon_INSTALL_DATA_DIR "share")
ENDIF()


# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------

#
# VTK
#
find_package(VTK REQUIRED)
include(${VTK_USE_FILE})
set(vtkAddon_LIBS
  ${VTK_LIBRARIES}
  )


# --------------------------------------------------------------------------
# Configure headers
# --------------------------------------------------------------------------
set(configure_header_file vtkAddonConfigure.h)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/${configure_header_file}.in
  ${CMAKE_CURRENT_BINARY_DIR}/${configure_header_file}
  )

# --------------------------------------------------------------------------
# Install headers
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_INSTALL_NO_DEVELOPMENT)
  set(${PROJECT_NAME}_INSTALL_NO_DEVELOPMENT ON)
endif()
if(NOT ${PROJECT_NAME}_INSTALL_NO_DEVELOPMENT)
  file(GLOB headers "${CMAKE_CURRENT_SOURCE_DIR}/*.(h|txx)")
  install(
    FILES ${headers} ${CMAKE_CURRENT_BINARY_DIR}/${configure_header_file}
    DESTINATION include/${PROJECT_NAME} COMPONENT Development)
endif()

# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
set(vtkAddon_SRCS
  vtkStreamingVolumeCodec.cxx
  vtkStreamingVolumeCodec.h
  vtkStreamingVolumeFrame.cxx
  vtkStreamingVolumeFrame.h
  vtkStreamingVolumeCodecFactory.cxx
  vtkStreamingVolumeCodecFactory.h
  vtkRawRGBVolumeCodec.cxx
  vtkRawRGBVolumeCodec.h
)

# --------------------------------------------------------------------------
# Include dirs
# --------------------------------------------------------------------------
set(include_dirs
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  )
include_directories(BEFORE ${include_dirs})

# --------------------------------------------------------------------------
# Build library
# --------------------------------------------------------------------------
set(lib_name ${PROJECT_NAME})

set(srcs ${vtkAddon_SRCS})

# --------------------------------------------------------------------------
# Python Wrapping
# --------------------------------------------------------------------------
# if(VTK_WRAP_PYTHON)
  # include(vtkMacroKitPythonWrap)

  # vtkMacroKitPythonWrap(
    # KIT_NAME ${lib_name}
    # KIT_SRCS ${vtkAddon_SRCS}
    # KIT_INSTALL_BIN_DIR ${${PROJECT_NAME}_INSTALL_BIN_DIR}
    # KIT_INSTALL_LIB_DIR ${${PROJECT_NAME}_INSTALL_LIB_DIR}
    # )
  # # Export target
  # export(TARGETS ${lib_name}Python ${lib_name}PythonD APPEND FILE ${${PROJECT_NAME}_EXPORT_FILE})
  # # Folder
  # if(NOT "${${PROJECT_NAME}_FOLDER}" STREQUAL "")
    # set_target_properties(${lib_name}Python PROPERTIES FOLDER ${${PROJECT_NAME}_FOLDER})
    # set_target_properties(${lib_name}PythonD PROPERTIES FOLDER ${${PROJECT_NAME}_FOLDER})
    # if(TARGET ${lib_name}Hierarchy)
      # set_target_properties(${lib_name}Hierarchy PROPERTIES FOLDER ${${PROJECT_NAME}_FOLDER})
    # endif()
  # endif()
# endif()

# --------------------------------------------------------------------------
# Set INCLUDE_DIRS variable
# --------------------------------------------------------------------------
set(${PROJECT_NAME}_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
  CACHE INTERNAL "${PROJECT_NAME} include dirs" FORCE)

#-----------------------------------------------------------------------------
# Create the library
IF(MSVC OR ${CMAKE_GENERATOR} MATCHES "Xcode")
  ADD_LIBRARY(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
ELSE()
  ADD_LIBRARY(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS})
ENDIF()
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_LIBS})

INSTALL(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  RUNTIME DESTINATION ${vtkAddon_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${vtkAddon_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${vtkAddon_INSTALL_LIB_DIR} COMPONENT Development)

FOREACH(p IN LISTS ${PROJECT_NAME}_INCLUDE_DIRS)
  TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${p}>)
ENDFOREACH()
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${${PROJECT_NAME}_INSTALL_INCLUDE_DIR}>)

include(GenerateExportHeader)
GENERATE_EXPORT_HEADER(${PROJECT_NAME})

SET_PROPERTY(GLOBAL APPEND PROPERTY vtkAddon_TARGETS ${PROJECT_NAME})

SET(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} CACHE INTERNAL "")

# --------------------------------------------------------------------------
# Configure support file
# --------------------------------------------------------------------------
#-----------------------------------------------------------------------------
# Export all targets at once from the build tree in their final configuration.
get_property(_vtkaddon_targets GLOBAL PROPERTY vtkAddon_TARGETS)
if (_vtkaddon_targets)
  list(REMOVE_DUPLICATES _vtkaddon_targets)
  EXPORT(TARGETS ${_vtkaddon_targets} FILE ${CMAKE_CURRENT_BINARY_DIR}/vtkAddonTargets.cmake)
endif()
unset(_vtkaddon_targets)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/vtkAddonConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/vtkAddonConfig.cmake
  )

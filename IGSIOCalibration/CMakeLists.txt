project(vtkIGSIOCalibration)

SET(${PROJECT_NAME}_INSTALL_INCLUDE_DIR ${IGSIOLIB_INCLUDE_INSTALL} CACHE INTERNAL "")

# --------------------------------------------------------------------------
# Sources
set(${PROJECT_NAME}_SRCS
  vtkIGSIOAbstractStylusCalibrationAlgo.cxx
  vtkIGSIOLandmarkDetectionAlgo.cxx
  vtkIGSIOPivotCalibrationAlgo.cxx
  vtkIGSIOSpinCalibrationAlgo.cxx
  )

set(${PROJECT_NAME}_HDRS
  vtkIGSIOAbstractStylusCalibrationAlgo.h
  vtkIGSIOPivotCalibrationAlgo.h
  vtkIGSIOLandmarkDetectionAlgo.h
  vtkIGSIOSpinCalibrationAlgo.h
  ${CMAKE_CURRENT_BINARY_DIR}/vtkigsiocalibration_export.h
  )

set(${PROJECT_NAME}_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${vtkIGSIOCommon_INCLUDES}
  CACHE INTERNAL "" FORCE)

# --------------------------------------------------------------------------
# Build the library
set(${PROJECT_NAME}_LIBS
  vtkIGSIOCommon
  )

add_library(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
foreach(p IN LISTS ${PROJECT_NAME}_INCLUDE_DIRS)
  target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${p}>)
endforeach()
target_include_directories(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${${PROJECT_NAME}_INSTALL_INCLUDE_DIR}>)
target_link_libraries(${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_LIBS})

set_property(GLOBAL APPEND PROPERTY IGSIO_TARGETS ${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} EXPORT IGSIOTargets
  RUNTIME DESTINATION ${IGSIO_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT Development)

GENERATE_EXPORT_HEADER(
    ${PROJECT_NAME}
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/vtkigsiocalibration_export.h)

install(FILES ${${PROJECT_NAME}_HDRS} DESTINATION ${${PROJECT_NAME}_INSTALL_INCLUDE_DIR})

set_property(GLOBAL APPEND PROPERTY IGSIO_TARGETS ${PROJECT_NAME})

set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} CACHE INTERNAL "")

# --------------------------------------------------------------------------
# Set external MS project
set(IGSIO_DEPENDENCIES ${IGSIO_DEPENDENCIES} ${PROJECT_NAME} CACHE INTERNAL "" FORCE)
list(REMOVE_DUPLICATES IGSIO_DEPENDENCIES)
# Add this variable to UseIGSIO.cmake.in INCLUDE_IGSIO_MS_PROJECTS macro
set(vcProj_${PROJECT_NAME} ${PROJECT_NAME};${IGSIO_BINARY_DIR}/src/${PROJECT_NAME}/${PROJECT_NAME}.vcxproj;vtkIGSIOCommon CACHE INTERNAL "" FORCE)

# --------------------------------------------------------------------------
# Install
#
#IGSIOInstallLibrary(vtk${PROJECT_NAME} ${PROJECT_NAME})

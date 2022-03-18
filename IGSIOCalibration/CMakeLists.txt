PROJECT(IGSIOCalibration)

# --------------------------------------------------------------------------
# Sources
SET(${PROJECT_NAME}_SRCS
  vtkIGSIOPivotCalibrationAlgo.cxx
  vtkIGSIOLandmarkDetectionAlgo.cxx
  )

SET(${PROJECT_NAME}_HDRS
  vtkIGSIOPivotCalibrationAlgo.h
  vtkIGSIOLandmarkDetectionAlgo.h
  )

SET(${PROJECT_NAME}_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "" FORCE)

# --------------------------------------------------------------------------
# Build the library
SET(${PROJECT_NAME}_LIBS
  vtkIGSIOCommon
  )

GENERATE_EXPORT_DIRECTIVE_FILE(vtk${PROJECT_NAME})

ADD_LIBRARY(vtk${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
FOREACH(p IN LISTS ${PROJECT_NAME}_INCLUDE_DIRS)
  target_include_directories(vtk${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${p}>)
ENDFOREACH()
target_include_directories(vtk${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${IGSIO_INCLUDE_INSTALL}>)
TARGET_LINK_LIBRARIES(vtk${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_LIBS})

SET_PROPERTY(GLOBAL APPEND PROPERTY IGSIO_TARGETS vtk${PROJECT_NAME})

# --------------------------------------------------------------------------
# Set external MS project
SET(IGSIO_DEPENDENCIES ${IGSIO_DEPENDENCIES} vtk${PROJECT_NAME} CACHE INTERNAL "" FORCE)
LIST(REMOVE_DUPLICATES IGSIO_DEPENDENCIES)
# Add this variable to UseIGSIO.cmake.in INCLUDE_IGSIO_MS_PROJECTS macro
SET(vcProj_vtk${PROJECT_NAME} vtk${PROJECT_NAME};${IGSIO_BINARY_DIR}/src/${PROJECT_NAME}/vtk${PROJECT_NAME}.vcxproj;vtkIGSIOCommon CACHE INTERNAL "" FORCE)

# --------------------------------------------------------------------------
# Install
#
IGSIOInstallLibrary(vtk${PROJECT_NAME} ${PROJECT_NAME})

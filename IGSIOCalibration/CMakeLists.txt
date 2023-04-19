project(IGSIOCalibration)

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
  )

set(${PROJECT_NAME}_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "" FORCE)

# --------------------------------------------------------------------------
# Build the library
set(${PROJECT_NAME}_LIBS
  vtkIGSIOCommon
  )

add_library(vtk${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
foreach(p IN LISTS ${PROJECT_NAME}_INCLUDE_DIRS)
  target_include_directories(vtk${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${p}>)
endforeach()
target_include_directories(vtk${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${IGSIO_INCLUDE_INSTALL}>)
target_link_libraries(vtk${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_LIBS})

set_property(GLOBAL APPEND PROPERTY IGSIO_TARGETS vtk${PROJECT_NAME})

GENERATE_EXPORT_HEADER(vtk${PROJECT_NAME})

# --------------------------------------------------------------------------
# Set external MS project
set(IGSIO_DEPENDENCIES ${IGSIO_DEPENDENCIES} vtk${PROJECT_NAME} CACHE INTERNAL "" FORCE)
list(REMOVE_DUPLICATES IGSIO_DEPENDENCIES)
# Add this variable to UseIGSIO.cmake.in INCLUDE_IGSIO_MS_PROJECTS macro
set(vcProj_vtk${PROJECT_NAME} vtk${PROJECT_NAME};${IGSIO_BINARY_DIR}/src/${PROJECT_NAME}/vtk${PROJECT_NAME}.vcxproj;vtkIGSIOCommon CACHE INTERNAL "" FORCE)

# --------------------------------------------------------------------------
# Install
#
IGSIOInstallLibrary(vtk${PROJECT_NAME} ${PROJECT_NAME})

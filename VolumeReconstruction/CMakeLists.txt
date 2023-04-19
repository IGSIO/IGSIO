project(vtkVolumeReconstruction)

# --------------------------------------------------------------------------
# Sources
set(${PROJECT_NAME}_SRCS
  vtkIGSIOPasteSliceIntoVolume.cxx
  vtkIGSIOVolumeReconstructor.cxx
  vtkIGSIOFillHolesInVolume.cxx
  vtkIGSIOFanAngleDetectorAlgo.cxx
  )

if(MSVC OR ${CMAKE_GENERATOR} MATCHES "Xcode")
  set(${PROJECT_NAME}_HDRS
    fixed.h
    vtkIGSIOPasteSliceIntoVolume.h
    vtkIGSIOPasteSliceIntoVolumeHelperCommon.h
    vtkIGSIOPasteSliceIntoVolumeHelperOptimized.h
    vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized.h
    vtkIGSIOVolumeReconstructor.h
    vtkIGSIOFillHolesInVolume.h
    vtkIGSIOFanAngleDetectorAlgo.h
    )
endif()

set(${PROJECT_NAME}_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${vtkIGSIOCommon_INCLUDES}
  ${vtkSequenceIO_INCLUDE_DIRS}
  CACHE INTERNAL "" FORCE )

# --------------------------------------------------------------------------
# Build the library
set(${PROJECT_NAME}_LIBS
  vtkIGSIOCommon
  vtkSequenceIO
  )

set(${PROJECT_NAME}_LIBS_PRIVATE
  )

if(IGSIO_USE_GPU)
  find_package(OpenCL REQUIRED)
  list(APPEND ${PROJECT_NAME}_INCLUDE_DIRS
    ${OpenCL_INCLUDE_DIRS}
    ${OPENCL_INCLUDE_DIRS}
    )
  list(APPEND ${PROJECT_NAME}_LIBS_PRIVATE
    ${OpenCL_LIBRARIES}
    ${OPENCL_LIBRARIES}
    )
endif()

add_library(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
foreach(p IN LISTS ${PROJECT_NAME}_INCLUDE_DIRS)
  target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${p}>)
endforeach()
target_include_directories(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${IGSIOLIB_INCLUDE_INSTALL}>)
target_link_libraries(${PROJECT_NAME}
  PUBLIC ${${PROJECT_NAME}_LIBS}
  PRIVATE ${${PROJECT_NAME}_LIBS_PRIVATE}
  )

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  RUNTIME DESTINATION ${IGSIO_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT Development)

GENERATE_EXPORT_HEADER(${PROJECT_NAME})
set_property(GLOBAL APPEND PROPERTY IGSIO_TARGETS ${PROJECT_NAME})

# --------------------------------------------------------------------------
# Set external MS project
set(IGSIOLIB_DEPENDENCIES ${IGSIOLIB_DEPENDENCIES} ${PROJECT_NAME} CACHE INTERNAL "" FORCE)
list(REMOVE_DUPLICATES IGSIOLIB_DEPENDENCIES)
# Add this variable to UseIGSIOLib.cmake.in INCLUDE_IGSIOLIB_MS_PROJECTS macro
set(vcProj_${PROJECT_NAME} ${PROJECT_NAME};${IGSIOLib_BINARY_DIR}/src/${PROJECT_NAME}/${PROJECT_NAME}.vcxproj;vtkIGSIOCommon;vtkIGSIODataCollection CACHE INTERNAL "" FORCE)

if(IGSIOBUILD_BUILD_IGSIOLib_TOOLS)
  add_executable(VolumeReconstructor Tools/VolumeReconstructor.cxx )
  set_target_properties(VolumeReconstructor PROPERTIES FOLDER Tools)
  target_link_libraries(VolumeReconstructor
    vtkIGSIOVolumeReconstruction
    )
  GENERATE_HELP_DOC(VolumeReconstructor)

  add_executable(CreateSliceModels Tools/CreateSliceModels.cxx )
  set_target_properties(CreateSliceModels PROPERTIES FOLDER Tools)
  target_link_libraries(CreateSliceModels
    vtkIGSIOCommon
    vtkIGSIOVolumeReconstruction
    vtkFiltersSources
    vtkFiltersGeneral
    vtkIOLegacy
    )
  GENERATE_HELP_DOC(CreateSliceModels)

  add_executable(CompareVolumes Tools/CompareVolumes.cxx Tools/vtkIGSIOCompareVolumes.cxx )
  set_target_properties(CompareVolumes PROPERTIES FOLDER Tools)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Tools)
  target_link_libraries(CompareVolumes
    vtkIGSIOCommon
    vtkIOLegacy
    vtkImagingMath
    vtkImagingStatistics
    )

  add_executable(DrawClipRegion Tools/DrawClipRegion.cxx )
  set_target_properties(DrawClipRegion PROPERTIES FOLDER Tools)
  target_link_libraries(DrawClipRegion
    vtkIGSIOCommon
    vtkIGSIOVolumeReconstruction
    )
  GENERATE_HELP_DOC(DrawClipRegion)

  install(
    TARGETS
      VolumeReconstructor
      CreateSliceModels
      DrawClipRegion
    DESTINATION "${IGSIOLIB_BINARY_INSTALL}"
    COMPONENT RuntimeExecutables
    )
endif()

# --------------------------------------------------------------------------
# Testing
#
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()
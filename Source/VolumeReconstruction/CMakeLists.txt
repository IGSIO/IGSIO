PROJECT(vtkVolumeReconstruction)

# --------------------------------------------------------------------------
# Sources
SET(${PROJECT_NAME}_SRCS
  vtkIGSIOPasteSliceIntoVolume.cxx
  vtkIGSIOVolumeReconstructor.cxx
  vtkIGSIOFillHolesInVolume.cxx
  vtkIGSIOFanAngleDetectorAlgo.cxx
  )

IF(MSVC OR ${CMAKE_GENERATOR} MATCHES "Xcode")
  SET(${PROJECT_NAME}_HDRS
    fixed.h
    vtkIGSIOPasteSliceIntoVolume.h
    vtkIGSIOPasteSliceIntoVolumeHelperCommon.h
    vtkIGSIOPasteSliceIntoVolumeHelperOptimized.h
    vtkIGSIOPasteSliceIntoVolumeHelperUnoptimized.h
    vtkIGSIOVolumeReconstructor.h
    vtkIGSIOFillHolesInVolume.h
    vtkIGSIOFanAngleDetectorAlgo.h
    )
ENDIF()

SET(${PROJECT_NAME}_INCLUDE_DIRS 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${vtkIGSIOCommon_INCLUDES}
  ${vtkSequenceIO_INCLUDE_DIRS}
  CACHE INTERNAL "" FORCE )

# --------------------------------------------------------------------------
# Build the library
SET(${PROJECT_NAME}_LIBS
  vtkIGSIOCommon
  vtkSequenceIO
  vtkInteractionStyle
  vtkRenderingFreeType
  vtkRendering${VTK_RENDERING_BACKEND}
  vtkRenderingVolume${VTK_RENDERING_BACKEND}
  )

ADD_LIBRARY(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
FOREACH(p IN LISTS ${PROJECT_NAME}_INCLUDE_DIRS)
  target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${p}>)
ENDFOREACH()
target_include_directories(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${IGSIOLIB_INCLUDE_INSTALL}>)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${${PROJECT_NAME}_LIBS})

INSTALL(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  RUNTIME DESTINATION ${IGSIO_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT Development)

GENERATE_EXPORT_HEADER(${PROJECT_NAME})
SET_PROPERTY(GLOBAL APPEND PROPERTY IGSIO_TARGETS ${PROJECT_NAME})

# --------------------------------------------------------------------------
# Set external MS project
SET(IGSIOLIB_DEPENDENCIES ${IGSIOLIB_DEPENDENCIES} ${PROJECT_NAME} CACHE INTERNAL "" FORCE)
LIST(REMOVE_DUPLICATES IGSIOLIB_DEPENDENCIES)
# Add this variable to UseIGSIOLib.cmake.in INCLUDE_IGSIOLIB_MS_PROJECTS macro
SET(vcProj_${PROJECT_NAME} ${PROJECT_NAME};${IGSIOLib_BINARY_DIR}/src/${PROJECT_NAME}/${PROJECT_NAME}.vcxproj;vtkIGSIOCommon;vtkIGSIODataCollection CACHE INTERNAL "" FORCE)

IF(IGSIOBUILD_BUILD_IGSIOLib_TOOLS)
  ADD_EXECUTABLE(VolumeReconstructor Tools/VolumeReconstructor.cxx )
  SET_TARGET_PROPERTIES(VolumeReconstructor PROPERTIES FOLDER Tools)
  TARGET_LINK_LIBRARIES(VolumeReconstructor 
    vtkIGSIOVolumeReconstruction
    )
  GENERATE_HELP_DOC(VolumeReconstructor)

  ADD_EXECUTABLE(CreateSliceModels Tools/CreateSliceModels.cxx )
  SET_TARGET_PROPERTIES(CreateSliceModels PROPERTIES FOLDER Tools)
  TARGET_LINK_LIBRARIES(CreateSliceModels 
    vtkIGSIOCommon 
    vtkIGSIOVolumeReconstruction 
    vtkFiltersSources 
    vtkFiltersGeneral
    vtkIOLegacy
    )
  GENERATE_HELP_DOC(CreateSliceModels)

  ADD_EXECUTABLE(CompareVolumes Tools/CompareVolumes.cxx Tools/vtkIGSIOCompareVolumes.cxx )
  SET_TARGET_PROPERTIES(CompareVolumes PROPERTIES FOLDER Tools)
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/Tools)
  TARGET_LINK_LIBRARIES(CompareVolumes 
    vtkIGSIOCommon 
    vtkIOLegacy 
    vtkImagingMath 
    vtkImagingStatistics
    )

  ADD_EXECUTABLE(DrawClipRegion Tools/DrawClipRegion.cxx )
  SET_TARGET_PROPERTIES(DrawClipRegion PROPERTIES FOLDER Tools)
  TARGET_LINK_LIBRARIES(DrawClipRegion 
    vtkIGSIOCommon 
    vtkIGSIOVolumeReconstruction
    )
  GENERATE_HELP_DOC(DrawClipRegion)

  INSTALL(
    TARGETS
      VolumeReconstructor
      CreateSliceModels
      DrawClipRegion
    DESTINATION "${IGSIOLIB_BINARY_INSTALL}"
    COMPONENT RuntimeExecutables
    )
ENDIF()

# --------------------------------------------------------------------------
# Testing
#
IF(BUILD_TESTING)
  ADD_SUBDIRECTORY(Testing)
ENDIF()
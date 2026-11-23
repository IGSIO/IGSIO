# Generate the igsioConfig.cmake file in the build tree.  Also configure
# one for installation.  The file tells external projects how to use
# IGSIO.

#-----------------------------------------------------------------------------
# Configure igsioConfig.cmake

# The "use" file.
set(IGSIO_USE_FILE ${CMAKE_CURRENT_BINARY_DIR}/UseIGSIO.cmake)

# The library dependencies file.
set(IGSIO_LIBRARY_TARGETS_FILE_CONFIG IGSIOTargets.cmake)

# Build tree
# vtkSequenceIO
set(VTKSEQUENCEIO_INCLUDE_DIRS_CONFIG ${vtkSequenceIO_INCLUDE_DIRS})
# vtkIGSIOCommon
set(VTKIGSIOCOMMON_INCLUDE_DIRS_CONFIG ${vtkIGSIOCommon_INCLUDE_DIRS})
# vtkVolumeReconstruction
set(VTKVOLUMERECONSTRUCTION_INCLUDE_DIRS_CONFIG ${vtkVolumeReconstruction_INCLUDE_DIRS})
# vtkIGSIOCalibration
set(VTKIGSIOCALIBRATION_INCLUDE_DIRS_CONFIG ${vtkIGSIOCalibration_INCLUDE_DIRS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/IGSIOConfig.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/IGSIOConfig.cmake @ONLY IMMEDIATE)

# Install tree
set(VTKSEQUENCEIO_INCLUDE_DIRS_CONFIG ${CMAKE_INSTALL_PREFIX}/${vtkIGSIOCommon_INSTALL_INCLUDE_DIR})
# vtkIGSIOCommon
set(VTKIGSIOCOMMON_INCLUDE_DIRS_CONFIG ${CMAKE_INSTALL_PREFIX}/${vtkSequenceIO_INSTALL_INCLUDE_DIR})
# vtkVolumeReconstruction
set(VTKVOLUMERECONSTRUCTION_INCLUDE_DIRS_CONFIG ${CMAKE_INSTALL_PREFIX}/${vtkVolumeReconstruction_INSTALL_INCLUDE_DIR})
# vtkIGSIOCalibration
set(VTKIGSIOCALIBRATION_INCLUDE_DIRS_CONFIG ${CMAKE_INSTALL_PREFIX}/${vtkIGSIOCalibration_INSTALL_INCLUDE_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/IGSIOConfig.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/install/IGSIOConfig.cmake @ONLY IMMEDIATE)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/install/IGSIOConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/IGSIOConfigVersion.cmake
  DESTINATION ${IGSIO_INSTALL_CMAKE_DIR} COMPONENT dev
)

install(EXPORT IGSIOTargets 
  DESTINATION "${IGSIO_INSTALL_CMAKE_DIR}" COMPONENT dev
)

# Generate the igsioConfig.cmake file in the build tree.  Also configure
# one for installation.  The file tells external projects how to use
# IGSIO.

#-----------------------------------------------------------------------------
# Configure igsioConfig.cmake

# The "use" file.
SET(IGSIO_USE_FILE ${IGSIO_BINARY_DIR}/UseIGSIO.cmake)

# vtkSequenceIO
SET(VTKSEQUENCEIO_INCLUDE_DIRS_CONFIG ${vtkSequenceIO_INCLUDE_DIRS})
# vtkIGSIOCommon
SET(VTKIGSIOCOMMON_INCLUDE_DIRS_CONFIG ${vtkIGSIOCommon_INCLUDE_DIRS})
# vtkVolumeReconstruction
SET(VTKVOLUMERECONSTRUCTION_INCLUDE_DIRS_CONFIG ${vtkVolumeReconstruction_INCLUDE_DIRS})

# The library dependencies file.
SET(IGSIO_LIBRARY_TARGETS_FILE_CONFIG ${IGSIO_BINARY_DIR}/IGSIOTargets.cmake)

CONFIGURE_FILE(${IGSIO_SOURCE_DIR}/IGSIOConfig.cmake.in
               ${IGSIO_BINARY_DIR}/IGSIOConfig.cmake @ONLY IMMEDIATE)

# Generate the igsioConfig.cmake file in the build tree.  Also configure
# one for installation.  The file tells external projects how to use
# IGSIO.

#-----------------------------------------------------------------------------
# Configure igsioConfig.cmake

# The "use" file.
SET(IGSIO_USE_FILE ${IGSIO_BINARY_DIR}/UseIGSIO.cmake)

# Determine the include directories needed.
SET(VTKVIDEOIO_INCLUDE_DIRS_CONFIG
  ${vtkVideoIO_INCLUDE_DIRS}
  )

# Determine the include directories needed.
SET(VTKVIDEOIO_LIBRARY_DIRS_CONFIG
  ${vtkVideoIO_LIBRARY_DIRS}
  )

# The library dependencies file.
SET(IGSIO_LIBRARY_TARGETS_FILE_CONFIG ${IGSIO_BINARY_DIR}/IGSIOTargets.cmake)
  
CONFIGURE_FILE(${IGSIO_SOURCE_DIR}/IGSIOConfig.cmake.in
               ${IGSIO_BINARY_DIR}/IGSIOConfig.cmake @ONLY IMMEDIATE)

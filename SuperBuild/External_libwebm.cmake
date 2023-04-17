IF(libwebm_DIR)
  # libwebm has been built already
  FIND_PACKAGE(libwebm REQUIRED NO_MODULE)
  MESSAGE(STATUS "Using libwebm available at: ${libwebm_DIR}")

  # Copy libraries to CMAKE_RUNTIME_OUTPUT_DIRECTORY
  IF (COPY_EXTERNAL_LIBS)
    CopyLibrariesToDirectory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${libwebm_LIBRARIES})
  ENDIF()

  SET (IGSIO_libwebm_DIR "${libwebm_DIR}" CACHE INTERNAL "Path to store libwebm binaries")
ELSE()

  SET (IGSIO_libwebm_SRC_DIR ${CMAKE_BINARY_DIR}/libwebm CACHE INTERNAL "Path to store libwebm contents.")
  SET (IGSIO_libwebm_PREFIX_DIR ${CMAKE_BINARY_DIR}/libwebm-prefix CACHE INTERNAL "Path to store libwebm prefix data.")
  SET (IGSIO_libwebm_DIR ${CMAKE_BINARY_DIR}/libwebm-bin CACHE INTERNAL "Path to store libwebm binaries")

  SET(BUILD_OPTIONS
    -DENABLE_WEBMINFO:BOOL=OFF
    -DENABLE_WEBMTS:BOOL=OFF
    -DENABLE_WEBM_PARSER:BOOL=OFF
    -DENABLE_SAMPLES:BOOL=OFF
    -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS:BOOL=ON
    )

  IF(APPLE)
    LIST(APPEND BUILD_OPTIONS
      -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
      -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
      -DCMAKE_OSX_SYSROOT:STRING=${CMAKE_OSX_SYSROOT}
      )
  ENDIF()

  IF(NOT DEFINED(libwebm_GIT_REPOSITORY))
    SET(libwebm_GIT_REPOSITORY "https://github.com/Sunderlandkyl/libwebm.git" CACHE STRING "Set libwebm desired git url")
  ENDIF()
  IF(NOT DEFINED(libwebm_GIT_REVISION))
    SET(libwebm_GIT_REVISION "f0072f4a22bb259c99b39d1870af2c0c511207ca" CACHE STRING "Set libwebm desired git hash") # Sunderlandkyl/libwebm_d1b981b79
  ENDIF()

 ExternalProject_Add( libwebm
   SOURCE_DIR ${IGSIO_libwebm_SRC_DIR}
   BINARY_DIR ${IGSIO_libwebm_DIR}
   #--Download step--------------
   GIT_REPOSITORY ${libwebm_GIT_REPOSITORY}
   GIT_TAG ${libwebm_GIT_REVISION}
   #--Configure step-------------
   CMAKE_ARGS
     ${ep_common_args}
     -DEXECUTABLE_OUTPUT_PATH:PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
     -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
     -DCMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
     -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
     -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
     -DCMAKE_CXX_FLAGS:STRING=${ep_common_cxx_flags}
     -DCMAKE_C_FLAGS:STRING=${ep_common_c_flags}
     -DCMAKE_MACOSX_RPATH:BOOL=${CMAKE_MACOSX_RPATH}
     -DBUILD_SHARED_LIBS:BOOL=OFF
     # Options
     ${BUILD_OPTIONS}
   #--Build step-----------------
   BUILD_ALWAYS 1
   #--Install step-----------------
   INSTALL_COMMAND ""
   )
ENDIF()

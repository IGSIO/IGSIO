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

 ExternalProject_Add( libwebm
   SOURCE_DIR ${IGSIO_libwebm_SRC_DIR}
   BINARY_DIR ${IGSIO_libwebm_DIR}
   #--Download step--------------
   GIT_REPOSITORY "https://github.com/Sunderlandkyl/libwebm.git"
   GIT_TAG "master"
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
     -DBUILD_SHARED_LIBS:BOOL=OFF
     # Options
     -DENABLE_WEBMINFO:BOOL=OFF
     -DENABLE_WEBMTS:BOOL=OFF
     -DENABLE_WEBM_PARSER:BOOL=OFF
     -DENABLE_SAMPLES:BOOL=OFF
     -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS:BOOL=ON
   #--Build step-----------------
   BUILD_ALWAYS 1
   #--Install step-----------------
   INSTALL_COMMAND ""
   )
ENDIF()

if(libwebm_DIR)
  # libwebm has been built already
  find_package(libwebm REQUIRED NO_MODULE)
  message(STATUS "Using libwebm available at: ${libwebm_DIR}")

  # Copy libraries to CMAKE_RUNTIME_OUTPUT_DIRECTORY
  if (COPY_EXTERNAL_LIBS)
    CopyLibrariesToDirectory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${libwebm_LIBRARIES})
  endif()

  set (IGSIO_libwebm_DIR "${libwebm_DIR}" CACHE INTERNAL "Path to store libwebm binaries")
else()

  set (IGSIO_libwebm_SRC_DIR ${CMAKE_BINARY_DIR}/libwebm CACHE INTERNAL "Path to store libwebm contents.")
  set (IGSIO_libwebm_PREFIX_DIR ${CMAKE_BINARY_DIR}/libwebm-prefix CACHE INTERNAL "Path to store libwebm prefix data.")
  set (IGSIO_libwebm_DIR ${CMAKE_BINARY_DIR}/libwebm-bin CACHE INTERNAL "Path to store libwebm binaries")

  set(BUILD_OPTIONS
    -DENABLE_WEBMINFO:BOOL=OFF
    -DENABLE_WEBMTS:BOOL=OFF
    -DENABLE_WEBM_PARSER:BOOL=OFF
    -DENABLE_SAMPLES:BOOL=OFF
    -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS:BOOL=ON
    )

  if(APPLE)
    list(APPEND BUILD_OPTIONS
      -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
      -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
      -DCMAKE_OSX_SYSROOT:STRING=${CMAKE_OSX_SYSROOT}
      )
  endif()

  if(NOT DEFINED(libwebm_GIT_REPOSITORY))
    set(libwebm_GIT_REPOSITORY "https://github.com/Sunderlandkyl/libwebm.git" CACHE STRING "Set libwebm desired git url")
  endif()
  if(NOT DEFINED(libwebm_GIT_REVISION))
    set(libwebm_GIT_REVISION "f0072f4a22bb259c99b39d1870af2c0c511207ca" CACHE STRING "Set libwebm desired git hash") # Sunderlandkyl/libwebm_d1b981b79
  endif()

  ExternalProject_Add( libwebm
    SOURCE_DIR ${IGSIO_libwebm_SRC_DIR}
    BINARY_DIR ${IGSIO_libwebm_DIR}
    #--Download step--------------
    GIT_REPOSITORY ${libwebm_GIT_REPOSITORY}
    GIT_TAG ${libwebm_GIT_REVISION}
    #--Configure step-------------
    CMAKE_ARGS
      ${ep_common_args}
      # Compiler settings
      # Not used -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
      # Not used -DCMAKE_C_FLAGS:STRING=${ep_common_c_flags}
      -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
      -DCMAKE_CXX_FLAGS:STRING=${ep_common_cxx_flags}
      -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
      -DCMAKE_MACOSX_RPATH:BOOL=${CMAKE_MACOSX_RPATH}
      # Output directories
      -DEXECUTABLE_OUTPUT_PATH:PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
      -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
      # Options
      -DBUILD_SHARED_LIBS:BOOL=OFF
      ${BUILD_OPTIONS}
    #--Build step-----------------
    BUILD_ALWAYS 1
    #--Install step-----------------
    INSTALL_COMMAND ""
    )
endif()

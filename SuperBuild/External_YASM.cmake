cmake_minimum_required(VERSION 2.8.2)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
include(${IGSIO_SOURCE_DIR}/Modules/FindYASM.cmake)
include(${CMAKE_ROOT}/Modules/FindPythonInterp.cmake)

if(YASM_FOUND)
  # YASM has been built already
  message(STATUS "Using YASM available at: ${YASM_BINARY_DIR}")
else()
  set(YASM_PYTHON_EXECUTABLE "" CACHE STRING "Python Interpreter")
  if("${YASM_PYTHON_EXECUTABLE}" STREQUAL "")
    find_package(PythonInterp "2.7" REQUIRED)
    set(YASM_PYTHON_EXECUTABLE ${PYTHON_EXECUTABLE})
  endif()

  if(NOT DEFINED(YASM_GIT_REPOSITORY))
    set(YASM_GIT_REPOSITORY "https://github.com/yasm/yasm.git" CACHE STRING "Set YASM desired git url")
  endif()

  if(NOT DEFINED(YASM_GIT_REVISION))
    set(YASM_GIT_REVISION "master" CACHE STRING "Set YASM desired git hash (master means latest)")
  endif()

  # yas has not been built yet, so download and build it as an external project
  set(GIT_REPOSITORY ${YASM_GIT_REPOSITORY})
  set(GIT_TAG ${YASM_GIT_REVISION})
  if(MSVC)
    list(APPEND PLATFORM_SPECIFIC_ARGS -DCMAKE_CXX_MP_FLAG:BOOL=ON)
  endif()

  message(STATUS "Downloading yasm ${GIT_TAG} from: ${GIT_REPOSITORY}")

  set (YASM_SOURCE_DIR "${CMAKE_BINARY_DIR}/yasm" CACHE PATH "YASM source directory" FORCE)
  set (YASM_BINARY_DIR "${CMAKE_BINARY_DIR}/yasm-bin" CACHE PATH "YASM library directory" FORCE)
  if(TARGET YASM)
    return()
  endif()
  ExternalProject_Add( YASM
    PREFIX "${CMAKE_BINARY_DIR}/yasm-prefix"
    SOURCE_DIR "${YASM_SOURCE_DIR}"
    BINARY_DIR "${YASM_BINARY_DIR}"
    #--Download step--------------
    GIT_REPOSITORY "${GIT_REPOSITORY}"
    GIT_TAG ${GIT_TAG}
    #--Configure step-------------
    CMAKE_ARGS
      ${PLATFORM_SPECIFIC_ARGS}
      # Compiler settings
      -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
      -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
      -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
      -DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
      # Output directories
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY:STRING=${YASM_BINARY_DIR}
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:STRING=${YASM_BINARY_DIR}
      -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${YASM_BINARY_DIR}
      # Install directories
      -DYASM_INSTALL_BIN_DIR:STRING=bin
      # Options
      -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
      -DBUILD_TESTING:BOOL=OFF
      -DBUILD_EXAMPLES:BOOL=OFF
      # Dependencies
      -DPYTHON_EXECUTABLE:STRING=${YASM_PYTHON_EXECUTABLE}
    #--Build step-----------------
    BUILD_ALWAYS 1
    INSTALL_COMMAND ""
    )
endif()

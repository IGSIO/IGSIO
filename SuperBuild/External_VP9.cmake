set(proj "VP9")

set(${proj}_DEPENDENCIES)

if(VP9_DIR)
  find_package(VP9 REQUIRED)
  set(IGSIO_VP9_DIR ${VP9_DIR})
else()

  set(IGSIO_VP9_DIR "${CMAKE_BINARY_DIR}/VP9")

  # VP9 has not been built yet, so download and build it as an external project
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include(${IGSIO_SOURCE_DIR}/SuperBuild/External_YASM.cmake)
    if(NOT YASM_FOUND)
      list(APPEND ${proj}_DEPENDENCIES YASM)
    endif()

    set (VP9_INCLUDE_DIR "${CMAKE_BINARY_DIR}/VP9/vpx" CACHE PATH "VP9 source directory" FORCE)
    set (VP9_LIBRARY_DIR "${CMAKE_BINARY_DIR}/VP9" CACHE PATH "VP9 library directory" FORCE)

    if(NOT DEFINED(VP9_GIT_REPOSITORY))
      set(VP9_GIT_REPOSITORY "https://github.com/webmproject/libvpx/" CACHE STRING "Set VP9 desired git url")
    endif()
    if(NOT DEFINED(VP9_GIT_REVISION))
      set(VP9_GIT_REVISION "v1.12.0" CACHE STRING "Set VP9 desired git hash (master means latest)")
    endif()

    message(STATUS "Downloading and compiling VP9 from https://github.com/webmproject/libvpx.git")
    ExternalProject_Add(VP9
      PREFIX "${CMAKE_BINARY_DIR}/VP9-prefix"
      GIT_REPOSITORY ${VP9_GIT_REPOSITORY}
      GIT_TAG ${VP9_GIT_REVISION}
      SOURCE_DIR        "${IGSIO_VP9_DIR}"
      CONFIGURE_COMMAND "${IGSIO_VP9_DIR}/configure" --disable-examples --as=yasm --enable-pic --disable-tools --disable-docs --disable-vp8 --disable-libyuv --disable-unit_tests --disable-postproc
      BUILD_ALWAYS 1
      BUILD_COMMAND PATH=${YASM_BINARY_DIR}:$ENV{PATH}; make
      BUILD_IN_SOURCE 1
      INSTALL_COMMAND   ""
      TEST_COMMAND      ""
      DEPENDS ${VP9_DEPENDENCIES}
    )
  else()
    if( ("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v120*") OR
        ("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v140*") OR
        ("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v141*") OR
        ("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v142*") OR
        ("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v143*"))
      set (VP9_INCLUDE_DIR "${IGSIO_VP9_DIR}/include/vpx" CACHE PATH "VP9 source directory" FORCE)
      set (BinaryURL "https://github.com/ShiftMediaProject/libvpx/releases/download/v1.12.0/libvpx_v1.12.0_")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set (VP9_LIBRARY_DIR "${IGSIO_VP9_DIR}/lib/x64" CACHE PATH "VP9 library directory" FORCE)
    else()
      set (VP9_LIBRARY_DIR "${IGSIO_VP9_DIR}/lib/x86" CACHE PATH "VP9 library directory" FORCE)
    endif()

    if("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v120*")
      set (BinaryURL "${BinaryURL}msvc12.zip")
    elseif("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v140*")
      set (BinaryURL "${BinaryURL}msvc14.zip")
    elseif("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v141*")
      set (BinaryURL "${BinaryURL}msvc15.zip")
    elseif("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v142*")
      set (BinaryURL "${BinaryURL}msvc16.zip")
    elseif("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v143*")
      set (BinaryURL "${BinaryURL}msvc17.zip")
    endif()

      message(STATUS "Downloading VP9 from ${BinaryURL}")
      ExternalProject_Add(VP9
        URL               ${BinaryURL}
        SOURCE_DIR        "${IGSIO_VP9_DIR}"
        CONFIGURE_COMMAND ""
        BUILD_ALWAYS 0
        BUILD_COMMAND ""
        INSTALL_COMMAND   ""
        TEST_COMMAND      ""
      )
    else()
      message(FATAL_ERROR "Unsupported version of Visual Studio for VP9. Supported versions are: MSVC12, MSVC14, MSVC15, MSVC16")
    endif()
  endif()
endif()

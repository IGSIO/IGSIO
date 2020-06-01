set(proj "VP9")

SET(${proj}_DEPENDENCIES)

IF(VP9_DIR)
  FIND_PACKAGE(VP9 REQUIRED)
  SET(IGSIO_VP9_DIR ${VP9_DIR})
ELSE()

  SET(IGSIO_VP9_DIR "${CMAKE_BINARY_DIR}/VP9")

  # VP9 has not been built yet, so download and build it as an external project
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    INCLUDE(${IGSIO_SOURCE_DIR}/SuperBuild/External_YASM.cmake)
    IF(NOT YASM_FOUND)
      LIST(APPEND ${proj}_DEPENDENCIES YASM)
    ENDIF()

    SET (VP9_INCLUDE_DIR "${CMAKE_BINARY_DIR}/VP9/vpx" CACHE PATH "VP9 source directory" FORCE)
    SET (VP9_LIBRARY_DIR "${CMAKE_BINARY_DIR}/VP9" CACHE PATH "VP9 library directory" FORCE)

    MESSAGE(STATUS "Downloading and compiling VP9 from https://github.com/webmproject/libvpx.git")
    ExternalProject_Add(VP9
      PREFIX "${CMAKE_BINARY_DIR}/VP9-prefix"
      GIT_REPOSITORY https://github.com/webmproject/libvpx/
      GIT_TAG v1.8.2
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
        ("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v142*"))
      SET (VP9_INCLUDE_DIR "${IGSIO_VP9_DIR}/include/vpx" CACHE PATH "VP9 source directory" FORCE)
      SET (BinaryURL "https://github.com/ShiftMediaProject/libvpx/releases/download/v1.8.2/libvpx_v1.8.2_")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      SET (VP9_LIBRARY_DIR "${IGSIO_VP9_DIR}/lib/x64" CACHE PATH "VP9 library directory" FORCE)
    else()
      SET (VP9_LIBRARY_DIR "${IGSIO_VP9_DIR}/lib/x86" CACHE PATH "VP9 library directory" FORCE)
    endif()

    if("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v120*")
      SET (BinaryURL "${BinaryURL}msvc12.zip")
    elseif("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v140*")
      SET (BinaryURL "${BinaryURL}msvc14.zip")
    elseif("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v141*")
      SET (BinaryURL "${BinaryURL}msvc15.zip")
    elseif("${CMAKE_VS_PLATFORM_TOOLSET}" MATCHES "v142*")
      SET (BinaryURL "${BinaryURL}msvc16.zip")
    endif()

      MESSAGE(STATUS "Downloading VP9 from ${BinaryURL}")
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
ENDIF()

cmake_minimum_required(VERSION 2.8.2)

SET(VP9_DEPENDENCIES)

IF(VP9_DIR)
  FIND_PACKAGE(VP9 REQUIRED)
  SET(IGSIO_VP9_DIR ${VP9_DIR})
ELSE()

  SET(IGSIO_VP9_DIR "${CMAKE_BINARY_DIR}/VP9-bin")

  # VP9 has not been built yet, so download and build it as an external project
  MESSAGE(STATUS "Downloading VP9 from https://github.com/webmproject/libvpx.git")
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    INCLUDE(${IGSIO_SOURCE_DIR}/SuperBuild/External_YASM.cmake)
    IF(NOT YASM_FOUND)
      LIST(APPEND VP9_DEPENDENCIES YASM)
    ENDIF()
    SET (VP9_INCLUDE_DIR "${CMAKE_BINARY_DIR}/VP9/vpx" CACHE PATH "VP9 source directory" FORCE)
    SET (VP9_LIBRARY_DIR "${CMAKE_BINARY_DIR}/VP9" CACHE PATH "VP9 library directory" FORCE)
    ExternalProject_Add(VP9
      PREFIX "${CMAKE_BINARY_DIR}/VP9-prefix"
      GIT_REPOSITORY https://github.com/webmproject/libvpx/
      GIT_TAG v1.6.1
      SOURCE_DIR        "${CMAKE_BINARY_DIR}/VP9"
      BINARY_DIR        "${CMAKE_BINARY_DIR}/VP9-bin"
      CONFIGURE_COMMAND "${CMAKE_BINARY_DIR}/VP9/configure" --disable-examples --as=yasm --enable-pic --disable-tools --disable-docs --disable-vp8 --disable-libyuv --disable-unit_tests --disable-postproc WORKING_DIRECTORY "${VP9_LIBRARY_DIR}"
      BUILD_ALWAYS 1
      BUILD_COMMAND PATH=${YASM_BINARY_DIR}:$ENV{PATH}; make
      INSTALL_COMMAND   ""
      TEST_COMMAND      ""
      DEPENDS ${VP9_DEPENDENCIES}
    )
  else()
    if( ("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 14 2015") OR ("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 14 2015 Win64" ) OR
        ("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 12 2013") OR ("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 12 2013 Win64" ) OR
        ("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017") OR ("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017 Win64" )
          )
      SET (VP9_INCLUDE_DIR "${CMAKE_BINARY_DIR}/VP9/include/vpx" CACHE PATH "VP9 source directory" FORCE)
      SET (BinaryURL "https://github.com/ShiftMediaProject/libvpx/releases/download/v1.7.0/libvpx_v1.7.0_")
      if("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017" )
        SET (VP9_LIBRARY_DIR "${CMAKE_BINARY_DIR}/VP9/lib/x86" CACHE PATH "VP9 library directory" FORCE)
        SET (BinaryURL "${BinaryURL}msvc15.zip")
      elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 15 2017 Win64" )
        SET (VP9_LIBRARY_DIR "${CMAKE_BINARY_DIR}/VP9/lib/x64" CACHE PATH "VP9 library directory" FORCE)
        SET (BinaryURL "${BinaryURL}msvc15.zip")
      elseif ("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 14 2015")
        SET (VP9_LIBRARY_DIR "${CMAKE_BINARY_DIR}/VP9/lib/x86" CACHE PATH "VP9 library directory" FORCE)
        SET (BinaryURL "${BinaryURL}msvc14.zip")
      elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 14 2015 Win64" )
        SET (VP9_LIBRARY_DIR "${CMAKE_BINARY_DIR}/VP9/lib/x64" CACHE PATH "VP9 library directory" FORCE)
        SET (BinaryURL "${BinaryURL}msvc14.zip")
      elseif ("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 12 2013")
        SET (VP9_LIBRARY_DIR "${CMAKE_BINARY_DIR}/VP9/lib/x86" CACHE PATH "VP9 library directory" FORCE)
        SET (BinaryURL "${BinaryURL}msvc12.zip")
      elseif("${CMAKE_GENERATOR}" STREQUAL "Visual Studio 12 2013 Win64" )
        SET (VP9_LIBRARY_DIR "${CMAKE_BINARY_DIR}/VP9/lib/x64" CACHE PATH "VP9 library directory" FORCE)
        SET (BinaryURL "${BinaryURL}msvc12.zip")
      endif()
      ExternalProject_Add(VP9
        URL               ${BinaryURL}
        SOURCE_DIR        "${CMAKE_BINARY_DIR}/VP9-bin"
        CONFIGURE_COMMAND ""
        BUILD_ALWAYS 0
        BUILD_COMMAND ""
        INSTALL_COMMAND   ""
        TEST_COMMAND      ""
      )
    else()
      message(ERROR "Unsupported version of Visual Studio for VP9. Supported versions are: MSVC12, MSVC14, MSVC15")
    endif()
  endif()
ENDIF()

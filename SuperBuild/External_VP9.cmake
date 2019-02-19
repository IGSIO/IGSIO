set(proj "VP9")

SET(${proj}_DEPENDENCIES)

IF(VP9_DIR)
  FIND_PACKAGE(VP9 REQUIRED)
  SET(IGSIO_VP9_DIR ${VP9_DIR})
ELSE()

  SET(IGSIO_VP9_DIR "${CMAKE_BINARY_DIR}/VP9-bin")

  # VP9 has not been built yet, so download and build it as an external project
  MESSAGE(STATUS "Downloading VP9 from https://github.com/webmproject/libvpx.git")
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(EP_SOURCE_DIR "${CMAKE_BINARY_DIR}/${proj}")

    INCLUDE(${IGSIO_SOURCE_DIR}/SuperBuild/External_YASM.cmake)
    IF(NOT YASM_FOUND)
      LIST(APPEND ${proj}_DEPENDENCIES YASM)
    ENDIF()

    SET(IGSIO_YASM_DIR "${CMAKE_BINARY_DIR}/yasm-bin")
    IF (YASM_DIR)
      SET(IGSIO_YASM_DIR ${YASM_DIR})
    ENDIF()

    include(ExternalProjectForNonCMakeProject)

    # environment
    set(_env_script ${CMAKE_BINARY_DIR}/${proj}_Env.cmake)
    ExternalProject_Write_SetBuildEnv_Commands(${_env_script})

    # configure step
    set(_configure_script ${CMAKE_BINARY_DIR}/${proj}_configure_step.cmake)
    file(WRITE ${_configure_script}
    "include(\"${_env_script}\")
  set(${proj}_WORKING_DIR \"${EP_SOURCE_DIR}\")
  ExternalProject_Execute(${proj} \"configure\" sh ${EP_SOURCE_DIR}/configure
  --disable-examples --as=yasm --enable-pic --disable-tools --disable-docs --disable-vp8 --disable-libyuv --disable-unit_tests --disable-postproc
  )
  ")

    # build step
    set(_build_script ${CMAKE_BINARY_DIR}/${proj}_build_step.cmake)
    file(WRITE ${_build_script}
    "include(\"${_env_script}\")
  set(${proj}_WORKING_DIR \"${EP_SOURCE_DIR}\")
  set(ENV{PATH} \"${IGSIO_YASM_DIR}:${IGSIO_YASM_DIR}/Debug:${IGSIO_YASM_DIR}/Release:$ENV{PATH}\")
  ExternalProject_Execute(${proj} \"build\" make)
  ")

    ExternalProject_Add(${proj}
      GIT_REPOSITORY "https://github.com/webmproject/libvpx.git"
      GIT_TAG "v1.6.1"
      SOURCE_DIR "${EP_SOURCE_DIR}"
      BUILD_IN_SOURCE 1
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -P ${_configure_script}
      BUILD_COMMAND ${CMAKE_COMMAND} -P ${_build_script}
      INSTALL_COMMAND ""
      DEPENDS ${${proj}_DEPENDENCIES}
      )

    set(VP9_LIBRARY_DIR "${EP_SOURCE_DIR}")

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

IF(vtkAddon_DIR)
  # vtkAddon has been built already
  FIND_PACKAGE(vtkAddon REQUIRED NO_MODULE)
  MESSAGE(STATUS "Using vtkAddon available at: ${vtkAddon_DIR}")
  SET(IGSIO_vtkAddon_DIR ${vtkAddon_DIR})
ELSE()

  IF (NOT vtkAddon_INSTALL_BIN_DIR)
    set(vtkAddon_INSTALL_BIN_DIR ${IGSIO_INSTALL_BIN_DIR})
  ENDIF()
  IF (NOT vtkAddon_INSTALL_LIB_DIR)
    set(vtkAddon_INSTALL_LIB_DIR ${IGSIO_INSTALL_LIB_DIR})
  ENDIF()

  IF (NOT vtkAddon_CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(vtkAddon_CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  ENDIF()

  IF (NOT vtkAddon_CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(vtkAddon_CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  ENDIF()

  SET (IGSIO_vtkAddon_SRC_DIR ${CMAKE_BINARY_DIR}/vtkAddon CACHE INTERNAL "Path to store vtkAddon contents.")
  SET (IGSIO_vtkAddon_PREFIX_DIR ${CMAKE_BINARY_DIR}/vtkAddon-prefix CACHE INTERNAL "Path to store vtkAddon prefix data.")
  SET (IGSIO_vtkAddon_DIR ${CMAKE_BINARY_DIR}/vtkAddon-bin CACHE INTERNAL "Path to store vtkAddon binaries")

  IF(WIN32)
    set(ep_common_cxx_flags
      /DWIN32
      )
  ENDIF()

  IF(NOT DEFINED(vtkAddon_GIT_REPOSITORY))
    SET(vtkAddon_GIT_REPOSITORY "https://github.com/Slicer/vtkAddon.git" CACHE STRING "Set vtkAddon desired git url")
  ENDIF()
  IF(NOT DEFINED(vtkAddon_GIT_REVISION))
    SET(vtkAddon_GIT_REVISION "main" CACHE STRING "Set vtkAddon desired git hash (main means latest)")
  ENDIF()

  ExternalProject_Add(vtkAddon
    PREFIX ${PLUS_IGSIO_PREFIX_DIR}
    "${PLUSBUILD_EXTERNAL_PROJECT_CUSTOM_COMMANDS}"
    SOURCE_DIR "${IGSIO_vtkAddon_SRC_DIR}"
    BINARY_DIR "${IGSIO_vtkAddon_DIR}"
    #--Download step--------------
    GIT_REPOSITORY  ${vtkAddon_GIT_REPOSITORY}
    GIT_TAG ${vtkAddon_GIT_REVISION}
    #--Configure step-------------
    CMAKE_ARGS
      ${ep_common_args}
      -DEXECUTABLE_OUTPUT_PATH:PATH=${${PROJECT_NAME}_INSTALL_BIN_DIR}
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=${vtkAddon_CMAKE_RUNTIME_OUTPUT_DIRECTORY}
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH=${vtkAddon_CMAKE_LIBRARY_OUTPUT_DIRECTORY}
      -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
      -DCMAKE_MACOSX_RPATH:BOOL=${CMAKE_MACOSX_RPATH}

      -DCMAKE_CXX_FLAGS:STRING=${ep_common_cxx_flags}
      -DCMAKE_C_FLAGS:STRING=${ep_common_c_flags}
      -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}

      -DVTK_DIR:PATH=${VTK_DIR}
      -DvtkAddon_INSTALL_BIN_DIR:PATH=${vtkAddon_INSTALL_BIN_DIR}
      -DvtkAddon_INSTALL_LIB_DIR:PATH=${vtkAddon_INSTALL_LIB_DIR}
      -DQt5_DIR:PATH=${Qt5_DIR}

    #--Build step-----------------
    BUILD_ALWAYS 1
    #--Install step-----------------
    INSTALL_COMMAND ""
    DEPENDS ${IGSIO_DEPENDENCIES}
    )

  SET(IGSIO_vtkAddon_DIR "${CMAKE_BINARY_DIR}/vtkAddon-bin")
ENDIF()

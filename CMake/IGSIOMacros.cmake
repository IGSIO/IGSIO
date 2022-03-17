MACRO(IGSIOInstallLibrary _target_name _variable_root)
  IF(CMAKE_CONFIGURATION_TYPES)
    SET(CONFIG ${CMAKE_CONFIGURATION_TYPES})
  ELSEIF(CMAKE_BUILD_TYPE)
    SET(CONFIG ${CMAKE_BUILD_TYPE})
  ELSE()
    SET(CONFIG Release)
  ENDIF()
  
  INSTALL(TARGETS ${_target_name} EXPORT IGSIO
    RUNTIME DESTINATION "${IGSIO_INSTALL_BIN_DIR}" CONFIGURATIONS ${CONFIG} COMPONENT RuntimeLibraries
    LIBRARY DESTINATION "${IGSIO_INSTALL_LIB_DIR}" CONFIGURATIONS ${CONFIG} COMPONENT RuntimeLibraries
    ARCHIVE DESTINATION "${IGSIO_INSTALL_ARCHIVE_DIR}" CONFIGURATIONS ${CONFIG} COMPONENT Development
    )
  INSTALL(FILES ${${_variable_root}_HDRS}
    DESTINATION "${IGSIO_INCLUDE_INSTALL}" COMPONENT Development
    )
  GET_TARGET_PROPERTY(_library_type ${_target_name} TYPE)
  IF(${_library_type} STREQUAL SHARED_LIBRARY AND MSVC)
    INSTALL(FILES "$<TARGET_PDB_FILE:${_target_name}>" OPTIONAL
      DESTINATION "${IGSIO_INSTALL_BIN_DIR}" COMPONENT RuntimeLibraries
      )
  ENDIF()
ENDMACRO()

MACRO(IGSIOAddVersionInfo target_name description internal_name product_name)
  IF(MSVC)
    IF(NOT TARGET ${target_name})
      MESSAGE(FATAL_ERROR IGSIOAddVersionInfo called but target parameter does not exist)
    ENDIF()

    # Configure file does not see these variables unless we re-set them locally
    set(target_name ${target_name})
    set(description ${description})
    set(internal_name ${internal_name})
    set(product_name ${product_name})

    CONFIGURE_FILE(
      ${CMAKE_SOURCE_DIR}/CMake/MSVCVersion.rc.in 
      ${CMAKE_CURRENT_BINARY_DIR}/${target_name}MSVCVersion.rc
      )

    GET_TARGET_PROPERTY(_target_type ${target_name} TYPE)
    IF(${_target_type} STREQUAL "EXECUTABLE")
      TARGET_SOURCES(${target_name} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/${target_name}MSVCVersion.rc)
    ELSE()
      TARGET_SOURCES(${target_name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${target_name}MSVCVersion.rc)
    ENDIF()

    # If this macro was called from the target they're currently configuring
    IF(${PROJECT_NAME} STREQUAL ${target_name} OR vtk${PROJECT_NAME} STREQUAL ${target_name})
      SOURCE_GROUP(Resources FILES ${CMAKE_CURRENT_BINARY_DIR}/${target_name}MSVCVersion.rc)
    ENDIF()
  ENDIF()
ENDMACRO()

MACRO(CopyLibrariesToDirectory _destination)
  FOREACH(lib ${ARGN})
    IF(NOT TARGET ${lib})
      continue()
    ENDIF()
    
    GET_TARGET_PROPERTY(_lib_type ${lib} TYPE)

    IF(NOT ${_lib_type} STREQUAL "INTERFACE_LIBRARY")
      GET_TARGET_PROPERTY(_debug_file ${lib} IMPORTED_LOCATION_DEBUG)
      GET_TARGET_PROPERTY(_release_file ${lib} IMPORTED_LOCATION_RELEASE)
      GET_TARGET_PROPERTY(_rel_deb_info_file ${lib} IMPORTED_LOCATION_RELWITHDEBINFO)
      GET_TARGET_PROPERTY(_min_size_rel_file ${lib} IMPORTED_LOCATION_MINSIZEREL)
      GET_TARGET_PROPERTY(_pdb_name ${lib} PDB_NAME)
      GET_TARGET_PROPERTY(_pdb_directory ${lib} PDB_OUTPUT_DIRECTORY)

      IF(MSVC OR ${CMAKE_GENERATOR} MATCHES "Xcode")
        # Release
        IF(EXISTS ${_release_file})
          FILE(COPY ${_release_file} DESTINATION ${_destination}/Release)
        ENDIF()

        # Debug
        IF(EXISTS ${_debug_file})
          FILE(COPY ${_debug_file} DESTINATION ${_destination}/Debug)
        ENDIF()

        # Release with debug info
        IF(EXISTS ${_rel_deb_info_file})
          FILE(COPY ${_rel_deb_info_file} DESTINATION ${_destination}/RelWithDebInfo)
        ELSEIF(EXISTS ${_release_file})
          FILE(COPY ${_release_file} DESTINATION ${_destination}/RelWithDebInfo)
        ENDIF()

        # Min size release
        IF(EXISTS ${_min_size_rel_file})
          FILE(COPY ${_min_size_rel_file} DESTINATION ${_destination}/MinSizeRel)
        ELSEIF(EXISTS ${_release_file})
          FILE(COPY ${_release_file} DESTINATION ${_destination}/MinSizeRel)
        ENDIF()
      ELSE()
        IF(_debug_file EQUAL _release_file AND EXISTS ${_release_file})
          FILE(COPY ${_release_file} DESTINATION ${_destination})
        ELSE()
          IF(EXISTS ${_release_file})
            FILE(COPY ${_release_file} DESTINATION ${_destination})
          ENDIF()
          IF(EXISTS ${_debug_file})
            FILE(COPY ${_debug_file} DESTINATION ${_destination})
          ENDIF()
        ENDIF()
      ENDIF()
    ENDIF()
  ENDFOREACH()
ENDMACRO()

# Library export directive file generation
# This macro generates a ...Export.h file that specifies platform-specific DLL export directives,
# for example on Windows: __declspec( dllexport )
MACRO(GENERATE_EXPORT_DIRECTIVE_FILE LIBRARY_NAME)
  SET(MY_LIBNAME ${LIBRARY_NAME})
  SET(MY_EXPORT_HEADER_PREFIX ${MY_LIBNAME})
  SET(MY_LIBRARY_EXPORT_DIRECTIVE "${MY_LIBNAME}Export")
  CONFIGURE_FILE(
    ${IGSIO_SOURCE_DIR}/igsioExport.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/${MY_EXPORT_HEADER_PREFIX}Export.h
    )
  INSTALL(FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/${MY_EXPORT_HEADER_PREFIX}Export.h
    DESTINATION "${IGSIO_INCLUDE_INSTALL}"
    )
ENDMACRO()

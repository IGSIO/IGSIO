macro(IGSIOInstallLibrary _target_name _variable_root)
  if(CMAKE_CONFIGURATION_TYPES)
    set(CONFIG ${CMAKE_CONFIGURATION_TYPES})
  elseif(CMAKE_BUILD_TYPE)
    set(CONFIG ${CMAKE_BUILD_TYPE})
  else()
    set(CONFIG Release)
  endif()
  
  install(TARGETS ${_target_name} EXPORT IGSIO
    RUNTIME DESTINATION "${IGSIO_INSTALL_BIN_DIR}" CONFIGURATIONS ${CONFIG} COMPONENT RuntimeLibraries
    LIBRARY DESTINATION "${IGSIO_INSTALL_LIB_DIR}" CONFIGURATIONS ${CONFIG} COMPONENT RuntimeLibraries
    ARCHIVE DESTINATION "${IGSIO_INSTALL_ARCHIVE_DIR}" CONFIGURATIONS ${CONFIG} COMPONENT Development
    )
  install(FILES ${${_variable_root}_HDRS}
    DESTINATION "${IGSIO_INCLUDE_INSTALL}" COMPONENT Development
    )
  get_target_property(_library_type ${_target_name} TYPE)
  if(${_library_type} STREQUAL SHARED_LIBRARY AND MSVC)
    install(FILES "$<TARGET_PDB_FILE:${_target_name}>" OPTIONAL
      DESTINATION "${IGSIO_INSTALL_BIN_DIR}" COMPONENT RuntimeLibraries
      )
  endif()
endmacro()

macro(IGSIOAddVersionInfo target_name description internal_name product_name)
  if(MSVC)
    if(NOT TARGET ${target_name})
      message(FATAL_ERROR IGSIOAddVersionInfo called but target parameter does not exist)
    endif()

    # Configure file does not see these variables unless we re-set them locally
    set(target_name ${target_name})
    set(description ${description})
    set(internal_name ${internal_name})
    set(product_name ${product_name})

    configure_file(
      ${CMAKE_SOURCE_DIR}/CMake/MSVCVersion.rc.in 
      ${CMAKE_CURRENT_BINARY_DIR}/${target_name}MSVCVersion.rc
      )

    get_target_property(_target_type ${target_name} TYPE)
    if(${_target_type} STREQUAL "EXECUTABLE")
      target_sources(${target_name} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/${target_name}MSVCVersion.rc)
    else()
      target_sources(${target_name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${target_name}MSVCVersion.rc)
    endif()

    # If this macro was called from the target they're currently configuring
    if(${PROJECT_NAME} STREQUAL ${target_name} OR vtk${PROJECT_NAME} STREQUAL ${target_name})
      source_group(Resources FILES ${CMAKE_CURRENT_BINARY_DIR}/${target_name}MSVCVersion.rc)
    endif()
  endif()
endmacro()

macro(CopyLibrariesToDirectory _destination)
  foreach(lib ${ARGN})
    if(NOT TARGET ${lib})
      continue()
    endif()
    
    get_target_property(_lib_type ${lib} TYPE)

    if(NOT ${_lib_type} STREQUAL "INTERFACE_LIBRARY")
      get_target_property(_debug_file ${lib} IMPORTED_LOCATION_DEBUG)
      get_target_property(_release_file ${lib} IMPORTED_LOCATION_RELEASE)
      get_target_property(_rel_deb_info_file ${lib} IMPORTED_LOCATION_RELWITHDEBINFO)
      get_target_property(_min_size_rel_file ${lib} IMPORTED_LOCATION_MINSIZEREL)
      get_target_property(_pdb_name ${lib} PDB_NAME)
      get_target_property(_pdb_directory ${lib} PDB_OUTPUT_DIRECTORY)

      if(MSVC OR ${CMAKE_GENERATOR} MATCHES "Xcode")
        # Release
        if(EXISTS ${_release_file})
          file(COPY ${_release_file} DESTINATION ${_destination}/Release)
        endif()

        # Debug
        if(EXISTS ${_debug_file})
          file(COPY ${_debug_file} DESTINATION ${_destination}/Debug)
        endif()

        # Release with debug info
        if(EXISTS ${_rel_deb_info_file})
          file(COPY ${_rel_deb_info_file} DESTINATION ${_destination}/RelWithDebInfo)
        elseif(EXISTS ${_release_file})
          file(COPY ${_release_file} DESTINATION ${_destination}/RelWithDebInfo)
        endif()

        # Min size release
        if(EXISTS ${_min_size_rel_file})
          file(COPY ${_min_size_rel_file} DESTINATION ${_destination}/MinSizeRel)
        elseif(EXISTS ${_release_file})
          file(COPY ${_release_file} DESTINATION ${_destination}/MinSizeRel)
        endif()
      else()
        if(_debug_file EQUAL _release_file AND EXISTS ${_release_file})
          file(COPY ${_release_file} DESTINATION ${_destination})
        else()
          if(EXISTS ${_release_file})
            file(COPY ${_release_file} DESTINATION ${_destination})
          endif()
          if(EXISTS ${_debug_file})
            file(COPY ${_debug_file} DESTINATION ${_destination})
          endif()
        endif()
      endif()
    endif()
  endforeach()
endmacro()

# Library export directive file generation
# This macro generates a ...Export.h file that specifies platform-specific DLL export directives,
# for example on Windows: __declspec( dllexport )
macro(GENERATE_EXPORT_DIRECTIVE_FILE LIBRARY_NAME)
  set(MY_LIBNAME ${LIBRARY_NAME})
  set(MY_EXPORT_HEADER_PREFIX ${MY_LIBNAME})
  set(MY_LIBRARY_EXPORT_DIRECTIVE "${MY_LIBNAME}Export")
  configure_file(
    ${IGSIO_SOURCE_DIR}/igsioExport.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/${MY_EXPORT_HEADER_PREFIX}Export.h
    )
  install(FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/${MY_EXPORT_HEADER_PREFIX}Export.h
    DESTINATION "${IGSIO_INCLUDE_INSTALL}"
    )
endmacro()

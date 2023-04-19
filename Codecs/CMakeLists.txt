project(vtkIGSIOCodecs)

set(${PROJECT_NAME}_EXPORT_DIRECTIVE "VTK_IGSIO_CODECS_EXPORT")

if (NOT IGSIO_USE_3DSlicer)
  find_package(vtkAddon REQUIRED)
else()
  find_package(Slicer REQUIRED)
endif()

set(${PROJECT_NAME}_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_LIBRARY_DIR}
  ${CMAKE_CURRENT_ARCHIVE_DIR}
  ${vtkAddon_INCLUDE_DIRS}
  )

set(${PROJECT_NAME}_SRCS
  )

set(${PROJECT_NAME}_LIBS
  ${VTK_LIBRARIES}
  vtkAddon
  )

set(${PROJECT_NAME}_LIBS_PRIVATE
  )

if(IGSIO_USE_VP9)
  find_package(VP9 REQUIRED)
  list(APPEND ${PROJECT_NAME}_INCLUDE_DIRS
    ${VP9_INCLUDE_DIR}
    )
  list(APPEND ${PROJECT_NAME}_SRCS
    vtkVP9VolumeCodec.cxx
    vtkVP9VolumeCodec.h
    )

  list(APPEND ${PROJECT_NAME}_LIBS_PRIVATE
    ${VP9_STATIC_LIBRARIES}
    )
  link_directories(${VP9_LIBRARY_DIR})
endif()

if(NOT ${PROJECT_NAME}_SRCS)
  # No codecs are enabled
  # Nothing to compile
  return()
endif()

#-----------------------------------------------------------------------------
# Create the library
if(MSVC OR ${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
else()
  add_library(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS})
endif()
target_link_libraries(${PROJECT_NAME}
  PUBLIC ${${PROJECT_NAME}_LIBS}
  PRIVATE ${${PROJECT_NAME}_LIBS_PRIVATE}
  )
if (NOT IGSIO_USE_3DSlicer)
  add_dependencies(${PROJECT_NAME}
    vtkAddon
  )
endif()

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  RUNTIME DESTINATION ${IGSIO_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${IGSIO_INSTALL_DATA_DIR} COMPONENT Development)

foreach(p IN LISTS ${PROJECT_NAME}_INCLUDE_DIRS)
  target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${p}>)
endforeach()
target_include_directories(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${${PROJECT_NAME}_INSTALL_INCLUDE_DIR}>)

GENERATE_EXPORT_HEADER(${PROJECT_NAME})

set_property(GLOBAL APPEND PROPERTY IGSIO_TARGETS ${PROJECT_NAME})

set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} CACHE INTERNAL "")

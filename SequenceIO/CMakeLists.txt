project(vtkSequenceIO)

if (IGSIO_SEQUENCEIO_ENABLE_MKV)
  find_package(libwebm REQUIRED)
endif()

# --------------------------------------------------------------------------
# Configure library
set(${PROJECT_NAME}_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${vtkIGSIOCommon_INCLUDES}
  CACHE INTERNAL ""
  )

set(${PROJECT_NAME}_SRCS
  vtkIGSIOSequenceIO.cxx
  vtkIGSIOSequenceIOBase.cxx
  vtkIGSIOMetaImageSequenceIO.cxx
  vtkIGSIONrrdSequenceIO.cxx
  )

set(${PROJECT_NAME}_HDRS
  vtkIGSIOSequenceIO.h
  vtkIGSIOSequenceIOBase.h
  vtkIGSIOMetaImageSequenceIO.h
  vtkIGSIONrrdSequenceIO.h
  )

set(${PROJECT_NAME}_LIBS
  ${IGSIO_VTK_PREFIX}CommonCore
  ${IGSIO_VTK_PREFIX}CommonDataModel
  ${IGSIO_VTK_PREFIX}IOImage
  vtkIGSIOCommon
  )

if(VTK_VERSION VERSION_LESS 8.9.0)
  list(APPEND ${PROJECT_NAME}_LIBS vtksys)
else()
  list(APPEND ${PROJECT_NAME}_LIBS VTK::vtksys)
endif()

set(${PROJECT_NAME}_LIBS_PRIVATE
  )

if (IGSIO_SEQUENCEIO_ENABLE_MKV)
  set(MKV_SRCS
    vtkIGSIOMkvSequenceIO.cxx
    )
  set(MKV_HDRS
    vtkIGSIOMkvSequenceIO.h
    )

  list(APPEND ${PROJECT_NAME}_INCLUDE_DIRS
    ${LIBWEBM_INCLUDE_DIRS}
    )
  list(APPEND ${PROJECT_NAME}_SRCS
    ${MKV_SRCS}
    )
  list(APPEND ${PROJECT_NAME}_HDRS
    ${MKV_HDRS}
    )

  list(APPEND ${PROJECT_NAME}_LIBS_PRIVATE
    ${LIBWEBM_LIBRARIES}
    )

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

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  RUNTIME DESTINATION ${IGSIO_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT Development)

foreach(p IN LISTS ${PROJECT_NAME}_INCLUDE_DIRS)
  target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${p}>)
endforeach()
target_include_directories(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${${PROJECT_NAME}_INSTALL_INCLUDE_DIR}>)

GENERATE_EXPORT_HEADER(${PROJECT_NAME})

set_property(GLOBAL APPEND PROPERTY IGSIO_TARGETS ${PROJECT_NAME})

set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} CACHE INTERNAL "")

# --------------------------------------------------------------------------
# Testing
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()

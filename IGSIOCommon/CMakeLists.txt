project(vtkIGSIOCommon)

if (IGSIO_USE_3DSlicer)
  find_package(Slicer REQUIRED)
else()
  find_package(vtkAddon REQUIRED)
  include(${vtkAddon_DIR}/vtkAddonTargets.cmake)
endif()

# --------------------------------------------------------------------------
# Configure library
set(${PROJECT_NAME}_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_BINARY_DIR}
  ${vtkAddon_INCLUDE_DIRS}
  ${IGSIOZLIB_INCLUDE_DIR}
  
  CACHE INTERNAL ""
  )

if(IGSIO_BUILD_CODECS)
  list(APPEND ${PROJECT_NAME}_INCLUDE_DIRS ${vtkIGSIOCodecs_INCLUDE_DIRS})
endif()

set(${PROJECT_NAME}_SRCS
  igsioCommon.cxx
  vtkIGSIOLogger.cxx
  igsioMath.cxx
  vtkIGSIOAccurateTimer.cxx
  igsioVideoFrame.cxx
  igsioTrackedFrame.cxx
  vtkIGSIOFrameConverter.cxx
  vtkIGSIOTrackedFrameList.cxx
  vtkIGSIOTransformRepository.cxx
  vtkIGSIORecursiveCriticalSection.cxx
  )

set(${PROJECT_NAME}_HDRS
  igsioCommon.h
  igsioCommon.txx
  igsioXMLUtils.h
  vtkIGSIOLogger.h
  igsioMath.h
  vtkIGSIOAccurateTimer.h
  WindowsAccurateTimer.h
  igsioVideoFrame.h
  igsioTrackedFrame.h
  vtkIGSIOFrameConverter.h
  vtkIGSIOTrackedFrameList.h
  vtkIGSIOTransformRepository.h
  vtkIGSIORecursiveCriticalSection.h
  )

set(${PROJECT_NAME}_LIBS
  ${IGSIO_VTK_PREFIX}CommonCore
  ${IGSIO_VTK_PREFIX}IOImage
  ${IGSIO_VTK_PREFIX}IOXMLParser
  ${IGSIO_VTK_PREFIX}ImagingCore
  ${IGSIO_VTK_PREFIX}CommonSystem
  vtkAddon
  itkvnl_algo
  )

if(VTK_VERSION VERSION_LESS 8.9.0)
  list(APPEND ${PROJECT_NAME}_LIBS vtksys)
else()
  list(APPEND ${PROJECT_NAME}_LIBS VTK::vtksys)
endif()

if(IGSIO_BUILD_CODECS)
  list(APPEND ${PROJECT_NAME}_LIBS
    vtkIGSIOCodecs
    )
endif()

set(${PROJECT_NAME}_LIBS_PRIVATE
  )

if(WIN32)
  list(APPEND ${PROJECT_NAME}_LIBS
    Winmm
    )
endif(WIN32)

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

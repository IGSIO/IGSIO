PROJECT(vtkIGSIOCommon)

IF (NOT USE_SLICER)
  FIND_PACKAGE(vtkAddon REQUIRED)
ENDIF()

# --------------------------------------------------------------------------
# Configure library
SET(${PROJECT_NAME}_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_BINARY_DIR}
  ${vtkAddon_INCLUDE_DIRS}
  ${IGSIOZLIB_INCLUDE_DIR}
  CACHE INTERNAL ""
  )

IF(IGSIO_BUILD_CODECS)
  LIST(APPEND ${PROJECT_NAME}_INCLUDE_DIRS ${vtkIGSIOCodecs_INCLUDE_DIRS})
ENDIF()

SET(${PROJECT_NAME}_SRCS
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

SET(${PROJECT_NAME}_HDRS
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

SET(${PROJECT_NAME}_LIBS
  vtkCommonCore
  vtksys
  vtkIOImage
  vtkIOXMLParser
  vtkImagingCore
  vtkCommonSystem
  vtkAddon
  )

IF(IGSIO_BUILD_CODECS)
  LIST(APPEND ${PROJECT_NAME}_LIBS vtkIGSIOCodecs)
ENDIF()

SET(${PROJECT_NAME}_LIBS_PRIVATE
  )

IF(WIN32)
  LIST(APPEND ${PROJECT_NAME}_LIBS Winmm)
ENDIF(WIN32)

#-----------------------------------------------------------------------------
# Create the library
IF(MSVC OR ${CMAKE_GENERATOR} MATCHES "Xcode")
  ADD_LIBRARY(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
ELSE()
  ADD_LIBRARY(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS})
ENDIF()
TARGET_LINK_LIBRARIES(${PROJECT_NAME}
  PUBLIC ${${PROJECT_NAME}_LIBS}
  PRIVATE ${${PROJECT_NAME}_LIBS_PRIVATE}
  )

INSTALL(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  RUNTIME DESTINATION ${IGSIO_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${IGSIO_INSTALL_LIB_DIR} COMPONENT Development)

FOREACH(p IN LISTS ${PROJECT_NAME}_INCLUDE_DIRS)
  TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${p}>)
ENDFOREACH()
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${${PROJECT_NAME}_INSTALL_INCLUDE_DIR}>)

GENERATE_EXPORT_HEADER(${PROJECT_NAME})

SET_PROPERTY(GLOBAL APPEND PROPERTY IGSIO_TARGETS ${PROJECT_NAME})

SET(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} CACHE INTERNAL "")

# --------------------------------------------------------------------------
# Testing
IF(BUILD_TESTING)
  ADD_SUBDIRECTORY(Testing)
ENDIF()

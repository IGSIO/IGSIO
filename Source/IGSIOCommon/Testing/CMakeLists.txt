SET( TestDataDir ${CMAKE_CURRENT_SOURCE_DIR}/Data )

#--------------------------------------------------------------------------------------------
function(ADD_COMPARE_FILES_TEST TestName DependsOnTestName TestFileName)

  # If a platform-specific reference file is found then use that
  IF(WIN32)
    SET(PLATFORM "Windows")
  ELSE()
    SET(PLATFORM "Linux")
  ENDIF()
  SET(CommonFilePath "${TestDataDir}/${TestFileName}")
  SET(PlatformSpecificFilePath "${TestDataDir}/${PLATFORM}/${TestFileName}")
  if(EXISTS "${PlatformSpecificFilePath}")
    SET(FoundReferenceFilePath ${PlatformSpecificFilePath})
  ELSE()
    SET(FoundReferenceFilePath ${CommonFilePath})
  endif()

  ADD_TEST(${TestName} ${CMAKE_COMMAND} -E compare_files "${TEST_OUTPUT_PATH}/${TestFileName}" "${FoundReferenceFilePath}")
  SET_TESTS_PROPERTIES(${TestName} PROPERTIES DEPENDS ${DependsOnTestName})

endfunction()

#--------------------------------------------------------------------------------------------
ADD_EXECUTABLE(vtkIGSIOLoggerTest vtkIGSIOLoggerTest.cxx )
SET_TARGET_PROPERTIES(vtkIGSIOLoggerTest PROPERTIES FOLDER Tests)
TARGET_LINK_LIBRARIES(vtkIGSIOLoggerTest vtkIGSIOCommon vtkIGSIOCommon )

ADD_TEST(vtkIGSIOLoggerTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkIGSIOLoggerTest
  --verbose=5
  )

#--------------------------------------------------------------------------------------------
ADD_EXECUTABLE(igsioCommonTest igsioCommonTest.cxx )
SET_TARGET_PROPERTIES(igsioCommonTest PROPERTIES FOLDER Tests)
TARGET_LINK_LIBRARIES(igsioCommonTest vtkIGSIOCommon vtkIGSIOCommon )

ADD_TEST(igsioCommonTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/igsioCommonTest
  )

#--------------------------------------------------------------------------------------------
ADD_EXECUTABLE(igsioMathTest igsioMathTest.cxx )
SET_TARGET_PROPERTIES(igsioMathTest PROPERTIES FOLDER Tests)
TARGET_LINK_LIBRARIES(igsioMathTest vtkIGSIOCommon vtkIGSIOCommon )

ADD_TEST(igsioMathTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/igsioMathTest
  --xml-file=${TestDataDir}/igsioMathTestData.xml
  )

#--------------------------------------------------------------------------------------------
ADD_EXECUTABLE(AccurateTimerTest AccurateTimerTest.cxx )
SET_TARGET_PROPERTIES(AccurateTimerTest PROPERTIES FOLDER Tests)
TARGET_LINK_LIBRARIES(AccurateTimerTest vtkIGSIOCommon vtkIGSIOCommon )

ADD_TEST(AccurateTimerTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AccurateTimerTest
  --testTimeSec=10
  --averageIntendedDelaySec=0.005
  --numberOfThreads=3
  --verbose=3
  )
SET_TESTS_PROPERTIES(AccurateTimerTest PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING")

#--------------------------------------------------------------------------------------------
ADD_EXECUTABLE(vtkTransformRepositoryTest vtkTransformRepositoryTest.cxx )
SET_TARGET_PROPERTIES(vtkTransformRepositoryTest PROPERTIES FOLDER Tests)
TARGET_LINK_LIBRARIES(vtkTransformRepositoryTest vtkIGSIOCommon vtkIGSIOCommon )

ADD_TEST(vtkTransformRepositoryTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkTransformRepositoryTest
  --verbose=3
  )


#--------------------------------------------------------------------------------------------
# Install
#
INSTALL(
  TARGETS
    AccurateTimerTest
  DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
  COMPONENT RuntimeExecutables
  )

set( TestDataDir ${CMAKE_CURRENT_SOURCE_DIR}/Data )

#--------------------------------------------------------------------------------------------
function(ADD_COMPARE_FILES_TEST TestName DependsOnTestName TestFileName)

  # If a platform-specific reference file is found then use that
  if(WIN32)
    set(PLATFORM "Windows")
  else()
    set(PLATFORM "Linux")
  endif()
  set(CommonFilePath "${TestDataDir}/${TestFileName}")
  set(PlatformSpecificFilePath "${TestDataDir}/${PLATFORM}/${TestFileName}")
  if(EXISTS "${PlatformSpecificFilePath}")
    set(FoundReferenceFilePath ${PlatformSpecificFilePath})
  else()
    set(FoundReferenceFilePath ${CommonFilePath})
  endif()

  add_test(${TestName} ${CMAKE_COMMAND} -E compare_files "${TEST_OUTPUT_PATH}/${TestFileName}" "${FoundReferenceFilePath}")
  set_tests_properties(${TestName} PROPERTIES DEPENDS ${DependsOnTestName})

endfunction()

#--------------------------------------------------------------------------------------------
add_executable(vtkIGSIOLoggerTest vtkIGSIOLoggerTest.cxx )
set_target_properties(vtkIGSIOLoggerTest PROPERTIES FOLDER Tests)
target_link_libraries(vtkIGSIOLoggerTest vtkIGSIOCommon vtkIGSIOCommon )

add_test(vtkIGSIOLoggerTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkIGSIOLoggerTest
  --verbose=5
  )

#--------------------------------------------------------------------------------------------
add_executable(igsioCommonTest igsioCommonTest.cxx )
set_target_properties(igsioCommonTest PROPERTIES FOLDER Tests)
target_link_libraries(igsioCommonTest vtkIGSIOCommon vtkIGSIOCommon )

add_test(igsioCommonTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/igsioCommonTest
  )

#--------------------------------------------------------------------------------------------
add_executable(igsioMathTest igsioMathTest.cxx )
set_target_properties(igsioMathTest PROPERTIES FOLDER Tests)
target_link_libraries(igsioMathTest vtkIGSIOCommon vtkIGSIOCommon )

add_test(igsioMathTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/igsioMathTest
  --xml-file=${TestDataDir}/igsioMathTestData.xml
  )

#--------------------------------------------------------------------------------------------
add_executable(AccurateTimerTest AccurateTimerTest.cxx )
set_target_properties(AccurateTimerTest PROPERTIES FOLDER Tests)
target_link_libraries(AccurateTimerTest vtkIGSIOCommon vtkIGSIOCommon )

add_test(AccurateTimerTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/AccurateTimerTest
  --testTimeSec=10
  --averageIntendedDelaySec=0.005
  --numberOfThreads=3
  --verbose=3
  )
set_tests_properties(AccurateTimerTest PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING")

#--------------------------------------------------------------------------------------------
add_executable(vtkTransformRepositoryTest vtkTransformRepositoryTest.cxx )
set_target_properties(vtkTransformRepositoryTest PROPERTIES FOLDER Tests)
target_link_libraries(vtkTransformRepositoryTest vtkIGSIOCommon vtkIGSIOCommon )

add_test(vtkTransformRepositoryTest
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkTransformRepositoryTest
  --verbose=3
  )


#--------------------------------------------------------------------------------------------
# Install
#
install(
  TARGETS
    AccurateTimerTest
  DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
  COMPONENT RuntimeExecutables
  )


set(SEQUENCEIO_TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Data)

#*************************** vtkMetaImageSequenceIOTest  ***************************
add_executable(vtkMetaImageSequenceIOTest vtkMetaImageSequenceIOTest.cxx )
set_target_properties(vtkMetaImageSequenceIOTest PROPERTIES FOLDER Tests)
target_link_libraries(vtkMetaImageSequenceIOTest vtkSequenceIO)
add_test(vtkMetaImageSequenceIOTest 
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkMetaImageSequenceIOTest
  --img-seq-file=${SEQUENCEIO_TEST_DATA_DIR}/MetaImageSequenceIOTest1.igs.mhd
  --output-img-seq-file=MetaImageSequenceIOTest1Output.igs.mha
  )
set_tests_properties(vtkMetaImageSequenceIOTest PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING")

if (IGSIO_SEQUENCEIO_ENABLE_MKV)

  #*************************** vtkMkvSequenceIOTest ***************************
  add_executable(vtkMkvSequenceIOTest vtkMkvSequenceIOTest.cxx)
  set_target_properties(vtkMkvSequenceIOTest PROPERTIES FOLDER Tests)
  target_link_libraries(vtkMkvSequenceIOTest vtkSequenceIO)
  add_test(vtkMkvSequenceIOTest 
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkMkvSequenceIOTest
      --input-filename=${SEQUENCEIO_TEST_DATA_DIR}/uncompressedRV24.mkv
      --output-filename=mkvWriterTest1.mkv
    )
  set_tests_properties(vtkMkvSequenceIOTest PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING")

endif()


SET(SEQUENCEIO_TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Data)

#*************************** vtkMetaImageSequenceIOTest  ***************************
ADD_EXECUTABLE(vtkMetaImageSequenceIOTest vtkMetaImageSequenceIOTest.cxx )
SET_TARGET_PROPERTIES(vtkMetaImageSequenceIOTest PROPERTIES FOLDER Tests)
TARGET_LINK_LIBRARIES(vtkMetaImageSequenceIOTest vtkSequenceIO)
ADD_TEST(vtkMetaImageSequenceIOTest 
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkMetaImageSequenceIOTest
  --img-seq-file=${SEQUENCEIO_TEST_DATA_DIR}/MetaImageSequenceIOTest1.igs.mhd
  --output-img-seq-file=MetaImageSequenceIOTest1Output.igs.mha
  )
SET_TESTS_PROPERTIES(vtkMetaImageSequenceIOTest PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING")

IF (IGSIO_SEQUENCEIO_ENABLE_MKV)

  #*************************** vtkMkvSequenceIOTest ***************************
  ADD_EXECUTABLE(vtkMkvSequenceIOTest vtkMkvSequenceIOTest.cxx)
  SET_TARGET_PROPERTIES(vtkMkvSequenceIOTest PROPERTIES FOLDER Tests)
  TARGET_LINK_LIBRARIES(vtkMkvSequenceIOTest vtkSequenceIO)
  ADD_TEST(vtkMkvSequenceIOTest 
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkMkvSequenceIOTest
      --input-filename=${SEQUENCEIO_TEST_DATA_DIR}/uncompressedRV24.mkv
      --output-filename=mkvWriterTest1.mkv
    )
  SET_TESTS_PROPERTIES(vtkMkvSequenceIOTest PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING")

ENDIF()

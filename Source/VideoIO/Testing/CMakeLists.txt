
IF (VTKVIDEOIO_ENABLE_MKV)

SET(VTKVIDEOIO_TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Data)

#************************************************************************
#*************************** vtkMKVWriterTest ***************************
#************************************************************************
ADD_EXECUTABLE(vtkMKVWriterTest vtkMKVWriterTest.cxx)
SET_TARGET_PROPERTIES(vtkMKVWriterTest PROPERTIES FOLDER Tests)
TARGET_LINK_LIBRARIES(vtkMKVWriterTest vtkVideoIO)

#*************************** vtkMKVWriterTest1 ***************************
ADD_TEST(vtkMKVWriterTest1
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkMKVWriterTest
    --filename=mkvWriterTest1.mkv
    --width=10
    --height=10
    --numFrames=100
    --fps=10.0
  )
SET_TESTS_PROPERTIES(vtkMKVWriterTest1 PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING")

#************************************************************************
#*************************** vtkMKVReaderTest ***************************
#************************************************************************
ADD_EXECUTABLE(vtkMKVReaderTest vtkMKVReaderTest.cxx)
SET_TARGET_PROPERTIES(vtkMKVReaderTest PROPERTIES FOLDER Tests)
TARGET_LINK_LIBRARIES(vtkMKVReaderTest vtkVideoIO)

#*************************** vtkMKVReaderTest1 ***************************
ADD_TEST(vtkMKVReaderTest1
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vtkMKVReaderTest
    --filename=${VTKVIDEOIO_TEST_DATA_DIR}/uncompressedRV24.mkv
    --width=10
    --height=10
    --numFrames=100
  )
SET_TESTS_PROPERTIES(vtkMKVReaderTest1 PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING")

ENDIF()

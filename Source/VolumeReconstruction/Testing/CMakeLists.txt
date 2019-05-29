SET( TestDataDir ${CMAKE_CURRENT_SOURCE_DIR}/Data )
SET( TestConfigDir ${CMAKE_CURRENT_SOURCE_DIR}/Config )

ADD_EXECUTABLE(VolumeReconstructorTest VolumeReconstructorTest.cxx)
SET_TARGET_PROPERTIES(VolumeReconstructorTest PROPERTIES FOLDER Tools)
TARGET_LINK_LIBRARIES(VolumeReconstructorTest
  vtkIGSIOCommon
  vtkSequenceIO
  vtkVolumeReconstruction
  )
FILE(MAKE_DIRECTORY ${TEST_OUTPUT_PATH})

function(VolRecRegressionTest TestName ConfigFileNameFragment InputSeqFile OutNameFragment)
  ADD_TEST(VolumeReconstructorTestRun${TestName}
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/VolumeReconstructorTest
    --config-file=${TestConfigDir}/Config_VolumeReconstruction_${ConfigFileNameFragment}.xml
    --source-seq-file=${TestDataDir}/${InputSeqFile}.igs.mha
    --output-volume-file=${TEST_OUTPUT_PATH}/VolumeReconstructorTest${OutNameFragment}volume.mha
    --image-to-reference-transform=ImageToReference
    --importance-mask-file=${TestDataDir}/ImportanceMask.png
    --disable-compression
    )
  SET_TESTS_PROPERTIES( VolumeReconstructorTestRun${TestName} PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING" )

  # If expected filename is specified in a platform-specific directory then use that
  IF(WIN32)
    SET(PLATFORM "Windows")
  ELSE()
    SET(PLATFORM "Linux")
  ENDIF()
  SET(CommonExpectedVolumeFileName "${TestDataDir}/VolumeReconstructorTest${OutNameFragment}volumeRef.mha")
  SET(PlatformSpecificExpectedVolumeFileName "${TestDataDir}/${PLATFORM}/VolumeReconstructorTest${OutNameFragment}volumeRef.mha")
  if(EXISTS "${PlatformSpecificExpectedVolumeFileName}")
    SET(ExpectedVolumeFileName ${PlatformSpecificExpectedVolumeFileName})
  ELSE()
    SET(ExpectedVolumeFileName ${CommonExpectedVolumeFileName})
  endif()

  ADD_TEST(VolumeReconstructorTestCompare${TestName}
    ${CMAKE_COMMAND} -E compare_files
    ${ExpectedVolumeFileName}
    ${TEST_OUTPUT_PATH}/VolumeReconstructorTest${OutNameFragment}volume.mha
    )
  SET_TESTS_PROPERTIES(VolumeReconstructorTestCompare${TestName} PROPERTIES DEPENDS VolumeReconstructorTestRun${TestName})
endfunction()

VolRecRegressionTest(NearLateUChar SonixRP_TRUS_D70mm_NN_LATE SpinePhantomFreehand NNLATE)
VolRecRegressionTest(NearMeanUChar SpinePhantom_NN_MEAN SpinePhantomFreehand NNMEAN)
VolRecRegressionTest(NearMaxiFloat SpinePhantom_NN_MAXI SpinePhantomFreehand3FramesFloat NNMAXI)
VolRecRegressionTest(LinrLateFloat SpinePhantom_LN_LATE SpinePhantomFreehand3FramesFloat LNLATE)
VolRecRegressionTest(LinrMeanUChar SonixRP_TRUS_D70mm_LN_MEAN SpinePhantomFreehand LNMEAN)
VolRecRegressionTest(LinrMaxiUChar SpinePhantom_LN_MAXI SpinePhantomFreehand LNMAXI)

# Importance mask tests
VolRecRegressionTest(IMLinearFull ImportanceMaskLinearFull ImportanceMaskInput IMLiF)
VolRecRegressionTest(IMLinearPartial ImportanceMaskLinearPartial ImportanceMaskInput IMLiP)
VolRecRegressionTest(IMLinearNone ImportanceMaskLinearNone ImportanceMaskInput IMLiN)
VolRecRegressionTest(IMNearFull ImportanceMaskNNFull ImportanceMaskInput IMNNF)
VolRecRegressionTest(IMNearPartial ImportanceMaskNNPartial ImportanceMaskInput IMNNP)
VolRecRegressionTest(IMNearNone ImportanceMaskNNNone ImportanceMaskInput IMNNN)

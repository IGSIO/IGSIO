set( TestDataDir ${CMAKE_CURRENT_SOURCE_DIR}/Data )
set( TestConfigDir ${CMAKE_CURRENT_SOURCE_DIR}/Config )

add_executable(VolumeReconstructorTest VolumeReconstructorTest.cxx)
set_target_properties(VolumeReconstructorTest PROPERTIES FOLDER Tools)
target_link_libraries(VolumeReconstructorTest
  vtkIGSIOCommon
  vtkSequenceIO
  vtkVolumeReconstruction
  )
file(MAKE_DIRECTORY ${TEST_OUTPUT_PATH})

function(VolRecRegressionTest TestName ConfigFileNameFragment InputSeqFile OutNameFragment)
  add_test(VolumeReconstructorTestRun${TestName}
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/VolumeReconstructorTest
    --config-file=${TestConfigDir}/Config_VolumeReconstruction_${ConfigFileNameFragment}.xml
    --source-seq-file=${TestDataDir}/${InputSeqFile}.igs.mha
    --output-volume-file=${TEST_OUTPUT_PATH}/VolumeReconstructorTest${OutNameFragment}volume.mha
    --image-to-reference-transform=ImageToReference
    --importance-mask-file=${TestDataDir}/ImportanceMask.png
    --disable-compression
    )
  set_tests_properties( VolumeReconstructorTestRun${TestName} PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR;WARNING" )

  # If expected filename is specified in a platform-specific directory then use that
  if(WIN32)
    set(PLATFORM "Windows")
  else()
    set(PLATFORM "Linux")
  endif()
  set(CommonExpectedVolumeFileName "${TestDataDir}/VolumeReconstructorTest${OutNameFragment}volumeRef.mha")
  set(PlatformSpecificExpectedVolumeFileName "${TestDataDir}/${PLATFORM}/VolumeReconstructorTest${OutNameFragment}volumeRef.mha")
  if(EXISTS "${PlatformSpecificExpectedVolumeFileName}")
    set(ExpectedVolumeFileName ${PlatformSpecificExpectedVolumeFileName})
  else()
    set(ExpectedVolumeFileName ${CommonExpectedVolumeFileName})
  endif()

  add_test(VolumeReconstructorTestCompare${TestName}
    ${CMAKE_COMMAND} -E compare_files
    ${ExpectedVolumeFileName}
    ${TEST_OUTPUT_PATH}/VolumeReconstructorTest${OutNameFragment}volume.mha
    )
  set_tests_properties(VolumeReconstructorTestCompare${TestName} PROPERTIES DEPENDS VolumeReconstructorTestRun${TestName})
endfunction()

VolRecRegressionTest(NearLateUChar SonixRP_TRUS_D70mm_NN_LATE SpinePhantomFreehand NNLATE)
VolRecRegressionTest(NearMeanUChar SpinePhantom_NN_MEAN SpinePhantomFreehand NNMEAN)
VolRecRegressionTest(NearMaxiFloat SpinePhantom_NN_MAXI SpinePhantomFreehand3FramesFloat NNMAXI)
VolRecRegressionTest(LinrLateFloat SpinePhantom_LN_LATE SpinePhantomFreehand3FramesFloat LNLATE)
VolRecRegressionTest(LinrMeanUChar SonixRP_TRUS_D70mm_LN_MEAN SpinePhantomFreehand LNMEAN)
VolRecRegressionTest(LinrMaxiUChar SpinePhantom_LN_MAXI SpinePhantomFreehand LNMAXI)

if(IGSIO_USE_GPU)
  VolRecRegressionTest(NearMaxiFloatOpenCL SpinePhantom_NN_MAXI_OpenCL SpinePhantomFreehand3FramesFloat NNMAXIO)
  VolRecRegressionTest(LinrMaxiUCharOpenCL SpinePhantom_LN_MAXI_OpenCL SpinePhantomFreehand LNMAXIO)
endif()

# Importance mask tests
VolRecRegressionTest(IMLinearFull ImportanceMaskLinearFull ImportanceMaskInput IMLiF)
VolRecRegressionTest(IMLinearPartial ImportanceMaskLinearPartial ImportanceMaskInput IMLiP)
VolRecRegressionTest(IMLinearNone ImportanceMaskLinearNone ImportanceMaskInput IMLiN)
VolRecRegressionTest(IMNearFull ImportanceMaskNNFull ImportanceMaskInput IMNNF)
VolRecRegressionTest(IMNearPartial ImportanceMaskNNPartial ImportanceMaskInput IMNNP)
VolRecRegressionTest(IMNearNone ImportanceMaskNNNone ImportanceMaskInput IMNNN)

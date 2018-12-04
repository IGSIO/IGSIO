/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

//#include "PlusConfigure.h"
#include "vtkImageData.h"

/* //----------------------------------------------------------------------------
template<typename ScalarType> igsioStatus igsioVideoFrame::DeepCopyVtkVolumeToItkVolume( vtkImageData* inFrame, typename itk::Image< ScalarType, 3 >::Pointer outFrame )
{
  //LOG_TRACE("igsioVideoFrame::ConvertVtkImageToItkImage"); 

  if ( inFrame == NULL )
  {
    //**LOG_ERROR("Failed to convert vtk image to itk image - input image is NULL!"); 
    return IGSIO_FAIL; 
  }

  if ( outFrame.IsNull() )
  {
    //**LOG_ERROR("Failed to convert vtk image to itk image - output image is NULL!"); 
    return IGSIO_FAIL; 
  }

  if( igsioVideoFrame::GetNumberOfBytesPerScalar(inFrame->GetScalarType()) != sizeof(ScalarType) )
  {
    //**LOG_ERROR("Mismatch between input and output scalar types. In: " << igsioVideoFrame::GetStringFromVTKPixelType(inFrame->GetScalarType()));
    return IGSIO_FAIL;
  }

  // convert vtkImageData to itkImage 
  vtkSmartPointer<vtkImageExport> imageExport = vtkSmartPointer<vtkImageExport>::New(); 
  imageExport->SetInputData(inFrame); 
  imageExport->Update(); 

  int extent[6]={0,0,0,0,0,0}; 
  inFrame->GetExtent(extent); 

  double width = extent[1] - extent[0] + 1; 
  double height = extent[3] - extent[2] + 1;
  double thickness = extent[5] - extent[4] + 1;
  typename itk::Image< ScalarType, 3 >::SizeType size;
  size[0] = width;
  size[1] = height;
  size[2] = thickness;
  typename itk::Image< ScalarType, 3 >::IndexType start;
  start[0]=0;
  start[1]=0;
  start[2]=0;
  typename itk::Image< ScalarType, 3 >::RegionType region;
  region.SetSize(size);
  region.SetIndex(start);
  outFrame->SetRegions(region);
  try 
  {
    outFrame->Allocate();
  }
  catch(itk::ExceptionObject & err)
  {
    //**LOG_ERROR("Failed to allocate memory for the image conversion: " << err.GetDescription() ); 
    return IGSIO_FAIL; 
  }

  imageExport->Export( outFrame->GetBufferPointer() ); 

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
template<typename ScalarType> igsioStatus igsioVideoFrame::DeepCopyVtkVolumeToItkImage( vtkImageData* inFrame, typename itk::Image< ScalarType, 2 >::Pointer outFrame )
{
  //LOG_TRACE("igsioVideoFrame::ConvertVtkImageToItkImage"); 

  if ( inFrame == NULL )
  {
    //**LOG_ERROR("Failed to convert vtk image to itk image - input image is NULL!"); 
    return IGSIO_FAIL; 
  }

  if ( outFrame.IsNull() )
  {
    //**LOG_ERROR("Failed to convert vtk image to itk image - output image is NULL!"); 
    return IGSIO_FAIL; 
  }

  if( igsioVideoFrame::GetNumberOfBytesPerScalar(inFrame->GetScalarType()) != sizeof(ScalarType) )
  {
    //**LOG_ERROR("Mismatch between input and output scalar types. In: " << igsioVideoFrame::GetStringFromVTKPixelType(inFrame->GetScalarType()));
    return IGSIO_FAIL;
  }

  int extent[6]={0,0,0,0,0,0}; 
  inFrame->GetExtent(extent);
  
  if( extent[5]-extent[4] > 1 )
  {
	//**LOG_WARNING("3D volume sent in to igsioVideoFrame::DeepCopyVtkVolumeToItkImage. Only first slice will be copied.");
  }

  // convert vtkImageData to itkImage 
  vtkSmartPointer<vtkImageExport> imageExport = vtkSmartPointer<vtkImageExport>::New(); 
  imageExport->SetInputData(inFrame); 
  imageExport->Update(); 

  double width = extent[1] - extent[0] + 1; 
  double height = extent[3] - extent[2] + 1;

  typename itk::Image< ScalarType, 2 >::SizeType size;
  size[0] = width;
  size[1] = height;
  typename itk::Image< ScalarType, 2 >::IndexType start;
  start[0]=0;
  start[1]=0;
  typename itk::Image< ScalarType, 2 >::RegionType region;
  region.SetSize(size);
  region.SetIndex(start);
  outFrame->SetRegions(region);
  try 
  {
    outFrame->Allocate();
  }
  catch(itk::ExceptionObject & err)
  {
    //**LOG_ERROR("Failed to allocate memory for the image conversion: " << err.GetDescription() ); 
    return IGSIO_FAIL; 
  }

  imageExport->Export( outFrame->GetBufferPointer() ); 

  return IGSIO_SUCCESS;
} */
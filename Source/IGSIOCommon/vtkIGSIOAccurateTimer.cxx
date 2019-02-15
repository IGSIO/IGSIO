/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#include "igsioCommon.h"
#include "vtkIGSIOAccurateTimer.h"
#include "vtkObjectFactory.h"
#include "vtkTimerLog.h"
#include "vtksys/SystemTools.hxx"
#include <sstream>
#include <time.h>

#ifdef _WIN32
#include "WindowsAccurateTimer.h"
WindowsAccurateTimer WindowsAccurateTimer::m_Instance;
#endif
#include "igsioCommon.h"

double vtkIGSIOAccurateTimer::SystemStartTime = 0;
double vtkIGSIOAccurateTimer::UniversalStartTime = 0;

//----------------------------------------------------------------------------
// The singleton, and the singleton cleanup

vtkIGSIOAccurateTimer* vtkIGSIOAccurateTimer::Instance = NULL;
vtkIGSIOAccurateTimerCleanup vtkIGSIOAccurateTimer::Cleanup;

vtkIGSIOAccurateTimerCleanup::vtkIGSIOAccurateTimerCleanup() {}

vtkIGSIOAccurateTimerCleanup::~vtkIGSIOAccurateTimerCleanup()
{
  // Destroy any remaining output window.
  vtkIGSIOAccurateTimer::SetInstance(NULL);
}

//----------------------------------------------------------------------------
vtkIGSIOAccurateTimer* vtkIGSIOAccurateTimer::GetInstance()
{
  if (!vtkIGSIOAccurateTimer::Instance)
  {
    vtkIGSIOAccurateTimer::Instance = static_cast<vtkIGSIOAccurateTimer*>(
                                       vtkObjectFactory::CreateInstance("vtkIGSIOAccurateTimer"));
    if (!vtkIGSIOAccurateTimer::Instance)
    {
      vtkIGSIOAccurateTimer::Instance = new vtkIGSIOAccurateTimer;
#ifdef VTK_HAS_INITIALIZE_OBJECT_BASE
      vtkIGSIOAccurateTimer::Instance->InitializeObjectBase();
#endif
    }
    vtkIGSIOAccurateTimer::Instance->SystemStartTime = vtkIGSIOAccurateTimer::Instance->GetInternalSystemTime();
    vtkIGSIOAccurateTimer::Instance->UniversalStartTime = vtkTimerLog::GetUniversalTime();
    LOG_INFO("System start timestamp: " << std::fixed << std::setprecision(0) << vtkIGSIOAccurateTimer::Instance->SystemStartTime);
    LOG_DEBUG("AccurateTimer universal start time: " << GetDateAndTimeString(DTF_DATE_TIME_MSEC, vtkIGSIOAccurateTimer::UniversalStartTime));
  }
  return vtkIGSIOAccurateTimer::Instance;
}

//----------------------------------------------------------------------------
void vtkIGSIOAccurateTimer::SetInstance(vtkIGSIOAccurateTimer* instance)
{
  if (vtkIGSIOAccurateTimer::Instance == instance)
  {
    return;
  }

  if (vtkIGSIOAccurateTimer::Instance)
  {
    vtkIGSIOAccurateTimer::Instance->Delete();
  }

  vtkIGSIOAccurateTimer::Instance = instance;

  // User will call ->Delete() after setting instance

  if (instance)
  {
    instance->Register(NULL);
  }
}

//----------------------------------------------------------------------------
vtkIGSIOAccurateTimer* vtkIGSIOAccurateTimer::New()
{
  vtkIGSIOAccurateTimer* ret = vtkIGSIOAccurateTimer::GetInstance();
  ret->Register(NULL);
  return ret;
}

//----------------------------------------------------------------------------
vtkIGSIOAccurateTimer::vtkIGSIOAccurateTimer()
{

}

//----------------------------------------------------------------------------
vtkIGSIOAccurateTimer::~vtkIGSIOAccurateTimer()
{

}

//----------------------------------------------------------------------------
void vtkIGSIOAccurateTimer::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkIGSIOAccurateTimer::Delay(double sec)
{
#ifndef PLUS_USE_SIMPLE_TIMER
  // Use accurate timer
#ifdef _WIN32
  WindowsAccurateTimer* timer = WindowsAccurateTimer::Instance();
  timer->Wait(sec * 1000);
#else
  vtksys::SystemTools::Delay(sec * 1000);
#endif
#else // PLUS_USE_SIMPLE_TIMER
  // Use simple timer
  vtksys::SystemTools::Delay(sec * 1000);
  return;
#endif
}

void vtkIGSIOAccurateTimer::DelayWithEventProcessing(double waitTimeSec)
{
#ifdef _WIN32
  double waitStartTime = vtkIGSIOAccurateTimer::GetSystemTime();
  const double commandQueuePollIntervalSec = 0.010;
  do
  {
    // Need to process messages because some devices (such as the vtkIGSIOWin32VideoSource2) require event processing
    MSG Msg;
    while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&Msg);
      DispatchMessage(&Msg);
    }
    Sleep(commandQueuePollIntervalSec * 1000); // give a chance to other threads to get CPU time now
  }
  while (vtkIGSIOAccurateTimer::GetSystemTime() - waitStartTime < waitTimeSec);
#else
  usleep(waitTimeSec * 1000000);
#endif
}

//----------------------------------------------------------------------------
double vtkIGSIOAccurateTimer::GetInternalSystemTime()
{
#ifdef _WIN32
  return WindowsAccurateTimer::GetSystemTime();
#else
  return vtkTimerLog::GetUniversalTime();
#endif
}

//----------------------------------------------------------------------------
double vtkIGSIOAccurateTimer::GetSystemTime()
{
  return (vtkIGSIOAccurateTimer::GetInternalSystemTime() - vtkIGSIOAccurateTimer::SystemStartTime);
}

//----------------------------------------------------------------------------
double vtkIGSIOAccurateTimer::GetUniversalTime()
{
  return vtkIGSIOAccurateTimer::UniversalStartTime + (vtkIGSIOAccurateTimer::GetInternalSystemTime() - vtkIGSIOAccurateTimer::SystemStartTime);
}

//----------------------------------------------------------------------------
std::string vtkIGSIOAccurateTimer::GetDateAndTimeString(CurrentDateTimeFormat detailsNeeded, double currentTime)
{
  time_t timeSec = floor(currentTime);
  // Obtain the time of day, and convert it to a tm struct.
#ifdef _WIN32
  struct tm tmstruct;
  struct tm* ptm = &tmstruct;
  localtime_s(ptm, &timeSec);
#else
  struct tm* ptm = localtime(&timeSec);
#endif

  // Format the date and time, down to a single second.
  char timeStrSec[80];
  switch (detailsNeeded)
  {
    case DTF_DATE:
      strftime(timeStrSec, sizeof(timeStrSec), "%m%d%y", ptm);
      break;
    case DTF_TIME:
      strftime(timeStrSec, sizeof(timeStrSec), "%H%M%S", ptm);
      break;
    case DTF_DATE_TIME:
    case DTF_DATE_TIME_MSEC:
      strftime(timeStrSec, sizeof(timeStrSec), "%m%d%y_%H%M%S", ptm);
      break;
    default:
      return "";
  }
  if (detailsNeeded != DTF_DATE_TIME_MSEC)
  {
    return timeStrSec;
  }
  // Get millisecond as well
  long milliseconds = floor((currentTime - floor(currentTime)) * 1000.0);

  std::ostringstream mSecStream;
  mSecStream << timeStrSec << "." << std::setw(3) << std::setfill('0') << milliseconds;

  return mSecStream.str();
}

//----------------------------------------------------------------------------
std::string vtkIGSIOAccurateTimer::GetDateString()
{
  return GetDateAndTimeString(DTF_DATE, vtkIGSIOAccurateTimer::GetUniversalTime());
}

//----------------------------------------------------------------------------
std::string vtkIGSIOAccurateTimer::GetTimeString()
{
  return GetDateAndTimeString(DTF_TIME, vtkIGSIOAccurateTimer::GetUniversalTime());
}

//----------------------------------------------------------------------------
std::string vtkIGSIOAccurateTimer::GetDateAndTimeString()
{
  return GetDateAndTimeString(DTF_DATE_TIME, vtkIGSIOAccurateTimer::GetUniversalTime());
}

//----------------------------------------------------------------------------
std::string vtkIGSIOAccurateTimer::GetDateAndTimeMSecString()
{
  return GetDateAndTimeString(DTF_DATE_TIME_MSEC, vtkIGSIOAccurateTimer::GetUniversalTime());
}

//----------------------------------------------------------------------------
double vtkIGSIOAccurateTimer::GetUniversalTimeFromSystemTime(double systemTime)
{
  return vtkIGSIOAccurateTimer::UniversalStartTime + systemTime;
}

//----------------------------------------------------------------------------
double vtkIGSIOAccurateTimer::GetSystemTimeFromUniversalTime(double utcTime)
{
  return utcTime - vtkIGSIOAccurateTimer::UniversalStartTime;
}

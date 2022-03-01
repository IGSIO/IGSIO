/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

/*=========================================================================
  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.
=========================================================================*/

//#include "PlusConfigure.h"

#include "vtkIGSIORecursiveCriticalSection.h"
#include "vtkObjectFactory.h"

vtkStandardNewMacro(vtkIGSIORecursiveCriticalSection);

// New for the SimpleRecursiveCriticalSection
vtkIGSIOSimpleRecursiveCriticalSection* vtkIGSIOSimpleRecursiveCriticalSection::New()
{
  return new vtkIGSIOSimpleRecursiveCriticalSection;
}

void vtkIGSIOSimpleRecursiveCriticalSection::Init()
{
#ifdef VTK_USE_SPROC
  init_lock(&this->CritSec);
#endif

#ifdef VTK_USE_WIN32_THREADS
  InitializeCriticalSection(&this->CritSec);
#endif

#ifdef VTK_USE_PTHREADS
  pthread_mutexattr_t recursiveAttr;
  pthread_mutexattr_init(&recursiveAttr);
  pthread_mutexattr_settype(&recursiveAttr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&(this->CritSec), &recursiveAttr);
  pthread_mutexattr_destroy(&recursiveAttr);
#endif
}


// Destruct the vtkMutexVariable
vtkIGSIOSimpleRecursiveCriticalSection::~vtkIGSIOSimpleRecursiveCriticalSection()
{
#ifdef VTK_USE_WIN32_THREADS
  DeleteCriticalSection(&this->CritSec);
#endif

#ifdef VTK_USE_PTHREADS
  pthread_mutex_destroy(&this->CritSec);
#endif
}

// Lock the vtkIGSIORecursiveCriticalSection
void vtkIGSIOSimpleRecursiveCriticalSection::Lock()
{
#ifdef VTK_USE_SPROC
  spin_lock(&this->CritSec);
#endif

#ifdef VTK_USE_WIN32_THREADS
  //WaitForSingleObject( this->MutexLock, INFINITE );
  EnterCriticalSection(&this->CritSec);
#endif

#ifdef VTK_USE_PTHREADS
  pthread_mutex_lock(&this->CritSec);
#endif
}

// Unlock the vtkIGSIORecursiveCriticalSection
void vtkIGSIOSimpleRecursiveCriticalSection::Unlock()
{
#ifdef VTK_USE_SPROC
  release_lock(&this->CritSec);
#endif

#ifdef VTK_USE_WIN32_THREADS
  LeaveCriticalSection(&this->CritSec);
#endif

#ifdef VTK_USE_PTHREADS
  pthread_mutex_unlock(&this->CritSec);
#endif
}

void vtkIGSIORecursiveCriticalSection::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}


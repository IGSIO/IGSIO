/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkIGSIORecursiveCriticalSection_h
#define __vtkIGSIORecursiveCriticalSection_h

#include "vtkigsiocommon_export.h"

// Get vtkCritSecType definition
#include "vtkCriticalSection.h"

/*!
  \class vtkIGSIOSimpleRecursiveCriticalSection
  \brief Allows the recursive locking of variables which are accessed through different threads. Not a VTK object.

  \sa vtkIGSIORecursiveCriticalSection
  \ingroup PlusLibCommon
*/

// Critical Section object that is not a vtkObject.
class VTKIGSIOCOMMON_EXPORT vtkIGSIOSimpleRecursiveCriticalSection
{
public:
  // Default cstor
  vtkIGSIOSimpleRecursiveCriticalSection()
  {
    this->Init();
  }
  // Construct object locked if isLocked is different from 0
  vtkIGSIOSimpleRecursiveCriticalSection(int isLocked)
  {
    this->Init();
    if (isLocked)
    {
      this->Lock();
    }
  }
  // Destructor
  virtual ~vtkIGSIOSimpleRecursiveCriticalSection();

  // Default vtkObject API
  static vtkIGSIOSimpleRecursiveCriticalSection* New();
  void Delete()
  {
    delete this;
  }

  void Init();

  // Description:
  // Lock the vtkIGSIORecursiveCriticalSection
  void Lock();

  // Description:
  // Unlock the vtkIGSIORecursiveCriticalSection
  void Unlock();

protected:
  vtkCritSecType   CritSec;
};


/*!
  \class vtkIGSIORecursiveCriticalSection
  \brief vtkIGSIORecursiveCriticalSection allows the recursive locking of variables which are accessed through different threads

  The class provides recursive locking (the same thread is allowed to lock multiple times without being blocked) consistently
  on Windows and non-Windows OS. vtkIGSIORecursiveCriticalSection is recursive on Windows and non-recursive on other operating systems.
  See a related bug report at: http://www.gccxml.org/Bug/view.php?id=9461

  \sa vtkIGSIOSimpleRecursiveCriticalSection
  \ingroup PlusLibCommon
*/
class VTKIGSIOCOMMON_EXPORT vtkIGSIORecursiveCriticalSection : public vtkObject
{
public:
  static vtkIGSIORecursiveCriticalSection* New();
  vtkTypeMacro(vtkIGSIORecursiveCriticalSection, vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  // Description:
  // Lock the vtkIGSIORecursiveCriticalSection
  void Lock();

  // Description:
  // Unlock the vtkIGSIORecursiveCriticalSection
  void Unlock();

protected:
  vtkIGSIOSimpleRecursiveCriticalSection SimpleRecursiveCriticalSection;
  vtkIGSIORecursiveCriticalSection() {}
  ~vtkIGSIORecursiveCriticalSection() {}

private:
  vtkIGSIORecursiveCriticalSection(const vtkIGSIORecursiveCriticalSection&);  // Not implemented.
  void operator=(const vtkIGSIORecursiveCriticalSection&);  // Not implemented.
};


inline void vtkIGSIORecursiveCriticalSection::Lock()
{
  this->SimpleRecursiveCriticalSection.Lock();
}

inline void vtkIGSIORecursiveCriticalSection::Unlock()
{
  this->SimpleRecursiveCriticalSection.Unlock();
}

#endif

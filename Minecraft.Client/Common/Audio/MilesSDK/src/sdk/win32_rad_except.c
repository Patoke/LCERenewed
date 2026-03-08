#include "rad_except.h"

// @cdep pre $requires( $clipfilename($file)/win32_rad_except_safeseh.asm )

EXCEPTION_DISPOSITION __cdecl rad_handler(
    struct _EXCEPTION_RECORD *ExceptionRecord,
    void * EstablisherFrame,
    struct _CONTEXT *ContextRecord,
    void * DispatcherContext )
{
  EXCEPT_REG * er = (EXCEPT_REG*)EstablisherFrame;

  ContextRecord->Ebx = er->_ebx;
  ContextRecord->Ecx = er->_ecx;
  ContextRecord->Esi = er->_esi;
  ContextRecord->Edi = er->_edi;
  ContextRecord->Ebp = er->_ebp;
  ContextRecord->Esp = er->_esp;
  ContextRecord->Edx = er->_esp; // on resume, edx goes into esp
  ContextRecord->Eip = er->_eip;

  er->had_error = ExceptionRecord->ExceptionCode;
  return ExceptionContinueExecution;  
}


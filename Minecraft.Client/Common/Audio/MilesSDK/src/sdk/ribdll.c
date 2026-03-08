#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL __stdcall DllMain( HINSTANCE hinstDll, DWORD fdwRreason, LPVOID plvReserved)
{
  return( 1 );
}

/* @cdep pre
   $define( STDLIBs, )
   $requires( radlibc.c )
*/

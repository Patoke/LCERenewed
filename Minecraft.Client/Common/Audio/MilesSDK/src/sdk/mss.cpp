//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  MSS.C: C API module and support routines                              ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#include "mss.h"
#include "imssapi.h"

//############################################################################
//##                                                                        ##
//## Return HWND used by MSS                                                ##
//##                                                                        ##
//############################################################################

#ifdef IS_WIN32

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#define STRICT
#include <windows.h>

extern "C" 
{
  HWND MSShWnd = NULL;
  HINSTANCE MSS_Instance;
  char MSS_Directory[260];
}

static HWND GetHwnd(void)
{
   DWORD curproc = GetCurrentProcessId();
   SINTa curmod  = (SINTa) GetModuleHandle(NULL);
   DWORD proc;
   HWND  scan;

   if ((MSShWnd==NULL) || (!IsWindow(MSShWnd))) {

     MSShWnd=NULL;

     scan = GetTopWindow(0);

     while (scan)
        {
        GetWindowThreadProcessId(scan, &proc);

        if (proc == curproc) {

           // make sure the module matches and the window isn't a child

#ifdef IS_WIN64
           if (GetWindowLongPtr(scan, GWLP_HINSTANCE) == curmod) {
#else
           if (GetWindowLong(scan, GWL_HINSTANCE) == curmod) {
#endif
             if ((GetWindowLong(scan,GWL_STYLE)&WS_CHILD)==0)
               MSShWnd=scan;
           }

           // however, if we haven't found anything, then save the window
           if ((MSShWnd==0) && (!(GetWindowLong(scan,GWL_STYLE) & WS_DISABLED)))
             MSShWnd=scan;

        }

        scan = GetWindow(scan, GW_HWNDNEXT);
        }

     //
     // If no window found, use active window or foreground window
     //

     if (MSShWnd==0)
       MSShWnd=GetActiveWindow();

     if (MSShWnd==0)
       MSShWnd=GetForegroundWindow();
   }

   return MSShWnd;
}


S32 check_dirs(HINSTANCE hInstance)
{
  int i;
  char buf[272];

  // check to make sure the DLL isn't incorrectly installed

  GetModuleFileName(hInstance,MSS_Directory,271);

  for(i=(int)AIL_strlen(MSS_Directory);i;i--) {
    if ((MSS_Directory[i]=='\\') || (MSS_Directory[i]==':'))
      break;
  }
  MSS_Directory[i]=0;

  GetWindowsDirectory(buf,271);
  if (AIL_strnicmp(MSS_Directory,buf,271)==0) {
   err:
    MessageBox(0,"The MSS DLL file is installed in the Windows or Windows system directory - it must be installed in the application directory.","Miles Error",MB_OK|MB_ICONSTOP);
    return(0);
  }
  GetSystemDirectory(buf,271);
  if (AIL_strnicmp(MSS_Directory,buf,271)==0) {
    goto err;
  }

  return(1);
}

#endif

//############################################################################
//#                                                                          #
//# Platform-specific property access                                        #
//#                                                                          #
//############################################################################

DXDEF S32 AILCALL AIL_platform_property (void                 *object, //)
                                         MSS_PLATFORM_PROPERTY property,
                                         void                 *before_value,
                                         void const           *new_value,
                                         void                 *after_value)
{
   HDIGDRIVER dig = (HDIGDRIVER) object;

   switch (property)
      {
      default:
         return 0;

#ifdef IS_XENON

      case XB360_LPXAB:       
         {
         if ( object == 0 ) return 0;
         if (before_value) *(void * *) before_value = dig->x2_voiceptr;
         return 1;
         }
#else
      case WIN32_HWAVEOUT:
         {
         if ( object == 0 ) return 0;
         if (before_value) *(HWAVEOUT *) before_value = dig->hWaveOut;
         return 1;
         }

      case WIN32_HWAVEIN:
         {
         HDIGINPUT digin = (HDIGINPUT) object;
         if ( object == 0 ) return 0;
         if (before_value) *(HWAVEIN *) before_value = digin->hWaveIn;
         return 1;
         }

      case WIN32_POSITION_ERR:
         {
         if (object == 0) return 0;
         if (before_value) *(U32 *) before_value = dig->position_error;
         return 1;
         }

      case WIN32_LPDS:  
         {
         if (before_value)
            {
            if ( object == 0 ) return 0;
            *(AILLPDIRECTSOUND *) before_value = dig->pDS;
            }
         return 1;
         }

      case WIN32_LPDSB: 
         {
         if (before_value)
            {
            if ( object == 0 ) return 0;
            *(AILLPDIRECTSOUNDBUFFER *) before_value = dig->lppdsb;
            }
         return 1;
         }

      case WIN32_HWND:  
          {
              if (before_value)
              {
                  *(HWND *) before_value = GetHwnd();
              }

              if (new_value && object != 0)
              {
                  dig->dsHwnd = (UINTa)(*(HWND*)new_value);
           
                  S32 UpdateDSHwnd(HDIGDRIVER);
                  UpdateDSHwnd(dig);

                  MSShWnd=(HWND)dig->dsHwnd;
              }

              if (after_value)
              {
                  *(HWND *) after_value = GetHwnd();
              }

              return 1;
          }
#endif
      }

   // can't get here
   // return 0;
}


//############################################################################
//##                                                                        ##
//## DLL routines                                                           ##
//##                                                                        ##
//############################################################################

#ifdef IS_WIN32

extern "C"
#ifdef MSSXTRA
DXDEC BOOL WINAPI MSSDllMain(
#else
BOOL WINAPI DllMain(
#endif
                          HINSTANCE hinstDll,
                          DWORD fdwRreason,
                          LPVOID plvReserved)
{
  MSS_Instance=hinstDll;
  switch (fdwRreason) {
    case DLL_PROCESS_ATTACH:
      if (check_dirs(hinstDll)==0)
        return(0);
      return(1);
  }
  return TRUE;
}

#endif



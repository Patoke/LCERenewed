//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  MSSDBG.C: Runtime debugging facilities for MSS API calls              ##
//##                                                                        ##
//##  Author: John Miles & Jeff Roberts                                     ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#if defined(_SEKRIT)

#elif defined(_XENON) || ( defined(_XBOX_VER) && (_XBOX_VER == 200) ) || defined(_XBOX)
  #define NOD3D
  #include <xtl.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(__NT__) || defined(__WIN32__)

  #define WIN32_LEAN_AND_MEAN
  #define WIN32_EXTRA_LEAN
  #define STRICT

  #include <windows.h>
  #include <mmsystem.h>
  #include <ks.h>
  #pragma warning( disable : 4214)
  #include <ksmedia.h>
#endif

#include "mss.h"
#include "imssapi.h"
#include "rrAtomics.h"

#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>

#ifdef IS_LINUX
#include <string.h>
#endif

#ifdef IS_WII
#include <revolution/dvd.h>
#endif

//############################################################################
//##                                                                        ##
//## Locked static data                                                     ##
//##                                                                        ##
//############################################################################

#if defined(IS_WIN32)
  U32 __declspec(dllexport) AIL_debug=0;
  U32 __declspec(dllexport) AIL_sys_debug=0;
  U32 __declspec(dllexport) AIL_indent=0;
#elif defined(IS_MAC) || defined(IS_LINUX) || defined(IS_IPHONE)
  U32 __attribute__((visibility("default"))) AIL_debug=0;
  U32 __attribute__((visibility("default"))) AIL_sys_debug=0;
  U32 __attribute__((visibility("default"))) AIL_indent=0;
#else
  U32 AIL_debug=0;
  U32 AIL_sys_debug=0;
  U32 AIL_indent=0;
#endif

static int AIL_didaninit = 0;
static U32 AIL_starttime = 0;
static AILTRACECB trace_callback = 0;
static FILE *AIL_debugfile = 0;

#define IN_AIL  InMilesMutex()
#define OUT_AIL OutMilesMutex()
#define IN_AIL_NM
#define OUT_AIL_NM


//############################################################################
//##                                                                        ##
//## Debug trace macros and helper functions                                ##
//##                                                                        ##
//############################################################################

#define START  if (AIL_indent++, (AIL_debug && \
                  ((AIL_indent==1) || AIL_sys_debug) && \
                  (!AIL_API_background())))

#define END    AIL_indent--

#define RESULT if (AIL_debug && ((AIL_indent==1) || AIL_sys_debug) && (!AIL_API_background()))

static char indentdot[]="                                ";

DXDEF
void AILEXPORT AIL_debug_log (char const * ifmt, ...)
{
   va_list ap;

   va_start(ap,
            ifmt);

   if ( AIL_debug )
   {
      char textbuf[ 512 ];
      char * text = textbuf + 15;
      char * t = text;
      char const * fmt = ifmt;

      if ( ( fmt[0] == '!' ) || ( fmt[0] == '@' ) )
      { 
        if ( AIL_indent > 1 )
        {
          AIL_strcpy( text, indentdot+33-(AIL_indent*2) );
          t = text + (AIL_indent * 2) - 1;
        }
        ++fmt;
      }

      #if !defined(IS_WIN32API)
      vsnprintf(t,
      #else
      _vsnprintf(t,
      #endif
                 (sizeof(textbuf)-1)-(t-textbuf),
                 fmt,
                 ap);
      textbuf[sizeof(textbuf)-1]=0;

#ifndef NTELEMETRY      
      if ( miles_context )
      {
        tmMessage( miles_context, TMMF_SEVERITY_LOG, "%s", tmDynamicString( miles_context, text ) );
      }
#endif

      if ( ( AIL_debugfile ) || ( trace_callback ) || ( miles_context == 0 ) )
      {
        char buf[ 16 ];
        U32 tm;
        U32 h,m,s,c;

        tm = AIL_ms_count()-AIL_starttime;

        h = tm / 3600000;
        if ( h > 99 ) h = 99;
        m = ( tm / 60000 ) % 60;
        s = ( tm /1000 ) % 60;
        c = tm % 1000;

        if ( ( AIL_indent == 1 ) && ( ifmt[0] == '@' ) )
        {
          sprintf( buf, "[%.02d:%.02d:%.02d.%.03d] ",h,m,s,c);
          AIL_memcpy( textbuf, buf, 15 );
          text = textbuf;
        }
        else if ( ( ifmt[0] == '@' ) || ( ifmt[0] == '!' ) )
        {
          AIL_memset( textbuf, 32, 15 );
          text = textbuf;
        }
        
        if ( AIL_debugfile )
        {
          fprintf(AIL_debugfile,
                  "%s\n",
                  text );
        }
        else if ( trace_callback )
        {
          trace_callback( text, AIL_indent );
        }
        else 
        {
          #ifdef IS_WII
            OSReport( "%s\n", text );
          #elif defined( IS_WINDOWS ) || defined( IS_XENON )
            AIL_strcat(t,"\n");
            OutputDebugString( text );
          #elif defined( IS_PS3 ) || defined( IS_LINUX ) || defined( IS_MAC ) || defined( IS_PSP ) || defined(IS_PSP2)
            printf( "%s\n", text );
          #endif
        }
      }
   }

   va_end(ap);
}


static void outresfloat(F32 val)
{
  AIL_debug_log("!-Result = %f" , val);
}

static void outresint(U32 val)
{
  AIL_debug_log("!-Result = %d" ,val);
}

static void outresint64(U64 val)
{
    AIL_debug_log("!-Result = %llu", val);
}

static void outresstr(const C8 *val)
{
  if (val == NULL)
      AIL_debug_log("!-Result = NULL (no error)" );
   else
      AIL_debug_log("!-Result = %s" ,val);
}

static void outreshex(U32 val)
{
  AIL_debug_log("!-Result = 0x%X" ,val);
}

static void outresuint(UINTa val)
{
  AIL_debug_log("!-Result = " SINTFMT ,val);
}

static void outresuhex(UINTa val)
{
  AIL_debug_log("!-Result = " PTRFMT ,val);
}


//############################################################################
DXDEC  AILTRACECB AILCALL AIL_configure_logging ( char const * filename,
                                                  AILTRACECB cb,
                                                  S32        level)
{
   AILTRACECB result;

   if ( AIL_starttime == 0 )
     AIL_starttime=AIL_ms_count();

   if ( AIL_debugfile )
   {
     fclose( AIL_debugfile );
     AIL_debugfile = 0;
   }

   if ( filename )
   {
     AIL_debugfile = fopen(filename,"at");       // Debug file is used as a journaling target
   }

   result         = trace_callback;
   trace_callback = cb;

   switch (level)
   {
      case 0:
         AIL_debug      = FALSE;
         AIL_sys_debug  = FALSE;
         break;

      case 1:
         AIL_debug      = TRUE;
         AIL_sys_debug  = FALSE;
         break;

      case 2:
         AIL_debug      = TRUE;
         AIL_sys_debug  = TRUE;
         break;
   }

   if ( level )
   {
     AIL_debug_log("-------------------------------------------------------------------------------");
     AIL_debug_log("Miles Sound System usage script generated by MSS V"MSS_VERSION"" );

#ifndef __RAD3DS__
     {
       time_t elapstime;
       struct tm *adjtime;
       char loctime[32];
       time(&elapstime);
       adjtime = localtime(&elapstime);
       AIL_strcpy(loctime,asctime(adjtime));
       loctime[24] = 0;
       AIL_debug_log("Start time: %s", loctime);
     }
#endif

     AIL_debug_log("-------------------------------------------------------------------------------"   );
   }
               
   return result;
}

//############################################################################
DXDEF S32    AILEXPORT AIL_startup                   (void)
{
   S32 res = 0;
   START
      {
      AIL_debug_log("@AIL_startup()" );
      }

   if (AIL_didaninit == 0)
   {
     AIL_didaninit=1;

     //
     // Initialize API
     //

     AIL_API_startup();
     res = 1;
   }

   RESULT
      {
      outresint( res );
      }

   END;

   return res;
}


//############################################################################

DXDEF
void    AILEXPORT  AIL_shutdown                  (void)
{
   START
      {
      AIL_debug_log("@AIL_shutdown()" );
      }

   IN_AIL;

   if (AIL_didaninit )
   {
     AIL_didaninit = 0;
     AIL_API_shutdown();
   }

   AIL_configure_logging( 0, 0, 0 );

   OUT_AIL;

   END;
}

//############################################################################


//############################################################################
//##                                                                        ##
//## AIL_debug_printf()                                                     ##
//##                                                                        ##
//############################################################################

#ifdef _DEBUG

DXDEF void AILEXPORT AIL_debug_printf( char const *fmt, ...)
{
   char work_string[768];

   va_list ap;

   va_start(ap,
            fmt);

   vsnprintf(work_string,766,
             fmt,
             ap);

   va_end  (ap);
   work_string[765]=0;

   #if defined(IS_WII)
     OSReport( "%s\n", work_string );
   #elif defined( IS_WINDOWS ) || defined( IS_XENON )
     AIL_strcat(work_string,"\n");
     OutputDebugString( work_string );
   #elif defined( IS_PS3 ) || defined( IS_LINUX ) || defined( IS_MAC ) || defined( IS_PSP ) || defined(IS_PSP2) || defined(FORNONWIN)
     printf( "%s\n", work_string );
   #endif
}

#endif


//############################################################################

DXDEF
void * AILEXPORT AIL_mem_alloc_lock_info( UINTa size, char const * file, U32 line )
{
   void *result;
   START
      {
      AIL_debug_log("@AIL_mem_alloc_lock(" SINTFMT ") from: %s, line: %d " ,size,file,line);
      }

   IN_AIL_NM;

   result=AIL_API_mem_alloc_lock(size);

   OUT_AIL_NM;

   tmAllocEx( miles_context, file, line, result, size, "Miles alloc: %t", tmSendCallStack( miles_context,0 ) );

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void AILEXPORT AIL_mem_free_lock(void *ptr)
{
   START
      {
      AIL_debug_log("@AIL_mem_free_lock(" PTRFMT ")" ,ptr);
      }

   IN_AIL_NM;

   tmFree( miles_context, ptr );

   AIL_API_mem_free_lock(ptr);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
SINTa    AILEXPORT  AIL_set_preference            (U32       number, //)
                                                   SINTa     value)
{
   SINTa result;

   START
      {
      AIL_debug_log("@AIL_set_preference(%ld," SINTFMT ")" ,number,value);
      }

   IN_AIL;

   result=AIL_API_set_preference(number,value);

   OUT_AIL;

   RESULT
      {
      outresuint((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
char* AILEXPORT AIL_last_error               (void)
{
   char *result;

   START
      {
      AIL_debug_log("@AIL_last_error()" );
      }

   IN_AIL_NM;

   result=AIL_API_last_error();

   OUT_AIL_NM;

   RESULT
      {
      outresstr(result);
      }

   END;

   return (char*) result;
}

//############################################################################

DXDEF
void AILEXPORT  AIL_set_error               (char const * error_msg)
{
   START
      {
      if (error_msg == NULL)
         {
         AIL_debug_log("@AIL_set_error(NULL)" );
         }
      else
         {
         AIL_debug_log("@AIL_set_error(%s)" , error_msg);
         }
      }

   IN_AIL_NM;

   AIL_API_set_error(error_msg);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void    AILEXPORT  AIL_sleep(U32 ms)
{
   START
      {
      AIL_debug_log("@AIL_sleep(%ld)" ,ms);
      }

   IN_AIL_NM;

   AIL_API_sleep(ms);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
S32    AILEXPORT  AIL_background                (void)
{
   S32 result;

   IN_AIL_NM;

   result=AIL_API_background();

   OUT_AIL_NM;

   return result;
}

//############################################################################

DXDEF
HTIMER  AILEXPORT  AIL_register_timer            (AILTIMERCB    callback_fn)
{
   HTIMER result;

   START
      {
      AIL_debug_log("@AIL_register_timer(" PTRFMT ")" ,callback_fn);
      }

   IN_AIL_NM;

   result=AIL_API_register_timer(callback_fn);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
UINTa AILEXPORT  AIL_set_timer_user            (HTIMER      timer, //)
                                                UINTa       user)
{
   UINTa result;

   START
      {
      AIL_debug_log("@AIL_set_timer_user(%ld," SINTFMT ")" ,timer,user);
      }

   IN_AIL;

   result=AIL_API_set_timer_user(timer,user);

   OUT_AIL;

   RESULT
      {
      outresuint((UINTa) result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void    AILEXPORT  AIL_set_timer_period          (HTIMER      timer, //)
                                               U32         microseconds)
{
   START
      {
      AIL_debug_log("@AIL_set_timer_period(%ld,%lu)" ,timer,microseconds);
      }

   IN_AIL;

   AIL_API_set_timer_period(timer,microseconds);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void    AILEXPORT  AIL_set_timer_frequency       (HTIMER      timer, //)
                                               U32         hertz)
{
   START
      {
      AIL_debug_log("@AIL_set_timer_frequency(%ld,%lu)" ,timer,hertz);
      }

   IN_AIL;

   AIL_API_set_timer_frequency(timer,hertz);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void    AILEXPORT  AIL_start_timer               (HTIMER      timer)
{
   START
      {
      AIL_debug_log("@AIL_start_timer(%lu)" ,timer);
      }

   IN_AIL;

   AIL_API_start_timer(timer);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void    AILEXPORT  AIL_start_all_timers          (void)
{
   START
      {
      AIL_debug_log("@AIL_start_all_timers()" );
      }

   IN_AIL;

   AIL_API_start_all_timers();

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void    AILEXPORT  AIL_stop_timer                (HTIMER      timer)
{
   START
      {
      AIL_debug_log("@AIL_stop_timer(%lu)" ,timer);
      }

   IN_AIL;

   AIL_API_stop_timer(timer);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void    AILEXPORT  AIL_stop_all_timers           (void)
{
   START
      {
      AIL_debug_log("@AIL_stop_all_timers()" );
      }

   IN_AIL;

   AIL_API_stop_all_timers();

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void    AILEXPORT  AIL_release_timer_handle      (HTIMER      timer)
{
   START
      {
      AIL_debug_log("@AIL_release_timer_handle(%lu)" ,timer);
      }

   IN_AIL;

   AIL_API_release_timer_handle(timer);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void    AILEXPORT  AIL_release_all_timers        (void)
{
   START
      {
      AIL_debug_log("@AIL_release_all_timers()" );
      }

   IN_AIL;

   AIL_API_release_all_timers();

   OUT_AIL;

   END;
}


//############################################################################
#if defined(IS_WIN32) || defined(IS_PS2) || defined(IS_MAC) || defined(IS_WII) || defined(IS_WIIU)

//
// Open a matrix- or output-filter provider
//

S32 MC_open_output_filter(C8 const  *name, //)
                          HDIGDRIVER driver,
                          S32        is_matrix_filter)
{
   HPROVIDER HP;

   if ((is_matrix_filter) && (!(driver->hw_format & DIG_F_16BITS_MASK)))
      {
      AIL_set_error("Matrix filters require 16-bit PCM output format");
      return 0;
      }

   if (!AIL_find_filter(name,
                       &HP))
      {
      AIL_set_error("Required output filter not found");
      return 0;
      }

   if (!AIL_open_filter(HP,
                        driver))
      {
      return 0;
      }

   return 1;
}

#endif


//############################################################################
#ifdef IS_GENERICDIG
DXDEF HDIGDRIVER AILEXPORT AIL_open_generic_digital_driver( U32 frequency,
                                                    S32 bits,
                                                    S32 channels,
                                                    U32 flags,
                                                    RADSS_OPEN_FUNC dig_open)
#else
DXDEF HDIGDRIVER AILEXPORT AIL_open_digital_driver( U32 frequency,
                                                    S32 bits,
                                                    S32 channels,
                                                    U32 flags )
#endif
{
   HDIGDRIVER result;

#if defined(IS_XENON)
   channels = MSS_MC_51_DISCRETE;   // TODO: headphone support on Xenon
#endif

   START
      {
      AIL_debug_log("@AIL_open_digital_driver(%ld, %ld, %ld, 0x%lX)" ,
                                                             frequency,
                                                             bits,
                                                             channels,
                                                             flags);
      }

   IN_AIL;

   // Mac, PS3, Xbox, Xenon, Linux, Wii, PS2, Windows.
#ifdef IS_GENERICDIG
   result = AIL_API_open_generic_digital_driver( frequency, bits, channels, flags, dig_open );
#else
   result = AIL_API_open_digital_driver( frequency, bits, channels, flags );
#endif

#ifdef IS_WII
   if ( result )
   {
     if ( result->logical_channels_per_sample == 3 )
     {
       if ( ! MC_open_output_filter("Dolby Surround Matrix Filter", result, TRUE) )
       {
         AIL_API_close_digital_driver(result);
         result = 0;
         // if wii hardware is in surround, but the programmer forgot to load the dolby flt, try stereo
         if ( channels == MSS_MC_USE_SYSTEM_CONFIG )
           result = AIL_API_open_digital_driver( frequency, bits, 2, flags );
       }
     }
     else if ( result->logical_channels_per_sample == 7 )
     {
       if ( ! MC_open_output_filter("SRS Circle Surround 6.1 Matrix Filter", result, TRUE) )
       {
         AIL_API_close_digital_driver(result);
         result = 0;
       }
     }
   }
#endif

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

#if !defined(IS_WII) && !defined(IS_XENON) && !defined(IS_PS3) && !defined(IS_WIN32)  // These platforms modify channel_spec themselves

   if (result)
      {
      result->channel_spec = (MSS_MC_SPEC)channels;
      }

#endif

   return result;
}

DXDEF void AILEXPORT AIL_close_digital_driver( HDIGDRIVER dig )
{
   START
      {
      AIL_debug_log("@AIL_close_digital_driver(" PTRFMT ")" ,dig);
      }

   IN_AIL_NM;

   AIL_API_close_digital_driver(dig);

   OUT_AIL_NM;

   END;
}

#if defined(IS_WINDOWS)

//############################################################################

DXDEF
S32         AILEXPORT AIL_digital_handle_release      (HDIGDRIVER dig)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_digital_handle_release(" PTRFMT ")" ,dig);
      }

   IN_AIL;

   result=AIL_API_digital_handle_release(dig);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_digital_handle_reacquire    (HDIGDRIVER dig)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_digital_handle_reacquire(" PTRFMT ")" ,dig);
      }

   IN_AIL;

   result=AIL_API_digital_handle_reacquire(dig);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

#endif

#if defined(IS_WINDOWS) || (defined(IS_MAC) && !defined(IS_IPHONE))

//############################################################################
DXDEF HDIGINPUT AILEXPORT AIL_open_input          (AIL_INPUT_INFO *info)
{
   HDIGINPUT result;

   START
      {
      AIL_debug_log("@AIL_open_input(" PTRFMT ")" ,info);
      }

   IN_AIL;

   result=AIL_API_open_input(info);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF void      AILEXPORT AIL_close_input         (HDIGINPUT         dig)
{
   START
      {
      AIL_debug_log("@AIL_close_input(" PTRFMT ")" ,dig);
      }

   IN_AIL;

   AIL_API_close_input(dig);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF AIL_INPUT_INFO * AILEXPORT AIL_get_input_info (HDIGINPUT         dig)
{
   AIL_INPUT_INFO *result;

   START
      {
      AIL_debug_log("@AIL_get_input_info(" PTRFMT ")" ,dig);
      }

   IN_AIL;

   result=AIL_API_get_input_info(dig);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF S32       AILEXPORT AIL_set_input_state     (HDIGINPUT         dig, //)
                                                   S32               enable)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_set_input_state(" PTRFMT ",%ld)" ,dig,enable);
      }

   IN_AIL;

   result=AIL_API_set_input_state(dig,enable);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

#endif

//############################################################################

DXDEF
S32         AILEXPORT AIL_digital_CPU_percent(HDIGDRIVER dig)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_digital_CPU_percent(" PTRFMT ")" ,dig);
      }

   IN_AIL_NM;

   result=AIL_API_digital_CPU_percent(dig);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

#ifdef HOST_SPU_PROCESS
DXDEF
S32         AILEXPORT AIL_digital_SPU_percent(HDIGDRIVER dig)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_digital_SPU_percent(" PTRFMT ")" ,dig);
      }

   IN_AIL_NM;

   result=AIL_API_digital_SPU_percent(dig);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}
#endif

//############################################################################

DXDEF
S32         AILEXPORT AIL_background_CPU_percent(void)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_background_CPU_percent()" );
      }

   IN_AIL_NM;

   result=AIL_API_background_CPU_percent();

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_digital_latency(HDIGDRIVER dig)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_digital_latency(" PTRFMT ")" ,dig);
      }

   IN_AIL_NM;

   result=AIL_API_digital_latency(dig);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
HSAMPLE      AILEXPORT AIL_allocate_sample_handle (HDIGDRIVER dig)

{
   HSAMPLE result;

   START
      {
      AIL_debug_log("@AIL_allocate_sample_handle(" PTRFMT ")" ,dig);
      }

   IN_AIL;

   result=AIL_API_allocate_sample_handle(dig);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
HSAMPLE      AILEXPORT AIL_allocate_bus (HDIGDRIVER dig)

{
   HSAMPLE result;

   START
      {
      AIL_debug_log("@AIL_allocate_bus(" PTRFMT ")" ,dig);
      }

   IN_AIL;

   result=AIL_API_allocate_bus(dig);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_install_bus_compressor(HDIGDRIVER dig, S32 bus_index, SAMPLESTAGE filter_stage, S32 input_bus_index)
{
    S32 result;

    START
    {
        AIL_debug_log("@AIL_install_bus_compressor(" PTRFMT ",%d,%d,%d)" ,dig, bus_index, filter_stage, input_bus_index);
    }

    IN_AIL;

    result = AIL_API_install_bus_compressor(dig,bus_index,filter_stage,input_bus_index);

    OUT_AIL;

    RESULT
    {
        outresint(result);
    }

    END;

    return result;
}

//############################################################################
DXDEF void      AILEXPORT AIL_set_sample_bus(HSAMPLE S, S32 bus_index)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_bus(" PTRFMT ",%d)" ,S, bus_index);
      }

   IN_AIL;

   if (bus_index >= 0 &&
       bus_index < MAX_BUSSES &&
       S->driver->bus_samples[bus_index] != 0)
   {
      S->bus = bus_index;
   }

   OUT_AIL;

   END;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_sample_bus(HSAMPLE S)
{
   S32 bus = 0;

   START
      {
      AIL_debug_log("@AIL_sample_bus(" PTRFMT ")" ,S);
      }

   IN_AIL;

   bus = S->bus;

   OUT_AIL;

   RESULT
      {
      outresint(bus);
      }

   END;

   return bus;
}

//############################################################################

DXDEF
HSAMPLE      AILEXPORT AIL_bus_sample_handle (HDIGDRIVER dig, S32 bus_index)

{
   HSAMPLE result;

   START
      {
      AIL_debug_log("@AIL_bus_sample_handle(" PTRFMT ",%d)" ,dig,bus_index);
      }

   IN_AIL;

   result=AIL_API_bus_sample_handle(dig,bus_index);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
void         AILEXPORT AIL_release_sample_handle (HSAMPLE S)
{
   START
      {
      AIL_debug_log("@AIL_release_sample_handle(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_release_sample_handle(S);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
S32        AILEXPORT AIL_init_sample        (HSAMPLE S, //)
                                             S32     format)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_init_sample(" PTRFMT ",%ld)" ,S,format);
      }

   IN_AIL;

   result=AIL_API_init_sample(S,format);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32        AILEXPORT AIL_set_sample_file        (HSAMPLE   S, //)
                                              void const *file_image,
                                              S32       block)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_set_sample_file(" PTRFMT "," PTRFMT ",%ld)" ,S,file_image,block);
      }

   IN_AIL;

   result=AIL_API_set_sample_file(S,file_image,block);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32        AILEXPORT AIL_set_sample_info     (HSAMPLE   S, //)
                                              AILSOUNDINFO const *info )
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_set_sample_info(" PTRFMT "," PTRFMT ")" ,S,info);
      }

   IN_AIL;

   result=AIL_API_set_sample_info(S,info);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32          AILEXPORT AIL_set_named_sample_file (HSAMPLE   S, //)
                                                  C8   const *file_type_suffix,
                                                  void const *file_image,
                                                  U32       file_size,
                                                  S32       block)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_set_named_sample_file(" PTRFMT ",%s," PTRFMT ",%ld,%ld)" ,
         S,file_type_suffix,file_image,file_size,block);
      }

   IN_AIL;

   result=AIL_API_set_named_sample_file(S,file_type_suffix,file_image,file_size,block);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
HPROVIDER    AILEXPORT AIL_set_sample_processor  (HSAMPLE     S, //)
                                                SAMPLESTAGE pipeline_stage,
                                                HPROVIDER   provider)
{
   HPROVIDER result;

   START
      {
      AIL_debug_log("@AIL_set_sample_processor(" PTRFMT ",%ld," HPFMT ")" ,
         S,pipeline_stage,provider);
      }

   IN_AIL;

   result=AIL_API_set_sample_processor(S,pipeline_stage,provider);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
HPROVIDER    AILEXPORT AIL_sample_processor  (HSAMPLE     S, //)
                                            SAMPLESTAGE pipeline_stage)
{
   HPROVIDER result;

   START
      {
      AIL_debug_log("@AIL_sample_processor(" PTRFMT ",%ld)" ,
         S,pipeline_stage);
      }

   IN_AIL;

   result=AIL_API_sample_processor(S,pipeline_stage);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
HPROVIDER    AILEXPORT AIL_set_digital_driver_processor  (HDIGDRIVER  dig, //)
                                                        DIGDRVSTAGE pipeline_stage,
                                                        HPROVIDER   provider)
{
   HPROVIDER result;

   START
      {
      AIL_debug_log("@AIL_set_digital_driver_processor(" PTRFMT ",%ld," HPFMT ")" ,
         dig,pipeline_stage,provider);
      }

   IN_AIL;

   result=AIL_API_set_digital_driver_processor(dig,pipeline_stage,provider);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
HPROVIDER  AILEXPORT AIL_digital_driver_processor  (HDIGDRIVER  dig, //)
                                                    DIGDRVSTAGE pipeline_stage)
{
   HPROVIDER result;

   START
      {
      AIL_debug_log("@AIL_digital_driver_processor(" PTRFMT ",%ld)" ,
         dig,pipeline_stage);
      }

   IN_AIL;

   result=AIL_API_digital_driver_processor(dig,pipeline_stage);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_address    (HSAMPLE S, //)
                                               void const    *start,
                                               U32     len)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_address(" PTRFMT "," PTRFMT ",%lu)" ,S,start,len);
      }

   IN_AIL;

   AIL_API_set_sample_address(S,start,len);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_adpcm_block_size(HSAMPLE S, //)
                                                       U32 blocksize)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_adpcm_block_size(" PTRFMT ",%lu)" ,S,blocksize);
      }

   IN_AIL;

   AIL_API_set_sample_adpcm_block_size(S,blocksize);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_start_sample          (HSAMPLE S)
{
   START
      {
      AIL_debug_log("@AIL_start_sample(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_start_sample(S);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_stop_sample           (HSAMPLE S)
{
   START
      {
      AIL_debug_log("@AIL_stop_sample(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_stop_sample(S);

   OUT_AIL;

   END;
}

DXDEF void AILEXPORT AIL_end_fade_sample(HSAMPLE S)
{
    START
    {
        AIL_debug_log("@AIL_end_fade_sample(" PTRFMT ")", S);
    }

    IN_AIL;

    AIL_API_end_fade_sample(S);

    OUT_AIL;
    END;
}

DXDEC void   AILEXPORT AIL_set_sample_id(HSAMPLE S, S32 id)
{
    START
    {
        AIL_debug_log("@AIL_set_sample_id("PTRFMT",%d)", S, id);
    }

    IN_AIL;
    S->group_id = id;
    OUT_AIL;
    END;
}

DXDEC S32 AILEXPORT AIL_sample_id(HSAMPLE S)
{
    S32 r = 0;
    IN_AIL;
    r = S->group_id;
    OUT_AIL;
    return r;
}

DXDEC void   AILEXPORT AIL_start_sample_group(HDIGDRIVER dig, S32 start_id, S32 set_to_id)
{
    START
    {
        AIL_debug_log("@AIL_start_sample_group("PTRFMT",%d,%d)", dig, start_id, set_to_id);
    }

    IN_AIL;
    AIL_API_sample_group_operation(dig, 0, start_id, set_to_id);
    OUT_AIL;
    END;
}


DXDEC void   AILEXPORT AIL_stop_sample_group(HDIGDRIVER dig, S32 stop_id, S32 set_to_id)
{
    START
    {
        AIL_debug_log("@AIL_stop_sample_group("PTRFMT",%d,%d)", dig, stop_id, set_to_id);
    }

    IN_AIL;
    AIL_API_sample_group_operation(dig, 1, stop_id, set_to_id);
    OUT_AIL;
    END;
}

DXDEC void   AILEXPORT AIL_resume_sample_group(HDIGDRIVER dig, S32 resume_id, S32 set_to_id)
{
    START
    {
        AIL_debug_log("@AIL_resume_sample_group("PTRFMT",%d,%d)", dig, resume_id, set_to_id);
    }

    IN_AIL;
    AIL_API_sample_group_operation(dig, 2, resume_id, set_to_id);
    OUT_AIL;
    END;
}

DXDEC void   AILEXPORT AIL_end_sample_group(HDIGDRIVER dig, S32 end_id)
{
    START
    {
        AIL_debug_log("@AIL_end_sample_group("PTRFMT",%d)", dig, end_id);
    }

    IN_AIL;
    AIL_API_sample_group_operation(dig, 3, end_id, 0);
    OUT_AIL;
    END;
}

DXDEF U64 AILEXPORT AIL_digital_mixed_samples(HDIGDRIVER dig)
{
    U32 mix_count = rrAtomicLoadAcquire32(&dig->num_mixes);
    return (U64)mix_count * dig->hardware_buffer_size / (2 * dig->physical_channels_per_sample);
}

DXDEF void AILEXPORT AIL_schedule_start_sample(HSAMPLE S, U64 mix_time_to_start)
{
    START
    {
        AIL_debug_log("@AIL_schedule_start_sample("PTRFMT",%llu)", S, mix_time_to_start);
    }

    IN_AIL;
    AIL_API_schedule_start_sample(S, mix_time_to_start);
    OUT_AIL;
    END;
}

DXDEF U64 AILEXPORT AIL_sample_schedule_time(HSAMPLE S)
{
    U64 r;
    IN_AIL;
    r = S->mix_start_time;
    OUT_AIL;
    return r;
}

DXDEF void AILEXPORT AIL_push_system_state(HDIGDRIVER dig, U32 flags, S16 crossfade_ms)
{
    START
    {
        AIL_debug_log("@AIL_push_system_state("PTRFMT",%d,%d)", dig, flags, crossfade_ms);
    }

    IN_AIL;
    AIL_API_push_system_state(dig, flags, crossfade_ms);
    OUT_AIL;
    END;
}

DXDEF void AILEXPORT AIL_pop_system_state(HDIGDRIVER dig, S16 crossfade_ms)
{
    START
    {
        AIL_debug_log("@AIL_pop_system_state("PTRFMT",%d)", dig, crossfade_ms);
    }

    IN_AIL;
    AIL_API_pop_system_state(dig, crossfade_ms);
    OUT_AIL;
    END;
}

DXDEF U8 AILEXPORT AIL_system_state_level(HDIGDRIVER dig)
{
    U8 r;
    IN_AIL;
    r = dig->state_index;
    OUT_AIL;
    return r;
}

DXDEF void AILEXPORT AIL_set_sample_level_mask(HSAMPLE S, U8 mask)
{
    START
    {
        AIL_debug_log("@AIL_set_sample_level_mask("PTRFMT",%d)", S, mask);
    }

    IN_AIL;
    S->state_level = mask;
    OUT_AIL;
    END;
}

DXDEF U8 AILEXPORT AIL_sample_level_mask(HSAMPLE S)
{
    U8 r;
    IN_AIL;
    r = S->state_level;
    OUT_AIL;
    return r;
}


//############################################################################

DXDEF
void         AILEXPORT AIL_resume_sample         (HSAMPLE S)
{
   START
      {
      AIL_debug_log("@AIL_resume_sample(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_resume_sample(S);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_end_sample            (HSAMPLE S)
{
   START
      {
      AIL_debug_log("@AIL_end_sample(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_end_sample(S);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_playback_rate//()
                                             (HSAMPLE S,
                                              S32     playback_rate)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_playback_rate(" PTRFMT ",%ld)" ,S,playback_rate);
      }

   IN_AIL;

   AIL_API_set_sample_playback_rate(S,playback_rate);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_playback_rate_factor//()
                                             (HSAMPLE S,
                                              F32     playback_rate_factor)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_playback_rate_factor(" PTRFMT ",%f)" ,S,playback_rate_factor);
      }

   IN_AIL;

   AIL_API_set_sample_playback_rate_factor(S,playback_rate_factor);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_playback_delay//()
                                             (HSAMPLE S,
                                              S32     playback_delay)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_playback_delay(" PTRFMT ",%ld)" ,S,playback_delay);
      }

   IN_AIL;

   AIL_API_set_sample_playback_delay(S,playback_delay);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
void         AILEXPORT AIL_set_sample_channel_levels (HSAMPLE                S, //)
                                                      MSS_SPEAKER const *source_speaker_indexes,
                                                      MSS_SPEAKER const *dest_speaker_indexes,
                                                      F32         const *levels,
                                                      S32                    n_levels)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_channel_levels(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ",%d)" ,
         S,source_speaker_indexes, dest_speaker_indexes, levels,n_levels);
      }

   IN_AIL;

   AIL_API_set_sample_channel_levels(S,source_speaker_indexes,
                                                   dest_speaker_indexes,
                                                   levels,
                                                   n_levels);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
void         AILEXPORT AIL_sample_channel_levels (HSAMPLE                S, //)
                                                  MSS_SPEAKER const *source_speaker_indexes,
                                                  MSS_SPEAKER const *dest_speaker_indexes,
                                                  F32               *levels,
                                                  S32                    n_levels)
{
   START
      {
      AIL_debug_log("@AIL_sample_channel_levels(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ",%d)" ,
         S,source_speaker_indexes, dest_speaker_indexes, levels,n_levels);
      }

   IN_AIL;

   AIL_API_sample_channel_levels(S,source_speaker_indexes,
                                               dest_speaker_indexes,
                                               levels,
                                               n_levels);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
void         AILEXPORT AIL_set_sample_speaker_scale_factors (HSAMPLE                 S, //)
                                                             MSS_SPEAKER const * dest_speaker_indexes,
                                                             F32         const * levels,
                                                             S32                     n_levels )
{
   START
      {
      AIL_debug_log("@AIL_set_sample_speaker_scale_factors(" PTRFMT "," PTRFMT "," PTRFMT ", %d)" ,
         S,dest_speaker_indexes,levels,n_levels);
      }

   IN_AIL;

   AIL_API_set_sample_speaker_scale_factors(S,dest_speaker_indexes,levels,n_levels);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
void         AILEXPORT AIL_sample_speaker_scale_factors (HSAMPLE                 S, //)
                                                         MSS_SPEAKER const * dest_speaker_indexes,
                                                         F32               * levels,
                                                         S32                     n_levels )
{

   START
      {
      AIL_debug_log("@AIL_sample_speaker_scale_factors(" PTRFMT "," PTRFMT "," PTRFMT ", %d)" ,
         S,dest_speaker_indexes,levels,n_levels);
      }

   IN_AIL;

   AIL_API_sample_speaker_scale_factors(S,dest_speaker_indexes,levels,n_levels);

   OUT_AIL;

   END;
    }


//############################################################################
DXDEF
S32          AILEXPORT AIL_set_sample_is_3D             (HSAMPLE                S, //)
                                                         S32                    onoff)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_set_sample_is_3D(" PTRFMT ", %d )" ,
         S,onoff);
      }

   IN_AIL;

   result = AIL_API_set_sample_is_3D(S,onoff);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}


//############################################################################
DXDEF
F32         AILEXPORT AIL_sample_output_levels (HSAMPLE                S, //)
                                                  MSS_SPEAKER const *source_speaker_indexes,
                                                  MSS_SPEAKER const *dest_speaker_indexes,
                                                  F32               *levels,
                                                  S32                    n_levels)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_sample_output_levels(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ",%d)" ,
         S,source_speaker_indexes, dest_speaker_indexes, levels,n_levels);
      }

   IN_AIL;

   result = AIL_API_sample_output_levels(S,source_speaker_indexes,
                                         dest_speaker_indexes,
                                         levels,
                                         n_levels);

   OUT_AIL;

   RESULT
      {
      outresfloat((F32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF
void         AILEXPORT AIL_set_speaker_configuration (HDIGDRIVER dig, //)
                                                      MSSVECTOR3D *speaker_positions,
                                                      S32  n_channels,
                                                      F32  falloff_power)
{
   START
      {
      AIL_debug_log("@AIL_set_speaker_configuration(" PTRFMT "," PTRFMT ",%ld,%f)" ,dig,speaker_positions,n_channels,falloff_power);
      }

   IN_AIL;

   AIL_API_set_speaker_configuration(dig,speaker_positions,n_channels,falloff_power);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
MSSVECTOR3D * AILEXPORT    AIL_speaker_configuration (HDIGDRIVER       dig, //)
                                                          S32         *n_physical_channels,
                                                          S32         *n_logical_channels,
                                                          F32         *falloff_power,
                                                          MSS_MC_SPEC *channel_spec)
{
   MSSVECTOR3D *result = NULL;

   START
      {
      AIL_debug_log("@AIL_speaker_configuration(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,dig,n_physical_channels,n_logical_channels,falloff_power,channel_spec);
      }

   IN_AIL;

   result = AIL_API_speaker_configuration(dig,n_physical_channels,n_logical_channels,falloff_power,channel_spec);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF
void AILEXPORT AIL_set_listener_relative_receiver_array (HDIGDRIVER               dig, //)
                                                         MSS_RECEIVER_LIST *array,
                                                         S32                      n_receivers)
{
   START
      {
      AIL_debug_log("@AIL_set_listener_relative_receiver_array(" PTRFMT "," PTRFMT ",%d)" ,dig,array,n_receivers);
      }

   IN_AIL;

   AIL_API_set_listener_relative_receiver_array(dig,array,n_receivers);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
MSS_RECEIVER_LIST * AILEXPORT    AIL_listener_relative_receiver_array (HDIGDRIVER  dig, //)
                                                                           S32    *n_receivers)
{
   MSS_RECEIVER_LIST *result = NULL;

   START
      {
      AIL_debug_log("@AIL_listener_relative_receiver_array(" PTRFMT "," PTRFMT ")" ,dig,n_receivers);
      }

   IN_AIL;

   result = AIL_API_listener_relative_receiver_array(dig,n_receivers);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

#ifndef IS_XBOX

//############################################################################
DXDEF
void  AILEXPORT AIL_set_speaker_reverb_levels (HDIGDRIVER   dig, //)
                                               F32         *wet_array,
                                               F32         *dry_array,
                                               MSS_SPEAKER const *speaker_array,
                                               S32          n_levels)
{
   START
      {
      AIL_debug_log("@AIL_set_speaker_reverb_levels(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ",%d)" ,dig,wet_array,dry_array,speaker_array,n_levels);
      }

   IN_AIL;

   AIL_API_set_speaker_reverb_levels(dig,wet_array,dry_array,speaker_array,n_levels);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
S32     AILEXPORT AIL_speaker_reverb_levels (HDIGDRIVER                   dig, //)
                                             F32               * *wet_array,
                                             F32               * *dry_array,
                                             MSS_SPEAKER const * *speaker_array)
{
   S32 result = 0;

   START
      {
      AIL_debug_log("@AIL_speaker_reverb_levels(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,dig,wet_array,dry_array,speaker_array);
      }

   IN_AIL;

   result = AIL_API_speaker_reverb_levels(dig,wet_array,dry_array,speaker_array);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF
S32   AILEXPORT AIL_calculate_3D_channel_levels      (HDIGDRIVER                   dig, //)
                                                      F32                     *channel_levels,
                                                      MSS_SPEAKER const * *speaker_array,
                                                      MSSVECTOR3D             *src_pos,
                                                      MSSVECTOR3D             *src_face,
                                                      MSSVECTOR3D             *src_up,
                                                      F32                          src_inner_angle,
                                                      F32                          src_outer_angle,
                                                      F32                          src_outer_volume,
                                                      F32                          src_max_dist,
                                                      F32                          src_min_dist,
                                                      MSSVECTOR3D             *listen_pos,
                                                      MSSVECTOR3D             *listen_face,
                                                      MSSVECTOR3D             *listen_up,
                                                      F32                          rolloff_factor,
                                                      MSSVECTOR3D             *doppler_velocity,
                                                      F32                         *doppler_shift)
{
   S32 result = 0;

   START
      {
      AIL_debug_log("@AIL_calculate_3D_channel_levels(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ",%f,%f,%f,%f,%f," PTRFMT "," PTRFMT "," PTRFMT ",%f," PTRFMT ",%s)" ,
         dig,
         channel_levels,
         speaker_array,
         src_pos,
         src_face,
         src_up,
         src_inner_angle,
         src_outer_angle,
         src_outer_volume,
         src_max_dist,
         src_min_dist,
         listen_pos,
         listen_face,
         listen_up,
         rolloff_factor,
         doppler_velocity,
         doppler_shift);
      }

   IN_AIL;

   result = AIL_API_calculate_3D_channel_levels(
         dig,
         channel_levels,
         speaker_array,
         src_pos,
         src_face,
         src_up,
         src_inner_angle,
         src_outer_angle,
         src_outer_volume,
         src_max_dist,
         src_min_dist,
         listen_pos,
         listen_face,
         listen_up,
         rolloff_factor,
         doppler_velocity,
         doppler_shift);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

#endif

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_volume_pan  (HSAMPLE S, //)
                                                   F32     volume,
                                                   F32     pan)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_volume_pan(" PTRFMT ",%f,%f)" ,S,volume,pan);
      }

   IN_AIL;

   AIL_API_set_sample_volume_pan(S,volume,pan);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_51_volume_pan  (HSAMPLE S, //)
                                                      F32     volume,
                                                      F32     pan,
                                                      F32     fb_pan,
                                                      F32     center_level,
                                                      F32     sub_level )
{
   START
      {
      AIL_debug_log("@AIL_set_sample_51_volume_pan(" PTRFMT ",%f,%f,%f,%f,%f)" ,S,volume,pan,fb_pan,center_level,sub_level);
      }

   IN_AIL;

   AIL_API_set_sample_51_volume_pan(S,volume,pan,fb_pan,center_level,sub_level);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_sample_51_volume_pan  (HSAMPLE   S, //)
                                                  F32 * volume,
                                                  F32 * pan,
                                                  F32 * fb_pan,
                                                  F32 * center_level,
                                                  F32 * sub_level )
{
   START
      {
      AIL_debug_log("@AIL_sample_51_volume_pan(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,S,volume,pan,fb_pan,center_level,sub_level);
      }

   IN_AIL_NM;

   AIL_API_sample_51_volume_pan(S,volume,pan,fb_pan,center_level,sub_level);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_51_volume_levels (HSAMPLE S, //)
                                                        F32     f_left_level,
                                                        F32     f_right_level,
                                                        F32     b_left_level,
                                                        F32     b_right_level,
                                                        F32     center_level,
                                                        F32     sub_level )
{
   START
      {
      AIL_debug_log("@AIL_set_sample_51_volume_levels(" PTRFMT ",%f,%f,%f,%f,%f)" ,S,f_left_level,f_right_level,b_left_level,b_right_level,center_level,sub_level);
      }

   IN_AIL;

   AIL_API_set_sample_51_volume_levels(S,f_left_level,f_right_level,b_left_level,b_right_level,center_level,sub_level);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_sample_51_volume_levels (HSAMPLE   S, //)
                                                    F32 * f_left_level,
                                                    F32 * f_right_level,
                                                    F32 * b_left_level,
                                                    F32 * b_right_level,
                                                    F32 * center_level,
                                                    F32 * sub_level )
{
   START
      {
      AIL_debug_log("@AIL_sample_51_volume_levels(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,S,f_left_level,f_right_level,b_left_level,b_right_level,center_level,sub_level);
      }

   IN_AIL_NM;

   AIL_API_sample_51_volume_levels(S,f_left_level,f_right_level,b_left_level,b_right_level,center_level,sub_level);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_volume_levels  (HSAMPLE S, //)
                                                      F32     left_level,
                                                      F32     right_level)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_volume_levels(" PTRFMT ",%f,%f)" ,S,left_level,right_level);
      }

   IN_AIL;

   AIL_API_set_sample_volume_levels(S,left_level,right_level);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_reverb_levels  (HSAMPLE S, //)
                                                      F32     dry_level,
                                                      F32     wet_level)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_reverb_levels(" PTRFMT ",%f,%f)" ,S,dry_level,wet_level);
      }

   IN_AIL;

   AIL_API_set_sample_reverb_levels(S,dry_level,wet_level);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_low_pass_cut_off(HSAMPLE S, //)
                                                       S32 /*-1 or MSS_SPEAKER*/ input_channel,
                                                       F32         cut_off)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_low_pass_cut_off(" PTRFMT ",%d,%f)" ,S,input_channel,cut_off);
      }

   IN_AIL;

   AIL_API_set_sample_low_pass_cut_off(S,input_channel,cut_off);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_loop_count (HSAMPLE S, //)
                                               S32     loop_count)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_loop_count(" PTRFMT ",%ld)" ,S,loop_count);
      }

   IN_AIL;

   AIL_API_set_sample_loop_count(S,loop_count);

   OUT_AIL;

   END;
}


//############################################################################

DXDEF
void         AILEXPORT AIL_set_sample_loop_block (HSAMPLE S,
                                                     S32     loop_start_offset,
                                                     S32     loop_end_offset)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_loop_block(" PTRFMT ",%ld,%ld)" ,S,loop_start_offset,loop_end_offset);
      }

   IN_AIL;

   AIL_API_set_sample_loop_block(S,loop_start_offset,loop_end_offset);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
S32        AILEXPORT AIL_sample_loop_block   (HSAMPLE S, //)
                                              S32    *loop_start_offset,
                                              S32    *loop_end_offset)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_loop_block(" PTRFMT "," PTRFMT "," PTRFMT ")" ,
          S,loop_start_offset,loop_end_offset);
      }

   IN_AIL_NM;

   result = AIL_API_sample_loop_block(S,loop_start_offset,loop_end_offset);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
U32        AILEXPORT AIL_sample_status         (HSAMPLE S)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_sample_status(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sample_status(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_sample_playback_rate  (HSAMPLE S)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_playback_rate(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sample_playback_rate(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
F32         AILEXPORT AIL_sample_playback_rate_factor  (HSAMPLE S)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_sample_playback_rate_factor(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sample_playback_rate_factor(S);

   OUT_AIL_NM;

   RESULT
      {
      outresfloat(result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_sample_playback_delay  (HSAMPLE S)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_playback_delay(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sample_playback_delay(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_sample_volume_pan         (HSAMPLE S,F32 *volume, F32 * pan)
{
   START
      {
      AIL_debug_log("@AIL_sample_volume_pan(" PTRFMT "," PTRFMT "," PTRFMT ")" ,S,volume,pan);
      }

   IN_AIL_NM;

   AIL_API_sample_volume_pan(S,volume,pan);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_sample_volume_levels  (HSAMPLE S, //)
                                                  F32 * left_level,
                                                  F32 * right_level)
{
   START
      {
      AIL_debug_log("@AIL_sample_volume_levels(" PTRFMT "," PTRFMT "," PTRFMT ")" ,S,left_level,right_level);
      }

   IN_AIL_NM;

   AIL_API_sample_volume_levels(S,left_level,right_level);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_sample_reverb_levels  (HSAMPLE S, //)
                                                  F32 * dry_level,
                                                  F32 * wet_level)
{
   START
      {
      AIL_debug_log("@AIL_sample_reverb_levels(" PTRFMT "," PTRFMT "," PTRFMT ")" ,S,dry_level,wet_level);
      }

   IN_AIL_NM;

   AIL_API_sample_reverb_levels(S,dry_level,wet_level);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
F32         AILEXPORT AIL_sample_low_pass_cut_off(HSAMPLE S, S32 /*-1 or MSS_SPEAKER*/ input_channel )
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_sample_low_pass_cut_off(" PTRFMT ", %d)" ,S, input_channel);
      }

   IN_AIL_NM;

   result=AIL_API_sample_low_pass_cut_off(S,input_channel);

   OUT_AIL_NM;

   RESULT
      {
      outresfloat(result);
      }

   END;

   return( result );
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_sample_loop_count     (HSAMPLE S)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_loop_count(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sample_loop_count(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_set_digital_master_volume_level
                                             (HDIGDRIVER dig,
                                              F32        master_volume)
{
   START
      {
      AIL_debug_log("@AIL_set_digital_master_volume_level(" PTRFMT ",%f)" ,dig,master_volume);
      }

   IN_AIL;

   AIL_API_set_digital_master_volume_level(dig,master_volume);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
F32     AILEXPORT AIL_digital_master_volume_level   (HDIGDRIVER dig)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_digital_master_volume_level(" PTRFMT ")" ,dig);
      }

   IN_AIL;

   result=AIL_API_digital_master_volume_level(dig);

   OUT_AIL;

   RESULT
      {
      outresfloat((F32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_digital_master_reverb_levels  (HDIGDRIVER dig, //)
                                                              S32     bus_index,
                                                              F32     dry_level,
                                                              F32     wet_level)
{
   START
      {
      AIL_debug_log("@AIL_set_digital_master_reverb_levels(" PTRFMT ",%ld,%f,%f)" ,dig,bus_index,dry_level,wet_level);
      }

   IN_AIL;

   AIL_API_set_digital_master_reverb_levels(dig,bus_index,dry_level,wet_level);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_digital_master_reverb_levels  (HDIGDRIVER dig, //)
                                                          S32 bus_index,
                                                          F32 * dry_level,
                                                          F32 * wet_level)
{
   START
      {
      AIL_debug_log("@AIL_digital_master_reverb_levels(" PTRFMT ",%ld," PTRFMT "," PTRFMT ")" ,dig,bus_index,dry_level,wet_level);
      }

   IN_AIL;

   AIL_API_digital_master_reverb_levels(dig,bus_index,dry_level,wet_level);

   OUT_AIL;

   END;
}

#ifndef IS_XBOX

//############################################################################

DXDEF
void         AILEXPORT AIL_set_digital_master_reverb         (HDIGDRIVER dig, //)
                                                              S32     bus_index,
                                                              F32     reverb_time,
                                                              F32     reverb_predelay,
                                                              F32     reverb_damping)
{
   START
      {
      AIL_debug_log("@AIL_set_digital_master_reverb(" PTRFMT ",%ld,%f,%f,%f)" ,dig,bus_index,reverb_time,reverb_predelay,reverb_damping);
      }

   IN_AIL;

   AIL_API_set_digital_master_reverb(dig,bus_index,reverb_time,reverb_predelay,reverb_damping);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_digital_master_reverb (HDIGDRIVER dig, //)
                                                  S32 bus_index,
                                                  F32 * reverb_time,
                                                  F32 * reverb_predelay,
                                                  F32 * reverb_damping)
{
   START
      {
      AIL_debug_log("@AIL_digital_master_reverb(" PTRFMT ",%ld," PTRFMT "," PTRFMT "," PTRFMT ")" ,dig,bus_index,reverb_time,reverb_predelay,reverb_damping);
      }

   IN_AIL_NM;

   AIL_API_digital_master_reverb(dig,bus_index,reverb_time,reverb_predelay,reverb_damping);

   OUT_AIL_NM;

   END;
}

#endif

//############################################################################

DXDEF
S32     AILEXPORT AIL_minimum_sample_buffer_size  (HDIGDRIVER dig, //)
                                                S32        playback_rate,
                                                S32        format)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_minimum_sample_buffer_size(" PTRFMT ",%ld,%ld)" ,dig,
         playback_rate,format);
      }

   IN_AIL;

   result=AIL_API_minimum_sample_buffer_size(dig,playback_rate,format);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_sample_buffer_available  (HSAMPLE S)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_buffer_available(" PTRFMT ")" ,S);
      }

   IN_AIL;

   result=AIL_API_sample_buffer_available(S);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_sample_buffer_count       (HSAMPLE S)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_buffer_count(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sample_buffer_count(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

DXDEF
S32     AILEXPORT AIL_sample_loaded_len       (HSAMPLE S)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_loaded_len(" PTRFMT ")" ,S);
      }

   IN_AIL;

   result=AIL_API_sample_loaded_len(S);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
S32     AILEXPORT AIL_set_sample_buffer_count (HSAMPLE S, //)
                                               S32     n_buffers)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_set_sample_buffer_count(" PTRFMT ")" ,S);
      }

   IN_AIL;

   result=AIL_API_set_sample_buffer_count(S,n_buffers);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_load_sample_buffer      (HSAMPLE S, //)
                                               S32     buff_num,
                                               void const *buffer,
                                               U32     len)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_load_sample_buffer(" PTRFMT ",%ld," PTRFMT ",%lu)" ,
         S,buff_num,buffer,len);
      }

   IN_AIL;

   result=AIL_API_load_sample_buffer(S,buff_num,buffer,len);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_sample_buffer_info      (HSAMPLE S, //)
            S32          buff_num,
            U32     *pos,
            U32     *len,
            S32     *head,
            S32     *tail)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_buffer_info(" PTRFMT ",%ld," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
                 S,buff_num,pos,len,head,tail);
      }

   IN_AIL;

   result=AIL_API_sample_buffer_info(S,buff_num,pos,len,head,tail);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
U32    AILEXPORT AIL_sample_granularity        (HSAMPLE S)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_sample_granularity(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sample_granularity(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
S32    AILEXPORT AIL_sample_channel_count       (HSAMPLE S, //)
                                                 U32    *mask)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_channel_count(" PTRFMT "," PTRFMT ")" ,S,mask);
      }

   IN_AIL_NM;

   result=AIL_API_sample_channel_count(S,mask);

   OUT_AIL_NM;

   RESULT
      {
      outresint((S32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_set_sample_position       (HSAMPLE S, //)
                                               U32     pos)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_position(" PTRFMT ",%lu)" ,S,pos);
      }

   IN_AIL;

   AIL_API_set_sample_position(S,pos);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
U32    AILEXPORT AIL_sample_position           (HSAMPLE S)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_sample_position(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sample_position(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILSAMPLECB AILEXPORT AIL_register_SOB_callback     (HSAMPLE S, //)
                                                    AILSAMPLECB SOB)
{
   AILSAMPLECB result;

   START
      {
      AIL_debug_log("@AIL_register_SOB_callback(" PTRFMT "," PTRFMT ")" ,S,SOB);
      }

   IN_AIL;

   result=AIL_API_register_SOB_callback(S,SOB);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILSAMPLECB AILEXPORT AIL_register_EOB_callback     (HSAMPLE S, //)
                                                    AILSAMPLECB EOB)
{
   AILSAMPLECB result;

   START
      {
      AIL_debug_log("@AIL_register_EOB_callback(" PTRFMT "," PTRFMT ")" ,S,EOB);
      }

   IN_AIL;

   result=AIL_API_register_EOB_callback(S,EOB);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILSAMPLECB AILEXPORT AIL_register_EOS_callback     (HSAMPLE S, //)
                                                    AILSAMPLECB EOS)
{
   AILSAMPLECB result;

   START
      {
      AIL_debug_log("@AIL_register_EOS_callback(" PTRFMT "," PTRFMT ")" ,S,EOS);
      }

   IN_AIL;

   result=AIL_API_register_EOS_callback(S,EOS);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILMIXERCB AILEXPORT AIL_register_mix_callback     (HDIGDRIVER dig, //)
                                                    AILMIXERCB mixcb)
{
   AILMIXERCB result = 0;

   START
      {
      AIL_debug_log("@AIL_register_mix_callback(" PTRFMT "," PTRFMT ")" ,dig,mixcb);
      }

   IN_AIL;

   if (dig)
   {
     result = dig->mixcb;
     dig->mixcb = mixcb;
   }

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILFALLOFFCB AILEXPORT AIL_register_falloff_function_callback     (HSAMPLE S, //)
                                                                  AILFALLOFFCB falloff_cb)
{
   AILFALLOFFCB result;

   START
      {
      AIL_debug_log("@AIL_register_falloff_function_callback(" PTRFMT "," PTRFMT ")" ,S,falloff_cb);
      }

   IN_AIL;

   result=AIL_API_register_falloff_function_callback(S,falloff_cb);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_set_sample_user_data      (HSAMPLE S, //)
                                                  U32     index,
                                                  SINTa   value)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_user_data(" PTRFMT ",%ld," SINTFMT ")" ,S,index,value);
      }

   IN_AIL_NM;

   AIL_API_set_sample_user_data(S,index,value);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
SINTa   AILEXPORT AIL_sample_user_data          (HSAMPLE S, //)
                                                 U32     index)
{
   SINTa result;

   START
      {
      AIL_debug_log("@AIL_sample_user_data(" PTRFMT ",%lu)" ,S,index);
      }

   IN_AIL_NM;

   result=AIL_API_sample_user_data(S,index);

   OUT_AIL_NM;

   RESULT
      {
      outresuint((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_active_sample_count       (HDIGDRIVER dig)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_active_sample_count(" PTRFMT ")" ,dig);
      }

   IN_AIL;

   result=AIL_API_active_sample_count(dig);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

void     AILEXPORT AIL_digital_configuration     (HDIGDRIVER dig,
                                               S32   *rate,
                                               S32   *format,
                                               char  *config)
{
   START
      {
      AIL_debug_log("@AIL_digital_configuration(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
         dig,rate,format,config);
      }

   IN_AIL;

   AIL_API_digital_configuration(dig,rate,format,config);

   OUT_AIL;

   END;
}


//############################################################################

DXDEF
S32        AILEXPORT AIL_file_error  (void)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_file_error()" );
      }

   IN_AIL_NM;

   result=AIL_API_file_error();

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
S32        AILEXPORT AIL_file_size_info (char const   *filename,
                                         char const * caller,
                                         U32 caller_line )
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_file_size(%s) from: %s, line: %d " ,filename,caller,caller_line);
      }

   IN_AIL_NM;

   result=AIL_API_file_size(filename,caller,caller_line);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void * AILEXPORT AIL_file_read_info   (char const   *filename,
                                       void *dest,
                                       char const * caller,
                                       U32 caller_line )
{
   void * result;

   START
      {
      AIL_debug_log("@AIL_file_read(%s," PTRFMT ") from: %s, line: %d " ,filename,dest,caller,caller_line);
      }

   IN_AIL_NM;

   result=AIL_API_file_read(filename,dest,caller,caller_line);

   OUT_AIL_NM;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
S32        AILEXPORT AIL_file_write  (char const   *filename,
                                          void const *buf,
                                          U32       len)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_file_write(%s," PTRFMT ",%ld)" ,filename,buf,len);
      }

   IN_AIL_NM;

   result=AIL_API_file_write(filename,buf,len);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
S32        AILEXPORT AIL_WAV_file_write  (char const   *filename,
                                          void const *buf,
                                          U32       len,
                                          S32       rate,
                                          S32       format)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_WAV_file_write(%s," PTRFMT ",%ld,%ld,%ld)" ,filename,buf,len,rate,format);
      }

   IN_AIL_NM;

   result=AIL_API_WAV_file_write(filename,buf,len,rate,format);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

#if defined(IS_WINDOWS) || defined(IS_MAC) || defined(IS_LINUX) || defined(IS_XBOX) || defined(IS_XENON) || defined(IS_PS2) || defined(IS_PS3) || defined(IS_WII) || defined(IS_PSP) || defined(IS_IPHONE) || defined(IS_PSP2) || defined(__RADANDROID__)

DXDEF
void       AILEXPORT AIL_serve()
{
   START
      {
      AIL_debug_log("@AIL_serve()" );
      }

   IN_AIL_NM;

   AIL_API_serve();

   OUT_AIL_NM;

   END;
}

#endif




//############################################################################

DXDEF
HSTREAM AILEXPORT AIL_open_stream(HDIGDRIVER dig, char const * filename, S32 stream_mem)
{
   HSTREAM result;

   START
      {
      AIL_debug_log("@AIL_open_stream(" PTRFMT ", %s, %ld)" ,dig,filename,stream_mem);
      }

   IN_AIL;

   result=AIL_API_open_stream(dig,filename,stream_mem);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void AILEXPORT AIL_close_stream(HSTREAM stream)
{
   START
      {
      AIL_debug_log("@AIL_close_stream(" PTRFMT ")" ,stream);
      }

   IN_AIL;

   AIL_API_close_stream(stream);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
HSAMPLE AILEXPORT AIL_stream_sample_handle(HSTREAM stream)
{
   HSAMPLE result = NULL;

   START
      {
      AIL_debug_log("@AIL_stream_sample_handle(" PTRFMT ")" ,stream);
      }

   if (stream != NULL)
      result = stream->samp;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}

//############################################################################

DXDEF
S32 AILEXPORT AIL_service_stream(HSTREAM stream, S32 fillup)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_service_stream(" PTRFMT ", %ld)" ,stream,fillup);
      }

   IN_AIL;

   result=AIL_API_service_stream(stream,fillup);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void AILEXPORT AIL_start_stream(HSTREAM stream)
{
   START
      {
      AIL_debug_log("@AIL_start_stream(" PTRFMT ")" ,stream);
      }

   IN_AIL;

   AIL_API_start_stream(stream);

   OUT_AIL;

   END;
}


//############################################################################

DXDEF
void AILEXPORT AIL_pause_stream(HSTREAM stream, S32 onoff)
{
   START
      {
      AIL_debug_log("@AIL_pause_stream(" PTRFMT ", %ld)" ,stream,onoff);
      }

   IN_AIL;

   AIL_API_pause_stream(stream,onoff);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
S32 AILEXPORT AIL_stream_loop_count(HSTREAM stream)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_stream_loop_count(" PTRFMT ")" ,stream);
      }

   IN_AIL_NM;

   result=AIL_API_stream_loop_count(stream);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void         AILEXPORT AIL_set_stream_loop_block (HSTREAM S,
                                                     S32     loop_start_offset,
                                                     S32     loop_end_offset)
{
   START
      {
      AIL_debug_log("@AIL_set_stream_loop_block(" PTRFMT ",%ld,%ld)" ,S,loop_start_offset,loop_end_offset);
      }

   IN_AIL;

   AIL_API_set_stream_loop_block(S,loop_start_offset,loop_end_offset);

   OUT_AIL;

   END;
}



//############################################################################

DXDEF
void AILEXPORT AIL_set_stream_loop_count(HSTREAM stream, S32 count)
{
   START
      {
      AIL_debug_log("@AIL_set_stream_loop_count(" PTRFMT ", %ld)" ,stream,count);
      }

   IN_AIL;

   AIL_API_set_stream_loop_count(stream,count);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
S32 AILEXPORT AIL_stream_status(HSTREAM stream)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_stream_status(" PTRFMT ")" ,stream);
      }

   IN_AIL_NM;

   result=AIL_API_stream_status(stream);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}

DXDEF
F32 AILEXPORT AIL_stream_filled_percent(HSTREAM stream)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_stream_filled_percent(" PTRFMT ")" ,stream);
      }

   IN_AIL_NM;

   result=AIL_API_stream_filled_percent(stream);

   OUT_AIL_NM;

   RESULT
      {
      outresfloat((F32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void AILEXPORT AIL_set_stream_position(HSTREAM stream,S32 offset)
{
   START
      {
      AIL_debug_log("@AIL_set_stream_position(" PTRFMT ", %ld)" ,stream,offset);
      }

   IN_AIL;

   AIL_API_set_stream_position(stream,offset);

   OUT_AIL;

   END;
}


//############################################################################

DXDEF
S32 AILEXPORT AIL_stream_position(HSTREAM stream)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_stream_position(" PTRFMT ")" ,stream);
      }

   IN_AIL;

   result=AIL_API_stream_position(stream);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void AILEXPORT AIL_stream_info(HSTREAM stream, S32 * datarate, S32 * sndtype, S32 * length, S32 * memory)
{
   START
      {
      AIL_debug_log("@AIL_stream_info(" PTRFMT ", " PTRFMT ", " PTRFMT ", " PTRFMT ", " PTRFMT ")" ,stream,datarate,sndtype,length,memory);
      }

   IN_AIL_NM;

   AIL_API_stream_info(stream,datarate,sndtype,length,memory);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
AILSTREAMCB AILEXPORT AIL_register_stream_callback(HSTREAM stream, AILSTREAMCB callback)
{
   AILSTREAMCB result;

   START
      {
      AIL_debug_log("@AIL_register_stream_callback(" PTRFMT ", " PTRFMT ")" ,stream,callback);
      }

   IN_AIL;

   result=AIL_API_register_stream_callback(stream,callback);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


#if defined(IS_WINDOWS) || defined(IS_MAC) || defined(IS_LINUX) || defined(IS_XBOX) || defined(IS_XENON) || defined(IS_PS2) || defined(IS_PS3) || defined(IS_WII) || defined(IS_PSP) || defined(IS_IPHONE) || defined(IS_PSP2)

//############################################################################

DXDEF
void AILEXPORT AIL_auto_service_stream(HSTREAM stream, S32 onoff)
{
   START
      {
      AIL_debug_log("@AIL_auto_service_stream(" PTRFMT ", %ld)" ,stream,onoff);
      }

   IN_AIL;

   AIL_API_auto_service_stream(stream,onoff);

   OUT_AIL;

   END;
}


#endif

//############################################################################

DXDEF
void     AILEXPORT AIL_set_stream_user_data      (HSTREAM S, //)
                                                  U32     index,
                                                  SINTa   value)
{
   START
      {
      AIL_debug_log("@AIL_set_stream_user_data(" PTRFMT ",%ld," SINTFMT ")" ,S,index,value);
      }

   IN_AIL_NM;

   AIL_API_set_stream_user_data(S,index,value);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
SINTa    AILEXPORT AIL_stream_user_data          (HSTREAM S, //)
                                                  U32     index)
{
   SINTa result;

   START
      {
      AIL_debug_log("@AIL_stream_user_data(" PTRFMT ",%lu)" ,S,index);
      }

   IN_AIL_NM;

   result=AIL_API_stream_user_data(S,index);

   OUT_AIL_NM;

   RESULT
      {
      outresuint((UINTa)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_sample_ms_lookup(HSAMPLE S, //)
                                    S32        milliseconds,
                                    S32*       actualms)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_sample_ms_lookup(" PTRFMT ",%d," PTRFMT ")" ,S,milliseconds,actualms);
      }

   IN_AIL_NM;

   result = AIL_API_sample_ms_lookup(S,milliseconds,actualms);

   OUT_AIL_NM;

   RESULT
   {
       outresint(result);
   }

   END;

   return result;
}


//############################################################################

DXDEF
void AILEXPORT AIL_sample_ms_position(HSAMPLE S, //)
                                  S32    *total_milliseconds,
                                  S32    *current_milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_sample_ms_position(" PTRFMT "," PTRFMT "," PTRFMT ")" ,S,total_milliseconds,current_milliseconds);
      }

   IN_AIL_NM;

   AIL_API_sample_ms_position(S,total_milliseconds,current_milliseconds);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void AILEXPORT    AIL_set_sample_ms_position  (HSAMPLE S, //)
                                               S32       milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_ms_position(" PTRFMT ",%lu)" ,S,milliseconds);
      }

   IN_AIL;

   AIL_API_set_sample_ms_position(S,milliseconds);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void AILEXPORT AIL_stream_ms_position(HSTREAM S, //)
                                  S32    *total_milliseconds,
                                  S32    *current_milliseconds)
{
    START
      {
      AIL_debug_log("@AIL_stream_ms_position(" PTRFMT "," PTRFMT "," PTRFMT ")" ,S,total_milliseconds,current_milliseconds);
      }

   IN_AIL_NM;

   AIL_API_stream_ms_position(S,total_milliseconds,current_milliseconds);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void AILEXPORT    AIL_set_stream_ms_position  (HSTREAM S, //)
                                               S32       milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_set_stream_ms_position(" PTRFMT ",%lu)" ,S,milliseconds);
      }

   IN_AIL;

   AIL_API_set_stream_ms_position(S,milliseconds);

   OUT_AIL;

   END;
}

DXDEF
S32 AILEXPORT AIL_decompress_ADPCM(AILSOUNDINFO const * info,
                                void * * outdata, U32 * outsize)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_decompress_ADPCM(" PTRFMT "," PTRFMT "," PTRFMT ")" ,
         info,outdata,outsize);
      }

   IN_AIL_NM;

   result=AIL_API_decompress_ADPCM(info,outdata,outsize);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

  return( result);

}


DXDEF
S32 AILEXPORT AIL_WAV_info(void const * data, AILSOUNDINFO * info)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_WAV_info(" PTRFMT "," PTRFMT ")" ,
         data,info);
      }

   IN_AIL_NM;

   result=AIL_API_WAV_info(data,info);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

  return( result);

}

DXDEF
S32 AILEXPORT AIL_file_type(void const * data, U32 size)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_file_type(" PTRFMT ",%lu)" ,
         data,size);
      }

   IN_AIL_NM;

   result=AIL_API_file_type(data,size);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

  return( result);

}

DXDEF
S32 AILEXPORT AIL_file_type_named(void const * data, char const * filename,U32 size)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_file_type(" PTRFMT ",%s,%lu)" ,
         data,filename,size);
      }

   IN_AIL_NM;

   result=AIL_API_file_type_named(data,filename,size);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

  return( result);

}


// include high level thunks.
#include "mileseventthunk.cpp"

extern HDIGDRIVER primary_digital_driver;   // from msswo.cpp

//############################################################################
DXDEF HDIGDRIVER AILEXPORT AIL_primary_digital_driver  (HDIGDRIVER new_primary)
{
   HDIGDRIVER result;

   START
      {
      AIL_debug_log("@AIL_primary_digital_driver(" PTRFMT ")" ,
         new_primary);
      }

   IN_AIL;

   result = primary_digital_driver;

   if (new_primary != NULL)
      {
      primary_digital_driver = new_primary;
      }

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF
void       AILEXPORT AIL_set_sample_obstruction (HSAMPLE S, //)
                                                 F32     obstruction)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_obstruction(" PTRFMT ",%f)" , S, obstruction);
      }

   IN_AIL;

   AIL_API_set_sample_obstruction(S, obstruction);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
void       AILEXPORT AIL_set_sample_exclusion (HSAMPLE S, //)
                                               F32     exclusion)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_exclusion(" PTRFMT ",%f)" , S, exclusion);
      }

   IN_AIL;

   AIL_API_set_sample_exclusion(S, exclusion);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
void       AILEXPORT AIL_set_sample_occlusion   (HSAMPLE S, //)
                                                 F32     occlusion)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_occlusion(" PTRFMT ",%s)" , S, occlusion);
      }

   IN_AIL;

   AIL_API_set_sample_occlusion(S, occlusion);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
void       AILEXPORT AIL_set_sample_3D_cone        (HSAMPLE S, //)
                                                    F32       inner_angle,
                                                    F32       outer_angle,
                                                    F32       outer_volume)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_cone(" PTRFMT ",%f,%f,%f)" ,S, inner_angle,outer_angle,outer_volume);
      }

   IN_AIL;

   AIL_API_set_sample_3D_cone(S, inner_angle,outer_angle,outer_volume);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF
F32        AILEXPORT AIL_sample_obstruction (HSAMPLE S)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_sample_obstruction(" PTRFMT ")" ,
          S);
      }

   IN_AIL_NM;

   result = AIL_API_sample_obstruction(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)(S32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF
F32        AILEXPORT AIL_sample_exclusion (HSAMPLE S)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_sample_exclusion(" PTRFMT ")" ,
          S);
      }

   IN_AIL_NM;

   result = AIL_API_sample_exclusion(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)(S32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF
F32        AILEXPORT AIL_sample_occlusion   (HSAMPLE S)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_sample_occlusion(" PTRFMT ")" ,
          S);
      }

   IN_AIL_NM;

   result = AIL_API_sample_occlusion(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)(S32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF
void       AILEXPORT AIL_sample_3D_cone        (HSAMPLE S,
                                                F32 * inner_angle,
                                                F32 * outer_angle,
                                                F32 * outer_volume)
{
   START
      {
      AIL_debug_log("@AIL_sample_3D_cone(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
          S,inner_angle,outer_angle,outer_volume);
      }

   IN_AIL_NM;

   AIL_API_sample_3D_cone(S,inner_angle,outer_angle,outer_volume);

   OUT_AIL_NM;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_distances (HSAMPLE samp, //)
                                                      F32     max_dist,
                                                      F32     min_dist,
                                                      S32     auto_3D_wet_atten)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_distances(" PTRFMT ",%f,%f,%ld)" ,
          samp,max_dist,min_dist,auto_3D_wet_atten);
      }

   IN_AIL;

   AIL_API_set_sample_3D_distances(samp,max_dist,min_dist,auto_3D_wet_atten);

   OUT_AIL;

   END;
}


//############################################################################
DXDEF void AILEXPORT     AIL_sample_3D_distances     (HSAMPLE samp, //)
                                                      F32 * max_dist,
                                                      F32 * min_dist,
                                                      S32 * auto_3D_wet_atten)
{
   START
      {
      AIL_debug_log("@AIL_sample_3D_distances(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
          samp,max_dist,min_dist,auto_3D_wet_atten);
      }

   IN_AIL_NM;

   AIL_API_sample_3D_distances(samp,max_dist,min_dist,auto_3D_wet_atten);

   OUT_AIL_NM;

   END;
}

//############################################################################
DXDEF  S32 AILEXPORT    AIL_room_type   (HDIGDRIVER dig, S32 bus_index)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_room_type(" PTRFMT ",%ld)" ,dig,bus_index);
      }

   IN_AIL_NM;

   result = AIL_API_room_type(dig,bus_index);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF  void AILEXPORT   AIL_set_room_type   (HDIGDRIVER dig, //)
                                             S32       bus_index,
                                             S32       EAX_room_type)
{
   START
      {
      AIL_debug_log("@AIL_set_room_type(" PTRFMT ",%ld,%d)" ,dig,bus_index,EAX_room_type);
      }

   IN_AIL;

   AIL_API_set_room_type(dig, bus_index, EAX_room_type);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF  F32 AILEXPORT    AIL_3D_rolloff_factor  (HDIGDRIVER dig)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_3D_rolloff_factor(" PTRFMT ")" ,dig);
      }

   IN_AIL_NM;

   result = AIL_API_3D_rolloff_factor(dig);

   OUT_AIL_NM;

   RESULT
      {
      outresfloat((F32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF  void AILEXPORT   AIL_set_3D_rolloff_factor (HDIGDRIVER dig, //)
                                                   F32       factor )
{
   START
      {
      AIL_debug_log("@AIL_set_3D_rolloff_factor(" PTRFMT ",%f)" ,dig,factor);
      }

   IN_AIL;

   AIL_API_set_3D_rolloff_factor(dig, factor);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF  F32 AILEXPORT    AIL_3D_doppler_factor  (HDIGDRIVER dig)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_3D_doppler_factor(" PTRFMT ")" ,dig);
      }

   IN_AIL_NM;

   result = AIL_API_3D_doppler_factor(dig);

   OUT_AIL_NM;

   RESULT
      {
      outresfloat((F32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF  void AILEXPORT   AIL_set_3D_doppler_factor (HDIGDRIVER dig, //)
                                                   F32       factor )
{
   START
      {
      AIL_debug_log("@AIL_set_3D_doppler_factor(" PTRFMT ",%f)" ,dig,factor);
      }

   IN_AIL;

   AIL_API_set_3D_doppler_factor(dig, factor);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF  F32 AILEXPORT    AIL_3D_distance_factor  (HDIGDRIVER dig)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_3D_distance_factor(" PTRFMT ")" ,dig);
      }

   IN_AIL_NM;

   result = AIL_API_3D_distance_factor(dig);

   OUT_AIL_NM;

   RESULT
      {
      outresfloat((F32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF  void AILEXPORT   AIL_set_3D_distance_factor (HDIGDRIVER dig, //)
                                                    F32       factor )
{
   START
      {
      AIL_debug_log("@AIL_set_3D_distance_factor(" PTRFMT ",%f)" ,dig,factor);
      }

   IN_AIL;

   AIL_API_set_3D_distance_factor(dig, factor);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void  AILEXPORT    AIL_set_sample_3D_position         (HSAMPLE S, //)
                                                      F32     X,
                                                      F32     Y,
                                                      F32     Z)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_position(" PTRFMT ",%f,%f,%f)" ,S,X,Y,Z);
      }

   IN_AIL;

   AIL_API_set_sample_3D_position(S,X,Y,Z);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_velocity         (HSAMPLE S, //)
                                                      F32     dX_per_ms,
                                                      F32     dY_per_ms,
                                                      F32     dZ_per_ms,
                                                      F32     magnitude)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_velocity(" PTRFMT ",%f,%f,%f,%f)" ,
          S,dX_per_ms,dY_per_ms,dZ_per_ms,magnitude);
      }

   IN_AIL;

   AIL_API_set_sample_3D_velocity(S,dX_per_ms,dY_per_ms,dZ_per_ms,magnitude);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_velocity_vector  (HSAMPLE S, //)
                                                      F32     dX_per_ms,
                                                      F32     dY_per_ms,
                                                      F32     dZ_per_ms)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_velocity_vector(" PTRFMT ",%f,%f,%f)" ,
          S,dX_per_ms,dY_per_ms,dZ_per_ms);
      }

   IN_AIL;

   AIL_API_set_sample_3D_velocity_vector(S,dX_per_ms,dY_per_ms,dZ_per_ms);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_orientation      (HSAMPLE S, //)
                                                      F32     X_face,
                                                      F32     Y_face,
                                                      F32     Z_face,
                                                      F32     X_up,
                                                      F32     Y_up,
                                                      F32     Z_up)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_orientation(" PTRFMT ",%f,%f,%f,%f,%f,%f)" ,
          S,X_face,Y_face,Z_face,X_up,Y_up,Z_up);
      }

   IN_AIL;

   AIL_API_set_sample_3D_orientation(S,X_face,Y_face,Z_face,X_up,Y_up,Z_up);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF S32 AILEXPORT     AIL_sample_3D_position       (HSAMPLE  S, //)
                                                      F32 *X,
                                                      F32 *Y,
                                                      F32 *Z)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sample_3D_position(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
          S,X,Y,Z);
      }

   IN_AIL_NM;

   result = AIL_API_sample_3D_position(S,X,Y,Z);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32) result);
      }

   END;

   return result;
}

//############################################################################
DXDEF void AILEXPORT     AIL_sample_3D_velocity             (HSAMPLE  S, //)
                                                      F32 *dX_per_ms,
                                                      F32 *dY_per_ms,
                                                      F32 *dZ_per_ms)
{
   START
      {
      AIL_debug_log("@AIL_sample_3D_velocity(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
          S,dX_per_ms,dY_per_ms,dZ_per_ms);
      }

   IN_AIL_NM;

   AIL_API_sample_3D_velocity(S,dX_per_ms,dY_per_ms,dZ_per_ms);

   OUT_AIL_NM;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_sample_3D_orientation          (HSAMPLE  S, //)
                                                      F32 *X_face,
                                                      F32 *Y_face,
                                                      F32 *Z_face,
                                                      F32 *X_up,
                                                      F32 *Y_up,
                                                      F32 *Z_up)
{
   START
      {
      AIL_debug_log("@AIL_sample_3D_orientation(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
          S,X_face,Y_face,Z_face,X_up,Y_up,Z_up);
      }

   IN_AIL_NM;

   AIL_API_sample_3D_orientation(S,X_face,Y_face,Z_face,X_up,Y_up,Z_up);

   OUT_AIL_NM;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_update_sample_3D_position      (HSAMPLE S, //)
                                                             F32     dt_milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_update_sample_3D_position(" PTRFMT ",%f)" ,
          S,dt_milliseconds);
      }

   IN_AIL;

   AIL_API_update_sample_3D_position(S, dt_milliseconds);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void  AILEXPORT    AIL_set_listener_3D_position         (HDIGDRIVER dig, //)
                                                      F32     X,
                                                      F32     Y,
                                                      F32     Z)
{
   START
      {
      AIL_debug_log("@AIL_set_listener_3D_position(" PTRFMT ",%f,%f,%f)" , dig,X,Y,Z);
      }

   IN_AIL;

   AIL_API_set_listener_3D_position(dig,X,Y,Z);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_listener_3D_velocity         (HDIGDRIVER dig, //)
                                                      F32     dX_per_ms,
                                                      F32     dY_per_ms,
                                                      F32     dZ_per_ms,
                                                      F32     magnitude)
{
   START
      {
      AIL_debug_log("@AIL_set_listener_3D_velocity(" PTRFMT ",%f,%f,%f,%f)" ,
          dig,dX_per_ms,dY_per_ms,dZ_per_ms,magnitude);
      }

   IN_AIL;

   AIL_API_set_listener_3D_velocity(dig,dX_per_ms,dY_per_ms,dZ_per_ms,magnitude);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_listener_3D_velocity_vector  (HDIGDRIVER dig, //)
                                                      F32     dX_per_ms,
                                                      F32     dY_per_ms,
                                                      F32     dZ_per_ms)
{
   START
      {
      AIL_debug_log("@AIL_set_listener_3D_velocity_vector(" PTRFMT ",%f,%f,%f)" ,
          dig,dX_per_ms,dY_per_ms,dZ_per_ms);
      }

   IN_AIL;

   AIL_API_set_listener_3D_velocity_vector(dig,dX_per_ms,dY_per_ms,dZ_per_ms);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_volume_falloff(HSAMPLE S, MSSGRAPHPOINT* graph, S32 pointcount)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_volume_falloff(" PTRFMT "," PTRFMT ",%d)" ,
          S, graph, pointcount);
      }

   IN_AIL;

   AIL_API_set_sample_3D_volume_falloff(S, graph, pointcount);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_lowpass_falloff(HSAMPLE S, MSSGRAPHPOINT* graph, S32 pointcount)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_lowpass_falloff(" PTRFMT "," PTRFMT ",%d)" ,
          S, graph, pointcount);
      }

   IN_AIL;

   AIL_API_set_sample_3D_lowpass_falloff(S, graph, pointcount);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_exclusion_falloff(HSAMPLE S, MSSGRAPHPOINT* graph, S32 pointcount)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_exclusion_falloff(" PTRFMT "," PTRFMT ",%d)" ,
          S, graph, pointcount);
      }

   IN_AIL;

   AIL_API_set_sample_3D_exclusion_falloff(S, graph, pointcount);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_spread_falloff(HSAMPLE S, MSSGRAPHPOINT* graph, S32 pointcount)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_spread_falloff(" PTRFMT "," PTRFMT ",%d)" ,
          S, graph, pointcount);
      }

   IN_AIL;

   AIL_API_set_sample_3D_spread_falloff(S, graph, pointcount);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_spread(HSAMPLE S, F32 spread)
{
   START
      {
      AIL_debug_log("@AIL_set_sample_3D_spread(" PTRFMT ",%f)" ,
          S, spread);
      }

   IN_AIL;

   AIL_API_set_sample_3D_spread(S, spread);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_sample_3D_position_segments(HSAMPLE S, MSSVECTOR3D* points, S32 point_count)
{
    START
    {
        AIL_debug_log("@AIL_set_sample_3D_position_segments(" PTRFMT "," PTRFMT ",%d)", S, points, point_count);
    }

    IN_AIL;

    AIL_API_set_sample_3D_position_segments(S, points, point_count);

    OUT_AIL;

    END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_set_listener_3D_orientation      (HDIGDRIVER dig, //)
                                                      F32     X_face,
                                                      F32     Y_face,
                                                      F32     Z_face,
                                                      F32     X_up,
                                                      F32     Y_up,
                                                      F32     Z_up)
{
   START
      {
      AIL_debug_log("@AIL_set_listener_3D_orientation(" PTRFMT ",%f,%f,%f,%f,%f,%f)" ,
          dig,X_face,Y_face,Z_face,X_up,Y_up,Z_up);
      }

   IN_AIL;

   AIL_API_set_listener_3D_orientation(dig,X_face,Y_face,Z_face,X_up,Y_up,Z_up);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_listener_3D_position             (HDIGDRIVER  dig, //)
                                                      F32 *X,
                                                      F32 *Y,
                                                      F32 *Z)
{
   START
      {
      AIL_debug_log("@AIL_listener_3D_position(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
          dig,X,Y,Z);
      }

   IN_AIL_NM;

   AIL_API_listener_3D_position(dig,X,Y,Z);

   OUT_AIL_NM;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_listener_3D_velocity             (HDIGDRIVER  dig, //)
                                                      F32 *dX_per_ms,
                                                      F32 *dY_per_ms,
                                                      F32 *dZ_per_ms)
{
   START
      {
      AIL_debug_log("@AIL_listener_3D_velocity(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
          dig,dX_per_ms,dY_per_ms,dZ_per_ms);
      }

   IN_AIL_NM;

   AIL_API_listener_3D_velocity(dig,dX_per_ms,dY_per_ms,dZ_per_ms);

   OUT_AIL_NM;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_listener_3D_orientation          (HDIGDRIVER  dig, //)
                                                      F32 *X_face,
                                                      F32 *Y_face,
                                                      F32 *Z_face,
                                                      F32 *X_up,
                                                      F32 *Y_up,
                                                      F32 *Z_up)
{
   START
      {
      AIL_debug_log("@AIL_listener_3D_orientation(" PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
          dig,X_face,Y_face,Z_face,X_up,Y_up,Z_up);
      }

   IN_AIL_NM;

   AIL_API_listener_3D_orientation(dig,X_face,Y_face,Z_face,X_up,Y_up,Z_up);

   OUT_AIL_NM;

   END;
}

//############################################################################
DXDEF void AILEXPORT     AIL_update_listener_3D_position      (HDIGDRIVER dig, //)
                                                      F32     dt_milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_update_listener_3D_position(" PTRFMT ",%f)" ,
          dig,dt_milliseconds);
      }

   IN_AIL;

   AIL_API_update_listener_3D_position(dig, dt_milliseconds);

   OUT_AIL;

   END;
}

#ifdef MSS_FLT_SUPPORTED

//############################################################################
DXDEF HPROVIDER AILEXPORT AIL_digital_output_filter (HDIGDRIVER dig)
{
   HPROVIDER result;

   START
      {
      AIL_debug_log("@AIL_digital_output_filter(" PTRFMT ")" ,
         dig);
      }

   IN_AIL;

#ifdef MSS_VFLT_SUPPORTED
   if (dig->voice_filter != NULL)
      {
      result = dig->voice_filter->provider;     // e.g., EAX
      }
   else
#endif
   if (dig->matrix_filter != NULL)
      {
      result = dig->matrix_filter->provider;    // e.g., Dolby Surround
      }
   else
      {
      result = 0;
      }

   OUT_AIL;

   RESULT
      {
      outreshex((U32) result);
      }

   END;

   return result;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_enumerate_filters (HMSSENUM  *next, //)
                                           HPROVIDER *dest,
                                           C8  * *name)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_enumerate_filters(" PTRFMT "," PTRFMT "," PTRFMT ")" ,
         next,dest,name);
      }

   IN_AIL_NM;

   result = AIL_API_enumerate_filters(next,dest,name);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF HDRIVERSTATE AILEXPORT AIL_open_filter        (HPROVIDER  provider, //)
                                                     HDIGDRIVER dig)
{
   HDRIVERSTATE result;

   START
      {
      AIL_debug_log("@AIL_open_filter(" HPFMT "," PTRFMT ")" ,
         provider,dig);
      }

   IN_AIL;

   result = AIL_API_open_filter(provider,dig);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF void  AILEXPORT    AIL_close_filter       (HDRIVERSTATE filter)
{
   START
      {
      AIL_debug_log("@AIL_close_filter(" PTRFMT ")" ,
         filter);
      }

   IN_AIL;

   AIL_API_close_filter(filter);

   OUT_AIL;

   END;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_enumerate_filter_properties(HPROVIDER                  lib,   //)
                                                    HMSSENUM *             next,
                                                    RIB_INTERFACE_ENTRY *  dest)
{
   S32 result = 0;

   START
      {
      AIL_debug_log("@AIL_enumerate_filter_properties(" HPFMT "," PTRFMT "," PTRFMT ")" ,
         lib,next,dest);
      }

   IN_AIL_NM;

   if (RIB_request_interface(lib,
                            "MSS pipeline filter",
                             0,
                             NULL) == RIB_NOERR)
      {
      result = RIB_enumerate_interface(lib,
                                      "MSS pipeline filter",
                                       RIB_PROPERTY,
                                       next,
                                       dest);
      }
   else if (RIB_request_interface(lib,
                                 "MSS voice filter",
                                  0,
                                  NULL) == RIB_NOERR)
      {
      result = RIB_enumerate_interface(lib,
                                      "MSS voice filter",
                                       RIB_PROPERTY,
                                       next,
                                       dest);

      }

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_enumerate_filter_sample_properties(HPROVIDER                  lib,   //)
                                                           HMSSENUM *             next,
                                                           RIB_INTERFACE_ENTRY *  dest)
{
   S32 result = 0;

   START
      {
      AIL_debug_log("@AIL_enumerate_filter_sample_properties(" HPFMT "," PTRFMT "," PTRFMT ")" ,
         lib,next,dest);
      }

   IN_AIL_NM;

   if (RIB_request_interface(lib,
                            "Pipeline filter sample services",
                             0,
                             NULL) == RIB_NOERR)
      {
      result = RIB_enumerate_interface(lib,
                                      "Pipeline filter sample services",
                                       RIB_PROPERTY,
                                       next,
                                       dest);
      }
   else if (RIB_request_interface(lib,
                                 "Voice filter sample services",
                                  0,
                                  NULL) == RIB_NOERR)
      {
      result = RIB_enumerate_interface(lib,
                                      "Voice filter sample services",
                                       RIB_PROPERTY,
                                       next,
                                       dest);

      }

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_enumerate_output_filter_driver_properties(HPROVIDER                  lib,   //)
                                                                  HMSSENUM *             next,
                                                                  RIB_INTERFACE_ENTRY *  dest)
{
   S32 result = 0;

   START
      {
      AIL_debug_log("@AIL_enumerate_output_filter_driver_properties(" HPFMT "," PTRFMT "," PTRFMT ")" ,
         lib,next,dest);
      }

   IN_AIL_NM;

   if (RIB_request_interface(lib,
                            "Voice filter driver services",
                             0,
                             NULL) == RIB_NOERR)
      {
      result = RIB_enumerate_interface(lib,
                                      "Voice filter driver services",
                                       RIB_PROPERTY,
                                       next,
                                       dest);

      }
   else if (RIB_request_interface(lib,
                                 "Pipeline filter driver services",
                                  0,
                                  NULL) == RIB_NOERR)
      {
      result = RIB_enumerate_interface(lib,
                                      "Pipeline filter driver services",
                                       RIB_PROPERTY,
                                       next,
                                       dest);
      }

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_enumerate_output_filter_sample_properties(HPROVIDER                  lib,   //)
                                                                  HMSSENUM *             next,
                                                                  RIB_INTERFACE_ENTRY *  dest)
{
   S32 result = 0;

   START
      {
      AIL_debug_log("@AIL_enumerate_output_filter_sample_properties(" HPFMT "," PTRFMT "," PTRFMT ")" ,
         lib,next,dest);
      }

   IN_AIL_NM;

   if (RIB_request_interface(lib,
                            "Voice filter sample services",
                             0,
                             NULL) == RIB_NOERR)
      {
      result = RIB_enumerate_interface(lib,
                                      "Voice filter sample services",
                                       RIB_PROPERTY,
                                       next,
                                       dest);

      }
   else if (RIB_request_interface(lib,
                                 "Pipeline filter sample services",
                                  0,
                                  NULL) == RIB_NOERR)
      {
      result = RIB_enumerate_interface(lib,
                                      "Pipeline filter sample services",
                                       RIB_PROPERTY,
                                       next,
                                       dest);

      }

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_filter_property(HPROVIDER   lib,  //)
                                         C8 const *    name,
                                         void *       before_value,
                                         void const * new_value,
                                         void *       after_value
                                         )
{
   S32 result = 0;
   UINTa token;
   union
   {
     UINTa token;
     PROVIDER_PROPERTY property;
   } un = {0};

   START
      {
      AIL_debug_log("@AIL_filter_property(" HPFMT ",%s," PTRFMT "," PTRFMT "," PTRFMT ")" ,
         lib,name,before_value,new_value,after_value);
      }

   IN_AIL_NM;

   if (RIB_request_interface_entry(lib,
                                  "MSS pipeline filter",
                                   RIB_FUNCTION,
                                  "PROVIDER_property",
                      (UINTa *)&un.token) == RIB_NOERR)
      {
      if (RIB_request_interface_entry(lib,
                                     "MSS pipeline filter",
                                      RIB_PROPERTY,
                                      name,
                                     &token) == RIB_NOERR)
         {
         result = un.property( token, before_value,new_value,after_value );
         }
      }
   else if (RIB_request_interface_entry(lib,
                                       "MSS voice filter",
                                        RIB_FUNCTION,
                                       "PROVIDER_property",
                           (UINTa *)&un.token) == RIB_NOERR)
      {
      if (RIB_request_interface_entry(lib,
                                     "MSS voice filter",
                                      RIB_PROPERTY,
                                      name,
                                     &token) == RIB_NOERR)
         {
         result = un.property( token, before_value,new_value,after_value );
         }
      }

   OUT_AIL_NM;

   END;

   return result;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_enumerate_sample_stage_properties(HSAMPLE                    samp,  //)
                                                          SAMPLESTAGE                stage,
                                                          HMSSENUM *             next,
                                                          RIB_INTERFACE_ENTRY *  dest)
{
   S32 result = 0;

   START
      {
      AIL_debug_log("@AIL_enumerate_sample_stage_properties(" PTRFMT ",%ld," PTRFMT "," PTRFMT ")" ,
         samp,stage,next,dest);
      }

   IN_AIL_NM;

   if (samp != NULL)
      {
      if ((stage >= SP_FILTER_0) && (stage <= SP_FILTER_7))
         {
         if (samp->pipeline[stage].active)
            {
            result = RIB_enumerate_interface(samp->pipeline[stage].provider,
                                            "Pipeline filter sample services",
                                             RIB_PROPERTY,
                                             next,
                                             dest);
            }
         }
#ifdef MSS_VFLT_SUPPORTED
      else
         {
         //
         // Note that matrix postmix filters can't have any per-sample attributes...
         //

         if (samp->driver->voice_filter != NULL)
            {
            result = RIB_enumerate_interface(samp->driver->voice_filter->provider,
                                            "Voice filter sample services",
                                             RIB_PROPERTY,
                                             next,
                                             dest);
            }
         }
#endif
      }

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF S32 AILEXPORT AIL_sample_stage_property  (HSAMPLE        samp, //)
                                                SAMPLESTAGE    stage,
                                                C8 const * name,
                                                S32 /*-1 or MSS_SPEAKER*/ input_channel,
                                                void *       before_value,
                                                void const * new_value,
                                                void *       after_value
                                                )
{
   UINTa token;
   S32 result = 0;

   START
      {
      AIL_debug_log("@AIL_sample_stage_property(" PTRFMT ",%ld,%s,%ld," PTRFMT "," PTRFMT "," PTRFMT ")" ,
         samp,stage,name,input_channel,before_value,new_value,after_value);
      }

   IN_AIL;

   if (samp != NULL)
      {
      if ((stage >= SP_FILTER_0) && (stage <= SP_FILTER_7))
         {
         if (samp->pipeline[stage].active)
            {
            FLTPROVIDER *provptr = samp->pipeline[stage].TYPE.FLT.provider;

            if (provptr != NULL)
               {
               FLTSMP_SAMPLE_PROPERTY sample_property = provptr->sample_property;

               if (sample_property != NULL)
                  {
                  if (RIB_request_interface_entry(samp->pipeline[stage].provider,
                                                 "Pipeline filter sample services",
                                                  RIB_PROPERTY,
                                                  name,
                                                 &token) == RIB_NOERR)
                     {
                       if ( input_channel == -1 )
                       {
                         int i;
                         for( i = 0 ; i < samp->n_channels ; i++ )
                           result = sample_property( samp->pipeline[stage].TYPE.FLT.sample_state[i],token, before_value,new_value,after_value );
                       }
                       else
                       {
                         S32 s;
                         s = samp->speaker_enum_to_source_chan[input_channel];
                         if ( s != -1 )
                           result = sample_property( samp->pipeline[stage].TYPE.FLT.sample_state[s],token, before_value,new_value,after_value );
                       }
                     }
                  }
               }
            }
         }
#ifdef MSS_VFLT_SUPPORTED
      else
         {
         //
         // Note that matrix postmix filters can't have any per-sample attributes...
         //

         if (samp->driver->voice_filter != NULL)
            {
            if (RIB_request_interface_entry(samp->driver->voice_filter->provider,
                                           "Voice filter sample services",
                                            RIB_PROPERTY,
                                            name,
                                           &token) == RIB_NOERR)
               {
                result = samp->driver->voice_filter->output_sample_property( samp,token, before_value,new_value,after_value );
               }
            }
         }
#endif
      }

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return( result );
}

//############################################################################
DXDEF S32 AILEXPORT AIL_output_filter_driver_property (HDIGDRIVER     dig, //)
                                                       C8 const * name,
                                                       void *       before_value,
                                                       void const * new_value,
                                                       void *       after_value
                                                       )
{
   S32 result = 0;
   UINTa token;

   START
      {
      AIL_debug_log("@AIL_output_filter_driver_property(" PTRFMT ",%s," PTRFMT "," PTRFMT "," PTRFMT ")" ,
         dig,name,before_value,new_value,after_value);
      }

   IN_AIL_NM;

   if (dig != NULL)
      {
#ifdef MSS_VFLT_SUPPORTED
      if (dig->voice_filter != NULL)
         {
         if (RIB_request_interface_entry(dig->voice_filter->provider,
                                        "Voice filter driver services",
                                         RIB_PROPERTY,
                                         name,
                                        &token) == RIB_NOERR)
            {
            result = dig->voice_filter->driver_property( dig->voice_filter->driver_state,token, before_value,new_value,after_value );
            }
         }
      else
#endif
      if (dig->matrix_filter != NULL)
         {
         if (RIB_request_interface_entry(dig->matrix_filter->provider,
                                        "Pipeline filter driver services",
                                         RIB_PROPERTY,
                                         name,
                                        &token) == RIB_NOERR)
            {
            result = dig->matrix_filter->driver_property( dig->matrix_filter->driver_state,token, before_value,new_value,after_value );
            }
         }
      }

   OUT_AIL_NM;

   END;

   return result;
}

#endif

//############################################################################
DXDEF
S32 AILEXPORT AIL_size_processed_digital_audio(
                                 U32             dest_rate,
                                 U32             dest_format,
                                 S32             num_srcs,
                                 AILMIXINFO const * src)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_size_processed_digital_audio(%li,%li,%li," PTRFMT ")" ,
          dest_rate,dest_format,num_srcs,src);
      }

   IN_AIL_NM;

   result = AIL_API_size_processed_digital_audio(dest_rate,dest_format,num_srcs,src);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
DXDEF
S32 AILEXPORT AIL_process_digital_audio(
                                 void       *dest_buffer,
                                 S32             dest_buffer_size,
                                 U32             dest_rate,
                                 U32             dest_format,
                                 S32             num_srcs,
                                 AILMIXINFO * src)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_process_digital_audio(" PTRFMT ",%li,%li,%li,%li," PTRFMT ")" ,
          dest_buffer,dest_buffer_size,dest_rate,dest_format,num_srcs,src);
      }

   IN_AIL_NM;

   result = AIL_API_process_digital_audio(dest_buffer,dest_buffer_size,dest_rate,dest_format,num_srcs,src);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################
//##                                                                        ##
//## AIL_MMX_available()                                                    ##
//##                                                                        ##
//############################################################################

#ifdef IS_X86

DXDEF U32 AILEXPORT AIL_MMX_available(void)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_MMX_available()" );
      }

   IN_AIL_NM;

   if (!AIL_get_preference(AIL_ENABLE_MMX_SUPPORT))
      {
      result = 0;
      }
   else
      {
      result = MSS_MMX_available();
      }

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

#endif


//
// this functions are for older apps that are linking right to the old 
//   non-tracked functions
//

#ifdef AIL_mem_alloc_lock
#undef AIL_mem_alloc_lock
#endif

DXDEC
void * AILCALL AIL_mem_alloc_lock( UINTa size );

DXDEF
void * AILEXPORT AIL_mem_alloc_lock( UINTa size )
{
  return AIL_mem_alloc_lock_trk( size );
}

#ifdef AIL_file_read
#undef AIL_file_read
#endif

DXDEC
void * AILCALL AIL_file_read( char const * filename, void * dest );

DXDEF
void * AILEXPORT AIL_file_read( char const * filename, void * dest )
{
  return AIL_file_read_trk( filename, dest );
}

#ifdef AIL_file_size
#undef AIL_file_size
#endif

DXDEC
S32 AILCALL AIL_file_size( char const * filename );

DXDEF
S32 AILEXPORT AIL_file_size( char const * filename )
{
  return AIL_file_size_trk( filename );
}


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
#include "rrThreads.h"
#include "rrAtomics.h"
#include "rrtime.h"

#ifdef RR_CAN_ALLOC_THREAD_STACK
#   define alloc_rr_thread( v_rr_thread, size ) { p##v_rr_thread = (rrThread*) AIL_mem_alloc_lock( sizeof( rrThread ) + size ); }
#   define dealloc_rr_thread( v_rr_thread ) { AIL_mem_free_lock( p##v_rr_thread ); p##v_rr_thread = 0; }
#   define THREADDEFAULT rrThreadCreateDefault|rrThreadStackAfterThread
#else
#   define alloc_rr_thread( v_rr_thread, size ) 
#   define dealloc_rr_thread( v_rr_thread ) 
#   define THREADDEFAULT rrThreadCreateDefault
#endif

//
// ASCII error type string
//

char AIL_error[256];

//
// DIG_DRIVER list
//

HDIGDRIVER DIG_first = NULL;

//
// Timer array
//

static struct _AILTIMER *timers = NULL;
static S32 n_timers = 0;

static S32 miles_thread_init = 0;
static S32 miles_mutex_init = 0;

#ifdef RR_CAN_ALLOC_THREAD_STACK
static rrThread * ps_TimerThread;
#define s_TimerThread (*ps_TimerThread)
#else
static rrThread s_TimerThread;
#endif

static rrMutex  s_MilesMutex;
static rrSemaphore s_CloseSem;

extern S32 AILCALL MilesAsyncStartup();
extern S32 AILCALL MilesAsyncShutdown();

#ifdef _DEBUG
static U32 volatile debug_mutex;
#endif

void AILCALL InMilesMutex()
{
#ifdef _DEBUG
    rrAtomicAddExchange32(&debug_mutex, 1);
#endif
    if ( miles_mutex_init )
    {
       if (rrMutexLockTimeout(&s_MilesMutex, 0) == 0 )
       {
         tmEnter( miles_context, TMZF_STALL, "Miles mutex block" );
         rrMutexLock(&s_MilesMutex);
         tmLeave( miles_context );
       }
       tmSetLockState(miles_context,&s_MilesMutex,TMLS_LOCKED,"Miles mutex");
    }
}

void AILCALL OutMilesMutex()
{
    if ( miles_mutex_init )
    {
        tmSetLockState(miles_context,&s_MilesMutex,TMLS_RELEASED,"Miles mutex");
        rrMutexUnlock(&s_MilesMutex);
    }
#ifdef _DEBUG
    rrAtomicAddExchange32(&debug_mutex, -1);
    if ( ( (S32) debug_mutex ) < 0 )
      RR_BREAK();
#endif
}

//############################################################################
//##                                                                        ##
//## Initialize AIL API modules and resources                               ##
//##                                                                        ##
//## Must be called prior to any other AIL_...() calls!                     ##
//##                                                                        ##
//############################################################################

static void init_preferences()
{
  AIL_preference[AIL_MM_PERIOD]= DEFAULT_AMP;
  AIL_preference[AIL_TIMERS]= DEFAULT_AT;
  AIL_preference[AIL_ENABLE_MMX_SUPPORT]= DEFAULT_AEMS;

  AIL_preference[DIG_MIXER_CHANNELS]= DEFAULT_DMC;
  AIL_preference[DIG_ENABLE_RESAMPLE_FILTER]= DEFAULT_DERF;
  AIL_preference[DIG_RESAMPLING_TOLERANCE]= DEFAULT_DRT;
  AIL_preference[DIG_DS_FRAGMENT_SIZE]= DEFAULT_DDFS;
  AIL_preference[DIG_DS_FRAGMENT_CNT]= DEFAULT_DDFC;
  AIL_preference[DIG_DS_MIX_FRAGMENT_CNT]= DEFAULT_DDMFC;
  AIL_preference[DIG_LEVEL_RAMP_SAMPLES]= DEFAULT_DLRS;
  AIL_preference[DIG_MAX_PREDELAY_MS]= DEFAULT_MPDMS;
  AIL_preference[DIG_3D_MUTE_AT_MAX]= DEFAULT_D3MAM;
  AIL_preference[DIG_DS_USE_PRIMARY]= DEFAULT_DDUP;
  AIL_preference[DIG_DS_DSBCAPS_CTRL3D]= DEFAULT_DDDC;
  AIL_preference[DIG_DS_CREATION_HANDLER]= DEFAULT_DDCH;
  AIL_preference[DIG_MAX_CHAIN_ELEMENT_SIZE]= DEFAULT_MCES;
  AIL_preference[DIG_MIN_CHAIN_ELEMENT_TIME]= DEFAULT_MCET;
  AIL_preference[DIG_USE_WAVEOUT]= DEFAULT_DUW;
  AIL_preference[DIG_OUTPUT_BUFFER_SIZE]= DEFAULT_DOBS;
  AIL_preference[DIG_PREFERRED_WO_DEVICE]= DEFAULT_DPWOD;
  AIL_preference[DIG_PREFERRED_DS_DEVICE]= DEFAULT_DPDSD;

  AIL_preference[MDI_SEQUENCES]= DEFAULT_MS;
  AIL_preference[MDI_SERVICE_RATE]= DEFAULT_MSR;
  AIL_preference[MDI_DEFAULT_VOLUME]= DEFAULT_MDV;
  AIL_preference[MDI_QUANT_ADVANCE]= DEFAULT_MQA;
  AIL_preference[MDI_ALLOW_LOOP_BRANCHING]= DEFAULT_ALB;
  AIL_preference[MDI_DEFAULT_BEND_RANGE]= DEFAULT_MDBR;
  AIL_preference[MDI_DOUBLE_NOTE_OFF]= DEFAULT_MDNO;
  AIL_preference[MDI_SYSEX_BUFFER_SIZE]= DEFAULT_MSBS;

  AIL_preference[DLS_VOICE_LIMIT]= DEFAULT_DVL;
  AIL_preference[DLS_TIMEBASE]= DEFAULT_DTB;
  AIL_preference[DLS_BANK_SELECT_ALIAS]= DEFAULT_DBSA;
  AIL_preference[DLS_STREAM_BOOTSTRAP]= DEFAULT_DSB;
  AIL_preference[DLS_VOLUME_BOOST]= DEFAULT_DVB;
  AIL_preference[DLS_ENABLE_FILTERING]= DEFAULT_DEF;
  AIL_preference[DLS_GM_PASSTHROUGH]= DEFAULT_DGP;
  AIL_preference[DLS_ADPCM_TO_ASI_THRESHOLD]= DEFAULT_DATAT;

#ifdef __RAD3DS__
  AIL_preference[AIL_3DS_USE_SYSTEM_CORE] = 0;
#endif
}


extern "C" void GetExeDirectory();

void AILCALL AIL_API_startup(void)
{
    init_preferences();

#if !defined(IS_IPHONE) && (defined(IS_MAC) || defined(IS_LINUX))
    //load directory for mac and linux
    GetExeDirectory();
#endif

    AIL_error[0] = 0;

    // Make sure our time base gets initialized, if it's dynamic
    rrGetTime();
        
    miles_mutex_init = 1;
    rrMutexCreate(&s_MilesMutex, rrMutexNeedFullTimeouts);
    tmLockName( miles_context, &s_MilesMutex, "Miles API mutex" );

    //
    // Load the registered static providers
    //

#ifndef STANDALONEMIXRIB
    RIB_load_static_provider_library(MIX_RIB_MAIN,"Miles Mixer");
#endif

    //
    // Load and initialize external MSS RIBs
    //

    HPROVIDER PROVIDER;
    HMSSENUM  next;

    //
    // Load and start all available mixer providers
    //
#ifndef IS_STATIC
    RIB_load_application_providers("*.mix");
    RIB_load_application_providers("*.asi");
    RIB_load_application_providers("*.flt");
#endif

    next = MSS_FIRST;

    while (RIB_enumerate_providers("MSS mixer services",
        &next,
        &PROVIDER))
    {
        MIXER_STARTUP MIXER_startup;

        if (RIB_request_interface_entry(PROVIDER,
            "MSS mixer services",
            RIB_FUNCTION,
            "MIXER_startup",
            (UINTa *) &MIXER_startup) == RIB_NOERR)
        {
            MIXER_startup();
        }
    }

    //
    // Load and start all available ASI providers
    //


    next = MSS_FIRST;

    while (RIB_enumerate_providers("ASI codec",
        &next,
        &PROVIDER))
    {
        ASI_STARTUP ASI_startup;

        if (RIB_request_interface_entry(PROVIDER,
            "ASI codec",
            RIB_FUNCTION,
            "ASI_startup",
            (UINTa  *) &ASI_startup) == RIB_NOERR)
        {
            ASIRESULT result = ASI_startup();

            if ((result != ASI_NOERR) && (result != ASI_ALREADY_STARTED))
            {
                //
                // ASI providers can fail to start up if they depend on 
                // missing hardware features such as SSE
                //
                // (OK to free RIB providers during enumeration)
                //

                RIB_free_provider_library(PROVIDER);
            }
        }
    }

    //
    // Load and start all available FLT providers
    //

    FLT_init_list();    // Initialize driver-association list so we can clean up at shutdown time

    next = MSS_FIRST;

    while (RIB_enumerate_providers("MSS pipeline filter",
        &next,
        &PROVIDER))
    {
        FLT_STARTUP FLT_startup;

        if (RIB_request_interface_entry(PROVIDER,
            "MSS pipeline filter",
            RIB_FUNCTION,
            "FLT_startup",
            (UINTa  *) &FLT_startup) == RIB_NOERR)
        {
            FLTRESULT result = FLT_startup();

            if ((result != FLT_NOERR) && (result != FLT_ALREADY_STARTED))
            {
                //
                // FLT providers can fail to start up if they depend on 
                // missing hardware features such as SSE
                //
                // (OK to free RIB providers during enumeration)
                //

                RIB_free_provider_library(PROVIDER);
            }
        }
    }

    MilesAsyncStartup();
}

//############################################################################
//##                                                                        ##
//## Background timer server                                                ##
//##                                                                        ##
//############################################################################

static U64 last_api_timer = 0;
static U64 highest_timer_delay = 0;
static U64 mixer_time = 0;
static U64 last_polled;
static U32 last_percent;

S32 AILCALL AIL_API_background_CPU_percent(void)
{
  U64 time;
  U64 diff;
  U32 perc;

  time = rrGetTime();

  diff = time - last_polled;

  if ( diff < rrMillisToTime( 150 ) )
    return(last_percent);

  last_polled = time;

  perc = (U32) ( ( mixer_time * 100 ) / diff );
  
  mixer_time = 0;
  
  if ( perc > 100 )
    perc = 100;
  
  last_percent = perc;

  return perc;
}

static volatile U32 timer_busy = 0;

static void API_timer ()
{
    S32 i;
    U64 diff;
    U64 end_time;
    U64 start_time;

    //
    // If timer services uninitialized or locked, or reentry attempted, exit
    //

    rrAtomicAddExchange32(&timer_busy, 1);

    if ((timers == NULL) || (timer_busy!=1))
    {
        goto resumethreadandexit;
    }

    //
    // Mark start of background processing
    //

    start_time = rrGetTime();

    // keep track of the longest delay
    if ( last_api_timer ) 
    {
      diff = start_time - last_api_timer;
      if ( diff > highest_timer_delay )
         highest_timer_delay = diff;
    }

    last_api_timer = start_time;

    //
    // Advance all running timers
    //

    for (i=0; i < n_timers; i++)
    {
        //
        // Skip timer if not active
        //

        if (timers[i].status != AILT_RUNNING)
        {
            continue;
        }

        //
        // If elapsed time >= timer's period, reset timer and
        // trigger callback function
        //

        while ( start_time >= timers[i].next )
        {
            timers[i].next += timers[i].delta;

            //
            // Invoke timer callback function with specified user value
            //

            MSS_do_cb1( (AILTIMERCB), timers[i].callback, timers[i].callingDS, timers[i].user);

            // check again, in case they canceled the time in the background
            if (timers[i].status != AILT_RUNNING)
            {
                break;
            }

        }
    }


    //
    // Maintain profiling information
    //

    end_time = rrGetTime();

    diff = end_time - start_time;

    mixer_time += diff;

    //
    // Resume application thread ("interrupt" handling finished)
    //

resumethreadandexit:

    //
    // Enable future timer calls
    //

    rrAtomicAddExchange32(&timer_busy, -1);
}


//############################################################################
//##                                                                        ##
//## Thread to call timer services                                          ##
//##                                                                        ##
//############################################################################

#ifdef IS_X86
#include <xmmintrin.h>
#endif

static U32 RADLINK ThreadProc(void* i_Param)
{
  tmThreadName( miles_context, 0, "Miles Timer" );
  
  #if defined(IS_X86)

    int oldMXCSR = _mm_getcsr(); 
    int newMXCSR = oldMXCSR | ( ( MSS_MMX_available() & 8 ) ? 0x8040 : 0x8000 ); 
    _mm_setcsr( newMXCSR );

  #endif

  for(;;)
  {
     tmEnter( miles_context, TMZF_IDLE, "Timer waiting" );
     if ( rrSemaphoreDecrementOrWait(&s_CloseSem, (U32)AIL_preference[AIL_MM_PERIOD]) )
     {
       tmLeave( miles_context );
       break;
     }
     tmLeave( miles_context );

     if (rrMutexLockTimeout(&s_MilesMutex, 0) == 0 )
     {
       tmEnter( miles_context, TMZF_IDLE, "Miles mutex block" );
       if ( rrMutexLockTimeout(&s_MilesMutex, 10) )
       {
         tmLeave( miles_context );
         goto gotit;
       }
       tmLeave( miles_context );
     }
     else
     {
      gotit:
       tmSetLockState(miles_context,&s_MilesMutex,TMLS_LOCKED,"Miles mutex");
       tmEnter( miles_context, TMZF_NONE, "Miles background processing" );
       API_timer();
       tmLeave( miles_context );
       tmSetLockState(miles_context,&s_MilesMutex,TMLS_RELEASED,"Miles mutex");
       rrMutexUnlock(&s_MilesMutex);
     }
  }

  return(0);
}

//############################################################################
//##                                                                        ##
//## Shut down AIL API modules and resources                                ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_shutdown(void)
{
    HDIGDRIVER dig,d;
    S32 i;

    // Shutdown async services.
    MilesAsyncShutdown();

    //
    // Shut down any active wave drivers
    //

    InMilesMutex();

    dig = DIG_first;

    while (dig != NULL)
    {
        d = dig->next;

        //! \todo OutMilesMutex here?
        AIL_close_digital_driver( dig );

        dig = d;
    }

    HMSSENUM  next;
    HPROVIDER PROVIDER;

    //
    // Shut down all FLT providers
    //

    next     = MSS_FIRST;
    PROVIDER = 0;

    while (RIB_enumerate_providers("MSS pipeline filter",
        &next,
        &PROVIDER))
    {
        FLT_SHUTDOWN FLT_shutdown;

        if (RIB_request_interface_entry(PROVIDER,
            "MSS pipeline filter",
            RIB_FUNCTION,
            "FLT_shutdown",
            (UINTa  *) &FLT_shutdown) == RIB_NOERR)
        {
            FLT_shutdown();
        }
    }

    //
    // Shut down all ASI providers
    //

    next = MSS_FIRST;

    while (RIB_enumerate_providers("ASI codec",
        &next,
        &PROVIDER))
    {
        ASI_SHUTDOWN ASI_shutdown;

        if (RIB_request_interface_entry(PROVIDER,
            "ASI codec",
            RIB_FUNCTION,
            "ASI_shutdown",
            (UINTa  *) &ASI_shutdown) == RIB_NOERR)
        {
            ASI_shutdown();
        }
    }

    //
    // Shut down all MIXER providers
    //

    next = MSS_FIRST;

    while (RIB_enumerate_providers("MSS mixer services",
        &next,
        &PROVIDER))
    {
        MIXER_SHUTDOWN MIXER_shutdown;

        if (RIB_request_interface_entry(PROVIDER,
            "MSS mixer services",
            RIB_FUNCTION,
            "MIXER_shutdown",
            (UINTa  *) &MIXER_shutdown) == RIB_NOERR)
        {
            MIXER_shutdown();
        }
    }

    //
    // Release all loaded libraries
    //

    RIB_free_libraries();

    //
    // Shut down timer services
    //

    if (timers != NULL)
    {
        AIL_release_all_timers();

        for (i=0; i < n_timers; i++)           // don't free the timer system
            if (timers[i].status != AILT_FREE)   // if any timers remain
                goto done;

        rrSemaphoreIncrement(&s_CloseSem, 1);
        rrThreadWaitDone(&s_TimerThread,RR_WAIT_INFINITE,0);
        rrThreadCleanUp(&s_TimerThread);
        dealloc_rr_thread(s_TimerThread);
        miles_thread_init = 1;

        //
        // Free timer array
        //

        AIL_mem_free_lock(timers);

        timers = NULL;

    }

done:
    OutMilesMutex();

    if ( miles_mutex_init )
    {
      miles_mutex_init = 0;
      rrMutexDestroy(&s_MilesMutex);
    }
}

//###########################################################################
//##                                                                       ##
//## Lock/unlock AIL timer service (to enable atomic operations)           ##
//##                                                                       ##
//###########################################################################

DXDEF
void    AILEXPORT AIL_lock_mutex       (void)
{
    InMilesMutex();
}

DXDEF
void    AILEXPORT AIL_unlock_mutex     (void)
{
    OutMilesMutex();
}

//############################################################################
//##                                                                        ##
//## System-independent delay in ms                                         ##
//##                                                                        ##
//## Returns at once if called from background                              ##
//##                                                                        ##
//############################################################################

void    AILCALL AIL_API_sleep(U32 ms)
{
    if (AIL_API_background())
    {
        return;
    }

    rrThreadSleep( ms );
}

//############################################################################
//##                                                                        ##
//## Returns TRUE if called from within timer handler or callback function  ##
//##                                                                        ##
//############################################################################

S32     AILCALL AIL_API_background                (void)
{
  if ( miles_thread_init )
  {
    rrThread * cur = rrThreadCurrent();
    if ( cur )
    {
      return ( cur == &s_TimerThread ) ? 1 : 0;
    }
  }

  return 0;
}


//############################################################################
//##                                                                        ##
//## Register a timer                                                       ##
//##                                                                        ##
//############################################################################

HTIMER  AILCALL AIL_API_register_timer            (AILTIMERCB    fn)
{
    S32 i;

    //
    // If timer array has not yet been allocated, allocate and initialize it
    //

    InMilesMutex();

    if (timers == NULL)
    {
        if ( miles_mutex_init == 0 )
          return -1;

        n_timers = (S32) AIL_preference[AIL_TIMERS];

        timers = (struct _AILTIMER  *) AIL_mem_alloc_lock(n_timers * sizeof(struct _AILTIMER));

        if (timers == NULL)
        {
            return -1;
        }

        //
        // Mark all timers free
        //

        for (i=0; i < n_timers; i++)
        {
            timers[i].status = AILT_FREE;

        }

        OutMilesMutex();

        U32 thread_flags = THREADDEFAULT;
#ifdef __RAD3DS__
        if (AIL_preference[AIL_3DS_USE_SYSTEM_CORE])
          thread_flags |= rrThreadCreate3DSSystemCore;
#endif
        rrSemaphoreCreate(&s_CloseSem, 0, 1);
        alloc_rr_thread( s_TimerThread, 80*1024 );
        rrThreadCreate(&s_TimerThread, ThreadProc, 80*1024, 0, thread_flags, "MSSTimer");
        rrThreadSetPriority(&s_TimerThread, rrThreadOneHigherThanCurrent);
        rrThreadSetPriority(&s_TimerThread, rrThreadOneHigherThanCurrent);

        // Move over to not-main-thread for some platforms
#if defined(IS_PSP2) || defined(IS_WIIU)
        rrThreadSetCPUCore(&s_TimerThread, 1);
#endif

        miles_thread_init = 1;
    }
    else
    {
        OutMilesMutex();
    }


    //
    // Find a free timer, if possible, and return its handle
    //

    for (i=0; i < n_timers; i++)
    {
        if (timers[i].status == AILT_FREE)
        {
            break;
        }
    }

    //
    // If no free timers, return -1
    //

    if (i == n_timers)
    {
        return -1;
    }

    //
    // Otherwise, mark timer "stopped" and record callback address
    //

    timers[i].status = AILT_STOPPED;

    timers[i].callback = fn;

    //
    // Set default rate of 100 Hz
    //

    timers[i].delta = rrMillisToTime( 10 );
    timers[i].next = 0;

    return i;
}

//############################################################################
//##                                                                        ##
//## Set timer user word                                                    ##
//##                                                                        ##
//############################################################################

UINTa   AILCALL AIL_API_set_timer_user            (HTIMER      timer,      //)
                                                   UINTa       user)
{
    UINTa temp;

    if (timer == -1)
    {
        return 0;
    }

    temp = timers[timer].user;

    timers[timer].user = user;

    return temp;
}

//############################################################################
//##                                                                        ##
//## Set timer period in microseconds, frequency in hertz, or equivalent    ##
//## interrupt divisor value                                                ##
//##                                                                        ##
//############################################################################

void    AILCALL AIL_API_set_timer_period          (HTIMER      timer,       //)
                                                   U32         microseconds)
{
    if (timer == -1)
    {
        return;
    }

    //
    // Begin atomic operation
    //

    InMilesMutex();

    //
    // Reset timer and set new period in microseconds
    //

    timers[timer].delta = rrMicrosToTime( microseconds );
    timers[timer].next = rrGetTime() + timers[timer].delta;

    //
    // End atomic operation
    //

    OutMilesMutex();
}


void    AILCALL AIL_API_set_timer_frequency       (HTIMER      timer,      //)
                                                   U32         hertz)
{
    if (timer == -1)
    {
        return;
    }

    AIL_set_timer_period(timer, 1000000 / hertz);
}



//############################################################################
//##                                                                        ##
//## Start timer(s)                                                         ##
//##                                                                        ##
//############################################################################

void    AILCALL AIL_API_start_timer               (HTIMER      timer)
{
    if (timer == -1)
    {
        return;
    }

    if (timers[timer].status == AILT_STOPPED)
    {
        timers[timer].next = rrGetTime() + timers[timer].delta;
        timers[timer].status = AILT_RUNNING;
    }
}

void    AILCALL AIL_API_start_all_timers          (void)
{
    S32 i;

    for (i=0; i < n_timers; i++)
    {

        AIL_start_timer(i);
    }
}


//############################################################################
//##                                                                        ##
//## Stop timer(s)                                                          ##
//##                                                                        ##
//############################################################################

void    AILCALL AIL_API_stop_timer                (HTIMER      timer)
{
    if (timer == -1)
    {
        return;
    }

    if (timers[timer].status == AILT_RUNNING)
    {
        timers[timer].status = AILT_STOPPED;
    }

}

void    AILCALL AIL_API_stop_all_timers           (void)
{
    S32 i;

    for (i=0; i < n_timers; i++)
    {
        AIL_stop_timer(i);
    }
}


//############################################################################
//##                                                                        ##
//## Release timer handle(s)                                                ##
//##                                                                        ##
//############################################################################

void    AILCALL AIL_API_release_timer_handle      (HTIMER      timer)
{
    if (timer == -1)
    {
        return;
    }

    timers[timer].status = AILT_FREE;
}

void    AILCALL AIL_API_release_all_timers        (void)
{
    S32 i;

    for (i=0; i < n_timers; i++)
    {

        AIL_release_timer_handle(i);
    }
}


DXDEF U32 AILEXPORT AIL_get_timer_highest_delay   (void)
{
  U64 min;
  U64 high;

  min = rrMillisToTime( AIL_preference[AIL_MM_PERIOD] );
  high = highest_timer_delay;
  highest_timer_delay = 0;

  if ( high < min )
    return 0;

  return (U32) rrTimeToMillis( high - min );
}


//############################################################################
//##                                                                        ##
//## Error text handling routines                                           ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_error(C8 const * error_msg)
{
    if (error_msg == NULL) 
    {
        AIL_error[0] = 0;
    }
    else
    {
        AIL_strcpy(AIL_error,error_msg);
    }
}

C8  * AILCALL AIL_API_last_error(void)
{
    return(AIL_error);
}

//
// AIL "preferences" array
//

#if defined(IS_MAC) || defined(IS_LINUX)
  SINTa __attribute__((visibility("default"))) AIL_preference[N_PREFS]; // exposed for midi separation
#else
  SINTa AIL_preference[N_PREFS];
#endif

//############################################################################
//##                                                                        ##
//## Set AIL operational preferences and policies                           ##
//##                                                                        ##
//## May be called by applications which need to alter the default          ##
//## behavior of the AIL system                                             ##
//##                                                                        ##
//############################################################################

SINTa AILCALL AIL_API_set_preference(U32 number, SINTa value)
{
   SINTa old;

   old = AIL_preference[number];

   AIL_preference[number] = value;

   return old;
}

//############################################################################
//##                                                                        ##
//## Get AIL operational preferences and policies                           ##
//##                                                                        ##
//############################################################################

DXDEF
SINTa AILEXPORT AIL_get_preference(U32 number)
{
   return AIL_preference[number];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_timer_thread_handle(void* o_handle)
{
    return rrThreadGetPlatformHandle(&s_TimerThread, o_handle);
}

extern "C"
{
   HTELEMETRY g_Miles_Tm = 0;
}

#ifdef NTELEMETRY

#define check()

#else

static void check(void)
{
  if ( tmCheckVersion( g_Miles_Tm, TelemetryMajorVersion, TelemetryMinorVersion, TelemetryBuildNumber, TelemetryCustomization ) != TM_OK )
  {
    AIL_set_error("Mismatched Miles/Telemetry versions.");
    g_Miles_Tm = 0;
  }
}

#endif

DXDEF void AILCALL MilesUseTmLite(void* context)
{
    g_Miles_Tm = (context) ? TM_CONTEXT_LITE(context) : 0;
    check();
}

DXDEF void AILCALL MilesUseTelemetry(void* context)
{
    g_Miles_Tm = (context) ? TM_CONTEXT_FULL(context) : 0;
    check();
}

DXDEF U32 AILEXPORT AIL_ms_count()
{
  return (U32) rrTimeToMillis( rrGetTime() );
}

DXDEF U64 AILEXPORT AIL_ms_count64()
{
  return rrTimeToMillis( rrGetTime() );
}

DXDEF U32 AILEXPORT AIL_us_count()
{
  return (U32) rrTimeToMicros( rrGetTime() );
}

DXDEF U64 AILEXPORT AIL_us_count64()
{
  return rrTimeToMicros( rrGetTime() );
}

DXDEF U64 AILEXPORT AIL_get_time(void)
{
  return rrGetTime();
}

DXDEF U64 AILEXPORT AIL_time_to_ms(U64 time)
{
  return rrTimeToMillis( time );
}

DXDEF U64 AILEXPORT AIL_ms_to_time(U64 ms)
{
  return rrMillisToTime( ms );
}


static AILMEMALLOCCB mss_alloc = platform_alloc;
static AILMEMFREECB mss_free = platform_free;

DXDEC AILMEMALLOCCB AILCALL AIL_mem_use_malloc(AILMEMALLOCCB fn)
{
    AILMEMALLOCCB ret = mss_alloc;
    mss_alloc = (fn)?fn:platform_alloc;
    return( ret );
}

DXDEC AILMEMFREECB AILCALL AIL_mem_use_free  (AILMEMFREECB fn)
{
    AILMEMFREECB ret = mss_free;
    mss_free = (fn)?fn:platform_free;
    return( ret );
}


//############################################################################
//##                                                                        ##
//## Allocate and free page-locked global memory for AIL resources          ##
//##                                                                        ##
//############################################################################

void * AILCALL AIL_API_mem_alloc_lock(UINTa size)
{
    return( mss_alloc( size ) );
}

void AILCALL AIL_API_mem_free_lock(void *ptr)
{
    if (ptr != NULL)
    {
        mss_free( ptr );
    }
}

DXDEF void * AILEXPORT MSS_alloc_info( UINTa size, UINTa user, char const * filename, U32 line )
{
  return AIL_mem_alloc_lock_info( size, filename, line );
}

DXDEF void AILEXPORT MSS_free_info( void * ptr, UINTa user, char const * filename, U32 line )
{
  AIL_mem_free_lock( ptr );
}

MSS_DEF_CB_STACK_ALIGN( MSS_alloc_info )
MSS_DEF_CB_STACK_ALIGN( MSS_free_info)

#include <stdarg.h>
#include <stdio.h>

DXDEF S32 AILEXPORT AIL_sprintf(char *dest,
                                char const *fmt, ...)
{
  S32 len;

  va_list ap;

  va_start(ap,
           fmt);

  len=vsprintf(dest,
               fmt,
               ap);

  va_end  (ap);

  return( len );
}



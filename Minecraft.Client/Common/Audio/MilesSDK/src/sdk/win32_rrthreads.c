/* @cdep pre
   $if( $equals($BuildPlatform,win32),$requiresbinary(winmm.lib), )
   $if( $equals($BuildPlatform,win64),$requiresbinary(winmm.lib), )
*/
 
// thread functions for Win32, Win64, Xbox, and Xenon

#include "rrThreads.h" 
#include "rrAtomics.h"
#include "rrCPU.h"


#pragma code_seg("RADCODE")
#pragma data_seg("RADDATA")
#pragma const_seg("RADCONST")
#pragma bss_seg("RADBSS")



#if defined(_XBOX)
  #undef S8
  #define NOD3D
  #include <xtl.h>
  #include <PPCIntrinsics.h>
  #define S8 signed char
  #define _T(x) x

#elif defined(__RADSEKRIT__)

   #include "rrTime.h"
   #include <windows.h>
   #include <synchapi.h>
   #include <processthreadsapi.h>
   #include <libloaderapi.h>

#else
  //c:\Program Files\Microsoft Visual Studio .NET 2003\Vc7\PlatformSDK\Include\MMSystem.h(2007) : warning C4201: nonstandard extension used : nameless struct/union
  #pragma warning(disable : 4201)
  
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <tchar.h>
  #include <windows.h>
  #include <mmsystem.h>
#endif


#if !defined( __RADXENON__ )
  // add replacements for the intrinsics for losers using vc6 and 2003
  #if _MSC_VER < 1400
    #if ( _MSC_VER < 1300 ) || defined ( __RADXBOX__ )
      // for msvc6
      static unsigned char _BitScanForward( unsigned long * Index, unsigned long Mask )
      {
        __asm
        {
          mov eax,[Mask]
          mov edx,[Index]
          bsf eax,eax
          mov [edx],eax
          setne al
        }
      }
      #define ULONG_PTR DWORD
    #else
      // for vsnet2003
      extern unsigned char _BitScanForward( unsigned long * Index, unsigned long Mask );
      #pragma intrinsic(_BitScanForward)
    #endif
  #endif
#endif


typedef BOOL WINAPI InitializeCriticalSectionAndSpinCountFunc(
  LPCRITICAL_SECTION lpCriticalSection,
  DWORD dwSpinCount
);

typedef BOOL WINAPI TryEnterCriticalSectionFunc(
  LPCRITICAL_SECTION lpCriticalSection
);

typedef DWORD WINAPI SetThreadIdealProcessorFunc(
  HANDLE hThread,
  DWORD dwIdealProcessor
);

typedef BOOL WINAPI SwitchToThreadFunc( void );

typedef BOOL WINAPI IsDebuggerPresentFunc( void );

typedef DWORD WINAPI SignalObjectAndWaitFunc(
    HANDLE hObjectToSignal,
    HANDLE hObjectToWaitOn,
    DWORD dwMilliseconds,
    BOOL bAlertable);
    
#if !defined( __RADXENON__ ) && !defined( __RADXBOX__ ) && !defined(__RADSEKRIT__)

static InitializeCriticalSectionAndSpinCountFunc * icsasc = 0;
static TryEnterCriticalSectionFunc * tecs = 0;
static SetThreadIdealProcessorFunc * stip = 0;
static SwitchToThreadFunc * stt = 0;
static IsDebuggerPresentFunc * idp = 0;
static SignalObjectAndWaitFunc * soaw = 0;

#else

#define tecs TryEnterCriticalSection

#endif

typedef struct rriThread
{
  U64 creation_count;
  thread_function * function;
  void * user_data;
  char const * name;
  void * extra_data[ RR_MAX_EXTRA_THREAD_DATA ];
  HANDLE handle;
  U32 flags;
  U32 status;
  S32 done;
  U32 index;  // rr index
  DWORD id;   // windows id
} rriThread;

#define TLS_NOT_SETUP ( (DWORD) -1 )
static DWORD volatile thread_tls_index = TLS_NOT_SETUP;
static U64 __declspec( align( 8 ) ) thread_increment = 0;
static U32 thread_masks[ ( RR_MAX_THREADS + 31 ) / 32 ] = {0};
static S32 number_cores = -1;
static U32 active_rrthreads = 0;
static S32 set_threadnames = 0;


#pragma pack(push,8)
typedef struct THREADNAME_INFO
{
  DWORD dwType;
  LPCSTR szName;
  DWORD dwThreadID;
  DWORD dwFlags;
} THREADNAME_INFO;
#pragma pack(pop)

static LONG WINAPI handle_bad( struct _EXCEPTION_POINTERS * info )
{
  if ( info->ExceptionRecord->ExceptionCode == 0x406d1388 )
  {
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

static void RADLINK rr_set_thread_name( DWORD thread_id, char const * thread_name )
{
  static U32 volatile raising = 0;
  
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = thread_name;
  info.dwThreadID = thread_id;
  info.dwFlags = 0;

#if !defined( __RADXENON__ ) && !defined( __RADXBOX__ ) && !defined(__RADSEKRIT__)
  if ( (UINTa) idp <= 1 )
  {
    if ( idp == 0 )
    {
      HANDLE kernel32Handle = GetModuleHandle(_T("KERNEL32"));
      if (kernel32Handle != 0)
      {
        idp = (IsDebuggerPresentFunc*) GetProcAddress( kernel32Handle, "IsDebuggerPresent" );
        if ( idp )
          goto found_idp;
      }
      idp = (IsDebuggerPresentFunc*) (UINTa) 1;
    }
  }
  else
  {
   found_idp:
    if ( idp() )
    {
      for(;;)
      {
        if ( rrAtomicCmpXchg32( &raising, 1, 0 ) == 0 )
        {
          LPTOP_LEVEL_EXCEPTION_FILTER flt;
          flt = SetUnhandledExceptionFilter( &handle_bad );
          RaiseException( 0x406d1388, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR *) &info);
          SetUnhandledExceptionFilter( flt );
          raising = 0;
          break;
        } 
        Sleep(1);
      }
    }
  }
#else
    __try
    {
      RaiseException( 0x406d1388, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR *) &info);
    }
    __except( GetExceptionCode()==0x406D1388 ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_EXECUTE_HANDLER )
    {
    }
#endif
}

static int CpuNumCores( void )
{
  if ( number_cores == -1 )
    number_cores = rrGetTotalCPUs();

  return number_cores;
}

// low level function to clean up after the thread
static void thread_cleanup( rriThread * thread );


static DWORD WINAPI win32_shim( LPVOID param )
{
  rrThread * rr;
  rriThread * t;
  DWORD ret;

  rr = (rrThread*) param;
  t = rr->i;

  // tls is guaranteed to be setup by now
  TlsSetValue( thread_tls_index, rr );

  // set the thread name in the debugger (more reliable to do this on the thread itself)
  if ( ( t->name ) && ( *t->name ) )
  {
#ifndef __RADXENON__
    if ( set_threadnames != -1 )
#endif
    {
      rr_set_thread_name( t->id, t->name );
    }
  }

  // run the user thread
  ret = t->function( t->user_data );

  // flip the 1 bit for not running and then see if we have to free on exit
  for(;;)
  {
    U32 st = t->status;

    if ( rrAtomicCmpXchg32( &t->status, st & ~1, st ) == st )
    {
      if ( st == 3 )
      {
        thread_cleanup( t );
      }
      break;
    }
  }

  // clear the tls!
  TlsSetValue( thread_tls_index, 0 );

  return( ret );
}


// utility function to snag our rrthread out of the tls
static rrThread * find_rrthread_in_tls( rrThread * t )
{
  if ( t == 0 )
  {
    if ( thread_tls_index == TLS_NOT_SETUP )
      return 0; // zero means non rrthread (user thread)

    t = (rrThread*) TlsGetValue( thread_tls_index );
  }

  return t;
}


// get globally-unique thread index 1..RR_MAX_THREADS-1
//   returns 0 for any thread that isn't an rrthread
RADDEFFUNC U32 RADLINK rrThreadIndex( rrThread * t )
{
  t = find_rrthread_in_tls( t );

  if ( t )
    return t->i->index;

  return 0; // zero means non rrthread
}


//you can use the extra data functions to save info that would otherwise have to be tls-ed
RADDEFFUNC S32 RADLINK rrThreadSetExtraData( rrThread * t, S32 index, void * val )
{
  if ( index >= RR_MAX_EXTRA_THREAD_DATA )
    return 0;

  t = find_rrthread_in_tls( t );

  if ( t )
  {
    t->i->extra_data[ index ] = val;
    return 1;
  }

  return 0; // zero means non rrthread
}


RADDEFFUNC S32 RADLINK rrThreadGetExtraData( rrThread * t, S32 index, void ** ret )
{
  if ( index >= RR_MAX_EXTRA_THREAD_DATA )
    return 0;

  t = find_rrthread_in_tls( t );

  if ( t )
  {
    if ( ret )
      *ret = t->i->extra_data[ index ];
    return 1;
  }

  return 0; // zero means non rrthread
}


// get entire program run globally-unique thread value (incrementing 64 bit number)
// returns 0 for any thread that isn't an rrthread
RADDEFFUNC U64 RADLINK rrThreadUnique( rrThread * t )
{
  t = find_rrthread_in_tls( t );

  if ( t )
    return t->i->creation_count;

  return 0; // zero means non rrthread
}


// get the string name
// returns 0 for any thread that isn't an rrthread
RADDEFFUNC char const * RADLINK rrThreadName( rrThread * t )
{
  t = find_rrthread_in_tls( t );

  if ( t )
    return t->i->name;

  return 0; // zero means non rrthread
}


// returns the current running rrthread pointer
// returns 0 for any thread that isn't an rrthread
RADDEFFUNC rrThread * RADLINK rrThreadCurrent( void )
{
  return (rrThread*) find_rrthread_in_tls( 0 );
}


// how many rrthreads are actively being tracked
RADDEFFUNC U32 RADLINK rrThreadGetNumThreads( void )
{
  return active_rrthreads;
}


RADDEFFUNC rrbool RADLINK rrThreadCreate( rrThread * rr, thread_function * function, U32 stack_size, void * user_data, S32 flags, char const * name )
{
  rriThread * thread;
  int index;
  int i;
  U64 count;

  #ifdef _DEBUG
     typedef char test[ ( sizeof ( rrThread ) >= ( sizeof( rriThread ) + 4 + 15 ) ) ? 1 : -1 ];  // make sure our rrthread isn't too big
  #endif

  RR_ASSERT_ALWAYS_NO_SHIP( stack_size != 0 );

  thread = (rriThread*) ( ( ( (UINTa) rr->data ) + 15 ) & ~15 );
  rr->i = thread;

  // get the unique thread increment value
  count = rrAtomicAddExchange64( &thread_increment, 1 );
  if ( count == 0 )
  {
    rrAtomicMemoryBarrierFull();

    // allocate the tls, if this is the first time
    thread_tls_index = TlsAlloc();
  }
  else
  {
    // wait for the tls index to be is valid (almost always will be)
    int spins = rrGetSpinCount() + 1;  //always at least one spin
    for( i = 0 ; i < spins ; i++ )
    {
      rrThreadSpinHyperYield();
      if ( thread_tls_index != TLS_NOT_SETUP )
        goto got_tls;
    }
    
    // after trying the fast path, just loop on it
    for( ; ; )
    {
      rrThreadSleep( 1 );
      if ( thread_tls_index != TLS_NOT_SETUP )
        goto got_tls;
    }
  }
 got_tls:

#if !defined( __RADXENON__ ) && !defined(__RADSEKRIT__)
  if ( set_threadnames == 0 )
  {
    char buf[ 16 ];
    set_threadnames = 1;

    if ( GetEnvironmentVariableA( "RAD_NO_SET_THREAD_NAMES", buf, 16 ) )
    {
      if ( buf[ 0 ] == '1' )
        set_threadnames = -1;
    }
  }
#endif

  // find an open bitmask to use
  for( i = 0 ; i < ( ( RR_MAX_THREADS + 31 ) / 32 ) ; i++ )
  {
    for( ; ; )
    {
      U32 mask, bit_index;

      mask = thread_masks[ i ];

      // try to find a zero bit in this mask
      #ifdef __RADXENON__
        bit_index = 31 - _CountLeadingZeros(~mask);
        if ( bit_index == (U32)~0 )
          break;
      #else  
        if ( _BitScanForward( (DWORD*) &bit_index, ~mask ) == 0 )
          break;
      #endif
      rrAssert( (mask & (1<<bit_index))== 0 );

      if ( rrAtomicCmpXchg32( &thread_masks[ i ], mask | ( 1 << bit_index ), mask ) == mask )
      {
        index = i * 32 + bit_index;
        goto got_slot;
      }
    }
  }

  // no thread positions found - error
  return 0;

 got_slot:

  // setup the info
  thread->creation_count = count;
  thread->function = function;
  thread->user_data = user_data;
  thread->index = index;
  thread->name = name;
  thread->flags = flags;
  thread->status = 1;
  thread->done = 0;
  thread->handle = CreateThread( 0, stack_size, win32_shim, rr, CREATE_SUSPENDED, &thread->id );  //jkr: assert non-zero stacksize?

  if ( thread->handle == 0 )
  {
    thread->status = 0;
    thread->done = 1;

    // clear the bit mask
    for( ; ; )
    {
      U32 mask, bit;

      mask = thread_masks[ index >> 5 ];
      bit = ( 1 << ( index & 31 ) );

      if ( rrAtomicCmpXchg32( &thread_masks[ index >> 5 ], mask & ~bit, mask ) == mask )
        break;
    }

    return 0;
  }

  rrThreadSetPriority( rr, rrThreadSameAsCalling );

  if ( rrAtomicAddExchange32( &active_rrthreads, 1 ) == 0 )
  {
    /*
      This function affects a global Windows setting. Windows uses the lowest value
      (that is, highest resolution) requested by any process. Setting a higher
      resolution can improve the accuracy of time-out intervals in wait functions.
      However, it can also reduce overall system performance, because the thread
      scheduler switches tasks more often. High resolutions can also prevent the CPU
      power management system from entering power-saving modes. Setting a higher
      resolution does not improve the accuracy of the high-resolution performance counter.
    */
    #if defined( __RADXENON__ ) || defined( __RADXBOX__ ) || defined( __RADSEKRIT__ )
      ;
    #else
      timeBeginPeriod( 1 );
    #endif
  }

  if ( ! ( flags & rrThreadCreateSuspended ) )
  {
    // start the thread (we created it suspended)
    ResumeThread( thread->handle );
  }

  return 1;
}


RADDEFFUNC void RADLINK rrThreadResume( rrThread * rr )
{
  rriThread * thread;

  rr = find_rrthread_in_tls( rr );
  if ( rr == 0 )
    return;
  thread = rr->i;

  // start the thread (if we created it suspended)
  ResumeThread( thread->handle );
}


// stick an rrthread directly on a specific cpu
RADDEFFUNC void RADLINK rrThreadSetCPUCore( rrThread * rr, S32 core_assign )
{
  U32 core_num;
  rriThread * thread;

  rr = find_rrthread_in_tls( rr );
  if ( rr == 0 )
    return;
  thread = rr->i;

  core_num = core_assign;

  // assign the thread to a core, if they want it
  if ( core_assign == RR_THREAD_AUTO_ASSIGN_CORE )
  {
    core_num = rrGetCPUCoreByPrecedence( thread->index );
  }

  // get the thread on a reasonable cpu
  #if defined(__RADXENON__)

    XSetThreadProcessor( thread->handle, core_num );
  
  #elif defined(__RADSEKRIT__)

    {
      PROCESSOR_NUMBER PN = {0};
      PN.Number =  (BYTE)core_num;
      SetThreadIdealProcessorEx(thread->handle, &PN, 0);
    }

  #elif !defined(__RADXBOX__)
  
    // on win32, try setthreadideal first, and then the affinitymask
    if ( (UINTa) stip <= 1 )
    {
      if ( stip == 0 )
      {
        HANDLE kernel32Handle = GetModuleHandle(_T("KERNEL32"));
        if (kernel32Handle != 0)
        {
          stip = (SetThreadIdealProcessorFunc*) GetProcAddress( kernel32Handle, "SetThreadIdealProcessor" );
          if ( stip )
            goto found;
        }
        stip = (SetThreadIdealProcessorFunc*) (UINTa) 1;
      }

      // didn't find it, use set affinity
      SetThreadAffinityMask( thread->handle, (UINTa) ( 1 << core_num ) );
    }
    else
    {
     found:
      // main thread gets proc 0, next gets proc 1 , etc. then loop
      stip( thread->handle, core_num );
    }

  #endif
}


RADDEFFUNC void RADLINK rrThreadSetPriority( rrThread * rr, S32 priority )
{
  int win_pri;
  rriThread * thread;

  rr = find_rrthread_in_tls( rr );
  if ( rr == 0 )
    return;
  thread = rr->i;

  win_pri = 0;
  switch( priority )
  {
    case rrThreadLowest:
      win_pri = THREAD_PRIORITY_LOWEST;
      break;
    case rrThreadLow:
      win_pri = THREAD_PRIORITY_BELOW_NORMAL;
      break;
    case rrThreadNormal:
      win_pri = THREAD_PRIORITY_NORMAL;
      break;
    case rrThreadHigh:
      win_pri = THREAD_PRIORITY_ABOVE_NORMAL;
      break;
    case rrThreadHighest:
      win_pri = THREAD_PRIORITY_HIGHEST;
      break;
    case rrThreadOneHigherThanCurrent:
      win_pri = GetThreadPriority( thread->handle ) + 1;
      break;
    case rrThreadOneLowerThanCurrent:
      win_pri = GetThreadPriority( thread->handle ) - 1;
      break;
    case rrThreadSameAsCalling:
      win_pri = GetThreadPriority(GetCurrentThread());
      break;
    case rrThreadOneHigherThanCalling:
      win_pri = GetThreadPriority(GetCurrentThread()) + 1;
      break;
    case rrThreadOneLowerThanCalling:
      win_pri = GetThreadPriority(GetCurrentThread()) - 1;
      break;

    default:
      return;
  }

  SetThreadPriority( thread->handle, win_pri );
}


// waits for the thread to close down - you still have to call rrthreadcleanup
//  -> note this cannot be used with threads that clean themselves up
RADDEFFUNC rrbool RADLINK rrThreadWaitDone( rrThread * rr, U32 ms, U32 * ecp )
{
  rriThread * thread;

  if ( rr == 0 )
    return 0;

  thread = rr->i;
  
  // has this thread already been marked as done?
  if ( thread->done )
  {
    if ( ecp )
      *ecp = thread->done;
    return 1;
  }

  #ifdef _DEBUG
    if ( thread->flags & rrThreadDontNeedWaitDoneAtAll )
      RR_BREAK();
    if ( ( ms != RR_WAIT_INFINITE ) && ( ( thread->flags & rrThreadNeedWaitDoneTimeOuts ) == 0 ) )
      RR_BREAK();
  #endif

  if ( WaitForSingleObject( thread->handle, ms ) == WAIT_OBJECT_0 )
  {
    thread->done = 0x80000000;

    if ( ecp )
    {
      *ecp = (U32)(S32)-1;
      GetExitCodeThread( thread->handle, (DWORD*) ecp );
      thread->done |= *ecp;
    }
    return 1;
  }

  return 0;
}

RADDEFFUNC rrbool RADLINK rrThreadGetPlatformHandle( rrThread * thread, void * out )
{
  thread = find_rrthread_in_tls( thread );

  if ( thread == 0 )
    return 0;
  
  ((HANDLE*)out)[ 0 ] = thread->i->handle;
  
  return 1;
}

static rrbool rrThreadGetPlatformId( rrThread * thread, DWORD * out )
{
  thread = find_rrthread_in_tls( thread );

  if ( thread == 0 )
    return 0;
  
	*out = thread->i->id;
  
  return 1;
}


static void thread_cleanup( rriThread * thread )
{
  HANDLE h;

  thread->done = (U32)(S32)-1;

  // have we freed this one before?
  h = thread->handle;

  // use an interlock so the shutdown is thread safe
  if ( rrAtomicCmpXchgPointer( (UINTa*)&thread->handle, (UINTa)INVALID_HANDLE_VALUE, h ) == h )
  {
    U32 old;

    if ( h == INVALID_HANDLE_VALUE )
      return;

    old = rrAtomicAddExchange32( &active_rrthreads, -1 );
    if ( old == 1 )
    {
      #if defined( __RADXENON__ ) || defined( __RADXBOX__ ) || defined( __RADSEKRIT__ )
        ;
      #else
        timeEndPeriod( 1 );
      #endif
    }

    // clear the bit mask
    for( ; ; )
    {
      U32 mask, bit;

      mask = thread_masks[ thread->index >> 5 ];
      bit = ( 1 << ( thread->index & 31 ) );

      if ( rrAtomicCmpXchg32( &thread_masks[ thread->index >> 5 ], mask & ~bit, mask ) == mask )
        break;
    }

    // if we are cleaning up our own rrthread, then clear the tls
    {
      rrThread * rr = rrThreadCurrent();
      if ( ( rr ) && ( rr->i == thread ) )
      {
        // clear the tls!
        TlsSetValue( thread_tls_index, 0 );
      }
    }

    CloseHandle( h );
  }
}


// frees the crap associated with a done rrthread
RADDEFFUNC void RADLINK rrThreadCleanUp( rrThread * rr )
{
  rriThread * thread;

  rr = find_rrthread_in_tls( rr );
  if ( rr == 0 )
    return;
  thread = rr->i;

  // if the thread that we are cleaning up is running, don't clear anything out, just set it to by the thread on exit
  rrAtomicCmpXchg32( &thread->status, 3, 1 );
  if ( thread->status )
    return;

  thread_cleanup( thread );
}


RADDEFFUNC rrOSThreadType RADLINK rrOSThreadGetCurrent()
{
  HANDLE h = GetCurrentThread();
  return (rrOSThreadType)(UINTa)h;
}

RADDEFFUNC rrOSThreadType RADLINK rrThreadGetOSThread( rrThread * thread )
{
	HANDLE h = 0;
	if ( ! rrThreadGetPlatformHandle(thread,&h) ) return 0;
	return (rrOSThreadType)(UINTa)h;
}

RADDEFFUNC int RADLINK  rrOSThreadGetPriority(rrOSThreadType t)
{
	int p = GetThreadPriority((HANDLE)(UINTa)t);
	return p;
}

RADDEFFUNC void RADLINK rrOSThreadSetPriority(rrOSThreadType t,int pri)
{
  SetThreadPriority( (HANDLE)(UINTa)t , pri );
}


RADDEFFUNC rrOSThreadIDType RADLINK rrOSThreadIDGetCurrent()
{
  // GetCurrentThread() is NOT a unique handle
  //  it's a per-thread TLS handle! (it's always -2)
  DWORD id = GetCurrentThreadId();
  return (rrOSThreadType)id;
}

RADDEFFUNC rrOSThreadIDType RADLINK rrThreadGetOSThreadID( rrThread * thread )
{
  DWORD id = 0;
  if ( ! rrThreadGetPlatformId(thread,&id) ) return 0;
  return (rrOSThreadType)(UINTa)id;
}

RADDEFFUNC int RADLINK  rrOSThreadIDGetPriority(rrOSThreadIDType t)
{
  int p;
  DWORD id = (DWORD)t;
  HANDLE h = OpenThread(THREAD_QUERY_INFORMATION,FALSE,id);
  if ( h == 0 ) return 0;
  p = GetThreadPriority(h);
  RR_ASSERT( p != THREAD_PRIORITY_ERROR_RETURN );
  CloseHandle(h);
  return p;
}

RADDEFFUNC void RADLINK rrOSThreadIDSetPriority(rrOSThreadIDType t,int pri)
{
  BOOL ok;
  DWORD id = (DWORD)t;
  HANDLE h = OpenThread(THREAD_SET_INFORMATION,FALSE,id);
  if ( h == 0 ) return;
  ok = SetThreadPriority(h,pri);
  RR_ASSERT( ok ); ok;
  CloseHandle(h);
}



typedef struct rriMutex
{
  U32 flags;
  union w
  {
    HANDLE m;
    CRITICAL_SECTION cs;
  } w;
} rriMutex;


// internal flags in rrMutex.flags (enum rrMutexType is or-ed into the bottom)
#define FLAGS_TYPE_MUTEX    32
#define FLAGS_TYPE_CRITSEC  64
#define FLAGS_TYPE_MASK      3

#define SIZEOF_MUTEX_TYPE() ( sizeof( HANDLE ) + (UINTa) &((rrMutex*)0)->w.m )


RADDEFFUNC rrbool RADLINK rrMutexCreate( rrMutex * rr, S32 mtype )
{
  rriMutex * p;
  U32 flags;
  int spins = rrGetSpinCount();

  #ifdef _DEBUG
    typedef char test[ ( sizeof ( rrMutex ) >= ( sizeof( rriMutex ) + 4 + 15 ) ) ? 1 : -1 ];  // make sure our rrmutex isn't too big
  #endif

  p = (rriMutex*) ( ( ( (UINTa) rr->data ) + 15 ) & ~15 );
  p->flags = 0;
  rr->i = p;

  RR_ASSERT( ( ((U64)(UINTa)rr->i) + sizeof(*(rr->i)) ) <= ( ((U64)(UINTa)rr) + sizeof(rr->data) ) );

  if ( mtype == rrMutexNeedFullTimeouts )
  {
#if !defined( __RADXENON__ ) && !defined( __RADXBOX__ ) && !defined(__RADSEKRIT__)
   use_mutex:
#endif
    // use a normal windows mutex
    p->w.m = CreateMutex( 0, 0, 0 );

    flags = p->flags | FLAGS_TYPE_MUTEX | ( (int) mtype );

    rrAtomicMemoryBarrierFull();

    p->flags = flags;

    return 1;
  }
    
#if defined( __RADXENON__ ) || defined(__RADSEKRIT__)
  InitializeCriticalSectionAndSpinCount( &p->w.cs, spins );
#elif defined(__RADXBOX__ )
  InitializeCriticalSection( &p->w.cs );
#else
  // see if we have tryentercriticalsection on win32
  if ( (UINTa) tecs <= 1 )
  {
    if ( tecs == 0 )
    {
      HANDLE kernel32Handle = GetModuleHandle(_T("KERNEL32"));
      if (kernel32Handle != 0)
      {
        tecs = (TryEnterCriticalSectionFunc*) GetProcAddress( kernel32Handle, "TryEnterCriticalSection" );
        if ( tecs )
          goto have_tecs;
      }
      tecs = (TryEnterCriticalSectionFunc*) (UINTa) 1;
    }

    // we don't have tryenter, so if we need timeouts, then we have to use a mutex
    if ( mtype != rrMutexDontNeedTimeouts )
      goto use_mutex;
  }

 have_tecs:

  // we can use critical sections...

  // spincount is way better on real multicore systems ; has no affect on single core
  if ( (UINTa) icsasc <= 1 )
  {
    if ( icsasc == 0 )
    {
      HANDLE kernel32Handle = GetModuleHandle(_T("KERNEL32"));
      if (kernel32Handle != 0)
      {
        icsasc = (InitializeCriticalSectionAndSpinCountFunc*) GetProcAddress( kernel32Handle, "InitializeCriticalSectionAndSpinCount" );
        if ( icsasc )
          goto found;
      }
      icsasc = (InitializeCriticalSectionAndSpinCountFunc*) (UINTa) 1;
    }

    // no spincount - use the old version
    InitializeCriticalSection( &p->w.cs );
  }
  else
  {
   found:
    icsasc( &p->w.cs, spins );
  }
#endif

  flags = p->flags | FLAGS_TYPE_CRITSEC | ( (int) mtype );

  rrAtomicMemoryBarrierFull();

  p->flags = flags;

  return 1;
}


RADDEFFUNC void RADLINK rrMutexDestroy( rrMutex * rr )
{
  U32 flags;
  rriMutex * p;

  if ( rr == 0 )
    return;
  p = rr->i;

  flags = p->flags;

  // tries to be sort of thread safe, but usually frees the rrMutex so not really
  //  also really if you can destroy mutex from multiple threads you likely have horrible problems
  if ( rrAtomicCmpXchg32( &p->flags, 0, flags ) == flags )
  {
    if ( flags & FLAGS_TYPE_MUTEX )
    {
      CloseHandle( p->w.m );
    }
    else if ( flags & FLAGS_TYPE_CRITSEC )
    {
      DeleteCriticalSection( &p->w.cs );
    }
  }
}


RADDEFFUNC void RADLINK rrMutexLock( rrMutex * rr )
{
  rriMutex * p;

  if ( rr == 0 )
    return;
  p = rr->i;

  RR_ASSERT( p != NULL );

  //lock it, man
  if ( p->flags & FLAGS_TYPE_MUTEX )
  {
    WaitForSingleObject( p->w.m, INFINITE );
  }
  else if ( p->flags & FLAGS_TYPE_CRITSEC )
  {
    EnterCriticalSection( &p->w.cs );
  }
}


RADDEFFUNC void RADLINK rrMutexUnlock( rrMutex * rr )
{
  rriMutex * p;

  if ( rr == 0 )
    return;
  p = rr->i;

  //unlock it, man
  if ( p->flags & FLAGS_TYPE_MUTEX )
  {
    ReleaseMutex( p->w.m );
  }
  else if ( p->flags & FLAGS_TYPE_CRITSEC )
  {
    LeaveCriticalSection( &p->w.cs );
  }
}

#ifdef __RADXENON__
  #define fasttimer() ( (DWORD)__mftb32() )
  #define fastms( v ) ( v * 50000 )
#elif defined(__RADSEKRIT__)
  #define fasttimer() ((DWORD)(rrTimeToMillis(rrGetTime())))
  #define fastms( v ) ( v )
#else
  #define fasttimer timeGetTime
  #define fastms( v ) ( v )
#endif

RADDEFFUNC rrbool RADLINK rrMutexLockTimeout( rrMutex * rr, U32 ms )
{
  rriMutex * p;

  if ( rr == 0 )
    return 0;
  p = rr->i;

  #ifdef _DEBUG
    if ( ( ( p->flags & FLAGS_TYPE_MASK ) == (int) rrMutexDontNeedTimeouts ) && ( ms != RR_WAIT_INFINITE ) )
      RR_BREAK();  // you tried to use a non-infinite timeout, when you said you didn't need them!

    if ( ( ( p->flags & FLAGS_TYPE_MASK ) == (int) rrMutexNeedOnlyZeroTimeouts ) && ( ms != 0 ) && ( ms != RR_WAIT_INFINITE ) )
      RR_BREAK();  // you tried to use a non-zero timeout, when you said you only need zero tests
  #endif

  if ( p->flags & FLAGS_TYPE_MUTEX )
  {
    switch ( WaitForSingleObject( p->w.m, ( ms == RR_WAIT_INFINITE ) ? INFINITE : ms ) )
    {
      case WAIT_OBJECT_0:
      case WAIT_ABANDONED:
        return 1;

      case WAIT_TIMEOUT:
      default:
         return 0;
    }
  }
  else if ( p->flags & FLAGS_TYPE_CRITSEC )
  {
    DWORD t;

    // infinite wait always works with critical sections
    if ( ms == RR_WAIT_INFINITE )
    {
      EnterCriticalSection( &p->w.cs );
      return 1;
    }

    t = fasttimer();

    // try a lock
    if ( tecs( &p->w.cs ) )
      return 1;

    // if instant read, return on lock fail
    if ( ms == 0 )
      return 0;

    //give it a mini yield
    Sleep( 0 );

    // try again
    if ( tecs( &p->w.cs ) )
      return 1;

    // convert ms to 
    ms = fastms( ms );

    // have we waited enough already?
    if ( ( fasttimer() - t ) >= fastms( ms ) )
      return 0;

    //give it bit more wait action
    rrThreadYieldToAny( );

    //ok, let just drop into a check loop
    for( ; ; )
    {
      // try again
      if ( tecs( &p->w.cs ) )
        return 1;

      // have we waited enough?
      if ( ( fasttimer() - t ) >= fastms( ms ) )
        return 0;

      rrThreadSleep( 1 );
    }

  }

  return 0;
}


RADDEFFUNC void RADLINK rrThreadSleep( U32 ms )
{
#if !defined( __RADXENON__ ) && !defined( __RADXBOX__ )
  {
    int p = (1<<30);

    // on windows, if the wait time is short enough to be
    //   close to the granuarity of the thread scheduler,
    //   then bump up the thread priority before you sleep,
    //   so the scheduler will wake you up more accurately.
    if ( ms <= 30 && ms != 0 )
    {
      p = GetThreadPriority( GetCurrentThread() );
      SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );
      //SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL+5 );
    }
  #endif

  Sleep( ms );

#if !defined( __RADXENON__ ) && !defined( __RADXBOX__ )
    if ( p != (1<<30) )
    {
      SetThreadPriority( GetCurrentThread(), p );
    }
  }
  #endif
}


RADDEFFUNC void RADLINK rrThreadYieldNotLower( void )
{
	Sleep( 0 );
}

RADDEFFUNC void RADLINK rrThreadYieldToAny( void )
{
#if defined( __RADXENON__ ) || defined( __RADXBOX__ ) || defined(__RADSEKRIT__)
  SwitchToThread();
#else
  if ( (UINTa) stt <= 1 )
  {
    if ( stt == 0 )
    {
      HANDLE kernel32Handle = GetModuleHandle(_T("KERNEL32") );
      if (kernel32Handle != 0)
      {
        stt = (SwitchToThreadFunc*) GetProcAddress( kernel32Handle, "SwitchToThread" );
        if ( stt )
          goto found;
      }
      stt = (SwitchToThreadFunc*) (UINTa) 1;
    }

	// no STT , what to do ?
	//	Sleep(0) doesn't yield enough, but Sleep(1) gives up too much time
	// I'm gonna say no, do not do the sleep(1) here
	//	because caller can always eventually escalate to a sleep(1) themselves
    //Sleep( 1 );
    Sleep( 0 );
  }
  else
  {
   found:
    stt();
  }
#endif  
}

// CB : rrThreadSpinHyperYield should be called in spin loops to yield hyper-threads
//	on a non-hyper threaded system it is a NOP so it should always be called in spins
RADDEFFUNC void         RADLINK rrThreadSpinHyperYield( void )
{
	// on hypeSpinBackOffrthreaded procs need this :
	//	benign on non-HT
	//	(actually better than benign - it improves performance even non-HT multi-core)
	#ifdef YieldProcessor
	YieldProcessor();
	#else
	_mm_pause();
	#endif	
}


//-----------------------------------------------------------------------------------------------------
// CB : event stuff

typedef struct rriEvent
{
  HANDLE handle;
} rriEvent;

RADDEFFUNC rrbool   RADLINK rrEventCreate( rrEvent * rr )
{
  #ifdef _DEBUG
	// no +15 cuz HANDLE doesn't need to be aligned
    typedef char test[ ( sizeof ( rrEvent ) >= ( sizeof( rriEvent ) + 4 ) ) ? 1 : -1 ];  // make sure our rrmutex isn't too big
  #endif

  // AUTO reset
  HANDLE h = CreateEvent(NULL, 0, 0, NULL);
  if ( h == INVALID_HANDLE_VALUE )
      return 0;

  rr->i = (rriEvent*) rr->data;
  
  // no aligning here
  //rr->i = (rriEvent*) ( ( ( (UINTa) rr->data ) + 15 ) & ~15 );
  //RR_ASSERT( ( ((U64)rr->i) + sizeof(rriEvent) ) <= ( ((U64)rr) + sizeof(rr->data) ) );
    
  rr->i->handle = h;

  return 1;
}

RADDEFFUNC void         RADLINK rrEventDestroy( rrEvent * event )
{
  if ( event )
  {
    CloseHandle( event->i->handle );
    event->i->handle = INVALID_HANDLE_VALUE;
  }
}

RADDEFFUNC rrbool         RADLINK rrEventWaitAndClear( rrEvent * event , U32 millis )
{
  // INFINITE is equal to millis = -1
  RR_COMPILER_ASSERT( RR_WAIT_INFINITE == INFINITE );
    
  if ( event )
  {
    // Event is auto-reset - this clears too !
    U32 hr = WaitForSingleObject( event->i->handle, millis );
    rrassert( hr == WAIT_OBJECT_0 || hr == WAIT_TIMEOUT );
    if ( hr == WAIT_OBJECT_0 )
		return 1;
  }
  
  return 0;
}

RADDEFFUNC void         RADLINK rrEventClearManual( rrEvent * event )
{
  if ( event )
  {
    ResetEvent( event->i->handle );
  }
}

RADDEFFUNC void         RADLINK rrEventSignal( rrEvent * event )
{
  if ( event )
  {
    SetEvent( event->i->handle );
  }
}

RADDEFFUNC rrbool		RADLINK rrEventSignalAndWait( rrEvent * signalHandle, rrEvent * waitHandle, U32 millis )
{
  RR_COMPILER_ASSERT( RR_WAIT_INFINITE == INFINITE );
  
  U32 hr;
  
  #ifdef __RADNT__

  #pragma warning(disable : 4055 ) // 'type cast' : from data pointer 'void *' to function pointer 'SignalObjectAndWaitFunc (__stdcall *)'

  SignalObjectAndWaitFunc * pf = (SignalObjectAndWaitFunc *) rrAtomicLoadAcquirePointer(&soaw);
  if ( (UINTa) pf <= 1 )
  {
    if ( pf == 0 )
    {
      HANDLE kernel32Handle = GetModuleHandle(_T("KERNEL32") );
      if (kernel32Handle != 0)
      {
        pf = (SignalObjectAndWaitFunc*) GetProcAddress( kernel32Handle, "SignalObjectAndWait" );
      }
      if ( pf == 0 )
        pf = (SignalObjectAndWaitFunc *) (UINTa)1;

      rrAtomicStoreReleasePointer(&soaw,pf);
    }
    
    if ( (UINTa)pf == 1 )
    {
      rrEventSignal(signalHandle);
      return rrEventWaitAndClear(waitHandle,millis);
    }
  }
  
  hr = (*pf)( signalHandle->i->handle, waitHandle->i->handle, millis, FALSE ); 
  
  #else
  
  hr = SignalObjectAndWait( signalHandle->i->handle, waitHandle->i->handle, millis, FALSE ); 

  #endif
  
  RR_ASSERT( hr == WAIT_OBJECT_0 || hr == WAIT_TIMEOUT );
  if ( hr == WAIT_OBJECT_0 )
    return 1;

  return 0;
}

/*
RADDEFFUNC rrbool       RADLINK rrEventIsSet( rrEvent * event )
{
  if ( event )
  {
    // wait for 0 millis = test
    DWORD ret = WaitForSingleObject( event->i->handle, 0 );
    return (ret == WAIT_OBJECT_0);
  }

  return 0;
}
*/

//-----------------------------------------------------------------------------------------------------
// BH : semaphore stuff

typedef struct rriSemaphore
{
	HANDLE hSemaphore;
} rriSemaphore;

RADDEFFUNC rrbool RADLINK rrSemaphoreCreate( rrSemaphore * rr, S32 initialCount, S32 maxCount )
{
	rriSemaphore *sem;

#ifdef _DEBUG
	typedef char test[ ( sizeof ( rrSemaphore ) >= ( sizeof( rriSemaphore ) + 4 + 15 ) ) ? 1 : -1 ];  // make sure our rrSemaphore isn't too big
#endif

	sem = (rriSemaphore*) ( ( ( (UINTa) rr->data ) + 15 ) & ~15 );
	rr->i = sem;

    RR_ASSERT_LITE( maxCount > 0 );

#ifdef __RADSEKRIT__
  rr->i->hSemaphore = CreateSemaphoreExW(0, initialCount, maxCount, NULL, 0, SEMAPHORE_ALL_ACCESS);
#else
	rr->i->hSemaphore = CreateSemaphore( NULL, initialCount, maxCount, NULL );
#endif

	return rr->i->hSemaphore != INVALID_HANDLE_VALUE;
}

RADDEFFUNC void RADLINK rrSemaphoreDestroy( rrSemaphore * rr )
{
  if ( ( rr ) && ( rr->i->hSemaphore != INVALID_HANDLE_VALUE ) )
  {
	  CloseHandle( rr->i->hSemaphore );
	  rr->i->hSemaphore = INVALID_HANDLE_VALUE;
  }
}

RADDEFFUNC rrbool RADLINK rrSemaphoreDecrementOrWait( rrSemaphore * rr, U32 ms )
{
	DWORD dwWaitResult;

	if ( !rr )
		return 0;

	dwWaitResult = WaitForSingleObject( rr->i->hSemaphore, ( ms == RR_WAIT_INFINITE ) ? INFINITE : ms );

	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
	case WAIT_ABANDONED:
		return 1;
	case WAIT_TIMEOUT:
	default:
		return 0;
	}
}

RADDEFFUNC void RADLINK rrSemaphoreIncrement( rrSemaphore * rr, S32 count )
{
	if ( !rr )
	{
		return;
	}

	ReleaseSemaphore( rr->i->hSemaphore, count, NULL );
}

//#define TEST
#ifdef TEST

#include <stdio.h>

U32 RADLINK tf( void * user_data )
{
  printf("thread %d %d %X %s\n", rrThreadIndex(0), (U32)rrThreadUnique(0), rrThreadCurrent(),rrThreadName(0) );
  Sleep(100);
  return 67;
}

static char name[]="This is crazy long name";

void main( int argc, char ** argv )
{
  int i;
  rrThread t1;
  rrThread t2;
  rrMutex m;

  printf("start\n");
  for(i=0;i<1;i++)
  {
    printf("threads: %d cores: %d\n",rrThreadGetNumThreads(), CpuNumCores() );
    rrThreadCreate(&t1,tf,8192, 0,0,name+i);
rrThreadSetCPUCore( &t1, RR_THREAD_AUTO_ASSIGN_CORE );
    printf("threads: %d cores: %d\n",rrThreadGetNumThreads(), CpuNumCores() );

    rrThreadCreate(&t2,tf,8192, 0,0,name+i);
    printf("threads: %d cores: %d\n",rrThreadGetNumThreads(), CpuNumCores() );

    rrThreadWaitDone( &t1, RR_WAIT_INFINITE, 0 );
    rrThreadCleanUp( &t1 );

    rrThreadWaitDone( &t2, RR_WAIT_INFINITE, 0 );
    rrThreadCleanUp( &t2 );
    printf("threads: %d cores: %d\n",rrThreadGetNumThreads(), rrGetTotalCPUs() );
  }

  rrThreadCreate(&t1,tf,8192, 0,rrThreadCreateSuspended,name+i);
  Sleep(10000);
  rrThreadResume( &t1 );
  rrThreadWaitDone( &t1, RR_WAIT_INFINITE, 0 );
  rrThreadCleanUp( &t1 );


  rrMutexCreate(&m,rrMutexDontNeedTimeouts);
  rrMutexLock(&m);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,RR_WAIT_INFINITE);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,1);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,0);
  rrMutexUnlock(&m);
  rrMutexDestroy(&m);

  rrThreadSleep( 5 );

  rrThreadYield( );

  rrMutexCreate(&m,rrMutexNeedOnlyZeroTimeouts);
  rrMutexLock(&m);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,RR_WAIT_INFINITE);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,1);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,0);
  rrMutexUnlock(&m);
  rrMutexDestroy(&m);

  rrMutexCreate(&m,rrMutexNeedFullTimeouts);
  rrMutexLock(&m);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,RR_WAIT_INFINITE);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,1);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,0);
  rrMutexUnlock(&m);
  rrMutexDestroy(&m);

  rrMutexCreate(&m,rrMutexWantSpinLockTimeouts);
  rrMutexLock(&m);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,RR_WAIT_INFINITE);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,1);
  rrMutexUnlock(&m);
  rrMutexLockTimeout(&m,0);
  rrMutexUnlock(&m);
  rrMutexDestroy(&m);




  printf("end\n");

}

#endif

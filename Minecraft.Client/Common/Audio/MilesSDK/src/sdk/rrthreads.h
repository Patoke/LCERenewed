#ifndef __RADRTL_THREADS_H__
#define __RADRTL_THREADS_H__

#include "rrCore.h"
#include "rrCpu.h"

RADDEFSTART

// yo, Bink is using this now - Steady As She Goes, folks...


// jkr: we have some functions with Get in the name and some not?

struct rriThread;
struct rriMutex;
struct rriEvent;
struct rriSemaphore;

typedef struct rrThread
{
  U8 data[ 256 - sizeof( void* ) ];
  struct rriThread * i;
} rrThread;


typedef struct rrMutex
{
  U8 data[ 128 - sizeof( void* ) ];
  struct rriMutex * i;
} rrMutex;

typedef struct rrEvent
{
    U8 data[ 64 - sizeof( void* ) ];
    struct rriEvent * i;
} rrEvent;

typedef struct rrSemaphoreCreate
{
    U8 data[ 128 - sizeof( void* ) ];
    struct rriSemaphore * i;
} rrSemaphore;

// type of function for user thread main
typedef U32 (RADLINK thread_function)( void * user_data );


#define RR_WAIT_INFINITE (~(U32)(0))

#if defined __RADARM__ && defined __RADLINUX__ && defined __ARM_ARCH_6__
#define RR_MAX_THREADS 32  // max range of thread indexes (actually 0 is reserved, so one less)
#else
#define RR_MAX_THREADS 64  // max range of thread indexes (actually 0 is reserved, so one less)
#endif

enum rrThreadPriority  // threads start at same level as the thread that created them
{
    rrThreadLowest  = -2,
    rrThreadLow     = -1,
    rrThreadNormal  =  0,
    rrThreadHigh    =  1,
    rrThreadHighest =  2,

    rrThreadOneHigherThanCurrent = 16,  //one higher than current level for that thread
    rrThreadOneLowerThanCurrent = 17,   //one lower than current level for that thread

    rrThreadSameAsCalling = 18,         //set to calling thread level (which is the thread creation default)
    rrThreadOneHigherThanCalling = 19,  //one higher than calling thread's level
    rrThreadOneLowerThanCalling = 20    //one lower than calling thread's level
};

#if defined(__RADWII__) || defined(__RADWIIU__)
#define RR_CAN_ALLOC_THREAD_STACK  // only usable on wii(u) so far
#endif

// thread functions
enum rrThreadCreateFlags
{
  rrThreadCreateDefault         = 0,
  rrThreadCreateSuspended       = 1,
  rrThreadNeedWaitDoneTimeOuts  = 2,
  rrThreadDontNeedWaitDoneAtAll = 4
#ifdef RR_CAN_ALLOC_THREAD_STACK  
  , rrThreadStackAfterThread    = 8  
#endif 

#ifdef __RADPSP2__
  , rrThreadCreateNGPGeneralCorePool = 16 
  // This signals rrThreadCreate to create the thread in the general pool, instead of the single core pool,
  // since the vita can't move threads between those two thread pools. (as of 2012.03.16)
#elif defined __RAD3DS__
  , rrThreadCreate3DSSystemCore = 16
  // Same as PSP2 - can't assign a thread to the system core after create, so pass this to create it
  // on the system core. NOTE that during system init SetApplicationCpuTimeLimit() needs to be called or this will fail.
#endif
};

#ifndef RR_THREAD_TYPES_ONLY

// create a new thread (create suspended if you want to set priority or core before it starts)
// NOTE: stack_size should never be 0 and will trigger an assert
// 
// If you want to set the stack to something that is based on OS specific minimums, here's
// the current information as of 9/19/2012:
//
// Xenon: 16Kb will always be allocated even if you specify less.  Stack size is rounded to 
//        multiple of 4K.  If it's a multiple of 64K will use 64K pages, otherwise it uses
//        4K pages.  By default will inherit stack size of parent executable.
// Windows: Threads inherit stack size of parent executable unless otherwise specified.  Default
//        is 1MB unless overridden by /STACKSIZE.  Granularity is typically 64K.
// PS3:   Rounded up to 4K pages, defaults to 16K (as per pthread_attr_setstacksize docs)
// WiiU:  No default, if you pass 0 for stack size to OSCreateThread it just fails.
// iOS:   Primary thread has 1MB stack by default, child threads are 512K.
// OS X:  pthread_attr_setstacksize must be at least PTHREAD_STACK_MIN (defined in <limits.h> and
//        currently 8K).  Default process stack size is 8MB, but child threads have a default stack
//        size of 512K (using pthreads) or 4K (using Carbon MPTasks).
// Linux: pthread_attr_setstacksize must be at least PTHREAD_STACK_MIN (16K) and on some systems
//        it must be a multiple of the system page size.  The default stack on Linux/x86-32 is 2MB,
//        and the max stack space is 8MB typically (verify with ulimit -s).
RADDEFFUNC rrbool RADLINK rrThreadCreate( rrThread * rr, thread_function * function, U32 stack_size, void * user_data,
                                          S32 flags RADDEFAULT( rrThreadCreateDefault ),
                                          char const * name  RADDEFAULT( 0 ) );

// returns the thread handle on windows, pthread_t on mac, sys_ppu_thread_t on ps3...
RADDEFFUNC rrbool RADLINK rrThreadGetPlatformHandle( rrThread * thread, void * out );


// used to start thread if you created it suspended
RADDEFFUNC void RADLINK rrThreadResume( rrThread * rr );


// wait for a thread to finish (must still call cleanup after)
RADDEFFUNC rrbool       RADLINK rrThreadWaitDone( rrThread * rr,
                                                  U32 ms RADDEFAULT( RR_WAIT_INFINITE ),
                                                  U32 * ecp RADDEFAULT( 0 ));  // ecp is null, or pointer to return exit code


// cleans up thread resourses (can be called from the thread or before the thread finished as long as no other rr calls are made)
//   basically, if you call this function for a running thread, that thread is no longer addressable by this API, and it will
//   clean itself up as it exits (like a DETACH in linux)
// NORMALLY, you just call this function after calling rrThreadWaitDone to clean up the thread (it's a separate call, because
//   you might want to access the exit code, or the TLS entries for that thread *after* waiting for it to be done, but
//   *before* it is destroyed.
RADDEFFUNC void         RADLINK rrThreadCleanUp( rrThread * handle );

// set the thread priority
RADDEFFUNC void         RADLINK rrThreadSetPriority( rrThread * handle, S32 pri );


// set the core to run on (not all platforms support this). AUTO_ASSIGN chooses a reasonable default
#define RR_THREAD_AUTO_ASSIGN_CORE -1
RADDEFFUNC void         RADLINK rrThreadSetCPUCore( rrThread * handle, S32 core_assign );


// how many active rrthreads are running?
RADDEFFUNC U32          RADLINK rrThreadGetNumThreads( void );


// get globally-unique thread index 1..RR_MAX_THREADS-1
//   returns 0 for any thread that isn't an rrthread
RADDEFFUNC U32          RADLINK rrThreadIndex ( rrThread * handle ); // 0 for current thread


// get entire program run globally-unique thread value (incrementing 64 bit number)
//   returns 0 for any thread that isn't an rrthread
RADDEFFUNC U64          RADLINK rrThreadUnique( rrThread * handle ); // 0 for current thread


// returns the current running rrthread pointer
//   returns 0 for any thread that isn't an rrthread
RADDEFFUNC rrThread *   RADLINK rrThreadCurrent( void );


// get the string name
//   returns 0 for any thread that isn't an rrthread
RADDEFFUNC char const * RADLINK rrThreadName( rrThread * t );


// lets you get and set small bits of data on a rrThread thread
#define RR_MAX_EXTRA_THREAD_DATA 8
RADDEFFUNC S32 RADLINK rrThreadSetExtraData( rrThread * handle, S32 index, void * ret );
RADDEFFUNC S32 RADLINK rrThreadGetExtraData( rrThread * handle, S32 index, void ** ret );


// CB 07/05/11 : added rrOSThread
//	it's just the low level os handle
//	it lets you do things on threads that are not rrThreads
typedef U64	rrOSThreadType;
RADDEFFUNC rrOSThreadType RADLINK rrOSThreadGetCurrent();
RADDEFFUNC rrOSThreadType RADLINK rrThreadGetOSThread( rrThread * handle );
// this is the OS priority not "rrThreadPriority"
RADDEFFUNC int RADLINK  rrOSThreadGetPriority(rrOSThreadType t);
RADDEFFUNC void RADLINK rrOSThreadSetPriority(rrOSThreadType t,int pri);


#if defined(__RADWIN__)
typedef U64 rrOSThreadIDType;
RADDEFFUNC rrOSThreadIDType RADLINK rrOSThreadIDGetCurrent();
RADDEFFUNC rrOSThreadIDType RADLINK rrThreadGetOSThreadID( rrThread * handle );
RADDEFFUNC int RADLINK  rrOSThreadIDGetPriority(rrOSThreadIDType t);
RADDEFFUNC void RADLINK rrOSThreadIDSetPriority(rrOSThreadIDType t,int pri);

#ifdef CBLOOM
#define rrOSThreadType	dont use me
#define rrOSThreadGetCurrent	dont use me
#endif

#else
#define rrOSThreadIDType rrOSThreadType
#define rrOSThreadIDGetCurrent rrOSThreadGetCurrent
#define rrThreadGetOSThreadID rrThreadGetOSThread
#define rrOSThreadIDGetPriority rrOSThreadGetPriority
#define rrOSThreadIDSetPriority rrOSThreadSetPriority
#endif

#endif // RR_THREAD_TYPES_ONLY

// mutex functions
enum rrMutexType
{
  rrMutexDontNeedTimeouts     = 0,  // don't need to call rrMutexLockTimeout
  rrMutexNeedOnlyZeroTimeouts = 1,  // need to be able to call rrMutexLockTimeout with 0 ms (test)
  rrMutexNeedFullTimeouts     = 2,  // need to be able to call rrMutexLockTimeout with any ms
  rrMutexWantSpinLockTimeouts = 3   // want to call rrMutexLockTimeout with any ms, but use spinlocks (userland waiting)
};

#ifndef RR_THREAD_TYPES_ONLY

RADDEFFUNC rrbool       RADLINK rrMutexCreate( rrMutex * rr, S32 mtype RADDEFAULT( rrMutexDontNeedTimeouts ) );
RADDEFFUNC void         RADLINK rrMutexLock( rrMutex * handle);
RADDEFFUNC rrbool       RADLINK rrMutexLockTimeout( rrMutex * handle, U32 ms );
RADDEFFUNC void         RADLINK rrMutexUnlock( rrMutex * handle );
RADDEFFUNC void         RADLINK rrMutexDestroy( rrMutex * handle );

#endif //RR_THREAD_TYPES_ONLY

// semaphore functions
enum rrSemaphoreMaxCount { rrSemaphoreMaxCount_Default = 1<<29 };

#ifndef RR_THREAD_TYPES_ONLY

RADDEFFUNC rrbool       RADLINK rrSemaphoreCreate( rrSemaphore * rr, S32 initialCount RADDEFAULT(0), S32 maxCount RADDEFAULT(rrSemaphoreMaxCount_Default) );
RADDEFFUNC void         RADLINK rrSemaphoreDestroy( rrSemaphore * rr );
RADDEFFUNC rrbool       RADLINK rrSemaphoreDecrementOrWait( rrSemaphore * rr, U32 ms RADDEFAULT(RR_WAIT_INFINITE) );
RADDEFFUNC void         RADLINK rrSemaphoreIncrement( rrSemaphore * rr, S32 cnt RADDEFAULT(1) );


// sleeps for the specified number of ms
// CB : note : behavior Sleep(0) is undefined and may vary by platform
//	try to avoid using it
RADDEFFUNC void         RADLINK rrThreadSleep( U32 ms );

//RADDEFFUNC void         RADLINK rrThreadYield( void );
// rrThreadYield is gone

// rrThreadYieldToAny gives up execution if any other thread can run
RADDEFFUNC void         RADLINK rrThreadYieldToAny( void );

//  rrThreadYieldNotLower only sleeps to threads of an equal or higher priority (platform dependent)
RADDEFFUNC void         RADLINK rrThreadYieldNotLower( void );

// CB : rrThreadSpinHyperYield should be called in spin loops to yield hyper-threads
//  on a non-hyper threaded system it is a NOP so it should always be called in spins
RADDEFFUNC void         RADLINK rrThreadSpinHyperYield( void );

//-------------------------------------------------------------------
// Event is a light object for threads to wait on and wake each other with
//  these events are *auto reset* so when you wake up they are cleared
//
//  be careful about race conditions if you check and clear, you generally need to wrap that operation in a Mutex

RADDEFFUNC rrbool       RADLINK rrEventCreate( rrEvent * rr );
RADDEFFUNC void         RADLINK rrEventDestroy( rrEvent * handle );
RADDEFFUNC void         RADLINK rrEventSignal( rrEvent * handle );

// ClearManual clears event : this can cause race conditions with the signaller, so use with care
//  usually should only be done inside a critsec that blocks the other thread from signalling the event
RADDEFFUNC void         RADLINK rrEventClearManual( rrEvent * handle );

// Wait until signaled or millis expired; use RR_WAIT_INFINITE to wait forever ; auto-clears if set
//	returns true if signalled, false if timed out
RADDEFFUNC rrbool         RADLINK rrEventWaitAndClear( rrEvent * handle , U32 millis RADDEFAULT( RR_WAIT_INFINITE ) );

RADDEFFUNC rrbool		RADLINK rrEventSignalAndWait( rrEvent * signalHandle, rrEvent * waitHandle, U32 millis RADDEFAULT( RR_WAIT_INFINITE ) );

#endif // RR_THREADS_TYPES_ONLY

RADDEFEND

//===================================

#ifndef RR_THREAD_TYPES_ONLY

#ifdef __cplusplus

/**

rrHoldMutexInScope takes mutex on construct and releases it on destruct (end of scope)

**/

#if !defined(_MSC_VER) || _MSC_VER > 1200

class rrHoldMutexInScope
{
public:
    explicit rrHoldMutexInScope(rrMutex * mut) : m_mutex(mut) { if ( mut ) rrMutexLock(mut); }
    ~rrHoldMutexInScope() { UnLock(); }

    void Lock(rrMutex * mut) { UnLock(); m_mutex = mut; if ( mut ) rrMutexLock(mut); }
    void UnLock() { if ( m_mutex ) rrMutexUnlock(m_mutex); m_mutex = 0; }

    void ChangeMutex(rrMutex * mut) { UnLock(); m_mutex = mut; }

    rrMutex * m_mutex;
};

#define RR_SCOPE_MUTEX(mut) rrHoldMutexInScope RR_NUMBERNAME(mutexscoper) (mut)
// use like RR_SCOPE_MUTEX(my_mutex)

#endif // _MSC_VER
#endif // __cplusplus
#endif // !RR_THREAD_TYPES_ONLY

//===============================================

#endif

#include "rrTime.h"
#include "rrAtomics.h"

#include <windows.h>

#pragma warning(push)
#pragma warning(disable:4035) // no return value

#ifdef __RADX64__

  #define our_rdtsc() __rdtsc()

#else
  static inline U64 our_rdtsc()
  { 
      __asm rdtsc;
      // eax/edx returned
  }
#endif

#ifdef YieldProcessor
  #define doyield()  YieldProcessor()
#else
  #define doyield() _mm_pause()
#endif  


// @cdep pre $requiresbinary(winmm.lib)

static S64 volatile adjust_qpc;
static U32 volatile init_lock = 0;  // 0 = need to init, 1 = initted, 2 = currently initting
static U32 volatile lock_count = 0; 
static U64 volatile last_largest_qpc;

static S32 tgt = 0; // use timegettime?
static U32 first_tgt;

static U64 first_qpc;
static U32 first_gtc;
static U64 last_qpc;
static U32 last_gtc;
static U64 freq;
static F64 inv_freq;
static S64 time_of_200_ms;
static U64 time_of_100_seconds;

static U64 first_rdtsc;

// On durango, we don't use any winmm functions...
#ifdef __RADSEKRIT__
#define timeGetTime() 0
#define timeBeginPeriod( v ) ( v )
#define GetTickCount() 0
#endif

#define YIELDCOUNT 300
#define SLEEPCOUNT 302

#ifdef TIMETEST
  #include <stdio.h>
  static U32 volatile spins;
#endif


static void GetBases( void )
{
  U32 count = 0;
  
  // get the init lock
  for(;;)
  {
    U32 val;
    
    val = rrAtomicCmpXchg32( &init_lock, 2, 0 );
    if ( val == 1 )
      return;
    if ( val == 0 )
      break;

    if ( count > SLEEPCOUNT )
    { 
      count = 0;
      Sleep( 1 );
    }
    else
    {
      ++count;
      doyield();
      if ( count > YIELDCOUNT )
        Sleep(0);
    }

  }
  
  // we have the lock
  
  // have we been told to use timegettime?
#ifndef __RADSEKRIT__
  char buf[ 16 ];
  if ( GetEnvironmentVariable( "RAD_USE_TGT", buf, 16 ) )
  {
    if ( buf[ 0 ] == '1' )
      tgt = 1;
  }
#else
  // Never use tgt on durango
  tgt = 0;
#endif

  if ( ( tgt == 0 ) && ( QueryPerformanceFrequency( (LARGE_INTEGER*) &freq ) ) )
  {
    inv_freq = ( 1.0 / (F64) freq );
    time_of_200_ms = ( 200 * freq + ((1000/2)-1) ) / 1000;
    time_of_100_seconds = ( 100 * 1000 * freq + ((1000/2)-1) ) / 1000;
    QueryPerformanceCounter( (LARGE_INTEGER*) &first_qpc );
  } 
  else
  {
    // use tgt, if qpf fails
    timeBeginPeriod( 1 );
    tgt = 1;
    freq = 1000;
    inv_freq = ( 1.0 / 1000.0f );
    time_of_200_ms = 200;
    time_of_100_seconds = 100 * 1000;
  }
  
  first_gtc = GetTickCount();
  first_tgt = timeGetTime();
  first_rdtsc = our_rdtsc();
  
  adjust_qpc = 0;
  last_qpc = 0;
  last_largest_qpc = 0;
  last_gtc = 0;
  
  // clear lock (which write barriers)
  rrAtomicStoreRelease32( &init_lock, 1 );
}

static U64 our_get_time( void )
{
  U32 count;
  U64 qpc, ll_qpc;

  if ( RAD_UNLIKELY( tgt ) )
  {
    // if using timegettime - simulate a 64-bit ms timer
    U32 tgt;
    qpc = rrAtomicLoadAcquire64( &last_largest_qpc );
    tgt = timeGetTime() - first_tgt;

    // if we have wrapped, bump the high word - we can alias if
    //   you don't call this function at least once every 12 days
    if ( ( tgt < (U32) qpc ) && ( ( ( tgt - (U32) qpc ) ) < 0x40000000 ) )
    {
      qpc += 0x100000000UL;
    }
    qpc = ( qpc & 0xffffffff00000000UL ) | (U64) tgt;
  }
  else
  {
    // otherwise use QPC
    QueryPerformanceCounter( (LARGE_INTEGER*)&qpc );
    
    // adjust for start time
    qpc -= first_qpc;
  }
  
  // get the largest current qpc (using atomic add to get atomic read)
  ll_qpc = rrAtomicLoadAcquire64( &last_largest_qpc );

  // is this read smaller than our largest current?
  if ( qpc < ll_qpc )
  {
    // return previous value plus the adjust amount and return
    return ll_qpc + rrAtomicLoadAcquire64( (U64 volatile*) &adjust_qpc );
  }
   
  // update the last_largest_qpc atomically to new largest value
  for(;;)
  {
    ll_qpc = rrAtomicLoadAcquire64( &last_largest_qpc );
    
    // is the current value already larger? if so, continue...
    if ( ll_qpc >= qpc )
      break;
    // did we successfully update? if so, continue...
    if ( rrAtomicCmpXchg64( &last_largest_qpc, qpc, ll_qpc ) == ll_qpc )
      break;
    
    // spin on hyperthread machines (this is rare)
    doyield();

    #ifdef TIMETEST
      rrAtomicAddExchange32( &spins, 1 );
    #endif
  }
  
  // now, we check to see if the qpc drastically varies from the gettickcount
  //   this is for oldish buggy AMD motherboards - we only do this check on
  //   the first synchronous thread calling this function.
  //   since you usually are getting time infrequently, this code almost 
  //   always runs.

  count = rrAtomicAddExchange32( &lock_count, 1 );
  if ( RAD_LIKELY ( count == 0 ) )
  {
    U64 dqpc;
    U32 dgtc;
    U32 gtc;
    
    gtc = GetTickCount() - first_gtc;
    dgtc = gtc - last_gtc;
  
    // don't allow backwards samples from either time
    if ( RAD_UNLIKELY ( ( qpc < last_qpc ) || ( dgtc > 0xc0000000 ) ) )
    {
      U64 ret = last_qpc + adjust_qpc;
      
      // release lock count
      rrAtomicAddExchange32( &lock_count, -1 );
      
      // return previous value plus the adjust amount and return
      return ret;
    }
    
    dqpc = qpc - last_qpc;
    
    // only do the two timer check, if the delta between calls is 
    //   less than 100 seconds (where gettickcount will never alias)
    if ( dqpc < time_of_100_seconds )
    {
      S64 diff;
  
      diff = (U64) ( ( dgtc * freq + ((1000/2)-1) ) / 1000 ) - dqpc;

      // have the two timers moved differently by more than 200 ms?
      if ( RAD_UNLIKELY ( ( diff > time_of_200_ms ) || ( -diff > time_of_200_ms ) ) )
      {
        // add in the different to now always adjust the QPC by.  That is,
        //   GetTickCount is considered more accurate, so if they are different
        //   we use GetTickCount() to skew the QPC value from then on.
        rrAtomicStoreRelease64( (U64 volatile*) &adjust_qpc, adjust_qpc + diff );
      }
    }

    // update the old values
    last_qpc = qpc;
    last_gtc = gtc;
    
    // add in the adjustment before we release the lock
    qpc += adjust_qpc;
    
    // release lock count
    rrAtomicAddExchange32( &lock_count, -1 );

    // return new count (adjust added in from above)
    return qpc;
  }
  
  // return new count plus the adjust amount and return
  return qpc + rrAtomicLoadAcquire64( (U64 volatile*) &adjust_qpc );
}

//  it's the slower reliable timer 
RADDEFFUNC U64 rrGetTime( void )
{
  if ( RAD_UNLIKELY ( rrAtomicLoadAcquire32( &init_lock ) != 1 ) )
  	GetBases();
	
  return our_get_time();
}

//  fast high resolution timer
RADDEFFUNC U64 rrGetTicks( void )
{
  if ( RAD_UNLIKELY ( rrAtomicLoadAcquire32( &init_lock ) != 1 ) )
  	GetBases();

  U64 ticks = our_rdtsc();
    
	ticks -= first_rdtsc;

  return ticks;
}

// converters
RADDEFFUNC F64_OR_32 rrTimeToSeconds(U64 time)
{
  if ( RAD_UNLIKELY ( rrAtomicLoadAcquire32( &init_lock ) != 1 ) )
    GetBases();

	return ( (F64) time ) * inv_freq;
}

RADDEFFUNC U64 rrTimePerSecond( void )
{
  if ( RAD_UNLIKELY ( rrAtomicLoadAcquire32( &init_lock ) != 1 ) )
    GetBases();

  return freq;
}

RADDEFFUNC U64 rrTicksToCycles(U64 ticks)
{
	return ticks;
}

// speedstep will screw this up
RADDEFFUNC F64_OR_32 rrTicksToSeconds(U64 ticks)
{
	// time and ticks are both deltas from start, so we can use them to tell rate :
	U64 curTime  = rrGetTime();
	U64 curTicks = rrGetTicks();
	
	return ( ( (F64) ticks ) * ( ( (F64) curTime ) / ( (F64) curTicks ) ) ) * inv_freq;
}


RADDEFFUNC U64 rrTimeToMillis(U64 time)
{
  if ( RAD_UNLIKELY ( rrAtomicLoadAcquire32( &init_lock ) != 1 ) )
    GetBases();

  if ( RAD_UNLIKELY ( tgt ) )
    return time;
  
  return ( time * 1000 + ((freq/2)-1) ) / freq;
}


RADDEFFUNC U64 rrTimeToMicros(U64 time)
{
  if ( RAD_UNLIKELY ( rrAtomicLoadAcquire32( &init_lock ) != 1 ) )
    GetBases();

  if ( RAD_UNLIKELY ( tgt ) )
    return time * 1000;
  
  return ( time * 1000000 + ((freq/2)-1) ) / freq;
}


RADDEFFUNC U64 rrMillisToTime(U64 millis)
{
  if ( RAD_UNLIKELY ( rrAtomicLoadAcquire32( &init_lock ) != 1 ) )
    GetBases();

  if ( RAD_UNLIKELY ( tgt ) )
    return millis;
  
  return ( millis * freq + ((1000/2)-1) ) / 1000;
}


RADDEFFUNC U64 rrMicrosToTime(U64 micros)
{
  if ( RAD_UNLIKELY ( rrAtomicLoadAcquire32( &init_lock ) != 1 ) )
    GetBases();

  if ( RAD_UNLIKELY ( tgt ) )
    return ( micros + ((1000/2)-1) ) / 1000;
  
  return ( micros * freq + ((1000000/2)-1) ) / 1000000;
}


//==================
extern"C" { typedef BOOL WINAPI QueryThreadCycleTimeFunc(
  #if _MSC_VER > 1300
  __in
  #endif
         HANDLE ThreadHandle,
  #if _MSC_VER > 1300
  __out
  #endif
         PULONG64 CycleTime
);
};

static QueryThreadCycleTimeFunc * s_qtctf = (QueryThreadCycleTimeFunc *)-1;

static void internalQueryThreadCycleTime(HANDLE ThreadHandle,PULONG64 CycleTime)
{
#ifdef __RADSEKRIT__
  s_qtctf = 0;
#else
  if ( s_qtctf == (QueryThreadCycleTimeFunc *)-1  )
  {
    s_qtctf = (QueryThreadCycleTimeFunc*) GetProcAddress( GetModuleHandle("KERNEL32" ), "QueryThreadCycleTime" );
    //RR_ASSERT( s_qtctf != NULL );
  }
#endif
  
  if ( s_qtctf )
  {
    (*s_qtctf)(ThreadHandle,CycleTime);
  }
  else
  {
    *CycleTime = rrGetTicks();
  }
}

RADDEFFUNC U64 rrGetThreadTicks()
{
  ULONG64 ret;
  internalQueryThreadCycleTime( GetCurrentThread(), &ret );
  return ret;
}


#ifdef TIMETEST

//include "rrThreads.h"

#define NUM  10000
#define THRS 24

U64 ts[NUM*THRS];
U64 tm[THRS]={0};
U64 tmx[THRS]={0};
U64 tmn[THRS]={0};


U32 RADLINK func( void * user_data )
{
  U32 i;
  U32 w = (U32)(UINTa)user_data;
  U64*tp = ts + ( NUM * w );
  for(i=0;i<NUM;i++)
  {
    U64 c,t;
    c = rrGetTicks();
    t = rrGetTime();
    c = rrGetTicks() - c;
    if ( tmx[w]<c )
      tmx[w]=c;
    if ( tmn[w]>c )
      tmn[w]=c;
    tm[w]+=c;
    *tp++ = t;
  }
  return 0;
}



RADDEFFUNC void testmain()
{
  U32 t, i;
  rrThread rr[THRS];

  for(t=0;t<THRS;t++)
  {
    tmn[t]=0;
    --tmn[t];
    rrThreadCreate( &rr[t], func, 65536, (void*)(UINTa)t, rrThreadCreateSuspended, "test" );
  }

  for(t=0;t<THRS;t++)
  {
    rrThreadResume( &rr[t]);
  }

  for(t=0;t<THRS;t++)
  {
    rrThreadWaitDone( &rr[t],RR_WAIT_INFINITE,0);
  }

  for( t = 0; t < THRS ; t++ )
  {
    U64*tp = ts + ( NUM * t );
    for( i = 1; i < NUM ; i++ )
    {
      ++tp;
      ;//printf("%7d %2d %3d %d %d %d %d %d\n",(U32)*tp,t,i,(U32)*tp,(U32)rrTimeToMillis(*tp),(U32)rrTimeToMicros(*tp),(U32)rrMicrosToTime(rrTimeToMicros(*tp)),(U32)(tp[0]-tp[-1]) );
    }
  }


  for( t = 0; t < THRS ; t++ )
  {
    printf("thr: %2d  ave: %4.4f  max: %8d min: %8d\n", t, ((F64)tm[t])/(F64)NUM,(U32)tmx[t],(U32)tmn[t]);
  }

  printf("spins: %d\n",spins);


}

#endif
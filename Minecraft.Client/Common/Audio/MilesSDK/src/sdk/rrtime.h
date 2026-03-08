#ifndef __RADRR_TIMEH__
#define __RADRR_TIMEH__

#include "rrCore.h"

// CB: note : U64 timers are rebased to make them start at 0
//	this is nice because U64 subtraction handles wrapping right
//	this lets you just compare to test what's newer or older
//	if you don't do this, you can't use < and > to compare ordering !!

//---------------------------------------------------------
// queries :

// rrGetTime is the slower reliable timer
//	usually on system bus frequency or something
RADDEFFUNC U64 rrGetTime( void );

// rrGetTicks is the fast high resolution timer
//	it may be reliable ; note this is not directly in units of cycles
RADDEFFUNC U64 rrGetTicks( void );

// pre-thread Ticks access (slower)
//	same units as GetTicks
RADDEFFUNC U64 rrGetThreadTicks( void );

//---------------------------------------------------------
// conversions :

RADDEFFUNC F64_OR_32 rrTimeToSeconds(U64 time);
RADDEFFUNC U64       rrTimePerSecond( void ); // returns how many units of rrtime == one sec

// note : Millis and Micros should only be used temporarily for deltas
//	you can NOT store them over time, because the U64 will overflow
//	micros runs out on the order of a few hours !
RADDEFFUNC U64 rrTimeToMillis(U64 time);
RADDEFFUNC U64 rrTimeToMicros(U64 time);
RADDEFFUNC U64 rrMillisToTime(U64 millis);
RADDEFFUNC U64 rrMicrosToTime(U64 micros);
RADDEFFUNC U64 rrMillisToTicks(U64 millis);

// Ticks to CPU HZ :
RADDEFFUNC U64 rrTicksToCycles(U64 ticks);

// note : Ticks to Time is unreliable on some platforms!
//	do not use for system timing, just for profiling/display :
RADDEFFUNC F64_OR_32 rrTicksToSeconds(U64 time);

//-----------------------------------
// TEMP : mappings to old names :


#define rrGetTSC()	rrTicksToCycles( rrGetTicks() )
#define rrGetThreadTSC()	rrTicksToCycles( rrGetThreadTicks() )
#define rrGetPerformanceCounter()	rrGetTime()
#define rrPerformanceCounterDeltaSeconds(start,end)	rrTimeToSeconds((end)-(start))
#define rrGetTSC_Scale()	(1)
#define rrGetTSC_TicksPerSec()	(1)

#if defined(_MSC_VER) && _MSC_VER >= 1300
#pragma deprecated("rrGetTSC")
#pragma deprecated("rrGetThreadTSC")
#pragma deprecated("rrGetTSC_Scale")
#pragma deprecated("rrGetTSC_TicksPerSec")
#pragma deprecated("rrGetPerformanceCounter")
#pragma deprecated("rrPerformanceCounterDeltaSeconds")
#endif

// END OLD MAPPINGS
//-----------------------------------

#ifdef __RADSPU__

// special spu interfaces :

RADDEFFUNC void spu_rrSetTime( U64 ppu_time );
RADDEFFUNC void spu_rrSyncTimeTop32( U64 ppu_time );

#endif

#endif // __RADRR_TIMEH__

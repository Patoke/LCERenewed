#include "rrCore.h"

#if defined(__RADNT__) || defined(__RADNACL__)
  #define RR_MAX_CPUS 64
  RADDEFFUNC S32 RADLINK rrGetSpinCount( void );
#elif defined (__RADMAC__)
  #define RR_MAX_CPUS 64
  RADDEFFUNC S32 RADLINK rrGetSpinCount( void );
#elif defined (__RADLINUX__)
  #if defined(ANDROID)
    // This is a guess for the moment.  4 core ARMs exist
    #define RR_MAX_CPUS 4
    #define rrGetSpinCount() 300
  #else
    #define RR_MAX_CPUS 64
    RADDEFFUNC S32 RADLINK rrGetSpinCount( void );
  #endif
#elif defined (__RADXENON__)
  #define RR_MAX_CPUS 6
  //#define rrGetSpinCount() 500
  #define rrGetSpinCount() 300
#elif defined (__RADPS3__)
  #define RR_MAX_CPUS 2
  // CB : PS3 gets upset if you spin too much; don't!
  //#define rrGetSpinCount() 300
  #define rrGetSpinCount() 60
#elif defined (__RADPSP__) || defined(__RADWII__) || defined(__RADNDS__) || defined(__RADPS2__)
  #define RR_MAX_CPUS 1
  #define rrGetTotalCPUs 1
  #define rrGetTotalSlowerCPUs 0
  #define rrGetCPUCoreByPrecedence(i) 0
  #define rrGetSlowerCPUCoreByPrecedence(i) 0
  #define rrGetSpinCount() 1
  #define NOCPUFUNCS
#elif defined (__RADPSP2__)
  #define RR_MAX_CPUS 3
  #define rrGetSpinCount() 300
#elif defined (__RAD3DS__)
  #define RR_MAX_CPUS 2
  #define rrGetSpinCount() 1
#elif defined (__RADIPHONE__)
  #define RR_MAX_CPUS 4
  #define rrGetSpinCount() 1
//  #define NOCPUFUNCS
#elif defined (__RADSPU__)
  #define RR_MAX_CPUS 1
  #define rrGetSpinCount() 1
  #define NOCPUFUNCS
#elif defined (__RADANDROID__)
  #define RR_MAX_CPUS 4
  #define rrGetSpinCount() 300
  #define NOCPUFUNCS
#elif defined (__RADWIIU__)
  #define RR_MAX_CPUS 3
  #define rrGetTotalCPUs() 3
  #define rrGetSpinCount() 1
  #define NOCPUFUNCS
#elif defined (__RADSEKRIT2__)
  #define RR_MAX_CPUS 7
  #define rrGetSpinCount() 300
#elif defined (__RADSEKRIT__)

  #define RR_MAX_CPUS 6 // not definitive, all of this is speculative.
  #define rrGetSpinCount() 300
#else
  // TODO: fill in the slower cpu functinos below, since the wiiu cores are asymetrical.
  #error Have not thought about your platform yet!
#endif

#ifndef NOCPUFUNCS

RADDEFSTART

// returns the total number of CPUS, full speed or otherwise
S32 rrGetTotalCPUs( void );

// returns the number of slower CPUS (hyperthreaded cores).
//   someday, when we have asymetrical cores, these would also
//   be in the slow list.
S32 rrGetTotalSlowerCPUs( void );

// returns the core index to use, in order of speed.  If you ask
//   for the 0th index, you will get the fastest CPU. 1st index
//   will be the second fastest, etc.  You can use these core
//   numbers to set the affinity to place the most important
//   threads on the fastest cores.  Usually this will be:
//   fast cores, and then all the hyperthreaded cores (in
//   HT alias order - so, if index 3 was the final real core,
//   then index 4 would be the first HT core that aliases
//   with index 3).  There is also logic here to keep
//   consecutive indexes together on the same core, so if
//   caching across threads is important, then use neighboring
//   indexes.
S32 rrGetCPUCoreByPrecedence( S32 index );

// returns the core index to use, in order of speed, starting
//   in the slow core section of the precedence array.  This
//   is useful when you have a non-CPU intensive task that you
//   can stick on a hyperthreaded core.  Eventually this list
//   wraps around to real cores (obviously when there are no
//   slow cores this happens immediately).
S32 rrGetSlowerCPUCoreByPrecedence( S32 index );

#ifdef __RADNT__
// feature checks
rrbool rrCPU_HasSSE2( void );
#endif

RADDEFEND

#endif

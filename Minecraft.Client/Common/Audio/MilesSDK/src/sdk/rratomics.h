#ifndef __RADRTL_ATOMICS_H__
#define __RADRTL_ATOMICS_H__

#include "rrCore.h"

//-----------------------------------------------------------

#if defined(__RADX64__) && !defined(__RADSEKRIT__)
#define __RADATOMIC128__
#include <emmintrin.h>
#define ATOMIC128 __m128i
#endif

RADDEFSTART

/**

Note on atomic ops : types usually have to be aligned to their size to ensure atomicity in all cases.
This is sometimes enforced by assert in rrAtomics.
eg. ATOMIC128 needs to be 16-byte aligned.

The best way to do this is RAD_ALIGN with __RRPTRBYTES__ or __RRTWOPTRBYTES__

**/

//-----------------------------------------------------------

// full read/write barrier :
RADDEFFUNC void RADLINK rrAtomicMemoryBarrierFull( void );

//-----------------------------------------------------------

// return *ptr; // gaurantees later reads do not move before this
U32 rrAtomicLoadAcquire32(U32 const volatile * ptr);
U64 rrAtomicLoadAcquire64(U64 const volatile * ptr);

// *ptr = val; // gaurantees earlier stores do not move afterthis
void rrAtomicStoreRelease32(U32 volatile * ptr,U32 val);
void rrAtomicStoreRelease64(U64 volatile * ptr,U64 val);

//-----------------------------------------------------------
// rr memory convention assumes that all the Exchange type functions are full fences

// Windows style CmpXchg 
// atomically { oldVal = *pDestVal; if ( *pDestVal == compareVal ) { *pDestVal = newVal; } return oldVal; }
U32 rrAtomicCmpXchg32(U32 volatile * pDestVal, U32 newVal, U32 compareVal);
U64 rrAtomicCmpXchg64(U64 volatile * pDestVal, U64 newVal, U64 compareVal);

// CAS like C++0x
// NOTE : Cmpx and CAS have different argument order convention, a little confusing
// atomically { if ( *pDestVal == *pOldVal ) { *pDestVal = newVal; return true; } else { *pOldVal = *pDestVal; return false; } }
rrbool rrAtomicCAS32(U32 volatile * pDestVal, U32 * pOldVal, U32 newVal);
rrbool rrAtomicCAS64(U64 volatile * pDestVal, U64 * pOldVal, U64 newVal);

// atomically { oldVal = *pDestVal; *pDestVal = newVal; return oldVal; };
U32 rrAtomicExchange32(U32 volatile * pDestVal, U32 newVal);
U64 rrAtomicExchange64(U64 volatile * pDestVal, U64 newVal);

// atomically { old = *pDestVal; *pDestVal += incVal; return old; }
U32 rrAtomicAddExchange32(U32 volatile * pDestVal, S32 incVal);
U64 rrAtomicAddExchange64(U64 volatile * pDestVal, S64 incVal);

//-----------------------------------------------------------
// 128 bit versions :

#ifdef __RADATOMIC128__
 ATOMIC128 rrAtomicLoadAcquire128(ATOMIC128 const volatile * const ptr);
 void rrAtomicStoreRelease128(ATOMIC128 volatile * const ptr,ATOMIC128 val);
 rrbool rrAtomicCAS128(ATOMIC128 volatile * pDestVal, ATOMIC128 * pOldVal, ATOMIC128 const * const pNewVal);
 void rrAtomicExchange128(ATOMIC128 volatile * pDestVal, ATOMIC128 * pOldVal, ATOMIC128 const * const pNewVal);
#endif

//-----------------------------------------------------------

//-----------------------------------------------------------
// make UINTa calls that select 32 or 64 :

#define rrAtomicExchange_3264       RR_STRING_JOIN(rrAtomicExchange,RAD_PTRBITS)
#define rrAtomicCAS_3264            RR_STRING_JOIN(rrAtomicCAS,RAD_PTRBITS)
#define rrAtomicCmpXchg_3264        RR_STRING_JOIN(rrAtomicCmpXchg,RAD_PTRBITS)
#define rrAtomicStoreRelease_3264   RR_STRING_JOIN(rrAtomicStoreRelease,RAD_PTRBITS)
#define rrAtomicLoadAcquire_3264    RR_STRING_JOIN(rrAtomicLoadAcquire,RAD_PTRBITS)

//-----------------------------------------------------------
// Pointer aliases that go through UINTa :

#ifdef __cplusplus

static RADINLINE void* rrAtomicLoadAcquirePointer(void* const volatile * ptr)
{
    return (void*)(UINTa) rrAtomicLoadAcquire_3264((RR_UINT3264 const volatile *)ptr);
}
static RADINLINE void rrAtomicStoreReleasePointer(void* volatile * pDest,void* val)
{
    rrAtomicStoreRelease_3264((RR_UINT3264 volatile *)pDest,(RR_UINT3264)(UINTa)val);
}
static RADINLINE rrbool rrAtomicCASPointer(void* volatile * pDestVal, void* * pOldVal, void* newVal)
{
    return rrAtomicCAS_3264((RR_UINT3264 volatile *)pDestVal,(RR_UINT3264 *)pOldVal,(RR_UINT3264)(UINTa)newVal);
}
static RADINLINE void* rrAtomicExchangePointer(void* volatile * pDestVal, void* newVal)
{
    return (void*)(UINTa) rrAtomicExchange_3264((RR_UINT3264 volatile *)pDestVal,(RR_UINT3264)(UINTa)newVal);
}
static RADINLINE void* rrAtomicCmpXchgPointer(void* volatile * pDestVal, void* newVal, void* compVal)
{
    return (void*)(UINTa) rrAtomicCmpXchg_3264((RR_UINT3264 volatile *)(pDestVal),(RR_UINT3264)(UINTa)(newVal),(RR_UINT3264)(UINTa)(compVal));
}

#else

#define rrAtomicLoadAcquirePointer(ptr)                 (void*)(UINTa) rrAtomicLoadAcquire_3264((RR_UINT3264 const volatile *)(ptr))
#define rrAtomicStoreReleasePointer(pDest,val)          rrAtomicStoreRelease_3264((RR_UINT3264 volatile *)(pDest),(RR_UINT3264)(UINTa)(val))
#define rrAtomicCASPointer(pDestVal,pOldVal,newVal)     rrAtomicCAS_3264((RR_UINT3264 volatile *)(pDestVal),(RR_UINT3264 *)(pOldVal),(RR_UINT3264)(UINTa)(newVal))
#define rrAtomicExchangePointer(pDestVal,newVal)        (void*)(UINTa) rrAtomicExchange_3264((RR_UINT3264 volatile *)(pDestVal),(RR_UINT3264)(UINTa)(newVal))
#define rrAtomicCmpXchgPointer(pDestVal,newVal,compVal) (void*)(UINTa) rrAtomicCmpXchg_3264((RR_UINT3264 volatile *)(pDestVal),(RR_UINT3264)(UINTa)(newVal),(RR_UINT3264)(UINTa)(compVal))

#endif

//-----------------------------------------------------------
// Atomics on TWO pointer-sized ints :
// TwoU3264 atomics :
// use RAD_ALIGN with RAD_TWOPTRBYTES 

#ifdef __RAD64__

#ifdef __RADATOMIC128__

RR_COMPILER_ASSERT( 2*sizeof(RR_UINT3264) == sizeof(ATOMIC128) );

static RADINLINE void rrAtomicLoadAcquireTwoU3264(RR_UINT3264 const volatile * from, RR_UINT3264 * pInto)
{
    ATOMIC128 ret = rrAtomicLoadAcquire128((ATOMIC128 const volatile *)from);
    pInto[0] = ((RR_UINT3264*)&ret)[0];
    pInto[1] = ((RR_UINT3264*)&ret)[1];
}
static RADINLINE void rrAtomicStoreReleaseTwoU3264(RR_UINT3264 volatile * pDest,RR_UINT3264 const volatile * from)
{
    rrAtomicStoreRelease128((ATOMIC128 volatile *)pDest,*((ATOMIC128 *)from));
}
static RADINLINE rrbool rrAtomicCASTwoU3264(RR_UINT3264 volatile * pDestVal, RR_UINT3264 * pOldVal, const RR_UINT3264 * pNewVal)
{
    return rrAtomicCAS128((ATOMIC128 volatile *)pDestVal,(ATOMIC128 *)pOldVal,(ATOMIC128 *)pNewVal);
}
static RADINLINE void rrAtomicExchangeTwoU3264(RR_UINT3264 volatile * pDestVal,RR_UINT3264 * pOldVal, const RR_UINT3264 * pNewVal)
{
    rrAtomicExchange128((ATOMIC128 volatile *)pDestVal,(ATOMIC128 *)pOldVal,(ATOMIC128 *)pNewVal);
}

#endif

#else

RR_COMPILER_ASSERT( 2*sizeof(RR_UINT3264) == sizeof(U64) );

static RADINLINE void rrAtomicLoadAcquireTwoU3264(RR_UINT3264 const volatile * from, RR_UINT3264 * pInto)
{
    U64 ret = rrAtomicLoadAcquire64((U64 const volatile *)from);
    //memcpy(pInto,&ret,sizeof(U64));
    *((U64 *)pInto) = ret;
    //memcpy(pInto,&ret,sizeof(U64));
}
static RADINLINE void rrAtomicStoreReleaseTwoU3264(RR_UINT3264 volatile * pDest,RR_UINT3264 const volatile * from)
{
    rrAtomicStoreRelease64((U64 volatile *)pDest,*((U64 *)from));
}
static RADINLINE rrbool rrAtomicCASTwoU3264(RR_UINT3264 volatile * pDestVal, RR_UINT3264 * pOldVal, const RR_UINT3264 * pNewVal)
{
    return rrAtomicCAS64((U64 volatile *)pDestVal,(U64 *)pOldVal,*((U64 *)pNewVal));
}
static RADINLINE void rrAtomicExchangeTwoU3264(RR_UINT3264 volatile * pDestVal, RR_UINT3264 * pOldVal, const RR_UINT3264 * pNewVal)
{
    *((U64 *)pOldVal) = rrAtomicExchange64((U64 volatile *)pDestVal,*((U64 *)pNewVal));
}

#endif

//-----------------------------------------------------------

RADDEFEND

//-----------------------------------------------------------

#endif // __RADRTL_ATOMICS_H__

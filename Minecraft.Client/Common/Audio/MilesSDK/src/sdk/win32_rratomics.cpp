#include "rrAtomics.h"

//__declspec(align(16)) int x;

#if defined(_XBOX)
  #undef S8
  #define NOD3D
  #include <xtl.h>
  #define S8 signed char
#else
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  
  #include <windows.h> 
  
  #ifdef _MSC_VER
    #pragma warning(disable:4995)
  #endif
  
  #if defined(_MSC_VER) && _MSC_VER < 1300
    #pragma warning(disable:4035)
    #include <mmintrin.h>
    #include <mmsystem.h>
  #else
    #include <emmintrin.h>
  #endif
  //#include <mmsystem.h>

  #if defined(_MSC_VER) && _MSC_VER > 1500
    #include <intrin.h>  // for _InterlockedCompareExchange128
  #endif
#endif


// add replacements for the intrinsics for losers using vc6 and 2003
#if !defined(_XENON) && !defined(_XBOX) && !defined(__RAD64__)
  #if _MSC_VER <= 1500
    // for msvc6 and vsnet2003
    extern "C" long __cdecl _InterlockedCompareExchange(volatile long *p, long n, long p_compare);
    extern "C" long __cdecl _InterlockedExchangeAdd(volatile long *p, long n);
    extern "C" long __cdecl _InterlockedExchange(volatile long *p, long n);
    extern "C" __int64 __cdecl _InterlockedCompareExchange64(volatile __int64 *dest, __int64 newv, __int64 p_compare);

    #pragma intrinsic(_InterlockedCompareExchange)
    #pragma intrinsic(_InterlockedExchangeAdd)
    #pragma intrinsic(_InterlockedExchange)

    #if _MSC_VER < 1400
    extern "C" __int64 __cdecl _InterlockedCompareExchange64(volatile __int64 *dest, __int64 newv, __int64 p_compare)
    {
      __asm
      {
        mov eax,dword ptr [p_compare]
        mov edx,dword ptr [p_compare+4]
        mov ebx,dword ptr [newv]
        mov ecx,dword ptr [newv+4]
        mov esi,dword ptr [dest]
        lock cmpxchg8b qword ptr [esi]
        // eax:edx = *dest, returned as int64
      }
    }
    #else
    #pragma intrinsic(_InterlockedCompareExchange64)
    #endif

    #if _MSC_VER < 1300
      // for msvc6
      #define ULONG_PTR DWORD
    #else
      // for vsnet2003
    #endif
  #endif
#endif


/*************************************************************************************************/
// get intrinsics in global namespace :


extern "C"
{
	// these are the *optimizer* barriers, not memory barriers :
	#if _MSC_VER >= 1400
	#define HAS_READ_BARRIER
	void _ReadBarrier(void); // ReadBarrier is VC2005
	#endif
	
	void _WriteBarrier(void);
	void _ReadWriteBarrier(void);
}

// _ReadBarrier, _WriteBarrier, and _ReadWriteBarrier
// these are just *compiler* memory operation barriers, they are NOT fences :

#if defined(_MSC_VER) && _MSC_VER < 1300
   // VC6 has no link optimization, so no worries about inlining
   // cross-file and needing compiler read/write barriers
   #define CompilerReadBarrier()
   #define CompilerWriteBarrier()
   #define CompilerReadWriteBarrier()
#else
   #pragma intrinsic(_WriteBarrier)
   #pragma intrinsic(_ReadWriteBarrier)

   #define CompilerReadWriteBarrier	_ReadWriteBarrier
   #define CompilerWriteBarrier	_WriteBarrier

   #ifdef HAS_READ_BARRIER
   #pragma intrinsic(_ReadBarrier)	// ReadBarrier is VC2005
   #define CompilerReadBarrier		_ReadBarrier	
   #else
   #define CompilerReadBarrier		_ReadWriteBarrier
   #endif
#endif

/*************************************************************************************************/

RADDEFFUNC void RADLINK rrAtomicMemoryBarrierFull( void )
{
  #ifdef MemoryBarrier
    // MemoryBarrier is in Windows.h on Vista or newer
    MemoryBarrier();
  #elif defined(__RADX64__)
    _mm_mfence();
  #else  
    volatile long temp;
    __asm lock xchg temp,eax;
  #endif
}

/*************************************************************************************************/

U32 rrAtomicLoadAcquire32(U32 volatile const * ptr)
{
	RR_ASSERT(	(((UINTa)ptr)&3) == 0 );
	// on x86, loads are Acquire :
	CompilerReadBarrier();
	U32 ret = *((volatile U32 *)ptr);
	CompilerReadBarrier();
	return ret;
}

/**

BTW there's one extra nasty issue with this.

In 32 bit builds, 64-bit values on the stack are NOT aligned to 8 bytes by the compiler automatically.  This can cause atomicity failures.

I try to have asserts about required alignment in rrAtomics, but it's something to be aware of.

I believe that statics/globals are aligned properly, which is what people normally use as atomic values

**/

U64 rrAtomicLoadAcquire64(U64 volatile const * src)
{
	RR_ASSERT(	(((UINTa)src)&7) == 0 );
#ifdef __RADX64__	
	// on x86, loads are Acquire :
	CompilerReadBarrier();
	U64 ret = *((volatile U64 *)src);
	CompilerReadBarrier();
	return ret;
#else
  U64 outv;
  __asm
  {
    mov eax,[src]
    fild qword ptr [eax]
    fistp qword ptr [outv]
  }
  return outv;
	
  // old way: (dirties the cache with a bogus write, so now we do a read with x87)
  
  // if 0, swap in 0, (eg. don't change *ptr)
	//	return old value :
	//return rrAtomicCmpXchg64(const_cast<U64 volatile *>(ptr),0,0);
#endif
}

// *ptr = val; // gaurantees earlier stores do not move afterthis
void rrAtomicStoreRelease32(U32 volatile * ptr,U32 val)
{
	RR_ASSERT(	(((UINTa)ptr)&3) == 0 );
	// on x86, stores are Release :
	CompilerWriteBarrier();
	*((volatile U32 *)ptr) = val;
	CompilerWriteBarrier();
}

void rrAtomicStoreRelease64(U64 volatile * dest,U64 val)
{
	RR_ASSERT(	(((UINTa)dest)&7) == 0 );
#ifdef __RADX64__	
	// on x86, stores are Release :
	CompilerWriteBarrier();
	*((volatile U64 *)dest) = val;
	CompilerWriteBarrier();
#else
	// on 32 bit windows
	// trick from Intel TBB : (atomic_support.asm)
	//	see http://www.niallryan.com/node/137
	// FPU can store 64 bits atomically
	__asm
	{
		mov eax, dest
		fild qword ptr [val]
		fistp qword ptr [eax]
	}
#endif
}

// Windows style CmpXchg 
U32 rrAtomicCmpXchg32(U32 volatile * pDestVal, U32 newVal, U32 compareVal)
{
	RR_ASSERT(	(((UINTa)pDestVal)&3) == 0 );
	return _InterlockedCompareExchange((LONG *)pDestVal,(LONG)newVal,(LONG)compareVal);
}

U64 rrAtomicCmpXchg64(U64 volatile * pDestVal, U64 newVal, U64 compareVal)
{
	RR_ASSERT(	(((UINTa)pDestVal)&7) == 0 );
	return _InterlockedCompareExchange64((__int64 *)pDestVal,(__int64)newVal,(__int64)compareVal);
}

// CAS like C++0x
// atomically { if ( *pDestVal == *pOldVal ) { *pDestVal = newVal; } else { *pOldVal = *pDestVal; } }
rrbool rrAtomicCAS32(U32 volatile * pDestVal, U32 * pOldVal, U32 newVal)
{
	U32 old = rrAtomicCmpXchg32(pDestVal,newVal,*pOldVal);
	if ( *pOldVal == old ) return true;
	*pOldVal = old;
	return false;
}
rrbool rrAtomicCAS64(U64 volatile * pDestVal, U64 * pOldVal, U64 newVal)
{
	U64 old = rrAtomicCmpXchg64(pDestVal,newVal,*pOldVal);
	if ( *pOldVal == old ) return true;
	*pOldVal = old;
	return false;
}

// atomically { *pOldVal = *pDestVal; *pDestVal = newVal; return *pOldVal; };
U32 rrAtomicExchange32(U32 volatile * pDestVal, U32 newVal)
{
	return _InterlockedExchange((LONG *)pDestVal,newVal);
}

U64 rrAtomicExchange64(U64 volatile * pDestVal, U64 newVal)
{
	#ifdef _WIN64
	// InterlockedExchange64 requires Vista
	return InterlockedExchange64((__int64 *)pDestVal,newVal);
	#else
	
	U64 oldVal = rrAtomicLoadAcquire64(pDestVal);
	while ( ! rrAtomicCAS64(pDestVal,&oldVal,newVal) )
	{
	}
	
	return oldVal;
	
	#endif	
}

// atomically { old = *pDestVal; *pDestVal += incVal; return old; }
U32 rrAtomicAddExchange32(U32 volatile * pDestVal, S32 incVal)
{
	return _InterlockedExchangeAdd((LONG *)pDestVal,incVal);
}

U64 rrAtomicAddExchange64(U64 volatile * pDestVal, S64 incVal)
{
	#ifdef _WIN64
	return _InterlockedExchangeAdd64((__int64 *)pDestVal,incVal);
	#else
	
	U64 oldVal = rrAtomicLoadAcquire64(pDestVal);
	for(;;)
	{
		U64 newVal = oldVal + incVal;
		if ( rrAtomicCAS64(pDestVal,&oldVal,newVal) )
			return oldVal;
	}
	
	#endif
}

#ifdef __RADATOMIC128__ 

// CB : SSE 128 bit store is *NOT* StoreStore on x86
// @@ !! this is ugly, needs work
	
void rrAtomicStoreRelease128(ATOMIC128 volatile * pTo,ATOMIC128 from)
{
	RR_ASSERT(	(((UINTa)pTo)&0xF) == 0 );

	CompilerWriteBarrier();
			
	// StoreRelease only needs sfence *before* store
	_mm_sfence();
		
	_mm_store_si128((__m128i *)pTo,*((__m128i *)&from));
}
	
ATOMIC128 rrAtomicLoadAcquire128(ATOMIC128 volatile const * ptr)
{
	RR_ASSERT(	(((UINTa)ptr)&0xF) == 0 );
		
	ATOMIC128 ret = _mm_load_si128((const __m128i *)ptr);

	_mm_lfence();
	// LoadAcquire only needs lfence *after* load
	
	CompilerReadBarrier();
	
	return ret;
}
	
#if _MSC_VER > 1500
	
rrbool rrAtomicCAS128(ATOMIC128 volatile * pDestVal, ATOMIC128 * pOldVal, ATOMIC128 const * const pNewVal)
{
	RR_ASSERT(	(((UINTa)pDestVal)&0xF) == 0 );
	RR_ASSERT(	(((UINTa)pOldVal)&0xF) == 0 );
	
	const __int64 * pNew64 = (const __int64 *)pNewVal;
		
	unsigned char did = _InterlockedCompareExchange128((__int64 *)&pDestVal,pNew64[0],pNew64[1],(__int64 *)&pOldVal);
	return (rrbool) did;
}

#else

extern "C" extern int __cdecl asm_x64_cmpxchg128( volatile void * pVal, void * pOld, const void * pNew);

rrbool rrAtomicCAS128(ATOMIC128 volatile * pDestVal, ATOMIC128 * pOldVal, ATOMIC128 const * const pNewVal)
{
	RR_ASSERT(	(((UINTa)pDestVal)&0xF) == 0 );
	RR_ASSERT(	(((UINTa)pOldVal)&0xF) == 0 );
	RR_ASSERT(	(((UINTa)pNewVal)&0xF) == 0 );

	int did = asm_x64_cmpxchg128(pDestVal,pOldVal,pNewVal);
	return (rrbool) did;
}

#endif


void rrAtomicExchange128(ATOMIC128 volatile * pDestVal, ATOMIC128 * pOldVal, ATOMIC128 const * const pNewVal)
{
	*pOldVal = rrAtomicLoadAcquire128(pDestVal);
	while ( ! rrAtomicCAS128(pDestVal,pOldVal,pNewVal) )
	{
	}
}

#endif


/*

// test program to make sure fild works on x86 without bit fuckups

//#include "rrcore.h"
//#include "rrthreads.h"
//#include "rratomics.h"

#define CHUNK_SIZE 0x1000000LL

typedef struct
{
  U64 value;
  U8 space[256-8];
} count;

count t[32];

void dochunk( U64 start, U32 index )
{
  U64 i;
  U64 e = start + CHUNK_SIZE;
  for ( i = start ; i < e ; i++ )
  {
    U64 v;

    v =((i>>20LL)<<(20LL+24LL)) | ((i<<(20LL+24LL))>>(20LL+24LL));
    rrAtomicStoreRelease64( &t[index].value, v );

    if ( v != t[index].value )
      __asm int 3;
  }
}

#include <float.h>

U64 all = 0;

static U32 RADLINK thread(void* param)
{
  U32 t = (U32)param;

  _controlfp( _DN_FLUSH, _MCW_DN );
  _controlfp( _PC_24, _MCW_PC );

  for(;;)
  {
    U64 v = rrAtomicAddExchange64( &all, CHUNK_SIZE );

    if ( v >= ( 1LL << 40LL ) )
      break;

    dochunk( v, t );

    if ( t == 0 )
      printf( "\r%10I64X", v );
  }

  return 0;
}

rrThread th[32];

void main(int argc, char **argv )
{
  S32 i;
  for ( i = 0 ; i < 24 ; i++ )
  {
    rrThreadCreate( &th[i], thread, 64*1024, (void*)i, 0, "" );
  }

  for ( i = 0 ; i < 24 ; i++ )
  {
    rrThreadWaitDone(&th[i],RR_WAIT_INFINITE,0);
    rrThreadCleanUp(&th[i]);
  }
}
*/


/*

// Xenon :
               
  asm
  long         // <- Zero on failure, one on success (r3).
AtomicCAS(
  long prev,   // -> Previous value (r3).
  long next,   // -> New value (r4).
  void *addr ) // -> Location to update (r5).
{
retry:
  lwarx  r6, 0, r5 // current = *addr;
  cmpw   r6, r3    // if( current != prev )
  bne    fail      // goto fail;
  stwcx. r4, 0, r5 // if( reservation == addr ) *addr = next;
  bne-   retry     // else goto retry;
  li      r3, 1    // Return true.
  blr              // We're outta here.
fail:
  stwcx. r6, 0, r5 // Clear reservation.
  li     r3, 0     // Return false.
  blr              // We're outta here.
}

*/


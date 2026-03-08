#ifdef IS_X86

#define SSE 1
#define X86 1
//#define SSE2 1

#endif


#ifdef IS_SPU

#define DO_SIMD_INTER
#define DO_SHUFF_INTER
#define DO_SHUFF_INTER3
#define DO_SHUFF_INTER5
#define DO_SHUFF_INTER6
#define DO_SHUFF_INTER7

#define bytes16 vec_short8
#define loadbytes16( reg, addr ) reg = *( (vec_short8*) (addr) )
#define storebytes16( reg, addr ) ( *( (vec_short8*) (addr) ) = reg )

#define ibytes16( var ) vec_short8 var
#define loadibytes16( reg, addr ) reg = *( (vec_short8*) (addr) )
#define storeibytes16( reg, addr ) ( *( (vec_short8*) (addr) ) = reg )

#define ibytest16t vec_short8
#define ibytest16constt vec_uchar16
#define ibytest16shufft vec_uchar16
#define shuffle spu_shuffle 
#define toibytest16shufft(v) ( (ibytest16shufft) v )

#elif defined( IS_PS3)

#define DO_SIMD_INTER
#define DO_SHUFF_INTER
#define DO_SHUFF_INTER3
#define DO_SHUFF_INTER5
#define DO_SHUFF_INTER6
#define DO_SHUFF_INTER7

#include <altivec.h>

#define bytes16 vector signed short
#define loadbytes16( reg, addr ) reg = *( (vector signed short*) (addr) )
#define storebytes16( reg, addr ) ( *( (vector signed short*) (addr) ) = reg )

#define ibytes16( var ) vector signed short var
#define loadibytes16( reg, addr ) reg = *( (vector signed short*) (addr) )
#define storeibytes16( reg, addr ) ( *( (vector signed short*) (addr) ) = reg )

#define ibytest16t vector signed short
#define ibytest16constt vector unsigned char
#define ibytest16shufft vector unsigned char
#define shuffle vec_perm 
#define toibytest16shufft(v) ( (ibytest16shufft) v )

#elif defined( IS_XENON )

#define DO_SIMD_INTER
#define DO_SHUFF_INTER
#define DO_SHUFF_INTER3
#define DO_SHUFF_INTER5
#define DO_SHUFF_INTER6
#define DO_SHUFF_INTER7

#define bytes16 __vector4
#define loadbytes16( reg, addr ) reg = *( (__vector4*) (addr) )
#define storebytes16( reg, addr ) ( *( (__vector4*) (addr) ) = reg )

#define ibytes16( var ) __vector4 var
#define loadibytes16( reg, addr ) reg = *( (__vector4*) (addr) )
#define storeibytes16( reg, addr ) ( *( (__vector4*) (addr) ) = reg )

#define ibytest16t __vector4
#define ibytest16shufft __vector4
#define shuffle __vperm

#include <VectorIntrinsics.h>

typedef __declspec(align(16)) struct ibytest16constt {
  union {
    unsigned char u[16];
    __vector4 v;
  };
} ibytest16constt;

#define toibytest16shufft(v2) v2.v

#elif defined( SSE )

#include <xmmintrin.h> 

#define bytes16 __m128
#define loadbytes16( reg, addr ) reg = _mm_load_ps( (float*) (addr) )
#define storebytes16( reg, addr ) _mm_store_ps( (float*) (addr), reg )

#elif defined( IS_PS2 )

#define bytes16 u_long128
#define loadbytes16( dest, src )  __asm__("lq %0,%1" : "=r"(dest) : "o"(*(U8*)(src)))
#define storebytes16( src, dest )  __asm__("sq %1,%0" : "=o"(*(U8*)(dest)) : "r"(src))

#else

typedef struct r128
{
  U64 v0, v1;
} r128;

#define bytes16 r128
#define loadbytes16( reg, addr ) reg = (* (r128*) (addr) )
#define storebytes16( reg, addr ) *( (r128*) (addr) ) =  reg 

#endif



#ifdef DO_SHUFF_INTER
static const ibytest16constt c1sts16 = { 0x00, 0x01, 0x10, 0x11, 0x02, 0x03, 0x12, 0x13, 0x04, 0x05, 0x14, 0x15, 0x06, 0x07, 0x16, 0x17}; 
static const ibytest16constt c2nds16 = { 0x08, 0x09, 0x18, 0x19, 0x0a, 0x0b, 0x1a, 0x1b, 0x0c, 0x0d, 0x1c, 0x1d, 0x0e, 0x0f, 0x1e, 0x1f}; 
static const ibytest16constt c1sts32 = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17}; 
static const ibytest16constt c2nds32 = { 0x08, 0x09, 0x0a, 0x0b, 0x18, 0x19, 0x1a, 0x1b, 0x0c, 0x0d, 0x0e, 0x0f, 0x1c, 0x1d, 0x1e, 0x1f}; 
static const ibytest16constt c1sts64 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17}; 
static const ibytest16constt c2nds64 = { 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

#define interleave1sts16( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(c1sts16));
#define interleave2nds16( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(c2nds16));
#define interleave1sts32( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(c1sts32));
#define interleave2nds32( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(c2nds32));
#define interleave1sts64( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(c1sts64));
#define interleave2nds64( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(c2nds64));
#endif

#ifdef DO_SHUFF_INTER3
static const ibytest16constt cA0b0A1b1A2 = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x04, 0x05, 0x06, 0x07, 0x12, 0x13, 0x08, 0x09, 0x0a, 0x0b}; 
static const ibytest16constt cb2A3b3XXb4x = { 0x14, 0x15, 0x0c, 0x0d, 0x0e, 0x0f, 0x16, 0x17, 0x00, 0x00, 0x00, 0x00, 0x18, 0x19, 0x00, 0x00};
static const ibytest16constt cA0A1B0a6b2 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x0c, 0x0d, 0x14, 0x15}; 
static const ibytest16constt ca3b5A2b6A3b7= { 0x06, 0x07, 0x1a, 0x1b, 0x08, 0x09, 0x0a, 0x0b, 0x1c, 0x1d, 0x0c, 0x0d, 0x0e, 0x0f, 0x1e, 0x1f};

#define interleavesA0b0A1b1A2( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cA0b0A1b1A2));
#define interleavesb2A3b3XXb4x( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cb2A3b3XXb4x));
#define interleavesA0A1B0a6b2( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cA0A1B0a6b2));
#define interleavesa3b5A2b6A3b7( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca3b5A2b6A3b7));
#endif

#ifdef DO_SHUFF_INTER5
static const ibytest16constt ca0a1a2a3b0a4a5a6 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d};
static const ibytest16constt ca7xxb0b1b2b3xxb4 = { 0x0e, 0x0f, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x00, 0x00, 0x18, 0x19};
static const ibytest16constt ca0b1a2a3a4a5b2a7 = { 0x00, 0x01, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x14, 0x15, 0x0e, 0x0f};
static const ibytest16constt ca5a6a7xxb0b1b2b3 = { 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
static const ibytest16constt ca0a1a2b3a4a5a6a7 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x16, 0x17, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
static const ibytest16constt cxxa4a5a6a7xxb0b1 = { 0x00, 0x00, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13};
static const ibytest16constt cb4a1a2a3a4b5a6a7 = { 0x18, 0x19, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x1a, 0x1b, 0x0c, 0x0d, 0x0e, 0x0f};
static const ibytest16constt ca2a3b6a4a5a6a7b7 = { 0x04, 0x05, 0x06, 0x07, 0x1c, 0x1d, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x1e, 0x1f};

#define interleavea0a1a2a3b0a4a5a6( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca0a1a2a3b0a4a5a6));
#define interleavea7xxb0b1b2b3xxb4( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca7xxb0b1b2b3xxb4));
#define interleavea0b1a2a3a4a5b2a7( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca0b1a2a3a4a5b2a7));
#define interleavea5a6a7xxb0b1b2b3( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca5a6a7xxb0b1b2b3));
#define interleavea0a1a2b3a4a5a6a7( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca0a1a2b3a4a5a6a7));
#define interleavexxa4a5a6a7xxb0b1( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cxxa4a5a6a7xxb0b1));
#define interleaveb4a1a2a3a4b5a6a7( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cb4a1a2a3a4b5a6a7));
#define interleavea2a3b6a4a5a6a7b7( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca2a3b6a4a5a6a7b7));
#endif

#ifdef DO_SHUFF_INTER6
static const ibytest16constt cA0B0XXA1 = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x00, 0x00, 0x00, 0x00, 0x04, 0x05, 0x06, 0x07};
static const ibytest16constt cA0A1B0A3 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x0c, 0x0d, 0x0e, 0x0f};
static const ibytest16constt cA1B1XXA2 = { 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x00, 0x00, 0x00, 0x00, 0x08, 0x09, 0x0a, 0x0b};
static const ibytest16constt cA0A1B2A3 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x18, 0x19, 0x1a, 0x1b, 0x0c, 0x0d, 0x0e, 0x0f};
static const ibytest16constt cA2B3XXA3 = { 0x08, 0x09, 0x0a, 0x0b, 0x1c, 0x1d, 0x1e, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0d, 0x0e, 0x0f};
static const ibytest16constt cA0A1B3A3 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x1c, 0x1d, 0x1e, 0x1f, 0x0c, 0x0d, 0x0e, 0x0f};

#define interleavesA0B0XXA1( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cA0B0XXA1));
#define interleavesA0A1B0A3( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cA0A1B0A3));
#define interleavesA1B1XXA2( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cA1B1XXA2));
#define interleavesA0A1B2A3( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cA0A1B2A3));
#define interleavesA2B3XXA3( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cA2B3XXA3));
#define interleavesA0A1B3A3( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cA0A1B3A3));
#endif

#ifdef DO_SHUFF_INTER7
static const ibytest16constt ca0a1a2a3b0b1b2a4 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x08, 0x09};
static const ibytest16constt ca5a6a7b3b4b5xxxx = { 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x00, 0x00, 0x00, 0x00};
static const ibytest16constt ca0a1a2a3a4a5b0b1 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x11, 0x12, 0x13};
static const ibytest16constt ca2a3b6b7xxa4a5a6 = { 0x04, 0x05, 0x06, 0x07, 0x1c, 0x1d, 0x1e, 0x1f, 0x00, 0x00, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d};
static const ibytest16constt ca0a1a2a3b0a5a6a7 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
static const ibytest16constt ca7b1b2b3xxxxxxxx = { 0x0e, 0x0f, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const ibytest16constt ca0a1a2a3b0b1b2b3 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
static const ibytest16constt cb4b5b6a4a5a6a7b7 = { 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0e, 0x1e, 0x1f};
static const ibytest16constt cb0b1a0a1a2a3b2b3 = { 0x10, 0x11, 0x12, 0x13, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17};
static const ibytest16constt cb4a4a5a6a7b5b6b7 = { 0x18, 0x19, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

#define interleavea0a1a2a3b0b1b2a4( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca0a1a2a3b0b1b2a4));
#define interleavea5a6a7b3b4b5xxxx( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca5a6a7b3b4b5xxxx));
#define interleavea0a1a2a3a4a5b0b1( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca0a1a2a3a4a5b0b1));
#define interleavea2a3b6b7xxa4a5a6( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca2a3b6b7xxa4a5a6));
#define interleavea0a1a2a3b0a5a6a7( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca0a1a2a3b0a5a6a7));
#define interleavea7b1b2b3xxxxxxxx( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca7b1b2b3xxxxxxxx));
#define interleavea0a1a2a3b0b1b2b3( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(ca0a1a2a3b0b1b2b3));
#define interleaveb4b5b6a4a5a6a7b7( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cb4b5b6a4a5a6a7b7));
#define interleaveb0b1a0a1a2a3b2b3( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cb0b1a0a1a2a3b2b3));
#define interleaveb4a4a5a6a7b5b6b7( oreg, r1, r2 ) oreg = (ibytest16t)shuffle( r1, r2, toibytest16shufft(cb4a4a5a6a7b5b6b7));
#endif

#ifdef X86

#ifdef SSE2

#define DO_SIMD_INTER
#define ibytes16( var ) __m128i var
#define loadibytes16( reg, addr ) reg = _mm_load_si128( (__m128i*) (addr) );
#define storeibytes16( reg, addr ) _mm_store_si128( (__m128i*) (addr), reg );
#define interleave1sts16( oreg, r1, r2 ) oreg = _mm_unpacklo_epi16( r1, r2 );
#define interleave2nds16( oreg, r1, r2 ) oreg = _mm_unpackhi_epi16( r1, r2 );
#define interleave1sts32( oreg, r1, r2 ) oreg = _mm_unpacklo_epi32( r1, r2 );
#define interleave2nds32( oreg, r1, r2 ) oreg = _mm_unpackhi_epi32( r1, r2 );
#define interleave1sts64( oreg, r1, r2 ) oreg = _mm_unpacklo_epi64( r1, r2 );
#define interleave2nds64( oreg, r1, r2 ) oreg = _mm_unpackhi_epi64( r1, r2 );

#elif defined(MMX)

#define DO_SIMD_INTER
#define ibytes16( var ) __m64 var##0, var##1
#define loadibytes16( reg, addr ) reg##0 = ( (__m64*) (addr) )[ 0 ]; reg##1 = ( (__m64*) (addr) )[ 1 ];
#define storeibytes16( reg, addr ) ( (__m64*) (addr) )[ 0 ] = reg##0; ( (__m64*) (addr) )[ 1 ] = reg##1;
#define interleave1sts16( oreg, r1, r2 ) oreg##0 = _mm_unpacklo_pi16( r1##0, r2##0 ); oreg##1 = _mm_unpackhi_pi16( r1##0, r2##0 );
#define interleave2nds16( oreg, r1, r2 ) oreg##0 = _mm_unpacklo_pi16( r1##1, r2##1 ); oreg##1 = _mm_unpackhi_pi16( r1##1, r2##1 );
#define interleave1sts32( oreg, r1, r2 ) oreg##0 = _mm_unpacklo_pi32( r1##0, r2##0 ); oreg##1 = _mm_unpackhi_pi32( r1##0, r2##0 );
#define interleave2nds32( oreg, r1, r2 ) oreg##0 = _mm_unpacklo_pi32( r1##1, r2##1 ); oreg##1 = _mm_unpackhi_pi32( r1##1, r2##1 );
#define interleave1sts64( oreg, r1, r2 ) oreg##0 = r1##0; oreg##1 = r2##0;
#define interleave2nds64( oreg, r1, r2 ) oreg##0 = r1##1; oreg##1 = r2##1;

#endif

#endif



// ( each * spans ) must be less 16384
static void interleave16chunk( void * v, unsigned int each_span_len, unsigned int spans )
{
  unsigned int i, len, sp, nl;
  unsigned char m[ 256 ];
  char * nv;
    
  const unsigned char masks[ 8 ] = { 1, 2, 4, 8, 16, 32, 64, 128 };
  const unsigned short div_val[ 9 ] = { 0, 0, 0x8000, 0x5556, 0x4000, 0x3334, 0x2aab, 0x2493, 0x2000 }; // good to 13000

  len = each_span_len / 8;
  sp = spans;
  memset( m, 0, ( len * sp ) >> 3 );

  i = 0;
  nl = len;
  nv = (char*) v;
  do
  {
    unsigned int ns = sp;
    do
    {
      bytes16 c;

      if ( ( m[ i>>3 ] & masks[ i & 7 ] ) == 0 )
      {
        unsigned int s, il;
        unsigned int x;

        x = i;
        // reorder formula
        il = ( x * (unsigned int) div_val[ sp ] ) >> 16;
        s = ( ( x - ( il * sp ) ) * len ) + il;
        
        loadbytes16( c, (char*) v + x * 16 );
        while( s != i ) 
        {
          bytes16 tmp;
          
          loadbytes16( tmp, (char*) v + s * 16 );
          storebytes16( tmp, (char*) v + x * 16 );
  
          m[ x>>3 ] |= masks[ x&7 ];
          x = s;
  
          // reorder formula
          il = ( s * (unsigned int) div_val[ sp ] ) >> 16;
          s = ( ( s - ( il * sp ) ) * len ) + il;
        }
        storebytes16( c, (char*) v + x * 16 );
        m[ x>>3 ] |= masks[ x&7 ];
      }
      
      ++i;
    } while( --ns );

    switch( sp )
    {
      case 2:
      {
        #ifdef DO_SIMD_INTER
        ibytes16( v0 );
        ibytes16( v1 );
        ibytes16( o0 );
        ibytes16( o1 );

        loadibytes16( v0, nv );
        loadibytes16( v1, nv + 16 );
        interleave1sts16( o0, v0, v1 );
        interleave2nds16( o1, v0, v1 );
        storeibytes16( o0, nv );
        storeibytes16( o1, nv + 16 );
        #else
        short tmp;
        tmp = ((short*)nv)[1];
        ((short*)nv)[1] = ((short*)nv)[8];
        ((short*)nv)[8] = ((short*)nv)[4];
        ((short*)nv)[4] = ((short*)nv)[2];
        ((short*)nv)[2] = tmp;
        tmp = ((short*)nv)[3];
        ((short*)nv)[3] = ((short*)nv)[9];
        ((short*)nv)[9] = ((short*)nv)[12];
        ((short*)nv)[12] = ((short*)nv)[6];
        ((short*)nv)[6] = tmp;
        tmp = ((short*)nv)[5];
        ((short*)nv)[5] = ((short*)nv)[10];
        ((short*)nv)[10] = tmp;
        tmp = ((short*)nv)[7];
        ((short*)nv)[7] = ((short*)nv)[11];
        ((short*)nv)[11] = ((short*)nv)[13];
        ((short*)nv)[13] = ((short*)nv)[14];
        ((short*)nv)[14] = tmp;
        #endif 
        break;
      }
      case 3:
      {
        #ifdef DO_SHUFF_INTER3

        ibytes16( v0 );
        ibytes16( v1 );
        ibytes16( v2 );
        ibytes16( o0 );
        ibytes16( o1 );
        ibytes16( o2 );

        loadibytes16( v0, nv );
        loadibytes16( v1, nv + 16 );
        loadibytes16( v2, nv + 32 );

        interleave1sts16( o0, v0, v1 );
        interleave2nds16( o1, v0, v1 );
        
        interleavesA0b0A1b1A2( v0, o0, v2 );

        interleavesb2A3b3XXb4x( v1, o0, v2 );
        interleavesA0A1B0a6b2( v1, v1, o1 );
        
        interleavesa3b5A2b6A3b7( o2, o1, v2 );

        storeibytes16( o0, nv );
        storeibytes16( o1, nv + 16 );
        storeibytes16( o2, nv + 32 );
        #else
        short tmp;
        tmp = ((short*)nv)[1];
        ((short*)nv)[1] = ((short*)nv)[8];
        ((short*)nv)[8] = ((short*)nv)[18];
        ((short*)nv)[18] = ((short*)nv)[6];
        ((short*)nv)[6] = ((short*)nv)[2];
        ((short*)nv)[2] = ((short*)nv)[16];
        ((short*)nv)[16] = ((short*)nv)[13];
        ((short*)nv)[13] = ((short*)nv)[12];
        ((short*)nv)[12] = ((short*)nv)[4];
        ((short*)nv)[4] = ((short*)nv)[9];
        ((short*)nv)[9] = ((short*)nv)[3];
        ((short*)nv)[3] = tmp;
        tmp = ((short*)nv)[5];
        ((short*)nv)[5] = ((short*)nv)[17];
        ((short*)nv)[17] = ((short*)nv)[21];
        ((short*)nv)[21] = ((short*)nv)[7];
        ((short*)nv)[7] = ((short*)nv)[10];
        ((short*)nv)[10] = ((short*)nv)[11];
        ((short*)nv)[11] = ((short*)nv)[19];
        ((short*)nv)[19] = ((short*)nv)[14];
        ((short*)nv)[14] = ((short*)nv)[20];
        ((short*)nv)[20] = ((short*)nv)[22];
        ((short*)nv)[22] = ((short*)nv)[15];
        ((short*)nv)[15] = tmp;
        #endif
        break;
      }
      case 4:
      {
        #ifdef DO_SIMD_INTER
        ibytes16( v0 );
        ibytes16( v1 );
        ibytes16( v2 );
        ibytes16( v3 );
        ibytes16( o0 );
        ibytes16( o1 );
        ibytes16( o2 );
        ibytes16( o3 );

        loadibytes16( v0, nv );
        loadibytes16( v1, nv + 16 );
        loadibytes16( v2, nv + 32 );
        loadibytes16( v3, nv + 48 );
        interleave1sts16( o0, v0, v1 );
        interleave2nds16( o1, v0, v1 );
        interleave1sts16( o2, v2, v3 );
        interleave2nds16( o3, v2, v3 );

        interleave1sts32( v0, o0, o2 );
        interleave2nds32( v1, o0, o2 );
        interleave1sts32( v2, o1, o3 );
        interleave2nds32( v3, o1, o3 );
        
        storeibytes16( v0, nv );
        storeibytes16( v1, nv + 16 );
        storeibytes16( v2, nv + 32 );
        storeibytes16( v3, nv + 48 );
        #else
        short tmp;
        tmp = ((short*)nv)[1];
        ((short*)nv)[1] = ((short*)nv)[8];
        ((short*)nv)[8] = ((short*)nv)[2];
        ((short*)nv)[2] = ((short*)nv)[16];
        ((short*)nv)[16] = ((short*)nv)[4];
        ((short*)nv)[4] = tmp;
        tmp = ((short*)nv)[3];
        ((short*)nv)[3] = ((short*)nv)[24];
        ((short*)nv)[24] = ((short*)nv)[6];
        ((short*)nv)[6] = ((short*)nv)[17];
        ((short*)nv)[17] = ((short*)nv)[12];
        ((short*)nv)[12] = tmp;
        tmp = ((short*)nv)[5];
        ((short*)nv)[5] = ((short*)nv)[9];
        ((short*)nv)[9] = ((short*)nv)[10];
        ((short*)nv)[10] = ((short*)nv)[18];
        ((short*)nv)[18] = ((short*)nv)[20];
        ((short*)nv)[20] = tmp;
        tmp = ((short*)nv)[7];
        ((short*)nv)[7] = ((short*)nv)[25];
        ((short*)nv)[25] = ((short*)nv)[14];
        ((short*)nv)[14] = ((short*)nv)[19];
        ((short*)nv)[19] = ((short*)nv)[28];
        ((short*)nv)[28] = tmp;
        tmp = ((short*)nv)[11];
        ((short*)nv)[11] = ((short*)nv)[26];
        ((short*)nv)[26] = ((short*)nv)[22];
        ((short*)nv)[22] = ((short*)nv)[21];
        ((short*)nv)[21] = ((short*)nv)[13];
        ((short*)nv)[13] = tmp;
        tmp = ((short*)nv)[15];
        ((short*)nv)[15] = ((short*)nv)[27];
        ((short*)nv)[27] = ((short*)nv)[30];
        ((short*)nv)[30] = ((short*)nv)[23];
        ((short*)nv)[23] = ((short*)nv)[29];
        ((short*)nv)[29] = tmp;
        #endif
        break;
      }
      case 5:
      {
        #ifdef DO_SHUFF_INTER5
        ibytes16( v0 );
        ibytes16( v1 );
        ibytes16( v2 );
        ibytes16( v3 );
        ibytes16( v4 );
        ibytes16( o0 );
        ibytes16( o1 );
        ibytes16( o2 );
        ibytes16( o3 );
        ibytes16( o4 );

        loadibytes16( v0, nv );
        loadibytes16( v1, nv + 16 );
        loadibytes16( v2, nv + 32 );
        loadibytes16( v3, nv + 48 );
        loadibytes16( v4, nv + 64 );

        interleave1sts16( o0, v0, v1 );
        interleave2nds16( o1, v0, v1 );
        interleave1sts16( o2, v2, v3 );
        interleave2nds16( o3, v2, v3 );

        interleave1sts32( v0, o0, o2 );
        interleave2nds32( v1, o0, o2 );
        interleave1sts32( v2, o1, o3 );
        interleave2nds32( v3, o1, o3 );
        
        interleavea0a1a2a3b0a4a5a6( o0, v0, v4 );
        
        interleavea7xxb0b1b2b3xxb4( o1, v0, v1 );
        interleavea0b1a2a3a4a5b2a7( o1, o1, v4 );

        interleavea5a6a7xxb0b1b2b3( o2, v1, v2 );
        interleavea0a1a2b3a4a5a6a7( o2, o2, v4 );

        interleavexxa4a5a6a7xxb0b1( o3, v2, v3 );
        interleaveb4a1a2a3a4b5a6a7( o3, o3, v4 );

        interleavea2a3b6a4a5a6a7b7( o4, v3, v4 );
        
        storeibytes16( o0, nv );
        storeibytes16( o1, nv + 16 );
        storeibytes16( o2, nv + 32 );
        storeibytes16( o3, nv + 48 );
        storeibytes16( o4, nv + 64 );
        #else
        short tmp;
        tmp = ((short*)nv)[1];
        ((short*)nv)[1] = ((short*)nv)[8];
        ((short*)nv)[8] = ((short*)nv)[25];
        ((short*)nv)[25] = ((short*)nv)[5];
        ((short*)nv)[5] = tmp;
        tmp = ((short*)nv)[2];
        ((short*)nv)[2] = ((short*)nv)[16];
        ((short*)nv)[16] = ((short*)nv)[11];
        ((short*)nv)[11] = ((short*)nv)[10];
        ((short*)nv)[10] = tmp;
        tmp = ((short*)nv)[3];
        ((short*)nv)[3] = ((short*)nv)[24];
        ((short*)nv)[24] = ((short*)nv)[36];
        ((short*)nv)[36] = ((short*)nv)[15];
        ((short*)nv)[15] = tmp;
        tmp = ((short*)nv)[4];
        ((short*)nv)[4] = ((short*)nv)[32];
        ((short*)nv)[32] = ((short*)nv)[22];
        ((short*)nv)[22] = ((short*)nv)[20];
        ((short*)nv)[20] = tmp;
        tmp = ((short*)nv)[6];
        ((short*)nv)[6] = ((short*)nv)[9];
        ((short*)nv)[9] = ((short*)nv)[33];
        ((short*)nv)[33] = ((short*)nv)[30];
        ((short*)nv)[30] = tmp;
        tmp = ((short*)nv)[7];
        ((short*)nv)[7] = ((short*)nv)[17];
        ((short*)nv)[17] = ((short*)nv)[19];
        ((short*)nv)[19] = ((short*)nv)[35];
        ((short*)nv)[35] = tmp;
        tmp = ((short*)nv)[12];
        ((short*)nv)[12] = ((short*)nv)[18];
        ((short*)nv)[18] = ((short*)nv)[27];
        ((short*)nv)[27] = ((short*)nv)[21];
        ((short*)nv)[21] = tmp;
        tmp = ((short*)nv)[13];
        ((short*)nv)[13] = ((short*)nv)[26];
        ((short*)nv)[26] = tmp;
        tmp = ((short*)nv)[14];
        ((short*)nv)[14] = ((short*)nv)[34];
        ((short*)nv)[34] = ((short*)nv)[38];
        ((short*)nv)[38] = ((short*)nv)[31];
        ((short*)nv)[31] = tmp;
        tmp = ((short*)nv)[23];
        ((short*)nv)[23] = ((short*)nv)[28];
        ((short*)nv)[28] = ((short*)nv)[29];
        ((short*)nv)[29] = ((short*)nv)[37];
        ((short*)nv)[37] = tmp;
        #endif
        break;
      }
      case 6:
      {
        #ifdef DO_SHUFF_INTER6
        ibytes16( v0 );
        ibytes16( v1 );
        ibytes16( v2 );
        ibytes16( v3 );
        ibytes16( v4 );
        ibytes16( v5 );
        ibytes16( o0 );
        ibytes16( o1 );
        ibytes16( o2 );
        ibytes16( o3 );
        ibytes16( o4 );
        ibytes16( o5 );

        loadibytes16( v0, nv );
        loadibytes16( v1, nv + 16 );
        loadibytes16( v2, nv + 32 );
        loadibytes16( v3, nv + 48 );
        loadibytes16( v4, nv + 64 );
        loadibytes16( v5, nv + 80 );

        interleave1sts16( o0, v0, v1 );
        interleave2nds16( o3, v0, v1 );
        interleave1sts16( o1, v2, v3 );
        interleave2nds16( o4, v2, v3 );
        interleave1sts16( o2, v4, v5 );
        interleave2nds16( o5, v4, v5 );

        interleavesA0B0XXA1( v0, o0, o1 );
        interleavesA0A1B0A3( v0, v0, o2 );

        interleavesA1B1XXA2( v1, o1, o2 );
        interleavesA0A1B2A3( v1, v1, o0 );

        interleavesA2B3XXA3( v2, o2, o0 );
        interleavesA0A1B3A3( v2, v2, o1 );

        interleavesA0B0XXA1( v3, o3, o4 );
        interleavesA0A1B0A3( v3, v3, o5 );

        interleavesA1B1XXA2( v4, o4, o5 );
        interleavesA0A1B2A3( v4, v4, o3 );

        interleavesA2B3XXA3( v5, o5, o3 );
        interleavesA0A1B3A3( v5, v5, o4 );

        storeibytes16( v0, nv );
        storeibytes16( v1, nv + 16 );
        storeibytes16( v2, nv + 32 );
        storeibytes16( v3, nv + 48 );
        storeibytes16( v4, nv + 64 );
        storeibytes16( v5, nv + 80 );
        #else
        short tmp;
        tmp = ((short*)nv)[1];
        ((short*)nv)[1] = ((short*)nv)[8];
        ((short*)nv)[8] = ((short*)nv)[17];
        ((short*)nv)[17] = ((short*)nv)[42];
        ((short*)nv)[42] = ((short*)nv)[7];
        ((short*)nv)[7] = ((short*)nv)[9];
        ((short*)nv)[9] = ((short*)nv)[25];
        ((short*)nv)[25] = ((short*)nv)[12];
        ((short*)nv)[12] = ((short*)nv)[2];
        ((short*)nv)[2] = ((short*)nv)[16];
        ((short*)nv)[16] = ((short*)nv)[34];
        ((short*)nv)[34] = ((short*)nv)[37];
        ((short*)nv)[37] = ((short*)nv)[14];
        ((short*)nv)[14] = ((short*)nv)[18];
        ((short*)nv)[18] = ((short*)nv)[3];
        ((short*)nv)[3] = ((short*)nv)[24];
        ((short*)nv)[24] = ((short*)nv)[4];
        ((short*)nv)[4] = ((short*)nv)[32];
        ((short*)nv)[32] = ((short*)nv)[21];
        ((short*)nv)[21] = ((short*)nv)[27];
        ((short*)nv)[27] = ((short*)nv)[28];
        ((short*)nv)[28] = ((short*)nv)[36];
        ((short*)nv)[36] = ((short*)nv)[6];
        ((short*)nv)[6] = tmp;
        tmp = ((short*)nv)[5];
        ((short*)nv)[5] = ((short*)nv)[40];
        ((short*)nv)[40] = ((short*)nv)[38];
        ((short*)nv)[38] = ((short*)nv)[22];
        ((short*)nv)[22] = ((short*)nv)[35];
        ((short*)nv)[35] = ((short*)nv)[45];
        ((short*)nv)[45] = ((short*)nv)[31];
        ((short*)nv)[31] = ((short*)nv)[13];
        ((short*)nv)[13] = ((short*)nv)[10];
        ((short*)nv)[10] = ((short*)nv)[33];
        ((short*)nv)[33] = ((short*)nv)[29];
        ((short*)nv)[29] = ((short*)nv)[44];
        ((short*)nv)[44] = ((short*)nv)[23];
        ((short*)nv)[23] = ((short*)nv)[43];
        ((short*)nv)[43] = ((short*)nv)[15];
        ((short*)nv)[15] = ((short*)nv)[26];
        ((short*)nv)[26] = ((short*)nv)[20];
        ((short*)nv)[20] = ((short*)nv)[19];
        ((short*)nv)[19] = ((short*)nv)[11];
        ((short*)nv)[11] = ((short*)nv)[41];
        ((short*)nv)[41] = ((short*)nv)[46];
        ((short*)nv)[46] = ((short*)nv)[39];
        ((short*)nv)[39] = ((short*)nv)[30];
        ((short*)nv)[30] = tmp;
        #endif
        break;
      }
      case 7:
      {
        #ifdef DO_SHUFF_INTER7
        ibytes16( v0 );
        ibytes16( v1 );
        ibytes16( v2 );
        ibytes16( v3 );
        ibytes16( v4 );
        ibytes16( v5 );
        ibytes16( v6 );
        ibytes16( o0 );
        ibytes16( o1 );
        ibytes16( o2 );
        ibytes16( o3 );
        ibytes16( o4 );
        ibytes16( o5 );
        ibytes16( o6 );

        loadibytes16( v0, nv );
        loadibytes16( v1, nv + 16 );
        loadibytes16( v2, nv + 32 );
        loadibytes16( v3, nv + 48 );
        loadibytes16( v4, nv + 64 );
        loadibytes16( v5, nv + 80 );
        loadibytes16( v6, nv + 96 );
        
        interleave1sts16( o0, v0, v1 );
        interleave2nds16( o1, v0, v1 );
        interleave1sts16( o2, v2, v3 );
        interleave2nds16( o3, v2, v3 );
        interleave1sts16( o4, v4, v5 );
        interleave2nds16( o5, v4, v5 );

        interleave1sts32( v0, o0, o2 );
        interleave2nds32( v1, o0, o2 );
        interleave1sts32( v2, o1, o3 );
        interleave2nds32( v3, o1, o3 );

        interleavesA0b0A1b1A2( v4, o4, v6 );
        interleavesb2A3b3XXb4x( v5, o4, v6 );
        interleavesA0A1B0a6b2( v5, v5, o5 );
        interleavesa3b5A2b6A3b7( o6, o5, v6 );
        // v0-v3, interleaved by 4, v4-v6, interleaved by 3

        interleavea0a1a2a3b0b1b2a4( o0, v0, v4 );
        interleavea5a6a7b3b4b5xxxx( o1, v0, v4 );
        interleavea0a1a2a3a4a5b0b1( o1, o1, v1 );
        interleavea2a3b6b7xxa4a5a6( o2, v1, v4 );
        interleavea0a1a2a3b0a5a6a7( o2, o2, v5 );
        interleavea7b1b2b3xxxxxxxx( o3, v1, v5 );
        interleavea0a1a2a3b0b1b2b3( o3, o3, v2 );
        interleaveb4b5b6a4a5a6a7b7( o4, v2, v5 );
        interleaveb0b1a0a1a2a3b2b3( o5, v3, v6 );
        interleaveb4a4a5a6a7b5b6b7( o6, v3, v6 );
        
        storeibytes16( o0, nv );
        storeibytes16( o1, nv + 16 );
        storeibytes16( o2, nv + 32 );
        storeibytes16( o3, nv + 48 );
        storeibytes16( o4, nv + 64 );
        storeibytes16( o5, nv + 80 );
        storeibytes16( o6, nv + 96 );
        #else
        short tmp;
        tmp = ((short*)nv)[1];
        ((short*)nv)[1] = ((short*)nv)[8];
        ((short*)nv)[8] = ((short*)nv)[9];
        ((short*)nv)[9] = ((short*)nv)[17];
        ((short*)nv)[17] = ((short*)nv)[26];
        ((short*)nv)[26] = ((short*)nv)[43];
        ((short*)nv)[43] = ((short*)nv)[14];
        ((short*)nv)[14] = ((short*)nv)[2];
        ((short*)nv)[2] = ((short*)nv)[16];
        ((short*)nv)[16] = ((short*)nv)[18];
        ((short*)nv)[18] = ((short*)nv)[34];
        ((short*)nv)[34] = ((short*)nv)[52];
        ((short*)nv)[52] = ((short*)nv)[31];
        ((short*)nv)[31] = ((short*)nv)[28];
        ((short*)nv)[28] = ((short*)nv)[4];
        ((short*)nv)[4] = ((short*)nv)[32];
        ((short*)nv)[32] = ((short*)nv)[36];
        ((short*)nv)[36] = ((short*)nv)[13];
        ((short*)nv)[13] = ((short*)nv)[49];
        ((short*)nv)[49] = ((short*)nv)[7];
        ((short*)nv)[7] = tmp;
        tmp = ((short*)nv)[3];
        ((short*)nv)[3] = ((short*)nv)[24];
        ((short*)nv)[24] = ((short*)nv)[27];
        ((short*)nv)[27] = ((short*)nv)[51];
        ((short*)nv)[51] = ((short*)nv)[23];
        ((short*)nv)[23] = ((short*)nv)[19];
        ((short*)nv)[19] = ((short*)nv)[42];
        ((short*)nv)[42] = ((short*)nv)[6];
        ((short*)nv)[6] = ((short*)nv)[48];
        ((short*)nv)[48] = ((short*)nv)[54];
        ((short*)nv)[54] = ((short*)nv)[47];
        ((short*)nv)[47] = ((short*)nv)[46];
        ((short*)nv)[46] = ((short*)nv)[38];
        ((short*)nv)[38] = ((short*)nv)[29];
        ((short*)nv)[29] = ((short*)nv)[12];
        ((short*)nv)[12] = ((short*)nv)[41];
        ((short*)nv)[41] = ((short*)nv)[53];
        ((short*)nv)[53] = ((short*)nv)[39];
        ((short*)nv)[39] = ((short*)nv)[37];
        ((short*)nv)[37] = ((short*)nv)[21];
        ((short*)nv)[21] = tmp;
        tmp = ((short*)nv)[5];
        ((short*)nv)[5] = ((short*)nv)[40];
        ((short*)nv)[40] = ((short*)nv)[45];
        ((short*)nv)[45] = ((short*)nv)[30];
        ((short*)nv)[30] = ((short*)nv)[20];
        ((short*)nv)[20] = ((short*)nv)[50];
        ((short*)nv)[50] = ((short*)nv)[15];
        ((short*)nv)[15] = ((short*)nv)[10];
        ((short*)nv)[10] = ((short*)nv)[25];
        ((short*)nv)[25] = ((short*)nv)[35];
        ((short*)nv)[35] = tmp;
        tmp = ((short*)nv)[11];
        ((short*)nv)[11] = ((short*)nv)[33];
        ((short*)nv)[33] = ((short*)nv)[44];
        ((short*)nv)[44] = ((short*)nv)[22];
        ((short*)nv)[22] = tmp;
        #endif
        break;
      }
      case 8:
      {
        #ifdef DO_SIMD_INTER
        ibytes16( v0 );
        ibytes16( v1 );
        ibytes16( v2 );
        ibytes16( v3 );
        ibytes16( v4 );
        ibytes16( v5 );
        ibytes16( v6 );
        ibytes16( v7 );
        ibytes16( o0 );
        ibytes16( o1 );
        ibytes16( o2 );
        ibytes16( o3 );
        ibytes16( o4 );
        ibytes16( o5 );
        ibytes16( o6 );
        ibytes16( o7 );

        loadibytes16( v0, nv );
        loadibytes16( v1, nv + 16 );
        loadibytes16( v2, nv + 32 );
        loadibytes16( v3, nv + 48 );
        loadibytes16( v4, nv + 64 );
        loadibytes16( v5, nv + 80 );
        loadibytes16( v6, nv + 96 );
        loadibytes16( v7, nv + 112 );

        interleave1sts16( o0, v0, v1 );
        interleave2nds16( o1, v0, v1 );
        interleave1sts16( o2, v2, v3 );
        interleave2nds16( o3, v2, v3 );
        interleave1sts16( o4, v4, v5 );
        interleave2nds16( o5, v4, v5 );
        interleave1sts16( o6, v6, v7 );
        interleave2nds16( o7, v6, v7 );

        interleave1sts32( v0, o0, o2 );
        interleave2nds32( v1, o0, o2 );
        interleave1sts32( v2, o1, o3 );
        interleave2nds32( v3, o1, o3 );
        interleave1sts32( v4, o4, o6 );
        interleave2nds32( v5, o4, o6 );
        interleave1sts32( v6, o5, o7 );
        interleave2nds32( v7, o5, o7 );
        
        interleave1sts64( o0, v0, v4 );
        interleave2nds64( o1, v0, v4 );
        interleave1sts64( o2, v1, v5 );
        interleave2nds64( o3, v1, v5 );
        interleave1sts64( o4, v2, v6 );
        interleave2nds64( o5, v2, v6 );
        interleave1sts64( o6, v3, v7 );
        interleave2nds64( o7, v3, v7 );
      
        storeibytes16( o0, nv );
        storeibytes16( o1, nv + 16 );
        storeibytes16( o2, nv + 32 );
        storeibytes16( o3, nv + 48 );
        storeibytes16( o4, nv + 64);
        storeibytes16( o5, nv + 80 );
        storeibytes16( o6, nv + 96 );
        storeibytes16( o7, nv + 112 );
        #else
        short tmp;
        tmp = ((short*)nv)[1];
        ((short*)nv)[1] = ((short*)nv)[8];
        ((short*)nv)[8] = tmp;
        tmp = ((short*)nv)[2];
        ((short*)nv)[2] = ((short*)nv)[16];
        ((short*)nv)[16] = tmp;
        tmp = ((short*)nv)[3];
        ((short*)nv)[3] = ((short*)nv)[24];
        ((short*)nv)[24] = tmp;
        tmp = ((short*)nv)[4];
        ((short*)nv)[4] = ((short*)nv)[32];
        ((short*)nv)[32] = tmp;
        tmp = ((short*)nv)[5];
        ((short*)nv)[5] = ((short*)nv)[40];
        ((short*)nv)[40] = tmp;
        tmp = ((short*)nv)[6];
        ((short*)nv)[6] = ((short*)nv)[48];
        ((short*)nv)[48] = tmp;
        tmp = ((short*)nv)[7];
        ((short*)nv)[7] = ((short*)nv)[56];
        ((short*)nv)[56] = tmp;
        tmp = ((short*)nv)[10];
        ((short*)nv)[10] = ((short*)nv)[17];
        ((short*)nv)[17] = tmp;
        tmp = ((short*)nv)[11];
        ((short*)nv)[11] = ((short*)nv)[25];
        ((short*)nv)[25] = tmp;
        tmp = ((short*)nv)[12];
        ((short*)nv)[12] = ((short*)nv)[33];
        ((short*)nv)[33] = tmp;
        tmp = ((short*)nv)[13];
        ((short*)nv)[13] = ((short*)nv)[41];
        ((short*)nv)[41] = tmp;
        tmp = ((short*)nv)[14];
        ((short*)nv)[14] = ((short*)nv)[49];
        ((short*)nv)[49] = tmp;
        tmp = ((short*)nv)[15];
        ((short*)nv)[15] = ((short*)nv)[57];
        ((short*)nv)[57] = tmp;
        tmp = ((short*)nv)[19];
        ((short*)nv)[19] = ((short*)nv)[26];
        ((short*)nv)[26] = tmp;
        tmp = ((short*)nv)[20];
        ((short*)nv)[20] = ((short*)nv)[34];
        ((short*)nv)[34] = tmp;
        tmp = ((short*)nv)[21];
        ((short*)nv)[21] = ((short*)nv)[42];
        ((short*)nv)[42] = tmp;
        tmp = ((short*)nv)[22];
        ((short*)nv)[22] = ((short*)nv)[50];
        ((short*)nv)[50] = tmp;
        tmp = ((short*)nv)[23];
        ((short*)nv)[23] = ((short*)nv)[58];
        ((short*)nv)[58] = tmp;
        tmp = ((short*)nv)[28];
        ((short*)nv)[28] = ((short*)nv)[35];
        ((short*)nv)[35] = tmp;
        tmp = ((short*)nv)[29];
        ((short*)nv)[29] = ((short*)nv)[43];
        ((short*)nv)[43] = tmp;
        tmp = ((short*)nv)[30];
        ((short*)nv)[30] = ((short*)nv)[51];
        ((short*)nv)[51] = tmp;
        tmp = ((short*)nv)[31];
        ((short*)nv)[31] = ((short*)nv)[59];
        ((short*)nv)[59] = tmp;
        tmp = ((short*)nv)[37];
        ((short*)nv)[37] = ((short*)nv)[44];
        ((short*)nv)[44] = tmp;
        tmp = ((short*)nv)[38];
        ((short*)nv)[38] = ((short*)nv)[52];
        ((short*)nv)[52] = tmp;
        tmp = ((short*)nv)[39];
        ((short*)nv)[39] = ((short*)nv)[60];
        ((short*)nv)[60] = tmp;
        tmp = ((short*)nv)[46];
        ((short*)nv)[46] = ((short*)nv)[53];
        ((short*)nv)[53] = tmp;
        tmp = ((short*)nv)[47];
        ((short*)nv)[47] = ((short*)nv)[61];
        ((short*)nv)[61] = tmp;
        tmp = ((short*)nv)[55];
        ((short*)nv)[55] = ((short*)nv)[62];
        ((short*)nv)[62] = tmp;
        #endif
        break;
      }
    }
    
    nv += ( sp * 16 );
  } while( --nl );
}

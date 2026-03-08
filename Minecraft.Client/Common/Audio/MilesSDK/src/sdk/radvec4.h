//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// This define forces the vector units to use 4 floats instead of the simd types.
//#define RADVEC_FORCE_FLOATS

//
// Note that some platforms don't support a decent way to do conversions in
// the vector units, and some don't do vector comparisons in a useful way.
// The two defines RADVEC_SUPPORTS_VECTOR_COMPARE and RADVEC_SUPPORTS_VECTOR_CONVERT
// represent these concepts.
//

//
// Misc things to know:
//
// VEC4F_STRUCT is the type used for passing parameters and making arrays.
// It is marshaled to a register type before use.
//
// V1LOAD may not zero the rest of the words on load. Functionality currently
// unneeded.
//
// The various SETUP_* macros are for platforms that don't readily allow
// for static const SIMD types.
//


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#if defined(IS_IPHONE) || defined(IS_LINUX) || defined(IS_3DS) || defined(IS_PSP2) || defined(IS_WIIU) || defined(__RADANDROID__)
#define RADVEC_FORCE_FLOATS
#endif


#ifdef RADVEC_FORCE_FLOATS

//
// Code path requiring no vector units at all
//

#ifndef FORCE_FLOAT_STRUCT_DEFINED
#define FORCE_FLOAT_STRUCT_DEFINED
struct ForceFloatStruct
{
    float x,y,z,w;
};
#endif

#define DECL_VEC4F(name) register float name##x; register float name##y; register float name##z; register float name##w;
#define VEC4F_STRUCT ForceFloatStruct
#define DECL_SCRATCH
#define DECL_ZEROES float zeroes[4] = {0}

#define V4LOAD(out, a) out##x = *(float*)(a); out##y = *(float*)((char*)(a)+4); out##z = *(float*)((char*)(a)+8); out##w = *(float*)((char*)(a)+12);
#define V2LOAD(out, fill, a) out##x = *(float*)(a); out##y = *(float*)((char*)(a)+4); out##z = fill##z; out##w = fill##w;
#define V1LOAD(out, a) out##x = *(float*)(a); out##y = out##z = out##w = 0;
#define V4STORE(mem, b) *(float*)(mem) = b##x; *(float*)((char*)(mem)+4) = b##y; *(float*)((char*)(mem)+8) = b##z; *(float*)((char*)(mem)+12) = b##w;
#define V4ASSIGN(left, right) left##x = right##x; left##y = right##y; left##z = right##z; left##w = right##w;

#define V4MUL(out, a, b) out##x = a##x * b##x; out##y = a##y * b##y; out##z = a##z * b##z; out##w = a##w * b##w
#define V4ADD(out, a, b) out##x = a##x + b##x; out##y = a##y + b##y; out##z = a##z + b##z; out##w = a##w + b##w
#define V4SUB(out, a, b) out##x = a##x - b##x; out##y = a##y - b##y; out##z = a##z - b##z; out##w = a##w - b##w

#define SETUP_PMPM
#define SETUP_MMMM
#define SETUP_PMMP
#define SETUP_MMPP
#define SETUP_MPMP

#define SETUP_V4ONE float ones[4] = {1, 1, 1, 1}; \
   DECL_VEC4F(v4one); \
   V4LOAD(v4one, ones);

#define SETUP_V4ZERO float zeroes[4] = {0};\
   DECL_VEC4F(v4zero);\
   V4LOAD(v4zero, zeroes);

#define V4NEG0101(out, a) out##x = a##x; out##y = -a##y; out##z = a##z; out##w = -a##w;
#define V4NEG1100(out, a) out##x = -a##x; out##y = -a##y; out##z = a##z; out##w = a##w;
#define V4NEG1010(out, a) out##x = -a##x; out##y = a##y; out##z = -a##z; out##w = a##w;
#define V4NEG1111(out, a) out##x = -a##x; out##y = -a##y; out##z = -a##z; out##w = -a##w;
#define V4NEG0110(out, a) out##x = a##x; out##y = -a##y; out##z = -a##z; out##w = a##w;

#define V4PERM0000(out, a, b) {out##x = a##x; out##y = a##x; out##z = b##x; out##w = b##x;}
#define V4PERM0011(out, a, b) {out##x = a##x; out##y = a##x; out##z = b##y; out##w = b##y;}
#define V4PERM0022(out, a, b) {out##x = a##x; out##y = a##x; out##z = b##z; out##w = b##z;}
#define V4PERM0101(out, a, b) {out##x = a##x; out##y = a##y; out##z = b##x; out##w = b##y;}
#define V4PERM0202(out, a, b) {out##x = a##x; out##y = a##z; out##z = b##x; out##w = b##z;}

#define V4PERM1010(out, a, b) {out##x = a##y; out##y = a##x; out##z = b##y; out##w = b##x;}
#define V4PERM1011(out, a, b) {out##x = a##y; out##y = a##x; out##z = b##y; out##w = b##y;}
#define V4PERM1032(out, a, b) {out##x = a##y; out##y = a##x; out##z = b##w; out##w = b##z;}
#define V4PERM1111(out, a, b) {out##x = a##y; out##y = a##y; out##z = b##y; out##w = b##y;}
#define V4PERM1132(out, a, b) {out##x = a##y; out##y = a##y; out##z = b##w; out##w = b##z;}
#define V4PERM1133(out, a, b) {out##x = a##y; out##y = a##y; out##z = b##w; out##w = b##w;}
#define V4PERM1313(out, a, b) {out##x = a##y; out##y = a##w; out##z = b##y; out##w = b##w;}

#define V4PERM2020(out, a, b) {out##x = a##z; out##y = a##x; out##z = b##z; out##w = b##x;}
#define V4PERM2222(out, a, b) {out##x = a##z; out##y = a##z; out##z = b##z; out##w = b##z;}
#define V4PERM2301(out, a, b) {out##x = a##z; out##y = a##w; out##z = b##x; out##w = b##y;}
#define V4PERM2323(out, a, b) {out##x = a##z; out##y = a##w; out##z = b##z; out##w = b##w;}

#define V4PERM3131(out, a, b) {out##x = a##w; out##y = a##y; out##z = b##w; out##w = b##y;}
#define V4PERM3210(out, a, b) {out##x = a##w; out##y = a##z; out##z = b##y; out##w = b##x;}
#define V4PERM3232(out, a, b) {out##x = a##w; out##y = a##z; out##z = b##w; out##w = b##z;}
#define V4PERM3333(out, a, b) {out##x = a##w; out##y = a##w; out##z = b##w; out##w = b##w;}

// End RADVEC_FORCE_FLOAT
#else
// SIMD / optimized paths
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define DECL_ZEROES RAD_ALIGN(float, zeroes[4], 16) = {0}

#if defined(_MSC_VER) && !(defined(_XENON) || (_XBOX_VER == 200)) || defined(__RADSEKRIT2__)
//
// Windows
//
#define RADVEC_SUPPORTS_VECTOR_COMPARE
#define RADVEC_SUPPORTS_VECTOR_CONVERT

#define __INTRIN_H_
#include <xmmintrin.h>
#include <emmintrin.h>

#define DECL_VEC4F(name) __m128 name;
#define DECL_SCRATCH
#define VEC4F_STRUCT __m128
#define VEC4I __m128i
#define VEC8I __m128i

#define V4LOAD(out, a)   out = _mm_load_ps((float*)(a))
#define V2LOAD(out, fill, a) out = _mm_loadl_pi(fill, (__m64 *) (a))
#define V1LOAD(out, a) out = _mm_load_ss((a))
#define V4STORE(a, b) _mm_store_ps((float*)a, b)
#define V4STOREI(a, b) _mm_store_si128((VEC4I *) a, b)
#define V8STOREI(a, b) _mm_store_si128((VEC8I *) a, b)
#define V4ASSIGN(left, right) left = right;

#define V4MUL(out, a, b) out = _mm_mul_ps(a, b)
#define V4ADD(out, a, b) out = _mm_add_ps(a, b)
#define V4SUB(out, a, b) out = _mm_sub_ps(a, b)

#define V4XOR(a, b) _mm_xor_ps(a, b)
#define V4AND(a, b) _mm_and_ps(a, b)
#define V4ANDNOT(a, b) _mm_andnot_ps(a, b)

// Windows doesn't need setup.
#ifndef WINDOWS_RADVEC_STATICS_DEFINED
#define WINDOWS_RADVEC_STATICS_DEFINED
RAD_ALIGN(static const float, zeroes[4], 16) = {0};
// The following constants are mostly used for flipping the sign of
// one or more elements of a VEC4 register, except when used as a
// source of actual zeroes.
static const VEC4F_STRUCT mpmp = {-0.0f,  0.0f, -0.0f,  0.0f};
static const VEC4F_STRUCT pmpm = { 0.0f, -0.0f,  0.0f, -0.0f};
static const VEC4F_STRUCT pmmp = { 0.0f, -0.0f, -0.0f,  0.0f};
static const VEC4F_STRUCT mmpp = {-0.0f, -0.0f,  0.0f,  0.0f};
static const VEC4F_STRUCT mmmm = {-0.0f, -0.0f, -0.0f, -0.0f};
static const VEC4F_STRUCT v4one ={ 1.0f,  1.0f,  1.0f,  1.0f};
#endif

#define SETUP_PMPM
#define SETUP_MMMM
#define SETUP_PMMP
#define SETUP_MMPP
#define SETUP_MPMP
#define SETUP_V4ONE
#define SETUP_V4ZERO

// In all the places where a four-digit constant is used, the
// first digit always corresponds to the element which was loaded
// from the lowest memory address, i.e. little endian ordering.

// This is important for the VPERM* and the VNEG* functions.

// Helper function to define SHUFPS constants
#define SHUF(a0, a1, a2, a3) (a0*1 + a1*4 + a2*16 + a3*64)

#define V4PERM(a, b, shuf) _mm_shuffle_ps(a, b, shuf)

#define V4PERM0000(out, a, b) out = V4PERM(a, b, SHUF(0,0,0,0))
#define V4PERM0011(out, a, b) out = V4PERM(a, b, SHUF(0,0,1,1))
#define V4PERM0022(out, a, b) out = V4PERM(a, b, SHUF(0,0,2,2))
#define V4PERM0101(out, a, b) out = V4PERM(a, b, SHUF(0,1,0,1))
#define V4PERM0202(out, a, b) out = V4PERM(a, b, SHUF(0,2,0,2))

#define V4PERM1010(out, a, b) out = V4PERM(a, b, SHUF(1,0,1,0))
#define V4PERM1011(out, a, b) out = V4PERM(a, b, SHUF(1,0,1,1))
#define V4PERM1032(out, a, b) out = V4PERM(a, b, SHUF(1,0,3,2))
#define V4PERM1111(out, a, b) out = V4PERM(a, b, SHUF(1,1,1,1))
#define V4PERM1132(out, a, b) out = V4PERM(a, b, SHUF(1,1,3,2))
#define V4PERM1133(out, a, b) out = V4PERM(a, b, SHUF(1,1,3,3))
#define V4PERM1313(out, a, b) out = V4PERM(a, b, SHUF(1,3,1,3))

#define V4PERM2020(out, a, b) out = V4PERM(a, b, SHUF(2,0,2,0))
#define V4PERM2222(out, a, b) out = V4PERM(a, b, SHUF(2,2,2,2))
#define V4PERM2301(out, a, b) out = V4PERM(a, b, SHUF(2,3,0,1))
#define V4PERM2323(out, a, b) out = V4PERM(a, b, SHUF(2,3,2,3))

#define V4PERM3131(out, a, b) out = V4PERM(a, b, SHUF(3,1,3,1))
#define V4PERM3210(out, a, b) out = V4PERM(a, b, SHUF(3,2,1,0))
#define V4PERM3232(out, a, b) out = V4PERM(a, b, SHUF(3,2,3,2))
#define V4PERM3333(out, a, b) out = V4PERM(a, b, SHUF(3,3,3,3))

#define v4zero mmmm

// Here the +/- zero values are used to negate specific elements:
#define V4NEG1010(out, a) out = V4XOR(a,mpmp)
#define V4NEG0101(out, a) out = V4XOR(a,pmpm)
#define V4NEG0110(out, a) out = V4XOR(a,pmmp)
#define V4NEG1100(out, a) out = V4XOR(a,mmpp)
#define V4NEG1111(out, a) out = V4XOR(a,mmmm)

// Compare pairs of fp values for (a > b), returning 000/111... for false/true
#define V4CMPGT(a, b) _mm_cmpgt_ps(a, b)

// Convert all 4 fp values into 32-bit ints:
#define V4FTOI(a) _mm_cvtps_epi32(a)
// Pack (with saturation) 4 32-bit ints from a and 4 from b, into 8 16-bit values:
#define V4ITO8I(a, b) _mm_packs_epi32(a, b)

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#elif defined(__psp__)
//
// PSP
//
#include <vfpu_intrinsics.h>

#define RADVEC_SUPPORTS_VECTOR_CONVERT

#define DECL_VEC4F(name) vfpu_v4 name;
#define VEC4F_STRUCT vfpu_v4
#define VEC4I vfpu_v4
#define VEC8I vfpu_v4
#define DECL_SCRATCH vfpu_v4 scratch;

// Load / Stores
//-----------------------------------------------------------------------------
#define V4LOAD(out, a) out = *(vfpu_v4*)(a);
#define V2LOAD(out, fill, a)\
    out = vfpu_vmov_q(fill, 0);\
    scratch = *(vfpu_v4*)(a);\
    out = vfpu_vmov_p_partial(out, scratch, 0);
#define V1LOAD(out, a) V4LOAD(out, a)
#define V4STORE(memloc, reg) *((vfpu_v4*)(memloc)) = reg;
#define V8STOREI(memloc, reg) *((vfpu_v4*)(memloc)) = reg;
#define V4ASSIGN(left, right) left = right;

// Arithmetic
//-----------------------------------------------------------------------------
#define V4MUL(out, a, b) out = vfpu_vmul_q(a, b, 0);
#define V4SUB(out, a, b) out = vfpu_vsub_q(a, b, 0);
#define V4ADD(out, a, b) out = vfpu_vadd_q(a, b, 0);

// NOTE! The psp pack doesn't work the same as the others.
#define V4FTOI(a) vfpu_vf2iz_q(a, 16, 0);
#define V4ITO8I(a, b) vfpu_vi2s_q(a, 0);

// Shuffles.
//-----------------------------------------------------------------------------
#define V4PERM0000(out, a, b) \
    scratch = vfpu_vmov_p(a, VFPU_PFX_VS_P(VFPU_PFX_X, VFPU_PFX_X));\
    scratch = vfpu_vmov_p_partial(scratch, b, VFPU_VD_ZW | VFPU_PFX_VS_P(VFPU_PFX_X, VFPU_PFX_X));\
    out = scratch;

#define V4PERM0011(out, a, b) \
    scratch = vfpu_vmov_p(a, VFPU_PFX_VS_P(VFPU_PFX_X, VFPU_PFX_X));\
    scratch = vfpu_vmov_p_partial(scratch, b, VFPU_VD_ZW | VFPU_PFX_VS_P(VFPU_PFX_Y, VFPU_PFX_Y));\
    out = scratch;

#define V4PERM0022(out, a, b) \
    scratch = vfpu_vmov_p(a, VFPU_PFX_VS_P(VFPU_PFX_X, VFPU_PFX_X));\
    scratch = vfpu_vmov_p_partial(scratch, b, VFPU_VD_ZW | VFPU_VS_ZW | VFPU_PFX_VS_P(VFPU_PFX_X, VFPU_PFX_X));\
    out = scratch;

#define V4PERM0101(out, a, b) \
    scratch = vfpu_vmov_p(a, VFPU_PFX_VS_P(VFPU_PFX_X, VFPU_PFX_Y));\
    scratch = vfpu_vmov_p_partial(scratch, b, VFPU_VD_ZW | VFPU_PFX_VS_P(VFPU_PFX_X, VFPU_PFX_Y));\
    out = scratch;

#define V4PERM0202(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Z, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_X, VFPU_PFX_Z));\
    out = scratch;

#define V4PERM1010(out, a, b) \
    scratch = vfpu_vmov_p(a, VFPU_PFX_VS_P(VFPU_PFX_Y, VFPU_PFX_X));\
    scratch = vfpu_vmov_p_partial(scratch, b, VFPU_VD_ZW | VFPU_PFX_VS_P(VFPU_PFX_Y, VFPU_PFX_X));\
    out = scratch;

#define V4PERM1011(out, a, b) \
    scratch = vfpu_vmov_p(a, VFPU_PFX_VS_P(VFPU_PFX_Y, VFPU_PFX_X));\
    scratch = vfpu_vmov_p_partial(scratch, b, VFPU_VD_ZW | VFPU_PFX_VS_P(VFPU_PFX_Y, VFPU_PFX_Y));\
    out = scratch;

#define V4PERM1032(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_Y, VFPU_PFX_X, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_W, VFPU_PFX_Z));\
    out = scratch;

#define V4PERM1111(out, a, b) \
    scratch = vfpu_vmov_p(a, VFPU_PFX_VS_P(VFPU_PFX_Y, VFPU_PFX_Y));\
    scratch = vfpu_vmov_p_partial(scratch, b, VFPU_VD_ZW | VFPU_PFX_VS_P(VFPU_PFX_Y, VFPU_PFX_Y));\
    out = scratch;

#define V4PERM1132(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_Y, VFPU_PFX_Y, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_W, VFPU_PFX_Z));\
    out = scratch;

#define V4PERM1133(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_Y, VFPU_PFX_Y, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_W, VFPU_PFX_W));\
    out = scratch;

#define V4PERM1313(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_Y, VFPU_PFX_W, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_Y, VFPU_PFX_W));\
    out = scratch;

#define V4PERM2020(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_Z, VFPU_PFX_X, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_Z, VFPU_PFX_X));\
    out = scratch;

#define V4PERM2222(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_Z, VFPU_PFX_Z, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_Z, VFPU_PFX_Z));\
    out = scratch;

#define V4PERM2301(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_Z, VFPU_PFX_W, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_X, VFPU_PFX_Y));\
    out = scratch;

#define V4PERM2323(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_Z, VFPU_PFX_W, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_Z, VFPU_PFX_W));\
    out = scratch;

#define V4PERM3131(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_W, VFPU_PFX_Y, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_W, VFPU_PFX_Y));\
    out = scratch;

#define V4PERM3210(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_W, VFPU_PFX_Z, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_Y, VFPU_PFX_X));\
    out = scratch;

#define V4PERM3232(out, a, b) \
    scratch = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_W, VFPU_PFX_Z, VFPU_PFX_X, VFPU_PFX_Z));\
    scratch = vfpu_vmov_q_partial(scratch, b, VFPU_PFX_VD_Q(VFPU_PFX_MASK, VFPU_PFX_MASK, VFPU_PFX_THRU, VFPU_PFX_THRU) | VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_Y, VFPU_PFX_W, VFPU_PFX_Z));\
    out = scratch;

#define V4PERM3333(out, a, b) \
    scratch = vfpu_vmov_p(a, VFPU_VS_ZW | VFPU_PFX_VS_P(VFPU_PFX_Y, VFPU_PFX_Y));\
    scratch = vfpu_vmov_p_partial(scratch, b, VFPU_VD_ZW | VFPU_VS_ZW | VFPU_PFX_VS_P(VFPU_PFX_Y, VFPU_PFX_Y));\
    out = scratch;

// PSP doesn't need setup.
#define SETUP_PMPM
#define SETUP_MMMM
#define SETUP_PMMP
#define SETUP_MMPP
#define SETUP_MPMP
#define SETUP_V4ONE
#define SETUP_V4ZERO

#ifndef PSP_RADVEC_STATICS_DEFINED
#define PSP_RADVEC_STATICS_DEFINED
static const VEC4F_STRUCT v4one = { 1.0f,  1.0f,  1.0f,  1.0f};
static const VEC4F_STRUCT v4zero = { 0.0f,  0.0f,  0.0f,  0.0f};
#endif

// Here the +/- zero values are used to negate specific elements:
#define V4NEG1010(out, a) out = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_MINUS_X, VFPU_PFX_Y, VFPU_PFX_MINUS_Z, VFPU_PFX_W))
#define V4NEG0101(out, a) out = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_MINUS_Y, VFPU_PFX_Z, VFPU_PFX_MINUS_W))
#define V4NEG0110(out, a) out = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_X, VFPU_PFX_MINUS_Y, VFPU_PFX_MINUS_Z, VFPU_PFX_W))
#define V4NEG1100(out, a) out = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_MINUS_X, VFPU_PFX_MINUS_Y, VFPU_PFX_Z, VFPU_PFX_W))
#define V4NEG1111(out, a) out = vfpu_vmov_q(a, VFPU_PFX_VS_Q(VFPU_PFX_MINUS_X, VFPU_PFX_MINUS_Y, VFPU_PFX_MINUS_Z, VFPU_PFX_MINUS_W))

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#elif defined(__CELLOS_LV2__) || (defined(__MACH__) && defined(__POWERPC__))
//
// PS3
//
#define RADVEC_SUPPORTS_VECTOR_COMPARE
#define RADVEC_SUPPORTS_VECTOR_CONVERT

// Mac PPC doesn't need the header file, due to compiler switches
#ifndef __MACH__
#include <altivec.h>
#endif

#define DECL_VEC4F(name) vector float name;
#define VEC4F_STRUCT vector float
#define VEC4I vector signed int
#define VEC8I vector signed short
#define DECL_SCRATCH

#ifndef PS3_RADVEC_STATICS_DEFINED
#define PS3_RADVEC_STATICS_DEFINED
// Constant used for passing through the higher order words in a v2load.
static const vector unsigned char V2LOAD_PERMCONST = (vector unsigned char){16+0, 16+1, 16+2, 16+3, 16+4, 16+5, 16+6, 16+7, 8, 9, 10, 11, 12, 13, 14, 15};

// Mask used to zero out the upper words in v1load.
static const vector unsigned char V1LOAD_MASK = (vector unsigned char){0xFF, 0xFF,0xFF,0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// The following constants are mostly used for flipping the sign of
// one or more elements of a VEC4 register, except when used as a
// source of actual zeroes.
RAD_ALIGN(static const float, zeroes[4], 16) = {0};
static const VEC4F_STRUCT mpmp = (vector float) {-0.0f,  0.0f, -0.0f,  0.0f};
static const VEC4F_STRUCT pmpm = (vector float) { 0.0f, -0.0f,  0.0f, -0.0f};
static const VEC4F_STRUCT pmmp = (vector float) { 0.0f, -0.0f, -0.0f,  0.0f};
static const VEC4F_STRUCT mmpp = (vector float) {-0.0f, -0.0f,  0.0f,  0.0f};
static const VEC4F_STRUCT mmmm = (vector float) {-0.0f, -0.0f, -0.0f, -0.0f};
static const VEC4F_STRUCT v4one =(vector float) { 1.0f,  1.0f,  1.0f,  1.0f};

#endif

#define V4LOAD(out, a)   out = vec_ld(0, a)
#define V2LOAD(out, fill, a) out = vec_perm(fill, vec_ld(0, a), V2LOAD_PERMCONST)
#define V1LOAD(out, a) out = ((vector float)vec_and((vector unsigned char)vec_lde(0, a), V1LOAD_MASK))
#define V4ASSIGN(left, right) left = right;

#define V4STORE(a, b) vec_st(b, 0, a)
#define V4STOREI(a, b) vec_st(b, 0, (VEC4I *) a)
#define V8STOREI(a, b) vec_st(b, 0, (VEC4I*)(void*)a)

#define V4MUL(out, a, b) out = vec_madd(a, b, pmpm)
#define V4ADD(out, a, b) out = vec_add(a, b)
#define V4SUB(out, a, b) out = vec_sub(a, b)
#define V4XOR(a, b) vec_xor(a, b)
#define V4AND(a, b) vec_and(a, b)
#define V4ANDNOT(a, b) vec_andc(b, a)

// In all the places where a four-digit constant is used, the
// first digit always corresponds to the element which was loaded
// from the lowest memory address, i.e. little endian ordering.

// This is important for the VPERM* and the VNEG* functions.

// Helper function to define permute constants for 4xfloat vectors
#define SHUF(a0, a1, a2, a3) ((vector unsigned char) \
                    {a0*4+0,a0*4+1,a0*4+2,a0*4+3, a1*4+0,a1*4+1,a1*4+2,a1*4+3, \
                    (a2)*4+0+16,(a2)*4+1+16,(a2)*4+2+16,(a2)*4+3+16, (a3)*4+0+16,(a3)*4+1+16,(a3)*4+2+16,(a3)*4+3+16})

#define V4PERM(a, b, shuf) vec_perm(a, b, shuf)

#define V4PERM0000(out, a, b) out = V4PERM(a, b, SHUF(0,0,0,0))
#define V4PERM0011(out, a, b) out = V4PERM(a, b, SHUF(0,0,1,1))
#define V4PERM0022(out, a, b) out = V4PERM(a, b, SHUF(0,0,2,2))
#define V4PERM0101(out, a, b) out = V4PERM(a, b, SHUF(0,1,0,1))
#define V4PERM0202(out, a, b) out = V4PERM(a, b, SHUF(0,2,0,2))

#define V4PERM1010(out, a, b) out = V4PERM(a, b, SHUF(1,0,1,0))
#define V4PERM1011(out, a, b) out = V4PERM(a, b, SHUF(1,0,1,1))
#define V4PERM1032(out, a, b) out = V4PERM(a, b, SHUF(1,0,3,2))
#define V4PERM1111(out, a, b) out = V4PERM(a, b, SHUF(1,1,1,1))
#define V4PERM1132(out, a, b) out = V4PERM(a, b, SHUF(1,1,3,2))
#define V4PERM1133(out, a, b) out = V4PERM(a, b, SHUF(1,1,3,3))
#define V4PERM1313(out, a, b) out = V4PERM(a, b, SHUF(1,3,1,3))

#define V4PERM2020(out, a, b) out = V4PERM(a, b, SHUF(2,0,2,0))
#define V4PERM2222(out, a, b) out = V4PERM(a, b, SHUF(2,2,2,2))
#define V4PERM2301(out, a, b) out = V4PERM(a, b, SHUF(2,3,0,1))
#define V4PERM2323(out, a, b) out = V4PERM(a, b, SHUF(2,3,2,3))

#define V4PERM3131(out, a, b) out = V4PERM(a, b, SHUF(3,1,3,1))
#define V4PERM3210(out, a, b) out = V4PERM(a, b, SHUF(3,2,1,0))
#define V4PERM3232(out, a, b) out = V4PERM(a, b, SHUF(3,2,3,2))
#define V4PERM3333(out, a, b) out = V4PERM(a, b, SHUF(3,3,3,3))

#define v4zero mmmm

// PS3 doesn't need setup.
#define SETUP_PMPM
#define SETUP_MMMM
#define SETUP_PMMP
#define SETUP_MMPP
#define SETUP_MPMP
#define SETUP_V4ONE
#define SETUP_V4ZERO

// Here the +/- zero values are used to negate specific elements:
#define V4NEG1010(out, a) out = V4XOR(a,mpmp)
#define V4NEG0101(out, a) out = V4XOR(a,pmpm)
#define V4NEG0110(out, a) out = V4XOR(a,pmmp)
#define V4NEG1100(out, a) out = V4XOR(a,mmpp)
#define V4NEG1111(out, a) out = V4XOR(a,mmmm)

// Compare pairs of fp values for (a > b), returning 000/111... for false/true
#define V4CMPGT(a, b) vec_cmpgt(a, b)

// Convert all 4 fp values into 32-bit ints:
#define V4FTOI(a) vec_cts(a,0)

// Pack (with saturation) 4 32-bit ints from a and 4 from b, into 8 16-bit values:
#define V4ITO8I(a, b) (VEC4I)vec_packs(a, b)

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#elif defined(R5900)

#define DECL_VEC4F(n) register u_long128 __attribute__((aligned(16))) n = 0;
#define VEC4F_STRUCT u_long128
#define DECL_SCRATCH DECL_VEC4F(scratch)

// Load / Stores.
//-----------------------------------------------------------------------------

#define V4LOAD(out, source)\
    asm volatile( "lqc2     %0, 0(%1)"\
        : "=j" (out)\
        : "r" ((void*)(source)), "j" (out) \
        );

#define V2LOAD(out, fill, source)\
    asm volatile( "lqc2     $vf28, 0(%1);"\
          "vmove.xy %0, $vf28"\
        : "=j" (out)\
        : "r" ((void*)(source)), "j" (out) \
        : "vf28" \
        );

// Technically this doesn't zero out the other 3 words like it should, but the only place it is used doesn't need it.
#define V1LOAD(out, source) V4LOAD(out, source)
#define V4ASSIGN(left, right) left = right;

#define V4STORE(memloc, source)\
    asm volatile( "sqc2     %0, 0(%1)"\
        :\
        : "j" (source), "r" (memloc)\
        : "memory"\
        );

// Arithmetic
//-----------------------------------------------------------------------------

#define V4MUL(out, a, b)\
    asm volatile( "vmul     %0, %1, %2"\
        : "=j" (out)\
        : "j" (a), "j" (b), "j" (out) \
        );

#define V4SUB(out, a, b)\
    asm volatile( "vsub     %0, %1, %2"\
        : "=j" (out)\
        : "j" (a), "j" (b), "j" (out) \
        );\

#define V4ADD(out, a, b)\
    asm volatile( "vadd     %0, %1, %2"\
        : "=j" (out)\
        : "j" (a), "j" (b), "j" (out) \
        );\

// Shuffles
//-----------------------------------------------------------------------------

#define V4PERM0202(out, a, b)\
    asm volatile( "vmr32.xyzw %0, %1" : "=j" (out) : "j"(b) ); \
    asm volatile( "vmr32.xyzw %0, %0" : "=j" (out) : "j"(out) ); \
    asm volatile( "vmr32.xyw  %0, %0"  : "=j" (out) : "j"(out) );  \
    asm volatile( "vmove.xy   %0, %1" : "=j" (out) : "j"(a), "j"(out) ); \
    asm volatile( "vmr32.y    %0, %1"  : "=j" (out) : "j"(a), "j"(out) );

#define V4PERM2020(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.yz     %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmove.zw     %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.w      %0, %1" : "=j" (out) : "j" (b), "j" (out) );

#define V4PERM1313(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.xyz    %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.yzw    %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmove.w      %0, %1" : "=j" (out) : "j" (b), "j" (out) );

#define V4PERM3131(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.xzw    %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xyw    %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmove.y      %0, %1" : "=j" (out) : "j" (a), "j" (out) );

#define V4PERM0000(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.xyz    %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.xzw    %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xz     %0, %0" : "=j" (out) : "j" (out) );

#define V4PERM0022(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xzw    %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyz    %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.xyzw   %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.xyz    %0, %0" : "=j" (out) : "j" (out) );

#define V4PERM1133(out, a, b)\
    asm volatile( "vmove.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmove.xzw    %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xz     %0, %0" : "=j" (out) : "j" (out) );

#define V4PERM1111(out, a, b)\
    asm volatile( "vmove.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xzw    %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xzw    %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.xz     %0, %0" : "=j" (out) : "j" (out) );

#define V4PERM2222(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmove.yz     %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.xz     %0, %0" : "=j" (out) : "j" (out) );

#define V4PERM3333(out, a, b)\
    asm volatile( "vmove.xyzw   %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xyz    %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyz    %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmr32.xz     %0, %0" : "=j" (out) : "j" (out) );

#define V4PERM1032(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xw     %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (scratch) : "j" (out), "j"(scratch) );         \
    asm volatile( "vmr32.yw     %0, %1" : "=j" (out) :  "j" (scratch), "j" (out) );

#define V4PERM3210(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.yz     %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (scratch) : "j" (out), "j" (scratch));         \
    asm volatile( "vmr32.xz     %0, %1" : "=j" (out) : "j" (scratch), "j" (out) );

#define V4PERM1132(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xy     %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmove.yw     %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xw     %0, %0" : "=j" (out) : "j" (out) ); \

#define V4PERM2323(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %0" : "=j" (out) : "j" (out) ); \
    asm volatile( "vmove.zw     %0, %1" : "=j" (out) : "j" (b), "j" (out) );

#define V4PERM3232(out, a, b)\
    asm volatile( "vmove.xyzw   %0, %1" : "=j" (scratch) : "j" (b), "j" (scratch));          \
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );  \
    asm volatile( "vmr32.xyw    %0, %1" : "=j" (out) : "j" (b), "j" (out) );  \
    asm volatile( "vmr32.xyz    %0, %1" : "=j" (scratch) : "j" (out), "j" (scratch));                   \
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );  \
    asm volatile( "vmr32.xzw    %0, %1" : "=j" (out) : "j" (scratch), "j" (out) );

#define V4PERM2301(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xw     %0, %1" : "=j" (out) : "j" (b), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %0" : "=j" (out) : "j" (out) );

#define V4PERM0101(out, a, b)\
    asm volatile( "vmove.xyzw   %0, %1" : "=j" (out) : "j" (a), "j" (out) );   \
    asm volatile( "vmr32.xyzw   %0, %1;" : "=j" (scratch) : "j" (b), "j" (scratch)); \
    asm volatile( "vmr32.zw     %0, %1" : "=j" (out) : "j" (scratch), "j" (out) ); \

#define V4PERM1010(out, a, b)\
    asm volatile( "vmr32.xyzw   %0, %1;" : "=j" (out) : "j" (a), "j" (out) ); \
    asm volatile( "vmove.xyzw   %0, %1;" : "=j" (scratch) : "j" (b), "j" (scratch) ); \
    asm volatile( "vmr32.zw     %0, %1;" : "=j" (scratch) : "j" (out), "j" (scratch) ); \
    asm volatile( "vmr32.xyz    %0, %1;" : "=j" (out): "j" (scratch), "j" (out) ); \
    asm volatile( "vmr32.yzw    %0, %1;" : "=j" (scratch) : "j" (out), "j" (scratch)); \
    asm volatile( "vmr32.xyzw   %0, %1"  : "=j" (out) : "j" (scratch), "j" (out) );

#ifndef PS2_RADVEC_STATICS_DEFINED
#define PS2_RADVEC_STATICS_DEFINED
RAD_ALIGN(static const float, pmpmraw[4], 16) = { 1.0f, -1.0f,  1.0f, -1.0f};
RAD_ALIGN(static const float, mmmmraw[4], 16) = {-1, -1, -1, -1};
RAD_ALIGN(static const float, pmmpraw[4], 16) = {1.0f, -1.0f, -1.0f, 1.0f};
RAD_ALIGN(static const float, mmppraw[4], 16) = {-1.0f, -1.0f, 1.0f, 1.0f};
RAD_ALIGN(static const float, mpmpraw[4], 16) = {-1, 1, -1, 1};
RAD_ALIGN(static const float, ones[4], 16) = {1, 1, 1, 1};
RAD_ALIGN(static const float, zeroes[4], 16) = {0};
#endif

#define SETUP_PMPM  \
   DECL_VEC4F(pmpm);\
   V4LOAD(pmpm, pmpmraw);

#define SETUP_MMMM \
   DECL_VEC4F(mmmm);\
   V4LOAD(mmmm, mmmmraw);

#define SETUP_PMMP \
    DECL_VEC4F(pmmp);\
    V4LOAD(pmmp, pmmpraw);

#define SETUP_MMPP \
    DECL_VEC4F(mmpp);\
    V4LOAD(mmpp, mmppraw);

#define SETUP_MPMP ;\
   DECL_VEC4F(mpmp);\
   V4LOAD(mpmp, mpmpraw);

#define SETUP_V4ONE  \
   DECL_VEC4F(v4one); \
   V4LOAD(v4one, ones);

#define SETUP_V4ZERO \
   DECL_VEC4F(v4zero);\
   V4LOAD(v4zero, zeroes);

#define V4NEG0101(out, a) V4MUL(out, a, pmpm)
#define V4NEG1100(out, a) V4MUL(out, a, mmpp)
#define V4NEG1010(out, a) V4MUL(out, a, mpmp)
#define V4NEG1111(out, a) V4MUL(out, a, mmmm)
#define V4NEG0110(out, a) V4MUL(out, a, pmmp)

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#elif defined(_XENON) || (_XBOX_VER == 200)
//
// XBox/360
//
#include <VectorIntrinsics.h>

#define RADVEC_SUPPORTS_VECTOR_COMPARE
#define RADVEC_SUPPORTS_VECTOR_CONVERT

#define DECL_VEC4F(name) __vector4 name;
#define VEC4F_STRUCT __vector4
#define VEC4I __vector4
#define VEC8I __vector4
#define DECL_SCRATCH



#define V4LOAD(out, a)   out = __lvx(a, 0)
#define V2LOAD(out, fill, a) out = __vperm(fill, __lvx(a, 0), *(__vector4*)V2LOAD_PERMCONST)

// Doesn't fill zeroes!
#define V1LOAD(out, a) out = __lvewx(a, 0);

#define V4STORE(a, b) __stvx(b, a, 0)
#define V4STOREI(a, b) __stvx(b, (VEC4I *) a, 0)
#define V8STOREI(a, b) __stvx(b, (VEC8I *) a, 0)
#define V4ASSIGN(left, right) left = right;

#define V4MUL(out, a, b) out = __vmaddfp(a, b, pmpm)
#define V4ADD(out, a, b) out = __vaddfp(a, b)
#define V4SUB(out, a, b) out = __vsubfp(a, b)
#define V4XOR(a, b) __vxor(a, b)
#define V4AND(a, b) __vand(a, b)
#define V4ANDNOT(a, b) __vandc(b, a)

// In all the places where a four-digit constant is used, the
// first digit always corresponds to the element which was loaded
// from the lowest memory address, i.e. little endian ordering.

// This is important for the VPERM* and the VNEG* functions.

// Helper function to define permute constants for 4xfloat vectors
#define SHUF(a0, a1, a2, a3) {\
    ((a0*4+0) << 24) + ((a0*4+1) << 16) + ((a0*4+2) << 8) + (a0*4+3),\
    ((a1*4+0) << 24) + ((a1*4+1) << 16) + ((a1*4+2) << 8) + (a1*4+3),\
    ((a2*4+0+16) << 24) + ((a2*4+1+16) << 16) + ((a2*4+2+16) << 8) + (a2*4+3+16),\
    ((a3*4+0+16) << 24) + ((a3*4+1+16) << 16) + ((a3*4+2+16) << 8) + (a3*4+3+16),\
}

#define SHUFNAME(a0,a1,a2,a3) SHUF##a0##a1##a2##a3
#define DEFINESHUFNAME( a0,a1,a2,a3) SHUFNAME(a0,a1,a2,a3)[4]=SHUF(a0,a1,a2,a3)

#ifndef XENON_RADVEC_STATICS_DEFINED
#define XENON_RADVEC_STATICS_DEFINED
// Const to pass through the upper 2 words in a v2load.
static const int __declspec(align(16)) V2LOAD_PERMCONST[4] = {
    ((16+0) << 24) + ((16+1) << 16) + ((16+2) << 8) + (16+3),
    ((16+4) << 24) + ((16+5) << 16) + ((16+6) << 8) + (16+7),
    ((8) << 24) + ((9) << 16) + ((10) << 8) + (11),
    ((12) << 24) + ((13) << 16) + ((14) << 8) + (15)};

static const int __declspec(align(16)) DEFINESHUFNAME(0,0,0,0);
static const int __declspec(align(16)) DEFINESHUFNAME(0,0,1,1);
static const int __declspec(align(16)) DEFINESHUFNAME(0,0,2,2);
static const int __declspec(align(16)) DEFINESHUFNAME(0,1,0,1);
static const int __declspec(align(16)) DEFINESHUFNAME(0,2,0,2);

static const int __declspec(align(16)) DEFINESHUFNAME(1,0,1,0);
static const int __declspec(align(16)) DEFINESHUFNAME(1,0,1,1);
static const int __declspec(align(16)) DEFINESHUFNAME(1,0,3,2);
static const int __declspec(align(16)) DEFINESHUFNAME(1,1,1,1);
static const int __declspec(align(16)) DEFINESHUFNAME(1,1,3,2);
static const int __declspec(align(16)) DEFINESHUFNAME(1,1,3,3);
static const int __declspec(align(16)) DEFINESHUFNAME(1,3,1,3);

static const int __declspec(align(16)) DEFINESHUFNAME(2,0,2,0);
static const int __declspec(align(16)) DEFINESHUFNAME(2,2,2,2);
static const int __declspec(align(16)) DEFINESHUFNAME(2,3,0,1);
static const int __declspec(align(16)) DEFINESHUFNAME(2,3,2,3);

static const int __declspec(align(16)) DEFINESHUFNAME(3,1,3,1);
static const int __declspec(align(16)) DEFINESHUFNAME(3,2,1,0);
static const int __declspec(align(16)) DEFINESHUFNAME(3,2,3,2);
static const int __declspec(align(16)) DEFINESHUFNAME(3,3,3,3);

// The following constants are mostly used for flipping the sign of
// one or more elements of a VEC4 register, except when used as a
// source of actual zeroes.
RAD_ALIGN(static const float, zeroes[4], 16) = {0};
static const VEC4F_STRUCT mpmp =  {-0.0f,  0.0f, -0.0f,  0.0f};
static const VEC4F_STRUCT pmpm =  { 0.0f, -0.0f,  0.0f, -0.0f};
static const VEC4F_STRUCT pmmp =  { 0.0f, -0.0f, -0.0f,  0.0f};
static const VEC4F_STRUCT mmpp =  {-0.0f, -0.0f,  0.0f,  0.0f};
static const VEC4F_STRUCT mmmm =  {-0.0f, -0.0f, -0.0f, -0.0f};
static const VEC4F_STRUCT v4one = { 1.0f,  1.0f,  1.0f,  1.0f};

#endif

#undef SHUF
#define SHUF(a0,a1,a2,a3) SHUFNAME(a0,a1,a2,a3)

#define V4PERM(a, b, shuf) __vperm(a, b, *(__vector4*)shuf)

#define V4PERM0000(out, a, b) out = V4PERM(a, b, SHUF(0,0,0,0))
#define V4PERM0011(out, a, b) out = V4PERM(a, b, SHUF(0,0,1,1))
#define V4PERM0022(out, a, b) out = V4PERM(a, b, SHUF(0,0,2,2))
#define V4PERM0101(out, a, b) out = V4PERM(a, b, SHUF(0,1,0,1))
#define V4PERM0202(out, a, b) out = V4PERM(a, b, SHUF(0,2,0,2))

#define V4PERM1010(out, a, b) out = V4PERM(a, b, SHUF(1,0,1,0))
#define V4PERM1011(out, a, b) out = V4PERM(a, b, SHUF(1,0,1,1))
#define V4PERM1032(out, a, b) out = V4PERM(a, b, SHUF(1,0,3,2))
#define V4PERM1111(out, a, b) out = V4PERM(a, b, SHUF(1,1,1,1))
#define V4PERM1132(out, a, b) out = V4PERM(a, b, SHUF(1,1,3,2))
#define V4PERM1133(out, a, b) out = V4PERM(a, b, SHUF(1,1,3,3))
#define V4PERM1313(out, a, b) out = V4PERM(a, b, SHUF(1,3,1,3))

#define V4PERM2020(out, a, b) out = V4PERM(a, b, SHUF(2,0,2,0))
#define V4PERM2222(out, a, b) out = V4PERM(a, b, SHUF(2,2,2,2))
#define V4PERM2301(out, a, b) out = V4PERM(a, b, SHUF(2,3,0,1))
#define V4PERM2323(out, a, b) out = V4PERM(a, b, SHUF(2,3,2,3))

#define V4PERM3131(out, a, b) out = V4PERM(a, b, SHUF(3,1,3,1))
#define V4PERM3210(out, a, b) out = V4PERM(a, b, SHUF(3,2,1,0))
#define V4PERM3232(out, a, b) out = V4PERM(a, b, SHUF(3,2,3,2))
#define V4PERM3333(out, a, b) out = V4PERM(a, b, SHUF(3,3,3,3))

#define v4zero mmmm

// XBox doesn't need setup.
#define SETUP_PMPM
#define SETUP_MMMM
#define SETUP_PMMP
#define SETUP_MMPP
#define SETUP_MPMP
#define SETUP_V4ONE
#define SETUP_V4ZERO

// Here the +/- zero values are used to negate specific elements:
#define V4NEG1010(out, a) out = V4XOR(a,mpmp)
#define V4NEG0101(out, a) out = V4XOR(a,pmpm)
#define V4NEG0110(out, a) out = V4XOR(a,pmmp)
#define V4NEG1100(out, a) out = V4XOR(a,mmpp)
#define V4NEG1111(out, a) out = V4XOR(a,mmmm)

// Compare pairs of fp values for (a > b), returning 000/111... for false/true
#define V4CMPGT(a, b) __vcmpgtfp(a, b)

// Convert all 4 fp values into 32-bit ints:
#define V4FTOI(a) __vctsxs(a,0)

// Pack (with saturation) 4 32-bit ints from a and 4 from b, into 8 16-bit values:
#define V4ITO8I(a, b) __vpkswss(a, b)

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#elif (defined(HOLLYWOOD_REV) || defined(REVOLUTION))
//
// Wii~~
//

// The wii doesn't have a true simd unit, so we kinda wing it.

#ifndef WII_RADVEC_STATICS_DEFINED
#define WII_RADVEC_STATICS_DEFINED

// Struct to handle arrays and parameter passing.
struct WiiVec4
{
    __vec2x32float__ l;
    __vec2x32float__ h;
};

RAD_ALIGN(static const float, pmpmraw[4], 16) = { 1.0f, -1.0f,  1.0f, -1.0f};
RAD_ALIGN(static const float, mmmmraw[4], 16) = {-1, -1, -1, -1};
RAD_ALIGN(static const float, pmmpraw[4], 16) = {1.0f, -1.0f, -1.0f, 1.0f};
RAD_ALIGN(static const float, mmppraw[4], 16) = {-1.0f, -1.0f, 1.0f, 1.0f};
RAD_ALIGN(static const float, mpmpraw[4], 16) = {-1, 1, -1, 1};
RAD_ALIGN(static const float, ones[4], 16) = {1, 1, 1, 1};
RAD_ALIGN(static const float, zeroes[4], 16) = {0};

#endif


#define DECL_VEC4F(n) register __vec2x32float__ n##l; register __vec2x32float__ n##h;
#define VEC4F_STRUCT WiiVec4
#define DECL_SCRATCH
// Load / Stores.
//-----------------------------------------------------------------------------

#define V4LOAD(out, source)\
    asm ( "psq_l     %0, 0(%1), 0, 0"\
        : "=f" (out##l)\
        : "r" (source)\
        );\
    asm ( "psq_l     %0, 8(%1), 0, 0"\
        : "=f" (out##h)\
        : "r" (source)\
        );

#define V4ASSIGN(left, right) left##l = right##l; left##h = right##h;

#define V2LOAD(out, fill, source)\
    asm ( "psq_l     %0, 0(%2), 0, 0;"\
          "ps_mr     %1, %3;"\
          : "=f" (out##l), "=f" (out##h)\
          : "r" (source), "f" (fill##h)\
        );

// Technically this doesn't zero out the other 3 words like it should, but the only place it is used doesn't need it.
#define V1LOAD(out, source)\
    asm ( "lfs     %0, 0(%2);"\
    "ps_sub  %1, %3, %3"\
    : "=f" (out##l), "=f" (out##h)\
    : "r" (source), "f" (out##h)\
        );

#define V4STORE(memloc, source)\
    asm ( "psq_st     %0, 0(%1), 0, 0"\
        :\
        : "f" (source##l), "r" (memloc)\
        );\
    asm ( "psq_st     %0, 8(%1), 0, 0"\
        :\
        : "f" (source##h), "r" (memloc) \
        );

// Arithmetic
//-----------------------------------------------------------------------------

#define V4MUL(out, a, b)\
    asm ( "ps_mul     %0, %1, %2"\
        : "=f" (out##l)\
        : "f" (a##l), "f" (b##l)\
        );\
    asm ( "ps_mul     %0, %1, %2"\
        : "=f" (out##h)\
        : "f" (a##h), "f" (b##h)\
        );\

#define V4SUB(out, a, b)\
    asm ( "ps_sub     %0, %1, %2"\
        : "=f" (out##l)\
        : "f" (a##l), "f" (b##l)\
        );\
    asm ( "ps_sub     %0, %1, %2"\
        : "=f" (out##h)\
        : "f" (a##h), "f" (b##h)\
        );\

#define V4ADD(out, a, b)\
    asm ( "ps_add     %0, %1, %2"\
        : "=f" (out##l)\
        : "f" (a##l), "f" (b##l)\
        );\
    asm ( "ps_add     %0, %1, %2"\
        : "=f" (out##h)\
        : "f" (a##h), "f" (b##h)\
        );\

// Shuffles
//-----------------------------------------------------------------------------

#define V4PERM0202(out, a, b)\
    asm ( "ps_merge00     %0, %2, %3;"\
          "ps_merge00     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##h),\
          "f" (b##l), "f" (b##h)\
        );

#define V4PERM2020(out, a, b)\
    asm ( "ps_merge00     %0, %2, %3;"\
          "ps_merge00     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##h), "f" (a##l),\
          "f" (b##h), "f" (b##l)\
        );

#define V4PERM1313(out, a, b)\
    asm ( "ps_merge11     %0, %2, %3;"\
          "ps_merge11     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##h),\
          "f" (b##l), "f" (b##h)\
        );

#define V4PERM3131(out, a, b)\
    asm ( "ps_merge11     %0, %2, %3;"\
          "ps_merge11     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##h), "f" (a##l),\
          "f" (b##h), "f" (b##l)\
        );

#define V4PERM0000(out, a, b)\
    asm ( "ps_merge00     %0, %2, %3;"\
          "ps_merge00     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##l),\
          "f" (b##l), "f" (b##l)\
        );

#define V4PERM0022(out, a, b)\
    asm ( "ps_merge00     %0, %2, %3;"\
          "ps_merge00     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##l),\
          "f" (b##h), "f" (b##h)\
        );

#define V4PERM1133(out, a, b)\
    asm ( "ps_merge11     %0, %2, %3;"\
          "ps_merge11     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##l),\
          "f" (b##h), "f" (b##h)\
        );

#define V4PERM1111(out, a, b)\
    asm ( "ps_merge11     %0, %2, %3;"\
          "ps_merge11     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##l),\
          "f" (b##l), "f" (b##l)\
        );

#define V4PERM2222(out, a, b)\
    asm ( "ps_merge00     %0, %2, %3;"\
          "ps_merge00     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##h), "f" (a##h),\
          "f" (b##h), "f" (b##h)\
        );

#define V4PERM3333(out, a, b)\
    asm ( "ps_merge11     %0, %2, %3;"\
          "ps_merge11     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##h), "f" (a##h),\
          "f" (b##h), "f" (b##h)\
        );

#define V4PERM1032(out, a, b)\
    asm ( "ps_merge10     %0, %2, %3;"\
          "ps_merge10     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##l),\
          "f" (b##h), "f" (b##h)\
        );

#define V4PERM3210(out, a, b)\
    asm ( "ps_merge10     %0, %2, %3;"\
          "ps_merge10     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##h), "f" (a##h),\
          "f" (b##l), "f" (b##l)\
        );

#define V4PERM1132(out, a, b)\
    asm ( "ps_merge11     %0, %2, %3;"\
          "ps_merge10     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##l),\
          "f" (b##h), "f" (b##h)\
        );

#define V4PERM2323(out, a, b)\
    asm ( "ps_merge01     %0, %2, %3;"\
          "ps_merge01     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##h), "f" (a##h),\
          "f" (b##h), "f" (b##h)\
        );

#define V4PERM3232(out, a, b)\
    asm ( "ps_merge10     %0, %2, %3;"\
          "ps_merge10     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##h), "f" (a##h),\
          "f" (b##h), "f" (b##h)\
        );

#define V4PERM2301(out, a, b)\
    asm ( "ps_merge01     %0, %2, %3;"\
          "ps_merge01     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##h), "f" (a##h),\
          "f" (b##l), "f" (b##l)\
        );

#define V4PERM0101(out, a, b)\
    asm ( "ps_merge01     %0, %2, %3;"\
          "ps_merge01     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##l),\
          "f" (b##l), "f" (b##l)\
        );

#define V4PERM1010(out, a, b)\
    asm ( "ps_merge10     %0, %2, %3;"\
          "ps_merge10     %1, %4, %5"\
        : "=f" (out##l), \
          "=f" (out##h)\
        : "f" (a##l), "f" (a##l),\
          "f" (b##l), "f" (b##l)\
        );

// Wii requires local scope negation setup.
//-----------------------------------------------------------------------------

#define SETUP_PMPM \
   DECL_VEC4F(pmpm);\
   V4LOAD(pmpm, pmpmraw);

#define SETUP_MMMM \
   DECL_VEC4F(mmmm);\
   V4LOAD(mmmm, mmmmraw);

#define SETUP_PMMP \
    DECL_VEC4F(pmmp);\
    V4LOAD(pmmp, pmmpraw);

#define SETUP_MMPP \
    DECL_VEC4F(mmpp);\
    V4LOAD(mmpp, mmppraw);

#define SETUP_MPMP \
   DECL_VEC4F(mpmp);\
   V4LOAD(mpmp, mpmpraw);

#define SETUP_V4ONE \
   DECL_VEC4F(v4one); \
   V4LOAD(v4one, ones);

#define SETUP_V4ZERO \
   DECL_VEC4F(v4zero);\
   V4LOAD(v4zero, zeroes);

#define V4NEG0101(out, a) V4MUL(out, a, pmpm)
#define V4NEG1100(out, a) V4MUL(out, a, mmpp)
#define V4NEG1010(out, a) V4MUL(out, a, mpmp)
#define V4NEG1111(out, a) V4MUL(out, a, mmmm)
#define V4NEG0110(out, a) V4MUL(out, a, pmmp)

#endif // wii

#endif // not RADVEC_FORCE_FLOATS

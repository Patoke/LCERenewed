//-----------------------------------------------------------------------------
/*!
    Setup merge mixers for each platform.

    All platforms must #define a CALL_MIXER_MERGE that matches the following:
        #define CALL_MIXER_MERGE(Op, Sample, Src, SrcEnd, Dest, DestEnd, Left, Right)

*/
//-----------------------------------------------------------------------------
#if (defined(IS_PS3) && !defined(MSS_SPU_PROCESS)) || (defined(IS_MAC) && defined(IS_PPC))


static RAD_ALIGN(unsigned char,sbswap16e[ 16 ],16) =
{
  0x01, 0x00, 0x05, 0x04,
  0x01, 0x00, 0x05, 0x04,
  0x03, 0x02, 0x07, 0x06,
  0x03, 0x02, 0x07, 0x06
};

static RAD_ALIGN(unsigned char,sbswap16o[ 16 ],16) =
{
  0x09, 0x08, 0x0d, 0x0c,
  0x09, 0x08, 0x0d, 0x0c,
  0x0b, 0x0a, 0x0f, 0x0e,
  0x0b, 0x0a, 0x0f, 0x0e
};

static RAD_ALIGN(unsigned char,sbswap16[ 16 ],16) =
{
  0x01, 0x00, 0x09, 0x08,
  0x03, 0x02, 0x0b, 0x0a,
  0x05, 0x04, 0x0d, 0x0c,
  0x07, 0x06, 0x0f, 0x0e
};

static RAD_ALIGN(unsigned char,sbswap16s[ 16 ],16) =
{
  0x01, 0x00, 0x03, 0x02,
  0x05, 0x04, 0x07, 0x06,
  0x09, 0x08, 0x0b, 0x0a,
  0x0d, 0x0c, 0x0f, 0x0e
};

static RAD_ALIGN(unsigned char,msvolrep[ 16 ],16) =
{
  0x00, 0x01, 0x00, 0x01,
  0x02, 0x03, 0x02, 0x03,
  0x00, 0x01, 0x00, 0x01,
  0x02, 0x03, 0x02, 0x03
};

static RAD_ALIGN(unsigned char,msvolreps[ 16 ],16) =
{
  0x00, 0x01, 0x02, 0x03,
  0x00, 0x01, 0x02, 0x03,
  0x00, 0x01, 0x02, 0x03,
  0x00, 0x01, 0x02, 0x03
};

#define vec_U8  vector unsigned char
#define vec_S16 vector signed short
#define vec_S32 vector signed int

static void srcmono_deststereo_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
  S32 src_count, dest_count, count;
  void * MSSRESTRICT src = *srcp;
  void * MSSRESTRICT dest = *destp;
  vec_U8 bswap16e;
  vec_U8 bswap16o;
  vec_S16 vol;

  count = ( left << 16 ) | right;

  vol = (vec_S16)((vec_S32){count});
  bswap16e = *(vec_U8*)msvolrep;
  vol = vec_perm( vol, vol, bswap16e );

  bswap16e = *(vec_U8*)sbswap16e;
  bswap16o = *(vec_U8*)sbswap16o;

  count = AIL_ptr_dif( src_end, src ) >> 1; // 16-bit mono

  dest_count = AIL_ptr_dif( dest_end, dest ) >> 3; // 32-bit stereo
  if ( count > dest_count )
    count = dest_count;

  while ( count >= 8 )
  {
    vec_S16 inv0, inv1;
    vec_S32 outv0, outv1, outv2, outv3;

#ifdef IS_MAC
    inv0 = vec_ld(0, (vec_S16*)src);
    inv1 = vec_ld(15, (vec_S16*)src);
    outv0 = vec_lvsl(0, (unsigned int*)src);
    inv1 = vec_perm(inv0, inv1, outv0);
#else
    inv1 = vec_lvlx(0, (vec_S16*)src);
    inv0 = vec_lvrx(16, (vec_S16*)src);
    inv1 = vec_or(inv0, inv1);
#endif

    inv0 = vec_perm( inv1, inv1, bswap16e );
    inv1 = vec_perm( inv1, inv1, bswap16o );

    outv0 = vec_mule( inv0, vol );
    outv1 = vec_mulo( inv0, vol );

    outv2 = vec_mule( inv1, vol );
    outv3 = vec_mulo( inv1, vol );

    ((vec_S32* MSSRESTRICT)dest)[ 0 ] += outv0;
    ((vec_S32* MSSRESTRICT)dest)[ 1 ] += outv1;
    ((vec_S32* MSSRESTRICT)dest)[ 2 ] += outv2;
    ((vec_S32* MSSRESTRICT)dest)[ 3 ] += outv3;

    src = AIL_ptr_add( src, 16 );
    dest = AIL_ptr_add( dest, 64 );
    count -= 8;
  }

  *srcp = src;
  *destp = dest;
}

static void srcstereo_deststereo_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
  void * MSSRESTRICT src = *srcp;
  void * MSSRESTRICT dest = *destp;
  S32 src_count, dest_count, count;
  vec_U8 bswap16;
  vec_S16 vol;

  count = ( left << 16 ) | right;


  vol = (vec_S16)((vec_S32){count});
  bswap16 = *(vec_U8*)msvolrep;
  vol = vec_perm( vol, vol, bswap16 );

  bswap16 = *(vec_U8*)sbswap16;

  count = AIL_ptr_dif( src_end, src ) >> 2; // 16-bit stereo

  dest_count = AIL_ptr_dif( dest_end, dest ) >> 3; // 32-bit stereo
  if ( count > dest_count )
    count = dest_count;

  while ( count >= 4 )
  {
    vec_S16 inv, inv1;
    vec_S32 outv0, outv1;

#ifdef IS_MAC
    inv = vec_ld(0, (vec_S16*)src);
    inv1 = vec_ld(15, (vec_S16*)src);
    outv0 = vec_lvsl(0, (unsigned int*)src);
    inv1 = vec_perm(inv, inv1, outv0);
#else
    inv = vec_lvlx(0, (vec_S16*)src);
    inv1 = vec_lvrx(16, (vec_S16*)src);
    inv = vec_or(inv, inv1);
#endif

    inv = vec_perm( inv, inv, bswap16 );

    outv0 = vec_mule( inv, vol );
    outv1 = vec_mulo( inv, vol );

    ((vec_S32* MSSRESTRICT)dest)[ 0 ] += outv0;
    ((vec_S32* MSSRESTRICT)dest)[ 1 ] += outv1;

    src = AIL_ptr_add( src, 16 );
    dest = AIL_ptr_add( dest, 32 );
    count -= 4;
  }

  *srcp = src;
  *destp = dest;
}

static void srcmono_destmono_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
  void * MSSRESTRICT src = *srcp;
  void * MSSRESTRICT dest = *destp;
  S32 src_count, dest_count, count;
  vec_U8 bswap16;
  vec_S16 vol;

  vol = (vec_S16){(short)left};
  bswap16 = (vec_U8)vec_splat_u16(1);
  vol = vec_perm( vol, vol, bswap16 );

  bswap16 = *(vec_U8*)sbswap16;

  count = AIL_ptr_dif( src_end, src ) >> 1; // 16-bit mono

  dest_count = AIL_ptr_dif( dest_end, dest ) >> 2; // 32-bit mono
  if ( count > dest_count )
    count = dest_count;

  while ( count >= 8 )
  {
    vec_S16 inv, inv1;
    vec_S32 outv0, outv1;

#ifdef IS_MAC
    inv = vec_ld(0, (vec_S16*)src);
    inv1 = vec_ld(15, (vec_S16*)src);
    outv0 = vec_lvsl(0, (unsigned int*)src);
    inv1 = vec_perm(inv, inv1, outv0);
#else
    inv = vec_lvlx(0, (vec_S16*)src);
    inv1 = vec_lvrx(16, (vec_S16*)src);
    inv = vec_or(inv, inv1);
#endif


    inv = vec_perm( inv, inv, bswap16 );

    outv0 = vec_mule( inv, vol );
    outv1 = vec_mulo( inv, vol );

    ((vec_S32* MSSRESTRICT)dest)[ 0 ] += outv0;
    ((vec_S32* MSSRESTRICT)dest)[ 1 ] += outv1;

    src = AIL_ptr_add( src, 16 );
    dest = AIL_ptr_add( dest, 32 );
    count -= 8;
  }

  *srcp = src;
  *destp = dest;
}

static void srcstereo_destmono_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
  void * MSSRESTRICT src = *srcp;
  void * MSSRESTRICT dest = *destp;
  S32 src_count, dest_count, count;
  vec_U8 bswap16;
  vec_S16 vol;

  count = ( left << 16 ) | right;

  vol = (vec_S16)((vec_S32){count});
  bswap16 = *(vec_U8*)msvolreps;
  vol = vec_perm( vol, vol, bswap16 );

  bswap16 = *(vec_U8*)sbswap16s;

  count = AIL_ptr_dif( src_end, src ) >> 2; // 16-bit stereo

  dest_count = AIL_ptr_dif( dest_end, dest ) >> 2; // 32-bit mono
  if ( count > dest_count )
    count = dest_count;

  while ( count >= 4 )
  {
    vec_S16 inv, inv1;
    vec_S32 outv0, outv1;

#ifdef IS_MAC
    inv = vec_ld(0, (vec_S16*)src);
    inv1 = vec_ld(15, (vec_S16*)src);
    outv0 = vec_lvsl(0, (unsigned int*)src);
    inv1 = vec_perm(inv, inv1, outv0);
#else
    inv = vec_lvlx(0, (vec_S16*)src);
    inv1 = vec_lvrx(16, (vec_S16*)src);
    inv = vec_or(inv, inv1);
#endif

    inv = vec_perm( inv, inv, bswap16 );

    outv0 = vec_mule( inv, vol );
    outv1 = vec_mulo( inv, vol );

    ((vec_S32* MSSRESTRICT)dest)[ 0 ] += outv0 + outv1;

    src = AIL_ptr_add( src, 16 );
    dest = AIL_ptr_add( dest, 16 );
    count -= 4;
  }

  *srcp = src;
  *destp = dest;
}

typedef void simple_MC_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right );

static simple_MC_mix * MC_mixers[ 4 ] = { srcmono_destmono_mix, srcmono_deststereo_mix, srcstereo_destmono_mix, srcstereo_deststereo_mix };

#define CALL_MIXER_MERGE(Op, Sample, Src, SrcEnd, Dest, DestEnd, Left, Right) \
    MC_mixers[ (Op&1)|((Op>>2)&2) ](Src, SrcEnd, Dest, DestEnd, Left, Right);

#elif defined(IS_PS2)

#define DECL_VEC4F(n) register u_long128 __attribute__((aligned(16))) n = 0;

static void srcmono_deststereo_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
  void * MSSRESTRICT src = *srcp;
  void * MSSRESTRICT dest = *destp;
  S32 src_count, dest_count;
  RAD_ALIGN(S32, count, 16);
  DECL_VEC4F(vol);

  count = ( right << 16 ) | left;

  // Load the left and right balances into the sse reg.
  asm volatile ("ld %0, %1" : "=r" (vol) : "m" (count) );

  // Arrange the volume so its interleaving l/r every 16 bits.
  asm volatile ("pextlw %0, %1, %2" : "=r" (vol) : "r" (vol) , "r" (vol));
  asm volatile ("pextlw %0, %1, %2" : "=r" (vol) : "r" (vol) , "r" (vol));

  count = AIL_ptr_dif( src_end, src ) >> 1; // 16-bit mono

  dest_count = AIL_ptr_dif( dest_end, dest ) >> 3; // 32-bit stereo
  if ( count > dest_count )
    count = dest_count;

  while ( count >= 8 )
  {
      DECL_VEC4F(inv0);
      DECL_VEC4F(inv1);
      DECL_VEC4F(outv0);
      DECL_VEC4F(outv1);
      DECL_VEC4F(outv2);
      DECL_VEC4F(outv3);
      DECL_VEC4F(Low);
      DECL_VEC4F(High);

      // Load 8 samples.
      asm volatile ("ldr %0, 0(%1)"       : "=r" (Low) : "r" (src)); \
      asm volatile ("ldl %0, 7(%1)"       : "=r" (Low) : "r" (src), "r" (Low) ); \
      asm volatile ("ldr %0, 8(%1)"       : "=r" (High) : "r" (src) ); \
      asm volatile ("ldl %0, 15(%1)"       : "=r" (High) : "r" (src), "r" (High) ); \
      asm volatile ("pcpyld %0, %1, %2"   : "=r" (inv1) : "r" (High), "r" (Low), "r" (inv1) ); \

      // Setup inv0 to be sample 0,0,1,1,2,2,3,3 and inv1 to be 4,4,5,5,6,6,7,7.
      asm volatile ("pextlh %0, %1, %1" : "=r" (inv0) : "r" (inv1));
      asm volatile ("pextuh %0, %1, %1" : "=r" (inv1) : "r" (inv1));

      // Multiply samples [0,3] by the volumes.
      asm volatile ("pmulth %0, %1, %2" : "=r" (outv0) : "r" (inv0), "r" (vol));

      // The results we care about are int LO and HI.
      asm volatile ("pmflo %0" : "=r" (outv2));
      asm volatile ("pmfhi %0" : "=r" (outv3));

      // The results have the 0,2 on lo and 1,3 in hi.
      asm volatile ("pcpyld %0, %1, %2" : "=r" (outv0) : "r" (outv3), "r" (outv2));
      asm volatile ("pcpyud %0, %1, %2" : "=r" (outv1) : "r" (outv2), "r" (outv3));

      // Add samples [0,3] to the dest.
      asm volatile ("lq %0, 0(%1)" : "=r" (outv2) : "r" (dest));
      asm volatile ("lq %0, 16(%1)" : "=r" (outv3) : "r" (dest));
      asm volatile ("paddw %0, %1, %2" : "=r" (outv2) : "r" (outv2), "r" (outv0));
      asm volatile ("paddw %0, %1, %2" : "=r" (outv3) : "r" (outv3), "r" (outv1));
      asm volatile ("sq %0, 0(%1)" : : "r" (outv2), "r" (dest) );
      asm volatile ("sq %0, 16(%1)" : : "r" (outv3), "r" (dest) );

      // Multiply samples [4,7] by the volumes.
      asm volatile ("pmulth %0, %1, %2" : "=r" (outv0) : "r" (inv1), "r" (vol));

      // The results we care about are int LO and HI.
      asm volatile ("pmflo %0" : "=r" (outv2));
      asm volatile ("pmfhi %0" : "=r" (outv3));

      // The results have the 4,6 on lo and 5,7 in hi.
      asm volatile ("pcpyld %0, %1, %2" : "=r" (outv0) : "r" (outv3), "r" (outv2));
      asm volatile ("pcpyud %0, %1, %2" : "=r" (outv1) : "r" (outv2), "r" (outv3));

      // Add samples [4,7] to the dest.
      asm volatile ("lq %0, 32(%1)" : "=r" (outv2) : "r" (dest));
      asm volatile ("lq %0, 48(%1)" : "=r" (outv3) : "r" (dest));
      asm volatile ("paddw %0, %1, %2" : "=r" (outv2) : "r" (outv2), "r" (outv0));
      asm volatile ("paddw %0, %1, %2" : "=r" (outv3) : "r" (outv3), "r" (outv1));
      asm volatile ("sq %0, 32(%1)" : : "r" (outv2), "r" (dest) );
      asm volatile ("sq %0, 48(%1)" : : "r" (outv3), "r" (dest) );

      src = AIL_ptr_add( src, 16 );
      dest = AIL_ptr_add( dest, 64 );
      count -= 8;
  }

  *srcp = src;
  *destp = dest;
}

static void srcstereo_deststereo_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
  void * MSSRESTRICT src = *srcp;
  void * MSSRESTRICT dest = *destp;
  S32 src_count, dest_count;
  RAD_ALIGN(S32, count, 16);

  DECL_VEC4F(vol);

  count = ( right << 16 ) | left;

  // Load the left and right balances into the sse reg.
  asm volatile ("ld %0, %1" : "=r" (vol) : "m" (count) );

  // Arrange the volume so its interleaving l/r every 16 bits.
  asm volatile ("pextlw %0, %1, %2" : "=r" (vol) : "r" (vol) , "r" (vol));
  asm volatile ("pextlw %0, %1, %2" : "=r" (vol) : "r" (vol) , "r" (vol));

  count = AIL_ptr_dif( src_end, src ) >> 2; // 16-bit stereo

  dest_count = AIL_ptr_dif( dest_end, dest ) >> 3; // 32-bit stereo
  if ( count > dest_count )
    count = dest_count;

  while ( count >= 4 )
  {
    DECL_VEC4F(inv);
    DECL_VEC4F(outv0);
    DECL_VEC4F(outv1);
    DECL_VEC4F(outv2);
    DECL_VEC4F(outv3);

    DECL_VEC4F(Low);
    DECL_VEC4F(High);

    // Load 8 samples.
    asm volatile ("ldr %0, 0(%1)"       : "=r" (Low) : "r" (src)); \
    asm volatile ("ldl %0, 7(%1)"       : "=r" (Low) : "r" (src), "r" (Low) ); \
    asm volatile ("ldr %0, 8(%1)"       : "=r" (High) : "r" (src) ); \
    asm volatile ("ldl %0, 15(%1)"       : "=r" (High) : "r" (src), "r" (High) ); \
    asm volatile ("pcpyld %0, %1, %2"   : "=r" (inv) : "r" (High), "r" (Low), "r" (inv) ); \

    // Multiply samples [0,3] by the volumes.
    asm volatile ("pmulth %0, %1, %2" : "=r" (outv0) : "r" (inv), "r" (vol));

    // The results we care about are int LO and HI.
    asm volatile ("pmflo %0" : "=r" (outv2));
    asm volatile ("pmfhi %0" : "=r" (outv3));

    // The results have the 0,2 on lo and 1,3 in hi.
    asm volatile ("pcpyld %0, %1, %2" : "=r" (outv0) : "r" (outv3), "r" (outv2));
    asm volatile ("pcpyud %0, %1, %2" : "=r" (outv1) : "r" (outv2), "r" (outv3));

    // Add samples [0,3] to the dest.
    asm volatile ("lq %0, 0(%1)" : "=r" (outv2) : "r" (dest));
    asm volatile ("lq %0, 16(%1)" : "=r" (outv3) : "r" (dest));
    asm volatile ("paddw %0, %1, %2" : "=r" (outv2) : "r" (outv2), "r" (outv0));
    asm volatile ("paddw %0, %1, %2" : "=r" (outv3) : "r" (outv3), "r" (outv1));
    asm volatile ("sq %0, 0(%1)" : : "r" (outv2), "r" (dest) );
    asm volatile ("sq %0, 16(%1)" : : "r" (outv3), "r" (dest) );

    src = AIL_ptr_add( src, 16 );
    dest = AIL_ptr_add( dest, 32 );
    count -= 4;
  }

  *srcp = src;
  *destp = dest;
}


static void srcmono_destmono_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
  void * MSSRESTRICT src = *srcp;
  void * MSSRESTRICT dest = *destp;
  S32 src_count, dest_count;
  RAD_ALIGN(S32, count, 16);
  DECL_VEC4F(vol);

  // Copy the left to an aligned source.
  count = left;

  // Load the left channel and replicate across the register.
  asm volatile ("lh %0, %1" : "=r" (vol) : "m" (count) );
  asm volatile ("pcpyh %0, %1" : "=r" (vol) : "r" (vol) );
  asm volatile ("pcpyld %0, %1, %1" : "=r" (vol) : "r" (vol) );

  count = AIL_ptr_dif( src_end, src ) >> 1; // 16-bit mono

  dest_count = AIL_ptr_dif( dest_end, dest ) >> 2; // 32-bit mono
  if ( count > dest_count )
    count = dest_count;

  while ( count >= 8 )
  {
      DECL_VEC4F(inv);
      DECL_VEC4F(outv0);
      DECL_VEC4F(outv1);
      DECL_VEC4F(outv2);
      DECL_VEC4F(outv3);

      DECL_VEC4F(Low);
      DECL_VEC4F(High);

      // Load 8 samples.
      asm volatile ("ldr %0, 0(%1)"       : "=r" (Low) : "r" (src)); \
      asm volatile ("ldl %0, 7(%1)"       : "=r" (Low) : "r" (src), "r" (Low) ); \
      asm volatile ("ldr %0, 8(%1)"       : "=r" (High) : "r" (src) ); \
      asm volatile ("ldl %0, 15(%1)"       : "=r" (High) : "r" (src), "r" (High) ); \
      asm volatile ("pcpyld %0, %1, %2"   : "=r" (inv) : "r" (High), "r" (Low), "r" (inv) ); \

      // Multiply samples [0,7] by the volumes.
      asm volatile ("pmulth %0, %1, %2" : "=r" (outv0) : "r" (inv), "r" (vol));

      // The results we care about are int LO and HI.
      asm volatile ("pmflo %0" : "=r" (outv2));
      asm volatile ("pmfhi %0" : "=r" (outv3));

      // The results have the 0,1,4,5 on lo and 2,3,5,6 in hi.
      asm volatile ("pcpyld %0, %1, %2" : "=r" (outv0) : "r" (outv3), "r" (outv2));
      asm volatile ("pcpyud %0, %1, %2" : "=r" (outv1) : "r" (outv2), "r" (outv3));

      // Add samples [0,8] to the dest.
      asm volatile ("lq %0, 0(%1)" : "=r" (outv2) : "r" (dest));
      asm volatile ("lq %0, 16(%1)" : "=r" (outv3) : "r" (dest));
      asm volatile ("paddw %0, %1, %2" : "=r" (outv2) : "r" (outv2), "r" (outv0));
      asm volatile ("paddw %0, %1, %2" : "=r" (outv3) : "r" (outv3), "r" (outv1));
      asm volatile ("sq %0, 0(%1)" : : "r" (outv2), "r" (dest) );
      asm volatile ("sq %0, 16(%1)" : : "r" (outv3), "r" (dest) );

      src = AIL_ptr_add( src, 16 );
      dest = AIL_ptr_add( dest, 32 );
      count -= 8;
  }

  *srcp = src;
  *destp = dest;
}

static void srcstereo_destmono_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
  void * MSSRESTRICT src = *srcp;
  void * MSSRESTRICT dest = *destp;
  S32 src_count, dest_count;
  RAD_ALIGN(S32, count, 16);
  DECL_VEC4F(vol);

  count = ( right << 16 ) | left;

  // Load the left and right balances into the sse reg.
  asm volatile ("ld %0, %1" : "=r" (vol) : "m" (count) );

  // Arrange the volume so its interleaving l/r every 16 bits.
  asm volatile ("pextlw %0, %1, %2" : "=r" (vol) : "r" (vol) , "r" (vol));
  asm volatile ("pextlw %0, %1, %2" : "=r" (vol) : "r" (vol) , "r" (vol));

  count = AIL_ptr_dif( src_end, src ) >> 2; // 16-bit stereo

  dest_count = AIL_ptr_dif( dest_end, dest ) >> 2; // 32-bit mono
  if ( count > dest_count )
    count = dest_count;

  while ( count >= 4 )
  {
      DECL_VEC4F(inv);
      DECL_VEC4F(outv0);
      DECL_VEC4F(outv1);
      DECL_VEC4F(outv2);
      DECL_VEC4F(outv3);

      DECL_VEC4F(Low);
      DECL_VEC4F(High);

      // Load 4 samples.
      asm volatile ("ldr %0, 0(%1)"       : "=r" (Low) : "r" (src)); \
      asm volatile ("ldl %0, 7(%1)"       : "=r" (Low) : "r" (src), "r" (Low) ); \
      asm volatile ("ldr %0, 8(%1)"       : "=r" (High) : "r" (src) ); \
      asm volatile ("ldl %0, 15(%1)"       : "=r" (High) : "r" (src), "r" (High) ); \
      asm volatile ("pcpyld %0, %1, %2"   : "=r" (inv) : "r" (High), "r" (Low), "r" (inv) ); \

      // Multiply samples [0,3] by the volumes.
      asm volatile ("pmulth %0, %1, %2" : "=r" (outv0) : "r" (inv), "r" (vol));

      // The results we care about are int LO and HI.
      asm volatile ("pmflo %0" : "=r" (outv2));
      asm volatile ("pmfhi %0" : "=r" (outv3));

      // The results have the 0,2 on lo and 1,3 in hi.
      asm volatile ("pcpyld %0, %1, %2" : "=r" (outv0) : "r" (outv3), "r" (outv2));
      asm volatile ("pcpyud %0, %1, %2" : "=r" (outv1) : "r" (outv2), "r" (outv3));

      // Add the channels to go to mono.
      asm volatile ("paddw %0, %1, %2" : "=r" (outv0) : "r" (outv0), "r" (outv1));

      // Add samples [0,3] to the dest.
      asm volatile ("lq %0, 0(%1)" : "=r" (outv2) : "r" (dest));
      asm volatile ("paddw %0, %1, %2" : "=r" (outv2) : "r" (outv2), "r" (outv0));
      asm volatile ("sq %0, 0(%1)" : : "r" (outv2), "r" (dest) );

      src = AIL_ptr_add( src, 16 );
      dest = AIL_ptr_add( dest, 16 );
      count -= 4;
  }

  *srcp = src;
  *destp = dest;
}

typedef void simple_MC_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right );

static simple_MC_mix * MC_mixers[ 4 ] = { srcmono_destmono_mix, srcmono_deststereo_mix, srcstereo_destmono_mix, srcstereo_deststereo_mix };

#define CALL_MIXER_MERGE(Op, Sample, Src, SrcEnd, Dest, DestEnd, Left, Right) \
    MC_mixers[(Op&1)|((Op>>2)&2)](Src, SrcEnd, Dest, DestEnd, Left, Right);

#elif defined(IS_XENON)
#include <vectorintrinsics.h>

/* Vector types */
#define vec_float __vector4
#define vec_U8 __vector4
#define vec_S16 __vector4
#define vec_S32 __vector4

/* Convert a little-endian 16-bit integer to a float */
#define LE16_TO_FLOAT(x) \
    ((float)(short)((((x) >> 8) & 0xffu) | (((x) & 0xffu) << 8)))

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define vec_ctfS32(i_vSrcA, i_uImmed) __vcfsx(i_vSrcA, i_uImmed)
#define vec_perm(i_vSrcA, i_vSrcB, i_vPermControl) __vperm(i_vSrcA, i_vSrcB, i_vPermControl)
#define vec_cts(i_vSrcA, i_uImmed) __vctsxs(i_vSrcA, i_uImmed)
#define vec_unpackhS16(i_vSrcA) __vupkhsh(i_vSrcA)
#define vec_unpacklS16(i_vSrcA) __vupklsh(i_vSrcA)

/*
 * Mix a mono little-endian 16-bit source onto a stereo host-endian 32-bit destination
 * NOTE: Both src and dest must be 16 byte aligned
 */
void srcmono_deststereo_mix_vmx128(void** srcp,
                                   void*  i_voidsrc_end,
                                   void** destp,
                                   void*  i_voiddest_end,
                                   S32    left,
                                   S32    right)
{
    void * MSSRESTRICT i_voidsrc = *srcp;
    void * MSSRESTRICT i_voiddest = *destp;

    const short* src = (const short*)i_voidsrc;
    const short* src_end = (const short*)i_voidsrc_end;
    int* dest = (int*)i_voiddest;
    int* dest_end = (int*)i_voiddest_end;

    size_t count = MIN(src_end - src, (dest_end - dest) / 2);
    vec_float vol;
    static RAD_ALIGN(unsigned char, bswap16h_raw[16], 16) = {1, 0, 1, 0, 3, 2, 3, 2, 5, 4, 5, 4, 7, 6, 7, 6};
    vec_U8 bswap16h = *(__vector4*)bswap16h_raw;

    static RAD_ALIGN(unsigned char, bswap16l_raw[16], 16) = {9, 8, 9, 8, 11, 10, 11, 10, 13, 12, 13, 12, 15, 14, 15, 14};
    vec_U8 bswap16l = *(__vector4*)bswap16l_raw;

    /* Convert volume to a float vector */
    RAD_ALIGN(S32, rawvol[4], 16) = {left, right, left, right};
    vol = vec_ctfS32(*(__vector4*)rawvol, 0);

    while (count >= 8) {
        vec_S16 inh, inl;

        /* Read 8 source samples */
        inl = __lvlx( src, 0 );
        inh = __lvrx( src, 16 );
        inl = __vor( inl, inh );

        /* Convert from little endian and replicate */
        inh = vec_perm(inl, inl, bswap16h);
        inl = vec_perm(inl, inl, bswap16l);

        /* Sign-extend, convert to float, multiply, convert back and add */
        ((vec_S32*)dest)[0] = __vaddsws(((vec_S32*)dest)[0], vec_cts(__vmulfp(vec_ctfS32(vec_unpackhS16(inh), 0), vol), 0));
        ((vec_S32*)dest)[1] = __vaddsws(((vec_S32*)dest)[1], vec_cts(__vmulfp(vec_ctfS32(vec_unpacklS16(inh), 0), vol), 0));
        ((vec_S32*)dest)[2] = __vaddsws(((vec_S32*)dest)[2], vec_cts(__vmulfp(vec_ctfS32(vec_unpackhS16(inl), 0), vol), 0));
        ((vec_S32*)dest)[3] = __vaddsws(((vec_S32*)dest)[3], vec_cts(__vmulfp(vec_ctfS32(vec_unpacklS16(inl), 0), vol), 0));

        src += 8;
        dest += 16;
        count -= 8;
    }

    *srcp = (void*) src;
    *destp = (void*) dest;
}

void srcstereo_deststereo_mix_vmx128(void** srcp,
                                     void*  i_voidsrc_end,
                                     void** destp,
                                     void*  i_voiddest_end,
                                     S32    left,
                                     S32    right)
{
    void * MSSRESTRICT i_voidsrc = *srcp;
    void * MSSRESTRICT i_voiddest = *destp;

    const short* src = (const short*)i_voidsrc;
    const short* src_end = (const short*)i_voidsrc_end;
    int* dest = (int*)i_voiddest;
    int* dest_end = (int*)i_voiddest_end;

    size_t count = MIN((src_end - src) / 2, (dest_end - dest) / 2);
    vec_float vol;
    static RAD_ALIGN(unsigned char, bswap16_raw[16], 16) = {1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14};
    vec_U8 bswap16 = *(__vector4*)bswap16_raw;

    /* Convert volume to a float vector */
    RAD_ALIGN(S32, rawvol[4], 16) = {left, right, left, right};
    vol = vec_ctfS32(*(__vector4*)rawvol, 0);

    while (count >= 8) {
        vec_S16 inh, inl, tmpl, tmph;

        /* Read 16 source samples */
        inh = __lvlx( src, 0 );
        tmph = __lvrx( src, 16 );
        inl = __lvlx( src, 16 );
        tmpl = __lvrx( src, 32 );
        inh = __vor( inh, tmph );
        inl = __vor( inl, tmpl );

        /* Convert from little endian */
        inh = vec_perm(inh, inh, bswap16);
        inl = vec_perm(inl, inl, bswap16);

        /* Sign-extend, convert to float, multiply, convert back and add */
        ((vec_S32*)dest)[0] = __vaddsws(((vec_S32*)dest)[0], vec_cts(__vmulfp(vec_ctfS32(vec_unpackhS16(inh), 0), vol), 0));
        ((vec_S32*)dest)[1] = __vaddsws(((vec_S32*)dest)[1], vec_cts(__vmulfp(vec_ctfS32(vec_unpacklS16(inh), 0), vol), 0));
        ((vec_S32*)dest)[2] = __vaddsws(((vec_S32*)dest)[2], vec_cts(__vmulfp(vec_ctfS32(vec_unpackhS16(inl), 0), vol), 0));
        ((vec_S32*)dest)[3] = __vaddsws(((vec_S32*)dest)[3], vec_cts(__vmulfp(vec_ctfS32(vec_unpacklS16(inl), 0), vol), 0));

        src += 16;
        dest += 16;
        count -= 8;
    }

    *srcp = (void*) src;
    *destp = (void*) dest;
}

/*
 * Mix a mono little-endian 16-bit source onto a mono host-endian 32-bit destination
 * NOTE: Both src and dest must be 16 byte aligned
 */
void srcmono_destmono_mix_vmx128(void** srcp,
                                 void*  i_voidsrc_end,
                                 void** destp,
                                 void*  i_voiddest_end,
                                 S32 left,
                                 S32 right)
{
    void * MSSRESTRICT i_voidsrc = *srcp;
    void * MSSRESTRICT i_voiddest = *destp;

    const short* src = (const short*)i_voidsrc;
    const short* src_end = (const short*)i_voidsrc_end;
    int* dest = (int*)i_voiddest;
    int* dest_end = (int*)i_voiddest_end;

    size_t count = MIN(src_end - src, dest_end - dest);
    vec_float vol;

    static RAD_ALIGN(unsigned char, bswap16_raw[16], 16) = {1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14};
    vec_U8 bswap16 = *(__vector4*)bswap16_raw;

    /* Convert volume to a float vector */
    RAD_ALIGN(S32, rawvol[4], 16) = {left, left, left, left};
    vol = vec_ctfS32(*(__vector4*)rawvol, 0);

    while (count >= 16) {
        vec_S16 inh, inl, tmpl, tmph;

        /* Read 16 source samples */
        inh = __lvlx( src, 0 );
        tmph = __lvrx( src, 16 );
        inl = __lvlx( src, 16 );
        tmpl = __lvrx( src, 32 );
        inh = __vor( inh, tmph );
        inl = __vor( inl, tmpl );

        /* Convert from little endian */
        inh = vec_perm(inh, inh, bswap16);
        inl = vec_perm(inl, inl, bswap16);

        /* Sign-extend, convert to float, multiply, convert back and add */
        ((vec_S32*)dest)[0] = __vaddsws(((vec_S32*)dest)[0], vec_cts(__vmulfp(vec_ctfS32(vec_unpackhS16(inh), 0), vol), 0));
        ((vec_S32*)dest)[1] = __vaddsws(((vec_S32*)dest)[1], vec_cts(__vmulfp(vec_ctfS32(vec_unpacklS16(inh), 0), vol), 0));
        ((vec_S32*)dest)[2] = __vaddsws(((vec_S32*)dest)[2], vec_cts(__vmulfp(vec_ctfS32(vec_unpackhS16(inl), 0), vol), 0));
        ((vec_S32*)dest)[3] = __vaddsws(((vec_S32*)dest)[3], vec_cts(__vmulfp(vec_ctfS32(vec_unpacklS16(inl), 0), vol), 0));

        src += 16;
        dest += 16;
        count -= 16;
    }

    *srcp = (void*) src;
    *destp = (void*) dest;
}

/*
 * Mix a stereo little-endian 16-bit source onto a mono host-endian 32-bit destination
 * NOTE: Both src and dest must be 16 byte aligned
 */
void srcstereo_destmono_mix_vmx128(void** srcp,
                                   void*  i_voidsrc_end,
                                   void** destp,
                                   void*  i_voiddest_end,
                                   S32 left,
                                   S32 right)
{
    void * MSSRESTRICT i_voidsrc = *srcp;
    void * MSSRESTRICT i_voiddest = *destp;

    const short* src = (const short*)i_voidsrc;
    const short* src_end = (const short*)i_voidsrc_end;
    int* dest = (int*)i_voiddest;
    int* dest_end = (int*)i_voiddest_end;

    size_t count = MIN((src_end - src) / 2, dest_end - dest);
    vec_float voll, volr;
    static RAD_ALIGN(unsigned char, bswap16_raw[16], 16) = {1, 0, 5, 4, 9, 8, 13, 12, 3, 2, 7, 6, 11, 10, 15, 14};
    vec_U8 bswap16 = *(__vector4*)bswap16_raw;

    /* Convert volume to a float vector */
    RAD_ALIGN(S32, rawvoll[4], 16) = {left, left, left, left};
    voll = vec_ctfS32(*(__vector4*)rawvoll, 0);
    RAD_ALIGN(S32, rawvolr[4], 16) = {right, right, right, right};
    volr = vec_ctfS32(*(__vector4*)rawvolr, 0);

    while (count >= 8) {
        vec_S16 inh, inl, tmpl, tmph;
        vec_S32 out;

        /* Read 8 source samples */
        /* Read 16 source samples */
        inh = __lvlx( src, 0 );
        tmph = __lvrx( src, 16 );
        inl = __lvlx( src, 16 );
        tmpl = __lvrx( src, 32 );
        inh = __vor( inh, tmph );
        inl = __vor( inl, tmpl );

        /* Convert from little endian and separate left/right channels */
        inh = vec_perm(inh, inh, bswap16);
        inl = vec_perm(inl, inl, bswap16);

        /* Sign-extend, convert to float, multiply, convert back and add */
        out = ((vec_S32*)dest)[0];
        out = __vaddsws(out, vec_cts(__vmulfp(vec_ctfS32(vec_unpackhS16(inh), 0), voll), 0));
        out = __vaddsws(out, vec_cts(__vmulfp(vec_ctfS32(vec_unpacklS16(inh), 0), volr), 0));
        ((vec_S32*)dest)[0] = out;

        out = ((vec_S32*)dest)[1];
        out = __vaddsws(out, vec_cts(__vmulfp(vec_ctfS32(vec_unpackhS16(inl), 0), voll), 0));
        out = __vaddsws(out, vec_cts(__vmulfp(vec_ctfS32(vec_unpacklS16(inl), 0), volr), 0));
        ((vec_S32*)dest)[1] = out;

        src += 16;
        dest += 8;
        count -= 8;
    }

    *srcp = (void*) src;
    *destp = (void*) dest;
}


typedef void simple_MC_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right );

static simple_MC_mix * MC_mixers[ 4 ] = { srcmono_destmono_mix_vmx128, srcmono_deststereo_mix_vmx128, srcstereo_destmono_mix_vmx128, srcstereo_deststereo_mix_vmx128 };

#define CALL_MIXER_MERGE(Op, Sample, Src, SrcEnd, Dest, DestEnd, Left, Right) \
    MC_mixers[(Op&1)|((Op>>2)&2)](Src, SrcEnd, Dest, DestEnd, Left, Right);

#else

#if defined(__RADWIN__) || defined( IS_WIN32 )//|| defined(IS_X86)

#ifndef IS_XBOX

#include <emmintrin.h>

static void srcmono_deststereo_mix_sse2( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
    void * MSSRESTRICT src = *srcp;
    void * MSSRESTRICT dest = *destp;
    SINTa dest_count;
    SINTa count;

    count = AIL_ptr_dif( src_end, src ) >> 1; // 16-bit mono

    dest_count = AIL_ptr_dif( dest_end, dest ) >> 3; // 32-bit stereo
    if ( count > dest_count )
        count = dest_count;

    __m128i VolumeVector = _mm_set_epi16((S16)right, (S16)left, (S16)right, (S16)left, (S16)right, (S16)left, (S16)right, (S16)left);

    count >>= 3;
    while ( count )
    {
        // Read 8 source samples
        __m128i SourceVectorL = _mm_loadu_si128((__m128i*)src);
        __m128i SourceVectorH = _mm_unpackhi_epi16(SourceVectorL, SourceVectorL);
        SourceVectorL = _mm_unpacklo_epi16(SourceVectorL, SourceVectorL);

        // Multiply...
        __m128i MulLo = _mm_mullo_epi16(SourceVectorL, VolumeVector);
        __m128i MulHi = _mm_mulhi_epi16(SourceVectorL, VolumeVector);

        SourceVectorL = _mm_unpacklo_epi16(MulLo, MulHi);
        MulHi = _mm_unpackhi_epi16(MulLo, MulHi);

        // Add the result to the dest.

        // Samples 0, 1, 2, 3
        __m128i DestVector = _mm_load_si128((__m128i*)dest);
        DestVector = _mm_add_epi32(DestVector, SourceVectorL);
        _mm_store_si128((__m128i*)dest, DestVector);
        dest = AIL_ptr_add(dest, 16);

        DestVector = _mm_load_si128((__m128i*)dest);
        DestVector = _mm_add_epi32(DestVector, MulHi);
        _mm_store_si128((__m128i*)dest, DestVector);
        dest = AIL_ptr_add(dest, 16);

        // Samples 4, 5, 6, 7
        MulLo = _mm_mullo_epi16(SourceVectorH, VolumeVector);
        MulHi = _mm_mulhi_epi16(SourceVectorH, VolumeVector);

        SourceVectorL = _mm_unpacklo_epi16(MulLo, MulHi);
        MulHi = _mm_unpackhi_epi16(MulLo, MulHi);

        // Add the result to the dest.
        DestVector = _mm_load_si128((__m128i*)dest);
        DestVector = _mm_add_epi32(DestVector, SourceVectorL);
        _mm_store_si128((__m128i*)dest, DestVector);
        dest = AIL_ptr_add(dest, 16);

        DestVector = _mm_load_si128((__m128i*)dest);
        DestVector = _mm_add_epi32(DestVector, MulHi);
        _mm_store_si128((__m128i*)dest, DestVector);

        dest = AIL_ptr_add(dest, 16);
        src = AIL_ptr_add( src, 16 );
        --count;
    }

    *srcp = src;
    *destp = dest;
}

static void srcstereo_deststereo_mix_sse2( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
    void * MSSRESTRICT src = *srcp;
    void * MSSRESTRICT dest = *destp;
    SINTa dest_count;
    SINTa count;

    count = AIL_ptr_dif( src_end, src ) >> 2; // 16-bit stereo

    dest_count = AIL_ptr_dif( dest_end, dest ) >> 3; // 32-bit stereo
    if ( count > dest_count )
        count = dest_count;

    __m128i VolumeVector = _mm_set_epi16((S16)right, (S16)left, (S16)right, (S16)left, (S16)right, (S16)left, (S16)right, (S16)left);

    count >>= 2;
    while ( count )
    {
        // Read 4 source samples
        __m128i SourceVector = _mm_loadu_si128((__m128i*)src);

        __m128i MulLo = _mm_mullo_epi16(SourceVector, VolumeVector);
        __m128i MulHi = _mm_mulhi_epi16(SourceVector, VolumeVector);

        __m128i ResLo = _mm_unpacklo_epi16(MulLo, MulHi);
        __m128i ResHi = _mm_unpackhi_epi16(MulLo, MulHi);

        __m128i DestVector = _mm_load_si128((__m128i*)dest);
        DestVector = _mm_add_epi32(DestVector, ResLo);
        _mm_store_si128((__m128i*)dest, DestVector);
        dest = AIL_ptr_add(dest, 16);

        DestVector = _mm_load_si128((__m128i*)dest);
        DestVector = _mm_add_epi32(DestVector, ResHi);
        _mm_store_si128((__m128i*)dest, DestVector);

        dest = AIL_ptr_add(dest, 16);
        src = AIL_ptr_add( src, 16 );
        --count;
    }

    *srcp = src;
    *destp = dest;
}


static void srcmono_destmono_mix_sse2( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
    void * MSSRESTRICT src = *srcp;
    void * MSSRESTRICT dest = *destp;
    SINTa dest_count;
    SINTa count;

    count = AIL_ptr_dif( src_end, src ) >> 1; // 16-bit mono

    dest_count = AIL_ptr_dif( dest_end, dest ) >> 2; // 32-bit mono
    if ( count > dest_count )
        count = dest_count;

    __m128i VolumeVector = _mm_set1_epi16((S16)left);

    count >>= 3;
    while ( count )
    {
        __m128i SourceVector = _mm_loadu_si128((__m128i*)src);

        __m128i MulLo = _mm_mullo_epi16(SourceVector, VolumeVector);
        __m128i MulHi = _mm_mulhi_epi16(SourceVector, VolumeVector);

        __m128i ResLo = _mm_unpacklo_epi16(MulLo, MulHi);
        __m128i ResHi = _mm_unpackhi_epi16(MulLo, MulHi);

        __m128i DestVector = _mm_load_si128((__m128i*)dest);
        DestVector = _mm_add_epi32(DestVector, ResLo);
        _mm_store_si128((__m128i*)dest, DestVector);
        dest = AIL_ptr_add(dest, 16);

        DestVector = _mm_load_si128((__m128i*)dest);
        DestVector = _mm_add_epi32(DestVector, ResHi);
        _mm_store_si128((__m128i*)dest, DestVector);

        dest = AIL_ptr_add(dest, 16);
        src = AIL_ptr_add( src, 16 );
        --count;
    }

    *srcp = src;
    *destp = dest;
}

static void srcstereo_destmono_mix_sse2( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
    void * MSSRESTRICT src = *srcp;
    void * MSSRESTRICT dest = *destp;
    SINTa dest_count;
    SINTa count;

    count = AIL_ptr_dif( src_end, src ) >> 2; // 16-bit stereo

    dest_count = AIL_ptr_dif( dest_end, dest ) >> 2; // 32-bit mono
    if ( count > dest_count )
        count = dest_count;

    __m128i VolumeVector = _mm_set_epi16((S16)right, (S16)left, (S16)right, (S16)left, (S16)right, (S16)left, (S16)right, (S16)left);

    count >>= 2;
    while ( count )
    {
        // Load 4 source samples.
        __m128i SourceVector = _mm_loadu_si128((__m128i*)src);

        // Multiply by volume and add the stereo channels together into mono.
        SourceVector = _mm_madd_epi16(SourceVector, VolumeVector);

        // Save 4 dest samples.
        __m128i DestVector = _mm_load_si128((__m128i*)dest);
        DestVector = _mm_add_epi32(DestVector, SourceVector);
        _mm_store_si128((__m128i*)dest, DestVector);

        src = AIL_ptr_add( src, 16 );
        dest = AIL_ptr_add( dest, 16 );
        --count;
    }

    *srcp = src;
    *destp = dest;
}

#endif

#ifndef IS_WIN64

static void srcmono_deststereo_mix_mmx( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
    void * MSSRESTRICT src = *srcp;
    void * MSSRESTRICT dest = *destp;
    SINTa dest_count;
    SINTa count;

    count = AIL_ptr_dif( src_end, src ) >> 1; // 16-bit mono

    dest_count = AIL_ptr_dif( dest_end, dest ) >> 3; // 32-bit stereo
    if ( count > dest_count )
        count = dest_count;

    __m64 VolumeVector = _mm_set_pi16((S16)right, (S16)left, (S16)right, (S16)left);

    count >>= 2;
    while ( count )
    {
        // Read 4 source samples
        __m64 SourceVectorL = *(__m64*)src;
        __m64 SourceVectorH = _mm_unpackhi_pi16(SourceVectorL, SourceVectorL);
        SourceVectorL = _mm_unpacklo_pi16(SourceVectorL, SourceVectorL);

        // Multiply...
        __m64 MulLo = _mm_mullo_pi16(SourceVectorL, VolumeVector);
        __m64 MulHi = _mm_mulhi_pi16(SourceVectorL, VolumeVector);

        SourceVectorL = _mm_unpacklo_pi16(MulLo, MulHi);
        MulHi = _mm_unpackhi_pi16(MulLo, MulHi);

        // Add the result to the dest.

        // Samples 0, 1
        __m64 DestVector = *(__m64*)dest;
        DestVector = _mm_add_pi32(DestVector, SourceVectorL);
        *(__m64*)dest = DestVector;
        dest = AIL_ptr_add(dest, 8);

        DestVector = *(__m64*)dest;
        DestVector = _mm_add_pi32(DestVector, MulHi);
        *(__m64*)dest = DestVector;
        dest = AIL_ptr_add(dest, 8);

        // Samples 2, 3
        MulLo = _mm_mullo_pi16(SourceVectorH, VolumeVector);
        MulHi = _mm_mulhi_pi16(SourceVectorH, VolumeVector);

        SourceVectorL = _mm_unpacklo_pi16(MulLo, MulHi);
        MulHi = _mm_unpackhi_pi16(MulLo, MulHi);

        // Add the result to the dest.
        DestVector = *(__m64*)dest;
        DestVector = _mm_add_pi32(DestVector, SourceVectorL);
        *(__m64*)dest = DestVector;
        dest = AIL_ptr_add(dest, 8);

        DestVector = *(__m64*)dest;
        DestVector = _mm_add_pi32(DestVector, MulHi);
        *(__m64*)dest = DestVector;
        dest = AIL_ptr_add(dest, 8);


        src = AIL_ptr_add( src, 8 );
        --count;
    }

    _mm_empty();

    *srcp = src;
    *destp = dest;
}

static void srcstereo_deststereo_mix_mmx( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
    void * MSSRESTRICT src = *srcp;
    void * MSSRESTRICT dest = *destp;
    SINTa dest_count;
    SINTa count;

    count = AIL_ptr_dif( src_end, src ) >> 2; // 16-bit stereo

    dest_count = AIL_ptr_dif( dest_end, dest ) >> 3; // 32-bit stereo
    if ( count > dest_count )
    count = dest_count;

    __m64 VolumeVector = _mm_set_pi16((S16)right, (S16)left, (S16)right, (S16)left);

    count >>= 1;
    while ( count )
    {
        // Read 2 source samples
        __m64 SourceVector = *(__m64*)src;

        __m64 MulLo = _mm_mullo_pi16(SourceVector, VolumeVector);
        __m64 MulHi = _mm_mulhi_pi16(SourceVector, VolumeVector);

        __m64 ResLo = _mm_unpacklo_pi16(MulLo, MulHi);
        __m64 ResHi = _mm_unpackhi_pi16(MulLo, MulHi);

        __m64 DestVector = *(__m64*)dest;
        DestVector = _mm_add_pi32(DestVector, ResLo);
        *(__m64*)dest = DestVector;
        dest = AIL_ptr_add(dest, 8);

        DestVector = *(__m64*)dest;
        DestVector = _mm_add_pi32(DestVector, ResHi);
        *(__m64*)dest = DestVector;

        dest = AIL_ptr_add(dest, 8);
        src = AIL_ptr_add( src, 8 );
        --count;
    }

    _mm_empty();

    *srcp = src;
    *destp = dest;
}


static void srcmono_destmono_mix_mmx( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
    void * MSSRESTRICT src = *srcp;
    void * MSSRESTRICT dest = *destp;
    SINTa dest_count;
    SINTa count;

    count = AIL_ptr_dif( src_end, src ) >> 1; // 16-bit mono

    dest_count = AIL_ptr_dif( dest_end, dest ) >> 2; // 32-bit mono
    if ( count > dest_count )
        count = dest_count;

    __m64 VolumeVector = _mm_set1_pi16((S16)left);

    count >>= 2;
    while ( count )
    {
        // Load 4 samples.
        __m64 SourceVector = *(__m64*)src;

        __m64 MulLo = _mm_mullo_pi16(SourceVector, VolumeVector);
        __m64 MulHi = _mm_mulhi_pi16(SourceVector, VolumeVector);

        __m64 ResLo = _mm_unpacklo_pi16(MulLo, MulHi);
        __m64 ResHi = _mm_unpackhi_pi16(MulLo, MulHi);

        __m64 DestVector = *(__m64*)dest;
        DestVector = _mm_add_pi32(DestVector, ResLo);
        *(__m64*)dest = DestVector;
        dest = AIL_ptr_add(dest, 8);

        DestVector = *(__m64*)dest;
        DestVector = _mm_add_pi32(DestVector, ResHi);
        *(__m64*)dest = DestVector;

        dest = AIL_ptr_add(dest, 8);
        src = AIL_ptr_add( src, 8 );
        --count;
    }

    _mm_empty();

    *srcp = src;
    *destp = dest;
}

static void srcstereo_destmono_mix_mmx( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right )
{
    void * MSSRESTRICT src = *srcp;
    void * MSSRESTRICT dest = *destp;
    SINTa dest_count;
    SINTa count;

    count = AIL_ptr_dif( src_end, src ) >> 2; // 16-bit stereo

    dest_count = AIL_ptr_dif( dest_end, dest ) >> 2; // 32-bit mono
    if ( count > dest_count )
    {
        count = dest_count;
    }

    __m64 VolumeVector = _mm_set_pi16((S16)right, (S16)left, (S16)right, (S16)left);

    count >>= 1;
    while ( count )
    {
        // Load 2 source samples.
        __m64 SourceVector = *(__m64*)src;

        // Multiply by volume and add the stereo channels together into mono.
        SourceVector = _mm_madd_pi16(SourceVector, VolumeVector);

        // Save 2 dest samples.
        __m64 DestVector = *(__m64*)dest;
        DestVector = _mm_add_pi32(DestVector, SourceVector);
        *(__m64*)dest = DestVector;

        src = AIL_ptr_add( src, 8 );
        dest = AIL_ptr_add( dest, 8 );
        --count;
    }

    _mm_empty();

    *srcp = src;
    *destp = dest;
}

#endif

typedef void simple_MC_mix( void ** srcp, void * src_end, void ** destp, void * dest_end, S32 left, S32 right );

static
simple_MC_mix *
MC_x86_mixers[] = {
#ifndef IS_WIN64
    srcmono_destmono_mix_mmx, srcmono_deststereo_mix_mmx, srcstereo_destmono_mix_mmx, srcstereo_deststereo_mix_mmx,
#endif
#ifndef IS_XBOX
    srcmono_destmono_mix_sse2, srcmono_deststereo_mix_sse2, srcstereo_destmono_mix_sse2, srcstereo_deststereo_mix_sse2
#endif
};

//-----------------------------------------------------------------------------
/*!
    Dispatches the correct optimized x86 asm mixer based on buffer properties and
    proc feature existence.

    \param[in] i_MMXFlags - bit flags specified in x86\mssa32.asm
    \param[in] i_Op - or'd op flags describing src/dest stereo-ness.
*/
//-----------------------------------------------------------------------------
static
void
dispatch_x86_mixer(
    HSAMPLE i_Sample,
    S32  Op,   // Specifies whether source/dest buffers are stereo.
    void** i_Src,
    void*  i_SrcEnd,
    void** i_Dest,
    void*  i_DestEnd,
    S32 i_Left,
    S32 i_Right
    )
{
#ifdef IS_WIN64
  MC_x86_mixers[(Op&1)|((Op>>2)&2)](i_Src, i_SrcEnd, i_Dest, i_DestEnd, i_Left, i_Right);
#elif defined( IS_XBOX )
  MC_x86_mixers[(Op&1)|((Op>>2)&2)](i_Src, i_SrcEnd, i_Dest, i_DestEnd, i_Left, i_Right);
#else
    //
    // From mssa32.asm for x86, cpuflags contains:
    //
    // 0x0001: MMX present
    // 0x0002: AMD 3DNow extensions present
    // 0x0004: SSE extensions present
    // 0x0008: SS2 extensions present
    //

    S32 cpuflags = i_Sample->driver->use_MMX;

    if (cpuflags)
    {
        S32 cpuflagsmask = 0;
        // Check for SSE2
        if (cpuflags & 0x8)
        {
            cpuflagsmask = 4;
        }

        // Dispatch.
        MC_x86_mixers[cpuflagsmask|(Op&1)|((Op>>2)&2)](i_Src, i_SrcEnd, i_Dest, i_DestEnd, i_Left, i_Right);
    }
    else
    {
        // Old proc, use basic mixer merger.
        S32 lrtemp = 0;
        U32 temp = 0;

        i_Sample->pipeline[SP_MERGE].TYPE.MSS_mixer_merge(
            (void const **)i_Src,
            &temp,
            i_SrcEnd,
            (S32 * *)(void*)i_Dest,
            i_DestEnd,
            &lrtemp,
            &lrtemp,
            65536,
            i_Left,
            i_Right,
            Op,
            cpuflags);
    }
#endif
}

#define CALL_MIXER_MERGE(Op, Sample, Src, SrcEnd, Dest, DestEnd, Left, Right) \
    dispatch_x86_mixer(Sample, Op, Src, SrcEnd, Dest, DestEnd, Left, Right);

#elif defined(IS_X86) // for the extra x86 param in the merger.
    #define CALL_MIXER_MERGE(Op, Sample, Src, SrcEnd, Dest, DestEnd, Left, Right) \
    {\
        S32 lrtemp = 0;\
        U32 temp = 0;\
        Sample->pipeline[SP_MERGE].TYPE.MSS_mixer_merge(\
            (void const **) Src,\
            &temp,\
            SrcEnd,\
            (S32 * *)(void*) Dest,\
            DestEnd,\
            &lrtemp,\
            &lrtemp,\
            65536,\
            Left,\
            Right,\
            Op,\
            Sample->driver->use_MMX\
            );\
    }
#else       // non-x86 platforms without a fastpath.
    #define CALL_MIXER_MERGE(Op, Sample, Src, SrcEnd, Dest, DestEnd, Left, Right) \
    {\
        S32 lrtemp = 0;\
        U32 temp = 0;\
        Sample->pipeline[SP_MERGE].TYPE.MSS_mixer_merge(\
            (void const **) Src,\
            &temp,\
            SrcEnd,\
            (S32 * *)(void*) Dest,\
            DestEnd,\
            &lrtemp,\
            &lrtemp,\
            65536,\
            Left,\
            Right,\
            Op);\
    }

#endif

#endif

#ifndef CALL_MIXER_MERGE
Compiler Freakout - call mixer merge must be defined!
#endif

static S32 slow_ramp_mix( S32 src_stereo, S32 dest_stereo, void **srcp, void *src_end, void **destp, void * dest_end, S32* lvp, S32* rvp, S32 ladj, S32 radj, S32 r )
{
  void * MSSRESTRICT dest;
  void * MSSRESTRICT src = *srcp;
  S32 op = M_SRC_16;
  S32 lv = *lvp;
  S32 rv = *rvp;
  void * mix_end;

  if ( ( lv < 2048 ) || ( rv < 2048 ) )
    op |= M_VOL_SCALING;

  lv <<= 16;
  rv <<= 16;
  ladj <<= 16;
  radj <<= 16;

  if ( src_stereo )
  {
    op |= M_SRC_STEREO;

    if( dest_stereo )
    {
      op |= M_DEST_STEREO;
      dest = *destp;

      #ifdef _DEBUG
      if ( ( (UINTa) dest ) & 7 ) MSSBreakPoint(); // should never be off sample
      #endif

      mix_end = AIL_ptr_add( dest, r*8 );
      if ( ( (UINTa) mix_end ) & 15 )
      {
        mix_end = AIL_ptr_add( dest, 8 );
        ++r;

        // Cap r if we don't have enough dest or src
        if ( mix_end > dest_end )
          r = (U32) ( AIL_ptr_dif( dest_end, dest ) / 8 );
        if (r > AIL_ptr_dif(src_end, src) / 4)
          r = (U32)AIL_ptr_dif(src_end, src) / 4;
      }

      if ( ( ladj ) && ( r > 1 ) ) ladj /= r;
      if ( ( radj ) && ( r > 1 ) ) radj /= r;

      while ( r )
      {
        ((S32*)dest)[0] += ( ( (S32) ( (S16) LE_SWAP16( src ) ) ) * ( lv >> 16 ) );
        ((S32*)dest)[1] += ( ( (S32) ( (S16) LE_SWAP16_OFS( src, 2 ) ) ) * ( rv >> 16 ) );
        lv += ladj;
        rv += radj;
        dest = AIL_ptr_add( dest, 8 );
        src = AIL_ptr_add( src, 4 );
        --r;
      }
    }
    else
    {
        // Dest is mono  - 32 bits per sample

      dest = *destp;

      #ifdef _DEBUG
      if ( ( (UINTa) dest ) & 3 ) MSSBreakPoint(); // should never be off sample
      #endif

      mix_end = AIL_ptr_add( dest, r*4 );
      if ( ( (UINTa) mix_end ) & 15 )
      {
        r += (U32) ( ( 16 - ( ( (UINTa) mix_end ) & 15 ) ) / 4 );
        mix_end = AIL_ptr_add( dest, r*4 );

        // Cap r if we don't have enough dest or src
        if ( mix_end > dest_end )
          r = (U32) ( AIL_ptr_dif( dest_end, dest ) / 4 );
        if (r > AIL_ptr_dif(src_end, src) / 4)
          r = (U32)AIL_ptr_dif(src_end, src) / 4;
      }

      if ( ( ladj ) && ( r > 1 ) ) ladj /= r;
      if ( ( radj ) && ( r > 1 ) ) radj /= r;

      while ( r )
      {
        ((S32*)dest)[0] += ( ( (S32) ( (S16) LE_SWAP16( src ) ) ) * ( lv >> 16 ) ) +
                             ( ( (S32) ( (S16) LE_SWAP16_OFS( src, 2 ) ) ) * ( rv >> 16 ) );
        lv += ladj;
        rv += radj;

        dest = AIL_ptr_add( dest, 4 );
        src = AIL_ptr_add( src, 4 );
        --r;
      }
    }
  }
  else
  {
    if( dest_stereo )
    {
      op |= M_DEST_STEREO;
      dest = *destp;

      #ifdef _DEBUG
      if ( ( (UINTa) dest ) & 7 ) MSSBreakPoint(); // should never be off sample
      #endif

      mix_end = AIL_ptr_add( dest, r*8 );
      if ( ( (UINTa) mix_end ) & 8 )
      {
        mix_end = AIL_ptr_add( dest, 8 );
        ++r;

        // Cap r if we don't have enough dest or src
        if ( mix_end > dest_end )
          r = (U32) ( AIL_ptr_dif( dest_end, dest ) / 8 );
        if (r > AIL_ptr_dif(src_end, src) / 2)
          r = (U32)AIL_ptr_dif(src_end, src) / 2;
      }

      if ( ( ladj ) && ( r > 1 ) ) ladj /= r;
      if ( ( radj ) && ( r > 1 ) ) radj /= r;

      while ( r )
      {
        S32 s = ( (S32) ( (S16) LE_SWAP16( src ) ) );
        ((S32*)dest)[0] += ( s * ( lv >> 16 ) );
        ((S32*)dest)[1] += ( s * ( rv >> 16 ) );
        lv += ladj;
        rv += radj;

        dest = AIL_ptr_add( dest, 8 );
        src = AIL_ptr_add( src, 2 );
        --r;
      }
    }
    else
    {
      dest = *destp;

      #ifdef _DEBUG
      if ( ( (UINTa) dest ) & 3 ) MSSBreakPoint(); // should never be off sample
      #endif

      mix_end = AIL_ptr_add( dest, r*4 );
      if ( ( (UINTa) mix_end ) & 15 )
      {
        r += (U32) ( ( 16 - ( ( (UINTa) mix_end ) & 15 ) ) / 4 );
        mix_end = AIL_ptr_add( dest, r*4 );

        // Cap r if we don't have enough dest or src
        if ( mix_end > dest_end )
          r = (U32) ( AIL_ptr_dif( dest_end, dest ) / 4 );
        if (r > AIL_ptr_dif(src_end, src) / 2)
          r = (U32)AIL_ptr_dif(src_end, src) / 2;
      }

      if ( ( ladj ) && ( r > 1 ) ) ladj /= r;

      while ( r )
      {
        ((S32*)dest)[0] += ( ( (S32) ( (S16) LE_SWAP16( src ) ) ) * ( lv >> 16 ) );
        lv += ladj;

        dest = AIL_ptr_add( dest, 4 );
        src = AIL_ptr_add( src, 2 );
        --r;
      }
    }
  }

  *srcp = src;
  *destp = dest;
  *lvp = lv >> 16;
  *rvp = rv >> 16;
  return op;
}


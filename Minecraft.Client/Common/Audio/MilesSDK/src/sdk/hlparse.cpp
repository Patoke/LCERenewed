#include "mss.h"

// This file also built in to Miles Studio.

S32 atoi_with_len(char const * source, S32 hex, S32* out);
S32 atoi_with_len(char const * source, S32 hex, S32* out)
{
  char const * os = source;
  char const * s = source;
  
  // eat leading minus or plus sign
  S32 neg = ( s[ 0 ] == '-')  ? 1 : 0;
  if (( s[ 0 ] == '-' ) || ( s[ 0 ] == '+' ) )
    ++s;
  
  S32 d = 0;
  
  if ( hex )
  {
    // snag the hex number
    for(;;)
    {
      if ( ( s[ 0 ] >= '0' ) && ( s[ 0 ] <= '9' ) )
        d = ( d * 16 ) + ( s[ 0 ] - '0' );
      else if ( ( s[ 0 ] >= 'A' ) && ( s[ 0 ] <= 'F' ) )
        d = ( d * 16 ) + ( s[ 0 ] - 'A' + 10 );
      else if ( ( s[ 0 ] >= 'a' ) && ( s[ 0 ] <= 'f' ) )
        d = ( d * 16 ) + ( s[ 0 ] - 'a' + 10 );
      else
        break;
      ++s;
    }
  }
  else
  {
    // snag the number 
    while ( ( s[ 0 ] >= '0' ) && ( s[ 0 ] <= '9' ) )
    {
      d = ( d * 10 ) + ( s[ 0 ] - '0' );
      ++s;
    }
  }
  
  if ( neg ) 
    d = -d;
    
  if ( out )
    *out = (S32) ( s - os );
    
  return d;
}  

#if defined( IS_PS2 ) || defined( IS_PSP )
  #define FTYPE float    // one bit of inprecision on 8% 32-bit float space

  static const float mult[ 39 + 39 ] =
  {
    1e-39f,1e-38f,1e-37f,1e-36f,1e-35f,1e-34f,1e-33f,1e-32f,1e-31f,1e-30f,1e-29f,1e-28f,1e-27f,1e-26f,1e-25f,1e-24f,1e-23f,1e-22f,1e-21f,1e-20f,1e-19f,1e-18f,1e-17f,1e-16f,1e-15f,1e-14f,1e-13f,1e-12f,1e-11f,1e-10f,1e-9f,1e-8f,1e-7f,1e-6f,1e-5f,1e-4f,1e-3f,1e-2f,1e-1f,
    1.f,1e+1f,1e+2f,1e+3f,1e+4f,1e+5f,1e+6f,1e+7f,1e+8f,1e+9f,1e+10f,1e+11f,1e+12f,1e+13f,1e+14f,1e+15f,1e+16f,1e+17f,1e+18f,1e+19f,1e+20f,1e+21f,1e+22f,1e+23f,1e+24f,1e+25f,1e+26f,1e+27f,1e+28f,1e+29f,1e+30f,1e+31f,1e+32f,1e+33f,1e+34f,1e+35f,1e+36f,1e+37f,1e+38f
  };
#else
  #define FTYPE double   // double's free on everything else

  static const double mult[ 39 + 39 ] =
  {
    1e-39,1e-38,1e-37,1e-36,1e-35,1e-34,1e-33,1e-32,1e-31,1e-30,1e-29,1e-28,1e-27,1e-26,1e-25,1e-24,1e-23,1e-22,1e-21,1e-20,1e-19,1e-18,1e-17,1e-16,1e-15,1e-14,1e-13,1e-12,1e-11,1e-10,1e-9,1e-8,1e-7,1e-6,1e-5,1e-4,1e-3,1e-2,1e-1,
    1,1e+1,1e+2,1e+3,1e+4,1e+5,1e+6,1e+7,1e+8,1e+9,1e+10,1e+11,1e+12,1e+13,1e+14,1e+15,1e+16,1e+17,1e+18,1e+19,1e+20,1e+21,1e+22,1e+23,1e+24,1e+25,1e+26,1e+27,1e+28,1e+29,1e+30,1e+31,1e+32,1e+33,1e+34,1e+35,1e+36,1e+37,1e+38 
  };
#endif

static const FTYPE fl[10]={0,1,2,3,4,5,6,7,8,9};

F32 atof_with_len(const char* source, S32* out);
F32 atof_with_len(const char* source, S32* out)
{
    char const * s = source;
    char const * os = source;
    
    // eat leading minus or plus sign
    S32 neg = ( s[ 0 ] == '-')  ? 1 : 0;
    if (( s[ 0 ] == '-' ) || ( s[ 0 ] == '+' ) )
      ++s;

    // eat leading zeros
    while( s[ 0 ] == '0' )
      ++s;
      
    // find the start, ending non-zero character, and the end of the integer bit
    char const * isnz = s;
    char const * ienz = s;
    char const * ie = s;
    
    // find the end of the integer portion 
    if ( ( s[ 0 ] >= '0' ) && ( s[ 0 ] <= '9' ) )
    {
      do
      {
        if ( s[ 0 ] != '0' ) ienz = s;
        ++s;
      } while ( ( s[ 0 ] >= '0' ) && ( s[ 0 ] <= '9' ) );
      
      ie = s;
    }

    // find the start, starting non-zero character, ending non-zero character, and the end of the fractional bit

    char const * fs = s;
    char const * fsnz = s;
    char const * fenz = s;
    char const * fe = s;

    // find the end of the fractional portion 
    if ( s[ 0 ] == '.' )
    {
      ++s;
      fs = s;

      // skip the leading zeros
      while ( s[ 0 ] == '0' )
        ++s;
        
      fsnz = s;
      fenz = s;
     
      // find the begin and end positions of the fractional portion
      if ( ( s[ 0 ] >= '0' ) && ( s[ 0 ] <= '9' ) )
      {
        do
        {
          if ( s[ 0 ] != '0' ) fenz = s;
          ++s;
        } while ( ( s[ 0 ] >= '0' ) && ( s[ 0 ] <= '9' ) );
      }
      
      fe = s;
    }

    // find the exponent power
    S32 expp = 0;
    if ( ( s[ 0 ] == 'e' ) || ( s[ 0 ] == 'E' ) )
    {
      ++s;
      S32 eneg = ( s[ 0 ] == '-')  ? 1 : 0;
      if (( s[ 0 ] == '-' ) || ( s[ 0 ] == '+' ) )
        ++s;

      while( ( s[ 0 ] >= '0' ) && ( s[ 0 ] <= '9' ) )
      {
        expp = ( expp * 10 ) + ( s[ 0 ] - '0' );
        ++s;
      }
      
      if ( eneg )
        expp = -expp;
    }

    FTYPE d = 0;
     
    // do we have an integer portion?
    if ( isnz != ie )
    {
      // calc the int portion (start from the least sig side)        
      FTYPE const * mp = &mult[ 39 + ie - isnz - 1 + expp ];
      if ( mp >= ( mult + sizeof( mult ) ) )
        d = 3.40282e+038f;
      else
      {
        // grab the data a digit at a time and do the exact scaling (from least sig side)
        mp += isnz - ienz;
        do
        {
          if ( mp >= mult )
            d += mp[ 0 ] * fl[ ienz[ 0 ] - '0' ];
          --ienz;
          ++mp;
        } while( ienz >= isnz );
      }
    }
    
    // is there a fractional portion?
    if ( fsnz != fe )
    {
      FTYPE f = 0;

      FTYPE const * mp = &mult[ 39 - ( fsnz - fs )- 1 + expp ];
      if ( mp >= ( mult + sizeof( mult ) ) )
        d = 3.40282e+038f;
      else
      {
        // calc the frac portion (start from the least sig side)        
        mp += fsnz - fenz;
        do
        {
          if ( mp >= mult )
            f += mp[ 0 ] * fl[ fenz[0] - '0' ];
          --fenz;
          ++mp;
        } while ( fenz >= fsnz );
     
        // add the fractional bit to the main number
        d += f;
     }
    }

    // if a negative number, flip it
    if ( neg )
      d = -d;

    if ( out )
      *out = (S32) ( s - os );      

    return (F32) d;
}






//===============================================================================================================

#ifdef TEST

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static U32 all;

U32 b1diffs[16]={0};
U32 a1diffs[16]={0};
U32 mb1diffs[16]={0};
U32 ma1diffs[16]={0};

DWORD WINAPI test( LPVOID )
{
  char fbuf[ 128 ];
  char mfbuf[ 128 ];
  U32 t;
    
  for(;;)
  {
    __asm 
    { 
      mov eax,1
      lock xadd [all],eax
      mov [t],eax
    }
    if ( t >= ( 254 << 23 ) ) // stop before specials...
      break;

    float f, mf;
    float af, maf;
    char *fb, * mfb;

    U32 mt = t | 0x80000000;
    f = *(float*)&t;
    mf = *(float*)&mt;

    sprintf( fbuf, "%50.50f", f );
    fb=fbuf;
    while(*fb==' ') ++fb;

    sprintf( mfbuf, "%50.50f", mf );
    mfb=mfbuf;
    while(*mfb==' ') ++mfb;

    
    af = atof_with_len( fb, 0 );
    if ( abs( (*(int*)&f)-(*(int*)&af) ) > 8 )
      printf("\nBad float error %X %f %f\n",t,f,af);
    else
    {
      if ( f < 1.0 )
        b1diffs[ 8+( (*(int*)&f)-(*(int*)&af) ) ]++;
      else
        a1diffs[ 8+( (*(int*)&f)-(*(int*)&af) ) ]++;
    }
    
    maf = atof_with_len( mfb, 0 );
    if ( abs( (*(int*)&mf)-(*(int*)&maf) ) > 8 )
      printf("\nBad error float %X %f %f\n",mt,mf,maf);
    else
    {
      if ( f < 1.0 )
        mb1diffs[ 8+( (*(int*)&mf)-(*(int*)&maf) ) ]++;
      else
        ma1diffs[ 8+( (*(int*)&mf)-(*(int*)&maf) ) ]++;
    }
    
    if ( ( t & 0xfffff ) == 0 )
    {
      if ( f > 1000000000000000000.0f)
        printf("%x %50.5f\r",t,f);
      else
        printf("%x %s\r",t,fb);
    }
  }
}

DWORD WINAPI testi( LPVOID )
{
  char buf[ 128 ];
  char mbuf[ 128 ];
  U32 t;
    
  for(;;)
  {
    __asm 
    { 
      mov eax,1
      lock xadd [all],eax
      mov [t],eax
    }
    if ( t >= 0xffffffe0 ) // stop before specials...
      break;

    char *b, * mb;

    sprintf( buf, "%d", t );
    b=buf;
    while(*b==' ') ++b;

    U32 mt = -t;
    sprintf( mbuf, "%d", mt );
    mb=mbuf;
    while(*mb==' ') ++mb;

    
    if ( t != atoi_with_len( b, 0, 0 ) )
      printf("\nBad int %d %d\n",t,atoi_with_len( b, 0, 0 ));

    if ( mt != atoi_with_len( mb, 0, 0 ) )
      printf("\nBad int %d %d\n",mt,atoi_with_len( mb, 0, 0 ));
    
    if ( ( t & 0xfffff ) == 0 )
    {
      printf("%X\r",t);
    }
  }
}


DWORD WINAPI testh( LPVOID )
{
  char buf[ 128 ];
  char mbuf[ 128 ];
  U32 t;
    
  for(;;)
  {
    __asm 
    { 
      mov eax,1
      lock xadd [all],eax
      mov [t],eax
    }
    if ( t >= 0xffffffe0 ) // stop before specials...
      break;

    char *b, * mb;

    sprintf( buf, "%x", t );
    b=buf;
    while(*b==' ') ++b;

    U32 mt = -t;
    sprintf( mbuf, "%x", mt );
    mb=mbuf;
    while(*mb==' ') ++mb;

    
    if ( t != atoi_with_len( b, 1, 0 ) )
      printf("\nBad int %d %d\n",t,atoi_with_len( b, 1, 0 ));

    if ( mt != atoi_with_len( mb, 1, 0 ) )
      printf("\nBad int %d %d\n",mt,atoi_with_len( mb, 1, 0 ));
    
    if ( ( t & 0xfffff ) == 0 )
    {
      printf("%X\r",t);
    }
  }
}


int main(int , char** )
{
  DWORD id;

  all = ( 0 << 23 );
  for ( S32 i = 0 ; i < 15 ; i++ )
    CreateThread( 0,0,testh,0,0,&id);
  testh( 0 );
  Sleep(500);
  printf("\n\n");
  for( S32 i = 0 ; i < 15 ; i++ )
  {
    printf("%2d diffs: az: %10d  bz: %10d,   -az: %10d  -bz: %10d\n",i-8,a1diffs[i],b1diffs[i],ma1diffs[i],mb1diffs[i] );
  }
}
#endif

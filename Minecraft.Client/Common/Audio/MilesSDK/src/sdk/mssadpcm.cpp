//############################################################################
//##                                                                        ##
//##  MSSADPCM.C                                                            ##
//##                                                                        ##
//##  PCM WAV / IMA ADPCM WAV converters                                    ##
//##                                                                        ##
//##  Original Author : Serge Plagnol (Adeline Software Intl.)              ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  For technical support, contact RAD Game Tools at 425-893-4300.        ##
//##                                                                        ##
//############################################################################

#include <stdio.h>

#include "mss.h"
#include "imssapi.h"

//
//  Step index for IMA ADPCM
//
const S16 next_step[16] =
{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

//
//  Step size for IMA ADPCM
//
const S16 step[89] =
{
        7,     8,     9,    10,    11,    12,    13,
       14,    16,    17,    19,    21,    23,    25,
       28,    31,    34,    37,    41,    45,    50,
       55,    60,    66,    73,    80,    88,    97,
      107,   118,   130,   143,   157,   173,   190,
      209,   230,   253,   279,   307,   337,   371,
      408,   449,   494,   544,   598,   658,   724,
      796,   876,   963,  1060,  1166,  1282,  1411,
     1552,  1707,  1878,  2066,  2272,  2499,  2749,
     3024,  3327,  3660,  4026,  4428,  4871,  5358,
     5894,  6484,  7132,  7845,  8630,  9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767
};


//############################################################################
//##                                                                        ##
//## Convert to 16 bits sign data                                           ##
//##                                                                        ##
//############################################################################

static S16  * conv8to16(S16 *dest, S8 *src, S32 size_src)
{
  S16  * d=dest;
  for(;size_src;size_src--)
    {
     *d = (S16)((*src^0x80)<<8) ;
     d=(S16  *) AIL_ptr_add(d,2);
     src=(S8  *) AIL_ptr_add(src,1);

    }

  return(dest);
}

//############################################################################
//##                                                                        ##
//## Macro to encode one sample and update related data                     ##
//##                                                                        ##
//############################################################################

#define encode(encoded,pred,sample,stepi)               \
{                                                       \
    S32    stepsize;                                    \
    S32    diff;                                        \
                                                        \
    stepsize    = step[stepi];                          \
                                                        \
    diff = sample - pred;                               \
    encoded = 0;                                        \
    if( diff<0 ) {                                      \
        encoded = 8;                                    \
        diff = -diff;                                   \
    }                                                   \
                                                        \
    if( diff >= stepsize ) {                            \
        encoded |= 4;                                   \
        diff -= stepsize;                               \
    }                                                   \
                                                        \
    stepsize >>= 1;                                     \
    if( diff >= stepsize ) {                            \
        encoded |= 2;                                   \
        diff -= stepsize;                               \
    }                                                   \
                                                        \
    stepsize >>= 1;                                     \
    if( diff >= stepsize ) {                            \
        encoded |= 1;                                   \
        diff -= stepsize;                               \
    }                                                   \
                                                        \
    if( encoded & 8 )                                   \
        pred = sample + diff - (stepsize>>1);           \
    else                                                \
        pred = sample - diff + (stepsize>>1);           \
                                                        \
    if( pred > 32767 )                                  \
        pred = 32767;                                   \
    else if( pred < -32768 )                            \
        pred = -32768;                                  \
                                                        \
    stepi += next_step[encoded];                        \
                                                        \
    if (stepi < 0)                                      \
        stepi = 0;                                      \
    else if (stepi > 88)                                \
        stepi = 88;                                     \
}


//############################################################################
//##                                                                        ##
//## Convert a block to 4 bits IMA ADPCM                                    ##
//##                                                                        ##
//############################################################################

static S32 convert_to_adpcm(S32 *dst, S16 *src, S32 size_src, S32 channels, S32 *plstepi, S32 *prstepi)
{
   S32 lstepi=0, rstepi=0;
   S32 samples ;
   S32 lsample, rsample ;
   S32 lpred=0, rpred=0;
   S32 res ;
   S32 encoded ;
   S32 i, j ;

   samples = size_src / (2*channels) ;

   lstepi = *plstepi ;
   lpred= (S16)LE_SWAP16( src );
   res = (lpred&0xFFFF) | (lstepi << 16) ;
   dst[0] = LE_SWAP32( &res );

   if(channels>1)
   {
      rstepi = *prstepi ;
      rpred= (S16)LE_SWAP16( src + 1 );
      res = (rpred&0xFFFF) | (rstepi << 16) ;
      dst[1] = LE_SWAP32( &res );
   }

   src += channels ;
   dst += channels ;

   samples-- ;

   samples = (samples+7)&~7 ;

   for(j=samples; j>0; j-=8)
   {
      res = 0 ;

      for(i=0; i<8*4; i+=4)
      {
         lsample = (S16)LE_SWAP16( src );
         src += channels ;

         encode(encoded,lpred,lsample,lstepi);

         res |= encoded << i;
      }

      *dst++ = LE_SWAP32( &res );

      if(channels>1)
      {
         src -= 8*2-1 ;

         res = 0 ;

         for(i=0; i<8*4; i+=4)
         {
            rsample = (S16)LE_SWAP16( src );
            src += channels ;

            encode(encoded,rpred,rsample,rstepi);

            res |= encoded << i;
         }

         src-- ;

         *dst++ = LE_SWAP32( &res );

      }
   }

   *plstepi = lstepi ;

   if(channels>1)
   {
      *prstepi = rstepi ;
   }

   return ((samples/2)+4)*channels ;
}

//############################################################################
//##                                                                        ##
//## Compress to ADPCM                                                      ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_compress_ADPCM(AILSOUNDINFO const * info, //)
                           void * * outdata, U32 * outsize)
{
  ADPCMOUT * adp;

  S32  alignment;
  S32  samples_per_block;
  S32  lstepi, rstepi ;
  S8    *src,  *dst;
  void * buf;
  U32  buffer_size;
  S16  * tempbuf;
  U32  length;
  U32  approxsize;

  if ((info==0) && (info->data_ptr))
    return(0);

  if (info->format!=WAVE_FORMAT_PCM) {
    AIL_set_error("Data is already compressed.");
    return(0);
  }

  if (((info->bits!=8) && (info->bits!=16)) || ((info->channels!=1) && (info->channels!=2))) {
    AIL_set_error("Unsupported PCM data format.");
    return(0);
  }

  //
  //  The data must be an integral number of S16s for mono, an even
  //  number of S16s for stereo.
  //

  alignment = 256 << (info->channels/2) ;

  //
  //  take sampling rate into consideration
  //  (allowing for small deviations due to MAKELOOP resampling)
  //

   if (info->rate > 11025)
    {
     alignment *= (info->rate + 5000) / 11025 ;
    }

   //
   // Compute samples per block
   //

   samples_per_block = 4 << (info->channels/2);
   samples_per_block = 1 + (alignment-samples_per_block)*8 / samples_per_block;

   //
   // Alocate a buffer for conversion
   //

   length=info->data_len;

   buffer_size = samples_per_block*2*info->channels;

   approxsize=(((length+samples_per_block)/samples_per_block)*alignment)+1024;

   buf = AIL_mem_alloc_lock(buffer_size+approxsize+sizeof(ADPCMOUT)+256);

   if (buf==0)
   {
      return(0);
   }

   adp=(ADPCMOUT *)buf;

   dst=(S8 *)(adp+1);

   tempbuf=(S16 *)AIL_ptr_add(dst,approxsize+sizeof(ADPCMOUT));

   //
   // create new format chunk
   //

   AIL_memcpy(&adp->riffmark,"RIFF",4);
   adp->rifflen=sizeof(ADPCMOUT)-8;
   AIL_memcpy(&adp->wavemark,"WAVE",4);
   AIL_memcpy(&adp->fmtmark,"fmt ",4);
   adp->fmtlen=20;
   adp->fmttag=WAVE_FORMAT_IMA_ADPCM;
   adp->channels=(U16)info->channels;
   adp->sampersec=info->rate;
   adp->avepersec=(info->rate * alignment) / samples_per_block;
   adp->blockalign=(U16)alignment;
   adp->bitspersam=4;
   adp->extra=2;
   adp->samples_per_block=(S16)samples_per_block;
   AIL_memcpy(&adp->factmark,"fact",4);
   adp->factlen=4;
   adp->samples = (length / (info->bits/8)) / info->channels ;
   AIL_memcpy(&adp->datamark,"data",4);


   src = (S8  *) info->data_ptr;
   lstepi = rstepi = 0 ;

   while (length)
   {
     U32 thisbuf;

     thisbuf = buffer_size/(16/info->bits);

     if (thisbuf>length) {
       AIL_memset(tempbuf,0,buffer_size);
       thisbuf=length;

       if (info->bits==16)
         AIL_memcpy(tempbuf,src,thisbuf);
       else
         conv8to16(tempbuf,src,thisbuf);

       dst = (S8  *) AIL_ptr_add(dst,convert_to_adpcm((S32 *)dst, tempbuf, thisbuf*(16/info->bits), info->channels, &lstepi, &rstepi));

     } else {

       dst = (S8  *)AIL_ptr_add(dst,convert_to_adpcm((S32 *)dst, (info->bits==8)?conv8to16(tempbuf,src,thisbuf):(S16  *) src, thisbuf*(16/info->bits), info->channels, &lstepi, &rstepi));

     }

     src = (S8  *) AIL_ptr_add(src,thisbuf);
     length -= thisbuf;
   }

   adp->datalen=(U32) AIL_ptr_dif(dst,(S8 *)(adp+1));
   adp->rifflen+=adp->datalen;


   if (outsize)
     *outsize=(U32) AIL_ptr_dif(dst,adp);

   if (outdata)
   {

     MEM_LE_SWAP32( &adp->rifflen );
     MEM_LE_SWAP32( &adp->fmtlen );
     MEM_LE_SWAP16( &adp->fmttag );
     MEM_LE_SWAP16( &adp->channels );
     MEM_LE_SWAP32( &adp->sampersec );
     MEM_LE_SWAP32( &adp->avepersec );
     MEM_LE_SWAP16( &adp->blockalign );
     MEM_LE_SWAP16( &adp->bitspersam );
     MEM_LE_SWAP16( &adp->extra );
     MEM_LE_SWAP16( &adp->samples_per_block );
     MEM_LE_SWAP32( &adp->factlen );
     MEM_LE_SWAP32( &adp->samples );
     MEM_LE_SWAP32( &adp->datalen );

     *outdata=adp;
   }
   else
     AIL_mem_free_lock(adp);

  return(1);
}

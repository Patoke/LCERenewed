//############################################################################
//##                                                                        ##
//##  DLSFILE.CPP                                                           ##
//##                                                                        ##
//##  DLS Level One utility routines                                        ##
//##                                                                        ##
//##  V1.00 of 14-Dec-97: Initial version                                   ##
//##                                                                        ##
//##  Author: John Miles                                                    ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  For technical support, contact RAD Game Tools at 425-893-4300.        ##
//##                                                                        ##
//############################################################################

#include "mss.h"
#include "imssapi.h"

#include "mssdls.h"
#include "dls1.h"
#include "synth.h"

#include <stdlib.h>

#ifndef WAVE_FORMAT_UNKNOWN
#define WAVE_FORMAT_UNKNOWN 0
#endif

#define min(a,b)  (((a) < (b)) ? (a) : (b))

#ifdef IS_DOS

//############################################################################
//##                                                                        ##
//## Locked code                                                            ##
//##                                                                        ##
//############################################################################

#define LOCK(x)   AIL_vmm_lock  (&(x),sizeof(x))
#define UNLOCK(x) AIL_vmm_unlock(&(x),sizeof(x))

static S32 locked = 0;

static void AIL_lock_end(void);

static void AIL_lock_start(void)
{
   if (!locked)
      {
      AIL_vmm_lock_range(AIL_lock_start, AIL_lock_end);

      LOCK(ASI_mem_src_ptr);
      LOCK(ASI_mem_src_len);
      LOCK(ASI_mem_src_pos);

      locked = 1;
      }
}

#define DOLOCK() AIL_lock_start()

#else

#define DOLOCK()

#endif

MSS_CB_STACK_ALIGN_DEC( ASI_mem_stream_CB )

// ---------------------------------------------------------------------------
// ptr_freebase
//
// Adjusts huge pointer by difference between it and specified cloned pointer,
// then frees the cloned pointer before returning the modified huge pointer
// ---------------------------------------------------------------------------

static void *ptr_freebase(void *clone, void *orig)
{
   void* ptr=AIL_ptr_from_clone(clone,orig);

   AIL_ptr_free_clone(clone);

   return ptr;
}


// ---------------------------------------------------------------------------
// Find named FORM in IFF file
//
// Returns NULL if not found, else returns huge pointer to FORM
// ---------------------------------------------------------------------------

static U8 * XMI_find_FORM(U8 *image, S32 size, C8 const *name)
{
   U32 len = 0;

   U8 *ptr = (U8 *) AIL_ptr_alloc_clone(image);

   U8 *end = (U8 *) AIL_ptr_add(image, size);

   do
      {
      //
      // Skip previous block, if any
      //

      AIL_ptr_inc_clone(ptr, len);

      //
      // Exit if not FORM or CAT block
      //

      if (AIL_ptr_ge(ptr,end))
         {
         AIL_ptr_free_clone(ptr);
         return NULL;
         }

      if ((AIL_strnicmp((char*)ptr,"FORM",4)) &&
          (AIL_strnicmp((char*)ptr,"CAT ",4)))
         {
         AIL_ptr_free_clone(ptr);
         return NULL;
         }

      //
      // Continue searching if not named form
      //
      // XMIDI files always have even FORM lengths; therefore, no
      // odd-byte compensation is needed
      //

      len = 8 + BEU_SWAP32( ((U8 *) (ptr+4)));
      }
   while (AIL_strnicmp((char*)ptr+8,name,4));

   //
   // Return location relative to selector originally passed in
   //

   return (U8 *) ptr_freebase(ptr, image);
}

// ---------------------------------------------------------------------------
// decode_ASI
//
// Decode specified number of bytes from ASI-encoded data source
//
// Backfill with silence if source stream ends prematurely
// ---------------------------------------------------------------------------

static S32 decode_ASI(C8   *file_suffix, //)
                      void *src,
                      S32       src_size,
                      void *dest,
                      S32       dest_size,
                      S32       desired_bits_per_sample)
{
   //
   // Decode MPEG or other ASI-compressed data
   //

   HPROVIDER ASI = find_ASI_provider("Input file types",
                                      file_suffix);

   if (!ASI)
      {
      AIL_debug_printf("DLSFILE: No ASI provider available for file type %s\n",
         file_suffix);
      return 0;
      }

   ASI_ERROR                 ASI_error;

   ASI_STREAM_OPEN           ASI_stream_open;
   ASI_STREAM_PROCESS        ASI_stream_process;
   ASI_STREAM_SEEK           ASI_stream_seek;
   ASI_STREAM_CLOSE          ASI_stream_close;
   ASI_STREAM_PROPERTY       ASI_stream_property;

   HPROPERTY INPUT_BIT_RATE;
   HPROPERTY INPUT_SAMPLE_RATE;
   HPROPERTY INPUT_BITS;
   HPROPERTY INPUT_CHANNELS;
   HPROPERTY OUTPUT_BIT_RATE;
   HPROPERTY OUTPUT_SAMPLE_RATE;
   HPROPERTY OUTPUT_BITS;
   HPROPERTY OUTPUT_CHANNELS;
   HPROPERTY OUTPUT_RESERVOIR;
   HPROPERTY POSITION;
   HPROPERTY PERCENT_DONE;
   HPROPERTY MIN_INPUT_BLOCK_SIZE;
   HPROPERTY RAW_RATE;
   HPROPERTY RAW_BITS;
   HPROPERTY RAW_CHANNELS;
   HPROPERTY REQUESTED_BIT_RATE;
   HPROPERTY REQUESTED_RATE;
   HPROPERTY REQUESTED_BITS;
   HPROPERTY REQUESTED_CHANS;

   RIB_INTERFACE_ENTRY ASICODEC[] =
      {
      FN(ASI_error)
      };

   RIB_INTERFACE_ENTRY ASISTR1[] =        // Break up into groups of 4 to avoid PS2 compiler warning 
      {
      FN(ASI_stream_property),
      FN(ASI_stream_open),
      FN(ASI_stream_seek),
      FN(ASI_stream_close),
      };

   RIB_INTERFACE_ENTRY ASISTR2[] =
      {
      FN(ASI_stream_process),
      PR("Input bit rate",           INPUT_BIT_RATE),
      PR("Input sample rate",        INPUT_SAMPLE_RATE),
      };

   RIB_INTERFACE_ENTRY ASISTR3[] =
      {
      PR("Input sample width",       INPUT_BITS),
      PR("Input channels",           INPUT_CHANNELS),
      PR("Output bit rate",          OUTPUT_BIT_RATE),
      PR("Output sample rate",       OUTPUT_SAMPLE_RATE),
      };

   RIB_INTERFACE_ENTRY ASISTR4[] =
      {
      PR("Output sample width",      OUTPUT_BITS),
      PR("Output channels",          OUTPUT_CHANNELS),
      PR("Output reservoir",         OUTPUT_RESERVOIR),
      PR("Position",                 POSITION),
      };

   RIB_INTERFACE_ENTRY ASISTR5[] =
      {
      PR("Percent done",             PERCENT_DONE),
      PR("Minimum input block size", MIN_INPUT_BLOCK_SIZE),
      PR("Raw source sample rate",   RAW_RATE),
      PR("Raw source sample width",  RAW_BITS),
      };

   RIB_INTERFACE_ENTRY ASISTR6[] =
      {
      PR("Raw source channels",      RAW_CHANNELS),
      PR("Requested bit rate",       REQUESTED_BIT_RATE),
      PR("Requested sample rate",    REQUESTED_RATE),
      PR("Requested sample width",   REQUESTED_BITS),
      };

   RIB_INTERFACE_ENTRY ASISTR7[] =
      {
      PR("Requested channels",       REQUESTED_CHANS)
      };

   RIB_request(ASI,"ASI codec",ASICODEC);
   RIB_request(ASI,"ASI stream",ASISTR1);
   RIB_request(ASI,"ASI stream",ASISTR2);
   RIB_request(ASI,"ASI stream",ASISTR3);
   RIB_request(ASI,"ASI stream",ASISTR4);
   RIB_request(ASI,"ASI stream",ASISTR5);
   RIB_request(ASI,"ASI stream",ASISTR6);
   RIB_request(ASI,"ASI stream",ASISTR7);

   //
   // Open stream with codec, registering callback function
   //

   ASI_mem_src_len = src_size;
   ASI_mem_src_ptr = (U8 *) src;
   ASI_mem_src_pos = 0;

   HASISTREAM stream =  ASI_stream_open((MSS_ALLOC_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_alloc_info ), 
                                        (MSS_FREE_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_free_info ),
                                        0,
                                        (AILASIFETCHCB) MSS_CALLBACK_ALIGNED_NAME( ASI_mem_stream_CB ),
                                        ASI_mem_src_len);

   if (stream == 0)
      {
      AIL_debug_printf("DLSFILE: Could not open stream (%s)\n",ASI_error());
      return 0;
      }

   S32 total_len = dest_size;

   //
   // Process input data
   //

   if (desired_bits_per_sample == 16)
      {
      S16 *d = (S16 *) dest;

      while (total_len)
         {
         S32 len = ASI_stream_process(stream,
                                      d,
                                      min(1024,total_len));
         if (!len)
            {
            if (total_len)
               {
               AIL_memset(d, 0, total_len);
               }

            break;
            }

         d = (S16 *) AIL_ptr_add(d, len);

         total_len -= len;
         }
      }
   else
      {
      U8 *d = (U8 *) dest;

      while (total_len)
         {
         static S16 sign_buffer[1024];

         S32 bytes = ASI_stream_process(stream,
                                        sign_buffer,
                                        min((S32) (1024 * sizeof(S16)), total_len*2));
         S32 len = bytes >> 1;

         if (!len)
            {
            if (total_len)
               {
               AIL_memset(d, 0x80, total_len);
               }

            break;
            }
         else
            {
            for (S32 i=0; i < len; i++)
               {
               d[i] = (U8) (((sign_buffer[i] >> 8) & 0xff) ^ 0x80);
               }
            }

         d = (U8 *) AIL_ptr_add(d, len);

         total_len -= len;
         }
      }

   //
   // Return success
   //

   ASI_stream_close(stream);

   return 1;
}

// ---------------------------------------------------------------------------
// get_expanded_DLS_size
// ---------------------------------------------------------------------------

static S32 get_expanded_DLS_size(void *MLS, S32 MLS_size)
{
   //
   // Locate wave pool table in file image
   //

   CHUNK *c1 = (CHUNK *) AIL_ptr_alloc_clone(MLS);

   CHUNK *n1 = (CHUNK *) AIL_ptr_add(MLS, LEU_SWAP32( &c1->ckSize ) + 8 + (LEU_SWAP32( &c1->ckSize) & 1));
   CHUNK *c2 = (CHUNK *) AIL_ptr_alloc_clone(c1->data);

   while (AIL_ptr_lt(c2,n1))
      {
      AIL_ptr_fixup_clone(c2);

      if (c2->ckID == MSSFOURCC_PTBL)
         {
         //
         // Look at cCues word in pool table structure -- if high byte zero,
         // assume this is an ADPCM-compressed file at 4:1
         //
         // Otherwise, the high byte is the approximate compression ratio
         // obtained from ASI encoding
         //

         POOLTABLE *ptbl = (POOLTABLE *)
            AIL_ptr_alloc_clone(((SUBCHUNK *) c2)->data);

         U32 ratio = (LEU_SWAP32( &ptbl->cCues) >> 24) & 0xff;

         //
         // Clear compression-ratio byte to restore DLS compatibility
         //

         STORE_LEU_SWAP32( &ptbl->cCues,LEU_SWAP32( &ptbl->cCues ) & 0x00ffffff );

         AIL_ptr_free_clone(ptbl);
         AIL_ptr_free_clone(c2);
         AIL_ptr_free_clone(c1);

         //
         // Return decompressed file size estimate (after conservative
         // rounding at compression time)
         //

         if (ratio == 0)
            {
            return MLS_size * 4;
            }
         else
            {
            return MLS_size * ratio;
            }
         }

      AIL_ptr_inc_clone(c2, LEU_SWAP32( &c2->ckSize ) + 8 + (LEU_SWAP32( &c2->ckSize ) & 1));
      }

// debug_printf("No PTBL chunk found\n");

   AIL_ptr_free_clone(c2);
   AIL_ptr_free_clone(c1);

   return 0;
}

// ---------------------------------------------------------------------------
// expand
// ---------------------------------------------------------------------------

static int expand(void *out, S32 *out_size,AILLENGTHYCB callback)
{
   //
   // Pass 1: Locate wave pool table in output file image
   //

   POOLTABLE *ptbl = NULL;
   POOLCUE   *cues = NULL;

   CHUNK *c1 = (CHUNK *) AIL_ptr_alloc_clone(out);

   CHUNK *n1 = (CHUNK *) AIL_ptr_add(out, LEU_SWAP32( &c1->ckSize ) + 8 + (LEU_SWAP32( &c1->ckSize ) & 1));
   CHUNK *c2 = (CHUNK *) AIL_ptr_alloc_clone(c1->data);

   while (AIL_ptr_lt(c2,n1))
      {
      AIL_ptr_fixup_clone(c2);

      if (c2->ckID == MSSFOURCC_PTBL)
         {
         ptbl = (POOLTABLE *) AIL_ptr_alloc_clone(((SUBCHUNK *) c2)->data);
         cues = (POOLCUE   *) AIL_ptr_alloc_clone(&((U8 *) (ptbl))[LEU_SWAP32( &ptbl->cbSize ) ]);
         break;
         }

      AIL_ptr_inc_clone(c2, LEU_SWAP32( &c2->ckSize ) + 8 + ( LEU_SWAP32( &c2->ckSize ) & 1));
      }

   if (ptbl == NULL)
      {
//      debug_printf("No PTBL chunk found\n");
      return 0;
      }

   AIL_ptr_free_clone(c2);
   AIL_ptr_free_clone(c1);

   //
   // Pass 2: Temporarily replace PTBL offsets with WAVE ordinal indexes
   //

   c1 = (CHUNK *) AIL_ptr_alloc_clone(out);

   n1 = (CHUNK *) AIL_ptr_add(out, LEU_SWAP32( &c1->ckSize ) + 8 + ( LEU_SWAP32( &c1->ckSize ) & 1));
   c2 = (CHUNK *) AIL_ptr_alloc_clone(c1->data);

   U32 nth_wave = 0;

   U32 n_wave_cues = LEU_SWAP32( &ptbl->cCues ) & 0xffffff;    // Low 3 bytes store # of cues

   U32 cues_so_far=0;

   while (AIL_ptr_lt(c2,n1))
      {
      AIL_ptr_fixup_clone(c2);

      if ((c2->ckID == MSSFOURCC_LIST) && (c2->subID == MSSFOURCC_WVPL))
         {
         CHUNK *n2 = (CHUNK *) AIL_ptr_alloc_clone(c2);
         AIL_ptr_inc_clone(n2, LEU_SWAP32( &c2->ckSize ) + 8 + (LEU_SWAP32( &c2->ckSize ) & 1));

         CHUNK *c3 = (CHUNK *) AIL_ptr_alloc_clone(c2->data);

         while (AIL_ptr_lt(c3,n2))
            {
            AIL_ptr_fixup_clone(c3);

            if ((c3->ckID == MSSFOURCC_LIST) && (c3->subID == MSSFOURCC_wave))
               {
               U32 offset = (U32) AIL_ptr_dif(c3, c2->data);

               for (U32 index=0; index < n_wave_cues; index++)
                  {
                  if (LEU_SWAP32( &cues[index].ulOffset ) == offset)
                     {
#if defined(IS_PS2) || defined(IS_PSP)
                     STORE_LEU_SWAP32(&cues[index].ulOffset, nth_wave);
#else
                     cues[index].ulOffset = nth_wave;
#endif
                     }
                  }

               ++nth_wave;
               }

            AIL_ptr_inc_clone(c3, LEU_SWAP32( &c3->ckSize ) + 8 + (LEU_SWAP32( &c3->ckSize ) & 1));
            }

         AIL_ptr_free_clone(c3);
         AIL_ptr_free_clone(n2);
         }

      AIL_ptr_inc_clone(c2, LEU_SWAP32( &c2->ckSize ) + 8 + (LEU_SWAP32( &c2->ckSize ) & 1));
      }

   AIL_ptr_free_clone(c2);
   AIL_ptr_free_clone(c1);

   //
   // Pass 3: Generate PCM data to replace compressed chunks
   //

   c1 = (CHUNK *) AIL_ptr_alloc_clone(out);
   c2 = (CHUNK *) AIL_ptr_alloc_clone(c1->data);

   while (AIL_ptr_lt(c2, AIL_ptr_add(out, LEU_SWAP32( &c1->ckSize ) + 8 + (LEU_SWAP32( &c1->ckSize ) & 1))))
      {
      AIL_ptr_fixup_clone(c2);

      if ((c2->ckID == MSSFOURCC_LIST) && (c2->subID == MSSFOURCC_WVPL))
         {
         CHUNK *n2 = (CHUNK *) AIL_ptr_alloc_clone(c2);
         AIL_ptr_inc_clone(n2, LEU_SWAP32( &c2->ckSize ) + 8 + (LEU_SWAP32( &c2->ckSize ) & 1));

         CHUNK *c3 = (CHUNK *) AIL_ptr_alloc_clone(c2->data);

         while (AIL_ptr_lt(c3,n2))
            {
            AIL_ptr_fixup_clone(c3);

            if ((c3->ckID == MSSFOURCC_LIST) && (c3->subID == MSSFOURCC_wave))
               {
               CHUNK *n3 = (CHUNK *) AIL_ptr_alloc_clone(c3);
               AIL_ptr_inc_clone(n3, LEU_SWAP32( &c3->ckSize ) + 8 + (LEU_SWAP32( &c3->ckSize ) & 1));

               CHUNK *c4 = (CHUNK *) AIL_ptr_alloc_clone(c3->data);

               ++cues_so_far;

               if (callback) {
                 F32 per=(F32)(cues_so_far*100)/(F32)n_wave_cues;
                 if (per>100.0F)
                   per=100.0F;

                 if (callback(AIL_LENGTHY_UPDATE,*(U32*)(void*)&per)==0) {
                   AIL_set_error("Decompression cancelled.");
                   return(0);
                 }
               }

               S32 total_samples     = 0;
               S32 bits_per_sample   = 0;
               S32 samples_per_block = 0;
               S32 alignment         = 0;
               S32 need_ASI          = 0;
               ASI_FMT ASI_chunk;

               while (AIL_ptr_lt(c4,n3))
                  {
                  AIL_ptr_fixup_clone(c4);

                  if (c4->ckID == MSSFOURCC_PCM)
                     {
                     //
                     // PCM = original PCM FMT chunk -- determine # of bits
                     // per sample to unpack, and rename it to 'fmt'
                     //

                     c4->ckID = MSSFOURCC_FMT;
                     bits_per_sample = LE_SWAP16( &((WAV_FMT *) c4)->bits_per_sample );
                     }
                  else if (c4->ckID == MSSFOURCC_FMT)
                     {
                     //
                     // FMT = IMA ADPCM FMT chunk -- read required packing
                     // information from it, and rename it to 'junk'
                     //
                     // Determine compression type (WAVE_FORMAT_IMA_ADPCM
                     // or WAVE_FORMAT_UNKNOWN) -- in the latter case, we'll
                     // need to search for an ASI codec to read the specified
                     // file type
                     //

                     c4->ckID = MSSFOURCC_JUNK;

                     if (LE_SWAP16( &((IMA_FMT *) c4)->format_tag ) == WAVE_FORMAT_IMA_ADPCM)
                        {
                        need_ASI = 0;
                        samples_per_block = LE_SWAP16( &((IMA_FMT *) c4)->samples_per_block );
                        alignment         = LE_SWAP16( &((IMA_FMT *) c4)->alignment );
                        }
                     else if (LE_SWAP16( &((IMA_FMT *) c4)->format_tag ) == WAVE_FORMAT_UNKNOWN)
                        {
                        need_ASI = 1;
                        ASI_chunk = *((ASI_FMT *) c4);
                        MEM_LE_SWAP32( &ASI_chunk.chunk_size );
                        MEM_LE_SWAP16( &ASI_chunk.format_tag );               // WAVE_FORMAT_UNKNOWN
                        MEM_LE_SWAP16( &ASI_chunk.channels );                 // # of channels
                        MEM_LE_SWAP32( &ASI_chunk.sample_rate );              // Sample rate in samples/second
                        MEM_LE_SWAP32( &ASI_chunk.average_data_rate );        // Stream rate in bytes per second
                        MEM_LE_SWAP16( &ASI_chunk.alignment );                // Always 1 (actual alignment constraints are determined by ASI decoder)
                        MEM_LE_SWAP16( &ASI_chunk.bits_per_sample );          // Bits/sample value from encoder output
                        MEM_LE_SWAP16( &ASI_chunk.extra );                    // Always 4
                        }
                     }
                  else if (c4->ckID == MSSFOURCC_FACT)
                     {
                     //
                     // FACT contains information about the # of samples
                     // to be unpacked -- it's also renamed to 'junk' to
                     // ensure .WAV compatibility
                     //

                     c4->ckID = MSSFOURCC_JUNK;

                     total_samples = LEU_SWAP32( &((IMA_FACT *) c4)->samples );
                     }
                  else if (c4->ckID == MSSFOURCC_DATA)
                     {
                     //
                     // DATA will be encountered after the other chunk types,
                     // and can be expanded based on information already
                     // encountered
                     //

                     S32 size = total_samples * (bits_per_sample / 8);

                     WAV_DATA *data = (WAV_DATA *) AIL_mem_alloc_lock(size + 8 + 1);

                     if (data == NULL)
                        {
//                        debug_printf("Insufficient free memory to expand file\n");
                        return 0;
                        }

                     AIL_memcpy(data->DATA_string,"data",4);
                     data->chunk_size = LEU_SWAP32( &size );

                     //
                     // Decode data stream
                     //

                     U8 *s = (U8 *) (((SUBCHUNK *) c4)->data);

                     if (need_ASI == 1)
                        {
                        if (!decode_ASI(ASI_chunk.original_file_suffix,
                                        s,
                                        LEU_SWAP32( &((IMA_DATA*) c4)->chunk_size ),
                                        data->data,
                                        LEU_SWAP32( &data->chunk_size ),
                                        bits_per_sample))
                           {
                           return 0;
                           }
                        }
                     else
                        {
                        //
                        // Decode ADPCM data
                        //

                        MIXER_ADPCM_DECODE  MIXER_adpcm_decode;

                        HPROVIDER HP;

                        RIB_enumerate_providers("MSS mixer services",
                                                 NULL,
                                                &HP);

                        RIB_INTERFACE_ENTRY MIXER[] =
                            {
                            FN(MIXER_adpcm_decode)
                            };

                        RIB_request(HP,"MSS mixer services",MIXER);

                         S16 *d = (S16*)(void*)data->data;

                         ADPCMDATA ad;

                         ad.blocksize = alignment;
                         ad.extrasamples=0;
                         ad.blockleft=0;

                        MIXER_adpcm_decode(&d,&s, size, LEU_SWAP32( &((IMA_DATA*)c4)->chunk_size ),
                                           (bits_per_sample==16) ? M_SRC_16:0, &ad);
                       
                        }

                     //
                     // Get change in size of expanded chunk
                     //

                     S32 old_size = LEU_SWAP32( &c4->ckSize ) + 8 + (LEU_SWAP32( &c4->ckSize ) & 1);
                     S32 new_size = LEU_SWAP32( &data->chunk_size ) + 8 + (LEU_SWAP32( &data->chunk_size ) & 1);
                     S32 delta_size = new_size - old_size;

                     //
                     // Resize memory image
                     //

                     void *end = AIL_ptr_add(out, LEU_SWAP32( &c1->ckSize ) + 8 + (LEU_SWAP32( &c1->ckSize ) & 1));

                     CHUNK *nxt = (CHUNK *) AIL_ptr_alloc_clone(c4);
                     AIL_ptr_inc_clone(nxt, old_size);

                     CHUNK *dest = (CHUNK *) AIL_ptr_alloc_clone(c4);
                     AIL_ptr_inc_clone(dest, new_size);             

                     AIL_memmove(dest, nxt, AIL_ptr_dif(end,nxt));

                     AIL_ptr_free_clone(dest);
                     AIL_ptr_free_clone(nxt);

                     //
                     // Copy new chunk data
                     //

                     AIL_memcpy(c4, data, LEU_SWAP32( &data->chunk_size ) + 8);

                     //
                     // Adjust file and chunk sizes
                     //

                     *out_size += delta_size;

                     AIL_ptr_inc_clone(n3, delta_size);
                     AIL_ptr_inc_clone(n2, delta_size);

                     STORE_LEU_SWAP32( &c1->ckSize, LEU_SWAP32( &c1->ckSize) + delta_size );
                     STORE_LEU_SWAP32( &c2->ckSize, LEU_SWAP32( &c2->ckSize) + delta_size );
                     STORE_LEU_SWAP32( &c3->ckSize, LEU_SWAP32( &c3->ckSize) + delta_size );

                     //
                     // Free working copy of DATA chunk
                     //

                     AIL_mem_free_lock(data);
                     }

                  AIL_ptr_inc_clone(c4, LEU_SWAP32( &c4->ckSize ) + 8 + (LEU_SWAP32( &c4->ckSize ) & 1));
                  }

               AIL_ptr_free_clone(c4);
               AIL_ptr_free_clone(n3);
               }

            AIL_ptr_inc_clone(c3, LEU_SWAP32( &c3->ckSize ) + 8 + (LEU_SWAP32( &c3->ckSize ) & 1));
            }

         AIL_ptr_free_clone(c3);
         AIL_ptr_free_clone(n2);
         }

      AIL_ptr_inc_clone(c2, LEU_SWAP32( &c2->ckSize ) + 8 + (LEU_SWAP32( &c2->ckSize ) & 1));
      }

   AIL_ptr_free_clone(c2);
   AIL_ptr_free_clone(c1);

   //
   // Pass 4: Replace PTBL ordinals with offsets to new WAVEs
   //

   c1 = (CHUNK *) AIL_ptr_alloc_clone(out);

   n1 = (CHUNK *) AIL_ptr_add(out, LEU_SWAP32( &c1->ckSize ) + 8 + (LEU_SWAP32( &c1->ckSize ) & 1));
   c2 = (CHUNK *) AIL_ptr_alloc_clone(c1->data);

   nth_wave = 0;

   while (AIL_ptr_lt(c2,n1))
      {
      AIL_ptr_fixup_clone(c2);

      if ((c2->ckID == MSSFOURCC_LIST) && (c2->subID == MSSFOURCC_WVPL))
         {
         CHUNK *n2 = (CHUNK *) AIL_ptr_alloc_clone(c2);
         AIL_ptr_inc_clone(n2, LEU_SWAP32( &c2->ckSize ) + 8 + (LEU_SWAP32( &c2->ckSize ) & 1));

         CHUNK *c3 = (CHUNK *) AIL_ptr_alloc_clone(c2->data);

         while (AIL_ptr_lt(c3,n2))
            {
            AIL_ptr_fixup_clone(c3);

            if ((c3->ckID == MSSFOURCC_LIST) && (c3->subID == MSSFOURCC_wave))
               {
               U32 offset = (U32) AIL_ptr_dif(c3, c2->data);

               for (U32 index=0; index < n_wave_cues; index++)
                  {
#if defined(IS_PS2) || defined(IS_PSP)
                  if (LEU_SWAP32(&cues[index].ulOffset) == nth_wave)
                     {
                     STORE_LEU_SWAP32(&cues[index].ulOffset, LEU_SWAP32( &offset ));
#else
                  if (cues[index].ulOffset == nth_wave)
                     {
                     STORE_LEU_SWAP32(&cues[index].ulOffset, offset );
#endif
                     }
                  }

               ++nth_wave;
               }

            AIL_ptr_inc_clone(c3, LEU_SWAP32( &c3->ckSize ) + 8 + (LEU_SWAP32( &c3->ckSize ) & 1));
            }

         AIL_ptr_free_clone(c3);
         AIL_ptr_free_clone(n2);
         }

      AIL_ptr_inc_clone(c2, LEU_SWAP32( &c2->ckSize ) + 8 + (LEU_SWAP32( &c2->ckSize ) & 1));
      }

   AIL_ptr_free_clone(c2);
   AIL_ptr_free_clone(c1);

   //
   // Free pool table and cue list selectors and return success
   //

   AIL_ptr_free_clone(ptbl);
   AIL_ptr_free_clone(cues);

   return 1;
}

// ---------------------------------------------------------------------------
// AIL_extract_DLS
// ---------------------------------------------------------------------------

extern "C" S32 AILCALL AIL_API_extract_DLS(void const       *source_image, //)
                                  U32             source_size,
                                  void * *XMI_output_data,
                                  U32        *XMI_output_size,
                                  void * *DLS_output_data,
                                  U32        *DLS_output_size,
                                  AILLENGTHYCB      callback)
{
   DOLOCK();

   //
   // Invalidate both output fields
   //

   if (XMI_output_data) *XMI_output_data = NULL;
   if (DLS_output_data) *DLS_output_data = NULL;
   if (XMI_output_size) *XMI_output_size = 0;
   if (DLS_output_size) *DLS_output_size = 0;

   U8 *XMI_start = NULL;

   U8 *XLS_start = NULL;
   S32     XLS_size  = 0;

   U8 *XDLS_start = NULL;

   //
   // If this file's first four characters are "FORM" or "CAT " (i.e., it's
   // an EA IFF file), search for FORMs in incoming file
   //

   S32 is_IFF = (!AIL_strnicmp((char*)source_image,"FORM",4)) ||
                (!AIL_strnicmp((char*)source_image,"CAT ",4));

   if (is_IFF)
      {
      //
      // Locate optional FORM/CAT XMID/XDIR in incoming IFF file
      //

      XMI_start = XMI_find_FORM((U8 *) source_image,
                                           source_size,
                                          "XDIR");

      if (XMI_start == NULL)
         {
         XMI_start = XMI_find_FORM((U8 *) source_image,
                                              source_size,
                                             "XMID");
         }

      //
      // Locate optional FORM XDLS block in incoming IFF file
      //

      XDLS_start = XMI_find_FORM((U8 *) source_image,
                                            source_size,
                                           "XDLS");
      if (XDLS_start != NULL)
         {
         XLS_start = (U8 *) AIL_ptr_add(XDLS_start, 12);
         XLS_size  = BEU_SWAP32( ((U8 *) (XDLS_start+4)));
         }
      }
   else
      {
      //
      // This is not an IFF file -- set up to see if it's a RIFF MLS or
      // RIFF DLS file
      //

      XLS_start = (U8 *) source_image;
      XLS_size  = source_size;
      }

   if (callback)
     if (callback(AIL_LENGTHY_INIT,0)==0) {
       AIL_set_error("Decompression cancelled.");
       return(0);
     }

   //
   // Return unpacked DLS data to application
   //
   // If this DLS file is compressed (type MLS with 4:1 ADPCM), reallocate
   // its memory block conservatively and decompress it
   //

   U8 *DLS_start = NULL;
   S32     DLS_size  = 0;

   if (XLS_start != NULL)
      {
      CHUNK *c1 = (CHUNK *) XLS_start;

      if (c1->subID == MSSFOURCC_MLS)
      {
         S32 new_size = get_expanded_DLS_size(XLS_start, XLS_size);

         c1->subID = MSSFOURCC_DLS;

         if (DLS_output_data)
         {
           DLS_start = (U8 *) AIL_mem_alloc_lock(new_size);
           DLS_size  = XLS_size;

           if (DLS_start == NULL)
           {
             return(0);
           }

           AIL_memcpy(DLS_start, XLS_start, DLS_size);

           expand(DLS_start, &DLS_size,callback);

           *DLS_output_data = DLS_start;
         }

         if (DLS_output_size) *DLS_output_size = DLS_size;
      }
      else if (c1->subID == MSSFOURCC_DLS)
      {
         if (DLS_output_data)
         {
           void *DLS = AIL_mem_alloc_lock(XLS_size);

           if (DLS == NULL)
           {
             return(0);
           }

           AIL_memcpy(DLS, XLS_start, XLS_size);

           *DLS_output_data = DLS;
         }

         if (DLS_output_size) *DLS_output_size = XLS_size;
      }
    } else {
      AIL_set_error("Unknown file type.");
      return(0);
    }

   //
   // Return XMIDI data to application
   //

   if (XMI_start != NULL)
   {
      //
      // Return XMIDI data to application
      //

      S32 XMI_size;

      if (XDLS_start != NULL)
         {
         XMI_size = (U32) AIL_ptr_dif(XDLS_start, XMI_start);
         }
      else
         {
         XMI_size = source_size;
         }

      if (XMI_output_data)
      {
        void *XMI = AIL_mem_alloc_lock(XMI_size);

        if (XMI == NULL)
        {
          return(0);
        }

        AIL_memcpy(XMI, XMI_start, XMI_size);

        *XMI_output_data = XMI;
      }

      if (XMI_output_size) *XMI_output_size = XMI_size;
   }

   if (callback)
     callback(AIL_LENGTHY_DONE,0);

   return(1);
}

extern "C" S32 AILCALL AIL_API_find_DLS       (void const* data, U32 size,
                                       void**xmi, U32* xmisize,
                                       void**dls, U32* dlssize)
{
  DOLOCK();

   if (((CHUNK*)data)->ckID == MSSFOURCC_RIFF)
      {
      if ((((CHUNK*)data)->subID == MSSFOURCC_DLS) ||
          (((CHUNK*)data)->subID == MSSFOURCC_MLS))
         {
         if (dls)     *dls = (void*)data;
         if (dlssize) *dlssize = LEU_SWAP32( &((CHUNK *) data)->ckSize) + 8;
         return 1;
         }
      }

  if ((!AIL_strnicmp((char*) (data)  , "FORM", 4)) &&
      (!AIL_strnicmp((char*) (data)+8, "XDIR", 4))) {

    //figure out type of XMIDI

    U8* XDLS_start = XMI_find_FORM((U8 *) data,
                                                  size,
                                                  "XDLS");
    if (XDLS_start != NULL)
       {
       XDLS_start = (U8 *) AIL_ptr_add(XDLS_start, 12);

       if (dls)
         *dls=XDLS_start;

       if (dlssize)
         *dlssize=LEU_SWAP32( &(((CHUNK*)XDLS_start)->ckSize ) )+8;

       } else {

         if (dls)
           *dls=0;

         if (dlssize)
           *dlssize=0;

       }

    if (xmi)
      *xmi=(void*)data;

    if (xmisize) {
      CHUNK* ch=(CHUNK*)data;

      U32 size=0;
      U32 chsize=0;
      do {
        ch=(CHUNK*)AIL_ptr_add(ch,chsize);
        chsize=BEU_SWAP32( &ch->ckSize )+8;
        size+=chsize;
      } while (AIL_strnicmp((char*)ch,"CAT ",4));

      *xmisize=size;
    }

    return(1);
  } else {
    AIL_set_error("Not an XMI file.");
    return(0);
  }
}

#ifdef IS_DOS

//############################################################################
//##                                                                        ##
//## End of locked code                                                     ##
//##                                                                        ##
//############################################################################

static void AIL_lock_end(void)
{
   if (locked)
      {
      AIL_vmm_unlock_range(AIL_lock_start, AIL_lock_end);

      locked = 0;
      }
}

#endif


//############################################################################
//##                                                                        ##
//##  MSSUTIL.CPP                                                           ##
//##                                                                        ##
//##  DLS Level One utility routines                                        ##
//##                                                                        ##
//##  Author: John Miles and Jeff Roberts                                   ##
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

//
// MDI_DRIVER list
//

HMDIDRIVER MDI_first = NULL;

//
// .WAV chunks
//

typedef MSS_STRUCT
{
  S8   FMT_string[4];
  U32  chunk_size;

  S16  format_tag;
  S16  channels;
  S32  sample_rate;
  S32  average_data_rate;
  S16  alignment;
  S16  bits_per_sample;
  S16  extra;
  S16  samples_per_block;
}
FMT;

typedef struct
{
  S8  DATA_string[4];
  U32 chunk_size;
  U8  data[4];
}
DATA;

#ifndef IS_STATIC

U8 *ASI_mem_src_ptr;
S32     ASI_mem_src_len;
S32     ASI_mem_src_pos;

// ---------------------------------------------------------------------------
// Stream callback for ASI decoder/encoder
// ---------------------------------------------------------------------------

S32 AILLIBCALLBACK ASI_mem_stream_CB(UINTa     user, //)
                                     void *dest,
                                     S32       bytes_requested,
                                     S32       offset)
{
   user = user; // for watcom

   if (offset != -1)
      {
      ASI_mem_src_pos = offset;
      }

   if ((ASI_mem_src_pos + bytes_requested) > ASI_mem_src_len)
      {
      bytes_requested = ASI_mem_src_len - ASI_mem_src_pos;
      }

   AIL_memcpy(dest, &ASI_mem_src_ptr[ASI_mem_src_pos], bytes_requested);

   ASI_mem_src_pos += bytes_requested;

   return bytes_requested;
}

MSS_CB_STACK_ALIGN( ASI_mem_stream_CB );

//############################################################################
//#                                                                          #
//# Find ASI provider for specified file type, and make sure it's up to date #
//#                                                                          #
//############################################################################

HPROVIDER find_ASI_provider(const C8 *attrib, //)  
                            const C8 *filename)
{
   HPROVIDER result = RIB_find_file_provider("ASI codec",
                                              attrib,
                                              filename);
   if (result == 0)
      {
      return 0;
      }
   
   PROVIDER_PROPERTY PROVIDER_property = NULL;

   RIB_INTERFACE_ENTRY ASI[] =
      {
      FN(PROVIDER_property),
      };

   if (RIB_request(result,"ASI codec",ASI) != RIB_NOERR)
      {
      AIL_set_error("Out of memory.");
      return 0;
      }

   return result;
}

#endif

//############################################################################
//##                                                                        ##
//## Set the compression preferences                                        ##
//##                                                                        ##
//############################################################################

static HPROVIDER comp_ASI;
static HASISTREAM comp_stream=0;
static ASI_STREAM_PROPERTY ASI_stream_property;

static S32 AILCALLBACK compress_setprop(char const* preference,void const* value)
{
   UINTa token;

   if (RIB_request_interface_entry(comp_ASI,"ASI stream",RIB_PROPERTY,preference,&token))
     return(0);

   ASI_stream_property(comp_stream,
                       token,
                       0,&value,0);
   return(1);
}


//############################################################################
//##                                                                        ##
//## Compress with ASI codec                                                ##
//##                                                                        ##
//############################################################################

static S32 AILCALL compress_ASI(AILSOUNDINFO const* info, //)
                                void** outdata,
                                U32* outsize,
                                AILLENGTHYCB callback)
{
   //
   // ASI functions
   //

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

   RIB_INTERFACE_ENTRY ASISTR1[] =
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

   RIB_request(comp_ASI,"ASI codec",ASICODEC);
   RIB_request(comp_ASI,"ASI stream",ASISTR1);
   RIB_request(comp_ASI,"ASI stream",ASISTR2);
   RIB_request(comp_ASI,"ASI stream",ASISTR3);
   RIB_request(comp_ASI,"ASI stream",ASISTR4);
   RIB_request(comp_ASI,"ASI stream",ASISTR5);
   RIB_request(comp_ASI,"ASI stream",ASISTR6);
   RIB_request(comp_ASI,"ASI stream",ASISTR7);

   //
   // Open stream with codec, registering callback function
   //

   ASI_mem_src_ptr = (U8 *) info->data_ptr;
   ASI_mem_src_len = info->data_len;
   ASI_mem_src_pos = 0;

   comp_stream = ASI_stream_open((MSS_ALLOC_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_alloc_info ), 
                                 (MSS_FREE_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_free_info ),
                                 0,
                                 (AILASIFETCHCB) MSS_CALLBACK_ALIGNED_NAME( ASI_mem_stream_CB ),
                                 ASI_mem_src_len);

   if (comp_stream == 0)
      {
      AIL_set_error(ASI_error());
      return 0;
      }

   //
   // Set source stream parameters
   //

   ASI_stream_property(comp_stream,
                       RAW_RATE,
                       0,&info->rate,0);

   ASI_stream_property(comp_stream,
                       RAW_BITS,
                       0,&info->bits,0);

   ASI_stream_property(comp_stream,
                       RAW_CHANNELS,
                       0,&info->channels,0);

   if (callback)
     if (callback(AIL_LENGTHY_SET_PROPERTY,(UINTa)compress_setprop)==0) {
       ASI_stream_close(comp_stream);
       comp_stream=0;
       AIL_set_error("Compression cancelled.");
       return(0);
     }

   //
   // Allocate output block equal in size to input data + 64K for overhead
   //

   void *out = AIL_mem_alloc_lock(ASI_mem_src_len + 65536L);

   if (out == 0)
      {
      ASI_stream_close(comp_stream);
      return 0;
      }

   //
   // Process input data
   //

   S32 len;
   S32 total_len = 0;
   void *dest = out;

   while ((len = ASI_stream_process(comp_stream,
                                    dest,
                                    1024)) != 0)
      {
      dest = AIL_ptr_add(dest, len);
      total_len += len;
      }

   if (outsize != 0)
      {
      *outsize = total_len;
      }

   if (outdata != 0)
      {
      *outdata = out;
      }

#if 0
   AIL_debug_printf("%d bytes generated by encoder\n",total_len);
#endif

   ASI_stream_close(comp_stream);

   return 1;
}

// ---------------------------------------------------------------------------
// merge
// ---------------------------------------------------------------------------

DXDEF
S32 AILEXPORT AIL_merge_DLS_with_XMI(void const* xmi, void const* dls,
                                  void** mss, U32* msssize)
{
  if ((xmi==0) || (dls==0))
    return(0);

   if ((AIL_strnicmp((char*) xmi, "FORM", 4)) ||
       (AIL_strnicmp(((char*) xmi)+8, "XDIR", 4)))
     {
     AIL_set_error("Not an XMI file.");
     return(0);
     }

  if ((((CHUNK*)dls)->subID != MSSFOURCC_DLS) && (((CHUNK*)dls)->subID != MSSFOURCC_MLS))
     {
     AIL_set_error("Not a DLS file.");
     return(0);
     }

  CHUNK* ch=(CHUNK*)xmi;

  U32 xmi_size=0;
  U32 chsize=0;
  do {
    ch=(CHUNK*)AIL_ptr_add(ch,chsize);
    chsize=BEU_SWAP32( &ch->ckSize)+8;
    xmi_size+=chsize;
  } while (AIL_strnicmp((char*)ch,"CAT ",4));

  HMEMDUMP m=AIL_mem_create();

  AIL_mem_write(m,xmi,xmi_size);
  
  AIL_mem_prints(m,"FORM");

  U32 orig_dls_size = ((CHUNK*)dls)->ckSize + 8;
  U32 dls_size=BEU_SWAP32( &orig_dls_size );

  AIL_mem_write(m,&dls_size,4);

  AIL_mem_prints(m,"XDLS");

  AIL_mem_write(m,dls,LE_SWAP32(&orig_dls_size));

  if ( !AIL_mem_close(m,mss,msssize) )
    AIL_set_error("Out of memory.");

  return(1);
}

// ---------------------------------------------------------------------------
// remove_chunk
//
// Removes chunk at given address from RIFF file at given address,
// returning # of bytes removed from file
// ---------------------------------------------------------------------------

typedef struct CDESC
{
  CHUNK *c;  // current LIST or RIFF chunk header
  CHUNK *n;  // end of LIST or RIFF chunk
} CDESC;

static S32 remove_chunk(CHUNK *chunk, void*form)
{
   CDESC chunk_stack[256];

   //
   // Traverse file until specified chunk found
   //

   CDESC *c = chunk_stack;

   c[0].n = c[0].c = NULL;

   CHUNK *cur = (CHUNK *) form;

   SINTa end = (SINTa)(form) + (LE_SWAP32( &cur->ckSize ) + 8 + (LE_SWAP32( &cur->ckSize ) & 1));

   S32 done = 0;

   while (!done)
      {
      //
      // Get size and address of next chunk
      //

      S32 size = LE_SWAP32( &cur->ckSize ) + 8 + (LE_SWAP32( &cur->ckSize ) & 1);

      CHUNK *nxt = (CHUNK *) AIL_ptr_add(cur, size);

      //
      // Process chunk
      //
      // If this chunk matches the address of the one to be removed, kill it
      //

      if (cur == chunk)
         {
         //
         // Remove chunk from memory image
         //

         AIL_memcpy(cur, nxt, end - (SINTa)(nxt));

         //
         // Adjust size of all higher-level directory chunks to compensate for
         // removed subchunk
         //

         while (c != chunk_stack)
            {
            STORE_LE_SWAP32( &c->c->ckSize, LE_SWAP32( &c->c->ckSize ) - size );
            --c;
            }

         //
         // Return # of bytes removed from file
         //

         return size;
         }

      //
      // If current chunk is a LIST or RIFF, save address of next chunk at
      // outer level, and descend into inner chunk
      //

      if ((cur->ckID == MSSFOURCC_LIST) || (cur->ckID == MSSFOURCC_RIFF))
         {
         c++;
         c->c = cur;
         c->n = nxt;

         cur = (CHUNK *)(void*) cur->data;
         continue;
         }

      //
      // Walk to next chunk at current level
      //

      cur = nxt;

      //
      // Next chunk = end of current chunk?  If so, pop back to outer
      // subchunk and continue traversal
      //

      while ((cur >= c->n) && (c != chunk_stack))
         {
         cur = c->n;
         c--;

         if (c == chunk_stack)
            {
            //
            // End of outermost chunk reached, exit
            //

            done = 1;
            break;
            }
         }
      }

   return 0;
}

// ---------------------------------------------------------------------------
// insert_chunk
//
// Inserts chunk at given address into RIFF file before specified chunk,
// returning # of bytes added to file
// ---------------------------------------------------------------------------

static S32 insert_chunk(CHUNK *insertion_point, void*form, CHUNK *incoming)
{
   CDESC chunk_stack[256];

   //
   // Traverse file until specified chunk found
   //

   CDESC *c = chunk_stack;

   c[0].n = c[0].c = NULL;

   CHUNK *cur = (CHUNK *) form;

   SINTa end = (SINTa)(form) + (LE_SWAP32( &cur->ckSize ) + 8 + (LE_SWAP32( &cur->ckSize ) & 1));

   S32 done = 0;

   while (!done)
      {
      //
      // Get size and address of next chunk
      //

      S32 size = LE_SWAP32( &cur->ckSize ) + 8 + (LE_SWAP32( &cur->ckSize ) & 1);

      CHUNK *nxt = (CHUNK *) AIL_ptr_add(cur, size);

      //
      // Process chunk
      //

      if (cur == insertion_point)
         {
         //
         // Make room for incoming chunk in memory image
         //

         S32 new_size = LE_SWAP32( &incoming->ckSize ) + 8 + (LE_SWAP32( &incoming->ckSize ) & 1);

         AIL_memmove(AIL_ptr_add(cur, new_size), cur, end - (SINTa)(cur));

         //
         // Copy chunk into memory image
         //

         AIL_memcpy(cur, incoming, LE_SWAP32( &incoming->ckSize ) + 8);

         //
         // Adjust size of all higher-level directory chunks to compensate for
         // added subchunk
         //

         while (c != chunk_stack)
            {
            STORE_LE_SWAP32( &c->c->ckSize, LE_SWAP32( &c->c->ckSize ) + new_size );
            --c;
            }

         //
         // Return # of bytes added to file
         //

         return new_size;
         }

      //
      // If current chunk is a LIST or RIFF, save address of next chunk at
      // outer level, and descend into inner chunk
      //

      if ((cur->ckID == MSSFOURCC_LIST) || (cur->ckID == MSSFOURCC_RIFF))
         {
         c++;
         c->c = cur;
         c->n = nxt;

         cur = (CHUNK *)(void*) cur->data;
         continue;
         }

      //
      // Walk to next chunk at current level
      //

      cur = nxt;

      //
      // Next chunk = end of current chunk?  If so, pop back to outer
      // subchunk and continue traversal
      //

      while ((cur >= c->n) && (c != chunk_stack))
         {
         cur = c->n;
         c--;

         if (c == chunk_stack)
            {
            //
            // End of outermost chunk reached, exit
            //

            done = 1;
            break;
            }
         }
      }

   return 0;
}

// ---------------------------------------------------------------------------
// replace_chunk
//
// Replace existing chunk, returning change in file image size
// ---------------------------------------------------------------------------

static S32 replace_chunk(CHUNK *target, void *form, CHUNK *incoming)
{
   CDESC chunk_stack[256];

   //
   // Traverse file until specified chunk found
   //

   CDESC *c = chunk_stack;

   c[0].n = c[0].c = NULL;

   CHUNK *cur = (CHUNK *) form;

   SINTa end = (SINTa)(form) + (LE_SWAP32( &cur->ckSize ) + 8 + ( LE_SWAP32( &cur->ckSize ) & 1));

   S32 done = 0;

   while (!done)
      {
      //
      // Get size and address of next chunk
      //

      S32 size = LE_SWAP32( &cur->ckSize ) + 8 + (LE_SWAP32( &cur->ckSize ) & 1);

      CHUNK *nxt = (CHUNK *) AIL_ptr_add(cur, size);

      //
      // Process chunk
      //

      if (cur == target)
         {
         //
         // Get # of bytes to add/subtract from ckSize parameter
         //

         S32 new_size = LE_SWAP32( &incoming->ckSize ) + 8 + (LE_SWAP32( &incoming->ckSize ) & 1);

         S32 delta = new_size - size;

         //
         // Resize memory image
         //

         AIL_memmove(AIL_ptr_add(cur, new_size), nxt, end - (SINTa)(nxt));

         //
         // Copy new chunk data
         //

         AIL_memcpy(cur, incoming, LE_SWAP32( &incoming->ckSize ) + 8);

         //
         // Adjust size of all higher-level directory chunks to compensate for
         // added subchunk
         //

         while (c != chunk_stack)
            {
            STORE_LE_SWAP32( &c->c->ckSize, LE_SWAP32( &c->c->ckSize ) + delta );
            --c;
            }

         //
         // Return # of bytes added to file
         //

         return delta;
         }

      //
      // If current chunk is a LIST or RIFF, save address of next chunk at
      // outer level, and descend into inner chunk
      //

      if ((cur->ckID == MSSFOURCC_LIST) || (cur->ckID == MSSFOURCC_RIFF))
         {
         c++;
         c->c = cur;
         c->n = nxt;

         cur = (CHUNK *)(void*) cur->data;
         continue;
         }

      //
      // Walk to next chunk at current level
      //

      cur = nxt;

      //
      // Next chunk = end of current chunk?  If so, pop back to outer
      // subchunk and continue traversal
      //

      while ((cur >= c->n) && (c != chunk_stack))
         {
         cur = c->n;
         c--;

         if (c == chunk_stack)
            {
            //
            // End of outermost chunk reached, exit
            //

            done = 1;
            break;
            }
         }
      }

   return 0;
}

//############################################################################
//##                                                                        ##
//## Read MIDI variable-length number                                       ##
//##                                                                        ##
//############################################################################

extern "C" U32 AILCALL XMI_read_VLN(U8 const * *ptr)
{
   U32 val,i,cnt;

   val = 0;
   cnt = 4;

   do
      {
      i = (U32) **ptr;

      *ptr=(U8*)AIL_ptr_add(*ptr,1);

      val = (val << 7) | (i & 0x7f);

      if (!(i & 0x80))
         {
         break;
         }
      }
   while (--cnt);

   return val;
}

//############################################################################
//##                                                                        ##
//## Return size in bytes of MIDI channel voice message, based on type      ##
//##                                                                        ##
//############################################################################

extern "C" S32 AILCALL XMI_message_size(S32 status)
{
   switch (status & 0xf0)
      {
      case EV_NOTE_OFF  :
      case EV_NOTE_ON   :
      case EV_POLY_PRESS:
      case EV_CONTROL   :
      case EV_PITCH     : return 3;

      case EV_PROGRAM   :
      case EV_CHAN_PRESS: return 2;
      }

   return 0;
}


//############################################################################
//##                                                                        ##
//## Find nth sequence in XMIDI IFF file                                    ##
//##                                                                        ##
//## Returns NULL if not found, else pointer to FORM XMID                   ##
//##                                                                        ##
//############################################################################

extern "C" void const * AILCALL XMI_find_sequence(U8 const *image, S32 sequence)
{
   U32     len;
   U8 const *end;

   len = 0;

   do
      {
      //
      // Skip previous block, if any
      //

      image += len;

      //
      // Exit if not FORM or CAT block
      //

      if ((AIL_strnicmp((char*)image,"FORM",4)) &&
          (AIL_strnicmp((char*)image,"CAT ",4)))
         {
         return NULL;
         }

      //
      // Continue searching if not FORM XMID or CAT XMID
      //
      // XMIDI files always have even FORM lengths; therefore, no
      // odd-byte compensation is needed
      //

      len = 8 + BEU_SWAP32( (U32 *) ((U8 *) (image+4)));
      }
   while (AIL_strnicmp((char*)image+8,"XMID",4));

   //
   // If outer header was a FORM, return successfully if first sequence
   // requested
   //
   // If second or higher sequence requested, return NULL if
   // single-sequence FORM encountered
   //

   if (!AIL_strnicmp((char*)image,"FORM",4))
      {
      if (sequence == 0)
         {
         return image;
         }
      else
         {
         return NULL;
         }
      }

   //
   // Calculate ending address of image, and skip 'CAT_nnnnXMID'
   // header to index first FORM chunk in CAT
   //

   end    = image + len;
   image += 12;

   //
   // Find nth FORM XMID chunk in outer CAT XMID block
   //

   while (image < end)
      {
      if (!AIL_strnicmp((char*)image+8,"XMID",4))
         {
         if (!sequence--)
            {
            return image;
            }
         }

      image += 8 + BEU_SWAP32( (U32 *) ((U8 *) (image+4)));
      }

   return NULL;
}

// ---------------------------------------------------------------------------
// compress
//
// If encoder == NULL, use standard IMA ADPCM encoding
// Otherwise, use specified ASI encoder to compress data
//
// ---------------------------------------------------------------------------

DXDEF
S32 AILEXPORT AIL_compress_DLS(void const* dls, //)
                               char const* compress_extension,
                               void** mls, U32* mlssize,
                               AILLENGTHYCB callback)
{

   //
   // find an encoder
   //

   comp_ASI = (compress_extension==0) ? 0 : find_ASI_provider("Output file types", compress_extension);

   //
   // Pass 1: Locate wave pool table in output file image
   //

   void* out;
   U32 orig_size,osize;

   POOLTABLE *ptbl = NULL;
   POOLCUE   *cues = NULL;

   CHUNK *c1 = (CHUNK *) dls;

   if (c1->subID != MSSFOURCC_DLS)
      {
      AIL_set_error("Not a DLS file.");
      return(0);
      }

   orig_size = osize = LE_SWAP32( &c1->ckSize ) + 8;

   out = AIL_mem_alloc_lock(osize);

   if (out==0) {
     return(0);
   }

   if (callback)
     if (callback(AIL_LENGTHY_INIT,0)==0) {
      cancelled:
       AIL_mem_free_lock(out);
       AIL_set_error("Compression cancelled.");
       return(0);
     }

   AIL_memcpy(out,c1,osize);

   c1 = (CHUNK *) out;
   c1->subID = MSSFOURCC_MLS;

   CHUNK *n1 = (CHUNK *) AIL_ptr_add(c1, LE_SWAP32( &c1->ckSize ) + 8 + (LE_SWAP32( &c1->ckSize ) & 1));
   CHUNK *c2 = (CHUNK *) (void*)c1->data;

   while (c2 < n1)
      {
      if (c2->ckID == MSSFOURCC_PTBL)
         {
         ptbl = (POOLTABLE *) (void*) /*&*/((SUBCHUNK *) c2)->data;
         cues = (POOLCUE   *) &((U8 *) (ptbl))[LE_SWAP32( &ptbl->cbSize )];
         break;
         }

      c2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
      }

   if (ptbl == 0)
      {
      AIL_set_error("No PTBL chunk found.");
      return(0);
      }

   //
   // Pass 2: Temporarily replace PTBL offsets with WAVE ordinal indexes
   //

   n1 = (CHUNK *) AIL_ptr_add(c1, LE_SWAP32( &c1->ckSize ) + 8 + (LE_SWAP32( &c1->ckSize ) & 1));
   c2 = (CHUNK *) (void*)c1->data;

   U32 nth_wave = 0;

   U32 n_wave_cues = LE_SWAP32( &ptbl->cCues ) & 0xffffff;
   U32 cues_so_far = 0;

   while (c2 < n1)
      {
      if ((c2->ckID == MSSFOURCC_LIST) && (c2->subID == MSSFOURCC_WVPL))
         {
         CHUNK *n2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
         CHUNK *c3 = (CHUNK *) (void*) c2->data;

         while (c3 < n2)
            {
            if ((c3->ckID == MSSFOURCC_LIST) && (c3->subID == MSSFOURCC_wave))
               {
               UINTa offset = ((UINTa) c3) - ((UINTa) c2->data);

               for (U32 index=0; index < n_wave_cues; index++)
                  {
                  if (LE_SWAP32( &cues[index].ulOffset ) == offset)
                     {
                     cues[index].ulOffset = nth_wave;
                     }
                  }

               ++nth_wave;
               }

            c3 = (CHUNK *)AIL_ptr_add(c3, LE_SWAP32( &c3->ckSize ) + 8 + (LE_SWAP32( &c3->ckSize ) & 1));
            }
         }

      c2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
      }

   //
   // Pass 3: Replace PCM chunks with compressed chunks
   //
   // Perform operation from end of file towards beginning to minimize time
   // spent moving memory blocks
   //

   for(;;)
      {
      CHUNK *old_fmt = NULL;
      CHUNK *old_data = NULL;

      CHUNK *c1 = (CHUNK *) out;

      CHUNK *n1 = (CHUNK *) AIL_ptr_add(c1, LE_SWAP32( &c1->ckSize ) + 8 + (LE_SWAP32( &c1->ckSize ) & 1));
      CHUNK *c2 = (CHUNK *) (void*) c1->data;

      CHUNK *chunk_list = NULL;

      while (c2 < n1)
         {
         if ((c2->ckID == MSSFOURCC_LIST) && (c2->subID == MSSFOURCC_WVPL))
            {
            CHUNK *n2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
            CHUNK *c3 = (CHUNK *) (void*)c2->data;

            while (c3 < n2)
               {
               if ((c3->ckID == MSSFOURCC_LIST) && (c3->subID == MSSFOURCC_wave))
                  {
                  CHUNK *pcm        = NULL;
                  CHUNK *found_data = NULL;
                  CHUNK *found_fmt  = NULL;
                  CHUNK *found_list = NULL;

                  CHUNK *n3 = (CHUNK *) AIL_ptr_add(c3, LE_SWAP32( &c3->ckSize ) + 8 + (LE_SWAP32( &c3->ckSize ) & 1));
                  CHUNK *c4 = (CHUNK *) (void*)c3->data;

                  found_list = c4;

                  while (c4 < n3)
                     {
                     if (c4->ckID == MSSFOURCC_DATA)
                        {
                        found_data = c4;
                        }
                     else if (c4->ckID == MSSFOURCC_FMT)
                        {
                        found_fmt = c4;
                        }
                     else if (c4->ckID == MSSFOURCC_PCM)
                        {
                        pcm = c4;
                        }

                     c4 = (CHUNK *) AIL_ptr_add(c4, LE_SWAP32( &c4->ckSize ) + 8 + (LE_SWAP32( &c4->ckSize ) & 1));
                     }

                  //
                  // If PCM chunk found, we've already processed this WAVE,
                  // so skip it
                  //

                  if (pcm == NULL)
                     {
                     old_data   = found_data;
                     old_fmt    = found_fmt;
                     chunk_list = found_list;
                     }
                  }

               c3 = (CHUNK *) AIL_ptr_add(c3, LE_SWAP32( &c3->ckSize ) + 8 + (LE_SWAP32( &c3->ckSize ) & 1));
               }
            }

         c2 = (CHUNK *)AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
         }

      //
      // Exit loop if no more LISTs to modify
      //

      if ((old_fmt == NULL) || (old_data == NULL) || (chunk_list == NULL))
         {
         break;
         }

      //
      // Rename old FMT chunk to PCM to preserve original format information
      //

      WAV_FMT PCM[2];
      AIL_memcpy(PCM, old_fmt,LE_SWAP32( &((WAV_FMT*)old_fmt)->chunk_size ) + 8);  // use two for extra words
      AIL_memcpy(PCM[0].FMT_string,"pcm ",4);

      //
      // Generate new data, format, and fact chunks for compressed data
      //

      void* new_ptr;

      AILSOUNDINFO info;
      info.format=WAVE_FORMAT_PCM;
      info.data_ptr=((SUBCHUNK*)old_data)->data;
      info.data_len=LE_SWAP32( &((SUBCHUNK*)old_data)->ckSize );
      info.rate=LE_SWAP32( &PCM->sample_rate );
      info.bits=LE_SWAP16( &PCM->bits_per_sample );
      info.channels=LE_SWAP16( &PCM->channels );
      info.block_size=LE_SWAP16( &PCM->alignment );

      ++cues_so_far;

      if (callback) {
        F32 per=(F32)(cues_so_far*100)/(F32)n_wave_cues;
        if (per>100.0F)
          per=100.0F;

        if (callback(AIL_LENGTHY_UPDATE,*(U32*)(void*)&per)==0)
          goto cancelled;
      }

      if ((comp_ASI == 0) || ((info.data_len/LE_SWAP16( &PCM->alignment ) ) < (U32) AIL_get_preference(DLS_ADPCM_TO_ASI_THRESHOLD)))
         {
         if (AIL_compress_ADPCM(&info,&new_ptr,0)==0)
            {
            AIL_mem_free_lock(out);
            return(0);
            }
         }
      else
         {

         //
         // Compress data with specified ASI provider
         //

         U32 out_len = 0;

         if (compress_ASI(&info,&new_ptr,&out_len,callback)==0)
            {
            AIL_mem_free_lock((void*)info.data_ptr);
            AIL_mem_free_lock(out);
            return(0);
            }

         //
         // Prepend ASIOUT file header structure to data
         //
         MSS_STRUCT ASIOUT
            {
            U32 riffmark;
            U32 rifflen;
            U32 wavemark;
            U32 fmtmark;
            U32 fmtlen;
            U16 fmttag;
            U16 channels;
            U32 sampersec;
            U32 avepersec;
            U16 blockalign;
            U16 bitspersam;
            S16 extra;
            C8  compressed_suffix[4];  // was S16 samples_per_block
            U32 factmark;
            U32 factlen;
            U32 samples;
            U32 datamark;
            U32 datalen;
            };

         ASIOUT *adp = (ASIOUT *)
            AIL_mem_alloc_lock(sizeof(ASIOUT) + out_len + 4);

         if (adp == NULL)
            {
            AIL_mem_free_lock(out);
            return(0);
            }

         AIL_memcpy(AIL_ptr_add(adp, sizeof(ASIOUT)),
                    new_ptr,
                    out_len);

         AIL_mem_free_lock(new_ptr);

         AIL_memcpy(&adp->riffmark,"RIFF",4);
         adp->rifflen=sizeof(ASIOUT)-8;
         AIL_memcpy(&adp->wavemark,"WAVE",4);
         AIL_memcpy(&adp->fmtmark,"fmt ",4);
         adp->fmtlen=22;   // was 20
         adp->fmttag=WAVE_FORMAT_UNKNOWN;
         adp->channels=(U16)info.channels;
         adp->sampersec=info.rate;
         adp->avepersec=info.rate * (info.bits/8) * info.channels;   // orig, not output
         adp->blockalign=1;
         adp->bitspersam=0;
         adp->extra=4;     // was 2
         AIL_memcpy(&adp->factmark,"fact",4);
         adp->factlen=4;
         adp->samples = (info.data_len / (info.bits/8)) / info.channels ;
         AIL_memcpy(&adp->datamark,"data",4);
         adp->datalen = out_len;
         adp->rifflen += adp->datalen;

         AIL_strcpy(adp->compressed_suffix,"MP3");

         new_ptr = adp;
         }

      IMA_FMT* IMA_fmt=(IMA_FMT*)(void*)((CHUNK*)new_ptr)->data;
      IMA_FACT* IMA_fact=(IMA_FACT *) AIL_ptr_add( IMA_fmt, LE_SWAP32( &IMA_fmt->chunk_size ) + 8 );
      CHUNK* new_data=(CHUNK *) AIL_ptr_add( IMA_fact, LE_SWAP32( &IMA_fact->chunk_size ) + 8 );

      //
      // Replace DATA chunk with new ADCPM copy
      //

      osize += replace_chunk(old_data, out, (CHUNK *) new_data);

      //
      // Remove old FMT chunk from list
      //

      osize -= remove_chunk(old_fmt, out);

      //
      // Insert new FACT and FMT chunks at beginning of chunk list
      //

      osize += insert_chunk(chunk_list, out, (CHUNK *) IMA_fact);
      osize += insert_chunk(chunk_list, out, (CHUNK *) IMA_fmt);

      AIL_mem_free_lock(new_ptr);

      //
      // Re-insert old FMT chunk as PCM chunk at beginning of list
      //

      osize += insert_chunk(chunk_list, out, (CHUNK *)(void*) PCM);

      }

   //
   // Pass 4: Replace PTBL ordinals with offsets to new WAVEs
   //

   c1 = (CHUNK *) out;

   n1 = (CHUNK *) AIL_ptr_add(c1, LE_SWAP32( &c1->ckSize ) + 8 + (LE_SWAP32( &c1->ckSize ) & 1));
   c2 = (CHUNK *) (void*) c1->data;

   nth_wave = 0;

   while (c2 < n1)
      {
      if ((c2->ckID == MSSFOURCC_LIST) && (c2->subID == MSSFOURCC_WVPL))
         {
         CHUNK *n2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
         CHUNK *c3 = (CHUNK *) (void*)c2->data;

         while (c3 < n2)
            {
            if ((c3->ckID == MSSFOURCC_LIST) && (c3->subID == MSSFOURCC_wave))
               {
               U32 offset = (U32) (((UINTa) c3) - ((UINTa) c2->data));

               for (U32 index=0; index < n_wave_cues; index++)
                  {
                  if (cues[index].ulOffset == nth_wave)
                     {
                     cues[index].ulOffset = LE_SWAP32( &offset );
                     }
                  }

               ++nth_wave;
               }

            c3 = (CHUNK *) AIL_ptr_add(c3, LE_SWAP32( &c3->ckSize ) + 8 + (LE_SWAP32( &c3->ckSize ) & 1));
            }
         }

      c2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
      }

  if (mlssize)
    *mlssize=osize;

  //
  // If ASI encoder used, store overall file compression ratio, rounded up,
  // in high byte of ptbl->cCues
  //
  // Otherwise, we leave the high byte clear, for a default (ADPCM) ratio of
  // 4:1.  This ensures that ADPCM-compressed MLS files remain compatible
  // with third-party ADPCM-compatible DLS tools.
  //

  if (comp_ASI != 0)
   {
   U32 ratio = ((orig_size + (osize / 2)) / osize) + 1;

   STORE_LE_SWAP32( &ptbl->cCues, (LE_SWAP32( &ptbl->cCues ) & 0x00ffffff) | (ratio << 24) );
   }

  if (mls)
    *mls=out;
  else
    AIL_mem_free_lock(out);

   if (callback)
     callback(AIL_LENGTHY_DONE,0);

  return(1);
}


DXDEF
S32 AILEXPORT AIL_filter_DLS_with_XMI(void const* xmiptr, void const* dls,
                                      void** dlsout, U32* dlssize,
                                      S32 flags, AILLENGTHYCB callback)
{
  //
  // Compile list of patch numbers used (independent of bank #s) by any/all
  // sequences in file
  //

  if ((xmiptr==0) || (dls==0))
    return(0);

  if ((((CHUNK*)dls)->subID != MSSFOURCC_DLS) && (((CHUNK*)dls)->subID != MSSFOURCC_MLS))
     {
     AIL_set_error("Not a DLS file.");
     return(0);
     }


  void* dlsdata=AIL_mem_alloc_lock( LE_SWAP32( &((CHUNK*)dls)->ckSize ) + 8);
  if (dlsdata==0)
     {
    err:
     return(0);
     }

  S32 patch[16];
  AIL_memset(patch, 0, 16*sizeof(S32));

  U8 patch_scoreboard[128];
  AIL_memset(patch_scoreboard, 0, 128);

  U8* region_scoreboard=(U8*)AIL_mem_alloc_lock((128L*128L*1)+(128L*128L*1));

  if (region_scoreboard==0) {
   err1:
    AIL_mem_free_lock(dlsdata);
    goto err;
  }

  AIL_memset(region_scoreboard, 0, 128*128);

  U8 drum_scoreboard[128];
  AIL_memset(drum_scoreboard, 0, 128);

  U8* bank_scoreboard=((U8*)region_scoreboard)+(128*128*1);
  AIL_memset(bank_scoreboard, 0, 128*128);

  S32 bank_l[128];
  AIL_memset(bank_l, 0, 128*sizeof(S32));

  S32 bank_h[128];
  AIL_memset(bank_h, 0, 128*sizeof(S32));

  S32 rpn_l[128];
  AIL_memset(rpn_l, 0, 128*sizeof(S32));

  S32 rpn_h[128];
  AIL_memset(rpn_h, 0, 128*sizeof(S32));

  S32 coarse_tune[128];
  AIL_memset(coarse_tune, 0, 128*sizeof(S32));

  U32 numxmis;
  UINTa localxmis[1];

  void** xmis;

  if (flags&AILFILTERDLS_USINGLIST) {
    numxmis=*((U32*)xmiptr);
    xmis=(void**)AIL_ptr_add(xmiptr,4);
  } else {
    numxmis=1;
    xmis=(void**)(void*)localxmis;
    localxmis[0]=(UINTa)xmiptr;
  }


  if (callback)
    if (callback(AIL_LENGTHY_INIT,0)==0) {
     cancelled:
      AIL_mem_free_lock(dlsdata);
      AIL_set_error("Filtering cancelled.");
      return(0);
    }


  while (numxmis--) {
    void* curxmi=xmis[numxmis];

    if ((AIL_strnicmp((char*) curxmi, "FORM", 4)) ||
        (AIL_strnicmp(((char*) curxmi)+8, "XDIR", 4)))
      {
      AIL_set_error("Not an XMI file.");
      return(0);
      }

    S32 XMI_cnt = 0;

    U8 const *image;

    AIL_memcpy(dlsdata,dls,LE_SWAP32( &((CHUNK*)dls)->ckSize ) + 8);

    while ((image = (U8*)XMI_find_sequence((U8*)curxmi,XMI_cnt)) != NULL)
       {
       //
       // Locate EVNT chunk within FORM XMID
       //

       U32     len = 8 + BEU_SWAP32( ((U8 *) (image+4)));
       U8 const *end = image + len;

       image += 12;

       while (image < end)
          {
          if (!AIL_strnicmp((char*)image,"EVNT",4))
             {
             //
             // Walk through all events in EVNT chunk, keeping track of
             // instrument regions used
             //

             U8 const *ptr = (U8 *) image + 8;

             S32 done = 0;

             while (!done)
                {
                S32 status = *ptr;
                S32 channel,type,len;

                if (status < 0x80)
                   {
                   //
                   // Skip delta time interval byte
                   //

                   ++ptr;
                   continue;
                   }

                switch (status)
                   {
                   //
                   // Process MIDI meta-event, checking for end of sequence
                   //

                   case EV_META:

                      type = ptr[1];

                      ptr += 2;
                      len = XMI_read_VLN(&ptr);

                      if (type == META_EOT)
                         {
                         done = 1;
                         }
                      else
                         {
                         ptr += len;
                         }
                      break;

                   //
                   // Skip MIDI System Exclusive message
                   //

                   case EV_SYSEX:
                   case EV_ESC:

                      ptr += 1;

                      len = XMI_read_VLN(&ptr);

                      ptr += len;
                      break;

                   //
                   // Process MIDI channel voice message
                   //

                   default:

                      channel = status & 0x0f;
                      status  = status & 0xf0;

                      switch (status)
                         {
                         case EV_PROGRAM:

                            //
                            // Log patch # for this channel
                            //

                            patch[channel] = ptr[1];
                            break;

                         case EV_CONTROL:

                            switch (ptr[1])
                               {
                               case GM_BANK_MSB:
                                  bank_h[channel] = ptr[2];
                                  break;

                               case GM_BANK_LSB:
                                  bank_l[channel] = ptr[2];
                                  break;

                               case PATCH_BANK_SEL:
                                  bank_l[channel] = ptr[2];
                                  bank_h[channel] = 0;
                                  break;

                               case RPN_LSB:
                                  rpn_l[channel] = ptr[2];
                                  break;

                               case RPN_MSB:
                                  rpn_h[channel] = ptr[2];
                                  break;

                               case DATA_MSB:
                                  if ((rpn_l[channel] == 2) && (rpn_h[channel] == 0))
                                     {
                                     coarse_tune[channel] = (S32)(ptr[2]) - 64;
                                     }
                                  break;
                               }
                            break;

                         case EV_NOTE_ON:
                            {
                            //
                            // Log drum or melodic key event
                            //

                            S32 key = ptr[1];

                            if (channel == PERCUSS_CHAN)
                               {
                               drum_scoreboard[key] = 1;
                               }
                            else
                               {
                               //
                               // Modify key # by coarse tuning value for
                               // this channel
                               //

                               key += coarse_tune[channel];

                               if (key < 0)   key = 0;
                               if (key > 127) key = 127;

                               region_scoreboard[patch[channel]*128+key] = 1;
                               }

                            patch_scoreboard[patch[channel]] = 1;
                            bank_scoreboard[(bank_h[channel] << 7) + bank_l[channel]] = 1;
                            break;
                            }
                         }

                      //
                      // Advance past channel-voice message
                      //

                      ptr += XMI_message_size(status);

                      //
                      // If this was EV_NOTE_ON, advance past duration word
                      //

                      if (status == EV_NOTE_ON)
                         {
                         XMI_read_VLN(&ptr);
                         }

                      break;
                   }
                }
             }

          image += 8 + BEU_SWAP32(((U8 *) (image+4)));
          }

       ++XMI_cnt;
       }
    }

  //
  // Find COLH collection header in DLS file
  //

  S32 *ins_cnt = NULL;

  CHUNK* c1 = (CHUNK *) dlsdata;

  CHUNK *c2 = (CHUNK *) (void*)c1->data;

  while (c2 < (CHUNK *) AIL_ptr_add(c1, LE_SWAP32( &c1->ckSize ) + 8 + (LE_SWAP32( &c1->ckSize ) & 1)))
     {
     if (c2->ckID == MSSFOURCC_COLH)
        {
        ins_cnt = (S32 *) (void*)/*&*/((SUBCHUNK *) c2)->data;
        }

     c2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
     }

  //
  // Find PTBL chunk (list of WAVE offsets) and allocate a matching array
  // to keep track of which WAVE entries are used by the set of
  // instruments needed to play the XMIDI file
  //

  POOLTABLE *ptbl = NULL;
  POOLCUE   *cues = NULL;

  c1 = (CHUNK *) dlsdata;
  c2 = (CHUNK *) (void*) c1->data;

  while (c2 < (CHUNK *) AIL_ptr_add(c1, LE_SWAP32( &c1->ckSize ) + 8 + (LE_SWAP32( &c1->ckSize ) & 1)))
     {
     if (c2->ckID == MSSFOURCC_PTBL)
        {
        ptbl = (POOLTABLE *) (void*) /*&*/((SUBCHUNK *) c2)->data;
        cues = (POOLCUE   *) &((U8 *) (ptbl))[LE_SWAP32( &ptbl->cbSize ) ];
        break;
        }

     c2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
     }

  U32 n_wave_cues = LE_SWAP32( &ptbl->cCues ) & 0xffffff;
  U32 cues_so_far=0;

  U8 *wave_scoreboard = (U8 *) AIL_mem_alloc_lock(n_wave_cues);

  if (wave_scoreboard == NULL)
     {
     AIL_mem_free_lock(region_scoreboard);
     goto err1;
     }

  AIL_memset(wave_scoreboard, 0, n_wave_cues);

  //
  // Find LINS instrument list chunk in DLS file
  //

  c1 = (CHUNK *) dlsdata;
  c2 = (CHUNK *) (void*)c1->data;

  while (c2 < (CHUNK *) AIL_ptr_add(c1, LE_SWAP32( &c1->ckSize ) + 8 + (LE_SWAP32( &c1->ckSize ) & 1)))
     {
     //
     // Look for outer LINS (instrument list) chunk
     //

     if ((c2->ckID == MSSFOURCC_LIST) && (c2->subID == MSSFOURCC_LINS))
        {
        CHUNK *c3 = (CHUNK *)(void*) c2->data;

        //
        // For each INS (instrument) chunk in instrument list...
        //

        while (c3 < (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1)))
           {
           if ((c3->ckID == MSSFOURCC_LIST) && (c3->subID == MSSFOURCC_INS))
              {
              S32        remove = 0;
              S32        drum_kit = 0;
              INSTHEADER header;

              header.Locale.ulInstrument = 0;
              
              ++cues_so_far;

              if (callback) {
                F32 per=(F32)(cues_so_far*50)/(F32)n_wave_cues;
                if (per>50.0F)
                  per=50.0F;

                if (callback(AIL_LENGTHY_UPDATE,*(U32*)(void*)&per)==0)
                  goto cancelled;
              }

              //
              // Look for INSH instrument header to determine patch
              // assignment
              //

              CHUNK *c4 = (CHUNK *) (void*)c3->data;

              while (c4 < (CHUNK *) AIL_ptr_add(c3, LE_SWAP32( &c3->ckSize ) + 8 + (LE_SWAP32( &c3->ckSize ) & 1)))
                 {
                 if (c4->ckID == MSSFOURCC_INSH)
                    {
                    header = *(INSTHEADER *) (void*)(((SUBCHUNK *) c4)->data);
                    MEM_LE_SWAP32( &header.cRegions );
                    MEM_LE_SWAP32( &header.Locale.ulBank );
                    MEM_LE_SWAP32( &header.Locale.ulInstrument );

                    header.Locale.ulBank = (header.Locale.ulBank &  0x8000007f) | 
                                          ((header.Locale.ulBank & ~0x8000007f) >> 1);

                    //
                    // Flag this instrument for removal if either its
                    // bank or its patch is not referenced by the XMIDI
                    // file
                    //
                    // Instruments are also removed if their
                    // bank #s are never referenced in the XMIDI file
                    //

                    drum_kit = 0;

                    if (header.Locale.ulBank & F_INSTRUMENT_DRUMS)
                       {
                       drum_kit = 1;
                       }

                    if (!patch_scoreboard[header.Locale.ulInstrument])
                       {
                       remove = 1;
                       }

                    if (!bank_scoreboard[header.Locale.ulBank & 0x3fff])
                       {
                       remove = 1;
                       }
                    }

                 c4 = (CHUNK *) AIL_ptr_add(c4, LE_SWAP32( &c4->ckSize ) + 8 + (LE_SWAP32( &c4->ckSize ) & 1));
                 }

              //
              // If this instrument is flagged for removal, excise it from
              // the DLS file memory image and continue
              //

              if (remove)
                 {
                 remove_chunk(c3, dlsdata);

                 if (ins_cnt != NULL)
                    {
                    STORE_LE_SWAP32( ins_cnt, LE_SWAP32( ins_cnt ) - 1 );
                    }

                 continue;
                 }

              //
              // Otherwise, if the instrument was NOT flagged for removal,
              // flag wave pool table entries referenced by all of its
              // regions as "in use"
              //

              c4 = (CHUNK *)(void*) c3->data;

              while (c4 < (CHUNK *) AIL_ptr_add(c3, LE_SWAP32( &c3->ckSize ) + 8 + (LE_SWAP32( &c3->ckSize ) & 1)))
                 {
                 if ((c4->ckID == MSSFOURCC_LIST) && (c4->subID == MSSFOURCC_LRGN))
                    {
                    CHUNK *c5 = (CHUNK *) (void*)c4->data;

                    //
                    // For each region in instrument...
                    //

                    while (c5 < (CHUNK *) AIL_ptr_add(c4, LE_SWAP32( &c4->ckSize ) + 8 + (LE_SWAP32( &c4->ckSize ) & 1)))
                       {
                       if ((c5->ckID == MSSFOURCC_LIST) && (c5->subID == MSSFOURCC_RGN))
                          {
                          S32 keep  = 0;
                          //S32 index = -1;

                          //
                          // Keep only the wave pool entries from regions
                          // which are needed to play the XMIDI file
                          //

                          CHUNK *c6 = (CHUNK *)(void*) c5->data;

                          while (c6 < (CHUNK *) AIL_ptr_add(c5, LE_SWAP32( &c5->ckSize ) + 8 + (LE_SWAP32( &c5->ckSize ) & 1)))
                             {
                             if (c6->ckID == MSSFOURCC_RGNH)
                                {
                                RGNHEADER *region = (RGNHEADER *) (void*)(((SUBCHUNK *) c6)->data);

                                for (S32 i=(S32)LE_SWAP16( &region->RangeKey.usLow ); i <= (S32)LE_SWAP16( &region->RangeKey.usHigh ); i++)
                                   {
                                   if (drum_kit)
                                      {
                                      if (drum_scoreboard[i])
                                         {
                                         keep = 1;
                                         break;
                                         }
                                      }
                                   else
                                      {
                                      if (region_scoreboard[header.Locale.ulInstrument*128+i])
                                         {
                                         keep = 1;
                                         break;
                                         }
                                      }
                                   }
                                }

                             c6 = (CHUNK *) AIL_ptr_add(c6, LE_SWAP32( &c6->ckSize ) + 8 + (LE_SWAP32( &c6->ckSize ) & 1));
                             }

                          if (keep)
                             {
                             //
                             // If we are keeping this region's WAVE data,
                             // look for the region's WLNK chunk and mark
                             // its wave link index for preservation
                             //

                             CHUNK *c6 = (CHUNK *)(void*) c5->data;

                             while (c6 < (CHUNK *) AIL_ptr_add(c5, LE_SWAP32( &c5->ckSize ) + 8 + (LE_SWAP32( &c5->ckSize ) & 1)))
                                {
                                if (c6->ckID == MSSFOURCC_WLNK)
                                   {
                                   WAVELINK *link = (WAVELINK *) (void*)(((SUBCHUNK *) c6)->data);

                                   wave_scoreboard[LE_SWAP32( &link->ulTableIndex )] = 1;
                                   }

                                c6 = (CHUNK *) AIL_ptr_add(c6, LE_SWAP32( &c6->ckSize ) + 8 + (LE_SWAP32( &c6->ckSize ) & 1));
                                }
                             }

                          }

                       c5 = (CHUNK *) AIL_ptr_add(c5, LE_SWAP32( &c5->ckSize ) + 8 + (LE_SWAP32( &c5->ckSize ) & 1));
                       }
                    }

                 c4 = (CHUNK *) AIL_ptr_add(c4, LE_SWAP32( &c4->ckSize ) + 8 + (LE_SWAP32( &c4->ckSize ) & 1));
                 }
              }

           c3 = (CHUNK *) AIL_ptr_add(c3, LE_SWAP32( &c3->ckSize ) + 8 + (LE_SWAP32( &c3->ckSize ) & 1));
           }
        }

     c2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
     }

  //
  // PTBL chunk has probably been moved -- find it again
  //

  c1 = (CHUNK *) dlsdata;
  c2 = (CHUNK *)(void*) c1->data;

  while (c2 < (CHUNK *) AIL_ptr_add(c1, LE_SWAP32( &c1->ckSize ) + 8 + (LE_SWAP32( &c1->ckSize ) & 1)))
     {
     if (c2->ckID == MSSFOURCC_PTBL)
        {
        ptbl = (POOLTABLE *) (void*)/*&*/((SUBCHUNK *) c2)->data;
        cues = (POOLCUE   *) &((U8 *) (ptbl))[ LE_SWAP32( &ptbl->cbSize ) ];
        break;
        }

     c2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
     }

  n_wave_cues = LE_SWAP32( &ptbl->cCues ) & 0xffffff;

  //
  // Set pool table offsets for all unused WAVEs to 0
  //

  for (U32 index=0; index < n_wave_cues; index++)
     {
     if (!wave_scoreboard[index])
        {
        cues[index].ulOffset = 0;
        }
     }

  cues_so_far=0;

  //
  // Remove DATA chunks from all LIST WAVE entries in the WVPL list
  // which are not used by any referenced instrument regions
  //

  c1 = (CHUNK *) dlsdata;
  c2 = (CHUNK *) (void*)c1->data;

  while (c2 < (CHUNK *) AIL_ptr_add(c1, LE_SWAP32( &c1->ckSize ) + 8 + (LE_SWAP32( &c1->ckSize ) & 1)))
     {
     if ((c2->ckID == MSSFOURCC_LIST) && (c2->subID == MSSFOURCC_WVPL))
        {
        CHUNK *c3 = (CHUNK *)(void*) c2->data;

        while (c3 < (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1)))
           {
           if ((c3->ckID == MSSFOURCC_LIST) && (c3->subID == MSSFOURCC_wave))
              {
              UINTa offset = ((UINTa) c3) - ((UINTa) c2->data);

              U32 index;
              for (index=0; index < n_wave_cues; index++)
                 {
                 if (LE_SWAP32( &cues[index].ulOffset ) == offset)
                    {
                    break;
                    }
                 }

              ++cues_so_far;

              if (callback) {
                F32 per=((F32)(cues_so_far*50)/(F32)n_wave_cues)+50.0F;
                if (per>100.0F)
                  per=100.0F;

                if (callback(AIL_LENGTHY_UPDATE,*(U32*)(void*)&per)==0)
                  goto cancelled;
              }

              //
              // If this WAVE is not referenced by any surviving
              // instruments, remove it, and adjust all subsequent pool
              // table offsets accordingly
              //

              if (index == n_wave_cues)
                 {
                 S32 delta = remove_chunk(c3, dlsdata);

                 for (index=0; index < n_wave_cues; index++)
                    {
                    if (wave_scoreboard[index] &&
                       (LE_SWAP32( &cues[index].ulOffset ) > offset))
                       {
                       STORE_LE_SWAP32( &cues[index].ulOffset, LE_SWAP32( &cues[index].ulOffset ) - delta );
                       }
                    }

                 continue;
                 }

              //
              // Show progress indicator
              //

              }

           c3 = (CHUNK *) AIL_ptr_add(c3, LE_SWAP32( &c3->ckSize ) + 8 + (LE_SWAP32( &c3->ckSize ) & 1));
           }
        }

     c2 = (CHUNK *) AIL_ptr_add(c2, LE_SWAP32( &c2->ckSize ) + 8 + (LE_SWAP32( &c2->ckSize ) & 1));
     }

  AIL_mem_free_lock(region_scoreboard);
  AIL_mem_free_lock(wave_scoreboard);

  if (dlssize)
    *dlssize=LE_SWAP32( &((CHUNK*)dlsdata)->ckSize ) + 8;

  if (dlsout)
    *dlsout=dlsdata;
  else
    AIL_mem_free_lock(dlsdata);

  if (callback)
    callback(AIL_LENGTHY_DONE,0);

  return(1);
}


//############################################################################
//##                                                                        ##
//## Compress with ASI codec                                                ##
//##                                                                        ##
//############################################################################

DXDEF S32 AILEXPORT AIL_compress_ASI(AILSOUNDINFO const* info, //)
                                     char const* filename_ext,
                                     void** outdata,
                                     U32* outsize,
                                     AILLENGTHYCB callback)
{
   //
   // ASI functions
   //

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

   RIB_INTERFACE_ENTRY ASISTR1[] =
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

   comp_ASI = (filename_ext==0) ? 0 : find_ASI_provider("Output file types",
                                                         filename_ext);

   if (comp_ASI==0) {
     AIL_set_error("No codec found for requested output type.");
     return(0);
   }

   if (callback)
     if (callback(AIL_LENGTHY_INIT,0)==0) {
      cancelled:
       AIL_set_error("Compression cancelled.");
       return(0);
     }

   RIB_request(comp_ASI,"ASI codec",ASICODEC);
   RIB_request(comp_ASI,"ASI stream",ASISTR1);
   RIB_request(comp_ASI,"ASI stream",ASISTR2);
   RIB_request(comp_ASI,"ASI stream",ASISTR3);
   RIB_request(comp_ASI,"ASI stream",ASISTR4);
   RIB_request(comp_ASI,"ASI stream",ASISTR5);
   RIB_request(comp_ASI,"ASI stream",ASISTR6);
   RIB_request(comp_ASI,"ASI stream",ASISTR7);

   ASI_mem_src_ptr = (U8 *) info->data_ptr;
   ASI_mem_src_len = info->data_len;

   U32 use_rate=info->rate;
   U8* new_ptr=0;

   if (info->format==WAVE_FORMAT_IMA_ADPCM) {
     AILMIXINFO mix;

    convert_or_resample:
     AIL_memcpy(&mix.Info,info,sizeof(mix.Info));

     S32 mssf=((info->channels==2)?DIG_F_STEREO_MASK:0)|((info->bits==16)?DIG_F_16BITS_MASK:0);
     U32 len=AIL_size_processed_digital_audio(use_rate,mssf,1,&mix);

     new_ptr=(U8*)AIL_mem_alloc_lock(len);
     if (new_ptr==0)
       return(0);

     len=AIL_process_digital_audio(new_ptr,len,use_rate,mssf,1,&mix);

     if (len==0) {
       AIL_mem_free_lock(new_ptr);
       return(0);
     }

     ASI_mem_src_ptr=(U8*)new_ptr;
     ASI_mem_src_len=len;
   }

   //
   // Open stream with codec, registering callback function
   //

   ASI_mem_src_pos = 0;

   comp_stream = ASI_stream_open((MSS_ALLOC_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_alloc_info ), 
                                 (MSS_FREE_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_free_info ),
                                 0,
                                 (AILASIFETCHCB) MSS_CALLBACK_ALIGNED_NAME( ASI_mem_stream_CB ),
                                 ASI_mem_src_len);

   if (comp_stream == 0)
      {
      AIL_set_error(ASI_error());
     err:
      if (new_ptr)
        AIL_mem_free_lock(new_ptr);
      return 0;
      }

   //
   // Set source stream parameters
   //

   ASI_stream_property(comp_stream,
                       RAW_RATE,
                       0,&use_rate,0);

   
   ASI_stream_property(comp_stream,
                       RAW_BITS,
                       0,&info->bits,0);

   ASI_stream_property(comp_stream,
                       RAW_CHANNELS,
                       0,&info->channels,0);

   if (callback)
     if (callback(AIL_LENGTHY_SET_PROPERTY,(UINTa)compress_setprop)==0) {
       ASI_stream_close(comp_stream);
       comp_stream=0;
       if (new_ptr)
         AIL_mem_free_lock(new_ptr);
       goto cancelled;
     }


   //
   // Allocate an output memory dump
   //

   HMEMDUMP mem=AIL_mem_create();

   if (mem == NULL)
      {
      ASI_stream_close(comp_stream);
      comp_stream=0;
      goto err;
      }

   //
   // Process input data
   //

   U8 temp_buf[1024];
   S32 len;

   len=ASI_stream_process(comp_stream,temp_buf,1024);

   U32 codec_rate = (U32)~0;
   ASI_stream_property(comp_stream,OUTPUT_SAMPLE_RATE,&codec_rate,0,0);
   
   if ( codec_rate != use_rate ) 
   {
     use_rate=codec_rate;
     ASI_stream_close(comp_stream);
     comp_stream=0;
     AIL_mem_close(mem,0,0);
     if (new_ptr)
       AIL_mem_free_lock(new_ptr);
     goto convert_or_resample;
   }

   S32 cancel=0;

   while ((len!=0) && (cancel==0))
      {
      if (callback)
      {
        F32 percent=0.0f;
        ASI_stream_property(comp_stream,PERCENT_DONE, &percent,0,0);
        
        if (callback(AIL_LENGTHY_UPDATE,*(UINTa*)(void*)&percent)==0)
          cancel=1;
      }
      AIL_mem_write(mem,temp_buf,len);
      len=ASI_stream_process(comp_stream, temp_buf,1024);
      }

   if ( !AIL_mem_close(mem,outdata,outsize) )
     AIL_set_error( "Out of memory." );

   ASI_stream_close(comp_stream);
   comp_stream=0;

   if (new_ptr)
     AIL_mem_free_lock(new_ptr);

   if (callback)
     if (callback(AIL_LENGTHY_DONE,0)==0)
       cancel=1;

   if (cancel)
     goto cancelled;

   return( 1 );
}


//############################################################################
//#                                                                          #
//# Acquire bitfield data of length n from header buffer, n <= 24            #
//#                                                                          #
//# Bit position 0 is MSB of byte 0                                          #
//#                                                                          #
//# Request for 0 bits is considered valid, returning 0                      #
//#                                                                          #
//############################################################################

#define H(n) (n ? read_bits(header_buffer, &ES->hpos, (n)) : 0)

#ifdef IS_LE

inline U32 fetch_U32(U8 *&p) 
{
   U32 val = ((U32)(p[3])) + (((U32)(p[2])) << 8) + (((U32)(p[1])) << 16) + (((U32)(p[0])) << 24);  

   p += 4;

   return val;
}

inline U32 read_bits(U8 *data, S32 *bitpos, S32 n)
{
   U32 val;

   S32     b = *bitpos;
   U8 *p = &data[b >> 3];

   val = ((U32)(p[3]))        +
        (((U32)(p[2])) << 8)  +
        (((U32)(p[1])) << 16) +
        (((U32)(p[0])) << 24);

   val <<= b & 7;
   val >>= 32 - n;

   *bitpos = b + n;

   return val;
}

#else

inline U32 fetch_U32(U8 *&p) 
{
   U32 val = *(U32 *) p;

   p += 4;

   return val;
}

U32 inline read_bits(U8 *data, S32 *bitpos, S32 n)
{
   U32 val;
   S32     b = *bitpos;

   val = *(U32*)&data[b>>3];

   val <<= b & 7;
   val >>= 32 - n;

   *bitpos = b + n;

   return val;
}

#endif

//
// Bit rate in bits/second for [MPEG version][value]
//

static const S32 MPEG_bit_rate[2][15] =
{
   { 0L,8000L,16000L,24000L,32000L,40000L,48000L,56000L,64000L,80000L,96000L,112000L,128000L,144000L,160000L         }
   ,
   { 0L,32000L,40000L,48000L,56000L,64000L,80000L,96000L,112000L,128000L,160000L,192000L,224000L,256000L,320000L     }
};

//
// Sample rate in samples/second for [MPEG25][MPEG version][value]
//

static const S32 MPEG_sample_rate[2][2][4] =
{{
   { 22050L,24000L,16000L,22050L },
   { 44100L,48000L,32000L,44100L }
},
{
   { 11025L,12000L, 8000L,11025L },
   { 44100L,48000L,32000L,44100L }
}};

//############################################################################
//##                                                                        ##
//## MP3 inspection                                                         ##
//##                                                                        ##
//############################################################################

#define FRAMES_FLAG    1
#define BYTES_FLAG     2
#define TOC_FLAG       4
#define VBR_SCALE_FLAG 8
#define NUMTOCENTRIES  100

DXDEF 
void AILEXPORT AIL_inspect_MP3 (MP3_INFO *ES, //)
                              U8       *file_image, 
                              S32           file_size)
{
   //
   // Initialize the MP3 inspection-state structure and adjust 
   // the beginning and end of the image to account for ID3 headers
   //

   AIL_memset(ES, 
              0,
              sizeof(*ES));

   ES->MP3_file_image = file_image;
   ES->MP3_image_size = file_size;

   //
   // Skip initial ID3v2 tag for compliance with rest of library
   //

   if ( ( ((U8 const* )file_image)[ 0 ] == 0x49 ) && ( ((U8 const* )file_image)[ 1 ] == 0x44 ) && ( ((U8 const* )file_image)[ 2 ] == 0x33 ) &&
        ( ((U8 const* )file_image)[ 3 ] < 0xff )  && ( ((U8 const* )file_image)[ 4 ] < 0xff )  &&
        ( ((U8 const* )file_image)[ 6 ] < 0x80 )  && ( ((U8 const* )file_image)[ 7 ] < 0x80 )  && ( ((U8 const* )file_image)[ 8 ] < 0x80 ) && ( ((U8 const* )file_image)[ 9 ] < 0x80 ) )
      {
      U32 skip = 10 + ( ( (U32) ((U8 const* )file_image)[ 9 ] ) | ( ( (U32) ((U8 const* )file_image)[ 8 ] ) << 7 ) |
                        ( ( (U32) ((U8 const* )file_image)[ 7 ] ) << 14 ) | ( ( (U32) ((U8 const* )file_image)[ 6 ] ) << 21 ) );

      ES->ID3v2      = file_image;
      ES->ID3v2_size = skip;

      file_image = ( (U8*) file_image ) + skip;
      file_size -= skip;
      }

   ES->start_MP3_data = file_image;

   //
   // Account for ID3v1 tag at end of file, if any
   //

   if (file_size >= 128)
      {
      U8 *ptr = &ES->start_MP3_data[file_size - 128];

      if ((ptr[0]=='T') && (ptr[1]=='A') && (ptr[2]=='G'))
         {
         ES->ID3v1 = ptr;
         file_size -= 128;
         }
      }

   ES->end_MP3_data = &file_image[file_size-1];

   //
   // Image pointer = offset from start_MP3_data
   //

   ES->ptr        = ES->start_MP3_data;
   ES->bytes_left = file_size;
}

DXDEF 
S32 AILEXPORT AIL_enumerate_MP3_frames (MP3_INFO *ES)
{
   //
   // Find next valid frame in file image, using same discrimination logic as ASI implementation
   //

   S32 i;

read_frame_header:

   ES->next_frame_expected = (S32) AIL_ptr_dif(ES->ptr, ES->start_MP3_data);

   //
   // ID3v2 tags can appear at any frame boundary, not just the first
   //

   while (ES->bytes_left >= 10)   
      {
      if (!( ( ES->ptr[ 0 ] == 0x49 ) && ( ES->ptr[ 1 ] == 0x44 ) && ( ES->ptr[ 2 ] == 0x33 ) &&
             ( ES->ptr[ 3 ] < 0xff )  && ( ES->ptr[ 4 ] < 0xff )  &&
             ( ES->ptr[ 6 ] < 0x80 )  && ( ES->ptr[ 7 ] < 0x80 )  && ( ES->ptr[ 8 ] < 0x80 ) && ( ES->ptr[ 9 ] < 0x80 ) ) )
         {
         //
         // No ID3v2 tag present
         //
       
         break;
         }
      else
         {
         S32 size = ( (U32) ES->ptr[ 9 ] ) | ( ( (U32) ES->ptr[ 8 ] ) << 7 ) | ( ( (U32) ES->ptr[ 7 ] ) << 14 ) | ( ( (U32) ES->ptr[ 6 ] ) << 21 );
         S32 total_len;
   
         if (ES->ptr[5] & 0x10)
            {
            total_len = 20 + size;
            }
         else
            {
            total_len = 10 + size;
            }

         //
         // Skip ID3v2 tag
         //

         if (ES->bytes_left < total_len)
            {
            return 0;   // ID3v2 tag truncated at EOF
            }

         ES->bytes_left -= total_len;
         ES->ptr += total_len;
         }
      }

   //
   // Evaluate frame header for validity
   //

   U32 seek_data   = 0;          
   U32 dest_data   = 0;          
   U32 valid_frame = 0;          

   for(;;)
      {
      if (ES->bytes_left <= 0)
         {
         return 0;
         }

#ifdef IS_LE
      seek_data >>= 8;
#else
      seek_data <<= 8;
#endif

      *(((U8 *) &dest_data) + 3) = *ES->ptr++;
      --ES->bytes_left;

      seek_data |= dest_data;

#ifdef IS_LE
      if ( (seek_data & 0x0000e0ff ) == 0x0000e0ff )
#else
      if ( (seek_data & 0xffe00000 ) == 0xffe00000 )
#endif
         {
         //
         // Potentially-valid 32-bit frame header
         //

         valid_frame = 1;
         break;
         }
      }

   if (!valid_frame)
      {
      return 0;      // End of MPEG audio data coincides with end of file, or 
      }              // no (more) 11-bit frame sync markers found in file 

   //
   // Log offset of frame in bytes from beginning of file
   //

   ES->byte_offset = ((S32) AIL_ptr_dif(ES->ptr, ES->start_MP3_data)) - 4;

   //
   // Parse first 32 bits of header, first skipping 11-bit sync field
   //

   U8 header_buffer[64];           // Raw header and side info for current frame

   *((U32 *) (void*) header_buffer) = seek_data;

   ES->hpos = 11;

   ES->MPEG25             = !H(1);
   ES->MPEG1              =  H(1);
   ES->layer              =  H(2);
   ES->protection_bit     =  H(1);
   ES->bitrate_index      =  H(4);
   ES->sampling_frequency =  H(2);
   ES->padding_bit        =  H(1);
   ES->private_bit        =  H(1);
   ES->mode               =  H(2);
   ES->mode_extension     =  H(2);
   ES->copyright          =  H(1);
   ES->original           =  H(1);
   ES->emphasis           =  H(2);

   //
   // Perform sanity check on header, since most encoders seem to be written
   // with complete disregard to the rule against >= 12 consecutive 1 bits
   // in stream....
   //
   // We assume the first header found in the stream is valid, and use its
   // contents to check the fields of all subsequent headers.  The fields
   // tested are those which should not ordinarily change in the course of
   // a single stream.
   //

   if ((ES->bitrate_index      == 0x0f) ||
       (ES->sampling_frequency == 0x03) )
      {
      //
      // Header contains one or more invalid bitfields, so skip it
      //
      // (Note that this will skip a valid frame if it begins within
      // the first 4 bytes of a false header)
      //

      goto read_frame_header;
      }

   // keep searching if we find a non layer 3 block
   if (ES->layer != 1)
      {
      goto read_frame_header;
      }

   if (!ES->check_valid)
      {
      ES->check_valid = 1;

      ES->check_MPEG25             = ES->MPEG25;
      ES->check_MPEG1              = ES->MPEG1;
      ES->check_layer              = ES->layer;
      ES->check_protection_bit     = ES->protection_bit;
      ES->check_sampling_frequency = ES->sampling_frequency;
      ES->check_mode               = ES->mode;
      ES->check_copyright          = ES->copyright;
      ES->check_original           = ES->original;
      }
   else
      {
      if ((ES->MPEG1              != ES->check_MPEG1)              ||
          (ES->MPEG25             != ES->check_MPEG25)             ||
          (ES->layer              != ES->check_layer)              ||
//        (ES->protection_bit     != ES->check_protection_bit)     ||
//        (ES->mode               != ES->check_mode)               ||
//        (ES->copyright          != ES->check_copyright)          ||
//        (ES->original           != ES->check_original)           ||
          (ES->sampling_frequency != ES->check_sampling_frequency) )
         {
         //
         // Header does not match characteristics of most-recent one encountered
         // in stream -- keep looking
         //
         // (Note that this will skip a valid frame if it begins within
         // the first 4 bytes of a false header)
         //

         ES->check_MPEG25             = ES->MPEG25;
         ES->check_MPEG1              = ES->MPEG1;
         ES->check_layer              = ES->layer;
         ES->check_protection_bit     = ES->protection_bit;
         ES->check_sampling_frequency = ES->sampling_frequency;
         ES->check_mode               = ES->mode;
         ES->check_copyright          = ES->copyright;
         ES->check_original           = ES->original;

         goto read_frame_header;
         }
      }

   //
   // Skip CRC word if present
   //

   ES->header_size = 4;

   if (ES->protection_bit == 0)
      {
      if (ES->bytes_left < 2)
         {
         return 0;   // Header truncated at CRC word
         }

      header_buffer[4] = *ES->ptr++;
      header_buffer[5] = *ES->ptr++;
      ES->bytes_left -= 2;

      ES->hpos += 16;
      ES->header_size = 6;
      }

   //
   // Read MPEG side info into header buffer
   //
   
   if (ES->MPEG1)
      {
      //
      // MPEG 1: Mono=17, stereo=32 bytes
      //

      ES->side_info_size = (ES->mode == 3) ? 17 : 32;
      }
   else
      {
      //
      // MPEG2: Mono=9, stereo=17 bytes
      //

      ES->side_info_size = (ES->mode == 3) ? 9 : 17;
      }

   if (ES->bytes_left < ES->side_info_size)
      {
      return 0;   // Truncated side info block 
      }

   ES->bytes_left -= ES->side_info_size;

   for (i=0; i < ES->side_info_size; i++)
      {
      header_buffer[ES->header_size+i] = *ES->ptr++;
      }

   //
   // Get main data offset word and skip private bits
   // This leaves hpos at the MPEG 1 scalefactor selection info block, which
   // we don't care about reading...
   //

   ES->channels_per_sample = (ES->mode == 3) ? 1 : 2;
   ES->ngr                 = (ES->MPEG1)     ? 2 : 1;

   if (ES->MPEG1)
      {
      ES->main_data_begin = H(9);
      (ES->channels_per_sample == 1) ? H(5) : H(3);
      }
   else
      {
      ES->main_data_begin = H(8);
      (ES->channels_per_sample == 1) ? H(1) : H(2);
      }

   //
   // Calculate average and actual frame size
   //
   // In Layer 3, MPEG1 has two granules per frame
   //

   ES->bit_rate = MPEG_bit_rate[ES->MPEG1][ES->bitrate_index];

   ES->sample_rate = MPEG_sample_rate[ES->MPEG25][ES->MPEG1][ES->sampling_frequency];

   if (ES->MPEG1) 
      {
      ES->average_frame_size = (144 * ES->bit_rate) / ES->sample_rate;
      }
   else
      {
      ES->average_frame_size = (72 * ES->bit_rate) / ES->sample_rate;
      }

   ES->data_size = ES->average_frame_size + ES->padding_bit - (ES->header_size + ES->side_info_size);

   if (ES->data_size < 0)
      {
      ES->data_size = 0;
      }
        
   if ((ES->sample_rate == 44100) || 
       (ES->sample_rate == 48000) || 
       (ES->sample_rate == 32000))
      {
      ES->samples_per_frame = 1152;
      }
   else
      {
      ES->samples_per_frame = 576;
      }

   //
   // Skip over actual audio data
   //

   if (ES->bytes_left < ES->data_size)
      {
      return 0;   // Truncated audio data block 
      }

   //
   // Parse Info/Xing tag if present (used principally by LAME to record encoder lead-out and trailing
   // silence-padding sample counts)
   //

   U8 *info = (U8 *) ES->ptr;
   S32 info_frame = FALSE;

   if ((info[0] == 'I') && (info[1] == 'n') && (info[2] == 'f') && (info[3] == 'o'))
      {
      ES->Info_valid = info_frame = TRUE;
      }
   else if ((info[0] == 'X') && (info[1] == 'i') && (info[2] == 'n') && (info[3] == 'g')) 
      {
      ES->Xing_valid = info_frame = TRUE;
      }

   if (info_frame)
      {
      info += 4;

      ES->header_flags = fetch_U32(info);

      if (ES->header_flags & FRAMES_FLAG) ES->frame_count = fetch_U32(info); 
      if (ES->header_flags & BYTES_FLAG)  ES->byte_count  = fetch_U32(info);

      if (ES->header_flags & TOC_FLAG)        
         {
         AIL_memcpy(info,
                    ES->TOC,
                    NUMTOCENTRIES);

         info += NUMTOCENTRIES;
         }

      if (ES->header_flags & VBR_SCALE_FLAG) ES->VBR_scale = fetch_U32(info);

      info += 21;
      ES->enc_delay   = ((S32) (info[0] << 4)) + ((S32) (info[1] >> 4));
      ES->enc_padding = ((S32) (info[1] & 0x0f) << 8) + (S32) info[2];

      if ((ES->enc_delay   < 0) || (ES->enc_delay   > 4096)) ES->enc_delay   = -1;
      if ((ES->enc_padding < 0) || (ES->enc_padding > 4096)) ES->enc_padding = -1;
      }

   //
   // Write frame offset and return 1 to indicate frame data valid
   //

   ES->ptr        += ES->data_size;
   ES->bytes_left -= ES->data_size;

   return 1;
}

DXDEF
U32 AILEXPORT AIL_inspect_BinkA(BINKA_INFO* state, const char* file_image, S32 file_size)
{
    AIL_memset(state, 0, sizeof(BINKA_INFO));

    U32 tag = LE_SWAP32((void*)file_image); // wii intrisic doesn't take const (?)
    if (tag != 'BCF1')
    {
        return 0; // not a bink audio file.
    }

    state->file_image = file_image;
    state->image_size = file_size;
    
    struct BCFHEADER
    {
        U32 tag; // BCF1
        U8 version;
        U8 channels;
        U16 rate;
        U32 sample_count;
        U32 max_comp_space_needed;
        U32 output_file_size;
        U32 seek_table_entry_count;
    };

    if (state->image_size < (S32)sizeof(BCFHEADER))
    {
        return 0; // not big enough.
    }

    BCFHEADER const* header = (BCFHEADER const*)file_image;

    S32 Rate = LE_SWAP16(&header->rate);
    S32 SeekTableCount= LE_SWAP32(&header->seek_table_entry_count) & 0xffff;  // mask off SeekTableFrames in upper word
    S32 SampleCount = LE_SWAP32(&header->sample_count);

    // Get output samples per frame.
    if (Rate >= 44100) state->samples_per_frame = 2048;
    else if (Rate >= 22050) state->samples_per_frame = 1024;
    else state->samples_per_frame = 512;

    state->total_samples = SampleCount;
    state->sample_rate = Rate;
    state->channel_count = header->channels;

    state->current_frame = file_image + (sizeof(BCFHEADER) + SeekTableCount*2);
    return 1;
}

DXDEF
S32 AILEXPORT AIL_enumerate_BinkA_frames(BINKA_INFO* state)
{
    state->byte_offset = (U32)(  (char*)state->current_frame - (char*)state->file_image );
    U32 FrameCheck = LE_SWAP32((void*)state->current_frame); // wii intrisic doesn't take const (?)
    if ((FrameCheck & 0xffff) != 0x9999)
    {
        // out of sync.
        return 0;
    }

    // advance...
    state->current_frame += (FrameCheck >> 16) + sizeof(U32);   // frame size doesn't include check word
    return 1;
}

DXDEF
void AILEXPORT AIL_inspect_Ogg (OGG_INFO *ES, //)
                              U8       *file_image, 
                              S32           file_size)
{
   //
   // Initialize the Ogg inspection-state structure, setting
   // all fields to NULL/0
   //

   AIL_memset(ES, 
              0,
              sizeof(*ES));

   //
   // Skip any metadata that might appear before initial Ogg page (.wav headers, etc)
   //

   ES->Ogg_file_image = file_image;
   ES->Ogg_image_size = file_size;

   // DT - I feel like this should be a while and not an if...
   while ( ( ((U8 const* )file_image)[ 0 ] != 'O' ) || 
        ( ((U8 const* )file_image)[ 1 ] != 'g' ) || 
        ( ((U8 const* )file_image)[ 2 ] != 'g' ) ||
        ( ((U8 const* )file_image)[ 3 ] != 'S'))
      {
      file_image++;
      file_size--;

      if (file_size <= 0)
         {
         ES->bytes_left = 0;
         return;
         }
      }

   ES->channel_count = (S32)file_image[0x27];
   ES->sample_rate = file_image[0x28] | ((S32)file_image[0x29] << 8) | ((S32)file_image[0x2a] << 16) | ((S32)file_image[0x2b] << 24);

   ES->start_Ogg_data =  file_image;
   ES->end_Ogg_data   = &file_image[file_size-1];

   //
   // Image pointer = offset from start_Ogg_data
   //

   ES->ptr        = ES->start_Ogg_data;
   ES->bytes_left = file_size;
}

DXDEF
S32 AILEXPORT AIL_enumerate_Ogg_pages (OGG_INFO *ES)
{
   //
   // Verify that we landed on a valid page
   //

   if (ES->bytes_left < 28)
      {
      //
      // No more pages in file: stop enumerating
      //

      return 0;
      }

   if ((ES->ptr[0] != 'O' ) || 
       (ES->ptr[1] != 'g' ) || 
       (ES->ptr[2] != 'g' ) ||
       (ES->ptr[3] != 'S'))
      {
      //
      // Lost sync, file must be corrupt: stop enumerating pages
      //

      return 0;
      }

   //
   // Fill in info for the current page
   //

   ES->byte_offset  = (S32)(ES->ptr - ES->start_Ogg_data);
   ES->sample_count = ((ES->ptr[ 9]*256+ES->ptr[ 8])*256+ES->ptr[ 7])*256+ES->ptr[ 6];
   ES->page_num     = ((ES->ptr[21]*256+ES->ptr[20])*256+ES->ptr[19])*256+ES->ptr[18];

   //
   // Skip to beginning of next page, if any
   //

   ES->ptr += 26;

   S32 num_seg = *ES->ptr++;
   S32 len = 0;

   while (num_seg > 0) 
     {
     while ((*ES->ptr == 255) && (num_seg > 0))
         {
         len += *ES->ptr++;
         num_seg--;
         }

     if (num_seg > 0)
        {
        len += *ES->ptr++;
        }

     num_seg--;
     }

   //
   // Write frame offset and return 1 to indicate page data valid
   //

   ES->ptr += len;
   ES->bytes_left -= (len+26);

   ES->next_page_expected = (S32)(ES->ptr - ES->start_Ogg_data);

   return 1;
}


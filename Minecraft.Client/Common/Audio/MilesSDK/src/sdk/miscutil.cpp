//############################################################################
//##                                                                        ##
//##  MISCUTIL.CPP                                                          ##
//##                                                                        ##
//##  Miscellaneous utility functions                                       ##
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

//
// .WAV file headers
//

typedef struct
{
  S8  RIFF_string[4];
  U32 chunk_size;
  S8  ID_string[4];
  U8  data[4];
}
RIFF;

typedef MSS_STRUCT
{
   U32 ID;            // unique identification value matching label ID
   S32 position;      // play order position
   U32 target_chunk;  // normally 'data'
   U32 chunk_start;   // byte offset of target_chunk in 'wavl' chunk, if any, else 0
   S32 block_start;   // byte offset into block containing target sample within 'data' or 'slnt' chunk, 0 for PCM
   S32 sample_offset; // byte offset within block
}
CUE_POINT;

typedef MSS_STRUCT
{
  S8   CUE_string[4];
  U32  chunk_size;
    
  S32  n_cue_points; 
  
  CUE_POINT cue_points[1];
}
CUE;

typedef struct
{
  S8  LIST_string[4];
  U32 chunk_size;
  S8  ID_string[4];
  U8  data[4];
}
LIST;

typedef MSS_STRUCT
{
   S8  LABL_string[4];
   U32 chunk_size;
   U32 cue_ID;
   S8  text[4];
}
LABL;

U8  *ASI_mem_src_ptr;
S32  ASI_mem_src_len;
S32  ASI_mem_src_pos;

// ---------------------------------------------------------------------------
// Stream callback for ASI decoder/encoder
// ---------------------------------------------------------------------------

S32 AILLIBCALLBACK ASI_mem_stream_CB(UINTa     user, //)
                                     void     *dest,
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

MSS_CB_STACK_ALIGN( ASI_mem_stream_CB )


#if !defined(IS_DOS) && !defined(IS_PS2) && !defined(IS_PS3) 

static char ourtoupper( char c )
{
  if ( ( c >= 'a' ) && ( c <= 'z' ) )
    return( (char) (c - ( 'a' - 'A' )) );
  return( c );
}

//############################################################################
//##                                                                        ##
//##     strncmp() substitute                                               ##
//##                                                                        ##
//############################################################################

DXDEF S32 AILEXPORT AIL_strnicmp(char const     *s1, char const     *s2, U32 count)
{
   char c1,c2;
   U8 const     *st1=(U8 const     *)s1;
   U8 const     *st2=(U8 const     *)s2;
   S32 c=count;

   for(;c;c--) {
     c1=*st1;
     c1=ourtoupper(c1);
     c2=*st2;
     c2=ourtoupper(c2);
     if (c1<c2)
       return(-1);
     if (c1>c2)
       return(1);
     if (c1==0)
       break;
     st1=(U8 const    *)AIL_ptr_add(st1,1);
     st2=(U8 const    *)AIL_ptr_add(st2,1);
   }

   return(0);
}

//############################################################################
//##                                                                        ##
//##     strncmp() substitute                                               ##
//##                                                                        ##
//############################################################################

DXDEF S32 AILEXPORT AIL_stricmp(char const     *s1, char const     *s2)
{
   char c1,c2;
   U8 const     *st1=(U8 const     *)s1;
   U8 const     *st2=(U8 const     *)s2;

   for(;;) {
     c1=*st1;
     c1=ourtoupper(c1);
     c2=*st2;
     c2=ourtoupper(c2);
     if (c1<c2)
       return(-1);
     if (c1>c2)
       return(1);
     if (c1==0)
       break;
     st1=(U8 const    *)AIL_ptr_add(st1,1);
     st2=(U8 const    *)AIL_ptr_add(st2,1);
   }

   return(0);
}

#endif


//
// memory file routines
//

#define EACHMEMBUF (63L*1024L)

DXDEF HMEMDUMP AILEXPORT AIL_mem_open(void    * addr, U32 size)
{
  HMEMDUMP m;

  if ((!addr) || (!size))
    return(0);

  m=(struct _MEMDUMP     *) AIL_mem_alloc_lock( sizeof(MEMDUMP) );
  if (m==0)
    return(0);

  m->buffer[0]=addr;

  m->current=(U8     *) addr;
  m->size=0;
  m->error=0;
  m->curbufnum=0;
  m->lastbufnum=0;
  m->totalsize=size;
  m->curpos=0;
  m->expandable=0;
  m->eachbuf=size;
  m->firstbuf=size;

  return(m);
}

DXDEF HMEMDUMP AILEXPORT AIL_mem_create()
{
  HMEMDUMP m;

  m=(struct _MEMDUMP     *) AIL_mem_alloc_lock(sizeof(MEMDUMP) );
  if (m==0)
    return(0);

  m->buffer[0]=AIL_mem_alloc_lock(EACHMEMBUF);
  if (m->buffer[0]==0) {
    AIL_mem_free_lock(m->buffer[0]);
    return(0);
  }

  m->current=(U8     *) m->buffer[0];
  m->size=0;
  m->error=0;
  m->curbufnum=0;
  m->lastbufnum=0;
  m->totalsize=0;
  m->curpos=0;
  m->expandable=1;
  m->eachbuf=EACHMEMBUF;
  m->firstbuf=EACHMEMBUF;

  return(m);
}

DXDEF HMEMDUMP AILEXPORT AIL_mem_create_from_existing(void* addr, U32 size)
{
  HMEMDUMP m;

  m=(struct _MEMDUMP     *) AIL_mem_alloc_lock(sizeof(MEMDUMP) );
  if (m==0)
    return(0);

  m->buffer[0]=addr;

  m->current=(U8     *) m->buffer[0];
  m->size=0;
  m->error=0;
  m->curbufnum=0;
  m->lastbufnum=0;
  m->totalsize=0;
  m->curpos=0;
  m->expandable=1;
  m->eachbuf=EACHMEMBUF;
  m->firstbuf=size;

  return(m);
}

DXDEF S32 AILEXPORT AIL_mem_close(HMEMDUMP m, void    *    * buf, U32    * size)
{
    S32 ret=1;

    if (m) 
    {
        U32 i;

        if (size) *size=m->totalsize;

        if (buf) 
        {
            // if we only used the first buffer, just give them the pointer and bail.
            if (m->lastbufnum == 0)
            {
                *buf = m->buffer[0];
                AIL_mem_free_lock(m);
                return 1;
            }

            char    * b;
            b=(C8     *) AIL_mem_alloc_lock(m->totalsize);
            *buf=(void    *)b;
            if (b)
            {
                i=0;
                while (m->totalsize) 
                {
                    U32 ThisBufSize = i == 0 ? m->firstbuf : m->eachbuf;
                    U32 thissize = (m->totalsize >= ThisBufSize) ? ThisBufSize : m->totalsize;
                    AIL_memcpy(b,m->buffer[i++],thissize);
                    b=(C8     *)AIL_ptr_add(b,thissize);
                    m->totalsize-=thissize;
                }
            } 
            else 
            {
                ret=0;
            }
        }

        for(i=0;i<=m->lastbufnum;i++) AIL_mem_free_lock(m->buffer[i]);

        AIL_mem_free_lock(m);
    }
    return(ret);
}

static void memcopydata(HMEMDUMP m,void const    * s, U32 bytes)
{
  AIL_memcpy(m->current,s,bytes);

  m->current=(U8     *) AIL_ptr_add(m->current,bytes);
  m->size+=bytes;

  m->curpos+=bytes;
  if (m->curpos>m->totalsize)
    m->totalsize=m->curpos;
}

DXDEF U32 AILEXPORT AIL_mem_write(HMEMDUMP m, void const    * s,U32 bytes)
{
  if ((m) && (bytes) && (m->error==0)) {

    U32 amt;
    U32 wrote=0;

   again:

    U32 ThisBufSize = m->curbufnum ? m->eachbuf : m->firstbuf;
    amt=(ThisBufSize-m->size);

    amt=(bytes>=amt)?amt:bytes;

    if (amt) {
      memcopydata(m,s,amt);
      bytes-=amt;
      wrote+=amt;
    }

    if (bytes) {

      // we need to bump a buffer

      s=AIL_ptr_add(s,amt);

      if (m->curbufnum==m->lastbufnum) {

        // we at the end of the file

        if ((m->curbufnum>=(sizeof(m->buffer)/4)-1) || (!m->expandable)) {
          m->error=1;
          return(0);
        }

        m->buffer[m->curbufnum+1]=AIL_mem_alloc_lock(EACHMEMBUF);
        if (m->buffer[m->curbufnum+1]==0) {
          m->error=1;
          return(0);
        }

        ++m->lastbufnum;
      }

      ++m->curbufnum;
      m->current=(U8    *)m->buffer[m->curbufnum];
      m->size=0;

      goto again;

    }

    return(wrote);
  }
  return(0);
}

static void memcopytodata(HMEMDUMP m,void    * d, U32 bytes)
{
  AIL_memcpy(d,m->current,bytes);

  m->current=(U8     *)AIL_ptr_add(m->current,bytes);
  m->size+=bytes;

  m->curpos+=bytes;
}

DXDEF U32 AILEXPORT AIL_mem_read(HMEMDUMP m, void    * s,U32 bytes)
{
  if ((m) && (bytes) && (m->error==0)) {
    U32 amt;
    U32 read=0;

   again:

    U32 ThisBufSize = m->curbufnum ? m->eachbuf : m->firstbuf;
    amt=(ThisBufSize-m->size);
    amt=(bytes>=amt)?amt:bytes;

    if (amt) {
      memcopytodata(m,s,amt);
      bytes-=amt;
      read+=amt;
    }

    if (bytes) {

      // we're at the end

      s=AIL_ptr_add(s,amt);

      if (m->curbufnum==m->lastbufnum)
        return(amt);

      ++m->curbufnum;
      m->current=(U8    *)m->buffer[m->curbufnum];
      m->size=0;

      goto again;
    }

    return(read);
  }
  return(0);
}

DXDEF U32
#if defined(IS_WIN32API)
__cdecl 
#endif
AIL_mem_printf(HMEMDUMP m, char const    * fmt, ...)
{
  char work_string[1024];
  va_list ap;

  va_start(ap,
           fmt);

  vsprintf(work_string,
            fmt,
            ap);

  va_end  (ap);

  return( AIL_mem_write(m,work_string,(S32) AIL_strlen(work_string)) );
}

DXDEF U32 AILEXPORT AIL_mem_printc(HMEMDUMP m, char c)
{
  return( AIL_mem_write(m,&c,1) );
}

DXDEF U32 AILEXPORT AIL_mem_prints(HMEMDUMP m, char const    * s)
{
  return( AIL_mem_write(m,s,(S32) AIL_strlen(s)) );
}

DXDEF U32 AILEXPORT AIL_mem_seek(HMEMDUMP m, U32 pos)
{
    if (m) 
    {
        if (pos > m->totalsize) pos = m->totalsize;

        if (pos < m->firstbuf)
        {
            m->curbufnum = 0;
            m->size = pos;
            m->current = (U8*)AIL_ptr_add(m->buffer[0], pos);
            m->curpos = pos;
        }
        else
        {
            m->curbufnum = 1 + (pos - m->firstbuf)/EACHMEMBUF;
            m->size = (pos - m->firstbuf) % EACHMEMBUF;
            m->current=(U8    *)AIL_ptr_add(m->buffer[m->curbufnum],m->size);
            m->curpos=pos;
        }
        return(m->curpos);
    }
    return 0;
}

DXDEF U32 AILEXPORT AIL_mem_size(HMEMDUMP m)
{
  return( m?m->totalsize:0);
}

DXDEF U32 AILEXPORT AIL_mem_pos(HMEMDUMP m)
{
  return( m?m->curpos:0);
}

DXDEF S32 AILEXPORT AIL_mem_error(HMEMDUMP m)
{
  return( m?m->error:0);
}

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

//############################################################################
//##                                                                        ##
//## Set the compression preferences                                        ##
//##                                                                        ##
//############################################################################

static HPROVIDER comp_ASI;
static HASISTREAM comp_stream=0;
static ASI_STREAM_PROPERTY ASI_stream_property;

static S32 AILCALLBACK compress_setprop(char const    * preference,void const    * value)
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

DXDEF S32 AILEXPORT AIL_decompress_ASI(void const    * indata, //)
                                       U32 insize,
                                       char const    * filename_ext,
                                       void    *    * outdata,
                                       U32    * outsize,
                                       AILLENGTHYCB callback)
{

   if ((indata==0) || (insize==0))
     return(0);

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

   comp_ASI = (filename_ext==0) ? 0 : find_ASI_provider("Input file types",
                                                         filename_ext);

   if (comp_ASI==0) {
     AIL_set_error("No codec found for requested input type.");
     return(0);
   }

   if (callback)
     if (callback(AIL_LENGTHY_INIT,0)==0) {
      cancelled:
       AIL_set_error("Decompression cancelled.");
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

   ASI_mem_src_ptr = (U8    *)indata;
   ASI_mem_src_len = insize;

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
      return 0;
      }

   if (callback)
     if (callback(AIL_LENGTHY_SET_PROPERTY,(UINTa)compress_setprop)==0) {
       ASI_stream_close(comp_stream);
       comp_stream=0;
       goto cancelled;
     }


   //
   // Allocate an output memory dump
   //


    S32 compressed_bit_rate   = 0;
    S32 uncompressed_bit_rate = 0;

    ASI_stream_property(comp_stream, INPUT_BIT_RATE,  &compressed_bit_rate,   0, 0 );
    ASI_stream_property(comp_stream, OUTPUT_BIT_RATE, &uncompressed_bit_rate, 0, 0 );

    S32 estimate_bytes = mult64anddiv(insize, uncompressed_bit_rate, compressed_bit_rate);
    //estimate_bytes += (estimate_bytes / 20);     // Add 5% for extra padding margin

    void* pStarterData = AIL_mem_alloc_lock(estimate_bytes);

    HMEMDUMP mem=AIL_mem_create_from_existing(pStarterData, estimate_bytes);

   if (mem == NULL)
      {
      ASI_stream_close(comp_stream);
      comp_stream=0;
      return(0);
      }

   WAVEOUT wav;

   AIL_mem_write(mem,&wav,sizeof(wav));

   //
   // Process input data
   //

   U8 temp_buf[1024];
   S32 len;
   S32 cancel=0;

   for(;;)
   {
     #ifdef IS_PS2
     InMilesMutex();
     #endif
     len = ASI_stream_process( comp_stream, temp_buf, 1024 );
     #ifdef IS_PS2
     OutMilesMutex();
     #endif

     if ( len == 0 )
       break;
       
     AIL_mem_write( mem, temp_buf, len );
     if ( callback )
     {
       F32 percent=0.0f;
       ASI_stream_property(comp_stream,PERCENT_DONE, &percent,0,0);
        
       if (callback(AIL_LENGTHY_UPDATE,*(UINTa    *)(void    *)&percent)==0)
       {
         cancel = 1;
         break;
       }
     }
    }

   //create the wave header
   AIL_memcpy(&wav.riffmark,"RIFF",4);
   STORE_LE_SWAP32( &wav.rifflen, AIL_mem_size(mem)-8 );
   AIL_memcpy(&wav.wavemark,"WAVE",4);
   AIL_memcpy(&wav.fmtmark,"fmt ",4);
   STORE_LE_SWAP32( &wav.fmtlen, 16 );
   STORE_LE_SWAP16( &wav.fmttag, WAVE_FORMAT_PCM );

   U32 val = (U32)~0; ASI_stream_property(comp_stream,OUTPUT_CHANNELS,&val,0,0);
   STORE_LE_SWAP16( &wav.channels,(S16)val );
   
   val = (U32)~0; ASI_stream_property(comp_stream,OUTPUT_SAMPLE_RATE,&val,0,0);
   STORE_LE_SWAP32( &wav.sampersec, val );
   
   STORE_LE_SWAP16( &wav.blockalign,(S16)((16*(U32)wav.channels) / 8) );
   STORE_LE_SWAP32( &wav.avepersec, (wav.sampersec*16*(U32)wav.channels) / 8 );
   STORE_LE_SWAP16( &wav.bitspersam, 16 );
   AIL_memcpy(&wav.datamark,"data",4);
   STORE_LE_SWAP32( &wav.datalen,AIL_mem_size(mem)-sizeof(wav) );

   //write out the wave header
   AIL_mem_seek(mem,0);
   AIL_mem_write(mem,&wav,sizeof(wav));
   if ( !AIL_mem_close(mem,outdata,outsize) )
     AIL_set_error( "Out of memory." );


   ASI_stream_close(comp_stream);
   comp_stream=0;

   if (callback)
     callback(AIL_LENGTHY_DONE,0);

   if (cancel)
     goto cancelled;

   return( 1 );
}

struct sample_loop
{
    U32 id;
    U32 type;
    U32 start;
    U32 end;
    U32 frac;
    U32 count;
};

struct smpl_chunk
{
    char id[4];
    U32 size;
    U32 manu;
    U32 product;
    U32 period;
    U32 note;
    U32 frac;
    U32 smpte_fmt;
    U32 smpte_offset;
    U32 loop_count;
    U32 data_size;
    sample_loop loops[1];
};

DXDEF
S32 AILEXPORT AIL_WAV_marker_count(void const *WAV_image)
{
   AILSOUNDINFO info;

   //
   // Identify .WAV file type (must be ADPCM or PCM)
   //

   if (!AIL_WAV_info(WAV_image, &info))
      {
      return -1;
      }

   if ((info.format != WAVE_FORMAT_PCM) && (info.format != WAVE_FORMAT_IMA_ADPCM))
      {
      AIL_set_error("Unsupported .WAV format");
      return -1;
      }

   const void *data = WAV_image;
   void *end  = AIL_ptr_add(data,LEU_SWAP32(&((RIFF *) data)->chunk_size) + 8 + (LEU_SWAP32(&((RIFF *) data)->chunk_size) & 1));

   //
   // Look for 'cue' chunk containing sample offsets of markers and loops
   //

   CUE *cue_chunk = (CUE *) (void *) (((RIFF *) data)->data);

   while (AIL_strnicmp((char *) cue_chunk->CUE_string,"cue ",4))
   {
       U32 chunk_size = LEU_SWAP32(&cue_chunk->chunk_size);

       cue_chunk = (CUE *) AIL_ptr_add(cue_chunk, chunk_size + 8 + (chunk_size & 1));

       if (AIL_ptr_dif(cue_chunk, end) >= 0)
       {
           cue_chunk = 0;
           break;
       }
   }

   // Look for the 'smpl' chunk which might contain an implicit loop.
   smpl_chunk* smpl = (smpl_chunk*) (void *) (((RIFF *) data)->data);

   while (AIL_strnicmp((char *) smpl->id,"smpl",4))
   {
       U32 chunk_size = LEU_SWAP32(&smpl->size);

       smpl = (smpl_chunk*) AIL_ptr_add(smpl, chunk_size + 8 + (chunk_size & 1));

       if (AIL_ptr_dif(smpl, end) >= 0)
       {
           smpl = 0;
           break;
       }
   }

   if (smpl == 0 && cue_chunk == 0)
   {
       AIL_set_error("No marker or loop information in file.");
       return -1;
   }

   // Count the entries.
   S32 marker_count = 0;
   if (smpl && LEU_SWAP32(&smpl->loop_count) == 1)
       marker_count += 2;

   if (cue_chunk)
       marker_count += LEU_SWAP32(&cue_chunk->chunk_size) / sizeof(CUE_POINT);

   return marker_count;
}


//############################################################################
//##                                                                        ##
//## Decompress from ADPCM                                                  ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_decompress_ADPCM(AILSOUNDINFO const * info, //)
                             void * * outdata, U32 * outsize)
{
  void * outp;
  U32 size;
  WAVEOUT * wav;
  ADPCMDATA ad;
  void  *dst;
  void const  *src;

  if ((info==0) || (info->data_ptr==0) || (info->samples==0))
    return(0);

  if ((info->format!=WAVE_FORMAT_IMA_ADPCM) || (info->bits!=4)) {
    AIL_set_error("Data is not IMA compressed.");
    return(0);
  }

  size=(info->samples*info->channels*16)/8;

  outp=AIL_mem_alloc_lock(size+sizeof(WAVEOUT));

  if (outp==0) {
    return(0);
  }

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


  wav=(WAVEOUT *)outp;

  AIL_memcpy(&wav->riffmark,"RIFF",4);
  wav->rifflen=size+sizeof(WAVEOUT)-8;
  AIL_memcpy(&wav->wavemark,"WAVE",4);
  AIL_memcpy(&wav->fmtmark,"fmt ",4);
  wav->fmtlen=16;
  wav->fmttag=WAVE_FORMAT_PCM;
  wav->channels=(S16)info->channels;
  wav->sampersec=info->rate;
  wav->blockalign=(S16)((16* info->channels) / 8);
  wav->avepersec=(info->rate * 16* info->channels) / 8;
  wav->bitspersam=16;
  AIL_memcpy(&wav->datamark,"data",4);
  wav->datalen=size;

  #ifdef IS_BE
  wav->rifflen=LE_SWAP32( &wav->rifflen );
  wav->fmtlen=LE_SWAP32( &wav->fmtlen );
  wav->fmttag=(U16)LE_SWAP16( &wav->fmttag );
  wav->channels=(U16)LE_SWAP16( &wav->channels );
  wav->sampersec=LE_SWAP32( &wav->sampersec );
  wav->blockalign=(U16)LE_SWAP16( &wav->blockalign );
  wav->avepersec=LE_SWAP32( &wav->avepersec );
  wav->bitspersam=(U16)LE_SWAP16( &wav->bitspersam );
  wav->datalen=LE_SWAP32( &wav->datalen );
  #endif

  ad.blocksize = info->block_size;
  ad.blockleft = 0;
  ad.extrasamples = 0;

  dst=AIL_ptr_add(outp,sizeof(WAVEOUT));
  src=info->data_ptr;

  MIXER_adpcm_decode(&dst,&src, size, info->data_len, (info->channels==2) ? (M_SRC_STEREO | M_SRC_16) : M_SRC_16, &ad);

  if (outsize)
    *outsize=size+sizeof(WAVEOUT);

  if (outdata)
    *outdata=outp;
  else
    AIL_mem_free_lock(outp);

  return(1);
}


DXDEF
S32 AILEXPORT AIL_WAV_marker_by_index(void const *WAV_image, S32 n, C8 const **name)
{
   AILSOUNDINFO info;

   //
   // Identify .WAV file type (must be ADPCM or PCM)
   //

   if (!AIL_WAV_info(WAV_image, &info))
      {
      return -1;
      }

   if ((info.format != WAVE_FORMAT_PCM) && (info.format != WAVE_FORMAT_IMA_ADPCM))
      {
      AIL_set_error("Unsupported .WAV format");
      return -1;
      }

   const void *data = WAV_image;
   void *end  = AIL_ptr_add(data,LEU_SWAP32(&((RIFF *) data)->chunk_size) + 8 + (LEU_SWAP32(&((RIFF *) data)->chunk_size) & 1));

   //
   // Look for 'cue' chunk containing sample offsets of markers and loops
   //

   CUE *cue_chunk = (CUE *) (void *) (((RIFF *) data)->data);

   while (AIL_strnicmp((char *) cue_chunk->CUE_string,"cue ",4))
   {
       U32 chunk_size = LEU_SWAP32(&cue_chunk->chunk_size);

       cue_chunk = (CUE *) AIL_ptr_add(cue_chunk, chunk_size + 8 + (chunk_size & 1));

       if (AIL_ptr_dif(cue_chunk, end) >= 0)
       {
           cue_chunk = 0;
           break;
       }
   }

   // Look for the 'smpl' chunk which might contain an implicit loop.
   smpl_chunk* smpl = (smpl_chunk*) (void *) (((RIFF *) data)->data);

   while (AIL_strnicmp((char *) smpl->id,"smpl",4))
   {
       U32 chunk_size = LEU_SWAP32(&smpl->size);

       smpl = (smpl_chunk*) AIL_ptr_add(smpl, chunk_size + 8 + (chunk_size & 1));

       if (AIL_ptr_dif(smpl, end) >= 0)
       {
           smpl = 0;
           break;
       }
   }

   if (smpl == 0 && cue_chunk == 0)
   {
       AIL_set_error("No marker or loop information in file.");
       return -1;
   }

   // if we have a loop spec, the first two indices are the endpoints.
   if (smpl && LEU_SWAP32(&smpl->loop_count) == 1)
   {
       if (n == 0)
       {
           *name = "LoopStart";
           return LEU_SWAP32(&smpl->loops[0].start) * info.channels * 2;
       }
       else if (n == 1)
       {
           *name = "LoopEnd";
           return LEU_SWAP32(&smpl->loops[0].end) * info.channels * 2;
       }
       n -= 2;
   }

   //
   // Identify the nth cue point
   //

   S32 n_points = LEU_SWAP32(&cue_chunk->chunk_size) / sizeof(CUE_POINT);

   if (n >= n_points)
      {
      AIL_set_error("Marker not found");
      return -1;
      }

   CUE_POINT *cue_points = cue_chunk->cue_points; 
   CUE_POINT *c = &cue_points[n];

   //
   // If name requested, search list of inner 'labl' chunks for 
   // ID that matches the nth cue point
   //

   if (name != NULL)
      {
      *name = "(Marker name not found)";

      //
      // Look for 'LIST adtl' block containing 'labl' chunks that identify the markers
      // 

      LIST *L = (LIST *) (void *) (((RIFF *) data)->data); 

      while (AIL_strnicmp((char *) L->LIST_string,"LIST",4) || AIL_strnicmp((char *) L->ID_string,"adtl",4)) 
         {
         U32 chunk_size = LEU_SWAP32(&L->chunk_size);

         L = (LIST *) AIL_ptr_add(L, chunk_size + 8 + (chunk_size & 1));

         if (AIL_ptr_dif(L, end) >= 0)
            {
            L = NULL;
            break;
            }
         }

      if (L != NULL)
         {
         void *L_end = AIL_ptr_add(L, LEU_SWAP32(&L->chunk_size) + 8 + (LEU_SWAP32(&L->chunk_size) & 1));
         LABL *l = (LABL *) (void*)L->data;

         while (l < L_end)
            {
            U32 chunk_size = LEU_SWAP32(&l->chunk_size);

            LABL *next_l = (LABL *) AIL_ptr_add(l, chunk_size + 8 + (chunk_size & 1));

            if (!AIL_strnicmp((char *) l->LABL_string,"labl",4))
               {
               if (c->ID == l->cue_ID) // no need to byte-swap
                  {
                  *name = (char *) l->text;
                  break;
                  }
               }

            l = next_l;
            }
         }
      }

   //
   // Return the offset of the nth cue point
   //

   S32 offset = LEU_SWAP32(&c->sample_offset);
   
   //
   // Sound Forge logs offsets in terms of what it thinks of as "samples"
   // rather than as byte offsets as described at www.sonicspot.com/guide/wavefiles.html
   //
   // The block_start field isn't used at all
   //

   offset *= info.channels;  

   if (info.bits == 16) offset *= 2;
   if (info.bits == 4)  offset /= 2;

   return offset;
}

DXDEF
S32 AILEXPORT AIL_WAV_marker_by_name(void const *WAV_image, C8 *name)
{
   AILSOUNDINFO info;

   //
   // Identify .WAV file type (must be ADPCM or PCM)
   //

   if (!AIL_WAV_info(WAV_image, &info))
      {
      return -1;
      }

   if ((info.format != WAVE_FORMAT_PCM) && (info.format != WAVE_FORMAT_IMA_ADPCM))
      {
      AIL_set_error("Unsupported .WAV format");
      return -1;
      }

   const void *data = WAV_image;
   void *end  = AIL_ptr_add(data,LEU_SWAP32(&((RIFF *) data)->chunk_size) + 8 + (LEU_SWAP32(&((RIFF *) data)->chunk_size) & 1));


   //
   // Look for 'cue' chunk containing sample offsets of markers and loops
   //
   CUE *cue_chunk = (CUE *) (void *) (((RIFF *) data)->data);

   while (AIL_strnicmp((char *) cue_chunk->CUE_string,"cue ",4))
   {
       U32 chunk_size = LEU_SWAP32(&cue_chunk->chunk_size);

       cue_chunk = (CUE *) AIL_ptr_add(cue_chunk, chunk_size + 8 + (chunk_size & 1));

       if (AIL_ptr_dif(cue_chunk, end) >= 0)
       {
           cue_chunk = 0;
           break;
       }
   }

   // Look for the 'smpl' chunk which might contain an implicit loop.
   smpl_chunk* smpl = (smpl_chunk*) (void *) (((RIFF *) data)->data);

   while (AIL_strnicmp((char *) smpl->id,"smpl",4))
   {
       U32 chunk_size = LEU_SWAP32(&smpl->size);

       smpl = (smpl_chunk*) AIL_ptr_add(smpl, chunk_size + 8 + (chunk_size & 1));

       if (AIL_ptr_dif(smpl, end) >= 0)
       {
           smpl = 0;
           break;
       }
   }

   if (smpl == 0 && cue_chunk == 0)
   {
       AIL_set_error("No marker or loop information in file.");
       return -1;
   }

   if (smpl && LEU_SWAP32(&smpl->loop_count) == 1)
   {
       if (AIL_stricmp(name, "LoopStart") == 0)
           return LEU_SWAP32(&smpl->loops[0].start);
       if (AIL_stricmp(name, "LoopEnd") == 0)
           return LEU_SWAP32(&smpl->loops[0].end);

       if (cue_chunk == 0)
       {
           AIL_set_error("Marker not found");
           return -1;
       }
   }


   //
   // Look for 'LIST adtl' block containing 'labl' chunks that identify the markers
   // 

   LIST *L = (LIST *) (void *) (((RIFF *) data)->data); 

   while (AIL_strnicmp((char *) L->LIST_string,"LIST",4) || AIL_strnicmp((char *) L->ID_string,"adtl",4)) 
      {
      U32 chunk_size = LEU_SWAP32(&L->chunk_size);

      L = (LIST *) AIL_ptr_add(L, chunk_size + 8 + (chunk_size & 1));

      if (AIL_ptr_dif(L, end) >= 0)
         {
         AIL_set_error("No list chunk found");
         return -1;
         }
      }

   void *L_end  = AIL_ptr_add(L, LEU_SWAP32(&L->chunk_size) + 8 + (LEU_SWAP32(&L->chunk_size) & 1));

   //
   // Search list of inner 'labl' chunks for specified marker name
   //

   LABL *l = (LABL *) (void*) L->data;

   while (l < L_end)
      {
      U32 chunk_size = LEU_SWAP32(&l->chunk_size);

      LABL *next_l = (LABL *) AIL_ptr_add(l, chunk_size + 8 + (chunk_size & 1));

      if (!AIL_strnicmp((char *) l->LABL_string,"labl",4))
         {
         if (!AIL_stricmp((char *) l->text,name))
            {
            break;
            }
         }

      l = next_l;
      }

   if (l >= L_end)
      {
      AIL_set_error("Marker not found");
      return -1;
      }

   //
   // Return the offset of the cue point whose ID matches the label's
   //
   // We assume no wave list exists, and that target_chunk is simply the
   // main 'data' chunk
   //

   CUE_POINT *cue_points = cue_chunk->cue_points; 
   S32        n_points   = LEU_SWAP32(&cue_chunk->chunk_size) / sizeof(CUE_POINT);

   for (S32 i=0; i < n_points; i++)
      {
      CUE_POINT *c = &cue_points[i];

      if (c->ID == l->cue_ID) // no need to byte-swap
         {
         S32 offset = LEU_SWAP32(&c->sample_offset);
         
         //
         // Sound Forge logs offsets in terms of what it thinks of as "samples"
         // rather than as byte offsets as described at www.sonicspot.com/guide/wavefiles.html
         //
         // The block_start field isn't used at all
         //

         offset *= info.channels;  

         if (info.bits == 16) offset *= 2;
         if (info.bits == 4)  offset /= 2;

         return offset;
         }
      }

   AIL_set_error("Cue point not found");    // shouldn't happen
   return -1;   
}

//
// Misc. definitions
//

MSS_STRUCT CHUNK
{
   MSSFOURCC ckID;
   S32    ckSize;
   MSSFOURCC subID;
   U8     data[4];
};

#define MSSFOURCC_RIFF MSSmmioFOURCC('R','I','F','F')
#define MSSFOURCC_MLS  MSSmmioFOURCC('M','L','S',' ')
#define MSSFOURCC_DLS  MSSmmioFOURCC('D','L','S',' ')

// ---------------------------------------------------------------------------
// ptr_freebase
//
// Adjusts huge pointer by difference between it and specified cloned pointer,
// then frees the cloned pointer before returning the modified huge pointer
// ---------------------------------------------------------------------------

static void     *ptr_freebase(void     *clone, void     *orig)
{
   void    * ptr=AIL_ptr_from_clone(clone,orig);

   AIL_ptr_free_clone(clone);

   return ptr;
}


// ---------------------------------------------------------------------------
// Find named FORM in IFF file
//
// Returns NULL if not found, else returns huge pointer to FORM
// ---------------------------------------------------------------------------

static U8     * XMI_find_FORM(U8     *image, S32 size, C8 const *name)
{
   U32 len = 0;

   U8     *ptr = (U8     *) AIL_ptr_alloc_clone(image);

   U8     *end = (U8     *) AIL_ptr_add(image, size);

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

      len = 8 + BEU_SWAP32( ((U8     *) (ptr+4)));
      }
   while (AIL_strnicmp((char*)ptr+8,name,4));

   //
   // Return location relative to selector originally passed in
   //

   return (U8     *) ptr_freebase(ptr, image);
}

extern "C" S32 AILCALL AIL_API_file_type(void const    * data, U32 size)
{
  AILSOUNDINFO si;

  if ((data==0) || (size<8))
    return(AILFILETYPE_UNKNOWN);

  if (AIL_WAV_info(data,&si))
  {
    switch (si.format)
    {
      case 1:
        return(AILFILETYPE_PCM_WAV);
      case WAVE_FORMAT_IMA_ADPCM:
        if (si.bits==4)
          return(AILFILETYPE_ADPCM_WAV);
        else
          return(AILFILETYPE_OTHER_WAV);
      case WAVE_FORMAT_XBOX_ADPCM:
        if (si.bits==4)
          return(AILFILETYPE_XBOX_ADPCM_WAV);
        else
          return(AILFILETYPE_OTHER_WAV);
      case 85:  //mpeg wrapped in a WAV
        data=si.data_ptr;
        size=si.data_len;
        break;
      case 0x77:
        return( AILFILETYPE_V12_VOICE );
      case 0x74:
        return( AILFILETYPE_V24_VOICE );
      case 0x75:
        return( AILFILETYPE_V29_VOICE );
      default:
        //
        // see if we have an ASI that can read it...
        //
        return(
                ( RIB_find_file_dec_provider( "ASI codec",
                                              "Input wave tag",
                                              si.format,
                                              "Output file types",
                                              ".raw" )) ?
                AILFILETYPE_OTHER_ASI_WAV:
                AILFILETYPE_OTHER_WAV
              );
    }
  }

  if (*(S32*)data == 'BCF1') return AILFILETYPE_BINKA;

  if (!AIL_strnicmp((char*)data,"OggS",4))
   {
   for (S32 i=0; i < 128; i++)
      {
      if (!AIL_strnicmp((C8 *) AIL_ptr_add(data,i), "Speex", 5))
         {
         return AILFILETYPE_OGG_SPEEX;
         }
      }

   return AILFILETYPE_OGG_VORBIS;
   }

  if (!AIL_strnicmp((char*)(((VOC     *) data)->ID_string),"Creative",8))
    return( AILFILETYPE_VOC );

  if ((!AIL_strnicmp((char    *) (data)  , "FORM", 4)) &&
      (!AIL_strnicmp((char    *) (data)+8, "XDIR", 4))) {

    //figure out type of XMIDI

    U8    * XDLS_start = XMI_find_FORM((U8     *) data,
                                                  size,
                                                  "XDLS");
    if (XDLS_start != NULL)
       {
       XDLS_start = (U8     *) AIL_ptr_add(XDLS_start, 12);

       if (((CHUNK    *)XDLS_start)->subID == MSSFOURCC_DLS)
         return(AILFILETYPE_XMIDI_DLS);

       if (((CHUNK    *)XDLS_start)->subID == MSSFOURCC_MLS)
         return(AILFILETYPE_XMIDI_MLS);

       }

    return(AILFILETYPE_XMIDI);
  }

  if (((CHUNK    *)data)->ckID == MSSFOURCC_RIFF) {

    if (((CHUNK    *)data)->subID == MSSFOURCC_DLS)
      return(AILFILETYPE_DLS);

    if (((CHUNK    *)data)->subID == MSSFOURCC_MLS)
      return(AILFILETYPE_MLS);

  }

  //
  // We'll assume this is an MPEG file if the first word
  // contains an MPEG header (12 successive '1' bits) and the first DWORD is
  // not 0xffffffff
  //
  // This is where we end up for most raw data files that aren't identified by their
  // filename suffix.  Such files are very often falsely identified as MP3s...
  //

  {
    U8    * src=(U8    *)data;
    U32 len;

    len = size;

    if ( ( src[ 0 ] == 0x49 ) && ( src[ 1 ] == 0x44 ) && ( src[ 2 ] == 0x33 ) &&
         ( src[ 3 ] < 0xff ) && ( src[ 4 ] < 0xff ) &&
         ( src[ 6 ] < 0x80 ) && ( src[ 7 ] < 0x80 ) && ( src[ 8 ] < 0x80 ) && ( src[ 9 ] < 0x80 ) )
    {
      U32 skip = 10 + ( ( (U32) src[ 9 ] ) | ( ( (U32) src[ 8 ] ) << 7 ) |
                        ( ( (U32) src[ 7 ] ) << 14 ) | ( ( (U32) src[ 6 ] ) << 21 ) );
      src+=skip;
      len-=skip;
    }

    S32 type=0;

    if (len>AIL_MAX_FILE_HEADER_SIZE)
      len=AIL_MAX_FILE_HEADER_SIZE;

    while (len-- >= 4L)
    {

      if ( ( LEU_SWAP16( src) & 0xF0FF) == 0xF0FF)
      {
        if  ((LEU_SWAP32( src ) != 0xFFFFFFFF) &&
           (((LEU_SWAP32( src )>>18)&0x3f) != 0x3F))
        {
          switch ((LE_SWAP16( src)>>9)&3)
          {
            case 1:  //yes, 1 for layer 3
              return AILFILETYPE_MPEG_L3_AUDIO;
            case 2:
              type=AILFILETYPE_MPEG_L2_AUDIO;
              break;
            case 3:  //yes, 3 for layer 1
              type=AILFILETYPE_MPEG_L1_AUDIO;
              break;
          }
          src+=3; // skip this DWORD
        }
        else
        {
          src+=3; // skip this DWORD
        }
      }
      ++src;
    }

    if (type != 0)
      return( type );
  }

  //
  // Check for MIDI files by scanning entire file for 'MThd' header
  //
  // Note that this is VERY likely to falsely identify any long data
  // file (e.g., mummer.mp3) as a MIDI file, so it should be the very
  // last test performed before giving up
  //

  {
   U8    *src;
   U32 len;

   src = (U8    *)data;
   len = size;
   while (len-- >= 4L)
      {
      if (!AIL_strnicmp((char    *) src,"MThd",4))
         {
         return(AILFILETYPE_MIDI);
         }
      src=(U8    *)AIL_ptr_add(src,1);
      }

   }

  return(AILFILETYPE_UNKNOWN);
}


extern "C" S32 AILCALL AIL_API_file_type_named(void const    * data, char const    * filename, U32 size)
{
  if ( filename )
  {
    if ( match_suffix( "\0.v12",filename ) )
      return AILFILETYPE_V12_VOICE;
    else if ( match_suffix( "\0.v24",filename ) )
      return AILFILETYPE_V24_VOICE;
    else if ( match_suffix( "\0.v29",filename ) )
      return AILFILETYPE_V29_VOICE;
    else if ( match_suffix( "\0.SPEEX8",filename ) )
      return AILFILETYPE_S8_VOICE;
    else if ( match_suffix( "\0.SPEEX16",filename ) )
      return AILFILETYPE_S16_VOICE;
    else if ( match_suffix( "\0.SPEEX32",filename ) )
      return AILFILETYPE_S32_VOICE;
  }

  return( AIL_API_file_type( data, size ) );
}


//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  MSSSTRM.C: High-level streaming API                                   ##
//##                                                                        ##
//##  Version 1.00 of 19-Jan-97: Initial                                    ##
//##  Version 1.01 of 11-May-97: Added ADPCM support (Serge Plagnol)        ##
//##  Version 1.10 of 1-Jul-98: Added ASI codec support (John Miles)        ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################
#include <stdlib.h>

#include "mss.h"
#include "imssapi.h"

#include "rrThreads.h"

//############################################################################
//##                                                                        ##
//## Locked code                                                            ##
//##                                                                        ##
//############################################################################

static HSTREAM streams=0;
static HTIMER timer=(HTIMER)-1;
static S32 stream_count=0;


#define BUFFERGRANULARITY (4*1024L)

static S32 start_IOs_if_we_can( HSTREAM stream );

static S32 AILCALLBACK DefaultAsyncStatus(MilesAsyncRead* async, S32 wait, U32* BytesRead)
{
  MilesAsyncRead* pAsync = (MilesAsyncRead*)async;

  S32 Complete = MilesAsyncFileStatus(pAsync, wait ? ~0U : 0);

  *BytesRead = (U32)pAsync->ReadAmt;

  if (Complete == 0) return 0;

  S32 Status = pAsync->Status;

  if (Status == MSSIO_STATUS_ERROR_CANCELLED) return 0;    // cancellation==incomplete, e.g., when doing a seek

  if (Status & MSSIO_STATUS_ERROR_MASK)
  {
    *BytesRead = 0;
    return 1;
  }

  return 1;
}

static void handle_asyncs( HSTREAM stream, S32 wait)
{
  U32 i;

  for( i = 0 ; i < MSS_STREAM_CHUNKS ; i++ )
  {
    if (stream->asyncs_loaded[i] == 1 )
    {
      U32 amt;

      if (DefaultAsyncStatus(stream->asyncs + i,wait,&amt))
      {
        stream->asyncs_loaded[i] = 2;
        if (amt==0)
        {
          stream->error=1;
          AIL_stop_sample( stream->samp );
        }
      }
    }
  }
}

static void cancel_asyncs( HSTREAM stream )
{
  U32 i;

  for( i = 0 ; i < MSS_STREAM_CHUNKS ; i++ )
  {
    if (stream->asyncs_loaded[i] == 1 )
      MilesAsyncFileCancel( &stream->asyncs[ i ] );
  }
}


//############################################################################
//##                                                                        ##
//## low level function to handle the buffer loads                          ##
//##                                                                        ##
//############################################################################

static S32 load_buffer_into_Miles(HSTREAM s)
{
  U32 index = s->into_Miles_index;

  //
  // Check the async buffer for completion, if in effect
  //

  handle_asyncs(s,0);

  // are we ready to load buffers into the miles?
  if (((s->loadedsizes[index]) || (s->alldone==1)) && (s->playcontrol==1) && ( s->asyncs_loaded[ index ] == 2 )) 
  {
    // is the whole buffer preloaded?  if so, just send to miles and return
    if (s->preload) 
    {
      AIL_set_sample_address(s->samp,s->bufs[ index ],s->loadedsizes[ index ] );

      if (s->preloadpos)
      {
        AIL_set_sample_position(s->samp,s->preloadpos);
        s->preloadpos=0;
      }
      s->asyncs_loaded[ index ] = 0;

      AIL_resume_sample(s->samp);

      s->noback=1; // prevents any more background processing
    } 
    else 
    {

      InMilesMutex();

      // ok, we're streaming, is there a buffer ready?
      S32 bn=AIL_sample_buffer_available(s->samp); 

      if (bn) {
       
        ++s->into_Miles_index;
        if ( s->into_Miles_index >= MSS_STREAM_CHUNKS )
          s->into_Miles_index = 0;

        // yup, load a buffer into miles

        if (s->reset_ASI[ index ])
        {
          // on a reset block, the true size is stored in the reset_ASI
          s->loadedsizes[ index ] = s->reset_ASI[ index ] - 1;
          s->reset_ASI[ index ] = 0;
          AIL_request_EOB_ASI_reset( s->samp, index, s->reset_seek_pos[ index ] );
        }

        AIL_load_sample_buffer( s->samp, index, s->bufs[ index ], s->loadedsizes[ index ]);
        s->samp->head = s->into_Miles_index;
        s->asyncs_loaded[ index ] = 0;
        OutMilesMutex();

        if ( ( s->primeleft <= s->loadedsizes[ index ] ) && ( s->playcontrol == 1 ) && ( s->error == 0 ) )
        {
          s->primeleft = 0;
          AIL_start_sample( s->samp );
        }
        else
        {
          s->primeleft -= s->loadedsizes[ index ];
        }
        
        if ((s->alldone==1) && (s->loadedsizes[ index ]==0)) {
          s->alldone=2;

          if (s->callback)
            s->docallback=1;
        }
        s->loadedsizes[ index ] = 0;        
        return 1;
      }
      else
      {
        OutMilesMutex();
      }
    }
  }

  return 0;
}

//############################################################################
//##                                                                        ##
//## Background sound streaming function                                    ##
//##                                                                        ##
//############################################################################

static void AILLIBCALLBACK background(UINTa junk)
{
  HSTREAM s=streams;
  HSTREAM n;

  junk=junk;

  InMilesMutex();  // should always already be taken

  {
    while (s) 
    {
      if (s->noback==0) 
      {
        if (s->autostreaming==2)
        {
          start_IOs_if_we_can( s );   // start async IO here
        }

        while ( load_buffer_into_Miles(s) )
          ;

        n =(HSTREAM)s->next;
  
        if (s->docallback)
        {
          if (s->callback)
          {
            if (AIL_sample_status(s->samp) != SMP_PLAYING)
            {
              s->docallback=0;
              MSS_do_cb1( (AILSTREAMCB),s->callback,s->samp->driver->callingDS,
                            s);
            }
            n = streams; // reset to the beginning of the list after a callback
          }
        }
      }
      else
      {
        n =(HSTREAM)s->next;
      }
      s=n;
    }
  }

  OutMilesMutex();
}

//############################################################################
//##                                                                        ##
//## Background sound streaming function                                    ##
//##                                                                        ##
//############################################################################

extern "C" void stream_background(void)  // called by AIL_serve()
{
  HSTREAM s=streams;

  while (s)
  {
    if (s->autostreaming==2)
      AIL_API_service_stream(s,0);

    s=(HSTREAM)s->next;
  }
}


//############################################################################
//##                                                                        ##
//## Set the flags based on the .WAV file's sound type                      ##
//##                                                                        ##
//############################################################################

static S32 setflags(HSTREAM stream, U32 type, U8 *subtype, U32 bits, U32 chans, U32 mask)
{
  type = type;

  stream->filemask = mask;

   if (bits==4) 
   {
      stream->totallen&=~((4*chans)-1);
      stream->filetype=DIG_F_16BITS_MASK|DIG_F_ADPCM_MASK;
   } 
   else 
   {
      stream->totallen&=~((bits*chans/8)-1);

      if (bits==16) 
      {
         stream->filetype=DIG_F_16BITS_MASK;
      } 
      else if (bits==8) 
      {
         stream->filetype=0;
      } 
      else
         return(0);
   }

   if (chans==2)
      stream->filetype|=DIG_F_STEREO_MASK;
   else if (chans > 2)
   {
      stream->filetype |= (DIG_F_MULTICHANNEL_MASK | (chans << 16));

      const U8 KSDATAFORMAT_SUBTYPE_PCM[16] = 
      { 
         0x01,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71,
      };

      for (S32 i=0; i < 16; i++)
      {
         if (subtype[i] != KSDATAFORMAT_SUBTYPE_PCM[i])
         {
            return(0);
         }
      }
   }

   AIL_set_sample_playback_rate(stream->samp,stream->filerate);
   SS_set_sample_type(stream->samp,stream->filetype,stream->filemask);
   AIL_set_sample_adpcm_block_size(stream->samp,stream->blocksize);

   return(1);
}


//############################################################################
//##                                                                        ##
//## Open the WAVE file itself                                              ##
//##                                                                        ##
//############################################################################

static S32 openwavefile(HSTREAM stream, C8 const *filename)
{
   U32 tag,i,pos,type;
   U8  subtype[16];
   U32 bits,chans,rate,mask;
   S32 tried_id3v2 = 0;

   //
   // Assume stream is uncompressed raw data with no gaps in its channel mask
   //

   stream->block_oriented = 0;
   stream->using_ASI = 0;
   stream->filemask = ~0U;

   //
   // Search for ASI codec capable of processing this input file type
   //
   // If no specific provider available for this file type, try the default
   // .WAV file handler
   //

   HPROVIDER HP = RIB_find_file_provider("ASI codec",
                                         "Input file types",
                                          filename);

   if (HP)
   {
      //
      // Set sample format according to file name and contents
      //
      // Load first chunk of file to determine the output data format,
      // then rewind the file to its beginning
      //

      #if defined(IS_WII) || defined(IS_PS2) || defined(IS_PS3)
        U8 __attribute__ ((aligned(32))) temp_buffer[AIL_MAX_FILE_HEADER_SIZE];
      #else
        U8 temp_buffer[AIL_MAX_FILE_HEADER_SIZE];
      #endif

      stream->startpos = MSS_seek(stream->fileh,0,AIL_FILE_SEEK_CURRENT);
      stream->totallen = MSS_seek(stream->fileh,0,AIL_FILE_SEEK_END)-stream->startpos;
      MSS_seek(stream->fileh,stream->startpos,AIL_FILE_SEEK_BEGIN);

domp3:

      i=MSS_read(stream->fileh,
                 temp_buffer,
                 AIL_MAX_FILE_HEADER_SIZE);

      if (!i)
      {
         return 0;
      }

      MSS_seek(stream->fileh,stream->startpos,AIL_FILE_SEEK_BEGIN);

      //
      // Set up to use specified ASI codec to decode data for mixer
      //
      // We must first set the address of the sample to the temp header
      // buffer, so the sample parameters (rate, width, channels, etc.) can
      // be determined correctly.  The buffered data is ignored (and
      // should be considered invalid) after openwavefile() returns.
      //

      AIL_set_sample_address(stream->samp,
                             temp_buffer,
                             i);

      if (AIL_set_sample_processor(stream->samp,
                                   SP_ASI_DECODER,
                                   HP) == (HPROVIDER) -1)
      {
         return 0;
      }

      //
      // Get handle to this HSAMPLE's decompression pipeline stage
      //

      stream->ASI = &stream->samp->pipeline[SP_ASI_DECODER].TYPE.ASI;

      if (stream->ASI->stream==0)
      {
        if ( !tried_id3v2 )
          goto try_id3v2;
        return( 0 );
      }

      //
      // Log streamed file properties
      //
      // datarate=bytes consumed per second
      // filerate=final output sample rate
      //

      stream->blocksize = 1; stream->ASI->ASI_stream_property(stream->ASI->stream, stream->ASI->MIN_INPUT_BLOCK_SIZE, &stream->blocksize, 0, 0 );

      MSS_seek(stream->fileh,stream->startpos,AIL_FILE_SEEK_BEGIN);

      if (stream->ASI->DATA_START_OFFSET != (UINTa) ~0)
      {
         stream->startpos = (U32)~0; stream->ASI->ASI_stream_property(stream->ASI->stream, stream->ASI->DATA_START_OFFSET, &stream->startpos, 0, 0);
         MSS_seek(stream->fileh,stream->startpos,AIL_FILE_SEEK_BEGIN);

         // NB: in XMA, startpos is now 0x3c (past the wavehdr chunk)
      }

      if (stream->ASI->DATA_LEN != (UINTa) ~0)
      {
         stream->totallen = ~0; stream->ASI->ASI_stream_property(stream->ASI->stream, stream->ASI->DATA_LEN, &stream->totallen,0,0);
      }

      stream->ASI->ASI_stream_seek( stream->ASI->stream, 0 );     // Rewind stream and force output-buffer flush

      if (stream->ASI->STREAM_SEEK_POS != (UINTa) ~0)
      {
         U32 offset = 0;

         stream->ASI->ASI_stream_property(stream->ASI->stream,
                                          stream->ASI->STREAM_SEEK_POS,
                                          0,&offset,&offset);

         if (offset != 0) // if stream does not start immediately after the header for some reason,
         {                // seek to where it does begin
            MSS_seek(stream->fileh,stream->startpos+offset,AIL_FILE_SEEK_BEGIN);
         }
      }

      stream->filerate = -1; stream->ASI->ASI_stream_property(stream->ASI->stream, stream->ASI->OUTPUT_SAMPLE_RATE,&stream->filerate,0,0);
      stream->datarate = (U32)~0; stream->ASI->ASI_stream_property(stream->ASI->stream, stream->ASI->INPUT_BIT_RATE,&stream->datarate,0,0);
      stream->datarate /= 8;

      U32 chans = (U32)~0; stream->ASI->ASI_stream_property(stream->ASI->stream, stream->ASI->OUTPUT_CHANNELS,&chans,0,0);
      U32 bits  = (U32)~0; stream->ASI->ASI_stream_property(stream->ASI->stream, stream->ASI->OUTPUT_BITS,&bits,0,0);
      U32 mask  = ~0U;

      if (stream->ASI->OUTPUT_CHANNEL_MASK != (UINTa)~0)
      {
         stream->ASI->ASI_stream_property(stream->ASI->stream, stream->ASI->OUTPUT_CHANNEL_MASK,&mask,0,0);
      }

      //
      // Reset sample position to throw away data resulting from temp buffer decode
      //

      stream->samp->buf[0].pos = 0;
      stream->samp->buf[0].len = 0;
      stream->samp->buf[1].pos = 0;
      stream->samp->buf[1].len = 0;

      if ((stream->datarate==0) || (chans==0))
      {
        if ( !tried_id3v2 )
          goto try_id3v2;
        return( 0 );
      }

      if (bits==16)
      {
         stream->filetype  = DIG_F_16BITS_MASK;
      }
      else if (bits==8)
      {
         stream->filetype  = 0;
      }

      if (chans==2)
      {
         stream->filetype |= DIG_F_STEREO_MASK;
      }
      else if (chans > 2)     // TODO: this means that 2-channel files files can't be multichannel streams; e.g.,
      {                       // are always front left/front right.  Fix later if needed?
         stream->filetype |= (DIG_F_MULTICHANNEL_MASK | (chans << 16));
         stream->filemask = mask;
      }

      //
      // Set flag to indicate block-oriented (i.e., compressed) stream
      //

      stream->using_ASI = 1;
      stream->block_oriented = 1;

      return 1;
   }
   else
   {
      //
      // ugly low-level wave file parse with a minimum of i/o
      //

      MSS_seek(stream->fileh,stream->startpos,AIL_FILE_SEEK_BEGIN);  // (account for nonzero start offset in filename)

      i=MSS_read(stream->fileh,&tag,4);

      if ( LE_SWAP32( &tag )==0x46464952) 
      {  // 'FFIR'
         i=MSS_read(stream->fileh,&tag,4);
         i=MSS_read(stream->fileh,&tag,4);

         if ( LE_SWAP32( &tag )==0x45564157) 
         {  // 'EVAW'
            i=MSS_read(stream->fileh,&tag,4);

            while ( LE_SWAP32( &tag )!=0x20746d66) 
            {  // ' tmf'
               i=MSS_read(stream->fileh,&tag,4);
               if (i!=4)
                 return(0);
               tag=( LE_SWAP32( &tag )+1)&~1;
               MSS_seek(stream->fileh,tag,AIL_FILE_SEEK_CURRENT);
               i=MSS_read(stream->fileh,&tag,4);
            }

            i=MSS_read(stream->fileh,&pos,4);
            pos=LE_SWAP32( &pos )+ MSS_seek(stream->fileh,0,AIL_FILE_SEEK_CURRENT);

            i=MSS_read(stream->fileh,&type,4);

            MEM_LE_SWAP32( &type );

            chans=type>>16;

            type&=0xffff;
            i=MSS_read(stream->fileh,&rate,4);     // nSamplesPerSec
            MEM_LE_SWAP32( &rate );

            i=MSS_read(stream->fileh,&tag,4);      // nAvgBytesPerSec
            i=MSS_read(stream->fileh,&tag,4);      // block align/bits per sample
            MEM_LE_SWAP32( &tag );

            bits=tag>>16;
            stream->blocksize=tag&0xFFFF;

            if (type != WAVE_FORMAT_EXTENSIBLE)       // Normal .WAV
            {
               mask = ~0U;                            // (no gaps in channel mask)
            }
            else                                      // Multichannel PCM .WAV
            {
               i=MSS_read(stream->fileh,&tag,4);      // cbSize/wSamplesPerBlock
               if (i!=4) return 0;
               MEM_LE_SWAP32( &tag );
               U32 cbSize = tag & 0xffff;
               if (cbSize < 22) return 0;

               i=MSS_read(stream->fileh,&mask,4);     // dwChannelMask
               if (i!=4) return 0;
               MEM_LE_SWAP32( &mask );
               
               i=MSS_read(stream->fileh,subtype,16);  // SubFormat GUID
               if (i!=16) return 0;
            }

            MSS_seek(stream->fileh,pos,AIL_FILE_SEEK_BEGIN);
            i=MSS_read(stream->fileh,&tag,4);

            while ( LE_SWAP32( &tag )!=0x61746164)   // 'atad'
            {
               i=MSS_read(stream->fileh,&tag,4);
               if (i!=4)
                  return(0);
               tag=( LE_SWAP32( &tag )+1)&~1;
               MSS_seek(stream->fileh,tag,AIL_FILE_SEEK_CURRENT);
               i=MSS_read(stream->fileh,&tag,4);
            }

            i=MSS_read(stream->fileh,&stream->totallen,4);
            MEM_LE_SWAP32( &stream->totallen );
            stream->startpos=MSS_seek(stream->fileh,0,AIL_FILE_SEEK_CURRENT);

            stream->filerate=rate;
            stream->datarate=rate*bits*chans/8;

            if ((bits != 8) && (bits != 16))
            {
               stream->block_oriented = 1;
            }

            if ( (type!=1) && (type != WAVE_FORMAT_EXTENSIBLE)
               && ((type!=0x11) || (bits!=4)) )
            {
               HP=RIB_find_file_dec_provider( "ASI codec",
                                              "Input wave tag",
                                              type,
                                              "Output file types",
                                              ".raw");
               if (HP)
                 goto domp3;

               return(0);
            }

            return(setflags(stream,type,subtype,bits,chans,mask));
         }
      }
      else
      {
         U8 buffer[16];
          
try_id3v2:
         tried_id3v2 = 1;

         MSS_seek(stream->fileh,stream->startpos,AIL_FILE_SEEK_BEGIN);

         i=MSS_read(stream->fileh,
                    buffer,
                    16 );

         if (!i)
         {
            return 0;
         }

         if ( ( buffer[ 0 ] == 0x49 ) && ( buffer[ 1 ] == 0x44 ) && ( buffer[ 2 ] == 0x33 ) &&
              ( buffer[ 3 ] < 0xff ) && ( buffer[ 4 ] < 0xff ) &&
              ( buffer[ 6 ] < 0x80 ) && ( buffer[ 7 ] < 0x80 ) && ( buffer[ 8 ] < 0x80 ) && ( buffer[ 9 ] < 0x80 ) )
         {
            stream->startpos = 10 + ( ( (U32) buffer[ 9 ] ) | ( ( (U32) buffer[ 8 ] ) << 7 ) | ( ( (U32) buffer[ 7 ] ) << 14 ) | ( ( (U32) buffer[ 6 ] ) << 21 ) );
            stream->totallen -= stream->startpos;
            MSS_seek(stream->fileh,stream->startpos,AIL_FILE_SEEK_BEGIN);
            goto domp3;
         }

         return( 0 );
      }
   }

  return(0);
}

//############################################################################
//##                                                                        ##
//## Open a streaming file                                                  ##
//##                                                                        ##
//############################################################################

HSTREAM  AILCALL AIL_API_open_stream(HDIGDRIVER dig, char const * filename, S32 stream_mem)
{
   HSTREAM stream;
   S32 mini;

   if ((dig==0) || (filename==0))
     return(0);

   //
   // Allocate memory for STREAM structure
   //

   stream = (HSTREAM) AIL_mem_alloc_lock(sizeof(*stream));

   if (stream == 0)
   {
      AIL_set_error("Out of memory.");
      return 0;
   }

   AIL_memset(stream, 0, sizeof(*stream));
   stream->next_read_offset = -1; // the open code will set us to the correct offset.

   OutMilesMutex();
   if (!MSS_open(filename,&stream->fileh))
     stream->fileh=(U32)-1;
   InMilesMutex();

   if (stream->fileh==(U32)-1)
   {
      AIL_set_error("Unable to open file.");
   error:
      AIL_mem_free_lock(stream);
      return(0);
   }

   stream->samp=AIL_allocate_sample_handle(dig);

   if (stream->samp==0)
   {
      AIL_set_error("Unable to obtain a sample handle.");
   errorclose:
      if (stream->fileh!=(U32)-1)
        MSS_close(stream->fileh);
      goto error;
   }

   AIL_init_sample(stream->samp, DIG_F_MONO_16);   // temporarily establish a valid format

   OutMilesMutex();

   if (!openwavefile(stream,filename))
   {
      InMilesMutex();
      AIL_set_error("Error getting sound format.");
     releaseclose:
      AIL_release_sample_handle(stream->samp);
      goto errorclose;
   }

   InMilesMutex();

   //
   // figure bufsize and do mallocs
   //

   if (stream_mem==0)
     stream->bufsize=stream->datarate/MSS_STREAM_CHUNKS;
   else
     stream->bufsize=stream_mem/MSS_STREAM_CHUNKS;

   mini=AIL_minimum_sample_buffer_size(dig, stream->filerate, stream->filetype);
   if (stream->bufsize<mini)
     stream->bufsize=mini;

   if (stream->using_ASI)
   {
      HPROPERTY min_stream = 0;
      RIB_INTERFACE_ENTRY ASISTR[] = {{ RIB_PROPERTY,   "Minimum input stream size",  (UINTa) &min_stream,      RIB_NONE }};
      
      SPINFO* SP = &stream->samp->pipeline[SP_ASI_DECODER]; 
      RIB_request(SP->provider,"ASI stream",ASISTR);

      S32 asi_min = 0; stream->ASI->ASI_stream_property(stream->ASI->stream, min_stream, &asi_min, 0, 0 );
      if (stream->bufsize<asi_min)
        stream->bufsize=asi_min;
   }

   stream->bufsize=(stream->bufsize+(BUFFERGRANULARITY-1))&~(BUFFERGRANULARITY-1);


   // If we are not block oriented, we need to slurp in on sample boundaries.
   if (!stream->block_oriented)
   {
       U32 BytesPerSample = ((stream->filetype&DIG_F_16BITS_MASK) ? 2 : 1) * stream->samp->n_channels;
       stream->bufsize = stream->bufsize - (stream->bufsize % BytesPerSample);
   }

   if (stream->bufsize==0)
     stream->bufsize=BUFFERGRANULARITY;

   if ((S32)(stream->totallen-(stream->bufsize*3))<(S32)(BUFFERGRANULARITY*3)) 
   {
     stream->primeamount=stream->bufsize=stream->totallen;
     stream->bufs[0]=(U8*)AIL_mem_alloc_lock(stream->totallen);
     if (stream->bufs[0]==0)
     {
       AIL_set_error("Out of memory.");
       goto releaseclose;
     }

     stream->preload=1;

   } 
   else 
   {
     U32 i;

     stream->primeamount=mini*2;
     if (stream->primeamount<(stream->bufsize*(MSS_STREAM_CHUNKS-1)))
       stream->primeamount=stream->bufsize*(MSS_STREAM_CHUNKS-1);

     stream->bufs[0]=(U8*)AIL_mem_alloc_lock( (stream->bufsize+64) * MSS_STREAM_CHUNKS);
     if (stream->bufs[0]==0) 
     {
       AIL_set_error("Out of memory.");
       goto releaseclose;
     }

     for( i = 1; i < MSS_STREAM_CHUNKS ; i++ )
     {
       stream->bufs[ i ] = (U8*) AIL_ptr_add( stream->bufs[ i - 1 ],  stream->bufsize+64 );
     }
   }

   
   //
   // setup the buffering callback
   //

   // This has been removed since hitting the io system from the mixer thread doesn't happen anymore.
   //AIL_register_EOB_callback(stream->samp,(AILSAMPLECB)background);
   stream->samp->system_data[STR_HSTREAM] = (UINTa)stream;

   AIL_set_sample_buffer_count(stream->samp, MSS_STREAM_CHUNKS );

   stream->sublen=0x7fffffff;
   stream->substart=0;

   stream->loopsleft=1;
   stream->primeleft=stream->primeamount;
   
   stream->autostreaming = 1;

   stream->last_level = -1;
   stream->percent_mult = 1.0f / ( stream->bufsize * MSS_STREAM_CHUNKS );
   stream->stream_count = stream_count++;

   //
   // insert this stream into the linked list
   //

   if (streams==0) 
   {
     //
     // start background timer;
     //

     timer=AIL_register_timer( background );
     if (timer==(HTIMER)-1) 
     {
       AIL_set_error("Out of timer handles.");
       AIL_mem_free_lock(stream->bufs[0]);
       goto releaseclose;
     }

     AIL_set_timer_frequency(timer,16);

     AIL_start_timer(timer);

     streams=stream;

   } 
   else 
   {
     stream->next=(void*)streams;
     streams=stream;
   }

   return(stream);
}

//############################################################################
//##                                                                        ##
//## Close a streaming file                                                 ##
//##                                                                        ##
//############################################################################

void  AILCALL AIL_API_close_stream(HSTREAM stream)
{
  if (stream)
  {
    //
    // lock out the streaming thread
    //

    InMilesMutex();

    if (stream->samp)
    {
      HSAMPLE samp=stream->samp;
      stream->samp=0;
      AIL_release_sample_handle(samp);
    }

    //
    // remove from the linked list
    //

    if (streams==stream)
      streams=(HSTREAM)streams->next;
    else
    {
      HSTREAM s=streams;
      while (s)
      {
        if (s->next==(void*)stream)
        {
          s->next=stream->next;
          break;
        }
        s=(HSTREAM)s->next;
      }
    }

    if (streams==0)
    {
      AIL_release_timer_handle(timer);
      timer=0;
    }

    OutMilesMutex();
    
    //
    // Wait for the async I/O to finish...
    //

    cancel_asyncs( stream );
    handle_asyncs( stream, RR_WAIT_INFINITE );

    if (stream->bufs[0])
      AIL_mem_free_lock(stream->bufs[0]);

    if (stream->fileh!=(U32)-1)
    {
      MSS_close(stream->fileh);
    }

    AIL_memset(stream,0,sizeof(*stream));
    AIL_mem_free_lock(stream);

  }
}


//############################################################################
//##                                                                        ##
//## Gets the status of a streaming file                                    ##
//##                                                                        ##
//############################################################################

S32  AILCALL AIL_API_stream_status(HSTREAM stream)
{
  if (stream)
  {
    if (stream->error==0)
    {
      if (stream->was_popped)
        return SMP_DONE;

      if ( ( stream->playcontrol & 8 ) || ( stream->playcontrol == 0 ) )
        return SMP_STOPPED;

      if ( stream->alldone && ( AIL_API_sample_loaded_len( stream->samp ) == 0 ) )
      {
        U32 index = stream->into_Miles_index;
        if (((stream->loadedsizes[index]) || (stream->alldone==1)) && (stream->playcontrol==1) && ( stream->asyncs_loaded[ index ] )) 
          return SMP_PLAYING;
        return SMP_DONE;
      }
      return SMP_PLAYING;
    }
  }
  return(-1);
}


//############################################################################
//##                                                                        ##
//## What percentage full, is the IO buffer                                 ##
//##                                                                        ##
//############################################################################

F32  AILCALL AIL_API_stream_filled_percent(HSTREAM stream)
{
  if (stream)
  {
    if (stream->error==0)
    {
      if ( ( stream->preload ) || ( stream->primeleft == 0 ) )
        return 1.0f;
      
      return stream->level;
    }
  }

  return 0.0f;
}


static F32 AILCALLBACK MilesStreamCB(void* i_Stream)
{
  return AIL_API_stream_filled_percent( (HSTREAM) i_Stream );
}

//############################################################################
//##                                                                        ##
//## low-level buffer reading                                               ##
//##                                                                        ##
//############################################################################

#define set_file_line( a ) a.caller = __FILE__; a.caller_line = __LINE__;
      
static S32 doread(HSTREAM stream,S32 cur, S32* size)
{
  U32 amt,read;

  amt=stream->totallen-stream->async_pos;
  if (amt > (U32)stream->bufsize )
    amt=stream->bufsize;

  read=stream->sublen-stream->async_pos;
  if (amt > read)
    amt=read;

  // make sure that we have a buffer - if we don't, the async layer allocates one, which leaks.
  if (amt && stream->bufs[cur])
  {
    U32 align_to_next_2k = ( stream->async_pos+stream->startpos ) & 2047;
    if ( align_to_next_2k && !stream->preload ) // a preload is small enough to be loaded in one read, so don't try to align as this will split it over two reads.
    {
      if ( ( align_to_next_2k < amt ) )
      {
          if ( (amt - align_to_next_2k) > 2048)
          {
              S32 gran = AIL_API_sample_granularity( stream->samp );
              if ( ( ( align_to_next_2k) % (  gran ) ) == 0 )
              {
                  amt -= align_to_next_2k;
              }
          }
      }
    }

    if ( amt )
    {
      //
      // Only launch one async buffer per buffer
      //
      AIL_memset(stream->asyncs + cur, 0, sizeof(MilesAsyncRead));
      stream->asyncs[cur].Buffer = stream->bufs[cur] + *size;
      stream->asyncs[cur].FileHandle = stream->fileh;
      stream->asyncs[cur].Count = amt;
      stream->asyncs[cur].StreamCB = MilesStreamCB;
      set_file_line( stream->asyncs[cur] );
      if (stream->next_read_offset != -1)
      {
          stream->asyncs[cur].Flags = MSSIO_FLAGS_DONT_CLOSE_HANDLE;
          stream->asyncs[cur].Offset = stream->next_read_offset;
          stream->next_read_offset = -1;
      }
      else
      {
          stream->asyncs[cur].Flags = MSSIO_FLAGS_DONT_CLOSE_HANDLE | MSSIO_FLAGS_DONT_USE_OFFSET;
      }
      stream->asyncs_loaded[cur] = 1;
      MilesAsyncFileRead(stream->asyncs + cur);
    }
 
    read=amt;
  }
  else
    read=0;

  stream->bufstart[cur]=stream->async_pos;

  stream->async_pos+=read;
  *size+=read;

  if (read!=amt)
    stream->error=1;

  //
  // are we at the end of the subblock loop?
  //

  if (stream->sublen==(S32)stream->async_pos)
  {
    if (stream->loopsleft)
      --stream->loopsleft;

    if (stream->loopsleft==1)
      stream->sublen=0x7fffffff;
    
    stream->async_pos=stream->substart;

    stream->next_read_offset = stream->substart+stream->startpos;

    //
    // If looping back, flush the ASI output buffer
    //

    if ( (stream->using_ASI) || (stream->block_oriented) )
    {
      // some dummy sound for the reset (this only happens when you seek to the end of a looping stream
      if ( *size == 0 )
      {
        *size = 32;
        AIL_memset(stream->bufs[cur], 0, 32);
      }

      // save the true size in reset_ASI
      stream->reset_ASI[cur]=(*size)+1;
      stream->reset_seek_pos[cur] = stream->substart;
    }
  }

  //
  // are we at the end of the file?
  //

  if (stream->totallen==(S32)stream->async_pos) 
  {
    if (stream->loopsleft==1)
      stream->alldone=1;
    else 
    {
      if(stream->loopsleft)
        --stream->loopsleft;

      stream->async_pos=0;
      stream->next_read_offset = stream->startpos;

      //
      // If looping back, flush the ASI output buffer
      //

      if ( (stream->using_ASI) || (stream->block_oriented) )
      {
        // some dummy sound for the reset (this only happens when you seek to the end of a looping stream
        if ( *size == 0 )
        {
          *size = 32;
          AIL_memset(stream->bufs[cur], 0, 32);
        }

        // save the true size in reset_ASI
        stream->reset_ASI[cur]=(*size)+1;
        stream->reset_seek_pos[cur] = 0;
      }
    }
  }

  return(read);
}


static S32 start_IOs_if_we_can( HSTREAM stream )
{
  S32 amt;
  S32 tamt=0;
  S32 i;

  if (stream->noback)
  {
     return 0;
  }

  S32 num = AIL_API_sample_loaded_len( stream->samp );
      
  // if we are preloading the file, there is only one buffer.
  if (stream->preload)
  {
    num = 1;
  }
  else
  {
    // always restart in case an SMP_DONE leaks in after a seek
    if ( ( num ) && ( stream->playcontrol == 1 ) && ( stream->error == 0 ) && ( ( num > stream->primeleft ) || ( stream->async_pos >= (U32) stream->totallen ) ) )
    {
      stream->primeleft = 0;
      AIL_start_sample( stream->samp );
    }

    stream->level = stream->percent_mult * (F32)num;
    if ( stream->last_level != stream->level )
    {
      tmPlot(miles_context, TMPT_PERCENTAGE_DIRECT, TMPF_NONE, stream->level, "Miles/Streams/Stream %d %%", stream->stream_count );
      stream->last_level = stream->level;
    }
  }

  for( i = 0 ; i < MSS_STREAM_CHUNKS ; i++ )
  {
    // is there a spot to read into?
    if ( ( stream->asyncs_loaded[ stream->read_IO_index ] ) || ( stream->samp->buf[ stream->read_IO_index ].pos != stream->samp->buf[ stream->read_IO_index ].len ) )
      break;

    amt=doread(stream,stream->read_IO_index,&stream->loadedsizes[ stream->read_IO_index ]);
    if (stream->error)
      return(-1);
    if ( amt == 0 )
      return -1;

    ++stream->read_IO_index;
    if ( stream->read_IO_index >= MSS_STREAM_CHUNKS )
      stream->read_IO_index = 0;

    tamt+=amt;
  }

  if ((stream->preload) && (stream->fileh!=(U32)-1)) 
  {
    stream->autostreaming=0;
    // leave the movie open if we will be waiting on an async IO
  }

  if (stream->autostreaming==1) // switch to full autostreaming when
    stream->autostreaming=2;    //    started (from standby auto)
  
  return( tamt );
}

//############################################################################
//##                                                                        ##
//## Service a streaming file                                               ##
//##                                                                        ##
//############################################################################

static U32 volatile noreenterserve=0;

S32  AILCALL AIL_API_service_stream(HSTREAM stream, S32 fillup)
{
  S32 tamt=0;

  if ((stream) && (stream->samp))
  {
    InMilesMutex();

    {
      if ((stream->error==0) && (stream->alldone==0) && (stream->samp))
      {
         tamt = start_IOs_if_we_can( stream );

         OutMilesMutex();

         if ( fillup )
         {
           OutMilesMutex();  // this is to release the lock in mssdbg
           handle_asyncs( stream, RR_WAIT_INFINITE );
           InMilesMutex();
         }

         return(tamt);
      }
    }

    OutMilesMutex();
  }

  return(-1);
}


// align position for a seek
static S32 stream_align(HSTREAM stream,S32 offset)
{
  if (!stream->using_ASI)
  {
    if (stream->block_oriented)
    {
      S32 aligned;

      aligned = (offset / stream->blocksize) * stream->blocksize;

      if((offset - aligned) > ((S32)stream->blocksize/2))
      {
        aligned += stream->blocksize;
      }

      offset = aligned;
    }
    else
    {
      offset = offset - (offset % ((((stream->filetype&DIG_F_16BITS_MASK)?2:1)*stream->samp->n_channels)));
    }
  }

  if (offset>stream->totallen)
  {
    offset=stream->totallen;
  }

  return( offset );
}


//############################################################################
//##                                                                        ##
//## Set the position of a streaming file                                   ##
//##                                                                        ##
//############################################################################

void  AILCALL AIL_API_set_stream_position(HSTREAM stream,S32 offset)
{

  if (stream) 
  {
    offset=stream_align(stream,offset);

    if ( offset == AIL_stream_position( stream ) )
      return;

    if ((stream->using_ASI) && (stream->ASI->STREAM_SEEK_POS != (UINTa) ~0))
    {
       //
       // Tell codec our seek position (and give it a chance to align it)
       //
       // Note that offset 0 is at the end of a WAVEHDR, ID3 tag, or other
       // metadata at the beginning of the stream file image
       //

       stream->ASI->ASI_stream_property(stream->ASI->stream,
                                        stream->ASI->STREAM_SEEK_POS,
                                        0,&offset,&offset);
    }

    // handle the simple case first (preloaded)
    if ( stream->preload ) 
    {
      if ( stream->primeleft != stream->primeamount )
        AIL_set_sample_position( stream->samp, offset );
      else
      {
        stream->preloadpos=offset;
      }
      return;
    }

    // non-preloaded
    stream->samp->adpcm.blockleft = 0;
    stream->samp->adpcm.extrasamples = 0;

    S32 t;
  
    t = stream->samp->tail;
    
    if ( ( t == stream->samp->head ) && ( !stream->samp->starved ) ) // we totally full?
      goto start_one_in;

    while( t != stream->samp->head )
    {
     start_one_in:   
      if ( ( stream->bufstart[ t ] <= offset ) && ( ( offset - stream->bufstart[ t ] ) < (S32) stream->samp->buf[ t ].len ) )
      {
        // found it in our buffer!

        // reset everything as if it played all the buffers
        while( stream->samp->tail != t )
        {
          stream->samp->buf[ stream->samp->tail ].pos = 32;
          stream->samp->buf[ stream->samp->tail ].len = 32;
          stream->asyncs_loaded[ stream->samp->tail ] = 0;
          stream->loadedsizes[ stream->samp->tail ] = 0;
          stream->reset_ASI[ stream->samp->tail ] = 0;
          stream->reset_seek_pos[ stream->samp->tail ] = 0;
          stream->bufstart[ stream->samp->tail ] = 0;
    
          ++stream->samp->tail;
          if ( stream->samp->tail >= stream->samp->n_buffers )
            stream->samp->tail = 0;
        }
        stream->samp->buf[ t ].pos = offset - stream->bufstart[ t ];

        if (stream->using_ASI) // need to reset ASI so it can resync
        {
          stream->ASI->ASI_stream_seek(stream->ASI->stream, -1);
        }

        return;
      }
      ++t;
      if ( t >= stream->samp->n_buffers )
        t = 0;
    }

    // ok, it wasn't in our buffers either

    //
    // Wait for the async I/O to finish...
    //

    cancel_asyncs( stream );
    handle_asyncs( stream, RR_WAIT_INFINITE );

    //
    // If stream has been started, set the buffer count to 
    // get the streaming buffers to reinit.
    //
    // If ASI codec in use, force frame resync at next read operation
    //

    if ( stream->primeleft != stream->primeamount )
    {
       AIL_set_sample_buffer_count( stream->samp, stream->samp->n_buffers );
       if (stream->using_ASI)
       {
         stream->ASI->ASI_stream_seek(stream->ASI->stream, -1);
       }
    }

    // reset everything
    stream->async_pos = offset;
    stream->next_read_offset = stream->startpos + offset;
    stream->into_Miles_index = 0;
    stream->read_IO_index = 0;
    stream->primeleft = stream->primeamount;
    stream->last_level = -1.0f;
    stream->level = 0.0f;

    for( t = 0 ; t < MSS_STREAM_CHUNKS ; t++ )
    {
      stream->asyncs_loaded[ t ] = 0;
      stream->loadedsizes[ t ] = 0;
      stream->reset_ASI[ t ] = 0;
      stream->reset_seek_pos[ t ] = 0;
      stream->bufstart[ t ] = offset;
    }

    if (stream->playcontrol != 1)
      stream->playcontrol = 0;
    stream->alldone = 0;
  }
}

//############################################################################
//##                                                                        ##
//## Get the position of a streaming file                                   ##
//##                                                                        ##
//############################################################################

S32  AILCALL AIL_API_stream_position(HSTREAM stream)
{
  if (stream) 
  {
    S32 pos;

    pos = AIL_sample_position(stream->samp);

    if ( !stream->preload)
      pos += stream->bufstart[ stream->samp->tail ];
   
    // just in case
    if ( pos > stream->totallen )
      pos = stream->totallen;

    return pos;
  }
  return(-1);
}

//############################################################################
//##                                                                        ##
//## Start the playback of a streaming file                                 ##
//##                                                                        ##
//############################################################################

void  AILCALL AIL_API_start_stream(HSTREAM stream)
{
  if (stream) 
  {
    if (stream->autostreaming==1)      // switch to full autostreaming when
      stream->autostreaming=2;         //    started (from standby auto)

    AIL_request_EOB_ASI_reset(stream->samp,(U32)~0,0);  // Cancel any pending reset requests

    AIL_set_stream_position( stream, 0 );

    if ((stream->preload) && (stream->noback == 1)) 
      {
      AIL_start_sample(stream->samp);
      } 

    stream->playcontrol = 1;
  }
}


//############################################################################
//##                                                                        ##
//## Pause the playback of a streaming file                                 ##
//##                                                                        ##
//############################################################################

void  AILCALL AIL_API_pause_stream(HSTREAM stream, S32 onoff)
{
  if (stream)
  {
    if (onoff) 
    {
      //
      // turn on pause
      //

      if (AIL_stream_status(stream)==SMP_PLAYING) {
        AIL_stop_sample(stream->samp);
        stream->playcontrol|=24; // 8 = stopped, 16=stopped the sample
      } else
        stream->playcontrol|=8;
    } 
    else 
    {
      //
      // turn off pause
      //

      if ((stream->playcontrol&7)==0)
      {
          if (stream->autostreaming==1)      // switch to full autostreaming when
              stream->autostreaming=2;         //    started (from standby auto)

          AIL_request_EOB_ASI_reset(stream->samp,(U32)~0,0);  // Cancel any pending reset requests
 
          if ((stream->preload) && (stream->noback==1))
          {
              AIL_start_sample(stream->samp);
          } 

          stream->playcontrol = 1;
      }
      else {
        if (stream->playcontrol&16)
          AIL_resume_sample(stream->samp);
        stream->playcontrol&=~24;
        background(0);
      }
   }
 }
}

//############################################################################
//##                                                                        ##
//## Changes the loop count of a streaming file                             ##
//##                                                                        ##
//############################################################################

void  AILCALL AIL_API_set_stream_loop_count(HSTREAM stream, S32 count)
{

  if (stream)
  {
    if (stream->preload)
      AIL_set_sample_loop_count(stream->samp,count);
    else
      stream->loopsleft=count;
  }

}


//############################################################################
//##                                                                        ##
//## Changes the loop sub block of a streaming file                         ##
//##                                                                        ##
//############################################################################

void  AILCALL AIL_API_set_stream_loop_block(HSTREAM stream,
                                            S32     loop_start_offset,
                                            S32     loop_end_offset)
{

  if (stream)
  {
    if (stream->preload)
      AIL_set_sample_loop_block(stream->samp,loop_start_offset,loop_end_offset);
    else
    {
      if ((loop_start_offset==0) && (loop_end_offset==-1))
      {
        stream->substart=0;
        stream->sublen=0x7fffffff;
      }
      else
      {
        if (loop_start_offset != -2) // -2 means don't set this.
          stream->substart=stream_align(stream,loop_start_offset);
        if (loop_end_offset==-1)
          loop_end_offset=stream->totallen;
        if (loop_end_offset != -2) // -2 means don't set this.
          stream->sublen=stream_align(stream,loop_end_offset);
      }
    }
  }
}


//############################################################################
//##                                                                        ##
//## Gets the remaining loop count of a streaming file                      ##
//##                                                                        ##
//############################################################################

S32  AILCALL AIL_API_stream_loop_count(HSTREAM stream)
{

  if (stream)
  {
    if (stream->preload)
      return( AIL_sample_loop_count(stream->samp) );
    else
      return(stream->loopsleft);
  }
  return(-1);

}


//############################################################################
//##                                                                        ##
//## Gets buffer info for the streaming file                                ##
//##                                                                        ##
//############################################################################

void  AILCALL AIL_API_stream_info(HSTREAM stream, S32* datarate, S32* sndtype, S32* length, S32* memory)
{
  if (stream) 
  {
    if (datarate)
      *datarate=stream->filerate;
    if (sndtype)
      *sndtype=stream->filetype|((stream->using_ASI)?DIG_F_USING_ASI:0); // TODO: Document
    if (length)
      *length=stream->totallen;
    if (memory)
      *memory=stream->bufsize*((stream->preload)?1:MSS_STREAM_CHUNKS);
  }
}


static void AILLIBCALLBACK preload_callback_broker(HSAMPLE samp)
{
  HSTREAM s=(HSTREAM)samp->system_data[STR_HSTREAM];

  if (s)
    MSS_do_cb1( (AILSTREAMCB),s->callback,s->samp->driver->callingDS,
                 s);
}

//############################################################################
//##                                                                        ##
//## Sets the end of stream callback                                        ##
//##                                                                        ##
//############################################################################

AILSTREAMCB  AILCALL AIL_API_register_stream_callback(HSTREAM stream, AILSTREAMCB callback)
{
  AILSTREAMCB cb=(AILSTREAMCB)-1;

  if (stream) 
  {
    cb=stream->callback;

    if (stream->preload)
    {
      if ( callback )
        AIL_register_EOS_callback(stream->samp,preload_callback_broker);
      else
        AIL_register_EOS_callback(stream->samp,0);
    }

    stream->callback=callback;
  }

  return(cb);
}

//############################################################################
//##                                                                        ##
//## Sets the user data values                                              ##
//##                                                                        ##
//############################################################################

void  AILCALL AIL_API_set_stream_user_data(HSTREAM stream, U32 index, SINTa value)
{
  if (stream)
    stream->user_data[index]=value;
}


//############################################################################
//##                                                                        ##
//## Gets the user data values                                              ##
//##                                                                        ##
//############################################################################

SINTa  AILCALL AIL_API_stream_user_data(HSTREAM stream, U32 index)
{
  if (stream)
    return(stream->user_data[index]);

  return(0);
}

//############################################################################
//##                                                                        ##
//## Turns auto serving on off                                              ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_auto_service_stream(HSTREAM stream, S32 onoff)
{
  if (stream)
    stream->autostreaming=(onoff)?2:0;
}

//############################################################################
//##                                                                        ##
//## Get size and current position of stream in milliseconds                ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_stream_ms_position(HSTREAM    S, //)
                                S32   *total_milliseconds,
                                S32   *current_milliseconds)
{
   U32 datarate;

   if (S) 
   {
      if (S->preload )
      {
        AIL_sample_ms_position( S->samp, total_milliseconds, current_milliseconds );
        return;
      }

      if (S->filetype & DIG_F_ADPCM_MASK)
      {
         U32 samples_per_block = 4 << ((S->filetype&DIG_F_STEREO_MASK)?1:0);

         samples_per_block = 1 + (S->blocksize-samples_per_block)*8 / samples_per_block;

         datarate=(S->filerate * S->blocksize)/samples_per_block;
      }
      else
      {
         // If it's an asi, and it has exact seek caps, just do it that way.
         if (S->samp->pipeline[SP_ASI_DECODER].active)
         {
              ASISTAGE *ASI = &S->samp->pipeline[ SP_ASI_DECODER ].TYPE.ASI;
              if (ASI->EXACT_GETPOS)
              {
                  InMilesMutex();
                  if ( current_milliseconds )
                  {
                    U32 pos = AIL_stream_position(S) | 0x80000000;  // 0x8000000 means this is the "current" position
                    *current_milliseconds = -1;
                    if ( ASI->ASI_stream_property(ASI->stream, ASI->EXACT_GETPOS, 0, &pos, current_milliseconds ) == 0 )
                    {
                      OutMilesMutex();
                      goto old_way;
                    }
                  }

                  if ( total_milliseconds )
                  {
                    U32 len = S->totallen;
                    *total_milliseconds = -1;
                    if ( ASI->ASI_stream_property(ASI->stream, ASI->EXACT_GETPOS, 0, &len, total_milliseconds ) == 0 )
                    {
                      OutMilesMutex();
                      goto old_way;
                    }
                  }
                  OutMilesMutex();
                  return;
              }
         }

        old_way:

         if (S->block_oriented)
         {
            datarate = S->datarate;
         }
         else
         {
            datarate = S->filerate * S->samp->n_channels * ((S->filetype&DIG_F_16BITS_MASK)?2:1);
         }
      }

      if (total_milliseconds)
         *total_milliseconds=(datarate==0)?1:((S32)(((float)S->totallen*1000.0F)/(float)datarate));

      if (current_milliseconds)
         *current_milliseconds=(datarate==0)?1:((S32)(((float)AIL_stream_position(S)*1000.0F)/(float)datarate));
   }
}

//############################################################################
//##                                                                        ##
//## Seek to a specified millisecond within a stream                        ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_stream_ms_position  (HSTREAM   S, //)
                                      S32       milliseconds)
{
    U32 datarate;

    if (S) 
    {
        if (S->filetype & DIG_F_ADPCM_MASK) 
        {
            U32 samples_per_block = 4 << ((S->filetype&DIG_F_STEREO_MASK)?1:0);

            samples_per_block = 1 + (S->blocksize-samples_per_block)*8 / samples_per_block;

            datarate=(S->filerate * S->blocksize)/samples_per_block;

        }
        else
        {
            // If it's an asi, and it has exact seek caps, just do it that way.
            if (S->samp->pipeline[SP_ASI_DECODER].active)
            {
                ASISTAGE *ASI = &S->samp->pipeline[ SP_ASI_DECODER ].TYPE.ASI;
                if (ASI->EXACT_SEEK)
                {
                    U32 seek_pos = 0;
                    if (ASI->ASI_stream_property(ASI->stream, ASI->EXACT_SEEK, 0, &milliseconds, &seek_pos) == 1)
                    {
                        // success
                        AIL_set_stream_position( S, seek_pos );
                        return;
                    }
                }
            }

            if (S->block_oriented)
            {
                datarate = S->datarate;
            }
            else
            {
                datarate = S->filerate * S->samp->n_channels * ((S->filetype&DIG_F_16BITS_MASK)?2:1);
            }
        }

        AIL_set_stream_position(S, (S32)(((float)datarate*(float)milliseconds)/1000.0F));
    }
}

void AILSTRM_shutdown(HDIGDRIVER driver)
{
   HSTREAM s;

   s=streams;
   
   while (s)
   {
     HSTREAM cur=s;
     s=(HSTREAM)(s->next);
     if (cur->samp)
     {
       if (cur->samp->driver==driver)
       {
         cur->samp=0;  // the driver shutdown that is calling this function will close the HSAMPLE
         AIL_close_stream(cur);
       }
     }
   }
}

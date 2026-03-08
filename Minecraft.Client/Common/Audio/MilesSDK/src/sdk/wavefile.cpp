//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  WAVEFILE.C: Digital sound API module for digital sound file access    ##
//##                                                                        ##
//##  Author: John Miles and Jeff Roberts                                   ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#include <string.h>

#include "mss.h"
#define MSS_H
#include "mss.h"
#include "imssapi.h"

#include "rrtime.h"

#ifndef IS_WIN32
#include <stdarg.h>
#endif

#include <math.h>

//
// Recognized file types
//

#define FTYP_VOC  0
#define FTYP_WAV  1
#define FTYP_ASI  2

#define MY_PI        3.14159265F
#define MY_TWO_PI   (3.14159265F*2.0F)
#define MSS_EPSILON 0.0001F
#define SQRTOF2 1.414213562f
#define LP_FP 8192.0f
#define LP_SHIFT 13
#define LP_RAMP_BITS 4

#define SPEED_OF_SOUND    0.355F   // Speed of sound in meters per millisecond
#define MAX_DOPPLER_SHIFT 4.00F
#define MIN_DOPPLER_SHIFT 0.25F

#define DEGS_TO_DIAMS(degs) ((degs) / 114.591559F)
#define DIAMS_TO_DEGS(degs) ((degs) * 114.591559F)

#define SERVICE_SINGLE_BUFFER 1
#define SERVICE_STREAMED 2

//
// .WAV FACT chunk
//

typedef struct
{
  S8  FACT_string[ 4 ];
  U32 chunk_size;
  U32 samples;
}
FACT;

//
// .VOC voice block
//

typedef MSS_STRUCT
{
  U8 block_ID;
  U8 block_len[ 3 ];
  U8 time_constant;
  U8 pack_method;
}
BLK_1;

//
// .VOC marker block
//

typedef MSS_STRUCT
{
  U8  block_ID;
  U8  block_len[ 3 ];
  S16 marker;
}
BLK_4;

//
// .VOC repeat loop block
//

typedef MSS_STRUCT
{
  U8  block_ID;
  U8  block_len[ 3 ];
  U16 repeat_count;
}
BLK_6;

//
// .VOC extended attribute block
//
// (always followed by block 1)
//

typedef MSS_STRUCT
{
  U8  block_ID;
  U8  block_len[ 3 ];
  U16 time_constant;
  U8  pack_method;
  U8  voice_mode;
}
BLK_8;

//
// .VOC extended voice block
//
// (replaces blocks 1 and 8)
//

typedef MSS_STRUCT
{
  U8  block_ID;
  U8  block_len[ 3 ];
  U32 sample_rate;
  U8  bits_per_sample;
  U8  channels;
  U16 format;
  U8  reserved[ 4 ];
}
BLK_9;

//
// .WAV file headers
//

typedef struct
{
  S8  RIFF_string[ 4 ];
  U32 chunk_size;
  S8  ID_string[ 4 ];
  U8  data[ 4 ];
}
RIFF;

//
// .WAV PCM file format chunk
//

typedef MSS_STRUCT
{
  S8   FMT_string[ 4 ];
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

//
// .WAV file data chunk
//

typedef struct
{
  S8  DATA_string[ 4 ];
  U32 chunk_size;
  U8  data[ 4 ];
}
DATA;

//#define CHECKDENORM
#ifdef CHECKDENORM

#ifdef __RADFINAL__
#error Oops, you have CHECKDENORM on!
#endif

static void check_denorm( float * f )
{
  union
  {
    float *f;
    unsigned int *i;
  } both;

  both.f = f;

  if ( *f !=0.0f )
  {
    if ((( both.i[0] >>23 )&255) == 0 )
      MSSBreakPoint();
    if ((( both.i[0] >>23 )&255) == 255 )
      MSSBreakPoint();
  }
}

#else

#define check_denorm( ptr )

#endif

static S32 AILCALLBACK SP_ASI_DECODER_callback(UINTa     user, //)
                                               void *dest,
                                               S32       bytes_requested,
                                               S32       offset);

MSS_CB_STACK_ALIGN( SP_ASI_DECODER_callback )

static RADINLINE void RAD_vector_cross_product(MSSVECTOR3D       *c,
                                              const MSSVECTOR3D *v1,
                                              const MSSVECTOR3D *v2)
{
  c->x = v1->z * v2->y - v1->y * v2->z;
  c->y = v1->x * v2->z - v1->z * v2->x;
  c->z = v1->y * v2->x - v1->x * v2->y;
}


static RADINLINE void RAD_vector_subtract(MSSVECTOR3D       *d,
                                         const MSSVECTOR3D *v1,
                                         const MSSVECTOR3D *v2)
{
  d->x = v1->x - v2->x;
  d->y = v1->y - v2->y;
  d->z = v1->z - v2->z;
}


static RADINLINE void RAD_vector_normalize(MSSVECTOR3D *v, //)
                                          F32         *mag = NULL)
{
  F32 len = AIL_fsqrt((v->x * v->x) +
                      (v->y * v->y) +
                      (v->z * v->z));

  if (mag != NULL)
   {
   *mag = len;
   }

  if (len < 0.0001F)
  {
    v->x=1.0F;
    v->y=0.0F;
    v->z=0.0F;
  }
  else
  {
    v->x /= len;
    v->y /= len;
    v->z /= len;
  }
}


static RADINLINE F32 RAD_vector_dot_product(const MSSVECTOR3D *v1,
                                    const MSSVECTOR3D *v2)
{
  F32 dot;

  dot  = v1->x * v2->x;
  dot += v1->y * v2->y;
  dot += v1->z * v2->z;

  return( dot );
}

static RADINLINE F32 RAD_vector_length(const MSSVECTOR3D *v)
{
  return AIL_fsqrt((v->x * v->x) +
                   (v->y * v->y) +
                   (v->z * v->z));
}



#ifdef MSS_SPU_PROCESS

#include "radspu.h" // @cdep ignore
#include "spumemstream.h" // @cdep ignore

extern "C"
{

  void AILCALL MSS_mixer_adpcm_decode( void * dest,
                                       void const * in,
                                       S32 out_len,
                                       S32 in_len,
                                       S32 input_format,
                                       ADPCMDATA *adpcm_data );
}

#else

extern C8 AIL_error[ 256 ];

//
// Map MSS_SPEAKER selection to target output channel index, in each supported
// output format
//

#ifdef __
#undef __
#endif
#define __ -1

const S8 output_speaker_index[ 10 ][ MSS_SPEAKER_MAX_INDEX+1 ] =
//  FL  FR  FC  LFE BL  BR  FLC FRC BC  SL  SR  TC  TFL TFC TFR TBL TBC TBR
{ { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },     // 0: Invalid
  {  0, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },     // 1: Mono
  {  0,  1, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },     // 2: Stereo
  {  0,  1, __, __, __, __, __, __,  2, __, __, __, __, __, __, __, __, __ },     // 3: Dolby Prologic
  {  0,  1, __, __,  2,  3, __, __, __, __, __, __, __, __, __, __, __, __ },     // 4: Quad
  {  0,  1,  2, __,  3,  4, __, __, __, __, __, __, __, __, __, __, __, __ },     // 5: 5.0
  {  0,  1,  2,  3,  4,  5, __, __, __, __, __, __, __, __, __, __, __, __ },     // 6: 5.1
  {  0,  1,  2,  3,  4,  5, __, __,  6, __, __, __, __, __, __, __, __, __ },     // 7: 6.1
  {  0,  1,  2,  3,  4,  5, __, __, __,  6,  7, __, __, __, __, __, __, __ },     // 8: 7.1
  {  0,  1,  2,  3,  4,  5, __, __,  6,  7,  8, __, __, __, __, __, __, __ },     // 9: 8.1
};

//
// Map output channel index to MSS_SPEAKER number, in each supported
// output format
//

#undef __
#define __ ((MSS_SPEAKER) -1)

const MSS_SPEAKER output_speaker_order[ 10 ][ MSS_SPEAKER_MAX_INDEX+1 ] =
{ { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
  {  MSS_SPEAKER_FRONT_LEFT, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
  {  MSS_SPEAKER_FRONT_LEFT,  MSS_SPEAKER_FRONT_RIGHT, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
  {  MSS_SPEAKER_FRONT_LEFT,  MSS_SPEAKER_FRONT_RIGHT,  MSS_SPEAKER_BACK_CENTER, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
  {  MSS_SPEAKER_FRONT_LEFT,  MSS_SPEAKER_FRONT_RIGHT,  MSS_SPEAKER_BACK_LEFT,  MSS_SPEAKER_BACK_RIGHT, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
  {  MSS_SPEAKER_FRONT_LEFT,  MSS_SPEAKER_FRONT_RIGHT,  MSS_SPEAKER_FRONT_CENTER,  MSS_SPEAKER_BACK_LEFT,  MSS_SPEAKER_BACK_RIGHT, __, __, __, __, __, __, __, __, __, __, __, __, __ },
  {  MSS_SPEAKER_FRONT_LEFT,  MSS_SPEAKER_FRONT_RIGHT,  MSS_SPEAKER_FRONT_CENTER,  MSS_SPEAKER_LOW_FREQUENCY,  MSS_SPEAKER_BACK_LEFT,  MSS_SPEAKER_BACK_RIGHT, __, __, __, __, __, __, __, __, __, __, __, __ },
  {  MSS_SPEAKER_FRONT_LEFT,  MSS_SPEAKER_FRONT_RIGHT,  MSS_SPEAKER_FRONT_CENTER,  MSS_SPEAKER_LOW_FREQUENCY,  MSS_SPEAKER_BACK_LEFT,  MSS_SPEAKER_BACK_RIGHT,  MSS_SPEAKER_BACK_CENTER, __, __, __, __, __, __, __, __, __, __, __ },
  {  MSS_SPEAKER_FRONT_LEFT,  MSS_SPEAKER_FRONT_RIGHT,  MSS_SPEAKER_FRONT_CENTER,  MSS_SPEAKER_LOW_FREQUENCY,  MSS_SPEAKER_BACK_LEFT,  MSS_SPEAKER_BACK_RIGHT,  MSS_SPEAKER_SIDE_LEFT, MSS_SPEAKER_SIDE_RIGHT, __, __, __, __, __, __, __, __, __, __ },
  {  MSS_SPEAKER_FRONT_LEFT,  MSS_SPEAKER_FRONT_RIGHT,  MSS_SPEAKER_FRONT_CENTER,  MSS_SPEAKER_LOW_FREQUENCY,  MSS_SPEAKER_BACK_LEFT,  MSS_SPEAKER_BACK_RIGHT,  MSS_SPEAKER_BACK_CENTER,  MSS_SPEAKER_SIDE_LEFT, MSS_SPEAKER_SIDE_RIGHT, __, __, __, __, __, __, __, __, __ },
};

#define SP_L   0x0001   // Left
#define SP_R   0x0002   // Right
#define SP_F   0x0004   // Front
#define SP_B   0x0008   // Back
#define SP_S   0x0010   // Side
#define SP_C   0x0020   // Center
#define SP_T   0x0040   // Top
#define SP_LFE 0x0080   // LFE

#define SP_FB_AXIS ((U8) (SP_F + SP_S + SP_B))
#define SP_LR_AXIS ((U8) (SP_L + SP_R))

const U8 speaker_masks[MSS_SPEAKER_MAX_INDEX+1] =
{
   SP_F + SP_L,         // FL
   SP_F + SP_R,         // FR
   SP_F + SP_C,         // FC
   SP_LFE,              // LFE
   SP_B + SP_L,         // BL
   SP_B + SP_R,         // BR
   SP_F + SP_L + SP_C,  // FLC
   SP_F + SP_R + SP_C,  // FRC
   SP_B + SP_C,         // BC
   SP_S + SP_L,         // SL
   SP_S + SP_R,         // SR
   SP_T + SP_C,         // TC
   SP_T + SP_F + SP_L,  // TFL
   SP_T + SP_F + SP_C,  // TFC
   SP_T + SP_F + SP_R,  // TFR
   SP_T + SP_B + SP_L,  // TBL
   SP_T + SP_B + SP_C,  // TBC
   SP_T + SP_B + SP_R   // TBR
};


static void clear_last_sample(HSAMPLE S)
{
   for (S32 i=0; i < S->n_channels; i++)
   {
      S->left_val[i]  = 0;
      S->right_val[i] = 0;
   }

   S->last_decomp[0] = 65536;
   S->src_fract      = 0;
}

static void init_sample_3D_state(HSAMPLE S)
{
   //
   // Ensure that S3D data is valid even for samples that aren't (yet) under 3D
   // control
   //
   // If we didn't do this, voice filters would need to do a lot more work when swapping
   // voices between samples...
   //

   S->S3D.doppler_valid    = 0;
   S->S3D.doppler_shift    = 1.0F;
   S->S3D.inner_angle      = DEGS_TO_DIAMS(360.0F);
   S->S3D.outer_angle      = DEGS_TO_DIAMS(360.0F);
   S->S3D.outer_volume     = 1.0F;
   S->S3D.cone_enabled     = 0;

   S->S3D.max_dist     = 200.0F;
   S->S3D.min_dist     = 1.0F;
   S->S3D.dist_changed = TRUE;

   S->S3D.position.x = 0.0F;
   S->S3D.position.y = 0.0F;
   S->S3D.position.z = 0.0F;

   S->S3D.face.x = 1.0F;
   S->S3D.face.y = 0.0F;
   S->S3D.face.z = 0.0F;

   S->S3D.up.x = 0.0F;
   S->S3D.up.y = 1.0F;
   S->S3D.up.z = 0.0F;

   S->S3D.velocity.x = 0.0F;
   S->S3D.velocity.y = 0.0F;
   S->S3D.velocity.z = 0.0F;

   S->S3D.auto_3D_atten = 1;
   S->S3D.atten_3D      = 1.0F;
   S->S3D.exclusion_3D = -1.0f;
   S->S3D.lowpass_3D = -1.0f;

   S->S3D.owner = S;
   S->S3D.falloff_function = 0;
   S->S3D.spread = 0;

   AIL_memset(&S->S3D.position_graph, 0, sizeof(S->S3D.position_graph));
   S->S3D.position_graph_count = 0;

   AIL_memset(&S->S3D.volgraph, 0, sizeof(S->S3D.volgraph));
   AIL_memset(&S->S3D.excgraph, 0, sizeof(S->S3D.excgraph));
   AIL_memset(&S->S3D.lpgraph, 0, sizeof(S->S3D.lpgraph));
   S->S3D.volgraphcnt = 0;
   S->S3D.excgraphcnt = 0;
   S->S3D.lpgraphcnt = 0;
}

//############################################################################
//#                                                                          #
//# Set default user channel-steering matrix based on the sample's source    #
//# and dest channel counts                                                  #
//#                                                                          #
//############################################################################

static void set_user_channel_defaults(HSAMPLE S)
{
   S32 m = S->driver->logical_channels_per_sample;

   if (m == 0)
      {
      return; // DS3D/EAX
      }

   S8 const *speaker_enum_to_driver_chan = output_speaker_index[m];

   //
   // Initialize the matrix to zero
   //

   for (S32 i=0; i < S->driver->logical_channels_per_sample; i++)
      {
      for (S32 j=0; j < S->n_channels; j++)
         {
         S->user_channel_levels[i][j] = 0.0F;
         }
      }

   //
   // Mono source channels are sent to all speakers by default
   // (so 3D and 5.1 panning will work without any further calls)
   //
   // Stereo files (as opposed to 2.0 multichannel files) are sent
   // to both halves of the first build buffer
   //

   if (S->format & DIG_F_STEREO_MASK)
      {
      S->user_channel_levels[0][0] = 1.0F;
      S->user_channel_levels[1][1] = 1.0F;
      }
   else if (!(S->format & DIG_F_MULTICHANNEL_MASK))
      {
      S->user_channel_levels[0][0] = 1.0F;
      S->user_channel_levels[1][0] = 1.0F;
      }
   else
      {
      if (S->driver->logical_channels_per_sample == 1)
         {
         //
         // Mono output: all non-LFE source channels are sent to FL
         //

         for (S32 ch=0; ch <= MSS_SPEAKER_MAX_INDEX; ch++)
            {
            if (speaker_masks[ch] & SP_LFE)
               {
               continue;
               }

            S32 src_chan = S->speaker_enum_to_source_chan[ch];

            if (src_chan == -1)
               {
               continue;
               }

            S->user_channel_levels[0][src_chan] = 1.0F;
            }
         }
      else
         {
         //
         // Otherwise, assign multiple source channels to multiple output speakers
         //

         for (S32 ch=0; ch <= MSS_SPEAKER_MAX_INDEX; ch++)
            {
            S32 src_chan = S->speaker_enum_to_source_chan[ch];

            if (src_chan == -1)
               {
               continue;
               }

            //
            // Try to send this source channel to its corresponding speaker
            //

            S32 dest_chan = speaker_enum_to_driver_chan[ch];

            if (dest_chan != -1)
               {
               S->user_channel_levels[dest_chan][src_chan] = 1.0F;
               continue;
               }

            //
            // Failing that, try to send the source channel to other dest channels
            // on the same (L/R) side
            //

            S32 mapped = 0;

            U8 src_X_mask = speaker_masks[ch] & (U8) ~SP_FB_AXIS;   // (left/right/center=valid, front/side/back=clear)

            S32 dch;
            for (dch=0; dch <= MSS_SPEAKER_MAX_INDEX; dch++)
               {
               if (speaker_masks[dch] & src_X_mask)
                  {
                  dest_chan = speaker_enum_to_driver_chan[dch];

                  if (dest_chan != -1)
                     {
                     S->user_channel_levels[dest_chan][src_chan] = 1.0F;
                     mapped++;
                     }
                  }
               }

            if (mapped)
               {
               continue;
               }

            //
            // Any remaining unmapped non-LFE channels (e.g., side channels) go to both
            // left and right destinations
            //

            for (dch=0; dch <= MSS_SPEAKER_MAX_INDEX; dch++)
               {
               if ((speaker_masks[dch] & SP_LR_AXIS) && (!(speaker_masks[ch] & SP_LFE)))
                  {
                  dest_chan = speaker_enum_to_driver_chan[dch];

                  if (dest_chan != -1)
                     {
                     S->user_channel_levels[dest_chan][src_chan] = 1.0F;
                     }
                  }
               }
            }
         }

      //
      // Perform simple normalization of source channel levels at each dest speaker
      // to discourage clipping
      //
      // Default channel level of 1 / n_speakers chosen empirically to keep the
      // perceived volume level constant when switching the Miles Player into 'Mono'
      // mode
      //
      // This has no effect except in the downmix cases, where multiple source channels
      // contribute to individual speakers
      //

      for (S32 dch=0; dch <= MSS_SPEAKER_MAX_INDEX; dch++)
         {
         S32 dest_chan = speaker_enum_to_driver_chan[dch];

         if (dest_chan == -1)
            {
            continue;
            }

         S32 n=0;

         for (S32 src_chan=0; src_chan < S->n_channels; src_chan++)
            {
            if (S->user_channel_levels[dest_chan][src_chan] > 0.0F)
               {
               n++;
               }
            }

         if (n > 1)
            {
            F32 norm = 1.0F / n;

            S->speaker_levels[dest_chan] = norm;
            }
         }
      }
}

//############################################################################
//#                                                                          #
//# Allocate/release objects needed for multichannel mixing                  #
//#                                                                          #
//############################################################################

static void free_channel_buffers(HSAMPLE S)
{
   if ( S->chan_buf_ptr )
   {
     AIL_mem_free_lock( S->chan_buf_ptr );
     S->chan_buf_ptr = 0;
   }
   S->chan_buf_alloced = 0;
   S->chan_buf_used = 0;
}

#endif

static void * assign_ptr( U8 ** ptr, U32 size )
{
  void * ret;

  ret = *ptr;
  size = ( size + 3 ) & ~3;

  *ptr += size;

  return ret;
}

void * assign_chan_ptrs( HSAMPLE S, U8 * start_ptr, S32 sub_ptrs_first );
void * assign_chan_ptrs( HSAMPLE S, U8 * start_ptr, S32 sub_ptrs_first )
{
   void* bogus_buf[ MAX_SPEAKERS + 1];
   F32 ** ucl;
   S32 ** cs;
   S32 ** ps;
   S32 ** rl;
   S32 i;
   S32 n = S->n_channels;
   S32 m = ( S->driver->logical_channels_per_sample + 1 ) & ~1;

   U8 * cur = (U8*) ( ( ( (UINTa) start_ptr ) + 15 ) & ~15 );

   S->lp = (LOWPASS_INFO*) assign_ptr( &cur, n * sizeof( S->lp[ 0 ] ) );

   //
   // Allocate buffers for volume scaling and user channel-level factors
   // (AIL_set_sample_channel_levels())
   //
   // These are [dest][src] arrays, where dest=the logical driver output channel number
   // and src=the source channel number as it appears in the file
   //
   // Initially, all entries are zero except for those that match each
   // source channel with its corresponding output channel.  If there
   // are source channels that don't have corresponding speakers, we
   // try to map the source channels to the speaker(s) that do exist
   // in a reasonably-logical fashion
   //

   ucl = S->user_channel_levels;
   cs  = S->cur_scale;
   ps  = S->prev_scale;
   rl  = S->ramps_left;

   S->user_channel_levels = (F32**) assign_ptr( &cur, ( m + 1 ) * sizeof( F32* ) );
   S->cur_scale           = (S32**) assign_ptr( &cur, ( m + 1 ) * sizeof( S32* ) );
   S->prev_scale          = (S32**) assign_ptr( &cur, ( m + 1 ) * sizeof( S32* ) );
   S->ramps_left          = (S32**) assign_ptr( &cur, ( m + 1 ) * sizeof( S32* ) );

   if ( !sub_ptrs_first )
   {
     ucl = S->user_channel_levels;
     cs  = S->cur_scale;
     ps  = S->prev_scale;
     rl  = S->ramps_left;
   }

   // when we are calling this routine just to calculate the length, use a temp buffer to receive the pointers
   if ( start_ptr == 0 )
   {
     ucl = (F32**) (void*) bogus_buf;
     cs = ps = rl = (S32**) (void*) bogus_buf;
   }


   for (i=0; i < m; i++)
   {
     ucl[i] = (F32*) assign_ptr( &cur, n * sizeof(F32));
     cs[i]  = (S32*) assign_ptr( &cur, n * sizeof(S32));
     ps[i]  = (S32*) assign_ptr( &cur, n * sizeof(S32));
     rl[i]  = (S32*) assign_ptr( &cur, n * sizeof(S32));
   }

   S->left_val            = (S32*) assign_ptr( &cur, n * sizeof( S->left_val[ 0 ] ) );
   S->right_val           = (S32*) assign_ptr( &cur, n * sizeof( S->right_val[ 0 ] ) );
   S->last_decomp         = (S32*) assign_ptr( &cur, n * sizeof( S->last_decomp[ 0 ] ) );

   S->auto_3D_channel_levels      = (F32 *) assign_ptr( &cur, m * sizeof( S->auto_3D_channel_levels[ 0 ] ) );
   S->speaker_levels              = (F32 *) assign_ptr( &cur, m * sizeof( S->speaker_levels[ 0 ] ) );

   return cur;
}

#ifndef MSS_SPU_PROCESS

static void reset_asi( HSAMPLE S, S32 type, S32 pos )
{
  // if requested, do a reset now that the ASI decoder has the data
  if ( S->pipeline[SP_ASI_DECODER].active )
  {
    ASISTAGE *ASI = &S->pipeline[SP_ASI_DECODER].TYPE.ASI;
    ASI->ASI_stream_seek(ASI->stream, type);

    if (ASI->STREAM_SEEK_POS != (UINTa) -1)
      {
      ASI->ASI_stream_property(ASI->stream,
                               ASI->STREAM_SEEK_POS,
                               0,&pos, &pos);
      }

  }
}

static S32 alloc_channel_buffers(HSAMPLE S)
{
   S32 i;
   S8  src_chan;
   S32 speaker_num;
   U32 bit_mask;
   U32 total;

   S32 n = S->n_channels;
   S32 m = ( S->driver->logical_channels_per_sample + 1 ) & ~1;

   //
   // Note that only multichannel formats use channel arrays -- normal mono and
   // stereo source samples use a single build buffer.  For sanity's sake, we'll go
   // ahead and allocate 2-channel arrays for stereo sources
   //

   total = (U32) ( ( (UINTa) assign_chan_ptrs( S, 0, 0 ) ) + 15 + 15 ) & ~15;

   // if the buffer is too small, or if it's less than half the size of the existing one, realloc it
   if ( ( total > S->chan_buf_alloced ) || ( total < ( S->chan_buf_alloced / 2 ) ) )
   {
     free_channel_buffers( S );
   }

   if ( S->chan_buf_ptr == 0 )
   {
     S->chan_buf_ptr = (U8*) AIL_mem_alloc_lock( total );
     if ( S->chan_buf_ptr == 0 )
     {
       AIL_set_error("Channel buffer allocation failed.");
       return 0;
     }
     S->chan_buf_alloced = total;
   }

   S->chan_buf_used = total;

   AIL_memset( S->chan_buf_ptr, 0, total );

   assign_chan_ptrs( S, S->chan_buf_ptr, 0 );

   for (i=0; i < S->n_channels; i++)
   {
     S->lp[i].cutoff = 1.0F;
     S->lp[i].calculated_cut = -1.0F;
     S->lp[i].queuedA = 0x7fff; // means unused
     S->lp[i].A = 0x7fff;       // means unused
   }

   //
   // speaker and 3D channel levels = full scale
   //

   for (i=0; i < S->driver->logical_channels_per_sample; i++)
   {
     S->auto_3D_channel_levels[i] = 1.0F;
     S->speaker_levels[i] = 1.0f;
   }

   clear_last_sample( S );

   if (m == 0)
   {
     return 1; // DS3D/EAX
   }

   AIL_memset( S->speaker_enum_to_source_chan, 0xff, sizeof( S->speaker_enum_to_source_chan ) );
   
   //
   // Create two-way source channel <-> speaker assignment maps
   //
   // These maps are needed to support gaps in the source data's channel mask
   // (i.e., dwChannelMask in the WAVEFORMATEXTENSIBLE structure)
   //

   src_chan    = 0;
   speaker_num = 0;
   bit_mask    = 1;

   for (i=0; (i < 32) && (src_chan < n) && (speaker_num < MAX_SPEAKERS); i++)
   {
     if (S->channel_mask & bit_mask)
     {
       S->speaker_enum_to_source_chan[speaker_num] = src_chan;
       src_chan++;
     }

     bit_mask <<= 1;
     speaker_num++;
   }


   set_user_channel_defaults(S);

   return 1;
}

//############################################################################
//##                                                                        ##
//## Set sample data format and flags                                       ##
//##                                                                        ##
//## Available formats:                                                     ##
//##                                                                        ##
//##   DIG_F_MONO_8                                                         ##
//##   DIG_F_MONO_16                                                        ##
//##   DIG_F_STEREO_8                                                       ##
//##   DIG_F_STEREO_16                                                      ##
//##   DIG_F_ADPCM_MONO_16                                                  ##
//##   DIG_F_ADPCM_STEREO_16                                                ##
//##   DIG_F_MULTICHANNEL_16 (pass channel count in upper word of format)   ##
//##                                                                        ##
//## Note: 16-bit sample data must be in Intel byte order                   ##
//##                                                                        ##
//############################################################################

S32 AILCALL SS_set_sample_type( HSAMPLE S, S32 format, U32 channel_mask )
 {
  if ( S == NULL )
  {
    return 0;
  }

#ifdef HOST_SPU_PROCESS
  if ( ( format & ( DIG_F_ADPCM_MASK | DIG_F_16BITS_MASK ) ) == 0 )
  {
    S->spu_on = 0;
  }
#endif

  S32 n_channels = format >> 16;
  format = format & 0xFFFF;

  if (format & DIG_F_STEREO_MASK)
    n_channels = 2;
  else if (!(format & DIG_F_MULTICHANNEL_MASK))
    n_channels = 1;

  if ( ( format       != S->format )      ||
       ( channel_mask != S->channel_mask) ||
       ( n_channels   != S->n_channels ))
  {
    S->format       = format;
    S->n_channels   = n_channels;
    S->channel_mask = channel_mask;

    if (!alloc_channel_buffers(S))
      {
      return 0;
      }

    // Reset ADPCM offset to the end of decode buffer
    // to force a decode buffer refill
    if ( format & DIG_F_ADPCM_MASK )
    {
      S->adpcm.blockleft = 0;
      S->adpcm.extrasamples = 0;
    }
  }

  return 1;
}

void AILCALL SS_update_sample_reverb_state(HSAMPLE S)
{
#ifdef MSS_VFLT_SUPPORTED
  if (S->driver->voice_filter != NULL)
    {
    return;
    }
#endif

  if (S->bus == MAX_BUSSES)
      return;

  if ( ( S->wet_level > 0.0001f ) && ( S->driver->reverb[S->bus].master_wet > 0.0001f ) && ( S->driver->reverb[S->bus].reverb_decay_time_s > 0.0001f ) )
  {
    S->driver->reverb[S->bus].reverb_off_time_ms = S->driver->num_mixes;
    S->driver->reverb[S->bus].reverb_on = 1;
  }
}

void AILCALL SS_update_driver_reverb_state(HDIGDRIVER dig)
{
#ifdef MSS_VFLT_SUPPORTED
  if (dig->voice_filter != NULL)
    {
    return;
    }
#endif

  for (S32 i = 0; i < MAX_BUSSES; i++)
  {
      if ( ( dig->reverb[i].master_wet > 0.0001f ) && ( dig->reverb[i].reverb_decay_time_s > 0.0001f ) )
      {
        dig->reverb[i].reverb_off_time_ms = dig->num_mixes;
        dig->reverb[i].reverb_on = 1;
      }
  }
}

//############################################################################
//##                                                                        ##
//## Get length of .VOC block                                               ##
//##                                                                        ##
//############################################################################

static U32 AIL_VOC_block_len( void *block )
{
  return( ( * (U32 *) block ) >> 8 );
}

//############################################################################
//##                                                                        ##
//## Terminate playback of .VOC file                                        ##
//##                                                                        ##
//## Invoke application callback function, if any, and release the sample   ##
//## allocated to play this file                                            ##
//##                                                                        ##
//############################################################################

static void AIL_VOC_terminate(HSAMPLE sample)
{
}

//############################################################################
//##                                                                        ##
//## Process .VOC file block                                                ##
//##                                                                        ##
//## Called by .VOC initialization code and as end-of-sample callback       ##
//## function (interrupt-based)                                             ##
//##                                                                        ##
//## If play_flag clear, search for first block after desired marker (if    ##
//## any) and return without playing it                                     ##
//##                                                                        ##
//############################################################################

static S32 AIL_process_VOC_block(HSAMPLE sample, S32 play_flag)
{
   S32  voice_block;
   void *b;

   voice_block = 0;

   //
   // Loop until voice block is found
   //

   while (!voice_block)
      {
      b = (void *) sample->system_data[VOC_BLK_PTR];

      switch (*(U8 *) b)
         {
         //
         // Terminator block
         //

         case 0:

            //
            // Terminate playback, then return without trying to advance
            // to next block
            //

            AIL_VOC_terminate(sample);

            return 1;

         //
         // Voice block
         //

         case 1:

            //
            // Skip block if desired marker has not been found
            //

            if (!sample->system_data[VOC_MARKER_FOUND])
               {
               break;
               }

            //
            // Set up sample data and start playback
            //

            AIL_set_sample_address(sample,
                                   AIL_ptr_add(b, sizeof(BLK_1)),
                                   AIL_VOC_block_len(b) - 2);

            AIL_set_sample_playback_rate(sample,(U32)
                             ( (S32)1000000L / (S32)(256 - ((BLK_1 *) b)->time_constant) ) );

            if (!SS_set_sample_type(sample,DIG_F_MONO_8,~0U))
               {
               return 0;
               }

            if (play_flag)
               AIL_start_sample(sample);

            voice_block = 1;
            break;

         //
         // Marker block
         //

         case 4:

            //
            // Ignore if entire file to be played
            //

            if (sample->system_data[VOC_MARKER] == -1)
               {
               break;
               }

            //
            // If this is the desired marker, set MARKER_FOUND flag --
            // otherwise, clear MARKER_FOUND flag to prevent playback
            // of future voice blocks
            //

            if (sample->system_data[VOC_MARKER] == (S32)
                                                   ((BLK_4 *) b)->marker)
               {
               sample->system_data[VOC_MARKER_FOUND] = 1;
               }
            else
               {
               sample->system_data[VOC_MARKER_FOUND] = 0;
               }

            break;

         //
         // Repeat block
         //

         case 6:

            //
            // Log repeat count and starting address of repeat block
            //

            sample->system_data[VOC_REP_BLK] = (UINTa) b;

            sample->system_data[VOC_N_REPS]  = (UINTa)
                                             ((BLK_6 *) b)->repeat_count;
            break;

         //
         // End repeat block
         //

         case 7:

            //
            // If finite repeat block active, check and decrement repeat
            // count
            //

            if (sample->system_data[VOC_N_REPS] != 0xffff)
               {
               if (sample->system_data[VOC_N_REPS]-- == 0)
                  {
                  break;
                  }
               }

            b = (void *) sample->system_data[VOC_REP_BLK];
            break;

         //
         // Extended attribute block
         // (followed by block 1)
         //

         case 8:

            //
            // Skip block if desired marker has not been found
            //

            if (!sample->system_data[VOC_MARKER_FOUND])
               {
               break;
               }

            //
            // Set up sample data and start playback
            //

            if (((BLK_8 *) b)->voice_mode)
               {
               if (!SS_set_sample_type(sample,DIG_F_STEREO_8,~0U)) return 0;

               AIL_set_sample_playback_rate(sample,(U32)
                  ( 128000000 / (((U32)(65536)) - ((BLK_8 *) b)->time_constant)));
               }
            else
               {
               if (!SS_set_sample_type(sample,DIG_F_MONO_8,~0U)) return 0;

               AIL_set_sample_playback_rate(sample,(U32)
                  ( 256000000 / (((U32)(65536)) - ((BLK_8 *) b)->time_constant)));
               }

            //
            // Advance to paired voice block (type 1) in .VOC image
            //

            b = (C8 *)AIL_ptr_add(b, AIL_VOC_block_len(b) + 4);

            //
            // Set sample address and size, and start playback
            //

            AIL_set_sample_address(sample,
                                   AIL_ptr_add(b, sizeof(BLK_1)),
                                   AIL_VOC_block_len(b) - 2);

            if (play_flag)
               AIL_start_sample(sample);

            voice_block = 1;
            break;

         //
         // Extended voice block
         //

         case 9:

            //
            // Skip block if desired marker has not been found
            //

            if (!sample->system_data[VOC_MARKER_FOUND])
               {
               break;
               }

            //
            // Set up sample data and start playback
            //

            AIL_set_sample_address(sample,
                                   AIL_ptr_add(b, sizeof(BLK_9)),
                                   AIL_VOC_block_len(b) - 12);

            AIL_set_sample_playback_rate(sample, ((BLK_9 *) b)->sample_rate);

            if ((((BLK_9 *) b)->channels == 1) &&
                (((BLK_9 *) b)->format   == 0))
               {
               if (!SS_set_sample_type(sample,DIG_F_MONO_8,~0U)) return 0;
               }
            else if ((((BLK_9 *) b)->channels == 2) &&
                     (((BLK_9 *) b)->format   == 0))
               {
               if (!SS_set_sample_type(sample,DIG_F_STEREO_8,~0U)) return 0;
               }
            else if ((((BLK_9 *) b)->channels == 1) &&
                     (((BLK_9 *) b)->format   == 4))
               {
               if (!SS_set_sample_type(sample,DIG_F_MONO_16,~0U)) return 0;
               }
            else if ((((BLK_9 *) b)->channels == 2) &&
                     (((BLK_9 *) b)->format   == 4))
               {
               if (!SS_set_sample_type(sample,DIG_F_STEREO_16,~0U)) return 0;
               }

            if (play_flag)
               AIL_start_sample(sample);

            voice_block = 1;
            break;
         }

      //
      // Advance pointer to next block in .VOC image
      //

      sample->system_data[VOC_BLK_PTR] = (SINTa)
                                         AIL_ptr_add(b, AIL_VOC_block_len(b) + 4);
      }

   return 1;
}

//############################################################################
//##                                                                        ##
//## End-of-sample callback handler for .VOC file playback                  ##
//##                                                                        ##
//############################################################################

//static void AILLIBCALLBACK AIL_VOC_EOS( HSAMPLE sample )
//{
//  AIL_process_VOC_block( sample, 1 );
//}


//############################################################################
//##                                                                        ##
//## Create sample instance by parsing .WAV file                            ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_WAV_info(void const* data, AILSOUNDINFO* info)
{
   void  *end;
   FMT   *f;
   DATA  *d;
   FACT  *fa;

   if (data==0)
     goto error;

   end=AIL_ptr_add(data,LEU_SWAP32(&((RIFF *) data)->chunk_size));

   if (AIL_strnicmp((char*)(((RIFF *) data)->ID_string),"WAVE",4))
      {
     error:
      AIL_strcpy(AIL_error,"Not a WAV file.");
      return(0);
      }

   info->channel_mask = ~0U;

   //
   // Find mandatory <fmt-ck>
   //

   f = (FMT*)(void*) (((RIFF *) data)->data);

   while (AIL_strnicmp((char*)f->FMT_string,"fmt ",4))
      {
      U32 chunk_size = LEU_SWAP32( &f->chunk_size );
      f = (FMT *) AIL_ptr_add(f, chunk_size + 8 + (chunk_size & 1));
      if (AIL_ptr_dif(f,end)>=0)
        goto error;
      }

   info->format= LE_SWAP16( &f->format_tag );

#ifdef IS_XENON
   //
   // Is this an XMA file?  If so, *f marks an XMAWAVEFORMAT structure
   //

   if (info->format == XWAVE_FORMAT_XMA)
      {
      XXMAWAVEFORMAT *xf = (XXMAWAVEFORMAT *) AIL_ptr_add(f, 8);

      info->rate       = LEU_SWAP32(&xf->XmaStreams[0].SampleRate);
      info->bits       = LE_SWAP16(&xf->BitsPerSample);
      info->channels   =            xf->XmaStreams[0].Channels;
      info->block_size = 2048;

      //
      // XMA data pointer is the beginning of the .WAV image itself
      // We have no way of knowing how many samples are in the file...
      //

      info->initial_ptr = data;
      info->data_ptr    = data;
      info->data_len    = AIL_ptr_dif(data, end);
      info->samples     = 0;

      return 1;
      }
#endif

   //
   // Not an XMA file -- must be standard ADPCM or PCM .WAV
   //

   info->rate= LEU_SWAP32( &f->sample_rate );
   info->bits= LE_SWAP16( &f->bits_per_sample );
   info->channels= LE_SWAP16( &f->channels );
   info->block_size= LE_SWAP16( &f->alignment );

   //
   // Find mandatory <data-ck>
   //

   d = (DATA *)(void*) (((RIFF *) data)->data);

   while (AIL_strnicmp((char*)d->DATA_string,"data",4))
      {
      U32 chunk_size = LEU_SWAP32( &d->chunk_size );
      d = (DATA *) AIL_ptr_add( d, chunk_size + 8 + (chunk_size & 1));
      if (AIL_ptr_dif(d,end)>=0)
        goto error;
      }

   info->data_ptr=d->data;
   info->data_len= LEU_SWAP32( &d->chunk_size );

   if (info->format == WAVE_FORMAT_EXTENSIBLE)
      {
      const U8 KSDATAFORMAT_SUBTYPE_PCM[16] =
         {
         0x01,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71,
         };

      MWAVEFORMATEXTENSIBLE const *wfex = (MWAVEFORMATEXTENSIBLE const *)(void*) &f->format_tag;

      if (LEU_SWAP16(&wfex->Format.cbSize) < 22)
         {
         AIL_strcpy(AIL_error,"Not a WAVEFORMATPCMEX file.");
         return(0);
         }

      for (S32 i=0; i < 16; i++)
         {
         if (wfex->SubFormat[i] != KSDATAFORMAT_SUBTYPE_PCM[i])
            {
            AIL_strcpy(AIL_error,"Not KSDATAFORMAT_SUBTYPE_PCM.");
            return(0);
            }
         }

      if (info->block_size != info->channels * sizeof(S16))
         {
         AIL_strcpy(AIL_error,"Unsupported multichannel format.");
         return(0);
         }

      info->channel_mask = LE_SWAP32((void*)&wfex->dwChannelMask);

      info->format = WAVE_FORMAT_PCM;  // treat it just like a normal PCM .WAV, but with extra channels
      info->samples=(info->bits==0)?0:((info->data_len*8)/info->bits);  
      info->initial_ptr = info->data_ptr;

      return 1;
      }
   else if ((info->format==WAVE_FORMAT_IMA_ADPCM) && (info->bits==4))
   {

     fa = (FACT *)(void*) (((RIFF *) data)->data);

     while (AIL_strnicmp((char*)fa->FACT_string,"fact",4))
        {
        U32 chunk_size = LEU_SWAP32( &fa->chunk_size );
        fa = (FACT *) AIL_ptr_add( fa, chunk_size + 8 + (chunk_size & 1));
        if (AIL_ptr_dif(fa,end)>=0)
          {

          U32 samples_per_block = 4 << (info->channels/2);
          samples_per_block = 1 + (info->block_size-samples_per_block)*8 / samples_per_block;
          info->samples=((info->data_len+info->block_size-1)/info->block_size)*samples_per_block;

          goto nofact;
          }
       }
       info->samples= LEU_SWAP32( &fa->samples );
     nofact:;
   } else
     info->samples=(info->bits==0)?0:((info->data_len*8)/info->bits);

   info->initial_ptr=info->data_ptr;

   return(1);
}


//############################################################################
//##                                                                        ##
//## Create sample instance by parsing .WAV file                            ##
//##                                                                        ##
//############################################################################

S32 AIL_process_WAV_image( AILSOUNDINFO const * info, HSAMPLE sample )
{
  S32 format=0;

  if ((info->channels > 2) || (info->channel_mask != ~0U))
  {
    format = DIG_F_MULTICHANNEL_MASK | (info->channels << 16);
  }
  else if ( info->channels == 2 )
  {
    format = DIG_F_STEREO_MASK;
  }

  {
    switch( info->bits )
    {
      case 4 :
        format |= DIG_F_ADPCM_MASK ;
      case 16:
        format |= DIG_F_16BITS_MASK ;
    }
  }

  if (!SS_set_sample_type( sample, format, info->channel_mask )) return 0;

  AIL_set_sample_playback_rate( sample, info->rate );

  //
  // Configure adpcm if required
  //

  if ( format & DIG_F_ADPCM_MASK )
  {
    AIL_set_sample_adpcm_block_size( sample, info->block_size );
  }

  //
  // Configure sample address and length based on DATA chunk
  //

  AIL_set_sample_address( sample, info->data_ptr, info->data_len );

  return 1;
}

//############################################################################
//#                                                                          #
//# Initiate playback of sample, calling driver or voice filter as           #
//# necessary                                                                #
//#                                                                          #
//############################################################################

static void SS_start_playback(HSAMPLE S)
{
#ifdef MSS_VFLT_SUPPORTED
   if (S->voice != NULL)
      {
      HDIGDRIVER dig = S->driver;

      dig->voice_filter->start_sample_voice(dig->voice_filter->driver_state,
                                            S);
      return;
      }
#endif

   SS_start_DIG_driver_playback(S);
}

//############################################################################
//##                                                                        ##
//## Update sample's 3D positional attributes for multichannel output       ##
//##                                                                        ##
//############################################################################

void AILCALL SS_set_speaker_configuration (D3DSTATE        *D3D, //)
                                           MSSVECTOR3D *speaker_positions,
                                           S32              n_channels,
                                           S32              logical_channels_per_sample)
{
   S32 i,spkr;

   if (n_channels > logical_channels_per_sample)
      {
      n_channels = logical_channels_per_sample;
      }

   for (i=0; i < n_channels; i++)
      {
      D3D->speaker_positions[i] = speaker_positions[i];
      }

   //
   // Create a default receiver constellation
   //
   // This is a set of worldspace points on the unit sphere surrounding the listener,
   // coincident with directional speakers by default, possibly including a virtual speaker
   //

   D3D->n_receiver_specs = 0;

   //
   // Assign each speaker to an ambient or directional channel slot
   //
   // Send each directional speaker vector through a 3x3 rotation matrix corresponding
   // to the listener's orientation
   //
   // Also calculate unweighted centroid (average) of directional speaker locations for
   // virtual-speaker placement calculation
   //

   D3D->n_directional_channels = 0;
   D3D->n_ambient_channels     = 0;

   MSSVECTOR3D V;

   V.x = 0.0F;
   V.y = 0.0F;
   V.z = 0.0F;

   F32 len;

   for (spkr=0; spkr < logical_channels_per_sample; spkr++)
      {
      //
      // Get unit vector from listener to speaker
      //

      MSSVECTOR3D P = D3D->speaker_positions[spkr];

      len = RAD_vector_length(&P);

      if (len < MSS_EPSILON)
         {
         //
         // Can't determine direction -- speaker is too close to listener
         //

         D3D->ambient_channels[D3D->n_ambient_channels++] = spkr;
         continue;
         }

      S32 index = D3D->n_directional_channels++;

      D3D->directional_channels[index] = spkr;

      P.x /= len;
      P.y /= len;
      P.z /= len;

      MSSVECTOR3D *C0 = &D3D->listen_cross;    // +X column unit vector
      MSSVECTOR3D *C1 = &D3D->listen_up;       // +Y column unit vector
      MSSVECTOR3D *C2 = &D3D->listen_face;     // +Z column unit vector

      D3D->listener_to_speaker[index].x = (P.x * C0->x) + (P.y * C1->x) + (P.z * C2->x);
      D3D->listener_to_speaker[index].y = (P.x * C0->y) + (P.y * C1->y) + (P.z * C2->y);
      D3D->listener_to_speaker[index].z = (P.x * C0->z) + (P.y * C1->z) + (P.z * C2->z);

      V.x += D3D->listener_to_speaker[index].x;
      V.y += D3D->listener_to_speaker[index].y;
      V.z += D3D->listener_to_speaker[index].z;

      D3D->receiver_specifications[D3D->n_receiver_specs  ].direction           = D3D->listener_to_speaker[index];
      D3D->receiver_specifications[D3D->n_receiver_specs  ].speaker_index[0]    = index;
      D3D->receiver_specifications[D3D->n_receiver_specs  ].speaker_level[0]    = 1.0F;
      D3D->receiver_specifications[D3D->n_receiver_specs++].n_speakers_affected = 1;
      }

   if (D3D->n_directional_channels > 0)
      {
      V.x /= (F32)D3D->n_directional_channels;
      V.y /= (F32)D3D->n_directional_channels;
      V.z /= (F32)D3D->n_directional_channels;

      //
      // See if a virtual directional speaker needs to be created to keep dot products from going
      // past their 180-degree wraparound points
      //

      len = RAD_vector_length(&V);

      if (len > 0.0001F)
         {
         //
         // First, normalize the vector to move it to the surface of the unit sphere
         // Then invert it to move it as far away from the existing speakers as possible
         //

         V.x = -(V.x / len);
         V.y = -(V.y / len);
         V.z = -(V.z / len);

         //
         // If virtual speaker location is less than 90 degrees from any real speaker, we
         // don't need it
         //

         for (i=0; i < D3D->n_directional_channels; i++)
            {
            F32 cosine = RAD_vector_dot_product(&D3D->listener_to_speaker[i],
                                                &V);
            if (cosine > 0.0F)
               {
               break;
               }
            }

         if (i == D3D->n_directional_channels)
            {
            //
            // Add virtual speaker with channel=-1 to list of directional speakers
            //

            S32 index = D3D->n_directional_channels++;

            D3D->directional_channels[index] = -1;
            D3D->listener_to_speaker [index] = V;

            D3D->receiver_specifications[D3D->n_receiver_specs  ].direction           = D3D->listener_to_speaker[index];
            D3D->receiver_specifications[D3D->n_receiver_specs  ].speaker_index[0]    = index;
            D3D->receiver_specifications[D3D->n_receiver_specs  ].speaker_level[0]    = 1.0F;
            D3D->receiver_specifications[D3D->n_receiver_specs++].n_speakers_affected = 1;
            }
         }
      }
}

#endif

F32 AILCALLBACK SS_default_falloff_function_callback(HSAMPLE   S, //)
                                                     F32       distance,
                                                     F32       rolloff_factor,
                                                     F32       min_dist,
                                                     F32       max_dist)
{
   //
   // Don't access S here -- it may be NULL (e.g, when called from
   // AIL_calculate_3D_channel_levels())
   //
   F32 use_rolloff = rolloff_factor; // default to global rolloff.
   if (S && S->S3D.rolloff != 0)
       use_rolloff = S->S3D.rolloff;

   return min_dist / (min_dist + (use_rolloff * (distance - min_dist)));
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
F32 evaluate_graph(MSSGRAPHPOINT* graph, S32 cnt, F32 X)
{
    if (cnt == 0)
        return 0;

    if (cnt == 1)
        return graph->Y;

    S32 i = 0;
    for (i = cnt - 1; i >= 0; i--)
    {
        if (graph[i].X <= X)
            break;
    }

    if (i < 0)
        return graph->Y;
    if (i == cnt - 1)
        return graph[i].Y;

    MSSGRAPHPOINT* Prev = graph + i;
    MSSGRAPHPOINT* Next = Prev + 1;

    if (Prev->OType == MILES_TANGENT_STEP || 
        Next->IType == MILES_TANGENT_STEP)
        return Prev->Y;

    F32 OTX = Prev->OTX;
    F32 OTY = Prev->OTY;

    if (Prev->OType == MILES_TANGENT_LINEAR)
    {
        OTX = Next->X - Prev->X;
        OTY = Next->Y - Prev->Y;
    }

    F32 ITX = Next->ITX;
    F32 ITY = Next->ITY;

    if (Next->IType == MILES_TANGENT_LINEAR)
    {
        ITX = Prev->X - Next->X;
        ITY = Prev->Y - Next->Y;
    }

    float x[4];
    float y[4];

    x[0] = Prev->X;
    y[0] = Prev->Y;

    x[1] = x[0] + OTX / 3.0f;
    y[1] = y[0] + OTY / 3.0f;

    x[3] = Next->X;
    y[3] = Next->Y;

    x[2] = x[3] - ITX / 3.0f;
    y[2] = y[3] - ITY / 3.0f;

    float hermite[4];

    float dx = x[3] - x[0];
    float dy = y[3] - y[0];

    float m1 = (y[1] - y[0]) / (x[1] - x[0]);
    float m2 = (y[3] - y[2]) / (x[3] - x[2]);

    float length = 1.0f / (dx * dx);

    float d1 = dx * m1;
    float d2 = dx * m2;
    hermite[0] = (d1 + d2 - dy - dy) * length / dx;
    hermite[1] = (dy + dy + dy - d1 - d1 - d2) * length;
    hermite[2] = m1;
    hermite[3] = y[0];

    // Evaluate the hermite.
    float t = X - Prev->X;
    return (t * (t * (t * hermite[0] + hermite[1]) + hermite[2]) + hermite[3]);
}


static S32 AILCALL SS_calculate_3D_attenuation(D3DSTATE* D3D, S3DSTATE* S3D, F32* distance, F32* spread, MSSVECTOR3D listener_to_source[MILES_MAX_SEGMENT_COUNT], F32 lts_scales[MILES_MAX_SEGMENT_COUNT], S32* lts_count, AILFALLOFFCB falloff_function)
{
    *spread = -1;
    *lts_count = 1;

    // If they have defined a position graph, use that instead of a point source.
    if (S3D->position_graph && S3D->position_graph_count >= 2)
    {
        MSSVECTOR3D segment, point;
        float closest_distance_sq = -1;
        S32 closest_index = -1;

        S32 out_idx = 0;
        // Find the closest point on any of the line segments.
        for (S32 i = 0; i < S3D->position_graph_count - 1; i++)
        {
            RAD_vector_subtract(&segment, S3D->position_graph + (i + 1), S3D->position_graph + i);
            float len_sq = RAD_vector_dot_product(&segment, &segment);
            if (len_sq <= MSS_EPSILON)
                continue;

            RAD_vector_subtract(&point, &D3D->listen_position, S3D->position_graph + i);
            float t = RAD_vector_dot_product(&point, &segment) / len_sq;

            if (t < 0)
                t = 0;
            else if (t > 1)
                t = 1;

            segment.x *= t;
            segment.y *= t;
            segment.z *= t;
            
            segment.x += S3D->position_graph[i].x;
            segment.y += S3D->position_graph[i].y;
            segment.z += S3D->position_graph[i].z;

            RAD_vector_subtract(listener_to_source + out_idx, &D3D->listen_position, &segment);
            len_sq = RAD_vector_dot_product(listener_to_source + out_idx, listener_to_source + out_idx);

            if (closest_distance_sq < 0 || closest_distance_sq > len_sq)
            {
                closest_distance_sq = len_sq;
                closest_index = out_idx;
            }
            out_idx++;
        }

        if (closest_distance_sq < 0)
        {
            // We got a degenerate line - treat as a point.
            RAD_vector_subtract(listener_to_source, &S3D->position_graph[0], &D3D->listen_position);
        }

        // Force the closest index in to the zeroth spot.
        if (closest_index != 0)
        {
            MSSVECTOR3D a = listener_to_source[closest_index];
            listener_to_source[closest_index] = listener_to_source[0];
            listener_to_source[0] = a;
        }

        *distance = sqrtf(closest_distance_sq);

        // If we are very close, add in some spread to prevent a sudden earswitch.
        if (*distance < S3D->min_dist && S3D->volgraphcnt == 0 && S3D->spreadgraphcnt == 0)
            *spread = (1 - *distance / S3D->min_dist);
        
        *lts_count = out_idx;

        lts_scales[0] = 1;
        for (S32 i = 1; i < out_idx; i++)
        {
            lts_scales[i] = (*distance / RAD_vector_length(listener_to_source + i));
            lts_scales[i] *= lts_scales[i];
            lts_scales[0] += lts_scales[i]; // track total.
            RAD_vector_normalize(listener_to_source + i);
        }

    }
    else
    {
        RAD_vector_subtract(listener_to_source, &S3D->position, &D3D->listen_position);
        *distance = RAD_vector_length(listener_to_source);
    }

   //
   // Normalize source-listener vector for cone, speaker-angle, and Doppler calculations
   //

   RAD_vector_normalize(listener_to_source);

   S3D->exclusion_3D = -1.0F;
   S3D->lowpass_3D = -1.0F;

   //
   // Return 0 if sound is muted due to max-distance constraint
   //

   if (D3D->mute_at_max)
      {
      if (*distance > S3D->max_dist)
         {
         S3D->atten_3D = 0.0F;
         return 0;
         }
      }
   else
      {
      //
      // Inhibit further attenuation at max dist
      //

      if (*distance > S3D->max_dist)
         {
         *distance = S3D->max_dist;
         }
      }

   if (S3D->excgraphcnt)
       S3D->exclusion_3D = evaluate_graph(S3D->excgraph, S3D->excgraphcnt, *distance);

   if (S3D->lpgraphcnt)
       S3D->lowpass_3D = evaluate_graph(S3D->lpgraph, S3D->lpgraphcnt, *distance);

   if (S3D->spreadgraphcnt)
       S3D->spread = evaluate_graph(S3D->spreadgraph, S3D->spreadgraphcnt, *distance);

   S3D->atten_3D = 1.0F;

   if (S3D->volgraphcnt)
   {
       F32 atten_db = evaluate_graph(S3D->volgraph, S3D->volgraphcnt, *distance);
       S3D->atten_3D = (F32)AIL_pow(10.0F, atten_db / 20.0F);
       if (S3D->atten_3D <= 1.0f/32767.0f)
           S3D->atten_3D = 0;
   }
   else
   {
       //
       // Calculate distance attenuation by inverse-square law for sources > min_dist away from
       // the listener
       //

       F32 min = S3D->min_dist;

       if (min < MSS_EPSILON)
          {
          min = MSS_EPSILON;
          }


       if (*distance > (min + MSS_EPSILON))
          {
          //
          // By default, the volume of a sound is attenuated by 6dB at twice
          // the minimum distance from the listener, 6 dB again at 4X the minimum distance,
          // and so on
          //
          // This behavior can be overridden via a user falloff callback in cases where the function
          // isn't being called on a 3D hardware driver; otherwise, the default DS3D-style handler
          // is used
          //


          // on spu, only call if falloff is null, on everything else, call default on nullc


#ifdef MSS_SPU_PROCESS
          if ( falloff_function == 0 )
             MSS_do_cb5_with_ret( S3D->atten_3D,
                                 (AILFALLOFFCB),
                                  SS_default_falloff_function_callback,
                                 (S3D->owner == NULL) ? 0 : S->driver->callingDS,
                                  S3D->owner,
                                 *distance,
                                  D3D->rolloff_factor,
                                  min,
                                  S3D->max_dist);
#else                                
             MSS_do_cb5_with_ret( S3D->atten_3D,
                                 (AILFALLOFFCB), 
                                 (falloff_function)?  
                                  falloff_function:
                                  SS_default_falloff_function_callback,
                                 (S3D->owner == NULL) ? 0 : S->driver->callingDS,
                                  S3D->owner,
                                 *distance,
                                  D3D->rolloff_factor,
                                  min,
                                  S3D->max_dist);
#endif


       }
   } // end if no volume falloff graph exists.

   //
   // Apply cone falloff if enabled
   //

   if (S3D->cone_enabled)
      {
      //
      // Get angle between source's (normalized) face and listener vectors
      //
      // The source's face vector is negated so it can be dotted with the
      // listener-to-source vector
      //

      MSSVECTOR3D negated_face = S3D->face;
      negated_face.x = -negated_face.x;
      negated_face.y = -negated_face.y;
      negated_face.z = -negated_face.z;

      F32 cosine = RAD_vector_dot_product(&negated_face,
                                           listener_to_source);

      F32 cone_angle;

           if (cosine >  0.9999F) cone_angle = 0.0F;
      else if (cosine < -0.9999F) cone_angle = MY_PI;
      else                        cone_angle = (F32)AIL_acos(cosine);

      //
      // If angle is less than inner angle, volume is unaltered
      // If angle is greater than outer angle, volume is scaled by outer_volume
      // Otherwise, volume is lerped between volume and volume*outer_volume
      //

      if (cone_angle > S3D->outer_angle)
         {
         S3D->atten_3D *= S3D->outer_volume;
         }
      else
         {
         if (cone_angle >= S3D->inner_angle)
            {
            F32 d = S3D->outer_angle - S3D->inner_angle;

            if (d > MSS_EPSILON)
               {
               // alpha=1.0 when angle=inner (level is scaled by 1.0)
               //       0.0 when angle=outer (level is scaled by outer volume)

               F32 alpha = (S3D->outer_angle - cone_angle) / d;

               F32 scalar = S3D->outer_volume + ((1.0F - S3D->outer_volume) * alpha);

               S3D->atten_3D *= scalar;
               }
            }
         }
      }

   //
   // Return 1 to indicate that sound is not past max distance
   //

   return 1;
}

void SS_calculate_3D_channel_levels(D3DSTATE          *D3D, //)
                                    S32                    logical_channels_per_sample,
                                    S3DSTATE          *S3D,
                                    F32               *channel_levels)
{
   //
   // Get 3D attenuation factor, plus normalized vector from listener to source
   //

   MSSVECTOR3D listener_to_source[MILES_MAX_SEGMENT_COUNT];
   F32         lts_scale[MILES_MAX_SEGMENT_COUNT];
   F32         distance;
   F32         spread;
   S32         lts_count;

   if (!SS_calculate_3D_attenuation(D3D, S3D, &distance, &spread, listener_to_source, lts_scale, &lts_count, S3D->falloff_function))
      {
      //
      // Sample is muted due to exceeding max distance with D3D.mute_at_max enabled
      //
      // Note: we now do this even if status==SMP_STOPPED ...
      //

      for (S32 i=0; i < logical_channels_per_sample; i++)
         {
         channel_levels[i] = 0.0F;
         }

      return;
      }

   //
   // Get base volume attenuation
   //

   F32 vol = S3D->atten_3D;

   if (spread == -1)
       spread = S3D->spread;

   //
   // If source and listener are coincident, divide unscaled 3D volume level linearly among
   // all channels
   //

   S32 spkr;

   if (distance <= MSS_EPSILON)
      {
      for (spkr=0; spkr < logical_channels_per_sample; spkr++)
         {
         channel_levels[spkr] = vol * AIL_fsqrt(1.0F / logical_channels_per_sample);
         }
      }
   else
      {
      F32 speaker_contributions[MAX_SPEAKERS];     // Physical speakers, indexed by [0,logical_channels_per_sample-1]
      AIL_memset(speaker_contributions, 0, sizeof(speaker_contributions));

      S32 i;

      //
      // Send proportion of sound to ambient radiators
      //

      F32 frac = AIL_fsqrt(1.0F / logical_channels_per_sample);

      for (i=0; i < D3D->n_ambient_channels; i++)
         {
         spkr = D3D->ambient_channels[i];

         speaker_contributions[spkr] = vol * frac;
         }

      //
      // Spatialize each source vector for the sample.
      //
      for (S32 source=0; source < lts_count; source++)
      {
          MSSVECTOR3D* lts = listener_to_source + source;

          // Sources are scaled based on relation to "loudest" vector - but only out of total scale (which is stored in [0])
          F32 vector_scale = lts_scale[source] / lts_scale[0];

          //
          // Record angle between source-listener vector and receiver-listener vector for each
          // receiver point
          //
          // Keep track of the sum of the angles
          //
          F32 reception_angle[MAX_RECEIVER_SPECS];
          F32 theta_sum = 0.0F;

          for (i=0; i < D3D->n_receiver_specs; i++)
          {
              MSS_RECEIVER_LIST *RCVR = &D3D->receiver_specifications[i];

              F32 cosine = RAD_vector_dot_product(&RCVR->direction, lts);

              if (cosine >  0.9999F) reception_angle[i] = MY_PI;
              else if (cosine < -0.9999F) reception_angle[i] = 0.0F;
              else                        reception_angle[i] = MY_PI - (F32) AIL_acos(cosine);   // pi = loud, 0 = quiet

              reception_angle[i] = MY_PI - (1 - spread)*(MY_PI - reception_angle[i]);

              theta_sum += reception_angle[i];
          }

          //
          // Perform constant-power pan among all directional channels, including
          // virtual speaker if present
          //

          for (i=0; i < D3D->n_receiver_specs; i++)
          {
              MSS_RECEIVER_LIST *RCVR = &D3D->receiver_specifications[i];

              F32 src_lvl = vector_scale * AIL_fsqrt(AIL_pow(reception_angle[i], D3D->falloff_power) / theta_sum);

              for (S32 j=0; j < RCVR->n_speakers_affected; j++)
              {
                  spkr = RCVR->speaker_index[j];

                  F32 contrib = src_lvl * RCVR->speaker_level[j];

                  S32 phys_spkr = D3D->directional_channels[spkr];

                  if (phys_spkr == -1)
                  {
                      //
                      // This is a virtual speaker
                      //
                      // If we don't spread a little of the virtual-speaker's contribution among the other speakers,
                      // then mono sources directly behind the listener will drop out entirely.  This calculation
                      // doesn't have to be physically-accurate
                      //

                      for (S32 ii=0; ii < D3D->n_receiver_specs; ii++)
                      {
                          MSS_RECEIVER_LIST *RCVR = &D3D->receiver_specifications[ii];

                          for (S32 ij=0; ij < RCVR->n_speakers_affected; ij++)
                          {
                              S32 phys_spkr = D3D->directional_channels[RCVR->speaker_index[ij]];

                              if (phys_spkr != -1)
                              {
                                  speaker_contributions[phys_spkr] += (contrib / (D3D->n_receiver_specs * 2));
                              }
                          }
                      }

                      continue;
                  }

                  speaker_contributions[phys_spkr] += contrib;
              }
          }
      }
      //
      // Final channel level calculation
      //
      // Attenuate by 0-6 dB for falloff powers = 1.0 - 4.0 to help compensate for
      // volume-boosting effect at higher falloff powers
      //

      F32 falloff_compensation = AIL_fsqrt(D3D->falloff_power);

      for (spkr=0; spkr < logical_channels_per_sample; spkr++)
         {
         F32 lvl = speaker_contributions[spkr] / (falloff_compensation * lts_count);

              if (lvl <= 0.0F) lvl = 0.0F;
         else if (lvl >= 1.0F) lvl = 1.0F;

         channel_levels[spkr] = lvl * vol;
         }
   }

   //
   // Enable Doppler frequency-shifting if relative velocity is nonzero
   //

   MSSVECTOR3D movement_dir;
   RAD_vector_subtract(&movement_dir, &S3D->velocity, &D3D->listen_velocity);

   F32 velocity;
   RAD_vector_normalize(&movement_dir, &velocity);

   S3D->doppler_valid = 0;

   if (velocity > MSS_EPSILON)
      {
      //
      // Sign of velocity determined by whether object is approaching
      // source or receding from it
      //

      velocity *= RAD_vector_dot_product(listener_to_source + 0,
                                         &movement_dir);

      velocity *= D3D->distance_factor * D3D->doppler_factor;

      if (velocity > SPEED_OF_SOUND)
         {
         S3D->doppler_shift = MIN_DOPPLER_SHIFT;
         }
      else if (velocity < -SPEED_OF_SOUND)
         {
         S3D->doppler_shift = MAX_DOPPLER_SHIFT;
         }
      else
         {
         F32 v = SPEED_OF_SOUND + velocity;

         S3D->doppler_shift = SPEED_OF_SOUND / v;

         if (S3D->doppler_shift > MAX_DOPPLER_SHIFT)      S3D->doppler_shift = MAX_DOPPLER_SHIFT;
         else if (S3D->doppler_shift < MIN_DOPPLER_SHIFT) S3D->doppler_shift = MIN_DOPPLER_SHIFT;
         }

      S3D->doppler_valid = 1;
      }
}

#ifndef MSS_SPU_PROCESS

//############################################################################
//##                                                                        ##
//## Make MSS 3D positioning algorithm available to API                     ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_calculate_3D_channel_levels(HDIGDRIVER                   driver, //)
                                                F32                     *channel_levels,
                                                MSS_SPEAKER const * *speaker_array,
                                                MSSVECTOR3D             *src_pos,
                                                MSSVECTOR3D             *src_face,
                                                MSSVECTOR3D             *src_up,
                                                F32                          src_inner_angle,
                                                F32                          src_outer_angle,
                                                F32                          src_outer_volume,
                                                F32                          src_max_dist,
                                                F32                          src_min_dist,
                                                MSSVECTOR3D             *listen_pos,
                                                MSSVECTOR3D             *listen_face,
                                                MSSVECTOR3D             *listen_up,
                                                F32                          rolloff_factor,
                                                MSSVECTOR3D             *doppler_velocity,
                                                F32                         *doppler_shift)
{
   if ((driver == NULL)                           ||
       (driver->logical_channels_per_sample == 0) ||
       (channel_levels == NULL)                   ||
       (speaker_array == NULL))
      {
      return 0;
      }

   //
   // Reproduce behavior of AIL_set_listener_3D_orientation() with a D3D object that
   // inherits its speaker positions and falloff power (but not its listener transform)
   // from the driver
   //

   D3DSTATE D3D;
   AIL_memcpy(&D3D, &driver->D3D, sizeof(D3D));

   D3D.rolloff_factor    = rolloff_factor;
   D3D.listen_velocity.x = D3D.listen_velocity.y = D3D.listen_velocity.z = 0.0F;

   D3D.listen_position = *listen_pos;
   D3D.listen_face     = *listen_face;
   D3D.listen_up       = *listen_up;

   RAD_vector_normalize(&D3D.listen_face);
   RAD_vector_normalize(&D3D.listen_up);

   RAD_vector_cross_product(&D3D.listen_cross,
                            &D3D.listen_face,
                            &D3D.listen_up);

   RAD_vector_normalize(&D3D.listen_cross);

   SS_set_speaker_configuration(&D3D,
                                 NULL,
                                 0,
                                 driver->logical_channels_per_sample);

   //
   // Create and initialize a source object without an associated HSAMPLE, and declare
   // our static channel-level array
   //
   // For Doppler calculations, we apply the optional Doppler velocity parameter to the source,
   // leaving the listener velocity at zero
   //

   S3DSTATE S3D;
   AIL_memset(&S3D, 0, sizeof(S3D));

   if (doppler_velocity != NULL)
      {
      S3D.velocity = *doppler_velocity;
      }

   S3D.position = *src_pos;
   S3D.face     = *src_face;
   S3D.up       = *src_up;

   S3D.inner_angle  = src_inner_angle;
   S3D.outer_angle  = src_outer_angle;
   S3D.outer_volume = src_outer_volume;
   S3D.cone_enabled = (src_inner_angle < 360.0F) && (src_outer_volume < 1.0F);

   S3D.max_dist         = src_max_dist;
   S3D.min_dist         = src_min_dist;
   S3D.dist_changed     = TRUE;
   S3D.owner            = NULL;
   S3D.falloff_function = 0;

   //
   // Calculate channel levels via 3D positioner
   //

   SS_calculate_3D_channel_levels(&D3D,
                                   driver->logical_channels_per_sample,
                                  &S3D,
                                   channel_levels);

   *speaker_array = output_speaker_order[driver->logical_channels_per_sample];

   //
   // Return Doppler scale factor if velocity above minimum epsilon value, else 1.0
   //

   if ((doppler_velocity != NULL) && (doppler_shift != NULL))
      {
      if (S3D.doppler_valid)
         *doppler_shift = S3D.doppler_shift;
      else
         *doppler_shift = 1.0F;
      }

   return driver->logical_channels_per_sample;
}

//############################################################################
//##                                                                        ##
//## Set parameters of existing HSAMPLE according to info struct            ##
//##                                                                        ##
//## Returns 0 on error, else 1                                             ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_set_sample_info(HSAMPLE S, AILSOUNDINFO const * info)
{
  S32 format=0;
  HPROVIDER HP = 0;

  if ((S==NULL) || (info==NULL))
     return(0);

  if ((info->channels > 2) || (info->channel_mask != ~0U))
  {
    format = DIG_F_MULTICHANNEL_MASK | (info->channels << 16);
  }
  else if ( info->channels == 2 )
  {
    format = DIG_F_STEREO_MASK;
  }

  {
    if (info->format==WAVE_FORMAT_PCM)
    {
      if ( info->bits == 16 )
      {
        format |= DIG_F_16BITS_MASK;
      }
      else if ( info->bits != 8 )
      {
        goto wave_err;
      }
    }
    else if ((info->format==WAVE_FORMAT_IMA_ADPCM) && (info->bits==4))
    {
      format |= DIG_F_ADPCM_MASK;
    }
    else
    {
      // find asi
      HP = RIB_find_file_dec_provider( "ASI codec",
                                       "Input wave tag",
                                       info->format,
                                       "Output file types",
                                       ".raw" );

      if (!HP)
      {
       wave_err:
        AIL_set_error("Unsupported wave file format.");
        return 0;
      }
    }
  }

  if (!AIL_init_sample( S, format ))
     {
     return 0;
     }

  AIL_set_sample_playback_rate ( S, info->rate );

  //
  // Configure adpcm if required
  //

  if ( format & DIG_F_ADPCM_MASK )
  {
    AIL_set_sample_adpcm_block_size( S, info->block_size );
  }

  //
  // Configure sample address and length based on DATA chunk
  //

  AIL_set_sample_address( S, info->data_ptr, info->data_len );

  //
  // Set up to use specified ASI codec (or none at all) to decode data for mixer
  //
  // Note that the incoming S->channel_mask member is used to determine whether
  // Ogg files are forced into multichannel mode
  //

  if (AIL_set_sample_processor(S,
                               SP_ASI_DECODER,
                               HP) == (HPROVIDER) -1)
      {
      return 0;
      }

  return( 1 );
}

//############################################################################
//##                                                                        ##
//## Set parameters of existing HSAMPLE according to file data              ##
//##                                                                        ##
//## Returns 0 on error, else 1                                             ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_set_sample_file(HSAMPLE S, void const *file_image, S32 block)
{
   S32 type;
   AILSOUNDINFO info;

   if ((S==NULL) || (file_image==NULL))
     return(0);

   if (!AIL_init_sample(S, DIG_F_MONO_16))
      {
      return 0;
      }

   //
   // Identify file type
   //
   //

   if (!AIL_strnicmp((char*)(((VOC *) file_image)->ID_string),"Creative",8))
      {
      type = FTYP_VOC;
      }
   else if (LE_SWAP32(file_image) == 'BCF1')
   {
       // Find the bink provider.
       HPROVIDER BinkAsi = find_ASI_provider("Input file types", ".binka");
       if (BinkAsi == 0)
       {
           AIL_set_error("Bink Audio provider not found.");
           return 0;
       }

       clear_last_sample(S);

       // The file size is stored in the header of the bcf file.
       // at offset 16.
       U32 FileSize = LEU_SWAP32(AIL_ptr_add(file_image, 16));

       AIL_set_sample_address(S, file_image, FileSize);

       if (AIL_set_sample_processor(S, SP_ASI_DECODER, BinkAsi) == (HPROVIDER) -1)
       {
           return 0;
       }

       if (AIL_sample_processor(S, SP_ASI_DECODER) == 0)
       {
           AIL_set_error("Bink Audio provider failed to initialize.");
           return 0;
       }

       return 1;
   }
   else if (!AIL_strnicmp((char*) file_image,"OggS",4))
      {
      //
      // If "Speex" present in first 128 bytes of file image, use .spx decoder
      // Otherwise, we'll assume it's a Vorbis (.ogg) file
      //

      C8 const *suffix = ".ogg";

      for (S32 i=0; i < 128; i++)
         {
         if (!AIL_strnicmp((C8 *) AIL_ptr_add(file_image,i), "Speex", 5))
            {
            suffix = ".spx";
            break;
            }
         }

      HPROVIDER ASI = find_ASI_provider("Input file types",
                                        suffix);
      if ( ASI )
        {
          //
          // Set up to use specified ASI codec to decode data for mixer
          //

          if (AIL_set_sample_processor(S,
                                       SP_ASI_DECODER,
                                       ASI) == (HPROVIDER) -1)
            {
            return 0;
            }

          return( 1 );
        }
      else
         {
        AIL_set_error("Unsupported wave file format.");
        return 0;
        }
      }
   else if (AIL_WAV_info(file_image,&info))
      {
      if ( (info.format==WAVE_FORMAT_PCM)
           || ((info.format==WAVE_FORMAT_IMA_ADPCM) && (info.bits==4))
         )
        type = FTYP_WAV;
      else
        {

        // handle asi wave files

        HPROVIDER ASI = RIB_find_file_dec_provider( "ASI codec",
                                                    "Input wave tag",
                                                    info.format,
                                                    "Output file types",
                                                    ".raw" );
        if ( ASI )
        {
          //
          // set sample address and size for encoded data image
          //

          AIL_set_sample_address(S,info.data_ptr,info.data_len );

          //
          // Set up to use specified ASI codec to decode data for mixer
          //

          if (AIL_set_sample_processor(S,
                                       SP_ASI_DECODER,
                                       ASI) == (HPROVIDER) -1)
            {
            return 0;
            }

          return( 1 );
        }

        AIL_set_error("Unsupported wave file format.");
        return 0;
        }
      }
   else
      {
      AIL_set_error("Unknown file type.");
      return 0;
      }

   //
   // Copy file attributes to sample
   //

   switch (type)
      {
      case FTYP_VOC:

         S->system_data[VOC_BLK_PTR]      = (UINTa) AIL_ptr_add(file_image,
                                                 ((VOC *) file_image)->
                                                 data_offset);

         S->system_data[VOC_MARKER]       = block;
         S->system_data[VOC_MARKER_FOUND] = (block == -1);

         if (!AIL_process_VOC_block(S,0))
            {
            return 0;
            }
         break;

      case FTYP_WAV:

         if (!AIL_process_WAV_image(&info,S))
            {
            return 0;
            }
         break;

      default:

         AIL_set_error("Invalid or missing data block.");
         return 0;
      }

   return 1;
}

//############################################################################
//##                                                                        ##
//## Set parameters of existing HSAMPLE according to file data, using       ##
//## the file suffix to specify the file type                               ##
//##                                                                        ##
//## Returns 0 on error, else 1                                             ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_set_named_sample_file (HSAMPLE   S, //)
                                           C8   const *file_type_suffix,
                                           void const *file_image,
                                           U32       file_size,
                                           S32       block)
{
   if ((S==NULL) || (file_image==NULL))
     return(0);

   //
   // is this a wacky id3?
   //

   if ( ( ((U8 const* )file_image)[ 0 ] == 0x49 ) && ( ((U8 const* )file_image)[ 1 ] == 0x44 ) && ( ((U8 const* )file_image)[ 2 ] == 0x33 ) &&
        ( ((U8 const* )file_image)[ 3 ] < 0xff ) && ( ((U8 const* )file_image)[ 4 ] < 0xff ) &&
        ( ((U8 const* )file_image)[ 6 ] < 0x80 ) && ( ((U8 const* )file_image)[ 7 ] < 0x80 ) && ( ((U8 const* )file_image)[ 8 ] < 0x80 ) && ( ((U8 const* )file_image)[ 9 ] < 0x80 ) )
   {
     U32 skip = 10 + ( ( (U32) ((U8 const* )file_image)[ 9 ] ) | ( ( (U32) ((U8 const* )file_image)[ 8 ] ) << 7 ) |
                       ( ( (U32) ((U8 const* )file_image)[ 7 ] ) << 14 ) | ( ( (U32) ((U8 const* )file_image)[ 6 ] ) << 21 ) );

     file_image = ( (U8*) file_image ) + skip;
     file_size -= skip;
     file_type_suffix = ".mp3";
   }

   //
   // If file type not specified, pass to .WAV / VOC handler
   //

   if ((file_type_suffix == NULL) ||
       (!AIL_strlen(file_type_suffix)))
      {
      return AIL_set_sample_file(S,
                                 file_image,
                                 block);
      }

   if (!AIL_init_sample(S, DIG_F_MONO_16))
      {
      return 0;
      }

   //
   // Search for ASI codec capable of processing this input file type
   //

   HPROVIDER HP = find_ASI_provider("Input file types",
                                     file_type_suffix);

   //
   // If no specific provider available for this file type, try the default
   // .WAV/.VOC file handler
   //

   if (!HP)
      {
      if (AIL_strlen(file_type_suffix) >= 4)
         {
         if ((!AIL_stricmp(&file_type_suffix[AIL_strlen(file_type_suffix)-4],
                        ".WAV"))
               ||
            (!AIL_stricmp(&file_type_suffix[AIL_strlen(file_type_suffix)-4],
                        ".VOC")))
            {
            return AIL_set_sample_file(S,
                                       file_image,
                                       block);
            }
         }

      //
      // Not a .WAV or .VOC file, fail call
      //

      AIL_set_error( "Couldn't find provider for this file type -- check your AIL_set_redist_directory() call." );

      return 0;
      }


   // reset default filter values

   clear_last_sample(S);

   //
   // Otherwise, set sample address and size for encoded data image
   //

   AIL_set_sample_address(S,
                          file_image,
                          file_size);

   //
   // Set up to use specified ASI codec to decode data for mixer
   //

   if (AIL_set_sample_processor(S,
                                SP_ASI_DECODER,
                                HP) == (HPROVIDER) -1)
      {
      return 0;
      }

   HPROVIDER check = AIL_sample_processor(S,
                                          SP_ASI_DECODER);

   if (check == 0)
      {
      AIL_set_error("ASI provider for this file type failed to initialize");
      return 0;
      }

   //
   // Return success
   //

   return 1;
}

//############################################################################
//##                                                                        ##
//## Get RIB provider, if any, for specified sample pipeline phase          ##
//##                                                                        ##
//############################################################################

HPROVIDER    AILCALL AIL_API_sample_processor  (HSAMPLE     S, //)
                                                SAMPLESTAGE pipeline_stage)
{
   S32 stage = pipeline_stage;

   if (stage == SAMPLE_ALL_STAGES)
      {
      stage = 0;
      }

   if (S->pipeline[stage].active == 0)
      {
      return 0;
      }

   return S->pipeline[stage].provider;
}

//############################################################################
//##                                                                        ##
//## Set a RIB provider for a specified sample pipeline phase               ##
//##                                                                        ##
//############################################################################

HPROVIDER    AILCALL AIL_API_set_sample_processor  (HSAMPLE     S, //)
                                                    SAMPLESTAGE pipeline_stage,
                                                    HPROVIDER   provider)
{
   S32 stage = pipeline_stage;

   if (stage == SAMPLE_ALL_STAGES)
      {
      stage = 0;
      }

   SPINFO *SP;

   do
      {
      SP = &S->pipeline[stage];

      //
      // If existing stage is being replaced, shut it down first
      //

      if (SP->active)
         {
         switch (stage)
            {
            //
            // Output voice filter
            //

            case SP_OUTPUT:
               break;

            //
            // "MSS mixer services"
            //

            case SP_MERGE:
               break;

            //
            // "ASI codec stream"
            //

            case SP_ASI_DECODER:
               SP->TYPE.ASI.ASI_stream_close(SP->TYPE.ASI.stream);
               break;

            //
            // "MSS pipeline filter"
            //

            case SP_FILTER_0:
            case SP_FILTER_1:
            case SP_FILTER_2:
            case SP_FILTER_3:
            case SP_FILTER_4:
            case SP_FILTER_5:
            case SP_FILTER_6:
            case SP_FILTER_7:

               #ifdef MSS_FLT_SUPPORTED
               {for( int i = 0 ; i < MAX_SPEAKERS ; i++ )
               {
                 if (SP->TYPE.FLT.sample_state[i])
                 {
                   SP->TYPE.FLT.provider->close_sample(SP->TYPE.FLT.sample_state[i]);
                   if ( SP->TYPE.FLT.sample_state[i] )
                   {
                     AIL_mem_free_lock( (void*)SP->TYPE.FLT.sample_state[i] );
                     SP->TYPE.FLT.sample_state[i]=0;
                   }
                 }
               }}
               --S->n_active_filters;
               #endif
               break;
            }

         SP->active = 0;
         }
      }
   while ((pipeline_stage == SAMPLE_ALL_STAGES) && (++stage < N_SAMPLE_STAGES));

   //
   // Set up to return previous provider
   //

   HPROVIDER previous = SP->provider;

   SP->provider = provider;

   //
   // If no new provider specified, exit normally after shutting down
   // existing provider
   //

   if (!provider)
      {
      return previous;
      }

   //
   // Query required attributes from provider interface
   //

   switch (stage)
      {
      //
      // Output voice filter
      //

      case SP_OUTPUT:
         break;

      //
      // "MSS mixer services"
      //

      case SP_MERGE:
         {
         //
         // Get codec properties and store in pipeline-stage data structure
         //

         RIB_INTERFACE_ENTRY MIXER[] =
            {
            { RIB_FUNCTION, "MIXER_merge", (UINTa) &SP->TYPE.MSS_mixer_merge, RIB_NONE },
            };

         RIB_request(SP->provider,"MSS mixer services",MIXER);

         //
         // Indicate installation of mixer module
         //

         SP->active = 1;
         break;
         }

      //
      // "MSS pipeline filter"
      //

      case SP_FILTER_0:
      case SP_FILTER_1:
      case SP_FILTER_2:
      case SP_FILTER_3:
      case SP_FILTER_4:
      case SP_FILTER_5:
      case SP_FILTER_6:
      case SP_FILTER_7:
         {

         #ifdef MSS_FLT_SUPPORTED

         //
         // open the filter if it ain't already opened
         //

         if (!AIL_open_filter(provider, S->driver))
            {
            SP->provider = 0;
            return previous;
            }

         //
         // Find DRIVERSTATE wrapper for this provider and driver
         //

         FLTPROVIDER *F = FLT_find_provider_instance(provider, S->driver);

         SP->TYPE.FLT.provider = F;

         {for( int i = 0 ; i < MAX_SPEAKERS ; i++ )
         {
             SP->TYPE.FLT.sample_state[ i ] = 0;
         }}

         if ((F==NULL) || (F->open_sample==NULL))
           {
           SP->provider = 0;
           return previous;
           }

         //
         // Create SAMPLESTATE structure for this HSAMPLE and HDRIVERSTATE
         //
#ifdef MSS_VFLT_SUPPORTED
         if (S->driver->voice_filter != NULL)
            {
            // (so the driver can perform initial calculations based on the post-resampled output rate)
            S->driver->DMA_rate = (S32) ( ( (F32) S->original_playback_rate ) * S->playback_rate_factor );
            }
#endif
         {for( int i = 0 ; i < S->n_channels ; i++ )
         {
             SP->TYPE.FLT.sample_state[ i ] = F->open_sample(F->driver_state,
                                                             S,
                                                             ( F->sample_size == 0 ) ? 0 : AIL_mem_alloc_lock( F->sample_size ) );
             if (!SP->TYPE.FLT.sample_state[i])
             {
               SP->provider = 0;
               return previous;
             }
         }}


#if defined(HOST_SPU_PROCESS) || defined(MSS_SPU_PROCESS)
         if ( ( F->provider_flags & FPROV_SPU_MASK ) == 0 )
           S->spu_on = 0;
#endif

         //
         // Indicate installation of filter module
         //

         SP->active = 1;

         ++S->n_active_filters;

         #endif

         break;
         }

      //
      // "ASI codec stream"
      //

      case SP_ASI_DECODER:
         {
         //
         // Get codec properties and store in pipeline-stage data structure
         // Do this in blocks of <= 4 entries to avoid external lib call warning on PS2 compiler
         //

         ASISTAGE *ASI = &SP->TYPE.ASI;

         RIB_INTERFACE_ENTRY ASISTR1[] =
            {
            { RIB_FUNCTION,   "ASI_stream_property",       (UINTa) &ASI->ASI_stream_property,       RIB_NONE },
            { RIB_FUNCTION,   "ASI_stream_open",           (UINTa) &ASI->ASI_stream_open,           RIB_NONE },
            { RIB_FUNCTION,   "ASI_stream_seek",           (UINTa) &ASI->ASI_stream_seek,           RIB_NONE },
            { RIB_FUNCTION,   "ASI_stream_close",          (UINTa) &ASI->ASI_stream_close,          RIB_NONE }
            };

         RIB_INTERFACE_ENTRY ASISTR2[] =
            {
            { RIB_FUNCTION,   "ASI_stream_process",        (UINTa) &ASI->ASI_stream_process,        RIB_NONE },
            { RIB_PROPERTY,   "Input bit rate",            (UINTa) &ASI->INPUT_BIT_RATE,            RIB_NONE },
            { RIB_PROPERTY,   "Input sample rate",         (UINTa) &ASI->INPUT_SAMPLE_RATE,         RIB_NONE },
            };

         RIB_INTERFACE_ENTRY ASISTR3[] =
            {
            { RIB_PROPERTY,   "Input sample width",        (UINTa) &ASI->INPUT_BITS,                RIB_NONE },
            { RIB_PROPERTY,   "Input channels",            (UINTa) &ASI->INPUT_CHANNELS,            RIB_NONE },
            { RIB_PROPERTY,   "Output bit rate",           (UINTa) &ASI->OUTPUT_BIT_RATE,           RIB_NONE },
            { RIB_PROPERTY,   "Output sample rate",        (UINTa) &ASI->OUTPUT_SAMPLE_RATE,        RIB_NONE },
            };


         RIB_INTERFACE_ENTRY ASISTR4[] =
            {
            { RIB_PROPERTY,   "Output sample width",       (UINTa) &ASI->OUTPUT_BITS,               RIB_NONE },
            { RIB_PROPERTY,   "Output channels",           (UINTa) &ASI->OUTPUT_CHANNELS,           RIB_NONE },
            { RIB_PROPERTY,   "Output reservoir",          (UINTa) &ASI->OUTPUT_RESERVOIR,          RIB_NONE },
            { RIB_PROPERTY,   "Position",                  (UINTa) &ASI->POSITION,                  RIB_NONE },
            };

         RIB_INTERFACE_ENTRY ASISTR5[] =
            {
            { RIB_PROPERTY,   "Percent done",              (UINTa) &ASI->PERCENT_DONE,              RIB_NONE },
            { RIB_PROPERTY,   "Minimum input block size",  (UINTa) &ASI->MIN_INPUT_BLOCK_SIZE,      RIB_NONE },
            { RIB_PROPERTY,   "Raw source sample rate",    (UINTa) &ASI->RAW_RATE,                  RIB_NONE },
            { RIB_PROPERTY,   "Raw source sample width",   (UINTa) &ASI->RAW_BITS,                  RIB_NONE },
            };

         RIB_INTERFACE_ENTRY ASISTR6[] =
            {
            { RIB_PROPERTY,   "Raw source channels",       (UINTa) &ASI->RAW_CHANNELS,              RIB_NONE },
            { RIB_PROPERTY,   "Requested sample rate",     (UINTa) &ASI->REQUESTED_RATE,            RIB_NONE },
            { RIB_PROPERTY,   "Requested sample width",    (UINTa) &ASI->REQUESTED_BITS,            RIB_NONE },
            { RIB_PROPERTY,   "Requested channels",        (UINTa) &ASI->REQUESTED_CHANS,           RIB_NONE }
            };

         RIB_INTERFACE_ENTRY ASISTR7[] =
            {
            { RIB_PROPERTY,   "Output channel mask",       (UINTa) &ASI->OUTPUT_CHANNEL_MASK,       RIB_NONE },
            { RIB_PROPERTY,   "Stream seek position",      (UINTa) &ASI->STREAM_SEEK_POS,           RIB_NONE },
            { RIB_PROPERTY,   "Data offset",               (UINTa) &ASI->DATA_START_OFFSET,         RIB_NONE },
            { RIB_PROPERTY,   "Data length",               (UINTa) &ASI->DATA_LEN,                  RIB_NONE },
            };

         RIB_INTERFACE_ENTRY ASISTR8[] = 
         {
             { RIB_PROPERTY, "Exact seek",         (UINTa)&ASI->EXACT_SEEK,   RIB_NONE },
             { RIB_PROPERTY, "Exact get position", (UINTa)&ASI->EXACT_GETPOS, RIB_NONE },
             { RIB_PROPERTY, "Seek table lookup",  (UINTa)&ASI->SEEK_LOOKUP, RIB_NONE },
             { RIB_PROPERTY, "Set Looping Samples", (UINTa)&ASI->SET_LOOPING_SAMPLES, RIB_NONE },
             { RIB_PROPERTY, "Clear Loop Meta",     (UINTa)&ASI->CLEAR_LOOP_META, RIB_NONE }
         };


         RIB_request(SP->provider,"ASI stream",ASISTR1);
         RIB_request(SP->provider,"ASI stream",ASISTR2);
         RIB_request(SP->provider,"ASI stream",ASISTR3);
         RIB_request(SP->provider,"ASI stream",ASISTR4);
         RIB_request(SP->provider,"ASI stream",ASISTR5);
         RIB_request(SP->provider,"ASI stream",ASISTR6);
         RIB_request(SP->provider,"ASI stream",ASISTR7);
         RIB_request(SP->provider,"ASI stream",ASISTR8);

         //
         // Open stream with codec, registering callback function
         //

         ASI->stream = ASI->ASI_stream_open( (MSS_ALLOC_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_alloc_info ), 
                                             (MSS_FREE_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_free_info ),
                                             (UINTa) S,
                                             (AILASIFETCHCB) MSS_CALLBACK_ALIGNED_NAME( SP_ASI_DECODER_callback ),
                                             S->buf[0].len);

#if defined(HOST_SPU_PROCESS) || defined(MSS_SPU_PROCESS)
        HPROPERTY spu_flags_prop = (UINTa) -1;
        RIB_INTERFACE_ENTRY SPUSTR[] =
        {
          { RIB_PROPERTY,   "SPU flags",       (UINTa) &spu_flags_prop,       RIB_NONE }
        };

        RIB_request(SP->provider,"ASI codec",SPUSTR);
  
        if ( spu_flags_prop == (UINTa) -1 )
           S->spu_on = 0;
#endif

         HPROPERTY profile_index = (UINTa) -1;
         RIB_INTERFACE_ENTRY PROFSTR[] =
         {
           { RIB_PROPERTY,   "Profile index",       (UINTa) &profile_index,       RIB_NONE }
         };
         RIB_request(SP->provider,"ASI codec",PROFSTR);
         if ( ( profile_index >= 0x1000 ) && ( profile_index <= 0x1010 ) )
           profile_index -= 0x1000;
         else
           profile_index = 0;


         // jkr - exit on error
         if (ASI->stream==0)
         {
           SP->provider=0;
           return(previous);
         }

         //
         // Request codec output format which matches hardware format
         //

         ASI->ASI_stream_property(ASI->stream,
                                  ASI->REQUESTED_RATE,
                                  0,&S->driver->DMA_rate,0);

         U32 achans = 1 + ((S->driver->hw_format & (DIG_F_STEREO_MASK | DIG_F_MULTICHANNEL_MASK)) != 0);

         ASI->ASI_stream_property(ASI->stream,
                                  ASI->REQUESTED_CHANS,
                                  0,&achans,0);

         U32 abits=8 + (8 * ((S->driver->hw_format & DIG_F_16BITS_MASK) != 0));

         ASI->ASI_stream_property(ASI->stream,
                                  ASI->REQUESTED_BITS,
                                  0,&abits,0);

         //
         // Reconfigure sample type and rate according to codec's actual output
         // format
         //
         // Most functions that call this one do so right after initializing the HSAMPLE... and
         // that means that most of the time, the channel mask is ~0U.  To preserve existing
         // behavior with stereo Ogg files, we want to treat 2-channel ASIs as stereo files in
         // that case.  If, however, we get here from a call to AIL_set_sample_info() where the
         // user specified a valid channel mask, we want to make sure we always reinitialize the
         // HSAMPLE in multichannel mode.  This is the only way to get MSS to apply full
         // multichannel routing to a 2-channel Ogg file!
         //

         U32 nch  = (U32)~0; ASI->ASI_stream_property(ASI->stream, ASI->OUTPUT_CHANNELS,&nch,0,0);
         U32 rate = (U32)~0; ASI->ASI_stream_property(ASI->stream, ASI->OUTPUT_SAMPLE_RATE,&rate,0,0);
         U32 bits = (U32)~0; ASI->ASI_stream_property(ASI->stream, ASI->OUTPUT_BITS,&bits,0,0);

         U32 current_mask = 0;
         U32 current_chan = AIL_sample_channel_count(S, &current_mask);
         U32 current_rate = AIL_sample_playback_rate(S);

         if (current_rate != rate)
         {
            AIL_set_sample_playback_rate(S, rate);
         }

         if ((nch > 2) || (S->channel_mask != ~0U))
            {
            U32 mask = ~0U;

            if (ASI->OUTPUT_CHANNEL_MASK != (UINTa)~0)
               {
               ASI->ASI_stream_property(ASI->stream, ASI->OUTPUT_CHANNEL_MASK,&mask,0,0);
               }

            if (mask != current_mask || current_chan != nch)
            {
                if (!SS_set_sample_type(S,
                                        DIG_F_MULTICHANNEL_16 | (nch << 16),
                                        mask)) return (HPROVIDER) -1;
            }
            }
         else if (nch != current_chan)
            {
            if (nch == 1)
               {
               if (!SS_set_sample_type(S,
                                      (bits == 16) ? DIG_F_MONO_16   : DIG_F_MONO_8,
                                      ~0U)) return (HPROVIDER) -1;
               }
            else
               {
               if (!SS_set_sample_type(S,
                                      (bits == 16) ? DIG_F_STEREO_16 : DIG_F_STEREO_8,
                                     ~0U)) return (HPROVIDER) -1;
               }
            }

         //
         // If ASI codec has identified a data chunk within the sample file image, adjust
         // our sample address and length parameters accordingly
         //
         // Henceforth, position 0 refers to the start of the data chunk, rather than the
         // beginning of the file...
         //

         if (S->service_type == SERVICE_SINGLE_BUFFER)
            {
            if (ASI->DATA_START_OFFSET != (UINTa)~0)
               {
               U32 offset = 0; ASI->ASI_stream_property(ASI->stream, ASI->DATA_START_OFFSET,&offset,0,0);

               S->buf[0].start = AIL_ptr_add(S->buf[0].start, offset );
               S->buf[0].pos   = 0;      // (this effectively moves the old .pos to .start, so we need to reset .pos to 0)
               }

            if (ASI->DATA_LEN != (UINTa)~0)
               {
               ASI->ASI_stream_property(ASI->stream, ASI->DATA_LEN,&S->buf[0].len,0,0);
               }
            }

         //
         // Indicate installation of decoder module
         //

         SP->active = (S32) ( profile_index + 1 );  // active also stores the profiling index
         break;
         }
      }

   //
   // Return success
   //

   return previous;
}

//############################################################################
//##                                                                        ##
//## Set a RIB provider for a specified driver pipeline phase               ##
//##                                                                        ##
//############################################################################

HPROVIDER    AILCALL AIL_API_digital_driver_processor(HDIGDRIVER  dig, //)
                                                      DIGDRVSTAGE pipeline_stage)
{
   if ( dig == 0 )
     return( 0 );

   S32 stage = pipeline_stage;

   if (stage == DIGDRV_ALL_STAGES)
      {
      stage = 0;
      }

   if (!dig->pipeline[stage].active)
      {
      return 0;
      }

   return dig->pipeline[stage].provider;
}

//############################################################################
//##                                                                        ##
//## Set a RIB provider for a specified driver pipeline phase               ##
//##                                                                        ##
//############################################################################

HPROVIDER    AILCALL AIL_API_set_digital_driver_processor(HDIGDRIVER  dig, //)
                                                          DIGDRVSTAGE pipeline_stage,
                                                          HPROVIDER   provider)
{
   if ( dig == 0 )
     return( 0 );

   S32 stage = pipeline_stage;

   if (stage == DIGDRV_ALL_STAGES)
      {
      stage = 0;
      }

   DPINFO *DP;

   //
   // handle the "default" pipeline stages specially
   //

   if ((pipeline_stage==DP_DEFAULT_FILTER) || (pipeline_stage==DP_DEFAULT_MERGE))
      {
      HPROVIDER previous=dig->pipeline[pipeline_stage].provider;
      dig->pipeline[pipeline_stage].provider=provider;
      return(previous);
      }

   do
      {
      DP = &dig->pipeline[stage];

      //
      // If existing stage is being replaced, shut it down first
      //

      if (DP->active)
         {
         switch (stage)
            {
            //
            // "MSS mixer services"
            //

            case DP_FLUSH:
            case DP_COPY:
            case DP_MC_COPY:
            case DP_ADPCM_DECODE:
               break;
            }

         DP->active = 0;
         }
      }
   while ((pipeline_stage == DIGDRV_ALL_STAGES) && (++stage < N_DIGDRV_STAGES));

   //
   // If no new provider specified, exit normally after shutting down
   // existing provider
   //

   if (!provider)
      {
      return 0;
      }

   //
   // Set up to return previous provider
   //

   HPROVIDER previous = DP->provider;

   DP->provider = provider;

   //
   // Query required attributes from provider interface
   //

   switch (stage)
      {
      //
      // "MSS mixer services"
      //

      case DP_FLUSH:
         {
         //
         // Get codec properties and store in pipeline-stage data structure
         //

         RIB_INTERFACE_ENTRY MIXER[] =
            {
            { RIB_FUNCTION, "MIXER_flush", (UINTa) &DP->TYPE.MSS_mixer_flush, RIB_NONE },
            };

         RIB_request(DP->provider,"MSS mixer services",MIXER);

         //
         // Indicate installation of mixer module
         //

         DP->active = 1;
         break;
         }

      case DP_COPY:
         {
         //
         // Get codec properties and store in pipeline-stage data structure
         //

         RIB_INTERFACE_ENTRY MIXER[] =
            {
            { RIB_FUNCTION, "MIXER_copy",  (UINTa) &DP->TYPE.MSS_mixer_copy,  RIB_NONE }
            };

         RIB_request(DP->provider,"MSS mixer services",MIXER);

         //
         // Indicate installation of mixer module
         //

         DP->active = 1;
         break;
         }

      case DP_MC_COPY:
         {
         //
         // Get codec properties and store in pipeline-stage data structure
         //

         RIB_INTERFACE_ENTRY MIXER[] =
            {
            { RIB_FUNCTION, "MIXER_mc_copy",  (UINTa) &DP->TYPE.MSS_mixer_mc_copy,  RIB_NONE }
            };

         RIB_request(DP->provider,"MSS mixer services",MIXER);

         //
         // Indicate installation of mixer module
         //

         DP->active = 1;
         break;
         }

      case DP_ADPCM_DECODE:
         {
         //
         // Get codec properties and store in pipeline-stage data structure
         //

         RIB_INTERFACE_ENTRY MIXER[] =
            {
            { RIB_FUNCTION, "MIXER_adpcm_decode",  (UINTa) &DP->TYPE.MSS_mixer_adpcm_decode,  RIB_NONE }
            };

         RIB_request(DP->provider,"MSS mixer services",MIXER);

         //
         // Indicate installation of mixer module
         //

         DP->active = 1;
         break;
         }
      }

   //
   // Return success
   //

   return previous;
}

//############################################################################
//##                                                                        ##
//## Runtime profiling                                                      ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_digital_CPU_percent(HDIGDRIVER dig)
{
  U64 time;
  U64 diff;
  U32 perc;

  if (dig==0)
    return(0);

  time = rrGetTime();

  diff = time - dig->last_polled;

  if ( diff < rrMillisToTime( 150 ) )
    return(dig->last_percent);

  dig->last_polled = time;

  perc = (U32) ( ( dig->mix_total * 100 ) / diff );
  
  dig->mix_total = 0;
  
  if ( perc > 100 )
    perc = 100;
  
  dig->last_percent = perc;

  return perc;
}


#ifdef HOST_SPU_PROCESS
S32 AILCALL AIL_API_digital_SPU_percent(HDIGDRIVER dig)
{
  U64 time;
  U64 diff;
  U32 perc;

  if (dig==0)
    return(0);

  time = rrGetTime();

  diff = time - dig->spu_last_polled;

  if ( diff < rrMillisToTime( 150 ) )
    return( dig->spu_last_percent );

  dig->spu_last_polled = time;

  perc = (U32) ( ( dig->spu_total * 100 ) / diff );
  
  dig->spu_total = 0;
  
  if ( perc > 100 )
    perc = 100;
  
  dig->spu_last_percent = perc;

  return perc;
}
#endif

#define min(a,b)  (((a) < (b)) ? (a) : (b))

static S32 get_operation(U32 dest_rate,U32 dest_format,U32 src_rate,U32 src_bits, U32 src_chans)
{
  //
  // Set mixer operation code
  //

  S32 op = 0;

  if (dest_format & DIG_F_STEREO_MASK)
     {
     op |= M_DEST_STEREO;
     }

  if (src_chans==2)
     {
     op |= M_SRC_STEREO;
     }

  if (src_bits!=8)
     {
     op |= M_SRC_16;
     }

  //
  // Set M_SRC_SCALE
  //
  // Force M_SRC_SCALE if stereo source mixed into mono output
  //
  // (This routine assumes no volume scaling in use)
  //

  if ((op & M_SRC_STEREO) && (!(op & M_DEST_STEREO)))
     {
     op |= M_VOL_SCALING;
     }

  if (src_rate != dest_rate)
     {
     op |= M_RESAMPLE;
     }

  //
  // Enable filtering if preference set
  //

  if ((AIL_get_preference(DIG_ENABLE_RESAMPLE_FILTER)) && ((src_rate) != dest_rate))
     {
     op |= M_FILTER;
     }

  return(op);
}

//############################################################################
//##                                                                        ##
//## Return how much memory a conversion will require                       ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_size_processed_digital_audio(
                                 U32             dest_rate,
                                 U32             dest_format,
                                 S32             num_srcs,
                                 AILMIXINFO const* src)
{
   if ((src==0) || (num_srcs==0))
     return(0);

   // a point is one sample in mono, or two samples in stereo -
   // it is one sample point in time

   S32 i;
   S32 max_src_points=0;

   //calculate the sample points for each input
   for(i=0;i<num_srcs;i++) {
     S32 points=src[i].Info.data_len;
     if (src[i].Info.format==WAVE_FORMAT_IMA_ADPCM)
       points<<=1;    //2 adpcm samples in a byte
     else
       if (src[i].Info.bits!=8)
         points>>=1;  //2 bytes in a 16-bit sample
     if (src[i].Info.channels==2)
       points>>=1;    //2 samples in a stereo point

     // adjust number of points for sample rate differences
#if defined(IS_PS2) || defined(IS_PSP)
     points = (S32) mult64anddiv(points, dest_rate, src[i].Info.rate);
#else
     points=(S32)(((F64)points*(F64)dest_rate)/((F64)src[i].Info.rate));
#endif

     // keep track of the greatest number of points
     if (points>max_src_points)
       max_src_points=points;
   }

   // figure number of dest points
   S32 dest_point_size=((dest_format&DIG_F_STEREO_MASK)?2:1)*((dest_format&DIG_F_16BITS_MASK)?2:1);

   return( (dest_point_size*max_src_points)+256 );  // 256 for slop

}

//############################################################################
//##                                                                        ##
//## Ask MSS to reset the ASI at the end of the specified buffer            ##
//##                                                                        ##
//############################################################################

DXDEC  void     AILCALL AIL_request_EOB_ASI_reset   (HSAMPLE S,
                                                     U32     buff_num,
                                                     S32     seek_position)
{
  if (S)
  {
   if (buff_num == (U32)~0)  // ~0 = cancel any pending reset request (e.g., when the stream is restarted)
      {
      for (S32 i=0; i < S->n_buffers; i++)
         {
         S->buf[i].reset_ASI      = FALSE;
         S->buf[i].reset_seek_pos = 0;
         }
      }
   else
      {
      S->buf[buff_num].reset_ASI      = TRUE;
      S->buf[buff_num].reset_seek_pos = seek_position;
      }
  }
}


//############################################################################
//##                                                                        ##
//## Convert data from one sample rate and format to another                ##
//##                                                                        ##
//############################################################################

#define NUMBUILDSAMPLES 2048

S32 AILCALL AIL_API_process_digital_audio(
                                 void       *dest_buffer,
                                 S32             dest_buffer_size,
                                 U32             dest_rate,
                                 U32             dest_format,
                                 S32             num_srcs,
                                 AILMIXINFO* src)
{
   UINTa nbytes_written=0;

   if ((src==0) || (dest_buffer==0) || (num_srcs==0))
     return(0);

   S32 adpcm_size=0;
   S16*adpcm_temp_buffer=0;

   S32 dest_chan=((dest_format&DIG_F_STEREO_MASK)?2:1);

   //
   // Get driver mixer providers
   //

   MIXER_FLUSH MIXER_flush;
   MIXER_MERGE MIXER_merge;
   MIXER_COPY  MIXER_copy;
   MIXER_ADPCM_DECODE  MIXER_adpcm_decode;

   HPROVIDER HP;


   RIB_enumerate_providers("MSS mixer services",
                            NULL,
                           &HP);

   RIB_INTERFACE_ENTRY MIXER[] =
      {
      FN(MIXER_merge),
      FN(MIXER_flush),
      FN(MIXER_copy),
      FN(MIXER_adpcm_decode)
      };

   RIB_request(HP,"MSS mixer services",MIXER);


   S32 i;

   //calculate all of the operations
   S32 operations[256];
   for(i=0;i<num_srcs;i++)
     operations[i]=get_operation(dest_rate,dest_format,src[i].Info.rate,src[i].Info.bits,src[i].Info.channels);

  keepconverting:

   // a point is one sample in mono, or two samples in stereo -
   // it is one sample point in time

   S32 max_src_points=0;

   //calculate the sample points for each input
   for(i=0;i<num_srcs;i++) {
     S32 points=src[i].Info.data_len;
     if (src[i].Info.format==WAVE_FORMAT_IMA_ADPCM)
       points<<=1; //2 adpcm samples in a byte
     else
       if (src[i].Info.bits!=8)
         points>>=1;  //2 bytes in a 16-bit sample
     if (src[i].Info.channels==2)
       points>>=1;  //2 samples in a stereo point

     // adjust number of points for sample rate differences
#if defined(IS_PS2) || defined(IS_PSP)
     points = (S32) mult64anddiv(points, dest_rate, src[i].Info.rate);
#else
     points=(S32)(((F64)points*(F64)dest_rate)/((F64)src[i].Info.rate));
#endif

     // keep track of the greatest number of points
     if (points>max_src_points)
       max_src_points=points;
   }

   // figure number of dest points
   S32 dest_point_size=dest_chan*((dest_format&DIG_F_16BITS_MASK)?2:1);
   S32 dest_points=dest_buffer_size/dest_point_size;

   //limit our outputs to the maximum number of inputs
   if (max_src_points<dest_points)
     dest_points=max_src_points;

   //just exit if we don't have any mixing to do
   if (dest_points==0)
     return (S32) nbytes_written;


   // Init persistent variables for all passes
   for(i=0;i<num_srcs;i++) {
     if ((src[i].mss_adpcm.blocksize!=src[i].Info.block_size) || (src[i].Info.initial_ptr==src[i].Info.data_ptr)) {
       src[i].mss_adpcm.blocksize    = src[i].Info.block_size;
       src[i].mss_adpcm.blockleft    = 0;
       src[i].mss_adpcm.extrasamples = 0;
       src[i].src_fract=0;
       src[i].left_val=0;
       src[i].right_val=0;
     }
   }

   //ok, get down to the mixing
   U8* dest=(U8*)dest_buffer;

   S32 build_buffer[NUMBUILDSAMPLES];
   S32 build_points=NUMBUILDSAMPLES/dest_chan;

   while (dest_points) {

     //figure out how many points we're going to do this pass
     S32 points=build_points;
     if (points>dest_points)
       points=dest_points;

     S32 build_size=points*sizeof(S32)*dest_chan;

     //clear the output buffer
     MIXER_flush( build_buffer,
                  build_size
#ifdef IS_X86
                  ,(S32) AIL_get_preference(AIL_ENABLE_MMX_SUPPORT)
#endif
                  );

     SINTa max_built_size=0;

     //now loop through the srcs mixing in a piece of each src's buffer
     for(i=0;i<num_srcs;i++) {

       //do we have anything to mix?
       if (src[i].Info.data_len==0)
         continue;

       void const* mix_ptr;
       S32 mix_size;

       // if we're dealing with ADPCM, we have to decompress into a temp buffer first
       if (src[i].Info.format==WAVE_FORMAT_IMA_ADPCM) {

         void const* src_orig=src[i].Info.data_ptr;

#if defined(IS_PS2) || defined(IS_PSP)
         mix_size = 2 * (S32) mult64anddiv(points, src[i].Info.rate, dest_rate) * src[i].Info.channels;
#else
         mix_size=2*((S32)(((((F64)points)*((F64)src[i].Info.rate))/(F64)dest_rate)+0.5F))*src[i].Info.channels;
#endif

         //make sure the buffer is big enough
         if (mix_size>adpcm_size) {
           if (adpcm_temp_buffer)
             AIL_mem_free_lock(adpcm_temp_buffer);
           adpcm_size=mix_size;
           adpcm_temp_buffer=(S16*)AIL_mem_alloc_lock(adpcm_size);
           if (adpcm_temp_buffer==0)
             return(0);
         }

         S16* temp=adpcm_temp_buffer;

         MIXER_adpcm_decode(&temp, &src[i].Info.data_ptr, mix_size, src[i].Info.data_len,
                            operations[i], &src[i].mss_adpcm );

         U32 src_moved=(U32) AIL_ptr_dif(src[i].Info.data_ptr,src_orig);
         src[i].Info.data_len=(src[i].Info.data_len<=src_moved)?0:(src[i].Info.data_len-src_moved);

         mix_ptr=adpcm_temp_buffer;

         //don't worry about the fractional bit with adpcm
         src[i].src_fract=0;

       } else {

         mix_ptr=src[i].Info.data_ptr;
         mix_size=src[i].Info.data_len;

       }


       //
       // Call mixer provider
       //

       #ifdef IS_32

         void const* mix_orig=mix_ptr;
         S32* build=build_buffer;

         InMilesMutex();

         MIXER_merge(&mix_ptr,
                     &src[i].src_fract,
                      AIL_ptr_add( mix_ptr, mix_size),
                     &build,
                      AIL_ptr_add( build, build_size),
                     &src[i].left_val,
                     &src[i].right_val,
#if defined(IS_PS2) || defined(IS_PSP)
                      (S32) mult64anddiv(src[i].Info.rate, 65536, dest_rate),
#else
                      (S32)((((F64) src[i].Info.rate) * 65536.0F) / (F64) dest_rate),
#endif
                      2048,
                      2048,
                      operations[i]
#ifdef IS_X86
                      ,(S32) AIL_get_preference(AIL_ENABLE_MMX_SUPPORT)
#endif
                      );

         OutMilesMutex();

         S32 built_size=(S32) AIL_ptr_dif(build,build_buffer);
         U32 src_moved=(U32) AIL_ptr_dif(mix_ptr,mix_orig);

       #else

         U32 src_offset  = MSSLOWORD(mix_ptr);
         U32 dest_offset = MSSLOWORD(build_buffer);

         MIXER_merge(MSSHIWORD(mix_ptr),
                     MSSHIWORD(build_buffer),
                    &src[i].src_fract,
                    &src_offset,
                    &dest_offset,
                     src_offset+mix_size,
                     dest_offset+build_size,
                    &src[i].left_val,
                    &src[i].right_val,
#if defined(IS_PS2) || defined(IS_PSP)
                     (S32) mult64anddiv(src[i].Info.rate, 65536, dest_rate),
#else
                     (S32)((((F64) src[i].Info.rate) * 65536.0F) / (F64) dest_rate),
#endif
                     (2048 << 16) | 2048,
                     operations[i]);

         U32 src_moved=(src_offset - MSSLOWORD(mix_ptr));
         S32 built_size=(dest_offset - MSSLOWORD(build_buffer));

       #endif

       // don't adjust the pointers for adpcm (already done)
       if (!(src[i].Info.format==WAVE_FORMAT_IMA_ADPCM)) {
         src[i].Info.data_ptr = AIL_ptr_add(src[i].Info.data_ptr, src_moved);
         src[i].Info.data_len=(src[i].Info.data_len<=src_moved)?0:(src[i].Info.data_len-src_moved);
       }

       if (built_size>max_built_size)
         max_built_size=built_size;

     }

     if (max_built_size==0)
       break;

     //
     // Copy from the build buffer into dest
     //
     // We can't use the RIB provider on PS2, because of the
     // 256-sample left/right interleaved funkiness
     //

#ifdef IS_PS2
     PS2_noninterleaved_mixer_copy(build_buffer,
                                   max_built_size,
                                   dest,
                                   dest_format);
#else
     MIXER_copy( build_buffer,
           (S32) max_built_size,
                 dest,
                 dest_format
#ifdef IS_X86
                 ,(S32) AIL_get_preference(AIL_ENABLE_MMX_SUPPORT)
#else
#ifdef IS_BE
                 ,0
#endif
#endif
                 );
#endif

     dest=(U8*)AIL_ptr_add(dest,points*dest_point_size);
     dest_points-=points;
   }

   SINTa written=(SINTa) AIL_ptr_dif(dest,dest_buffer);
   nbytes_written+=written;
   dest_buffer=dest;
   dest_buffer_size-=(S32) written;

   if (dest_buffer_size)
     goto keepconverting;

   if (adpcm_temp_buffer)
     AIL_mem_free_lock(adpcm_temp_buffer);

   return (S32) nbytes_written;
}

//
// ASCII alternate redist directory
//

char AIL_redist_directory[260]="";

//############################################################################
//##                                                                        ##
//## Set the alternate MSS driver directory                                 ##
//##                                                                        ##
//############################################################################

extern "C"
char* AILEXPORT  AIL_set_redist_directory(char const* dir)
{
   U32 len;

   if ( ( dir == NULL ) || ( ( dir[ 0 ] == '.' ) && ( dir[ 1 ] == 0 ) ) )
      *AIL_redist_directory=0;
   else
      {
#if defined(IS_MAC) && !defined(IS_MACHO)
      AIL_strcpy(AIL_redist_directory,dir);

      len=AIL_strlen(AIL_redist_directory);
      if (len)
      {
        if (AIL_redist_directory[len-1]!=':')
        {
          AIL_redist_directory[len]=':';
          AIL_redist_directory[len+1]=0;
        }
      }
#else
      AIL_strcpy(AIL_redist_directory,dir);
      len=(S32) AIL_strlen(AIL_redist_directory);
      if (len)
         {
         if ((AIL_redist_directory[len-1]!='/') && (AIL_redist_directory[len-1]!='\\') && (AIL_redist_directory[len-1]!=':'))
            {
            AIL_redist_directory[len]='/';
            AIL_redist_directory[len+1]=0;
            }
         }
#endif
      }

   return (char*) (AIL_redist_directory);
}


//############################################################################
//##                                                                        ##
//## Return number of actively playing samples for given driver             ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_active_sample_count( HDIGDRIVER dig )
{

  if ( dig == 0 )
  {
    return( 0 );
  }

  // n_active_samples adjusts for playing busses, which iterating over the sample list does not...

  return dig->n_active_samples;

}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void AILCALL AIL_API_pop_system_state(HDIGDRIVER dig, S16 crossfade_ms)
{
    if (dig->state_index == 0)
        return; // invalid

    if (crossfade_ms < 0)
        crossfade_ms = 0;

    char* state_buffer = (char*)dig->pushed_states[dig->state_index - 1];

    U32 flags = *(U32*)state_buffer;
    state_buffer += 4;

    U32 bus_count = *(U32*)state_buffer;
    state_buffer += 4;

    U32 num_mixes = *(U32*)state_buffer;
    state_buffer += 4;

    //
    // First order of business - stop all of the samples that were playing.
    // At the same time, update the timing of any scheduled samples.
    //
    {
        U32 current_num_mixes = dig->num_mixes;

        if (flags & MILES_PUSH_RESET)
        {
            dig->num_mixes = num_mixes;

            for (S32 i = 0; i < MAX_BUSSES; i++)
            {
                if (dig->reverb[i].reverb_build_buffer)
                {
                    dig->reverb[i].reverb_off_time_ms += num_mixes - current_num_mixes;
                }
            }
        }

        S64 adj = (num_mixes - current_num_mixes) * dig->hardware_buffer_size / (2 * dig->physical_channels_per_sample);

        S32 n;
        U32* status;
        HSAMPLE S;
        for (n = dig->n_samples,S = &dig->samples[0],status = &dig->sample_status[0]; n; --n,++S,++status)
        {
            if (*status == SMP_FREE)
                continue;

            if (S->bus == MAX_BUSSES)
                continue;

            S32 PlayingFrom = (S->state_level & (1 << dig->state_index));
            S32 PlayingTo = (S->state_level & (1 << (dig->state_index - 1)));

            if (!PlayingFrom && PlayingTo)
            {
                // We're about to resume play - fade in.
                S->pop_fade_time = crossfade_ms;
                S->pop_fade_total = -crossfade_ms;

                // Remove the from so we don't restart on next push.
                S->state_level &= (~(1 << dig->state_index));

                SS_update_sample_reverb_state(S);

                // Otherwise, we may need to update the scheduling.
                if (S->mix_start_time == 0)
                    continue;

                if ((flags & MILES_PUSH_RESET) == 0)
                    continue;

                if (adj < 0 && S->mix_start_time > (U32)-adj)
                    S->mix_start_time = 1;
                else
                    S->mix_start_time += adj;
            }
            else if (PlayingFrom && !PlayingTo)
            {
                // We're stopping - fade out if we are playing.
                if (*status == SMP_PLAYING)
                {
                    S->pop_fade_total = crossfade_ms;
                    S->pop_fade_time = crossfade_ms;
                    S->pop_fade_stop = 1; // mark that we end when complete.
                }
                else
                {
                    if (S->system_data[STR_HSTREAM])
                    {
                        // need to end the stream.
                        HSTREAM stream = (HSTREAM)S->system_data[STR_HSTREAM];
                        AIL_pause_stream(stream, 1);
                        stream->was_popped = 1;
                    }
                    else
                        AIL_end_sample(S);
                }
            }
        }
    }


    // Count the new # of buffers
    U32 allocated_bus_count = 0;
    for (; allocated_bus_count < MAX_BUSSES; allocated_bus_count++)
        if (dig->bus_samples[allocated_bus_count] == 0)
            break;

    if (allocated_bus_count == 0)
        allocated_bus_count = 1;


    // Restore everything we changed.
    if (flags & MILES_PUSH_REVERB)
    {
        for (U32 i = 0; i < bus_count; i++)
        {
            // struct always reverts.
            memcpy(dig->reverb + i, state_buffer, sizeof(REVERB_SETTINGS));
            state_buffer += sizeof(REVERB_SETTINGS);

            // If the bus still exists, reset the build buffer
            if (i < allocated_bus_count)
                memcpy(dig->reverb[i].reverb_build_buffer, state_buffer, dig->reverb[i].reverb_buffer_size);

            state_buffer += dig->reverb[i].reverb_buffer_size;
        }
    }

    if (flags & MILES_PUSH_3D)
    {
        memcpy(&dig->D3D, state_buffer, sizeof(D3DSTATE));
        state_buffer += sizeof(D3DSTATE);
    }

    if (flags & MILES_PUSH_VOLUME)
    {
        memcpy(&dig->master_volume, state_buffer, 4);
        state_buffer += 4;
    }

    AIL_mem_free_lock(dig->pushed_states[dig->state_index - 1]);
    dig->state_index --;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void AILCALL AIL_API_push_system_state(HDIGDRIVER dig, U32 flags, S16 crossfade_ms)
{
    // Check if we have room to push.
    if (dig->pushed_states[MILES_MAX_STATES - 1])
        return;

    if (crossfade_ms < 0)
        crossfade_ms = 0;

    // Determine the size needed to save the state
    U32 save_size = 12;

    U32 allocated_bus_count = 0;
    for (; allocated_bus_count < MAX_BUSSES; allocated_bus_count++)
        if (dig->bus_samples[allocated_bus_count] == 0)
            break;

    if (allocated_bus_count == 0)
        allocated_bus_count = 1;

    if (flags & MILES_PUSH_REVERB)
        save_size += allocated_bus_count*(sizeof(REVERB_SETTINGS) + dig->reverb[0].reverb_buffer_size);
    if (flags & MILES_PUSH_VOLUME)
        save_size += 4;
    if (flags & MILES_PUSH_3D)
        save_size += sizeof(D3DSTATE);

    
    void* state_buffer = AIL_mem_alloc_lock(save_size);

    char* buffer = (char*)state_buffer;

    memcpy(buffer, &flags, 4);
    buffer += 4;

    memcpy(buffer, &allocated_bus_count, 4);
    buffer += 4;

    memcpy(buffer, &dig->num_mixes, 4);
    buffer += 4;

    if (flags & MILES_PUSH_RESET)
    {
        U32 prev_mixes = dig->num_mixes;
        dig->num_mixes = 0;

        for (S32 i = 0; i < MAX_BUSSES; i++)
        {
            if (dig->reverb[i].reverb_build_buffer)
            {
                dig->reverb[i].reverb_off_time_ms -= prev_mixes;
            }
        }

        U64 adj = (prev_mixes) * dig->hardware_buffer_size / (2 * dig->physical_channels_per_sample);

        // If any samples are scheduled to play, we need to reset their schedule
        S32 n;
        U32* status;
        HSAMPLE S;
        for (n = dig->n_samples,S = &dig->samples[0],status = &dig->sample_status[0]; n; --n,++S,++status)
        {
            if (*status == SMP_FREE)
                continue;

            // If we'll be active in the next level
            if (S->state_level & (1 << (dig->state_index + 1)))
            {
                // add the mix timer difference if we are a pending play.
                if (*status == SMP_DONE && S->mix_start_time)
                    S->mix_start_time -= adj;
            }
            else
            {
                // If we are currently playing butwill stop, fade out.
                if (*status == SMP_PLAYING)
                {
                    S->pop_fade_total = crossfade_ms;
                    S->pop_fade_time = crossfade_ms;
                }
            }
        }
    }

    if (flags & MILES_PUSH_REVERB)
    {
        for (U32 i = 0; i < allocated_bus_count; i++)
        {
            memcpy(buffer, dig->reverb + i, sizeof(REVERB_SETTINGS));
            buffer += sizeof(REVERB_SETTINGS);

            memcpy(buffer, dig->reverb[i].reverb_build_buffer, dig->reverb[0].reverb_buffer_size);

            if (flags & MILES_PUSH_RESET)
            {
                memset(dig->reverb[i].reverb_build_buffer, 0, dig->reverb[0].reverb_buffer_size);
                dig->reverb[i].ri.u.X0 = 0;
                dig->reverb[i].ri.u.X1 = 0;
                dig->reverb[i].ri.u.Y0 = 0;
                dig->reverb[i].ri.u.Y1 = 0;
                dig->reverb[i].ri.c.A = 0;
                dig->reverb[i].ri.c.B0 = 0;
                dig->reverb[i].ri.c.B1 = 0;
                dig->reverb[i].ri.c.C0 = 0;
                dig->reverb[i].ri.c.C1 = 0;
                dig->reverb[i].ri.c.C2 = 0;
                dig->reverb[i].ri.c.C3 = 0;
                dig->reverb[i].ri.c.C4 = 0;
                dig->reverb[i].ri.c.C5 = 0;
            }

            buffer += dig->reverb[i].reverb_buffer_size;
        }
    }

    if (flags & MILES_PUSH_3D)
    {
        memcpy(buffer, &dig->D3D, sizeof(D3DSTATE));
        buffer += sizeof(D3DSTATE);

        if (flags & MILES_PUSH_RESET)
        {
            dig->D3D.mute_at_max = (S32) AIL_preference[DIG_3D_MUTE_AT_MAX];

            dig->D3D.rolloff_factor  = 1.0F;
            dig->D3D.doppler_factor  = 1.0F;
            dig->D3D.distance_factor = 1.0F;

            dig->D3D.listen_position.x = 0.0F;
            dig->D3D.listen_position.y = 0.0F;
            dig->D3D.listen_position.z = 0.0F;

            dig->D3D.listen_face.x = 0.0F;
            dig->D3D.listen_face.y = 0.0F;
            dig->D3D.listen_face.z = 1.0F;

            dig->D3D.listen_up.x = 0.0F;
            dig->D3D.listen_up.y = 1.0F;
            dig->D3D.listen_up.z = 0.0F;

            dig->D3D.listen_cross.x = 1.0F;
            dig->D3D.listen_cross.y = 0.0F;
            dig->D3D.listen_cross.z = 0.0F;

            dig->D3D.listen_velocity.x = 0.0F;
            dig->D3D.listen_velocity.y = 0.0F;
            dig->D3D.listen_velocity.z = 0.0F;
        }
    }

    if (flags & MILES_PUSH_VOLUME)
    {
        memcpy(buffer, &dig->master_volume, 4);
        buffer += 4;

        if (flags & MILES_PUSH_RESET)
            dig->master_volume = 1.0f;
    }

    // If we aren't resetting, then we need to mark the samples currently playing as also in the next level.
    if ((flags & MILES_PUSH_RESET) == 0)
    {
        S32 n;
        U32* status;
        HSAMPLE S;
        for (n = dig->n_samples,S = &dig->samples[0],status = &dig->sample_status[0]; n; --n,++S,++status)
        {
            if (*status == SMP_FREE || *status == SMP_DONE)
                continue;

            if (S->state_level & (1 << (dig->state_index))) // if we are currently active.
                S->state_level |= (1 << (dig->state_index + 1)); // add us to the next level.
        }
    }

    dig->pushed_states[dig->state_index] = state_buffer;
    dig->state_index++;
}

//############################################################################
//##                                                                        ##
//## Enable/Disable a driver limiter                                        ##
//##                                                                        ##
//############################################################################
DXDEF void AILEXPORT AIL_enable_limiter(HDIGDRIVER dig, S32 on_off)
{
    AIL_lock();
    dig->enable_limiter = !!on_off;
    AIL_unlock();
}


//############################################################################
//##                                                                        ##
//## Return the HSAMPLE handle for a given bus id                           ##
//##                                                                        ##
//############################################################################

HSAMPLE AILCALL AIL_API_bus_sample_handle(HDIGDRIVER dig, S32 bus_index)
{
    return dig->bus_samples[bus_index];
}

//############################################################################
//##                                                                        ##
//## Install a side chain compressor on a bus                               ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_install_bus_compressor(HDIGDRIVER dig, S32 bus_index, SAMPLESTAGE filter_stage, S32 input_bus_index)
{
    // Both indeces have to have a bus installed.
    if (dig->bus_samples[bus_index] == 0 ||
        dig->bus_samples[input_bus_index] == 0)
    {
        AIL_set_error("Both indices must refer to installed busses.");
        return 0;
    }

    HPROVIDER P = 0;
    if (AIL_find_filter("Compressor Filter", &P) == 0)
    {
        AIL_set_error("Compressor Filter Not Found");
        return 0;
    }

    AIL_set_sample_processor(dig->bus_samples[bus_index], filter_stage, P);

    // The property token for the compressor input is 3, in sidecompress.cpp
    dig->bus_samples[bus_index]->bus_comp_installed = (filter_stage << 16) | 3;
    dig->bus_samples[input_bus_index]->bus_comp_sends |= (1 << bus_index);    

    return 1;
}

//############################################################################
//##                                                                        ##
//## Free all busses allocated                                              ##
//##                                                                        ##
//############################################################################
DXDEF void AILCALL AIL_free_all_busses(HDIGDRIVER dig)
{
    for (S32 i = 0; i < MAX_BUSSES; i++)
    {
        if (dig->bus_samples[i])
            AIL_release_sample_handle(dig->bus_samples[i]);
        if (dig->bus_ptrs[i])
            AIL_mem_free_lock(dig->bus_ptrs[i]);
        if (i && dig->reverb[i].reverb_build_buffer)
            AIL_mem_free_lock(dig->reverb[i].reverb_build_buffer);

        dig->bus_samples[i] = 0;
        dig->bus_ptrs[i] = 0;

        if (i)
            dig->reverb[i].reverb_build_buffer = 0;
    }
}

//############################################################################
//##                                                                        ##
//## Enable/Disable a bus limiter                                           ##
//##                                                                        ##
//############################################################################
DXDEF void AILEXPORT AIL_bus_enable_limiter(HDIGDRIVER dig, S32 bus_index, S32 on_off)
{
    AIL_lock();
    if (dig->bus_samples[bus_index])
        dig->bus_samples[bus_index]->bus_enable_limiter = !!on_off;
    AIL_unlock();
}

//############################################################################
//##                                                                        ##
//## Allocate a bus for grouping of HSAMPLEs                                ##
//##                                                                        ##
//############################################################################

HSAMPLE AILCALL AIL_API_allocate_bus(HDIGDRIVER dig)
{
    // Find a free bus.
    S32 index = 0;
    for (; index < MAX_BUSSES; index++)
        if (dig->bus_samples[index] == 0)
            break;

    if (index == MAX_BUSSES)
    {
        AIL_set_error("Out of available busses.");
        return 0;
    }

    // Allocate the handle and set it to match the driver exactly.
    HSAMPLE bus = AIL_allocate_sample_handle(dig);
    if (bus == 0)
        return 0;

    // Can't use the dig's hw_format, as for srs its stereo instead of mc.
    S32 fmt = DIG_F_16BITS_MASK;
    if (dig->logical_channels_per_sample > 2)
        fmt |= (dig->logical_channels_per_sample << 16) | DIG_F_MULTICHANNEL_MASK;
    else if (dig->logical_channels_per_sample == 2)
        fmt |= DIG_F_STEREO_MASK;

    if (AIL_init_sample(bus, fmt) == 0)
    {
        AIL_release_sample_handle(bus);
        return 0;
    }

    AIL_set_sample_playback_rate(bus, dig->DMA_rate);
    dig->bus_samples[index] = bus;

    // Mark that this sample is a bus sample.
    bus->bus = MAX_BUSSES;

    // Allocate a buffer that is big enough to handle one mix.
    void* buffer = AIL_mem_alloc_lock(dig->samples_per_buffer * bus->n_channels * 2);
    if (buffer == 0)
    {
        AIL_set_error("Out of memory for bus buffer.");
        AIL_release_sample_handle(bus);
        dig->bus_samples[index] = 0;
        return 0;
    }

    dig->bus_ptrs[index] = buffer;

    if (index)
    {
        dig->reverb[index].master_dry = 1.0f;
        dig->reverb[index].master_wet = 1.0f;
        dig->reverb[index].room_type = 0;

        // We need to allocate the reverb buffers for this bus.
        if (AIL_allocate_reverb_buffers(dig, index) == 0)
        {
            AIL_mem_free_lock(buffer);
            AIL_release_sample_handle(bus);

            dig->bus_ptrs[index] = 0;
            dig->bus_samples[index] = 0;
            return 0;
        }

    }

    if (index == 0)
    {
        // This is the first bus to be created, so we actually need to make two and return the second one,
        // otherwise nothing happens.
        bus = AIL_API_allocate_bus(dig);
        if (bus == 0)
        {
            AIL_release_sample_handle(dig->bus_samples[0]);
            AIL_mem_free_lock(dig->bus_ptrs[0]);
            dig->bus_samples[0] = 0;
            dig->bus_ptrs[0] = 0;

            return 0;
        }
    }

    return bus;
}

//############################################################################
//##                                                                        ##
//## Allocate a SAMPLE structure for use with a given driver                ##
//##                                                                        ##
//############################################################################

HSAMPLE AILCALL AIL_API_allocate_sample_handle( HDIGDRIVER dig )
{
  S32    i;
  HSAMPLE S;

  //
  // Exit if driver invalid
  //

  if ( dig == NULL )
  {
    return( 0 );
  }

  //
  // Lock timer services to prevent reentry
  //

  InMilesMutex();

  //
  // Look for an unallocated sample structure
  //

  for ( i = 0 ; i < dig->n_samples ; i++ )
  {
    if ( dig->sample_status[ i ] == SMP_FREE )
      break;
  }

  //
  // If all structures in use, return NULL
  //

  if ( i == dig->n_samples )
  {
    AIL_set_error( "Out of sample handles." );

    OutMilesMutex();
    return( 0 );
  }

  S = &dig->samples[ i ];

  //
  // Initialize sample to default (SMP_DONE) status with nominal
  // sample attributes
  //

  if (!AIL_init_sample( S, DIG_F_MONO_16 ))
   {
   return 0;
   }

  //
  // If a voice filter is installed on this driver, call it to assign a voice to the
  // HSAMPLE
  //
  // Failure means the sample handle is re-released
  //

#ifdef MSS_VFLT_SUPPORTED
  if (dig->voice_filter != NULL)
    {
    if (!dig->voice_filter->assign_sample_voice(dig->voice_filter->driver_state,
                                                S))
      {
      S->driver->sample_status[S->index] = SMP_FREE;
      AIL_set_error( "Hardware voice assignment failed." );

      OutMilesMutex();
      return 0;
      }

    if (AIL_set_sample_processor(S,
                                 SP_OUTPUT,
                                 dig->voice_filter->provider) == (HPROVIDER) -1)
      {
      return 0;
      }
    }
#endif

  OutMilesMutex();
  return( S );
}

//############################################################################
//##                                                                        ##
//## Free a SAMPLE structure for later allocation                           ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_release_sample_handle( HSAMPLE S )
{
  if ( S == NULL )
  {
    return;
  }

  hardware_stop( S );

  S->driver->sample_status[S->index] = SMP_FREE;

  //
  // Shut down any installed pipeline providers
  //

#ifdef MSS_VFLT_SUPPORTED
  if (S->driver->voice_filter != NULL)
    {
    S->driver->voice_filter->release_sample_voice(S->driver->voice_filter->driver_state,
                                                  S);
    }
#endif

  AIL_set_sample_processor( S,
                            SAMPLE_ALL_STAGES,
                            0 );

  hardware_spin_until_stopped( S );

/*  if (S->buf != NULL)
    {
    AIL_mem_free_lock(S->buf);
    S->buf = NULL;
    }
*/
  free_channel_buffers(S);
}

//############################################################################
//##                                                                        ##
//## Initialize a SAMPLE structure to baseline values                       ##
//##                                                                        ##
//## Sample is allocated (not free), done playing, and stopped              ##
//##                                                                        ##
//## Beginning with MSS V7, sample source data format must be specified     ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_init_sample( HSAMPLE S, S32 format )
{
  if ( S == NULL )
  {
    return 0;
  }

  hardware_stop( S );

  //
  // Set status to FREE while manipulating vars, to keep callback thread
  // from reading invalid data
  //

  S->driver->sample_status[S->index] = SMP_FREE;

  //
  // Shut down any previously-active pipeline providers
  //   and init pipeline stages
  //

  AIL_set_sample_processor( S,
                            SAMPLE_ALL_STAGES,
                            0 );

  //
  // Revert to 2D-only control
  //

  S->is_3D = FALSE;
  init_sample_3D_state(S);

  //
  // Allocate buffers for static and streaming sources
  //

  AIL_set_sample_buffer_count(S, 2);

  //
  // Initialize sample vars
  //

  S->loop_count      =  1;
  S->loop_start      =  0;
  S->loop_end        = -1;
  S->orig_loop_count =  1;
  S->orig_loop_start =  0;
  S->orig_loop_end   = -1;

  S->original_playback_rate  =  11025;
  S->playback_rate_factor    =  1.0f;

  S->pop_fade_time = 0;
  S->pop_fade_stop = 0;
  S->pop_fade_total = 0;

  S->n_channels = 0; // Force allocation of channel arrays
  if (!SS_set_sample_type(S, format, ~0U)) return 0;

  clear_last_sample(S);

  //
  // Mark as single-buffered sample
  //

  S->service_type = SERVICE_SINGLE_BUFFER;

  // 100% volume to each speaker
  AIL_API_set_sample_volume_levels( S, 1.0f, 1.0f );

  for (S32 i = 0; i < MAX_SPEAKERS; i++)
    S->low_pass_cutoff[i] = 1.0f;

  S->sys_level     = 1.0F;
  S->dry_level     = 1.0F;
  S->wet_level     = 0.0F;
  S->exclusion     = 0.0F;
  S->obstruction   = 0.0F;
  S->occlusion     = 0.0F;
  S->extra_volume  = 1.0F;
  S->extra_wet     = 1.0F;
  S->extra_rate    = 1.0F;
  S->extra_lp      = 96000;

  S->bus           = 0;
  S->bus_override_wet = 0;
  S->bus_enable_limiter = 0;
#ifdef IS_WIIU
  S->route_to_drc  = 0;
#endif

  S->lp_any_on = 0;

  // Set default ADPCM data
  S->adpcm.blocksize    = 256;
  S->adpcm.blockleft    = 0;
  S->adpcm.extrasamples = 0;

  S->mix_bytes = 0;

  S->SOB =  NULL;
  S->EOB =  NULL;
  S->EOS =  NULL;
  S->dosob = 0;
  S->doeob = 0;
  S->doeos = 0;

  S->vol_ramps = (U32) AIL_preference[DIG_LEVEL_RAMP_SAMPLES];
  S->resamp_tolerance = (U32) AIL_preference[DIG_RESAMPLING_TOLERANCE];
  S->enable_resamp_filter = (U32) AIL_preference[ DIG_ENABLE_RESAMPLE_FILTER ];
  S->mix_delay = 0;

  //
  // Init pipeline stages
  //

  S32 i;
  for (i = 0 ; i < N_SAMPLE_STAGES ; i++ )
  {
    S->pipeline[ i ].active = 0;
  }

  S->n_active_filters = 0;

  //
  // Select default mixer merge provider
  //

  if (AIL_set_sample_processor( S,
                                SP_MERGE,
                                S->driver->pipeline[ DP_DEFAULT_MERGE ].provider ) == (HPROVIDER) -1)
    {
    return 0;
    }

  if (AIL_set_sample_processor( S,
                                SP_FILTER_0,
                                S->driver->pipeline[ DP_DEFAULT_FILTER ].provider ) == (HPROVIDER) -1)
    {
    return 0;
    }

  S->user_channels_need_deinterlace = 0;

  //
  // Mark sample initialized
  //

  S->driver->sample_status[S->index] =  SMP_DONE;

#if defined(HOST_SPU_PROCESS) || defined(MSS_SPU_PROCESS)
   S->spu_on = 1; // start with spu on (turn off with incompatible features as they are set)
#endif

  return 1;
}


S32 AILCALL AIL_API_set_sample_is_3D        (HSAMPLE                S, //)
                                             S32                    onoff)
{
  S32 old;

  if ( S == NULL )
    return 0;

  old = S->is_3D;

  S->is_3D = onoff;

  return old;
}


//############################################################################
//##                                                                        ##
//## Get status of sample                                                   ##
//##                                                                        ##
//############################################################################

U32 AILCALL AIL_API_sample_status( HSAMPLE S )
{
  if ( S == NULL )
  {
    return( 0 );
  }

  return( S->driver->sample_status[S->index] );
}

//############################################################################
//##                                                                        ##
//## Set adpcm sample block size                                            ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_adpcm_block_size( HSAMPLE S, U32 blocksize )
{
  if ( S )
  {
    S->adpcm.blocksize = blocksize;
  }
}

//############################################################################
//##                                                                        ##
//## Set starting address and length of sample                              ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_address( HSAMPLE S, void const *start, U32 len )
{
  if ( S == NULL )
  {
    return;
  }

  S->buf[0].start = start;
  S->buf[0].len   = len;

  for (S32 i=1; i < S->n_buffers; i++)
   {
   S->buf[i].start = NULL;
   S->buf[i].len   = 0;
   }

  //
  // Mark as single-buffered sample
  //

  S->service_type = SERVICE_SINGLE_BUFFER;
}

//############################################################################
//##                                                                        ##
//## Get the amount of time the sample has output to the mixer              ##
//##                                                                        ##
//############################################################################

U32 AILCALL AIL_sample_mixed_ms( HSAMPLE S )
{
  if ( S == NULL)
  {
    return 0;
  }
    
  AIL_lock();
  U64 result = S->mix_bytes * 1000;
  result /= S->driver->DMA_rate*2*S->driver->physical_channels_per_sample;
  AIL_unlock();
    
  return (U32)result;
}

//############################################################################
//##                                                                        ##
//## Get sample playback rate in hertz                                      ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_sample_playback_rate( HSAMPLE S )
{
  if ( S == NULL)
  {
    return 0;
  }

  return( S->original_playback_rate );
}

//############################################################################
//##                                                                        ##
//## Set sample playback rate in hertz                                      ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_playback_rate( HSAMPLE S, S32 playback_rate )
{
  if ( S )
  {
    if ( playback_rate > 0 )
    {
        if (playback_rate != S->original_playback_rate)
            S->low_pass_changed = ~0U;

      S->original_playback_rate = playback_rate;
    }
  }
}

//############################################################################
//##                                                                        ##
//## Get sample playback rate scale factor                                  ##
//##                                                                        ##
//############################################################################

F32 AILCALL AIL_API_sample_playback_rate_factor( HSAMPLE S )
{
  if ( S == NULL)
  {
    return 0;
  }

  return( S->playback_rate_factor );
}

//############################################################################
//##                                                                        ##
//## Set sample playback rate scale factor                                  ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_playback_rate_factor( HSAMPLE S, F32 playback_rate_factor )
{
  if ( S )
  {
    if ( playback_rate_factor > 0 )
    {
      if (fabs(playback_rate_factor - S->playback_rate_factor) > .0001f)
        S->low_pass_changed = ~0U;

      S->playback_rate_factor = playback_rate_factor;
    }
  }
}

//############################################################################
//##                                                                        ##
//## Get sample delay amount left                                           ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_sample_playback_delay( HSAMPLE S )
{
  if ( S == NULL)
  {
    return 0;
  }

  return( S->mix_delay / 256 );
}

//############################################################################
//##                                                                        ##
//## Set sample playback delay amount (in ms)                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_playback_delay( HSAMPLE S, S32 playback_delay )
{
  if ( S )
  {
    if ( playback_delay > 0 )
    {
      S->mix_delay = 256 * playback_delay;
    }
  }
}

//############################################################################
//##                                                                        ##
//## Get sample volume levels                                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_sample_volume_levels( HSAMPLE S, F32 * left_level, F32 * right_level )
{
  if (S == NULL)
  {
    return;
  }

  if ( left_level )
  {
    *left_level = S->left_volume;
  }
  if ( right_level )
  {
    *right_level = S->right_volume;
  }
}

//############################################################################
//##                                                                        ##
//## Set sample volume levels                                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_volume_levels( HSAMPLE S, F32 left_level, F32 right_level )
{
  if ( S == NULL )
  {
    return;
  }

  S->left_volume = left_level;      // These are already linear scalars
  S->right_volume = right_level;

  // build some reasonable values for reporting
  if ( left_level > 0.0001f )
  {
    F32 ratio = (F32) AIL_pow( right_level / left_level, 10.0f / 3.0f );
    S->save_pan = ratio / ( ratio + 1 );
    if ( S->save_pan < 0.0001f )
      S->save_volume = left_level;
    else
      S->save_volume = right_level * (F32) AIL_pow( S->save_pan, -0.3f );
  }
  else
  {
    if ( right_level > 0.0001f )
    {
      S->save_pan = 1.0f;
      S->save_volume = right_level;
    }
    else
    {
      S->save_pan = 0.5f;
      S->save_volume = 0.0f;
    }
  }

  S->leftb_volume = S->left_volume;
  S->rightb_volume = S->right_volume;
  S->center_volume = S->save_volume;
  S->low_volume = S->save_volume;
  S->save_fb_pan = 0.5f;

  set_hardware_volume( S );
}

//############################################################################
//##                                                                        ##
//## Get sample reverb levels                                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_sample_reverb_levels(HSAMPLE S,
                                          F32    *dry_level,
                                          F32    *wet_level)
{
   if ( S == NULL )
      {
      if (dry_level)
         {
         *dry_level = 0.0F;
         }

      if (wet_level)
         {
         *wet_level = 0.0F;
         }

      return;
      }

   if (dry_level)
      {
      *dry_level = S->dry_level;
      }

   if (wet_level)
      {
      *wet_level = S->wet_level;
      }
}

//############################################################################
//##                                                                        ##
//## Set sample reverb levels                                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_reverb_levels(HSAMPLE S,
                                              F32     dry_level,
                                              F32     wet_level)
{
   if ( S == NULL )
      {
      return;
      }

   S->dry_level = dry_level;
   S->wet_level = wet_level;

   // If we are a bus sample, then we mark ourselves as overriding child wet levels.
   S->bus_override_wet = 1;

   if (S->driver->sample_status[S->index] == SMP_PLAYING)
      {
      SS_update_sample_reverb_state(S);
      }

   set_hardware_volume(S);
}


//############################################################################
//##                                                                        ##
//## Set sample volume and panpot / stereo balance (0=left, 1=right)        ##
//##                                                                        ##
//## Value determines panning position for mono samples, and balance for    ##
//## stereo samples                                                         ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_volume_pan( HSAMPLE S, F32 volume, F32 pan )
{
  if ( S == NULL )
  {
    return;
  }

  S->save_volume = (F32) AIL_pow( volume, 10.0f / 6.0f ); // turns 0.5F into linear DAC scalar for -10 dB, .25F into -20 dB...
  S->save_pan = pan;

  if ( pan == 0.5f )
  {
    S->left_volume = S->save_volume * 0.812252196f;
    S->right_volume = S->save_volume * 0.812252196f;
  }
  else
  {
    S->left_volume = S->save_volume * (F32) AIL_pow( 1.0f - pan, 0.3f );
    S->right_volume = S->save_volume * (F32) AIL_pow( pan, 0.3f );
  }

  if (S->driver->logical_channels_per_sample >= 4)
  {
    // 0.812252196f is the front/back perfectly balance pan (formerly applied only on Xbox and Xenon)
    S->left_volume = S->left_volume * 0.812252196f;
    S->right_volume = S->right_volume * 0.812252196f;
  }

  S->leftb_volume = S->left_volume;
  S->rightb_volume = S->right_volume;
  S->center_volume = ( S->left_volume + S->right_volume ) / 2;
  S->low_volume = S->center_volume;

  S->save_fb_pan = 0.5f;
  S->save_center = 1.0f;
  S->save_low = 1.0f;

  set_hardware_volume( S );
}

//############################################################################
//##                                                                        ##
//## Get sample volume and panpot / stereo balance (0=left, 1=right)        ##
//##                                                                        ##
//## Value determines panning position for mono samples, and balance for    ##
//## stereo samples                                                         ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_sample_volume_pan( HSAMPLE S, F32 * volume, F32 * pan )
{
  if ( S == NULL )
  {
    return;
  }

  if (volume)
  {
    *volume = (F32) AIL_pow( S->save_volume, 6.0f / 10.0f );    // turns 0.3F (linear scalar for 10 dB down) into 0.5F
  }

  if (pan)
  {
    *pan = S->save_pan;
  }
}

//############################################################################
//##                                                                        ##
//## Set sample channel level scalars                                       ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_channel_levels (HSAMPLE                S, //)
                                                MSS_SPEAKER const *source_speaker_indexes,
                                                MSS_SPEAKER const *dest_speaker_indexes,
                                                F32 const         *levels,
                                                S32                    n_levels)
{
   if ((S == NULL) ||
       (S->driver->logical_channels_per_sample == 0))
      {
      return;
      }

   if ((source_speaker_indexes == NULL) ||
       (dest_speaker_indexes   == NULL) ||
       (levels                 == NULL))
      {
      set_user_channel_defaults(S);
      return;
      }

   //
   // Get table that turns MSS_SPEAKER equates into build-buffer channel indexes
   //

   S8 const *speaker_enum_to_driver_chan = output_speaker_index[S->driver->logical_channels_per_sample];

   //
   // Fill in requested channel-level array entries
   //

   for (S32 i=0; i < n_levels; i++)
      {
      S32 src_chan = S->speaker_enum_to_source_chan[ source_speaker_indexes[i] ];

      if (src_chan == -1)
         {
         continue;         // (HSAMPLE has no content for this source channel)
         }

      S32 dest_chan = speaker_enum_to_driver_chan[ dest_speaker_indexes[i] ];

      if (dest_chan == -1)
         {
         continue;         // (Driver output mode doesn't support this destination speaker)
         }

      S->user_channel_levels[dest_chan][src_chan] = levels[i];
      }


   //
   // turn on deinterlacing if our stereo mixing can't handle this pair
   //

   S->user_channels_need_deinterlace = 0;

   if (S->format & DIG_F_STEREO_MASK)
   {
     for( S32 b = 0 ; b < S->driver->n_build_buffers ; b++ )
     {
       if ( S->driver->build[b].chans >= 2 )
       {
         if ( ( S->user_channel_levels[ b*2+1 ][0] > 0.0f ) ||
              ( S->user_channel_levels[ b*2 ]  [1] > 0.0f ) )
         {
           S->user_channels_need_deinterlace = 1;
         }
       }
     }
   }
}

//############################################################################
//##                                                                        ##
//## Get sample channel level scalars                                       ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_sample_channel_levels (HSAMPLE                S, //)
                                            MSS_SPEAKER const *source_speaker_indexes,
                                            MSS_SPEAKER const *dest_speaker_indexes,
                                            F32               *levels,
                                            S32                    n_levels)
{
   if ((S                      == NULL) ||
       (source_speaker_indexes == NULL) ||
       (dest_speaker_indexes   == NULL) ||
       (levels                 == NULL) ||
       (S->driver->logical_channels_per_sample == 0))
      {
      return;
      }

   //
   // Get table that turns MSS_SPEAKER equates into build-buffer channel indexes
   //

   S8 const *speaker_enum_to_driver_chan = output_speaker_index[S->driver->logical_channels_per_sample];

   //
   // Store copies of requested channel-level array entries
   //

   for (S32 i=0; i < n_levels; i++)
      {
      S32 src_chan = S->speaker_enum_to_source_chan[ source_speaker_indexes[i] ];

      if (src_chan == -1)
         {
         levels[i] = 0.0F;
         continue;         // (HSAMPLE has no content for this source channel)
         }

      S32 dest_chan = speaker_enum_to_driver_chan[ dest_speaker_indexes[i] ];

      if (dest_chan == -1)
         {
         levels[i] = 0.0F;
         continue;         // (Driver output mode doesn't support this destination speaker)
         }

      levels[i] = S->user_channel_levels[dest_chan][src_chan];
      }
}


//############################################################################
//##                                                                        ##
//## Get/set final speaker scalars                                          ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_speaker_scale_factors (HSAMPLE                 S, //)
                                                       MSS_SPEAKER const * dest_speaker_indexes,
                                                       F32         const * levels,
                                                       S32                     n_levels )
{
  if ( ( S == NULL ) || ( dest_speaker_indexes == NULL ) || ( levels == NULL ) || ( n_levels == 0 ) )
    return;

  S8 const *speaker_enum_to_driver_chan = output_speaker_index[S->driver->logical_channels_per_sample];

  for( S32 i = 0 ; i < n_levels ; i++ )
  {
    S32 dest_chan = speaker_enum_to_driver_chan[ dest_speaker_indexes[ i ] ];

    if (dest_chan != -1)
    {
      S->speaker_levels[ dest_chan ] = levels[ i ];
    }
  }
}

void AILCALL AIL_API_sample_speaker_scale_factors (HSAMPLE                 S, //)
                                                   MSS_SPEAKER const * dest_speaker_indexes,
                                                   F32               * levels,
                                                   S32                     n_levels )
{
  if ( ( S == NULL ) || ( dest_speaker_indexes == NULL ) || ( levels == NULL ) || ( n_levels == 0 ) )
    return;

  S8 const *speaker_enum_to_driver_chan = output_speaker_index[S->driver->logical_channels_per_sample];

  for( S32 i = 0 ; i < n_levels ; i++ )
  {
    S32 dest_chan = speaker_enum_to_driver_chan[ dest_speaker_indexes[ i ] ];

    if (dest_chan != -1)
    {
      levels[ i ] = S->speaker_levels[ dest_chan ];
    }
  }
}


//############################################################################
//##                                                                        ##
//## Get sample output level scalars                                        ##
//##                                                                        ##
//############################################################################

F32 AILCALL AIL_API_sample_output_levels (HSAMPLE                S, //)
                                          MSS_SPEAKER const *source_speaker_indexes,
                                          MSS_SPEAKER const *dest_speaker_indexes,
                                          F32               *levels,
                                          S32                    n_levels)
{
   if ((S                      == NULL) ||
       (source_speaker_indexes == NULL) ||
       (dest_speaker_indexes   == NULL) ||
       (levels                 == NULL) ||
       (S->driver->logical_channels_per_sample == 0))
      {
      return 0.0;
      }

   //
   // Get table that turns MSS_SPEAKER equates into build-buffer channel indexes
   //

   S8 const *speaker_enum_to_driver_chan = output_speaker_index[S->driver->logical_channels_per_sample];

   //
   // Store copies of requested current-scale array entries
   //

   for (S32 i=0; i < n_levels; i++)
      {
      S32 src_chan = S->speaker_enum_to_source_chan[ source_speaker_indexes[i] ];

      if (src_chan == -1)
         {
         levels[i] = 0.0F;
         continue;         // (HSAMPLE has no content for this source channel)
         }

      S32 dest_chan = speaker_enum_to_driver_chan[ dest_speaker_indexes[i] ];

      if (dest_chan == -1)
         {
         levels[i] = 0.0F;
         continue;         // (Driver output mode doesn't support this destination speaker)
         }

      levels[i] = ((F32) S->cur_scale[dest_chan][src_chan]) / 2048.0F;
      }

   return S->max_output_mix_volume;
}

//############################################################################
//##                                                                        ##
//## Set listener-relative source locations used for multispeaker pan       ##
//## calculations                                                           ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_listener_relative_receiver_array (HDIGDRIVER             dig, //)
                                                           MSS_RECEIVER_LIST *array,
                                                           S32                    n_receivers)
{
   if (dig == NULL)
      {
      return;
      }

   if (n_receivers > MAX_RECEIVER_SPECS)
      {
      n_receivers = MAX_RECEIVER_SPECS;
      }

   dig->D3D.n_receiver_specs = n_receivers;

   if (n_receivers)
      {
      AIL_memcpy(dig->D3D.receiver_specifications,
                 array,
                 sizeof(MSS_RECEIVER_LIST) * n_receivers);
      }
}

//############################################################################
//##                                                                        ##
//## Get listener-relative speaker positions (common to all drivers,        ##
//## whether they use the information or not                                ##
//##                                                                        ##
//############################################################################

MSS_RECEIVER_LIST * AILCALL AIL_API_listener_relative_receiver_array (HDIGDRIVER dig, //)
                                                                          S32   *n_receivers)
{
  if (n_receivers != NULL)
   {
   *n_receivers = dig->D3D.n_receiver_specs;
   }

   return dig->D3D.receiver_specifications;
}

//############################################################################
//##                                                                        ##
//## Set reverb response at each speaker (common to all drivers,            ##
//## whether they use the information or not)                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_speaker_reverb_levels (HDIGDRIVER       dig, //)
                                                F32         *wet_array,
                                                F32         *dry_array,
                                                MSS_SPEAKER const *speaker_array,
                                                S32              n_levels)
{
   if ((dig == NULL) || (dig->logical_channels_per_sample == 0))
      {
      return;
      }

   //
   // Initialize to defaults if no level array provided
   //

   if ((wet_array == NULL) || (speaker_array == NULL))
      {
      for (S32 i=0; i < dig->logical_channels_per_sample; i++)
         {
         dig->D3D.speaker_wet_reverb_response[i] = 1.0F;
         }
      }

   if ((dry_array == NULL) || (speaker_array == NULL))
      {
      for (S32 i=0; i < dig->logical_channels_per_sample; i++)
         {
         dig->D3D.speaker_dry_reverb_response[i] = 1.0F;
         }
      }

   if (speaker_array == NULL)
      {
      return;
      }

   if (n_levels > dig->logical_channels_per_sample)
      {
      n_levels = dig->logical_channels_per_sample;
      }

   S8 const *speaker_enum_to_driver_chan = output_speaker_index[dig->logical_channels_per_sample];

   for (S32 i=0; i < n_levels; i++)
      {
      S32 dch = speaker_enum_to_driver_chan[ speaker_array[i] ];

      if (dch == -1)
         {
         continue;
         }

      if (wet_array != NULL) dig->D3D.speaker_wet_reverb_response[dch] = wet_array[i];
      if (dry_array != NULL) dig->D3D.speaker_dry_reverb_response[dch] = dry_array[i];
      }
}

//############################################################################
//##                                                                        ##
//## Set reverb response at each speaker (common to all drivers,            ##
//## whether they use the information or not)                               ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_speaker_reverb_levels (HDIGDRIVER                   dig, //)
                                           F32               * *wet_array,
                                           F32               * *dry_array,
                                           MSS_SPEAKER const * *speaker_array)
{
   if ((dig == NULL) || (dig->logical_channels_per_sample == 0))
      {
      return 0;
      }

   if (speaker_array != NULL)
      {
      *speaker_array = output_speaker_order[dig->logical_channels_per_sample];
      }

   if (wet_array != NULL)
      {
      *wet_array = dig->D3D.speaker_wet_reverb_response;
      }

   if (dry_array != NULL)
      {
      *dry_array = dig->D3D.speaker_dry_reverb_response;
      }

   return dig->logical_channels_per_sample;
}


//############################################################################
//##                                                                        ##
//## Set listener-relative speaker positions (common to all drivers,        ##
//## whether they use the information or not)                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_speaker_configuration (HDIGDRIVER       dig, //)
                                                MSSVECTOR3D *speaker_positions,
                                                S32              n_channels,
                                                F32              falloff_power)
{
   if ((dig == NULL) || (dig->logical_channels_per_sample == 0))
      {
      return;
      }

   //
   // Caller does not have to submit locations for all (or any) speakers -- any
   // speakers beyond n_channels retain their current positions
   //
   // To change the falloff power by itself, pass NULL and 0 for
   // speaker_positions and n_channels when calling this function
   //

   dig->D3D.falloff_power = falloff_power;

   SS_set_speaker_configuration(&dig->D3D,
                                 speaker_positions,
                                 n_channels,
                                 dig->logical_channels_per_sample);
}

//############################################################################
//##                                                                        ##
//## Get listener-relative speaker positions (common to all drivers,        ##
//## whether they use the information or not                                ##
//##                                                                        ##
//############################################################################

MSSVECTOR3D * AILCALL AIL_API_speaker_configuration (HDIGDRIVER         dig, //)
                                                         S32           *n_physical_channels,
                                                         S32           *n_logical_channels,
                                                         F32           *falloff_power,
                                                         MSS_MC_SPEC   *channel_spec)

{
   if ( dig == NULL )
      {
      return NULL;
      }

   if (n_physical_channels != NULL)
      {
      *n_physical_channels = dig->physical_channels_per_sample;
      }

   if (n_logical_channels != NULL)
      {
      *n_logical_channels = dig->logical_channels_per_sample;
      }

   if (falloff_power != NULL)
      {
      *falloff_power = dig->D3D.falloff_power;
      }

   if (channel_spec != NULL)
      {
      *channel_spec = dig->channel_spec;
      }

   return dig->D3D.speaker_positions;
}

//############################################################################
//##                                                                        ##
//## Set sample 5.1 volume and panpot / stereo balance (0=left, 1=right)    ##
//##                                                                        ##
//## Value determines panning position for mono samples, and balance for    ##
//## stereo samples                                                         ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_51_volume_pan( HSAMPLE S, F32 volume, F32 pan, F32 fb_pan, F32 center_level, F32 sub_level )
{
  if ( S == NULL )
  {
    return;
  }

  F32 left, right, front, back;

  S->save_volume = (F32) AIL_pow( volume, 10.0f / 6.0f );
  S->save_pan = pan;
  S->save_fb_pan = fb_pan;
  S->save_center = center_level;
  S->save_low = sub_level;

  if ( fb_pan == 0.5f )
  {
    front = 0.812252196f;
    back = 0.812252196f;
  }
  else
  {
    front = (F32) AIL_pow( 1.0f - fb_pan, 0.3f );
    back = (F32) AIL_pow( fb_pan, 0.3f );
  }

  if ( pan != fb_pan )
  {
    if ( pan == 0.5f )
    {
      left = S->save_volume * 0.812252196f;
      right = S->save_volume * 0.812252196f;
    }
    else
    {
      left = S->save_volume * (F32) AIL_pow( 1.0f - pan, 0.3f );
      right = S->save_volume * (F32) AIL_pow( pan, 0.3f );
    }
  }
  else
  {
    left = S->save_volume * front;
    right = S->save_volume * back;
  }

  S->left_volume = left * front;
  S->right_volume = right * front;

  S->leftb_volume = left * back;
  S->rightb_volume = right * back;

  S->center_volume = S->save_volume * center_level;
  S->low_volume = S->save_volume * sub_level;

  set_hardware_volume( S );
}


//############################################################################
//##                                                                        ##
//## Get sample 5.1 volume and panpot / stereo balance (0=left, 1=right)    ##
//##                                                                        ##
//## Value determines panning position for mono samples, and balance for    ##
//## stereo samples                                                         ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_sample_51_volume_pan( HSAMPLE S, F32 * volume, F32 * lr_pan, F32 * fb_pan, F32 * center_level, F32 * sub_level )
{
  if ( S == NULL )
  {
    return;
  }

  if ( volume )
  {
    *volume = (F32) AIL_pow( S->save_volume, 6.0f / 10.0f );
  }

  if ( lr_pan )
  {
    *lr_pan = S->save_pan;
  }

  if ( fb_pan )
  {
    *fb_pan = S->save_fb_pan;
  }

  if ( center_level )
  {
    *center_level = S->save_center;
  }

  if ( sub_level )
  {
    *sub_level = S->save_low;
  }
}

//############################################################################
//##                                                                        ##
//## Set sample volume levels                                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_51_volume_levels( HSAMPLE S, F32 f_left_level, F32 f_right_level, F32 b_left_level, F32 b_right_level, F32 center_level, F32 sub_level )
{
  if ( S == NULL )
  {
    return;
  }

  S->left_volume = f_left_level;
  S->right_volume = f_right_level;

  S->leftb_volume = b_left_level;
  S->rightb_volume = b_right_level;
  S->center_volume = center_level;
  S->low_volume = sub_level;

  // build some reasonable values for reporting
  if ( f_left_level > 0.0001f )
  {
    F32 ratio = (F32) AIL_pow( f_right_level / f_left_level, 10.0f / 3.0f );
    S->save_pan = ratio / ( ratio + 1 );

    ratio = (F32) AIL_pow( b_left_level / f_left_level, 10.0f / 3.0f );
    S->save_fb_pan = ratio / ( ratio + 1 );

    S->save_volume = ( S->save_pan > 0.0001f ) ?
                       ( ( S->save_fb_pan > 0.0001f ) ?
                         ( b_right_level * (F32) AIL_pow( S->save_pan * S->save_fb_pan, -0.3f ) ) :
                         ( f_right_level * (F32) AIL_pow( S->save_pan, -0.3f ) )
                        ) :
                        ( ( S->save_fb_pan > 0.0001f ) ?
                          ( b_left_level * (F32) AIL_pow( S->save_fb_pan, -0.3f ) ) :
                          f_left_level
                        );
  }
  else
  {
    if ( b_left_level > 0.0001f )
    {
      F32 ratio = (F32) AIL_pow( b_right_level / b_left_level, 10.0f / 3.0f );
      S->save_pan = ratio / ( ratio + 1 );

      ratio = (F32) AIL_pow( f_left_level / b_left_level, 10.0f / 3.0f );
      S->save_fb_pan = 1.0f - ( ratio / ( ratio + 1 ) );

      S->save_volume = ( S->save_pan > 0.0001f ) ?
                         ( ( S->save_fb_pan > 0.0001f ) ?
                           ( b_right_level * (F32) AIL_pow( S->save_pan * S->save_fb_pan, -0.3f ) ) :
                           ( f_right_level * (F32) AIL_pow( S->save_pan, -0.3f ) )
                          ) :
                          ( ( S->save_fb_pan > 0.0001f ) ?
                            ( b_left_level * (F32) AIL_pow( S->save_fb_pan, -0.3f ) ) :
                            f_left_level
                          );
    }
    else
    {
      if ( f_right_level > 0.0001f )
      {
        F32 ratio = (F32) AIL_pow( b_right_level / f_right_level, 10.0f / 3.0f );
        S->save_fb_pan = ratio / ( ratio + 1 );
        S->save_pan = 1.0f;

        S->save_volume = ( S->save_fb_pan > 0.0001f ) ?
                         ( b_right_level * (F32) AIL_pow( S->save_fb_pan, -0.3f ) ) :
                         f_right_level;
      }
      else
      {
        if ( b_right_level > 0.0001f )
        {
          S->save_pan = 1.0f;
          S->save_fb_pan = 1.0f;
          S->save_volume = b_right_level;
        }
        else
        {
          S->save_pan = 0.5f;
          S->save_fb_pan = 0.5f;
          S->save_volume = 0;
        }
      }
    }
  }

  if ( S->save_volume > 0.0001f )
  {
    S->save_center = center_level / S->save_volume;
    S->save_low = sub_level / S->save_volume;
  }
  else
  {
    S->save_center = 0.0f;
    S->save_low = 0.0f;
  }

  set_hardware_volume( S );
}

//############################################################################
//##                                                                        ##
//## Get sample 5.1 volume and panpot / stereo balance (0=left, 1=right)    ##
//##                                                                        ##
//## Value determines panning position for mono samples, and balance for    ##
//## stereo samples                                                         ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_sample_51_volume_levels( HSAMPLE S, F32 * f_left_level, F32 * f_right_level, F32 * b_left_level, F32 * b_right_level, F32 * center_level, F32 * sub_level )
{
  if ( S == NULL )
  {
    return;
  }

  if ( f_left_level )
  {
    *f_left_level = S->left_volume;
  }

  if ( f_right_level )
  {
    *f_right_level = S->right_volume;
  }

  if ( b_left_level )
  {
    *b_left_level = S->leftb_volume;
  }

  if ( b_right_level )
  {
    *b_right_level = S->rightb_volume;
  }

  if ( center_level )
  {
    *center_level = S->center_volume;
  }

  if ( sub_level )
  {
    *sub_level = S->low_volume;
  }
}

//############################################################################
//##                                                                        ##
//## Return # of bytes per sample                                           ##
//##                                                                        ##
//############################################################################

U32 SS_granularity( HSAMPLE S )
{
  switch ( S->format )
  {
    case DIG_F_MONO_16:
    case DIG_F_STEREO_8:
      return( 2 );

    case DIG_F_STEREO_16:
      return( 4 );

    case DIG_F_MULTICHANNEL_16:
      return S->n_channels * sizeof(S16);

    default:
      return( 1 );
  }
}

//############################################################################
//##                                                                        ##
//## Get the granularity of sample type (1=8/m,2=8/s,16/m,4=16/m, or adpcm) ##
//##                                                                        ##
//## Value is returned in bytes                                             ##
//##                                                                        ##
//############################################################################

U32 AILCALL AIL_API_sample_granularity( HSAMPLE S )
{
  if ( S == NULL )
  {
    return( 0 );
  }

  if ( S->pipeline[ SP_ASI_DECODER ].active )
  {
    //
    // All ASI compressed formats are assumed to have 1-byte granularity --
    // the ASI layer will handle any granularity issues associated with
    // seeks, etc. internally
    //

    return( 1 );
  }

  if ( S->format & DIG_F_ADPCM_MASK )
  {
    return( S->adpcm.blocksize );
  }

  return( SS_granularity( S ) );
}


//############################################################################
//##                                                                        ##
//## Get the sample's channel count and mask                                ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_sample_channel_count( HSAMPLE S, U32 *mask )
{
  if ( S == NULL )
  {
    return( 0 );
  }

  if (mask != NULL) *mask = S->channel_mask;

  return S->n_channels;
}

//############################################################################
//##                                                                        ##
//## Get position of next sample block to be transferred                    ##
//##                                                                        ##
//## Value is returned in bytes relative to start of sample, and may be     ##
//## treated as the sample's approximate playback position                  ##
//##                                                                        ##
//############################################################################

U32 AILCALL AIL_API_sample_position( HSAMPLE S )
{
  if ( S == NULL )
  {
    return( 0 );
  }

  check_hardware_buffer( S, 1 );

  return( S->buf[ S->tail ].pos );
}

//############################################################################
//##                                                                        ##
//## Set position of next sample block to be transferred                    ##
//##                                                                        ##
//## Value is specified in bytes relative to start of sample, and will      ##
//## determine the precise location of the next block of data which is      ##
//## fetched from the sample                                                ##
//##                                                                        ##
//## AIL_set_sample_position(0) "rewinds" the sample to its beginning       ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_position( HSAMPLE S, U32 pos )
{
  U32 gran;

  if ( S == NULL )
  {
    return;
  }

  if (pos > S->buf[S->tail].len) pos = S->buf[S->tail].len;

  gran = AIL_sample_granularity( S );

  pos = ( ( pos + ( gran / 2 ) ) / gran ) * gran;

  // Reset ADPCM offset to the end of decode buffer
  // to force a decode buffer refill

  if ( S->format & DIG_F_ADPCM_MASK )
  {
    S->adpcm.blockleft = 0;
    S->adpcm.extrasamples = 0;
  }

  //
  // If this is an ASI-compressed format, perform a seek with offset -1
  // to allow the decoder to reinitialize its state
  //
  // We could also implement this by seeking to the specified offset, but
  // we need to change the position here anyway so it will be reported
  // correctly in subsequent AIL_set_sample_position() calls.  Most ASI
  // decoders don't care about the seek position -- they only need
  // to be informed when we change it
  //

  reset_asi( S, -1, pos );

  if ( S->driver->sample_status[S->index] == SMP_PLAYING )
  {
    check_hardware_buffer( S, 0 );
  }

  if ( pos <= S->buf[S->tail].len )
  {
    S->buf[ S->tail ].pos = pos;
  }

  set_hardware_position( S );
}

//############################################################################
//##                                                                        ##
//## Get number of sample loops remaining                                   ##
//##                                                                        ##
//## 1 indicates that the sample is on its last iteration                   ##
//## 0 indicates that the sample is looping indefinitely                    ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_sample_loop_count( HSAMPLE S )
{
  if ( S == NULL )
  {
    return -1;
  }

  return( S->loop_count );
}

//############################################################################
//##                                                                        ##
//## Set sample loop count                                                  ##
//##                                                                        ##
//##  1: Single iteration, no looping                                       ##
//##  0: Loop indefinitely                                                  ##
//##  n: Play sample n times                                                ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_loop_count(HSAMPLE S, S32 loop_count)
{
  if (S == NULL)
  {
    return;
  }

  S->loop_count      = loop_count;
  S->orig_loop_count = loop_count;
}


static S32 nibbles_per_sample( S32 format )
{
  switch ( format & 0xffff )
  {
    case DIG_F_ADPCM_MONO_16:
      return( 1 );
    case DIG_F_ADPCM_STEREO_16:
    case DIG_F_MONO_8:
      return( 2 );
    case DIG_F_STEREO_8:
    case DIG_F_MONO_16:
      return( 4 );
    case DIG_F_MULTICHANNEL_16:
      return (format >> 16) * 4;
    default:
      return( 8 );
  }
}


DXDEC S32 AILEXPORT AIL_set_sample_loop_samples(HSAMPLE S, S32 loop_start_samples, S32 loop_end_samples)
{
    if (loop_start_samples >= 0 &&
        loop_end_samples >= 0 &&
        loop_start_samples >= loop_end_samples)
    {
        AIL_set_error("Invalid sample interval specified.");
        return 0;
    }

    if (loop_start_samples == -2 && loop_end_samples == -2)
        return 1;
    if (S == 0)
        return 0;


    if (S->pipeline[ SP_ASI_DECODER ].active)
    {
        ASISTAGE *ASI = &S->pipeline[ SP_ASI_DECODER ].TYPE.ASI;

        if (ASI->SET_LOOPING_SAMPLES != (UINTa) -1)
        {
            S32 sample_request[2];
            sample_request[0] = loop_start_samples;
            sample_request[1] = loop_end_samples;

            S32 blocks[2];
            if (ASI->ASI_stream_property(ASI->stream, ASI->SET_LOOPING_SAMPLES, 0, sample_request, blocks))
            {
                // Got block - use these as our loops. We don't call AIL_set_sample_loop_block as it clears the metadata
                // we just set, so we just dupe the code.

                // handle where we are only setting one or the other.
                if (blocks[0] == -2) 
                    blocks[0] = S->orig_loop_start;
                if (blocks[1] == -2) 
                    blocks[1] = S->orig_loop_end;

                S->orig_loop_start = blocks[0];
                S->orig_loop_end   = blocks[1];
                S->loop_start = blocks[0];
                S->loop_end = blocks[1];

                return 1;
            }
        }
        AIL_set_error("Sample's ASI Decoder doesn't support looping at sample accuracy.");
        return 0;
    }
    else
    {
        // PCM/ADPCM data uses normal looping - make sure the data is correct granularity.
        S32 start_sample_bytes = loop_start_samples * nibbles_per_sample(S->format) / 2;
        S32 end_sample_bytes = loop_end_samples * nibbles_per_sample(S->format) / 2;

        U32 gran = AIL_API_sample_granularity( S );
        start_sample_bytes = ( ( start_sample_bytes + ( gran / 2 ) ) / gran ) * gran;
        end_sample_bytes = ( ( end_sample_bytes + ( gran / 2 ) ) / gran ) * gran;

        if (loop_start_samples == -2)
            start_sample_bytes = -2;
        if (loop_end_samples == -2)
            loop_end_samples = -2;

        AIL_set_sample_loop_block(S, start_sample_bytes, end_sample_bytes);
        return 1;
    }
}

//############################################################################
//##                                                                        ##
//## Set optional subblock for looping                                      ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_loop_block( HSAMPLE S, //)
                                            S32     loop_start_offset,
                                            S32     loop_end_offset )
{
  U32 gran;

  if ( S == NULL || (loop_start_offset == -2 && loop_end_offset == -2))
  {
    return;
  }

  // if we are clearing either the front or the back, and we have an asi with
  // sample accurate looping, we need to clear the metadata.
  if (S->pipeline[ SP_ASI_DECODER ].active)
  {
      ASISTAGE *ASI = &S->pipeline[ SP_ASI_DECODER ].TYPE.ASI;
      if (ASI->CLEAR_LOOP_META != (UINTa) -1)
      {
          S32 clear_ends = 0;
          if (loop_start_offset != -2)
              clear_ends |= 1;
          if (loop_end_offset != -2)
              clear_ends |= 2;

          ASI->ASI_stream_property(ASI->stream, ASI->CLEAR_LOOP_META, 0, &clear_ends, 0);
      }
  }

  // handle where we are only setting one or the other.
  if (loop_start_offset == -2) 
      loop_start_offset = S->orig_loop_start;
  if (loop_end_offset == -2) 
      loop_end_offset = S->orig_loop_end;

  if ( (U32) loop_start_offset > (U32) loop_end_offset )
  {
    S32 temp = loop_start_offset;
    loop_start_offset = loop_end_offset;
    loop_end_offset = temp;
  }

  S->orig_loop_start = loop_start_offset;
  S->orig_loop_end   = loop_end_offset;

  gran = AIL_sample_granularity( S );

  S->loop_start = ( ( loop_start_offset + ( gran / 2 ) ) / gran ) * gran;

  if ( loop_end_offset == -1 )
    S->loop_end = -1;
  else
    S->loop_end = ( ( loop_end_offset + ( gran / 2 ) ) / gran ) * gran;

  set_hardware_loop_region( S );
}

//############################################################################
//##                                                                        ##
//## Return optional subblock boundaries                                    ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_sample_loop_block( HSAMPLE S, //)
                                       S32    *loop_start_offset,
                                       S32    *loop_end_offset )
{
  if ( S == NULL )
  {
    return 0;
  }

  if (loop_start_offset) *loop_start_offset = S->orig_loop_start;
  if (loop_end_offset)   *loop_end_offset   = S->orig_loop_end;

  return S->orig_loop_count;
}

//############################################################################
//##                                                                        ##
//## Set master volume for all samples                                      ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_digital_master_volume_level( HDIGDRIVER dig, F32 master_volume )
{
  if ( dig == 0 )
    return;

  dig->master_volume = master_volume;

  set_master_hardware_volume( dig );
}

//############################################################################
//##                                                                        ##
//## Return current master digital volume setting                           ##
//##                                                                        ##
//############################################################################

F32 AILCALL AIL_API_digital_master_volume_level(HDIGDRIVER dig)
{
  if ( dig == 0 )
    return( 0.0f );

  return( dig->master_volume );
}


//############################################################################
//##                                                                        ##
//## Get driver reverb levels                                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_digital_master_reverb_levels( HDIGDRIVER dig, S32 bus_index, F32* dry_level, F32 * wet_level )
{
  if (dig == NULL)
  {
    return;
  }

  if ( dry_level )
  {
    *dry_level = dig->reverb[bus_index].master_dry;
  }
  if ( wet_level )
  {
    *wet_level = dig->reverb[bus_index].master_wet;
  }
}

//############################################################################
//##                                                                        ##
//## Set driver reverb levels                                               ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_digital_master_reverb_levels( HDIGDRIVER dig, S32 bus_index, F32 dry_level, F32 wet_level )
{
  if (dig == NULL)
  {
    return;
  }

  dig->reverb[bus_index].master_dry = dry_level;
  dig->reverb[bus_index].master_wet = wet_level;

  SS_update_driver_reverb_state(dig);

  set_master_hardware_volume( dig );
}

#define REVERB_DELAYS 6

static const F32 ReverbDelayTimes[ REVERB_DELAYS ] = { 0.0297F, 0.0371F, 0.0411F, 0.0437F, 0.005F, 0.0017F };
static const F32 ReverbAllPass[ 2 ] = { 0.0968535f, 0.032924f };

S32 AILCALL AIL_allocate_reverb_buffers( HDIGDRIVER dig, S32 index )
{
  U32 mem;
  U32 del_sizes[ REVERB_DELAYS ];
  U32 del_sizes_pad[ REVERB_DELAYS ];
  U32 i;
  F32 rate = (F32) dig->DMA_rate;

  if (dig->hw_format & DIG_F_OUTPUT_FILTER_IN_USE )
   {
   // Configurations with no build buffers (e.g., those with voice filters) don't do reverb
   return 1;
   }

  // calculate the size of the reverb and predelay buffer sizes
  // This is always a mono buffer, so we'll begin with build_size / (mono or stereo build-buffer channel count)

  dig->reverb[index].reverb_fragment_size = ( dig->build[0].bytes >> ( dig->build[0].chans - 1 ) );

  i = (U32) (S32) ( ( ( ( (F32) AIL_preference[DIG_MAX_PREDELAY_MS] ) / 1000.0f ) * rate ) * 4.0f );

  i = ( ( i + dig->reverb[index].reverb_fragment_size - 1 ) / dig->reverb[index].reverb_fragment_size );

  dig->reverb[index].reverb_buffer_size = i * dig->reverb[index].reverb_fragment_size;

  mem = ( dig->reverb[index].reverb_buffer_size + 15 ) & ~15;

  // calculate the sizes for the delay lines

  for( i = 0 ; i < REVERB_DELAYS ; i++ )
  {
    del_sizes[ i ] = ( (U32) (S32) ( ( rate * ReverbDelayTimes[ i ] ) /*+ 0.9999f*/ ) ) * 4;
    del_sizes_pad[ i ] = ( del_sizes[ i ] + 15 ) & ~15;
    mem += del_sizes_pad[ i ];
  }

  // now allocate the entire buffer
  dig->reverb[index].reverb_total_size = mem;
  dig->reverb[index].reverb_build_buffer = (S32 *) AIL_mem_alloc_lock( mem );

  if ( dig->reverb[index].reverb_build_buffer == 0 )
  {
    return( 0 );
  }

  // clear everything to zeros
  AIL_memset( dig->reverb[index].reverb_build_buffer, 0, mem );

  // set the two reverb offsets
  dig->reverb[index].reverb_head = 0;
  dig->reverb[index].reverb_tail = 0;

  // set all of the delay line pointers
  REVERB_INFO* ri = &dig->reverb[index].ri;
  ri->c.start0 = (F32*) ( ( (U8*) dig->reverb[index].reverb_build_buffer ) + ( ( dig->reverb[index].reverb_buffer_size + 15 ) & ~15 ) );
  ri->c.start1 = (F32*) ( ( (U8*) ri->c.start0 ) + del_sizes_pad[ 0 ] );
  ri->c.start2 = (F32*) ( ( (U8*) ri->c.start1 ) + del_sizes_pad[ 1 ] );
  ri->c.start3 = (F32*) ( ( (U8*) ri->c.start2 ) + del_sizes_pad[ 2 ] );
  ri->c.start4 = (F32*) ( ( (U8*) ri->c.start3 ) + del_sizes_pad[ 3 ] );
  ri->c.start5 = (F32*) ( ( (U8*) ri->c.start4 ) + del_sizes_pad[ 4 ] );

  ri->c.end0 = (F32*) ( ( (U8*) ri->c.start0 ) + del_sizes[ 0 ] );
  ri->c.end1 = (F32*) ( ( (U8*) ri->c.start1 ) + del_sizes[ 1 ] );
  ri->c.end2 = (F32*) ( ( (U8*) ri->c.start2 ) + del_sizes[ 2 ] );
  ri->c.end3 = (F32*) ( ( (U8*) ri->c.start3 ) + del_sizes[ 3 ] );
  ri->c.end4 = (F32*) ( ( (U8*) ri->c.start4 ) + del_sizes[ 4 ] );
  ri->c.end5 = (F32*) ( ( (U8*) ri->c.start5 ) + del_sizes[ 5 ] );

  ri->u.address0 = ri->c.start0;
  ri->u.address1 = ri->c.start1;
  ri->u.address2 = ri->c.start2;
  ri->u.address3 = ri->c.start3;
  ri->u.address4 = ri->c.start4;
  ri->u.address5 = ri->c.start5;

  ri->u.X0 = 0;
  ri->u.X1 = 0;
  ri->u.Y0 = 0;
  ri->u.Y1 = 0;

  return( 1 );
}


static void calc_constants( REVERB_INFO * ri, F32 time, F32 damping )
{
  F32 C;

  ri->c.C0 = (F32) AIL_pow( 0.001f, ReverbDelayTimes[ 0 ] /  time );
  ri->c.C1 = (F32) AIL_pow( 0.001f, ReverbDelayTimes[ 1 ] /  time );
  ri->c.C2 = (F32) AIL_pow( 0.001f, ReverbDelayTimes[ 2 ] /  time );
  ri->c.C3 = (F32) AIL_pow( 0.001f, ReverbDelayTimes[ 3 ] /  time );
  ri->c.C4 = (F32) AIL_pow( 0.001f, ReverbDelayTimes[ 4 ] /  ReverbAllPass[ 0 ] );
  ri->c.C5 = (F32) AIL_pow( 0.001f, ReverbDelayTimes[ 5 ] /  ReverbAllPass[ 1 ] );

  // calculate new coeffs
  if ( damping < 0.0001f )
  {
    ri->c.A = 0.0f;
    ri->c.B0 = 0.0f;
    ri->c.B1 = 0.0f;
  }
  else
  {
    C =  1.0f / (F32) AIL_tan( MY_PI * damping * 0.125f );

    // calculate coefficients
    ri->c.A =  1.0f / ( 1.0f + AIL_fsqrt( 2.0f ) * C + C * C );

    // calculate coefficients
    ri->c.B0 = 2.0f * ( 1.0f - C * C ) * ri->c.A;
    ri->c.B1 = ( 1.0f - AIL_fsqrt( 2.0F ) * C + C * C ) * ri->c.A;
  }

  check_denorm( &ri->c.C0 );
  check_denorm( &ri->c.C1 );
  check_denorm( &ri->c.C2 );
  check_denorm( &ri->c.C3 );
  check_denorm( &ri->c.C4 );
  check_denorm( &ri->c.C5 );
  check_denorm( &ri->c.A );
  check_denorm( &ri->c.B0 );
  check_denorm( &ri->c.B1 );
}

#endif

#define f2i(val) ((S32)(val))

#ifdef MSS_SPU_PROCESS
  static S32 * savereverb = 0;
  static S32 savereverbhead = 0;
  static S32 * spureverb = 0;

  static void set_reverb_ptr( REVERB_INFO * ri, void * ptr )
  {
    void * st = ri->c.start0;

    ri->c.start0 = (F32*) ptr;
    ri->c.start1 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.start1, st ) );
    ri->c.start2 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.start2, st ) );
    ri->c.start3 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.start3, st ) );
    ri->c.start4 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.start4, st ) );
    ri->c.start5 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.start5, st ) );

    ri->c.end0 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.end0, st ) );
    ri->c.end1 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.end1, st ) );
    ri->c.end2 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.end2, st ) );
    ri->c.end3 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.end3, st ) );
    ri->c.end4 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.end4, st ) );
    ri->c.end5 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->c.end5, st ) );

    ri->u.address0 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->u.address0, st ) );
    ri->u.address1 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->u.address1, st ) );
    ri->u.address2 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->u.address2, st ) );
    ri->u.address3 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->u.address3, st ) );
    ri->u.address4 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->u.address4, st ) );
    ri->u.address5 = (F32*) AIL_ptr_add( ptr, AIL_ptr_dif( ri->u.address5, st ) );
  }

  static void reset_reverb_head( HDIGDRIVER dig )
  {
    RAD_memcpy_to_PPU( AIL_ptr_add( savereverb, savereverbhead ), dig->reverb[0].reverb_build_buffer, dig->reverb[0].reverb_fragment_size, -1 );
    dig->reverb[0].reverb_head = savereverbhead;
    dig->reverb[0].reverb_build_buffer = savereverb;
  }
#endif

static void do_reverb(HDIGDRIVER dig, S32 index)
{
#ifdef MSS_SPU_PROCESS
   addstackmark();
   reset_reverb_head( dig );

   F32 * savedelays = dig->reverb[0].ri.c.start0;
   F32 * delays = (F32*) spu_alloc( AIL_ptr_dif( dig->reverb[0].ri.c.end5, dig->reverb[0].ri.c.start0 ) + SPU_EXTRA );
   delays = (F32*) align_memcpy_from_PPU( delays, savedelays, AIL_ptr_dif( dig->reverb[0].ri.c.end5, dig->reverb[0].ri.c.start0 ), -1 );
   set_reverb_ptr( &dig->reverb[0].ri, delays );
#endif

   //
   // Advance head (input) pointer
   //

   dig->reverb[index].reverb_head += dig->reverb[index].reverb_fragment_size;

   if (dig->reverb[index].reverb_head >= dig->reverb[index].reverb_buffer_size)
      {
      dig->reverb[index].reverb_head = 0;
      }

   //
   // Get region to process, and advance tail (output) pointer
   //

   U32 bytes   = 0;
   U32 remnant = 0;

   void *src = ( (U8*) dig->reverb[index].reverb_build_buffer ) + dig->reverb[index].reverb_tail;

   S32 amount_left = dig->reverb[index].reverb_buffer_size - dig->reverb[index].reverb_tail;

   if (dig->reverb[index].reverb_fragment_size >= amount_left)
      {
      //
      // Do last part of source buffer followed by first part
      //

      bytes            = amount_left;
      remnant          = dig->reverb[index].reverb_fragment_size - amount_left;
      dig->reverb[index].reverb_tail = remnant;
      }
   else
      {
      //
      // Do the entire source buffer
      //

      bytes             = dig->reverb[index].reverb_fragment_size;
      remnant           = 0;
      dig->reverb[index].reverb_tail += dig->reverb[index].reverb_fragment_size;
      }

   //
   // Up to 2 iterations may be necessary to handle buffer wrap
   //

   S32 d = 0;

   while (bytes > 0)
      {
#ifdef MSS_SPU_PROCESS
      src = (S32*) align_memcpy_from_PPU( spureverb, src, bytes, -1 );
#endif

      U32 num = bytes / sizeof(S32);
      S32 *s = (S32 *) src;

      REVERB_INFO ri;
      AIL_memcpy(&ri, &dig->reverb[index].ri, sizeof(ri));

      while (num--)
         {
         F32 temp, U1, U2;
         F32 comb_sum;
         F32 samp, lp_samp;

         samp = ( (F32) *s++ ) * ri.c.A;
         check_denorm( &samp );

         // calculate sample
         lp_samp = samp + ri.u.X0 + ri.u.X0 + ri.u.X1
                   - ri.c.B0 * ri.u.Y0
                   - ri.c.B1 * ri.u.Y1 + ( 1.192092896e-06F * 2.0f );

         check_denorm( &lp_samp );

         // save sample history
         ri.u.X1 = ri.u.X0;
         ri.u.X0 = samp;
         ri.u.Y1 = ri.u.Y0;
         ri.u.Y0 = lp_samp;

         // process sum of comb filters
         comb_sum = *ri.u.address0 + \
                    *ri.u.address1 + \
                    *ri.u.address2 + \
                    *ri.u.address3;

         check_denorm( &comb_sum );

         // process coef for each comb filter

         *ri.u.address0 = ri.c.C0 * ( *ri.u.address0 ) + lp_samp;
         *ri.u.address1 = ri.c.C1 * ( *ri.u.address1 ) + lp_samp;
         *ri.u.address2 = ri.c.C2 * ( *ri.u.address2 ) + lp_samp;
         *ri.u.address3 = ri.c.C3 * ( *ri.u.address3 ) + lp_samp;

         check_denorm( ri.u.address0 );
         check_denorm( ri.u.address1 );
         check_denorm( ri.u.address2 );
         check_denorm( ri.u.address3 );

         // advance inputs
         ++ri.u.address0;
         ++ri.u.address1;
         ++ri.u.address2;
         ++ri.u.address3;

         // process clip for buffer wrap
         if ( ri.u.address0 >= ri.c.end0 ) ri.u.address0 = ri.c.start0;
         if ( ri.u.address1 >= ri.c.end1 ) ri.u.address1 = ri.c.start1;
         if ( ri.u.address2 >= ri.c.end2 ) ri.u.address2 = ri.c.start2;
         if ( ri.u.address3 >= ri.c.end3 ) ri.u.address3 = ri.c.start3;

         // get allpass value
         U1 = *ri.u.address4;
         check_denorm(ri.u.address4);
         check_denorm(ri.u.address5);

         // all pass
         temp = ri.c.C4 * U1 + comb_sum;

         *ri.u.address4 = temp;
         ++ri.u.address4;

         check_denorm(ri.u.address4);

         // adjust
         U1 -= ri.c.C4 * temp;
         U2 = *ri.u.address5;

         // all pass
         temp = ri.c.C5 * U2 + ( U1 * 0.25f );
         *ri.u.address5 = temp;
         ++ri.u.address5;

         check_denorm(ri.u.address5);

         // compute left
         F32 left;

         left = U2 - ri.c.C5 * temp;
         check_denorm(&left);

         // process clip for buffer wrap
         if ( ri.u.address4 >= ri.c.end4 )
           ri.u.address4 = ri.c.start4;

         if ( ri.u.address5 >= ri.c.end5 )
           ri.u.address5 = ri.c.start5;

         //
         // Add new reverb output sample to all build buffers
         //

         for (S32 i=0; i < dig->n_build_buffers; i++)
            {
            MSS_BB *B = &dig->build[i];

            if (B->chans == 2)
               {
               F32 right = U2 - ri.c.C5 * ( ri.c.C5 * U2 + ( U1 * 0.15f ) );
               check_denorm(&right);

               B->buffer[(d * 2)    ] += f2i(left  * dig->D3D.speaker_wet_reverb_response[B->speaker_offset    ]);
               B->buffer[(d * 2) + 1] += f2i(right * dig->D3D.speaker_wet_reverb_response[B->speaker_offset + 1]);
               }
            else
               {
               B->buffer[d] += f2i(left * dig->D3D.speaker_wet_reverb_response[B->speaker_offset]);
               }
            }

         //
         // Advance to next destination sample
         //

         ++d;
         }

      dig->reverb[index].ri.u = ri.u;

      bytes   = remnant;
      remnant = 0;

      if (bytes > 0)
         {
         src = dig->reverb[index].reverb_build_buffer;
         }
      }

#ifdef MSS_SPU_PROCESS
   RAD_memcpy_to_PPU( savedelays, delays, 15360, SPU_DEFAULT_TO_PPU );
   RAD_memcpy_to_PPU( AIL_ptr_add( savedelays, 15360 ), AIL_ptr_add( delays, 15360 ), ( AIL_ptr_dif( dig->reverb[0].ri.c.end5, dig->reverb[0].ri.c.start0 ) - 15360 + 15 ) & ~15, -1 );
   set_reverb_ptr( &dig->reverb[0].ri, savedelays );
#endif
}

#ifndef MSS_SPU_PROCESS

//############################################################################
//##                                                                        ##
//## Set driver reverb settings                                             ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_digital_master_reverb(HDIGDRIVER dig, //)
                                               S32        bus_index,
                                               F32        reverb_decay_time,
                                               F32        reverb_predelay,
                                               F32        reverb_damping)
{
   if (dig == NULL)
      {
      return;
      }

   F32 clamp = (F32) AIL_preference[DIG_MAX_PREDELAY_MS];

   if (reverb_predelay > clamp)
      {
      reverb_predelay = clamp;
      }

   dig->reverb[bus_index].reverb_decay_time_s = reverb_decay_time;
   dig->reverb[bus_index].reverb_predelay_s   = reverb_predelay;
   dig->reverb[bus_index].reverb_damping      = reverb_damping;

   //
   // Calculate software reverb filter parameters if applicable
   //

#ifdef MSS_VFLT_SUPPORTED
   if (dig->voice_filter == NULL)
#endif
      {
      dig->reverb[bus_index].reverb_duration_ms = (S32) ((reverb_decay_time + reverb_predelay) * 1000.0F);

      SS_update_driver_reverb_state(dig);

      //
      // Tail (output) pointer is <predelay> behind head (incoming) pointer
      //

      S32 bytes = ( (((S32) (reverb_predelay * (F32) dig->DMA_rate)) * sizeof(S32) ) + 15 ) & ~15;

      // bugfix - large predelays cause reverb_tail to be negative and things go very wrong.
      if (bytes >= dig->reverb[bus_index].reverb_buffer_size)
          bytes = dig->reverb[bus_index].reverb_buffer_size - 1;

      dig->reverb[bus_index].reverb_tail = dig->reverb[0].reverb_head - bytes;

      if (dig->reverb[bus_index].reverb_tail < 0)
         {
         dig->reverb[bus_index].reverb_tail += dig->reverb[bus_index].reverb_buffer_size;
         }

      calc_constants( &dig->reverb[bus_index].ri, dig->reverb[bus_index].reverb_decay_time_s, dig->reverb[bus_index].reverb_damping );
      }
}

//############################################################################
//##                                                                        ##
//## Get driver reverb settings                                             ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_digital_master_reverb( HDIGDRIVER dig, S32 bus_index, F32 * reverb_time, F32* reverb_predelay, F32* reverb_damping )
{
  if ( dig == NULL )
  {
    return;
  }

  if ( reverb_time )
  {
    *reverb_time = dig->reverb[bus_index].reverb_decay_time_s;
  }
  if ( reverb_predelay )
  {
    *reverb_predelay = dig->reverb[bus_index].reverb_predelay_s;
  }
  if ( reverb_damping )
  {
    *reverb_damping = dig->reverb[bus_index].reverb_damping;
  }
}

#endif

// The lowpass filter is fairly complicated state machine.  It handles blending
// into an lp, from one lp level to another, and out of an lp.  It also handles
// setting another level while one is be ramped to, and about 8 million corner
// cases <sigh>.

// the flags field in the lp structure hold the remaining ramps in the low 16-bits
// and the current state in the upper 16-bits.  If the upper 16-bits are zero,
// then the low pass is off. If the upper 16-bits is 1, then the lp is running
// normally, not in any ramped state.  If the upper 16-bits is 2, then the lp
// is blending over a ramp window from no-lp to the lp being turned on.  If the
// upper 16-bits is 3, then the lp is blending from lp-on to the lp being turned off.

// When the end of the ramp period is hit, then we turn off the lp when in state 3,
// we flip to normal mode from state 2, and simply stop blending between the two
// lp levels if we were in state 1.

// If the user changes the LP state to on when the LP isn't on, then we flip
// to state 2. If the lp was already running, and the user sets is an lp-on
// value, then we flip to mode 1. if the lp was running, and the user flips
// it off, then we flip to state 3.

// If there was already a ramp window in effect when the user sets the lp state,
// then, and if none of the ramp has been applied, then we cancel the old lp
// blend-to values, and replace them.  if the ramp has been partially applied,
// then we queue up the new value to take effect when the existing ramp finished.
// if there is already a queued up lp state, then we can safely just replace it
// (since the user has never heard it).

// To keep all of this state small, all of the values are packed into 32-bit
// values - the high 16-bit values are the previous LP setting, and the low
// 16-bit values are the current ones.  When we finish the ramp, then we clear
// the top half of the 16-bit values.

// We try to cache a lot of calculations, so you need to reset the A and queuedA
// fields to 0x7fff when resetting the lp, along with calculated_cut to -1.


//############################################################################
//##                                                                        ##
//## Lowpass helper functions                                               ##
//##                                                                        ##
//############################################################################

static void set_lp_info( LOWPASS_INFO * lp, S32 newA, S32 newB0, S32 newB1 )
{
  lp->A  <<= 16;
  lp->B0 <<= 16;
  lp->B1 <<= 16;
  lp->Y0 <<= 16;  // put the current values into the old slots
  lp->Y1 <<= 16;

  // set the coefficents in the low words (top word is the old slots)
  lp->A  |=  ( 0xffff & newA );
  lp->B0 |=  ( 0xffff & newB0 );
  lp->B1 |=  ( 0xffff & newB1 );

  // try to set up some kind of history
  S32 out_s1  = ( ( lp->X0 + lp->X1 * 3 ) * newA
                   - newB0 * ( lp->Y0 >> 16 ) - newB1 * ( lp->Y1 >> 16 ) ) >> LP_SHIFT;

  S32 out_s0  = ( ( lp->X0 * 3 + lp->X1 ) * newA
                   - newB0 * out_s1 - newB1 * ( lp->Y0 >> 16 ) ) >> LP_SHIFT;

  lp->Y0 |= ( 0xffff & out_s0 );
  lp->Y1 |= ( 0xffff & out_s1 );

  // only two options for flags: 0x10000 (lp in progress), or 0 (no lp)
  if ( lp->flags )
    lp->flags = 0x10000 | ( ( 1 << LP_RAMP_BITS ) - 1 );  // 1 to saying to ramp from old to new
  else
    lp->flags = 0x20000 | ( ( 1 << LP_RAMP_BITS ) - 1 );  // 2 to saying to ramp from none to new
}


static S32 do_low_pass( S16 * dest, S16 const * src, LOWPASS_INFO * plp, S32 num_samples )
{
  S16 const *s = src;
  S16 const *d = dest;
  S32 n_samples = num_samples;

  LOWPASS_INFO lp = *plp;

 start_over:

  // low word of flags is the ramp samples to do
  S32 ramps = lp.flags & 0xffff;
  S32 which = lp.flags >> 16;

  if (n_samples == 0)
      return ( (which == 3) ? 1 : 0 );

  // if we have ramping to do, start it up...
  if ( ramps )
  {
    for(;;)
    {
      register S32 in_s, oout_s, out_s;

      in_s = ( (S32) (S16) LE_SWAP16(s) );

      // calculate the previous filter value
      if ( which > 1 )
        oout_s = in_s;
      else
      {
        oout_s = ( ( in_s + lp.X0 + lp.X0 + lp.X1 ) * ( lp.A >> 16 )
                     - ( lp.B0 >> 16 ) * ( lp.Y0 >> 16 )
                     - ( lp.B1 >> 16 ) * ( lp.Y1 >> 16 ) ) >> LP_SHIFT;

        if ( oout_s < -32768 )
          oout_s = -32768;
        else if ( oout_s > 32767 )
          oout_s = 32767;
      }

      // calculate the new filter value
      out_s  = ( ( in_s + lp.X0 + lp.X0 + lp.X1 ) * ( (S32)(S16) lp.A )
                   - ( (S32)(S16) lp.B0 ) * ( (S32)(S16) lp.Y0 )
                   - ( (S32)(S16) lp.B1 ) * ( (S32)(S16) lp.Y1 ) ) >> LP_SHIFT;

      if ( out_s < -32768 )
        out_s = -32768;
      else if ( out_s > 32767 )
        out_s = 32767;

      lp.X1 = lp.X0;
      lp.X0 = in_s;
      lp.Y1 = lp.Y0;
      lp.Y0 = ( oout_s << 16 ) | ( out_s & 0xffff );

      // blend between them (but invert the weights when fading out)
      if ( which == 3 )
        out_s = ( oout_s * ( ( 1 << LP_RAMP_BITS ) - ramps )  + out_s * ramps ) >> LP_RAMP_BITS;
      else
        out_s = ( out_s * ( ( 1 << LP_RAMP_BITS ) - ramps )  + oout_s * ramps ) >> LP_RAMP_BITS;

      STORE_LE_SWAP16( d, (S16) out_s );

      ++s;
      ++d;

      --n_samples;
      --ramps;

      // have we hit the end of the ramp period?
      if ( ramps == 0 )
      {
        if ( which == 3 )
        {
          // when we have faded out the lp to normal, then reset all of the lp values
          lp.flags = 0;
          lp.A = 0x7fff;
          lp.X0 = 0;
          lp.X1 = 0;
          lp.Y0 = 0;
          lp.Y1 = 0;
          lp.calculated_cut = -1.0f;
        }
        else
        {
          // otherwise, just flip the flags into normal lp mode (no blending)
          lp.flags = 0x10000;
        }


        // before we drop out of the loop - see if we have a queued lp value to switch to
        if ( lp.queuedA != 0x7fff )
        {
          if ( lp.queuedA == 0x30000 )
          {
            // we have a queued up fade lp out command - set the proper flags value
            lp.flags = 0x30000 | ( ( 1 << LP_RAMP_BITS ) - 1 );

            // clear the queued state
            lp.queuedA = 0x7fff;

            // if we don't have any more samples, then jump to the clean up clause
            if ( n_samples == 0 )
              goto end_nsamp;

            // otherwise start over at the top of this function
            goto start_over;
          }
          else
          {
            // set the lp settings from the queued values
            set_lp_info( &lp, lp.queuedA, (S32)(S16) lp.queuedB, lp.queuedB >> 16 );

            // clear the queued state
            lp.queuedA = 0x7fff;

            // if we don't have any more samples, then jump to the clean up clause
            if ( n_samples == 0 )
              goto end_nsamp;

            // otherwise start over at the top of this function
            goto start_over;
          }
        }
        break;
      }

      if ( n_samples == 0 )
      {
        // ok, we have hit the end of the buffer, and we still have ramps left
       end_nsamp:
        // save the ramps left, and exit the loop
        lp.flags = ( lp.flags & 0xffff0000 ) | ramps;
        break;
      }
    }
  }

  // do we have any more samples after that loop?
  if ( n_samples )
  {
    if ( which == 3 )
    {
      // if we were blending out the filtering, just copy the rest of the buffer
      for(;;)
      {
        register S32 in_s;

        in_s = ( (S32) (S16) LE_SWAP16(s) );
        STORE_LE_SWAP16( d, (S16) in_s );

        --n_samples;
        if ( n_samples == 0 )
          break;

        ++s;
        ++d;
      }
    }
    else
    {
      // this is the loop that takes all the cpu on the lp filter (ramping is over quickly)

      // reset the 16-bit values to just the current values
      lp.A  = (S32)(S16) lp.A;
      lp.B0 = (S32)(S16) lp.B0;
      lp.B1 = (S32)(S16) lp.B1;
      lp.Y0 = (S32)(S16) lp.Y0;
      lp.Y1 = (S32)(S16) lp.Y1;

      // we aren't blending, so just normal low-passing
      for(;;)
      {
        register S32 in_s, out_s;

        in_s = ( (S32) (S16) LE_SWAP16(s) );

        out_s  = ( ( in_s + lp.X0 + lp.X0 + lp.X1 ) * lp.A
                  - lp.B0 * lp.Y0
                  - lp.B1 * lp.Y1 ) >> LP_SHIFT;

        if ( out_s < -32768 )
          out_s = -32768;
        else if ( out_s > 32767 )
          out_s = 32767;

        lp.X1 = lp.X0;
        lp.X0 = in_s;
        lp.Y1 = lp.Y0;
        lp.Y0 = out_s;

        STORE_LE_SWAP16( d, (S16) out_s );

        --n_samples;
        if ( n_samples == 0 )
          break;

        ++s;
        ++d;
      }
    }
  }

  *plp = lp;

  return ( (which == 3) ? 1 : 0 );
}

static void update_low_pass_coef(HSAMPLE S, S32 channel, F32 cutoff)
{
    S32 ramping;

    // check to see if a ramp is already happening
    ramping = ( S->lp[channel].flags & 0xffff );
    if ( ramping )
    {
      if ( ramping == ( ( 1 << LP_RAMP_BITS ) - 1 ) )
      {
        // if none of the ramping has happened, then just override the old setting
        switch ( S->lp[channel].flags >> 16 )
        {
          case 2:
            // we simply clear out the old blend into amount (since the new one will replace it)
            S->lp[channel].flags = 0;
            S->lp[channel].calculated_cut = -1.0f;
            S->lp[channel].A = 0x7fff;
            break;
          case 1:
            // put the old blend from amounts in the current slot (we'll move them back with the new values)
            S->lp[channel].A  >>= 16;
            S->lp[channel].B0 >>= 16;
            S->lp[channel].B1 >>= 16;
            S->lp[channel].Y0 >>= 16;
            S->lp[channel].Y1 >>= 16;
            S->lp[channel].flags = 0x10000;
            S->lp[channel].calculated_cut = -1.0f;
            break;
        }

        // clear the ramp flag
        ramping = 0;
      }
    }


    // now let's apply the settings
    if (cutoff < 0.0f || cutoff >= 0.999f)
    {
       S->lp[channel].calculated_cut = -1.0F;
       S->lp[channel].cutoff = 1.0f;

       if ( ramping )
       {
         // we are ramping, so we can't immediately set this flag - queue for later
         S->lp[channel].queuedA = 0x30000;
       }
       else
       {
         // if we are already lowpass filtering, then ramp out the filtering to non-filtering
         if ( S->lp[channel].flags )
           S->lp[channel].flags = 0x30000 | ( ( 1 << LP_RAMP_BITS ) - 1 );
       }
    }
    else
    {
      // low pass is a function of the playback rate, so factor all that in
      F32 rate_Hz = (F32) S->original_playback_rate * S->playback_rate_factor;
      F32 cut = (cutoff * rate_Hz) / ((F32) S->driver->DMA_rate);
      if (cut > 0.98F) cut = 0.98F;   // (higher values cause feedback artifacts, esp. when upsampling)
      if (cut < 0.0001F) cut = 0.0F;

      if (cut != S->lp[channel].calculated_cut)
      {
        S->lp[channel].calculated_cut = cut;               // filter parms need recalculating
        S->lp_any_on = 1;
        S->lp[channel].cutoff = cutoff;

        if ( cut < 0.001F )
        {
          //  the cut level is so low, that we are filtering all frequencies
         all_filtered:
          if ( ramping )
          {
            // if currently ramping, then queue it
            S->lp[channel].queuedA = 0;
            S->lp[channel].queuedB = 0;
          }
          else
          {
            // otherwise set the new zero values
            set_lp_info( &S->lp[ channel ], 0, 0, 0 );
          }
        }
        else
        {
          F32 A, C;
          S32 newA, newB0, newB1;

          // calculate new coeffs
          C =  1.0F / (F32) AIL_tan( MY_PI * cut * 0.5f );

          // calculate coefficients
          A = LP_FP / ( 1.0F + ( SQRTOF2 * C ) + C * C );

          // clamp to fixed point
          newA = (S32) A;
          if ( A == 0 )
            goto all_filtered;

          // back to float
          A = (F32)( newA ) / LP_FP;

          // back calculate the C to use (after clamping to int)
          C = ( AIL_fsqrt( ( 4.0f / A ) - 2.0f ) - SQRTOF2 ) / 2.0f;

          newB0 = (S32)( 2.0F * ( 1.0F - C * C ) * A * LP_FP );
          newB1 = (S32)( ( 1.0F - ( SQRTOF2 * C ) + C * C ) * A * LP_FP );

          if ( ramping )
          {
            // we're in the middle of a ramp, so queue this setting
            S->lp[channel].queuedA = newA;
            S->lp[channel].queuedB = ( newB1 << 16 ) | ( newB0 & 0xffff );
          }
          else
          {
            // if the new setting is different than the old one, then set it!
            if ( newA != (S32)(S16) S->lp[channel].A )
              set_lp_info( &S->lp[ channel ], newA, newB0, newB1 );
          }
        }
      }
    }

  set_hardware_low_pass( S );
}

#ifndef MSS_SPU_PROCESS


void AILCALL AIL_API_set_sample_low_pass_cut_off( HSAMPLE S,
                                                  S32 /*-1 or MSS_SPEAKER*/ input_channel,
                                                  F32         cutoff )
{
  S32 s, e, channel;

  if (S == NULL)
  {
    return;
  }

  if ( input_channel == -1 )
  {
    s = 0;
    e = S->n_channels;
  }
  else
  {
    s = S->speaker_enum_to_source_chan[input_channel];

    if ( s == -1 )
      return;
    e = s + 1;
  }

  for( channel = s ; channel < e ; channel++ )
  {
      S->low_pass_cutoff[channel] = cutoff;
      S->low_pass_changed |= (1 << channel);
  }
}



F32 AILCALL AIL_API_sample_low_pass_cut_off(HSAMPLE S, S32 /*-1 or MSS_SPEAKER*/ input_channel)
{
   S32 channel;

   if ( S == NULL )
      return 1.0f;

   if ( input_channel == -1 )
      input_channel = 0;

   channel = S->speaker_enum_to_source_chan[input_channel];

   if ( channel != -1 )
      if ( S->lp[ channel ].flags )
         return( S->lp[ channel ].cutoff );

  return( 1.0f );
 }


void AILCALL AIL_API_schedule_start_sample(HSAMPLE S, U64 mix_time_to_start)
{
    if (S == 0)
        return;

    if (S->driver->sample_status[S->index] == SMP_FREE)
        return;

    //
    // If this is a double-buffered sample that's already playing, just return without
    // rewinding it
    //
    if (((S->driver->sample_status[S->index] == SMP_PLAYING) || (S->driver->sample_status[S->index] == SMP_PLAYINGBUTRELEASED))
        &&
        (S->service_type == SERVICE_STREAMED))
    {
        return;
    }

    //
    // Make sure valid sample data exists
    //
    if ( ( S->buf[ S->tail ].len   == 0 ) ||
        ( S->buf[ S->tail ].start == NULL ) )
    {
        return;
    }

    S->state_level = 1 << S->driver->state_index;

    S->mix_start_time = mix_time_to_start;
    S->driver->scheduled_sample_count++;
}

//############################################################################
//##                                                                        ##
//## Start playback of sample from beginning                                ##
//##                                                                        ##
//## At a minimum, sample must first have been initialized with prior calls ##
//## to AIL_init_sample() and AIL_set_sample_address() or                   ##
//## AIL_load_sample_buffer()                                               ##
//##                                                                        ##
//## Playback will begin at the next DMA half-buffer transition             ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_start_sample( HSAMPLE S )
{
  if ( S == NULL )
  {
    return;
  }

  //
  // Make sure sample has been allocated
  //

  if ( S->driver->sample_status[S->index] == SMP_FREE )
  {
    return;
  }

  //
  // If this is a double-buffered sample that's already playing, just return without
  // rewinding it
  //

  if (((S->driver->sample_status[S->index] == SMP_PLAYING) || (S->driver->sample_status[S->index] == SMP_PLAYINGBUTRELEASED))
       &&
      (S->service_type == SERVICE_STREAMED))
   {
   return;
   }

  //
  // Make sure valid sample data exists
  //

  if ( ( S->buf[ S->tail ].len   == 0 ) ||
       ( S->buf[ S->tail ].start == NULL ) )
  {
    return;
  }

  // Clear residuals
  clear_last_sample(S);

  // Reset ADPCM data
  S->adpcm.blockleft = 0;
  S->adpcm.extrasamples = 0;

  // Reset fading, in case we were forcibly stopped during the fade.
  S->fade_to_stop = 0;
  S->pop_fade_stop = 0;
  S->pop_fade_time = 0;
  S->pop_fade_total = 0;

  // Set the sample to the current pushed level.
  S->state_level = (1 << S->driver->state_index);

  //
  // Rewind sample to beginning
  //

  S->buf[ S->tail ].pos = 0;

  // Flush and reset ASI decoder, if any
  if (S->pipeline[ SP_ASI_DECODER ].active)
  {
    ASISTAGE *ASI = &S->pipeline[ SP_ASI_DECODER ].TYPE.ASI;

    ASI->ASI_stream_seek( ASI->stream, -1 );

    if ((S->service_type == SERVICE_SINGLE_BUFFER) && (ASI->STREAM_SEEK_POS != (UINTa) -1))
      {
      ASI->ASI_stream_property(ASI->stream,
                               ASI->STREAM_SEEK_POS,
                               0,&S->buf[ S->tail ].pos, &S->buf[ S->tail ].pos);
      }
  }

  #if defined(IS_LINUX)
  if ( S->driver->released )
  {
     S->driver->sample_status[S->index] = SMP_PLAYINGBUTRELEASED;
  }
  else
  #endif
  {
    //
    // start reverb if we need to
    //

    SS_update_sample_reverb_state(S);

    //
    // Set 'playing' status
    //

    S->driver->sample_status[S->index] = SMP_PLAYING;

    //
    // If sample's driver is not already transmitting data, start it
    //
    SS_start_playback( S );
  }
}

//############################################################################
//##                                                                        ##
//## Stop playback of sample                                                ##
//##                                                                        ##
//## Sample playback may be resumed with AIL_resume_sample(), or restarted  ##
//## from the beginning with AIL_start_sample()                             ##
//##                                                                        ##
//## Playback will stop at the next DMA half-buffer transition              ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_stop_sample( HSAMPLE S )
{
  if ( S == NULL )
  {
    return;
  }

  //
  // Make sure sample is currently playing
  //

  if ( S->driver->sample_status[S->index] != SMP_PLAYING )
  {
    // on the xbox, if the sound is stopped and they call stop again, do the spin
    hardware_spin_until_stopped( S );
    return;
  }

  //
  // Mask 'playing' status
  //

  S->driver->sample_status[S->index] = SMP_STOPPED;
}

//############################################################################
//##                                                                        ##
//## Resume playback of sample from current position                        ##
//##                                                                        ##
//## Playback will resume at the next DMA half-buffer transition            ##
//##                                                                        ##
//############################################################################
void AILCALL AIL_API_end_fade_sample(HSAMPLE S)
{
    if (S == 0)
        return;

    S->fade_to_stop = 1;
}

//############################################################################
//##                                                                        ##
//## Resume playback of sample from current position                        ##
//##                                                                        ##
//## Playback will resume at the next DMA half-buffer transition            ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_resume_sample( HSAMPLE S )
{
  if ( S == NULL )
  {
    return;
  }

  //
  // See if sample has been previously stopped
  //

  if ( S->driver->sample_status[S->index] == SMP_STOPPED )
  {
    goto startsound;
  }
  else
  {
    if ( S->driver->sample_status[S->index] == SMP_DONE )
    {
      //
      // this means they called resume before start (good for non-zero
      //   start offsets)
      //

      //
      // Make sure valid sample data exists
      //

      if ( ( S->buf[ S->tail ].len   == 0 ) ||
           ( S->buf[ S->tail ].start == NULL ) )
      {
        return;
      }

startsound:

      // Reset fading, in case we were forcibly stopped during the fade.
      S->fade_to_stop = 0;
      S->pop_fade_stop = 0;
      S->pop_fade_time = 0;
      S->pop_fade_total = 0;

      // Set the sample to the current pushed level.
      S->state_level = (1 << S->driver->state_index);

      // Clear residuals
      clear_last_sample(S);

      #if defined(IS_LINUX)
      if ( S->driver->released )
      {
        S->driver->sample_status[S->index] = SMP_PLAYINGBUTRELEASED;
      }
      else
      #endif
      {
        //
        // start reverb if we need to
        //

        SS_update_sample_reverb_state(S);

        //
        // Set 'playing' status
        //

        S->driver->sample_status[S->index] = SMP_PLAYING;

        //
        // If sample's driver is not already transmitting data, start it
        //

        SS_start_playback( S );
      }
    }
  }
}

//############################################################################
//##                                                                        ##
//## Start/stop/etc a group of samples at once.                             ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_sample_group_operation(HDIGDRIVER dig, S32 op, S32 start_id, S32 set_to_id)
{
    HSAMPLE S;
    S32 n;
    U32* status;

    for (n = dig->n_samples,S = &dig->samples[0],status = &dig->sample_status[0]; n; --n,++S,++status)
    {
        if (*status == SMP_FREE) // ignore unallocated samples.
            continue;

        if (S->group_id != start_id)
            continue;

        if (op != 4)
            S->group_id = set_to_id;

        switch (op)
        {
        case 0: AIL_API_start_sample(S); break;
        case 1: AIL_API_stop_sample(S); break;
        case 2: AIL_API_resume_sample(S); break;
        case 3: AIL_API_end_sample(S); break;
        }
    }
}


//############################################################################
//##                                                                        ##
//## Terminate playback of sample, setting sample status to SMP_DONE        ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_end_sample(HSAMPLE S)
{
  if ( S == NULL )
  {
    return;
  }

  hardware_stop( S );

  //
  // Make sure sample has been allocated
  //

  if ( S->driver->sample_status[S->index] == SMP_FREE )
  {
    return;
  }

  hardware_spin_until_stopped( S );

  //
  // If sample is not already done, halt it and invoke its end-of-buffer
  // and end-of-sample callback functions
  //

  if ( S->driver->sample_status[S->index] != SMP_DONE )
  {
    //
    // Set 'done' status
    //

    S->driver->sample_status[S->index] = SMP_DONE;
    S->starved = 1;

    //
    // Call EOB and EOS functions
    //

    if ( S->EOB != NULL )
    {
      MSS_do_cb1( (AILSAMPLECB) ,S->EOB,S->driver->callingDS,
                   S);
    }

    if ( S->EOS != NULL )
    {
      MSS_do_cb1( (AILSAMPLECB) ,S->EOS,S->driver->callingDS,
                  S);
    }
  }
}


//############################################################################
//##                                                                        ##
//## Set start-of-block (SOB) callback function for sample                  ##
//##                                                                        ##
//## Callback function will be invoked prior to playback of each sample     ##
//## data block, whose position within the sample can be determined by      ##
//## calling AIL_sample_position()                                          ##
//##                                                                        ##
//## The rate at which this function is called is determined by the DMA     ##
//## half-buffer transition period for the driver in use; e.g., a 1K half-  ##
//## buffer being played at 22 kHz will trigger start-of-block callbacks    ##
//## at a rate of 22 per second                                             ##
//##                                                                        ##
//## This function returns the sample's previous SOB callback handler       ##
//## address, or NULL if no SOB callback handler was registered             ##
//##                                                                        ##
//############################################################################

AILSAMPLECB AILCALL AIL_API_register_SOB_callback( HSAMPLE S, AILSAMPLECB SOB )
{
  AILSAMPLECB old;

  if ( S == NULL )
  {
    return( 0 );
  }

  old = S->SOB;

  S->SOB = SOB;

  return( old );
}

//############################################################################
//##                                                                        ##
//## Set end-of-buffer (EOB) callback function for sample                   ##
//##                                                                        ##
//## Callback function will be invoked when either of two sample buffers    ##
//## finish playing                                                         ##
//##                                                                        ##
//## When not double-buffering, the end-of-sample (EOS) callback, if any,   ##
//## will be triggered immediately after the end of buffer 0                ##
//##                                                                        ##
//## This function returns the sample's previous EOB callback handler       ##
//## address, or NULL if no EOB callback handler was registered             ##
//##                                                                        ##
//############################################################################

AILSAMPLECB AILCALL AIL_API_register_EOB_callback( HSAMPLE S, AILSAMPLECB EOB )
{
  AILSAMPLECB old;

  if ( S == NULL )
  {
    return( 0 );
  }

  old = S->EOB;

  S->EOB = EOB;

  return( old );
}

//############################################################################
//##                                                                        ##
//## Set 3D distance falloff function callback for sample                   ##
//##                                                                        ##
//## This function returns the sample's previous falloff callback handler   ##
//## address, which will be the internal default handler if no previous     ##
//## user falloff function has been registered                              ##
//##                                                                        ##
//############################################################################

AILFALLOFFCB AILCALL AIL_API_register_falloff_function_callback( HSAMPLE S, AILFALLOFFCB falloff_cb )
{
  AILFALLOFFCB old;

  if ( S == NULL )
  {
    return( 0 );
  }

  if (falloff_cb == NULL)
   {
   falloff_cb = SS_default_falloff_function_callback;
   }

  old = S->S3D.falloff_function;

  S->S3D.falloff_function = falloff_cb;

  return( old );
}

//############################################################################
//##                                                                        ##
//## Set end-of-sample (EOS) callback function for sample                   ##
//##                                                                        ##
//## Callback function will be invoked when all valid sample buffers have   ##
//## finished playing                                                       ##
//##                                                                        ##
//## When not double-buffering, the end-of-sample (EOS) callback will be    ##
//## triggered immediately after the end of buffer 0                        ##
//##                                                                        ##
//## This function returns the sample's previous EOS callback handler       ##
//## address, or NULL if no EOS callback handler was registered             ##
//##                                                                        ##
//############################################################################

AILSAMPLECB AILCALL AIL_API_register_EOS_callback( HSAMPLE S, AILSAMPLECB EOS )
{
  AILSAMPLECB old;

  if ( S == NULL )
  {
    return( 0 );
  }

  old = S->EOS;

  S->EOS = EOS;

  return( old );
}

//############################################################################
//##                                                                        ##
//## Set sample user data value at specified index                          ##
//##                                                                        ##
//## Any desired value may be stored at one of eight user data words        ##
//## associated with a given SAMPLE                                         ##
//##                                                                        ##
//## Callback functions may access the user data array at interrupt time    ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_user_data( HSAMPLE S, U32 index, SINTa value )
{
  if ( S == NULL )
  {
    return;
  }

  S->user_data[ index ] = value;
}

//############################################################################
//##                                                                        ##
//## Get sample user data value at specified index                          ##
//##                                                                        ##
//## Any desired value may be stored at one of eight user data words        ##
//## associated with a given SAMPLE                                         ##
//##                                                                        ##
//## Callback functions may access the user data array at interrupt time    ##
//##                                                                        ##
//############################################################################

SINTa AILCALL AIL_API_sample_user_data( HSAMPLE S, U32 index )
{
  if ( S == NULL )
  {
    return 0;
  }

  return( S->user_data[ index ] );
}


//############################################################################
//##                                                                        ##
//## Return minimum buffer size for dual-buffer playback                    ##
//##                                                                        ##
//## Channel count must be stored in upper word if format has               ##
//## DIG_F_MULTICHANNEL_MASK bit set                                        ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_minimum_sample_buffer_size( HDIGDRIVER dig, //)
                                                S32        playback_rate,
                                                S32        format )
{
  S32 hw_nibbles_per_sample;
  S32 n;

  //
  // Get # of application nibbles per sample unit
  //

  S32 app_nibbles_per_sample = nibbles_per_sample( format );
  S32 app_bytes_per_sample = app_nibbles_per_sample / 2;
  if (app_bytes_per_sample == 0) app_bytes_per_sample = 1;

#ifdef MSS_VFLT_SUPPORTED
   if (dig->voice_filter != NULL)
      {
      n = 65536;     // Default value guaranteed to be sufficient if driver fails to implement minimum-size attribute

      AIL_output_filter_driver_property(dig,
                                        "Minimum sample buffer size",
                                        &n,0,0);

      return n - (n % app_bytes_per_sample);
      }
#endif

  //
  // Get # of hardware nibbles per sample unit
  // This will be used to determine the rate at which the (physical) hardware consumes the source (logical) data
  //

  hw_nibbles_per_sample = dig->logical_channels_per_sample * dig->bytes_per_channel * 2;

  //
  // Multiply size of hardware half-buffer by ratio of logical-to-physical
  // sample size
  //

  n = dig->hardware_buffer_size * app_nibbles_per_sample / hw_nibbles_per_sample ;

  //
  // Scale n by resampling ratio
  //

  n = n * playback_rate / dig->DMA_rate;

  //
  // Scale n by 2X resampling tolerance to provide safety margin
  //

  n = n + ( ( n * (S32) AIL_preference[ DIG_RESAMPLING_TOLERANCE ] ) / 32768 );

  //
  // If DMA rate is not 1X, 2X, or 4X times playback rate, round buffer
  // size up 1 sample to avoid possible truncation errors
  //

  if ( ( dig->DMA_rate != ( 1 * playback_rate ) ) &&
       ( dig->DMA_rate != ( 2 * playback_rate ) ) &&
       ( dig->DMA_rate != ( 4 * playback_rate ) ) )
  {
    n += 4;
  }

  //
  // Round n up to nearest multiple of 256 bytes
  //

  n = ( n + 255 ) & ~255;

  //
  // Return size *3 (*4 in Win32) to ensure enough data is
  // available for initial buffer fill
  //

#ifndef IS_DOS

#if defined(IS_WIN32)
  if ( AIL_preference[ DIG_USE_WAVEOUT ] )
  {
    n = n * 4;
  }
  else
#endif
#if defined(IS_WIN32)
  {
    // get the number of fragments to mix ahead
    S32 DS_frag_mix_ahead = (S32) AIL_preference[ DIG_DS_MIX_FRAGMENT_CNT ];
    if ( DS_frag_mix_ahead >= dig->DS_frag_cnt )
    {
      DS_frag_mix_ahead = dig->DS_frag_cnt - 1;
    }

    n = n * DS_frag_mix_ahead;
  }
#elif defined(IS_PSP) || defined (IS_MAC) || defined (IS_IPHONE) || defined(IS_WIIU)
    // get the number of fragments to mix ahead
    S32 DS_frag_mix_ahead = (S32) AIL_preference[ DIG_DS_MIX_FRAGMENT_CNT ];

    n = n * DS_frag_mix_ahead;
#elif defined(IS_PS3) || defined(IS_WII)
  {
    // get the number of fragments to mix ahead
    S32 DS_frag_mix_ahead = (S32) AIL_preference[ DIG_DS_MIX_FRAGMENT_CNT ];
    if ( DS_frag_mix_ahead >= dig->snd_frag_cnt )
    {
      DS_frag_mix_ahead = dig->snd_frag_cnt - 1;
    }

    n = n * DS_frag_mix_ahead;
  }
#else
  n = n * 3;
#endif
#endif

 //
 // Round down to sample-size multiple, so returned value is guaranteed to work
 // with oddball sample sizes like 6-channel PCM
 //

 return n - (n % app_bytes_per_sample);
}

//############################################################################
//#                                                                          #
//# Get # of buffer slots associated with an HSAMPLE                         #
//#                                                                          #
//############################################################################

S32 AILCALL AIL_API_sample_buffer_count(HSAMPLE S)
{
   if (S == NULL)
      {
      return 0;
      }

   return S->n_buffers;
} 

//############################################################################
//#                                                                          #
//# Set # of buffer slots associated with an HSAMPLE                         #
//#                                                                          #
//############################################################################

S32 AILCALL AIL_API_set_sample_buffer_count(HSAMPLE S,
                                            S32     n_buffers)
{
   if ((S == NULL) || (n_buffers < 2) || (n_buffers > 8) )
      {
      return 0;
      }

   S->n_buffers      =  n_buffers;
   S->tail           =  0;
   S->head           =  0;
   S->starved        =  1;
   S->exhaust_ASI    =  0;

   AIL_memset(S->buf, 0, S->n_buffers * sizeof(S->buf[0]));

   for (S32 i=0; i < S->n_buffers; i++)
      {
      S->buf[i].start          = NULL;
      S->buf[i].len            = 0;
      S->buf[i].pos            = 0;
      S->buf[i].done           = (i == 0) ? 0 : 1;    // By default, all buffers except the first are
      S->buf[i].reset_ASI      = 0;                   // marked 'done' so they won't be eligible for playback
      S->buf[i].reset_seek_pos = 0;                   // until AIL_load_sample_buffer() is called on them
      }

   return 1;
}

//############################################################################
//##                                                                        ##
//## Set address and length for a sample buffer                             ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_load_sample_buffer(HSAMPLE S, S32 buff_num, void const *buffer, U32 len)
{
   if ((S == NULL) || (buff_num >= S->n_buffers))
      {
      return -1;
      }

   if (buff_num == MSS_BUFFER_HEAD) 
      {
      buff_num = S->head;
      S->head = (buff_num + 1) % S->n_buffers;
      }

   if (buffer == NULL)
      {
      //
      // Remove this buffer # from the ring by moving all entries from buff_num+1 to tail
      // back down to buff_num, and decrementing head pointer
      //

      for (;;)
         {
         S32 next_buff = (buff_num + 1) % S->n_buffers;

         if (next_buff == S->tail)
            {
            break;
            }

         AIL_memmove(&S->buf[buff_num],
                     &S->buf[next_buff],
                     sizeof(S->buf[buff_num]));

         buff_num = next_buff;
         }

      S->head = (S->head == 0) ? S->n_buffers-1 : S->head-1;
      return buff_num;
      }

   S->starved = 0;

   S->buf[buff_num].done  = (len == 0);
   S->buf[buff_num].start = buffer;
   S->buf[buff_num].len   = len;
   S->buf[buff_num].pos   = 0;

   if (len)
      {
      if (S->driver->sample_status[S->index] != SMP_PLAYING)
         {
         //
         // Mark as double-buffered sample
         //

         S->service_type = SERVICE_STREAMED;
         }
      }

   return buff_num;
}

//############################################################################
//##                                                                        ##
//## Get buffer playback status for sample                                  ##
//##                                                                        ##
//## Nonzero: There's room in the queue for at least one buffer             ##
//## Zero: All buffers are already full                                     ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_sample_buffer_available( HSAMPLE S )
{
   if (S == NULL)
      {
      return -1;
      }

   //
   // head==tail under two possible conditions: all ring buffers are either full (i.e.,
   // not starved, no room for more), or all are empty (either uninitialized,
   // or starved when tail ran into head)
   //
   // If the ring is full, return 0
   // If the ring has room for at least one buffer, return 1
   //

   if ((S->head == S->tail) && (!S->starved))
      {
      return 0;
      }

   return 1;
}

S32 AILCALL AIL_API_sample_loaded_len( HSAMPLE S )
{
  S32 amt = 0;

  if ( S )
  {
    S32 t;
  
    t = S->tail;
    
    if ( ( t == S->head ) && ( !S->starved ) ) // we totally full?
      goto start_one_in;

    for(;;)
    {
      amt += ( S->buf[t].len - S->buf[t].pos );
      if ( t == S->head )
        break;
     start_one_in:   
      ++t;
      if ( t >= S->n_buffers )
        t = 0;
    }
  }

  return amt;
}

//############################################################################
//##                                                                        ##
//## Get information about status of streaming buffer pair                  ##
//##                                                                        ##
//############################################################################

S32  AILCALL AIL_API_sample_buffer_info( HSAMPLE   S, //)
                                         S32       buff_num,
                                         U32 * pos,
                                         U32 * len,
                                         S32 * head,
                                         S32 * tail)
{
   if ( S == 0 )
      {
      if ( pos ) *pos = 0;
      if ( len ) *len = 0;
      if ( head ) *head = -1;
      if ( tail ) *tail = -1;
      return( 0 );
      }

   if ( pos != NULL )
      {
      *pos = S->buf[ buff_num ].pos;
      }

   if ( len != NULL )
      {
      *len = S->buf[ buff_num ].len;
      }

   if ( head != NULL )
      {
      *head = S->head;
      }

   if ( tail != NULL )
      {
      *tail = S->tail;
      }

  return( S->starved );
}

//############################################################################
//##                                                                        ##
//## Get size and current position of sample in milliseconds                ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_sample_ms_position( HSAMPLE  S, //)
                                         S32 *total_milliseconds,
                                         S32 *current_milliseconds )
{
  U32 datarate;

  if ( S == NULL )
  {
    return;
  }

  // If it's an asi, and it has exact seek caps, just do it that way.
  if ( S->pipeline[SP_ASI_DECODER].active )
  {
      ASISTAGE *ASI = &S->pipeline[ SP_ASI_DECODER ].TYPE.ASI;
      if (ASI->EXACT_GETPOS)
      {
          InMilesMutex();
          if ( current_milliseconds )
          {
            U32 pos = S->buf[ S->tail ].pos | 0x80000000;  // 0x8000000 means this is the "current" position
            *current_milliseconds = -1;
            if ( ASI->ASI_stream_property(ASI->stream, ASI->EXACT_GETPOS, 0, &pos, current_milliseconds ) == 0 )
            {
              OutMilesMutex();
              goto old_way;
            }
          }

          if ( total_milliseconds )
          {
            U32 len = S->buf[ S->tail ].len;
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
  F32 rate_Hz = (F32) S->original_playback_rate * S->playback_rate_factor;

  S32 effective_rate = (S32) rate_Hz;

  //
  // Get data rate in bytes per second
  //

  if ( S->format & DIG_F_ADPCM_MASK )
  {
    //
    // ADPCM compression
    //

    U32 samples_per_block = 4 << ( ( S->format & DIG_F_STEREO_MASK ) ? 1 : 0 );
    samples_per_block = 1 + ( S->adpcm.blocksize - samples_per_block ) * 8 / samples_per_block;

    datarate = ( effective_rate * S->adpcm.blocksize ) / samples_per_block;
  }
  else
  {
    if ( S->pipeline[ SP_ASI_DECODER ].active )
    {
      //
      // ASI compression
      //

      ASISTAGE *ASI = &S->pipeline[ SP_ASI_DECODER ].TYPE.ASI;

      datarate = (U32)~0; ASI->ASI_stream_property( ASI->stream,
                                                    ASI->INPUT_BIT_RATE, &datarate,0,0 );
      datarate /= 8;
    }
    else
    {
      //
      // Straight PCM
      //

      datarate = ( effective_rate * nibbles_per_sample( S->format | (S->n_channels << 16)) ) / 2;
    }
  }

  //
  // Return requested values
  //

  if ( total_milliseconds )
  {
    *total_milliseconds = (S32) ( ( (float) S->buf[ S->tail ].len * 1000.0F ) / (float) datarate );
  }

  if ( current_milliseconds )
  {
    *current_milliseconds = (S32) ( ( (float) S->buf[ S->tail ].pos * 1000.0F ) / (float) datarate );
  }
}

//############################################################################
//##                                                                        ##
//## Request the data offset for a given ms.                                ##
//##                                                                        ##
//############################################################################

U32 AILCALL AIL_API_sample_ms_lookup(HSAMPLE S, S32 milliseconds, S32* actualms)
{
    if (S == 0)
        return ~0U;

    // If it's an asi, use the lookup
    if (S->pipeline[SP_ASI_DECODER].active)
    {
        ASISTAGE *ASI = &S->pipeline[ SP_ASI_DECODER ].TYPE.ASI;
        if (ASI->SEEK_LOOKUP)
        {
            struct
            {
                U32 datapos;
                S32 actualms;
            } output;

            if (ASI->ASI_stream_property(ASI->stream, ASI->SEEK_LOOKUP, 0, &milliseconds, &output) == 1)
            {
                // success
                if (actualms)
                    *actualms = output.actualms;
                return output.datapos;
            }
        }
    }

    // none
    F32 rate_Hz = (F32) S->original_playback_rate * S->playback_rate_factor;

    S32 effective_rate = (S32) rate_Hz;

    U32 datarate = 0;

    //
    // Get data rate in bytes per second
    //

    if ( S->format & DIG_F_ADPCM_MASK )
    {
        //
        // ADPCM compression
        //

        U32 samples_per_block = 4 << ( ( S->format & DIG_F_STEREO_MASK ) ? 1 : 0 );
        samples_per_block = 1 + ( S->adpcm.blocksize - samples_per_block ) * 8 / samples_per_block;

        datarate = ( effective_rate * S->adpcm.blocksize ) / samples_per_block;
    }
    else
    {
        if ( S->pipeline[ SP_ASI_DECODER ].active )
        {
            //
            // ASI compression
            //

            ASISTAGE *ASI = &S->pipeline[ SP_ASI_DECODER ].TYPE.ASI;

            datarate = (U32)~0; ASI->ASI_stream_property( ASI->stream,
                ASI->INPUT_BIT_RATE, &datarate,0,0 );
            datarate /= 8;
        }
        else
        {
            //
            // Straight PCM
            //

            datarate = ( effective_rate * nibbles_per_sample( S->format | (S->n_channels << 16)) ) / 2;
        }
    }

    //
    // Set requested position
    //
    if (actualms)
        *actualms = milliseconds;

    return (U32)(S32) ( ( (float) datarate * (float) milliseconds ) / 1000.0F );
}

//############################################################################
//##                                                                        ##
//## Seek to a specified millisecond within a sample                        ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_set_sample_ms_position( HSAMPLE S, //)
                                             S32     milliseconds )
{
  U32 datarate;

  if ( S == NULL )
  {
    return;
  }

  // If it's an asi, and it has exact seek caps, just do it that way.
  if (S->pipeline[SP_ASI_DECODER].active)
  {
      ASISTAGE *ASI = &S->pipeline[ SP_ASI_DECODER ].TYPE.ASI;
      if (ASI->EXACT_SEEK)
      {
          U32 seek_pos = 0;
          if (ASI->ASI_stream_property(ASI->stream, ASI->EXACT_SEEK, 0, &milliseconds, &seek_pos) == 1)
          {
              // success
              AIL_set_sample_position( S, seek_pos );
              return;
          }
      }
  }

  F32 rate_Hz = (F32) S->original_playback_rate * S->playback_rate_factor;

  S32 effective_rate = (S32) rate_Hz;

  //
  // Get data rate in bytes per second
  //

  if ( S->format & DIG_F_ADPCM_MASK )
  {
    //
    // ADPCM compression
    //

    U32 samples_per_block = 4 << ( ( S->format & DIG_F_STEREO_MASK ) ? 1 : 0 );
    samples_per_block = 1 + ( S->adpcm.blocksize - samples_per_block ) * 8 / samples_per_block;

    datarate = ( effective_rate * S->adpcm.blocksize ) / samples_per_block;
  }
  else
  {
    if ( S->pipeline[ SP_ASI_DECODER ].active )
    {
      //
      // ASI compression
      //

      ASISTAGE *ASI = &S->pipeline[ SP_ASI_DECODER ].TYPE.ASI;

      datarate = (U32)~0; ASI->ASI_stream_property( ASI->stream,
                                                    ASI->INPUT_BIT_RATE, &datarate,0,0 );
      datarate /= 8;
    }
    else
    {
      //
      // Straight PCM
      //

      datarate = ( effective_rate * nibbles_per_sample( S->format | (S->n_channels << 16)) ) / 2;
    }
  }

  //
  // Set requested position
  //

  AIL_set_sample_position( S, (U32)(S32) ( ( (float) datarate * (float) milliseconds ) / 1000.0F ) );
}

//############################################################################
//##                                                                        ##
//## ASI callback routine to fetch encoded source data from sample          ##
//##                                                                        ##
//## Used by both MSSWO.CPP and MSSDS.CPP                                   ##
//##                                                                        ##
//############################################################################

static S32 AILCALLBACK SP_ASI_DECODER_callback(UINTa     user, //)
                                               void *dest,
                                               S32       bytes_requested,
                                               S32       offset)
{
   HSAMPLE S = (HSAMPLE) user;

   S32 total = 0;

   S32 n = S->tail;

   if (offset > (S32) S->buf[n].len)
      {
      offset = S->buf[n].len;     // Needed to make XMA ASI work with double-buffers whose len < the total RIFF len
      }

   if (offset != -1)
      {
      S->buf[n].pos = offset;
      }

   if (S->exhaust_ASI)
      {
      //
      // We decremented S->pos by one byte in SS_merge() to keep it from reaching S->len
      // until all buffered data was retrieved from the ASI codec.  Now, since we're in the decoder
      // callback, all of the buffered output data has been used up and it's time to fetch
      // some more source data if possible.  Undo the decrement here to let the sample
      // terminate normally, or switch its buffers, without feeding the byte at len-1 to the ASI codec.
      //

      S->exhaust_ASI = 0;
      S->buf[n].pos = S->buf[n].len;
      }

   //
   // Fulfill as much of request as possible from current buffer
   //

   S32 amount = bytes_requested;

   //
   // Cap input to the looping input if we aren't streaming.
   //
   U32 in_limit = S->buf[n].len;

   if (S->service_type == SERVICE_SINGLE_BUFFER && S->loop_count != 1)
   {
       if ((S->loop_end != -1) && ((U32) S->loop_end < S->buf[S->tail].len))
       {
        in_limit = S->loop_end;
       }

       if (S->buf[n].pos >= in_limit)
           return 0;
   }

   if ((S->buf[n].pos + amount) > in_limit)
      {
      amount = in_limit - S->buf[n].pos;
      }



   AIL_memcpy(dest,
              AIL_ptr_add(S->buf[n].start, S->buf[n].pos),
              amount);

   dest = AIL_ptr_add(dest, amount);

   S->buf[n].pos += amount;

   total += amount;
   bytes_requested -= amount;

   //
   // If end of buffer reached with samples left to fetch, try to
   // switch buffers or execute a loop iteration
   //

   while (bytes_requested > 0)
      {
      if (S->EOB != NULL)
         ++S->doeob;

      if (S->service_type != SERVICE_STREAMED)
         {
         break;
         }
      else
         {
         // reset the ASI, if requested
         if (S->buf[n].reset_ASI)
         {
           // return without switching buffers if we were requested to do a reset
           return(total);
         }

         n = (n + 1) % S->n_buffers;

         //
         // If explicit 0-length buffer was posted, fall through to allow
         // the sample to terminate
         //

         if (S->buf[n].done)
            {
            return total;
            }
         else
            {
            //
            // If alternate buffer not yet initialized, break out of loop
            // to allow application to supply data as soon as it can
            //

            if ((n == S->head) || (!S->buf[n].len))
               {
               S->starved = 1;
               return total;
               }

            //
            // If alternate buffer already played, break out of loop to
            // allow application to refresh it as soon as possible
            //

            if (S->buf[n].pos)
               {
               S->starved = 1;
               return total;
               }

            //
            // Otherwise, alternate buffer is ready to play -- switch to
            // it and keep filling output buffer
            //

            S->tail = n;

            amount = bytes_requested;

            if ((S->buf[n].pos + amount) > S->buf[n].len)
               {
               amount = S->buf[n].len - S->buf[n].pos;
               }

            AIL_memcpy(dest,
                       AIL_ptr_add(S->buf[n].start, S->buf[n].pos),
                       amount);

            dest = AIL_ptr_add(dest, amount);

            S->buf[n].pos += amount;

            total += amount;
            bytes_requested -= amount;
            }
         }
      }

   return total;
}

#endif

#include "mc_mix.inl"

//############################################################################
//#                                                                          #
//# Add audio data to build and reverb buffers with optional level scaling   #
//#                                                                          #
//# This routine is aware of reverb and multichannel build buffers, but does #
//# not do resampling or format conversion like the "real" mixer             #
//#                                                                          #
//# It can replicate mono data to a stereo build buffer, but this capability #
//# is not currently needed                                                  #
//#                                                                          #
//############################################################################

//#define DEBUGSIMDMIXERS
#ifdef DEBUGSIMDMIXERS
static RAD_ALIGN(S32, testdest[64*1024], 16);

#ifdef __RADFINAL__
#error Oops, you have DEBUGSIMDMIXERS on!
#endif

#endif


//#define SHOWMIXVOLS
#if defined(SHOWMIXVOLS) && defined(IS_WIN32)
#include <stdio.h>

#ifdef __RADFINAL__
#error Oops, you have SHOWMIXVOLS on!
#endif

static S32 vols[ MAX_SPEAKERS ][ MAX_SPEAKERS ] = {-1 };

#endif

#ifdef NTELEMETRY
#define bump_count( n, s )
#else
#define bump_count( n, s ) { if ( miles_context ) { if ( s != S ) { s = S; ++n; } } }
#endif

static U32 MC_mix (HSAMPLE                  S,         //)
                   S16    * MSSRESTRICT ssrc,
                   S32                      src_bytes,
                   MSS_BB *             dest_buffers,
                   S32                      n_dest_buffers,
                   S32    * MSSRESTRICT reverb_dest,
                   S32                      reverb_channel_scale,
                   S32                      dest_offset,
                   S32                      src_stereo,
                   S32                      MC_source_channel)
{
  void * src;
  void * MSSRESTRICT src_end;
  void * dest;
  void * MSSRESTRICT dest_end;
  S32 i;
  S32 sl, sr;
  S32 op;
  U32 mixed=0;

  #ifdef _DEBUG
  // we baked the exact enums into the code in mc_mix.inl
  typedef char assert1[ (M_SRC_STEREO==8)?1:-1 ];
  typedef char assert2[ (M_DEST_STEREO==1)?1:-1 ];
  if ( src_stereo & ~1 ) MSSBreakPoint(); // should just be a flag
  #endif

  src = ssrc;
  src_end = AIL_ptr_add( ssrc, src_bytes );

  if ( reverb_dest )
  {
    if ( reverb_channel_scale )
    {
      sl = sr = reverb_channel_scale;

      // reverb buffer is always mono, and the dest_offset is in terms of the build_buffer[0] which might be stereo
      if ( dest_buffers[0].chans > 1 )
        dest = AIL_ptr_add( reverb_dest, dest_offset >> 1 );
      else
        dest = AIL_ptr_add( reverb_dest, dest_offset );

      dest_end = AIL_ptr_add( reverb_dest, S->driver->reverb[0].reverb_fragment_size );

      // this is called to align the buffer (not to ramp - it can do both)
      op = slow_ramp_mix( src_stereo, 0, (void**)&src, src_end, (void**)&dest, dest_end, &sl, &sr, 0, 0, 0 );

      #ifdef _DEBUG
        if ((UINTa)dest & 0xf) MSSBreakPoint();
      #endif

      bump_count( S->driver->rev_num, S->driver->rev_sam );
      tmEnterAccumulationZone( miles_context, S->driver->rev_time );
      CALL_MIXER_MERGE(op, S, (void**)&src, src_end, (void**)&dest, dest_end, reverb_channel_scale, reverb_channel_scale)
      tmLeaveAccumulationZone( miles_context, S->driver->rev_time );
    }
  }

  S32 src_samples = ( src_stereo ) ? ( src_bytes >> 2 ) : ( src_bytes >> 1 );

  #if defined(SHOWMIXVOLS) && defined(IS_WIN32)
    S32 print=0;
    for ( i = 0 ; i < n_dest_buffers ; i++ )
    {
      if (( vols[i*2][MC_source_channel]!=S->prev_scale[i*2][MC_source_channel] ) ||( vols[i*2+1][MC_source_channel]!=S->prev_scale[i*2+1][MC_source_channel] ))
        print=1;
      vols[i*2][MC_source_channel]=S->prev_scale[i*2][MC_source_channel];
      vols[i*2+1][MC_source_channel]=S->prev_scale[i*2+1][MC_source_channel];
    }
    if( print )
      printf("input: %d %d - vols %d: ", (S32)S, MC_source_channel ,n_dest_buffers );
  #endif

  for ( i = 0 ; i < n_dest_buffers ; i++ )
  {
    S32 left, right, oleft, oright;

    src = ssrc;

    // is this a mono remnant, then adjust
    if ( dest_buffers[i].chans < dest_buffers[0].chans )
      dest = AIL_ptr_add( dest_buffers[i].buffer, dest_offset >> 1 );
    else
      dest = AIL_ptr_add( dest_buffers[i].buffer, dest_offset );

    dest_end = AIL_ptr_add( dest_buffers[i].buffer, dest_buffers[i].bytes );

    U32 dest_stereo = ( dest_buffers[ i ].chans >= 2 ) ? 1 : 0;
    S32 dest_samples = (S32) ( AIL_ptr_dif( dest_end, dest ) >> ( dest_stereo + 2 ) ); // +2 == S32 size

    left   = S->cur_scale [ i * 2     ][MC_source_channel];
    oleft  = S->prev_scale[ i * 2     ][MC_source_channel];
    right  = S->cur_scale [ i * 2 + 1 ][MC_source_channel];
    oright = S->prev_scale[ i * 2 + 1 ][MC_source_channel];

    if ( ( ( left != oleft ) || ( right != oright ) ) && ( S->ramps_left[i][MC_source_channel] == 0 ) )
    {
      // volume has changed and we aren't ramping, so start a new ramp period
      #ifdef DEBUGSIMDMIXERS
        S->ramps_left[i][MC_source_channel]=0;
      #else
        S->ramps_left[i][MC_source_channel]=(S32) S->vol_ramps;
      #endif
    }

    #ifdef DEBUGSIMDMIXERS
      AIL_memcpy( testdest, dest_buffers[i].buffer, AIL_ptr_dif( dest_end, dest_buffers[i].buffer ) );

      S32* rawdest = (S32*)dest_buffers[i].buffer;
      void* rawsrc = (S32*)src;
      S16* testsrc = (S16*)rawsrc;
      S32* localdest;
    #endif

    sl = oleft;
    sr = oright;

    S32 r = S->ramps_left[i][MC_source_channel];
    if ( r > src_samples )
      r = src_samples;
    if ( r > dest_samples )
      r = dest_samples;

    op = slow_ramp_mix( src_stereo, dest_stereo,
                        (void**)&src, src_end,
                        (void**)&dest, dest_end,
                        &sl, &sr, left-oleft, right-oright, r );

    S->ramps_left[i][MC_source_channel] -= r;
    if (S->ramps_left[i][MC_source_channel])
    {
      //
      // We may not have made it all the way to cur_scale in this sub-interval, so
      // keep track of how far we got to avoid discontinuities in the next call
      //
      S->cur_scale[ i * 2     ][MC_source_channel] = sl;
      S->cur_scale[ i * 2 + 1 ][MC_source_channel] = sr;
    }
    else
    {
      // The ramp is complete. If we are supposed to fade to stop, mark us ready to do so.
      if (S->fade_to_stop)
        S->fade_to_stop = 2;
    }

    if ( ( src != src_end ) && ( dest != dest_end ) )
    {
      #ifdef _DEBUG
        if ((UINTa)dest & 15 ) MSSBreakPoint();
      #endif

      if ( ( left | right ) == 0 )
      {
         S32 d_samples = (S32) ( AIL_ptr_dif( dest_end, dest ) >> ( dest_stereo + 2 ) );

         S32 s_samples = (S32) ( AIL_ptr_dif( src_end, src ) >> (src_stereo+1) ); // +1 == sizeof(S16)
         if ( d_samples > s_samples )
           d_samples = s_samples;

         dest = AIL_ptr_add( dest, d_samples << ( dest_stereo + 2 ) );
         #ifdef DEBUGSIMDMIXERS
           src = AIL_ptr_add( src, d_samples << (src_stereo+1) );
         #endif
      }
      else
      {
        F32 t = ( (F32) left ) * ( 1.0f / 2048.f );
        if ( t > S->max_output_mix_volume )
           S->max_output_mix_volume = t;
        t = ( (F32) right ) * ( 1.0f / 2048.f );
        if ( t > S->max_output_mix_volume )
           S->max_output_mix_volume = t;
           
        bump_count( S->driver->mix_num, S->driver->mix_sam );
        tmEnterAccumulationZone( miles_context, S->driver->mix_time );
        CALL_MIXER_MERGE(op, S, (void**)&src, src_end, (void**)&dest, dest_end, left, right)
        tmLeaveAccumulationZone( miles_context, S->driver->mix_time );

        if ( ( src != src_end ) && ( dest != dest_end ) )
        {
          S32 lrtemp = 0;
          U32 temp = 0;

          tmEnterAccumulationZone( miles_context, S->driver->mix_time );
          S->pipeline[SP_MERGE].TYPE.MSS_mixer_merge (
           (void const **) &src,
           &temp,
           src_end,
           (S32 * *)(void*)&dest,
           dest_end,
           &lrtemp,
           &lrtemp,
           65536,
           left,
           right,
           op|M_VOL_SCALING
           #ifdef IS_X86
             ,S->driver->use_MMX
           #endif
           );
           tmLeaveAccumulationZone( miles_context, S->driver->mix_time );
        }
      }

      #ifdef DEBUGSIMDMIXERS
      {
        if ( dest_buffers[i].chans < dest_buffers[0].chans )
          localdest = (S32*)AIL_ptr_add( testdest, dest_offset>>1 );
        else
          localdest = (S32*)AIL_ptr_add( testdest, dest_offset );


        S32 lrtemptest = 0;
        U32 temptest = 0;

        S->pipeline[SP_MERGE].TYPE.MSS_mixer_merge(
          (void const **)(void*) &testsrc,
          &temptest,
          src_end,
          (S32 * *)(void*)&localdest,
          AIL_ptr_add(testdest,AIL_ptr_dif( dest_end, rawdest)),
          &lrtemptest,
          &lrtemptest,
          65536,
          left,
          right,
          op|M_VOL_SCALING
          #ifdef IS_X86
            ,S->driver->use_MMX
          #endif
        );

        if ( AIL_ptr_dif( testsrc, rawsrc ) != AIL_ptr_dif( src, rawsrc ) )
          MSSBreakPoint();

        if ( AIL_ptr_dif( localdest, testdest ) != AIL_ptr_dif( dest, rawdest ) )
          MSSBreakPoint();

        for (SINTa q=0; q<(AIL_ptr_dif( dest, rawdest)/4); q++)
        {
            int diff = rawdest[q] - testdest[q];
            if ( diff < 0 ) diff = -diff;

            if (diff > 3)
            {
                MSSBreakPoint();
            } 
        }
      }
      #endif

    }

    #if defined(SHOWMIXVOLS) && defined(IS_WIN32)
      if( print )
        printf("l:%d r:%d  ",left,right);
    #endif

    if ( i == 0 )
    {
      mixed = (U32) AIL_ptr_dif( dest, dest_buffers[0].buffer );
    }
  }

  #if defined(SHOWMIXVOLS) && defined(IS_WIN32)
    if( print )
      printf("\n");
  #endif

  return( mixed );
}



#ifdef MSS_FLT_SUPPORTED

#ifdef MSS_SPU_PROCESS
  static void call_spu_filter( int index,
                               HSAMPLESTATE state,
                               void * MSSRESTRICT  source_buffer,
                               void * MSSRESTRICT  dest_buffer,
                               S32          n_samples,
                               MSS_SPEAKER  channel);
#endif

//############################################################################
//#                                                                          #
//# Apply filter chain to data at specified location in build buffer FLT_A   #
//#                                                                          #
//############################################################################

void * SS_execute_filter_chain(HSAMPLE  S, //)
                                   void * src,
                                   void * temp_dest,
                                   S32      src_bytes,
                                   S32      MC_chan,
                                   S16 *dest_mono_sample_buffer)
{
   addstackmark();
   S32 n_samples = (S32) ( src_bytes / sizeof(S16) );

   //
   // Build linear list of filters to execute
   //

   SAMPLESTAGE stage,filter_list[8];
   S32 i,n = 0;

   for (stage = SP_FILTER_0; stage <= SP_FILTER_7; stage = (SAMPLESTAGE) (stage + 1))
      {
      if (S->pipeline[stage].active)
         {
         filter_list[n++] = stage;
         }
      }

   if (n == 0)
      {
      return 0;     // Shouldn't happen (data won't get copied to the output buffer)
      }

   //
   // Find the intended speaker for this source channel so we can
   // pass it to the filters
   //
   // (This doesn't take into account channel steering that might direct the
   // source channel into another output channel altogether)
   //
   //

   MSS_SPEAKER src_channel = MSS_SPEAKER_FRONT_LEFT;

   for (i=0; i < MSS_SPEAKER_MAX_INDEX; i++)
      {
      if (S->speaker_enum_to_source_chan[i] == MC_chan)
         {
         src_channel = (MSS_SPEAKER) i;
         break;
         }
      }

   //
   // Execute all filters in list
   //
   // Don't apply volume scaling here -- it will be applied by a later MC_mix() call, or by the voice output
   // filter itself
   //

   for ( i = 0; i < n; )
      {
      stage = filter_list[i];

      FLTPROVIDER *F = S->pipeline[stage].TYPE.FLT.provider;
      HSAMPLESTATE state = S->pipeline[stage].TYPE.FLT.sample_state[MC_chan];

#ifdef MSS_SPU_PROCESS
      void * fpbuf = spu_alloc( 16 + SPU_EXTRA );
      F = (FLTPROVIDER*) align_memcpy_from_PPU( fpbuf, F, 16, -1 ); // we only need the first couple of words
      void * stbuf = spu_alloc( F->sample_size + SPU_EXTRA );
      state = (HSAMPLESTATE) align_memcpy_from_PPU( stbuf, (void*)state, F->sample_size, -1 );
#endif

      ++i;

      // save out new dest in the return pointer
      void * dest = ( ( dest_mono_sample_buffer ) && ( i == n ) ) ? dest_mono_sample_buffer : temp_dest;

      // if we are using an output filter and we are on the last buffer, filter right into
      //   the dest_mono_sample_buffer
#ifdef MSS_SPU_PROCESS
      call_spu_filter( FPROV_SPU_INDEX( F->provider_flags ) - 1,
#else
      F->sample_process(
#endif
                        state,
                        src,
                        dest,        // usually will be the same value as src
                        n_samples,
                        src_channel );


#ifdef MSS_SPU_PROCESS
      RAD_memcpy_to_PPU( (void*) S->pipeline[stage].TYPE.FLT.sample_state[MC_chan], (void*)state, F->sample_size, -1 );
#endif

      // future versions of the filters should be able to signal a zero output signal (to save mixing)
      src = dest;
      }

   return src;
}

#endif

//############################################################################
//##                                                                        ##
//## Flush mixer buffer                                                     ##
//##                                                                        ##
//############################################################################


void AILCALL SS_flush (HDIGDRIVER dig, S32 reverb_index)
{
   //
   // Initialize the build buffer by flushing with 0
   //

   for (S32 i=0; i < dig->n_build_buffers; i++)
      {
      dig->pipeline[DP_FLUSH].TYPE.MSS_mixer_flush(dig->build[i].buffer,
                                                   dig->build[i].bytes
#ifdef IS_X86
                                                  ,dig->use_MMX
#endif
                                                   );
      }
/*
#ifdef MSS_FLT_SUPPORTED
      dig->pipeline[DP_FLUSH].TYPE.MSS_mixer_flush(dig->build[FLT_A].buffer,
                                                   dig->build[FLT_A].bytes
#ifdef IS_X86
                                                  ,dig->use_MMX
#endif
                                                   );

      dig->pipeline[DP_FLUSH].TYPE.MSS_mixer_flush(dig->build[FLT_B].buffer,
                                                   dig->build[FLT_B].bytes
#ifdef IS_X86
                                                  ,dig->use_MMX
#endif
                                                   );

#endif
*/
   if (reverb_index < MAX_BUSSES)
   {
       if ( dig->reverb[reverb_index].reverb_on )
       {
           {
               // clear the reverb buffer
               dig->pipeline[DP_FLUSH].TYPE.MSS_mixer_flush((S32 *)AIL_ptr_add( dig->reverb[reverb_index].reverb_build_buffer, dig->reverb[reverb_index].reverb_head ),
                   dig->reverb[reverb_index].reverb_fragment_size
#ifdef IS_X86
                   ,dig->use_MMX
#endif
                   );
           }

           if ( ( (U32) ( dig->num_mixes - dig->reverb[reverb_index].reverb_off_time_ms ) * dig->mix_ms ) > ( ( dig->reverb[reverb_index].reverb_duration_ms + 100 ) * 256 ) )
           {
#ifdef MSS_SPU_PROCESS
               reset_reverb_head( dig );
#endif
               dig->reverb[reverb_index].reverb_on = 0;
           }

       }
   }
#ifdef MSS_FLT_SUPPORTED

   //
   // Invoke installable filters
   //

#ifndef MSS_SPU_PROCESS
   if (reverb_index == MAX_BUSSES)
      FLT_call_premix_processors(dig);
#endif

#endif
}

#if defined( MSS_SPU_PROCESS) || defined( HOST_SPU_PROCESS )

typedef struct STR_header
{
  S64 emitted_sample_count;        // # of mono or stereo 16-bit PCM samples returned to caller
  U8  *frame_buffer;
  S16 *samples;                // points into frame_buffer
  S32 frame_size;                  // Size of output frame in bytes
  S32 output_cursor;               // Next location from which to copy output data
} STR_header;

#endif

#ifdef MSS_SPU_PROCESS

  #define SPU_INPUT_SIZE 1024

  static SPU_MEM_STREAM sms_input;
  static void * spu_input;

  static STR_header * spu_stream_ptr;
  static U8 spu_stream[ 32 + SPU_EXTRA ];

  static S32 get_mem_from_ppu( U8 ** process_buffer, S32 src_adj, S32 nch, S32 amt )
  {
    U8* outptr;
    S16 b0, b1,b2,b3,b4,b5,b6,b7;

    b7 = ((S16*)((*process_buffer)+src_adj))[-8];
    b6 = ((S16*)((*process_buffer)+src_adj))[-7];
    b5 = ((S16*)((*process_buffer)+src_adj))[-6];
    b4 = ((S16*)((*process_buffer)+src_adj))[-5];
    b3 = ((S16*)((*process_buffer)+src_adj))[-4];
    b2 = ((S16*)((*process_buffer)+src_adj))[-3];
    b0 = ((S16*)((*process_buffer)+src_adj))[-2];
    b1 = ((S16*)((*process_buffer)+src_adj))[-1];

    S32 avail = spu_stream_ptr->frame_size - spu_stream_ptr->output_cursor;
    if ( avail > amt )
      avail = amt;

    outptr = (U8*) align_memcpy_from_PPU( ( (*process_buffer) + 16 ), spu_stream_ptr->frame_buffer + spu_stream_ptr->output_cursor, avail, -1 );

    spu_stream_ptr->emitted_sample_count += (avail >> nch);
    spu_stream_ptr->output_cursor += avail;

    ((S16*)outptr)[-8] = b7;
    ((S16*)outptr)[-7] = b6;
    ((S16*)outptr)[-6] = b5;
    ((S16*)outptr)[-5] = b4;
    ((S16*)outptr)[-4] = b3;
    ((S16*)outptr)[-3] = b2;
    ((S16*)outptr)[-2] = b0;
    ((S16*)outptr)[-1] = b1;
    
    *process_buffer = ( outptr - src_adj );

    return avail;     
  }

#endif

//############################################################################
//##                                                                        ##
//## Merge source data into output buffer by calling processor and mixer    ##
//## handlers                                                               ##
//##                                                                        ##
//## Optionally, processed source data may be downconverted to mono         ##
//## and written to a specified 16-bit destination buffer for the benefit   ##
//## of per-sample voice filters.  Reverb, volume-scaling, and resampling   ##
//## are unsupported by this path.                                          ##
//##                                                                        ##
//############################################################################

static 
S32 SS_merge (HSAMPLE  S, //)
              S32      in_len,
              S32      out_pos,
              S16 *dest_mono_sample_buffer,
              S32      dest_buffer_size)
{
#ifdef MSS_SPU_PROCESS
  addstackmark();
  char * tmp_spu_buf = 0;
  char * tmp_mc_buffer = 0;
  SPU_MEM_STREAM * sms = &sms_input;
#endif

   //
   // Set mixer operation code
   //
   // Multichannel or filtered sources are handled with multiple mono-source passes
   //

#ifdef _DEBUG
  if ( out_pos > dest_buffer_size ) MSSBreakPoint();
#endif

   S32 op                = 0;
   S32 src_sample_bytes  = 1;
   S32 dest_sample_shift = 0;

   HDIGDRIVER dig = S->driver;

   S32 filters_enabled = 0;

#ifdef MSS_FLT_SUPPORTED
   if (S->n_active_filters > 0)
   {
     filters_enabled = 1;
   }
#endif

   if ( S->lp_any_on )
   {
     filters_enabled = 1;
   }

   S32 originally_16bit = ((S->format & DIG_F_16BITS_MASK) || (S->format & DIG_F_ADPCM_MASK) || (S->pipeline[SP_ASI_DECODER].active) ) ? 1 : 0;

   S32 need_deinterlace = ((S->format & DIG_F_MULTICHANNEL_MASK) ||
                           ((S->format & DIG_F_STEREO_MASK) &&
                            ((S->is_3D) || (filters_enabled) ||
                             (S->user_channels_need_deinterlace)) &&
                            (dest_mono_sample_buffer == 0 )
                           )
                          );

   if (dest_mono_sample_buffer == NULL)
   {
      if (dig->hw_format & (DIG_F_STEREO_MASK | DIG_F_MULTICHANNEL_MASK)) // stereo or MC output
      {
         if ((S->format & DIG_F_STEREO_MASK) && (!need_deinterlace) && (!filters_enabled))
         {
            op |= M_DEST_STEREO;                                           // to stereo build buffer(s)
         }
      }
   }

   if (need_deinterlace)
   {
      op |= M_SRC_16;                     // We mix multichannel sources as a set of discrete mono channels

      if ( originally_16bit )
        src_sample_bytes = S->n_channels * sizeof(S16);
      else
        src_sample_bytes = 2 * sizeof(U8);  // only stereo 8-bit files are deinterlaced
   }
   else
   {
      if (S->format & DIG_F_STEREO_MASK)
      {
         op |= M_SRC_STEREO;
         src_sample_bytes *= 2;
      }

      if ( originally_16bit )
      {
         op |= M_SRC_16;
         src_sample_bytes *= 2;
      }
   }

   //
   // Calculate sampling fraction
   //
   // If a voice filter is in use, we must set driver->DMA_rate to whatever the current
   // HSAMPLE's playback rate is, so filters can do their Nyquist calculations properly
   //

   if (dest_mono_sample_buffer != NULL)
   {
      dig->DMA_rate = (S32) ((F32) S->original_playback_rate * S->playback_rate_factor * S->extra_rate);
   }

   S32 playback_ratio;

   if (dest_mono_sample_buffer != NULL)
   {
      playback_ratio = 65536;
      S->src_fract   = 0;
   }
   else
   {
      F32 rate_shift = S->playback_rate_factor * S->extra_rate;

      if (S->is_3D && S->S3D.doppler_valid)
      {
         rate_shift *= S->S3D.doppler_shift;
      }

      playback_ratio = (S32) ((((F32) S->original_playback_rate) * rate_shift * 65536.0F) /
                                (F32) dig->DMA_rate);

      if ((playback_ratio > (65536 + S->resamp_tolerance)) ||
          (playback_ratio < (65536 - S->resamp_tolerance)))
      {
         op |= M_RESAMPLE;

         //
         // Enable filtering if preference set
         //

         if (S->enable_resamp_filter)
         {
            op |= M_FILTER;
         }
      }
      else
      {
         playback_ratio = 65536;
         S->src_fract   = 0x20000000;  //flag to say last buffer was non-filter
      }

      // don't go to infinitely slow (allows a 1 to 2048 slowdown)
      if ( playback_ratio < 32 )
        playback_ratio = 32;
   }

   //
   // Calculate # of bytes needed for this loop
   //
   // This calculation takes resampling into account, as well as the
   // difference in sample sizes between the output of the decode stage
   // ("source sample") and the mixer build buffer.  The resulting sample
   // count reflects the amount of mixer input data needed to fill the build buffer
   // from its current out_pos write position.
   //

   S32 needed_dest = dest_buffer_size - out_pos;  // in bytes!


   if (dest_mono_sample_buffer == 0)
   {
      // dest is either 2 S32 for stereo, or 1 S32 for mono
      if ( dig->build[0].chans >= 2)
         dest_sample_shift = 3;
      else
         dest_sample_shift = 2;

      // clamp if we are resampling
      if ( ( op & M_RESAMPLE ) && ( need_deinterlace ) )
      {
        // Cap the dest in case we can't fit our pre-resampling data into the working buffer.
        S32 max_deinterlace_samples = dig->samples_per_buffer;

        max_deinterlace_samples = shift64addsubanddiv( max_deinterlace_samples,
                                                       16,
                                                       65536 - ( S->src_fract & 0xffff ),
                                                       playback_ratio >> 1,
                                                       playback_ratio );

        if ( ( needed_dest >> dest_sample_shift ) > max_deinterlace_samples )
           needed_dest = max_deinterlace_samples << dest_sample_shift;
      }
   }
   else
   {
     // if we have a mono sample buffer, then it's an S16
     dest_sample_shift = 1;
   }

   //
   // based on the destination bytes, figure out how many src bytes we need
   //

   S32 needed_src = mult64addsubandshift( ( needed_dest >> dest_sample_shift ), playback_ratio,
                                           0xffff, 65536 - ( S->src_fract & 0xffff ), 16
                                        ) * src_sample_bytes;

   if ( S->src_fract & 0xc0000000 )
      needed_src -= ( ( ( playback_ratio >> 16 ) - ( ( S->src_fract & ~0xc0000000 ) >> 16 ) ) * src_sample_bytes );
   else
      needed_src += src_sample_bytes;

   if ( needed_src <= 0 )
      needed_src = src_sample_bytes;


   //
   // align our temp pointers and round the process buffer size down to the nearest
   // decoded sample
   //

   S32 using_process = 0;

#ifdef MSS_SPU_PROCESS
   U8 process_buffer_start [ DIG_PROCESS_BUFFER_SIZE + SPU_EXTRA + 16 + 8 ];
#else
   U8 process_buffer_start [ DIG_PROCESS_BUFFER_SIZE + 16 + 8 ];
#endif
   U8 * process_buffer = (U8*) ( ( ( (UINTa) process_buffer_start ) + 16 + 8 ) & ~15 );
   void * process_last_sample_ptr = 0; // points to the last sample of the buffer

   S32 process_buffer_bytes = (DIG_PROCESS_BUFFER_SIZE / src_sample_bytes) * src_sample_bytes;

   //
   // If ASI or ADPCM compression is in use, unpack data into
   // temporary buffer
   //

   void const *src;
   void       *src_end;
   ASISTAGE   *ASI = NULL;

   if (S->pipeline[SP_ASI_DECODER].active)
   {
      ASI = &S->pipeline[SP_ASI_DECODER].TYPE.ASI;

      S32 src_adjust = 0;

      //
      // ASI decoder in use -- source data must be obtained from
      // SP_ASI_DECODER_callback() via ASI_stream_process()
      //
      // ok, if we have a sample left over from the last decompressed buffer,
      //   then we need to use it first

      if ( S->last_decomp[0] != 65536 )
      {
         src_adjust = src_sample_bytes;

         // back up the front of the buffer to put the initial samples
         process_buffer -= src_sample_bytes;
         
         // we don't store these specifically little endian because we are
         //   just storing exactly what we read earlier (that is, on big endian
         //   machines, last_decomp_left/right are byte swapped in the value)

         for (S32 i=0; i < S->n_channels; i++)
         {
            ( ( S16* ) process_buffer)[ i ] = (S16) S->last_decomp[i];
         }
      }

      S32 decode_src = needed_src - src_adjust;

      if ( decode_src > 0 )
      {
         #ifdef MSS_SPU_PROCESS
         needed_src = get_mem_from_ppu( &process_buffer, src_adjust, S->n_channels,
         #else

         bump_count( S->driver->asi_nums[S->pipeline[SP_ASI_DECODER].active-1], S->driver->asi_sams[S->pipeline[SP_ASI_DECODER].active-1] );
         tmEnterAccumulationZone( miles_context, S->driver->asi_times[S->pipeline[SP_ASI_DECODER].active-1] );
         needed_src = ASI->ASI_stream_process(ASI->stream,
                                              process_buffer + src_adjust,
         #endif                                     
                                            ( decode_src > process_buffer_bytes ) ? process_buffer_bytes : decode_src );
         tmLeaveAccumulationZone( miles_context, S->driver->asi_times[S->pipeline[SP_ASI_DECODER].active-1] );

         needed_src += src_adjust;
      }

      //
      // Don't let position cursor hit the end of the input buffer if there's still some
      // decoded data in the ASI output buffer
      //

      if (needed_src != 0)
      {
         #ifdef MSS_SPU_PROCESS
           ;
         #else 
         S32 n = S->tail;

         if ((S->buf[n].pos == S->buf[n].len) && (ASI->OUTPUT_RESERVOIR != (UINTa) -1))
         {
            S32 left = ~0;

            ASI->ASI_stream_property(ASI->stream,
                                     ASI->OUTPUT_RESERVOIR,&left,0,0);

            if (left > 0)
            {
               S->buf[n].pos = S->buf[n].len - 1;
               S->exhaust_ASI = 1;
            }
         }
         #endif
      }
      else
      {
         // if no data was output and if we have to reset the ASI,
         //   don't advance the output pointer, because we'll just call this
         //   routine again immediately (after the ASI has been reset out at
         //   the upper layer).

         if ( S->buf[S->tail].reset_ASI)
         {
            return 0;
         }
      }

      using_process = 1;
      src = process_buffer;
      src_end = AIL_ptr_add( src, needed_src );
      process_last_sample_ptr = AIL_ptr_add( src, needed_src - src_sample_bytes );
   }
   else
   {
      //
      // No ASI decoder in use -- source data is available in its entirety
      // from the current buffer
      //

      src = AIL_ptr_add(S->buf[S->tail].start,
                        S->buf[S->tail].pos  );

      src_end = AIL_ptr_add(src,
                            in_len);

      //
      // If ADPCM compression in use, decompress the data into the decode
      // buffer
      //

      if (S->format & DIG_F_ADPCM_MASK)
      {
         S32 src_adjust = 0;

         // ok, if we have a sample left over from the last decompressed buffer,
         //   then we need to use it first

         if ( S->last_decomp[0] != 65536 )
         {
            src_adjust = src_sample_bytes;

            // back up the front of the buffer to put the initial samples
            process_buffer -= src_sample_bytes;
            
            // we don't store these specifically little endian because we are
            //   just storing exactly what we read earlier (that is, on big endian
            //   machines, last_decomp_left/right are byte swapped in the value)

            for (S32 i=0; i < S->n_channels; i++)
            {
               ( ( S16* ) process_buffer)[ i ] = (S16) S->last_decomp[i];
            }

         }

         S32 decode_src = needed_src - src_adjust;

         if ( decode_src > 0)
         {
            void const *in  = src;
            void *out       = process_buffer + src_adjust;

            //
            // Decode block of data from source sample to decode buffer
            //
            // Size of block to decode is determined by decoder
            // given smaller amount of available and needed data
            //

            // If we are downmixing, then the source is actually stereo.

#ifdef MSS_SPU_PROCESS
            if ( in_len > SPU_INPUT_SIZE )
              in_len = SPU_INPUT_SIZE;

            spu_mem_stream( sms, &spu_input, in_len, 0, -1 );
            in = spu_input;
#endif

            bump_count( S->driver->adpcm_num, S->driver->adpcm_sam );
            tmEnterAccumulationZone( miles_context, dig->adpcm_time );
            S->driver->pipeline[DP_ADPCM_DECODE].TYPE.MSS_mixer_adpcm_decode(
                                 &out, &in,
                                ( decode_src > process_buffer_bytes ) ? process_buffer_bytes : decode_src,
                                in_len, ( (S->format & DIG_F_STEREO_MASK) ? M_SRC_STEREO|M_SRC_16 : M_SRC_16), &S->adpcm);
            tmLeaveAccumulationZone( miles_context, dig->adpcm_time );

            //
            // Update source sample position index
            //

            using_process = 1;
            src = process_buffer;
            src_end = out;

            needed_src = (S32) AIL_ptr_dif( src_end, src );
            process_last_sample_ptr = AIL_ptr_add( src, needed_src - src_sample_bytes );

#ifdef MSS_SPU_PROCESS
            S->buf[S->tail].pos += (S32) AIL_ptr_dif(in,
                                                    spu_input);
            spu_input = (void*)in;
#else
            S->buf[S->tail].pos = (S32) AIL_ptr_dif(in,
                                                    S->buf[S->tail].start);
#endif
         }
      }
      else
      {

         //
         // Straight PCM
         //

         if ( needed_src <= 0 )
         {
            //
            // we always need at least one input sample, but only *advance* the
            // source pointer if we are about to wrap around to the next one
            //

            needed_src = src_sample_bytes;

            if ( ( S->src_fract & 0xffff ) + playback_ratio >= 65536 )
              goto uncomp_advance;
         }
         else
         {
uncomp_advance:
            //
            // clamp to the end of the buffer
            //

            if ( needed_src > in_len )
              needed_src = in_len;

#ifndef MSS_FLT_SUPPORTED
            if ( S->lp_any_on )
            {
               if ( needed_src > process_buffer_bytes )
               {
                  needed_src = process_buffer_bytes;
               }
            }
#endif

#ifdef MSS_SPU_PROCESS
            if ( needed_src > SPU_INPUT_SIZE )
              needed_src = ( SPU_INPUT_SIZE / src_sample_bytes ) * src_sample_bytes;

            spu_mem_stream( sms, &spu_input, needed_src, 0, -1 );
            src = spu_input;
#endif
         }

         src_end = AIL_ptr_add( src, needed_src );
      }
   }

#ifdef _DEBUG

   //
   // Src must be a multiple of sample size.
   //

   if (AIL_ptr_dif( src_end, src ) % src_sample_bytes)
   {
     MSSBreakPoint();
   }

#endif

   //
   // If empty source range, return to let the caller switch buffers, rewind 
   // loops, etc.  (Caller is responsible for flagging potential infinite loops)
   //

   if (needed_src <= 0)
      {
      return 0;
      }

   //
   // If empty dest range, simulate a full buffer to force end of mixing interval
   // (should never happen)
   //

   if (needed_dest <= 0)
      {
      return dest_buffer_size;
      }

   //
   // Calc a rough new needed_dest based on our src size (which may have changed
   //   due to input buffer size and/or decompression - this is only *directly
   //   used for zero volumes, so it just has to be conservative
   //

   S32 dest_from_src = ( shift64addsubanddiv( ( needed_src / src_sample_bytes ) + 1,
                                              16,
                                              65536 - ( S->src_fract & 0xffff ),
                                              playback_ratio >> 1,
                                              playback_ratio ) + 1 ) << dest_sample_shift;

   if ( dest_from_src < needed_dest )
   {
      needed_dest = ( dest_from_src ) ? dest_from_src : ( 1 << dest_sample_shift );
   }

   S->max_output_mix_volume = 0.0f;

   //
   // If something has changed that affects low pass, recompute the lowpass parameters.
   //
   for (S32 chan = 0; chan < MAX_SPEAKERS; chan++)
   {
       if ((S->low_pass_changed & 1) && chan < S->n_channels)
       {
           F32 low_pass_hz = S->extra_lp;
           if (S->S3D.lowpass_3D != -1.0f && S->S3D.lowpass_3D < S->extra_lp)
               low_pass_hz = S->S3D.lowpass_3D;

           F32 cutoff = low_pass_hz / (S->original_playback_rate / 2);
           if (cutoff > 1.0f)
               cutoff = 1.0f;

           if (S->low_pass_cutoff[chan] < cutoff)
               cutoff = S->low_pass_cutoff[chan];

           update_low_pass_coef(S, chan, cutoff);
       }
       S->low_pass_changed >>= 1;
   }

   S->low_pass_changed = 0;

   F32 fade_volume = 1.0f;
   if (S->pop_fade_total)
   {
       fade_volume = S->pop_fade_time / (float)S->pop_fade_total;
       if (fade_volume < 0) // fading in not out.
           fade_volume += 1;
   }

   // ====================================================================================
   // Get output processing options
   // ====================================================================================

   S32  *reverb_dest          = NULL;
   S32       reverb_channel_scale = 0;

#ifdef MSS_REVERB_SUPPORTED

   F32 use_wet_level = S->wet_level;
   if (S->bus < MAX_BUSSES &&
       S->driver->bus_samples[S->bus] != 0 &&
       S->driver->bus_samples[S->bus]->bus_override_wet)
       use_wet_level = S->driver->bus_samples[S->bus]->wet_level;

   F32 reverb_scale = dig->master_volume *
                      (S->bus == MAX_BUSSES ? 1 : dig->reverb[S->bus].master_wet) *
                      S->save_volume     *
                      use_wet_level      *
                      S->sys_level       *
                      S->extra_wet       *
                      S->extra_volume    *
                      fade_volume * 
                      (1.0F - S->exclusion);

   if (S->S3D.auto_3D_atten && S->S3D.excgraphcnt == 0) // disable auto 3d attentuation if they have an exclusion graph, since it does the same thing.
      {
      reverb_scale *= S->S3D.atten_3D;
      }
      
   if ( reverb_scale > S->max_output_mix_volume )
     S->max_output_mix_volume = reverb_scale;

   S32 zero_reverb_volume = FALSE;

   if (dest_mono_sample_buffer == NULL)
      {
      if (reverb_scale < 0.0001F)
         {
         zero_reverb_volume = TRUE;
         }

      if (S->bus < MAX_BUSSES && (dig->reverb[S->bus].reverb_on) && (!zero_reverb_volume))
         {
         reverb_channel_scale = (S32) ((2048.0F * reverb_scale) + 0.5F);
         if (reverb_channel_scale >= 2048) reverb_channel_scale = 2048;

         //
         // Calculate destination address in global mono reverb buffer
         //

         reverb_dest = (S32 *) AIL_ptr_add(dig->reverb[S->bus].reverb_build_buffer, dig->reverb[S->bus].reverb_head);

         // advance the time when the reverb should turn off

         dig->reverb[S->bus].reverb_off_time_ms = S->driver->num_mixes;
         }
      }
#endif

   //
   // Table of level-control indexes for current formats of up to 10 channels (5 build buffers)
   // Must be expanded if formats > MSS_MC_81_DISCRETE are supported!
   //
   // Entries: 0=FL, 1=FR, 2=C, 3=LFE, 4=RL, 5=RR, 6=RL+RR/2
   //

   static const S32 SC_index[10][10] =   // [# of logical channels] [build_buffer_num*2+channel]
   {   { 0,0,0,0,0,0,0,0,0,0 },          // N/A
       { 0,0,0,0,0,0,0,0,0,0 },          // Mono
       { 0,1,0,0,0,0,0,0,0,0 },          // Stereo/headphones
       { 0,1,6,0,0,0,0,0,0,0 },          // Dolby Surround
       { 0,1,4,5,0,0,0,0,0,0 },          // 4.0 discrete
       { 0,0,0,0,0,0,0,0,0,0 },          // N/A (no 5-channel formats are supported)
       { 0,1,2,3,4,5,0,0,0,0 },          // 5.1 discrete
       { 0,1,2,3,4,5,6,0,0,0 },          // 6.1 discrete
       { 0,1,2,3,4,5,4,5,0,0 },          // 7.1 discrete/SRS
       { 0,1,2,3,4,5,6,4,5,0 }};         // 8.1 discrete

   F32 dry_scale = dig->master_volume * (S->bus == MAX_BUSSES ? 1 : dig->reverb[S->bus].master_dry) * S->sys_level * S->dry_level * (1.0F - S->obstruction);

   dry_scale *= S->extra_volume * fade_volume;

   F32 SC[7];

   SC[0] = S->left_volume  * dry_scale;
   SC[1] = S->right_volume * dry_scale;

   if (dig->logical_channels_per_sample >= 3)
   {
      //
      // Calculate additional 5.1-derived "shortcut" volume scalars
      //

      SC[2] = S->center_volume * dry_scale;
      SC[3] = S->low_volume    * dry_scale;
      SC[4] = S->leftb_volume  * dry_scale;
      SC[5] = S->rightb_volume * dry_scale;
      SC[6] = (SC[4] + SC[5]) / 2.0F;
   }

   const S32 *LVL = SC_index[dig->logical_channels_per_sample];

   //
   // If this is a deinterlacing sample, make one mono pass per source channel,
   // copying it to the deinterlacing buffer
   //

   S32             MC_chan          = 0;
   S32             MC_out_pos       = out_pos;
   void const *MC_src           = src;
   S32             MC_needed_src    = needed_src;
   U32             MC_src_fract     = S->src_fract;         //used to restore the src fract before each additional pass
   S32             n_samples        = MC_needed_src / src_sample_bytes;
   S32             MC_src_bytes     = src_sample_bytes;
   S32             MC_did_mix_out_pos = 0;
   U32             MC_did_mix_src_fract = 0;
   void const* MC_did_mix_out_src = 0;
   S32 *           left_val = &S->left_val[ 0 ];
   S32 *           right_val = &S->right_val[ 0 ];
   S32             tmp_val=0;
   
do_MC_src_channel:

  if (need_deinterlace)
  {
     #ifdef MSS_SPU_PROCESS
       if ( tmp_mc_buffer == 0 )
         tmp_mc_buffer = (char*) ( ( ( (UINTa)spu_alloc( sizeof(S16) * dig->samples_per_buffer + 16 + SPU_EXTRA ) ) + 15 ) & ~15 );
       src        = tmp_mc_buffer;
     #else
       src        = dig->MC_buffer;
     #endif

     S16 *out = (S16 *) src;
     S32 step      = S->n_channels;

     #ifdef _DEBUG
       if (n_samples > (dig->samples_per_buffer+4)) MSSBreakPoint();
     #endif

      bump_count( S->driver->deinterlace_num, S->driver->deinterlace_sam );
      tmEnterAccumulationZone( miles_context, dig->deinterlace_time );
      if ( !originally_16bit )
      {
        U8 *in  = (U8 *) MC_src + MC_chan;
        for (S32 i=0; i < n_samples; i++)
        {
          S16 v = (S16)( ( (*in) - 128 ) << 8 );
          STORE_LE_SWAP16( out, (S16) v );
          ++out;
          in += step;
        }
      }
      else
      {
        S16 *in  = (S16 *) MC_src + MC_chan;
        for (S32 i=0; i < n_samples; i++)
        {
          *out++ = *in;
          in += step;
        }
      }
      tmLeaveAccumulationZone( miles_context, dig->deinterlace_time );

      src_end    = out;
      needed_src = (S32) AIL_ptr_dif( src_end, src );
      MC_src_bytes = 2;
      op |= M_SRC_16;
      op &= ~M_SRC_STEREO;
   }

   // ====================================================================================
   // Get reverb and per-channel dry destination levels, and see if zero-volume optimizations are
   // possible
   // ====================================================================================

   S32 zero_volume = FALSE;
   S32 z = 0;

   if (dest_mono_sample_buffer == NULL)
   {
      for (S32 b = 0; b < dig->n_build_buffers; b++)
      {
         S32 index = b*2;
         S32 rindex = index;
         S32 MC_chanR = MC_chan;

         if (dig->build[b].chans > 1 )
           ++rindex;
         if ( op & M_SRC_STEREO )
           ++MC_chanR;

         F32 left_scale  = SC [ LVL [index ] ] * dig->D3D.speaker_dry_reverb_response[index];
         F32 right_scale = SC [ LVL [rindex] ] * dig->D3D.speaker_dry_reverb_response[rindex];

         if (S->bus == MAX_BUSSES)
         {
             // We want no driver effects on the sample, so they don't get double-applied,
             // but we want to leave all of the 3d stuff so that a bus can be spatialized.
             left_scale = S->extra_volume;
             right_scale = S->extra_volume;
         }

         //
         // Set up previous-scale values for volume-scaled writes to this build buffer
         // on behalf of the currently-processed HSAMPLE
         //
         // Note that prev and cur_scale just use index and index+1, that's because
         // we always store the volumes in pairs (just like the build buffers themselves)
         //

         S->prev_scale[index]  [MC_chan] = S->cur_scale[index ] [MC_chan];
         S->prev_scale[index+1][MC_chan] = S->cur_scale[index+1][MC_chan];

         //
         // Calculate volume scalars used for writes to each build buffer
         //
         // All build buffers are stereo except the last, which is mono if the output
         // format is mono or if it's a remnant buffer in an odd multichannel
         // configuration (e.g., 6.1)
         //

         F32 level_scalar = 1.0f;
         F32 rlevel_scalar = 1.0f;

         if (S->is_3D)
         {
             level_scalar = S->auto_3D_channel_levels[index];
             rlevel_scalar = S->auto_3D_channel_levels[rindex];
         }
         else
         {
             level_scalar = S->user_channel_levels[index][MC_chan];
             rlevel_scalar = S->user_channel_levels[rindex][MC_chanR];
         }

         if (S->fade_to_stop)
             level_scalar = rlevel_scalar = 0.0f;

#ifdef IS_WIIU
         // If we are supposed to redirect all channels to the drc, and we have drc channels on to the end of our
         // dig.
         if (S->route_to_drc && dig->logical_channels_per_sample == 8)
         {
             if (index < 6)
             {
                 level_scalar = 0;
                 rlevel_scalar = 0;
             }
             else
             {
                 level_scalar = 1.0f;
                 rlevel_scalar = 1.0f;
                 if (S->is_3D)
                 {
                     level_scalar = (S->auto_3D_channel_levels[0] + S->auto_3D_channel_levels[4] + S->auto_3D_channel_levels[2]/2)/3;
                     rlevel_scalar = (S->auto_3D_channel_levels[1] + S->auto_3D_channel_levels[5] + S->auto_3D_channel_levels[2]/2)/3;
                 }
                 else
                 {
                     // Ignore user channel levels
                 }
             }
         }
#endif


         if ( S->is_3D )
         {
           S->cur_scale[index]  [MC_chan] = (S32) ((2048.0f * left_scale  * S->speaker_levels[index ] * level_scalar) + 0.5F);
           S->cur_scale[index+1][MC_chan] = (S32) ((2048.0f * right_scale * S->speaker_levels[rindex] * rlevel_scalar) + 0.5F);
         }
         else
         {
           S->cur_scale[index]  [MC_chan] = (S32) ((2048.0f * left_scale  * S->speaker_levels[index ] * level_scalar  ) + 0.5F);
           S->cur_scale[index+1][MC_chan] = (S32) ((2048.0f * right_scale * S->speaker_levels[rindex] * rlevel_scalar ) + 0.5F);
         }

         if (S->cur_scale[index]  [MC_chan] >= 2048) S->cur_scale[index]  [MC_chan] = 2048;
         if (S->cur_scale[index+1][MC_chan] >= 2048) S->cur_scale[index+1][MC_chan] = 2048;

         if ((S->cur_scale[index][MC_chan] == 0)  && (S->cur_scale[index+1][MC_chan] == 0) &&
             (S->prev_scale[index][MC_chan] == 0) && (S->prev_scale[index+1][MC_chan] == 0))
         {
            if ( ( S->wet_level * dig->reverb[S->bus].master_wet * S->sys_level ) < MSS_EPSILON )
            {
               z++;
            }
         }
      }

      #if defined(SHOWMIXVOLS) && defined(IS_WIN32)
        z = -1; // when displaying mix volumes, never skip mc_mix
      #endif

      if (z == dig->n_build_buffers)
      {
         zero_volume = TRUE;
      }

      // if the low pass is ON, and the level is ZERO, and there are no ramps LEFT
      if ( (S->lp[MC_chan].flags == 0x10000 ) && (S->lp[MC_chan].A == 0 ) )
      {
         zero_volume = TRUE;
      }
   }


   // ====================================================================================
   // Call mixer provider
   // ====================================================================================

   const void *mixer_input = src;

   if (zero_volume)
   {
      // --------------------------------------------------------------------------------
      // Fast path #1: volume is zero, so just advance the dest pointer (don't mix)
      //
      // The last channel of an MC sample is always sent through the normal (non-silent) path
      // to ensure that the final source-fraction and output-position deltas match those
      // in any audible channels
      // --------------------------------------------------------------------------------

      *left_val  = 0;
      *right_val = 0;

      S->last_decomp[0] = 65536;
      S->src_fract      = 0;

      src = src_end;
      out_pos += needed_dest;
   }
   else
   {
      // ------------------------------------------------------------------------------------
      // Initialize last-value history for filters properly following a non-resampled
      // mixer frame
      // ------------------------------------------------------------------------------------

      if ((S->src_fract == 0x20000000) && (op & M_FILTER))
      {
         S->src_fract = 0;

         if (op & M_SRC_16)
         {
            *left_val   = (S16) LE_SWAP16(&( (S16*) src )[ 0 ]);
            *right_val = *left_val;
            if ( op & M_SRC_STEREO )
              *right_val  = (S16) LE_SWAP16(&( (S16*) src )[ 1 ]);
         }
         else
         {
            *left_val = ( (U8*) src )[ 0 ] - 128;
            *right_val = *left_val;
            if ( op & M_SRC_STEREO )
               *right_val = ( (U8*) src )[ 1 ] - 128;
         }
      }

#ifdef MSS_FLT_SUPPORTED
      if ( (!filters_enabled) && ( dest_mono_sample_buffer ) )
      {
         // --------------------------------------------------------------------------------
         // Fast path #2: no filters into a hardware buffer (direct copy)
         // --------------------------------------------------------------------------------

          const void *temp = AIL_ptr_add(dest_mono_sample_buffer, out_pos);
          void *temp_end   = AIL_ptr_add(dest_mono_sample_buffer, out_pos + needed_dest);

          bump_count( S->driver->mix_num, S->driver->mix_sam );
          tmEnterAccumulationZone( miles_context, dig->mix_time );
          S->pipeline[SP_MERGE].TYPE.MSS_mixer_merge(&src,
                                                     &S->src_fract,
                                                      src_end,
                        (S32 * *)(void*) &temp,
                                                      temp_end,
                                                      left_val,
                                                      right_val,
                                                      playback_ratio,
                                                      0,
                                                      0,
                                                      (op | M_COPY16_NOVOL ) & ~M_DEST_STEREO
#ifdef IS_X86
                                                     ,dig->use_MMX
#endif
                                                      );
          tmLeaveAccumulationZone( miles_context, dig->mix_time );

          out_pos = (S32) AIL_ptr_dif(temp, dest_mono_sample_buffer);
      }
      else
#endif
      {
         // --------------------------------------------------------------------------------
         // "Full" path: internal reverb engine and/or outboard pipeline filters are
         // potentially enabled
         // --------------------------------------------------------------------------------

         //
         // First, call mixer to resample data if we need to resample or format convert,
         //   and then filter it if necessary
         //

         S32 mix_bytes=0;

         void *mixer_output;

         S32 input_format_to_mc_mix_is_stereo = op & M_SRC_STEREO;

         if (( op & M_SRC_16 ) && ( ! ( op & M_RESAMPLE ) ) && ( (!input_format_to_mc_mix_is_stereo) || (!filters_enabled) )  )
         {
            mixer_output = (void *) src;
            mix_bytes    = (S32) AIL_ptr_dif( src_end, src );

            S32 DestSizeInSrcFormat = ( needed_dest >> dest_sample_shift ) * MC_src_bytes;

            if ( mix_bytes > DestSizeInSrcFormat )
            {
               mix_bytes = DestSizeInSrcFormat;
               src = AIL_ptr_add( src, DestSizeInSrcFormat );
            }
            else
            {
               src = src_end;
            }
         }
         else
         {
#ifdef MSS_FLT_SUPPORTED
#ifdef MSS_SPU_PROCESS
            if ( tmp_spu_buf == 0 )
              tmp_spu_buf = (char*) ( ( ( (UINTa)spu_alloc( S->driver->build[FLT_A].bytes + 16 + SPU_EXTRA ) ) + 15 ) & ~15 );
            void *temp_buf = tmp_spu_buf;
#else
            void *temp_buf = dig->build[FLT_A].buffer;
#endif

            // clamp to the amount of data that mc_mix is going to eat exactly
            // (otherwise we will resample too much data and drop it)
            mix_bytes = ( needed_dest >> dest_sample_shift ) * sizeof( S16 );
            if ( op & M_DEST_STEREO )
              mix_bytes <<= 1;

            void *temp_end = AIL_ptr_add( temp_buf, mix_bytes );
#else
            void *temp_buf = AIL_ptr_add( dig->build[0].buffer, out_pos );
            void *temp_end = AIL_ptr_add( dig->build[0].buffer, dig->build[0].bytes );
#endif

            mixer_output = temp_buf;

            input_format_to_mc_mix_is_stereo = op & M_DEST_STEREO;

            bump_count( S->driver->reformat_num, S->driver->reformat_sam );
            tmEnterAccumulationZone( miles_context, dig->reformat_time );
            S->pipeline[SP_MERGE].TYPE.MSS_mixer_merge(
                                                       &src,
                                                       &S->src_fract,
                                                        src_end,
                          (S32 * *)(void*) &temp_buf,
                                                        temp_end,
                                                        left_val,
                                                        right_val,
                                                        playback_ratio,
                                                        0,
                                                        0,
                                                        op | M_COPY16_NOVOL
#ifdef IS_X86
                                                       ,dig->use_MMX
#endif
                                                        );
            tmLeaveAccumulationZone( miles_context, dig->reformat_time );

            mix_bytes = (S32) ( AIL_ptr_dif( temp_buf, mixer_output ) );

         }

         //
         // Next, apply low pass filtering
         //

         if (S->lp[MC_chan].flags )
         {
            S16 *d = (S16 *) ((dest_mono_sample_buffer != NULL) && (S->n_active_filters == 0)) ?
               (S16 *) AIL_ptr_add( dest_mono_sample_buffer, out_pos )
                  :
#ifdef MSS_FLT_SUPPORTED
               (S16 *) AIL_ptr_add( dig->build[FLT_A].buffer, 0 );
#else
               process_buffer;
#endif

            bump_count( dig->lowpass_num, dig->lowpass_sam );
            tmEnterAccumulationZone( miles_context, dig->lowpass_time );
            if ( do_low_pass( d, (S16 const *) mixer_output, &S->lp[MC_chan], mix_bytes / sizeof(S16) ) )
            {
              // update the sample any-low-passes-on flag
              S->lp_any_on = 0;
              for (S32 i=0; i < S->n_channels; i++)
               if ( S->lp[i].flags )
                 S->lp_any_on = 1;
            }
            tmLeaveAccumulationZone( miles_context, dig->lowpass_time );

            mixer_output = d;
         }

#ifdef MSS_FLT_SUPPORTED

         //
         // Next, apply filtering on platforms that support it
         //

         if (S->n_active_filters > 0)
         {
            bump_count( S->driver->filter_num, S->driver->filter_sam );
            tmEnterAccumulationZone( miles_context, dig->filter_time );
            mixer_output = SS_execute_filter_chain( S,
                                                    mixer_output,
                                                    dig->build[FLT_A].buffer,
                                                    mix_bytes,
                                                    MC_chan,
                                                    (dest_mono_sample_buffer == NULL) ? NULL : (S16 *) AIL_ptr_add( dest_mono_sample_buffer, out_pos));
            tmLeaveAccumulationZone( miles_context, dig->filter_time );
         }

#endif

         if (dest_mono_sample_buffer == NULL)
         {
            //
            // Perform level scaling and copy the data to the reverb/build buffers
            //

            out_pos = MC_mix(S,
                   (S16 *) mixer_output,          // src
                   mix_bytes,                         // src_bytes
                   dig->build,                        // dest_buffers
                   dig->n_build_buffers,              // n_dest_buffers
                   reverb_dest,                       // reverb_dest
                   reverb_channel_scale,              // reverb_channel_scale
                   out_pos,                           // dest offset
                   input_format_to_mc_mix_is_stereo ? 1 : 0,
                   MC_chan);                          // Always zero for conventional mono or stereo samples

            #ifdef _DEBUG
              S32 bytes_we_used = out_pos - MC_out_pos;
              S32 bytes_we_should_have_used = mix_bytes << 1; // S16 to S32
              if ( ( dig->build[MC_chan].chans == 1 ) && ( input_format_to_mc_mix_is_stereo ) )
                bytes_we_should_have_used >>= 1;
              else if ( ( dig->build[MC_chan].chans == 2 ) && ( input_format_to_mc_mix_is_stereo == 0 ) )
                bytes_we_should_have_used <<= 1;

              if ( bytes_we_used < bytes_we_should_have_used )
              {
                MSSBreakPoint();
              }
            #endif
         }
         else
         {
            // mix_bytes is actually the source buffer size, but in dest_mono_sample_buffer mode, dest_size == src size == mix_bytes
            out_pos += mix_bytes;
         }
      }

      MC_did_mix_out_pos = out_pos;
      MC_did_mix_src_fract = S->src_fract;
      MC_did_mix_out_src = src;
   }

   if (need_deinterlace)
   {
      ++MC_chan;

      if (MC_chan < S->n_channels)
      {
         out_pos      = MC_out_pos;
         S->src_fract = MC_src_fract;
         // only the left_chan matters (because we are turning it into mono)
         if ( MC_chan & 1 )
           left_val = &S->right_val[ MC_chan >> 1 ];
         else
           left_val = &S->left_val[ MC_chan >> 1 ];
         // dummy value
         right_val = &tmp_val;
         goto do_MC_src_channel;
      }

      // if we ever did a full mix, use those outputs
      if ( MC_did_mix_out_pos )
      {
        out_pos = MC_did_mix_out_pos;
        S->src_fract = MC_did_mix_src_fract;
        src = MC_did_mix_out_src;
        // convert the deinterlaced 16-bit/mono format to the input format size
        needed_src = (((S32)AIL_ptr_dif(src, mixer_input))>>1)*src_sample_bytes;
      }
      else
      {
        // only used in (using_process==0) clause below
        needed_src = MC_needed_src;
      }
   }
   else
   {
      // only used in (using_process==0) clause below
      needed_src = (S32)AIL_ptr_dif(src, mixer_input);
   }


   if ( using_process == 0 )
   {
     #ifdef _DEBUG
       if (needed_src % MC_src_bytes )
       {
         MSSBreakPoint();
       }
     #endif

     //
     // Update source sample position based on amount actually consumed by mixer, if the
     // sample is uncompressed
     //

     S->buf[ S->tail ].pos += needed_src;

#ifdef MSS_SPU_PROCESS
     spu_input = AIL_ptr_add( spu_input, needed_src );
#endif

   }
   else
   {
      //
      // We are using the process buffer -- either we used all of it, or
      // we didn't use the very last sample.  if we didn't use the last
      // one, then save it (since the process_buffer is only on the stack)
      //

      if (src >= src_end)
      {
        S->last_decomp[0] = 65536;
      }
      else
      {
        #ifdef _DEBUG
          // adjust the compare for super high rates
          S32 adj = ( ( playback_ratio >> 16 ) * src_sample_bytes ) + src_sample_bytes;
          if (adj<=src_sample_bytes) adj = src_sample_bytes;
          if ( playback_ratio > ( 2 * 64 * 1024 ) )
            adj += src_sample_bytes;

          if ( AIL_ptr_dif( src_end, src ) > adj )
          {
            MSSBreakPoint();
          }
        #endif


         // we don't store these specifically little endian because we are
         //   just storing exactly what we read earlier (that is, on big endian
         //   machines, last_decomp_left/right are byte swapped in the value)

         for (S32 i=0; i < S->n_channels; i++)
         {
           S->last_decomp[i] = ( (S16*) process_last_sample_ptr )[ i ];
         }
      }
   }

   //
   // Return updated build buffer output offset
   //

   return( out_pos );
}

#ifndef MSS_SPU_PROCESS


static S32 SS_limiter(MSS_BB* build, S32 n_build_buffers, S32 samples_per_buffer, S32 last_attenuation)
{
    // limit over ~-10 db
    S32 threshold = (12000) << 11;

    S32 samples_left = samples_per_buffer;
    S32 samples_done = 0;
    S32 active_atten = last_attenuation;
    for (;;)
    {
        S32 max_strength = 0;
        

        for (S32 i=0; i < n_build_buffers; i++)
        {
            MSS_BB *B = &build[i];
            S32 * MSSRESTRICT src  = B->buffer + samples_done*B->chans;

            S32* end = src + 256 * B->chans;

            if (end > src + samples_left*B->chans)
                end = src + samples_left*B->chans;

            for (; src != end; src++)
            {
                if (src[0] < 0 && -src[0] > max_strength)
                    max_strength = -src[0];
                else if (src[0] > 0 && src[0] > max_strength)
                    max_strength = src[0];
            }
        }

        // If there isn't any attenuation, return to 0db.
        S32 target_attenuate = 32767;

        if (max_strength > threshold)
        {
            S32 attenuate = max_strength - threshold;
            attenuate >>= 11;
            
            // at 0, 32768 scalar (no attenuation)
            // at 20767, 4096 scalar (-18 db)

            attenuate = -attenuate;
            attenuate = attenuate * 4096 / 20767;
            attenuate += 32767;
            if (attenuate < 0)
                attenuate = 0;

            target_attenuate = attenuate;
        }

        {
            // move the attenuation towards the target over time to prevent zipper.
            S32 d = active_atten - target_attenuate;
            if (d < 0)
                d = -d;
            if (d < 200)
                active_atten = target_attenuate;
            else
                active_atten = ((target_attenuate * 16384) >> 15) + ((active_atten * 16384) >> 15);
        }

        // If there is an active attenuation, scale the build buffers.
        if (active_atten != 32767)
        {
            for (S32 i=0; i < n_build_buffers; i++)
            {
                MSS_BB *B = &build[i];
                S32 * MSSRESTRICT src  = B->buffer + samples_done*B->chans;

                S32* end = src + 256 * B->chans;

                if (end > src + samples_left*B->chans)
                    end = src + samples_left*B->chans;

                for (; src != end; src++)
                {
                    S32 a = src[0] >> 11;
                    a = (a * active_atten) >> 15;
                    src[0] = a << 11;
                }
            }
        }

        samples_done += 256;
        samples_left -= 256;
        if (samples_left <= 0)
            break;
    }

    return active_atten;
}

//############################################################################
//##                                                                        ##
//## Copy build buffer contents to bus input                                ##
//##                                                                        ##
//############################################################################
static void SS_copy_to_bus(MSS_BB * build, S32 n_build_buffers, void * lpWaveAddr,
    S32 samples_per_buffer, S32 physical_channels_per_sample)
{
    for (S32 i=0; i < n_build_buffers; i++)
    {
        MSS_BB *B = &build[i];

        S32 * MSSRESTRICT src  = B->buffer;
        S16 * MSSRESTRICT dest = &((S16 *) lpWaveAddr)[ B->speaker_offset ];

        if (B->chans == 2)
        {
            S32 n = samples_per_buffer * 2;

            for (S32 j=0; j < n; j += 2)
            {
                S32 left  = src[j]   >> 11;
                S32 right = src[j+1] >> 11;

                S16 l, r;

                if      (left <= -32768)  l = -32768;
                else if (left >=  32767)  l =  32767;
                else                      l =  (S16) left;

                if      (right <= -32768) r = -32768;
                else if (right >=  32767) r =  32767;
                else                      r =  (S16) right;

                STORE_LE_SWAP16(dest, l);
                STORE_LE_SWAP16_OFS(dest, r, 2);

                dest += physical_channels_per_sample;
            }
        }
        else
        {
            S32 n = samples_per_buffer;

            for (S32 j=0; j < n; j++)
            {
                S32 mono = src[j] >> 11;

                S16 out;

                if      (mono <= -32768)  out = -32768;
                else if (mono >=  32767)  out =  32767;
                else                      out =  (S16) mono;

                STORE_LE_SWAP16(dest, out);

                dest += physical_channels_per_sample;
            }
        }
    }
}

#endif
//############################################################################
//##                                                                        ##
//## Copy mixer buffer to DMA buffer                                        ##
//##                                                                        ##
//############################################################################

inline F32 MSS_scalar_to_dB(F32 scalar)  // turns linear DAC scalar into dB (0.5 = -6 dB, 0.25 = -12 dB, ...)
{
   if (scalar < 0.00001F)
      {
      return -96.0F;
      }

   return 20.0F * log10(scalar);
}

void AILCALL SS_copy (HDIGDRIVER dig, S32 reverb_index, void *lpWaveAddr)
{
    //
    // Add reverb buffer contents, if any, to build buffers
    //
    S32 use_index = reverb_index;

    // If we have no busses active, we need to use the base reverb.
    if (dig->bus_samples[0] == 0)
        use_index = 0;
    if ( dig->reverb[use_index].reverb_on )
        do_reverb( dig, use_index );

#ifndef MSS_SPU_PROCESS

    // If we are copying to a bus, handle that first then bail.
    if (reverb_index < MAX_BUSSES)
    {
        tmEnterAccumulationZone( miles_context, dig->copy_time );

        HSAMPLE target_sample = dig->bus_samples[reverb_index];

        if (target_sample->bus_enable_limiter)
            target_sample->bus_limiter_atten = SS_limiter(dig->build, dig->n_build_buffers, dig->samples_per_buffer, target_sample->bus_limiter_atten);

        SS_copy_to_bus(dig->build, dig->n_build_buffers, lpWaveAddr, dig->samples_per_buffer, target_sample->n_channels);

        {
            // TODO: run a filter on the data before it gets input to the compressor.
            S32 sig_strength = 0;

            S16* src = (S16*)lpWaveAddr;
            S16* srcend = src + dig->samples_per_buffer * target_sample->n_channels;

            for (; src != srcend; src++)
            {
                S16 l = src[0];
                if (l < 0 && sig_strength < -l)
                    sig_strength = -l;
                else if (l > 0 && sig_strength < l)
                    sig_strength = l;
            }

            target_sample->bus_signal_strength = sig_strength;

            // Set the signal strength for each send we have.
            S32 Sends = target_sample->bus_comp_sends;
            for (S32 b = 0; b < MAX_BUSSES && Sends; b++, Sends >>= 1)
            {
                HSAMPLE S = dig->bus_samples[b];

                if ( (Sends & 1) && S->bus_comp_installed)
                {
                    if (S->bus_comp_input < (U32)sig_strength)
                        S->bus_comp_input = sig_strength;
                }
            }
        }

        tmLeaveAccumulationZone( miles_context, dig->copy_time );
        return;
    }
#endif

#ifndef MSS_SPU_PROCESS
    if (dig->enable_limiter)
        dig->limiter_atten = SS_limiter(dig->build, dig->n_build_buffers, dig->samples_per_buffer, dig->limiter_atten);
#endif

#ifdef MSS_FLT_SUPPORTED

   //
   // Invoke installable filters
   //
   // If any filter's postmix processor performed the buffer copy, we're done
   //

#ifndef MSS_SPU_PROCESS
   // Only call postmix on the final output.
    tmEnterAccumulationZone( miles_context, dig->filter_time );
    if (FLT_call_postmix_processors(dig, lpWaveAddr))
    {
        return;
    }
    tmLeaveAccumulationZone( miles_context, dig->filter_time );
#endif

#endif

    if (dig->logical_channels_per_sample != dig->physical_channels_per_sample)
    {
        // This should never happen, and be handled by the postmix filters above
        return;
    }


    S32 force_mc_copy = 0;
#ifdef IS_PS3
    force_mc_copy = 1; // PS3 requires use of the mc_copy due to floats. This should compile the non mc copy out...
#endif

    tmEnterAccumulationZone( miles_context, dig->copy_time );
#ifndef IS_WII // wii doesn't have mc copy.
    if (dig->logical_channels_per_sample > 2 || force_mc_copy)
    {
        dig->pipeline[DP_MC_COPY].TYPE.MSS_mixer_mc_copy( 
            dig->build,
            dig->n_build_buffers,
            lpWaveAddr,
            dig->hw_format,
#ifdef IS_X86
            dig->use_MMX,
#endif
            dig->samples_per_buffer,
            dig->physical_channels_per_sample );
    }
    else
#endif // IS_WII
    {
        dig->pipeline[DP_COPY].TYPE.MSS_mixer_copy(dig->build[0].buffer,
            dig->build[0].bytes,
            lpWaveAddr,
            dig->hw_format
#if defined(IS_X86)
            ,dig->use_MMX
#elif defined(IS_BE)
            ,1      // Use big-endian output
#endif
            );
    }
    tmLeaveAccumulationZone( miles_context, dig->copy_time );
}

//############################################################################
//##                                                                        ##
//## Copy data from source sample to specified target buffer with ASI       ##
//## decompression                                                          ##
//##                                                                        ##
//############################################################################

S32 AILCALLBACK SS_stream_to_buffer(HSAMPLE  S, //)
                                    S16 *dest_mono_sample_buffer,
                                    S32      dest_buffer_size)
{
  S32 out_pos;
  S32 last_out_pos;
  S32 in_len;
  S32 next_buffer;

#ifdef MSS_SPU_PROCESS
  addstackmark();
  char * input_buf = (char*)spu_alloc( SPU_INPUT_SIZE + SPU_STREAM_EXTRA );
#endif

  S->dosob=0;
  S->doeob=0;
  S->doeos=0;

  //
  // Bail if source stream ended in previous call
  //

  if (S->driver->sample_status[S->index] != SMP_PLAYING)
  {
    return 0;
  }

      if (S->driver->logical_channels_per_sample > 0)
      {
         if (S->is_3D)
         {
#ifdef MSS_SPU_PROCESS
             if ( S->S3D.falloff_function == 0 )
             {
#endif
                F32 last_lp = S->S3D.lowpass_3D;

                SS_calculate_3D_channel_levels(&S->driver->D3D,
                                             S->driver->logical_channels_per_sample,
                                            &S->S3D,
                                             S->auto_3D_channel_levels);

                if (S->S3D.exclusion_3D != -1.0f)
                    S->exclusion = S->S3D.exclusion_3D;

                if (fabs(last_lp - S->S3D.lowpass_3D) > .0001f)
                    S->low_pass_changed = ~0U;

#ifdef MSS_SPU_PROCESS
             }
#endif
         }
      }

#ifdef MSS_SPU_PROCESS
  spu_input = ( (U8*) spu_mem_init( &sms_input, input_buf, SPU_INPUT_SIZE + SPU_STREAM_EXTRA, S->buf[S->tail].start, S->buf[S->tail].len ) ) + S->buf[S->tail].pos;
#endif

  //
  // Init output buffer position to beginning of build buffer
  //

  out_pos = 0;
  last_out_pos = -1;
  
  //
  // adjust for mid-buffer start delay
  //
  
  if ( S->mix_delay )
  {
    out_pos = mult64anddiv( dest_buffer_size, S->mix_delay, S->driver->mix_ms );
    out_pos &= ~15;
    S->mix_delay = 0;
  }

  //
  // Merge source data into output buffer until source exhausted or output full
  //
  // Loop can be exited under the following conditions:
  //
  // 1) Output buffer full (normal condition)
  //
  // 2) Source sample ended (normal condition)
  //
  // 3) Source stream "starved" (abnormal condition)
  //

  while (out_pos < dest_buffer_size)
  {
    //
    // Set input len = size of source block to merge, based on
    // size and playback position of source sample
    //

    in_len = S->buf[S->tail].len - S->buf[S->tail].pos;

    //
    // Initial block may terminate before end of source buffer, if loop
    // endpoint has been declared
    //

    if (S->loop_count != 1)
    {
      if ((S->loop_end != -1) && ((U32) S->loop_end < S->buf[S->tail].len))
      {
        in_len = S->loop_end - S->buf[S->tail].pos;
      }
    }

    //
    // If no input data left, check for buffer switching and loop iteration
    //
    // (Note: in_len may fall below 0 if source data is undersampled!)
    //

    if (in_len <= 0) 
    {
      //
      // Set end-of-buffer function, if any, to signal end of loop block
      // or buffer
      //
      // Note that this function will be called repeatedly if buffer
      // starvation occurs
      //

      if (S->EOB != NULL)
        ++S->doeob;

      #ifndef MSS_SPU_PROCESS
        // reset the ASI, if requested 
        if (S->buf[S->tail].reset_ASI)
        {
          if (S->format & DIG_F_ADPCM_MASK)
          {
            S->adpcm.blockleft = 0;
            S->adpcm.extrasamples = 0;
          }
          else
          {
            reset_asi( S, -2, S->buf[ S->tail ].reset_seek_pos );
            S->buf[ S->tail ].reset_ASI = 0;
          }
        }
      #endif

      //
      // If looping active, go back to beginning of loop to fetch next
      // source data block
      //

      if (S->loop_count != 1)
      {
        //
        // Reset source sample position to beginning of loop
        //

#ifdef MSS_SPU_PROCESS
        spu_input = AIL_ptr_add( spu_input, (S32) S->loop_start - (S32)S->buf[S->tail].pos );
#endif

        if ( S->loop_start <= (S32)S->buf[S->tail].len )
          S->buf[S->tail].pos = S->loop_start;
        else
        {
          S->buf[S->tail].pos =  S->buf[S->tail].len;
          break;
        }

        //
        // Reset ADPCM offset to the end of decode buffer
        // to force a decode buffer refill
        //

        if (S->format & DIG_F_ADPCM_MASK)
        {
          S->adpcm.blockleft = 0;
          S->adpcm.extrasamples = 0;
        }

#ifndef MSS_SPU_PROCESS
        //
        // Reset ASI state, if applicable (on spu, already reset by ppu)
        //
        reset_asi( S, -2, S->buf[ S->tail ].pos );
#endif

        //
        // Decrement loop count if not infinite
        //

        if (S->loop_count != 0)
        {
          --S->loop_count;
        }

        //
        // Recalculate size and address of source block
        //

        continue;
      }


      //
      // If streaming sample and alternate buffer available,
      // set up to fetch next source data block from it
      //
      // Typically, buffer-switching takes place in the ASI fetch
      // callback function if an ASI decoder is in use.  We must
      // still be able to switch here, though, in case the callback
      // function returns at the end of its current buffer.
      //

      if (S->service_type == SERVICE_STREAMED)
      {

        next_buffer = (S->tail + 1) % S->n_buffers;

        //
        // If explicit 0-length buffer was posted, fall through to allow
        // the sample to terminate
        //

        if (!S->buf[next_buffer].done)
        {
          //
          // If alternate buffer not yet initialized, break out of loop
          // to allow application to supply data as soon as it can
          //

          if (!S->buf[next_buffer].len)
          {
            goto starved;
          }

          //
          // If alternate buffer already played, break out of loop to
          // allow application to refresh it as soon as possible
          //

          if (S->buf[next_buffer].pos)
          {
starved:
            S->starved=1;
            goto docbs;
          }

          //
          // Otherwise, alternate buffer is ready to play -- switch to
          // it and keep filling output buffer
          //

          S->tail = next_buffer;
#ifdef MSS_SPU_PROCESS
          spu_input = ( (U8*) spu_mem_init( &sms_input, input_buf, SPU_INPUT_SIZE + SPU_STREAM_EXTRA, S->buf[S->tail].start, S->buf[S->tail].len ) ) + S->buf[S->tail].pos;
#endif
          continue;
        }
      }

      //
      // If new input data is still not available after looping
      // and/or switching buffers, terminate the sample
      //

      S->driver->sample_status[S->index] = SMP_DONE;
      S->starved = 1;

      if (S->EOS != NULL)
         ++S->doeos;

      goto docbs;
    }

    //
    // Set to perform start-of-block callback
    //

    if (S->SOB != NULL)
       ++S->dosob;

    //
    // Merge source block with contents of output buffer, updating output
    // buffer position counter
    //
    // Note that buffer switching (but not loop iteration) may take place 
    // within this routine if an ASI decoder is in use!
    // 
    // If SS_merge() returns 0, it typically means that a single-buffered compressed sample
    // has reached EOS and requested ASI exhaustion, but no further decoded data was available.  
    // The first time this happens, we stay in the routine to check for loop iteration.  
    // If it happens twice in a row, we bail out
    // 

    out_pos = SS_merge(S,                       // sample handle
                       in_len,                  // max len of source block to copy
                       out_pos,                 // dest position in build buffer
                       dest_mono_sample_buffer, // optional output-filter buffer
                       dest_buffer_size);       // total size of output build buffer

    if ((out_pos == 0) && (last_out_pos == 0))
      {
      break;     
      }

    last_out_pos = out_pos;
  }

docbs:

  #ifndef MSS_SPU_PROCESS

  //
  // make the callbacks after everything's been merged
  //
 
  tmEnterAccumulationZone( miles_context, S->driver->sob_time );
  while (S->dosob--)
  {
    MSS_do_cb1( (AILSAMPLECB) ,S->SOB,S->driver->callingDS,
    S);
  }
  tmLeaveAccumulationZone( miles_context, S->driver->sob_time );

  tmEnterAccumulationZone( miles_context, S->driver->eob_time );
  while (S->doeob--)
  {
    MSS_do_cb1( (AILSAMPLECB) ,S->EOB,S->driver->callingDS,
    S);
  }
  tmLeaveAccumulationZone( miles_context, S->driver->eob_time );

  tmEnterAccumulationZone( miles_context, S->driver->eos_time );
  while (S->doeos--)
  {
    MSS_do_cb1( (AILSAMPLECB) ,S->EOS,S->driver->callingDS,
    S);
  }
  tmLeaveAccumulationZone( miles_context, S->driver->eos_time );

  #endif

  return out_pos;
}

//############################################################################
//##                                                                        ##
//## Copy data from source sample to build buffer, with mixing and ASI      ##
//## decompression                                                          ##
//##                                                                        ##
//############################################################################

static
void AILCALL SS_stream_and_mix(HSAMPLE S)
{
  S32 dest_buffer_size = S->driver->build[0].bytes;

   SS_stream_to_buffer(S,
                       NULL,
                       dest_buffer_size);
}


#define QUEUE_FLUSH       0
#define QUEUE_MP3_DECODE  1
#define QUEUE_BINK_DECODE 2
#define QUEUE_STREAM      3
#define QUEUE_COPY        4
#define QUEUE_NULL        5

#ifdef MSS_SPU_PROCESS

  HDIGDRIVER sdig;

  #include "radspu.h"
  #include "spumemstream.h"

  SPU_NAME( Phaser, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                             void * MSSRESTRICT  source_buffer,
                                             void * MSSRESTRICT  dest_buffer,
                                             S32          n_samples,
                                             MSS_SPEAKER  channel);

  SPU_NAME( BandPass, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                               void * MSSRESTRICT  source_buffer,
                                               void * MSSRESTRICT  dest_buffer,
                                               S32          n_samples,
                                               MSS_SPEAKER  channel);

  SPU_NAME( Chorus, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                             void * MSSRESTRICT  source_buffer,
                                             void * MSSRESTRICT  dest_buffer,
                                             S32          n_samples,
                                             MSS_SPEAKER  channel);

  SPU_NAME( Flange, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                             void * MSSRESTRICT  source_buffer,
                                             void * MSSRESTRICT  dest_buffer,
                                             S32          n_samples,
                                             MSS_SPEAKER  channel);

  SPU_NAME( HighPass, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                               void * MSSRESTRICT  source_buffer,
                                               void * MSSRESTRICT  dest_buffer,
                                               S32          n_samples,
                                               MSS_SPEAKER  channel);

  SPU_NAME( LowPass, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                              void * MSSRESTRICT  source_buffer,
                                              void * MSSRESTRICT  dest_buffer,
                                              S32          n_samples,
                                              MSS_SPEAKER  channel);

  SPU_NAME( AllPass, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                              void * MSSRESTRICT  source_buffer,
                                              void * MSSRESTRICT  dest_buffer,
                                              S32          n_samples,
                                              MSS_SPEAKER  channel);

  SPU_NAME( ParamEQ, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                              void * MSSRESTRICT  source_buffer,
                                              void * MSSRESTRICT  dest_buffer,
                                              S32          n_samples,
                                              MSS_SPEAKER  channel);

  SPU_NAME( GainBlock, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                                void * MSSRESTRICT  source_buffer,
                                                void * MSSRESTRICT  dest_buffer,
                                                S32          n_samples,
                                                MSS_SPEAKER  channel);

  SPU_NAME( ResonEQ, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                              void * MSSRESTRICT  source_buffer,
                                              void * MSSRESTRICT  dest_buffer,
                                              S32          n_samples,
                                              MSS_SPEAKER  channel);

  SPU_NAME( RingMod, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                              void * MSSRESTRICT  source_buffer,
                                              void * MSSRESTRICT  dest_buffer,
                                              S32          n_samples,
                                              MSS_SPEAKER  channel);

  SPU_NAME( ShelfEQ, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                              void * MSSRESTRICT  source_buffer,
                                              void * MSSRESTRICT  dest_buffer,
                                              S32          n_samples,
                                              MSS_SPEAKER  channel);

  SPU_NAME( VolRamp, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                              void * MSSRESTRICT  source_buffer,
                                              void * MSSRESTRICT  dest_buffer,
                                              S32          n_samples,
                                              MSS_SPEAKER  channel);

  SPU_NAME( Delay, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                            void * MSSRESTRICT  source_buffer,
                                            void * MSSRESTRICT  dest_buffer,
                                            S32          n_samples,
                                            MSS_SPEAKER  channel);

  #include "mp3decode.h" // @cdep ignore
  #include "bcfdec.h" // @cdep ignore
  
  static void call_spu_filter( int index,
                               HSAMPLESTATE state,
                               void * MSSRESTRICT  source_buffer,
                               void * MSSRESTRICT  dest_buffer,
                               S32          n_samples,
                               MSS_SPEAKER  channel)
  {
    switch( index )
    {
      case  0: Phaser_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case  1: BandPass_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case  2: Chorus_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case  3: Flange_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case  4: HighPass_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case  5: LowPass_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case  6: AllPass_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case  7: ParamEQ_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case  8: GainBlock_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case  9: ResonEQ_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case 10: RingMod_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case 11: ShelfEQ_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case 12: VolRamp_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
      case 13: Delay_FLTSMP_sample_process(state, source_buffer, dest_buffer, n_samples, channel ); break;
    }
  }

 
#ifdef DEBUGSPUMEM

  void * all_mals[ 1024 ];
  extern "C" U32 num_mals = 0;

  extern "C" void * spu_alloc( U32 bytes )
  {
    bytes = ( bytes + 15 ) & ~15;

    all_mals[ num_mals ] = AIL_API_mem_alloc_lock( bytes );
    ++num_mals;

    return all_mals[ num_mals - 1 ];
  }

  extern "C" void free_mals( int level )
  {
    U32 i;

    for ( i = level ; i < num_mals ; i++ )
      AIL_API_mem_free_lock( all_mals[ i ] );

    num_mals = level;
  }
 
#else 

  #define free_mals()

#endif

  static void do_mp3_frame( void * STR )
  {
    addstackmark();
 
    S32 asi_size = ( sizeof(ASISTREAM) + 15 ) & ~15;
    ASISTREAM * spu_dest = (ASISTREAM*) align_memcpy_from_PPU( spu_alloc( asi_size + SPU_EXTRA ), STR, asi_size, -1 );
 
    U8 * fb = spu_dest->frame_buffer;
    S32 fs = spu_dest->frame_size;
    S32 size = 32 * 18 * spu_dest->nch * spu_dest->ngr * sizeof(S16);

    spu_dest->frame_buffer = (U8*) align_spu_to_ppu( spu_alloc( size + SPU_EXTRA ), fb + fs );
    spu_dest->frame_size = 0;

    if ( L3_frame( spu_dest ) == 1 )
      RAD_memcpy_to_PPU( fb + fs, spu_dest->frame_buffer, size, SPU_DEFAULT_TO_PPU );
    else
      size = 0;
        
    spu_dest->samples = (S16*) AIL_ptr_add( fb, AIL_ptr_dif( spu_dest->samples, spu_dest->frame_buffer ) );
    spu_dest->frame_buffer = fb;
    spu_dest->frame_size = fs + size;

    // save the header for later
    spu_stream_ptr = (STR_header*) align_spu_to_ppu( spu_stream, STR );
    AIL_memcpy( spu_stream_ptr, spu_dest, 32 );
    
    RAD_memcpy_to_PPU( STR, spu_dest, asi_size - STREAM_BUFSIZE, -1 );
  }

  void AILCALL decode_bink_buffer( BcfStream* BcfStr );

  static void do_bink_frame( void * STR )
  {
    addstackmark();
 
    S32 asi_size = ( sizeof(BcfStream) + 15 ) & ~15;
    BcfStream * spu_dest = (BcfStream*) align_memcpy_from_PPU( spu_alloc( asi_size + SPU_EXTRA ), STR, asi_size, -1 );
 
    decode_bink_buffer( spu_dest );
    
    // save the header for later
    spu_stream_ptr = (STR_header*) align_spu_to_ppu( spu_stream, STR );
    AIL_memcpy( spu_stream_ptr, spu_dest, 32 );
    
    RAD_memcpy_to_PPU( STR, spu_dest, asi_size, -1 );
  }



  extern "C" void AILCALL MSS_mixer_mc_copy ( MSS_BB * build,
                                        S32 n_build_buffers,
                                        void * lpWaveAddr,
                                        S32 hw_format,
#ifdef IS_X86
                                        S32 use_MMX,
#endif
                                        S32 samples_per_buffer,
                                        S32 physical_channels_per_sample );


  static void AILCALL simpflush(S32 *dest, S32      len
  #ifdef IS_X86
                                           ,U32             MMX_available
  #endif
  )
  {
    AIL_memset( dest, 0, len );
  }


  static void AILCALL simpcopy(void const  *src,
                               S32       src_len,
                               void *dest,
                               U32       operation
#if defined(IS_BE) || defined(IS_X86) || defined(IS_PS2)
                              ,U32       option
#endif
                               )
  {
    addstackmark();
    HDIGDRIVER dig = sdig;
    void * spu_dest;

    U32 amt = ( dig->samples_per_buffer * dig->physical_channels_per_sample *
#ifdef IS_PS3
             4
#else
             2
#endif
             );

    spu_dest = (UINTa*) align_memcpy_from_PPU( spu_alloc( amt + SPU_EXTRA ), dest, amt, -1 );

    MSS_mixer_mc_copy( dig->build,
                       dig->n_build_buffers,
                       spu_dest,
                       dig->hw_format,
#ifdef IS_X86
                       dig->use_MMX,
#endif
                       dig->samples_per_buffer,
                       dig->physical_channels_per_sample );

    SINTa adj = ( (SINTa) dest ) & 15;
    RAD_memcpy_to_PPU( ( (U8*) dest ) - adj, ( (U8*) spu_dest ) - adj, ( amt + adj + 15 ) & ~15, -1 );
  }


  extern "C" void AILCALL MSS_mixer_mc_copy
                                       ( MSS_BB * build,
                                        S32 n_build_buffers,
                                        void * lpWaveAddr,
                                        S32 hw_format,
#ifdef IS_X86
                                        S32 use_MMX,
#endif
                                        S32 samples_per_buffer,
                                        S32 physical_channels_per_sample );

  static void AILCALL simpmccopy( MSS_BB * build,
                                        S32 n_build_buffers,
                                        void * lpWaveAddr,
                                        S32 hw_format,
#ifdef IS_X86
                                        S32 use_MMX,
#endif
                                        S32 samples_per_buffer,
                                        S32 physical_channels_per_sample )
  {
    addstackmark();
    void * spu_dest;

    U32 amt = ( samples_per_buffer * physical_channels_per_sample *
#ifdef IS_PS3
             4
#else
             2
#endif
             );

    spu_dest = (UINTa*) align_memcpy_from_PPU( spu_alloc( amt + SPU_EXTRA ), lpWaveAddr, amt, -1 );

    MSS_mixer_mc_copy( build,
                       n_build_buffers,
                       spu_dest,
                       hw_format,
#ifdef IS_X86
                       use_MMX,
#endif
                       samples_per_buffer,
                       physical_channels_per_sample );

    SINTa adj = ( (SINTa) lpWaveAddr ) & 15;
    RAD_memcpy_to_PPU( ( (U8*) lpWaveAddr ) - adj, ( (U8*) spu_dest ) - adj, ( amt + adj + 15 ) & ~15, -1 );
  }

  void * assign_chan_ptrs( HSAMPLE S, U8 * start_ptr, S32 sub_ptrs_first );

  static void spu_process( UINTa * ppu, UINTa size )
  {
    addstackmark();
    char * buffer = (char*) spu_alloc( size + SPU_EXTRA );
    char * digb = (char*) spu_alloc( sizeof( struct _DIG_DRIVER ) + SPU_EXTRA );
    char * sampb = (char*) spu_alloc( sizeof( struct _SAMPLE ) + SPU_EXTRA );
    char * status = (char*) spu_alloc( 256 * sizeof(U32) + SPU_EXTRA );
    char * extrasampb = (char*) spu_alloc( 2048 + SPU_EXTRA );
    UINTa * b;

    S32 i;
    MIXER_FLUSH saveflush = 0;
    MIXER_COPY savecopy = 0;
    MIXER_MC_COPY savemccopy = 0;

    MIXER_MERGE savemerge = 0;
    MIXER_ADPCM_DECODE saveadpcm = 0;

    U32 * savestatus=0;

    MSS_BB oldbuild[MAX_SPEAKERS+EXTRA_BUILD_BUFFERS];

    HDIGDRIVER dig = 0;
    HSAMPLE samp;

    b = (UINTa*) align_memcpy_from_PPU( buffer, ppu, size, -1 );

    for(;;)
    {
      UINTa c;

      c = b[ 0 ];
      switch ( c )
      {
        case QUEUE_FLUSH:
          dig = (HDIGDRIVER) align_memcpy_from_PPU( digb, (void*) b[ 1 ], sizeof( struct _DIG_DRIVER ), -1 );
          sdig = dig;

          AIL_memcpy( &oldbuild, dig->build, sizeof( dig->build ) );
          for( i = 0 ; i < MAX_SPEAKERS+EXTRA_BUILD_BUFFERS ; i++ )
          {
            if ( dig->build[ i ].buffer )
            {
              dig->build[ i ].buffer = (S32*) spu_alloc( dig->build[ i ].bytes + SPU_EXTRA );
              dig->build[ i ].buffer = (S32*) align_spu_to_ppu( dig->build[ i ].buffer, oldbuild[i].buffer );
            }
          }

          savestatus = dig->sample_status;
          dig->sample_status = (U32*) align_memcpy_from_PPU( status, savestatus, dig->n_samples * sizeof(U32), -1 );

          if ( dig->reverb[0].reverb_on )
          {
            savereverb = dig->reverb[0].reverb_build_buffer;
            savereverbhead = dig->reverb[0].reverb_head;
            spureverb = (S32*) spu_alloc( dig->reverb[0].reverb_fragment_size + SPU_EXTRA );
            dig->reverb[0].reverb_build_buffer = (S32*) align_spu_to_ppu( spureverb, AIL_ptr_add( savereverb, savereverbhead ) );
            dig->reverb[0].reverb_head = 0;
          }

          saveflush = dig->pipeline[DP_FLUSH].TYPE.MSS_mixer_flush;
          dig->pipeline[DP_FLUSH].TYPE.MSS_mixer_flush = simpflush;
          savecopy = dig->pipeline[DP_COPY].TYPE.MSS_mixer_copy;
          dig->pipeline[DP_COPY].TYPE.MSS_mixer_copy = simpcopy;
          savemccopy = dig->pipeline[DP_MC_COPY].TYPE.MSS_mixer_mc_copy;
          dig->pipeline[DP_MC_COPY].TYPE.MSS_mixer_mc_copy = simpmccopy;
          saveadpcm = dig->pipeline[DP_ADPCM_DECODE].TYPE.MSS_mixer_adpcm_decode;
          dig->pipeline[DP_ADPCM_DECODE].TYPE.MSS_mixer_adpcm_decode = MSS_mixer_adpcm_decode;

          SS_flush( dig, 0 );
          b += 2;
          break;

        case QUEUE_BINK_DECODE:
          do_bink_frame( (void*) b[ 4 ] );
          goto stream_it;

        case QUEUE_MP3_DECODE:
          do_mp3_frame( (void*) b[ 4 ] );
          // falls through to QUEUE_STREAM!!!

        case QUEUE_STREAM:
          {
            HDIGDRIVER savedig;
            U8* extra;
stream_it:
            if ( ( b[ 4 ] ) && ( c == QUEUE_STREAM ) )
            {
              spu_stream_ptr = (STR_header*) align_memcpy_from_PPU( spu_stream, (void*) b[ 4 ], 32, SPU_DEFAULT_FROM_PPU );
            }
              
            samp = (HSAMPLE) align_memcpy_from_PPU( sampb, (void*) b[ 1 ], sizeof( struct _SAMPLE ), SPU_DEFAULT_FROM_PPU );
            
            extra = (U8*) align_memcpy_from_PPU( extrasampb, (void*) b[ 2 ], b[ 3 ], -1 );

            savedig = samp->driver;

            samp->driver = dig;
            assign_chan_ptrs( samp, extra, 0 );

            savemerge = samp->pipeline[SP_MERGE].TYPE.MSS_mixer_merge;
            samp->pipeline[SP_MERGE].TYPE.MSS_mixer_merge = MSS_mixer_merge;

            SS_stream_and_mix( samp );

            samp->pipeline[SP_MERGE].TYPE.MSS_mixer_merge = savemerge;

            assign_chan_ptrs( samp, samp->chan_buf_ptr, 1 );
            samp->driver = savedig;

            RAD_memcpy_to_PPU( (void*) b[ 1 ], samp, sizeof( struct _SAMPLE ), SPU_DEFAULT_TO_PPU );
            if ( b[ 4 ] )
              RAD_memcpy_to_PPU( (void*) b[ 4 ], spu_stream_ptr, 32, SPU_DEFAULT_TO_PPU );
            RAD_memcpy_to_PPU( (void*) b[ 2 ], extra, b[ 3 ], -1 );

            b += 5;
          }
          break;

        case QUEUE_COPY:
          {
            SS_copy( dig, MAX_BUSSES, (void*) b[ 2 ] );

            dig->pipeline[DP_FLUSH].TYPE.MSS_mixer_flush = saveflush;
            dig->pipeline[DP_COPY].TYPE.MSS_mixer_copy = savecopy;
            dig->pipeline[DP_MC_COPY].TYPE.MSS_mixer_mc_copy = savemccopy;
            dig->pipeline[DP_ADPCM_DECODE].TYPE.MSS_mixer_adpcm_decode = saveadpcm;

            for( i = 0 ; i < MAX_SPEAKERS+EXTRA_BUILD_BUFFERS ; i++ )
            {
              if ( dig->build[ i ].buffer )
              {
                dig->build[ i ].buffer = oldbuild[i].buffer;
              }
            }

            RAD_memcpy_to_PPU( savestatus, dig->sample_status, dig->n_samples * sizeof(U32), SPU_DEFAULT_TO_PPU );
            dig->sample_status = savestatus;

            AIL_memcpy( dig->build, &oldbuild, sizeof( dig->build ) );
            RAD_memcpy_to_PPU( (void*) b[ 1 ], dig, sizeof( struct _DIG_DRIVER ), -1 );

            b += 3;
          }
          return;

        case QUEUE_NULL:
          dig = (HDIGDRIVER) align_memcpy_from_PPU( digb, (void*) b[ 1 ], sizeof( struct _DIG_DRIVER ), -1 );
          sdig = dig;

          AIL_memcpy( &oldbuild, dig->build, sizeof( dig->build ) );
          for( i = 0 ; i < MAX_SPEAKERS+EXTRA_BUILD_BUFFERS ; i++ )
          {
            if ( dig->build[ i ].buffer )
              dig->build[ i ].buffer = (S32*) align_memcpy_from_PPU( spu_alloc( dig->build[ i ].bytes + SPU_EXTRA ), dig->build[ i ].buffer, dig->build[ i ].bytes, SPU_DEFAULT_FROM_PPU );
          } 
 
          if ( dig->reverb[0].reverb_on )
          {
            savereverb = dig->reverb[0].reverb_build_buffer;
            savereverbhead = dig->reverb[0].reverb_head;
            spureverb = (S32*) spu_alloc( dig->reverb[0].reverb_fragment_size + SPU_EXTRA );
            dig->reverb[0].reverb_build_buffer = (S32*) align_memcpy_from_PPU( spureverb, AIL_ptr_add( savereverb, savereverbhead ), dig->reverb[0].reverb_fragment_size, SPU_DEFAULT_FROM_PPU );
            dig->reverb[0].reverb_head = 0; 
          }

          savestatus = dig->sample_status;
          dig->sample_status = (U32*) align_memcpy_from_PPU( status, savestatus, dig->n_samples * sizeof(U32), -1 );

          saveflush = dig->pipeline[DP_FLUSH].TYPE.MSS_mixer_flush;
          dig->pipeline[DP_FLUSH].TYPE.MSS_mixer_flush = simpflush;
          savecopy = dig->pipeline[DP_COPY].TYPE.MSS_mixer_copy;
          dig->pipeline[DP_COPY].TYPE.MSS_mixer_copy = simpcopy;
          savemccopy = dig->pipeline[DP_MC_COPY].TYPE.MSS_mixer_mc_copy;
          dig->pipeline[DP_MC_COPY].TYPE.MSS_mixer_mc_copy = simpmccopy;
          saveadpcm = dig->pipeline[DP_ADPCM_DECODE].TYPE.MSS_mixer_adpcm_decode;
          dig->pipeline[DP_ADPCM_DECODE].TYPE.MSS_mixer_adpcm_decode = MSS_mixer_adpcm_decode;

          b += 2;
          break;
      }
    }
  }

  extern "C" void RAD_main( void )
  {
    for(;;)
    {
      UINTa buf, size; 

      if ( RAD_receive_from_ppu( -1, &buf, &size ) )
      {
        addstackmark();

        if ( buf == 0 )
          return;

        unsigned long long start;
        start = RAD_timer_read();

        spu_process( (UINTa*) buf, size );

        RAD_send_to_ppu( buf, (U32) RAD_delta_timer_to_us( start, RAD_timer_read() ) );
      }
    }
  }

#elif defined( HOST_SPU_PROCESS )

  #include "radppu.h"

  extern "C"
  {
    static UINTa wait_for_spu_process( S32 spu_num, UINTa * buffer, UINTa size )
    {
      if ( RAD_send_to_spu( spu_num, (UINTa) buffer, size ) )
      {
        UINTa temp0, temp1;
        RAD_receive_from_spu( spu_num, -1, &temp0, &temp1 );
        
        return temp1;
      }

      return 0;
    }

    DXDEF S32 AILEXPORT MilesStartAsyncThread( S32 thread_num, void const * param )
    {
      return RAD_start_spu( thread_num, param );
    }

    DXDEF S32 AILEXPORT MilesRequestStopAsyncThread( S32 thread_num )
    {
      return RAD_stop_spu( thread_num );
    }

    DXDEF S32 AILEXPORT MilesWaitStopAsyncThread( S32 thread_num )
    {
      return RAD_wait_stop_spu( thread_num );
    }
  }

#endif

#ifdef IS_WII
  #include <revolution/os.h>
#endif

#ifndef MSS_SPU_PROCESS

//############################################################################
//##                                                                        ##
//## Fill output buffer with mixed data and/or silence                      ##
//##                                                                        ##
//############################################################################

void SS_fill(HDIGDRIVER dig, void *lpData)
{ 
   S32        cnt,n;
   HSAMPLE    S;
   U32       *status;
  
   U64 start_time = rrGetTime();

   //
   // spin up any background decodes (like xma)
   //
   
   tmEnter( miles_context, TMZF_NONE, "Miles mixing" );
#ifndef NTELEMETRY
   U64 start = tmGetAccumulationStart( miles_context );
#endif

   dig->adpcm_time = 0;
   dig->deinterlace_time = 0;
   dig->mix_time = 0;
   dig->rev_time = 0;
   dig->reformat_time = 0;
   dig->lowpass_time = 0;
   dig->filter_time = 0;
   dig->copy_time = 0;
   dig->sob_time = 0;
   dig->eob_time = 0;
   dig->eos_time = 0;
   dig->spu_wait_time = 0;
   dig->asi_times[0] = 0;
   dig->asi_times[1] = 0;
   dig->asi_times[2] = 0;
   dig->asi_times[3] = 0;

   dig->adpcm_sam = 0;
   dig->deinterlace_sam = 0;
   dig->mix_sam = 0;
   dig->rev_sam = 0;
   dig->reformat_sam = 0;
   dig->lowpass_sam = 0;
   dig->filter_sam = 0;
   dig->asi_nums[0] = 0;
   dig->asi_nums[1] = 0;
   dig->asi_nums[2] = 0;
   dig->asi_nums[3] = 0;

   dig->adpcm_num = 0;
   dig->deinterlace_num = 0;
   dig->mix_num = 0;
   dig->rev_num = 0;
   dig->reformat_num = 0;
   dig->lowpass_num = 0;
   dig->filter_num = 0;
   dig->asi_sams[0] = 0;
   dig->asi_sams[1] = 0;
   dig->asi_sams[2] = 0;
   dig->asi_sams[3] = 0;

   //
   // Call the user callback for the mix.
   //
   if (dig->mixcb)
       MSS_do_cb1( (AILMIXERCB) ,dig->mixcb, 0, dig);

   //
   // Run over the samples and see if any are scheduled to start.
   //
   if (dig->scheduled_sample_count)
   {
       U32 samples_per_mix = dig->hardware_buffer_size / (2 * dig->physical_channels_per_sample);

       U64 baseline_mix = dig->num_mixes * (U64)samples_per_mix;
       U64 will_mix_this_time = baseline_mix + samples_per_mix;

       for (n = dig->n_samples,S = &dig->samples[0],status = &dig->sample_status[0]; n; --n,++S,++status)
       {
           // If the sample is stopped.
           if (*status != SMP_DONE && *status != SMP_STOPPED)
               continue;

           // Check if the sample belongs to this level.
           if (S->bus != MAX_BUSSES && (S->state_level & (1 << dig->state_index)) == 0)
               continue;

           // Check if there is a schedule.
           if (S->mix_start_time == 0)
               continue;

           // Check if this sample needs to start this pass.
           if (S->mix_start_time >= will_mix_this_time)
               continue;

           // The sample needs to start - figure out the mix delay needed.
           // mix_delay is in 256ths of a ms.
           U64 delay_until_start = (U32)(S->mix_start_time - baseline_mix);
           if (S->mix_start_time < baseline_mix)
               delay_until_start = 0;

           delay_until_start <<= 8;
           delay_until_start = delay_until_start * 1000 / dig->DMA_rate;
           
           // Add to the mix delay they've already specified.
           S->mix_delay += (U32)delay_until_start;

           // Decrement the number of pending schedules.
           dig->scheduled_sample_count--;

           S->mix_start_time = 0;

           // start the sample
           U8 push_level = S->state_level;
           AIL_API_start_sample(S);
           S->state_level = push_level;
       }
   }

#ifdef HOST_SPU_PROCESS
   if ( ! dig->spu_on )
#endif
   {
     //
     // Force any asynchronous ASI decoders to prefetch their first data block
     //

     for (n = dig->n_samples,S = &dig->samples[0], status = dig->sample_status; n; --n,++S,++status)
     {
        //
        // Skip sample if not playing, not static, not newly-triggered (or looped), or
        // not ASI-based
        //
  
        if ((*status != SMP_PLAYING) || (S->buf[0].pos != 0) || (S->service_type != SERVICE_SINGLE_BUFFER) || (S->bus != MAX_BUSSES && S->pop_fade_total == 0 && (S->state_level & (1 << dig->state_index)) == 0))
        {
           continue;
        }

        SPINFO *SS = &S->pipeline[SP_ASI_DECODER];
  
        if (SS->active)
        {
          ASISTAGE *ASI = &SS->TYPE.ASI;
  
          // read the input data into the stream buffer
          ASI->ASI_stream_process(ASI->stream,
                                  NULL,
                                  0);

#ifndef HOST_SPU_PROCESS
          S32 n = S->tail;
          if ((S->buf[n].pos == S->buf[n].len) && (ASI->OUTPUT_RESERVOIR != (UINTa) -1))
          {
              S32 left = ~0;

              ASI->ASI_stream_property(ASI->stream, ASI->OUTPUT_RESERVOIR, &left, 0, 0);

              if (left > 0)
              {
                  S->buf[n].pos = S->buf[n].len - 1;
                  S->exhaust_ASI = 1;
              }
          }
#endif
        }
     }
   }

#ifdef HOST_SPU_PROCESS
   static RAD_ALIGN( UINTa, buffer[ 1024 ], 16 );
   UINTa * b = buffer;

   typedef char t1[ ( sizeof( *dig ) & 15 ) ? -1 : 1 ];
   typedef char t2[ ( sizeof( *S ) & 15 ) ? -1 : 1 ];

#endif

   // whether there are no busses, so we just forward straight to output.
   S32 SinglePass = dig->bus_samples[0] == 0;

   // reset the bus inputs to zero
   for (S32 b = 0; b < MAX_BUSSES; b++)
       if (dig->bus_samples[b])
           dig->bus_samples[b]->bus_comp_input = 0;

   cnt = 0;

   for (S32 bus = SinglePass ? MAX_BUSSES : 0; bus <= MAX_BUSSES; bus++)
   {
       // If the bus isn't initialised, skip.
       if (bus < MAX_BUSSES && dig->bus_samples[bus] == 0)
           continue;

       //
       // Flush build buffer with silence (or schedule it on the spu)
       //
#ifdef HOST_SPU_PROCESS

       b = buffer;
       S32 done_flush = 0;
       if ( dig->spu_on )
       {
           *b++ = (UINTa) QUEUE_FLUSH;
           *b++ = (UINTa) dig;
       }
       else
#endif
       {
           SS_flush(dig, SinglePass ? 0 : bus);
       }



       //
       // Merge active samples (if any) into build buffer
       //
       if (bus < MAX_BUSSES)
          dig->bus_active_count[bus] = 0;

       for (n = dig->n_samples,S = &dig->samples[0],status = &dig->sample_status[0]; n; --n,++S,++status)
       {
           //
           // Skip sample if stopped, finished, or not allocated
           //

           if (*status != SMP_PLAYING)
               continue;

           if (S->bus != MAX_BUSSES && S->pop_fade_total == 0 && (S->state_level & (1 << dig->state_index)) == 0)
               continue;

           // If we are single pass then we want all samples to mix.
           if (!SinglePass && S->bus != bus)
               continue;

           if (SinglePass)
               cnt++;
           else if (bus < MAX_BUSSES)
           {
                ++cnt;
                dig->bus_active_count[bus]++;
           }

           S->mix_bytes += dig->hardware_buffer_size;
           
           if ( S->mix_delay >= (U32) dig->mix_ms )
           {
               S->mix_delay -= dig->mix_ms;
               continue;
           }

           if (S->fade_to_stop == 2)
           {
               S->fade_to_stop = 0;
               AIL_API_end_sample(S); // not sure about this with 
               continue;
           }

           if (S->pop_fade_total)
           {
               S->pop_fade_time -= (S16)(dig->mix_ms >> 8);
               if (S->pop_fade_time < 0)
                   S->pop_fade_time = 0;
           }

           // Set the compressor input.
           if (bus == MAX_BUSSES)
           {
               if (S->bus_comp_installed)
               {
                    // Filter index is higher bits.
                    S32 FilterIndex = (S->bus_comp_installed >> 16);
                    S32 PropertyId = S->bus_comp_installed & 0xffff;

                    if (S->pipeline[FilterIndex].active)
                    {
                        FLTPROVIDER* Provider = S->pipeline[FilterIndex].TYPE.FLT.provider;
                        if (Provider && (Provider->provider_flags & FPROV_SIDECHAIN))
                        {
                            FLTSMP_SAMPLE_PROPERTY PropFn = Provider->sample_property;

                            for (S32 i = 0 ; i < S->n_channels ; i++)
                                PropFn( S->pipeline[FilterIndex].TYPE.FLT.sample_state[i], PropertyId, 0, &S->bus_comp_input, 0);
                        }
                    }
               }
           }

#ifdef HOST_SPU_PROCESS
           S->dosob=0;
           S->doeob=0;
           S->doeos=0;

           if ( ( dig->spu_on ) && ( S->spu_on ) ) 
           {

               //
               // In 3D mode, the channel levels are controlled by 3D positioning logic
               //

               if (S->driver->logical_channels_per_sample > 0)
               {
                   if (S->is_3D)
                   {
                       // if they have a falloff function, call it here (otherwise we do this on the spu too) 
                       if ( S->S3D.falloff_function )
                       {
                           SS_calculate_3D_channel_levels(&S->driver->D3D,
                               S->driver->logical_channels_per_sample,
                               &S->S3D,
                               S->auto_3D_channel_levels);
                       }
                   }
               }

               b[0] = (UINTa) QUEUE_STREAM;
               b[1] = (UINTa) S;
               b[2] = (UINTa) S->chan_buf_ptr;
               b[3] = (UINTa) S->chan_buf_used;
               b[4] = 0;

               SPINFO *SS = &S->pipeline[SP_ASI_DECODER];

               if (SS->active)
               {
                   ASISTAGE *ASI = &SS->TYPE.ASI;

                   b[4] = (UINTa) ASI->stream;

                   // reset the ASI, if requested
                   if (S->buf[S->tail].reset_ASI)
                   {
                       reset_asi( S, -2, S->buf[ S->tail ].reset_seek_pos );
                       S->buf[ S->tail ].reset_ASI = 0;
                   }

                   // read the input data into the stream buffer
                   U32 type = ASI->ASI_stream_process(ASI->stream,
                       NULL,
                       -1);

                   if ( type )
                   {
                       b[0] = (UINTa) ( QUEUE_MP3_DECODE - 1 + type );
                   }

                   // handle the end of the block (reseting the asi and moving back the stream pos)
                   S32 n = S->tail;
                   if (S->buf[n].pos == S->buf[n].len) 
                   {
                       if (S->loop_count != 1 )
                       {
                           reset_asi( S, -2, S->loop_start );
                       }

                       if (ASI->OUTPUT_RESERVOIR != (UINTa) -1)
                       {
                           S32 left = ~0;

                           ASI->ASI_stream_property(ASI->stream,
                               ASI->OUTPUT_RESERVOIR,&left,0,0);

                           if (left > 0)
                           {
                               S->buf[n].pos = S->buf[n].len - 1;
                               S->exhaust_ASI = 1;
                           }
                       }
                   }
               }

               b += 5;
           }
           else
           {
               if ( !done_flush )
               {
                   done_flush = 1;
                   buffer[ 0 ] = QUEUE_NULL;
                   SS_flush( dig, 0 );
               }

               SS_stream_and_mix(S);
           }
#else
           //
           // Convert sample to 16-bit signed format and mix with
           // contents of build buffer
           //
           // Note that SS_stream_and_mix() may invoke user callback functions
           // which may free or otherwise alter the sample being merged
           //
           // If ASI codec is in use, buffer maintenance can take place within
           // either SS_stream_and_mix() or the ASI fetch callback
           //

           SS_stream_and_mix(S);
#endif

           // If we are done fading out, end the sample.
           if (S->pop_fade_total && S->pop_fade_time == 0)
           {
               if (S->pop_fade_stop)
               {
                    if (S->system_data[STR_HSTREAM])
                    {
                        // need to end the stream.
                        HSTREAM stream = (HSTREAM)S->system_data[STR_HSTREAM];
                        AIL_pause_stream(stream, 1);
                        stream->was_popped = 1;
                    }
                    else
                        AIL_end_sample(S);
               }

               S->pop_fade_stop = 0;
               S->pop_fade_total = 0;
           }           
       } // end for each sample

       // Stop our bus sample if there's nothing on it, and there's no potential for reverb.
       if (bus < MAX_BUSSES && dig->bus_active_count[bus] == 0 && !dig->reverb[bus].reverb_on)
       {
           if (dig->sample_status[dig->bus_samples[bus]->index] == SMP_PLAYING)
               AIL_API_stop_sample(dig->bus_samples[bus]);
           continue;
       }

       //
       // Copy build buffer contents to bus buffer
       //

       // Do we route to a bus? Supposed to be a zero.
       if (dig->bus_samples[0] && bus < MAX_BUSSES)
       {
           SS_copy(dig, bus, dig->bus_ptrs[bus]);

           //
           // Find the sample used for this bus, set the buffers, and go.
           //
           HSAMPLE BusSample = dig->bus_samples[bus];
           AIL_API_load_sample_buffer(BusSample, MSS_BUFFER_HEAD, dig->bus_ptrs[bus], dig->samples_per_buffer * BusSample->n_channels * 2);

           // Resume only if something was playing, so that the sample will stop when reverb dies off.
           if (dig->bus_active_count[bus] && dig->sample_status[dig->bus_samples[bus]->index] != SMP_PLAYING)
               AIL_API_resume_sample(BusSample);
       }
       else
       {
           // Routing to output

#ifdef IS_WII

           if ( dig->physical_channels_per_sample == 1 )
           {
               SS_copy(dig, MAX_BUSSES, ( (U8*) dig->buffer[ 0 ] ) + (U32) lpData );
               DCFlushRange( ( (U8*) dig->buffer[ 0 ] ) + (U32) lpData, dig->snd_frag_size );
           }
           else
           {
               register U32 * MSSRESTRICT l;
               register U32 * MSSRESTRICT r;

               l = (U32*) ( ( (U8*) dig->buffer[ 0 ] ) + ( (U32) lpData >> 1 ) );
               r = (U32*) ( ( (U8*) dig->buffer[ 1 ] ) + ( (U32) lpData >> 1 ) );

               l[0] = (U32) r;
               SS_copy( dig, MAX_BUSSES, l );

               DCFlushRange( l, dig->snd_frag_size >> 1 );
               DCFlushRange( r, dig->snd_frag_size >> 1 );
           }

#else

#ifdef HOST_SPU_PROCESS
           if ( dig->spu_on )
           {
               *b++ = (UINTa) QUEUE_COPY;
               *b++ = (UINTa) dig;
               *b++ = (UINTa) lpData;
           }
           else
#endif
           {
               SS_copy(dig, MAX_BUSSES, lpData);
           }
#endif

       } // end if routing to output and not a bus.

   } // end for each bus


   //
   // Set number of active samples
   //
   dig->n_active_samples = cnt;
   ++dig->num_mixes;


#ifdef HOST_SPU_PROCESS
   if ( dig->spu_on )
   {
     U64 end_time=rrGetTime();

     start_time=end_time-start_time;

     dig->mix_total+=start_time;

     tmEnterAccumulationZone( miles_context, dig->spu_wait_time );
     U32 delta = wait_for_spu_process( dig->spu_num, buffer, AIL_ptr_dif( b, buffer ) );
     tmEnterAccumulationZone( miles_context, dig->spu_wait_time );

     start_time = rrGetTime();

     dig->spu_total += delta;

     //
     // do callbacks from ppu side
     //

     b = buffer;
     for(;;)
     {
       UINTa c;

       c = b[ 0 ];
       switch ( c )
       {
         case QUEUE_FLUSH:
           b += 2;
           break;

        case QUEUE_MP3_DECODE:
        case QUEUE_BINK_DECODE:
        case QUEUE_STREAM:
          S = (HSAMPLE) b[ 1 ];

          //
          // make the callbacks after everything's been merged
          //

          tmEnterAccumulationZone( miles_context, S->driver->sob_time );
          while (S->dosob--)
          {
            MSS_do_cb1( (AILSAMPLECB) ,S->SOB,S->driver->callingDS,
            S);
          }
          tmLeaveAccumulationZone( miles_context, S->driver->sob_time );

          tmEnterAccumulationZone( miles_context, S->driver->eob_time );
          while (S->doeob--)
          {
            MSS_do_cb1( (AILSAMPLECB) ,S->EOB,S->driver->callingDS,
            S);
          }
          tmLeaveAccumulationZone( miles_context, S->driver->eob_time );

          tmEnterAccumulationZone( miles_context, S->driver->eos_time );
          while (S->doeos--)
          {
            MSS_do_cb1( (AILSAMPLECB) ,S->EOS,S->driver->callingDS,
            S);
          }
          tmLeaveAccumulationZone( miles_context, S->driver->eos_time );
          b += 5;
          break;

        case QUEUE_COPY:
          goto donecb;

        case QUEUE_NULL:
          b += 2;
          break;
      }
    }

    donecb:;
   }
#endif

#ifndef NTELEMETRY
   if ( miles_context )
   {
     if ( dig->adpcm_num ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->adpcm_num, dig->adpcm_time, "ADCPM decompression" ); }
     if ( dig->deinterlace_num ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->deinterlace_num, dig->deinterlace_time, "Input deinterlace" ); }
     if ( dig->mix_num ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->mix_num, dig->mix_time, "Mix" ); }
     if ( dig->rev_num ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->rev_num, dig->rev_time, "Reverb mix" ); }
     if ( dig->reformat_num ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->reformat_num, dig->reformat_time, "Format conversion" ); }
     if ( dig->lowpass_num ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->lowpass_num, dig->lowpass_time, "Low-pass" ); }
     if ( dig->filter_num ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->filter_num, dig->filter_time, "DSP filters" ); }
     if ( dig->copy_time ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, 0, dig->copy_time, "Copy to final output" ); }
     if ( dig->sob_time ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, 0, dig->sob_time, "Start of buffer callbacks" ); }
     if ( dig->eob_time ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, 0, dig->eob_time, "End of buffer callbacks" ); }
     if ( dig->eos_time ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, 0, dig->eos_time, "End of sample callbacks" ); }
     if ( dig->spu_wait_time ) { tmEmitAccumulationZone( miles_context, TMZF_IDLE, &start, 0, dig->spu_wait_time, "Waiting on SPU" ); }
     if ( dig->asi_nums[0] ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->asi_nums[0], dig->asi_times[0], "Other decompression" ); }
     if ( dig->asi_nums[1] ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->asi_nums[1], dig->asi_times[1], "Bink Audio decompression" ); }
     if ( dig->asi_nums[2] ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->asi_nums[2], dig->asi_times[2], "MP3 decompression" ); }
     if ( dig->asi_nums[3] ) { tmEmitAccumulationZone( miles_context, TMZF_NONE, &start, dig->asi_nums[3], dig->asi_times[3], "Ogg decompression" ); }

     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->adpcm_num, "Miles/ADPCM (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->deinterlace_num, "Miles/Input deinterlace (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->mix_num, "Miles/Mix (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->rev_num, "Miles/Reverb mix (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->reformat_num, "Miles/Format conversion (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->lowpass_num, "Miles/Low-pass (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->filter_num, "Miles/DSP (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->asi_nums[0], "Miles/Other codec (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->asi_nums[1], "Miles/Bink Audio (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->asi_nums[2], "Miles/MP3 (samples)");
     tmPlot(miles_context, TMPT_INTEGER, TMPF_NONE, (F32) dig->asi_nums[3], "Miles/Ogg (samples)");
     
     tmLeave( miles_context );
   }
#endif
   //
   // keep the profiling information
   //

   U64 end_time=rrGetTime();

   start_time=end_time-start_time;

   dig->mix_total+=start_time;
}

#endif


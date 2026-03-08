//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  API.CPP: FLT module for Stereo Delay                                  ##
//##                                                                        ##
//##  Author: John Miles, Nick Skrepetos                                    ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#include "mss.h"
#include "imssapi.h"

#define FILTER_NAME "Delay Filter"

#define FLT_F_DELAY_FEEDFORWARD  "Delay Feedforward"
#define FLT_F_DELAY_MAX_TIME     "Max Delay Time"
#define FLT_F_DELAY_FEEDBACK     "Delay Feedback"
#define FLT_F_DELAY_TIME         "Delay Time"
#define FLT_F_WET_LEVEL "Wet Level"
#define FLT_F_DRY_LEVEL "Dry Level"

#define PROVIDER_SPU_INDEX 14


// Stereo Delay Defaults
#define  _FX_ABSMAX_DELAY        10000.0F
#define  _FX_ABSMIN_DELAY        0.005F

#define  _FX_DEFAULT_MAX_DELAY   300.0F
#define  _FX_DEFAULT_DELAY       200.0F
#define  _FX_DEFAULT_FEEDBACK    0.5F
#define  _FX_DEFAULT_FEEDFORWARD 0.0F
#define  _FX_WET_DEFAULT         0.5F
#define  _FX_DRY_DEFAULT         0.5F

//
// Attribute tokens
//

enum PROP
{
   //
   // Additional filter attribs (beyond standard RIB PROVIDER_ attributes)
   //

   _FX_PROVIDER_FLAGS,
   _FX_PROVIDER_DRIVER_STATE_SIZE,
   _FX_PROVIDER_SAMPLE_STATE_SIZE,

   _FX_WET,
   _FX_DRY,
   _FX_MAX_DELAY,
   _FX_MDELAY_DELAY,
   _FX_MDELAY_FEEDBACK,
   _FX_MDELAY_FEEDFORWARD
};

//
// Driver state descriptor
//
// One state descriptor is associated with each HDIGDRIVER
//

struct DRIVERSTATE
{
   HDIGDRIVER dig;
   MSS_ALLOC_TYPE * alloc_CB;
   MSS_FREE_TYPE  *  free_CB;
   UINTa            user;
};

//
// Per-sample filter state descriptor
//
// One state descriptor is associated with each HSAMPLE
//

struct SAMPLESTATE
{
   //
   // Members common to all pipeline filter providers
   //

   HSAMPLE          sample;   // HSAMPLE with which this descriptor is associated
   DRIVERSTATE *driver;   // Driver with which this descriptor is associated

   //
   // Members associated with specific filter provider
   //

   S32       calculated_rate;

   F32       fMaxDelay;

   F32       fDelay;
   F32       fFeedback;
   F32       fFeedforward;
   F32 fWet, fDry;

   // pointers to delay buffers
   F32   *   pfDelayBuffer;

   // total bytes in delay buffer, and current index
   U32       dwBufferBytes;
   U32       dwDelayIndex;
};

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#include "common.inl"

#ifdef MSS_SPU_PROCESS
  #include "radspu.h"
  #include "spumemstream.h"
#endif

//############################################################################
//#                                                                          #
//#  Process sample data                                                     #
//#                                                                          #
//#  Parameters:                                                             #
//#                                                                          #
//#    state is the sample descriptor.  You can retrieve the HSAMPLE via     #
//#    the state.sample member, if needed.                                   #
//#                                                                          #
//#    source_buffer is a pointer to the a stereo or mono 16-bit sample      #
//#    buffer.                                                               #
//#                                                                          #
//#    n_samples is the number of samples (either mono or stereo) to         #
//#    process.                                                              #
//#                                                                          #
//#    dest_buffer is the destination 16-bit sample buffer.                  #
//#                                                                          #
//#    dest_playback_rate is the hardware sample rate.  Filters must watch   #
//#    for changes in the playback rate and recalculate any dependent        #
//#    parameters.                                                           #
//#                                                                          #
//#    channel tells you which input channel this is                         #
//#                                                                          #
//############################################################################

SPU_NAME( Delay, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                          S16  * MSSRESTRICT  source_buffer,
                                          S16  * MSSRESTRICT  dest_buffer,
                                          S32          n_samples,
                                          MSS_SPEAKER  channel)
{
   SAMPLESTATE *SSp  = (SAMPLESTATE *) state;
   SAMPLESTATE SS;
   AIL_memcpy(&SS, SSp, sizeof(SS));
   //HSAMPLE          S   = SS.sample;
   //DRIVERSTATE *DRV = SS.driver;

   S32 dwIndex;
   F32 fInput;
   F32 fOut;
   F32 delOut;

   F32 fRate = (F32) SS.calculated_rate;

   U32 dwDelayLength =  ( (U32) (S32) (( SS.fDelay * fRate ) / 1000.0F) ) & ~31;

   if (dwDelayLength >= ( SS.dwBufferBytes / sizeof(F32) ) ) dwDelayLength = ( SS.dwBufferBytes / sizeof(F32) ); // (in case the sample playback rate

   // set wet/dry mix
   F32 fDryOut  =  SS.fDry;
   F32 fWetOut  =  SS.fWet;

   //fast path
   if ( fDryOut > 0.999f )
   {
     if ( source_buffer != dest_buffer )
       AIL_memcpy( dest_buffer, source_buffer, n_samples * 2 );
     return;
   }

#ifdef MSS_SPU_PROCESS
   S32 chunk1, chunk2;

   U32 saveindex = SS.dwDelayIndex;
   F32 * savedelay = SS.pfDelayBuffer;

   SS.pfDelayBuffer = (F32*) spu_alloc( n_samples * sizeof( F32 ) + SPU_EXTRA );
   SS.pfDelayBuffer = (F32*) align_spu_to_ppu( SS.pfDelayBuffer, savedelay );

   chunk1 = dwDelayLength - SS.dwDelayIndex;
   if ( chunk1 > n_samples )
     chunk1 = n_samples;

   chunk2 = n_samples - chunk1;

   if ( chunk2 )
     RAD_memcpy_from_PPU( SS.pfDelayBuffer+ chunk1, savedelay, chunk2 * sizeof( F32 ), SPU_DEFAULT_FROM_PPU );
   RAD_memcpy_from_PPU( SS.pfDelayBuffer, savedelay + SS.dwDelayIndex, chunk1 * sizeof( F32 ), -1 );

   SS.dwDelayIndex = 0;
#endif

    for( dwIndex = 0; dwIndex < n_samples; dwIndex++ )
    {
       // get input sample
       fInput   =  (F32)(S16)LE_SWAP16(source_buffer);
       ++source_buffer;

       // apply feedforward
       delOut = SS.pfDelayBuffer[ SS.dwDelayIndex ] + (fInput * -SS.fFeedforward) + _FX_DENORMVAL;

       // set output
       fOut  =  ( fInput * fDryOut ) + ( delOut * fWetOut );

       // calculate output sample
       SS.pfDelayBuffer[ SS.dwDelayIndex ]  =  fInput + ( delOut * SS.fFeedback ) + _FX_DENORMVAL;

#ifdef MSS_SPU_PROCESS
       ++SS.dwDelayIndex;
#else
       // advance index
       if ( ++SS.dwDelayIndex >= dwDelayLength )
          SS.dwDelayIndex  =  0;
#endif

       // store output
       WRITE_MONO_SAMPLE( dest_buffer, fOut );
    }

#ifdef MSS_SPU_PROCESS

     if ( chunk2 )
       RAD_memcpy_to_PPU( savedelay, SS.pfDelayBuffer + chunk1, chunk2 * sizeof( F32 ), SPU_DEFAULT_TO_PPU );

     SINTa adj = ( (SINTa) ( savedelay + saveindex ) ) & 15;
     RAD_memcpy_to_PPU( ( (U8*) ( savedelay + saveindex ) ) - adj, ( (U8*) SS.pfDelayBuffer ) - adj, ( ( chunk1 * sizeof( F32 ) ) + adj + 15 ) & ~15, -1 );

     SS.pfDelayBuffer = savedelay;

     SS.dwDelayIndex = saveindex + n_samples;
     if ( SS.dwDelayIndex >= dwDelayLength )
       SS.dwDelayIndex -= dwDelayLength;

#endif

   AIL_memcpy(SSp, &SS, sizeof(*SSp));
}

#ifndef MSS_SPU_PROCESS

//############################################################################
//#                                                                          #
//# Allocate buffers for specified delay                                     #
//#                                                                          #
//############################################################################

static void FXSetMaxDelay(SAMPLESTATE *SS, //)
                          S32              playback_rate,
                          F32              delay_ms)
{
   // get sample rate
   SS->calculated_rate = playback_rate;
   SS->fMaxDelay       = delay_ms;

   FX_CLIPRANGE( SS->fMaxDelay, _FX_ABSMIN_DELAY, _FX_ABSMAX_DELAY);
   FX_CLIPRANGE( SS->fDelay,   _FX_ABSMIN_DELAY, SS->fMaxDelay);

   // convert MS to samples
   U32 dwSamples = ((U32)(S32) (( SS->fMaxDelay * F32(playback_rate) ) / 1000.0F)) & ~31;

   // set buffer size plus some padding for overflow safety
   SS->dwBufferBytes = (dwSamples * sizeof(F32)) + 256;

   if (SS->pfDelayBuffer != NULL)
      {
      SS->driver->free_CB( SS->pfDelayBuffer,SS->driver->user,__FILE__,__LINE__);
      }

   SS->pfDelayBuffer =  (F32 *)SS->driver->alloc_CB( SS->dwBufferBytes,SS->driver->user,__FILE__,__LINE__);

   // clear buffer
   AIL_memset( SS->pfDelayBuffer, 0, SS->dwBufferBytes );

   // reset buffer index
   SS->dwDelayIndex =  0;
}

//############################################################################
//#                                                                          #
//# Calculate coefficients and values based on parameter set                 #
//#                                                                          #
//############################################################################

static void  FXCalcParams( SAMPLESTATE * SS, S32 playback_rate )
{
}

static void init_sample( SAMPLESTATE * SS )
{
   //
   // Initialize provider-specific members to their default values
   //

   // set default values
   SS->fMaxDelay    =  _FX_DEFAULT_MAX_DELAY;
   SS->fDelay       =  _FX_DEFAULT_DELAY;
   SS->fFeedback    =  _FX_DEFAULT_FEEDBACK;
   SS->fFeedforward =  _FX_DEFAULT_FEEDFORWARD;
   SS->fWet         =  _FX_WET_DEFAULT;
   SS->fDry         =  _FX_DRY_DEFAULT;

   FXSetMaxDelay(SS, SS->driver->dig->DMA_rate, SS->fMaxDelay);

   // calculate params
   FXCalcParams( SS, SS->driver->dig->DMA_rate );
}

static void close_sample( SAMPLESTATE * SS )
{
  if (SS->pfDelayBuffer != NULL)
  {
    SS->driver->free_CB( SS->pfDelayBuffer,SS->driver->user,__FILE__,__LINE__);
    SS->pfDelayBuffer = NULL;
  }
}

//############################################################################
//#                                                                          #
//# Set sample preference value, returning previous setting                  #
//#                                                                          #
//############################################################################

static S32 AILCALL FLTSMP_sample_property(HSAMPLESTATE state, HPROPERTY property,void * before_value, void const * new_value, void * after_value)
{
   SAMPLESTATE *SS  = (SAMPLESTATE *) state;
   //HSAMPLE          S   = SS->sample;

   // determine preference
   switch (property)
      {
      //
      // Preferences
      //
      case  _FX_MAX_DELAY :
       if ( before_value ) *(F32*)before_value = SS->fMaxDelay;
       if ( new_value )
       {
         SS->fMaxDelay = *(F32 const*)new_value;
         // clip to valid range
         FXSetMaxDelay(SS, SS->driver->dig->DMA_rate, SS->fMaxDelay);
         FXCalcParams(SS, SS->driver->dig->DMA_rate);
       }
       if ( after_value ) *(F32*)after_value = SS->fMaxDelay;
       return 1;

      case  _FX_MDELAY_DELAY     :
       if ( before_value ) *(F32*)before_value = SS->fDelay;
       if ( new_value )
       {
         SS->fDelay = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fDelay, _FX_ABSMIN_DELAY, SS->fMaxDelay);
         FXCalcParams(SS, SS->driver->dig->DMA_rate);
       }         
       if ( after_value ) *(F32*)after_value = SS->fDelay;
       return 1;

      case  _FX_MDELAY_FEEDBACK     :
       if ( before_value ) *(F32*)before_value = SS->fFeedback;
       if ( new_value ) 
       {
         SS->fFeedback = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fFeedback, 0.0F, 1.0F );
       }         
       if ( after_value ) *(F32*)after_value = SS->fFeedback;
       return 1;

      case  _FX_MDELAY_FEEDFORWARD    :
       if ( before_value ) *(F32*)before_value = SS->fFeedforward;
       if ( new_value ) 
       {
         SS->fFeedforward = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fFeedforward, 0.0F, 1.0F );
       }         
       if ( after_value ) *(F32*)after_value = SS->fFeedforward;
       return 1;

     case _FX_WET:
       if ( before_value ) *(F32*)before_value = SS->fWet;
       if ( new_value ) 
       {
         SS->fWet = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fWet, 0.0F, 1.0F );
       }         
       if ( after_value ) *(F32*)after_value = SS->fWet;
       return 1;

     case _FX_DRY:
       if ( before_value ) *(F32*)before_value = SS->fDry;
       if ( new_value ) 
       {
         SS->fDry = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fDry, 0.0F, 1.0F );
       }         
       if ( after_value ) *(F32*)after_value = SS->fDry;
       return 1;

      }


   return 0;
}

extern "C" S32 DelayMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern "C" S32 DelayMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg )
{
   const RIB_INTERFACE_ENTRY FLT1[] =
      {
      REG_FN(PROVIDER_property),
      REG_PR("Name",                     PROVIDER_NAME,        (RIB_DATA_SUBTYPE) (RIB_STRING|RIB_READONLY)),
      REG_PR("Version",                  PROVIDER_VERSION,     (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),
      REG_PR("Flags",                   _FX_PROVIDER_FLAGS,    (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),
      };

   const RIB_INTERFACE_ENTRY FLT2[] =
      {
      REG_FN(FLT_startup),
      REG_FN(FLT_error),
      REG_FN(FLT_shutdown),
      REG_FN(FLT_open_driver),
      };

   const RIB_INTERFACE_ENTRY FLT3[] =
      {
      REG_FN(FLT_close_driver),
      REG_FN(FLT_premix_process),
      REG_FN(FLT_postmix_process),
      };

   const RIB_INTERFACE_ENTRY FLT4[] =
      {
      REG_PR("Sample_state_size",       _FX_PROVIDER_SAMPLE_STATE_SIZE, (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),
      REG_PR("Driver_state_size",       _FX_PROVIDER_DRIVER_STATE_SIZE, (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),
      };

   const RIB_INTERFACE_ENTRY FLTSMP1[] =
      {
      REG_FN(FLTSMP_open_sample),
      REG_FN(FLTSMP_close_sample),
      REG_FN(FLTSMP_sample_process),
      REG_FN(FLTSMP_sample_property),
      };

   const RIB_INTERFACE_ENTRY FLTSMP2[] =
      {
      REG_PR(FLT_F_DELAY_TIME,        _FX_MDELAY_DELAY  ,       RIB_FLOAT),
      REG_PR(FLT_F_DELAY_FEEDBACK,    _FX_MDELAY_FEEDBACK  ,    RIB_FLOAT),
      REG_PR(FLT_F_DELAY_FEEDFORWARD, _FX_MDELAY_FEEDFORWARD ,  RIB_FLOAT),
      };

   const RIB_INTERFACE_ENTRY FLTSMP3[] =
      {
      REG_PR(FLT_F_DELAY_MAX_TIME,    _FX_MAX_DELAY ,           RIB_FLOAT),
      REG_PR(FLT_F_WET_LEVEL,              _FX_WET,                   RIB_FLOAT),
      REG_PR(FLT_F_DRY_LEVEL,              _FX_DRY,                   RIB_FLOAT),
      };

   if (up_down)
      {
         RIB_registerP(provider_handle, "MSS pipeline filter", FLT1);
         RIB_registerP(provider_handle, "MSS pipeline filter", FLT2);
         RIB_registerP(provider_handle, "MSS pipeline filter", FLT3);
         RIB_registerP(provider_handle, "MSS pipeline filter", FLT4);

         RIB_registerP(provider_handle, "Pipeline filter sample services", FLTSMP1);
         RIB_registerP(provider_handle, "Pipeline filter sample services", FLTSMP2);
         RIB_registerP(provider_handle, "Pipeline filter sample services", FLTSMP3);
      }
   else
      {
         RIB_unregister_allP(provider_handle);
      }

   return TRUE;
}

#endif

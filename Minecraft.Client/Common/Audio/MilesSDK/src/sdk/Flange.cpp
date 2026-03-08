//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  API.CPP: FLT module for Flanger                                       ##
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

#define FILTER_NAME "Flange Filter"

#define FLT_F_FLANGE_DEPTH      "Flange Depth"
#define FLT_F_FLANGE_FEEDBACK   "Flange Feedback"
#define FLT_F_FLANGE_SWEEPRATE  "Flange SweepRate"
#define FLT_F_WET_LEVEL "Wet Level"
#define FLT_F_DRY_LEVEL "Dry Level"

#define PROVIDER_SPU_INDEX 4

// Chorus / Flange Buffer
#define  _FX_CHORUS_BUFFER    4096L

// Flange Defaults
#define  _FX_FEEDBACK_DEFAULT    0.7F
#define  _FX_RATE_DEFAULT        2.0F
#define  _FX_DEPTH_DEFAULT       6.0F
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
   _FX_FLANGE_FEEDBACK,
   _FX_FLANGE_RATE,
   _FX_FLANGE_DEPTH
};

//
// Driver state descriptor
//
// One state descriptor is associated with each HDIGDRIVER
//

struct DRIVERSTATE
{
   //
   // Members common to all pipeline filter providers
   //

   HDIGDRIVER dig;                  // Driver with which this descriptor is associated
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
   DRIVERSTATE *    driver;   // Driver with which this descriptor is associated

   //
   // Members associated with specific filter provider
   //

   F32 fWet, fDry;
   F32       fDepth;
   F32       fSweepRate;
   F32       fFeedback;

   S32       wFp;
   S32       wEp1;
   S32       wEp2;
   F32       MinSweep;
   F32       MaxSweep;
   F32       Sweep;
   F32       Step;

   F32       * pfDelayBuffer;
};

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

SPU_NAME( Flange, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                           S16  * MSSRESTRICT  source_buffer,
                                           S16  * MSSRESTRICT  dest_buffer,
                                           S32          n_samples,
                                           MSS_SPEAKER  channel)
{
   SAMPLESTATE *SSp  = (SAMPLESTATE *) state;
   SAMPLESTATE SS;
   AIL_memcpy( &SS, SSp, sizeof(SS) );
   //HSAMPLE          S   = SS.sample;
   //DRIVERSTATE *DRV = SS.driver;

   S32 dwIndex;
   F32 fInputL;
   F32 fOutL;
   
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
    F32 * savedelay = SS.pfDelayBuffer;
    S32 savefp, saveep;

    SS.pfDelayBuffer = (F32*) spu_alloc( _FX_CHORUS_BUFFER * sizeof(F32) + SPU_EXTRA );
    SS.pfDelayBuffer = (F32*) align_spu_to_ppu( SS.pfDelayBuffer, savedelay );

    savefp = SS.wFp * sizeof( F32 );
    wrap_copy_to_spu( SS.pfDelayBuffer, savedelay, savefp, n_samples * sizeof(F32), _FX_CHORUS_BUFFER * sizeof(F32), SPU_DEFAULT_TO_SPU );

    S32 extra = (S32) ( SS.Step * (F32) n_samples );
    if ( extra < 0 ) extra = -extra;
    extra = ( n_samples + extra + 2 ) * sizeof(F32);
    saveep = SS.wEp1 * sizeof( F32 );
    wrap_copy_to_spu( SS.pfDelayBuffer, savedelay, saveep, extra, _FX_CHORUS_BUFFER * sizeof(F32), -1 );

#endif

    // mix into build buffer
    for( dwIndex = 0; dwIndex < n_samples; dwIndex++ )
    {
       S32 next;

       // get input sample
       fInputL = (F32) (S16) LE_SWAP16(source_buffer);
       ++source_buffer;

       next = (S32) ( SS.Sweep + SS.Step );

       // calculate interpolation

       fOutL = SS.pfDelayBuffer[ SS.wEp1 ] + ( ( SS.pfDelayBuffer[ SS.wEp2 ] - SS.pfDelayBuffer[ SS.wEp1 ] ) * ( SS.Sweep - AIL_floor( SS.Sweep ) ) );

       // set output sample
       SS.pfDelayBuffer[ SS.wFp ]  =  fInputL + ( fOutL * SS.fFeedback ) + _FX_DENORMVAL;
       SS.wFp = ( SS.wFp + 1 ) & ( _FX_CHORUS_BUFFER - 1 );

       // calculate new output
       fOutL = ( fOutL * fWetOut ) + ( fInputL * fDryOut );

       // adjust sweep
       SS.Sweep += SS.Step;

       // calculate buffer positions
       SS.wEp1  =  ( SS.wFp + next ) & ( _FX_CHORUS_BUFFER - 1 );
       SS.wEp2  =  ( SS.wFp + next + 1 ) & ( _FX_CHORUS_BUFFER - 1 );

       // check for sweep reversal
       if ( ( SS.Sweep >= SS.MaxSweep ) || ( SS.Sweep <= SS.MinSweep ) )
         SS.Step = -SS.Step;

       // store output
       WRITE_MONO_SAMPLE( dest_buffer, fOutL );
    }

#ifdef MSS_SPU_PROCESS
   wrap_copy_to_ppu( savedelay, SS.pfDelayBuffer, savefp, n_samples * sizeof( F32), _FX_CHORUS_BUFFER * sizeof(F32), SPU_DEFAULT_TO_PPU );
   wrap_copy_to_ppu( savedelay, SS.pfDelayBuffer, saveep, extra, _FX_CHORUS_BUFFER * sizeof(F32), -1 );
   SS.pfDelayBuffer = savedelay;
#endif

   AIL_memcpy( SSp, &SS, sizeof(SS) );
}


#ifndef MSS_SPU_PROCESS

//############################################################################
//#                                                                          #
//# Calculate coefficients and values based on parameter set                 #
//#                                                                          #
//############################################################################

static void  FXCalcParams( SAMPLESTATE * SS, S32 playback_rate )
{
   F32 fRate;

   // get sample rate
   fRate = ( (F32) playback_rate) / 1000.f;

   F32 Depth = SS->fDepth * fRate;

      // set parameters
   SS->Step = SS->fSweepRate / 1000.f;

   // set min/max sweep
   SS->MaxSweep = _FX_CHORUS_BUFFER - 2;
   SS->MinSweep = SS->MaxSweep - Depth;

   // clip
   if ( SS->MinSweep < 0 )
      SS->MinSweep = 0;

   // set sweep
   SS->Sweep = ( SS->MinSweep + SS->MaxSweep ) / 2;

   SS->wEp1  =  ( SS->wFp + (S32) ( SS->Sweep + SS->Step ) ) & ( _FX_CHORUS_BUFFER - 1 );
   SS->wEp2  =  ( SS->wFp + (S32) ( SS->Sweep + SS->Step ) + 1 ) & ( _FX_CHORUS_BUFFER - 1 );
}


static void init_sample( SAMPLESTATE * SS )
{
   //
   // Initialize provider-specific members to their default values
   //

   // set flange defaults
   SS->fFeedback     =  _FX_FEEDBACK_DEFAULT;
   SS->fSweepRate    =  _FX_RATE_DEFAULT;
   SS->fDepth        =  _FX_DEPTH_DEFAULT;
   SS->fWet          =  _FX_WET_DEFAULT;
   SS->fDry          =  _FX_DRY_DEFAULT;

   // update params
   SS->wFp   =  0;
   FXCalcParams( SS, SS->driver->dig->DMA_rate );

   // allocate delay buffer
   SS->pfDelayBuffer =  (F32 *)SS->driver->alloc_CB( _FX_CHORUS_BUFFER * sizeof( F32 ),SS->driver->user,__FILE__,__LINE__);

   // clear buffer
   AIL_memset( SS->pfDelayBuffer, 0, _FX_CHORUS_BUFFER * sizeof( F32 ));
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

      case  _FX_FLANGE_DEPTH        :
       if ( before_value ) *(F32*)before_value = SS->fDepth;
       if ( new_value ) 
       {
         SS->fDepth = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fDepth, 1.0F, 1000.0F );
         FXCalcParams( SS, SS->driver->dig->DMA_rate );
       }
       if ( after_value ) *(F32*)after_value = SS->fDepth;
       return 1;

      case  _FX_FLANGE_RATE        :
       if ( before_value ) *(F32*)before_value = SS->fSweepRate;
       if ( new_value )
       {
         SS->fSweepRate = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fSweepRate, 0.01F, 10.0F );
         FXCalcParams( SS, SS->driver->dig->DMA_rate );
       }
       if ( after_value ) *(F32*)after_value = SS->fSweepRate;
       return 1;

      case  _FX_FLANGE_FEEDBACK     :
       if ( before_value ) *(F32*)before_value = SS->fFeedback;
       if ( new_value )
       {
         SS->fFeedback = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fFeedback, -0.9F, 0.9F );
       }
       if ( after_value ) *(F32*)after_value = SS->fFeedback;
       return 1;
      }

   return 0;
}

extern "C" S32 FlangeMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern "C" S32 FlangeMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg )
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
      REG_PR(FLT_F_WET_LEVEL,              _FX_WET,                   RIB_FLOAT),
      REG_PR(FLT_F_DRY_LEVEL,              _FX_DRY,                   RIB_FLOAT),
      REG_PR(FLT_F_FLANGE_DEPTH,     _FX_FLANGE_DEPTH,    RIB_FLOAT),
      };

   const RIB_INTERFACE_ENTRY FLTSMP3[] =
      {
      REG_PR(FLT_F_FLANGE_SWEEPRATE, _FX_FLANGE_RATE,     RIB_FLOAT),
      REG_PR(FLT_F_FLANGE_FEEDBACK,  _FX_FLANGE_FEEDBACK, RIB_FLOAT),
      };

   if (up_down)
      {
         RIB_registerP(provider_handle, "MSS pipeline filter",  FLT1);
         RIB_registerP(provider_handle, "MSS pipeline filter",  FLT2);
         RIB_registerP(provider_handle, "MSS pipeline filter",  FLT3);
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

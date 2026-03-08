//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  API.CPP: FLT module for Phaser Filter                                 ##
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

#define FILTER_NAME "Phaser Filter"

#define FLT_F_PHASER_RATE      "Phaser Rate"
#define FLT_F_PHASER_RANGE     "Phaser Range"
#define FLT_F_PHASER_FEEDBACK  "Phaser Feedback"
#define FLT_F_PHASER_BASEFREQ  "Phaser BaseFreq"
#define FLT_F_WET_LEVEL "Wet Level"
#define FLT_F_DRY_LEVEL "Dry Level"

#define PROVIDER_SPU_INDEX 1

// Phaser defaults
#define  _FX_FEEDBACK_DEFAULT    0.8F
#define  _FX_RATE_DEFAULT        2.0F
#define  _FX_RANGE_DEFAULT       4.0F
#define  _FX_BASEFREQ_DEFAULT    200.0F
#define  _FX_WET_DEFAULT         0.8F
#define  _FX_DRY_DEFAULT         0.0F

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
   _FX_PHASER_FEEDBACK,
   _FX_PHASER_RATE,
   _FX_PHASER_RANGE,
   _FX_PHASER_BASEFREQ
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

   S32    calculated_rate;

   F32    fFeedback;
   F32    fRate;
   F32    fRange;
   F32    fBaseFreq;
   F32 fWet, fDry;

   F32    fLX1;
   F32    fLY1;
   F32    fLX2;
   F32    fLY2;
   F32    fLX3;
   F32    fLY3;
   F32    fLX4;
   F32    fLY4;

   F32    fWP;
   F32    fSweepMin;
   F32    fSweepRange;
   F32    fSweepMax;
   F32    fSweepRate;
   F32    fSweepRateInv;
   F32    fSweepFrac;
};

#include "common.inl"

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

SPU_NAME( Phaser, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
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
   F32 fOutL;
   F32 fX1, fCoef, fCoefn;

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

   // calculate coef
   fCoef    =  ( 1.0F - SS.fWP ) / ( 1.0F + SS.fWP );

   {
      // mix into build buffer
      for( dwIndex = 0; dwIndex < n_samples; dwIndex++ )
      {
         // get input sample
         fInput   =  (F32)(S16)LE_SWAP16(source_buffer);
         ++source_buffer;

         // get next coef

         // adjust frequency of sweep filter
         SS.fWP  *= SS.fSweepFrac;

         // check sweep ranges
         if ( SS.fWP > SS.fSweepMax )
            SS.fSweepFrac =  SS.fSweepRateInv;
         else
         {
            // check for minimum
            if ( SS.fWP < SS.fSweepMin )
               SS.fSweepFrac =  SS.fSweepRate;
         }

         // calculate next coef
         fCoefn    =  ( 1.0F - SS.fWP ) / ( 1.0F + SS.fWP );

         // process
         fX1   =  fInput + ( SS.fFeedback * SS.fLY4 );

         // do process all filters
         SS.fLY1 =  fCoef * ( SS.fLY1 + fX1 ) - SS.fLX1;
         SS.fLX1 =  fX1;
         SS.fLY2 =  fCoef * ( SS.fLY2 + SS.fLY1 ) - SS.fLX2;
         SS.fLX2 =  SS.fLY1;
         SS.fLY3 =  fCoef * ( SS.fLY3 + SS.fLY2 ) - SS.fLX3;
         SS.fLX3 =  SS.fLY2;
         SS.fLY4 =  fCoef * ( SS.fLY4 + SS.fLY3 ) - SS.fLX4;
         SS.fLX4 =  SS.fLY3;

         // calculate final output
         fOutL  =  SS.fLY4 * fWetOut + fInput * fDryOut;

         // store output
         WRITE_MONO_SAMPLE( dest_buffer, fOutL );

         fCoef = fCoefn;
      }
   }
   AIL_memcpy(SSp, &SS, sizeof(*SSp));
}

#ifndef MSS_SPU_PROCESS

//############################################################################
//#                                                                          #
//# Calculate coefficients and values based on parameter set                 #
//#                                                                          #
//############################################################################

static void FXCalcParams( SAMPLESTATE * SS, S32 playback_rate )
{
   // get sample rate
   F32 fRate = F32(playback_rate);
   SS->calculated_rate = playback_rate;

   // calculate parameters for sweeping filters
   SS->fWP  =  ( F_PI * SS->fBaseFreq ) / fRate;

   // set minimum sweep
   SS->fSweepMin     =  SS->fWP;

   // calculate range
   SS->fSweepRange   =  (F32) AIL_pow( 2.0F, SS->fRange );

   // set maximum sweep
   SS->fSweepMax     =  ( F_PI * SS->fBaseFreq * SS->fSweepRange ) / fRate;

   // set sweep rate
   SS->fSweepRate    =  (F32) AIL_pow( SS->fSweepRange, SS->fRate / ( fRate / 2.0F ));
   SS->fSweepRateInv = 1.0F / SS->fSweepRate;

   // set fractional sweep
   SS->fSweepFrac    =  SS->fSweepRate;
}

static void init_sample( SAMPLESTATE * SS )
{
   //
   // Initialize provider-specific members to their default values
   //

   SS->fFeedback     =  _FX_FEEDBACK_DEFAULT;
   SS->fRate         =  _FX_RATE_DEFAULT;
   SS->fRange        =  _FX_RANGE_DEFAULT;
   SS->fBaseFreq     =  _FX_BASEFREQ_DEFAULT;
   SS->fWet          =  _FX_WET_DEFAULT;
   SS->fDry          =  _FX_DRY_DEFAULT;

   // calculate params
   FXCalcParams( SS, SS->driver->dig->DMA_rate );

   // reset all history values
   SS->fLX1    =  0.0F;
   SS->fLY1    =  0.0F;
   SS->fLX2    =  0.0F;
   SS->fLY2    =  0.0F;
   SS->fLX3    =  0.0F;
   SS->fLY3    =  0.0F;
   SS->fLX4    =  0.0F;
   SS->fLY4    =  0.0F;
}

static void close_sample( SAMPLESTATE * SS )
{
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
      case  _FX_PHASER_FEEDBACK  :
       if ( before_value ) *(F32*)before_value = SS->fFeedback;
       if ( new_value )
       {
         SS->fFeedback = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fFeedback, 0.0F, 1.0F );
       }
       if ( after_value ) *(F32*)after_value = SS->fFeedback;
       return 1;

      case  _FX_PHASER_RATE      :
       if ( before_value ) *(F32*)before_value = SS->fRate;
       if ( new_value )
       {
         SS->fRate = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fRate, 0.01F, 4.0f );
         FXCalcParams(SS, SS->driver->dig->DMA_rate);
       }
       if ( after_value ) *(F32*)after_value = SS->fRate;
       return 1;

      case  _FX_PHASER_RANGE     :
       if ( before_value ) *(F32*)before_value = SS->fRange;
       if ( new_value )
       {
         SS->fRange = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fRange, 0.01F, 4.0f );
         FXCalcParams(SS, SS->driver->dig->DMA_rate);
       }
       if ( after_value ) *(F32*)after_value = SS->fRange;
       return 1;

      case  _FX_PHASER_BASEFREQ  :
       if ( before_value ) *(F32*)before_value = SS->fBaseFreq;
       if ( new_value )
       {
         SS->fBaseFreq = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fBaseFreq, 20.0F, ((F32) SS->driver->dig->DMA_rate) / 2.0F - 1.0F );
         FXCalcParams(SS, SS->driver->dig->DMA_rate);
       }
       if ( after_value ) *(F32*)after_value = SS->fBaseFreq;
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

extern "C" S32 PhaserMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern "C" S32 PhaserMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg )
{
   const RIB_INTERFACE_ENTRY FLT1[] =
      {
      REG_FN(PROVIDER_property),
      REG_PR("Name",                     PROVIDER_NAME,          (RIB_DATA_SUBTYPE) (RIB_STRING|RIB_READONLY)),
      REG_PR("Version",                  PROVIDER_VERSION,       (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),
      REG_PR("Flags",                   _FX_PROVIDER_FLAGS,      (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),
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
      REG_PR(FLT_F_PHASER_FEEDBACK,      _FX_PHASER_FEEDBACK,    RIB_FLOAT),
      REG_PR(FLT_F_PHASER_RATE,          _FX_PHASER_RATE,        RIB_FLOAT),
      REG_PR(FLT_F_PHASER_RANGE,         _FX_PHASER_RANGE,       RIB_FLOAT),
      REG_PR(FLT_F_PHASER_BASEFREQ,      _FX_PHASER_BASEFREQ,    RIB_FLOAT),
      };

   const RIB_INTERFACE_ENTRY FLTSMP3[] =
      {
      REG_PR(FLT_F_WET_LEVEL,              _FX_WET,                   RIB_FLOAT),
      REG_PR(FLT_F_DRY_LEVEL,              _FX_DRY,                   RIB_FLOAT),
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

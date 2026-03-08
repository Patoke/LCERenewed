//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  API.CPP: FLT module for Ring/Amplutide Modulator                      ##
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

#define FILTER_NAME "Ring Modulator Filter"

#define FLT_F_RINGMOD_FREQ     "Ring Mod Freq"
#define FLT_F_RINGMOD_PHASE    "Ring Mod Phase"
#define FLT_F_WET_LEVEL "Wet Level"
#define FLT_F_DRY_LEVEL "Dry Level"

#define PROVIDER_SPU_INDEX 11

// Ring modulator
#define  _FX_FREQ_DEFAULT        16.0f
#define  _FX_PHASE               0
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
   _FX_RINGMOD_FREQ,
   _FX_RINGMOD_PHASE
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

   F32    fFrequency;
   F32 fWet, fDry;

   // sample index
   F32    fPhaseFreq;
   F32    fPhaseAdd;
   S32    iPhaseAdd;
   F32    fPhaseL;
   S32    iPhaseL;
};

#include "common.inl"

#define MAXLUF 16.0f
#define MAXLU 16

static F32 const sintable[ MAXLU + 1 ] = {
 0.0f, 0.382683432f, 0.707106781f, 0.923879533f,
 1.0f, 0.923879533f, 0.707106781f, 0.382683432f,
 0.0f,-0.382683432f,-0.707106781f,-0.923879533f,
-1.0f,-0.923879533f,-0.707106781f,-0.382683432f,
 0.0f };


/*
  #ifdef IS_MAC
  #define sinf sin
  #endif
  #ifdef IS_PS2
  #define sinf AIL_sin
  #endif
  static S32 do_init = 1;
  
   if (do_init)
   {
     S32 i;
     do_init = 0;
     for ( i = 0 ; i < ( MAXLU + 1 ) ; i++ )
     {
       sintable[ i ] = (F32) sinf( ( 2.0f * F_PI * (F32) i ) / MAXLUF );
     }
   }
*/


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

SPU_NAME( RingMod, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
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

   S32      dwIndex;
   F32      fInput;
   F32      fOutL;

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

   {
      // mix into build buffer
      for( dwIndex = 0; dwIndex < n_samples; dwIndex++ )
      {
         // get input sample
         fInput   =  (F32)(S16)LE_SWAP16(source_buffer);
         ++source_buffer;

         // modulate input   fracpor * ( n - f ) + f
         fOutL   = fInput *
                   ( SS.fPhaseL * ( sintable[ SS.iPhaseL + 1 ] - sintable[ SS.iPhaseL ] ) +
                     sintable[ SS.iPhaseL ] );

         SS.iPhaseL += SS.iPhaseAdd;
         SS.fPhaseL += SS.fPhaseAdd;
         if ( SS.fPhaseL >= 1.0f )
         {
           SS.fPhaseL -= 1.0f;
           ++SS.iPhaseL;
         }
         SS.iPhaseL &= ( MAXLU-1 );

         fOutL  =  ( fInput * fDryOut ) +
                    ( fOutL * fWetOut );

         // store output
         WRITE_MONO_SAMPLE( dest_buffer, fOutL );
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

static void  FXCalcParams( SAMPLESTATE * SS, S32 sample_rate )
{
   F32 fPhaseAdd, fRate = F32(sample_rate);
   SS->calculated_rate = sample_rate;

   fPhaseAdd = ( MAXLUF * SS->fFrequency / fRate );

   SS->iPhaseAdd = (S32) AIL_floor(fPhaseAdd);
   SS->fPhaseAdd = fPhaseAdd - (F32)SS->iPhaseAdd;
}


static void init_sample( SAMPLESTATE * SS )
{
   //
   // Initialize provider-specific members to their default values
   //

   // init default values
   SS->fFrequency    =  _FX_FREQ_DEFAULT;
   SS->iPhaseL       =  _FX_PHASE;
   SS->fPhaseL       =  0;
   SS->fWet          =  _FX_WET_DEFAULT;
   SS->fDry          =  _FX_DRY_DEFAULT;

   // update params
   FXCalcParams( SS, SS->driver->dig->DMA_rate );
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

   F32 phase;

   // determine preference
   switch (property)
      {
      //
      // Preferences
      //
      case  _FX_RINGMOD_FREQ   :
       if ( before_value ) *(F32*)before_value = SS->fFrequency;
       if ( new_value ) 
       {
         SS->fFrequency = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->fFrequency, 20.0F, ((F32) SS->driver->dig->DMA_rate) / 2.0F - 1.0F );
         FXCalcParams(SS, SS->driver->dig->DMA_rate);
       }         
       if ( after_value ) *(F32*)after_value = SS->fFrequency;
       return 1;

      case  _FX_RINGMOD_PHASE   :
       if ( before_value ) 
       {
         phase = ( (F32) SS->iPhaseL ) + SS->fPhaseL;
         phase = ( phase / MAXLUF ) * 2.0f * F_PI;
         *(F32*)before_value = phase;
       }
       
       if ( new_value ) 
       {
         phase = *(F32 const*)new_value;
         // clip width to valid range
         FX_CLIPRANGE( phase, 0.0f, 2.0F*F_PI );
         phase = ( phase / ( 2.0f * F_PI ) ) * MAXLUF;
         SS->iPhaseL = (S32) AIL_floor(phase);
         SS->fPhaseL = phase - (F32)SS->iPhaseL;

         FXCalcParams(SS, SS->driver->dig->DMA_rate);
       }         
       
       if ( after_value ) 
       {
         phase = ( (F32) SS->iPhaseL ) + SS->fPhaseL;
         phase = ( phase / MAXLUF ) * 2.0f * F_PI;
         *(F32*)after_value = phase;
       }
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

extern "C" S32 RingModMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern "C" S32 RingModMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg )
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
      REG_PR(FLT_F_RINGMOD_FREQ,     _FX_RINGMOD_FREQ,         RIB_FLOAT),
      REG_PR(FLT_F_RINGMOD_PHASE,    _FX_RINGMOD_PHASE,        RIB_FLOAT),
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
      }
   else
      {
         RIB_unregister_allP(provider_handle);
      }

   return TRUE;
}

#endif

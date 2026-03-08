//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  GAINBLK.CPP: FLT module for non-linear gain block                     ##
//##                                                                        ##
//##  Author: John Miles                                                    ##
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

#define FILTER_NAME "Gain Block Filter"

#define FLT_F_GAINBLOCK_MIDPOINT_GAIN              "Midpoint Gain"
#define FLT_F_GAINBLOCK_BASE_GAIN                  "Base Gain"
#define FLT_F_GAINBLOCK_RAIL_GAIN                  "Rail Gain"
#define FLT_F_GAINBLOCK_MIDPOINT_LEVEL             "Midpoint Level"
#define FLT_B_GAINBLOCK_NEGATIVE_CONTROLS_ENABLED  "Negative Controls Enabled"
#define FLT_F_GAINBLOCK_NEGATIVE_MIDPOINT_GAIN     "Negative Midpoint Gain"
#define FLT_F_GAINBLOCK_NEGATIVE_RAIL_GAIN         "Negative Rail Gain"
#define FLT_F_GAINBLOCK_NEGATIVE_MIDPOINT_LEVEL    "Negative Midpoint Level"
#define FLT_F_WET_LEVEL "Wet Level"
#define FLT_F_DRY_LEVEL "Dry Level"

#define PROVIDER_SPU_INDEX 9


// Gain block defaults
#define  _FX_DEFAULT_POS_RAIL_GAIN   0.0F
#define  _FX_DEFAULT_POS_LEVEL       0.5F
#define  _FX_DEFAULT_POS_GAIN        0.0F
#define  _FX_DEFAULT_BASE_GAIN       0.0F
#define  _FX_DEFAULT_NEG_GAIN        0.0F
#define  _FX_DEFAULT_NEG_LEVEL       0.5F
#define  _FX_DEFAULT_NEG_RAIL_GAIN   0.0F
#define  _FX_WET_DEFAULT             0.5F
#define  _FX_DRY_DEFAULT             0.5F
#define  _FX_DEFAULT_NEG_ENABLED     FALSE

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
   _FX_POS_RAIL_GAIN,
   _FX_POS_LEVEL,
   _FX_POS_GAIN,
   _FX_BASE_GAIN,      
   _FX_NEG_GAIN,     
   _FX_NEG_LEVEL,     
   _FX_NEG_RAIL_GAIN, 
   _FX_NEG_ENABLED
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
   DRIVERSTATE *driver;   // Driver with which this descriptor is associated

   //
   // Members associated with specific filter provider
   //

   F32 fWet, fDry;

   F32    pos_rail_gain;
   F32    pos_level;
   F32    pos_gain;
   F32    base_gain;
   F32    neg_gain;
   F32    neg_level;
   F32    neg_rail_gain;

   S32    neg_enabled;
   S32    lerp_gain;
   F32    flat_gain;

   F32    gain_table[256];    // 2.0 = +6 dB, 1.0 = 0 dB, 0.5 = -6 dB...
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

SPU_NAME( GainBlock, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                              S16  * MSSRESTRICT  source_buffer,
                                              S16  * MSSRESTRICT  dest_buffer,
                                              S32          n_samples,
                                              MSS_SPEAKER  channel)
{
   SAMPLESTATE *SSp  = (SAMPLESTATE *) state;
   SAMPLESTATE SScopy;
   SAMPLESTATE *SS;

   // set wet/dry mix
   F32 fDryOut  =  SSp->fDry;
   F32 fWetOut  =  SSp->fWet;

   // early out if all dry
   if ( fDryOut > 0.9999f )
   {
     if ( source_buffer != dest_buffer )
       AIL_memcpy( dest_buffer, source_buffer, n_samples * 2 );
     return;
   }

   if (SSp->lerp_gain)
      {
      //
      // Copy structure with 1K table only if we need it
      //

      AIL_memcpy(&SScopy, SSp, sizeof(SScopy));
      SS = &SScopy;
      }
   else
      {
      SS = SSp;
      }

   //HSAMPLE          S   = SS->sample;
   //DRIVERSTATE *DRV = SS->driver;

   S32 I,dwIndex;
   F32 fInput;
   F32 fOutL;

    {
    if (SS->lerp_gain)
       {
       // mix into build buffer
       for (dwIndex = 0; dwIndex < n_samples; dwIndex++)
          {
          I = (S16) LE_SWAP16(source_buffer);
          ++source_buffer;

          fInput = (F32) I;
          fOutL  = fInput * SS->gain_table[ (((U32) I) & 0xff00) >> 8 ];

          fOutL = ( fOutL * fWetOut ) + ( fInput * fDryOut );

          // store output
          WRITE_MONO_SAMPLE( dest_buffer, fOutL );
          }
       }
    else
       {
       // fast path doesn't use LUT when all gain points are equal, or when mix is 100% dry
       for (dwIndex = 0; dwIndex < n_samples; dwIndex++)
          {
          fInput = (F32) (S16) LE_SWAP16(source_buffer);
          ++source_buffer;

          fOutL  = fInput * SS->gain_table[ 0 ];
          fOutL = ( fOutL * fWetOut ) + ( fInput * fDryOut );

          // store output
          WRITE_MONO_SAMPLE( dest_buffer, fOutL );
          }
       }
    }

#if 0 // This filter doesn't alter any structure members at sample-process time
   if (SS != SSp)
      {
      AIL_memcpy(SSp, SS, sizeof(*SSp));
      }
#endif
}

#ifndef MSS_SPU_PROCESS

static void  FXCalcParams( SAMPLESTATE * SS )
{
   S32 i;

   //
   // Build gain_table[], indexed by upper 8 bits of signed sample value
   // (Table is built with indexes xor'ed with 0x80 to support direct conversion
   // of signed sample MSBs)
   //
   // First, copy positive level/gain parameters to negative parameters if
   // separate negative-half controls aren't enabled...
   //

   if (!SS->neg_enabled)
      {
      SS->neg_rail_gain = SS->pos_rail_gain;
      SS->neg_level     = SS->pos_level;
      SS->neg_gain      = SS->pos_gain;
      }

   //
   // If all the gain factors are essentially the same, there's no need to lerp them...
   //

   if ((AIL_fabs(SS->pos_gain - SS->neg_gain)      < 0.1F) &&
       (AIL_fabs(SS->pos_gain - SS->base_gain)     < 0.1F) &&
       (AIL_fabs(SS->pos_gain - SS->pos_rail_gain) < 0.1F) &&
       (AIL_fabs(SS->pos_gain - SS->neg_rail_gain) < 0.1F))
      {
      SS->lerp_gain     = FALSE;
      SS->gain_table[0] = (F32) AIL_pow(10.0F, SS->pos_gain / 20.0F);
      return;
      }

   SS->lerp_gain = TRUE;

   //
   // Build lower portion of positive half of gain table (entries [128,brk))
   //

   S32 brk = 128 + (S32) ((SS->pos_level * 128.0F) + 0.5F);  // 0-1 -> 128-256

   S32 n = brk - 128;

   F32 dg = (n == 0) ? 0.0F : ((SS->pos_gain - SS->base_gain) / n);
   F32 g  = SS->base_gain;

   for (i=128; i < brk; i++)
      {
      SS->gain_table[i ^ 128] = (F32) AIL_pow(10.0F, g / 20.0F);
      g += dg;
      }

   //
   // Build upper portion of positive half (entries [brk,256))
   //

   n = 256 - brk;

   dg = (n == 0) ? 0.0F : ((SS->pos_rail_gain - SS->pos_gain) / n);
   g  = SS->pos_gain;

   for (i=brk; i < 256; i++)
      {
      SS->gain_table[i ^ 128] = (F32) AIL_pow(10.0F, g / 20.0F); 
      g += dg;
      }

   //
   // Build upper portion of negative half of gain table (entries [128,brk))
   //

   brk = 128 - (S32) ((SS->neg_level * 128.0F) + 0.5F);  // 0-1 -> 128-0

   n = 128 - brk;

   dg = (n == 0) ? 0.0F : ((SS->neg_gain - SS->base_gain) / n);
   g  = SS->base_gain;

   for (i=128; i > brk; i--)
      {
      SS->gain_table[i ^ 128] = (F32) AIL_pow(10.0F, g / 20.0F); 
      g += dg;
      }

   //
   // Build lower portion of negative half (entries [brk,-1))
   //

   n = brk;

   dg = (n == 0) ? 0.0F : ((SS->neg_rail_gain - SS->neg_gain) / n);
   g  = SS->neg_gain;

   for (i=brk; i > -1; i--)
      {
      SS->gain_table[i ^ 128] = (F32) AIL_pow(10.0F, g / 20.0F); 
      g += dg;
      }
}

static void init_sample( SAMPLESTATE * SS )
{
   //
   // Initialize provider-specific members to their default values
   //

   SS->fWet          =  _FX_WET_DEFAULT;
   SS->fDry          =  _FX_DRY_DEFAULT;

   SS->pos_rail_gain = _FX_DEFAULT_POS_RAIL_GAIN;
   SS->pos_level     = _FX_DEFAULT_POS_LEVEL;
   SS->pos_gain      = _FX_DEFAULT_POS_GAIN;
   SS->base_gain     = _FX_DEFAULT_BASE_GAIN;
   SS->neg_gain      = _FX_DEFAULT_NEG_GAIN;
   SS->neg_level     = _FX_DEFAULT_NEG_LEVEL;
   SS->neg_rail_gain = _FX_DEFAULT_NEG_RAIL_GAIN;
   SS->neg_enabled   = _FX_DEFAULT_NEG_ENABLED;
   
   // calculate params
   FXCalcParams(SS);
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

      case  _FX_NEG_ENABLED :
       if ( before_value ) *(S32*)before_value = SS->neg_enabled;
       if ( new_value ) 
       {
         SS->neg_enabled = *(S32 const*)new_value;
         FXCalcParams(SS);
       }         
       if ( after_value ) *(S32*)after_value = SS->neg_enabled;
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

      case  _FX_POS_RAIL_GAIN       :
       if ( before_value ) *(F32*)before_value = SS->pos_rail_gain;
       if ( new_value ) 
       {
         SS->pos_rail_gain = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->pos_rail_gain, -96.0F, 96.0f );
         FXCalcParams(SS);
       }         
       if ( after_value ) *(F32*)after_value = SS->pos_rail_gain;
       return 1;

      case  _FX_POS_GAIN       :
       if ( before_value ) *(F32*)before_value = SS->pos_gain;
       if ( new_value ) 
       {
         SS->pos_gain= *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->pos_gain, -96.0F, 96.0f );
         FXCalcParams(SS);
       }         
       if ( after_value ) *(F32*)after_value = SS->pos_gain;
       return 1;

      case  _FX_POS_LEVEL       :
       if ( before_value ) *(F32*)before_value = SS->pos_level;
       if ( new_value ) 
       {
         SS->pos_level = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->pos_level, 0.0F, 1.0F );
         // set new parameters
         FXCalcParams( SS );
       }
       if ( after_value ) *(F32*)after_value = SS->pos_level;
       return 1;

      case  _FX_NEG_RAIL_GAIN       :
       if ( before_value ) *(F32*)before_value = SS->neg_rail_gain;
       if ( new_value ) 
       {
         SS->neg_rail_gain = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->neg_rail_gain, -96.0F, 96.0f );
         FXCalcParams(SS);
       }         
       if ( after_value ) *(F32*)after_value = SS->neg_rail_gain;
       return 1;

      case  _FX_NEG_GAIN       :
       if ( before_value ) *(F32*)before_value = SS->neg_gain;
       if ( new_value ) 
       {
         SS->neg_gain = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->neg_gain, -96.0F, 96.0f );
         FXCalcParams(SS);
       }         
       if ( after_value ) *(F32*)after_value = SS->neg_gain;
       return 1;

      case  _FX_NEG_LEVEL       :
       if ( before_value ) *(F32*)before_value = SS->neg_level;
       if ( new_value ) 
       {
         SS->neg_level = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->neg_level, 0.0F, 1.0F );
         // set new parameters
         FXCalcParams( SS );
       }         
       if ( after_value ) *(F32*)after_value = SS->neg_level;
       return 1;

      case  _FX_BASE_GAIN       :
       if ( before_value ) *(F32*)before_value = SS->base_gain;
       if ( new_value ) 
       {
         SS->base_gain = *(F32 const*)new_value;
         // clip to valid range
         FX_CLIPRANGE( SS->base_gain, -96.0F, 96.0f );
         FXCalcParams(SS);
       }         
       if ( after_value ) *(F32*)after_value = SS->base_gain;
       return 1;

      }

   return 0;
}

extern "C" S32 GainMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern "C" S32 GainMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg )
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
      REG_PR(FLT_F_GAINBLOCK_RAIL_GAIN,                 _FX_POS_RAIL_GAIN  ,         RIB_FLOAT),
      REG_PR(FLT_F_GAINBLOCK_MIDPOINT_LEVEL,            _FX_POS_LEVEL      ,         RIB_FLOAT),
      REG_PR(FLT_F_GAINBLOCK_MIDPOINT_GAIN,             _FX_POS_GAIN       ,         RIB_FLOAT),
      REG_PR(FLT_F_GAINBLOCK_BASE_GAIN,                 _FX_BASE_GAIN       ,        RIB_FLOAT),
      };

   const RIB_INTERFACE_ENTRY FLTSMP3[] =
      {
      REG_PR(FLT_F_WET_LEVEL,              _FX_WET,                   RIB_FLOAT),
      REG_PR(FLT_F_DRY_LEVEL,              _FX_DRY,                   RIB_FLOAT),
      REG_PR(FLT_F_GAINBLOCK_NEGATIVE_MIDPOINT_GAIN,    _FX_NEG_GAIN       ,         RIB_FLOAT),
      };

   const RIB_INTERFACE_ENTRY FLTSMP4[] =
      {
      REG_PR(FLT_F_GAINBLOCK_NEGATIVE_MIDPOINT_LEVEL,   _FX_NEG_LEVEL      ,         RIB_FLOAT),
      REG_PR(FLT_F_GAINBLOCK_NEGATIVE_RAIL_GAIN,        _FX_NEG_RAIL_GAIN  ,         RIB_FLOAT),
      REG_PR(FLT_B_GAINBLOCK_NEGATIVE_CONTROLS_ENABLED, _FX_NEG_ENABLED,             RIB_BOOL),
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
         RIB_registerP(provider_handle, "Pipeline filter sample services", FLTSMP4);
      }
   else
      {
         RIB_unregister_allP(provider_handle);
      }

   return TRUE;
}

#endif

//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
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

#define FILTER_NAME "Compressor Filter"
#define FLT_F_COMPRESSOR_INPUT "Input"
#define FLT_F_COMPRESSOR_THRESHOLD "Threshold"
#define FLT_F_COMPRESSOR_RATIO "Ratio"
#define FLT_F_COMPRESSOR_ATTACK "Attack"
#define FLT_F_COMPRESSOR_RELEASE "Release"

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

    _FX_COMPRESSOR_INPUT, // This index can not change, its referred to by ordinal in AIL_API_install_bus_compressor
    _FX_COMPRESSOR_THRESHOLD,
    _FX_COMPRESSOR_RATIO,
    _FX_COMPRESSOR_ATTACK,
    _FX_COMPRESSOR_RELEASE
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
    S32 Input;

    float Threshold;
    float Ratio;
    float AttactMs;
    float ReleaseMs;
    float AttenuateLatency;
};

// Mark that we are side chain and can accept input.
#define FILTER_PROPERTY_FLAGS FPROV_SIDECHAIN
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

SPU_NAME( SideCompress, FLTSMP_sample_process ) (HSAMPLESTATE state,  //)
                                            S16  *   source_buffer,
                                            S16  *   dest_buffer,
                                            S32          n_samples,
                                            MSS_SPEAKER  channel)
{
    SAMPLESTATE *SS  = (SAMPLESTATE *) state;

    // When the input is above this value, start attenuating.
    float AttenuateAboveDb = SS->Threshold;

    // For each db above the limit, decrease by the given db
    float AttenuateRatio = -SS->Ratio;

    // Figure amount to attenuate.
    float AttenuateAmount = 0;
    {
        float Input = (float)SS->Input;
        Input /= 32767.0f;

        if (Input < .00001f)
            Input = -96.0f;
        else
            Input = 20.0f * log10(Input);

        float OverAmt = Input - AttenuateAboveDb;
        if (OverAmt > 0)
        {
            AttenuateAmount = OverAmt * AttenuateRatio;
            if (AttenuateAmount < -96.0f)
                AttenuateAmount = -96.0f;
        }
    }

    // Convert to a linamp from dB.
    AttenuateAmount = pow(10.0f, AttenuateAmount / 20.0f);

    // Attack over 50 ms.
    float rate = (float)SS->sample->original_playback_rate;
    float buffer_time = n_samples * 1000.0f / rate;

    float AttactPct = buffer_time / SS->AttactMs;
    float ReleasePct = buffer_time / SS->ReleaseMs;
    if (AttactPct > 1)
        AttactPct = 1;
    if (ReleasePct > 1)
        ReleasePct = 1;

    if (SS->AttenuateLatency > AttenuateAmount)
    {
        // We're in attack.
        SS->AttenuateLatency = (1 - AttactPct) * SS->AttenuateLatency + AttactPct * AttenuateAmount;
    }
    else
    {
        // We're in release
        SS->AttenuateLatency = (1 - ReleasePct) * SS->AttenuateLatency + ReleasePct * AttenuateAmount;
    }

    AttenuateAmount = SS->AttenuateLatency;

    // Convert from linear in loudness to amplitude.
    AttenuateAmount = AIL_pow(AttenuateAmount, 10.0f / 6.0f);

    S32 scale = (S32)(AttenuateAmount * 32767.0f);
    if (scale >= 32767)
        scale = 32767;

    // Attenuate the buffer
    S16* src = source_buffer;
    S16* src_end = src + n_samples;
    S16* dest = dest_buffer;

    // Fast path.
    if (AttenuateAmount >= .99f)
    {
        if (source_buffer != dest_buffer)
            AIL_memcpy( dest_buffer, source_buffer, n_samples * 2 );
        return;
    }

    for (; src != src_end; src++)
    {
        S16 dry = (LE_SWAP16(src));

        S16 wet = (S16)(((S32)dry * (S32)scale) >> 15);

        *dest = wet;
        dest++;
    }
}

#ifndef MSS_SPU_PROCESS

static void init_sample( SAMPLESTATE * SS )
{
    SS->AttactMs = 50.0f;
    SS->ReleaseMs = 200.0f;
    SS->Ratio = 1.7;
    SS->Threshold = -35.0f;
    SS->AttenuateLatency = 1.0f;
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

    // determine preference
    switch (property)
    {
    case _FX_COMPRESSOR_INPUT: { SS->Input = *(S32*)new_value; return 1; }
    case _FX_COMPRESSOR_THRESHOLD: { SS->Threshold = *(float*)new_value; return 1; }
    case _FX_COMPRESSOR_RATIO: { SS->Ratio = *(float*)new_value; return 1; }
    case _FX_COMPRESSOR_ATTACK: { SS->AttactMs = *(float*)new_value; return 1; }
    case _FX_COMPRESSOR_RELEASE: { SS->ReleaseMs = *(float*)new_value; return 1; }
    }
    return 0;
}

extern "C" S32 SideCompressMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern "C" S32 SideCompressMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg )
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
        REG_PR(FLT_F_COMPRESSOR_INPUT,       _FX_COMPRESSOR_INPUT,      RIB_HEX),
        REG_PR(FLT_F_COMPRESSOR_THRESHOLD,       _FX_COMPRESSOR_THRESHOLD,      RIB_FLOAT),
        REG_PR(FLT_F_COMPRESSOR_RATIO,       _FX_COMPRESSOR_RATIO,      RIB_FLOAT),
        REG_PR(FLT_F_COMPRESSOR_ATTACK,       _FX_COMPRESSOR_ATTACK,      RIB_FLOAT),
        REG_PR(FLT_F_COMPRESSOR_RELEASE,       _FX_COMPRESSOR_RELEASE,      RIB_FLOAT)
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

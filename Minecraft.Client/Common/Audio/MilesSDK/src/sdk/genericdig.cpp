//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  mssdig.C: Digital Sound module for Xaudio                             ##
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
#include "radss.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "rrAtomics.h"

#define SETTAG(var,tag) AIL_memcpy(var,tag,4);


//
// Flag to keep SS_serve from reentering
//

static U32 volatile SS_servicing = 0;

//
// Primary digital driver (the one used with 3D providers and other
// future single-driver extensions)
//

extern "C"
{
HDIGDRIVER primary_digital_driver = NULL;
}

extern "C"
{
    void AILCALL AIL_sleep(U32 ms);
}

//############################################################################
//##                                                                        ##
//## Start driver-based DMA buffer playback                                 ##
//##                                                                        ##
//############################################################################

void AILCALL SS_start_DIG_driver_playback(HSAMPLE S)
{
    HDIGDRIVER dig;
    dig = S->driver;

    // Return if playback already active, or we aren't responsible for output.
    if (dig->playing || dig->physical_channels_per_sample == 0) 
    {
        return;
    }

    dig->playing = 1;

    InMilesMutex();    // Lock to avoid deadlocks with SS_serve

    RadSoundSystem* pSoundSystem = (RadSoundSystem*)dig->dig_ss;
    if (pSoundSystem) pSoundSystem->SetPaused(pSoundSystem, 0);

    OutMilesMutex();
}

//############################################################################
//##                                                                        ##
//## Stop driver-based DMA buffer playback                                  ##
//##                                                                        ##
//############################################################################

static void SS_stop_DIG_driver_playback(HDIGDRIVER dig)
{
    if (!dig->playing)
    {
        return;
    }

    dig->playing = 0;

    RadSoundSystem* pSS = (RadSoundSystem*)dig->dig_ss;
    if (pSS) pSS->SetPaused(pSS, 1);
}

extern "C" void stream_background(void); // background service for streaming

//############################################################################
//##                                                                        ##
//## Timer callback function to mix data into output buffers                ##
//##                                                                        ##
//############################################################################

static void SS_serve(HDIGDRIVER dig)
{
    //
    // Return immediately if driver not actively playing
    //
    // dig->playing will never be TRUE when an output filter is in use
    //

    if (!dig->playing)
    {
        return;
    }

    //
    // Increment background count so callback functions will run in
    // background
    //

    rrAtomicAddExchange32(&SS_servicing, 1);

    if (SS_servicing == 1)
    {
        S32 MixAheadSizeMS = (S32)(AIL_preference[DIG_DS_MIX_FRAGMENT_CNT] * AIL_preference[DIG_DS_FRAGMENT_SIZE]);
        S32 BytesPerSecond = 2 * dig->DMA_rate * dig->physical_channels_per_sample;
        S32 TotalBufferSizeMS = (S32)(AIL_preference[DIG_DS_FRAGMENT_CNT] * AIL_preference[DIG_DS_FRAGMENT_SIZE]);
        S32 DesiredRemainderMS = TotalBufferSizeMS - MixAheadSizeMS;
        S32 DesiredRemainderBytes = (DesiredRemainderMS * BytesPerSecond) / 1000;

        
        RadSoundSystem* pSoundSystem = (RadSoundSystem*)dig->dig_ss;
        S32 ReadyCount = pSoundSystem->GetReadyBytes(pSoundSystem);
        S32 Limiter = 0;

        while (ReadyCount >= DesiredRemainderBytes)
        {
            if (Limiter >= 6) // Prevent spinlocks if the hardware layer decides to freak out.
                break;
            Limiter++;

            void* pMem;
            pSoundSystem->Lock(pSoundSystem, &pMem, dig->hardware_buffer_size);

            SS_fill(dig, pMem);

            pSoundSystem->Unlock(pSoundSystem);
            ReadyCount = pSoundSystem->GetReadyBytes(pSoundSystem);
        }
    }

    rrAtomicAddExchange32(&SS_servicing,-1);
}


//############################################################################
//##                                                                        ##
//## Initialize digital driver and allocate output buffers                  ##
//##                                                                        ##
//############################################################################

extern "C"
{
HDIGDRIVER AILCALL AIL_API_open_generic_digital_driver(
    U32 frequency,
    S32 bits,
    S32 channel_spec,
    U32 flags,
    RADSS_OPEN_FUNC dig_open
    )
{
    S32         i;
    HDIGDRIVER  dig;

    //
    // Allocate memory for DIG_DRIVER structure
    //

    dig = (HDIGDRIVER) AIL_mem_alloc_lock(sizeof(struct _DIG_DRIVER));

    if (dig == NULL)
    {
        AIL_set_error("Could not allocate memory for driver descriptor.");
        return 0;
    }

    //
    // Explicitly initialize all DIG_DRIVER fields to NULL/0
    //

    AIL_memset(dig,
        0,
        sizeof(*dig));

    SETTAG(dig->tag,"HDIG");

    //
    // Set reasonable 3D listener defaults
    //

    dig->reverb[0].room_type   = 0;
    dig->D3D.mute_at_max = (S32)AIL_preference[DIG_3D_MUTE_AT_MAX];

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

    //
    // Attempt to open wave output device
    //
    if (channel_spec == MSS_MC_USE_SYSTEM_CONFIG)
    {
        MSS_MC_SPEC NewSpec = MSS_MC_USE_SYSTEM_CONFIG;
        if (get_system_speaker_configuration(&NewSpec) == 0)
        {
            // Failed to get speaker config.
            AIL_set_error("Could not retrieve system speaker config.");
            AIL_mem_free_lock(dig);
            return 0;
        }
        // If the function didn't change anything, also fail.
        if (NewSpec == MSS_MC_USE_SYSTEM_CONFIG)
        {
            // Failed to get speaker config.
            AIL_set_error("Could not retrieve system speaker config.");
            AIL_mem_free_lock(dig);
            return 0;
        }
        channel_spec = NewSpec;
    }

    U32 LogChannelCount = 0;
    U32 PhysChannelCount = 0;
    switch ( channel_spec )
    {
        //
        // EAX / hardware formats use 0 channel count to prevent dig init.
        //
    case MSS_MC_MONO:                   LogChannelCount = PhysChannelCount = 1; break;
    case MSS_MC_STEREO:                 LogChannelCount = PhysChannelCount = 2; break;
    case MSS_MC_HEADPHONES:             LogChannelCount = PhysChannelCount = 2; break;
    case MSS_MC_40_DISCRETE:            LogChannelCount = PhysChannelCount = 4; break;
    case MSS_MC_51_DISCRETE:            LogChannelCount = PhysChannelCount = 6; break;
    case MSS_MC_61_DISCRETE:            LogChannelCount = PhysChannelCount = 7; break;
    case MSS_MC_71_DISCRETE:            LogChannelCount = PhysChannelCount = 8; break;
    case MSS_MC_81_DISCRETE:            LogChannelCount = PhysChannelCount = 9; break;
    case MSS_MC_SRS_CIRCLE_SURROUND:    LogChannelCount = 7; PhysChannelCount = 2; break;
    case MSS_MC_DOLBY_SURROUND:         LogChannelCount = 3; PhysChannelCount = 2; break;
    case MSS_MC_40_DTS:                 LogChannelCount = 4; PhysChannelCount = 2; break;
    case MSS_MC_51_DTS:                 LogChannelCount = 6; PhysChannelCount = 2; break;
    case MSS_MC_USE_SYSTEM_CONFIG:
    case MSS_MC_EAX2:
    case MSS_MC_EAX3:
    case MSS_MC_EAX4:
    case MSS_MC_DIRECTSOUND3D:          LogChannelCount = 0; PhysChannelCount = 0; break;
    }

    dig->channel_spec = (MSS_MC_SPEC)channel_spec;

    dig->physical_channels_per_sample = PhysChannelCount;
    dig->logical_channels_per_sample  = LogChannelCount;


    //
    // Set sample rate and size values and calling params
    //

    dig->DMA_rate          = frequency;
    dig->bytes_per_channel = 2;
    dig->playing          = 0;

    if (dig->physical_channels_per_sample == 0)
    {
#define MAX_OUTPUT_FILTER_SAMPLES 16384
        dig->hw_format = DIG_F_OUTPUT_FILTER_IN_USE;
        dig->bytes_per_channel = 0;
        dig->hardware_buffer_size = 0;
        dig->samples_per_buffer = MAX_OUTPUT_FILTER_SAMPLES;
        dig->n_build_buffers = 0;
        dig->playing = 0;
    }
    else if (dig->physical_channels_per_sample == 1)
    {
        dig->hw_format = DIG_F_MONO_16;
    }
    else
    {
        if (dig->physical_channels_per_sample > 2)
        {
            dig->hw_format = DIG_F_MULTICHANNEL_16;
        }
        else
        {
            dig->hw_format = DIG_F_STEREO_16;
        }
    }

    // Check and make sure we aren't outside our platform's max.
    if (dig->physical_channels_per_sample > MAX_SPEAKERS)
    {
        AIL_set_error("Requested channel count is outside the maximum for this platform.");
        AIL_mem_free_lock(dig);
        return 0;
    }

    // Init sound system.
    RadSoundSystem* pSoundSystem = (RadSoundSystem*)AIL_mem_alloc_lock(sizeof(RadSoundSystem));
    if (pSoundSystem == 0)
    {
        AIL_set_error("Out of memory.");
        AIL_mem_free_lock(dig);
        return 0;
    }
    dig->dig_ss = pSoundSystem;

    S32 TotalMs = 0 ;
    if (dig->physical_channels_per_sample)
    {
        S32 MsPerBuffer = (S32)AIL_preference[DIG_DS_FRAGMENT_SIZE];
        TotalMs = MsPerBuffer * (S32)AIL_preference[DIG_DS_FRAGMENT_CNT];

        // hardware size is frament size.
        dig->hardware_buffer_size = (MsPerBuffer * frequency) / 1000;

        // Align.
        dig->hardware_buffer_size = (dig->hardware_buffer_size + 15) & ~15;

        //! \todo use a more accurate method for ensuring LCM of 2, chan, and 16.
        // Ensure we are divisible by a sample by doing it last.
        dig->hardware_buffer_size *= PhysChannelCount * 2;
    }

    U32 open_flags = 0;
#ifdef __RAD3DS__
    if (AIL_preference[AIL_3DS_USE_SYSTEM_CORE])
        open_flags |= RADSS_OPEN_3DS_USE_SYSTEM_CORE;
#endif

    if (dig_open(
        pSoundSystem, 
        TotalMs,
        dig->DMA_rate, 
        dig->physical_channels_per_sample,
        dig->hardware_buffer_size,
        open_flags) == 0)
    {
        AIL_set_error(pSoundSystem->m_ErrorString);
        AIL_mem_free_lock(pSoundSystem);
        AIL_mem_free_lock(dig);
        return 0;
    }

    if (dig->physical_channels_per_sample)
    {
        // Allocate the system's heap.
        dig->dig_heap = AIL_mem_alloc_lock(pSoundSystem->m_MemoryNeeded);
        if (dig->dig_heap == 0)
        {
            pSoundSystem->Close(pSoundSystem);
            AIL_mem_free_lock(pSoundSystem);
            AIL_mem_free_lock(dig);
            AIL_set_error("Out of memory.");
            return 0;
        }

        pSoundSystem->SetMem(pSoundSystem, dig->dig_heap);

        S32 physical_channels_per_buffer = dig->hardware_buffer_size / dig->bytes_per_channel;
        dig->samples_per_buffer = physical_channels_per_buffer / dig->physical_channels_per_sample;
    }

    dig->mix_ms = mult64anddiv( dig->samples_per_buffer, 256000, dig->DMA_rate );

#ifdef IS_XENON
    // We (apparently) need to be able to provide the xaudio2 source voice to the client.
    //! \todo move this out of the generic dig?
    dig->x2_voiceptr = (void*)pSoundSystem->GetSetProperty(pSoundSystem, RADSS_PROP_X2_SRCVOICE, 0);
#endif

    //
    // Allocate build buffer(s) for each *logical* channel, plus three working buffers for FLT chain
    //
    // The build buffers correspond 1:1 to the output DMA buffers, except when a matrix format is in use
    //
    if (dig->logical_channels_per_sample)
    {
        if (dig->logical_channels_per_sample <= 2)
        {
            //
            // Single mono or stereo build buffer
            //

            dig->n_build_buffers = 1;
        }
        else
        {
            //
            // channels/2 stereo build buffers (plus one leftover mono build buffer, if necessary)
            //

            dig->n_build_buffers = (dig->logical_channels_per_sample / 2) + (dig->logical_channels_per_sample & 1);
        }
    }

    AIL_memset(dig->build,
        0,
        sizeof(dig->build));

    S32 nch = dig->logical_channels_per_sample;

    S32 spkr_chan = 0;

    for (i=0; i < dig->n_build_buffers; i++)
    {
        dig->build[i].chans  = (nch > 1) ? 2 : 1;
        dig->build[i].bytes  = sizeof(S32) * dig->samples_per_buffer * dig->build[i].chans;
        dig->build[i].buffer = (S32 *) AIL_mem_alloc_lock(dig->build[i].bytes);

        dig->build[i].speaker_offset = spkr_chan;
        spkr_chan += dig->build[i].chans;

        if (dig->build[i].buffer == NULL)
        {
            AIL_set_error("Could not allocate build buffer.");

free_build:
            for (S32 j=0; j < (S32)(sizeof(dig->build) / sizeof(dig->build[0])); j++)
            {
                if (dig->build[j].buffer != NULL)
                {
                    AIL_mem_free_lock(dig->build[j].buffer);
                    dig->build[j].buffer = NULL;
                }
            }

            RadSoundSystem* pSoundSystem = (RadSoundSystem*)dig->dig_ss;
            if (pSoundSystem) pSoundSystem->Close(pSoundSystem);

            AIL_mem_free_lock(dig->dig_heap);
            AIL_mem_free_lock(dig->dig_ss);
            AIL_mem_free_lock(dig);

            return 0;
        }

        nch -= dig->build[i].chans;
    }

    //
    // buffer for deinterlacing samples
    //

    dig->MC_buffer = AIL_mem_alloc_lock( sizeof(S16) * dig->samples_per_buffer + 16 );

    for (i=0; i < EXTRA_BUILD_BUFFERS; i++)
    {
        S32 j = MAX_SPEAKERS + i;

        //
        // Working build buffers always have space allocated for 2 channels, even
        // though they may be used as mono buffers by the filter chain
        //

        dig->build[j].chans  = 2;
        dig->build[j].bytes  = sizeof(S32) * dig->samples_per_buffer * dig->build[j].chans;
        dig->build[j].buffer = (S32 *) AIL_mem_alloc_lock(dig->build[j].bytes);

        if (dig->build[j].buffer == NULL)
        {
            AIL_set_error("Could not allocate build buffer.");
            goto free_build;
        }
    }

    if (AIL_allocate_reverb_buffers( dig, 0) == 0 )
    {
        AIL_set_error("Could not allocate reverb build buffer.");
        goto free_build;
    }

    //
    // Initialize default speaker-position array
    //
    // Positions assume left-handed coordinate frame with identity =
    // face = +Z, right = +X, up = +Y
    //

#define RSQRTF 0.707106781F

    MSSVECTOR3D SPKR[MAX_SPEAKERS];  // Listener-relative speaker locations

    AIL_memset(SPKR,
        0,
        sizeof(SPKR));

    S32 spat_spkr_count = dig->logical_channels_per_sample;
    switch (dig->channel_spec)
    {
    case MSS_MC_USE_SYSTEM_CONFIG:
    case MSS_MC_EAX2:
    case MSS_MC_EAX3:
    case MSS_MC_EAX4:
    case MSS_MC_DIRECTSOUND3D: 
    case MSS_MC_FORCE_32:
    case MSS_MC_INVALID: break;

    case MSS_MC_MONO:
        {
            SPKR[0].x = 0.0F;   SPKR[0].y = 0.0F;   SPKR[0].z = 1.0F;  
            break;
        }
    case MSS_MC_STEREO:
        {
            SPKR[0].x = -RSQRTF; SPKR[0].y = 0.0F; SPKR[0].z = RSQRTF;
            SPKR[1].x =  RSQRTF; SPKR[1].y = 0.0F; SPKR[1].z = RSQRTF;
            break;
        }
    case MSS_MC_HEADPHONES:   
        {
            SPKR[0].x = -1.0F; SPKR[0].y = 0.0F; SPKR[0].z = 0.0F;
            SPKR[1].x =  1.0F; SPKR[1].y = 0.0F; SPKR[1].z = 0.0F;
            break;
        }
#if MAX_SPEAKERS > 2
    case MSS_MC_DOLBY_SURROUND:
        {
            SPKR[0].x = -RSQRTF; SPKR[0].y = 0.0F;   SPKR[0].z =  RSQRTF;
            SPKR[1].x =  RSQRTF; SPKR[1].y = 0.0F;   SPKR[1].z =  RSQRTF;
            SPKR[2].x =  0.0F;   SPKR[2].y = 0.0F;   SPKR[2].z = -1.0F;  
            break;
        }
    case MSS_MC_40_DISCRETE:
    case MSS_MC_40_DTS:
        {
            SPKR[0].x = -RSQRTF; SPKR[0].y = 0.0F;   SPKR[0].z =  RSQRTF;
            SPKR[1].x =  RSQRTF; SPKR[1].y = 0.0F;   SPKR[1].z =  RSQRTF;
            SPKR[2].x = -RSQRTF; SPKR[2].y = 0.0F;   SPKR[2].z = -RSQRTF;
            SPKR[3].x =  RSQRTF; SPKR[3].y = 0.0F;   SPKR[3].z = -RSQRTF;
            break;
        }
    case MSS_MC_51_DTS:
    case MSS_MC_51_DISCRETE:
        {
            SPKR[0].x = -RSQRTF; SPKR[0].y = 0.0F;   SPKR[0].z =  RSQRTF;
            SPKR[1].x =  RSQRTF; SPKR[1].y = 0.0F;   SPKR[1].z =  RSQRTF;
            SPKR[2].x =  0.0F;   SPKR[2].y = 0.0F;   SPKR[2].z =  1.0F;  
            SPKR[3].x =  0.0F;   SPKR[3].y = 0.0F;   SPKR[3].z =  0.0F;  
            SPKR[4].x = -RSQRTF; SPKR[4].y = 0.0F;   SPKR[4].z = -RSQRTF;
            SPKR[5].x =  RSQRTF; SPKR[5].y = 0.0F;   SPKR[5].z = -RSQRTF;
            break;
        }
#else
    case MSS_MC_DOLBY_SURROUND:
    case MSS_MC_40_DISCRETE:
    case MSS_MC_40_DTS:
    case MSS_MC_51_DTS:
    case MSS_MC_51_DISCRETE:
        {
            AIL_set_error("Invalid channel spec for platform.");
            goto free_build;
            break;
        }
#endif
#if MAX_SPEAKERS > 6
    case MSS_MC_61_DISCRETE:
    case MSS_MC_SRS_CIRCLE_SURROUND:
        {
            SPKR[0].x = -RSQRTF; SPKR[0].y = 0.0F;   SPKR[0].z =  RSQRTF;
            SPKR[1].x =  RSQRTF; SPKR[1].y = 0.0F;   SPKR[1].z =  RSQRTF;
            SPKR[2].x =  0.0F;   SPKR[2].y = 0.0F;   SPKR[2].z =  1.0F;  
            SPKR[3].x =  0.0F;   SPKR[3].y = 0.0F;   SPKR[3].z =  0.0F;  
            SPKR[4].x = -RSQRTF; SPKR[4].y = 0.0F;   SPKR[4].z = -RSQRTF;
            SPKR[5].x =  RSQRTF; SPKR[5].y = 0.0F;   SPKR[5].z = -RSQRTF;
            SPKR[6].x =  0.0F;   SPKR[6].y = 0.0F;   SPKR[6].z = -1.0F;  
            break;
        }
#else
    case MSS_MC_61_DISCRETE:
    case MSS_MC_SRS_CIRCLE_SURROUND:
        {
            AIL_set_error("Invalid channel spec for platform.");
            goto free_build;
            break;
        }
#endif
#if MAX_SPEAKERS > 7
    case MSS_MC_71_DISCRETE:
        {
#ifdef IS_WIIU
            // Two of the channels are never spatialized to.
            spat_spkr_count = 6;
            SPKR[0].x = -RSQRTF; SPKR[0].y = 0.0F;   SPKR[0].z =  RSQRTF;
            SPKR[1].x =  RSQRTF; SPKR[1].y = 0.0F;   SPKR[1].z =  RSQRTF;
            SPKR[2].x =  0.0F;   SPKR[2].y = 0.0F;   SPKR[2].z =  1.0F;  
            SPKR[3].x =  0.0F;   SPKR[3].y = 0.0F;   SPKR[3].z =  0.0F;  
            SPKR[4].x = -RSQRTF; SPKR[4].y = 0.0F;   SPKR[4].z = -RSQRTF;
            SPKR[5].x =  RSQRTF; SPKR[5].y = 0.0F;   SPKR[5].z = -RSQRTF;

#else
            SPKR[0].x = -RSQRTF; SPKR[0].y = 0.0F;   SPKR[0].z =  RSQRTF;
            SPKR[1].x =  RSQRTF; SPKR[1].y = 0.0F;   SPKR[1].z =  RSQRTF;
            SPKR[2].x =  0.0F;   SPKR[2].y = 0.0F;   SPKR[2].z =  1.0F;  
            SPKR[3].x =  0.0F;   SPKR[3].y = 0.0F;   SPKR[3].z =  0.0F;  
            SPKR[4].x = -RSQRTF; SPKR[4].y = 0.0F;   SPKR[4].z = -RSQRTF;
            SPKR[5].x =  RSQRTF; SPKR[5].y = 0.0F;   SPKR[5].z = -RSQRTF;
            SPKR[6].x = -1.0F;   SPKR[6].y = 0.0F;   SPKR[6].z =  0.0F;  
            SPKR[7].x =  1.0F;   SPKR[7].y = 0.0F;   SPKR[7].z =  0.0F;  
#endif
            break;
        }
#else
    case MSS_MC_71_DISCRETE:
        {
            AIL_set_error("Invalid channel spec for platform.");
            goto free_build;
            break;
        }
#endif
#if MAX_SPEAKERS > 8
    case MSS_MC_81_DISCRETE:
        {
            SPKR[0].x = -RSQRTF; SPKR[0].y = 0.0F;   SPKR[0].z =  RSQRTF;
            SPKR[1].x =  RSQRTF; SPKR[1].y = 0.0F;   SPKR[1].z =  RSQRTF;
            SPKR[2].x =  0.0F;   SPKR[2].y = 0.0F;   SPKR[2].z =  1.0F;  
            SPKR[3].x =  0.0F;   SPKR[3].y = 0.0F;   SPKR[3].z =  0.0F;  
            SPKR[4].x = -RSQRTF; SPKR[4].y = 0.0F;   SPKR[4].z = -RSQRTF;
            SPKR[5].x =  RSQRTF; SPKR[5].y = 0.0F;   SPKR[5].z = -RSQRTF;
            SPKR[6].x =  0.0F;   SPKR[6].y = 0.0F;   SPKR[6].z = -1.0F;  
            SPKR[7].x = -1.0F;   SPKR[7].y = 0.0F;   SPKR[7].z =  0.0F;  
            SPKR[8].x =  1.0F;   SPKR[8].y = 0.0F;   SPKR[8].z =  0.0F;  
            break;
        }
#else
    case MSS_MC_81_DISCRETE:
        {
            AIL_set_error("Invalid channel spec for platform.");
            goto free_build;
            break;
        }
#endif
    } // end speaker setup.

    //
    // Set default falloff power and reverb response
    //

    AIL_set_speaker_configuration(dig, 
        SPKR, 
        spat_spkr_count, 
        3.0F);

    AIL_set_speaker_reverb_levels(dig, 
        NULL,
        NULL,
        NULL,
        0);

#ifdef IS_X86
    dig->use_MMX = AIL_MMX_available();
#endif

    //
    // Allocate physical SAMPLE structures for driver
    //

    dig->n_samples        = (S32)AIL_get_preference(DIG_MIXER_CHANNELS);
    dig->n_active_samples = 0;

    dig->master_volume    = 1.0F;
    dig->reverb[0].master_dry       = 1.0F;
    dig->reverb[0].master_wet       = 1.0F;

    dig->samples = (HSAMPLE) AIL_mem_alloc_lock(sizeof(struct _SAMPLE) * dig->n_samples);

    if (dig->samples == NULL)
    {
free_reverb:
        AIL_set_error("Could not allocate SAMPLE structures.");

        AIL_mem_free_lock(dig->reverb[0].reverb_build_buffer);
        goto free_build;
    }

    dig->sample_status = (U32 *) AIL_mem_alloc_lock(sizeof(U32) * dig->n_samples);

    if (dig->sample_status == NULL)
    {
        AIL_mem_free_lock((void *) dig->samples);
        goto free_reverb;
    }

    AIL_memset(dig->samples,
        0,
        sizeof(struct _SAMPLE) * dig->n_samples);

    for (i=0; i < dig->n_samples; i++)
    {
        SETTAG(dig->samples[i].tag,"HSAM");

        dig->sample_status[i] = SMP_FREE;
        dig->samples[i].index  = i;
        dig->samples[i].driver = dig;
        dig->samples[i].is_3D  = FALSE;
    }

    //
    // Link HDIGDRIVER into chain used by timers
    //

    if (DIG_first!=NULL)
    {
        dig->next=DIG_first;
    }
    else
    {
        dig->next = NULL;

        AIL_primary_digital_driver(dig);
    }

    DIG_first = dig;

    //
    // Background timer 
    //

    dig->backgroundtimer = AIL_register_timer((AILTIMERCB)SS_serve);

    //
    // If timer allocation failed, release all resources and return
    // error
    //

    if (dig->backgroundtimer == -1)
    {
        AIL_set_error("Out of timer handles.");

        AIL_mem_free_lock((void *) dig->reverb[0].reverb_build_buffer);
        AIL_mem_free_lock((void *) dig->samples);
        AIL_mem_free_lock((void *) dig->sample_status);
        goto free_build;
    }

    //
    // Init driver pipeline stages
    //

    for (i=0; i < N_DIGDRV_STAGES; i++)
    {
        dig->pipeline[i].active = 0;
    }

    //
    // Select default mixer flush/copy providers
    //

    HPROVIDER HP;

    RIB_enumerate_providers("MSS mixer services",
        NULL,
        &HP);

    if (HP == 0)
    {
        AIL_set_error("No mixer services exist.");
        AIL_mem_free_lock((void *) dig->reverb[0].reverb_build_buffer);
        AIL_mem_free_lock((void *) dig->samples);
        AIL_mem_free_lock((void *) dig->sample_status);
        goto free_build;
    }

    AIL_set_digital_driver_processor(dig,
        DP_DEFAULT_FILTER,
        0);

    AIL_set_digital_driver_processor(dig,
        DP_DEFAULT_MERGE,
        HP);

    AIL_set_digital_driver_processor(dig,
        DP_FLUSH,
        HP);

    AIL_set_digital_driver_processor(dig,
        DP_COPY,
        HP);

    AIL_set_digital_driver_processor(dig,
        DP_MC_COPY,
        HP);

    AIL_set_digital_driver_processor( dig,
        DP_ADPCM_DECODE,
        HP);

    //
    // Start 100 Hz background service timer to process WOM_DONE events
    //

    AIL_set_timer_frequency(dig->backgroundtimer,         1000 / (S32) AIL_preference[DIG_DS_FRAGMENT_SIZE]);
    AIL_set_timer_user     (dig->backgroundtimer, (UINTa) dig);
    AIL_start_timer        (dig->backgroundtimer             );

    //
    // If we need to install an output filter, do it now.
    //

    //
    // Output filters don't exist for platforms other than these, so if
    // we need one, explicitely fail.
    //
#if !defined(IS_WIN32) && !defined(IS_PS2) && !defined(IS_MAC) && !defined(IS_WII) && !defined(IS_WIIU)
#define MC_open_output_filter(x, y, z) (0)
#endif

    S32 FilterOk = 1;
    switch (channel_spec)
    {
    case MSS_MC_40_DTS:
    case MSS_MC_51_DTS:
        {
            FilterOk = MC_open_output_filter("DTS 4.0/5.1 Matrix Filter", dig, TRUE);
            break;
        }
    case MSS_MC_SRS_CIRCLE_SURROUND:
        {
            FilterOk = MC_open_output_filter("SRS Circle Surround 6.1 Matrix Filter", dig, TRUE);
            break;
        }
    case MSS_MC_DOLBY_SURROUND:
        {
            FilterOk = MC_open_output_filter("Dolby Surround Matrix Filter", dig, TRUE);
            break;
        }
    case MSS_MC_DIRECTSOUND3D:
        {
            FilterOk = MC_open_output_filter("DirectSound3D Hardware Support", dig, FALSE);
            break;
        }
    case MSS_MC_EAX2:
        {
            FilterOk = MC_open_output_filter("Creative Labs EAX 2 (TM)", dig, FALSE);
            break;
        }
    case MSS_MC_EAX3:
        {
            FilterOk = MC_open_output_filter("Creative Labs EAX 3 (TM)", dig, FALSE);
            break;
        }
    case MSS_MC_EAX4:
        {
            FilterOk = MC_open_output_filter("Creative Labs EAX 4 (TM)", dig, FALSE);
            break;
        }
    }

#ifdef MC_open_output_filter
#undef MC_open_output_filter
#endif

    if (FilterOk == 0)
    {
        AIL_API_close_digital_driver(dig);
        AIL_set_error("Couldn't create output filter.");
        return 0;
    }

    //
    // Return normally
    //

    return dig;
}
} // extern C

//############################################################################
//##                                                                        ##
//## Shut down digital driver and free output buffers                       ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_close_digital_driver(HDIGDRIVER dig)
{
  HDIGDRIVER cur,prev;
  S32        i;

  if ((dig == NULL) || (DIG_first == NULL))
  {
    return;
  }

  AILSTRM_shutdown(dig);

  rrAtomicAddExchange32( &SS_servicing, 1 );

  InMilesMutex();

  //
  // Stop playback
  //

  SS_stop_DIG_driver_playback(dig);

  //
  // Release any open sample handles (to ensure that pipeline resources
  // are deallocated properly)
  //

  for (i=0; i < dig->n_samples; i++)
  {
    if (dig->sample_status[i] != SMP_FREE)
    {
      AIL_release_sample_handle(&dig->samples[i]);
    }
  }

  // Release any busses allocated on the driver.
  for (i = 0; i < MAX_BUSSES; i++)
  {
      if (dig->bus_ptrs[i])
          AIL_mem_free_lock(dig->bus_ptrs[i]);
      if (dig->reverb[i].reverb_build_buffer)
          AIL_mem_free_lock(dig->reverb[i].reverb_build_buffer);
  }

  //
  // Release any filters associated with this driver
  //

  FLT_disconnect_driver(dig);

  //
  // Kill buffer maintenance timer
  //

  AIL_release_timer_handle(dig->backgroundtimer);

  //
  // Unlink from foreground service chain
  //

  if (dig == DIG_first)
  {
    DIG_first = dig->next;
  }
  else
  {
    prev = DIG_first;
    cur  = DIG_first->next;

    while (cur != dig)
    {
      if (cur == NULL)
      {
        goto out_of_list;
      }

      prev = cur;
      cur  = cur->next;
    }

    prev->next = cur->next;
  }
 
 out_of_list:
 
  //
  // Reset driver to force return of all structures
  //

  OutMilesMutex();

  InMilesMutex();

  //
  // Close driver and free resources
  //

  RadSoundSystem* pSoundSystem = (RadSoundSystem*)dig->dig_ss;
  if (pSoundSystem)
  {
      pSoundSystem->Close(pSoundSystem);
  }
  AIL_mem_free_lock(dig->dig_heap);
  AIL_mem_free_lock(dig->dig_ss);

  S32 j;

  AIL_mem_free_lock((void *) dig->samples);
  AIL_mem_free_lock((void *) dig->sample_status);

  for (j=0; j < (S32)(sizeof(dig->build) / sizeof(dig->build[0])); j++)
  {
    if (dig->build[j].buffer != NULL)
    {
      AIL_mem_free_lock(dig->build[j].buffer);
      dig->build[j].buffer = NULL;
    }
  }

  if ( dig->MC_buffer )
    AIL_mem_free_lock( dig->MC_buffer );

  rrAtomicAddExchange32( &SS_servicing, -1 );

  OutMilesMutex();

  AIL_mem_free_lock(dig);
}

//############################################################################
//##                                                                        ##
//## Externally-callable service function                                   ##
//##                                                                        ##
//############################################################################

static U32 volatile only_one_serve=0;
static U32 last_serve=0;

static void low_serve()
{
  last_serve=AIL_ms_count();

  rrAtomicAddExchange32(&only_one_serve,1);

  if (only_one_serve==1)
  {
    OutMilesMutex();
    stream_background();
    InMilesMutex();
  }
       
  rrAtomicAddExchange32(&only_one_serve,-1);

  // check all the buffer status's
  HDIGDRIVER dig=DIG_first;
  while (dig)
  {
    SS_serve(dig);
    dig=dig->next;
  }
}


void AILCALL AIL_API_serve()
{
  U32 t=AIL_ms_count();
  if ((t-last_serve)>(U32)AIL_preference[DIG_DS_FRAGMENT_SIZE])
  {
    InMilesMutex();
    low_serve();
    OutMilesMutex();
  }
}

//############################################################################
//##                                                                        ##
//## Get digital driver configuration                                       ##
//##                                                                        ##
//############################################################################
void
AILCALL 
AIL_API_digital_configuration(
    HDIGDRIVER dig,
    S32* rate,
    S32* format,
    char* config
    )
{
    if (dig==NULL)
        return;

    if (rate != NULL)
    {
        *rate = dig->DMA_rate;
    }

    if (format != NULL)
    {
        *format = dig->hw_format;
    }

    if (config != NULL)
    {
        RadSoundSystem* pRadSS = (RadSoundSystem*)dig->dig_ss;
        AIL_strcpy(config, (const char*)pRadSS->GetSetProperty(pRadSS, RADSS_PROP_TYPESTRING, 0));
    }
}

S32 AILCALL AIL_API_digital_latency(HDIGDRIVER dig)
{
    if (dig)
    {
        return( mult64anddiv(dig->hardware_buffer_size,1000,(dig->DMA_rate * 2 * dig->logical_channels_per_sample))*
            (S32)AIL_preference[DIG_DS_MIX_FRAGMENT_CNT] );
    }

    return(0);
}

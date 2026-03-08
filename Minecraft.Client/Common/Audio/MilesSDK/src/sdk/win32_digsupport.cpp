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

#include <windows.h>
#define DIRECTSOUND_VERSION 0x300
#include <dsound.h>

#include "mss.h"
#include "imssapi.h"

extern "C" void AILCALL AIL_sleep(U32 ms)
{
    Sleep(ms);
}

S32 UpdateDSHwnd(HDIGDRIVER dig)
{
    if (dig->dig_ss)
    {
        OutMilesMutex();
        RadSoundSystem* pSS = (RadSoundSystem*)dig->dig_ss;
        if (pSS->GetSetProperty(pSS, RADSS_PROP_DS_HWND, &dig->dsHwnd) == 0)
        {
            InMilesMutex();
            AIL_set_error(pSS->m_ErrorString);
            return 0;
        }
        InMilesMutex();
    }

    return 1;

}

typedef HRESULT (WINAPI *DirectSoundCreateType)(void* lpGUID,void* lpds,void* pUnkOuter);
static DirectSoundCreateType DirectSoundCreatePtr=0;

S32 AILCALL get_system_speaker_configuration(MSS_MC_SPEC *channel_spec)
{
    // Default to stereo.
    *channel_spec = MSS_MC_STEREO;

    UINT err = SetErrorMode(0x8000);
    HINSTANCE DSDLL = LoadLibrary("DSOUND.DLL");
    SetErrorMode(err);

    if (DSDLL == 0) return 0;

    DirectSoundCreateType DSCreate = (DirectSoundCreateType)GetProcAddress(DSDLL, "DirectSoundCreate");
    if (DSCreate == 0)
    {
        FreeLibrary(DSDLL);
        return 0;
    }

    // Create a dsound instance in order to get the speaker config.
    LPDIRECTSOUND pDS = 0;
    HRESULT hr = DSCreate(0, &pDS, 0);

    if (FAILED(hr))
    {
        FreeLibrary(DSDLL);
        return 0;
    }

    DWORD SpeakerConfig = 0;
    hr = pDS->GetSpeakerConfig(&SpeakerConfig);

    pDS->Release();
    FreeLibrary(DSDLL);

    if (FAILED(hr))
    {
        FreeLibrary(DSDLL);
        return 0;
    }

    switch (DSSPEAKER_CONFIG(SpeakerConfig & 0xFF))
    {
        case DSSPEAKER_DIRECTOUT : *channel_spec = MSS_MC_STEREO;              break; // Not clear what should be done with this
        case DSSPEAKER_HEADPHONE : *channel_spec = MSS_MC_HEADPHONES;          break;
        case DSSPEAKER_MONO      : *channel_spec = MSS_MC_MONO;                break;
        case DSSPEAKER_QUAD      : *channel_spec = MSS_MC_40_DISCRETE;         break;
        case DSSPEAKER_STEREO    : *channel_spec = MSS_MC_STEREO;              break;
        case DSSPEAKER_SURROUND:
            {
                // 
                // For generic "surround" configurations, we prefer SRS, then Dolby, then 
                // fall back to stereo if neither of those providers are available
                //
                if (AIL_find_filter("SRS Circle Surround 6.1 Matrix Filter", NULL))
                {
                    *channel_spec = MSS_MC_SRS_CIRCLE_SURROUND; 
                }
                else if (AIL_find_filter("Dolby Surround Matrix Filter", NULL))
                {
                    *channel_spec = MSS_MC_DOLBY_SURROUND;
                }
                else
                {
                    *channel_spec = MSS_MC_STEREO;
                }
                break;
            } 
        case DSSPEAKER_5POINT1   : *channel_spec = MSS_MC_51_DISCRETE;         break;
        case DSSPEAKER_7POINT1   : *channel_spec = MSS_MC_71_DISCRETE;         break;

    }

    return 1;
}

// Deprecated stuff for backcompat.
//-----------------------------------------------------------------------------

S32 AILCALL AIL_API_digital_handle_release(HDIGDRIVER) { return 1;}
S32 AILCALL AIL_API_digital_handle_reacquire(HDIGDRIVER) { return 1;}

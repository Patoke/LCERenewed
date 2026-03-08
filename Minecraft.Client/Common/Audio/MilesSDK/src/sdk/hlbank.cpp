//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  For technical support, contact RAD Game Tools at 425-893-4300.        ##
//##  or at miles3@radgametools.com                                         ##
//##                                                                        ##
//############################################################################

#include "mss.h"
#include "imssapi.h"

#include "hlruntime.h"
#include "mileseventhandle.h"
#include "mileseventarray.h"
#include "mileseventinternal.h"

#define RESOLVE(x, isfloat) if ((x)->sNameOffset) fail |= !eval(context, (char const*)AIL_ptr_add(preset, (x)->sNameOffset), -1, &(x)->fData, isfloat);

#define RESOLVE_FILTER(x) if ((x).ValueString) fail |= !eval(context, (char const*)AIL_ptr_add(preset, (x).ValueString), -1, &(x).Value, 1);

//
// SoundBanks are laid out on disk in the endianness of the target platform.
//
// SoundBank::Events, Env, Presets, Sounds, and Files are stored on disk as
// the offset in the file to the location of those structures, and patched up
// on load. Enough room is stored on disk for the pointer size of the target
// platform.
//
// +-----------------------+
// | sizeof(SoundBank)
// +-----------------------+
// | Events (sizeof(AssetEntry) * EventCount)
// +-----------------------+
// | Environments (")
// +-----------------------+
// | Presets
// +-----------------------+
// | Sounds
// +-----------------------+
// | Settings
// +-----------------------+
// | String Table
// +-----------------------+
// | Sound Data
// +-----------------------+
// size = SoundBank::MetaSize
//
// Anything on disk past the MetaSize are sound files (SoundBank::FileEntry) and
// not loaded in to memory on bank load.
//

// also in MssBank.cpp
#define BANK_VERSION 8


struct SoundBank
{
    S32 Tag; // 'BANK'
    S32 Version;

    // Size of the bank, excluding sound data
    S32 MetaSize; 

    // Allocated on load, the file name passed to AIL_open_soundbank()
    S32 SoundBankFileNameLen;
    char* SoundBankFileName;

    struct AssetEntry
    {
        // The offset from the SoundBank to the Name in the string table.
        U32 NameOffset;

        // The offset from the SoundBank to the asset data. eg Preset, Environment.
        U32 DataOffset;
    };

    AssetEntry* Events, *Envs, *Presets, *Sounds;
    void* Settings;

    S32 EventCount, EnvCount, PresetCount, SoundCount;


    char SoundBankName[4];
};

struct Sound
{
    U32 NameOffset;
    U32 FileNameOffset;
    S32 FileOffset;
    MILESBANKSOUNDINFO Info;

    S32 MarkerCount;
    struct Marker
    {
        S32 NameOffset; // Offset from the MarkerList to the string table entry.
        S32 Offset; // Offset in the sound file in compressed space for the marker.
        S32 IsSample; // Nonzero if the marker is stored in sample space and not bytes
    };
    Marker Markers[1];    
};

struct Environment
{
    U32 Tag;
    U32 NameOffset; // Offset from the SoundBank to the environment name. Same as the AssetEntry for the environment.
    U32 Mask; // Bits set for what attributes the environment affects.
    U32 EnvSize;

    struct Value
    {
        U32 Bit;
        struct
        {
            union 
            {
                S32 iData;
                F32 fData;
            };
            U32 sNameOffset;
        } Data[1];
    };

    S32 Values[1];
};


struct Preset
{
    U32 Tag;    // has to be 'PST '.
    U32 NameOffset; // Offset from the SoundBank to the preset name. Same as the AssetEntry for the preset
    U32 Mask; // Bits set for what attributes the preset affects.
    U32 PresetSize;

    //
    // Presets are laid out with a series of Values. The Bit determines how many
    // values actual exist. Filters act basically the same way, except their 'Values' are
    // properties for the filter.
    //

    struct Value
    {
        U32 Bit;
        struct
        {
            union 
            {
                S32 iData;
                F32 fData;
            };
            U32 sNameOffset;
        } Data[1];
    };

    struct Graph
    {
        U32 Bit; // must be BIT_SND_3D_FALLOFF_GRAPH
        FalloffGraph G;
    };

    struct Filter
    {
        U32 Bit; // must be BIT_SND_FILTER_PROPERTY
        S32 Index;

        U32 FilterNameOffset;
        S32 PropCount;
        
        struct Property
        {
            U32 NameOffset;
            F32 Value;
            U32 ValueString;
        };

        Property Props[1];
    };

    S32 Values[1]; // Filters are also in this list. S32 to hopefully get the compiler to load by dword.
};

static char ourtoupper( char c )
{
    if ( ( c >= 'a' ) && ( c <= 'z' ) )
        return( (char) (c - ( 'a' - 'A' )) );
    return( c );
}

static S32 asset_name_cmp(char const* s1, char const* s2)
{
    char c1,c2;
    U8 const* st1=(U8 const*)s1;
    U8 const* st2=(U8 const*)s2;

    for(;;) 
    {
        c1=*st1;
        c1=ourtoupper(c1);
        c2=*st2;
        c2=ourtoupper(c2);
        if (c1<c2)
            return(-1);
        if (c1>c2)
            return(1);
        if (c1==0)
            break;
        st1=(U8 const*)AIL_ptr_add(st1,1);
        st2=(U8 const*)AIL_ptr_add(st2,1);
    }

    return(0);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static SoundBank::AssetEntry* FindAsset(SoundBank* i_Bank, char const* i_Name, SoundBank::AssetEntry* i_List, S32 i_Count)
{
    if (i_Count == 0) return 0;

    S32 MidPoint = i_Count / 2;
    S32 CheckResult = asset_name_cmp((char*)AIL_ptr_add(i_Bank, i_List[MidPoint].NameOffset), i_Name);

    if (CheckResult == 0)
    {
        return i_List + MidPoint;
    }

    if (i_Count == 1) return 0;

    if (CheckResult > 0)
    {
        return FindAsset(i_Bank, i_Name, i_List, MidPoint);
    }
    else
    {
        return FindAsset(i_Bank, i_Name, i_List + MidPoint + 1, i_Count - MidPoint - 1);
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT AIL_close_soundbank(SoundBank* i_Bank)
{
    if (i_Bank)
    {
        AIL_mem_free_lock(i_Bank->SoundBankFileName);
        AIL_mem_free_lock(i_Bank);
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void CloseAsync(MilesAsyncRead* R)
{
    if (R->FileHandle == 0)
        return;

    // Issue a zero count read.
    R->Count = 0;
    R->Flags = 0;
    MilesAsyncFileRead(R);
    MilesAsyncFileStatus(R, ~0U);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF SoundBank* AILEXPORT AIL_open_soundbank(char const *filename, char const* name)
{
    if (filename == NULL)
    {
        return NULL;
    }

#define HEADER_READ_SIZE 4096

    char HeaderData[HEADER_READ_SIZE];

    MilesAsyncRead R;
    AIL_memset(&R, 0, sizeof(R));

    R.Buffer = HeaderData;
    strcpy(R.FileName, filename);
    R.caller = __FILE__;
    R.caller_line = __LINE__;
    R.Count = HEADER_READ_SIZE;
    R.Flags = MSSIO_FLAGS_DONT_CLOSE_HANDLE;

    // Read enough of the header to get the metadata size.
    MilesAsyncFileRead(&R);
    S32 Result = MilesAsyncFileStatus(&R, ~0U);
    if (Result != MSSIO_STATUS_COMPLETE)
    {
        // Failed to open
        CloseAsync(&R);
        AIL_set_error("Failed to open sound bank.");
        return 0;
    }

    // Check tag.
    if (((SoundBank*)HeaderData)->Tag != 'BANK')
    {
        // Invalid file - close and bail.
        CloseAsync(&R);
        AIL_set_error("Sound file isn't a bank.");
        return 0;
    }

    // Check version
    if (((SoundBank*)HeaderData)->Version != BANK_VERSION)
    {
        CloseAsync(&R);
        AIL_set_error("Bank file version doesn't match this runtime.");
        return 0;
    }

    // Check name
    if (name && AIL_stricmp(((SoundBank*)HeaderData)->SoundBankName, name))
    {
        CloseAsync(&R);
        AIL_set_error("Bank file name doesn't match the loaded file.");
        return 0;
    }

    SoundBank* pHeader = (SoundBank*)HeaderData;
    SoundBank* pBank = (SoundBank*)AIL_mem_alloc_lock(pHeader->MetaSize);

    // Read the rest of the meta data.
    S32 MetaSizeReady = R.ReadAmt;
    if (MetaSizeReady > pHeader->MetaSize)
        MetaSizeReady = pHeader->MetaSize;

    memcpy(pBank, HeaderData, MetaSizeReady);

    if (pHeader->MetaSize > R.ReadAmt)
    {
        R.Buffer = AIL_ptr_add(pBank, R.ReadAmt);
        R.Count = pHeader->MetaSize - R.ReadAmt;
        R.Flags = MSSIO_FLAGS_DONT_USE_OFFSET;
        R.caller = __FILE__;
        R.caller_line = __LINE__;
        R.ReadAmt = 0;
        MilesAsyncFileRead(&R);
        S32 ReadResult = MilesAsyncFileStatus(&R, ~0U);
        if (ReadResult != MSSIO_STATUS_COMPLETE)
        {
            AIL_mem_free_lock(pBank);
            return 0;
        }

        // File is already closed...
    }
    else
    {
        CloseAsync(&R);
    }


    //
    // Everything except the sound file data is in memory now.
    //
    // Patch up the pointers. The offsets are stored where the pointers should be.
    //
    S32 Offset = *(S32*)&pBank->Events;
    pBank->Events = (SoundBank::AssetEntry*)AIL_ptr_add(pBank, Offset);

    Offset = *(S32*)&pBank->Envs;
    pBank->Envs = (SoundBank::AssetEntry*)AIL_ptr_add(pBank, Offset);

    Offset = *(S32*)&pBank->Sounds;
    pBank->Sounds = (SoundBank::AssetEntry*)AIL_ptr_add(pBank, Offset);

    Offset = *(S32*)&pBank->Presets;
    pBank->Presets = (SoundBank::AssetEntry*)AIL_ptr_add(pBank, Offset);

    Offset = *(S32*)&pBank->Settings;
    if (Offset)
    {
        pBank->Settings = AIL_ptr_add(pBank, Offset);
        AIL_set_event_settings(pBank->Settings);
    }

    // Copy the filename.
    S32 FileNameLen = (S32)AIL_strlen(filename);
    pBank->SoundBankFileName = (char*)AIL_mem_alloc_lock(FileNameLen + 1);
    AIL_memcpy(pBank->SoundBankFileName, filename, FileNameLen + 1);
    pBank->SoundBankFileNameLen = FileNameLen;

    return pBank;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_enumerate_events(SoundBank* i_Bank, HMSSENUM* next, char const* list, char const** name)
{
    if (i_Bank->Tag != 'BANK') 
        return 0;

    if (*next == MSS_FIRST)
        *next = 0;

    if (*next >= i_Bank->EventCount)
        return 0;

    *name = (char*)AIL_ptr_add(i_Bank, i_Bank->Events[*next].NameOffset);
    *next = *next + 1;
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_enumerate_sound_presets(SoundBank* i_Bank, HMSSENUM* next, char const* list, char const **name)
{
    if (i_Bank->Tag != 'BANK')
        return 0;

    if (*next == MSS_FIRST)
        *next = 0;

    if (*next >= i_Bank->PresetCount) 
        return 0;

    *name = (char*)AIL_ptr_add(i_Bank, i_Bank->Presets[*next].NameOffset);
    *next = *next + 1;
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_enumerate_environment_presets(SoundBank* i_Bank, HMSSENUM* next, char const* list, char const **name)
{   
    if (i_Bank->Tag != 'BANK') 
        return 0;

    if (*next == MSS_FIRST)
        *next = 0;

    if (*next >= i_Bank->EnvCount) 
        return 0;

    *name = (char*)AIL_ptr_add(i_Bank, i_Bank->Envs[*next].NameOffset);
    *next = *next + 1;
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_get_event_contents(SoundBank* i_Bank, char const* name, U8 const** event)
{
    if (i_Bank == NULL || event == NULL || name == NULL)
        return 0;

    if (i_Bank->Tag != 'BANK') 
        return 0;

    SoundBank::AssetEntry* pAsset = FindAsset(i_Bank, name, i_Bank->Events, i_Bank->EventCount);
    if (pAsset == 0)
    {
        AIL_set_error("Event Not Found.");
        return 0;
    }

    *event = (unsigned char*)AIL_ptr_add(i_Bank, pAsset->DataOffset);
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_resolve_raw_environment_preset(void* i_Environment, MilesResolveFunc eval)
{
    if (i_Environment == 0 || eval == 0)
        return 0;

    void* context = 0;
    Environment* preset= (Environment*)i_Environment;
    if (preset->Tag != 'ENV ')
        return 0; // not an env

    S32* pStart = preset->Values;
    void* pEnd = AIL_ptr_add(preset, preset->EnvSize);

    while (pStart != pEnd)
    {
        Environment::Value* pValue = (Environment::Value*)pStart;
        S32 Consume = 2;
        U32 Bit = pValue->Bit;

        S32 fail = 0;

        switch (Bit)
        {
        case BIT_ENV_REVERB_ROOM_TYPE: break; // not an expression
        case BIT_ENV_REVERB_PARAMS:
            {
                RESOLVE(pValue->Data + 0, 1);
                RESOLVE(pValue->Data + 1, 1);
                RESOLVE(pValue->Data + 2, 1);
                Consume = 6;
                break;
            }
        case BIT_ENV_REVERB_MASTER_VOLUME:
            {
                RESOLVE(pValue->Data + 0, 1);
                RESOLVE(pValue->Data + 1, 1);
                Consume = 4;
                break;
            }
        case BIT_ENV_MASTER_VOLUME:
        case BIT_ENV_ROLLOFF:
        case BIT_ENV_DOPPLER:
        case BIT_ENV_FALLOFF_POWER:
            {
                RESOLVE(pValue->Data + 0, 1);
                break;
            }
        }

        // Advance.
        pStart = (S32*)AIL_ptr_add(pStart, sizeof(U32) * (1 + Consume));
    } // end while preset not complete

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_apply_raw_environment_preset(HDIGDRIVER i_Driver, void* i_Environment)
{
    if (i_Driver == 0 || i_Environment == 0)
        return 0;

    Environment* pEnv= (Environment*)i_Environment;
    if (pEnv->Tag != 'ENV ') 
        return 0; // not an env

    S32* pStart = pEnv->Values;
    void* pEnd = AIL_ptr_add(pEnv, pEnv->EnvSize);

    while (pStart != pEnd)
    {
        Environment::Value* pValue = (Environment::Value*)pStart;
        S32 Consume = 2;
        U32 Bit = pValue->Bit;

        switch (Bit)
        {
        case BIT_ENV_REVERB_ROOM_TYPE:
            {
                AIL_set_room_type(i_Driver, 0, pValue->Data[0].iData);
                break;
            }
        case BIT_ENV_REVERB_PARAMS:
            {
                AIL_set_digital_master_reverb(i_Driver, 0, pValue->Data[0].fData, pValue->Data[1].fData, pValue->Data[2].fData);
                Consume = 6;
                break;
            }
        case BIT_ENV_REVERB_MASTER_VOLUME:
            {
                AIL_set_digital_master_reverb_levels(i_Driver, 0, MSS_dB_to_scalar(pValue->Data[1].fData), MSS_dB_to_scalar(pValue->Data[0].fData));
                Consume = 4;
                break;
            }
        case BIT_ENV_MASTER_VOLUME:
            {
                AIL_set_digital_master_volume_level(i_Driver, MSS_dB_to_scalar(pValue->Data[0].fData));
                break;
            }
        case BIT_ENV_ROLLOFF:
            {
                AIL_set_3D_rolloff_factor(i_Driver, pValue->Data[0].fData);
                break;
            }
        case BIT_ENV_DOPPLER:
            {
                AIL_set_3D_doppler_factor(i_Driver, pValue->Data[0].fData);
                break;
            }
        case BIT_ENV_FALLOFF_POWER:
            {
                AIL_set_speaker_configuration(i_Driver, 0, 0, pValue->Data[0].fData);
                break;
            }
        }

        // Advance.
        pStart = (S32*)AIL_ptr_add(pStart, sizeof(U32) * (1 + Consume));
    } // end while preset not complete

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void* AILEXPORT AIL_find_environment_preset(SoundBank* i_Bank, char const *i_Name)
{
    if (i_Name == 0 || i_Bank == 0)
        return 0;

    if (i_Bank->Tag != 'BANK') 
        return 0;

    SoundBank::AssetEntry* pAsset = FindAsset(i_Bank, i_Name, i_Bank->Envs, i_Bank->EnvCount);
    if (pAsset == 0)
    {
        AIL_set_error("Environment Not Found.");
        return 0;
    }

    return AIL_ptr_add(i_Bank, pAsset->DataOffset);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void* AILEXPORT AIL_find_sound_preset(SoundBank* i_Bank, char const* i_Name)
{
    if (i_Bank == 0 || i_Name == 0)
        return 0;

    if (i_Bank->Tag != 'BANK')
        return 0;

    SoundBank::AssetEntry* pAsset = FindAsset(i_Bank, i_Name, i_Bank->Presets, i_Bank->PresetCount);
    if (pAsset == 0)
    {
        AIL_set_error("Preset Not Found.");
        return 0;
    }

    return AIL_ptr_add(i_Bank, pAsset->DataOffset);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_apply_environment_preset(HDIGDRIVER i_Driver, SoundBank* i_Bank, const char* i_Name)
{
    return AIL_apply_raw_environment_preset(i_Driver, AIL_find_environment_preset(i_Bank, i_Name));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_unapply_raw_environment_preset(HDIGDRIVER i_Driver, void* i_Environment)
{
    if (i_Driver == 0 || i_Environment == 0)
        return 0;


    Environment* pEnv= (Environment*)i_Environment;
    if (pEnv->Tag != 'ENV ')
        return 0; // not an env

    S32* pStart = pEnv->Values;
    void* pEnd = AIL_ptr_add(pEnv, pEnv->EnvSize);

    while (pStart != pEnd)
    {
        Environment::Value* pValue = (Environment::Value*)pStart;
        S32 Consume = 2;
        U32 Bit = pValue->Bit;

        switch (Bit)
        {
        case BIT_ENV_REVERB_ROOM_TYPE:
            {
                AIL_set_room_type(i_Driver, 0, 0);
                break;
            }
        case BIT_ENV_REVERB_PARAMS:
            {
                AIL_set_digital_master_reverb(i_Driver, 0, 1.493f, .011f, .5f);
                Consume = 6;
                break;
            }
        case BIT_ENV_REVERB_MASTER_VOLUME:
            {
                AIL_set_digital_master_reverb_levels(i_Driver, 0, 1, 0);
                Consume = 4;
                break;
            }
        case BIT_ENV_MASTER_VOLUME:
            {
                AIL_set_digital_master_volume_level(i_Driver, 1);
                break;
            }
        case BIT_ENV_ROLLOFF:
            {
                AIL_set_3D_rolloff_factor(i_Driver, 1);
                break;
            }
        case BIT_ENV_DOPPLER:
            {
                AIL_set_3D_doppler_factor(i_Driver, 1);
                break;
            }
        case BIT_ENV_FALLOFF_POWER:
            {
                AIL_set_speaker_configuration(i_Driver, 0, 0, 3);
                break;
            }
        }

        // Advance.
        pStart = (S32*)AIL_ptr_add(pStart, sizeof(U32) * (1 + Consume));
    } // end while preset not complete

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_unapply_environment_preset(HDIGDRIVER i_Driver, SoundBank* i_Bank, const char* i_Name)
{
    if (i_Name == 0 || i_Driver == 0 || i_Bank == 0)
        return 0;

    if (i_Bank->Tag != 'BANK')
        return 0;

    SoundBank::AssetEntry* pAsset = FindAsset(i_Bank, i_Name, i_Bank->Envs, i_Bank->EnvCount);
    if (pAsset == 0)
    {
        AIL_set_error("Environment Not Found.");
        return 0;
    }

    return AIL_unapply_raw_environment_preset(i_Driver, AIL_ptr_add(i_Bank, pAsset->DataOffset));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF char const* AILEXPORT AIL_get_soundbank_name(SoundBank* i_Bank)
{
    return i_Bank->SoundBankName;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF const char* AILEXPORT AIL_get_soundbank_filename(SoundBank* i_Bank)
{
    return i_Bank->SoundBankFileName;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_get_soundbank_mem_usage(SoundBank* i_Bank)
{
    return i_Bank->MetaSize;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_enumerate_sound_assets(SoundBank* i_Bank, HMSSENUM* next, char const **name)
{
    if ((i_Bank == NULL) || (next == NULL) || (name == NULL))
        return 0;

    if (i_Bank->Tag != 'BANK') 
        return 0;

    if (*next == MSS_FIRST)
    {
        *next = 0;
    }

    if (*next >= i_Bank->SoundCount) 
        return 0;

    *name = (char*)AIL_ptr_add(i_Bank, i_Bank->Sounds[*next].NameOffset);
    *next = *next + 1;
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_resolve_raw_sound_preset(void* preset, void* context, MilesResolveFunc eval)
{
    if (preset == 0) 
        return 0;

    Preset* pPreset = (Preset*)preset;
    if (pPreset->Tag != 'PST ')
        return 0; // not a preset

    S32* pStart = pPreset->Values;
    void* pEnd = AIL_ptr_add(pPreset, pPreset->PresetSize);

    while (pStart != pEnd)
    {
        Preset::Value* pValue = (Preset::Value*)pStart;
        S32 Consume = 2;
        U32 Bit = pValue->Bit;

        S32 fail = 0;

        switch (Bit)
        {
        case BIT_SND_3D_POS:
        case BIT_SND_3D_CONE:
            {
                RESOLVE(pValue->Data + 0, 1);
                RESOLVE(pValue->Data + 1, 1);
                RESOLVE(pValue->Data + 2, 1);
                Consume = 6;
                break;
            }
        case BIT_SND_3D_DISTANCES:
            {
                RESOLVE(pValue->Data + 0, 1);
                RESOLVE(pValue->Data + 1, 1);
                RESOLVE(pValue->Data + 2, 0);
                Consume = 6;
                break;
            }
        case BIT_SND_3D_ROLLOFF:
        case BIT_SND_OCCLUSION:
        case BIT_SND_EXCLUSION:
        case BIT_SND_OBSTRUCTION:
        case BIT_SND_LPF_CUTOFF:
        case BIT_SND_PLAYBACK_RATE:
            {
                RESOLVE(pValue->Data, 1);
                break;
            }
        case BIT_SND_VOLUME_PAN:
            {
                RESOLVE(pValue->Data + 0, 1);
                RESOLVE(pValue->Data + 1, 1);
                RESOLVE(pValue->Data + 2, 1);
                RESOLVE(pValue->Data + 3, 1);
                RESOLVE(pValue->Data + 4, 1);
                Consume = 10;
                break;
            }
        case BIT_SND_REVERB_LEVELS:
            {
                RESOLVE(pValue->Data + 0, 1);
                RESOLVE(pValue->Data + 1, 1);
                Consume = 4;
                break;
            }
        case BIT_SND_3D_VEL:
            {
                RESOLVE(pValue->Data + 0, 1);
                RESOLVE(pValue->Data + 1, 1);
                RESOLVE(pValue->Data + 2, 1);
                RESOLVE(pValue->Data + 3, 1);
                Consume = 8;
                break;
            }
        case BIT_SND_SPKR_LEVELS: break;
        case BIT_SND_CHAN_MATRIX: Consume = 2; break;
        case BIT_SND_SPAT_TYPE: break;

        case BIT_SND_3D_ORIENT:
            {
                RESOLVE(pValue->Data + 0, 1);
                RESOLVE(pValue->Data + 1, 1);
                RESOLVE(pValue->Data + 2, 1);
                RESOLVE(pValue->Data + 3, 1);
                RESOLVE(pValue->Data + 4, 1);
                RESOLVE(pValue->Data + 5, 1);

                Consume = 12;
                break;
            }
        case BIT_SND_3D_FALLOFF_GRAPH:
            {
                Preset::Graph* G = (Preset::Graph*)pValue;
                Consume = 5 + ((G->G.ExcCount + G->G.LPCount + G->G.VolCount)*8);
                break;
            }
        case BIT_SND_FILTER_PROPERTY:
            {
                Preset::Filter* pFilter = (Preset::Filter*)pStart;
                
                S32 PropCount = pFilter->PropCount;
                for (S32 i = 0; i < PropCount; i++)
                {
                    RESOLVE_FILTER(pFilter->Props[i]);
                }
                
                Consume = 3 + 3*pFilter->PropCount;
                break;
            }
        } // end switch bit

        // Advance.
        pStart = (S32*)AIL_ptr_add(pStart, sizeof(U32) * (1 + Consume));
    } // end while preset not complete

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_apply_raw_sound_preset(HSAMPLE i_Sample, void* preset)
{
    if (i_Sample == 0 || preset == 0) 
        return 0;

    Preset* pPreset = (Preset*)preset;
    if (pPreset->Tag != 'PST ') 
        return 0; // not a preset
    
    MSS_SPEAKER SpeakerIndeces[MAX_SPEAKERS];
    F32 SpeakerLevels[MAX_SPEAKERS];
    if (pPreset->Mask & (1 << BIT_SND_SPKR_LEVELS))
    {
        for (S32 i = 0; i < MAX_SPEAKERS; i++) { SpeakerIndeces[i] = (MSS_SPEAKER)i; }
        AIL_sample_speaker_scale_factors(i_Sample, SpeakerIndeces, SpeakerLevels, MAX_SPEAKERS);
    }

    // Initialize channel mapping.
    MSS_SPEAKER ChannelSourceIndeces[MAX_SPEAKERS * MAX_SPEAKERS]; 
    MSS_SPEAKER ChannelDestIndeces[MAX_SPEAKERS * MAX_SPEAKERS]; 
    F32 ChannelLevels[MAX_SPEAKERS * MAX_SPEAKERS];

    if (pPreset->Mask & (1 << BIT_SND_CHAN_MATRIX))
    {
        S32 LevelDepth = 0;
        for (S32 d=0; d < MAX_SPEAKERS; d++)
        {
            for (S32 s=0; s < MAX_SPEAKERS; s++)
            {
                // LevelDepth = MAX_SPEAKERS * d + s
                ChannelSourceIndeces[LevelDepth] = (MSS_SPEAKER) s;
                ChannelDestIndeces[LevelDepth] = (MSS_SPEAKER) d;
                LevelDepth++;
            }
        }

        AIL_sample_channel_levels(i_Sample, ChannelSourceIndeces, ChannelDestIndeces, ChannelLevels, LevelDepth);
    }

    S32* pStart = pPreset->Values;
    void* pEnd = AIL_ptr_add(pPreset, pPreset->PresetSize);

    while (pStart != pEnd)
    {
        Preset::Value* pValue = (Preset::Value*)pStart;
        S32 Consume = 2;
        U32 Bit = pValue->Bit;

        switch (Bit)
        {
        case BIT_SND_3D_FALLOFF_GRAPH:
            {
                Preset::Graph* G = (Preset::Graph*)pValue;
                FalloffGraph_Set(i_Sample, G->G.Affects, &G->G);

                Consume = 5 + ((G->G.ExcCount + G->G.LPCount + G->G.VolCount)*8);
                break;
            }
        case BIT_SND_3D_ROLLOFF:
            {
                i_Sample->S3D.rolloff = pValue->Data[0].fData;
                break;
            }
        case BIT_SND_3D_POS:
            {
                AIL_set_sample_3D_position(i_Sample, pValue->Data[0].fData, pValue->Data[1].fData, pValue->Data[2].fData);
                Consume = 6;
                break;
            }
        case BIT_SND_3D_CONE:
            {
                AIL_set_sample_3D_cone(i_Sample, pValue->Data[0].fData, pValue->Data[1].fData, MSS_dB_to_scalar(pValue->Data[2].fData));
                Consume = 6;
                break;
            }
        case BIT_SND_3D_DISTANCES:
            {
                AIL_set_sample_3D_distances(i_Sample, pValue->Data[1].fData, pValue->Data[0].fData, pValue->Data[2].iData);
                Consume = 6;
                break;
            }
        case BIT_SND_OCCLUSION:
            {
                AIL_set_sample_occlusion(i_Sample, pValue->Data[0].fData);
                break;
            }
        case BIT_SND_EXCLUSION:
            {
                AIL_set_sample_exclusion(i_Sample, pValue->Data[0].fData);
                break;
            }
        case BIT_SND_OBSTRUCTION:
            {
                AIL_set_sample_obstruction(i_Sample, pValue->Data[0].fData);
                break;
            }
        case BIT_SND_LPF_CUTOFF:
            {
                float LPF = pValue->Data[0].fData / (i_Sample->original_playback_rate / 2);
                if (LPF > 1)
                    LPF = 1;

                AIL_set_sample_low_pass_cut_off(i_Sample, -1, LPF);
                break;
            }
        case BIT_SND_PLAYBACK_RATE:
            {
                AIL_set_sample_playback_rate_factor(i_Sample, MSS_sT_to_scalar(pValue->Data[0].fData));
                break;
            }
        case BIT_SND_VOLUME_PAN:
            {
                // Convert dB to linloud.
                float vol = MSS_dB_to_scalar(pValue->Data[0].fData);
                vol = AIL_pow(vol, 6.0f/10.0f);

                float center = MSS_dB_to_scalar(pValue->Data[3].fData);
                center = AIL_pow(center, 6.0f/10.0f);

                float lfe = MSS_dB_to_scalar(pValue->Data[4].fData);
                lfe = AIL_pow(lfe, 6.0f/10.0f);

                AIL_set_sample_51_volume_pan(i_Sample, 
                    vol, pValue->Data[1].fData, pValue->Data[2].fData,
                    center, lfe);

                Consume = 10;
                break;
            }
        case BIT_SND_REVERB_LEVELS:
            {
                AIL_set_sample_reverb_levels(i_Sample, MSS_dB_to_scalar(pValue->Data[1].fData), MSS_dB_to_scalar(pValue->Data[0].fData));
                Consume = 4;
                break;
            }
        case BIT_SND_3D_VEL:
            {
                AIL_set_sample_3D_velocity(i_Sample, pValue->Data[0].fData, pValue->Data[1].fData, pValue->Data[2].fData, pValue->Data[3].fData);
                Consume = 8;
                break;
            }
        case BIT_SND_SPKR_LEVELS:
            {
                //U32 iData = pValue->Data[0].iData;
                //SpeakerLevels[iData >> 16] = iData & 0xffff;
                break;
            }
        case BIT_SND_CHAN_MATRIX:
            {
                U32 iData = pValue->Data[0].iData;

                S32 SourceIndex = iData >> 16;
                S32 DestIndex = iData & 0xffff;
                F32 Value = pValue->Data[1].fData;

                if (DestIndex < MAX_SPEAKERS && SourceIndex < MAX_SPEAKERS)
                {
                    ChannelLevels[MAX_SPEAKERS * DestIndex + SourceIndex] = Value;
                }

                Consume = 4;
                break;
            }
        case BIT_SND_3D_ORIENT:
            {
                AIL_set_sample_3D_orientation(i_Sample, 
                    pValue->Data[0].fData, pValue->Data[1].fData, pValue->Data[2].fData,
                    pValue->Data[3].fData, pValue->Data[4].fData, pValue->Data[5].fData);
                Consume = 12;
                break;
            }
        case BIT_SND_SPAT_TYPE:
            {
                switch (pValue->Data[0].iData)
                {
                case MSS_2D_DEFAULT: AIL_set_sample_channel_levels(i_Sample, 0, 0, 0, 0); break; // force default.
                case MSS_3D: AIL_set_sample_is_3D(i_Sample, 1); break; // enable 3d spatialization
                case MSS_2D_USER_CHANMAT: break; // chanmat will get set via its bits.
                }

                break;
            }
        case BIT_SND_FILTER_PROPERTY:
            {
                Preset::Filter* pFilter = (Preset::Filter*)pStart;
                
                S32 Index = pFilter->Index;

                if (pFilter->FilterNameOffset == 0)
                {
                    // Remove the existing filter.
                    AIL_set_sample_processor(i_Sample, (SAMPLESTAGE)(Index + SP_FILTER_0), 0);
                }
                else
                {
                    HPROVIDER hFilter = 0;
                    char* FilterName = (char*)AIL_ptr_add(preset, pFilter->FilterNameOffset);
                    if (AIL_find_filter(FilterName, &hFilter) == 0)
                    {
                        AIL_set_error("Sound Preset Error - Filter Not Found.");
                        return 0;
                    }

                    // Set this as the filter for this stage.
                    HPROVIDER hOldProvider = AIL_sample_processor(i_Sample, (SAMPLESTAGE)(Index + SP_FILTER_0));
                    if (hOldProvider != hFilter)
                    {
                        // Only replace if different.

                        S32 IsCompressor = strcmp(FilterName, "Compressor Filter") == 0;
                        // If we are a compressor and are installing on to a bus.
                        if (i_Sample->bus == MAX_BUSSES && 
                            IsCompressor)
                        {
                            S32 BusIndex = 0;
                            for (S32 i = 0; i < MAX_BUSSES; i++)
                            {
                                if (i_Sample->driver->bus_samples[i] == i_Sample)
                                {
                                    BusIndex = i;
                                    break;
                                }
                            }

                            // If we are a compressor, then we need to install.
                            // The first filter property is the input.
                            S32 InputBusIndex = 0;
                            for (S32 i = 0; i < pFilter->PropCount; i++)
                            {
                                char* PropName = (char*)AIL_ptr_add(preset, pFilter->Props[i].NameOffset);
                                if (strcmp(PropName, "Input") == 0)
                                {
                                    InputBusIndex = (S32)pFilter->Props[i].Value;
                                }
                            }

                            if (InputBusIndex < 0 || InputBusIndex >= MAX_BUSSES)
                                InputBusIndex = 0;

                            AIL_install_bus_compressor(i_Sample->driver, BusIndex, (SAMPLESTAGE)(Index + SP_FILTER_0), InputBusIndex);
                        }
                        else
                        {
                            AIL_set_sample_processor(i_Sample, (SAMPLESTAGE)(Index + SP_FILTER_0), hFilter);
                        }
                    } // if different.

                    for (S32 i = 0; i < pFilter->PropCount; i++)
                    {
                        float f = pFilter->Props[i].Value;
                        AIL_sample_stage_property(i_Sample, (SAMPLESTAGE)(Index + SP_FILTER_0), (char*)AIL_ptr_add(preset, pFilter->Props[i].NameOffset), -1, 0, &f, 0);
                    }
                }

                Consume = 3 + 3*pFilter->PropCount;
                break;
            }
        } // end switch bit

        // Advance.
        pStart = (S32*)AIL_ptr_add(pStart, sizeof(U32) * (1 + Consume));
    } // end while preset not complete

    if (pPreset->Mask & (1 << BIT_SND_SPKR_LEVELS))
    {
        AIL_set_sample_speaker_scale_factors(i_Sample, SpeakerIndeces, SpeakerLevels, MAX_SPEAKERS);
    }
    if (pPreset->Mask & (1 << BIT_SND_CHAN_MATRIX))
    {
        AIL_set_sample_channel_levels(i_Sample, ChannelSourceIndeces, ChannelDestIndeces, ChannelLevels, MAX_SPEAKERS * MAX_SPEAKERS);
    }

    return 1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_apply_sound_preset(HSAMPLE i_Sample, HMSOUNDBANK i_Bank, const char* i_Name)
{
    return AIL_apply_raw_sound_preset(i_Sample, AIL_find_sound_preset(i_Bank, i_Name));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_unapply_raw_sound_preset(HSAMPLE i_Sample, void* preset)
{
    if (i_Sample == 0 || preset == 0) 
        return 0;

    Preset* pPreset = (Preset*)preset;
    if (pPreset->Tag != 'PST ') 
        return 0; // not a preset


    S32* pStart = pPreset->Values;
    void* pEnd = AIL_ptr_add(pPreset, pPreset->PresetSize);

    while (pStart != pEnd)
    {
        Preset::Value* pValue = (Preset::Value*)pStart;
        S32 Consume = 2;
        U32 Bit = pValue->Bit;

        switch (Bit)
        {
        case BIT_SND_3D_FALLOFF_GRAPH:
            {
                Preset::Graph* G = (Preset::Graph*)pValue;
                Consume = 5 + ((G->G.ExcCount + G->G.LPCount + G->G.VolCount)*8);

                FalloffGraph_Set(i_Sample, G->G.Affects, 0);
                break;
            }
        case BIT_SND_3D_POS:
            {
                AIL_set_sample_3D_position(i_Sample, 0, 0, 0);
                Consume = 6;
                break;
            }
        case BIT_SND_3D_CONE:
            {
                AIL_set_sample_3D_cone(i_Sample, 360.0f, 360.0f, 1.0f);
                Consume = 6;
                break;
            }
        case BIT_SND_3D_DISTANCES:
            {
                AIL_set_sample_3D_distances(i_Sample, 200, 0, 1);
                Consume = 6;
                break;
            }
        case BIT_SND_OCCLUSION:
            {
                AIL_set_sample_occlusion(i_Sample, 0);
                break;
            }
        case BIT_SND_EXCLUSION:
            {
                AIL_set_sample_exclusion(i_Sample, 0);
                break;
            }
        case BIT_SND_OBSTRUCTION:
            {
                AIL_set_sample_obstruction(i_Sample, 0);
                break;
            }
        case BIT_SND_LPF_CUTOFF:
            {
                AIL_set_sample_low_pass_cut_off(i_Sample, -1, 1);
                break;
            }
        case BIT_SND_PLAYBACK_RATE:
            {
                AIL_set_sample_playback_rate_factor(i_Sample, 1);
                break;
            }
        case BIT_SND_VOLUME_PAN:
            {
                AIL_set_sample_51_volume_pan(i_Sample, 1, .5f, .5f, 1, 1);
                Consume = 10;
                break;
            }
        case BIT_SND_REVERB_LEVELS:
            {
                AIL_set_sample_reverb_levels(i_Sample, 1, 0);
                Consume = 4;
                break;
            }
        case BIT_SND_3D_VEL:
            {
                AIL_set_sample_3D_velocity(i_Sample, 1, 0, 0, 0);
                Consume = 8;
                break;
            }
        case BIT_SND_SPKR_LEVELS:
                break;
        case BIT_SND_CHAN_MATRIX:
            {
                Consume = 4;
                break;
            }
        case BIT_SND_3D_ROLLOFF:
            {
                i_Sample->S3D.rolloff = 0;
                break;
            }
        case BIT_SND_3D_ORIENT:
            {
                AIL_set_sample_3D_orientation(i_Sample, 1, 0, 0, 0, 1, 0);
                Consume = 12;
                break;
            }
        case BIT_SND_SPAT_TYPE:
            {
                AIL_set_sample_is_3D(i_Sample, 0);
                AIL_set_sample_channel_levels(i_Sample, 0, 0, 0, 0);
                break;
            }
        case BIT_SND_FILTER_PROPERTY:
            {
                Preset::Filter* pFilter = (Preset::Filter*)pStart;
                
                // Remove the index of a filter we added.
                S32 Index = pFilter->Index;
                if (pFilter->FilterNameOffset)
                {
                    AIL_set_sample_processor(i_Sample, (SAMPLESTAGE)(Index + SP_FILTER_0), 0);
                }

                Consume = 3 + 3*pFilter->PropCount;
                break;
            }
        } // end switch bit

        // Advance.
        pStart = (S32*)AIL_ptr_add(pStart, sizeof(U32) * (1 + Consume));
    } // end while preset not complete

    if (pPreset->Mask & (1 << BIT_SND_SPKR_LEVELS))
    {
        F32 Levels[MAX_SPEAKERS];
        MSS_SPEAKER Indeces[MAX_SPEAKERS];
        for (S32 i = 0; i < MAX_SPEAKERS; i++)
        {
            Indeces[i] = (MSS_SPEAKER)i;
            Levels[i] = 1;
        }

        AIL_set_sample_speaker_scale_factors(i_Sample, Indeces, Levels, MAX_SPEAKERS);
    }
    if (pPreset->Mask & (1 << BIT_SND_CHAN_MATRIX))
    {
        MSS_SPEAKER src_indexes [MAX_SPEAKERS * MAX_SPEAKERS]; 
        MSS_SPEAKER dest_indexes[MAX_SPEAKERS * MAX_SPEAKERS]; 

        S32 n_levels = 0;

        F32 levels[MAX_SPEAKERS * MAX_SPEAKERS] = {0};
        AIL_memset(levels, 0, sizeof(levels));

        for (S32 d=0; d < MAX_SPEAKERS; d++)
        {
            for (S32 s=0; s < MAX_SPEAKERS; s++)
            {
                src_indexes [n_levels] = (MSS_SPEAKER) s;
                dest_indexes[n_levels] = (MSS_SPEAKER) d;
                n_levels++;
            }
            levels[d * MAX_SPEAKERS + d] = 1.0f;
        }

        AIL_set_sample_channel_levels(i_Sample, src_indexes, dest_indexes, levels, MAX_SPEAKERS * MAX_SPEAKERS);
    }

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_unapply_sound_preset(HSAMPLE i_Sample, SoundBank* i_Bank, char const* i_Name)
{
    return AIL_unapply_raw_sound_preset(i_Sample, AIL_find_sound_preset(i_Bank, i_Name));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//DXDEF S32 AILEXPORT AIL_sound_asset_filename(SoundBank* i_Bank, char const* i_Name, char* o_FileName)
//{
//    SoundBank::AssetEntry* pAsset = FindAsset(i_Bank, i_Name, i_Bank->Sounds, i_Bank->SoundCount);
//    if (pAsset == 0) 
//    {
//        if (o_FileName) *o_FileName = 0;
//        return -1;
//    }
//
//    Sound* pSound = (Sound*)AIL_ptr_add(i_Bank, pAsset->DataOffset);
//
//    o_FileName[0] = '*';
//    memcpy(o_FileName + 1, i_Bank->SoundBankFileName, i_Bank->SoundBankFileNameLen);
//    strcpy(o_FileName + i_Bank->SoundBankFileNameLen + 1, (char*)AIL_ptr_add(pSound, pSound->FileNameOffset));
//
//    return pSound->Info.DataLen;
//}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_sound_asset_info(SoundBank* i_Bank, char const* i_Name, char* o_FileName, MILESBANKSOUNDINFO* out_info)
{
    SoundBank::AssetEntry* pAsset = FindAsset(i_Bank, i_Name, i_Bank->Sounds, i_Bank->SoundCount);
    if (pAsset == 0) 
    {
        if (o_FileName) *o_FileName = 0;
        return 0;
    }

    Sound* pSound = (Sound*)AIL_ptr_add(i_Bank, pAsset->DataOffset);
    if (out_info)
        *out_info = pSound->Info;

    S32 Req = 2; // null + '*'
    Req += i_Bank->SoundBankFileNameLen;
    Req += (S32)AIL_strlen((char*)AIL_ptr_add(pSound, pSound->FileNameOffset));

    if (o_FileName == 0)
    {
        return Req;
    }

    o_FileName[0] = '*';
    memcpy(o_FileName + 1, i_Bank->SoundBankFileName, i_Bank->SoundBankFileNameLen);
    strcpy(o_FileName + i_Bank->SoundBankFileNameLen + 1, (char*)AIL_ptr_add(pSound, pSound->FileNameOffset));

    return Req;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_find_marker_in_list(SINTa i_MarkerList, const char* i_MarkerName, S32* o_IsSamples)
{
    if (i_MarkerList == 0 || i_MarkerName == 0) 
        return -1;

    S32 MarkerCount = *(S32*)i_MarkerList;
    Sound::Marker* pMarker = (Sound::Marker*)AIL_ptr_add(i_MarkerList, 4);

    while (MarkerCount)
    {
        if (AIL_stricmp((char*)AIL_ptr_add(i_MarkerList, pMarker->NameOffset), i_MarkerName) == 0) 
        {
            *o_IsSamples = pMarker->IsSample;
            return pMarker->Offset;
        }

        pMarker++;
        MarkerCount--;
    }

    return -1;
}   

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF SINTa AILEXPORT
AIL_get_marker_list(SoundBank* i_Bank, const char* i_SoundName)
{
    if (i_Bank == 0 || i_SoundName == 0) 
        return 0;

    SoundBank::AssetEntry* pAsset = FindAsset(i_Bank, i_SoundName, i_Bank->Sounds, i_Bank->SoundCount);
    if (pAsset == 0)
        return 0;

    Sound* pSound = (Sound*)AIL_ptr_add(i_Bank, pAsset->DataOffset);
    return (SINTa)&pSound->MarkerCount;
}

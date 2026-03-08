#include "mss.h"
#include "imssapi.h"

#include "mileseventhandle.h"
#include "mileseventarray.h"

static PODHandleArray<HMSOUNDBANK> s_LoadedBanks;

// This is used by auditionbindings.cpp:HandleOpenBank
struct AuditionBank
{
    S32 Tag;
    char Name[256];
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" HMSOUNDBANK AILCALL
MilesAddSoundBankInternal(const char* i_FileName, char const* i_Name)
{
    // Only route banks through the network if we are doing a full audition
    if (Audition_Status() == 2)
    {
        if (i_Name == 0)
        {
            AIL_set_error("Can't audition without a sound bank name.");
            return 0;
        }
        HMSOUNDBANK Bank = (HMSOUNDBANK)Audition_OpenBank(i_Name);
        if (Bank == 0)
        {
            return 0;
        }

        s_LoadedBanks.Reacquire();
        s_LoadedBanks.Push((HMSOUNDBANK)Bank);

        AIL_unlock();
        while (Audition_OpenComplete(Bank) == 0)
        {
            AIL_delay(0);
        }
        AIL_lock();
        return Bank;
    }

    HMSOUNDBANK Bank = AIL_open_soundbank(i_FileName, i_Name);
    if (Bank == 0)
    {
        return 0;
    }

    s_LoadedBanks.Reacquire();
    s_LoadedBanks.Push(Bank);
    return Bank;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" S32 AILCALL
MilesReleaseSoundBankInternal(HMSOUNDBANK i_Bank)
{
    s_LoadedBanks.Reacquire();
    S32 BankCount = s_LoadedBanks.Count();
    HMSOUNDBANK* pCheck = s_LoadedBanks.Data();
    while (BankCount)
    {
        if (*pCheck == i_Bank)
        {
            if (*(S32*)i_Bank == 'AUDB')
            {
                Audition_CloseBank(i_Bank);
            }
            else
            {
                AIL_close_soundbank(i_Bank);
            }
            S32 RemoveIndex = (S32)(pCheck - s_LoadedBanks.Data());
            s_LoadedBanks.RemoveUnsorted(RemoveIndex);
            return 1;
        }
        BankCount--;
        pCheck++;
    }
    
    return 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static HMSOUNDBANK GetContainer(char const* i_AssetAndContainerName, char const** o_AssetName)
{
    char const* pSlash = strchr(i_AssetAndContainerName, '/');
    if (pSlash == 0) 
        return 0;

    // Strip off the container name.
    char BankName[256];
    strncpy(BankName, i_AssetAndContainerName, pSlash - i_AssetAndContainerName);
    BankName[pSlash - i_AssetAndContainerName] = 0;

    // Find the container.
    HMSOUNDBANK Container = 0;

    s_LoadedBanks.Reacquire();

    S32 BankCount = s_LoadedBanks.Count();
    HMSOUNDBANK* pCheck = s_LoadedBanks.Data();
    for (; BankCount; BankCount--, pCheck++)
    {
        if (*(S32*)(*pCheck) == 'AUDB')
        {
            continue;
        }

        char const* Name = AIL_get_soundbank_name(*pCheck);
        if (AIL_stricmp(Name, BankName) == 0)
        {
            Container = *pCheck;
            break;
        }
    }
    
    *o_AssetName = pSlash + 1;
    return Container;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 AILCALLBACK Default_GetLoadedCount(void)
{
    return s_LoadedBanks.Count();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static char const* AILCALLBACK Default_GetLoadedName(S32 i_Index)
{
    s_LoadedBanks.Reacquire();
    HMSOUNDBANK Bank = s_LoadedBanks[i_Index];
    char const* FileName = 0;
    if (*(S32*)Bank == 'AUDB')
    {
        AuditionBank* pBank = (AuditionBank*)Bank;
        FileName = pBank->Name;
    }
    else
    {
        FileName = AIL_get_soundbank_name(s_LoadedBanks[i_Index]);
    }
    return FileName;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 AILCALLBACK Default_GetMemUsage(void)
{
    S32 Mem = 0;

    s_LoadedBanks.Reacquire();
    S32 BankCount = s_LoadedBanks.Count();
    HMSOUNDBANK* pCheck = s_LoadedBanks.Data();
    while (BankCount)
    {
        if (*(S32*)(*pCheck) == 'BANK')
        {
            Mem += AIL_get_soundbank_mem_usage(*pCheck);
        }
        BankCount--;
        pCheck++;
    }
    
    return Mem;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void AILCALLBACK Default_FreeAll(void)
{
    s_LoadedBanks.Reacquire();
    S32 BankCount = s_LoadedBanks.Count();
    HMSOUNDBANK* pCheck = s_LoadedBanks.Data();
    while (BankCount)
    {
        if (*(S32*)(*pCheck) == 'BANK')
        {
            AIL_close_soundbank(*pCheck);
        }
        else
        {
            Audition_CloseBank(*pCheck);
        }
        BankCount--;
        pCheck++;
    }
    
    s_LoadedBanks.FreeMem();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void* AILCALLBACK Default_GetPreset(char const* i_PresetName)
{
    char const* AssetName = 0;
    HMSOUNDBANK Container = GetContainer(i_PresetName, &AssetName);

    // Didn't find...
    if (Container == 0) 
        return 0;
    return AIL_find_sound_preset(Container, i_PresetName);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void* AILCALLBACK Default_GetEnvironment(char const* i_EnvironmentName)
{
    char const* AssetName = 0;
    HMSOUNDBANK Container = GetContainer(i_EnvironmentName, &AssetName);

    // Didn't find...
    if (Container == 0) 
        return 0;
    return AIL_find_environment_preset(Container, i_EnvironmentName);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 AILCALLBACK Default_GetSound(char const* i_SoundName, char* o_SoundFileName, MILESBANKSOUNDINFO* o_Info)
{
    char const* AssetName = 0;
    HMSOUNDBANK Container = GetContainer(i_SoundName, &AssetName);

    // Didn't find...
    if (Container == 0) 
        return 0;

    S32 Found = AIL_sound_asset_info(Container, i_SoundName, o_SoundFileName, o_Info);
    if (Found == 0)
        return 0;
    
    return Found;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void* AILCALLBACK Default_GetEvent(char const* i_EventName)
{
    char const* AssetName = 0;
    HMSOUNDBANK Container = GetContainer(i_EventName, &AssetName);

    // Didn't find...
    if (Container == 0)
        return 0;

    U8 const* Event = 0;
    AIL_get_event_contents(Container, i_EventName, &Event);
    return (void*)Event;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void* AILCALLBACK Default_GetMarkerList(char const* i_SoundName)
{
    char const* AssetName = 0;
    HMSOUNDBANK Container = GetContainer(i_SoundName, &AssetName);

    // Didn't find...
    if (Container == 0) 
        return 0;

    return (void*)AIL_get_marker_list(Container, i_SoundName);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static _MILESBANKFUNCTIONS s_Functions = 
{
    Default_FreeAll,
    Default_GetPreset,
    Default_GetEnvironment,
    Default_GetSound,
    Default_GetEvent,
    Default_GetMarkerList,
    Default_GetLoadedCount,
    Default_GetMemUsage,
    Default_GetLoadedName
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void        Container_FreeAll() { s_Functions.FreeAll(); }
void*       Container_GetPreset(char const* i_PresetName) { return s_Functions.GetPreset(i_PresetName); }
void*       Container_GetEnvironment(char const* i_EnvironmentName) { return s_Functions.GetEnvironment(i_EnvironmentName); }
S32         Container_GetSound(char const* i_SoundName, char* o_SoundFileName, MILESBANKSOUNDINFO* o_Info) { return s_Functions.GetSound(i_SoundName, o_SoundFileName, o_Info); }
void*       Container_GetEvent(char const* i_EventName) { return s_Functions.GetEvent(i_EventName); }
void*       Container_GetMarkerList(char const* i_SoundName) { return s_Functions.GetMarkerList(i_SoundName); }
S32         Container_GetLoadedCount() { return s_Functions.GetLoadedCount(); }
S32         Container_GetMemUsage() { return s_Functions.GetMemUsage(); }
char const* Container_GetLoadedName(S32 i_Index) { return s_Functions.GetLoadedName(i_Index); }

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void AILCALL
MilesSetBankFunctions(MILESBANKFUNCTIONS const* i_Functions)
{
    s_Functions.FreeAll = Default_FreeAll;
    s_Functions.GetEnvironment = Default_GetEnvironment;
    s_Functions.GetEvent = Default_GetEvent;
    s_Functions.GetLoadedCount = Default_GetLoadedCount;
    s_Functions.GetLoadedName = Default_GetLoadedName;
    s_Functions.GetMarkerList = Default_GetMarkerList;
    s_Functions.GetMemUsage = Default_GetMemUsage;
    s_Functions.GetPreset = Default_GetPreset;
    s_Functions.GetSound = Default_GetSound;

    if (i_Functions == 0)
        return;

    if (i_Functions->FreeAll) s_Functions.FreeAll = i_Functions->FreeAll;
    if (i_Functions->GetEnvironment) s_Functions.GetEnvironment = i_Functions->GetEnvironment;
    if (i_Functions->GetEvent) s_Functions.GetEvent = i_Functions->GetEvent;
    if (i_Functions->GetLoadedCount) s_Functions.GetLoadedCount = i_Functions->GetLoadedCount;
    if (i_Functions->GetLoadedName) s_Functions.GetLoadedName = i_Functions->GetLoadedName;
    if (i_Functions->GetMarkerList) s_Functions.GetMarkerList = i_Functions->GetMarkerList;
    if (i_Functions->GetMemUsage) s_Functions.GetMemUsage = i_Functions->GetMemUsage;
    if (i_Functions->GetPreset) s_Functions.GetPreset = i_Functions->GetPreset;
    if (i_Functions->GetSound) s_Functions.GetSound = i_Functions->GetSound;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" MILESBANKFUNCTIONS const* AILCALL MilesGetBankFunctions() { return &s_Functions; }

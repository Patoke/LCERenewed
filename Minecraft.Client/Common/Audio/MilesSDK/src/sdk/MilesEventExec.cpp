#include "mss.h"
#include "imssapi.h"

#include "mileseventhandle.h"
#include "mileseventarray.h"
#include "rrThreads.h"
#include "rrAtomics.h"

#include "mileseventinternal.h"

#include <string.h>


#ifndef TEST

static EventSystem* s_SystemRoot = 0;


static EventSystem* s_VarContext = 0;
S32 EvalFunc(void* context, char const* exp, S32 explen, void* output, S32 isfloat)
{
    if (context)
    {
        SoundInstance* pSound = (SoundInstance*)context;
        return Var_LookupWithContext(pSound->m_System, pSound->m_VarRoot, exp, isfloat, output);
    }

    // Requires valid system context if context == 0!
    return Var_LookupWithContext(s_VarContext, s_VarContext->VarRoot, exp, isfloat, output);
}

inline F32 MSS_scalar_to_dB(F32 scalar)  // turns linear DAC scalar into dB (0.5 = -6 dB, 0.25 = -12 dB, ...)
{
   if (scalar < 0.00001F)
      {
      return -96.0F;
      }

   return 20.0F * log10(scalar);
}

#define SETTINGS_COMPILE_VERSION 3

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILCALL AIL_set_event_settings(void* i_Settings)
{
    U32 LimitCount, BlendCount, RampCount, LFOCount, VarCount, BusCount, GraphCount;

    U32* Data = (U32*)i_Settings;

    if (*Data != SETTINGS_COMPILE_VERSION)
        return;

    LimitCount = Data[1];
    BlendCount = Data[2];
    RampCount = Data[3];
    LFOCount = Data[4];
    VarCount = Data[5];
    BusCount = Data[6];
    GraphCount = Data[7];

    Data += 8;

    U32 TotalCount = LimitCount + BlendCount + RampCount + LFOCount;

    char Buffer[MAX_EVENT_SIZE];

    for (U32 i = 0; i < TotalCount; i++)
    {
        char* Source = (char*)AIL_ptr_add(i_Settings, *Data);
        Data++;

        EVENT_STEP_INFO* Step = 0;
        AIL_next_event_step((U8*)Source, &Step, Buffer, MAX_EVENT_SIZE);

        if (Step == 0)
            return; // Bad settings

        if (LimitCount)
        {
            if (Step->type != EVENT_STEPTYPE_SETLIMITS)
                return;

            Limit_Add(s_SystemRoot, Step, 0);
            LimitCount--;
            continue;
        }

        if (BlendCount)
        {
            if (Step->type != EVENT_STEPTYPE_SETBLEND)
                return;

            Blend_Add(s_SystemRoot, Step);
            BlendCount--;
            continue;
        }

        if (RampCount)
        {
            if (Step->type != EVENT_STEPTYPE_RAMP)
                return;

            Ramp_Add(&s_SystemRoot->RampRoot, s_SystemRoot, Step);
            RampCount--;
            continue;
        }

        if (LFOCount)
        {
            if (Step->type != EVENT_STEPTYPE_SETLFO)
                return;

            // Use it!
            LFO_Add(s_SystemRoot, Step);
            LFOCount--;
            continue;
        }
    }

    // Set up the busses.
    U32 FirstFlags = 0;
    Handle Last = Handle_Clear();
    for (U32 i = 0; i < BusCount; i++)
    {
        char* Labels = (char*)AIL_ptr_add(i_Settings, *Data);
        Data++;

        char* Preset = (char*)AIL_ptr_add(i_Settings, *Data);
        Data++;

        U32 Flags = *Data;
        Data++;

        S32 Size = sizeof(Bus) + (S32)AIL_strlen(Labels) + 1;
        Handle New = Handle_Allocate(Size);
        Bus* B = (Bus*)Handle_Get(New);
        if (B)
        {
            // Allocate the space for the preset name.
            Size = (S32)AIL_strlen(Preset) + 1;
            Handle PH = Handle_Allocate(Size);
            char* P = (char*)Handle_Get(PH);
            if (P)
            {
                if (i)
                {
                    B->Sample = AIL_allocate_bus(s_SystemRoot->Driver);
                    B->Sample->bus_enable_limiter = Flags & 2;
                }
                else
                    FirstFlags = Flags;

                AIL_strcpy(B->Labels, Labels);
                AIL_strcpy(P, Preset);
                B->Preset = PH;
                B->Index = (U8)i;
                B->Self = New;
                B->Dynamic = (U8)(Flags & 1);

                Bus* L = (Bus*)Handle_Get(Last);
                if (L)
                    L->Next = New;

                if (i == 0)
                    s_SystemRoot->BusRoot = B->Self;
                else if (i == 1)
                {
                    L->Sample = AIL_bus_sample_handle(s_SystemRoot->Driver, 0);
                    L->Sample->bus_enable_limiter = FirstFlags & 2;
                }

#ifdef IS_WIIU
                // If we are on WiiU, and the first label is "drc" on the bus, then route to the drc.
                if (AIL_strnicmp(B->Labels, "drc", 3) == 0 && (B->Labels[3] == 0 || B->Labels[3] == ','))
                    B->Sample->route_to_drc = 1;
#endif
                Last = New;
            }
            else
                Handle_Free(New);
        }
    }


    // Variable graph transforms.
    for (U32 i = 0; i < GraphCount; i++)
    {
        char* Name = (char*)AIL_ptr_add(i_Settings, *Data);
        Data++;

        S32 GraphSize = *Data;
        Data++;


        S32 NameLen = (S32)AIL_strlen(Name);
        S32 AllocLen = sizeof(VarDef) + NameLen;
        Handle New = Handle_Allocate(AllocLen);
        Handle Graph = Handle_Allocate(GraphSize);

        VarDef* V = (VarDef*)Handle_Get(New);
        VarGraph* G = (VarGraph*)Handle_Get(Graph);
        if (V == 0 || G == 0)
        {
            Handle_Free(New);
            Handle_Free(Graph);
        }
        else
        {
            V->Next = s_SystemRoot->VarRoot;
            s_SystemRoot->VarRoot = New;
            V->IsFloat = 1;
            V->Self = New;
            V->Graph = Graph;
            AIL_strcpy(V->Name, Name);
            AIL_memcpy(G, Data, GraphSize);

            V->f = G->Min;
        }

        Data += GraphSize / 4;;
    }

    // Read in default variable values.
    for (U32 i = 0; i < VarCount; i++)
    {
        char* Name = (char*)AIL_ptr_add(i_Settings, *Data);
        Data++;

        Var_SetInstance(&s_SystemRoot->VarRoot, Name, Data, 1);
        Data++;
    }

}

// Async State
//-----------------------------------------------------------------------------
MilesAsyncRead   s_Asyncs[MAX_ASYNCS];
S32              s_AsyncUsed[MAX_ASYNCS]; // nonzero if the async struct is in use.
S32              s_AsyncCancelled[MAX_ASYNCS]; // nonzero if the async struct has been cancelled and is waiting for that.
EventSystem*     s_AsyncContext[MAX_ASYNCS];
S32              s_AsyncId = 1;

// Global State
//-----------------------------------------------------------------------------
static void*            s_MemoryBuffer = 0; // only valid if we allocated, ie buffer wasn't passed to us.

static U64              s_NextSoundId = 0x8000000000000001ULL;

static const U64        s_SilentSoundData = 0; // used for 'playing' silent sounds.


// Event Execution
//-----------------------------------------------------------------------------
static S32              ExecuteStartSound(const EVENT_STEP_INFO* i_Step, EventSystem* i_System, const U8* i_pCurrentEventLocation,
                            void* i_InitBlock, U32 i_InitBlockLen, U64 i_EventId, U64 i_ParentId);
static S32              ExecuteControlSounds(const EVENT_STEP_INFO* i_Step, EventSystem* i_System, U64 i_EventId, U64 i_Filter);
static S32              ExecutePersistPreset(const EVENT_STEP_INFO* i_Step, U64 i_Id, EventSystem* i_System);
static S32              ExecuteCacheSounds(const EVENT_STEP_INFO* i_Step, EventSystem* i_System, U64 i_Id, S32 i_IsLoading);
static S32              ExecuteApplyEnv(const EVENT_STEP_INFO* i_Step, U64 i_Id, EventSystem* i_System);
static S32              ExecuteApplyRamp(const EVENT_STEP_INFO* i_Step, EventSystem* i_System, U64 i_EventId, U64 i_Filter);
static S32              ExecuteSetBlend(const EVENT_STEP_INFO* i_Step, U64 i_EventId, EventSystem* i_System);
static S32              ExecuteClearState(EventSystem* i_System, U64 i_EventId);
static S32              ExecuteEnableLimit(EventSystem* i_System, char const* i_Name, U64 i_Id);
static S32              ExecuteMoveVar(EVENT_STEP_INFO const* i_Step, EventSystem* i_System);






// Random Number Generator
//-----------------------------------------------------------------------------
static U64 s_RandomSeed = 0;
#define s_RandomA 1664525
#define s_RandomC 1013904223

// External random number function, if provided.
static MSS_USER_RAND* s_ExternalRand = 0;
static U32 HorribleRand()
{
    if (s_ExternalRand) 
        return (U32)(s_ExternalRand());

    U64 Next = (U64)(U32)(s_RandomA * s_RandomSeed + s_RandomC);

    s_RandomSeed = Next;
    return (U32)((Next >> 12) & 0xFFFFFFFF);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void AILCALL MilesRegisterRandInternal(MSS_USER_RAND * rand)
{
    s_ExternalRand = rand;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 MilesGetVarInternal(void* i_Context, char const* i_Name, void* o_Value, S32 i_IsFloat)
{
    Handle* Root = 0;
    if (i_Context == 0)
    {
        Root = &s_SystemRoot->VarRoot;
    }
    else if (*(U32*)i_Context == 'ESYS')
    {
        Root = &((EventSystem*)i_Context)->VarRoot;
    }
    else
    {
        EventSystem* System = s_SystemRoot;
        S32 SystemIndex = ((U32)(UINTa)i_Context) >> 16;
        S32 SoundIndex = ((U32)(UINTa)i_Context) & 0xffff;
        while (SystemIndex) { System = System->Next; SystemIndex--; }

        if (SoundIndex > System->SoundInstances.Count()) 
            return 0;

        SoundInstance* pSound = (SoundInstance*)Handle_Get(System->SoundInstances[SoundIndex]);
        Root = &pSound->m_VarRoot;
    }

    VarDef* pRoot = (VarDef*)Handle_Get(*Root);
    for (; pRoot; pRoot = (VarDef*)Handle_Get(pRoot->Next))
    {
        if (AIL_stricmp(pRoot->Name, i_Name) == 0)
        {
            if (i_IsFloat != pRoot->IsFloat)
                return 0;

            if (i_IsFloat)
                *(F32*)o_Value = pRoot->f;
            else
                *(S32*)o_Value = pRoot->i;
            return 1;
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" S32 AILCALL MilesGetVarIInternal(UINTa i_Context, char const* i_Name, S32* o_Value)
{
    return MilesGetVarInternal((void*)i_Context, i_Name, o_Value, 0);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" S32 AILCALL MilesGetVarFInternal(UINTa i_Context, char const* i_Name, F32* o_Value)
{
    return MilesGetVarInternal((void*)i_Context, i_Name, o_Value, 1);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void AILCALL MilesSetVarFInternal(UINTa i_System, char const* i_Name, F32 i_Value)
{
    // Find the root.
    if (i_System == 0)
    {
        Var_SetInstance(&s_SystemRoot->VarRoot, i_Name, &i_Value, 1);
    }
    else if (*(U32*)i_System == 'ESYS')
    {
        Var_SetInstance(&((EventSystem*)i_System)->VarRoot, i_Name, &i_Value, 1);
    }
    else
    {
        EventSystem* System = s_SystemRoot;
        S32 SystemIndex = ((U32)(UINTa)i_System) >> 16;
        S32 SoundIndex = ((U32)(UINTa)i_System) & 0xffff;
        while (SystemIndex) { System = System->Next; SystemIndex--; }

        if (SoundIndex > System->SoundInstances.Count()) 
            return;

        SoundInstance* pSound = (SoundInstance*)Handle_Get(System->SoundInstances[SoundIndex]);
        Var_SetInstance(&pSound->m_VarRoot, i_Name, &i_Value, 1);
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void AILCALL MilesSetVarIInternal(UINTa i_System, char const* i_Name, S32 i_Value)
{
    // Find the root.
    if (i_System == 0)
    {
        Var_SetInstance(&s_SystemRoot->VarRoot, i_Name, &i_Value, 0);
    }
    else if (*(U32*)i_System == 'ESYS')
    {
        Var_SetInstance(&((EventSystem*)i_System)->VarRoot, i_Name, &i_Value, 0);
    }
    else
    {
        EventSystem* System = s_SystemRoot;
        S32 SystemIndex = ((U32)(UINTa)i_System) >> 16;
        S32 SoundIndex = ((U32)(UINTa)i_System) & 0xffff;
        while (SystemIndex) { System = System->Next; SystemIndex--; }

        if (SoundIndex > System->SoundInstances.Count()) 
            return;

        SoundInstance* pSound = (SoundInstance*)Handle_Get(System->SoundInstances[SoundIndex]);
        Var_SetInstance(&pSound->m_VarRoot, i_Name, &i_Value, 0);
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void AILCALL MilesSetSoundStartOffsetInternal(HMSSENUM i_Instance, S32 i_Offset, S32 i_IsMs)
{
    EventSystem* System = s_SystemRoot;
    S32 SystemIndex = ((U32)(UINTa)i_Instance) >> 16;
    S32 SoundIndex = ((U32)(UINTa)i_Instance) & 0xffff;
    while (SystemIndex) { System = System->Next; SystemIndex--; }

    if (SoundIndex > System->SoundInstances.Count()) 
        return;

    SoundInstance* pSound = (SoundInstance*)Handle_Get(System->SoundInstances[SoundIndex]);
    if (pSound->m_Status != MILESEVENT_SOUND_STATUS_PENDING)
        return;

    pSound->m_StartOffset = i_Offset;
    pSound->m_StartOffsetIsMS = i_IsMs;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" S32 AILCALL
MilesEnumerateSoundInstancesInternal(HEVENTSYSTEM i_System, HMSSENUM* io_Next, S32 i_Status, const char* i_Labels, U64 search_for_ID, MILESEVENTSOUNDINFO* o_Info)
{
    EventSystem* System = i_System ? (EventSystem*)i_System : s_SystemRoot;

    S32 SystemIndex = 0;
    for (EventSystem* SystemSeek = s_SystemRoot; System != SystemSeek; SystemSeek = SystemSeek->Next, SystemIndex++) {}

    U16 parsed_query[ 256 ];
    parsed_query[ 0 ] = 0xffff;
    if ( i_Labels == 0 )
      i_Labels = "";
    if ( i_Status == 0 )  // blank status means everything
      i_Status = 0xffffffff;
    
    S32 SearchToken = 0;
    if (io_Next)
    {
        SearchToken = (S32)(SINTa)*io_Next + 1;
        SearchToken &= 0xffff;
    }

    for ( ; ; SearchToken++)
    {
        if (SearchToken >= System->SoundInstances.Count())
            return 0;

        SoundInstance* pSound = (SoundInstance*)Handle_Get(System->SoundInstances[SearchToken]);

        // Skip if doesn't match status.
        if ((pSound->m_Status & i_Status) == 0) 
            continue;

        // Skip if doesn't match non-zero ID.
        if ( search_for_ID )
        {
           if ( 
               ( pSound->m_EventId != search_for_ID ) &&
               ( pSound->m_SoundId != search_for_ID ) &&
               ( pSound->m_ParentId != search_for_ID )
               ) 
               continue;
        }

        char* pSoundLabels = (char*)Handle_Get(pSound->m_Labels);

        if ( parsed_query[ 0 ] == 0xffff )
          parse_wildcard_query( parsed_query, i_Labels, 0xffffffff, ',', 0 );

        if (CheckLabelMatchQueryParsed( pSoundLabels, 0xffffffff, parsed_query, i_Labels ))
        {
            o_Info->QueuedID = pSound->m_ParentId;
            o_Info->EventID = pSound->m_EventId;
            o_Info->Sample = pSound->m_Sample;
            o_Info->Status = pSound->m_Status;
            o_Info->Stream = pSound->m_Stream;
            o_Info->InstanceID = pSound->m_SoundId;
            o_Info->Flags = pSound->m_Flags;
            o_Info->UsedDelay = pSound->m_Delay;
            o_Info->UsedPitch = pSound->m_Pitch;
            o_Info->UsedVolume = pSound->m_Volume;
            o_Info->HasCompletionEvent = Handle_Get(pSound->m_CompletionEvent) != 0;
            o_Info->UsedSound = (char const*)Handle_Get(pSound->m_Name);

            o_Info->UserBuffer = 0;
            o_Info->UserBufferLen = 0;

            if (pSound->m_UserBufferOffset != -1)
            {
                UserBufferEntry* E = (UserBufferEntry*)((char*)Handle_Get(pSound->m_InitializationBlock) + pSound->m_UserBufferOffset);
                o_Info->UserBuffer = E->buf;
                o_Info->UserBufferLen = E->len;
                if (E->is_ptr)
                    o_Info->UserBuffer = *(void**)E->buf;
            }

            *io_Next = ( SystemIndex << 16 ) | SearchToken;
            return 1;
        }
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" S32 AILCALL
MilesEnumeratePresetPersistsInternal(HEVENTSYSTEM i_System, HMSSENUM* io_Next, const char** o_Name)
{
    EventSystem* System = i_System ? (EventSystem*)i_System : s_SystemRoot;

    PersistedPreset* Cur = 0;
    if (*io_Next == MSS_FIRST || *io_Next == 0)
    {
        Cur = (PersistedPreset*)Handle_Get(System->PersistRoot);
    }
    else
    {
        Cur = (PersistedPreset*)*io_Next;
        Cur = (PersistedPreset*)Handle_Get(Cur->Next);
    }

    if (Cur == 0)
        return 0; // No more entries.

    *o_Name = Cur->Name;
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" S32 AILCALL
MilesSetSoundLabelLimitsInternal(HEVENTSYSTEM i_System, U64 i_Id, const char* i_SoundLimits, char const* i_Name)
{
    EventSystem* System = i_System ? (EventSystem*)i_System : s_SystemRoot;

    EVENT_STEP_INFO Blah;
    Blah.limits.limits.str = i_SoundLimits;
    Blah.limits.limits.len = (S32)AIL_strlen(i_SoundLimits);
    Blah.limits.name.str = i_Name;
    Blah.limits.name.len = (S32)AIL_strlen(i_Name);

    return Limit_Add(System, &Blah, i_Id);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 ExecuteEnableLimit(EventSystem* i_System, char const* i_Name, U64 i_Id)
{
    // Migrate the limit to the root.
    Handle Current = i_System->LimitRoot;
    Handle Prev = Handle_Clear();

    for (;;)
    {
        SoundLimit* Limit = (SoundLimit*)Handle_Get(Current);
        if (Limit == 0)
        {
            ErrorHandler(eBadSetLimit, i_Id, i_Name);
            return 0; // Name doesn't exist.
        }

        if (AIL_stricmp(Limit->Name, i_Name) == 0)
        {
            // See if we aren't already enabled.
            if (Handle_Equal(Current, i_System->LimitRoot))
            {
                // Already set.
                return 1;
            }

            // Move us to the front of the list.
            SoundLimit* PrevLimit = (SoundLimit*)Handle_Get(Prev);

            PrevLimit->Next = Limit->Next;
            i_System->LimitRoot = Limit->Self;
            Limit->Next = PrevLimit->Self;

            // Setup the counts
            SoundLimitEntry* Entry = (SoundLimitEntry*)Handle_Get(Limit->Entries);
            for (; Entry; Entry = (SoundLimitEntry*)Handle_Get(Entry->Next))
            {
                Entry->Count = 0;

                U16 parsed_query[ 256 ];
                parse_wildcard_query( parsed_query, Entry->Label, 0xffffffff, ',', ' ' );

                const S32 PlayingCount = i_System->SoundInstances.Count();
                for (S32 PlayingIndex = 0; PlayingIndex < PlayingCount; PlayingIndex++)
                {
                    SoundInstance* pSound = (SoundInstance*)Handle_Get(i_System->SoundInstances[PlayingIndex]);
                    if (pSound->m_Status == MILESEVENT_SOUND_STATUS_COMPLETE) 
                        continue;

                    char* pSoundLabels = (char*)Handle_Get(pSound->m_Labels);

                    if (CheckLabelMatchQueryParsed(pSoundLabels, 0xffffffff, parsed_query, Entry->Label ))
                    {
                        Entry->Count++;
                    }
                }
            }

            return 1;
        }

        Prev = Current;
        Current = Limit->Next;
    }
}

#define set_file_line( a ) a.caller = __FILE__; a.caller_line = __LINE__;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" S32 AILCALL
MilesBeginEventQueueProcessingInternal()
{
    if (s_SystemRoot == 0) // No valid event systems.
        return 0;

    Audition_FrameStart();

    // Check for completion of any cancels.
    for (S32 i = 0; i < MAX_ASYNCS; i++)
    {
        if (s_AsyncCancelled[i])
        {
            if (MilesAsyncFileStatus(s_Asyncs + i, 0))
            {
                Audition_AsyncCancel((S32)(SINTa)s_Asyncs[i].StreamUserData);

                // Done with the cancel.
                AIL_mem_free_lock(s_Asyncs[i].Buffer);
                s_AsyncCancelled[i] = 0;
                s_AsyncUsed[i] = 0;
            }
        }
    }

    // Update Async State For All Systems
    //-------------------------------------------------------------------------
    for (EventSystem* CurrentSystem = s_SystemRoot; CurrentSystem; CurrentSystem = CurrentSystem->Next)
    {
        // Check for outstanding asyncs - they are at the front of the source list
        S32 SourceCount = CurrentSystem->SoundSources.Count();
        for (S32 SourceIndex = 0; SourceIndex < SourceCount; SourceIndex++)
        {
            SoundSource* pSource = (SoundSource*)Handle_Get(CurrentSystem->SoundSources[SourceIndex]);
            if (pSource->m_AsyncIndex == 0)
            {
                break;
            }

            // Poll for completion.
            if (MilesAsyncFileStatus(s_Asyncs + pSource->m_AsyncIndex - 1, 0) == 0) 
            {
                continue;
            }

            S32 AsyncStatus = s_Asyncs[pSource->m_AsyncIndex-1].Status;

            // If we loaded an external file, update the size and buffer.
            if (AsyncStatus == MSSIO_STATUS_COMPLETE &&
                pSource->m_Data == 0)
            {
                pSource->m_DataLen = (S32)s_Asyncs[pSource->m_AsyncIndex-1].Count;
                pSource->m_Data = s_Asyncs[pSource->m_AsyncIndex-1].Buffer;
            }

            // Only update sound data size on completed asyncs.
            CurrentSystem->SoundDataSize += pSource->m_DataLen;

            if (AsyncStatus == MSSIO_STATUS_COMPLETE)
            {
                Audition_AsyncComplete((S32)(SINTa)s_Asyncs[pSource->m_AsyncIndex-1].StreamUserData, pSource->m_DataLen);
            }
            else
            {
                Audition_AsyncError((S32)(SINTa)s_Asyncs[pSource->m_AsyncIndex-1].StreamUserData);
            }

            // Clear the usage of our asyncs.
            s_AsyncUsed[pSource->m_AsyncIndex-1] = 0;
            pSource->m_AsyncIndex = 0;

            // Push completed asyncs to the back to ensure that any other pending asyncs
            // behind get handled
            {
                for (S32 NextIndex = SourceIndex + 1; NextIndex < SourceCount; NextIndex++)
                {
                    Handle Next = CurrentSystem->SoundSources[NextIndex];
                    SoundSource* pNext = (SoundSource*)Handle_Get(Next);
                    if (pNext->m_AsyncIndex != 0)
                    {
                        // Bubble us up.
                        CurrentSystem->SoundSources[NextIndex] = CurrentSystem->SoundSources[NextIndex - 1];
                        CurrentSystem->SoundSources[NextIndex - 1] = Next;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            if (AsyncStatus != MSSIO_STATUS_COMPLETE)
            {
                // Error
                pSource->m_AsyncFailed = 1;
                ErrorHandler(eAsyncLoadFail, 0, (char*)Handle_Get(pSource->m_Name));
            }
        } // end check for completed asyncs

        // Count the available asyncs.
        S32 AsyncsAvail = 0;
        for (S32 i = 0; i < MAX_ASYNCS; i++) if (s_AsyncUsed[i] == 0) AsyncsAvail++;

        // Queue new sounds if avail.
        if (AsyncsAvail)
        {
            for (S32 SourceIndex = 0; SourceIndex < SourceCount; SourceIndex++)
            {
                SoundSource* pSource = (SoundSource*)Handle_Get(CurrentSystem->SoundSources[SourceIndex]);
                if (pSource->m_AsyncIndex || pSource->m_Data || pSource->m_AsyncFailed)
                {
                    // Already queued, already loaded, or already tried.
                    continue;
                }

                // Needs to queue.
                char soundfilename[MSS_MAX_PATH_BYTES];
                MILESBANKSOUNDINFO SoundInfo;
                if (Container_GetSound((char*)Handle_Get(pSource->m_Name), soundfilename, &SoundInfo) == 0)
                {
                    // This should never happen - sound existence gets checked in load. If it does, let it
                    // sit around until it does show up in case there is a pending sound waiting for it.
                    continue;
                }

                S32 AsyncIndex = 0;
                for (AsyncIndex = 0; AsyncIndex < MAX_ASYNCS; AsyncIndex++)
                {
                    if (s_AsyncUsed[AsyncIndex] == 0) break;
                }

                AIL_memset(s_Asyncs + AsyncIndex, 0, sizeof(MilesAsyncRead));
                AIL_strcpy(s_Asyncs[AsyncIndex].FileName, soundfilename);
                s_AsyncContext[AsyncIndex] = CurrentSystem;
                set_file_line(s_Asyncs[AsyncIndex]);
                
                if (SoundInfo.IsExternal == 0)
                {
                    void* Buffer = AIL_mem_alloc_lock(SoundInfo.DataLen);
                    if (Buffer == 0)
                    {
                        ErrorHandler(eOutOfMem, 0, (char*)Handle_Get(pSource->m_Name));
                        Sound_Free(CurrentSystem, CurrentSystem->SoundSources[SourceIndex]);
                        SourceIndex--;
                        SourceCount--;
                        continue;
                    }

                    pSource->m_Data = Buffer;
                    pSource->m_DataLen = SoundInfo.DataLen;

                    s_Asyncs[AsyncIndex].Buffer = Buffer;
                    s_Asyncs[AsyncIndex].Count = SoundInfo.DataLen;
                }
                else
                {
                    s_Asyncs[AsyncIndex].Count = -1;
                }

                Audition_AsyncLoad(pSource->m_AsyncQueueId, SoundInfo.DataLen);
                s_Asyncs[AsyncIndex].StreamUserData = (void*)pSource->m_AsyncQueueId;
                s_AsyncUsed[AsyncIndex] = 1;
                pSource->m_AsyncIndex = AsyncIndex + 1;
                MilesAsyncFileRead(s_Asyncs + AsyncIndex);

                // Move this source to the front.
                S32 LimitIndex = SourceIndex;
                for (S32 i = 0; i < LimitIndex; i++)
                {
                    SoundSource* pSource = (SoundSource*)Handle_Get(CurrentSystem->SoundSources[i]);
                    if (pSource->m_AsyncIndex == 0)
                    {
                        // Swap here.
                        Handle Temp = CurrentSystem->SoundSources[i];
                        CurrentSystem->SoundSources[i] = CurrentSystem->SoundSources[SourceIndex];
                        CurrentSystem->SoundSources[SourceIndex] = Temp;
                        break;
                    }
                }

                AsyncsAvail--;
                if (AsyncsAvail == 0)
                {
                    // No more spaces open!
                    break;
                }
            } // end for each source.
        } // end if we need to queue pending.
    } // end update async for all systems

    char EventBuffer[MAX_EVENT_SIZE];

    // Traverse the command queue
    U64 EnumToken = Queue_EnumCreate();
    U64 SavedEnum = EnumToken;

    QueuedEvent* pEvent = 0;

    for (pEvent = Queue_EnumNext(&EnumToken, pEvent); pEvent; pEvent = Queue_EnumNext(&EnumToken, pEvent))
    {
        EventSystem* System = pEvent->context ? (EventSystem*)pEvent->context : s_SystemRoot;
        EVENT_STEP_INFO* pStep = 0;
        const U8* pEventSource = pEvent->event;
        const U8* pEventName = 0;
        S32 Exists = 1;

        // Find the name and the source.
        if (pEvent->flags & MILESEVENT_ENQUEUE_BY_NAME)
        {
            pEventName = pEventSource;
            pEventSource = (U8*)Container_GetEvent((char const*)pEventName);
            Exists = pEventSource != 0;

            if (Exists == 0 &&
                Audition_Status() == 0)
            {
                ErrorHandler(eMissingAsset, pEvent->eventid, (char const*)pEventName);
            }
        }
        else if (pEventSource)
        {
            if (*pEventSource == 'Z') // Only the name - missing event.
            {
                pEventName = pEventSource + 1;
                pEventSource = 0;
                Exists = 0;
            }
        }

        // The auditioner only knows about events we have a name for, for now.
        if (pEventName)
        {
            Audition_Event((char const*)pEventName, pEvent->eventid, pEvent->eventfilter, Exists, pEvent->buffer, pEvent->bufferlen);
        }

        while (pEventSource)
        {
            AIL_set_error(0);
            const U8* pCur = pEventSource;
            pStep = 0;
            pEventSource = AIL_next_event_step(pEventSource, &pStep, EventBuffer, MAX_EVENT_SIZE);

            if (pStep == 0)
                break; // Either error or insufficient room.
            // \todo if event is too big, allocate a temp slot to decode to.

            S32 Result = 0;
            switch (pStep->type)
            {
            case EVENT_STEPTYPE_SETLFO: Result = 1; break;
            case EVENT_STEPTYPE_VERSION: break;
            case EVENT_STEPTYPE_COMMENT: Result = 1; break;
            case EVENT_STEPTYPE_CLEARSTATE: Result = ExecuteClearState(System, pEvent->eventid); break;
            case EVENT_STEPTYPE_APPLYENV: Result = ExecuteApplyEnv(pStep, pEvent->eventid, System); break;
            case EVENT_STEPTYPE_CACHESOUNDS: Result = ExecuteCacheSounds(pStep, System, pEvent->eventid, 1); break;
            case EVENT_STEPTYPE_PURGESOUNDS: Result = ExecuteCacheSounds(pStep, System, pEvent->eventid, 0); break;
            case EVENT_STEPTYPE_PERSIST: Result = ExecutePersistPreset(pStep, pEvent->eventid, System); break;
            case EVENT_STEPTYPE_SETLIMITS: Result = 1; break;
            case EVENT_STEPTYPE_ENABLELIMIT: Result = ExecuteEnableLimit(System, pStep->enablelimit.limitname.str, pEvent->eventid); break;
            case EVENT_STEPTYPE_CONTROLSOUNDS: 
                {
                    if (pEvent->flags & 0x8)
                    {
                        // If the event is an internal control fro Miles*Instance functions, report the targets.
                        Audition_Control(pEvent->eventid, pStep->control.labels.str, pStep->control.type, pEvent->eventfilter);
                    }

                    Result = ExecuteControlSounds(pStep, System, pEvent->eventid, pEvent->eventfilter); 
                    break;
                }
            case EVENT_STEPTYPE_RAMP: Result = ExecuteApplyRamp(pStep, System, pEvent->eventid, pEvent->eventfilter); break;
            case EVENT_STEPTYPE_SETBLEND: Result = 1; break;
            case EVENT_STEPTYPE_STARTSOUND:
                {
                    Result = ExecuteStartSound(pStep, System, pCur, pEvent->buffer, pEvent->bufferlen, pEvent->eventid, pEvent->parentid);
                    break;
                }
            case EVENT_STEPTYPE_EXECEVENT: 
                {
                    U64 Id = 0;
                    if (pStep->exec.eventname.len)
                    {
                        Id = AIL_enqueue_event_from_init_block(pStep->exec.eventname.str, pEvent->buffer, pEvent->bufferlen, pEvent->eventid);
                        // \todo Inform auditioner that the thunk is ours...
                    }
                    Result = Id != 0;
                    break;
                }
            case EVENT_STEPTYPE_MOVEVAR: Result = ExecuteMoveVar(pStep, System); break;
            }

            // If Result is zero, it should be already reported.
        }

        // Free the event if we own it.
        if (pEvent->flags & MILESEVENT_ENQUEUE_FREE_EVENT)
        {
            AIL_mem_free_lock((void*)pEvent->event);

            // Clear the flag in case we get a full queue_clear, since its already gone.
            pEvent->flags ^= ~MILESEVENT_ENQUEUE_FREE_EVENT;
        }
    }

    // Mark completion of processing.
    Queue_EnumClose(SavedEnum, EnumToken);

    if (Audition_Pump())
    {
        // We need to flush our state.
        EventSystem* AuditionSystem = s_SystemRoot;

        //! \todo can we keep our sounds going somehow?
        //! \todo persisted presets
        //! \todo does the source free need to be after sound completion?
        S32 SoundCount = AuditionSystem->SoundInstances.Count();
        for (S32 SoundIndex = 0; SoundIndex < SoundCount; SoundIndex++)
        {
            SoundInstance* pSound = (SoundInstance*)Handle_Get(AuditionSystem->SoundInstances[SoundIndex]);
            DestroySoundInstance(AuditionSystem, pSound);
        }

        // Go over the sound sources and purge them, then reload them so that they are at the
        // same load count.
        PODHandleArray<Handle> Sources;
        Sources.Construct();
        Sources.Reacquire();
        Sources.PushCount(AuditionSystem->SoundSources.Data(), AuditionSystem->SoundSources.Count());

        S32 SourceCount = AuditionSystem->SoundSources.Count();
        for (S32 SourceIndex = 0; SourceIndex < SourceCount; SourceIndex++)
        {
            SoundSource* pSource = (SoundSource*)Handle_Get(Sources[SourceIndex]);

            char Name[512];

            S32 RefCount = pSource->m_RefCount;
            strcpy(Name, (char*)Handle_Get(pSource->m_Name));

            for (S32 i = 0; i < RefCount; i++)
            {
                Sound_Free(AuditionSystem, Sources[SourceIndex]);
            }

            for (S32 i = 0; i < RefCount; i++)
            {
                Sound_Load(AuditionSystem, 0, Name);
            }
        }

        Sources.FreeMem();
    }

    // Handle sound termination for all systems
    for (EventSystem* CurrentSystem = s_SystemRoot; CurrentSystem; CurrentSystem = CurrentSystem->Next)
    {
        S32 SoundCount = CurrentSystem->SoundInstances.Count();
        for (S32 SoundIndex = 0; SoundIndex < SoundCount; SoundIndex++)
        {
            SoundInstance* pSound = (SoundInstance*)Handle_Get(CurrentSystem->SoundInstances[SoundIndex]);
            if (pSound->m_Status == MILESEVENT_SOUND_STATUS_PLAYING)
            {
                S32 SampleStatus = 0;
                if (pSound->m_Stream) 
                {
                    SampleStatus = AIL_stream_status(pSound->m_Stream);
                    if (SampleStatus == -1)
                    {
                        // Stream went to error.
                        SampleStatus = SMP_DONE;
                    }
                }
                else 
                {
                    SampleStatus = AIL_sample_status(pSound->m_Sample);
                }

                if (SampleStatus == SMP_DONE)
                {
                    // If there is a completion event, queue it.
                    if (Handle_Get(pSound->m_CompletionEvent))
                    {
                        U64 CompletionId = AIL_enqueue_event_from_init_block((char*)Handle_Get(pSound->m_CompletionEvent), Handle_Get(pSound->m_InitializationBlock), pSound->m_InitBlockLen, pSound->m_EventId);
                        Audition_CompletionEvent(CompletionId, pSound->m_SoundId);
                    }

                    // Mark the sound as complete so they will catch it.
                    DestroySoundInstance(CurrentSystem, pSound);
                } // end if done playing
            } // end if playing
        } // end for each sound.
    } // end for each system for sound termination.

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void CompleteEventQueueForContext(EventSystem* System, float TimeDelta)
{
    S32 AuditionEnabled = Audition_Status();
    if (AuditionEnabled)
    {
        float x, y, z;
        AIL_listener_3D_position(System->Driver, &x, &y, &z);
        Audition_ListenerPosition(x, y, z);

        Audition_SendCPU(System->Driver);
        Audition_UpdateDataCount(System->SoundDataSize);
        Audition_SendCount(System->SoundInstances.Count());

        S32 Total, Current;
        Handle_MemStats(&Total, &Current);
        Audition_HandleSystemLoad(Current, Total);
    }

    //
    // Update busses
    //
    {
        Bus* B = (Bus*)Handle_Get(System->BusRoot);
        for (; B; B = (Bus*)Handle_Get(B->Next))
        {
            // Update the variable representing the bus strength.

            F32 BusDAC = B->Sample->bus_signal_strength / 32767.0f;
            char BusNameDAC[] = "_AuxBusDAC0";
            BusNameDAC[10] = '0' + B->Index;

            char const* UseName = BusNameDAC;
            if (B->Index == 0)
                UseName = "_MainBusDAC";

            Var_SetInstance(&System->VarRoot, UseName, &BusDAC, 1);

            char BusName[] = "_AuxBusDb0";
            BusName[9] = '0' + B->Index;
            UseName = BusName;

            if (B->Index == 0)
                UseName = "_MainBusDb";

            F32 BusDb = MSS_scalar_to_dB(BusDAC);

            Var_SetInstance(&System->VarRoot, UseName, &BusDb, 1);

            if (B->PresetApplied == 0 || B->Dynamic)
            {
                B->PresetApplied = 1;

                char* Preset = (char*)Handle_Get(B->Preset);
                if (Preset[0])
                {
                    void* PresetData = Container_GetPreset(Preset);
                    if (PresetData == 0)
                    {
                        ErrorHandler(eMissingAsset, 0, Preset);
                        B->Dynamic = 0;
                    }
                    else
                    {
                        s_VarContext = System;
                        AIL_resolve_raw_sound_preset(PresetData, 0, EvalFunc);
                        AIL_apply_raw_sound_preset(B->Sample, PresetData);
                    }
                }
            } // end preset management

            // Find any ramps affecting the busses.
            float FinalVolume = 1.0f;
            float FinalWet = 1.0f;
            float FinalLP = 96000;

            // Evaluate the global ramps that affect us.
            Ramp* pCurrent = (Ramp*)Handle_Get(System->RampRoot);
            for (; pCurrent; pCurrent = (Ramp*)Handle_Get(pCurrent->Next))
            {
                if (strncmp(pCurrent->Labels, "_Bus", 4))
                    continue;

                S32 TargetIndex = (S32)(pCurrent->Labels[4] - '0');
                if (TargetIndex != B->Index)
                    continue;

                float Value = Ramp_GetCurrentValue(System, pCurrent, 1);
                switch (pCurrent->ValueType)
                {
                case RAMPTYPE_VOLUME:   FinalVolume *= Value; break;
                case RAMPTYPE_WET:      FinalWet *= Value; break;
                case RAMPTYPE_LOWPASS:  FinalLP = Value; break;
                }
            }

            if (AIL_fabs(B->Sample->extra_lp - FinalLP) > .00001f)
            {
                B->Sample->low_pass_changed = ~0U;
            }

            B->Sample->extra_volume = FinalVolume;
            B->Sample->extra_wet = FinalWet;
            B->Sample->extra_lp = FinalLP;
        }
    }

    //
    // Update dynamic environment if exists.
    //
    if (System->CurrentEnv.index && System->EnvIsDynamic)
    {
        void* Env = Container_GetEnvironment((char*)Handle_Get(System->CurrentEnv));
        if (Env == 0)
            ErrorHandler(eMissingAsset, 0, (char*)Handle_Get(System->CurrentEnv));
        else
        {
            s_VarContext = System;
            AIL_resolve_raw_environment_preset(Env, EvalFunc);
            AIL_apply_raw_environment_preset(System->Driver, Env);
        }
    }

    // Update LFOs
    {
        LFO* L = (LFO*)Handle_Get(System->LFORoot);
        for (; L; L = (LFO*)Handle_Get(L->Next))
        {
            // Check for changes to the values.
            char const* FreqStr = (char const*)Handle_Get(L->FreqStr);
            char const* BaseStr = (char const*)Handle_Get(L->BaseStr);
            char const* AmpStr = (char const*)Handle_Get(L->AmpStr);

            if (FreqStr)
                Var_LookupWithContext(System, System->VarRoot, FreqStr, 1, &L->Freq);
            if (BaseStr)
                Var_LookupWithContext(System, System->VarRoot, BaseStr, 1, &L->Base);
            if (AmpStr)
                Var_LookupWithContext(System, System->VarRoot, AmpStr, 1, &L->Amp);

            // Find the oscillation amount.
            float Period = 1 / L->Freq;

            float Value = 0;
            switch (L->Waveform)
            {
            case 0: Value = (L->Polarity ? 1 : 0) + sinf(2*3.14159f * L->Time / Period + L->Invert*3.1415f); break;

            case 1: // Triangle
                {
                    if (L->Time > Period/2)
                        Value = 2 - 2 * L->Time / Period;
                    else
                        Value = 2 * L->Time / Period;

                    if (L->Invert)
                        Value = 1 - Value;

                    if (L->Polarity == 0)
                        Value -= .5f;
                    break;
                }

            case 2: // Saw
                {
                    Value = L->Time / Period;
                    if (L->Invert)
                        Value = 1 - Value;
                    if (L->Polarity == 0)
                        Value -= .5f;
                    break;
                }
            case 3: // Square
                {
                    if (100 * L->Time / Period < L->DutyCycle)
                        Value = 1;
                    else
                        Value = 0;
                    if (L->Invert)
                        Value = !Value;
                    if (L->Polarity == 0)
                        Value -= .5f;
                    break;
                }
            }

            Value = Value * L->Amp + L->Base;


            L->Time += TimeDelta;
            if (L->Time >= Period)
                L->Time -= Period;

            Var_SetInstance(&System->VarRoot, L->Var, &Value, 1);
        }
    }

    //
    // Update Move Vars
    //
    {
        MoveVar* Last = 0;
        MoveVar* V = (MoveVar*)Handle_Get(System->MoveRoot);
        for (; V; )
        {
            // Update the variable.
            S32 Index = V->CurrentTime >= V->Times[0];
            F32 Param = V->CurrentTime;
            
            if (Index)
                Param = (Param - V->Times[0]) / (V->Times[1] - V->Times[0]);
            else
                Param = Param / V->Times[0];

            switch (V->InterpTypes[Index])
            {
            case 0: Param = 1 - (1 - Param)*(1 - Param); break;
            case 1: Param = Param*Param; break;
            case 2: 
                Param -= .5f;
                Param = Param * Param * Param;
                Param *= 4;
                Param += .5f;
                break;
            case 3: break; // Linear
            }

            F32 Value = V->Values[Index] + Param*(V->Values[Index + 1] - V->Values[Index]);
            Var_SetInstance(&System->VarRoot, V->Name, &Value, 1);            

            // Advance and free.
            if (V->CurrentTime >= V->Times[1])
            {
                // Expired - remove.
                MoveVar* Next = (MoveVar*)Handle_Get(V->Next);

                if (Last)
                    Last->Next = V->Next;
                else
                    System->MoveRoot = V->Next;

                Handle_Free(V->Self);
                V = Next;
            }
            else
            {
                V->CurrentTime += TimeDelta;

                Last = V;
                V = (MoveVar*)Handle_Get(V->Next);
            }   
        }
    }

    //
    // Send variable state.
    //
    if (AuditionEnabled)
    {
        VarDef* pCurrent = (VarDef*)Handle_Get(System->VarRoot);
        for (; pCurrent; pCurrent = (VarDef*)Handle_Get(pCurrent->Next))
        {
            Audition_VarState(pCurrent->Name, 0, !pCurrent->IsFloat, &pCurrent->i);
        }
    }

    //
    // Update Ramps
    //
    {
        Ramp* pCurrent = (Ramp*)Handle_Get(System->RampRoot);
        for (; pCurrent; pCurrent = (Ramp*)Handle_Get(pCurrent->Next))
        {
            pCurrent->CurrentTime += TimeDelta;
            if (pCurrent->CurrentTime > pCurrent->TotalTime) 
                pCurrent->CurrentTime = pCurrent->TotalTime;

            if (AuditionEnabled)
                Audition_RampState(pCurrent->Name, 0, pCurrent->ValueType, Ramp_GetCurrentValue(System, pCurrent, 1));
        }
    } // End ramp update


    S32 PendingCount = 0;
    S32 PlayingCount = 0;

    // Start pending sounds, free completed sounds, update running sounds.
    S32 SoundCount = System->SoundInstances.Count();
    for (S32 SoundIndex = 0; SoundIndex < SoundCount; SoundIndex++)
    {
        SoundInstance* pSound = (SoundInstance*)Handle_Get(System->SoundInstances[SoundIndex]);

        if (AuditionEnabled &&
            pSound->m_Sample->is_3D)
        {
            float x, y, z;
            AIL_sample_3D_position(pSound->m_Sample, &x, &y, &z);
            Audition_SoundPosition(pSound->m_SoundId, x, y, z);
        }

        if (AuditionEnabled)
            Audition_SoundFlags(pSound->m_SoundId, pSound->m_Flags);

        if (pSound->m_Status == MILESEVENT_SOUND_STATUS_PENDING)
        {
            PendingCount++;
            // Check the async status, only if we have a sound source
            // We can not have one by a) being an enqueued silence, or
            // b) being a missing sound.
            //
            if (!(pSound->m_Flags & MILESEVENT_SOUND_FLAG_MISSING_SOUND) &&
                pSound->m_NameLen != 0)
            {
                if (pSound->m_Flags & MILESEVENT_SOUND_FLAG_PENDING_ASYNC)
                {
                    // See if the async has started.
                    SoundSource* pSource = (SoundSource*)Handle_Get(pSound->m_Source);
                    if (pSource->m_AsyncFailed)
                    {
                        // The load failed.
                        DestroySoundInstance(System, pSound);

                        pSound->m_Flags ^= MILESEVENT_SOUND_FLAG_PENDING_ASYNC;
                        pSound->m_Flags |= MILESEVENT_SOUND_FLAG_FAILED_ASYNC;
                        Audition_SoundFlags(pSound->m_SoundId, pSound->m_Flags);
                        continue;
                    }
                    else if (pSource->m_AsyncIndex)
                    {
                        pSound->m_Flags ^= MILESEVENT_SOUND_FLAG_PENDING_ASYNC;
                        pSound->m_Flags |= MILESEVENT_SOUND_FLAG_WAITING_ASYNC;
                    }
                    else
                    {
                        // Still haven't been assigned an async.
                        continue;
                    }
                }

                if (pSound->m_Flags & MILESEVENT_SOUND_FLAG_WAITING_ASYNC)
                {
                    // Find the data offset for the initial position if needed.
                    S32 OffsetCheck = pSound->m_StartOffset;
                    if (pSound->m_StartOffsetIsMS)
                    {
                        // We can't look up the offset since the ASI hasn't been opened, so we need to just try
                        // and guess.
                        MILESBANKSOUNDINFO SoundInfo;
                        Container_GetSound((char const*)Handle_Get(pSound->m_Name), 0, &SoundInfo);
                        OffsetCheck = mult64anddiv(OffsetCheck, SoundInfo.Rate, 1000);

                        // Got a sample offset... need byte offset though. 
                        OffsetCheck = OffsetCheck * 2 * SoundInfo.ChannelCount;

                        // Estimate at around 8x compression.
                        OffsetCheck >>= 3;
                    }

                    // Try to catch the situation where we need data that isn't at the beginning to start th esound.
                    S32 RequiredData = 64*1024 + OffsetCheck;

                    // Check if the sound has completed loading.
                    SoundSource* pSource = (SoundSource*)Handle_Get(pSound->m_Source);
                    if (pSource->m_AsyncFailed)
                    {
                        DestroySoundInstance(System, pSound);

                        // Mark that the load failed.
                        pSound->m_Flags ^= MILESEVENT_SOUND_FLAG_WAITING_ASYNC;
                        pSound->m_Flags |= MILESEVENT_SOUND_FLAG_FAILED_ASYNC;
                        Audition_SoundFlags(pSound->m_SoundId, pSound->m_Flags);
                        continue;
                    }
                    else if (pSource->m_AsyncIndex == 0 ||
                        s_Asyncs[pSource->m_AsyncIndex-1].ReadAmt >= RequiredData)
                    {
                        // Load is complete, or we've got enough loaded to start.
                        if (pSource->m_AsyncIndex == 0)
                        {
                            pSound->m_Flags ^= MILESEVENT_SOUND_FLAG_WAITING_ASYNC;
                            Audition_SoundFlags(pSound->m_SoundId, pSound->m_Flags);
                        }

                        void* Data = pSource->m_Data;
                        S32 DataLen = pSource->m_DataLen;
                        if (pSource->m_AsyncIndex)
                        {
                            Data = s_Asyncs[pSource->m_AsyncIndex - 1].Buffer;
                            DataLen = (S32)s_Asyncs[pSource->m_AsyncIndex - 1].Count;
                        }
                        if (pSource->m_Provider)
                        {
                            AIL_set_sample_address(pSound->m_Sample, Data, DataLen);
                            if (AIL_set_sample_processor(pSound->m_Sample, SP_ASI_DECODER, pSource->m_Provider) == (HPROVIDER)-1)
                            {
                                // Bad format.
                                ErrorHandler(eMilesError, pSound->m_SoundId, (char*)Handle_Get(pSource->m_Name));
                                DestroySoundInstance(System, pSound);
                                continue;
                            }
                        }
                        else
                        {
                            // Find the wav data ptr.
                            AILSOUNDINFO Info;
                            if (AIL_WAV_info(Data, &Info) == 0)
                            {
                                ErrorHandler(eMilesError, pSound->m_SoundId, (char*)Handle_Get(pSource->m_Name));
                                DestroySoundInstance(System, pSound);
                                continue;
                            }
                            AIL_set_sample_address(pSound->m_Sample, Info.data_ptr, Info.data_len);
                        }
                    }
                    else
                    {
                        // Not enough data loaded to start.
                        continue;
                    }
                } // end if waiting on async.
            } // end if sound isn't silence

            if (pSound->m_LoopLookup)
            {
                // Convert loop offsets from MS if they were ms.
                U32 StartLookup = (pSound->m_LoopLookup >> LOOP_START_SHIFT) & TIME_MASK;
                U32 EndLookup = (pSound->m_LoopLookup >> LOOP_END_SHIFT) & TIME_MASK;

                // Now depending on the type of offset, we call the appropriate functions
                if (StartLookup == TIME_SAMPLES_MASK && EndLookup == TIME_SAMPLES_MASK)
                {
                    if (!pSound->m_Stream)
                        AIL_set_sample_loop_samples(pSound->m_Sample, pSound->m_LoopStart, pSound->m_LoopEnd);
                }
                else if (StartLookup == TIME_SAMPLES_MASK)
                {
                    if (!pSound->m_Stream)
                    {
                        AIL_set_sample_loop_samples(pSound->m_Sample, pSound->m_LoopStart, -2);
                        AIL_set_sample_loop_block(pSound->m_Sample, -2, pSound->m_LoopEnd);
                    }
                }
                else if (EndLookup == TIME_SAMPLES_MASK)
                {
                    if (!pSound->m_Stream)
                    {
                        AIL_set_sample_loop_samples(pSound->m_Sample, -2, pSound->m_LoopEnd);
                        AIL_set_sample_loop_block(pSound->m_Sample, pSound->m_LoopStart, -2);
                    }
                }
                else
                {
                    if (pSound->m_Stream)
                        AIL_set_stream_loop_block(pSound->m_Stream, pSound->m_LoopStart, pSound->m_LoopEnd);
                    else
                        AIL_set_sample_loop_block(pSound->m_Sample, pSound->m_LoopStart, pSound->m_LoopEnd);
                }
            }

            if (pSound->m_StartOffset != 0)
            {
                if (pSound->m_StartOffsetIsMS == 0)
                {
                    if (pSound->m_Stream) AIL_set_stream_position(pSound->m_Stream, pSound->m_StartOffset);
                    else AIL_set_sample_position(pSound->m_Sample, pSound->m_StartOffset);
                }
                else
                {
                    if (pSound->m_Stream) AIL_set_stream_ms_position(pSound->m_Stream, pSound->m_StartOffset);
                    else AIL_set_sample_ms_position(pSound->m_Sample, pSound->m_StartOffset);
                }
            }

            // It may have been some time since the initial preset apply, so if we are dynamic, reeval.
            if (pSound->m_DynamicPreset.index)
            {
                char* PresetName = (char*)Handle_Get(pSound->m_DynamicPreset);
                void* Preset = Container_GetPreset(PresetName);
                if (Preset == 0)
                    ErrorHandler(eMissingAsset, pSound->m_SoundId, PresetName);
                else
                {
                    AIL_resolve_raw_sound_preset(Preset, pSound, EvalFunc);
                    AIL_apply_raw_sound_preset(pSound->m_Sample, Preset);
                }
            }

            if (pSound->m_Stream) AIL_pause_stream(pSound->m_Stream, 0);
            else AIL_resume_sample(pSound->m_Sample);

            Audition_SoundPlaying(pSound->m_SoundId);
            pSound->m_Status = MILESEVENT_SOUND_STATUS_PLAYING;
        } // end if sound pending
        else if (pSound->m_Status == MILESEVENT_SOUND_STATUS_COMPLETE)
        {
            // Sound is done, free everything.
            Audition_SoundComplete(pSound->m_SoundId);

            Handle_Free(pSound->m_InitializationBlock);
            Handle_Free(pSound->m_CompletionEvent);

            if (pSound->m_Stream)
                AIL_close_stream(pSound->m_Stream);
            else
                AIL_release_sample_handle(pSound->m_Sample);

            // Free the local ramps
            {
                Ramp* pCurrent = (Ramp*)Handle_Get(pSound->m_RampRoot);
                while (pCurrent)
                {
                    Handle Next = pCurrent->Next;
                    Handle_Free(pCurrent->Self);
                    pCurrent = (Ramp*)Handle_Get(Next);
                }
            }

            // Free the local vars.
            {
                VarDef* pCurrent = (VarDef*)Handle_Get(pSound->m_VarRoot);
                while (pCurrent)
                {
                    Handle Next = pCurrent->Next;
                    Handle_Free(pCurrent->Self);
                    pCurrent = (VarDef*)Handle_Get(Next);
                }
            }

            SoundSource* pSoundSource = (SoundSource*)Handle_Get(pSound->m_Source);
            if (pSoundSource)
                pSoundSource->m_SoundCount--;

            // Mark that we are done with the sound source.
            Handle_Free(pSound->m_Name);
            Handle_Free(pSound->m_Labels);
            Handle_Free(pSound->m_DynamicPreset);
            Sound_Free(System, pSound->m_Source);
            Handle_Free(System->SoundInstances[SoundIndex]);
            System->SoundInstances.RemoveUnsorted(SoundIndex);
            SoundCount--;
            SoundIndex--;
        } // end if sound complete
        else if (pSound->m_Status == MILESEVENT_SOUND_STATUS_PLAYING)
        {
            PlayingCount++;
            SoundSource* pSource = (SoundSource*)Handle_Get(pSound->m_Source);
            if (pSource) // Source can be null if the sound was replaced with silence.
            {
                if (pSource->m_AsyncFailed)
                {
                    DestroySoundInstance(System, pSound);

                    // Mark that the load failed.
                    pSound->m_Flags ^= MILESEVENT_SOUND_FLAG_WAITING_ASYNC;
                    pSound->m_Flags |= MILESEVENT_SOUND_FLAG_FAILED_ASYNC;
                    Audition_SoundFlags(pSound->m_SoundId, pSound->m_Flags);
                }
                else if (pSound->m_Flags & MILESEVENT_SOUND_FLAG_WAITING_ASYNC &&
                    pSource->m_AsyncIndex == 0)
                {
                    // Complete.
                    pSound->m_Flags ^= MILESEVENT_SOUND_FLAG_WAITING_ASYNC;
                    Audition_SoundFlags(pSound->m_SoundId, pSound->m_Flags);
                }
            } // end if valid source

            // Check for blend status.
            float BlendVol, BlendPitch;
            Blend_Apply(pSound, &BlendVol, &BlendPitch);

            // Apply dynamic presets
            if (pSound->m_DynamicPreset.index)
            {
                char* PresetName = (char*)Handle_Get(pSound->m_DynamicPreset);
                void* Preset = Container_GetPreset(PresetName);
                if (Preset == 0)
                    ErrorHandler(eMissingAsset, pSound->m_SoundId, PresetName);
                else
                {
                    AIL_resolve_raw_sound_preset(Preset, pSound, EvalFunc);
                    AIL_apply_raw_sound_preset(pSound->m_Sample, Preset);
                }
            }

            // Apply persists
            char* Labels = (char*)Handle_Get(pSound->m_Labels);
            Persist_Apply(System, pSound, pSound->m_Sample, Labels, (S32)strlen(Labels), 1);

            Ramp_ApplyAll(System, pSound, TimeDelta);

            // Update per-instance fades.
            if (pSound->m_FadeDeltaPerSecond != 0)
            {
                F32 FadeDelta = 0;
                if (pSound->m_FadeDelay > 0)
                {
                    pSound->m_FadeDelay -= TimeDelta;
                    if (pSound->m_FadeDelay <= 0)
                    {
                        FadeDelta = -pSound->m_FadeDelay;
                        pSound->m_FadeDelay = 0;
                    }
                }
                else 
                    FadeDelta = TimeDelta;

                pSound->m_FadeVolume += pSound->m_FadeDeltaPerSecond * FadeDelta;
                if (pSound->m_FadeVolume > 1) 
                {
                    // Sound has faded all the way in.
                    pSound->m_FadeVolume = 1;
                    pSound->m_FadeDeltaPerSecond = 0;
                }
                if (pSound->m_FadeVolume <= 0 && pSound->m_FadeDeltaPerSecond < 0)
                {
                    // Sound has faded all the way out.
                    pSound->m_FadeDeltaPerSecond = 0;
                    pSound->m_FadeVolume = 0;
                    DestroySoundInstance(System, pSound);
                }
            }

            if (AuditionEnabled)
            {
                Audition_SoundState(pSound->m_SoundId, 
                    pSound->m_Sample->max_output_mix_volume,
                    pSound->m_Sample->S3D.atten_3D, BlendVol, BlendPitch, 
                    pSound->m_Sample->extra_volume, pSound->m_Sample->extra_wet, 
                    pSound->m_Sample->extra_lp, pSound->m_Sample->extra_rate
                    );

                VarDef* pCurrent = (VarDef*)Handle_Get(pSound->m_VarRoot);
                for (; pCurrent; pCurrent = (VarDef*)Handle_Get(pCurrent->Next))
                {
                    Audition_VarState(pCurrent->Name, pSound->m_SoundId, !pCurrent->IsFloat, &pCurrent->i);
                }
            }
        } // end if sound playing
    } // end for each sound
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" S32 AILCALL
MilesCompleteEventQueueProcessingInternal()
{
    if (s_SystemRoot == 0) return 1;

    //! \todo this should be on a timer.
    static S32 LastUpdate = 0;
    S32 CurrentTime = AIL_ms_count();
    float SecondsSinceLastUpdate = 0;

    // Ignore updates that are too long.
    if ((CurrentTime - LastUpdate) < 1000)
    {
        SecondsSinceLastUpdate = (CurrentTime - LastUpdate) / 1000.0f;
    }
    LastUpdate = CurrentTime;

    Audition_Suppress(0);
    for (EventSystem* CurrentSystem = s_SystemRoot; CurrentSystem; CurrentSystem = CurrentSystem->Next)
    {
        CompleteEventQueueForContext(CurrentSystem, SecondsSinceLastUpdate);
        Audition_Suppress(1);
    }
    Audition_Suppress(0);

    Audition_DefragStart();

    Handle_Defrag();

    for (EventSystem* CurrentSystem = s_SystemRoot; CurrentSystem; CurrentSystem = CurrentSystem->Next)
    {
        CurrentSystem->SoundInstances.Reacquire();
        CurrentSystem->SoundSources.Reacquire();
    }

    Audition_FrameEnd();

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
EventSystem* EventSystem_Allocate(HDIGDRIVER i_Driver)
{
    EventSystem* System = (EventSystem*)AIL_mem_alloc_lock(sizeof(EventSystem));
    AIL_memset(System, 0, sizeof(EventSystem));

    System->Tag = 'ESYS';
    System->Driver = i_Driver;

    // Leave the root in place if it already exists.
    if (s_SystemRoot)
    {
        System->Next = s_SystemRoot->Next;
        s_SystemRoot->Next = System;
    }
    else
    {
        System->Next = s_SystemRoot;
        s_SystemRoot = System;
    }

    return System;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void EventSystem_Free(EventSystem* System)
{
    // ONLY clears memory/resources that won't be cleared by a Handle_Shutdown()

    // Sounds...
    S32 SoundCount = System->SoundInstances.Count();
    for (S32 SoundIndex = 0; SoundIndex < SoundCount; SoundIndex++)
    {
        SoundInstance* pSound = (SoundInstance*)Handle_Get(System->SoundInstances[SoundIndex]);

        if (pSound->m_Stream)
        {
            AIL_close_stream(pSound->m_Stream);
        }
        else
        {
            AIL_release_sample_handle(pSound->m_Sample);
        }

        // Handles are free'd when by Handle_Shutdown();
    }
    System->SoundInstances.FreeMem();

    //! \todo outstanding asyncs!
    // Sources...
    SoundCount = System->SoundSources.Count();
    for (S32 SoundIndex = 0; SoundIndex < SoundCount; SoundIndex++)
    {
        SoundSource* pSource = (SoundSource*)Handle_Get(System->SoundSources[SoundIndex]);
        AIL_mem_free_lock(pSource->m_Data);
    }
    System->SoundSources.FreeMem();

    AIL_mem_free_lock(System);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" HEVENTSYSTEM AILCALL MilesAddEventSystemInternal(HDIGDRIVER i_Driver)
{
    return EventSystem_Allocate(i_Driver);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" HEVENTSYSTEM AILCALL
MilesStartupEventSystemInternal(HDIGDRIVER i_Driver, S32 i_CommandBufferSize, char* i_Memory, S32 i_MemorySize)
{
    if (s_SystemRoot != 0)
    {
        // Additional drivers are added in a different call.
        return 0;
    }

    // Set up global state.
    // Seed the randomizer
    s_RandomSeed = AIL_us_count();

    // Check if we need to allocate memory.
    if (i_Memory == 0)
    {
        S32 UseSize = i_MemorySize;
        if (UseSize == 0)
        {
            // Just use 65k
            UseSize = 64*1024;
        }

        s_MemoryBuffer = AIL_mem_alloc_lock(UseSize);

        if (s_MemoryBuffer == 0) return 0;

        i_Memory = (char*)s_MemoryBuffer;
        i_MemorySize = UseSize;
    }

    // Init handle system.
    Handle_Init(i_Memory, i_MemorySize);

    // Initialize the command buffer
    {
        S32 use_size = i_CommandBufferSize;
        if (use_size == 0)
            use_size = 5*1024;
        if (use_size >= i_MemorySize)
            use_size = i_MemorySize / 2;
        Handle CmbBufHandle = Handle_Allocate(use_size);
        if (Handle_Get(CmbBufHandle) == 0)
        {
            Handle_Shutdown();
            if (s_MemoryBuffer)
            {
                AIL_mem_free_lock(s_MemoryBuffer);
                s_MemoryBuffer = 0;
            }
            return 0;
        }

        Queue_SetMem(use_size, CmbBufHandle);
    }

    AIL_memset(s_Asyncs, 0, sizeof(s_Asyncs));
    AIL_memset(s_AsyncUsed, 0, sizeof(s_AsyncUsed));
    AIL_memset(s_AsyncCancelled, 0, sizeof(s_AsyncCancelled));
    AIL_memset(s_AsyncContext, 0, sizeof(s_AsyncContext));

    EventSystem* System = EventSystem_Allocate(i_Driver);
    if (System == 0)
    {
        Handle_Shutdown();
        if (s_MemoryBuffer)
        {
            AIL_mem_free_lock(s_MemoryBuffer);
            s_MemoryBuffer = 0;
        }
        return 0;
    }

    return System;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void AILCALL
MilesShutdownEventSystemInternal()
{
    // Cancel all outstanding asyncs.
    for (;;)
    {
        S32 StillWaiting = 0;
        for (S32 i = 0; i < MAX_ASYNCS; i++)
        {
            if (s_AsyncUsed[i] && s_AsyncCancelled[i] == 0)
            {
                MilesAsyncFileCancel(s_Asyncs + i);
                s_AsyncCancelled[i] = 1;
            }

            if (s_AsyncCancelled[i])
            {
                if (MilesAsyncFileStatus(s_Asyncs + i, 0))
                {
                    // Done with the cancel.
                    AIL_mem_free_lock(s_Asyncs[i].Buffer);
                    s_AsyncCancelled[i] = 0;
                    s_AsyncUsed[i] = 0;
                }
                else
                    StillWaiting = 1;
            }
        }

        if (StillWaiting == 0)
        {
            break;
        }
        rrThreadSleep(1);
    }

    // Clear all sound sources that would be sharing data with the above asyncs
    for (EventSystem* CurrentSystem = s_SystemRoot; CurrentSystem; CurrentSystem = CurrentSystem->Next)
    {
        // Check for outstanding asyncs - they are at the front of the source list
        S32 SourceCount = CurrentSystem->SoundSources.Count();
        for (S32 SourceIndex = 0; SourceIndex < SourceCount; SourceIndex++)
        {
            SoundSource* pSource = (SoundSource*)Handle_Get(CurrentSystem->SoundSources[SourceIndex]);
            if (pSource->m_AsyncIndex == 0)
            {
                break;
            }

            pSource->m_Data = 0;
        }
    }

    // Free all systems.
    for (EventSystem* CurrentSystem = s_SystemRoot; CurrentSystem; )
    {
        EventSystem* Next = CurrentSystem->Next;
        EventSystem_Free(CurrentSystem);
        CurrentSystem = Next;
    }

    s_SystemRoot = 0;
   
    // We have to do an actual clear in case we own any memory.
    MilesClearEventQueueInternal();

    // Banks...
    Container_FreeAll();

    // Invalidate all handle allocated memory.
    Handle_Shutdown();

    // Free memory if we had to.
    if (s_MemoryBuffer)
    {
        AIL_mem_free_lock(s_MemoryBuffer);
        s_MemoryBuffer = 0;
    }
}
#define SELECT_TYPE(step) (step->start.selecttype & MILES_START_STEP_SELECT_MASK)


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 InitSample(HSAMPLE S, MILESBANKSOUNDINFO* i_Info)
{
    S32 format = DIG_F_16BITS_MASK;
    if (i_Info->ChannelCount > 2 || i_Info->ChannelMask != ~0U) format |= DIG_F_MULTICHANNEL_MASK | (i_Info->ChannelCount << 16);
    else if (i_Info->ChannelCount == 2) format |= DIG_F_STEREO_MASK;
    if (i_Info->IsAdpcm)
        format |= DIG_F_ADPCM_MASK;

    if (AIL_init_sample(S, format) == 0)
    {
        return 0;
    }

    AIL_set_sample_playback_rate(S, i_Info->Rate);

    if (i_Info->IsAdpcm)
        AIL_set_sample_adpcm_block_size(S, i_Info->AdpcmBlockSize);

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 StartSingle(
    const EVENT_STEP_INFO* i_Step, EventSystem* i_System, char const* i_SoundName, S32 i_SoundNameLen,
    void* i_InitBlock, U32 i_InitBlockLen, U64 i_EventId, U64 i_ParentId, S8 i_Index
    )
{

    //
    // Resolve the random delay before we have a chance to fail due to
    // memory, in the hopes that we retain predictability in the face of
    // OOM.
    // (This is futile if we have random var values, handled after allocations)
    //
    S32 UseDelay = i_Step->start.delaymin;
    if (i_Step->start.delaymax - i_Step->start.delaymin) 
        UseDelay += HorribleRand() % (i_Step->start.delaymax - i_Step->start.delaymin);

    F32 UseVol = i_Step->start.volmin;
    if (i_Step->start.volmax - i_Step->start.volmin > .001f) 
        UseVol += (i_Step->start.volmax - i_Step->start.volmin) * ( (F32) (HorribleRand() & 1023) / 1023.0f);

    F32 UsePitch = i_Step->start.pitchmin;
    if (i_Step->start.pitchmax - i_Step->start.pitchmin > .001f) 
        UsePitch += (i_Step->start.pitchmax - i_Step->start.pitchmin) * ( (F32) (HorribleRand() & 1023) / 1023.0f);

    char soundname[MSS_MAX_PATH_BYTES];
    soundname[0] = 0;
    if (i_SoundNameLen)
    {
        strncpy(soundname, i_SoundName, i_SoundNameLen);
        soundname[i_SoundNameLen] = 0;

        if (i_Step->start.selecttype >> MILES_START_STEP_SELECT_BITS)
        {
            S32 Selection = -1; 
            // Check if we have a selection in the init block.
            {
                char* buffer = (char*)i_InitBlock;
                char* buffer_end = (char*)buffer + i_InitBlockLen;
                U32 buffer_token = *(U32*)buffer;

                while (buffer_token && buffer < buffer_end)
                {
                    U32 advance_amt = *(U32*)(buffer + 4);

                    if (buffer_token == BUFFER_SELECT)
                    {
                        Selection = *(U32*)(buffer + 8);
                        break;
                    }
                    
                    buffer += advance_amt;
                    if (buffer < buffer_end)
                        buffer_token = *(U32*)buffer;
                    else 
                        break;
                }
            }

            if (Selection >= 0)
            {
                // Find the number at the end.
                char* NumberIndex = soundname + i_SoundNameLen;

                for (;;)
                {
                    char v = NumberIndex[-1];
                    if (v >= '0' && v <= '9')
                    {
                        NumberIndex--;
                        continue;
                    }
                    break;
                }

                if (NumberIndex != soundname + i_SoundNameLen)
                {
                    SINTa zero_count = (soundname + i_SoundNameLen) - NumberIndex;
                    char fmt[] = "%00d";
                    fmt[2] = '0' + (char)zero_count;
                    sprintf(NumberIndex, fmt, Selection);
                }
            }
            else
                ErrorHandler(eMissingSelection, i_EventId, soundname);

        } // end if SELECT
    } // end if not playing silence.

    // Reserve our sound id up front for error association and auditioning.
    U64 SoundId = s_NextSoundId++;

    Audition_Sound(i_EventId, SoundId, soundname, i_Step->start.labels.str, UseVol, UseDelay, UsePitch);


    //
    // Check for instance limits.
    //
    {
        SoundLimitEntry* EffectiveLimit = 0;
        for (;;)
        {
            EffectiveLimit = IsLabelLimited(i_System, i_Step->start.labels);
            if (EffectiveLimit == 0) break; // No limits!

            // See if there is a lower priority sound we can evict.
            if (EvictSound(i_System, SoundId, i_Step->start.evictiontype, i_Step->start.priority, EffectiveLimit, 0, 0) == 0)
            {
                Audition_SoundLimited(SoundId, EffectiveLimit->Label);
                // We can't play due to sound limits.
                return 1;
            }

            // Loop to make sure there aren't multiple limits.
        }
    }

    char soundfilename[MSS_MAX_PATH_BYTES];
    HSTREAM NewStream = 0;
    HSAMPLE Sample;
    Handle Source = Handle_Clear();
    U8 Flags = 0;
    U8 BusIndex = 0;

    MILESBANKSOUNDINFO SoundInfo;
    AIL_memset(&SoundInfo, 0, sizeof(SoundInfo));
    if (i_SoundNameLen && Container_GetSound(soundname, soundfilename, &SoundInfo) == 0)
    {
        ErrorHandler(eMissingAsset, SoundId, soundname);

        // Switch to silence.
        i_SoundNameLen = 0;
        soundname[0] = 0;
        Flags |= MILESEVENT_SOUND_FLAG_MISSING_SOUND;
    }

    if (i_SoundNameLen && i_Step->start.stream)
    {
        // If we have a sound limit, check the number of instances with this sound name.
        if (SoundInfo.SoundLimit)
        {
            S32 Count = 0;
            S32 InstanceCount = i_System->SoundInstances.Count();
            for (S32 i = 0; i < InstanceCount; i++)
            {
                SoundInstance* pSound = (SoundInstance*)Handle_Get(i_System->SoundInstances[i]);
                if (pSound->m_NameLen == i_SoundNameLen)
                {
                    char const* pSoundName = (char const*)Handle_Get(pSound->m_Name);
                    if (AIL_stricmp(pSoundName, soundname) == 0) Count++;
                    if (Count >= SoundInfo.SoundLimit) break;
                }
            }

            if (Count >= SoundInfo.SoundLimit)
            {
                // Try to evict a sound.
                if (EvictSound(i_System, SoundId, i_Step->start.evictiontype, i_Step->start.priority, 0, 0, soundname) == 0)
                {
                    Audition_SoundLimited(SoundId, "FailedEvict");
                    // Limited - can't play.
                    return 1;
                }
            }
        }

        NewStream = AIL_open_stream(i_System->Driver, soundfilename, SoundInfo.StreamBufferSize);
        if (NewStream == 0)
        {
            ErrorHandler(eMilesError, SoundId, soundname);
            return 0;
        }
        Sample = AIL_stream_sample_handle(NewStream);
    }
    else if (i_SoundNameLen) // only try to find and load a sound if we had a sound.
    {
        Handle SourceHandle = Sound_Find(i_System, soundname);
        SoundSource* pSoundSource = (SoundSource*)Handle_Get(SourceHandle);
        if (pSoundSource == 0)
        {
            // Don't already have it. Sounds can only be loaded if its allowed.
            if (i_Step->start.canload == 0)
            {
                // Instead, we enqueue silence
                i_SoundNameLen = 0;
                soundname[0] = 0;
                Flags |= MILESEVENT_SOUND_FLAG_FAILED_HITCH;
                goto EnqueueSilence;
            }

            SourceHandle = Sound_Load(i_System, SoundId, soundname);

            // Success?
            pSoundSource = (SoundSource*)Handle_Get(SourceHandle);
            if (pSoundSource == 0)
            {
                return 0;
            }

            // Don't need to check limits here, since they have to be at least 1...
        }
        else
        {
            if (pSoundSource->m_SoundLimit && pSoundSource->m_SoundCount >= pSoundSource->m_SoundLimit)
            {
                // Try to evict a sound.
                if (EvictSound(i_System, SoundId, i_Step->start.evictiontype, i_Step->start.priority, 0, pSoundSource, 0) == 0)
                {
                    Audition_SoundLimited(SoundId, "FailedEvict");

                    // Limited - can't play.
                    return 1;
                }
            }

            pSoundSource->m_RefCount++;
        }

        Sample = AIL_allocate_sample_handle(i_System->Driver);
        if (Sample == 0)
        {
            ErrorHandler(eMilesError, SoundId, soundname);
            Sound_Free(i_System, SourceHandle);
            return 0;
        }

        //
        // Check if the sound is already completely loaded, and hasn't failed.
        //
        if (pSoundSource->m_Data && pSoundSource->m_AsyncIndex == 0 && pSoundSource->m_AsyncFailed == 0)
        {
            if (AIL_set_named_sample_file(Sample, soundfilename, pSoundSource->m_Data, pSoundSource->m_DataLen, 0) == 0)
            {
                ErrorHandler(eMilesError, SoundId, soundname);
                AIL_release_sample_handle(Sample);
                Sound_Free(i_System, SourceHandle);
                return 0;
            }
        }
        else 
        {
            // Init the sample to the data format.
            InitSample(Sample, &SoundInfo);
            if (pSoundSource->m_AsyncIndex)
            {
                Flags |= MILESEVENT_SOUND_FLAG_WAITING_ASYNC;
            }
            else
            {
                Flags |= MILESEVENT_SOUND_FLAG_PENDING_ASYNC;
            }
        }

        // If we have busses, we need to assign sounds to the bus.
        {
            U8 parsed_lbls[ 512 ];
            parse_string_list( parsed_lbls, i_Step->start.labels.str, i_Step->start.labels.len, ',', 0);

            Handle H = i_System->BusRoot;
            Bus* B = (Bus*)Handle_Get(H);
            for (; B; B = (Bus*)Handle_Get(B->Next))
            {
                // Skip over the main bus, since we default to that.
                if (B->Index == 0)
                    continue;

                U8 parsed_qry[ 512 ];
                parse_wildcard_query( parsed_qry, B->Labels, (U32)AIL_strlen(B->Labels), ',', 0 );

                if (wild_card_match_list_to_query( parsed_lbls, i_Step->start.labels.str, parsed_qry, B->Labels))
                {
                    BusIndex = B->Index;
                    AIL_set_sample_bus(Sample, B->Index);
                    break; // Set the first bus.
                }
             }
        }

        Source = SourceHandle;
    }
    else
    {
EnqueueSilence:
        // No sound specified - we need to queue silence.
        Sample = AIL_allocate_sample_handle(i_System->Driver);
        if (Sample == 0)
        {
            ErrorHandler(eMilesError, SoundId, "Silence");
            return 0;
        }

        if (AIL_init_sample(Sample, DIG_F_STEREO_16) == 0)
        {
            ErrorHandler(eMilesError, SoundId, "Silence");
            AIL_release_sample_handle(Sample);
            return 0;
        }

        AIL_set_sample_address(Sample, &s_SilentSoundData, sizeof(U64));
    }


    //
    // Allocate all the buffers we'll need up front and check for out of memory.
    //
    if (i_System->SoundInstances.PushEmpty() == 0)
    {
        if (NewStream) AIL_close_stream(NewStream);
        else AIL_release_sample_handle(Sample);
        Sound_Free(i_System, Source);
        ErrorHandler(eOutOfHandleMem, SoundId, soundname[0] ? soundname : "Silence");
        return 0;
    }

    Handle Sound = Handle_Allocate(sizeof(SoundInstance));
    SoundInstance* pSound = (SoundInstance*)Handle_Get(Sound);
    if (pSound == 0) 
        goto start_out_of_mem;

    AIL_memset(pSound, 0, sizeof(SoundInstance));

    //
    // Parse the init block.
    //
    {
        pSound->m_UserBufferOffset = -1;
        char* buffer = (char*)i_InitBlock;
        char* buffer_end = (char*)buffer + i_InitBlockLen;
        U32 buffer_token = *(U32*)buffer;

        while (buffer_token && buffer < buffer_end)
        {
            U32 advance_amt = *(U32*)(buffer + 4);

            switch (buffer_token)
            {
            case BUFFER_POSITION:   Sample->S3D.position.x = *(F32*)(buffer + 8); Sample->S3D.position.y = *(F32*)(buffer + 12); Sample->S3D.position.z = *(F32*)(buffer + 16); break;
            case BUFFER_VELOCITY:   Sample->S3D.velocity.x = *(F32*)(buffer + 8)*(*(F32*)(buffer + 20)); Sample->S3D.velocity.y = *(F32*)(buffer + 12)*(*(F32*)(buffer + 20)); Sample->S3D.velocity.z = *(F32*)(buffer + 16)*(*(F32*)(buffer + 20)); break;
            case BUFFER_USER:       pSound->m_UserBufferOffset = (S32)(buffer - (char*)i_InitBlock); break;
            case BUFFER_VAR:        VarEntry* V = (VarEntry*)buffer; Var_SetInstance(&pSound->m_VarRoot, V->name, &V->value, 1); break;
            }

            buffer += advance_amt;
            if (buffer < buffer_end)
                buffer_token = *(U32*)buffer;
            else 
                break;
        }

        // Copy the init block.
        if (Handle_AllocBuffer(&pSound->m_InitializationBlock, i_InitBlock, i_InitBlockLen) == 0)
            goto start_out_of_mem;
        pSound->m_InitBlockLen = i_InitBlockLen;
    }

    if (Handle_AllocString(&pSound->m_Name, &pSound->m_NameLen, soundname) == 0)
        goto start_out_of_mem;

    if (Handle_AllocString(&pSound->m_Labels, 0, i_Step->start.labels.str) == 0)
        goto start_out_of_mem;

    if (i_Step->start.eventname.len)
    {
        if (Handle_AllocString(&pSound->m_CompletionEvent, 0, i_Step->start.eventname.str) == 0)
            goto start_out_of_mem;
    }

    // do we need to allocate a dynamic preset?
    if (i_Step->start.presetname.len && i_Step->start.presetisdynamic)
    {
        if (Handle_AllocString(&pSound->m_DynamicPreset, 0, i_Step->start.presetname.str) == 0)
            goto start_out_of_mem;
    }

    //
    // If we are a blended sound, add us to the blend list.
    //
    if (SELECT_TYPE(i_Step) == MILES_START_STEP_BLENDED)
    {
        BlendedSound* pBlend = 0;
        // Find existing.
        pBlend = (BlendedSound*)Handle_Get(i_System->BlendRoot);
        for (; pBlend; pBlend = (BlendedSound*)Handle_Get(pBlend->Next))
        {
            if (AIL_stricmp(pBlend->Name, i_Step->start.statevar.str) == 0) break;
        }

        if (pBlend == 0)
        {
            S32 Size = i_Step->start.statevar.len + sizeof(BlendedSound);
            Handle New = Handle_Allocate(Size);
            pBlend = (BlendedSound*)Handle_Get(New);
            if (pBlend == 0)
                goto start_out_of_mem;

            AIL_memset(pBlend, 0, sizeof(BlendedSound));
            AIL_strcpy(pBlend->Name, i_Step->start.statevar.str);
            pBlend->Self = New;

            // Add to list.
            pBlend->Next = i_System->BlendRoot;
            i_System->BlendRoot = New;
        }

        pSound->m_Blend = pBlend->Self;
    }

    // All allocations successfull.
    //-------------------------------------

    // Setup the instance data.
    pSound->m_Sample = Sample;
    pSound->m_Flags = Flags;
    pSound->m_Stream = NewStream;
    pSound->m_Status = MILESEVENT_SOUND_STATUS_PENDING;
    pSound->m_System = i_System;
    pSound->m_Source = Source;
    pSound->m_EventId = i_EventId;
    pSound->m_ParentId = i_ParentId;
    pSound->m_Priority = i_Step->start.priority;
    pSound->m_SoundId = SoundId;
    pSound->m_StartTime = AIL_ms_count();
    pSound->m_FadeVolume = 1;
    if (i_Step->start.fadeintime > 0.1f)
    {
        pSound->m_FadeVolume = 0;
        pSound->m_FadeDeltaPerSecond = 1 / i_Step->start.fadeintime;
    }

    pSound->m_Pitch = UsePitch;
    pSound->m_Delay = UseDelay;
    pSound->m_Volume = UseVol;
    pSound->m_FadeDelay = (F32)UseDelay / 1000.0f;
    pSound->m_BlendIndex = i_Index;

    i_System->SoundInstances.Top() = Sound;

    Sample->system_data[EVT_HANDLE_INDEX] = (SINTa)Sound.index;
    Sample->system_data[EVT_HANDLE_MAGIC] = (SINTa)Sound.magic;

    // If we have any initial variable state, set that up before doing any variable checks.
    if (i_Step->start.varinit.len)
    {
        char const* srcstr = i_Step->start.varinit.str;
        while (*srcstr)
        {
            char const* varend = strchr(srcstr, ':');
            if (varend == 0) break;

            char varname[128];
            S32 varlen = (S32)(varend - srcstr);
            AIL_memcpy(varname, srcstr, varlen);
            varname[varlen] = 0;

            varend++;
            F32 varmin = atof_with_len(varend, 0);
            varend = strchr(varend, ':');
            if (varend == 0) break;

            varend++;
            F32 varmax = atof_with_len(varend, 0);

            F32 varuse = varmin;
            if (varmax - varmin > .001f) varuse += (varmax - varmin) * ( (F32) (HorribleRand() & 1023) / 1023.0f);

            Var_SetInstance(&pSound->m_VarRoot, varname, &varuse, 1);

            srcstr = strchr(varend + 1, ':');
            if (srcstr == 0) break;

            srcstr++;
        }
    } // end var init


    // All the special stuff we only do if we have an actual sound.
    // [That's a lie! we don't get delayed silence in that case! bugfix]
    {
        //
        // Check if a persisted preset applies to this sound.
        //
        Persist_Apply(i_System, pSound, Sample, i_Step->start.labels.str, i_Step->start.labels.len, 0);

        // Apply a preset to the sound if we have one.
        if (i_Step->start.presetname.len != 0)
        {
            void* PresetData = Container_GetPreset(i_Step->start.presetname.str);
            if (PresetData == 0)
            {
                ErrorHandler(eMissingAsset, SoundId, i_Step->start.presetname.str);
            }
            else
            {
                AIL_resolve_raw_sound_preset(PresetData, pSound, EvalFunc);
                AIL_apply_raw_sound_preset(Sample, PresetData);
            }
        }

        // If there are markers for the sound, make them accessible from the HSAMPLE
        Sample->hl_marker_list = (SINTa)Container_GetMarkerList(soundname);

        pSound->m_LoopEnd = -1;
        pSound->m_LoopStart = 0;

        // Find loop points
        if (i_Step->start.markerend.len)
        {
            S32 TimeMask = 0;
            if (FindMarker(i_System, pSound, Sample->hl_marker_list, i_Step->start.markerend.str, &pSound->m_LoopEnd, &TimeMask))
            {

                // Convert ms to samples
                if (TimeMask == TIME_MS_MASK)
                {
                    if (pSound->m_LoopEnd)
                        pSound->m_LoopEnd = mult64anddiv(pSound->m_LoopEnd, SoundInfo.Rate, 1000);
                    TimeMask = TIME_SAMPLES_MASK;
                }

                pSound->m_LoopLookup |= (TimeMask << LOOP_END_SHIFT);
            }
        }

        if (i_Step->start.markerstart.len)
        {
            S32 StartMarker = 0;
            S32 TimeMask = 0;
            if (FindMarker(i_System, pSound, Sample->hl_marker_list, i_Step->start.markerstart.str, &StartMarker, &TimeMask))
            {

                if (StartMarker == -1)
                    StartMarker = 0; // Invalid - force beginning of sample

                // Convert ms to samples
                if (TimeMask == TIME_MS_MASK)
                {
                    pSound->m_LoopStart = mult64anddiv(pSound->m_LoopStart, SoundInfo.Rate, 1000);
                    TimeMask = TIME_SAMPLES_MASK;
                }

                pSound->m_LoopStart = StartMarker;
                if (TimeMask)
                    pSound->m_LoopLookup |= (TimeMask << LOOP_START_SHIFT);
            }
        }

        // Check for an initial offset.
        if (i_Step->start.startoffset.len)
        {
            if (FindMarker(i_System, pSound, Sample->hl_marker_list, i_Step->start.startoffset.str, &pSound->m_StartOffset, &pSound->m_StartOffsetIsMS) )
            {
                if (pSound->m_StartOffsetIsMS == TIME_BYTES_MASK)
                    pSound->m_StartOffsetIsMS = 0;
                else if (pSound->m_StartOffsetIsMS == TIME_MS_MASK)
                    pSound->m_StartOffsetIsMS = 1;
                else if (pSound->m_StartOffsetIsMS == TIME_SAMPLES_MASK)
                {
                    pSound->m_StartOffsetIsMS = 1;
                    // We don't support samples in the start, so convert to ms. Rate is defined since
                    // sample markers are only with valid sounds, never silence.
                    pSound->m_StartOffset = mult64anddiv(pSound->m_StartOffset, 1000, SoundInfo.Rate);
                }
            }
        }

        // Setup the initial loop count.
        if (NewStream) 
            AIL_set_stream_loop_count(NewStream, i_Step->start.loopcount);
        else 
            AIL_set_sample_loop_count(Sample, i_Step->start.loopcount);

        // Set delay, vol, pitch
        AIL_set_sample_playback_delay(Sample, UseDelay);

        F32 l,r;
        AIL_sample_volume_levels( Sample, &l, &r );
        AIL_set_sample_volume_levels( Sample, l * UseVol, r * UseVol);

        F32 p;
        p = AIL_sample_playback_rate_factor( Sample );
        AIL_set_sample_playback_rate_factor( Sample, p * UsePitch );
    } // end if we have a sound.

    // only allocation left is for blends...



    float BlendVol, BlendPitch;
    Blend_Apply(pSound, &BlendVol, &BlendPitch);

    Ramp_ApplyAll(i_System, pSound, 0);

    // Update limits for this sound.
    {
        SoundLimit* Limit = (SoundLimit*)Handle_Get(i_System->LimitRoot);
        if (Limit)
        {
            U16 parsed_lbls[ 256 ];
            parse_string_list( parsed_lbls, i_Step->start.labels.str, i_Step->start.labels.len, ',', 0 );

            SoundLimitEntry* Entry = (SoundLimitEntry*)Handle_Get(Limit->Entries);
            for ( ; Entry ; Entry = (SoundLimitEntry*)Handle_Get(Entry->Next))
            {
                if (CheckLabelMatchLabelParsed(parsed_lbls, i_Step->start.labels.str, Entry->Label, 0xffffffff, 0))
                    Entry->Count++;
            }
        }
    }

    // Increment our usage of the sound source.
    {
        SoundSource* pSoundSource = (SoundSource*)Handle_Get(pSound->m_Source);
        if (pSoundSource) // streams don't have a source right now.
        {
            pSoundSource->m_SoundCount++;
        }
    }

    Audition_SoundFlags(SoundId, pSound->m_Flags);
    Audition_SoundBus(SoundId, BusIndex);

    return 1;

start_out_of_mem:
    if (pSound)
    {
        Handle_Free(pSound->m_Labels);
        Handle_Free(pSound->m_Name);
        Handle_Free(pSound->m_CompletionEvent);
        Handle_Free(pSound->m_InitializationBlock);
        Handle_Free(pSound->m_DynamicPreset);
    }
    Handle_Free(Sound);
    i_System->SoundInstances.Pop();

    ErrorHandler(eOutOfHandleMem, SoundId, soundname[0] ? soundname : "Silence");

    if (NewStream) AIL_close_stream(NewStream);
    else AIL_release_sample_handle(Sample);

    Sound_Free(i_System, Source);

    return 0;
} // end startsingle

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 ExecuteStartSound(
    const EVENT_STEP_INFO* i_Step, EventSystem* i_System, const U8* i_pCurrentEventLocation, 
    void* i_InitBlock, U32 i_InitBlockLen, U64 i_EventId, U64 i_ParentId)
{
    const char* pSoundToPlay = 0;
    S32 SoundNameLen = 0;

    if (SELECT_TYPE(i_Step) == MILES_START_STEP_BLENDED)
    {
        // Look up the blend and make sure it exists.
        if (Blend_Exists(i_System, i_Step->start.statevar.str) == 0)
        {
            ErrorHandler(eBlendDoesntExist, i_EventId, i_Step->start.statevar.str);
            return 0;
        }

        // Start all of the sounds.
        S8 Index = 0;
        const char* pRead = i_Step->start.soundname.str;
        while (*pRead)
        {
            const char* pName = pRead;

            // Check for empty entries, and seek to the weight
            if (*pRead != ':') pRead = strchr(pRead, ':') + 1;
            else pRead++;

            // Use this one.
            pSoundToPlay = pName;
            SoundNameLen = (S32)(pRead - pName - 1);

            if (Index >= MAX_BLEND_SOUNDS)
            {
                ErrorHandler(eTooManyBlends, i_EventId, 0);
                return 1;
            }

            if (StartSingle(i_Step, i_System, pSoundToPlay, SoundNameLen, i_InitBlock, i_InitBlockLen, i_EventId, i_ParentId, Index) == 0)
            {
                return 0;
            }

            Index++;
            pRead = strchr(pRead, ':') + 1;
        }

        return 1;
    } // end if blended.

    //
    // If there is a state var, check it
    //
    S32 ForceSoundIndex = -1;
    S32 DisallowSoundMask = 0;

    if (SELECT_TYPE(i_Step))
    {
        S32 State = 0;
        if (i_Step->start.statevar.len == 0)
        {
            // Use a var based on the event pointer
            State = StateVar_Lookup(i_System, i_pCurrentEventLocation);
        }
        else
        {
            Var_LookupWithContext(i_System, i_System->VarRoot, i_Step->start.statevar.str, 0, &State);
        }

        if (SELECT_TYPE(i_Step) == MILES_START_STEP_NO_REPEATS ||
            SELECT_TYPE(i_Step) == MILES_START_STEP_RANDOM_ALL_BEFORE_REPEAT)
        {
            DisallowSoundMask = State;
        }
        else if (SELECT_TYPE(i_Step) == MILES_START_STEP_IN_ORDER)
        {
            ForceSoundIndex = State;
        }
    }

    S32 TotalWeight = 0;
    S32 AdjustedWeight = 0;
    S32 SoundCount = 0;
    S32 DisallowedCount = 0;

    // Sum the weights.
    const char* pRead = i_Step->start.soundname.str;
    while (*pRead)
    {
        // Check for empty entries, and seek to the weight
        if (*pRead != ':')  pRead = strchr(pRead, ':') + 1;
        else pRead++;

        S32 Weight = strtoi(pRead, 0);

        TotalWeight += Weight;

        // Only include the weight if it isn't in our disallow mask.
        if (DisallowSoundMask & ( 1 << SoundCount) )
        {
            DisallowedCount++;
        }
        else
        {
            AdjustedWeight += Weight;
        }

        pRead = strchr(pRead, ':') + 1;
        SoundCount++;
    }

    if (ForceSoundIndex >= SoundCount) ForceSoundIndex = 0; // Wrap when forcing.
    if (DisallowedCount != SoundCount)
    {
        TotalWeight = AdjustedWeight;
    }
    else DisallowSoundMask = 0;

    S32 UsedSoundIndex = 0;
    if (TotalWeight)
    {
        S32 RandomValue = HorribleRand() % TotalWeight;

        // Find which entry to use.
        pRead = i_Step->start.soundname.str;
        while (*pRead)
        {
            const char* pName = pRead;

            // Check for empty entries, and seek to the weight
            if (*pRead != ':') pRead = strchr(pRead, ':') + 1;
            else pRead++;

            S32 UseThisSound = 0;

            if (UseThisSound == 0 && 
                (DisallowSoundMask & (1 << UsedSoundIndex)) == 0)
            {
                S32 Weight = strtoi(pRead, 0);
                if (RandomValue < Weight)
                {
                    UseThisSound = 1;
                }

                RandomValue -= Weight;
            }

            if (ForceSoundIndex != -1)
            {
                UseThisSound = ForceSoundIndex == UsedSoundIndex;
            }

            if (UseThisSound)
            {
                // Use this one.
                pSoundToPlay = pName;
                SoundNameLen = (S32)(pRead - pName - 1);
                break;
            }

            UsedSoundIndex++;
            pRead = strchr(pRead, ':') + 1;
        }
    }
    else
    {
        // We are just using the name straight up.
        pSoundToPlay = i_Step->start.soundname.str;
        if (pSoundToPlay[0])
        {
            SoundNameLen = (S32)(strchr(pSoundToPlay, ':') - pSoundToPlay);
        }
    }

    // Store the index we used in to the state var, if we have one.
    if (SELECT_TYPE(i_Step))
    {
        S32 State = UsedSoundIndex;

        if (SELECT_TYPE(i_Step) == MILES_START_STEP_RANDOM_ALL_BEFORE_REPEAT)
            State = DisallowSoundMask | (1 << UsedSoundIndex);
        else if (SELECT_TYPE(i_Step) == MILES_START_STEP_NO_REPEATS) 
            State = 1 << UsedSoundIndex;
        else if (SELECT_TYPE(i_Step) == MILES_START_STEP_IN_ORDER)
            State = ForceSoundIndex + 1;

        if (i_Step->start.statevar.len)
            MilesSetVarIInternal((UINTa)i_System, i_Step->start.statevar.str, State);
        else
            StateVar_Set(i_System, i_pCurrentEventLocation, State);
    }

    return StartSingle(i_Step, i_System, pSoundToPlay, SoundNameLen, i_InitBlock, i_InitBlockLen, i_EventId, i_ParentId, -1);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 ExecuteControlSounds(const EVENT_STEP_INFO* i_Step, EventSystem* i_System, U64 i_EventId, U64 i_Filter)
{
    //
    // Check any playing samples for name matches.
    //
    U16 parsed_query[ 256 ];
    parsed_query[ 0 ] = 0xffff;

    if (i_Step->control.labels.len == 5 &&
        strncmp(i_Step->control.labels.str, "_Bus", 4) == 0)
    {
        S32 BusIndex = (S32)(i_Step->control.labels.str[4] - '0');

        // Only presets matter...
        if (i_Step->control.presetname.len == 0)
            return 1;

        void* PresetData = Container_GetPreset(i_Step->control.presetname.str);

        if (PresetData == 0)
        {
            ErrorHandler(eMissingAsset, i_EventId, i_Step->control.presetname.str);
            return 1;
        }

        // Find the bus
        Bus* B = (Bus*)Handle_Get(i_System->BusRoot);
        for (; B; B = (Bus*)Handle_Get(B->Next))
        {
            if (B->Index == BusIndex)
            {
                s_VarContext = i_System;
                AIL_resolve_raw_sound_preset(PresetData, 0, EvalFunc);
                AIL_apply_raw_sound_preset(B->Sample, PresetData);

                // Update the name of the preset on the bus.
                Handle_Free(B->Preset);
                B->Preset = Handle_Allocate(i_Step->control.presetname.len + 1);
                char* S = (char*)Handle_Get(B->Preset);
                if (S == 0)
                    ErrorHandler(eOutOfHandleMem, i_EventId, i_Step->control.presetname.str);
                else
                    strcpy(S, i_Step->control.presetname.str);

                B->Dynamic = i_Step->control.presetapplytype != 0;
            }
            else
            {
                ErrorHandler(eMissingAsset, i_EventId, i_Step->control.presetname.str);
            }
        }

        return 1;
    } // end if Bus

    unsigned int SampleCount = i_System->SoundInstances.Count();
    for (unsigned int SampleIndex = 0; SampleIndex < SampleCount; SampleIndex++)
    {
        SoundInstance* pSound = (SoundInstance*)Handle_Get(i_System->SoundInstances[SampleIndex]);
        if (pSound->m_Status == MILESEVENT_SOUND_STATUS_COMPLETE) 
            continue;

        HSAMPLE Sample = pSound->m_Sample;

        // Check event filter.
        if (i_Filter)
        {
            if (pSound->m_EventId != i_Filter && 
                pSound->m_SoundId != i_Filter &&
                pSound->m_ParentId != i_Filter) continue;
        }

        S32 Match = !i_Step->control.labels.len; // no labels specified matches all sounds.

        // If it isn't applied to all, check labels.
        if (!Match)
        {
            char* pLabels = (char*)Handle_Get(pSound->m_Labels);

            if ( parsed_query[ 0 ] == 0xffff )
              parse_wildcard_query( parsed_query, i_Step->control.labels.str, i_Step->control.labels.len, ',', 0 );

            Match = CheckLabelMatchQueryParsed(pLabels, 0xffffffff, parsed_query, i_Step->control.labels.str );
        }

        if (Match)
        {
            if (i_Step->control.type == MILES_CONTROL_STEP_STOP)
            {
                // Stop.
                DestroySoundInstance(i_System, pSound);
                continue;
            }
            if (i_Step->control.type == MILES_CONTROL_STEP_STOP_NO_EVENTS)
            {
                DestroySoundInstance(i_System, pSound);
                Handle_Free(pSound->m_CompletionEvent);
                continue;
            }
            if (i_Step->control.type == MILES_CONTROL_STEP_STOP_FADE)
            {
                if (i_Step->control.fadeouttime < .1f)
                {
                    DestroySoundInstance(i_System, pSound);
                    continue;
                }

                pSound->m_FadeDeltaPerSecond = -1 / i_Step->control.fadeouttime;
                continue;
            }

            if (i_Step->control.type == MILES_CONTROL_STEP_PAUSE)
            {
                // Pause before controlling.
                if (pSound->m_Stream)
                    AIL_pause_stream(pSound->m_Stream, 1);
                else
                    AIL_stop_sample(Sample);
            }

            S32 LoopStart = -2;
            S32 LoopEnd = -2;
            S32 Position = -2;
            S32 EndMs = 0, StartMs = 0, PosMs = 0;

            if (i_Step->control.markerend.len)
            {
                if (FindMarker(i_System, pSound, Sample->hl_marker_list, i_Step->control.markerend.str, &LoopEnd, &EndMs) == 0)
                    LoopEnd = -2;
            }
            if (i_Step->control.markerstart.len)
            {
                if (FindMarker(i_System, pSound, Sample->hl_marker_list, i_Step->control.markerstart.str, &LoopStart, &StartMs) == 0)
                    LoopStart = -2;
            }

            if (i_Step->control.position.len)
            {
                if (FindMarker(i_System, pSound, Sample->hl_marker_list, i_Step->control.position.str, &Position, &PosMs) == 0)
                    Position = -2;
            }

            MILESBANKSOUNDINFO SoundInfo;
            SoundInfo.Rate = 0; // Init to avoid warning
            if (EndMs == TIME_MS_MASK || StartMs == TIME_MS_MASK || PosMs == TIME_SAMPLES_MASK)
                Container_GetSound((char*)Handle_Get(pSound->m_Name), 0, &SoundInfo);

            if (EndMs == TIME_MS_MASK)
            {
                EndMs = TIME_SAMPLES_MASK;
                LoopEnd = mult64anddiv(LoopEnd, SoundInfo.Rate, 1000);
            }
            if (StartMs == TIME_MS_MASK)
            {
                StartMs = TIME_SAMPLES_MASK;
                LoopStart = mult64anddiv(LoopStart, SoundInfo.Rate, 1000);
            }
            if (PosMs == TIME_SAMPLES_MASK)
            {
                PosMs = TIME_MS_MASK;
                Position = mult64anddiv(Position, 1000, SoundInfo.Rate);
            }

            if (pSound->m_Stream)
            {
                if (LoopStart != -2 || LoopEnd != -2)
                {
                    // Streams only support byte based loop points right now.
                    if (StartMs == TIME_BYTES_MASK && EndMs == TIME_BYTES_MASK)
                        AIL_set_stream_loop_block(pSound->m_Stream, LoopStart, LoopEnd);
                }
                if (Position != -2)
                {
                        if (PosMs == TIME_MS_MASK) 
                            AIL_set_stream_ms_position(pSound->m_Stream, Position);
                        else 
                            AIL_set_stream_position(pSound->m_Stream, Position);
                }

                if (i_Step->control.loopcount != MILES_CONTROL_STEP_IGNORELOOP)
                    AIL_set_stream_loop_count(pSound->m_Stream, i_Step->control.loopcount);
            }
            else
            {
                if (LoopStart != -2 || LoopEnd != -2)
                {
                    if (StartMs == TIME_BYTES_MASK && EndMs == TIME_BYTES_MASK)
                        AIL_set_sample_loop_block(Sample, LoopStart, LoopEnd);
                    else if (StartMs == TIME_BYTES_MASK && EndMs == TIME_SAMPLES_MASK)
                    {
                        AIL_set_sample_loop_samples(Sample, -2, LoopEnd);
                        AIL_set_sample_loop_block(Sample, LoopStart, -2);
                    }
                    else if (StartMs == TIME_SAMPLES_MASK && EndMs == TIME_BYTES_MASK)
                    {
                        AIL_set_sample_loop_samples(Sample, LoopStart, -2);
                        AIL_set_sample_loop_block(Sample, -2, LoopEnd);
                    }
                    else
                        AIL_set_sample_loop_samples(Sample, LoopStart, LoopEnd);
                }

                if (Position != -2)
                {
                    if (PosMs == TIME_MS_MASK)
                        AIL_set_sample_ms_position(Sample, Position);
                    else 
                        AIL_set_sample_position(Sample, Position);
                }

                if (i_Step->control.loopcount != MILES_CONTROL_STEP_IGNORELOOP)
                    AIL_set_sample_loop_count(Sample, i_Step->control.loopcount);
            }

            if (i_Step->control.presetname.len)
            {
                void* PresetData = Container_GetPreset(i_Step->control.presetname.str);
                if (PresetData)
                {
                    AIL_resolve_raw_sound_preset(PresetData, pSound, EvalFunc);
                    AIL_apply_raw_sound_preset(Sample, PresetData);

                    // If there is a dynamic preset in existence on the sound, replace it or remove it.
                    if (pSound->m_DynamicPreset.index)
                    {
                        Handle_Free(pSound->m_DynamicPreset);
                        pSound->m_DynamicPreset = Handle_Clear();
                    }

                    if (i_Step->control.presetapplytype)
                    {
                        // replace
                        pSound->m_DynamicPreset = Handle_Allocate(i_Step->control.presetname.len + 1);
                        char* str = (char*)Handle_Get(pSound->m_DynamicPreset);
                        if (str == 0)
                            ErrorHandler(eOutOfHandleMem, i_EventId, i_Step->control.presetname.str);
                        else    
                            strcpy(str, i_Step->control.presetname.str);
                        
                    }
                }
                else
                {
                    ErrorHandler(eMissingAsset, i_EventId, i_Step->control.presetname.str);
                }
            }

            if (i_Step->control.type == MILES_CONTROL_STEP_RESUME)
            {
                // Resume
                if (pSound->m_Stream)
                    AIL_pause_stream(pSound->m_Stream, 0);
                else
                    AIL_resume_sample(Sample);
            }

            // Do stuff
            continue;
        }
    }
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 ExecutePersistPreset(const EVENT_STEP_INFO* i_Step, U64 i_Id, EventSystem* i_System)
{
    if (i_Step->persist.name.len == 0) return 1; // ignore null names.

    if (i_Step->persist.presetname.len == 0)
    {
        // We are removing a preset.
        Persist_Free(i_System, i_Id, i_Step->persist.name.str);
        return 1;
    }

    return Persist_Create(i_System, i_Id, i_Step);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 ExecuteClearState(EventSystem* i_System, U64 i_EventId)
{
    // Persists
    for (;;)
    {
        PersistedPreset* Persist = (PersistedPreset*)Handle_Get(i_System->PersistRoot);
        if (Persist == 0)
            break;

        Persist_Free(i_System, i_EventId, Persist->Name);
    }
    
    // Blends
    for (;;)
    {
        BlendedSound* Blend = (BlendedSound*)Handle_Get(i_System->BlendRoot);
        if (Blend == 0)
            break;

        i_System->BlendRoot = Blend->Next;
        Handle_Free(Blend->Self);
    }

    // Ramps
    for (;;)
    {
        Ramp* Cur = (Ramp*)Handle_Get(i_System->RampRoot);
        if (Cur == 0)
            break;
        
        i_System->RampRoot = Cur->Next;
        Handle_Free(Cur->Self);
    }

    // Environment
    if (i_System->CurrentEnv.index)
    {
        void* EnvData = Container_GetEnvironment((char*)Handle_Get(i_System->CurrentEnv));
        if (EnvData)
        {
            AIL_unapply_raw_environment_preset(i_System->Driver, EnvData);
        }
        
        Handle_Free(i_System->CurrentEnv);
        i_System->CurrentEnv = Handle_Clear();
    }

    Audition_ClearState();

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 ExecuteCacheSounds(const EVENT_STEP_INFO* i_Step, EventSystem* i_System, U64 i_Id, S32 i_IsLoading)
{
    //
    // Now we need to load the sounds from the library.
    //
    S32 SoundCount = i_Step->load.namecount;
    const char** pNameList = i_Step->load.namelist;
    char Name[512];
    strcpy(Name, i_Step->load.lib.str);
    Name[i_Step->load.lib.len] = '/';
    char* pNameWrite = Name + i_Step->load.lib.len + 1;

    for (; SoundCount; pNameList++, SoundCount--)
    {
        strcpy(pNameWrite, *pNameList);
        Handle SourceHandle = Sound_Find(i_System, Name);
        SoundSource* pSound = (SoundSource*)Handle_Get(SourceHandle);
        if (pSound)
        {
            if (i_IsLoading)
            {
                // Already have it, increment refcount.
                pSound->m_RefCount++;

                // Check if the async needs to be reissued.
                if (pSound->m_AsyncFailed &&
                    pSound->m_AsyncIndex == 0)
                {
                    // Reissue the async, since the last one failed.
                    char soundfilename[MSS_MAX_PATH_BYTES];
                    MILESBANKSOUNDINFO SoundInfo;
                    if (Container_GetSound(Name, soundfilename, &SoundInfo) == 0)
                    {
                        ErrorHandler(eMissingAsset, i_Id, Name);
                        continue;
                    }

                    AIL_mem_free_lock(pSound->m_Data);
                    pSound->m_Data = 0;
                    pSound->m_AsyncFailed = 0;
                    pSound->m_AsyncIndex = 0;
                    pSound->m_AsyncQueueId = s_AsyncId++;
                    Audition_AsyncQueued(i_Id, pSound->m_AsyncQueueId, Name);

                    // See if we have a free async to kick it off.
                    for (S32 i = 0; i < MAX_ASYNCS; i++)
                    {
                        if (s_AsyncUsed[i] == 0)
                        {
                            AIL_memset(s_Asyncs + i, 0, sizeof(MilesAsyncRead));
                            strcpy(s_Asyncs[i].FileName, soundfilename);

                            Audition_AsyncLoad(pSound->m_AsyncQueueId, SoundInfo.DataLen);

                            if (SoundInfo.IsExternal)
                            {
                                // Allocations handled in the async system.
                                s_Asyncs[i].Count = -1;
                            }
                            else
                            {
                                pSound->m_Data = AIL_mem_alloc_lock(SoundInfo.DataLen);
                                if (pSound->m_Data == 0)
                                {
                                    ErrorHandler(eOutOfMem, i_Id, Name);

                                    // Move back to failed state, so maybe the next cache will get it.
                                    pSound->m_AsyncFailed = 1;

                                    break;
                                }

                                pSound->m_DataLen = SoundInfo.DataLen;

                                s_Asyncs[i].Count = pSound->m_DataLen;
                                s_Asyncs[i].Buffer = pSound->m_Data;
                            }

                            s_Asyncs[i].StreamUserData = (void*)pSound->m_AsyncQueueId;

                            MilesAsyncFileRead(s_Asyncs + i);
                            
                            s_AsyncUsed[i] = 1;
                            s_AsyncContext[i] = i_System;

                            pSound->m_AsyncIndex = i + 1;
                            break;
                        }
                    }

                    //
                    // Based on our async state, move us to the front.
                    //
                    // Very front are current outstanding asyncs.
                    //
                    S32 LimitIndex = i_System->SoundSources.Count() - 1;
                    for (S32 i = 0; i < LimitIndex; i++)
                    {
                        SoundSource* pSource = (SoundSource*)Handle_Get(i_System->SoundSources[i]);
                        if (pSource->m_AsyncIndex == 0)
                        {
                            // Swap here.
                            Handle Temp = i_System->SoundSources[i];
                            i_System->SoundSources[i] = i_System->SoundSources.Top();
                            i_System->SoundSources.Top() = Temp;
                            break;
                        }
                    }
                } // end if we need to reissue.
            } // end if loading.
            else
            {
                Sound_Free(i_System, SourceHandle);
            }
        }
        else
        {
            if (i_IsLoading == 0)
            {
                // Already gone!
                continue;
            }

            Sound_Load(i_System, i_Id, Name);
        } // end if need to load
    }

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 ExecuteApplyEnv(const EVENT_STEP_INFO* i_Step, U64 i_EventId, EventSystem* i_System)
{
    if (i_Step->env.envname.len == 0)
    {
        // Clear the current environment
        if (i_System->CurrentEnv.index)
        {
            void* EnvData = Container_GetEnvironment((char*)Handle_Get(i_System->CurrentEnv));
            if (EnvData)
            {
                AIL_unapply_raw_environment_preset(i_System->Driver, EnvData);
            }
            
            Handle_Free(i_System->CurrentEnv);
            i_System->CurrentEnv = Handle_Clear();
        }

        i_System->EnvIsDynamic = 0;
        return 1;
    }

    void* EnvData = Container_GetEnvironment(i_Step->env.envname.str);
    if (EnvData == 0)
    {
        ErrorHandler(eMissingAsset, i_EventId, i_Step->env.envname.str);
        return 0;
    }

    if (i_System->CurrentEnv.index)
    {
        Handle_Free(i_System->CurrentEnv);
        i_System->CurrentEnv = Handle_Clear();
    }

    // Save off the current env for unapply.
    i_System->CurrentEnv = Handle_Allocate(i_Step->env.envname.len + 1);
    char* str = (char*)Handle_Get(i_System->CurrentEnv);
    if (str == 0)
    {
        ErrorHandler(eOutOfHandleMem, i_EventId, i_Step->env.envname.str);
        return 0;
    }

    strcpy(str, i_Step->env.envname.str);

    i_System->EnvIsDynamic = !!i_Step->env.isdynamic;

    s_VarContext = i_System;
    AIL_resolve_raw_environment_preset(EnvData, EvalFunc);
    return AIL_apply_raw_environment_preset(i_System->Driver, EnvData);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 ExecuteMoveVar(EVENT_STEP_INFO const* i_Step, EventSystem* i_System)
{
    MoveVar* V = (MoveVar*)Handle_Get(i_System->MoveRoot);
    for (; V; V = (MoveVar*)Handle_Get(V->Next))
    {
        if (AIL_stricmp(V->Name, i_Step->movevar.name.str) == 0)
        {
            // Replace.
            V->CurrentTime = 0;

            AIL_memcpy(V->InterpTypes, i_Step->movevar.interpolate_type, 8);
            AIL_memcpy(V->Values, i_Step->movevar.value, 12);
            AIL_memcpy(V->Times, i_Step->movevar.time, 8);
            return 1;
        }
    }

    S32 Size = sizeof(MoveVar) + i_Step->movevar.name.len + 1;
    Handle New = Handle_Allocate(Size);
    V = (MoveVar*)Handle_Get(New);
    if (V == 0)
        return 0; // oom

    AIL_memcpy(V->Name, i_Step->movevar.name.str, i_Step->movevar.name.len + 1);

    V->Self = New;
    V->Next = i_System->MoveRoot;
    i_System->MoveRoot = New;

    AIL_memcpy(V->InterpTypes, i_Step->movevar.interpolate_type, 8);
    AIL_memcpy(V->Values, i_Step->movevar.value, 12);
    AIL_memcpy(V->Times, i_Step->movevar.time, 8);

    V->CurrentTime = 0;

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 ExecuteApplyRamp(EVENT_STEP_INFO const* i_Step, EventSystem* i_System, U64 i_EventId, U64 i_Filter)
{
    //
    // If we apply to new sounds as well as old, we just stay in the global list
    // and the iteration will handle us.
    //
    if (i_Filter == 0 && i_Step->ramp.apply_to_new == 1)
    {
        Audition_AddRamp(0, i_Step->ramp.type, i_Step->ramp.name.str, i_Step->ramp.labels.str, i_EventId);
        return Ramp_Add(&i_System->RampRoot, i_System, i_Step);
    }
    
    //
    // if we don't apply to new, place in internal instance ramp list.
    // if we have a filter, place in internal instance ramp list.
    //
    U16 parsed_query[ 256 ];
    parsed_query[ 0 ] = 0xffff;

    unsigned int SampleCount = i_System->SoundInstances.Count();
    for (unsigned int SampleIndex = 0; SampleIndex < SampleCount; SampleIndex++)
    {
        SoundInstance* pSound = (SoundInstance*)Handle_Get(i_System->SoundInstances[SampleIndex]);
        if (pSound->m_Status == MILESEVENT_SOUND_STATUS_COMPLETE) continue;

        // Check event filter.
        if (i_Filter)
        {
            if (pSound->m_EventId != i_Filter && 
                pSound->m_SoundId != i_Filter &&
                pSound->m_ParentId != i_Filter) continue;
        }

        char* pLabels = (char*)Handle_Get(pSound->m_Labels);

        if ( parsed_query[ 0 ] == 0xffff )
          parse_wildcard_query( parsed_query, i_Step->ramp.labels.str, i_Step->ramp.labels.len, ',', 0 );

        if (CheckLabelMatchQueryParsed(pLabels, 0xffffffff, parsed_query, i_Step->ramp.labels.str ))
        {
            Audition_AddRamp(pSound->m_SoundId, i_Step->ramp.type, i_Step->ramp.name.str, i_Step->ramp.labels.str, i_EventId);
            if (Ramp_Add(&pSound->m_RampRoot, i_System, i_Step) == 0)
            {
                return 0;
            }
        }
    }

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void AILCALL MilesRequeueAsyncsInternal()
{
    for (EventSystem* CurrentSystem = s_SystemRoot; CurrentSystem; CurrentSystem = CurrentSystem->Next)
    {
        // Check for any failed asyncs.
        S32 SourceCount = CurrentSystem->SoundSources.Count();
        for (S32 SourceIndex = 0; SourceIndex < SourceCount; SourceIndex++)
        {
            SoundSource* pSource = (SoundSource*)Handle_Get(CurrentSystem->SoundSources[SourceIndex]);
            if (pSource->m_AsyncFailed == 0 ||
                pSource->m_AsyncIndex)
            {
                continue;
            }

            char* Name = (char*)Handle_Get(pSource->m_Name);

            // Reissue the async, since the last one failed.
            char soundfilename[MSS_MAX_PATH_BYTES];
            MILESBANKSOUNDINFO SoundInfo;
            if (Container_GetSound(Name, soundfilename, &SoundInfo) == 0)
            {
                ErrorHandler(eMissingAsset, 0, Name);
                continue;
            }

            AIL_mem_free_lock(pSource->m_Data);
            pSource->m_Data = 0;
            pSource->m_AsyncFailed = 0;
            pSource->m_AsyncIndex = 0;
            pSource->m_AsyncQueueId = s_AsyncId++;
            Audition_AsyncQueued(0, pSource->m_AsyncQueueId, Name);

            // See if we have a free async to kick it off.
            for (S32 i = 0; i < MAX_ASYNCS; i++)
            {
                if (s_AsyncUsed[i] == 0)
                {
                    AIL_memset(s_Asyncs + i, 0, sizeof(MilesAsyncRead));
                    strcpy(s_Asyncs[i].FileName, soundfilename);

                    Audition_AsyncLoad(pSource->m_AsyncQueueId, SoundInfo.DataLen);

                    if (SoundInfo.IsExternal)
                    {
                        // Allocations handled in the async system.
                        s_Asyncs[i].Count = -1;
                    }
                    else
                    {
                        pSource->m_Data = AIL_mem_alloc_lock(SoundInfo.DataLen);
                        if (pSource->m_Data == 0)
                        {
                            ErrorHandler(eOutOfMem, 0, Name);

                            // Move back to failed state, so maybe the next cache will get it.
                            pSource->m_AsyncFailed = 1;

                            break;
                        }

                        pSource->m_DataLen = SoundInfo.DataLen;

                        s_Asyncs[i].Count = pSource->m_DataLen;
                        s_Asyncs[i].Buffer = pSource->m_Data;
                    }

                    s_Asyncs[i].StreamUserData = (void*)pSource->m_AsyncQueueId;

                    MilesAsyncFileRead(s_Asyncs + i);

                    s_AsyncUsed[i] = 1;
                    s_AsyncContext[i] = CurrentSystem;

                    pSource->m_AsyncIndex = i + 1;
                    break;
                }
            }

            //
            // Based on our async state, move us to the front.
            //
            // Very front are current outstanding asyncs.
            //
            S32 LimitIndex = CurrentSystem->SoundSources.Count() - 1;
            for (S32 i = 0; i < LimitIndex; i++)
            {
                SoundSource* pSource = (SoundSource*)Handle_Get(CurrentSystem->SoundSources[i]);
                if (pSource->m_AsyncIndex == 0)
                {
                    // Swap here.
                    Handle Temp = CurrentSystem->SoundSources[i];
                    CurrentSystem->SoundSources[i] = CurrentSystem->SoundSources.Top();
                    CurrentSystem->SoundSources.Top() = Temp;
                    break;
                }
            }

            // We always move forward, so we don't need to correct the index.
        }
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" char* AILCALL MilesTextDumpEventSystemInternal()
{
    S32 SystemCount = 0;
    for (EventSystem* System = s_SystemRoot; System; System = System->Next, SystemCount++) {}

    StringBuilder Out;

    Out.AppendFormat("Event System Count: %d\n", SystemCount);
    
    SystemCount = 0;
    for (EventSystem* System = s_SystemRoot; System; System = System->Next)
    {
        Out.AppendFormat("System #%d\n", SystemCount + 1);

        Out.AppendFormat("Sound Source Count: %d\n", System->SoundSources.Count());
        Out.AppendFormat("Sound Instance Count: %d\n", System->SoundInstances.Count());

        S32 PersistCount = 0;
        PersistedPreset* Persist = (PersistedPreset*)Handle_Get(System->PersistRoot);
        for (; Persist; PersistCount++, Persist = (PersistedPreset*)Handle_Get(Persist->Next)) {}

        Out.AppendFormat("Persistent Preset Count: %d\n", PersistCount);

        Out.Append("Source Dump:\n");

        for (S32 SoundIndex = 0; SoundIndex < System->SoundSources.Count(); SoundIndex++)
        {
            SoundSource* pSource = (SoundSource*)Handle_Get(System->SoundSources[SoundIndex]);
            char* pName = (char*)Handle_Get(pSource->m_Name);
            Out.AppendFormat("\t%s: refcount: %d\n", pName, pSource->m_RefCount);
        }

        Out.Append("Current Instances:\n");

        for (S32 SoundIndex = 0; SoundIndex < System->SoundInstances.Count(); SoundIndex++)
        {
            SoundInstance* pInstance = (SoundInstance*)Handle_Get(System->SoundInstances[SoundIndex]);
            char* pName = (char*)Handle_Get(pInstance->m_Name);
            char* pLabels = (char*)Handle_Get(pInstance->m_Labels);
            char const * pStatus = "playing";
            if (pInstance->m_Status == MILESEVENT_SOUND_STATUS_PENDING) pStatus = "pending";
            else if (pInstance->m_Status == MILESEVENT_SOUND_STATUS_COMPLETE) pStatus = "complete";

            Out.AppendFormat("\t%s, labeled (%s) - status %s\n", pName, pLabels, pStatus);
        }

        Out.Append("Current Persists:\n");

        Persist = (PersistedPreset*)Handle_Get(System->PersistRoot);
        for (; Persist; Persist = (PersistedPreset*)Handle_Get(Persist->Next)) {}
        {
            char* pPreset = (char*)Handle_Get(Persist->Preset);
            Out.AppendFormat("\t%s - %s\n", Persist->Name, pPreset);
        }
    }

    Out.AppendFormat("Loaded Bank Count: %d\n", Container_GetLoadedCount());
    Out.Append("Bank Dump:\n");

    for (S32 BankIndex = 0; BankIndex < Container_GetLoadedCount(); BankIndex++)
    {   
        Out.AppendFormat("\t%s\n", Container_GetLoadedName(BankIndex));
    }

    char* ret = Out.Str();
    Out.Relinquish();
    return ret;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesGetEventLength(char const* i_EventName)
{
    void* Event = Container_GetEvent(i_EventName);
    if (Event == 0)
        return 0;

    const U8* EventSource = (const U8*)Event;

    char EventBuffer[MAX_EVENT_SIZE];
    EVENT_STEP_INFO* Step = 0;

    AIL_set_error(0);
    while (EventSource)
    {
        Step = 0;
        EventSource = AIL_next_event_step(EventSource, &Step, EventBuffer, MAX_EVENT_SIZE);

        // Out of start sound steps? Out of mem?
        if (Step == 0)
            return 0;

        if (Step->type != EVENT_STEPTYPE_STARTSOUND)
            continue;

        // Find the first sound.
        S32 SoundLen = (S32)(strchr(Step->start.soundname.str, ':') - Step->start.soundname.str);

        AIL_memmove(EventBuffer, Step->start.soundname.str, SoundLen);
        EventBuffer[SoundLen] = 0;

        MILESBANKSOUNDINFO Info;
        Container_GetSound(EventBuffer, 0, &Info);

        return Info.DurationMs;
    }

    return 0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" const U8* AILCALL
MilesFindEventInternal(HMSOUNDBANK i_Bank, const char* i_EventName)
{
    //
    // Check if the bank is an audition bank.
    //
    S32 IsAuditioning = Audition_Status();
    U8 const* pEvent = 0;

    //
    // If we are auditioning, reroute the check through the networking layer.
    // Only reroute if we are doing a full audition.
    if (IsAuditioning == 2)
    {
        //
        // This is a little backwards because the default container
        // logic calls *us*, however since the user code will most
        // likely also call us, we have to reroute back out to the
        // container, since thats where the auditioner intercepts.
        //
        pEvent = (U8*)Container_GetEvent(i_EventName);
    }
    else
    {
        // Normal path, just return the event.
        if (i_Bank)
        {
            AIL_get_event_contents(i_Bank, i_EventName, &pEvent);
        }
    }

    return pEvent;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesGetEventSystemState(HEVENTSYSTEM i_System, MILESEVENTSTATE* o_State)
{
    EventSystem* System = (EventSystem*)i_System;

    if (o_State == 0) return;

    //! \todo get cmb buffer size
    o_State->CommandBufferSize = 0;
    o_State->LoadedBankCount = Container_GetLoadedCount();
    o_State->LoadedSoundCount = System->SoundSources.Count();
    o_State->PlayingSoundCount = System->SoundInstances.Count();

    Handle_MemStats(&o_State->HeapSize, &o_State->HeapRemaining);

    o_State->SoundDataMemory = System->SoundDataSize;
    o_State->SoundBankManagementMemory = Container_GetMemUsage();

    S32 PersistCount = 0;
    PersistedPreset* Persist = (PersistedPreset*)Handle_Get(System->PersistRoot);
    for (; Persist; PersistCount++, Persist = (PersistedPreset*)Handle_Get(Persist->Next)) {}

    o_State->PersistCount = PersistCount;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF U64 AILEXPORT MilesStopSoundInstances(const char* i_Labels, U64 i_Filter)
{
    HMSSEVENTCONSTRUCT hEvent = AIL_create_event();
    AIL_add_control_sounds_event_step(hEvent, i_Labels, 0, 0, 0, 0, 0, 0, MILES_CONTROL_STEP_IGNORELOOP, MILES_CONTROL_STEP_STOP);
    return MilesEnqueueEvent(AIL_close_event(hEvent), 0, 0, MILESEVENT_ENQUEUE_FREE_EVENT | 0x8, i_Filter);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF U64 AILEXPORT MilesPauseSoundInstances(const char* i_Labels, U64 i_Filter)
{
    HMSSEVENTCONSTRUCT hEvent = AIL_create_event();
    AIL_add_control_sounds_event_step(hEvent, i_Labels, 0, 0, 0, 0, 0, 0, MILES_CONTROL_STEP_IGNORELOOP, MILES_CONTROL_STEP_PAUSE);
    return MilesEnqueueEvent(AIL_close_event(hEvent), 0, 0, MILESEVENT_ENQUEUE_FREE_EVENT | 0x8, i_Filter);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF U64 AILEXPORT MilesResumeSoundInstances(const char* i_Labels, U64 i_Filter)
{
    HMSSEVENTCONSTRUCT hEvent = AIL_create_event();
    AIL_add_control_sounds_event_step(hEvent, i_Labels, 0, 0, 0, 0, 0, 0, MILES_CONTROL_STEP_IGNORELOOP, MILES_CONTROL_STEP_RESUME);
    return MilesEnqueueEvent(AIL_close_event(hEvent), 0, 0, MILESEVENT_ENQUEUE_FREE_EVENT | 0x8, i_Filter);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF U64 AILEXPORT MilesStartSoundInstance(
    HMSOUNDBANK i_SoundBank, const char* i_SoundName, U8 i_LoopCount,
    S32 i_Stream, const char* i_Labels,
    void* i_UserBuffer, S32 i_UserBufferLen, S32 i_UserBufferFlags
    )
{
    const char* pBankFileName = AIL_get_soundbank_name(i_SoundBank);

    char SoundName[256];
    AIL_strcpy(SoundName, pBankFileName);
    AIL_strcat(SoundName, "/");
    AIL_strcat(SoundName, i_SoundName);
    AIL_strcat(SoundName, ":0:");

    HMSSEVENTCONSTRUCT hEvent = AIL_create_event();
    AIL_add_start_sound_event_step(hEvent, SoundName, 0, 0, 0, 0, 0, 0, 0,
        i_Labels, i_Stream, 1, 0, 0, 128, i_LoopCount, 0, 1.0f, 1.0f, 1.0f, 1.0f, 0, MILES_START_STEP_PRIORITY, 0);

    return MilesEnqueueEvent(
        AIL_close_event(hEvent),
        i_UserBuffer,
        i_UserBufferLen,
        MILESEVENT_ENQUEUE_FREE_EVENT | (i_UserBufferFlags & MILESEVENT_ENQUEUE_BUFFER_PTR),
        0);
}

#endif


#ifdef TEST

static
S32
CheckLabelMatch(
    char const * labels, U32 labels_len, char label_terminator,
    char const * query, U32 query_len
    )
{
  U8 parsed_lbls[ 512 ];
  U8 parsed_qry[ 512 ];

  parse_string_list( parsed_lbls, labels, labels_len, ',', label_terminator );
  parse_wildcard_query( parsed_qry, query, query_len, ',', 0 );
  return wild_card_match_list_to_query( parsed_lbls, labels, parsed_qry, query );
}


int main(int , char** )
{
  char l[ 512];
  strcpy( l, "abcdefghijklmnopqrstuvwxyz,0123456789,!@#$%^&*()" );
  for(;;)
  {
    for(;;)
    {
      char q[512];
      printf("Against %s\nquery: ",l );
      gets( q );

      {
        U8 pq[512];
        char o[512];
        parse_wildcard_query( pq, q, 0xffffffff, ',', 0 );
        unparse_wild_card_match_query( o, pq, q );
        printf("Parsed: %s\n",o);
      }

      if ( CheckLabelMatch( l, strlen( l ), 0, q, strlen( q ) ) )
        printf("Matched!\n");
      else
        printf("Failed to match!\n");
      if ( CheckLabelMatch( l, 0xffffffff, 0, q, strlen( q ) ) )
        printf("Matched!\n");
      else
        printf("Failed to match!\n");
      if ( CheckLabelMatch( l, strlen( l ), 0, q, 0xffffffff ) )
        printf("Matched!\n");
      else
        printf("Failed to match!\n");
      if ( CheckLabelMatch( l, 0xffffffff, 0, q, 0xffffffff ) )
        printf("Matched!\n");
      else
        printf("Failed to match!\n");

      if ( q[0] == 0 )
        break;
    }
    printf("labels: " );
    gets( l );
  }
}

#endif

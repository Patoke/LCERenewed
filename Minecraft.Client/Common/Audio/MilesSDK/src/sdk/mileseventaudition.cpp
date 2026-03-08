//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#include "mss.h"
#include "imssapi.h"

static MILESAUDITIONFUNCTIONS s_Functions;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILCALL MilesEventSetAuditionFunctions(MILESAUDITIONFUNCTIONS const* i_Functions)
{
    s_Functions = *i_Functions;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 Audition_Status()
{
    if (s_Functions.Status) return s_Functions.Status();
    return 0; // by default auditioning isn't on.
}

S32 Audition_Pump() { if (s_Functions.Pump) return s_Functions.Pump(); return 0; }
void* Audition_OpenBank(char const* i_FileName) { if (s_Functions.OpenBank) return s_Functions.OpenBank(i_FileName); return 0; }
S32 Audition_OpenComplete(void* i_Bank) { if (s_Functions.OpenComplete) return s_Functions.OpenComplete(i_Bank); return 0; }
void Audition_CloseBank(void* i_Bank) { if (s_Functions.CloseBank) s_Functions.CloseBank(i_Bank); }

void Audition_Suppress(S32 i_IsSuppressed) { if (s_Functions.Suppress) s_Functions.Suppress(i_IsSuppressed); }
void Audition_FrameStart() { if (s_Functions.FrameStart) s_Functions.FrameStart(); }
void Audition_FrameEnd() { if (s_Functions.FrameEnd) s_Functions.FrameEnd(); }
void Audition_DefragStart() { if (s_Functions.DefragStart) s_Functions.DefragStart(); }
void Audition_SetBlend(U64 i_EventId, char const* i_Name) { if (s_Functions.SetBlend) s_Functions.SetBlend(i_EventId, i_Name); }
void Audition_SetPersist(U64 i_EventId, char const* i_Name, char const* i_Preset) { if (s_Functions.SetPersist) s_Functions.SetPersist(i_EventId, i_Name, i_Preset); }
void Audition_Event(char const* i_EventName, U64 i_EventId, U64 i_Filter, S32 i_Exists, void* i_InitBlock, S32 i_InitBlockLen) { if (s_Functions.Event) s_Functions.Event(i_EventName, i_EventId, i_Filter, i_Exists, i_InitBlock, i_InitBlockLen); }
void Audition_Sound(U64 i_EventId, U64 i_SoundId, char const* i_Sound, char const* i_Labels, float i_Volume, S32 i_Delay, float i_Pitch) { if (s_Functions.Sound) s_Functions.Sound(i_EventId, i_SoundId, i_Sound, i_Labels, i_Volume, i_Delay, i_Pitch); }
void Audition_SoundComplete(U64 i_SoundId) { if (s_Functions.SoundComplete) s_Functions.SoundComplete(i_SoundId); }
void Audition_SoundPlaying(U64 i_SoundId) { if (s_Functions.SoundPlaying) s_Functions.SoundPlaying(i_SoundId); }
void Audition_SoundFlags(U64 i_SoundId, S32 i_Flags) { if (s_Functions.SoundFlags) s_Functions.SoundFlags(i_SoundId, i_Flags); }
void Audition_SoundLimited(U64 i_SoundId, char const* i_Label) { if (s_Functions.SoundLimited) s_Functions.SoundLimited(i_SoundId, i_Label); }
void Audition_SoundEvicted(U64 i_SoundId, U64 i_ForSound, S32 i_Reason) { if (s_Functions.SoundEvicted) s_Functions.SoundEvicted(i_SoundId, i_ForSound, i_Reason); }
void Audition_Control(U64 i_EventId, char const* i_Labels, U8 i_ControlType, U64 i_Filter) { if (s_Functions.Control) s_Functions.Control(i_EventId, i_Labels, i_ControlType, i_Filter); }
void Audition_SoundBus(U64 i_SoundId, U8 i_BusIndex) { if (s_Functions.SoundBus) s_Functions.SoundBus(i_SoundId, i_BusIndex); }

void Audition_Error(U64 i_Id, char const* i_Details) { if (s_Functions.Error) s_Functions.Error(i_Id, i_Details); }

void Audition_AsyncQueued(U64 i_RelevantId, S32 i_AsyncId, char const* i_Asset) { if (s_Functions.AsyncQueued) s_Functions.AsyncQueued(i_RelevantId, i_AsyncId, i_Asset); }
void Audition_AsyncLoad(S32 i_AsyncId, S32 i_ExpectedData) { if (s_Functions.AsyncLoad) s_Functions.AsyncLoad(i_AsyncId, i_ExpectedData); }
void Audition_AsyncError(S32 i_AsyncId) { if (s_Functions.AsyncError) s_Functions.AsyncError(i_AsyncId); }
void Audition_AsyncComplete(S32 i_AsyncId, S32 i_DataLoaded) { if (s_Functions.AsyncComplete) s_Functions.AsyncComplete(i_AsyncId, i_DataLoaded); }
void Audition_AsyncCancel(S32 i_AsyncId) { if (s_Functions.AsyncCancel) s_Functions.AsyncCancel(i_AsyncId); }
void Audition_ListenerPosition(float x, float y, float z) { if (s_Functions.ListenerPosition) s_Functions.ListenerPosition(x, y, z); }
void Audition_SoundPosition(U64 i_Sound, float x, float y, float z) { if (s_Functions.SoundPosition) s_Functions.SoundPosition(i_Sound, x, y, z); }
void Audition_SendCPU(HDIGDRIVER i_Driver) { if (s_Functions.SendCPU) s_Functions.SendCPU(i_Driver); }
void Audition_UpdateDataCount(S32 i_CurrentDataLoaded) { if (s_Functions.UpdateDataCount) s_Functions.UpdateDataCount(i_CurrentDataLoaded); }
void Audition_SendCount(S32 i_Count) { if (s_Functions.SendCount) s_Functions.SendCount(i_Count); }
void Audition_HandleSystemLoad(S32 i_Avail, S32 i_Total) { if (s_Functions.HandleSystemLoad) s_Functions.HandleSystemLoad(i_Avail, i_Total); }
void Audition_VarState(char const* i_Var, U64 i_SoundId, S32 i_Int, void* i_4ByteValue) { if (s_Functions.VarState) s_Functions.VarState(i_Var, i_SoundId, i_Int, i_4ByteValue); }
void Audition_RampState(char const* i_Ramp, U64 i_SoundId, S32 i_Type, float i_Current) { if (s_Functions.RampState) s_Functions.RampState(i_Ramp, i_SoundId, i_Type, i_Current); }
void Audition_SoundState(U64 i_SoundId, float i_FinalVol, float i_3DVol, float i_BlendVol, float i_BlendPitch, float i_RampVol, float i_RampWet, float i_RampLp, float i_RampRate)
{ if (s_Functions.SoundState) s_Functions.SoundState(i_SoundId, i_FinalVol, i_3DVol, i_BlendVol, i_BlendPitch, i_RampVol, i_RampWet, i_RampLp, i_RampRate); }
void Audition_ClearState() { if (s_Functions.ClearState) s_Functions.ClearState(); }
void Audition_CompletionEvent(U64 i_CompletionEventId, U64 i_ParentSoundId) { if (s_Functions.CompletionEvent) s_Functions.CompletionEvent(i_CompletionEventId, i_ParentSoundId); }
void Audition_AddRamp(U64 i_ParentSoundId, S32 i_Type, char const* i_Name, char const* i_LabelQuery, U64 i_EventId) { if (s_Functions.AddRamp) s_Functions.AddRamp(i_ParentSoundId, i_Type, i_Name, i_LabelQuery, i_EventId); }



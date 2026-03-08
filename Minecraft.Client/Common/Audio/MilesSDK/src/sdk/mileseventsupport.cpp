#include "mss.h"
#include "imssapi.h"

#include "mileseventhandle.h"
#include "mileseventarray.h"
#include "mileseventinternal.h"


static AILEVENTERRORCB* s_EventErrorCB = 0;


//-----------------------------------------------------------------------------
//
// State Vars
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 StateVar_Lookup(EventSystem* System, void const* Key)
{
    Handle Cur = System->StateVarRoot;
    for (;;)
    {
        StateVar* S = (StateVar*)Handle_Get(Cur);
        if (S == 0)
            return 0;
        if (S->Key == Key)
            return S->Value;
        Cur = S->Next;
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void StateVar_Set(EventSystem* System, void const* Key, S32 Value)
{
    Handle H = System->StateVarRoot;
    for (;;)
    {
        StateVar* S = (StateVar*)Handle_Get(H);
        if (S == 0)
            break;
        if (S->Key == Key)
        {
            S->Value = Value;
            return;
        }
        H = S->Next;
    }

    H = Handle_Allocate(sizeof(StateVar));
    StateVar* S = (StateVar*)Handle_Get(H);
    if (S == 0)
        return;

    S->Self = H;
    S->Next = System->StateVarRoot;
    S->Key = Key;
    S->Value = Value;
}

//-----------------------------------------------------------------------------
//
// Blends
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 Blend_Exists(EventSystem* i_System, char const* i_Name)
{
    BlendedSound* pBlend = (BlendedSound*)Handle_Get(i_System->BlendRoot);
    for (; pBlend; pBlend = (BlendedSound*)Handle_Get(pBlend->Next))
    {
        if (AIL_stricmp(pBlend->Name, i_Name) == 0)
            return 1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Blend_Apply(SoundInstance* i_Sound, float* o_Volume, float* o_Pitch)
{
    *o_Volume = 1;
    *o_Pitch = 1;

    if (i_Sound->m_BlendIndex != -1)
    {
        BlendedSound* Blend = (BlendedSound*)Handle_Get(i_Sound->m_Blend);

        float BlendVar = 0;
        Var_LookupWithContext(i_Sound->m_System, i_Sound->m_VarRoot, Blend->Name, 1, &BlendVar);

        if (AIL_fabs(Blend->InMax[i_Sound->m_BlendIndex] - Blend->InMin[i_Sound->m_BlendIndex]) > .01f &&
            AIL_fabs(Blend->OutMax[i_Sound->m_BlendIndex] - Blend->OutMin[i_Sound->m_BlendIndex]) > .01f)
        {
            float Volume = 1;

            if (BlendVar < Blend->InMax[i_Sound->m_BlendIndex])
            {
                Volume = (BlendVar - Blend->InMin[i_Sound->m_BlendIndex]) / (Blend->InMax[i_Sound->m_BlendIndex] - Blend->InMin[i_Sound->m_BlendIndex]);
            }
            else if (BlendVar > Blend->OutMin[i_Sound->m_BlendIndex])
            {
                Volume = 1 - (BlendVar - Blend->OutMin[i_Sound->m_BlendIndex]) / (Blend->OutMax[i_Sound->m_BlendIndex] - Blend->OutMin[i_Sound->m_BlendIndex]);
            }

            if (Volume < 0)
                Volume = 0;
            if (Volume > 1) 
                Volume = 1;

            Volume = pow(Volume, 10.0f / 6.0f);

            *o_Volume = Volume;

            AIL_set_sample_volume_levels(i_Sound->m_Sample, Volume, Volume);
            if (Volume > 0)
            {
                // Check for pitch adjustment
                if (AIL_fabs(Blend->MinP[i_Sound->m_BlendIndex] - Blend->MaxP[i_Sound->m_BlendIndex]) > .01f)
                {
                    float Pitch = (BlendVar - Blend->InMin[i_Sound->m_BlendIndex]) / (Blend->OutMax[i_Sound->m_BlendIndex] - Blend->InMin[i_Sound->m_BlendIndex]);
                    Pitch = Blend->MinP[i_Sound->m_BlendIndex] + Pitch * (Blend->MaxP[i_Sound->m_BlendIndex] - Blend->MinP[i_Sound->m_BlendIndex]);
                    Pitch = MSS_sT_to_scalar(Pitch);

                    *o_Pitch = Pitch;

                    AIL_set_sample_playback_rate_factor(i_Sound->m_Sample, Pitch);
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 Blend_Add(EventSystem* i_System, EVENT_STEP_INFO const* i_Step)
{
    BlendedSound* pBlend = (BlendedSound*)Handle_Get(i_System->BlendRoot);
    for (; pBlend; pBlend = (BlendedSound*)Handle_Get(pBlend->Next))
    {
        if (AIL_stricmp(pBlend->Name, i_Step->blend.name.str) == 0)
            break;
    }

    if (pBlend == 0)
    {
        S32 Size = sizeof(BlendedSound) + i_Step->blend.name.len;
        Handle New = Handle_Allocate(Size);
        pBlend = (BlendedSound*)Handle_Get(New);
        if (pBlend == 0)
            return 0;

        AIL_memcpy(pBlend->Name, i_Step->blend.name.str, i_Step->blend.name.len + 1);
        pBlend->Self = New;

        pBlend->Next = i_System->BlendRoot;
        i_System->BlendRoot = New;
    }

    S32 count = (S32)i_Step->blend.count;
    AIL_memcpy(pBlend->InMax, i_Step->blend.inmax, 4 * count);
    AIL_memcpy(pBlend->InMin, i_Step->blend.inmin, 4 * count);
    AIL_memcpy(pBlend->OutMax, i_Step->blend.outmax, 4 * count);
    AIL_memcpy(pBlend->OutMin, i_Step->blend.outmin, 4 * count);
    AIL_memcpy(pBlend->MinP, i_Step->blend.minp, 4 * count);
    AIL_memcpy(pBlend->MaxP, i_Step->blend.maxp, 4 * count);
    return 1;
}

//-----------------------------------------------------------------------------
//
// Ramps
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Ramp_ApplyAll(EventSystem* i_System, SoundInstance* i_Sound, F32 i_TimeDelta)
{
    //! \todo perf - mark sounds that are affected by ramps when ramps are applied/removed? Skip this whole section.

    char* SoundLabels = (char*)Handle_Get(i_Sound->m_Labels);

    SoundSource* Source = (SoundSource*)Handle_Get(i_Sound->m_Source);

    float FinalVolume = pow(i_Sound->m_FadeVolume, 10.0f / 6.0f) * (Source ? Source->m_SoundMixDAC : 1.0f);
    float FinalWet = 1.0f;
    float FinalLP = 96000;
    float FinalRate = 1.0f;


    //! \todo parse the labels up front.
    // Evaluate the global ramps that affect us.
    Ramp* pCurrent = (Ramp*)Handle_Get(i_System->RampRoot);
    for (; pCurrent; pCurrent = (Ramp*)Handle_Get(pCurrent->Next))
    {
        U16 parsed_query[ 256 ];
        parse_wildcard_query( parsed_query, pCurrent->Labels, 0xffffffff, ',', 0 );

        if (CheckLabelMatchQueryParsed( SoundLabels, 0xffffffff, parsed_query, pCurrent->Labels))
        {
            float Value = Ramp_GetCurrentValue(i_System, pCurrent, 1);
            switch (pCurrent->ValueType)
            {
            case RAMPTYPE_VOLUME:   FinalVolume *= Value; break;
            case RAMPTYPE_WET:      FinalWet *= Value; break;
            case RAMPTYPE_RATE:     FinalRate *= Value; break;
            case RAMPTYPE_LOWPASS:  FinalLP = Value; break;
            }
        }
    }

    // Evaluate the local ramps...
    pCurrent = (Ramp*)Handle_Get(i_Sound->m_RampRoot);
    for (; pCurrent; pCurrent = (Ramp*)Handle_Get(pCurrent->Next))
    {
        F32 Value = 0;

        pCurrent->CurrentTime += i_TimeDelta;
        if (pCurrent->CurrentTime >= pCurrent->TotalTime) 
        {
            pCurrent->CurrentTime = pCurrent->TotalTime;
        }

        Value = Ramp_GetCurrentValue(i_System, pCurrent, 1);

        Audition_RampState(pCurrent->Name, i_Sound->m_SoundId, pCurrent->ValueType, Value);

        switch (pCurrent->ValueType)
        {
        case RAMPTYPE_VOLUME:   FinalVolume *= Value; break;
        case RAMPTYPE_WET:      FinalWet *= Value; break;
        case RAMPTYPE_RATE:     FinalRate *= Value; break;
        case RAMPTYPE_LOWPASS:  FinalLP = Value; break;
        }
    }

    //need to test different low pass routes taking the minimum.

    if (AIL_fabs(i_Sound->m_Sample->extra_rate - FinalRate) > .00001f ||
        AIL_fabs(i_Sound->m_Sample->extra_lp - FinalLP) > .00001f)
    {
        i_Sound->m_Sample->low_pass_changed = ~0U;
    }

    i_Sound->m_Sample->extra_volume = FinalVolume;
    i_Sound->m_Sample->extra_wet = FinalWet;
    i_Sound->m_Sample->extra_lp = FinalLP;
    i_Sound->m_Sample->extra_rate = FinalRate;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 Ramp_Add(Handle* i_Root, EventSystem* i_System, EVENT_STEP_INFO const* i_Step)
{
    // Check for an existing matching ramp.
    Ramp* pCurrent = (Ramp*)Handle_Get(*i_Root);
    for (; pCurrent; pCurrent = (Ramp*)Handle_Get(pCurrent->Next))
    {
        if (AIL_stricmp(pCurrent->Name, i_Step->ramp.name.str))
            continue;
        break;
    }

    if (pCurrent == 0)
    {
        Handle NewRamp = Handle_Allocate(sizeof(Ramp));
        pCurrent = (Ramp*)Handle_Get(NewRamp);
        if (pCurrent == 0)
        {
            // oom
            return 0;
        }

        if (i_Step->ramp.name.len >= 64 ||
            i_Step->ramp.labels.len >= 256 ||
            i_Step->ramp.target.len >= 64)
        {
            Handle_Free(NewRamp);
            return 0;
        }

        pCurrent->Self = NewRamp;
        memcpy(pCurrent->Name, i_Step->ramp.name.str, i_Step->ramp.name.len + 1);
        memcpy(pCurrent->Labels, i_Step->ramp.labels.str, i_Step->ramp.labels.len + 1);
        memcpy(pCurrent->Target, i_Step->ramp.target.str, i_Step->ramp.target.len + 1);

        // Grab the initial value of the ramp.
        F32 InitialValue = 0;
        {
            // If it is numerical, atof it.
            if ((i_Step->ramp.target.str[0] >= '0' && i_Step->ramp.target.str[0] <= '9') || i_Step->ramp.target.str[0] == '-' || i_Step->ramp.target.str[0] == '.')
                InitialValue = atof_with_len(i_Step->ramp.target.str, 0);
            else
            {
                switch (i_Step->ramp.type)
                {
                case RAMPTYPE_LOWPASS:  InitialValue = 24000; break;
                case RAMPTYPE_RATE:     InitialValue = 0; break;
                case RAMPTYPE_WET:
                case RAMPTYPE_VOLUME:   InitialValue = 1; break;
                }
            }

            // If we are dealing with dB, convert to LinLoud
            if (i_Step->ramp.type == RAMPTYPE_VOLUME ||
                i_Step->ramp.type == RAMPTYPE_WET)
            {
                if (InitialValue <= -96.0f)
                    InitialValue = 0;
                else
                {
                    InitialValue = MSS_dB_to_scalar(InitialValue);
                    InitialValue = pow(InitialValue, 6.0f/10.0f);
                }
            }
        }


        pCurrent->StartValue = InitialValue;
        pCurrent->ValueType = i_Step->ramp.type;
        pCurrent->Next = *i_Root;
        *i_Root = NewRamp;
    }
    else
    {
        pCurrent->StartValue = Ramp_GetCurrentValue(i_System, pCurrent, 0);
    }

    if (i_Step->ramp.target.len < 64)
    {
        memcpy(pCurrent->Target, i_Step->ramp.target.str, i_Step->ramp.target.len + 1);
    }
    
    pCurrent->IsBaked = 0;
    pCurrent->TotalTime = i_Step->ramp.time;
    pCurrent->InterpolationType = i_Step->ramp.interpolate_type;
    pCurrent->CurrentTime = 0;
    return 1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
F32 Ramp_GetCurrentValue(EventSystem* i_System, Ramp* i_Ramp, S32 i_ConvertToScalar)
{
    // We can't return the baked if we want no conversion, since the conversion is baked!
    if (i_Ramp->IsBaked && i_ConvertToScalar) 
        return i_Ramp->BakedValue;

    // If it is numerical, atof it.
    F32 TargetValue = 0;
    if ((i_Ramp->Target[0] >= '0' && i_Ramp->Target[0] <= '9') || i_Ramp->Target[0] == '-' || i_Ramp->Target[0] == '.')
    {
        TargetValue = atof_with_len(i_Ramp->Target, 0);
    }
    else
    {
        Var_LookupWithContext(i_System, i_System->VarRoot, i_Ramp->Target, 1, &TargetValue);
    }

    // If we are dealing with dB, convert to LinLoud
    if (i_Ramp->ValueType == RAMPTYPE_VOLUME ||
        i_Ramp->ValueType == RAMPTYPE_WET)
    {
        if (TargetValue <= -96.0f)
            TargetValue = 0;
        else
        {
            TargetValue = MSS_dB_to_scalar(TargetValue);
            TargetValue = pow(TargetValue, 6.0f/10.0f);
        }
    }

    F32 ResultValue = TargetValue;
    if (i_Ramp->TotalTime > .001f)
    {
        F32 InterpParam = (i_Ramp->CurrentTime / i_Ramp->TotalTime);
        switch (i_Ramp->InterpolationType)
        {
        case RAMPINT_LINEAR:
            {
                break;
            }
        case RAMPINT_EXP:
            {
                InterpParam = InterpParam * InterpParam;
                break;
            }
        case RAMPINT_SCURVE:
            {
                InterpParam -= .5f;
                InterpParam = InterpParam * InterpParam * InterpParam;
                InterpParam *= 4;
                InterpParam += .5f;
                break;
            }
        }

        ResultValue = (TargetValue - i_Ramp->StartValue) * InterpParam + i_Ramp->StartValue;
    }

    if (i_ConvertToScalar)
    {
        switch (i_Ramp->ValueType)
        {
        case RAMPTYPE_VOLUME:
        case RAMPTYPE_WET:      ResultValue = pow(ResultValue, 10.0f/6.0f); break;
        case RAMPTYPE_LOWPASS:  break;
        case RAMPTYPE_RATE:     ResultValue = MSS_sT_to_scalar(ResultValue); break;
        }
    }

    if (fabs(ResultValue) < .001f) 
        ResultValue = 0;

    // Regular == works since it gets manually set to totaltime.
    if (i_Ramp->CurrentTime == i_Ramp->TotalTime)
    {
        // Bake
        i_Ramp->BakedValue = ResultValue;
        i_Ramp->IsBaked = 1;
    }
    return ResultValue;
}


//-----------------------------------------------------------------------------
//
// LFO
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 LFO_Add(EventSystem* i_System, EVENT_STEP_INFO const* i_Step)
{
    S32 Size = sizeof(LFO) + i_Step->setlfo.name.len + 1;
    Handle New = Handle_Allocate(Size);
    LFO* L = (LFO*)Handle_Get(New);
    if (L == 0)
        return 0; // oom

    AIL_memcpy(L->Var, i_Step->setlfo.name.str, i_Step->setlfo.name.len + 1);

    L->Self = New;
    L->Next = i_System->LFORoot;
    i_System->LFORoot = New;

    if (OnlyNum(i_Step->setlfo.base.str))
    {
        L->Base = (float)atof_with_len(i_Step->setlfo.base.str, 0);
    }
    else
    {
        L->BaseStr = Handle_Allocate(i_Step->setlfo.base.len + 1);
        char* S = (char*)Handle_Get(L->BaseStr);
        if (S)
            AIL_memcpy(S, i_Step->setlfo.base.str, i_Step->setlfo.base.len + 1);
    }

    if (OnlyNum(i_Step->setlfo.amplitude.str))
    {
        L->Amp = (float)atof_with_len(i_Step->setlfo.amplitude.str, 0);
    }
    else
    {
        L->AmpStr = Handle_Allocate(i_Step->setlfo.amplitude.len + 1);
        char* S = (char*)Handle_Get(L->AmpStr);
        if (S)
            AIL_memcpy(S, i_Step->setlfo.amplitude.str, i_Step->setlfo.amplitude.len + 1);
    }

    if (OnlyNum(i_Step->setlfo.freq.str))
    {
        L->Freq = (float)atof_with_len(i_Step->setlfo.freq.str, 0);
    }   
    else
    {
        L->FreqStr = Handle_Allocate(i_Step->setlfo.freq.len + 1);
        char* S = (char*)Handle_Get(L->FreqStr);
        if (S)
            AIL_memcpy(S, i_Step->setlfo.freq.str, i_Step->setlfo.freq.len + 1);
    }


    L->Waveform = (U8)i_Step->setlfo.waveform;
    L->Invert = (U8)i_Step->setlfo.invert;
    L->DutyCycle = (U8)i_Step->setlfo.dutycycle;
    L->Polarity = (U8)i_Step->setlfo.polarity;

    // Set the initial value.
    float Value = 0;
    switch (L->Waveform)
    {
    case 0: Value = (L->Polarity ? 1 : 0) + 1.0f; break;

    case 1: // Triangle
        {
            Value = 0;
            if (L->Invert)
                Value = 1 - Value;
            if (L->Polarity == 0)
                Value -= .5f;
            break;
        }

    case 2: // Saw
        {
            Value = 0;
            if (L->Invert)
                Value = 1 - Value;
            if (L->Polarity == 0)
                Value -= .5f;
            break;
        }
    case 3: // Square
        {
            Value = 0;
            if (L->DutyCycle)
                Value = 1;
            if (L->Invert)
                Value = !Value;
            if (L->Polarity == 0)
                Value -= .5f;
            break;
        }
    }

    Value = Value * L->Amp + L->Base;

    Var_SetInstance(&i_System->VarRoot, L->Var, &Value, 1);

    return 1;
}

//-----------------------------------------------------------------------------
//
// Sound Source Management
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Sound_Free(EventSystem* i_System, Handle i_SoundHandle)
{
    SoundSource* pSound = (SoundSource*)Handle_Get(i_SoundHandle);
    if (pSound == 0) return;

    pSound->m_RefCount--;

    if (pSound->m_RefCount == 0)
    {
        // If we have an outstanding async, we need to cancel it.
        if (pSound->m_AsyncIndex)
        {
            if (MilesAsyncFileCancel(s_Asyncs + pSound->m_AsyncIndex - 1) == 0)
            {
                // Async is already in progress, we have to sit on it.
                s_AsyncCancelled[pSound->m_AsyncIndex-1] = 1;

                // The cancellation logic has to free it since it may be in use.
                pSound->m_Data = 0;
            }
            else
            {
                Audition_AsyncCancel((S32)(SINTa)s_Asyncs[pSound->m_AsyncIndex-1].StreamUserData);

                // Async completed.
                s_AsyncUsed[pSound->m_AsyncIndex-1] = 0;

                AIL_mem_free_lock(pSound->m_Data);
            }
        }
        else
        {
            // The async system isn't hanging on to our data, so we can free
            i_System->SoundDataSize -= pSound->m_DataLen;
            AIL_mem_free_lock(pSound->m_Data);
        }

        Handle_Free(pSound->m_Name);

        // Free our actual data.
        Handle_Free(i_SoundHandle);

        //! \todo perf - needs to be way faster!
        Handle* pSeek = i_System->SoundSources.Data();
        Handle* pEnd = pSeek + i_System->SoundSources.Count();
        while (pSeek != pEnd)
        {
            if (Handle_Equal(*pSeek, i_SoundHandle))
            {
                // Needs to be sorted so the asyncs stay in front.
                i_System->SoundSources.RemoveSortedCount( (S32) ( pSeek - i_System->SoundSources.Data() ), 1 );
                break;
            }
            pSeek++;
        }
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Handle Sound_Find(EventSystem* i_System, const char* i_Name)
{
    S32 NameLen = (S32)AIL_strlen(i_Name);

    S32 Count = i_System->SoundSources.Count();
    Handle* pSeek = i_System->SoundSources.Data();
    while (Count)
    {
        SoundSource* pSource = (SoundSource*)Handle_Get(*pSeek);
        if (pSource->m_NameLen == NameLen)
        {
            char* pName = (char*)Handle_Get(pSource->m_Name);
            if (AIL_stricmp(pName, i_Name) == 0)
            {
               return *pSeek;
            }
        }

        Count--;
        pSeek++;
    }

    return Handle_Clear();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Handle Sound_Load(EventSystem* i_System, U64 i_Id, const char* i_Name)
{
    // 
    // Kicks off an async load for a sound.
    //

    // Get the filename.
    char soundfilename[MSS_MAX_PATH_BYTES];
    MILESBANKSOUNDINFO SoundInfo;
    if (Container_GetSound(i_Name, soundfilename, &SoundInfo) == 0)
    {
        ErrorHandler(eMissingAsset, i_Id, i_Name);
        return Handle_Clear();
    }

    if (i_System->SoundSources.PushEmpty() == 0)
    {
        ErrorHandler(eOutOfHandleMem, i_Id, i_Name);
        return Handle_Clear();
    }

    Handle SourceHandle = Handle_Allocate(sizeof(SoundSource));
    SoundSource* pSound = (SoundSource*)Handle_Get(SourceHandle);
	if (pSound == 0)
	{
		ErrorHandler(eOutOfHandleMem, i_Id, i_Name);
		i_System->SoundSources.Pop();
		return Handle_Clear();
	}

    AIL_memset(pSound, 0, sizeof(SoundSource));

    char const* ext = strrchr(soundfilename, '.');
    if (ext && strlen(ext) < 8)
    {
        strcpy(pSound->m_Ext, ext);
    }

    pSound->m_Provider = find_ASI_provider("Input file types", ext);
    if (AIL_stricmp(ext, ".wav"))
    {
        // We have to have found the provider.
        if (pSound->m_Provider == 0)
        {
            AIL_set_error( "Couldn't find provider for this file type -- check your AIL_set_redist_directory() call." );
            ErrorHandler(eMilesError, i_Id, i_Name);
            Handle_Free(SourceHandle);
            i_System->SoundSources.Pop();
            return Handle_Clear();
        }
    }

    pSound->m_SoundLimit = SoundInfo.SoundLimit;
    pSound->m_SoundMixDAC = SoundInfo.MixVolumeDAC;

    pSound->m_NameLen = (S32)AIL_strlen(i_Name);
    pSound->m_Name = Handle_Allocate(pSound->m_NameLen + 1);
    char* pStr = (char*)Handle_Get(pSound->m_Name);
    if (pStr == 0)
    {
        ErrorHandler(eOutOfHandleMem, i_Id, i_Name);
        Handle_Free(SourceHandle);
        i_System->SoundSources.Pop();
        return Handle_Clear();
    }

    pSound->m_AsyncQueueId = s_AsyncId++;
    Audition_AsyncQueued(i_Id, pSound->m_AsyncQueueId, i_Name);

    AIL_strcpy(pStr, i_Name);
    pSound->m_RefCount = 1;
    i_System->SoundSources.Top() = SourceHandle;

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
                    ErrorHandler(eOutOfMem, i_Id, i_Name);
                    Handle_Free(pSound->m_Name);
                    Handle_Free(SourceHandle);
                    i_System->SoundSources.Pop();
                    return Handle_Clear();
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


    // If we don't have a free async, then the async index and the data are both null, and 
    // will get picked up during tick.
    return SourceHandle;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void DestroySoundInstance(EventSystem* i_System, SoundInstance* i_Sound)
{
    if (i_Sound->m_Status == MILESEVENT_SOUND_STATUS_COMPLETE) return;

    // End the sample and mark as complete.
    AIL_end_sample(i_Sound->m_Sample);
    i_Sound->m_Status = MILESEVENT_SOUND_STATUS_COMPLETE;

    char* pSoundLabels = (char*)Handle_Get(i_Sound->m_Labels);

    // Update limits for this sound.
    SoundLimit* Limit = (SoundLimit*)Handle_Get(i_System->LimitRoot);
    if (Limit)
    {
        U16 parsed_lbls[ 256 ];
        parse_string_list( parsed_lbls, pSoundLabels, ~0U, ',', 0 );

        SoundLimitEntry* Entry = (SoundLimitEntry*)Handle_Get(Limit->Entries);
        for ( ; Entry ; Entry = (SoundLimitEntry*)Handle_Get(Entry->Next))
        {
            if (CheckLabelMatchLabelParsed(parsed_lbls, pSoundLabels, Entry->Label, ~0U, 0))
                Entry->Count--;
        }
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 EvictSound(EventSystem* i_System, U64 i_InFavorOf, U8 i_EvictType, S32 i_Priority, SoundLimitEntry* i_Limit, SoundSource* i_Source, char const* i_SourceName)
{
    U16 parsed_query[ 256 ];
    if (i_Limit)
    {
        parse_wildcard_query(parsed_query, i_Limit->Label, ~0U, ' ', 0);
    }

    // Used for non pri tests.
    S32 PurgeIndex = -1;

    F32 ListenerX, ListenerY, ListenerZ;
    AIL_listener_3D_position(i_System->Driver, &ListenerX, &ListenerY, &ListenerZ);

    F32 CurrentDistanceSq = 0;
    F32 CurrentVolume = 100.0f;
    S32 CurrentOldest = AIL_ms_count();
    S32 BreakerPriority = 255; // used for tie breakers.

    S32 SoundCount = i_System->SoundInstances.Count();
    for (S32 SoundIndex = 0; SoundIndex < SoundCount; SoundIndex++)
    {
        SoundInstance* pSound = (SoundInstance*)Handle_Get(i_System->SoundInstances[SoundIndex]);
        if (pSound->m_Status == MILESEVENT_SOUND_STATUS_COMPLETE) continue; // ignore complete sounds.

        if (i_EvictType == MILES_START_STEP_PRIORITY)
        {
            if (pSound->m_Priority >= i_Priority) continue;
        }

        // Check based on our limitation type.
        if (i_Source && i_Source != Handle_Get(pSound->m_Source)) 
            continue;

        if (i_SourceName && AIL_stricmp(i_SourceName, (char*)Handle_Get(pSound->m_Name))) 
            continue;

        if (i_Limit)
        {
            char* pSoundLabels = (char*)Handle_Get(pSound->m_Labels);
            if (CheckLabelMatchQueryParsed(pSoundLabels, ~0U, parsed_query, i_Limit->Label) == 0)
                continue;
        }

        // Pri just gets to bail.
        if (i_EvictType == MILES_START_STEP_PRIORITY)
        {
            // Cancel completion events for evicted sounds.
            Handle_Free(pSound->m_CompletionEvent);
            pSound->m_Flags |= MILESEVENT_SOUND_FLAG_EVICTED;
            DestroySoundInstance(i_System, pSound);
            Audition_SoundEvicted(pSound->m_SoundId, i_InFavorOf, MILES_START_STEP_PRIORITY);
            return 1;
        }

        // Otherwise we update our checks.
        switch (i_EvictType)
        {
        case MILES_START_STEP_OLDEST:
            {
                if (pSound->m_StartTime < CurrentOldest)
                {
                    PurgeIndex = SoundIndex;
                    CurrentOldest = pSound->m_StartTime;
                }
                break;
            }
        case MILES_START_STEP_VOLUME:
            {
                F32 SampleVolume = pSound->m_Sample->max_output_mix_volume;

                if (AIL_fabs(SampleVolume - CurrentVolume) < .01f)
                {
                    // Tie, check pri.
                    if (pSound->m_Priority < BreakerPriority)
                    {
                        PurgeIndex = SoundIndex;
                        CurrentVolume = SampleVolume;
                        BreakerPriority = pSound->m_Priority;
                    }
                }
                else if (SampleVolume < CurrentVolume)
                {
                    PurgeIndex = SoundIndex;
                    CurrentVolume = SampleVolume;
                    BreakerPriority = pSound->m_Priority;
                }
                break;
            }
        case MILES_START_STEP_DISTANCE:
            {
                F32 X, Y, Z;
                AIL_sample_3D_position(pSound->m_Sample, &X, &Y, &Z);
                F32 DistanceSq = (X-ListenerX)*(X-ListenerX) + (Y-ListenerY)*(Y-ListenerY) + (Z-ListenerZ)*(Z-ListenerZ);
                if (DistanceSq > CurrentDistanceSq)
                {
                    CurrentDistanceSq = DistanceSq;
                    PurgeIndex = SoundIndex;
                }
            }
        }
    } // end for each sound.

    if (i_EvictType == MILES_START_STEP_PRIORITY || PurgeIndex == -1)
    {
        // Didn't find a sound to limit.
        return 0;
    }

    // Cancel completion event, and destroy.
    SoundInstance* pEvict = (SoundInstance*)Handle_Get(i_System->SoundInstances[PurgeIndex]);
    Handle_Free(pEvict->m_CompletionEvent);
    pEvict->m_Flags |= MILESEVENT_SOUND_FLAG_EVICTED;
    DestroySoundInstance(i_System, pEvict);
    Audition_SoundEvicted(pEvict->m_SoundId, i_InFavorOf, i_EvictType);
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
SoundLimitEntry* IsLabelLimited(EventSystem* i_System, MSSSTRINGC i_Labels)
{
    // Check if no limits apply.
    SoundLimit* Limit = (SoundLimit*)Handle_Get(i_System->LimitRoot);
    if (Limit == 0 || i_Labels.len == 0)
    {
        return 0;
    }

    SoundLimitEntry* Entry = (SoundLimitEntry*)Handle_Get(Limit->Entries);

    //
    // Check the instance counts for the current labels.
    //
#ifdef _DEBUG
    for ( ; Entry; Entry = (SoundLimitEntry*)Handle_Get(Entry->Next))
    {
        U16 parsed_query[ 256 ];
        parsed_query[ 0 ] = 0xffff;

        S32 CheckCount = Entry->Count;
        Entry->Count = 0;

        const S32 PlayingCount = i_System->SoundInstances.Count();
        for (S32 PlayingIndex = 0; PlayingIndex < PlayingCount; PlayingIndex++)
        {
            SoundInstance* pSound = (SoundInstance*)Handle_Get(i_System->SoundInstances[PlayingIndex]);
            if (pSound->m_Status == MILESEVENT_SOUND_STATUS_COMPLETE) continue;

            char* pSoundLabels = (char*)Handle_Get(pSound->m_Labels);

            if ( parsed_query[ 0 ] == 0xffff )
              parse_wildcard_query( parsed_query, Entry->Label, 0xffffffff, ',', ' ' );

            if (CheckLabelMatchQueryParsed(pSoundLabels, 0xffffffff, parsed_query, Entry->Label ))
            {
                Entry->Count++;
            }
        }

        if (CheckCount != Entry->Count)
        {
            MSSBreakPoint();
        }
    }
    Entry = (SoundLimitEntry*)Handle_Get(Limit->Entries);

#endif

    U16 parsed_labels[256];
    parse_string_list(parsed_labels, i_Labels.str, i_Labels.len, ',', 0);

    for ( ; Entry; Entry = (SoundLimitEntry*)Handle_Get(Entry->Next))
    {
        if (CheckLabelMatchLabelParsed(parsed_labels, i_Labels.str, Entry->Label, ~0U, 0))
        {
            if (Entry->Count + 1 > Entry->Limit)
                return Entry;
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------
//
// Falloff Graphs
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void FalloffGraph_Set(HSAMPLE S, S32 Affects, FalloffGraph* G)
{
    Handle InstanceHandle;
    InstanceHandle.index = (S32)S->system_data[EVT_HANDLE_INDEX];
    InstanceHandle.magic = (U32)S->system_data[EVT_HANDLE_MAGIC];

    SoundInstance* Sound = (SoundInstance*)Handle_Get(InstanceHandle);
    if (Sound == 0)
    {
        // Not managed by the event system.
        return;
    }

    if (G == 0)
    {       
        if (Affects & 1)
            AIL_set_sample_3D_volume_falloff(S, 0, 0);
        if (Affects & 2)
            AIL_set_sample_3D_lowpass_falloff(S, 0, 0);
        if (Affects & 4)
            AIL_set_sample_3D_exclusion_falloff(S, 0, 0);
    }


    // The max distance computation happens prior to the callback
    S->S3D.max_dist = G->MaxDistance;

    if (Affects & 1)
        AIL_set_sample_3D_volume_falloff(S, G->Points, G->VolCount);

    if (Affects & 2)
        AIL_set_sample_3D_lowpass_falloff(S, G->Points + G->VolCount, G->LPCount);

    if (Affects & 4)
        AIL_set_sample_3D_exclusion_falloff(S, G->Points + G->VolCount + G->LPCount, G->ExcCount);

    S->is_3D = 1;
}

//-----------------------------------------------------------------------------
//
// Vars
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 Var_LookupWithContext(EventSystem* i_System, Handle i_Root, char const* i_Variable, S32 i_WantFloat, void* o_Value)
{
    for (Handle Root = i_Root; ; Root = i_System->VarRoot)    
    {
        VarDef* pRoot = (VarDef*)Handle_Get(Root);
        for (; pRoot; pRoot = (VarDef*)Handle_Get(pRoot->Next))
        {
            if (AIL_stricmp(i_Variable, pRoot->Name) == 0)
            {
                if (i_WantFloat)
                {
                    if (pRoot->IsFloat) *(F32*)o_Value = pRoot->f;
                    else *(F32*)o_Value = (F32)pRoot->i;
                }
                else
                {
                    if (pRoot->IsFloat) *(S32*)o_Value = (S32)(pRoot->f + .001f);
                    else *(S32*)o_Value = pRoot->i;
                }
                return 1;
            }
        }

        if (Handle_Equal(Root, i_System->VarRoot))
        {
            ErrorHandler(eMissingVar, 0, i_Variable);
            return 0;
        }
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Var_SetInstance(Handle* i_Root, char const* i_Name, void* i_Value, S32 i_IsFloat)
{
    VarDef* pRoot = (VarDef*)Handle_Get(*i_Root);
    for (; pRoot; pRoot = (VarDef*)Handle_Get(pRoot->Next))
    {
        if (AIL_stricmp(pRoot->Name, i_Name) == 0)
        {
            pRoot->IsFloat = i_IsFloat;
            if (i_IsFloat) 
                pRoot->f = *(F32*)i_Value;
            else 
                pRoot->i = *(S32*)i_Value;

            VarGraph* G = (VarGraph*)Handle_Get(pRoot->Graph);
            if (G)
            {
                // Transform the value by the graph.
                F32 Value = *(F32*)i_Value;
                if (i_IsFloat == 0)
                    Value = (F32)*(S32*)i_Value;

                if (Value < G->Min)
                {
                    switch (G->InfLow)
                    {
                    case 0: Value = G->Min; break;
                    case 1: Value = fmodf(Value - G->Min, G->Max - G->Min) + G->Max; break;
                    case 2: 
                            S32 Check = (S32)((Value - G->Min) / (G->Max - G->Min));
                            Value = fmodf(Value - G->Min, G->Max - G->Min);
                            Check--;
                            if (Check % 2)
                                Value = (G->Max - G->Min) - (G->Min - Value);
                            Value += G->Max;
                            break;
                    }
                }
                if (Value > G->Max)
                {
                    switch (G->InfHigh)
                    {
                    case 0: Value = G->Max; break;
                    case 1: Value = fmodf(Value - G->Min, G->Max - G->Min) + G->Min; break;
                    case 2: 
                            S32 Check = (S32)((Value - G->Min) / (G->Max - G->Min));
                            Value = fmodf(Value - G->Min, G->Max - G->Min);
                            if (Check % 2)
                                Value = (G->Max - G->Min) - Value;
                            Value += G->Min;
                            break;
                    }
                }

                Value = evaluate_graph(G->Points, G->PointCount, Value);
                pRoot->f = Value;
            }
            return;
        }
    }

    S32 NameLen = (S32)AIL_strlen(i_Name);
    S32 AllocLen = sizeof(VarDef) + NameLen;
    Handle New = Handle_Allocate(AllocLen);
    pRoot = (VarDef*)Handle_Get(New);
    if (pRoot == 0) return;

    pRoot->Next = *i_Root;
    *i_Root = New;

    if (i_IsFloat) 
        pRoot->f = *(F32*)i_Value;
    else 
        pRoot->i = *(S32*)i_Value;

    pRoot->IsFloat = i_IsFloat;
    pRoot->Self = New;
    AIL_strcpy(pRoot->Name, i_Name);
}

//-----------------------------------------------------------------------------
//
// Persists
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 Persist_Create(EventSystem* i_System, U64 i_EventId, const EVENT_STEP_INFO* i_Step)
{
    PersistedPreset* Cur = (PersistedPreset*)Handle_Get(i_System->PersistRoot);
    for (; Cur; Cur = (PersistedPreset*)Handle_Get(Cur->Next))
    {
        if (AIL_stricmp(Cur->Name, i_Step->persist.name.str) == 0)
        {
            break;
        }
    }

    S32 Exists = 0;
    if (Cur)
    {
        // If we already exist, update our contents.
        Handle_Free(Cur->Preset);
        Handle_Free(Cur->Labels);
        Exists = 1;
    }
    else
    {
        Handle Self = Handle_Allocate(sizeof(PersistedPreset) + i_Step->persist.name.len);
        Cur = (PersistedPreset*)Handle_Get(Self);
        if (Cur == 0)
        {
            ErrorHandler(eOutOfHandleMem, i_EventId, i_Step->persist.name.str);
            return 0;
        }

        Cur->Self = Self;
        AIL_strcpy(Cur->Name, i_Step->persist.name.str);
    }

    Cur->Dynamic = i_Step->persist.isdynamic;
    Cur->Preset = Handle_Allocate(i_Step->persist.presetname.len + 1);
    Cur->Labels = Handle_Allocate(i_Step->persist.labels.len + 1);

    char* pPresetName = (char*)Handle_Get(Cur->Preset);
    char* pLabels = (char*)Handle_Get(Cur->Labels);

    if (pPresetName == 0 ||
        pLabels == 0)
    {
        ErrorHandler(eOutOfHandleMem, i_EventId, i_Step->persist.name.str);

        if (Exists)
        {
            // Remove us.
            Persist_Free(i_System, i_EventId, Cur->Name);
        }
        else
        {
            Handle_Free(Cur->Preset);
            Handle_Free(Cur->Labels);
            Handle_Free(Cur->Self);
        }
        return 0;
    }

    AIL_strcpy(pPresetName, i_Step->persist.presetname.str);
    AIL_strcpy(pLabels, i_Step->persist.labels.str);

    if (Exists == 0)
    {
        Cur->Next = i_System->PersistRoot;
        i_System->PersistRoot = Cur->Self;
    }

    // Ensure we have preset data.
    void* PresetData = Container_GetPreset(i_Step->persist.presetname.str);
    if (PresetData == 0)
    {
        ErrorHandler(eMissingAsset, i_EventId, i_Step->persist.presetname.str);
        return 1;
    }

    U16 parsed_query[ 256 ];
    parsed_query[ 0 ] = 0xffff;

    // Apply to all the current sounds.
    S32 SoundCount = i_System->SoundInstances.Count();
    for (S32 SoundIndex = 0; SoundIndex < SoundCount; SoundIndex++)
    {
        SoundInstance* pSound = (SoundInstance*)Handle_Get(i_System->SoundInstances[SoundIndex]);
        const char* pSoundLabels = (const char*)Handle_Get(pSound->m_Labels);

        if ( parsed_query[ 0 ] == 0xffff )
          parse_wildcard_query( parsed_query, i_Step->persist.labels.str, i_Step->persist.labels.len, ',', 0 );

        if (CheckLabelMatchQueryParsed( pSoundLabels, 0xffffffff, parsed_query, i_Step->persist.labels.str ))
        {
            AIL_resolve_raw_sound_preset(PresetData, pSound, EvalFunc);
            AIL_apply_raw_sound_preset(pSound->m_Sample, PresetData);
        }
        SoundCount--;
        pSound++;
    }

    Audition_SetPersist(i_EventId, Cur->Name, pPresetName);

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Persist_Free(EventSystem* i_System, U64 i_EventId, const char* i_Name)
{
    PersistedPreset* Last = 0;
    PersistedPreset* Cur = (PersistedPreset*)Handle_Get(i_System->PersistRoot);
    for (; Cur; Last = Cur, Cur = (PersistedPreset*)Handle_Get(Cur->Next))
    {
        if (AIL_stricmp(i_Name, Cur->Name) == 0) break;
    }
   
    if (Cur == 0)
        return; // Doesn't exist.


    void* PresetData = Container_GetPreset((char*)Handle_Get(Cur->Preset));

    const char* pPersistLabels = (const char*)Handle_Get(Cur->Labels);

    U16 parsed_query[ 256 ];
    parsed_query[ 0 ] = 0xffff;

    // Unapply to all the current sounds.
    S32 SoundCount = i_System->SoundInstances.Count();
    for (S32 SoundIndex = 0; SoundIndex < SoundCount; SoundIndex++)
    {
        SoundInstance* pSound = (SoundInstance*)Handle_Get(i_System->SoundInstances[SoundIndex]);
        const char* pSoundLabels = (const char*)Handle_Get(pSound->m_Labels);

        if ( parsed_query[ 0 ] == 0xffff )
          parse_wildcard_query( parsed_query, pPersistLabels, 0xffffffff, ',', 0 );

        if (CheckLabelMatchQueryParsed( pSoundLabels, 0xffffffff, parsed_query, pPersistLabels ))
        {
            AIL_unapply_raw_sound_preset(pSound->m_Sample, PresetData);
        }
        SoundCount--;
        pSound++;
    }

    if (Last)
    {
        Last->Next = Cur->Next;
    }
    else
    {
        i_System->PersistRoot = Cur->Next;
    }

    Audition_SetPersist(i_EventId, Cur->Name, 0);

    // Free mem.
    Handle_Free(Cur->Preset);
    Handle_Free(Cur->Labels);
    Handle_Free(Cur->Self);
    return;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Persist_Apply(EventSystem* i_System, SoundInstance* i_Instance, HSAMPLE i_Sample, const char* i_Labels, S32 i_LabelLen, S32 i_DynamicCheck)
{
    U16 parsed_lbls[ 256 ];
    parsed_lbls[ 0 ] = 0xffff;

    // Check all valid presets to see if they match.
    PersistedPreset* Cur = (PersistedPreset*)Handle_Get(i_System->PersistRoot);
    for (; Cur; Cur = (PersistedPreset*)Handle_Get(Cur->Next))
    {
        if (i_DynamicCheck && !Cur->Dynamic) continue; // Don't apply persists that aren't dynamic.

        const char* pCheckLabels = (const char*)Handle_Get(Cur->Labels);

        if ( parsed_lbls[ 0 ] == 0xffff )
          parse_string_list( parsed_lbls, i_Labels, i_LabelLen, ',', 0 );

        // No labels on the preset means apply to all.
        if (CheckLabelMatchLabelParsed( parsed_lbls, i_Labels, pCheckLabels, 0xffffffff, 0 ))
        {
            void* PresetData = Container_GetPreset((char*)Handle_Get(Cur->Preset));

            AIL_resolve_raw_sound_preset(PresetData, i_Instance, EvalFunc);
            AIL_apply_raw_sound_preset(i_Sample, PresetData);
        }
    }
}

//-----------------------------------------------------------------------------
//
// Limits
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 Limit_Add(EventSystem* i_System, EVENT_STEP_INFO const* i_Step, S64 i_Id)
{
    S32 NameLen = i_Step->limits.name.len;
    Handle New = Handle_Allocate(sizeof(SoundLimit) + NameLen + 1);

    SoundLimit* Limit = (SoundLimit*)Handle_Get(New);
    if (Limit == 0)
    {
        ErrorHandler(eOutOfHandleMem, i_Id, 0);
        return 0;
    }

    strcpy(Limit->Name, i_Step->limits.name.str);
    Limit->Self = New;
    Limit->Next = i_System->LimitRoot;
    i_System->LimitRoot = Limit->Self;

    //
    // Parse the string into the labels.
    //
    char const* pCur = i_Step->limits.limits.str;
    while (*pCur)
    {
        // Seek to the label terminator.
        char const* pLabelEnd = strstr(pCur, " ");
        if (pLabelEnd == 0)
        {
            ErrorHandler(eBadSetLimit, i_Id, 0);
            return 0;
        }

        // Get the decimal count for the label.
        const char* pDecimalStart = pLabelEnd + 1;
        if (*pDecimalStart == 0)
        {
            ErrorHandler(eBadSetLimit, i_Id, 0);
            return 0;
        }

        if (pDecimalStart[0] < '0' || pDecimalStart[0] > '9')
        {
            ErrorHandler(eBadSetLimit, i_Id, 0);
            return 0;
        }

        // Get the limit.
        U32 MaxCount = strtoi(pDecimalStart, &pDecimalStart);

        S32 LabelLen = (S32)(pLabelEnd - pCur);

        Handle Self = Handle_Allocate(sizeof(SoundLimitEntry) + LabelLen + 1);
        SoundLimitEntry* Entry = (SoundLimitEntry*)Handle_Get(Self);
        if (Entry == 0)
        {
            ErrorHandler(eOutOfHandleMem, i_Id, 0);
            return 0;
        }

        Entry->Self = Self;

        Entry->Limit = (S16)MaxCount;
        Entry->Count = 0;
        Entry->Next = Limit->Entries;
        AIL_memcpy(Entry->Label, pCur, LabelLen);
        Entry->Label[LabelLen] = 0;
        Limit->Entries = Self;

        // Advance past the semicolon if it exists.
        if (*pDecimalStart) 
            pDecimalStart++;

        pCur += pDecimalStart - pCur;
    }

    return 1;
}


//-----------------------------------------------------------------------------
//
// Misc
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 CheckLabelMatchLabelParsed(
    void const * parsed_lbls, char const * labels,
    char const * query, U32 query_len, char query_terminator
    )
{
  U8 parsed_qry[ 512 ];

  parse_wildcard_query( parsed_qry, query, query_len, ',', query_terminator );
  return wild_card_match_list_to_query( parsed_lbls, labels, parsed_qry, query );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 CheckLabelMatchQueryParsed(
    char const * labels, U32 labels_len,
    void const * parsed_qry, char const * query
    )
{
  U8 parsed_lbls[ 512 ];

  parse_string_list( parsed_lbls, labels, labels_len, ',', 0 );
  return wild_card_match_list_to_query( parsed_lbls, labels, parsed_qry, query );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 OnlyNum(char const* s)
{
    char const* start = s;
    for (; *s; s++)
    {
        if (s == start && s[0] == '-') continue;
        char r = s[0] - '0';
        if (r >= 0 && r <= 9) continue;
        if (*s == '.') continue;
        return 0;
    }

    return 1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 strtoi(const char* i_str, const char** o_terminator)
{
    S32 value = 0;
    while (*i_str && i_str[0] >= '0' && i_str[0] <= '9')
    {
        value = value*10 + (S32)(i_str[0] - '0');
        i_str++;
    }

    if (o_terminator) 
        *o_terminator = i_str;
    return value;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 FindMarker(EventSystem* i_System, SoundInstance* i_Instance, SINTa i_MarkerList, const char* i_Name, S32* o_Offset, S32* o_IsTime)
{
    if (AIL_stricmp(i_Name, "SampleStart") == 0)
    {
        *o_IsTime = TIME_BYTES_MASK;
        *o_Offset = 0;
        return 1;
    }
    else if (AIL_stricmp(i_Name, "SampleEnd") == 0)
    {
        *o_IsTime = TIME_BYTES_MASK;
        *o_Offset = -1;
        return 1;
    }

    // Check if we are to use a number as a ms count - this is ignored for markers.
    S32 TimeSpec = TIME_MS_MASK;
    if (i_Name[0] == '@')
    {
        // Its a byte offset
        TimeSpec = TIME_BYTES_MASK;
        i_Name++;
    }

    // Check if the name isn't a variable.
    if (i_Name[0] != '=')
    {
        // Look up the variable
        *o_IsTime = TimeSpec;
        return Var_LookupWithContext(i_System, i_Instance ? i_Instance->m_VarRoot : i_System->VarRoot, i_Name, 0, o_Offset);
    }

    i_Name++;

    // Check if the name is a number.
    if (OnlyNum(i_Name))
    {
        *o_Offset = strtoi(i_Name, 0);
        *o_IsTime = TimeSpec;
        return 1;
    }

    S32 Result = AIL_find_marker_in_list(i_MarkerList, i_Name, &TimeSpec);
    if (Result == -1) 
        return 0;

    // Markers are never in MS space - only bytes or samples.
    if (TimeSpec)
        TimeSpec = TIME_SAMPLES_MASK;
    else
        TimeSpec = TIME_BYTES_MASK;

    *o_Offset = Result;
    *o_IsTime = TimeSpec;
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void ErrorHandler(ErrorType i_Error, U64 i_Id, char const* i_Details)
{
    // A basic spot for catching errors.

    // Currently we just do a AIL_set_error() and return, so the user has some
    // recourse.
    switch (i_Error)
    {
    case eOutOfMem:             AIL_set_error("MilesEvent: Out Of Memory (AIL_mem_alloc_lock failed)."); break;
    case eMilesError: // Leave the miles error
        break;
    case eBadSetLimit:          AIL_set_error("MilesEvent: Bad string passed to SetLimits."); break;
    case eCommandBufferFull:    AIL_set_error("MilesEvent: The command buffer was full."); break;
    case eMissingVar:           AIL_set_error("MilesEvent: Missing Variable."); break;
    case eOutOfHandleMem:       AIL_set_error("MilesEvent: Out of event system memory (pool passed to event system startup exhausted)."); break;
    case eMissingAsset:         AIL_set_error("MilesEvent: An asset requested by an event was not found in memory."); break;
    case eAsyncLoadFail:        AIL_set_error("MilesEvent: An asynchronous load returned failure (file doesn't exist?)."); break;
    case eTooManyBlends:        AIL_set_error("MilesEvent: Too many sounds were start in a blended sound."); break;
    case eBlendDoesntExist:     AIL_set_error("MilesEvent: The requested blend has not been set as it doesn't exist."); break;
    case eEnqueueBufferFull:    AIL_set_error("MilesEvent: The enqueue buffer is full."); break;
    case eMissingSelection:     AIL_set_error("MilesEvent: The event expected a selection that did not exist."); break;
    }

    if (s_EventErrorCB) s_EventErrorCB(i_Id, i_Details);

    Audition_Error(i_Id, i_Details);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void AILCALL MilesSetEventErrorCallbackInternal(AILEVENTERRORCB i_Callback) 
{
    s_EventErrorCB = i_Callback; 
}

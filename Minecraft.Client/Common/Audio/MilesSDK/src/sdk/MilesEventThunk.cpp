
// #included in to mssdbg.c

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF const U8* AILEXPORT MilesFindEvent(HMSOUNDBANK i_Bank, const char* i_EventName)
{
    const U8* result;

    START
    {
        AIL_debug_log("AIL_find_event(" PTRFMT ",%s)" ,
            i_Bank, i_EventName);
    }

    IN_AIL;

    result = MilesFindEventInternal(i_Bank, i_EventName);

    OUT_AIL;

    RESULT
    {
        outreshex((U32)(UINTa)result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesClearEventQueue()
{
    START
    {
        AIL_debug_log("AIL_clear_event_queue()" );
    }

    IN_AIL;

    MilesClearEventQueueInternal();

    OUT_AIL;

    END;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesRegisterRand(MSS_USER_RAND * rand)
{
    START
    {
        AIL_debug_log("AIL_register_random("PTRFMT")" ,
            rand);
    }

    IN_AIL;

    MilesRegisterRandInternal(rand);

    OUT_AIL;

    END;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesEnumerateSoundInstances(HEVENTSYSTEM i_System, HMSSENUM* io_Next, S32 i_Status, const char* i_Labels, U64 search_for_ID, MILESEVENTSOUNDINFO* o_Info)
{
    S32 result;

    START
    {
        AIL_debug_log("AIL_enumerate_sound_instances(" PTRFMT ",%d,%s,%ld," PTRFMT ")" ,
            io_Next, i_Status, i_Labels, search_for_ID, o_Info);
    }

    IN_AIL;

    result = MilesEnumerateSoundInstancesInternal(i_System, io_Next, i_Status, i_Labels, search_for_ID, o_Info);

    OUT_AIL;

    RESULT
    {
        outresint(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesEnumeratePresetPersists(HEVENTSYSTEM i_System, HMSSENUM* io_Next, const char** o_Name)
{
    S32 result;

    START
    {
        AIL_debug_log("AIL_enumerate_preset_persists(" PTRFMT "," PTRFMT ")" ,
            io_Next, o_Name);
    }

    IN_AIL;

    result = MilesEnumeratePresetPersistsInternal(i_System, io_Next, o_Name);

    OUT_AIL;

    RESULT
    {
        outresint(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF U64 AILEXPORT MilesEnqueueEvent(const U8* i_Event, void* i_UserBuffer, S32 i_UserBufferLen, S32 i_EnqueueFlags, U64 i_EventFilter)
{
    U64 result;

    START
    {
        AIL_debug_log("AIL_enqueue_event(%s,"PTRFMT",%d,%d,%ld)" ,
            i_Event, i_UserBuffer, i_UserBufferLen, i_EnqueueFlags, i_EventFilter);
    }

    IN_AIL_NM;

    result = MilesEnqueueEventInternal(i_Event, i_UserBuffer, i_UserBufferLen, i_EnqueueFlags, i_EventFilter);

    OUT_AIL_NM;

    RESULT
    {
        outresint64(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesBeginEventQueueProcessing()
{
    S32 result;

    START
    {
        AIL_debug_log("AIL_begin_event_queue_processing()" );
    }

    IN_AIL;

    result = MilesBeginEventQueueProcessingInternal();

    OUT_AIL;

    RESULT
    {
        outresint(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesCompleteEventQueueProcessing()
{
    S32 result;

    START
    {
        AIL_debug_log("AIL_complete_event_queue_processing()" );
    }

    IN_AIL;

    result = MilesCompleteEventQueueProcessingInternal();

    OUT_AIL;

    RESULT
    {
        outresint(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF 
HEVENTSYSTEM
AILEXPORT MilesStartupEventSystem(HDIGDRIVER i_Driver, S32 i_CommandBufferSize, char* i_Memory, S32 i_MemoryLen)
{
    HEVENTSYSTEM result;

    START
    {
        AIL_debug_log("AIL_startup_event_system("PTRFMT",%d,"PTRFMT,"%d)" ,
            i_Driver, i_CommandBufferSize, i_Memory, i_MemoryLen);
    }

    IN_AIL;

    result = MilesStartupEventSystemInternal(i_Driver, i_CommandBufferSize, i_Memory, i_MemoryLen);

    OUT_AIL;

    RESULT
    {
        outresuint((SINTa)result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesShutdownEventSystem()
{
    START
    {
        AIL_debug_log("AIL_shutdown_event_system()" );
    }

    IN_AIL;

    MilesShutdownEventSystemInternal();

    OUT_AIL;

    END;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF HMSOUNDBANK AILEXPORT MilesAddSoundBank(const char* i_FileName, char const* i_Name)
{
    HMSOUNDBANK result;

    START
    {
        AIL_debug_log("AIL_add_soundbank(%s)" ,
            i_FileName);
    }

    IN_AIL;

    result = MilesAddSoundBankInternal(i_FileName, i_Name);

    OUT_AIL;

    RESULT
    {
        outresuint((UINTa)result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesSetEventErrorCallback(AILEVENTERRORCB * i_Callback)
{
    START
    {
        AIL_debug_log("AIL_set_event_error_callback("PTRFMT")" , i_Callback);
    }

    IN_AIL;

    MilesSetEventErrorCallbackInternal(i_Callback);

    OUT_AIL;

    END;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesReleaseSoundBank(HMSOUNDBANK i_Bank)
{
    S32 result;

    START
    {
        AIL_debug_log("AIL_release_soundbank("PTRFMT")" ,
            i_Bank);
    }

    IN_AIL;

    result = MilesReleaseSoundBankInternal(i_Bank);

    OUT_AIL;

    RESULT
    {
        outresint(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesSetSoundLabelLimits(HEVENTSYSTEM i_System, const char* i_SoundLimits)
{
    S32 result;

    START
    {
        AIL_debug_log("AIL_set_sound_label_limits(%s)" ,
            i_SoundLimits);
    }

    IN_AIL;

    result = MilesSetSoundLabelLimitsInternal(i_System, 0, i_SoundLimits, "CodeLimit");

    OUT_AIL;

    RESULT
    {
        outresint(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF char* AILEXPORT MilesTextDumpEventSystem()
{
    char* result;

    START
    {
        AIL_debug_log("AIL_text_dump_event_system()" );
    }

    IN_AIL;

    result = MilesTextDumpEventSystemInternal();

    OUT_AIL;

    END;

    return result;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF U64 AILEXPORT MilesEnqueueEventContext(HEVENTSYSTEM i_Context, const U8* i_Event, void* i_UserBuffer, S32 i_UserBufferLen, S32 i_EnqueueFlags, U64 i_EventFilter)
{
    U64 result;

    START
    {
        AIL_debug_log("AIL_enqueue_event_context("PTRFMT",%s,"PTRFMT",%d,%d,%ld)" ,
            i_Context, i_Event, i_UserBuffer, i_UserBufferLen, i_EnqueueFlags, i_EventFilter);
    }

    IN_AIL_NM;

    result = MilesEnqueueEventInternal(i_Event, i_UserBuffer, i_UserBufferLen, i_EnqueueFlags, i_EventFilter);

    OUT_AIL_NM;

    RESULT
    {
        outresint64(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF U64 AILEXPORT MilesEnqueueEventByName(char const* i_Name)
{
    U64 result;

    START
    {
        AIL_debug_log("AIL_enqueue_event_by_name(%s)", i_Name);
    }

    IN_AIL_NM;

    result = MilesEnqueueEventByNameInternal(i_Name);

    OUT_AIL_NM;

    RESULT
    {
        outresint64(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesSetSoundStartOffset(HMSSENUM i_Instance, S32 i_Offset, S32 i_IsMs)
{
    START
    {
        AIL_debug_log("AIL_set_sound_start_offset("PTRFMT",%d,%d)", i_Instance, i_Offset, i_IsMs );
    }

    IN_AIL_NM;

    MilesSetSoundStartOffsetInternal(i_Instance, i_Offset, i_IsMs);

    OUT_AIL_NM;

    END;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF HEVENTSYSTEM AILCALL MilesAddEventSystem(HDIGDRIVER i_Driver)
{
    void* result;

    START
    {
        AIL_debug_log("AIL_add_event_system("PTRFMT")", i_Driver);
    }

    IN_AIL;

    result = MilesAddEventSystemInternal(i_Driver);

    OUT_AIL;

    RESULT
    {
        outresuhex((UINTa)result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesGetVarI(UINTa i_Context, char const* i_Name, S32* o_Value)
{
    S32 result;

    START
    {
        AIL_debug_log("AIL_variable_int("PTRFMT"%s, "PTRFMT")", i_Context, i_Name, o_Value);
    }

    IN_AIL_NM;

    result = MilesGetVarIInternal(i_Context, i_Name, o_Value);

    OUT_AIL_NM;

    RESULT
    {
        outresint(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesGetVarF(UINTa i_Context, char const* i_Name, F32* o_Value)
{
    S32 result;

    START
    {
        AIL_debug_log("AIL_variable_float("PTRFMT"%s, "PTRFMT")", i_Context, i_Name, o_Value);
    }

    IN_AIL_NM;

    result = MilesGetVarFInternal(i_Context, i_Name, o_Value);

    OUT_AIL_NM;

    RESULT
    {
        outresint(result);
    }

    END;

    return result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesSetVarF(UINTa i_System, char const* i_Name, F32 i_Value)
{
    START
    {
        AIL_debug_log("AIL_set_variable_float("PTRFMT"%s, %f)", i_System, i_Name, i_Value);
    }

    IN_AIL_NM;

    MilesSetVarFInternal(i_System, i_Name, i_Value);

    OUT_AIL_NM;

    END;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesSetVarI(UINTa i_System, char const* i_Name, S32 i_Value)
{
    START
    {
        AIL_debug_log("AIL_set_variable_int("PTRFMT"%s, %d)", i_System, i_Name, i_Value);
    }

    IN_AIL_NM;

    MilesSetVarIInternal(i_System, i_Name, i_Value);

    OUT_AIL_NM;

    END;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesRequeueAsyncs()
{
    START
    {
        AIL_debug_log("AIL_requeue_failed_asyncs()");
    }

    IN_AIL_NM;

    MilesRequeueAsyncsInternal();

    OUT_AIL_NM;

    END;
}

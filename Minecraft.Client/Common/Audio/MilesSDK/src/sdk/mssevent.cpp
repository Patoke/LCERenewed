#include "mss.h"
#include "imssapi.h"

F32 atof_with_len(const char* i_source, S32* o_count);
S32 atoi_with_len(const char* i_source, S32 hex, S32* o_count);

// Echoed in tool/mssevent.cpp
#define CURRENT_EVENT_VERSION 4
#define EVENT_VERSION_STRING "4"


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF HMSSEVENTCONSTRUCT AILCALL AIL_create_event() 
{
    HMSSEVENTCONSTRUCT hEvent = AIL_mem_create(); 
    AIL_mem_printc(hEvent, (char)EVENT_STEPTYPE_VERSION + '0');
    AIL_mem_printf(hEvent, ";%d;", CURRENT_EVENT_VERSION);
    return hEvent;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_add_cache_sounds_event_step(HMSSEVENTCONSTRUCT i_Event, 
    const char* i_LibraryName, const char* i_Sounds)
{
    // Empty strings mean we can't actually do anything.
    if (i_LibraryName == 0) return 0;
    if (i_Sounds == 0) return 0;

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_CACHESOUNDS + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_LibraryName);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_Sounds);
    AIL_mem_printc(i_Event, ';');
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_add_uncache_sounds_event_step(HMSSEVENTCONSTRUCT i_Event, 
    const char* i_LibraryName, const char* i_Sounds)
{
    // Empty strings mean we can't actually do anything.
    if (i_LibraryName == 0) return 0;
    if (i_Sounds == 0) return 0;

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_PURGESOUNDS + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_LibraryName);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_Sounds);
    AIL_mem_printc(i_Event, ';');
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_add_exec_event_event_step(HMSSEVENTCONSTRUCT i_Event,
    char const* i_EventName)
{
    if (i_EventName == 0) i_EventName = "";

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_EXECEVENT + '0');
    AIL_mem_printf(i_Event, ";%s;", i_EventName);
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL
AIL_add_start_sound_event_step(
    HMSSEVENTCONSTRUCT i_Event, 
    const char* i_SoundNames,
    const char* i_PresetName,
    U8 i_PresetIsDynamic,
    const char* i_EventName,
    const char* i_StartMarker, const char* i_EndMarker,
    char const* i_StateVar, char const* i_VarInit,
    const char* i_labels, U32 i_Streaming, U8 i_CanLoad,
    U16 i_Delay, U16 i_DelayMax, U8 i_Priority, U8 i_LoopCount,
    const char* i_StartOffset,
    F32 i_VolMin, F32 i_VolMax, F32 i_PitchMin, F32 i_PitchMax,
    F32 i_FadeInTime,
    U8 i_EvictionType,
    U8 i_SelectType
    )
{
    // null labels just means emtpy string.
    if (i_labels == 0) i_labels = "";
    if (i_PresetName == 0) i_PresetName = "";
    if (i_SoundNames == 0) i_SoundNames = "";
    if (i_EventName == 0) i_EventName = "";
    if (i_Delay > i_DelayMax) i_Delay = i_DelayMax;
    if (i_StartMarker == 0) i_StartMarker = "";
    if (i_EndMarker == 0) i_EndMarker = "";
    if (i_StartOffset == 0) i_StartOffset = "";
    if (i_StateVar == 0) i_StateVar = "";
    if (i_VarInit == 0) i_VarInit = "";


    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_STARTSOUND + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_printf(i_Event, "%s;%s;%s;%s;%s;%s;%s;%s;",
        i_SoundNames, i_PresetName, i_EventName, i_StartMarker, i_EndMarker, i_labels, i_StateVar, i_VarInit);
    AIL_mem_printc(i_Event, (char)i_Streaming + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_printc(i_Event, (char)i_CanLoad + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_printc(i_Event, (char)i_PresetIsDynamic + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_printf(i_Event, "%04x;", i_Delay);
    AIL_mem_printf(i_Event, "%04x;", i_DelayMax);

    { // priority
        char w2 = (char)(i_Priority / 16);
        if (w2 > 9) w2 = w2 - 10 + 'a';
        else w2 += '0';
        AIL_mem_printc(i_Event, w2);
        char w1 = (char)(i_Priority % 16);
        if (w1 > 9) w1 = w1 - 10 + 'a';
        else w1 += '0';
        AIL_mem_printc(i_Event, w1);
        AIL_mem_printc(i_Event, ';');
    }

    { // loop count
        char w2 = (char)(i_LoopCount / 16);
        if (w2 > 9) w2 = w2 - 10 + 'a';
        else w2 += '0';
        AIL_mem_printc(i_Event, w2);
        char w1 = (char)(i_LoopCount % 16);
        if (w1 > 9) w1 = w1 - 10 + 'a';
        else w1 += '0';
        AIL_mem_printc(i_Event, w1);
        AIL_mem_printc(i_Event, ';');
    }

    AIL_mem_prints(i_Event, i_StartOffset);
    AIL_mem_printc(i_Event, ';');

    AIL_mem_printf(i_Event, "%f;", i_VolMin);
    AIL_mem_printf(i_Event, "%f;", i_VolMax);
    AIL_mem_printf(i_Event, "%f;", i_PitchMin);
    AIL_mem_printf(i_Event, "%f;", i_PitchMax);

    AIL_mem_printf(i_Event, "%f;", i_FadeInTime);

    if (i_EvictionType > MILES_START_STEP_OLDEST) i_EvictionType = 0;

    AIL_mem_printc(i_Event, '0' + (char)i_EvictionType);
    AIL_mem_printc(i_Event, ';');

    AIL_mem_printc(i_Event, '0' + (char)i_SelectType);
    AIL_mem_printc(i_Event, ';');


    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_add_setblend_event_step(HMSSEVENTCONSTRUCT i_Event,
    char const* i_Name, S32 i_SoundCount, F32 const* i_InMin, F32 const* i_InMax,
    F32 const* i_OutMin, F32 const* i_OutMax, F32 const* i_MinP, F32 const* i_MaxP)
{
    if (i_Name == 0) i_Name = "";
    if (i_SoundCount > 10) i_SoundCount = 10;
    if (i_InMin == 0 || i_InMax == 0 ||
        i_OutMin == 0 || i_OutMax == 0 ||
        i_MinP == 0 || i_MaxP == 0) return 0;

    AIL_mem_printf(i_Event, "%c;%s;%d;", (char)EVENT_STEPTYPE_SETBLEND + '0', i_Name, i_SoundCount);

    for (S32 i = 0; i < i_SoundCount; i++)
    {
        AIL_mem_printf(i_Event, "%f;%f;%f;%f;%f;%f;", 
            i_InMin[i], i_InMax[i], i_OutMin[i], i_OutMax[i], 
            i_MinP[i], i_MaxP[i]);
    }

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF
S32
AILCALL
AIL_add_control_sounds_event_step(
    HMSSEVENTCONSTRUCT i_Event, 
    const char* i_labels,
    const char* i_MarkerStart, const char* i_MarkerEnd, const char* i_Position,
    const char* i_PresetName,
    U8 i_PresetApplyType,
    F32 i_FadeOutTime,
    U8 i_LoopCount, U8 i_Type
    )
{
    if (i_labels == 0) i_labels = "";
    if (i_MarkerStart == 0) i_MarkerStart = "";
    if (i_MarkerEnd == 0) i_MarkerEnd = "";
    if (i_Position == 0) i_Position = "";
    if (i_Type > 5) i_Type = 0;
    if (i_PresetName == 0) i_PresetName = "";

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_CONTROLSOUNDS + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_labels);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_MarkerStart);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_MarkerEnd);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_Position);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_PresetName);
    AIL_mem_printc(i_Event, ';');

    { // loop count
        char w2 = (char)(i_LoopCount / 16);
        if (w2 > 9) w2 = w2 - 10 + 'a';
        else w2 += '0';
        AIL_mem_printc(i_Event, w2);
        char w1 = (char)(i_LoopCount % 16);
        if (w1 > 9) w1 = w1 - 10 + 'a';
        else w1 += '0';
        AIL_mem_printc(i_Event, w1);
        AIL_mem_printc(i_Event, ';');
    }

    // type
    {
        AIL_mem_printc(i_Event, (char)i_Type + '0');
        AIL_mem_printc(i_Event, ';');
    }

    AIL_mem_printf(i_Event, "%f;", i_FadeOutTime);
    AIL_mem_printc(i_Event, (char)i_PresetApplyType + '0');
    AIL_mem_printc(i_Event, ';');

    return 1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF
S32
AILCALL
AIL_add_apply_environment_event_step(
    HMSSEVENTCONSTRUCT i_Event,
    const char* i_EnvName,
    U8 i_IsDynamic
    )
{
    if (i_EnvName == 0) i_EnvName = "";

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_APPLYENV + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_EnvName);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_printc(i_Event, i_IsDynamic ? '1' : '0');
    AIL_mem_printc(i_Event, ';');

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF
S32
AILCALL
AIL_add_persist_preset_event_step(
    HMSSEVENTCONSTRUCT i_Event, 
    const char* i_PresetName, 
    const char* i_PersistName, 
    const char* i_Labels,
    U8 i_Dynamic
    )
{
    if (i_PersistName == 0) return 0;
    if (i_PresetName == 0) i_PresetName = "";
    if (i_Labels == 0) i_Labels = "";

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_PERSIST + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_PresetName);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_PersistName);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_Labels);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_printc(i_Event, '0' + (char)i_Dynamic);
    AIL_mem_printc(i_Event, ';');

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF
S32
AILCALL
AIL_add_sound_limit_event_step(
    HMSSEVENTCONSTRUCT i_Event, 
    char const* i_LimitName,
    const char* i_SoundLimits
    )
{
    if (i_SoundLimits == 0) i_SoundLimits = "";

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_SETLIMITS + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_LimitName);
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_SoundLimits);
    AIL_mem_printc(i_Event, ';');

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF
S32
AILCALL
AIL_add_comment_event_step(
    HMSSEVENTCONSTRUCT i_Event,
    const char* i_Comment
    )
{
    if (i_Comment == 0) i_Comment = "";

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_COMMENT + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_prints(i_Event, i_Comment);
    AIL_mem_printc(i_Event, ';');
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_add_ramp_event_step(
    HMSSEVENTCONSTRUCT i_Event, char const* i_Name, char const* i_Labels,
    F32 i_Time, char const* i_Target, U8 i_Type, U8 i_ApplyToNew, U8 i_InterpolationType
    )
{
    if (i_Name == 0) return 0;
    if (i_Labels == 0) i_Labels = "";

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_RAMP + '0');
    AIL_mem_printf(i_Event, ";%s;%s;%f;%s;%x;%x;%x;",
        i_Name, i_Labels, i_Time, i_Target, i_Type % 16, i_ApplyToNew % 16, i_InterpolationType % 16);
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_add_clear_state_event_step(HMSSEVENTCONSTRUCT i_Event)
{
    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_CLEARSTATE + '0');
    AIL_mem_printc(i_Event, ';');
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_add_enable_limit_event_step(HMSSEVENTCONSTRUCT i_Event, char const* i_LimitName)
{
    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_ENABLELIMIT + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_printf(i_Event, "%s;", i_LimitName);
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_add_set_lfo_event_step(HMSSEVENTCONSTRUCT i_Event, char const* i_Name, 
    char const* i_Base, char const* i_Amp, char const* i_Freq, S32 i_Invert, 
    S32 i_Polarity, S32 i_Waveform, S32 i_DutyCycle, S32 i_IsLFO)
{
    if (i_Name == 0)
        return 0;
    if (i_Base == 0) i_Base = "";
    if (i_Amp == 0) i_Amp = "";
    if (i_Freq == 0) i_Freq = "";

    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_SETLFO + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_printf(i_Event, "%s;%s;%s;%s;%d;%d;%d;%2x;%d;", i_Name, i_Base, i_Amp, i_Freq,
        !!i_Invert, !!i_Polarity, i_Waveform & 3, ((U32)i_DutyCycle) & 255, !!i_IsLFO);

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_add_move_var_event_step(HMSSEVENTCONSTRUCT i_Event, char const* i_Name, 
    const F32 i_Times[2], const S32 i_InterpolationTypes[2], const F32 i_Values[3])
{
    if (i_Name == 0)
        i_Name = "";
    AIL_mem_printc(i_Event, (char)EVENT_STEPTYPE_MOVEVAR + '0');
    AIL_mem_printc(i_Event, ';');
    AIL_mem_printf(i_Event, "%s;%f;%f;%d;%d;%f;%f;%f;", i_Name, 
        i_Times[0], i_Times[1],
        i_InterpolationTypes[0], i_InterpolationTypes[1],
        i_Values[0], i_Values[1], i_Values[2]);

    return 1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF U8* AILCALL AIL_close_event(HMSSEVENTCONSTRUCT i_Event)
{
    // Null term.
    AIL_mem_printc(i_Event, 0);

    void* buf = 0;
    AIL_mem_close(i_Event, &buf, 0);

    return (U8*)buf;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define SETUP_STRING(x) \
    pStep->x.str = (char*)i_EventString;\
    pStep->x.len = 0;\
    while (*i_EventString != ';')\
    {\
        pStep->x.len++;\
        i_EventString++;\
    }\
    i_EventString++;

#define COPY_STRING(x) \
    if (pWriteLocation + pStep->x.len + 1 >= pWriteLimit) \
    {\
        AIL_set_error("Insufficient Buffer Space");\
        return 0;\
    }\
    AIL_memcpy(pWriteLocation, pStep->x.str, pStep->x.len); \
    pStep->x.str = pWriteLocation; \
    pWriteLocation += pStep->x.len; \
    *pWriteLocation = 0; \
    pWriteLocation++; 

#define COPY_UCHAR(x) \
    { \
        char w1 = *i_EventString - '0'; \
        if (w1 > 9) w1 = w1 + '0' - 'a' + 10; \
        i_EventString++; \
        char w2 = *i_EventString - '0'; \
        if (w2 > 9) w2 = w2 + '0' - 'a' + 10; \
        i_EventString++; \
        i_EventString++; \
        pStep->x = w1 * 16 + w2; \
    }

#define COPY_DIGIT(x) \
    { \
        char w1 = *i_EventString - '0'; \
        if (w1 > 9) w1 = w1 + '0' - 'a' + 10; \
        i_EventString++; \
        i_EventString++; \
        pStep->x = w1; \
    }

#define COPY_FLOAT(x) \
    { \
        S32 len = 0; \
        pStep->x = atof_with_len((char*)i_EventString, &len); \
        i_EventString += len + 1; \
    }

#define COPY_USHORT(x) \
    { \
        char d1, d2, d3, d4;\
        d1 = *i_EventString - '0';\
        if (d1 > 9) d1 = d1 + '0' - 'a' + 10;\
        i_EventString++;\
        d2 = *i_EventString - '0';\
        if (d2 > 9) d2 = d2 + '0' - 'a' + 10;\
        i_EventString++;\
        d3 = *i_EventString - '0';\
        if (d3 > 9) d3 = d3 + '0' - 'a' + 10;\
        i_EventString++;\
        d4 = *i_EventString - '0';\
        if (d4 > 9) d4 = d4 + '0' - 'a' + 10;\
        i_EventString++;\
        i_EventString++;\
        pStep->x = d1 * 4096 + d2 * 256 + d3 * 16 + d4;\
    }


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF
const U8*
AILCALL
AIL_next_event_step(const U8* i_EventString, EVENT_STEP_INFO** o_Step, void* i_Buffer, S32 i_BufferSize)
{
    // Empty string!
    if (*i_EventString == 0) return 0;

    // Check for space.
    if ((U32)i_BufferSize < sizeof(EVENT_STEP_INFO))
    {
        AIL_set_error("Insufficient Buffer Space");
        return 0;
    }

    EVENT_STEP_INFO* pStep = (EVENT_STEP_INFO*)i_Buffer;
    char* pWriteLocation = (char*)i_Buffer + sizeof(EVENT_STEP_INFO);
    char* pWriteLimit = (char*)i_Buffer + i_BufferSize;
    *o_Step = 0;
    U32 type = *i_EventString - '0';

    pStep->type = (EVENT_STEPTYPE)type;
    switch (type)
    {
    case EVENT_STEPTYPE_VERSION:
        {
            i_EventString += 2;
            
            S32 len;
            S32 ver = atoi_with_len((char*)i_EventString, 0, &len);

            i_EventString += len + 1;

            if (ver != CURRENT_EVENT_VERSION)
            {
                AIL_set_error("Invalid event version - required version is: " EVENT_VERSION_STRING);
                return 0;
            }

            // Immediately update to the next step.
            if (*i_EventString && i_EventString[0] != '\r' && i_EventString[0] != '\n')
            {
                return AIL_next_event_step(i_EventString, o_Step, i_Buffer, i_BufferSize);;
            }

            return 0; // done with the event.
        }
    case EVENT_STEPTYPE_SETBLEND:
        {
            i_EventString += 2;

            SETUP_STRING(blend.name);
            COPY_STRING(blend.name);

            COPY_DIGIT(blend.count);

            AIL_memset(pStep->blend.inmin, 0, 4 * 10);
            AIL_memset(pStep->blend.inmax, 0, 4 * 10);
            AIL_memset(pStep->blend.outmin, 0, 4 * 10);
            AIL_memset(pStep->blend.outmax, 0, 4 * 10);
            AIL_memset(pStep->blend.minp, 0, 4 * 10);
            AIL_memset(pStep->blend.maxp, 0, 4 * 10);

            for (U8 i = 0; i < pStep->blend.count; i++)
            {
                COPY_FLOAT(blend.inmin[i]);
                COPY_FLOAT(blend.inmax[i]);
                COPY_FLOAT(blend.outmin[i]);
                COPY_FLOAT(blend.outmax[i]);
                COPY_FLOAT(blend.minp[i]);
                COPY_FLOAT(blend.maxp[i]);
            }

            break;
        }
    case EVENT_STEPTYPE_PURGESOUNDS:
    case EVENT_STEPTYPE_CACHESOUNDS:
        {
            // advance past type and semi.
            i_EventString += 2;

            SETUP_STRING(load.lib);
            COPY_STRING(load.lib);

            // Count how many names we have.
            S32 NameCount = 0;
            const U8* pStart = i_EventString;
            while (*i_EventString != ';')
            {
                if (*i_EventString == ':') NameCount++;
                i_EventString++;
            }
            
            // If we have any strings, we need to add 1 for the one past the last :
            if (pStart != i_EventString) NameCount++;

            // We need space in the buffer for NameCount pointers, plus the
            // actual string space.
            S32 StringSpaceNeeded = (S32)(i_EventString - pStart + 1);
            if (pWriteLocation + StringSpaceNeeded + NameCount*sizeof(void*) >= pWriteLimit)
            {
                AIL_set_error("Insufficient Buffer Space");
                return 0;
            }

            // Setup the actual pointer location.
            pStep->load.namecount = NameCount;
            pStep->load.namelist = (const char**)(void*)pWriteLocation;
            pWriteLocation += NameCount*sizeof(void*);

            // Copy the strings.
            AIL_memcpy(pWriteLocation, pStart, StringSpaceNeeded);
            pWriteLocation += StringSpaceNeeded - 1;
            *pWriteLocation = 0;
            pWriteLocation++;

            // Null terminate the individual names.
            char* pWriter = pWriteLocation - StringSpaceNeeded;
            char* pTrailer = pWriter;
            S32 CurrentString = 0;
            while (*pWriter)
            {
                if (*pWriter == ':') 
                {
                    pStep->load.namelist[CurrentString] = pTrailer;
                    pTrailer = pWriter + 1;
                    CurrentString++;
                    *pWriter = 0;
                }
                pWriter++;
            }

            if (pWriter != pTrailer)
            {
                // Still have one to save.
                pStep->load.namelist[CurrentString] = pTrailer;
            }

            i_EventString++; // eat the ;

            break;
        }
    case EVENT_STEPTYPE_SETLIMITS:
        {
            i_EventString += 2;

            SETUP_STRING(limits.name);
            COPY_STRING(limits.name);

            SETUP_STRING(limits.limits);
            COPY_STRING(limits.limits);

            break;
        }
    case EVENT_STEPTYPE_CLEARSTATE:
        {
            i_EventString += 2;
            break;
        }
    case EVENT_STEPTYPE_ENABLELIMIT:
        {
            i_EventString += 2;

            SETUP_STRING(enablelimit.limitname);
            COPY_STRING(enablelimit.limitname);
            break;
        }
    case EVENT_STEPTYPE_COMMENT:
        {
            // advance past type and semi.
            i_EventString += 2;

            SETUP_STRING(comment.comment);
            COPY_STRING(comment.comment);

            break;
        }
    case EVENT_STEPTYPE_RAMP:
        {
            i_EventString += 2;

            SETUP_STRING(ramp.name);
            COPY_STRING(ramp.name);

            SETUP_STRING(ramp.labels);
            COPY_STRING(ramp.labels);

            COPY_FLOAT(ramp.time);

            SETUP_STRING(ramp.target);
            COPY_STRING(ramp.target);

            COPY_DIGIT(ramp.type);
            COPY_DIGIT(ramp.apply_to_new);
            COPY_DIGIT(ramp.interpolate_type);

            break;
        }

    case EVENT_STEPTYPE_APPLYENV:
        {
            // advance past type and semi.
            i_EventString += 2;

            SETUP_STRING(env.envname);
            COPY_STRING(env.envname);

            COPY_DIGIT(env.isdynamic);

            break;
        }
    case EVENT_STEPTYPE_CONTROLSOUNDS:
        {
            i_EventString += 2;

            SETUP_STRING(control.labels);
            COPY_STRING(control.labels);

            SETUP_STRING(control.markerstart);
            COPY_STRING(control.markerstart);

            SETUP_STRING(control.markerend);
            COPY_STRING(control.markerend);

            SETUP_STRING(control.position);
            COPY_STRING(control.position);

            SETUP_STRING(control.presetname);
            COPY_STRING(control.presetname);

            COPY_UCHAR(control.loopcount);

            COPY_DIGIT(control.type);

            COPY_FLOAT(control.fadeouttime);

            COPY_DIGIT(control.presetapplytype);

            break;
        }
    case EVENT_STEPTYPE_PERSIST:
        {
            i_EventString += 2;

            SETUP_STRING(persist.presetname);
            COPY_STRING(persist.presetname);

            SETUP_STRING(persist.name);
            COPY_STRING(persist.name);

            SETUP_STRING(persist.labels);
            COPY_STRING(persist.labels);

            COPY_DIGIT(persist.isdynamic);

            break;
        }
    case EVENT_STEPTYPE_EXECEVENT:
        {
            i_EventString += 2;
            SETUP_STRING(exec.eventname);
            COPY_STRING(exec.eventname);

            break;
        }
    case EVENT_STEPTYPE_SETLFO:
        {
            i_EventString += 2;

            SETUP_STRING(setlfo.name);
            COPY_STRING(setlfo.name);

            SETUP_STRING(setlfo.base);
            COPY_STRING(setlfo.base);

            SETUP_STRING(setlfo.amplitude);
            COPY_STRING(setlfo.amplitude);

            SETUP_STRING(setlfo.freq);
            COPY_STRING(setlfo.freq);

            COPY_DIGIT(setlfo.invert);
            COPY_DIGIT(setlfo.polarity);
            COPY_DIGIT(setlfo.waveform);
            COPY_UCHAR(setlfo.dutycycle);
            COPY_DIGIT(setlfo.islfo);
            
            break;
        }
    case EVENT_STEPTYPE_MOVEVAR:
        {
            i_EventString += 2;

            SETUP_STRING(movevar.name);
            COPY_STRING(movevar.name);

            COPY_FLOAT(movevar.time[0]);
            COPY_FLOAT(movevar.time[1]);
            COPY_DIGIT(movevar.interpolate_type[0]);
            COPY_DIGIT(movevar.interpolate_type[1]);
            COPY_FLOAT(movevar.value[0]);
            COPY_FLOAT(movevar.value[1]);
            COPY_FLOAT(movevar.value[2]);
            break;
        }
    case EVENT_STEPTYPE_STARTSOUND:
        {
            // advance past type and semi.
            i_EventString += 2;

            SETUP_STRING(start.soundname);
            COPY_STRING(start.soundname);

            SETUP_STRING(start.presetname);
            COPY_STRING(start.presetname);

            SETUP_STRING(start.eventname);
            COPY_STRING(start.eventname);

            SETUP_STRING(start.markerstart);
            COPY_STRING(start.markerstart);

            SETUP_STRING(start.markerend);
            COPY_STRING(start.markerend);

            SETUP_STRING(start.labels);
            COPY_STRING(start.labels);

            SETUP_STRING(start.statevar);
            COPY_STRING(start.statevar);

            SETUP_STRING(start.varinit);
            COPY_STRING(start.varinit);

            COPY_DIGIT(start.stream);
            COPY_DIGIT(start.canload);
            COPY_DIGIT(start.presetisdynamic);

            COPY_USHORT(start.delaymin);

            COPY_USHORT(start.delaymax);

            COPY_UCHAR(start.priority);

            COPY_UCHAR(start.loopcount);


            SETUP_STRING(start.startoffset);
            COPY_STRING(start.startoffset);

            COPY_FLOAT(start.volmin);
            COPY_FLOAT(start.volmax);
            COPY_FLOAT(start.pitchmin);
            COPY_FLOAT(start.pitchmax);

            COPY_FLOAT(start.fadeintime);

            COPY_DIGIT(start.evictiontype);
            COPY_DIGIT(start.selecttype);

            break;
        }
#ifndef __RADFINAL__
    default: MSSBreakPoint();
#endif
    }

    *o_Step = pStep;

    if (*i_EventString && i_EventString[0] != '\r' && i_EventString[0] != '\n')
    {
        return i_EventString;
    }

    return 0;
}

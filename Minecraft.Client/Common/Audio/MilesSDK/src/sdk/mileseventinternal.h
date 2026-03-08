//
// Header file for event system related source files.
//

#include <math.h>
#include <stdarg.h> // for debug string creation
#include <stdio.h> // for debug string creation

#if !defined(IS_WIN32API)
#define _vsnprintf vsnprintf
#endif

#include "stringbuilder.h"

static S32 char_match( char n, char w )
{
  if ( w == '?' ) return 1;   // ? match anything
  if ( ( n == '/' ) && ( w == '\\' ) ) return 1;
  if ( ( n == '\\' ) && ( w == '/' ) ) return 1;

  if ( ( n >= 'a' ) && ( n <= 'z' ) ) n = n - 'a' + 'A';
  if ( ( w >= 'a' ) && ( w <= 'z' ) ) w = w - 'a' + 'A';

  return( n == w );
}

#include "rrwildcard.inl"



static inline F32 MSS_dB_to_scalar(F32 dB)      // turns dBfs into linear DAC scalar (-6 dB = 0.5F, -12 dB = 0.25F...)
{
   return (F32) pow(10.0F, dB / 20.0F);
}

static inline F32 MSS_sT_to_scalar(F32 sT)      // turns semitones into linear scalar
{
   return (F32) pow(2.0F, sT / 12.0F);
}


F32 atof_with_len(const char* i_source, S32* o_count);



//! Max number of bytes an event can occupy.
#define MAX_EVENT_SIZE 16*1024

//! Types of errors passed to ErrorHandler
enum ErrorType
{
    eOutOfMem,
    eOutOfHandleMem,
    eMilesError,
    eBadSetLimit,
    eCommandBufferFull,
    eMissingVar,
    eMissingAsset,
    eAsyncLoadFail,
    eTooManyBlends,
    eBlendDoesntExist,
    eEnqueueBufferFull,
    eMissingSelection
};


// Loaded State
//-----------------------------------------------------------------------------
static const S32 MAX_ASYNCS = 4;

struct EventSystem
{
    U32 Tag; // 'ESYS'
    EventSystem* Next;

    PODHandleArray<Handle> SoundSources;
    PODHandleArray<Handle> SoundInstances;
    Handle RampRoot;
    Handle VarRoot;
    Handle BlendRoot;
    Handle PersistRoot;
    Handle LimitRoot;
    Handle StateVarRoot;
    Handle LFORoot;
    Handle MoveRoot;
    Handle BusRoot;

    HDIGDRIVER Driver;

    Handle CurrentEnv;
    S32 EnvIsDynamic;
    

    // Loaded data for this event system.
    S32 SoundDataSize;
};

extern MilesAsyncRead   s_Asyncs[MAX_ASYNCS];
extern S32              s_AsyncUsed[MAX_ASYNCS]; // nonzero if the async struct is in use.
extern S32              s_AsyncCancelled[MAX_ASYNCS]; // nonzero if the async struct has been cancelled and is waiting for that.
extern EventSystem*     s_AsyncContext[MAX_ASYNCS];
extern S32              s_AsyncId;


//! Sound data, to be used for starting instances of sounds.
struct SoundSource
{
    Handle m_Name;
    HPROVIDER m_Provider;
    void* m_Data;
    S32 m_NameLen;
    S32 m_DataLen;
    S32 m_AsyncFailed;
    S32 m_RefCount;
    S32 m_AsyncIndex;
    S32 m_AsyncQueueId;
    S32 m_SoundLimit;
    S32 m_SoundCount; // Separate from RefCount because Cache events can affect refcount.
    F32 m_SoundMixDAC;
    char m_Ext[8];
};

void             Sound_Free(EventSystem* i_System, Handle i_SoundHandle);
Handle           Sound_Load(EventSystem* i_System, U64 i_Id, const char* i_Name);
Handle           Sound_Find(EventSystem* i_System, const char* i_Name);


//! A pending, completed, or playing sound instance in the system.
struct SoundInstance
{
    U64 m_EventId;
    U64 m_ParentId;
    U64 m_SoundId;
    HSAMPLE m_Sample;
    HSTREAM m_Stream;
    EventSystem* m_System;
    Handle m_CompletionEvent;
    Handle m_Source;
    Handle m_Name;
    Handle m_Labels;
    Handle m_RampRoot;
    Handle m_VarRoot;
    Handle m_Blend;
    Handle m_DynamicPreset;
    Handle m_InitializationBlock;
    S32 m_InitBlockLen;
    S32 m_UserBufferOffset;
    S32 m_NameLen;
    S32 m_Status;
    S32 m_StartTime;
    S32 m_StartOffset;
    S32 m_StartOffsetIsMS;
    S32 m_LoopStart;
    S32 m_LoopEnd;
    F32 m_Pitch;
    F32 m_Volume;
    S32 m_Delay;
    F32 m_FadeVolume;
    F32 m_FadeDeltaPerSecond;
    F32 m_FadeDelay;
    U16 m_LoopLookup; // whether to use tho look bounds as samples after stream start.
    U8 m_Priority;
    U8 m_Flags;
    S8 m_BlendIndex; // Our index in to the blend array.
};

struct PersistedPreset
{
    Handle Next;
    Handle Self;
    Handle Preset;
    Handle Labels;
    S32 Dynamic;
    char Name[1];
};


// Preset Persistence
//-----------------------------------------------------------------------------
S32     Persist_Create(EventSystem* i_System, U64 i_Id, const EVENT_STEP_INFO* i_Step);
void    Persist_Free(EventSystem* i_System, U64 i_Id, const char* i_Name);
void    Persist_Apply(EventSystem* i_System, SoundInstance* i_Instance, HSAMPLE i_Sample, const char* i_Labels, S32 i_LabelLen, S32 i_DynamicCheck);

struct SoundLimit
{
    Handle Next;
    Handle Self;
    Handle Entries;
    char Name[1];
};

struct SoundLimitEntry
{
    Handle Next;
    Handle Self;
    S16 Limit;
    S16 Count;
    char Label[1];
};

S32     Limit_Add(EventSystem* i_System, EVENT_STEP_INFO const* i_Step, S64 i_Id);

struct LFO
{
    Handle Next;
    Handle Self;
    float Base, Amp, Freq;
    Handle BaseStr, AmpStr, FreqStr;
    float Time;
    U8 Invert, Waveform, Polarity, DutyCycle;
    char Var[1];
};

S32     LFO_Add(EventSystem* i_System, EVENT_STEP_INFO const* i_Step);

struct MoveVar
{
    Handle Next, Self;
    F32 Values[3];
    F32 Times[2];
    S32 InterpTypes[2];
    F32 CurrentTime;
    char Name[1];
};

struct Bus
{
    Handle Next, Self;
    Handle Preset;
    HSAMPLE Sample;
    U8 Index;
    U8 PresetApplied;   // We can't apply the preset during settings setup since the bank isn't fully loaded yet.
    U8 Dynamic;
    char Labels[1];
};


// Ramps
//-----------------------------------------------------------------------------

#define RAMPTYPE_VOLUME 0
#define RAMPTYPE_WET 1
#define RAMPTYPE_LOWPASS 2
#define RAMPTYPE_RATE 3

#define RAMPINT_LINEAR 0
#define RAMPINT_EXP 1
#define RAMPINT_SCURVE 2

struct Ramp
{
    Handle Next;
    Handle Self;

    float CurrentTime;
    float TotalTime;
    float StartValue;
    float BakedValue;
    S32 IsBaked;

    S32 InterpolationType;
    S32 ValueType;
    char Target[64];
    char Name[64];
    char Labels[256];
};

F32      Ramp_GetCurrentValue(EventSystem* i_System, Ramp* i_Ramp, S32 i_ConvertToScalars);
void     Ramp_ApplyAll(EventSystem* i_System, SoundInstance* i_Sound, F32 i_TimeDelta);
S32      Ramp_Add(Handle* i_Root, EventSystem* i_System, EVENT_STEP_INFO const* i_Step);

// Vars
//-----------------------------------------------------------------------------
struct VarGraph
{
    F32 Min, Max;
    S32 InfLow, InfHigh;
    S32 PointCount;
    MSSGRAPHPOINT Points[1];
};

struct VarDef
{
    Handle Next, Self;
    union 
    {
        F32 f;
        S32 i;
    };
    S32 IsFloat;
    Handle Graph;
    char Name[1];
};

S32      Var_LookupWithContext(EventSystem* i_System, Handle i_Root, char const* i_Variable, S32 i_WantFloat, void* o_Value);
void     Var_SetInstance(Handle* i_Root, char const* i_Name, void* i_Value, S32 i_IsFloat);


// StateVar
//-----------------------------------------------------------------------------
struct StateVar
{
    Handle Next, Self;
    void const* Key;
    S32 Value;
};

S32      StateVar_Lookup(EventSystem* System, void const* Key);
void     StateVar_Set(EventSystem* System, void const* Key, S32 Value);



// Blends
//-----------------------------------------------------------------------------
#define MAX_BLEND_SOUNDS 10
struct BlendedSound
{
    Handle Next;
    Handle Self;
    float InMin[MAX_BLEND_SOUNDS];
    float InMax[MAX_BLEND_SOUNDS];
    float OutMin[MAX_BLEND_SOUNDS];
    float OutMax[MAX_BLEND_SOUNDS];
    float MinP[MAX_BLEND_SOUNDS];
    float MaxP[MAX_BLEND_SOUNDS];
    char Name[1];
};

void    Blend_Apply(SoundInstance* i_Sound, float* o_Volume, float* o_Pitch);
S32     Blend_Add(EventSystem* i_System, EVENT_STEP_INFO const* i_Step);
S32     Blend_Exists(EventSystem* i_System, char const* i_Name);

// Falloff Graph
//-----------------------------------------------------------------------------

struct FalloffGraph
{
    S32 Affects; // bit 0 - vol, 1 - lp, 2 - exc.
    F32 MaxDistance;
    S32 VolCount, ExcCount, LPCount;

    MSSGRAPHPOINT Points[1]; // Volume, LowPass, then Exclusion
};
void    FalloffGraph_Set(HSAMPLE S, S32 Affects, FalloffGraph* G);


// Misc
//-----------------------------------------------------------------------------

#define TIME_SAMPLES_MASK   0x4
#define TIME_MS_MASK        0x2
#define TIME_BYTES_MASK     0x1
#define TIME_MASK           0x7

#define LOOP_START_SHIFT    0
#define LOOP_END_SHIFT      3
#define LOOP_INIT_SHIFT     6
S32 CheckLabelMatchLabelParsed(void const * parsed_lbls, char const * labels, char const * query, U32 query_len, char query_terminator);
S32 CheckLabelMatchQueryParsed(char const * labels, U32 labels_len, void const * parsed_qry, char const * query);
S32 OnlyNum(char const* s);
S32 strtoi(const char* i_str, const char** o_terminator);
S32 FindMarker(EventSystem* i_System, SoundInstance* i_Instance, SINTa i_MarkerList, const char* i_Name, S32* o_Offset, S32* o_IsTime);
void ErrorHandler(ErrorType i_Error, U64 i_Id, char const* i_Details);
S32              EvictSound(EventSystem* i_System, U64 i_InFavorIf, U8 i_EvictType, S32 i_Priority, SoundLimitEntry* i_Limit, SoundSource* i_Source, char const* i_Name);
SoundLimitEntry* IsLabelLimited(EventSystem* i_System, MSSSTRINGC i_Labels);
void             DestroySoundInstance(EventSystem* i_System, SoundInstance* i_Sound);
S32 EvalFunc(void* context, char const* exp, S32 explen, void* output, S32 isfloat);

// Command Queue
//-----------------------------------------------------------------------------
struct QueuedEvent
{
    S32 size;
    S32 reservedlen;
    const U8* event;
    HEVENTSYSTEM context;
    U64 eventid;
    U64 eventfilter;
    U64 parentid;
    S32 flags;
    S32 bufferlen;
    char buffer[4];
};

struct VarEntry
{
    S32 tag;
    S32 advance_len;
    F32 value;
    char name[4];
};

struct UserBufferEntry
{
    S32 tag;
    S32 offset_amt;
    S32 is_ptr;
    S32 len;
    char buf[sizeof(UINTa)];
};
 
// These values mirrored in studio/MssAudition.cpp
#define BUFFER_END 0
#define BUFFER_POSITION 1
#define BUFFER_SELECT 2
#define BUFFER_USER 3
#define BUFFER_VAR 4
#define BUFFER_FILTER 5
#define BUFFER_NAME 6
#define BUFFER_EVENT 7
#define BUFFER_CONTEXT 8
#define BUFFER_PARENT 9
#define BUFFER_CONTROL 10
#define BUFFER_VELOCITY 11

U64             Queue_EnumCreate();
QueuedEvent*    Queue_EnumNext(U64* io_Enum, QueuedEvent* i_Last);
void            Queue_EnumClose(U64 i_OriginalEnum, U64 i_Enum);

QueuedEvent*    Queue_Add(S32 i_UserBufferLen);
void            Queue_Commit(QueuedEvent* i_Event); // Completes an add operation so the item appears in enumeration.

void            Queue_SetMem(S32 amount, Handle memory);

// Private function for queueing from an event.
U64             AIL_enqueue_event_from_init_block(char const* i_NewName, void* i_InitBlock, S32 i_InitBlockLen, U64 parentid);

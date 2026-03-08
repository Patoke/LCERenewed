#include "mss.h"
#include "imssapi.h"

#include "mileseventhandle.h"
#include "mileseventarray.h"
#include "rrThreads.h"
#include "rrAtomics.h"

#include "mileseventinternal.h"



static Handle       s_CommandBuffer;
static S32          s_CommandBufferSize;
static RAD_ALIGN(U64 volatile, s_CommandBufferControls, 8) = 0;
static RAD_ALIGN(U64 volatile, s_NextEventId, 8) = 1;

static volatile U32 enqueue_mask;
static char enqueue_buffer[4096];
#define CHUNK_SIZE (512)
#define MAX_BITS (4096 / CHUNK_SIZE)

void Queue_SetMem(S32 amount, Handle memory)
{
    s_CommandBufferSize = amount;
    s_CommandBuffer = memory;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
QueuedEvent* Queue_EnumNext(U64* io_Enum, QueuedEvent* i_Last)
{
    // Wipe the last one, so the sentinel is set correctly.
    if (i_Last) AIL_memset(i_Last, 0, i_Last->size);

    // ValidByteCount is a running count of the remaining valid bytes.
    U32 ReadOffset = (U32)(*io_Enum >> 32);
    U32 ValidByteCount = (U32)*io_Enum;

    if (ReadOffset >= (U32)s_CommandBufferSize) ReadOffset -= s_CommandBufferSize;

    //
    // Enumeration Cases
    //
    // a) The offset + length could extend beyond the length of the buffer, but
    //      still be under the length of the buffer.
    // b) Offset + Length could fit normally.
    // c) Offset + Length could fit normally, but are marked as zero size and thus not valid data yet.

    // Sanity...
#ifdef DEBUG
    if (ValidByteCount > (U32)s_CommandBufferSize)
    {
        MSSBreakPoint();
    }
#endif

    char* pCommandBuffer = (char*)Handle_Get(s_CommandBuffer);
    while (ValidByteCount)
    {
        QueuedEvent* pEvent = (QueuedEvent*)(pCommandBuffer + ReadOffset);
        register U32 EventSize = pEvent->size;
        if (EventSize == 0)
        {
            // Handle case C - however this data still remains in the validbytecount.
            return 0;
        }

#ifdef DEBUG
        if (EventSize > ValidByteCount)
        {
            MSSBreakPoint();
            return 0;
        }
#endif

        // Case A: The item wraps around the buffer, and should be ignored.
        if (ReadOffset < (U32)s_CommandBufferSize &&
            ReadOffset + pEvent->size >= (U32)s_CommandBufferSize)
        {
            // We need to ignore this item and move on to the next.
            ReadOffset += pEvent->size;
            ReadOffset -= s_CommandBufferSize;
            ValidByteCount -= pEvent->size;
            continue;
        }

        // Update the enumeration to the next item.
        {
            ReadOffset += pEvent->size;
            ValidByteCount -= pEvent->size;

            // wrap..
            if (ReadOffset >= (U32)s_CommandBufferSize) ReadOffset -= s_CommandBufferSize;

            U64 NewEnum = ValidByteCount | ((U64)ReadOffset << 32);
            *io_Enum = NewEnum;
        }
        return pEvent;
    }

    return 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Queue_EnumClose(U64 i_OriginalEnum, U64 i_Enum)
{
    // We may have bailed before we consumed all of the bytes due to an
    // unset sentinel.
    U32 BytesConsumed = (U32)i_OriginalEnum - (U32)i_Enum;
    U32 NewReadOffset = (U32)(i_Enum >> 32);

    // Try to update the markers.
    U64 Old = rrAtomicLoadAcquire64( &s_CommandBufferControls );
    for (;;)
    {
        U64 Compare = Old;
        U64 New = ((U64)NewReadOffset << 32) | (((U32)Old) - BytesConsumed);

        Old = rrAtomicCmpXchg64(&s_CommandBufferControls, New, Compare);
        if (Old == Compare)
        {
            break;
        }
    }

}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
U64 Queue_EnumCreate()
{
    // must be an atomic load
    return rrAtomicLoadAcquire64( &s_CommandBufferControls );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
U32 AILEXPORT AIL_event_system_command_queue_remaining()
{
    return (U32)s_CommandBufferSize - (U32)(rrAtomicLoadAcquire64( &s_CommandBufferControls ));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
QueuedEvent* Queue_Add(S32 i_UserBufferLen)
{
    S32 MemNeeded = sizeof(QueuedEvent);
    MemNeeded += i_UserBufferLen;

    // Round to alignment for the structure.
    MemNeeded = (MemNeeded + sizeof(S64) - 1) & ~(sizeof(S64) - 1);

    //
    // Reserve a slot in the queue
    //
    U64 NewControls = 0;
    U64 OldControls = rrAtomicLoadAcquire64( &s_CommandBufferControls );
    for (;;)
    {
        U64 Comparator;

        // Mask off the read offset to update the amount avail.
        NewControls = (OldControls & ~0x80000000LL) + MemNeeded;
        Comparator = OldControls;

        // See if there's room in the command queue real quick.
        if ((U32)NewControls >= (U32)s_CommandBufferSize)
        {
            // We haven't actually set the new position yet, so we don't have to revert anything.
            ErrorHandler(eCommandBufferFull, 0, 0);
            return 0;
        }

        OldControls = rrAtomicCmpXchg64(&s_CommandBufferControls, NewControls, Comparator);
        if (OldControls == Comparator)
        {
            break;
        }
    }

    // Split the controls.
    U32 ReadOffset = (U32)(NewControls >> 32);
    U32 ValidByteCount = (U32)NewControls;

    // Note that ValidByteCount *contains* our memory request from above.

    // Check if our start point is before the end, but we end after the end.
    // In this case we straddle the wrap point and need to just insert a 4 byte
    // placeholder.
    S32 WriteLocation = ReadOffset + ValidByteCount - MemNeeded;
    if (WriteLocation < s_CommandBufferSize &&
        WriteLocation + MemNeeded >= s_CommandBufferSize)
    {
        // Still save the size, so the process loop knows how to loop.
        QueuedEvent* pEvent = (QueuedEvent*)((char*)Handle_Get(s_CommandBuffer) + WriteLocation);
        pEvent->size = MemNeeded;

        return Queue_Add(i_UserBufferLen);
    }

    //
    // We fit normally, but we might have a starting point that needs to get
    // modded down to the base addy, as WriteLocation might be > s_CommandBufferSize
    //
    if (WriteLocation >= s_CommandBufferSize) WriteLocation -= s_CommandBufferSize;

    QueuedEvent* pEvent = (QueuedEvent*)((char*)Handle_Get(s_CommandBuffer) + WriteLocation);
    pEvent->reservedlen = i_UserBufferLen;

    // We can't mark the size until we have written the entire item.
    //pEvent->size = MemNeeded;
    return pEvent;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Queue_Commit(QueuedEvent* i_Event)
{
    S32 MemNeeded = sizeof(QueuedEvent);
    MemNeeded += i_Event->reservedlen;

    // Round to alignment for the structure.
    MemNeeded = (MemNeeded + sizeof(S64) - 1) & ~(sizeof(S64) - 1);

    // Ensure the write is atomic and not moved around.
    rrAtomicStoreRelease32((volatile U32*)&i_Event->size, MemNeeded);
    return;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void AILCALL MilesClearEventQueueInternal()
{
    // Traverse the command queue
    U64 EnumToken = Queue_EnumCreate();
    U64 SavedEnum = EnumToken;

    QueuedEvent* pEvent = 0;

    for (pEvent = Queue_EnumNext(&EnumToken, pEvent); pEvent; pEvent = Queue_EnumNext(&EnumToken, pEvent))
    {
        // Free the event if we own it.
        if (pEvent->flags & MILESEVENT_ENQUEUE_FREE_EVENT)
        {
            AIL_mem_free_lock((void*)pEvent->event);
        }
    }

    // Mark completion of processing.
    Queue_EnumClose(SavedEnum, EnumToken);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT AIL_enqueue_event_start()
{
    S32 got_bit = -1;
    U32 new_mask = 0;
    U32 old_mask = rrAtomicLoadAcquire32(&enqueue_mask);
    for (;;)
    {
        U32 comp_mask = old_mask;

        // Find a bit to check.
        got_bit = -1;
        for (S32 i = 0; i < MAX_BITS; i++)
        {
            if ((old_mask & (1 << i)) == 0)
            {
                new_mask = old_mask |= (1 << i);
                got_bit = i;
                break;
            }
        }

        if (got_bit == -1)
        {
            // We failed to find a bit - spin until we can.
            rrThreadSleep(0);
            old_mask = rrAtomicLoadAcquire32(&enqueue_mask);
            continue;
        }

        // try to set the new mask
        old_mask = rrAtomicCmpXchg32(&enqueue_mask, new_mask, comp_mask);
        if (old_mask == comp_mask)
            break;
    }

    // return + 1 so that zero is a failure.
    return got_bit + 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void ClearQueueToken(S32 token) // token has been decremented already...
{
    U32 new_mask = 0;
    U32 old_mask = rrAtomicLoadAcquire32( &enqueue_mask);
    for (;;)
    {
        U32 comp_mask = old_mask;

        new_mask = new_mask & (~(1 << token));

        // try to set the new mask
        old_mask = rrAtomicCmpXchg32(&enqueue_mask, new_mask, comp_mask);
        if (old_mask == comp_mask)
            break;
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC void AILEXPORT AIL_enqueue_event_cancel(S32 token)
{
    ClearQueueToken(token);
}

#define NULL_TERM if (buffer + total_len + 4 < buffer_end) *(U32*)(buffer + total_len) = 0;

#define CREATE_TOKEN ((token + 1) | ((cur_offset + total_len) << 16))

#define INIT_PTR(bytes_needed) \
    if (ptoken == 0) return 0; \
    S32 token = *ptoken; \
    S32 cur_offset = token >> 16; token &= 0xffff; \
    if (token == 0 || token >= MAX_BITS) \
    { \
        *ptoken = 0; \
        return 0; \
    } \
    token--; char* buffer = enqueue_buffer + token*CHUNK_SIZE; char* buffer_end = buffer + CHUNK_SIZE; buffer += cur_offset; \
    S32 total_len = bytes_needed; \
    if (buffer + total_len >= buffer_end) \
    { \
        ErrorHandler(eEnqueueBufferFull, 0, 0); \
        ClearQueueToken(token); \
        *ptoken = 0; \
        return 0; \
    }


#define COMPLETE_PTR NULL_TERM; *ptoken = CREATE_TOKEN; return 1;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC S32 AILEXPORT AIL_enqueue_event_position(S32* ptoken, F32 x, F32 y, F32 z)
{
    INIT_PTR(20);

    *(U32*)(buffer) = BUFFER_POSITION;
    *(U32*)(buffer + 4) = 20;
    *(F32*)(buffer + 8) = x;
    *(F32*)(buffer + 12) = y;
    *(F32*)(buffer + 16) = z;

    COMPLETE_PTR;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC S32 AILEXPORT AIL_enqueue_event_velocity(S32* ptoken, F32 vx, F32 vy, F32 vz, F32 mag)
{
    INIT_PTR(24);

    *(U32*)(buffer) = BUFFER_VELOCITY;
    *(U32*)(buffer + 4) = 24;
    *(F32*)(buffer + 8) = vx;
    *(F32*)(buffer + 12) = vy;
    *(F32*)(buffer + 16) = vz;
    *(F32*)(buffer + 20) = mag;

    COMPLETE_PTR;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC S32 AILEXPORT AIL_enqueue_event_buffer(S32* ptoken, void* user_buffer, S32 user_buffer_len, S32 user_buffer_is_ptr)
{
    if (user_buffer_is_ptr)
        user_buffer_len = sizeof(UINTa);

    S32 bytes_needed = user_buffer_len;
    bytes_needed += sizeof(UserBufferEntry) - sizeof(UINTa);
    bytes_needed = (bytes_needed + 3) & ~3;

    INIT_PTR(bytes_needed);

    {
        UserBufferEntry* E = (UserBufferEntry*)buffer;

        E->tag = BUFFER_USER;
        E->is_ptr = user_buffer_is_ptr;
        E->len = user_buffer_len;
        E->offset_amt = total_len;

        if (user_buffer_is_ptr)
            AIL_memcpy(E->buf, &user_buffer, user_buffer_len);
        else
            AIL_memcpy(E->buf, user_buffer, user_buffer_len);
    }

    COMPLETE_PTR;
}

struct NameEntry
{
    S32 tag;
    S32 advance_len;
    char name[4];
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 AIL_enqueue_event_name(S32* ptoken, char const* i_Name)
{
    S32 bytes_needed = (S32)AIL_strlen(i_Name) + 1;
    bytes_needed += sizeof(NameEntry) - 4;
    bytes_needed = (bytes_needed + 3) & ~3;

    INIT_PTR(bytes_needed);

    NameEntry* E = (NameEntry*)buffer;
    E->tag = BUFFER_NAME;
    E->advance_len = total_len;
    AIL_strcpy(E->name, i_Name);

    COMPLETE_PTR;
}

struct EventEntry
{
    S32 tag;
    S32 advance_len;
    S32 free_event;
    void* event_ptr;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 AIL_enqueue_event_raw(S32* ptoken, void* i_Event, S32 i_FreeEvent)
{
    S32 bytes_needed = sizeof(EventEntry);

    INIT_PTR(bytes_needed);

    EventEntry* E = (EventEntry*)buffer;
    E->tag = BUFFER_EVENT;
    E->advance_len = sizeof(EventEntry);
    E->event_ptr = i_Event;
    E->free_event = i_FreeEvent;

    COMPLETE_PTR;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC S32 AILEXPORT  AIL_enqueue_event_context(S32* ptoken, HEVENTSYSTEM context)
{
    S32 bytes = sizeof(void*) + 8;

    INIT_PTR(bytes);

    *(U32*)buffer = BUFFER_CONTEXT;
    *(U32*)(buffer + 4) = sizeof(void*) + 8;
    *(void**)(buffer + 8) = context;

    COMPLETE_PTR;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC S32 AILEXPORT  AIL_enqueue_event_filter(S32* ptoken, U64 filter)
{
    S32 bytes = 16;

    INIT_PTR(bytes);

    *(U32*)buffer = BUFFER_FILTER;
    *(U32*)(buffer + 4) = 16;
    *(U64*)(buffer + 8) = filter;

    COMPLETE_PTR;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC S32 AILEXPORT AIL_enqueue_event_selection(S32* ptoken, U32 selection)
{
    S32 bytes = 12;

    INIT_PTR(bytes);

    *(U32*)buffer = BUFFER_SELECT;
    *(U32*)(buffer + 4) = bytes;
    *(U32*)(buffer + 8) = selection;
    COMPLETE_PTR;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 AIL_enqueue_event_parent(S32* ptoken, U64 parentid)
{
    INIT_PTR(16);
    *(U32*)buffer = BUFFER_PARENT;
    *(U32*)(buffer + 4) = 16;
    *(U64*)(buffer + 8) = parentid;
    COMPLETE_PTR
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 AIL_enqueue_event_control(S32* ptoken)
{
    INIT_PTR(8);
    *(U32*)buffer = BUFFER_CONTROL;
    *(U32*)(buffer + 4) = 8;
    COMPLETE_PTR;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC S32 AILEXPORT AIL_enqueue_event_variablef(S32* ptoken, char const* varname, F32 value)
{
    S32 bytes_needed = (S32)AIL_strlen(varname) + 1;
    bytes_needed += sizeof(VarEntry) - 4;
    bytes_needed = (bytes_needed + 3) & ~3;

    INIT_PTR(bytes_needed);

    VarEntry* E = (VarEntry*)buffer;
    E->tag = BUFFER_VAR;
    E->advance_len = total_len;
    E->value = value;
    AIL_strcpy(E->name, varname);

    COMPLETE_PTR;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
U64 AIL_enqueue_event_from_init_block(char const* i_NewName, void* i_InitBlock, S32 i_InitBlockLen, U64 parentid)
{
    // The name of the event is always queued last, so we find the offset to the name, and the copy 
    // the rest of the info straight in.

    void* context = 0;
    U64 filter = 0;
    S32 control = 0;
    char* name = 0;

    {
        U32 buffer_token = 0;
        char* buffer = (char*)i_InitBlock;
        char* buffer_end = buffer + i_InitBlockLen;

        buffer_token = *(U32*)buffer;
        while (buffer_token)
        {
            U32 advance_amt = *(U32*)(buffer + 4);

            // Get any data we need.
            switch (buffer_token)
            {
            case BUFFER_NAME: name = buffer; break;
            case BUFFER_FILTER: filter = *(U64*)(buffer + 8);break;
            case BUFFER_CONTEXT: context = *(void**)(buffer + 8); break;
            case BUFFER_CONTROL: control = 1; break;
            }

            buffer += advance_amt;

            if (buffer < buffer_end)
                buffer_token = *(U32*)buffer;
            else 
                break;
        }
    }

    // If we didin't get a name, then this is an unnamed event with a completion event, which we no longer support.
    if (name == 0)
        return 0;

    S32 NewBlockLen = (S32)(name - (char*)i_InitBlock);
    S32 NameEntryLen = sizeof(NameEntry);
    NameEntryLen += (S32)AIL_strlen(i_NewName) - 4;
    NameEntryLen = (NameEntryLen + 3) & ~3;
    NewBlockLen += NameEntryLen;

    QueuedEvent* E = Queue_Add(NewBlockLen);
    if (E == 0)
        return 0;

    NameEntry* NE = (NameEntry*)AIL_ptr_add(E->buffer, name - (char*)i_InitBlock);
    NE->advance_len = NameEntryLen;
    NE->tag = BUFFER_NAME;
    strcpy(NE->name, i_NewName);

    U64 qid = rrAtomicAddExchange64(&s_NextEventId, 1);

    E->flags = MILESEVENT_ENQUEUE_BY_NAME;
    E->flags |= control ? 0x8 : 0;

    E->eventid = qid;
    E->context = context;
    E->eventfilter = filter;
    E->parentid = parentid;
    E->bufferlen = i_InitBlockLen;

    E->event = (U8*)NE->name;

    // Copy the old init block chunk in.
    memcpy(E->buffer, i_InitBlock, (S32)(name - (char*)i_InitBlock));

    Queue_Commit(E);

    return qid;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static U64 AIL_enqueue_event_end(S32 token, void* i_NameOrEvent, S32 i_IsName, S32 i_FreeEvent)
{
    S32 name_offset = token >> 16;

    // Add the name.
    S32 succ = 0;
    if (i_IsName)
        succ = AIL_enqueue_event_name(&token, (char const*)i_NameOrEvent);
    else
        succ = AIL_enqueue_event_raw(&token, i_NameOrEvent, i_FreeEvent);

    if (succ == 0)
    {
        if (i_IsName == 0 && i_FreeEvent)
            AIL_mem_free_lock(i_NameOrEvent);
        return 0;
    }

    // resume as though the client added it...
    S32 cur_offset = token >> 16;
    token &= 0xffff;

    if (token == 0 || token >= MAX_BITS)
    {
        if (i_IsName == 0 && i_FreeEvent)
            AIL_mem_free_lock(i_NameOrEvent);
        return 0;
    }

    token--; // one based

    // Check for the user buffer.
    void* context = 0;
    U64 parentid = 0;
    U64 filter = 0;
    S32 control = 0;

    {
        U32 buffer_token = 0;
        char* buffer = enqueue_buffer + token * CHUNK_SIZE;
        char* buffer_end = buffer + CHUNK_SIZE;

        buffer_token = *(U32*)buffer;
        while (buffer_token)
        {
            U32 advance_amt = *(U32*)(buffer + 4);

            // Get any data we need.
            switch (buffer_token)
            {
            case BUFFER_FILTER: filter = *(U64*)(buffer + 8);break;
            case BUFFER_CONTEXT: context = *(void**)(buffer + 8); break;
            case BUFFER_PARENT: parentid = *(U64*)(buffer + 8); break;
            case BUFFER_CONTROL: control = 1; break;
            }

            buffer += advance_amt;

            if (buffer < buffer_end)
                buffer_token = *(U32*)buffer;
            else 
                break;
        }
    }


    // If we have a user buffer, we need to add an offset, which will go at the beginning of the buffer space to point to where the user buffer actually is.
    QueuedEvent* E = Queue_Add(cur_offset);
    if (E == 0)
    {
        ClearQueueToken(token);
        if (i_FreeEvent && i_IsName == 0)
            AIL_mem_free_lock(i_NameOrEvent);
        return 0;
    }

    U64 qid = rrAtomicAddExchange64(&s_NextEventId, 1);

    E->flags = i_IsName ? MILESEVENT_ENQUEUE_BY_NAME : 0;
    E->flags |= control ? 0x8 : 0;

    E->eventid = qid;
    E->context = context;
    E->eventfilter = filter;
    E->parentid = parentid ? parentid : qid;
    E->bufferlen = cur_offset;

    // Get the ptr to the name string.
    if (i_IsName)
    {
        NameEntry* NE = (NameEntry*)(enqueue_buffer + token * CHUNK_SIZE + name_offset);
        E->event = (U8*)AIL_ptr_add(E->buffer, name_offset + ((char*)&NE->name - (char*)NE));
    }
    else
    {
        EventEntry* EE = (EventEntry*)(enqueue_buffer + token * CHUNK_SIZE + name_offset);
        E->event = (U8*)EE->event_ptr;
        E->flags |= EE->free_event ? MILESEVENT_ENQUEUE_FREE_EVENT : 0;
    }

    // Copy the whole stream in to the user data section.
    memcpy(E->buffer, enqueue_buffer + token*CHUNK_SIZE, cur_offset);

    Queue_Commit(E);

    ClearQueueToken(token);

    return qid;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC U64 AILEXPORT AIL_enqueue_event_end_named(S32 token, char const* i_Name)
{
    return AIL_enqueue_event_end(token, (void*)i_Name, 1, 0);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" U64 AILCALL MilesEnqueueEventByNameInternal(char const* i_Name)
{
    if (i_Name == 0)
        return 0;

    return AIL_enqueue_event_end(AIL_enqueue_event_start(), (void*)i_Name, 1, 0);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static U64 MilesEnqueueEventWithParent(HEVENTSYSTEM i_Context, const U8* i_Event, void* i_UserBuffer, S32 i_UserBufferLen, S32 i_EnqueueFlags, U64 i_EventFilter, U64 i_ParentId)
{
    if (i_Event == 0) 
        return 0;

    S32 Token = AIL_enqueue_event_start();
    S32 Fail = 0;
    if (i_UserBuffer)
        Fail |= !AIL_enqueue_event_buffer(&Token, i_UserBuffer, i_UserBufferLen, i_EnqueueFlags & MILESEVENT_ENQUEUE_BUFFER_PTR);

    if (i_EventFilter)
        Fail |= !AIL_enqueue_event_filter(&Token, i_EventFilter);

    if (i_EnqueueFlags & 0x8)
        Fail |= !AIL_enqueue_event_control(&Token);

    if (i_ParentId)
        Fail |= !AIL_enqueue_event_parent(&Token, i_ParentId);

    if (i_Context)
        Fail |= !AIL_enqueue_event_context(&Token, i_Context);

    if (Fail)
    {
        if (i_EnqueueFlags & MILESEVENT_ENQUEUE_FREE_EVENT)
            AIL_mem_free_lock((void*)i_Event);
        return 0;
    }

    if (i_EnqueueFlags & MILESEVENT_ENQUEUE_BY_NAME)
        return AIL_enqueue_event_end(Token, (void*)i_Event, 1, 0);

    return AIL_enqueue_event_end(Token, (void*)i_Event, 0, i_EnqueueFlags & MILESEVENT_ENQUEUE_FREE_EVENT);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" U64 AILCALL MilesEnqueueEventInternal(const U8* i_Event, void* i_UserBuffer, S32 i_UserBufferLen, S32 i_EnqueueFlags, U64 i_EventFilter)
{
    return MilesEnqueueEventWithParent(0, i_Event, i_UserBuffer, i_UserBufferLen, i_EnqueueFlags, i_EventFilter, 0);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" U64 AILCALL MilesEnqueueEventContextInternal(HEVENTSYSTEM i_Context, const U8* i_Event, void* i_UserBuffer, S32 i_UserBufferLen, S32 i_EnqueueFlags, U64 i_EventFilter)
{
    return MilesEnqueueEventWithParent(i_Context, i_Event, i_UserBuffer, i_UserBufferLen, i_EnqueueFlags, i_EventFilter, 0);
}


#include "mss.h"

#include <string.h> // for memmove
#include "mileseventhandle.h"

//
// The handle system is a stack allocator that is capable
// of defragmenting when the client can assert that there
// are no outstanding pointers.
//
// We begin life with a heap, and all of our allocations
// occur within it.
//
// The handle table is stored at the end of the heap, and
// grows 'downward'. User data allocations are supplied
// from the bottom.
//
// User data allocations require a single 4 byte index
// applied before the return pointer that reference the
// handle table index that uses them (negative number).
//
// The handle index returned to the user is positive, and
// must be negated before use.
//
// The free list is stored as a delta-linked list within
// the handle table. if s_MilesEvFreeBase is nonzero,
// it is the index of the last free index. That indeces
// ptr value is the delta to the next free index. It is
// set up to return s_MilesEvFreeBase to zero if it is the
// last one.
//

//
// The defragmentation should be very fast, but it is linear in
// active allocation count if no work needs to be done, and linear
// in byte count when moving data.
//
// Since it only "scoots" allocations towards the bottom of the
// heap, it has the effect of packing long term allocations towards
// the bottom, so frequent small allocations cause minimal data
// movement as they will always be at the top of the stack.
//
// Freeing a long term allocation causes basically the entire
// heap to scooch.
//
// Note that for shutdown, the entire system can be reinitialized
// in only a few instructions, which may be preferrable to
// a huge number of frees followed by a defrag().
//

//#define DEBUG_CLEARS

#define DELETED_ENTRY 1

/* extern */S32 g_MilesEvValidHandleCount = 0;
/* extern */HandleEntry* g_MilesEvValidHandlePtr = 0;

static S32 s_MilesEvFreeBase = 0;
static S32 s_MilesEvLastMagic = 1;

char* s_BasePointer = 0;
char* s_Current = 0;

// Pointers passed to handle_init
char* s_Heap = 0;
S32 s_HeapMax = 0;

// Flag to determine if we should try to defrag.
S32 s_ShouldDefrag = 0;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Handle_Shutdown()
{
    s_Heap = 0;
    s_HeapMax = 0;
    s_BasePointer = 0;
    s_Current = 0;

    g_MilesEvValidHandleCount = 0;
    s_MilesEvFreeBase = 0;

    // We don't reinit magic.
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Handle_Init(void* i_Memory, S32 i_MemorySize)
{
    Handle_Shutdown();

    //
    // Init the base pointer to the offset we need.
    //
    // We want allocations to be on 16 byte alignment, 
    // so start us off at 16 byte - 4 for the index.
    //
    s_Heap = (char*)i_Memory;
    s_HeapMax = i_MemorySize;

    s_BasePointer = (char*)( ((SINTa)s_Heap + 0xf) & ~0xf);

    if (s_BasePointer - 4 < s_Heap) s_BasePointer += 16;

    // Terminate our allocation list.
    *(S32*)(s_BasePointer - 4) = 0;

    s_Current = s_BasePointer;

    // Setup the handle table to point at the end of the heap.
    g_MilesEvValidHandlePtr = (HandleEntry*)(s_Heap + s_HeapMax);
    g_MilesEvValidHandleCount = 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Handle_Defrag()
{
    if (s_ShouldDefrag == 0) return;

    //
    // Walk the heap...
    //
    S32* pCurrentIndex = (S32*)(s_BasePointer - 4);
    char* pWritePointer = s_BasePointer;

    while (*pCurrentIndex != 0)
    {
        S32 UseBytes = 0;
        if (*pCurrentIndex == DELETED_ENTRY)
        {
            UseBytes = *(pCurrentIndex + 1);
        }
        else
        {
            // Valid entry
            // Move to our current position.
            HandleEntry CurHandle = g_MilesEvValidHandlePtr[*pCurrentIndex];
            // Copy the memory for this entry forward.
            if (CurHandle.ptr != pWritePointer)
            {
                S32 MovedIndex = *pCurrentIndex;
                memmove(pWritePointer-4, (char*)CurHandle.ptr-4, CurHandle.bytes);
                g_MilesEvValidHandlePtr[MovedIndex].ptr = pWritePointer;
            }
            pWritePointer += CurHandle.bytes;

            UseBytes = CurHandle.bytes;
        }

        // Use the bytecount to advance to the next block.
        pCurrentIndex += (UseBytes) >> 2;
    }

#ifdef DEBUG_CLEARS
    memset(pWritePointer, 0, (char*)g_MilesEvValidHandlePtr - g_MilesEvValidHandleCount * sizeof(HandleEntry) - pWritePointer);
#endif

    *(S32*)(pWritePointer - 4) = 0;
    s_Current = pWritePointer;
    
    s_ShouldDefrag = 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Handle Handle_Allocate(S32 i_ByteCount)
{
    // Check for room.
    //
    // s_Current is 16 byte aligned.
    // s_Current[-4] is going to hold our result index.
    //
    // We need (i_ByteCount+4) to be divisible by 16.
    //
    i_ByteCount += 4;
    i_ByteCount = (i_ByteCount + 15) & ~15;

    // In addition, we need to check for handle table space.
    S32 HandleSpaceNeeded = g_MilesEvValidHandleCount * sizeof(HandleEntry);

    // if we have to allocate a new handle entry, the space needed goes up.
    if (s_MilesEvFreeBase == 0)
        HandleSpaceNeeded += sizeof(HandleEntry);

    S32 AvailMem = (S32)(((char*)g_MilesEvValidHandlePtr - s_Current) - HandleSpaceNeeded);
    if (AvailMem < i_ByteCount)
    {
        return Handle_Clear();
    }

    //
    // Find the handle entry we are going to use
    //
    S32 ResultIndex = 0;

    if (s_MilesEvFreeBase)
    {
        // There are entries in the free list.
        ResultIndex = -s_MilesEvFreeBase;
        s_MilesEvFreeBase = s_MilesEvFreeBase + (S32)(SINTa)g_MilesEvValidHandlePtr[s_MilesEvFreeBase].ptr;
    }
    else
    {
        // We are using the next open entry.
        g_MilesEvValidHandleCount++;
        ResultIndex = g_MilesEvValidHandleCount;
    }


    // We return s_Current
    void* mem = s_Current;

    // Handle index is the 4 bytes before us.
    S32* pHandleIndex = (S32*)(s_Current - 4);

    // Update s_Current, and save off the termination marker.
    s_Current += i_ByteCount;
    *(S32*)(s_Current - 4) = 0;

    // Setup return
    Handle Result;
    Result.index = ResultIndex;
    Result.magic = s_MilesEvLastMagic++;
    if (s_MilesEvLastMagic == 0) s_MilesEvLastMagic = 1; // handle wrap.

    // Update state.
    g_MilesEvValidHandlePtr[-ResultIndex].bytes = i_ByteCount;
    g_MilesEvValidHandlePtr[-ResultIndex].magic = Result.magic;
    g_MilesEvValidHandlePtr[-ResultIndex].ptr = mem;
    *pHandleIndex = -ResultIndex;
    
    return Result;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Handle_Free(Handle handle)
{
    if (handle.index == 0 || (U32)handle.index > (U32)g_MilesEvValidHandleCount) return; // invalid handle
    HandleEntry HE = g_MilesEvValidHandlePtr[-handle.index];
    if (HE.magic != handle.magic) return; // invalid handle.

    S32 ActualIndex = -handle.index;

    // invalidate the entry.
    g_MilesEvValidHandlePtr[ActualIndex].magic = 0;

#ifdef DEBUG_CLEARS
    memset((char*)g_MilesEvValidHandlePtr[ActualIndex].ptr, 0, g_MilesEvValidHandlePtr[ActualIndex].bytes - 4);
#endif

    *((S32*)g_MilesEvValidHandlePtr[ActualIndex].ptr - 1) = DELETED_ENTRY;
    *((S32*)g_MilesEvValidHandlePtr[ActualIndex].ptr) = g_MilesEvValidHandlePtr[ActualIndex].bytes;


    // If we happened to free the last handle, just pop it.
    if (handle.index == g_MilesEvValidHandleCount)
    {
        g_MilesEvValidHandleCount--;
        s_ShouldDefrag = 1;
        return;
    }

    //
    // Update the free list.
    //
    if (s_MilesEvFreeBase)
    {
        S32 Diff = s_MilesEvFreeBase - ActualIndex;
        g_MilesEvValidHandlePtr[ActualIndex].ptr = (void*)(SINTa)Diff;
        s_MilesEvFreeBase = ActualIndex;
    }
    else
    {
        g_MilesEvValidHandlePtr[ActualIndex].ptr = (void*)(SINTa)(-ActualIndex);
        s_MilesEvFreeBase = ActualIndex;
    }

    s_ShouldDefrag = 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Handle_MemStats(S32* o_TotalSize, S32* o_RemainingSize)
{
    if (o_RemainingSize == 0 || o_TotalSize == 0) return;

    *o_TotalSize = s_HeapMax;

    S32 HandleSpaceNeeded = g_MilesEvValidHandleCount * sizeof(HandleEntry);
    S32 AvailMem = (S32)(((char*)g_MilesEvValidHandlePtr - s_Current) - HandleSpaceNeeded);
    *o_RemainingSize = AvailMem;
}

#include "mss.h"
#include "imssapi.h"

#include "rrThreads.h"
#include "rrAtomics.h"

//=============================================================================

S32 AILCALLBACK MilesAsyncFileRead_default(struct MilesAsyncRead* i_Request);
S32 AILCALLBACK MilesAsyncFileCancel_default(struct MilesAsyncRead* i_Request);
S32 AILCALLBACK MilesAsyncFileStatus_default(struct MilesAsyncRead* i_Request, U32 i_MS);
S32 AILCALLBACK MilesAsyncStartup_default();
S32 AILCALLBACK MilesAsyncShutdown_default();
void AILCALLBACK MilesAsyncSetPaused_default(S32 i_IsPaused);
S32 AILCALLBACK AIL_IO_thread_handle_default(void* o_Handle);

//=============================================================================

static MilesAsyncFileRead_callback s_MilesAsyncFileRead_callback = MilesAsyncFileRead_default;
static MilesAsyncFileCancel_callback s_MilesAsyncFileCancel_callback = MilesAsyncFileCancel_default;
static MilesAsyncFileStatus_callback s_MilesAsyncFileStatus_callback = MilesAsyncFileStatus_default;
static MilesAsyncStartup_callback s_MilesAsyncStartup_callback = MilesAsyncStartup_default;
static MilesAsyncShutdown_callback s_MilesAsyncShutdown_callback = MilesAsyncShutdown_default;
static MilesAsyncSetPaused_callback s_MilesAsyncSetPaused_callback = MilesAsyncSetPaused_default;
static AIL_IO_thread_handle_callback s_AIL_IO_thread_handle_callback = AIL_IO_thread_handle_default;

DXDEF  void  AILEXPORT AIL_set_async_callbacks(
  MilesAsyncFileRead_callback read,
  MilesAsyncFileCancel_callback cancel,
  MilesAsyncFileStatus_callback status,
  MilesAsyncStartup_callback startup,
  MilesAsyncShutdown_callback shutdown,
  MilesAsyncSetPaused_callback setpaused,
  AIL_IO_thread_handle_callback threadhandle)
{
  s_MilesAsyncFileRead_callback = read;
  s_MilesAsyncFileCancel_callback = cancel;
  s_MilesAsyncFileStatus_callback = status;
  s_MilesAsyncStartup_callback = startup;
  s_MilesAsyncShutdown_callback = shutdown;
  s_MilesAsyncSetPaused_callback = setpaused;
  s_AIL_IO_thread_handle_callback = threadhandle;
}

//=============================================================================

DXDEF S32 AILEXPORT MilesAsyncFileRead(struct MilesAsyncRead* i_Request)
{
  return (*s_MilesAsyncFileRead_callback)(i_Request);
}

DXDEF S32 AILEXPORT MilesAsyncFileCancel(struct MilesAsyncRead* i_Request)

{
  return (*s_MilesAsyncFileCancel_callback)(i_Request);
}

DXDEF S32 AILEXPORT MilesAsyncFileStatus(struct MilesAsyncRead* i_Request, U32 i_MS)
{
  return (*s_MilesAsyncFileStatus_callback)(i_Request,i_MS);
}

DXDEF S32 AILEXPORT MilesAsyncStartup()
{
  return (*s_MilesAsyncStartup_callback)();
}

DXDEF S32 AILEXPORT MilesAsyncShutdown()
{
  return (*s_MilesAsyncShutdown_callback)();
}

DXDEF S32 AILEXPORT AIL_IO_thread_handle(void* o_Handle)
{
  return (*s_AIL_IO_thread_handle_callback)(o_Handle);
}

DXDEF void AILEXPORT MilesAsyncSetPaused(S32 i_IsPaused)
{
  return (*s_MilesAsyncSetPaused_callback)(i_IsPaused);
}

//===============================================================

#ifdef RR_CAN_ALLOC_THREAD_STACK
#define alloc_rr_thread( v_rr_thread, size ) { p##v_rr_thread = (rrThread*) AIL_mem_alloc_lock( sizeof( rrThread ) + size ); }
#define dealloc_rr_thread( v_rr_thread ) { AIL_mem_free_lock( p##v_rr_thread ); p##v_rr_thread = 0; }
#define THREADDEFAULT rrThreadCreateDefault|rrThreadStackAfterThread
#else
#define alloc_rr_thread( v_rr_thread, size ) 
#define dealloc_rr_thread( v_rr_thread ) 
#define THREADDEFAULT rrThreadCreateDefault
#endif

static MilesAsyncRead* Root = 0;
static rrMutex QueueLock;

#ifdef RR_CAN_ALLOC_THREAD_STACK
static rrThread * pAsyncThread;
#define AsyncThread (*pAsyncThread)
#else
static rrThread AsyncThread;
#endif

static rrSemaphore wake_up_reader;
static rrSemaphore waiting_count;
static volatile S32 StopThreads = 0;

static S32 Running = 0;


#define MAX_WAITS 32 
static rrSemaphore our_semas[ MAX_WAITS ];
static struct MilesAsyncRead * our_wait_reqs[ MAX_WAITS ];


//-----------------------------------------------------------------------------
static void LockQueue()
{
  if (rrMutexLockTimeout(&QueueLock, 0) == 0 )
  {
   tmEnter( miles_context, TMZF_STALL, "Miles IO mutex block" );
   rrMutexLock(&QueueLock);
   tmLeave( miles_context );
  }
  tmSetLockState(miles_context,&QueueLock,TMLS_LOCKED,"Miles IO mutex");
}

//-----------------------------------------------------------------------------
static void UnlockQueue()
{
  tmSetLockState(miles_context,&QueueLock,TMLS_RELEASED,"Miles IO mutex");
  rrMutexUnlock(&QueueLock);
}


struct InternalData
{
    MilesAsyncRead *Next, *Prev; // Linked List.

    S32 Seeked;     // Flags so subsequest DoRead calls on the same async don't reseek.
    S32 Cancel;     // Nonzero if the user has requested that the read be cancelled as soon as possible
    S32 DidAlloc;   // Nonzero if buffer was alloced
};



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void Remove(MilesAsyncRead* i_Item, S32 i_Lock)
{
    if (i_Lock) LockQueue();

    InternalData* pData = (InternalData*)i_Item->Internal;
    
    MilesAsyncRead* pNext = pData->Next;
    MilesAsyncRead* pPrev = pData->Prev;

    if (Root == i_Item)
    {
        Root = pNext;
    }

    if (pNext)
    {
        ((InternalData*)pData->Next->Internal)->Prev = pPrev;
    }
    if (pPrev) 
    {
        ((InternalData*)pData->Prev->Internal)->Next = pNext;
    }

    if (i_Lock) UnlockQueue();

    // Clear the pointers just so we can't get double removed easily.
    pData->Next = 0;
    pData->Prev = 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void wait_on_request( struct MilesAsyncRead * req, U32 ms )
{
  U32 i;
  U32 t, d;

  // finds an open semaphore to wait on, records our
  //   request pointer and then spins

  d = ms;
  t = AIL_ms_count();

  LockQueue();
  
  // scan until you find a free wait spot
  for( i = 0 ; i < MAX_WAITS ; i++ )
  {
    if ( our_wait_reqs[ i ] == 0 )
    {
      // found one, claim it, and then unlock
      our_wait_reqs[ i ] = req;
      UnlockQueue();

      tmEnter( miles_context, TMZF_STALL, "Waiting on Miles IO" );

      // mark that we are waiting (this will never block)
      rrSemaphoreDecrementOrWait( &waiting_count, RR_WAIT_INFINITE );

      // wait loop
      while( !StopThreads )
      {
        // return, if we now have status
        if ( req->Status )
          break;

        // now wait on our specific semaphore
        rrSemaphoreDecrementOrWait( &our_semas[ i ], ( d == RR_WAIT_INFINITE ) ? 10 : 0 );
        
        // return, if we now have status
        if ( req->Status )
          break;
       
        // since the semaphores are global, there might be an extra poke from a
        //   previous use, so we wait in a loop

        // check for a timeout
        if ( ms != RR_WAIT_INFINITE )
        {
          d = AIL_ms_count() - t;
          if ( d >= ms )
            break;
          d = ( ms - d );
        }
      }

      tmLeave( miles_context );

      // release waiting status
      rrSemaphoreIncrement( &waiting_count, 1 );

      // remove the wait request
      LockQueue();
      our_wait_reqs[ i ] = 0;
      UnlockQueue();

      return;
    }
  }

  UnlockQueue();

  // there were no wait primatives available
  //   so sleep in a loop (hopefully never happens)
  while( ( AIL_ms_count() - t ) < ms )
  {
    if ( req->Status )
      break;
    rrThreadSleep( 1 );
  }
}

static void unblock_waits( struct MilesAsyncRead * req )
{
  U32 i;

  // spin through the whole list, and blip each semaphore
  //   that has a matching request pointer
  
  // we don't really have to lock here, but we do so to be cool

  LockQueue();
  for( i = 0 ; i < MAX_WAITS ; i++ )
  {
    // we could check the
    if ( our_wait_reqs[ i ] == req )
    {
      rrSemaphoreIncrement( &our_semas[ i ], 1 );
    }
  }
  UnlockQueue();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 InternalCancelOrWait(struct MilesAsyncRead* i_Request, U32 i_MS, S32 i_Cancel)
{
  // Returns 1 if the async has completed reading or cancelling.
  InternalData* pData = (InternalData*)i_Request->Internal;

  // only wait, if we aren't already done
  if ( i_Request->Status == 0 )
  {
    // mark as cancel
    if ( i_Cancel )
      pData->Cancel = 1;

    // wake the IO thread when we're about to wait
    rrSemaphoreIncrement( &wake_up_reader, 1 );

    wait_on_request( i_Request, i_MS );
  }
  
  return i_Request->Status;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static volatile S32 s_Paused = 0;
void AILCALLBACK MilesAsyncSetPaused_default(S32 i_IsPaused)
{
    if (s_Paused == 0 && i_IsPaused)
    {
        s_Paused = 1;
        return;
    }
    else if (s_Paused && i_IsPaused == 0)
    {
        s_Paused = 0;
        
        // Wake up the thread.
        rrSemaphoreIncrement(&wake_up_reader, 1);
        return;
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 AILCALLBACK MilesAsyncFileStatus_default(struct MilesAsyncRead* i_Request, U32 i_MS)
{
    if (s_Paused && i_MS <= 1)
        return 0;

    return InternalCancelOrWait(i_Request, i_MS, 0);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 AILCALLBACK MilesAsyncFileCancel_default(MilesAsyncRead* i_Request)
{
    return InternalCancelOrWait(i_Request, 0, 1);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 AILCALLBACK MilesAsyncFileRead_default(MilesAsyncRead* i_Request)
{
    if ( !Running )
      return 0;
      
    InternalData* pData = (InternalData*)i_Request->Internal;

    // Check if this async read is in use.

    i_Request->Status = 0;
    pData->Seeked = 0;  
    pData->Cancel = 0;  
    pData->DidAlloc = 0;

    LockQueue();

    if (Root == 0)
    {
        Root = i_Request;
    }
    else
    {
        // Enqueue the request.
        MilesAsyncRead* pSeek = Root;
        for (;;)
        {
            InternalData* pSeekData = (InternalData*)pSeek->Internal;
            MilesAsyncRead* pNext = pSeekData->Next;
            if (pNext == 0)
            {
                pSeekData->Next = i_Request;
                ((InternalData*)i_Request->Internal)->Prev = pSeek;
                break;
            }
            pSeek = pNext;
        }
    }

    UnlockQueue();

    rrSemaphoreIncrement( &wake_up_reader, 1 );

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void set_status( MilesAsyncRead* i_Async, S32 stat )
{
  InternalData* pInternal = (InternalData*)i_Async->Internal;

  // Error.
  Remove( i_Async, 1 );

  rrAtomicMemoryBarrierFull();
  i_Async->Status = stat;

  // hose the buffer if it got allocated, on error
  if ( ( stat & MSSIO_STATUS_ERROR_MASK ) && ( i_Async->Buffer ) && ( pInternal->DidAlloc ) )
  {
    pInternal->DidAlloc = 0;
    AIL_mem_free_lock( i_Async->Buffer );
    i_Async->Buffer = 0;
  }

  // free the file handle, if it got allocated
  if (i_Async->FileHandle)
  {
    if ( ( i_Async->Flags & MSSIO_FLAGS_DONT_CLOSE_HANDLE ) == 0 )
    {
      MSS_close( i_Async->FileHandle );
      i_Async->FileHandle = 0;
    }
  }

  unblock_waits( i_Async );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static S32 DoRead(MilesAsyncRead* i_Async)
{
    InternalData* pInternal = (InternalData*)i_Async->Internal;

    // are we cancelled
    if ( pInternal->Cancel )
    {
      set_status( i_Async, MSSIO_STATUS_ERROR_CANCELLED );
      return 1;
    }

    //
    // Check if we need to open the file.
    //
    if (i_Async->FileHandle == 0)
    {
        tmEnter( miles_context, TMZF_NONE, "File open: %s (%s line: %d)", tmDynamicString( miles_context, i_Async->FileName ), (i_Async->caller)?i_Async->caller:"unknown",i_Async->caller_line );
        if (MSS_open(i_Async->FileName, &i_Async->FileHandle) == 0)
        {
            set_status( i_Async, MSSIO_STATUS_ERROR_FAILED_OPEN );
            tmLeave( miles_context );
            return 1;
        }
        tmLeave( miles_context );
    }

    S32 ReadCount = (S32)i_Async->Count;
    if (ReadCount < 0)
    {
        // We read until the end of the file, from the given offset.
        S32 FileLength = MSS_seek(i_Async->FileHandle, 0, AIL_FILE_SEEK_END);
        ReadCount = FileLength - (S32)i_Async->Offset;
        i_Async->Count = ReadCount;
    }

    if ( (i_Async->Flags & MSSIO_FLAGS_QUERY_SIZE_ONLY) || ReadCount == 0) // read size or empty files
    {
        set_status( i_Async, MSSIO_STATUS_COMPLETE );
        return 1;
    }

    //
    // If we weren't given a buffer, allocate one.
    //
    // Don't allocate if we aren't reading any data
    if (i_Async->Buffer == 0)
    {
        // \todo Do we need to ensure the AdditionalBuffer amount is granular 16?
        i_Async->Buffer = AIL_mem_alloc_lock_info(ReadCount + i_Async->AdditionalBuffer,i_Async->caller,i_Async->caller_line);
        if ( i_Async->Buffer == 0 )
        {
            set_status( i_Async, MSSIO_STATUS_ERROR_MEMORY_ALLOC_FAIL );
            return 1;
        }
        pInternal->DidAlloc = 1;
    }

    // Setup for read if we are supposed to seek.
    if (!(i_Async->Flags & MSSIO_FLAGS_DONT_USE_OFFSET) && pInternal->Seeked == 0)
    {
        tmEnter( miles_context, TMZF_NONE, "File seek: %s to %d (%s line: %d)", tmDynamicString( miles_context, i_Async->FileName ), i_Async->Offset, (i_Async->caller)?i_Async->caller:"unknown",i_Async->caller_line  );
        MSS_seek(i_Async->FileHandle, (S32)i_Async->Offset, AIL_FILE_SEEK_BEGIN);
        tmLeave( miles_context );
        pInternal->Seeked = 1;
    }

    ReadCount = (S32)(i_Async->Count - i_Async->ReadAmt);

    // If reading 32K will leave 32K more, read 32K.  Otherwise, read the rest. (this avoids little shitlet reads)
    S32 ReadAmt = 32 * 1024;
    if ( ( ReadCount - ReadAmt ) < ReadAmt )
      ReadAmt = ReadCount;

    tmEnter( miles_context, TMZF_NONE, "File read: %s bytes: %d (%s line: %d)", tmDynamicString( miles_context, i_Async->FileName ), ReadAmt, (i_Async->caller)?i_Async->caller:"unknown",i_Async->caller_line  );
    S32 GotAmt = MSS_read(i_Async->FileHandle, AIL_ptr_add(i_Async->Buffer, i_Async->ReadAmt + i_Async->AdditionalBuffer), ReadAmt);
    tmLeave( miles_context );
    i_Async->ReadAmt += GotAmt;

    if (GotAmt == 0)
    {
        set_status( i_Async,  MSSIO_STATUS_ERROR_FAILED_READ );
        return 1;
    }
    else if (i_Async->ReadAmt == i_Async->Count)
    {
        set_status( i_Async,  MSSIO_STATUS_COMPLETE );
        return 1;
    }
    else if (GotAmt != ReadAmt)
    {
        // We tried to read this amount but hit end of file.
        // Added because a hard coded read amount of 4096 was failing of files less than 4096, and
        // we want it to return the amount it actually got.
        set_status( i_Async,  MSSIO_STATUS_COMPLETE );
        return 1;
    }

    return 0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static U32 RADLINK ThreadProc(void* i_Param)
{
    MilesAsyncRead* pLast = 0;
    tmThreadName( miles_context, 0, "Miles IO" );

    while (!StopThreads)
    {
        MilesAsyncRead* pUse = 0;

        LockQueue();

        //
        // Find a read to execute.
        //
        MilesAsyncRead* pUnstarted = 0;
        MilesAsyncRead* pRandom = 0;
        for (pUse = Root; pUse; pUse = ((InternalData*)pUse->Internal)->Next)
        {
            // If there is another read queued before us with the same file handle, we can't go.
            if (pUse->FileHandle)
            {
                S32 Valid = 1;
                for (MilesAsyncRead* pCheck = Root; pCheck != pUse; pCheck = ((InternalData*)pCheck->Internal)->Next)
                {
                    if (pUse->FileHandle == pCheck->FileHandle) 
                    {
                        Valid = 0; break;
                    }
                }
                if (Valid == 0) continue;
            }

            if (pUse->StreamCB)
            {
                // If the stream is low, we go straight to it.
                F32 Pct = pUse->StreamCB(pUse->StreamUserData);
                if (Pct < 30.0f)
                {
                    break;
                }
            }

            if (pUse->Status == 0 && pUnstarted == 0)
            {
                pUnstarted = pUse;
            }

            if (pUse->Status == 1 && pRandom == 0) 
            {
                pRandom = pUse;
            }
        }

        // Unstarted reads are first, then the last one we read, then whatever.
        if (pUse == 0) pUse = pUnstarted;
        if (pUse == 0) pUse = pLast; 
        if (pUse == 0) pUse = pRandom;

        // If the async system is paused, force sleep
        if (s_Paused)
        {
            rrThreadSleep(100);
            pUse = 0;
        }

        if (pUse == 0)
        {
            UnlockQueue();
            
            // sleep, or until someone wakes us up
            tmEnter( miles_context, TMZF_IDLE, "Waiting for IO request" );
            rrSemaphoreDecrementOrWait( &wake_up_reader, RR_WAIT_INFINITE );
            tmLeave( miles_context );
            continue; 
        }

        UnlockQueue();

        if ( DoRead( pUse ) == 1 )
        {
            // Clear the 'last'
            pLast = 0;
        }
        else
        {
            pLast = pUse;
        }
    }

    return 1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 AILCALLBACK MilesAsyncStartup_default()
{
    RR_COMPILER_ASSERT( sizeof(InternalData) <= sizeof( ((MilesAsyncRead*)0)->Internal ) );

    if ( Running )
      return 1;

    Running = 1;

    StopThreads = 0;

    // Create mutex for enqueing.
    rrMutexCreate( &QueueLock, rrMutexNeedOnlyZeroTimeouts );
    tmLockName( miles_context, &QueueLock, "Miles IO mutex" );

    rrSemaphoreCreate( &wake_up_reader, 0, 512 ); // for telling the async thread there's work to do.

    rrSemaphoreCreate( &waiting_count, MAX_WAITS, MAX_WAITS ); // start out fully loaded
    
    // Create the semaphores.
    for (U32 i = 0; i < MAX_WAITS; i++)
    {
        rrSemaphoreCreate( &our_semas[ i ], 0, 128 );
    }

    // Start thread.
    alloc_rr_thread( AsyncThread, 16*1024 );
    rrThreadCreate( &AsyncThread, ThreadProc, 16*1024, 0, THREADDEFAULT, "MSSAsync" );
    rrThreadSetPriority( &AsyncThread, rrThreadOneHigherThanCurrent );

#if defined(IS_WIIU) || defined(IS_PSP2)
    rrThreadSetCPUCore(&AsyncThread, 2);
#endif

    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 AILCALLBACK MilesAsyncShutdown_default()
{
    U32 i;

    if ( !Running )
      return 1;

    Running = 0;

    // Stop threads singal
    StopThreads = 1;

    rrSemaphoreIncrement( &wake_up_reader, 512 ); // some arbitrary count

    // Wait until threads die.
    rrThreadWaitDone( &AsyncThread, RR_WAIT_INFINITE, 0 );
    rrThreadCleanUp( &AsyncThread );
    dealloc_rr_thread( AsyncThread );
    rrSemaphoreDestroy( &wake_up_reader );

    // Go over root chain, set error on each
    while( Root )
    {
        // this will remove the root from the root list (eventually emptying)
        set_status( Root, MSSIO_STATUS_ERROR_SHUTDOWN );
    }

    // wait until all waiting status are signaled
    for (i = 0; i < MAX_WAITS; i++)
    {
      rrSemaphoreDecrementOrWait( &waiting_count, RR_WAIT_INFINITE );
    }
    rrSemaphoreDestroy( &waiting_count );

    // hose all the wait semas
    for (i = 0; i < MAX_WAITS; i++)
    {
        rrSemaphoreDestroy( &our_semas[ i ] );
    }
    
    rrMutexDestroy( &QueueLock );
    
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
S32 AILCALLBACK AIL_IO_thread_handle_default(void* o_Handle)
{
    return rrThreadGetPlatformHandle(&AsyncThread, o_Handle);
}

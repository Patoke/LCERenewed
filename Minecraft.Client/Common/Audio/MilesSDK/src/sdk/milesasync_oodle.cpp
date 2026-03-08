#include "mss.h"
#include "imssapi.h"

#include "rrThreads.h"
#include "rrAtomics.h"

#include "c:\devel\projects\oodle\public\Oodle.h"
#include "c:\devel\projects\oodle\public\OodleExt.h"

/*
@cdep post

	$requiresbinary( C:\devel\projects\oodle\cdepbuild\win32_debug_dll\oodle_core_lib.lib )
	$requiresbinary( kernel32.lib )
*/

/******

//
//
//

#define MSSIO_FLAGS_DONT_CLOSE_HANDLE 1
#define MSSIO_FLAGS_QUERY_SIZE_ONLY 2
#define MSSIO_FLAGS_DONT_USE_OFFSET 4

#define MSSIO_STATUS_COMPLETE                     1
#define MSSIO_STATUS_ERROR_FAILED_OPEN       0x1003
#define MSSIO_STATUS_ERROR_FAILED_READ       0x1004
#define MSSIO_STATUS_ERROR_SHUTDOWN          0x1005
#define MSSIO_STATUS_ERROR_CANCELLED         0x1006
#define MSSIO_STATUS_ERROR_MEMORY_ALLOC_FAIL 0x1007
#define MSSIO_STATUS_ERROR_MASK              0x1000

// returns percent full (1.0 = 100%)
typedef F32 (AILCALLBACK *MilesAsyncStreamCallback)(void* i_User);

struct MilesAsyncRead
{
    char FileName[256];
    U64 Offset;
    S64 Count;
    void* Buffer;
    void* StreamUserData;
    MilesAsyncStreamCallback StreamCB;
    char const * caller;
    U32 caller_line;
    UINTa FileHandle;
    S32 Flags;
    S32 ReadAmt; // current read amt.
    S32 AdditionalBuffer;
    S32 volatile Status; // This is only valid after a call to MilesAsyncFileWait or MilesAsyncFileCancel has succeeded.
    char Internal[48+128];
};

DXDEC S32 AILCALL MilesAsyncFileRead(struct MilesAsyncRead* i_Request);
DXDEC S32 AILCALL MilesAsyncFileCancel(struct MilesAsyncRead* i_Request); // 1 if the request has completed, 0 otherwise. Use Wait if needed.
DXDEC S32 AILCALL MilesAsyncFileStatus(struct MilesAsyncRead* i_Request, U32 i_MS); // 1 if complete, 0 if timeout exceeded.
DXDEC S32 AILCALL MilesAsyncStartup();
DXDEC S32 AILCALL MilesAsyncShutdown();
DXDEC S32 AILCALL AIL_IO_thread_handle(void* o_Handle);
DXDEC void AILCALL MilesAsyncSetPaused(S32 i_IsPaused);


MilesAsyncFileStatus( ,-1) is a block
MilesAsyncFileStatus( ,0) is a getstatus query

    ReadStruct.Count = -1;
    ReadStruct.Flags = MSSIO_FLAGS_QUERY_SIZE_ONLY;

	fills out .Count with the file size


    if (ReadStruct.Status == MSSIO_STATUS_COMPLETE)
    {
        disk_err = 0;
        return (S32)ReadStruct.Count;
    }

    // Error condition.
    if (ReadStruct.Status == MSSIO_STATUS_ERROR_FAILED_OPEN)
    {

******/	

// warning : OodleMilesFile is duplicated in milesasync.cpp and MilesOodleIO.cpp
RADSTRUCT OodleMilesFile
{
	enum { check_val = 0x70AD331F };

	OodleIOQStream	m_stream;
	S64				m_pos;
	OodleFileInfo	m_info;
	U64				m_check;
};

struct InternalData
{
	OodleIOQStream		stream;
	rrbool				CloseStream;
	U64					LastOffset;
	rrbool				DidAlloc;
};

struct InternalData * ID(struct MilesAsyncRead* read)
{
	return (InternalData *) (read->Internal);
}

//-----------------------------------------------------------------------------
DXDEF void AILEXPORT MilesAsyncSetPaused(S32 i_IsPaused)
{
	//OodleIOQ_RequestThreadPause(i_IsPaused);
}

//-----------------------------------------------------------------------------

static void Cleanup( MilesAsyncRead* i_Async )
{
	InternalData * pInternal = ID(i_Async);
	
	//if ( i_Async->Status & MSSIO_STATUS_ERROR_MASK )
	if ( i_Async->Status != 0 )
	{
		if ( pInternal->stream && pInternal->CloseStream )
		{
			OodleIOQStream_RequestClose(pInternal->stream,NULL);
			pInternal->stream = 0;
		}
	
		// hose the buffer if it got allocated, on error
		if ( ( i_Async->Status & MSSIO_STATUS_ERROR_MASK ) && ( i_Async->Buffer ) && ( pInternal->DidAlloc ) )
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
	}
} 

static S32 ReadFromIOQStream( OodleIOQStream stream, U8 * pTo, U64 offset, S32 count, S32 millis )
{
	OodleIOQStream_WaitForBytes(stream,offset,count,millis);
	
	S32 got = 0;
	
	while(got < count)
	{
		void * pMem; S32 avail;
		OodleIOQStream_Peek(stream,offset,&pMem,&avail,NULL);
		
		if ( avail == 0 )
			break;
		
		avail = RR_MIN(avail,count);
		
		memcpy(pTo,pMem,avail);
		pTo += avail;
		got += avail;
	}
	
	return got;
}

//-----------------------------------------------------------------------------
DXDEF S32 AILEXPORT MilesAsyncFileStatus(struct MilesAsyncRead* i_Async, U32 i_MS)
{
	InternalData * id = ID(i_Async);
	
	if ( id->stream == 0 )
		return i_Async->Status;
		
	//-----------------------------------------

	// Count is the amount wanted
	// ReadAmt is the amount already done
    S32 ReadCount = (S32)(i_Async->Count - i_Async->ReadAmt);

	U8 * ReadPtr = (U8 *) AIL_ptr_add(i_Async->Buffer, i_Async->ReadAmt + i_Async->AdditionalBuffer);

	S32 GotAmt = OodleIOQStream_SyncRead(id->stream,id->LastOffset,ReadPtr,ReadCount,(S32)i_MS);
	
	i_Async->ReadAmt += GotAmt;
	id->LastOffset += GotAmt;

	if ( i_Async->FileHandle != 0 )
	{
		// update the MSS file seek pos :
		MSS_seek(i_Async->FileHandle,(S32)id->LastOffset,AIL_FILE_SEEK_BEGIN);
	}

	if (i_Async->ReadAmt == i_Async->Count)
	{
		i_Async->Status = MSSIO_STATUS_COMPLETE;
	}
	// need to check for why SyncRead returned - was it timeout or EOF ?
	/*
	else if (GotAmt == 0)
	{
		i_Async->Status = MSSIO_STATUS_ERROR_FAILED_READ;
	}
	else if (GotAmt != ReadCount)
	{
		// We tried to read this amount but hit end of file.
		// Added because a hard coded read amount of 4096 was failing of files less than 4096, and
		// we want it to return the amount it actually got.			
		i_Async->Status = MSSIO_STATUS_COMPLETE;
	}
	*/
	
	if ( i_Async->Status != 0 )
	{
		Cleanup(i_Async);
	}
	
	return i_Async->Status;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL MilesAsyncFileCancel(MilesAsyncRead* i_Request)
{
	MilesAsyncFileStatus(i_Request,0);

	InternalData * id = ID(i_Request);
	if ( id->stream != 0 && id->CloseStream )
	{
		OodleIOQStream_RequestClose(id->stream,NULL);
	}
	id->stream = 0;
	
	return i_Request->Status;
}

#if 0
template <typename t_func_type>
t_func_type GetWindowsImport( t_func_type * pFunc , const char * funcName, const char * libName )
{
    if ( *pFunc == 0 )
    {
        HMODULE m = GetModuleHandle(libName);
        if ( m == 0 ) m = LoadLibrary(libName); // adds extension for you
        RR_ASSERT_ALWAYS( m != 0 );
        t_func_type f = (t_func_type) GetProcAddress( m, funcName );
        // not optional : (* should really be a throw)
        RR_ASSERT_ALWAYS( f != 0 );
        *pFunc = f;
    }
    return (*pFunc); 
}

#define CALL_IMPORT(lib,name) (*GetWindowsImport(&RR_STRING_JOIN(fp_,name),RR_STRINGIZE(name),lib))
#define CALL_KERNEL32(name) CALL_IMPORT("kernel32",name)


DWORD
(WINAPI
* fp_GetFinalPathNameByHandleA) (
    __in HANDLE hFile,
    __out_ecount(cchFilePath) LPSTR lpszFilePath,
    __in DWORD cchFilePath,
    __in DWORD dwFlags
) = NULL;
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEF S32 AILCALL MilesAsyncFileRead(MilesAsyncRead* i_Async)
{
	/*
	static bool doPause = true;
	if ( doPause ) __asm int 3;
	doPause = false;
	*/
	
	InternalData * id = ID(i_Async);

	if ( id->stream == 0 )
	{
		if ( i_Async->FileHandle != 0 )
		{			
			/*
			id->LastOffset = MSS_seek(i_Async->FileHandle,0,AIL_FILE_SEEK_CURRENT);
		
		    PLATFORM_FILE* pFile = (PLATFORM_FILE*) i_Async->FileHandle;
			HANDLE hFile = *((HANDLE*)pFile->plat_specific);

			OodleLog_Printf_v1("Fetching name from handle : %08X\n",hFile);

			char nameBuf[256];
			CALL_KERNEL32(GetFinalPathNameByHandleA)(hFile,nameBuf,sizeof(nameBuf),FILE_NAME_OPENED|VOLUME_NAME_DOS);
			nameBuf[255] = 0;
			// + 4 chars of crap at the start
			strcpy(i_Async->FileName,nameBuf+4);
			*/
		
			OodleMilesFile * omf = (OodleMilesFile *) i_Async->FileHandle;
			
			if ( omf->m_check != OodleMilesFile::check_val )
				return 0;
			
			id->LastOffset = omf->m_pos;
			id->stream = omf->m_stream;
			id->CloseStream = false;
		}
		else
		{
			//OodleLog_Printf_v1("opening stream : %s\n",i_Async->FileName);

			S32 bufsize = OodleIOQStream_MakeValidBufSize(64*1024);
			id->stream = OodleIOQStream_RequestOpen(i_Async->FileName,NULL,false,bufsize,true);
			id->CloseStream = true;
		}
	}

    S32 ReadCount = (S32)i_Async->Count;
    if (ReadCount < 0)
    {
		OodleIOQFile file = OodleIOQStream_GetFile(id->stream);
		OodleFileInfo info;
		if ( ! OodleIOQ_GetInfo(file,&info) )
		{
			i_Async->Status = MSSIO_STATUS_ERROR_FAILED_OPEN;
			Cleanup(i_Async);
			return 0;
		}
		
        // We read until the end of the file, from the given offset.
        ReadCount = (S32)(info.size - i_Async->Offset);
        i_Async->Count = ReadCount;
    }

    if ( (i_Async->Flags & MSSIO_FLAGS_QUERY_SIZE_ONLY) || ReadCount == 0) // read size or empty files
    {
		i_Async->Status = MSSIO_STATUS_COMPLETE;
		Cleanup(i_Async);
		return 1; // success
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
			i_Async->Status = MSSIO_STATUS_ERROR_MEMORY_ALLOC_FAIL;
			Cleanup(i_Async);
			return 0;
        }
        id->DidAlloc = 1;
    }

    // Setup for read if we are supposed to seek.
    if ( (i_Async->Flags & MSSIO_FLAGS_DONT_USE_OFFSET) )
    {
		if ( i_Async->FileHandle != 0 )
		{
			U64 seekOffset = MSS_seek(i_Async->FileHandle,0,AIL_FILE_SEEK_CURRENT);
			//OodleLog_Printf_v1("DONT_USE_OFFSET ; seekOffset = %d , lastOffset = %d\n",
			//		(S32)seekOffset,(S32)id->LastOffset);
		}
			
		i_Async->Offset = id->LastOffset;
    }
    else
    {
		id->LastOffset = i_Async->Offset;
    }

    MilesAsyncFileStatus(i_Async,0);
    
    return 1;
}

static bool NeedsStreamService(MilesAsyncRead* pUse)
{
    if (pUse->StreamCB)
    {
        // If the stream is low, we go straight to it.
        F32 Pct = pUse->StreamCB(pUse->StreamUserData);
        if (Pct < 30.0f)
        {
            return true;
        }
    }
    
    return false;
}

//-----------------------------------------------------------------------------
S32 AILCALL MilesAsyncStartup()
{
    return 1;
}

//-----------------------------------------------------------------------------
S32 AILCALL MilesAsyncShutdown()
{
    return 1;
}

//-----------------------------------------------------------------------------
DXDEF S32 AILCALL AIL_IO_thread_handle(void* o_Handle)
{
	// OodleIOQ_GetThread();
	
    //return rrThreadGetPlatformHandle(&AsyncThread, o_Handle);
    return 0;
}

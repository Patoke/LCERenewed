#include "mss.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include "imssapi.h"


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void get_subfile_info( char * buf, char const * filename, FNVALUES * fnv )
{
  fnv->size = 0xffffffff;
  fnv->start = 0;

  if ( filename[ 0 ] != '*' )
  {
    fnv->filename = filename;
  }
  else
  {
    char * b;
    char const * f;

    fnv->filename = buf;
    
    b = buf;
    f = filename + 1;

    while ( ( f[ 0 ] ) && ( f[ 0 ] != '*' ) )
      *b++ = *f++;
    *b = 0;
    
    if ( f[0] == '*' )
    {
      U32 v;
      S32 n;
      ++f;

      n = 0; if ( f[0]=='-' ) { ++f; n = 1; }
      v = 0;
      while (((*f)>='0') && ((*f)<='9'))
        v=(v*10)+((*f++)-'0');
      if ( n ) v = (U32)-(S32)v;
      
      fnv->size = v;

      if ( f[0] == '*' )
      {
        ++f;
        
        // read pos
        n = 0; if ( f[0]=='-' ) { ++f; n = 1; }
        v = 0;
        while (((*f)>='0') && ((*f)<='9'))
          v=(v*10)+((*f++)-'0');
        if ( n ) v = (U32)-(S32)v;

        fnv->start = v;
      }
    }
  }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static 
U32 AILLIBCALLBACK
DefaultFileOpen(char const* i_FileName, UINTa* o_FileHandle)
{
    PLATFORM_FILE* pFile = (PLATFORM_FILE*)AIL_mem_alloc_lock(sizeof(PLATFORM_FILE));
    if (pFile == 0) 
    {
        return 0;
    }

    AIL_memset(pFile, 0, sizeof(PLATFORM_FILE));

    //
    // Open as per normal.
    //

    char buf[ 512 ];
    FNVALUES fnv;
    get_subfile_info( buf, i_FileName, &fnv );

    if ( Platform_OpenFile( pFile, fnv.filename ) )
    {
       if ( fnv.start >= pFile->raw_length )
       {
          // OOB - zero len file at the end.
          fnv.start = pFile->raw_length;
          fnv.size = 0;
       }
       else if ( fnv.size > ( pFile->raw_length - fnv.start ) )
       {
          // OOB - cap length.
          fnv.size = ( pFile->raw_length - fnv.start );
       }

       pFile->file_length = fnv.size;
       if (fnv.start != 0xffffffff)
       {
           pFile->start_pos = fnv.start;
       }

//    {
//        pFile->dont_close = 1; // use if the file isn't to be closed (not used any more)
//    }

       *o_FileHandle = (UINTa) pFile;
       return 1;
    }

    AIL_mem_free_lock( pFile );
    return 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static
void AILLIBCALLBACK
DefaultFileClose(UINTa i_FileHandle)
{
    if (i_FileHandle == 0)
    {
        return;
    }

    PLATFORM_FILE* pFile = (PLATFORM_FILE*)i_FileHandle;
    if (pFile->dont_close == 0)
    {
        Platform_CloseFile(pFile);
    }
    AIL_mem_free_lock(pFile);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static
S32 AILLIBCALLBACK
DefaultFileSeek(UINTa i_FileHandle, S32 i_Offset, U32 i_Type)
{
    if (i_FileHandle == 0)
    {
        return 0;
    }

    PLATFORM_FILE* pFile = (PLATFORM_FILE*)i_FileHandle;
    
    if (i_Type == AIL_FILE_SEEK_END)
    {
        pFile->offset = pFile->file_length + i_Offset;
    }
    else if (i_Type == AIL_FILE_SEEK_CURRENT)
    {
        pFile->offset += i_Offset;
    }
    else
    {
        pFile->offset = i_Offset;
    }

    if (pFile->offset > pFile->file_length)
    {
        pFile->offset = pFile->file_length;
    }

    Platform_SeekFromBeginning(pFile, pFile->offset);

    return pFile->offset;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

static 
U32 AILLIBCALLBACK
DefaultFileRead(UINTa i_FileHandle, void* o_Buffer, U32 i_ReadBytes)
{
    if (i_FileHandle == 0)
    {
        return 0;
    }

    PLATFORM_FILE* pFile = (PLATFORM_FILE*)i_FileHandle;

    if (i_ReadBytes + pFile->offset > pFile->file_length)
    {
        i_ReadBytes = pFile->file_length - pFile->offset;
    }

    S32 BytesRead = Platform_ReadFile((PLATFORM_FILE*)i_FileHandle, o_Buffer, i_ReadBytes);

    pFile->offset += BytesRead;
    return BytesRead;
}


AIL_file_open_callback          MSS_open    = DefaultFileOpen;
AIL_file_close_callback         MSS_close   = DefaultFileClose;
AIL_file_seek_callback          MSS_seek    = DefaultFileSeek;
AIL_file_read_callback          MSS_read    = DefaultFileRead;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC void AILEXPORT AIL_file_callbacks(
    AIL_file_open_callback* opencb, 
    AIL_file_close_callback* closecb, 
    AIL_file_seek_callback* seekcb, 
    AIL_file_read_callback* readcb
    )
{
    *opencb = MSS_open;
    *closecb = MSS_close;
    *seekcb = MSS_seek;
    *readcb = MSS_read;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DXDEC void AILEXPORT AIL_set_file_callbacks  (AIL_file_open_callback opencb,
                                              AIL_file_close_callback closecb,
                                              AIL_file_seek_callback seekcb,
                                              AIL_file_read_callback readcb)
{
  MSS_open=(opencb)?opencb:DefaultFileOpen;
  MSS_close=(closecb)?closecb:DefaultFileClose;
  MSS_seek=(seekcb)?seekcb:DefaultFileSeek;
  MSS_read=(readcb)?readcb:DefaultFileRead;
}


//############################################################################
//##                                                                        ##
//##  Return most recent system error code                                  ##
//##                                                                        ##
//############################################################################
S32 disk_err = 0; // written to from AIL_API_file_write
S32 AILCALL AIL_API_file_error(void)
{
   return disk_err;
}

//############################################################################
//##                                                                        ##
//##  Return size of file, or -1 if error                                   ##
//##                                                                        ##
//############################################################################
S32 AILCALL AIL_API_file_size(char const* filename, char const * caller, U32 caller_line)
{
    MilesAsyncRead ReadStruct;
    AIL_memset(&ReadStruct, 0, sizeof(ReadStruct));
    AIL_strcpy(ReadStruct.FileName, filename);
    ReadStruct.Count = -1;
    ReadStruct.Flags = MSSIO_FLAGS_QUERY_SIZE_ONLY;
    ReadStruct.caller = caller;
    ReadStruct.caller_line = caller_line;
    
    if (MilesAsyncFileRead(&ReadStruct) == 0)
    {
        disk_err = AIL_NO_AVAIL_ASYNC;
        return -1;
    }

    MilesAsyncFileStatus(&ReadStruct, ~0U);

    if (ReadStruct.Status == MSSIO_STATUS_COMPLETE)
    {
        disk_err = 0;
        return (S32)ReadStruct.Count;
    }

    // Error condition.
    if (ReadStruct.Status == MSSIO_STATUS_ERROR_FAILED_OPEN)
    {
        disk_err = AIL_FILE_NOT_FOUND;
    }

    return -1;
}


//############################################################################
//##                                                                        ##
//##  Read file into memory at *dest, allocating memory first if dest=NULL  ##
//##                                                                        ##
//############################################################################
void* AILCALL AIL_API_file_read(char const     *filename, void     *dest, char const * caller, U32 caller_line)
{
    MilesAsyncRead ReadStruct;
    AIL_memset(&ReadStruct, 0, sizeof(ReadStruct));
    AIL_strcpy(ReadStruct.FileName, filename);
    ReadStruct.caller = caller;
    ReadStruct.caller_line = caller_line;
    ReadStruct.Count = -1;

    if (dest == FILE_READ_WITH_SIZE)
    {
        ReadStruct.AdditionalBuffer = 4;
    }
    else if (dest)
    {
        ReadStruct.Buffer = dest;
    }

    if (MilesAsyncFileRead(&ReadStruct) == 0)
    {
        disk_err = AIL_NO_AVAIL_ASYNC;
        return 0;
    }

    MilesAsyncFileStatus(&ReadStruct, ~0U);

    if (ReadStruct.Status == MSSIO_STATUS_COMPLETE)
    {
        disk_err = 0;

        if (dest == FILE_READ_WITH_SIZE)
        {
            *(S32*)ReadStruct.Buffer = (S32)ReadStruct.Count;
        }

        return ReadStruct.Buffer;
    }

    // Error condition.
    if (ReadStruct.Status == MSSIO_STATUS_ERROR_FAILED_OPEN)
    {
        disk_err = AIL_FILE_NOT_FOUND;
    }
    else if (ReadStruct.Status == MSSIO_STATUS_ERROR_FAILED_READ)
    {
        disk_err = AIL_CANT_READ_FILE;
    }

    return 0;
}

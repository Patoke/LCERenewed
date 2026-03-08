#if defined(_XENON) || ( defined(_XBOX_VER) && (_XBOX_VER == 200) ) || defined(_XBOX)
#define NOD3D
#include <xtl.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(__NT__) || defined(__WIN32__)
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#define STRICT
#include <windows.h>
#endif

#include "mss.h"
#include "imssapi.h"


//-----------------------------------------------------------------------------

extern "C" void Platform_CloseFile(PLATFORM_FILE* i_File)
{
    CloseHandle(*(HANDLE*)i_File->plat_specific);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" void Platform_SeekFromBeginning(PLATFORM_FILE* i_File, S32 i_Offset)
{
    SetFilePointer(*(HANDLE*)i_File->plat_specific, i_Offset + i_File->start_pos, 0, FILE_BEGIN);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" U32 Platform_ReadFile(PLATFORM_FILE* i_File, void* o_Buffer, U32 i_ReadBytes)
{
    DWORD BytesRead = 0;
    ReadFile(*(HANDLE*)i_File->plat_specific, o_Buffer, i_ReadBytes, &BytesRead, 0);

    return BytesRead;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern "C" int Platform_OpenFile(struct PLATFORM_FILE* i_File, char const * fn)
{
    RR_COMPILER_ASSERT( sizeof(HANDLE) <= MAX_PLATFILE_SPECIFIC_SIZE );

    HANDLE FileHandle = CreateFile(fn, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, 0);
    if (FileHandle == INVALID_HANDLE_VALUE)
    {
      return 0;
    }
    
    *(HANDLE*)i_File->plat_specific = FileHandle;

    i_File->raw_length = SetFilePointer( FileHandle, 0, 0, FILE_END );
    SetFilePointer( FileHandle, 0, 0, FILE_BEGIN );

    return 1;
}
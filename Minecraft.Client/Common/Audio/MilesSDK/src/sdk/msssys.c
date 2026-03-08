//############################################################################
//##                                                                        ##
//##  MSSSYS.C                                                              ##
//##                                                                        ##
//##  Windows MSS support routines                                          ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#if defined(_XENON) || (_XBOX_VER == 200) || defined(_XBOX)
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

#ifdef MSS_SPU_PROCESS
  #define AIL_set_error( ... )
  #define AIL_debug_log( ... )
  int disk_err;
#endif


#if defined(MSS_SPU_PROCESS)
//#define DEBUGALLOC
#endif


#ifdef DEBUGALLOC
  #ifdef __RADFINAL__
    #error Oops, you have debug allocs on!
  #endif

  static U32 count = 0;
#endif

void  * AILCALLBACK platform_alloc(UINTa size)
{
   void * p;

#ifndef DEBUGALLOC
   void * up;

   up = (void *) GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT, size + 32 + 16 );
   if ( up == 0 )
   {
     AIL_debug_log( "!Miles memory allocation failed! (requested %d bytes)", size + 32 + 16 );
     AIL_set_error("Out of memory.");
     return( 0 );
   }

   p = (void *)( ( ( (UINTa) up ) + 32 + 16 ) & ~31 );
   ( (void**) p )[ -1 ] = up;
#else

   UINTa s=(size+4+4095)&~4095;

   p=VirtualAlloc(0,size+3+4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);

   if (p==0)
     AIL_set_error("Out of memory.");
   else
   {
     DWORD old;
     void* o=p;

     ++count;

     VirtualProtect(((U8*)p)+s,4096,PAGE_NOACCESS,&old);
     p=((U8*)p)+(s-size);
     ((void**)p)[-1]=o;
   }

   //
   // Fill the buffer with junk
   //

   {
     UINTa i;
     for(i=0;i<(size/8);i++)
     {
       ((U32*)p)[i*2]=0x76541234;
       ((U32*)p)[i*2+1]=0x24689753;
     }
     for(i=0;i<(size&7);i++)
       ((U8*)p)[(size&(~7))+i]=(U8)(127+(i*33));
   }

#endif

  ( (SINTa*) p )[ -2 ] = -12340987;
  return( p );
}

void AILCALLBACK platform_free( void  * ptr )
{
  void  * p;

  p = ( (void**) ptr )[ -1 ];
  ( (void**) ptr )[ -1 ] = 0;

  if ( ( p == 0 ) || ( ( (SINTa*) ptr )[ -2 ] != -12340987 ) )
  {
    AIL_debug_log( "!Miles detected memory error - Double freed memory...?(%X)", ptr );
    #ifdef _DEBUG
      RR_BREAK();
    #endif
  }
  else
  {
    ( (SINTa*) ptr )[ -2 ] = 0;

    #ifndef DEBUGALLOC
      GlobalFree( (HANDLE) p );
    #else
      --count;
      VirtualFree(p,0,MEM_RELEASE);
    #endif
  }
}


//############################################################################
//##                                                                        ##
//##  Write file at *buf of length len                                      ##
//##                                                                        ##
//##  Overwrites any existing file                                          ##
//##                                                                        ##
//##  Returns 0 on error, else 1                                            ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_file_write(char const  *filename, void const  *buf, U32 len)
{
  HANDLE handle;
  U32    nbytes;

  disk_err = 0;

  handle = CreateFile(filename,
                     GENERIC_WRITE,
                     0,
                     NULL,
                     CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL,
                     NULL);

  if (handle == INVALID_HANDLE_VALUE)
  {
    disk_err = AIL_CANT_WRITE_FILE;
    AIL_set_error("Unable to create file.");
    return 0;
  }

  if ((!WriteFile(handle,
                 buf,
                 len,
                (DWORD*)&nbytes,
                 NULL))
      || (nbytes != len))
  {
    CloseHandle(handle);
    disk_err = AIL_DISK_FULL;
    AIL_set_error("Unable to write to file (disk full?).");
    return 0;
  }

  CloseHandle(handle);

  return 1;
}


//############################################################################
//##                                                                        ##
//##  Write wave file at *buf of length len                                 ##
//##                                                                        ##
//##  Overwrites any existing file                                          ##
//##                                                                        ##
//##  Returns 0 on error, else 1                                            ##
//##                                                                        ##
//############################################################################

S32 AILCALL AIL_API_WAV_file_write(char const  *filename, void const  *buf, U32 len, S32 rate, S32 format)
{
  WAVEOUT wo;
  HANDLE handle;
  U32    nbytes;

  disk_err = 0;

  handle = CreateFile(filename,
                     GENERIC_WRITE,
                     0,
                     NULL,
                     CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL,
                     NULL);

  if (handle == INVALID_HANDLE_VALUE)
  {
    disk_err = AIL_CANT_WRITE_FILE;
    AIL_set_error("Unable to create file.");
    return 0;
  }

  AIL_memcpy(&wo.riffmark,"RIFF",4);
  wo.rifflen=len+sizeof(WAVEOUT)-8;
  AIL_memcpy(&wo.wavemark,"WAVE",4);
  AIL_memcpy(&wo.fmtmark,"fmt ",4);
  wo.fmtlen=16;
  wo.fmttag=WAVE_FORMAT_PCM;

  if (format & DIG_F_MULTICHANNEL_MASK)
  {
   wo.channels = (S16)(format >> 16);
  }
  else
  {
   wo.channels=(S16)((format&DIG_F_STEREO_MASK)?2:1);
  }

  wo.sampersec=rate;
  wo.bitspersam=(S16)((format&DIG_F_16BITS_MASK)?16:8);
  wo.blockalign=(S16)(((S32)wo.bitspersam*(S32)wo.channels) / 8);
  wo.avepersec=(rate *(S32)wo.bitspersam*(S32)wo.channels) / 8;
  AIL_memcpy(&wo.datamark,"data",4);
  wo.datalen=len;

  WriteFile(handle,&wo,sizeof(wo),(DWORD*)&nbytes,0);

  if ((!WriteFile(handle,
                 buf,
                 len,
                (DWORD*)&nbytes,
                 NULL))
      || (nbytes != len))
  {

    disk_err = AIL_DISK_FULL;
    AIL_set_error("Unable to write to file (disk full?).");
    return 0;
  }

  CloseHandle(handle);

  return 1;
}


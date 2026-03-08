//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  DLSHL.C: High-level interface to DLS DLLs (both the MSS soft-synth    ##
//##               and the S3 hardware DLS card under Windows).             ##
//##                                                                        ##
//##  16-bit protected-mode source compatible with MSC 7.0                  ##
//##  32-bit protected-mode source compatible with MSC 9.0                  ##
//##                                                                        ##
//##  Version 1.00 of 02-Feb-98: Derived from DLS test beds (Jeff Roberts)  ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#include "mss.h"
#include "imssapi.h"

#ifdef IS_WIN32
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#define STRICT
#include <windows.h>
#endif

#include "mssdls.h"

#ifdef IS_DOS

//############################################################################
//##                                                                        ##
//## Locked code                                                            ##
//##                                                                        ##
//############################################################################

#define LOCK(x)   AIL_vmm_lock  (&(x),sizeof(x))
#define UNLOCK(x) AIL_vmm_unlock(&(x),sizeof(x))

static S32 locked = 0;

static void AIL_lock_end(void);

static void AIL_lock_start(void)
{
   if (!locked)
      {
      AIL_vmm_lock_range(AIL_lock_start, AIL_lock_end);

      locked = 1;
      }
}

#define DOLOCK() AIL_lock_start()

#else

#define DOLOCK()

#endif

// ---------------------------------------------------------------------------
// DLS_poll_callback
// ---------------------------------------------------------------------------

static S32 AILLIBCALLBACK DLS_poll_callback(UINTa user)
{
   return AIL_sample_buffer_available( ((HDLSDEVICE)user)->sample );
}

// ---------------------------------------------------------------------------
// DLS_lock_callback
// ---------------------------------------------------------------------------

static void * AILLIBCALLBACK DLS_lock_callback(UINTa user, S32 buffer_section)
{
   return ((HDLSDEVICE)user)->buffer[buffer_section];
}

// ---------------------------------------------------------------------------
// DLS_unlock_callback
// ---------------------------------------------------------------------------

static void AILLIBCALLBACK DLS_unlock_callback(UINTa user, S32 buffer_section)
{
   AIL_load_sample_buffer(  ((HDLSDEVICE)user)->sample,
                            MSS_BUFFER_HEAD,
                            ((HDLSDEVICE)user)->buffer[buffer_section],
                            ((HDLSDEVICE)user)->buffer_size);

   AIL_start_sample(((HDLSDEVICE)user)->sample);
}

//############################################################################
//##                                                                        ##
//## Open a handle to either a software or hardware DLS device              ##
//##                                                                        ##
//## Returns 0 on error, else the device handle                             ##
//##                                                                        ##
//############################################################################

HDLSDEVICE AILCALL AIL_API_DLS_open(HMDIDRIVER mdi, HDIGDRIVER dig,
#ifdef IS_STATIC
                                    AILSTATICDLS const* dosdls,
#else
                                    char const* libname,
#endif
                                    U32 flags, U32 rate, S32 bits, S32 channels)
{
  HDLSDEVICE dls;
  #ifdef IS_WINDOWS
  HINSTANCE lib;
  #endif

  flags=flags;

  DOLOCK();

  //
  // try to load the DLS library
  //

  #ifdef IS_WINDOWS
  if (libname==0)
    lib=0;
  else {
    lib=LoadLibrary(libname);

    if (((UINTa)lib)<=32) {
      AIL_set_error("Error loading DLS library.");
      return(0);
    }
  }
  #endif

  dls=(HDLSDEVICE)AIL_mem_alloc_lock(sizeof(DLSDEVICE));

  if (dls==0)
    return(0);

  AIL_memset(dls,0,sizeof(DLSDEVICE));

  dls->mdi=mdi;
  dls->dig=dig;

  //
  // load all of the entry points
  //

  #if !defined(IS_MAC) && !defined(IS_LINUX) && !defined(__RADSEKRIT__)

  #ifdef IS_WINDOWS
  dls->lib=lib;

  if (lib==0) {
  #else
  if (dosdls==0) {
    dls->DOSname=0;
  #endif
  #endif
    dls->pMSSOpen=(VOIDFUNC *)DLSMSSOpen;
    dls->pOpen=0;
    dls->pClose=(VOIDFUNC *)DLSClose;
    dls->pLoadFile=(VOIDFUNC *)DLSLoadFile;
    dls->pLoadMem=(VOIDFUNC *)DLSLoadMemFile;
    dls->pUnloadFile=(VOIDFUNC *)DLSUnloadFile;
    dls->pUnloadAll=(VOIDFUNC *)DLSUnloadAll;
    dls->pGetInfo=(VOIDFUNC *)DLSGetInfo;
    dls->pCompact=(VOIDFUNC *)DLSCompactMemory;
    dls->pSetAttr=(VOIDFUNC *)DLSSetAttribute;
  #if !defined(IS_MAC) && !defined(IS_LINUX) && !defined(__RADSEKRIT__)
  } else {
    #ifdef IS_WINDOWS
      dls->pMSSOpen=(VOIDFUNC *)GetProcAddress(lib,"DLSMSSOpen");
      dls->pOpen=(VOIDFUNC *)GetProcAddress(lib,"DLSOpen");
      dls->pClose=(VOIDFUNC *)GetProcAddress(lib,"DLSClose");
      dls->pLoadFile=(VOIDFUNC *)GetProcAddress(lib,"DLSLoadFile");
      dls->pLoadMem=(VOIDFUNC *)GetProcAddress(lib,"DLSLoadMemFile");
      dls->pUnloadFile=(VOIDFUNC *)GetProcAddress(lib,"DLSUnloadFile");
      dls->pUnloadAll=(VOIDFUNC *)GetProcAddress(lib,"DLSUnloadAll");
      dls->pGetInfo=(VOIDFUNC *)GetProcAddress(lib,"DLSGetInfo");
      dls->pCompact=(VOIDFUNC *)GetProcAddress(lib,"DLSCompactMemory");
      dls->pSetAttr=(VOIDFUNC *)GetProcAddress(lib,"DLSSetAttribute");
    #else
      dls->DOSname=dosdls->description;
      dls->pMSSOpen=dosdls->pMSSOpen;
      dls->pOpen=dosdls->pOpen;
      dls->pClose=dosdls->pClose;
      dls->pLoadFile=dosdls->pLoadFile;
      dls->pLoadMem=dosdls->pLoadMem;
      dls->pUnloadFile=dosdls->pUnloadFile;
      dls->pUnloadAll=dosdls->pUnloadAll;
      dls->pGetInfo=dosdls->pGetInfo;
      dls->pCompact=dosdls->pCompact;
      dls->pSetAttr=dosdls->pSetAttr;
    #endif
  }
  #endif

  if (dls->pMSSOpen) {

    if ((mdi==0) || (dig==0)) {
      AIL_set_error("Trying to use the MSS DLS library with a null driver.");
     error1:
      #ifdef IS_WINDOWS
      if (lib)
        FreeLibrary(lib);
      #endif
      AIL_mem_free_lock(dls);
      return(0);
    }

   dls->format=((bits==16)?DIG_F_16BITS_MASK:0) | ((channels==2)?DIG_F_STEREO_MASK:0);

   dls->buffer_size=AIL_minimum_sample_buffer_size(dig,
                                                   rate,
                                                   dls->format);

   //
   // allocate the buffers
   //

   dls->buffer[0]  = AIL_mem_alloc_lock(dls->buffer_size);

   if (dls->buffer[0]==0) {
    error2:
     goto error1;
   }


   dls->buffer[1]  = AIL_mem_alloc_lock(dls->buffer_size);

   if (dls->buffer[1]==0) {
     AIL_mem_free_lock(dls->buffer[0]);
     goto error2;
   }

   //
   // allocate the sample handle
   //

   dls->sample     = AIL_allocate_sample_handle(dig);

   if (dls->sample==0) {
     AIL_set_error("Unable to allocate a sample handle.");
    error3:
     AIL_mem_free_lock(dls->buffer[0]);
     AIL_mem_free_lock(dls->buffer[1]);
     goto error1;
   }

   //
   // configure the sample handle
   //

   AIL_init_sample(dls->sample, dls->format);

   AIL_set_sample_playback_rate(dls->sample, rate);

   //
   // Call AIL DLS initialization function to initialize
   // software synthesizer
   //

   if (((DLSMSSOPEN)dls->pMSSOpen) ( &dls->DLSHandle,
                                     0,
                                     mdi,
                                     dls->format,
                                     rate,
                                     dls->buffer_size,
                                     (UINTa) dls,
                                     DLS_poll_callback,
                                     DLS_lock_callback,
                                     DLS_unlock_callback)!=DLS_NOERR)
    {

      AIL_set_error("DLSMSSOpen call failed.");
      goto error3;

    }

  } else if (dls->pOpen) {

    //
    // No MSS-specific initialization needed
    //

    if (((DLSOPEN)dls->pOpen) ( &dls->DLSHandle, 0 ) != DLS_NOERR) {
      AIL_set_error("DLSOpen call failed.");
      goto error1;
    }

  } else {

    AIL_set_error("DLS library functions not found - probably not a DLS DLL.");
    goto error1;

  }

  return(dls);
}


//############################################################################
//##                                                                        ##
//## Closes a previously opened DLS device                                  ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_DLS_close(HDLSDEVICE dls, U32 flags)
{
  HDLSFILEID di,ti;

  if (dls) {

    //
    // close the open DLS device
    //

    ((DLSCLOSE)(dls->pClose))( dls->DLSHandle, flags);


    //
    // free and release everything
    //

    #ifdef IS_WINDOWS
    if (dls->lib)
    {
      FreeLibrary(dls->lib);
    }
    #endif

    if (dls->sample)
      AIL_release_sample_handle(dls->sample);

    if (dls->buffer[0])
      AIL_mem_free_lock(dls->buffer[0]);

    if (dls->buffer[1])
      AIL_mem_free_lock(dls->buffer[1]);

    //
    // free all of the FILEIDs
    //

    di=dls->first;
    while (di) {
      ti=di->next;
      AIL_memset(di,0,sizeof(DLSFILEID));
      AIL_mem_free_lock(di);
      di=ti;
    }

    AIL_memset(dls,0,sizeof(DLSDEVICE));
    AIL_mem_free_lock(dls);

  }
}


//############################################################################
//##                                                                        ##
//## Loads a DLS file that is already in memory into a specified DLS device ##
//##                                                                        ##
//## Returns 0 on error, else the handle to the DLS file data               ##
//##                                                                        ##
//############################################################################

HDLSFILEID AILCALL AIL_API_DLS_load_memory(HDLSDEVICE dls, void const* memfile, U32 flags)
{
  SINTa id;
  HDLSFILEID fid;

  if ((dls==0) || (memfile==0))
    return(0);

  #if !defined(IS_XBOX) && !defined(IS_XENON) && !defined(__RADSEKRIT__)

  if (dls->pLoadMem==0) {
    #if defined(IS_MAC) || defined(IS_LINUX) || defined(IS_PS2) || defined(IS_PS3) || defined(IS_PSP)
      return( 0 );
    #else
    
    char buf[128];
    S32 DLS_size;

    // handle non-memory APIs

    if ( LE_SWAP32( ((U32*)memfile) )!=0x46464952)
    {
      AIL_set_error("Unable to load DLS memory file.");
      return(0);
    }

    DLS_size=  LE_SWAP32( &(((U32*)memfile)[1]) )+8;

    #ifdef IS_STATIC

      AIL_strcpy(buf,"temp.dls");

    #else

      #ifdef IS_32

        GetTempPath(127,buf);
        AIL_strcat(buf,"temp.dls");

      #else

        GetTempFileName(0,"DLS",0,buf);

      #endif

    #endif

    if (AIL_file_write(buf, memfile, DLS_size)==0) {
      AIL_set_error("Could not write temporary DLS file.");
      return(0);
    }

    fid=AIL_DLS_load_file(dls,buf,flags);

    #ifdef IS_WINDOWS
    {
    OFSTRUCT fs;
    OpenFile(buf,&fs,OF_DELETE);
    }
    #else
    #ifdef IS_DOS
      AIL_ferase(buf);
    #endif
    #endif

    return(fid);
    #endif
  } 
  else 
  #endif
  {

    if (((DLSLOADMEMFILE)dls->pLoadMem)
      (dls->DLSHandle,flags|DLS_OVERWRITE_EXISTING_PATCH,&id,memfile)!=DLS_NOERR) {
      AIL_set_error("Unable to load DLS memory file.");
      return(0);
    }

  }

  fid=(HDLSFILEID)AIL_mem_alloc_lock(sizeof(DLSFILEID));

  if (fid==0)
    return(0);

  AIL_memset(fid,0,sizeof(DLSFILEID));

  fid->id=id;
  fid->next=dls->first;

  dls->first=fid;

  return(fid);
}


//############################################################################
//##                                                                        ##
//## Loads DLS file into the specified DLS device                           ##
//##                                                                        ##
//## Returns 0 on error, else the handle to the DLS file data               ##
//##                                                                        ##
//############################################################################

HDLSFILEID AILCALL AIL_API_DLS_load_file(HDLSDEVICE dls, char const* filename, U32 flags)
{
  SINTa id;
  HDLSFILEID fid;

  if ((dls==0) || (filename==0))
    return(0);

  if (((DLSLOADFILE)dls->pLoadFile)
   (dls->DLSHandle,flags|DLS_OVERWRITE_EXISTING_PATCH,&id,filename)!=DLS_NOERR) {
    AIL_set_error("Unable to load DLS memory.");
    return(0);
  }

  fid=(HDLSFILEID)AIL_mem_alloc_lock(sizeof(DLSFILEID));

  if (fid==0)
    return(0);

  AIL_memset(fid,0,sizeof(DLSFILEID));

  fid->id=id;
  fid->next=dls->first;

  dls->first=fid;

  return(fid);
}


//############################################################################
//##                                                                        ##
//## Unloads a previously loaded DLS file or DLS data                       ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_DLS_unload(HDLSDEVICE dls, HDLSFILEID dlsid)
{
  HDLSFILEID di,ti;

  if (dls) {

    if (dlsid==AIL_DLS_UNLOAD_MINE) {

      // unload just ours

      if (dls->pUnloadAll)
        ((DLSUNLOADALL)dls->pUnloadAll)(dls->DLSHandle,DLS_REMOVE_ONLY_MINE);

      goto freethefiles;

    } else if (dlsid==AIL_DLS_UNLOAD_ALL) {

      // unload just everything

      if (dls->pUnloadAll)
        ((DLSUNLOADALL)dls->pUnloadAll)(dls->DLSHandle,DLS_REMOVE_ALL);

     freethefiles:

      //
      // since we've unloaded everything, free the DLSFILEIDs
      //

      di=dls->first;
      while (di) {
        ti=di->next;
        AIL_memset(di,0,sizeof(DLSFILEID));
        AIL_mem_free_lock(di);
        di=ti;
      }

    } else {

      if (dls->first) {

        //
        // find the pointer and free it
        //

        if (dls->first==dlsid)

          dls->first=dlsid->next;

        else {

          HDLSFILEID di=dls->first;

          while ((di) && (di->next!=dlsid))
            di=di->next;

          if (di)
            di->next=di->next->next;

        }

        //
        // call the unload
        //

        if (dls->pUnloadFile)
          ((DLSUNLOADFILE)dls->pUnloadFile)(dls->DLSHandle,dlsid->id);


        //
        // clear and free the memory
        //

        AIL_memset(dlsid,0,sizeof(DLSFILEID));
        AIL_mem_free_lock(dlsid);

        }
    }

  }
}


//############################################################################
//##                                                                        ##
//## Compacts the memory of the specified DLS device                        ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_DLS_compact(HDLSDEVICE dls)
{
   if (dls)
   {
     if (dls->pCompact)
       ((DLSCOMPACTMEMORY)dls->pCompact)(dls->DLSHandle);
   }
}


//############################################################################
//##                                                                        ##
//## Returns information about the specified DLS device                     ##
//##                                                                        ##
//############################################################################

void AILCALL AIL_API_DLS_get_info(HDLSDEVICE dls, AILDLSINFO* info, S32* PercentCPU)
{
  if (dls) {

    if ((info) && (dls->pGetInfo)) {
      DLS_INFO_STRUC di;

      #if !defined(IS_MAC) && !defined(IS_LINUX) && !defined(__RADSEKRIT__)

      #ifdef IS_WINDOWS
      if (dls->lib==0)
      #else
      if (dls->DOSname==0)
      #endif
      #endif
        AIL_strcpy(info->Description,"Miles DLS Software Synthesizer");
      #if !defined(IS_MAC) && !defined(IS_LINUX) && !defined(__RADSEKRIT__)
      else {
        #ifdef IS_WINDOWS
        GetModuleFileName(dls->lib,info->Description,127);
        #else
        AIL_strcpy(info->Description,dls->DOSname);
        #endif
      }
      #endif

      di.dwSize=sizeof(di);
      if (((DLSGETINFO)dls->pGetInfo)(&di)==DLS_NOERR) {
        info->MaxDLSMemory=di.dwMaxDlsMem;
        info->CurrentDLSMemory=di.dwCurrDlsMem;
        info->LargestSize=di.dwLargestBuf;
        info->GMAvailable=di.dwGMBankStatus&1;
        info->GMBankSize=di.dwGMBankSize;
      }
    }

    if (PercentCPU)
      *PercentCPU=
      #if defined(IS_MAC) || defined(IS_LINUX) || defined(__RADSEKRIT__)
      DLSMSSGetCPU(dls->DLSHandle);
      #else
        #ifdef IS_WINDOWS
        (dls->lib==0)
        #else
        (dls->DOSname==0)
        #endif
        ?DLSMSSGetCPU(dls->DLSHandle):0;
      #endif
  }
}

#ifdef IS_DOS

//############################################################################
//##                                                                        ##
//## End of locked code                                                     ##
//##                                                                        ##
//############################################################################

static void AIL_lock_end(void)
{
   if (locked)
      {
      AIL_vmm_unlock_range(AIL_lock_start, AIL_lock_end);

      locked = 0;
      }
}

#endif


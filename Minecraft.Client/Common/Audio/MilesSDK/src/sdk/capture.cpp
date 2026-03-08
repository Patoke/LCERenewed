//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  API.CPP: FLT module for capturing output data to a wave file          ##
//##                                                                        ##
//##  32-bit protected-mode source compatible with MSC 11.0/Watcom 10.6     ##
//##                                                                        ##
//##  Version 1.00 of 5-Feb-99: Initial                                     ##
//##                                                                        ##
//##  Author: John Miles                                                    ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################


//############################################################################
//
// How to use this filter:
//
//   1) Get filter's HPROVIDER via enumeration or AIL_find_filter("Capture Filter") 
//   2) Set the filter-level preference "Filename" to the output filename
//   3) Associate the filter with the driver with AIL_open_filter()
//
//############################################################################

#if defined(_SEKRIT)

#elif defined(_XENON) || ( defined(_XBOX_VER) && (_XBOX_VER == 200) ) || defined(_XBOX)
  #define NOD3D
  #include <xtl.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(__NT__) || defined(__WIN32__)
  #define WIN32_LEAN_AND_MEAN
  #define WIN32_EXTRA_LEAN
  #define STRICT
  #include <windows.h>
#endif

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mss.h"
#include "imssapi.h"
#include "rrAtomics.h" // @cdep ignore


#if defined( IS_WIN32API ) && !defined(FORNONWIN)  && !defined(__RADSEKRIT__)

#define CAPTURE_BUFFER_SIZE 65536*16     // 1 meg capture buffer by default

//
// Attribute tokens
//

enum PROP
{
   _FX_PROVIDER_FLAGS,
   _FX_PROVIDER_DRIVER_STATE_SIZE,
   _FX_PROVIDER_SAMPLE_STATE_SIZE,

  FILENAME,
  SERVICE_THREAD,
  RECORD_ENABLED,
};

//
// Driver state descriptor
//
// One state descriptor is associated with each HDIGDRIVER
//

struct DRIVERSTATE
{
   //
   // Members common to all pipeline filter providers
   //

   HDIGDRIVER       dig;                  // Driver with which this descriptor is associated
   MSS_ALLOC_TYPE * alloc_CB;
   MSS_FREE_TYPE * free_CB;
   UINTa user;

   //
   // Members associated with specific filter provider
   //

   U8         *capture_buffer;        // start of capture buffer
   U8         *capture_end;           // end of capture buffer
   S32             capture_buffer_size;

   U8         *current_head;          // writing data out of here
   U8         *current_tail;          // adding data here

   S32            capturing;

   HANDLE         fhand;
   U32            total_written;
};

//
// Per-sample filter state descriptor
//
// One state descriptor is associated with each HSAMPLE
//

struct SAMPLESTATE
{
   //
   // Members common to all pipeline filter providers
   //

   HSAMPLE          sample;   // HSAMPLE with which this descriptor is associated
   DRIVERSTATE *driver;   // Driver with which this descriptor is associated

   //
   // Members associated with specific filter provider (none)
   //

};

//
// Globals
//

static S32 FLT_started = 0;

static C8 FLT_error_text[256];

static char filename[256]="default.wav";
static char oldfilename[256]="default.wav";

static S32 enabled = 0;

static HANDLE threadhandle,threadwait;

//############################################################################
//#                                                                          #
//# Retrieve a standard RIB provider attribute by index                      #
//#                                                                          #
//############################################################################

static S32 AILCALL PROVIDER_property (HPROPERTY index, void * before_value, void const * new_value, void * after_value)
{
   switch ( /* (PROP) */ index )
      {
      case PROVIDER_NAME:
        if (before_value)
        {
          *(char **)before_value = "Capture Filter";
          return 1;
        }
        break;
        
      case PROVIDER_VERSION:
        if (before_value) 
        {
          *(S32*)before_value = 0x130;
          return 1;
        }
        break;

      case _FX_PROVIDER_SAMPLE_STATE_SIZE:
        if (before_value)
        {
          *(S32*)before_value = ( sizeof(SAMPLESTATE) + 15 ) & ~15;
          return 1;
        }
        break;

      case _FX_PROVIDER_DRIVER_STATE_SIZE:
        if (before_value)
        {
          *(S32*)before_value = ( sizeof(DRIVERSTATE) + 15 ) & ~15;
          return 1;
        }
        break;

      case _FX_PROVIDER_FLAGS:
        if (before_value)
        {
          *(S32*)before_value = FPROV_ON_POSTMIX;
          return 1;
        }
        break;

      case RECORD_ENABLED:
        if (before_value)
        {
          *(S32*)before_value = enabled;
        }
        if (new_value)
         {
         enabled = *(S32 *) new_value;
         }
       if (after_value)
         {
          *(S32*)after_value = enabled;
         }

        return 1;

     case SERVICE_THREAD:
        if (before_value) 
        {
          *(SINTa*) before_value = (SINTa) threadhandle;
          return 1;
        }
        break;

     case FILENAME:
       if (new_value)
       {
          AIL_strcpy(oldfilename,filename);
          AIL_strcpy(filename,(char const *)new_value);
       }
       if (before_value) *(char **)before_value = oldfilename;
       if (after_value) *(char **)before_value = filename;
       return 1;

      }
  return 0;
}

//############################################################################
//#                                                                          #
//# Function to write out a wave file header                                 #
//#                                                                          #
//############################################################################

static void write_header(DRIVERSTATE* DRV)
{
   WAVEOUTEXT wo;
   DWORD wrote;

   AIL_memcpy(&wo.riffmark,"RIFF",4);
   STORE_LE_SWAP32(&wo.rifflen, DRV->total_written+sizeof(WAVEOUT)-8);

   AIL_memcpy(&wo.wavemark,"WAVE",4);
   AIL_memcpy(&wo.fmtmark,"fmt ",4);
   STORE_LE_SWAP32(&wo.fmtlen,( ((char*)&wo.datamark)-((char*)&wo.fmttag) ));
   STORE_LE_SWAP16(&wo.fmttag,0xfffe); 
   STORE_LE_SWAP16(&wo.channels,(S16)DRV->dig->logical_channels_per_sample);
   STORE_LE_SWAP32(&wo.sampersec,DRV->dig->DMA_rate);
   STORE_LE_SWAP16(&wo.bitspersam,(S16)16 );
   STORE_LE_SWAP16(&wo.blockalign,(S16)(((S32)wo.bitspersam*(S32)wo.channels) / 8));
   STORE_LE_SWAP32(&wo.avepersec,(DRV->dig->DMA_rate *(S32)wo.bitspersam*(S32)wo.channels) / 8);
   AIL_memcpy(&wo.datamark,"data",4);
   STORE_LE_SWAP32(&wo.datalen,DRV->total_written);

   STORE_LE_SWAP16(&wo.size, (S16) ( ((char*)&wo.datamark)-((char*)&wo.sampbits) ) );
   STORE_LE_SWAP16(&wo.sampbits, (S16)16 );

   U32 masks[] = // .wav channel masks for mono through 8.1 configurations
      {
      0,          // (invalid)
      1 << 0,
      (1 << 0) | (1 << 1),
      (1 << 0) | (1 << 1) | (1 << 8),
      (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5),
      (1 << 0) | (1 << 1) | (1 << 2) | (1 << 4) | (1 << 5),
      (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5),
      (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 8),
      (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 9) | (1 << 10),
      (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 8) | (1 << 9) | (1 << 10)
      };

   STORE_LE_SWAP32(&wo.chanmask, masks[DRV->dig->logical_channels_per_sample]);

   static const U8 SUBTYPE_PCM[] = { 1,0,0,0, 0, 0, 16, 0, 0x80, 0, 0, 0xaa,  0, 0x38, 0x9b, 0x71 };
   AIL_memcpy(&wo.subfmt, SUBTYPE_PCM, 16 );

   SetFilePointer(DRV->fhand,0,0,FILE_BEGIN);
   WriteFile(DRV->fhand,&wo,sizeof(wo),&wrote,0);
}


//############################################################################
//#                                                                          #
//# Function to dump data accumulated in the capture buffer                  #
//#                                                                          #
//############################################################################

static U32 volatile in_do_output=0;

static void do_output(DRIVERSTATE* DRV)
{
  rrAtomicAddExchange32(&in_do_output, 1);

  while (in_do_output!=1)
    Sleep(1);

  while (DRV->current_tail!=DRV->current_head) {
    UINTa writeamt;
    DWORD wrote;

    writeamt=(DRV->current_tail<DRV->current_head)?(DRV->capture_end-DRV->current_head):(DRV->current_tail-DRV->current_head);
    if (writeamt>32768)
      writeamt=32768;

    WriteFile(DRV->fhand,DRV->current_head,(DWORD) writeamt,&wrote,0);

    Sleep(1);

    DRV->current_head+=writeamt;
    if (DRV->current_head>=DRV->capture_end)
      DRV->current_head-=DRV->capture_buffer_size;

    DRV->total_written+=wrote;
  }

  rrAtomicAddExchange32(&in_do_output,-1);
}


//############################################################################
//#                                                                          #
//# Return FLT filter error text                                             #
//#                                                                          #
//############################################################################

static C8 *       AILCALL FLT_error       (void)
{
   if (!AIL_strlen(FLT_error_text))
      {
      return 0;
      }

   return FLT_error_text;
}

//############################################################################
//#                                                                          #
//# Initialize FLT sample filter                                             #
//#                                                                          #
//############################################################################

static FLTRESULT AILCALL FLT_startup     (void)
{
   if (FLT_started++)
      {
      strcpy(FLT_error_text,"Already started");
      return FLT_ALREADY_STARTED;
      }

   //
   // Init static prefs/attributes
   //

   FLT_error_text[0]            = 0;

   return FLT_NOERR;
}

//############################################################################
//#                                                                          #
//# Shut down FLT sample filter                                              #
//#                                                                          #
//############################################################################

static FLTRESULT      AILCALL FLT_shutdown    (void)
{
   if (!FLT_started)
      {
      strcpy(FLT_error_text,"Not initialized");
      return FLT_NOT_INIT;
      }

   --FLT_started;

   return FLT_NOERR;
}


//############################################################################
//#                                                                          #
//# Background thread that does the writing to disk                          #
//#                                                                          #
//############################################################################

DWORD WINAPI write_thread(LPVOID user)
{
  DRIVERSTATE* DRV=(DRIVERSTATE*)user;

  while (WaitForSingleObject(threadwait,300)==WAIT_TIMEOUT) {

    if (DRV->capturing)
      do_output(DRV);

  }

  return(0);
}

//############################################################################
//#                                                                          #
//# Allocate driver-specific descriptor                                      #
//#                                                                          #
//############################################################################

static HDRIVERSTATE AILCALL FLT_open_driver (MSS_ALLOC_TYPE * palloc,
                                             MSS_FREE_TYPE  * pfree,
                                             UINTa         user,  
                                             HDIGDRIVER dig, void * memory)

{
   U32 id;
   DRIVERSTATE *DRV;

   if (*filename==0) {
     strcpy(FLT_error_text,"NULL filename");
     return(0);
   }

   DRV = (DRIVERSTATE *) memory;

   if (DRV == NULL)
      {
      strcpy(FLT_error_text,"Out of memory");
      return 0;
      }

   AIL_memset(DRV,
              0,
              sizeof(DRIVERSTATE));

   //
   // Initialize generic members
   // Filter captures contents of build buffer 0 (front left/right) by default
   //

   DRV->dig               = dig;
   DRV->alloc_CB = palloc;
   DRV->free_CB = pfree;
   DRV->user = user;

   //
   // Initialize provider-specific members to their default values
   //

   DRV->capturing=0;

   // make it so the buffer is divisible by any MSS supported speaker count
   DRV->capture_buffer_size=((CAPTURE_BUFFER_SIZE+(sizeof(S16)*9*8*7*5)-1)/(sizeof(S16)*9*8*7*5))*(sizeof(S16)*9*8*7*5);

   DRV->capture_buffer = (U8 *) palloc(DRV->capture_buffer_size, user, __FILE__,__LINE__);
   if (DRV->capture_buffer == NULL)
      {
      strcpy(FLT_error_text,"Could not allocate capture buffer");
      pfree(DRV, user, __FILE__,__LINE__);
      return 0;
      }

   DRV->capture_end=DRV->capture_buffer+DRV->capture_buffer_size;

   DRV->current_head=DRV->capture_buffer;
   DRV->current_tail=DRV->capture_buffer;

   //
   // Create thread to perform output
   // TODO: Make a single thread perform output writes for multiple drivers
   // (and make filenames non-global, too)
   //

   threadwait=CreateEvent(0,TRUE,0,0);
   if (threadwait==0) {
     strcpy(FLT_error_text,"Could not create thread event");
     goto err;
   }

   threadhandle=CreateThread(0,0,write_thread,(LPVOID)DRV,0,(LPDWORD)&id);
   if (threadhandle==0) {
     strcpy(FLT_error_text,"Could not create IO thread");
     goto err;
   }

   DRV->fhand=CreateFile(filename,
                        GENERIC_WRITE,
                        0,
                        NULL,
                        CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

   if (DRV->fhand==INVALID_HANDLE_VALUE) {
     strcpy(FLT_error_text,"Could not create file");
    err:
     pfree(DRV->capture_buffer, user, __FILE__,__LINE__);
     pfree(DRV, user, __FILE__,__LINE__);
     return(NULL);
   }

   DRV->current_head=DRV->capture_buffer;
   DRV->current_tail=DRV->capture_buffer;
   DRV->total_written=0;

   write_header(DRV);

   DRV->capturing=1;  

   //
   // Return descriptor address cast to handle
   //

   return (HSAMPLESTATE) DRV;
}

//############################################################################
//#                                                                          #
//# Close filter driver instance                                             #
//#                                                                          #
//############################################################################

static FLTRESULT     AILCALL FLT_close_driver (HDRIVERSTATE state)
{
   DRIVERSTATE *DRV = (DRIVERSTATE *) state;

   // shut down the thread
   SetEvent(threadwait);
   WaitForSingleObject(threadhandle,INFINITE);
   CloseHandle(threadwait);
   CloseHandle(threadhandle);

   //turn off capturing
   if (DRV->capturing) {
     DRV->capturing=0;
     do_output(DRV);
     write_header(DRV);
     CloseHandle(DRV->fhand);
   }

   if (DRV->capture_buffer != NULL)
      {
      DRV->free_CB(DRV->capture_buffer, DRV->user, __FILE__,__LINE__);
      DRV->capture_buffer = NULL;
      }

   return FLT_NOERR;
}

//############################################################################
//#                                                                          #
//#  Perform any needed processing before per-sample mixing begins           #
//#                                                                          #
//#  Called after the build buffer has been flushed prior to the mixing      #
//#  phase, but before any samples have been mixed into it                   #
//#                                                                          #
//############################################################################

void AILCALL FLT_premix_process (HDRIVERSTATE driver)
{
}

//############################################################################
//#                                                                          #
//#  Process data after mixing                                               #
//#                                                                          #
//#  Called after all samples have been mixed into the 32-bit build buffer,  #
//#  prior to copying the build-buffer contents to the driver's output       #
//#  buffer                                                                  #
//#                                                                          #
//############################################################################

S32 AILCALL FLT_postmix_process (HDRIVERSTATE driver, //)
                                 void *   output_buffer)
{
   if (!enabled)
      {
      return 0;
      }

   DRIVERSTATE *DRV = (DRIVERSTATE *) driver;
   MSS_BB * build = DRV->dig->build;

   //calculate how much space we have left
   UINTa leftincap=(UINTa)DRV->current_head;
   if (leftincap==(UINTa)DRV->current_tail)
     leftincap=DRV->capture_buffer_size;
   else if (leftincap>(UINTa)DRV->current_tail)
     leftincap=(leftincap-(UINTa)DRV->current_tail)-2;
   else
     leftincap=((DRV->capture_buffer_size+leftincap)-(UINTa)DRV->current_tail)-2;

   U32 mono_buf = 0;
   U32 stereo_bufs = DRV->dig->n_build_buffers;
   U32 count = build[0].bytes >> 2; // sizeof(S32)
   count >>= (build[stereo_bufs-1].chans - 1); // 1 for stereo, 0 for mono

   if ( build[stereo_bufs-1].chans < 2 )
   {
     --stereo_bufs;
     mono_buf = 1;
   }

   {
     leftincap/=(2*DRV->dig->logical_channels_per_sample);

     U32 i = 0;
     while( leftincap-- )
     {
       U32 b;
       for( b = 0 ; b < stereo_bufs; b++ )
       {
         S32 val;
         val=((S32*)build[b].buffer)[i+i];
         val>>=11;
         if (val>32767)
           val=32767;
         else if (val<-32768)
           val=-32768;

         STORE_LE_SWAP16((S16*)DRV->current_tail,(S16)val);
         DRV->current_tail+=2;

         val=((S32*)build[b].buffer)[i+i+1];
         val>>=11;
         if (val>32767)
           val=32767;
         else if (val<-32768)
           val=-32768;

         STORE_LE_SWAP16((S16*)DRV->current_tail,(S16)val);
         DRV->current_tail+=2;
         if (DRV->current_tail>=DRV->capture_end)
           DRV->current_tail-=DRV->capture_buffer_size;
       }

       if ( mono_buf )
       {
         S32 val;
         val=((S32*)build[b].buffer)[i];
         val>>=11;
         if (val>32767)
           val=32767;
         else if (val<-32768)
           val=-32768;
         STORE_LE_SWAP16((S16*)DRV->current_tail,(S16)val);
         DRV->current_tail+=2;
         if (DRV->current_tail>=DRV->capture_end)
           DRV->current_tail-=DRV->capture_buffer_size;
       }

       ++i;
       if ( i >= count )
         break;
     }
   }

   return 0;
}

extern "C" S32 CaptureMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern "C" S32 CaptureMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg )
{
   const RIB_INTERFACE_ENTRY FLT[] =
      {
      REG_FN(PROVIDER_property),
      REG_PR("Name",                     PROVIDER_NAME,        (RIB_DATA_SUBTYPE) (RIB_STRING|RIB_READONLY)),
      REG_PR("Version",                  PROVIDER_VERSION,     (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),
      REG_PR("Flags",                   _FX_PROVIDER_FLAGS,    (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),
      REG_PR("Sample_state_size",       _FX_PROVIDER_SAMPLE_STATE_SIZE, (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),
      REG_PR("Driver_state_size",       _FX_PROVIDER_DRIVER_STATE_SIZE, (RIB_DATA_SUBTYPE) (RIB_HEX|RIB_READONLY)),

      REG_FN(FLT_startup),
      REG_FN(FLT_error),
      REG_FN(FLT_shutdown),

      REG_FN(FLT_open_driver),
      REG_FN(FLT_close_driver),

      REG_FN(FLT_premix_process),
      REG_FN(FLT_postmix_process),

      REG_PR("Filename", FILENAME, RIB_STRING),
      REG_PR("Service thread", SERVICE_THREAD, (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_HEX)),
      REG_PR("Enabled", RECORD_ENABLED, (RIB_DATA_SUBTYPE) (RIB_BOOL)),
      };

   if (up_down)
      {
      RIB_registerP(provider_handle,
                  "MSS pipeline filter",
                   FLT);
      }
   else
      {
      RIB_unregister_allP(provider_handle);
      }

   return TRUE;
}

#endif

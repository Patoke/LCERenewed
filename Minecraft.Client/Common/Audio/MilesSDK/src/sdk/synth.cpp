//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  SYNTH.CPP: Virtual wavetable synthesizer for XMIDI-triggered          ##
//##             DLS voices                                                 ##
//##                                                                        ##
//##  16-bit protected-mode source compatible with MSC 7.0                  ##
//##  32-bit protected-mode source compatible with MSC 11.0/Watcom 10.6     ##
//##                                                                        ##
//##  Version 1.00 of 12-Oct-97: Initial                                    ##
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

#include <limits.h>

#include "mss.h"
#include "imssapi.h"

#include "mssdls.h"
#include "dls1.h"
#include "synth.h"


// ---------------------------------------------------------------------------
// Preference initialization
// ---------------------------------------------------------------------------

#define SHOW_DEBUG_TRACE 0    // Do not show debug text output
#define DEBUG_LOGFILE    0    // Do not write debug output to debug.log file


//############################################################################
//#                                                                          #
//# Former member functions from synth.h                                     #
//#                                                                          #
//############################################################################

void INS_init(INSTRUMENT *INS, void const *data)
{
   data=data;
   INS->DLS_file    = -1;
   INS->region_list = NULL;
   INS->art_list    = NULL;
}

void INS_cleanup(INSTRUMENT *INS)
{
   if (INS->region_list != NULL)
      {
      AIL_mem_free_lock(INS->region_list);
      INS->region_list = NULL;
      }

   if (INS->art_list != NULL)
      {
      AIL_mem_free_lock(INS->art_list);
      INS->art_list = NULL;
      }
}

S32 INS_is_match(INSTRUMENT *INS, void *data)
{
   data=data;
   return 0;
}

U32 INS_set(void const *data)
{
   data=data;
   return 0;
}

void ILM_construct(InsListMgr *ILM)
   {
   ILM->current_size = INS_BLOCK_SIZE;

   ILM->array = (INSTRUMENT*) AIL_mem_alloc_lock(sizeof(INSTRUMENT) * (size_t) ILM->current_size);

   S32 i;
   for (i=0; i < ILM->current_size; i++)
      {
      ILM->array[i].prev = &ILM->array[i-1];
      ILM->array[i].next = &ILM->array[i+1];
      ILM->array[i].entry_num = -1;
      }

   ILM->array[0].prev = NULL;
   ILM->array[ILM->current_size-1].next = NULL;

   ILM->used = NULL;
   ILM->avail = &ILM->array[0];

   for (i=0; i < INS_SET_DIM; i++) ILM->table[i] = NULL;
   }

void ILM_dealloc(InsListMgr *ILM, INSTRUMENT *entry)
   {
   AIL_lock_mutex();
   INS_cleanup(entry);

   if (entry->next_in_set) entry->next_in_set->prev_in_set = entry->prev_in_set;
   if (entry->prev_in_set) entry->prev_in_set->next_in_set = entry->next_in_set;

   entry->entry_num = -1;

   if (entry->next) entry->next->prev = entry->prev;
   if (entry->prev) entry->prev->next = entry->next;

   if (ILM->used == entry) ILM->used = entry->next;

   if (ILM->table[entry->set_key] == entry)
      ILM->table[entry->set_key] = entry->next_in_set;

   entry->next = ILM->avail;
   entry->prev = NULL;

   if (ILM->avail) ILM->avail->prev = entry;
   ILM->avail = entry;
   AIL_unlock_mutex();
   }

void ILM_dealloc(InsListMgr *ILM, S32 entry_num)
{
   if ((entry_num != -1) && (ILM->array[entry_num].entry_num != -1)) ILM_dealloc(ILM, &ILM->array[entry_num]);
}

void ILM_cleanup(InsListMgr *ILM)
{
   if (ILM->array)
      {
	   while (ILM->used) ILM_dealloc(ILM, ILM->used);

	   ILM->current_size = 0;
	   AIL_mem_free_lock(ILM->array);
	   ILM->array = NULL;
      }
}

void ILM_destroy(InsListMgr *ILM)
{
   ILM_cleanup(ILM);
}

INSTRUMENT *ILM_lookup(InsListMgr *ILM, void *seed)
{
   INSTRUMENT *found = ILM->table[INS_set(seed)];

   while (found)
      {
      if (INS_is_match(found, seed))
         break;

      found = found->next_in_set;
      }

   return found;
}

INSTRUMENT *ILM_alloc(InsListMgr *ILM, void const *seed)
{
   U32 key = INS_set(seed);
   INSTRUMENT *entry;

   if (ILM->avail == NULL)
      {
      S32 prev_size = ILM->current_size;
      S32 new_size = prev_size + INS_BLOCK_SIZE;

      INSTRUMENT *old_array = ILM->array;
      INSTRUMENT *new_array = (INSTRUMENT *) AIL_mem_alloc_lock(sizeof(INSTRUMENT) * (size_t) new_size);
      if (new_array == NULL) return NULL;

      UINTa adjust = ((UINTa) new_array) - ((UINTa) old_array);

      S32 i;
      
      for (i=0; i < prev_size; i++)
         {
         new_array[i] = old_array[i];

         if (new_array[i].next_in_set) new_array[i].next_in_set = (INSTRUMENT *) (((UINTa) new_array[i].next_in_set) + adjust);
         if (new_array[i].prev_in_set) new_array[i].prev_in_set = (INSTRUMENT *) (((UINTa) new_array[i].prev_in_set) + adjust);

         if (new_array[i].next) new_array[i].next = (INSTRUMENT *) (((UINTa) new_array[i].next) + adjust);
         if (new_array[i].prev) new_array[i].prev = (INSTRUMENT *) (((UINTa) new_array[i].prev) + adjust);
         }

      for (i=0; i < INS_SET_DIM; i++)
         if (ILM->table[i]) ILM->table[i] = (INSTRUMENT *) (((UINTa) ILM->table[i]) + adjust);

      for (i=prev_size; i < new_size; i++)
         {
         new_array[i].entry_num = -1;
         new_array[i].prev = &new_array[i-1];
         new_array[i].next = &new_array[i+1];
         }

      new_array[prev_size].prev = NULL;
      new_array[new_size-1].next = NULL;
      ILM->used = (INSTRUMENT *) (((UINTa) ILM->used) + adjust);
      ILM->avail = &new_array[prev_size];
      ILM->current_size = new_size;

      AIL_mem_free_lock(ILM->array);
      ILM->array = new_array;
      }

   entry = ILM->avail;
   ILM->avail = entry->next;
   entry->next = ILM->used;
   entry->prev = NULL;

   if (ILM->used) ILM->used->prev = entry;
   ILM->used = entry;

   entry->set_key = key;
   entry->next_in_set = ILM->table[key];
   entry->prev_in_set = NULL;

   if (ILM->table[key])
      ILM->table[key]->prev_in_set = entry;

   ILM->table[key] = entry;      // TODO: 64-bit...
   entry->entry_num = (S32) ((((UINTa)(entry)) - ((UINTa)(ILM->array))) / sizeof(INSTRUMENT));

   INS_init(entry, seed);

   return entry;
}

void DF_init(DLS_FILE *DF, void const *data)
{
   data=data;
   DF->filename[0] = 0;
   DF->image       = NULL;
   DF->ptbl        = NULL;
   DF->cues        = NULL;
   DF->WAVE_list   = NULL;
}

void DF_cleanup(DLS_FILE *DF)
{
   //
   // Terminate any voices which use an instrument defined in this file,
   // before freeing file resources
   //

   S32 killed = 0;

   if (DLS) {
   
     for (S32 vn=0; vn < DLS->n_voices; vn++)
        {
        VOICE *V = &DLS->voice_list[vn];

        if ((V == NULL) || (!V->active))
           {
           continue;
           }

        if (instrument_list->array[V->trigger.instrument_index].DLS_file == DF->entry_num)
           {
           V->active = FALSE;
           ++killed;
           }
        }
   }

   //
   // If we terminated any voices, pause briefly to ensure
   // any background timer tick in progress has a chance to finish
   // executing before freeing instrument data
   //

   if (killed)
      {
      AIL_delay(1);
      }

   //
   // Free all instrument entries which refer to file
   //

   INSTRUMENT *ptr = instrument_list->used;

   while (ptr != NULL)
      {
      INSTRUMENT *next = ptr->next;

      if (ptr->DLS_file == DF->entry_num)
         {
         ILM_dealloc(instrument_list, ptr);
         }

      ptr = next;
      }

   //
   // Free cloned selectors
   //

   if (DF->ptbl != NULL)
      {
      AIL_ptr_free_clone(DF->ptbl);
      DF->ptbl = NULL;
      }

   if (DF->cues != NULL)
      {
      AIL_ptr_free_clone(DF->cues);
      DF->cues = NULL;
      }

   //
   // Free file image and other data
   //

   if (DF->image != NULL)
      {
      //
      // Free file data image only if we loaded it (as opposed to the
      // app having passed it to us with DLSLoadMemFile)
      //

      if (AIL_strcmp(DF->filename,"(Memory file)"))
         {
         AIL_mem_free_lock((void*)DF->image);
         DF->image = NULL;
         }
      }

   if (DF->WAVE_list != NULL)
      {
      AIL_mem_free_lock(DF->WAVE_list);
      DF->WAVE_list = NULL;
      }
}

S32 DF_is_match(DLS_FILE *DF, void const *data)
{
   data=data;
   return 0;
}

U32 DF_set(void const *data)
{
   data=data;
   return 0;
}

void FLM_construct(FileListMgr *FLM)
{
   FLM->current_size = FILE_BLOCK_SIZE;

   FLM->array = (DLS_FILE *) AIL_mem_alloc_lock(sizeof(DLS_FILE) * (size_t) FLM->current_size);

   S32 i;
   for (i=0; i < FLM->current_size; i++)
      {
      FLM->array[i].prev = &FLM->array[i-1];
      FLM->array[i].next = &FLM->array[i+1];
      FLM->array[i].entry_num = -1;
      }

   FLM->array[0].prev = NULL;
   FLM->array[FLM->current_size-1].next = NULL;

   FLM->used = NULL;
   FLM->avail = &FLM->array[0];

   for (i=0; i < FILE_SET_DIM; i++) FLM->table[i] = NULL;
}

void FLM_dealloc(FileListMgr *FLM, DLS_FILE *entry)
{
   DF_cleanup(entry);

   if (entry->next_in_set) entry->next_in_set->prev_in_set = entry->prev_in_set;
   if (entry->prev_in_set) entry->prev_in_set->next_in_set = entry->next_in_set;

   entry->entry_num = -1;

   if (entry->next) entry->next->prev = entry->prev;
   if (entry->prev) entry->prev->next = entry->next;

   if (FLM->used == entry) FLM->used = entry->next;

   if (FLM->table[entry->set_key] == entry) 
      FLM->table[entry->set_key] = entry->next_in_set;

   entry->next = FLM->avail;
   entry->prev = NULL;

   if (FLM->avail) FLM->avail->prev = entry;
   FLM->avail = entry;
}

void FLM_dealloc(FileListMgr *FLM, S32 entry_num)
{
   if ((entry_num != -1) && (FLM->array[entry_num].entry_num != -1)) FLM_dealloc(FLM, &FLM->array[entry_num]);
}

void FLM_destroy(FileListMgr *FLM)
{
   FLM_cleanup(FLM);
}

void FLM_cleanup(FileListMgr *FLM)
{
   if (FLM->array)
      {
	   while (FLM->used) FLM_dealloc(FLM, FLM->used);

	   FLM->current_size = 0;
	   AIL_mem_free_lock(FLM->array);
	   FLM->array = NULL;
      }
}

DLS_FILE *FLM_lookup(FileListMgr *FLM, void const *seed)
{
   DLS_FILE *found = FLM->table[DF_set(seed)];

   while (found)
      {
      if (DF_is_match(found, seed))
         break;

      found = found->next_in_set;
      }

   return found;
}

DLS_FILE *FLM_alloc(FileListMgr *FLM, void const *seed)
{
   U32 key = DF_set(seed);
   DLS_FILE *entry;

   if (FLM->avail == NULL)
      {
      S32 prev_size = FLM->current_size;
      S32 new_size = prev_size + FILE_BLOCK_SIZE;

      DLS_FILE *old_array = FLM->array;
      DLS_FILE *new_array = (DLS_FILE *) AIL_mem_alloc_lock(sizeof(DLS_FILE) * (size_t) new_size);
      if (new_array == NULL) return NULL;

      UINTa adjust = ((UINTa) new_array) - ((UINTa) old_array);

      S32 i;
      for (i=0; i < prev_size; i++)
         {
         AIL_memcpy(&new_array[i], &old_array[i], sizeof(old_array[i])); // WARNING: No operator= semantics!

         if (new_array[i].next_in_set) new_array[i].next_in_set = (DLS_FILE *) (((UINTa) new_array[i].next_in_set) + adjust);
         if (new_array[i].prev_in_set) new_array[i].prev_in_set = (DLS_FILE *) (((UINTa) new_array[i].prev_in_set) + adjust);

         if (new_array[i].next) new_array[i].next = (DLS_FILE *) (((UINTa) new_array[i].next) + adjust);
         if (new_array[i].prev) new_array[i].prev = (DLS_FILE *) (((UINTa) new_array[i].prev) + adjust);
         }

      for (i=0; i < FILE_SET_DIM; i++)    // TODO: 64-bit arith
         if (FLM->table[i]) FLM->table[i] = (DLS_FILE *) (((UINTa) FLM->table[i]) + adjust);

      for (i=prev_size; i < new_size; i++)
         {
         new_array[i].entry_num = -1;
         new_array[i].prev = &new_array[i-1];
         new_array[i].next = &new_array[i+1];
         }

      new_array[prev_size].prev = NULL;
      new_array[new_size-1].next = NULL;
      FLM->used = (DLS_FILE *) (((UINTa) FLM->used) + adjust);
      FLM->avail = &new_array[prev_size];
      FLM->current_size = new_size;

      AIL_mem_free_lock(FLM->array);
      FLM->array = new_array;
      }

   entry = FLM->avail;
   FLM->avail = entry->next;
   entry->next = FLM->used;
   entry->prev = NULL;

   if (FLM->used) FLM->used->prev = entry;
   FLM->used = entry;

   entry->set_key = key;
   entry->next_in_set = FLM->table[key];
   entry->prev_in_set = NULL;

   if (FLM->table[key])
      FLM->table[key]->prev_in_set = entry;

   FLM->table[key] = entry;
   entry->entry_num = (S32) ((((UINTa) entry) - ((UINTa) FLM->array)) / sizeof(DLS_FILE));

   DF_init(entry, seed);

   return entry;
}

//############################################################################
//#                                                                          #
//#  Init synthesizer resources                                              #
//#                                                                          #
//############################################################################

DXDEF S32 AILEXPORT DLSMSSOpen      (SINTa   *lpdwHandle, //)
                                      S32        dwFlags,
                                      HMDIDRIVER MIDI_driver,        // XMIDI driver handle
                                      S32        output_format,      // DIG_F format for output
                                      S32        output_sample_rate, // Output samples/second
                                      S32        stream_buffer_size, // Size of each buffer segment in bytes
                                      UINTa      user,               // User value passed to buffer callbacks
                                      AILDLSPCB  stream_poll_CB,     // Buffer polling handler
                                      AILDLSLCB  stream_lock_CB,     // Buffer lock handler
                                      AILDLSUCB  stream_unlock_CB)   // Buffer unlock handler
{
   return DLS_init(lpdwHandle,
                   dwFlags,
                   MIDI_driver,
                   output_format,
                   output_sample_rate,
                   stream_buffer_size,
                   user,
                   stream_poll_CB,
                   stream_lock_CB,
                   stream_unlock_CB);
}

//############################################################################
//#                                                                          #
//#  DLSClose()                                                              #
//#                                                                          #
//############################################################################

DXDEF S32 AILEXPORT DLSClose         (SINTa      dwDLSHandle,  //)
                                      S32      dwFlags)
{
   return DLS_shutdown(dwDLSHandle,
                       dwFlags);
}

//############################################################################
//#                                                                          #
//#  DLSLoadFile()                                                           #
//#                                                                          #
//############################################################################

DXDEF S32 AILEXPORT DLSLoadFile (SINTa      dwDLSHandle,  //)
                                 S32      dwFlags,
                                 SINTa *lpdwDownloadID,
                                 char const*lpFileName)
{
   dwFlags=dwFlags;

   if (dwDLSHandle != (SINTa) DLS)
      {
      return DLS_INVALID_HANDLE;
      }

   //
   // Attempt to load file
   //

   void *data = AIL_file_read(lpFileName, NULL);

   if (data == NULL)
      {
      switch (AIL_file_error())
         {
         case AIL_OUT_OF_MEMORY:  return DLS_OUT_OF_MEM;
         case AIL_FILE_NOT_FOUND: return DLS_FILE_NOT_FOUND;
         default:                 return DLS_FILE_NOT_LOADED;
         }
      }

   //
   // Attempt to parse file
   // 

   DLS_FILE*file;

   if ((((CHUNK*)data)->ckID != MSSFOURCC_RIFF) || (((CHUNK*)data)->subID != MSSFOURCC_DLS))
   {
     AIL_mem_free_lock(data);
     return(DLS_INVALID_FILE);
   }

   S32 result = DLSFILE_parse(data, &file, lpFileName, 0);

   if (result != DLS_NOERR)
      {
       AIL_mem_free_lock(data);
       return(result);
      }

   //
   // Return success
   //

   *lpdwDownloadID = file->entry_num;

   return DLS_NOERR;
}



//############################################################################
//#                                                                          #
//#  DLSLoadMemFile()                                                        #
//#                                                                          #
//############################################################################

DXDEC S32 AILEXPORT DLSLoadMemFile   (SINTa    dwDLSHandle,
                                      S32      dwFlags,
                                      SINTa *lpdwDownloadID,
                                      void const*lpMemPtr)
{
   dwFlags=dwFlags;

   if (dwDLSHandle != (SINTa) DLS)
      {
      return DLS_INVALID_HANDLE;
      }

   //
   // Attempt to parse file
   //

   DLS_FILE*file;

   if ((((CHUNK*)lpMemPtr)->ckID != MSSFOURCC_RIFF) || (((CHUNK*)lpMemPtr)->subID != MSSFOURCC_DLS))
   {
     return(DLS_INVALID_FILE);
   }

   S32 result = DLSFILE_parse(lpMemPtr, &file, "(Memory file)", 0);

   if (result != DLS_NOERR)
      {
      return result;
      }

   //
   // Return success
   //

   *lpdwDownloadID = file->entry_num;

   return DLS_NOERR;
}

//############################################################################
//#                                                                          #
//#  DLSGetInfo()                                                            #
//#                                                                          #
//############################################################################

DXDEF S32 AILEXPORT DLSGetInfo       (DLS_INFO_STRUC *lpDlsInfo)
{
   if (lpDlsInfo == NULL)
      {
      return DLS_INVALID_HANDLE;
      }

   lpDlsInfo->dwVersion = (AIL_DLS_VERSION << 8) |
                           AIL_DLS_REVISION;

#if defined(IS_PS2) || defined(__RADSEKRIT2__)
   lpDlsInfo->dwHwStatus     = DLS_NOERR;
   lpDlsInfo->dwMaxDlsMem    = INT_MAX;
   lpDlsInfo->dwLargestBuf   = INT_MAX;
   lpDlsInfo->dwGMBankStatus = 1;
   lpDlsInfo->dwGMBankSize   = INT_MAX;
#else
   lpDlsInfo->dwHwStatus     = DLS_NOERR;
   lpDlsInfo->dwMaxDlsMem    = LONG_MAX;
   lpDlsInfo->dwLargestBuf   = LONG_MAX;
   lpDlsInfo->dwGMBankStatus = 1;
   lpDlsInfo->dwGMBankSize   = LONG_MAX;
#endif

   return DLS_NOERR;
}

//############################################################################
//#                                                                          #
//#  DLSCompactMemory()                                                      #
//#                                                                          #
//############################################################################

DXDEF S32 AILEXPORT DLSCompactMemory (SINTa dwDLSHandle)
{
   dwDLSHandle=dwDLSHandle;
   return DLS_NOERR;
}

//############################################################################
//#                                                                          #
//#  DLSUnloadAll()                                                          #
//#                                                                          #
//############################################################################

DXDEF S32 AILEXPORT DLSUnloadAll     (SINTa dwDLSHandle, //)
                                      S32 dwFlags)
{
   dwFlags=dwFlags;

   if (dwDLSHandle != (SINTa) DLS)
      {
      return DLS_INVALID_HANDLE;
      }

   //
   // Unload all files in list
   //

   while (file_list->used)
      {
      DLSUnloadFile(dwDLSHandle,
                    file_list->used->entry_num);
      }

   return DLS_NOERR;
}

//############################################################################
//#                                                                          #
//#  DLSUnloadFile()                                                         #
//#                                                                          #
//############################################################################

DXDEF S32 AILEXPORT DLSUnloadFile    (SINTa dwDLSHandle,  //)
                                      SINTa dwDownloadID)
{
   if (dwDLSHandle != (SINTa) DLS)
      {
      return DLS_INVALID_HANDLE;
      }

   DLS_FILE*file = &file_list->array[dwDownloadID];

   if (file == NULL)
      {
      return DLS_INVALID_HANDLE;
      }

   //
   // Remove file entry from list and return success
   //

   FLM_dealloc(file_list, file);

   return DLS_NOERR;
}

//############################################################################
//#                                                                          #
//#  DLSSetAttribute()                                                       #
//#                                                                          #
//############################################################################

DXDEF S32 AILEXPORT DLSSetAttribute  (SINTa  dwDLSHandle, //)
                                      S32    dwAttribute,
                                      void*lpDlsParam)
{
   dwDLSHandle=dwDLSHandle;
   dwAttribute=dwAttribute;
   lpDlsParam=lpDlsParam;

   return DLS_ERR_NOT_IMPLEMENTED;
}

//############################################################################
//#                                                                          #
//#  DLSMSSGetCPU()                                                          #
//#                                                                          #
//############################################################################

DXDEF S32 AILEXPORT DLSMSSGetCPU (SINTa dwDLSHandle)
{
  if (dwDLSHandle != (SINTa) DLS)
    return 0;

  if (DLS==0)
    return(0);

  U64 time;
  U64 diff;
  U32 perc;

  time = AIL_get_time();

  diff = time - DLS->last_polled;

  if ( diff < AIL_ms_to_time( 150 ) )
    return( DLS->last_percent );

  DLS->last_polled = time;

  perc = (U32) ( ( DLS->mix_total * 100 ) / diff );
  
  DLS->mix_total = 0;
  
  if ( perc > 100 )
    perc = 100;
  
  DLS->last_percent = perc;

  return perc;
}


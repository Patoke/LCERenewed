#if defined(_SEKRIT)

#elif defined(_XENON) || ( defined(_XBOX_VER) && (_XBOX_VER == 200) ) || defined(_XBOX)
   #define NOD3D
   #include <xtl.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(__NT__) || defined(__WIN32__)
   #define WIN32_LEAN_AND_MEAN
   #define WIN32_EXTRA_LEAN
   #define STRICT
   #include <windows.h>
   #include <mmsystem.h>
#endif

#ifdef IS_PS2
#include <eekernel.h>
#endif

#ifdef IS_WII
#include <revolution/dvd.h>
#endif


#include "mss.h"
#include "imssapi.h"

#if defined(IS_WIN32)
  extern U32 __declspec(dllimport) AIL_debug;
  extern U32 __declspec(dllimport) AIL_sys_debug;
  extern U32 __declspec(dllimport) AIL_indent;
#else
  extern U32 AIL_debug;
  extern U32 AIL_sys_debug;
  extern U32 AIL_indent;
#endif


#define START  if (AIL_indent++, (AIL_debug && \
                  ((AIL_indent==1) || AIL_sys_debug) && \
                  (!AIL_background())))

#define END    AIL_indent--

#define RESULT if (AIL_debug && ((AIL_indent==1) || AIL_sys_debug) && (!AIL_background()))


#define IN_AIL  AIL_lock_mutex()
#define OUT_AIL AIL_unlock_mutex()

#define IN_AIL_NM
#define OUT_AIL_NM


static void outresuhex(UINTa val)
{
  AIL_debug_log("!-Result = " PTRFMT ,val);
}

static void outresint(U32 val)
{
  AIL_debug_log("!-Result = %d" ,val);
}

static void outresfloat(F32 val)
{
  AIL_debug_log("!-Result = %f" ,val);
}

static void outreshex(U32 val)
{
  AIL_debug_log("!-Result = 0x%X" ,val);
}

static void outresuint(UINTa val)
{
  AIL_debug_log("!-Result = " SINTFMT ,val);
}



#if defined(IS_WINDOWS)

//############################################################################

DXDEF
HREDBOOK AILEXPORT AIL_redbook_open(U32 which)
{
   HREDBOOK result;

   START
      {
      AIL_debug_log("@AIL_redbook_open(%ld)" ,which);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_open(which);

   OUT_AIL_NM;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
HREDBOOK AILEXPORT
#ifdef IS_MAC
AIL_redbook_open_volume(char const* drive)
#else
AIL_redbook_open_drive(S32 drive)
#endif
{
   HREDBOOK result;

   START
      {
      AIL_debug_log(
#ifdef IS_MAC
      "AIL_redbook_open_volume(%s:)" 
#else
      "AIL_redbook_open_drive(%c:)" 
#endif
      ,drive);
      }

   IN_AIL_NM;

#ifdef IS_MAC
   result=AIL_API_redbook_open_volume(drive);
#else
   result=AIL_API_redbook_open_drive(drive);
#endif

   OUT_AIL_NM;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void AILEXPORT AIL_redbook_close(HREDBOOK hand)
{
   START
      {
      AIL_debug_log("@AIL_redbook_close(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   AIL_API_redbook_close(hand);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
void AILEXPORT AIL_redbook_eject(HREDBOOK hand)
{
   START
      {
      AIL_debug_log("@AIL_redbook_eject(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   AIL_API_redbook_eject(hand);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
void AILEXPORT AIL_redbook_retract(HREDBOOK hand)
{
   START
      {
      AIL_debug_log("@AIL_redbook_retract(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   AIL_API_redbook_retract(hand);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_redbook_status(HREDBOOK hand)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_status(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_status(hand);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_redbook_tracks(HREDBOOK hand)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_tracks(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_tracks(hand);

   OUT_AIL_NM;

   RESULT
      {
      outresuint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_redbook_track(HREDBOOK hand)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_track(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_track(hand);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void AILEXPORT AIL_redbook_track_info(HREDBOOK hand,U32 tracknum,U32 * startmsec,U32 * endmsec)
{
   START
      {
      AIL_debug_log("@AIL_redbook_track_info(" PTRFMT ", %ld, " PTRFMT ", " PTRFMT ")" ,hand,tracknum,startmsec,endmsec);
      }

   IN_AIL_NM;

   AIL_API_redbook_track_info(hand,tracknum,startmsec,endmsec);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_redbook_id(HREDBOOK hand)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_id(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_id(hand);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_redbook_position(HREDBOOK hand)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_position(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_position(hand);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_redbook_play(HREDBOOK hand,U32 startmsec, U32 endmsec)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_play(" PTRFMT ", %ld, %ld)" ,hand,startmsec,endmsec);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_play(hand,startmsec,endmsec);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_redbook_stop(HREDBOOK hand)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_stop(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_stop(hand);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_redbook_pause(HREDBOOK hand)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_pause(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_pause(hand);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
U32 AILEXPORT AIL_redbook_resume(HREDBOOK hand)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_resume(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_resume(hand);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
F32 AILEXPORT AIL_redbook_volume_level(HREDBOOK hand)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_volume_level(" PTRFMT ")" ,hand);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_volume_level(hand);

   OUT_AIL_NM;

   RESULT
      {
      outresfloat(result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
F32 AILEXPORT AIL_redbook_set_volume_level(HREDBOOK hand, F32 volume)
{
   F32 result;

   START
      {
      AIL_debug_log("@AIL_redbook_set_volume(" PTRFMT ",%f)" ,hand,volume);
      }

   IN_AIL_NM;

   result=AIL_API_redbook_set_volume_level(hand,volume);

   OUT_AIL_NM;

   RESULT
      {
      outresfloat(result);
      }
   END;

   return result;
}

#endif


//############################################################################

DXDEF HMDIDRIVER AILEXPORT AIL_open_XMIDI_driver( U32 flags )
{
   HMDIDRIVER result;

   START
      {
      AIL_debug_log("@AIL_open_XMIDI_driver(0x%lX)" ,
                                                       flags);
      }

   IN_AIL;

   #ifdef IS_WINDOWS

   if (AIL_midiOutOpen(&result,0,(flags&AIL_OPEN_XMIDI_NULL_DRIVER)?(U32)MIDI_NULL_DRIVER:(U32)MIDI_MAPPER))
     result=0;

   #else

   #ifdef IS_DOS

   if (flags&AIL_OPEN_XMIDI_NULL_DRIVER)
   {
     result=AIL_install_MDI_driver_file("NULL.MDI",0);
   }
   else
   {
     if (AIL_install_MDI_INI(&result)!=AIL_INIT_SUCCESS)
       result=0;
   }

   #else

   result = AIL_API_open_XMIDI_driver( flags );

   #endif

   #endif

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}


DXDEF void AILEXPORT AIL_close_XMIDI_driver( HMDIDRIVER mdi )
{
   START
      {
      AIL_debug_log("@AIL_close_XMIDI_driver(" PTRFMT ")" ,mdi);
      }

   IN_AIL;

   #ifdef IS_WINDOWS
   AIL_API_midiOutClose(mdi);
   #else
   AIL_API_close_XMIDI_driver( mdi );
   #endif

   OUT_AIL;

   END;
}


#ifdef IS_WINDOWS

DXDEF
S32          AILEXPORT AIL_midiOutOpen      (HMDIDRIVER  *drvr,   //)
                                             LPHMIDIOUT  *lphMidiOut,
                                             S32         dwDeviceID)

{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_midiOutOpen(" PTRFMT ", " PTRFMT ", %d)" ,drvr,lphMidiOut,dwDeviceID);
      }

   IN_AIL;

   result=AIL_API_midiOutOpen(drvr,lphMidiOut,(U32)dwDeviceID);

   OUT_AIL;

   RESULT
      {
      outreshex((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_midiOutClose      (HMDIDRIVER mdi)
{
   START
      {
      AIL_debug_log("@AIL_midiOutClose(" PTRFMT ")" ,mdi);
      }

   IN_AIL;

   AIL_API_midiOutClose(mdi);

   OUT_AIL;

   END;
}

#endif

//############################################################################

#if defined(IS_WINDOWS)

DXDEF
S32         AILEXPORT AIL_MIDI_handle_release     (HMDIDRIVER mdi)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_MIDI_handle_release(" PTRFMT ")" ,mdi);
      }

   IN_AIL;

   result=AIL_API_MIDI_handle_release(mdi);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_MIDI_handle_reacquire     (HMDIDRIVER mdi)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_MIDI_handle_reacquire(" PTRFMT ")" ,mdi);
      }

   IN_AIL;

   result=AIL_API_MIDI_handle_reacquire(mdi);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

#endif

//############################################################################

DXDEF
HSEQUENCE    AILEXPORT AIL_allocate_sequence_handle//()
                                             (HMDIDRIVER mdi)
{
   HSEQUENCE result;

   START
      {
      AIL_debug_log("@AIL_allocate_sequence_handle(" PTRFMT ")" ,mdi);
      }

   IN_AIL;

   result=AIL_API_allocate_sequence_handle(mdi);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_release_sequence_handle//()
                                             (HSEQUENCE S)
{
   START
      {
      AIL_debug_log("@AIL_release_sequence_handle(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_release_sequence_handle(S);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_init_sequence         (HSEQUENCE S, //)
                                              void const  *start,
                                              S32       sequence_num)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_init_sequence(" PTRFMT "," PTRFMT ",%ld)" ,S,start,sequence_num);
      }

   IN_AIL;

   result=AIL_API_init_sequence(S,start,sequence_num);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_start_sequence        (HSEQUENCE S)
{
   START
      {
      AIL_debug_log("@AIL_start_sequence(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_start_sequence(S);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_stop_sequence         (HSEQUENCE S)
{
   START
      {
      AIL_debug_log("@AIL_stop_sequence(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_stop_sequence(S);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_resume_sequence       (HSEQUENCE S)
{
   START
      {
      AIL_debug_log("@AIL_resume_sequence(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_resume_sequence(S);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_end_sequence          (HSEQUENCE S)
{
   START
      {
      AIL_debug_log("@AIL_end_sequence(" PTRFMT ")" ,S);
      }

   IN_AIL;

   AIL_API_end_sequence(S);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sequence_tempo    (HSEQUENCE S, //)
                                               S32       tempo,
                                               S32       milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_set_sequence_tempo(" PTRFMT ",%ld,%ld)" ,S,tempo,
         milliseconds);
      }

   IN_AIL;

   AIL_API_set_sequence_tempo(S,tempo,milliseconds);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sequence_volume   (HSEQUENCE S, //)
                                               S32       volume,
                                               S32       milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_set_sequence_volume(" PTRFMT ",%ld,%ld)" ,S,volume,
         milliseconds);
      }

   IN_AIL_NM;

   AIL_API_set_sequence_volume(S,volume,milliseconds);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void         AILEXPORT AIL_set_sequence_loop_count (HSEQUENCE S, //)
                                                 S32       loop_count)
{
   START
      {
      AIL_debug_log("@AIL_set_sequence_loop_count(" PTRFMT ",%ld)" ,S,loop_count);
      }

   IN_AIL_NM;

   AIL_API_set_sequence_loop_count(S,loop_count);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
U32        AILEXPORT AIL_sequence_status       (HSEQUENCE S)
{
   U32 result;

   START
      {
      AIL_debug_log("@AIL_sequence_status(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sequence_status(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_sequence_tempo        (HSEQUENCE S)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sequence_tempo(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sequence_tempo(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_sequence_volume       (HSEQUENCE S)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sequence_volume(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sequence_volume(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32         AILEXPORT AIL_sequence_loop_count   (HSEQUENCE S)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_sequence_loop_count(" PTRFMT ")" ,S);
      }

   IN_AIL_NM;

   result=AIL_API_sequence_loop_count(S);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_set_XMIDI_master_volume
                                             (HMDIDRIVER mdi,
                                              S32        master_volume)
{
   START
      {
      AIL_debug_log("@AIL_set_XMIDI_master_volume(" PTRFMT ",%d)" ,mdi,master_volume);
      }

   IN_AIL;

   AIL_API_set_XMIDI_master_volume(mdi,master_volume);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_XMIDI_master_volume   (HMDIDRIVER mdi)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_XMIDI_master_volume(" PTRFMT ")" ,mdi);
      }

   IN_AIL;

   result=AIL_API_XMIDI_master_volume(mdi);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_active_sequence_count     (HMDIDRIVER mdi)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_active_sequence_count(" PTRFMT ")" ,mdi);
      }

   IN_AIL;

   result=AIL_API_active_sequence_count(mdi);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_controller_value          (HSEQUENCE S, //)
                                              S32       channel,
                                              S32       controller_num)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_controller_value(" PTRFMT ",%ld,%ld)" ,S,channel,
         controller_num);
      }

   IN_AIL_NM;

   result=AIL_API_controller_value(S,channel,controller_num);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_channel_notes             (HSEQUENCE S, //)
                                              S32       channel)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_channel_notes(" PTRFMT ",%ld)" ,S,channel);
      }

   IN_AIL;

   result=AIL_API_channel_notes(S,channel);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_sequence_position         (HSEQUENCE S, //)
                                                  S32        *beat,
                                                  S32        *measure)
{
   START
      {
      AIL_debug_log("@AIL_sequence_position(" PTRFMT "," PTRFMT "," PTRFMT ")" ,S,beat,measure);
      }

   IN_AIL_NM;

   AIL_API_sequence_position(S,beat,measure);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_branch_index              (HSEQUENCE S, //)
                                               U32       marker)
{
   START
      {
      AIL_debug_log("@AIL_branch_index(" PTRFMT ",%lu)" ,S,marker);
      }

   IN_AIL;

   AIL_API_branch_index(S,marker);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
AILPREFIXCB AILEXPORT AIL_register_prefix_callback  (HSEQUENCE S, //)
                                               AILPREFIXCB   callback)
{
   AILPREFIXCB result;

   START
      {
      AIL_debug_log("@AIL_register_prefix_callback(" PTRFMT "," PTRFMT ")" ,
         S,callback);
      }

   IN_AIL;

   result=AIL_API_register_prefix_callback(S,callback);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILTRIGGERCB AILEXPORT AIL_register_trigger_callback (HSEQUENCE S, //)
                                              AILTRIGGERCB   callback)
{
   AILTRIGGERCB result;

   START
      {
      AIL_debug_log("@AIL_register_trigger_callback(" PTRFMT "," PTRFMT ")" ,
         S,callback);
      }

   IN_AIL;

   result=AIL_API_register_trigger_callback(S,callback);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILSEQUENCECB AILEXPORT AIL_register_sequence_callback(HSEQUENCE S, //)
                                              AILSEQUENCECB   callback)
{
   AILSEQUENCECB result;

   START
      {
      AIL_debug_log("@AIL_register_sequence_callback(" PTRFMT "," PTRFMT ")" ,
         S,callback);
      }

   IN_AIL;

   result=AIL_API_register_sequence_callback(S,callback);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILBEATCB AILEXPORT AIL_register_beat_callback(HSEQUENCE S, //)
                                              AILBEATCB   callback)
{
   AILBEATCB result;

   START
      {
      AIL_debug_log("@AIL_register_beat_callback(" PTRFMT "," PTRFMT ")" ,
         S,callback);
      }

   IN_AIL;

   result=AIL_API_register_beat_callback(S,callback);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILEVENTCB AILEXPORT AIL_register_event_callback   (HMDIDRIVER mdi, //)
                                                    AILEVENTCB callback)
{
   AILEVENTCB result;

   START
      {
      AIL_debug_log("@AIL_register_event_callback(" PTRFMT "," PTRFMT ")" ,
         mdi,callback);
      }

   IN_AIL;

   result=AIL_API_register_event_callback(mdi,callback);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
AILTIMBRECB AILEXPORT AIL_register_timbre_callback  (HMDIDRIVER mdi, //)
                                                     AILTIMBRECB callback)
{
   AILTIMBRECB result;

   START
      {
      AIL_debug_log("@AIL_register_timbre_callback(" PTRFMT "," PTRFMT ")" ,
         mdi, callback);
      }

   IN_AIL;

   result=AIL_API_register_timbre_callback(mdi,callback);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_set_sequence_user_data    (HSEQUENCE S, //)
                                                  U32       index,
                                                  SINTa     value)
{
   START
      {
      AIL_debug_log("@AIL_set_sequence_user_data(" PTRFMT ",%ld," SINTFMT ")" ,S,index,value);
      }

   IN_AIL_NM;

   AIL_API_set_sequence_user_data(S,index,value);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
SINTa   AILEXPORT AIL_sequence_user_data        (HSEQUENCE S, //)
                                                 U32       index)
{
   SINTa result;

   START
      {
      AIL_debug_log("@AIL_sequence_user_data(" PTRFMT ",%lu)" ,S,index);
      }

   IN_AIL_NM;

   result=AIL_API_sequence_user_data(S,index);

   OUT_AIL_NM;

   RESULT
      {
      outresuint((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_register_ICA_array        (HSEQUENCE   S,  //)
                                                  U8      *array)
{
   START
      {
      AIL_debug_log("@AIL_register_ICA_array(" PTRFMT "," PTRFMT ")" ,S,array);
      }

   IN_AIL;

   AIL_API_register_ICA_array(S,array);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_lock_channel              (HMDIDRIVER mdi)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_lock_channel(" PTRFMT ")" ,mdi);
      }

   IN_AIL;

   result=AIL_API_lock_channel(mdi);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_release_channel           (HMDIDRIVER mdi, //)
                                               S32        channel)
{
   START
      {
      AIL_debug_log("@AIL_release_channel(" PTRFMT ",%ld)" ,mdi,channel);
      }

   IN_AIL;

   AIL_API_release_channel(mdi,channel);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_map_sequence_channel      (HSEQUENCE S, //)
                                               S32       seq_channel,
                                               S32       new_channel)
{
   START
      {
      AIL_debug_log("@AIL_map_sequence_channel(" PTRFMT ",%ld,%ld)" ,
         S,seq_channel,new_channel);
      }

   IN_AIL;

   AIL_API_map_sequence_channel(S,seq_channel,new_channel);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
S32     AILEXPORT AIL_true_sequence_channel     (HSEQUENCE S, //)
                                              S32       seq_channel)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_true_sequence_channel(" PTRFMT ",%ld)" ,
         S,seq_channel);
      }

   IN_AIL;

   result=AIL_API_true_sequence_channel(S,seq_channel);

   OUT_AIL;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_send_channel_voice_message(HMDIDRIVER mdi, //)
                                               HSEQUENCE  S,
                                               S32        status,
                                               S32        data_1,
                                               S32        data_2)
{
   START
      {
      AIL_debug_log("@AIL_send_channel_voice_message(" PTRFMT "," PTRFMT ",0x%lX,0x%lX,0x%lX)" ,
         mdi,S,status,data_1,data_2);
      }

   IN_AIL;

   AIL_API_send_channel_voice_message(mdi,S,status,data_1,data_2);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_send_sysex_message        (HMDIDRIVER mdi, //)
                                               void const  *buffer)
{
   START
      {
      AIL_debug_log("@AIL_send_sysex_message(" PTRFMT "," PTRFMT ")" ,mdi,buffer);
      }

   IN_AIL;

   AIL_API_send_sysex_message(mdi,buffer);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
HWAVESYNTH    AILEXPORT AIL_create_wave_synthesizer   (HDIGDRIVER dig,  //)
                                                    HMDIDRIVER mdi,
                                                    void const  *wave_lib,
                                                    S32        polyphony)
{
   HWAVESYNTH result;

   START
      {
      AIL_debug_log("@AIL_create_wave_synthesizer(" PTRFMT "," PTRFMT "," PTRFMT ",%ld)" ,
         dig,mdi,wave_lib,polyphony);
      }

   IN_AIL;

   result=AIL_API_create_wave_synthesizer(dig,mdi,wave_lib,polyphony);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
void     AILEXPORT AIL_destroy_wave_synthesizer  (HWAVESYNTH W)
{
   START
      {
      AIL_debug_log("@AIL_destroy_wave_synthesizer(" PTRFMT ")" ,W);
      }

   IN_AIL;

   AIL_API_destroy_wave_synthesizer(W);

   OUT_AIL;

   END;
}

//############################################################################

#if defined(IS_WINDOWS) || defined(IS_MAC) || defined(IS_LINUX) || defined(IS_XBOX) || defined(IS_XENON) || defined(IS_PS2) || defined(IS_PS3) || defined(IS_WII) || defined(IS_PSP)

DXDEF
S32   AILEXPORT AIL_quick_startup             (S32         use_digital,
                                               S32         use_MIDI,
                                               U32         output_rate,
                                               S32         output_bits,
                                               S32         output_channels)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_quick_startup(%ld, %ld, %ld, %ld, %ld )" ,use_digital,use_MIDI,output_rate,output_bits,output_channels);
      }

   IN_AIL_NM;

   result=AIL_API_quick_startup(use_digital,use_MIDI,output_rate,output_bits,output_channels);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}

#endif

//############################################################################

DXDEF
void   AILEXPORT AIL_quick_shutdown            (void)
{
   START
      {
      AIL_debug_log("@AIL_quick_shutdown()" );
      }

   IN_AIL_NM;

   AIL_API_quick_shutdown();

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
void   AILEXPORT AIL_quick_handles ( HDIGDRIVER * pdig, HMDIDRIVER * pmdi, HDLSDEVICE * pdls )
{
   START
      {
      AIL_debug_log("@AIL_quick_handles( " PTRFMT ", " PTRFMT " , " PTRFMT " )" ,pdig,pmdi,pdls);
      }

   IN_AIL_NM;

   AIL_API_quick_handles(pdig,pmdi,pdls);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
HAUDIO  AILEXPORT AIL_quick_load                (char const    *filename)
{
   HAUDIO result;

   START
      {
      AIL_debug_log("@AIL_quick_load(%s)" ,filename);
      }

   IN_AIL_NM;

   result=AIL_API_quick_load(filename);

   OUT_AIL_NM;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
HAUDIO  AILEXPORT AIL_quick_load_mem            (void const    *mem,
                                                 U32    size)
{
   HAUDIO result;

   START
      {
      AIL_debug_log("@AIL_quick_load_mem(" PTRFMT ",%lu)" ,mem,size);
      }

   IN_AIL_NM;

   result=AIL_API_quick_load_mem(mem,size);

   OUT_AIL_NM;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
HAUDIO  AILEXPORT AIL_quick_load_named_mem      (void const    *mem,
                                                 char const    *filename,
                                                 U32    size)
{
   HAUDIO result;

   START
      {
      AIL_debug_log("@AIL_quick_load_mem(" PTRFMT ",%s%lu)" ,mem,filename,size);
      }

   IN_AIL_NM;

   result=AIL_API_quick_load_named_mem(mem,filename,size);

   OUT_AIL_NM;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
HAUDIO  AILEXPORT AIL_quick_copy                (HAUDIO audio)
{
   HAUDIO result;

   START
      {
      AIL_debug_log("@AIL_quick_copy(" PTRFMT ")" ,audio);
      }

   IN_AIL_NM;

   result=AIL_API_quick_copy(audio);

   OUT_AIL_NM;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void    AILEXPORT AIL_quick_unload              (HAUDIO      audio)
{
   START
      {
      AIL_debug_log("@AIL_quick_unload(" PTRFMT ")" ,audio);
      }

   IN_AIL_NM;

   AIL_API_quick_unload(audio);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
S32    AILEXPORT AIL_quick_play                (HAUDIO      audio,
                                                U32         loop_count)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_quick_play(" PTRFMT ", %ld)" ,audio,loop_count);
      }

   IN_AIL_NM;

   result=AIL_API_quick_play(audio,loop_count);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void   AILEXPORT AIL_quick_halt                (HAUDIO      audio)
{
   START
      {
      AIL_debug_log("@AIL_quick_halt(" PTRFMT ")" ,audio);
      }

   IN_AIL_NM;

   AIL_API_quick_halt(audio);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
S32     AILEXPORT AIL_quick_status              (HAUDIO      audio)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_quick_status(" PTRFMT ")" ,audio);
      }

   IN_AIL_NM;

   result=AIL_API_quick_status(audio);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
HAUDIO  AILEXPORT AIL_quick_load_and_play       (char const   *filename,
                                                 U32         loop_count,
                                                 S32         wait_request)
{
   HAUDIO result;

   START
      {
      AIL_debug_log("@AIL_quick_load_and_play(%s, %ld, %ld)" ,filename,loop_count,wait_request);
      }

   IN_AIL_NM;

   result=AIL_API_quick_load_and_play(filename,loop_count,wait_request);

   OUT_AIL_NM;

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}


//############################################################################

DXDEF
void   AILEXPORT AIL_quick_set_speed     (HAUDIO      audio, S32 speed)
{
   START
      {
      AIL_debug_log("@AIL_quick_set_speed(" PTRFMT ", %ld)" ,audio,speed);
      }

   IN_AIL_NM;

   AIL_API_quick_set_speed(audio,speed);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
void   AILEXPORT AIL_quick_set_volume (HAUDIO audio, F32 volume, F32 extravol)
{
   START
      {
      AIL_debug_log("@AIL_quick_set_volume(" PTRFMT ", %f, %f)" ,audio,volume,extravol);
      }

   IN_AIL_NM;

   AIL_API_quick_set_volume(audio,volume,extravol);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
void   AILEXPORT AIL_quick_set_reverb_levels (HAUDIO audio,
                                              F32     dry_level,
                                              F32     wet_level)
{
   START
      {
      AIL_debug_log("@AIL_quick_set_reverb_levels(" PTRFMT ", %f, %f)" ,audio,dry_level,wet_level);
      }

   IN_AIL_NM;

   AIL_API_quick_set_reverb_levels(audio,dry_level,wet_level);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
void   AILEXPORT AIL_quick_set_low_pass_cut_off(HAUDIO audio,
                                                S32    input_channel,
                                                F32    cut_off)
{
   START
      {
      AIL_debug_log("@AIL_quick_set_low_pass_cut_off(" PTRFMT ", %d, %f)" ,audio,input_channel,cut_off);
      }

   IN_AIL_NM;

   AIL_API_quick_set_low_pass_cut_off(audio,input_channel,cut_off);

   OUT_AIL_NM;

   END;
}


//############################################################################

DXDEF
HDLSDEVICE AILEXPORT AIL_DLS_open(HMDIDRIVER mdi, HDIGDRIVER dig,
#if defined(IS_WINDOWS) || (defined(IS_MAC) && !defined(IS_IPHONE)) || defined(IS_LINUX)
                                  char const * dls,
#else
                                  AILSTATICDLS const * dls,
#endif
                                  U32 flags, U32 rate, S32 bits, S32 channels)
{
   HDLSDEVICE result;

   START
      {
#if defined(IS_WINDOWS) ||  (defined(IS_MAC) && !defined(IS_IPHONE)) || defined(IS_LINUX)
      AIL_debug_log("@AIL_DLS_open(" PTRFMT "," PTRFMT ",%s,%lX,%lu,%lu,%lu)" ,mdi,dig,dls,flags,rate,bits,channels);
#else
      AIL_debug_log("@AIL_DLS_open(" PTRFMT "," PTRFMT "," PTRFMT ",%lX,%lu,%lu,%lu)" ,mdi,dig,dls,flags,rate,bits,channels);
#endif
      }

   IN_AIL;

   result=AIL_API_DLS_open(mdi,dig,dls,flags,rate,bits,channels);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
void   AILEXPORT  AIL_DLS_close(HDLSDEVICE dls, U32 flags)
{
   START
      {
      AIL_debug_log("@AIL_DLS_close(" PTRFMT ",%ld)" ,dls,flags);
      }

   IN_AIL;

   AIL_API_DLS_close(dls,flags);

   OUT_AIL;

   END;
}


//############################################################################

DXDEF
HDLSFILEID AILEXPORT AIL_DLS_load_file(HDLSDEVICE dls, char const * filename, U32 flags)
{
   HDLSFILEID result;

   START
      {
      AIL_debug_log("@AIL_DLS_load_file(" PTRFMT ",%s,%lu)" ,dls,filename,flags);
      }

   IN_AIL;

   result=AIL_API_DLS_load_file(dls,filename,flags);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
HDLSFILEID AILEXPORT AIL_DLS_load_memory(HDLSDEVICE dls, void const * memfile, U32 flags)
{
   HDLSFILEID result;

   START
      {
      AIL_debug_log("@AIL_DLS_load_memory(" PTRFMT "," PTRFMT ",%lu)" ,dls,memfile,flags);
      }

   IN_AIL;

   result=AIL_API_DLS_load_memory(dls,memfile,flags);

   OUT_AIL;

   RESULT
      {
      outresuhex((UINTa)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
void   AILEXPORT AIL_DLS_unload(HDLSDEVICE dls, HDLSFILEID dlsid)
{
   START
      {
      AIL_debug_log("@AIL_DLS_unload(" PTRFMT "," PTRFMT ")" ,dls,dlsid);
      }

   IN_AIL;

   AIL_API_DLS_unload(dls,dlsid);

   OUT_AIL;

   END;
}


//############################################################################

DXDEF
void   AILEXPORT AIL_DLS_compact(HDLSDEVICE dls)
{
   START
      {
      AIL_debug_log("@AIL_DLS_compact(" PTRFMT ")" ,dls);
      }

   IN_AIL;

   AIL_API_DLS_compact(dls);

   OUT_AIL;

   END;
}


//############################################################################

DXDEF
HSAMPLE AILEXPORT AIL_DLS_sample_handle(HDLSDEVICE dls)
{
   HSAMPLE result = 0;

   START
      {
      AIL_debug_log("@AIL_DLS_sample_handle(" PTRFMT ")" ,dls);
      }

  if (dls) {
    // are we using the soft-synthesizer
#if !defined(IS_MAC) && !defined(IS_LINUX)
#ifdef IS_WINDOWS
    if (dls->lib==0)
#else
    if(dls->DOSname==0)
#endif
#endif
    {
      result = dls->sample;
    }
  }

   RESULT
      {
      outresuhex((UINTa)result);
      }
   END;

   return result;
}

//############################################################################

DXDEF
void   AILEXPORT AIL_DLS_get_info(HDLSDEVICE dls, AILDLSINFO * info, S32 * PercentCPU)
{
   START
      {
      AIL_debug_log("@AIL_DLS_get_info(" PTRFMT "," PTRFMT "," PTRFMT ")" ,dls,info,PercentCPU);
      }

   IN_AIL_NM;

   AIL_API_DLS_get_info(dls,info,PercentCPU);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void AILEXPORT    AIL_set_sequence_ms_position  (HSEQUENCE S, //)
                                                 S32       milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_set_sequence_ms_position(" PTRFMT ",%lu)" ,S,milliseconds);
      }

   IN_AIL;

   AIL_API_set_sequence_ms_position(S,milliseconds);

   OUT_AIL;

   END;
}

//############################################################################

DXDEF
void AILEXPORT AIL_sequence_ms_position(HSEQUENCE S, //)
                                  S32     *total_milliseconds,
                                  S32     *current_milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_sequence_ms_position(" PTRFMT "," PTRFMT "," PTRFMT ")" ,S,total_milliseconds,current_milliseconds);
      }

   IN_AIL_NM;

   AIL_API_sequence_ms_position(S,total_milliseconds,current_milliseconds);

   OUT_AIL_NM;

   END;
}

//############################################################################

DXDEF
void AILEXPORT AIL_quick_set_ms_position(HAUDIO audio,S32 milliseconds)
{
   START
      {
      AIL_debug_log("@AIL_quick_set_ms_position(" PTRFMT ",%lu)" ,audio,milliseconds);
      }

   IN_AIL;

   AIL_API_quick_set_ms_position(audio,milliseconds);

   OUT_AIL;

   END;
}


//############################################################################

DXDEF
S32 AILEXPORT AIL_quick_ms_position(HAUDIO audio)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_quick_ms_position(" PTRFMT ")" ,audio);
      }

   IN_AIL_NM;

   result=AIL_API_quick_ms_position(audio);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}

//############################################################################

DXDEF
S32 AILEXPORT AIL_quick_ms_length(HAUDIO audio)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_quick_ms_length(" PTRFMT ")" ,audio);
      }

   IN_AIL_NM;

   result=AIL_API_quick_ms_length(audio);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}


//############################################################################

DXDEF
S32 AILEXPORT AIL_quick_type(HAUDIO audio)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_quick_type(" PTRFMT ")" ,audio);
      }

   IN_AIL_NM;

   result=AIL_API_quick_type(audio);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;
}


DXDEF
S32  AILEXPORT      AIL_MIDI_to_XMI       (void const * MIDI,
                                           U32       MIDI_size,
                                           void * *XMIDI,
                                           U32  * XMIDI_size,
                                           S32       flags)

{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_MIDI_to_XMI(" PTRFMT ",%lu," PTRFMT "," PTRFMT ",0x%lX)" ,
         MIDI, MIDI_size,XMIDI,XMIDI_size,flags);
      }

   IN_AIL_NM;

   result=AIL_API_MIDI_to_XMI(MIDI, MIDI_size,XMIDI,XMIDI_size,flags);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

  return( result);

}


DXDEF
S32 AILEXPORT AIL_find_DLS    (void const * data, U32 size,
                               void * *xmi, U32 * xmisize,
                               void * *dls, U32 * dlssize)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_find_DLS_in_XMI(" PTRFMT ",%lu," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
         data,size,xmi,xmisize,dls,dlssize);
      }

   IN_AIL_NM;

   result=AIL_API_find_DLS(data,size,xmi,xmisize,dls,dlssize);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

  return( result);

}

//############################################################################

DXDEF
S32 AILEXPORT AIL_extract_DLS      (void const        *source_image, //)
                                     U32             source_size,
                                     void  *  *XMI_output_data,
                                     U32         *XMI_output_size,
                                     void  *  *DLS_output_data,
                                     U32       *DLS_output_size,
                                     AILLENGTHYCB    callback)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_DLS_extract_image(" PTRFMT ",0x%lu," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT "," PTRFMT ")" ,
         source_image,source_size,XMI_output_data,XMI_output_size,DLS_output_data,DLS_output_size,callback);
      }

   IN_AIL_NM;

   result=AIL_API_extract_DLS(source_image,source_size,XMI_output_data,XMI_output_size,DLS_output_data,DLS_output_size,callback);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

   return result;

}

DXDEF
S32 AILEXPORT AIL_compress_ADPCM(AILSOUNDINFO const * info,
                              void * * outdata, U32 * outsize)
{
   S32 result;

   START
      {
      AIL_debug_log("@AIL_compress_ADPCM(" PTRFMT "," PTRFMT "," PTRFMT ")" ,
         info,outdata,outsize);
      }

   IN_AIL_NM;

   result=AIL_API_compress_ADPCM(info,outdata,outsize);

   OUT_AIL_NM;

   RESULT
      {
      outresint((U32)result);
      }

   END;

  return( result);

}


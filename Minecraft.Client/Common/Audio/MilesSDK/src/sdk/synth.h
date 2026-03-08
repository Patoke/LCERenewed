//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  SYNTH.H                                                               ##
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

#ifndef SYNTH_H
#define SYNTH_H

#include <stdio.h>
#include <string.h>

#define N_BUILD_BUFFERS      9       // 9 build buffers in chain (0-8)
#define BUILD_LOW_WATER      2       // Synth is falling behind when ring margin falls this low
#define BUILD_HIGH_WATER     6       // DAC is falling behind when ring margin rises this high
#define BUILD_CENTER         4       // Midway point in ring buffer

#define INTERVAL_BUFFER_SIZE 65536   // Interval buffer is >= 200 ms long
#define MQ_SIZE              4096    // 4K message slots in MIDI queue

#ifndef F32
#define F32 float
#endif

#define MSSFOURCC_MLS  MSSmmioFOURCC('M','L','S',' ')
#define MSSFOURCC_FMT  MSSmmioFOURCC('f','m','t',' ')
#define MSSFOURCC_PCM  MSSmmioFOURCC('p','c','m',' ')
#define MSSFOURCC_JUNK MSSmmioFOURCC('j','u','n','k')
#define MSSFOURCC_DATA MSSmmioFOURCC('d','a','t','a')
#define MSSFOURCC_FACT MSSmmioFOURCC('f','a','c','t')
#define MSSFOURCC_INFO MSSmmioFOURCC('I','N','F','O')
#define MSSFOURCC_INAM MSSmmioFOURCC('I','N','A','M')

#define WAVE_FORMAT_PCM         1
#define WAVE_FORMAT_IMA_ADPCM   0x0011

//
// DLL instance handle
//

//
// Single global instance of synthesizer device
//

extern struct SYNTHDEVICE *DLS;


//
// Misc. definitions
//

MSS_STRUCT CHUNK
{
   MSSFOURCC ckID;
   S32    ckSize;
   MSSFOURCC subID;
   U8     data[4];
};

struct SUBCHUNK
{
   MSSFOURCC ckID;
   S32    ckSize;
   U8     data[4];
};

//
// .WAV FACT chunk
//

typedef struct
{
   S8  FACT_string[4];
   U32 chunk_size;
   U32 samples;
}
IMA_FACT;

//
// .WAV file data chunk
//

typedef struct
{
   S8  DATA_string[4];
   U32 chunk_size;
   S8  data[4];
}
IMA_DATA;


//
// .WAV file data chunk
//

typedef struct
{
   S8  DATA_string[4];
   U32 chunk_size;
   U8  data[4];
}
WAV_DATA;

//
// MIDI channel status descriptor
//

struct MIDI_CHANNEL
{
   S32 bank_LSB;
   S32 bank_MSB;
   S32 patch;
   S32 instrument_index;

   S8     control[128];
   S32    coarse_tuning;
   F32 fine_tuning;
   F32 pitch_bend_range;
   F32 pitch;
};

struct BUILD_BUFFER
{
   S32 *block;
   S32      status;
};

#if !defined(IS_PS2) && !defined(IS_PS3) && !defined(IS_PSP)
#pragma pack(1)
#endif

//
// .WAV PCM file format chunk
//

typedef MSS_STRUCT
{
   S8   FMT_string[4];
   U32  chunk_size;

   S16  format_tag;
   S16  channels;
   S32  sample_rate;
   S32  average_data_rate;
   S16  alignment;
   S16  bits_per_sample;
}
WAV_FMT;

//
// .WAV IMA ADPCM file format chunk
//

typedef MSS_STRUCT
{
   S8  FMT_string[4];
   U32 chunk_size;

   S16 format_tag;
   S16 channels;
   S32 sample_rate;
   S32 average_data_rate;
   S16 alignment;
   S16 bits_per_sample;
   S16 extra;
   S16 samples_per_block;
}
IMA_FMT;

//
// ASI MPEG Audio file format chunk
//
// This is a proprietary WAV extension, used to wrap DLS sample files
// created by ASI encoders.  (ASI encoders always output raw data, which
// needs a RIFF header in order to be navigable by DLS readers.)
//
// The IMA_FACT and IMA_DATA chunks are reused by the ASI format to
// store the total sample count and data stream, respectively.
//

typedef MSS_STRUCT
{
   S8 FMT_string[4];
   U32 chunk_size;

   S16 format_tag;               // WAVE_FORMAT_UNKNOWN
   S16 channels;                 // # of channels
   S32 sample_rate;              // Sample rate in samples/second
   S32 average_data_rate;        // Stream rate in bytes per second
   S16 alignment;                // Always 1 (actual alignment constraints are determined by ASI decoder)
   S16 bits_per_sample;          // Bits/sample value from encoder output
   S16 extra;                    // Always 4

   C8 original_file_suffix[4];   // Example: "MP3" with zero terminator
   U16 pad;
}
ASI_FMT;

//
// Articulation data
//

MSS_STRUCT CONN
{
   C8 const *name;
   U16 val;
   U16 pad;
};

extern const CONN src[12];

extern const CONN dest[18];

MSS_STRUCT CID
{
   U16 usSource;
   U16 usControl;
   U16 usDestination;
   U16 usTransform;
};

#define CONN_SRC_RPN0 -1
#define CONN_SRC_RPN1 -1
#define CONN_SRC_RPN2 -1

extern const CID CID_list[29];

enum ARTNAME
{
   LFO_FREQUENCY = 0,
   LFO_START_DELAY,
   LFO_ATTENUATION_SCALE,
   LFO_PITCH_SCALE,
   LFO_MODW_TO_ATTENUATION,
   LFO_MODW_TO_PITCH,
   VOL_EG_ATTACK_TIME,
   VOL_EG_DECAY_TIME,
   VOL_EG_SUSTAIN_LEVEL,
   VOL_EG_RELEASE_TIME,
   VOL_EG_VELOCITY_TO_ATTACK,
   VOL_EG_KEY_TO_DECAY,
   PITCH_EG_ATTACK_TIME,
   PITCH_EG_DECAY_TIME,
   PITCH_EG_SUSTAIN_LEVEL,
   PITCH_EG_RELEASE_TIME,
   PITCH_EG_VELOCITY_TO_ATTACK,
   PITCH_EG_KEY_TO_DECAY,
   DEFAULT_PAN,
   EG1_TO_ATTENUATION,
   EG2_TO_PITCH,
   KEY_ON_VELOCITY_TO_ATTENUATION,
   PITCH_WHEEL_TO_PITCH,
   KEY_NUMBER_TO_PITCH,
   MIDI_CONTROLLER_7_TO_ATTENUATION,
   MIDI_CONTROLLER_10_TO_PAN,
   MIDI_CONTROLLER_11_TO_ATTENUATION,
   RPN1_TO_PITCH,
   RPN2_TO_PITCH
};

MSS_STRUCT ARTICULATION
{
   S32 lScale[29];
};

#define IMPLICIT -1                      // This connection is fixed and cannot be overridden
#define ABSOLUTE_ZERO ((S32) 0x80000000) // 32-bit absolute-zero value

#define F_PI  3.141592654F
#define F_2PI 6.283185308F

//
// Default ARTICULATION members used where no articulator values are
// explicitly defined in region
//

extern const ARTICULATION ARTDEFAULT;

//
// Default WSMPL -- used if neither WAVE nor RGN contains a valid WSMP chunk
//

extern const WSMPL WSMPLDEFAULT;

//
// Build buffer descriptor: status = FULL, EMPTY, or index of next chunk
// to be filled
//

#define BUILD_FULL  (-1)
#define BUILD_EMPTY (0)

MSS_STRUCT MIDI_MESSAGE
{
   U8  status;
   U8  channel;
   U8  data_1;
   U8  data_2;
   S32 instrument_index;
};

#if !defined(IS_PS2) && !defined(IS_PS3) && !defined(IS_PSP)
#pragma pack()
#endif

//
// DLS voice descriptor
//

#define ATTACK_PHASE  0
#define DECAY_PHASE   1
#define SUSTAIN_PHASE 2
#define RELEASE_PHASE 3

struct REGION
{
   RGNHEADER           header;
   WSMPL               sample;
   WLOOP               loop;
   WAVELINK            wave;
   ARTICULATION   *connection;
   CONNECTIONLIST *art1;        // Original ART1 chunk data, for diagnostics
};

struct WAVEDESC
{
   S32       valid;

   S32       format;
   F32    rate;
   void *data;
   S32       len;

   S32       WSMP_valid;
   WSMPL     sample;
   WLOOP     loop;
};

struct VOICE
{
   S32                  active;
   S32                  key;  
   S32                  release_request;

   struct WAVEDESC *wave;

   MIDI_MESSAGE         trigger;
   struct REGION   *region;

   S32                  mixer_operation;

   S32                  play_cursor;
   U32                  src_fract;
   S32                  left_val;
   S32                  right_val;

   S32                  loop_start;
   S32                  loop_end;
   S32                  loop_size;

   F32               static_pitch;
   F32               static_atten;

   S32                  BPS;

   S32                  LFO_holdoff;
   F32               LFO_radians_per_interval;
   F32               LFO_phase_accumulator;
   F32               LFO_atten_scale;
   F32               LFO_pitch_scale;
   F32               LFO_CC1_to_atten;
   F32               LFO_CC1_to_pitch;

   S32                  EG1_active;
   S32                  EG1_phase;
   F32               EG1_scale;
   F32               EG1_atten;
   F32               EG1_attack_dB_per_interval;
   F32               EG1_lin_scale;
   S32                  EG1_attack_intervals;
   S32                  EG1_attack_time;
   F32               EG1_decay_dB_per_interval;
   F32               EG1_sustain_atten;
   F32               EG1_release_dB_per_interval;
   S32                  EG1_release_intervals;

   S32                  EG2_active;
   S32                  EG2_phase;
   F32               EG2_scale;
   F32               EG2_pitch;
   F32               EG2_attack_per_interval;
   S32                  EG2_attack_intervals;
   F32               EG2_decay_per_interval;
   F32               EG2_sustain_pitch;
   F32               EG2_release_per_interval;
   S32                  EG2_release_intervals;

   S32                  default_pan;
};

//
// DLS synthesizer type definitions
//

struct SYNTHDEVICE
{
   U64           mix_total;
   U64           last_polled;
   U64           last_interval_count;

   MIXER_FLUSH   mixer_flush;
   MIXER_MERGE   mixer_merge;
   MIXER_COPY    mixer_copy;

   HMDIDRIVER    mdi;

   S32 * *reverb_buffers;
   S32          *interval_buffer;
   VOICE        *voice_list;

   U32           output_format;
   F32           output_sample_rate;
   S32           channels_per_sample;
   S32           bytes_per_channel;
   S32           bytes_per_sample;

   S32           stream_buffer_size;
   S32           stream_buff;
   UINTa         user;
   AILDLSPCB     stream_poll_CB;
   AILDLSLCB     stream_lock_CB;
   AILDLSUCB     stream_unlock_CB;

   F32           service_rate_uS;
   U32           last_total;
   HTIMER        timer_handle;

   S32           bytes_to_write;

   BUILD_BUFFER  build[N_BUILD_BUFFERS];
   S32           bytes_per_buffer;

               
   S32            n_reverb_buffers;

   S32               build_queue_head;
   S32               build_queue_tail;
   S32               buffers_filled;

   AILEVENTCB    prev_event_fn;
   AILTIMBRECB   prev_timb_fn; 

   MIDI_CHANNEL  channel[16];

   MIDI_MESSAGE  MIDI_queue[MQ_SIZE];
   S32           MIDI_queue_head;
   S32           MIDI_queue_tail;

   S32           n_voices;

   U32           use_MMX;

   S32           enabled;

   S32           time_bias;
   U32           last_percent;

};

struct INSTRUMENT
{
   INSTRUMENT *next;
   INSTRUMENT *prev;
   INSTRUMENT *next_in_set;
   INSTRUMENT *prev_in_set;
   S32         set_key;
   S32         entry_num;

   S32                 DLS_file;    // DLS file containing instrument

   INSTHEADER          header;      // Bank/patch#/region count

   ARTICULATION   *art_list;    // Articulation set(s)
   S32                 art_cnt;     // # of articulation set(s)

   REGION         *region_list; // Instrument definitions by keyboard region
};

// ---------------------------------------------------------------------------
// Definitions/prototypes
// ---------------------------------------------------------------------------

#define ARYSIZE(x) ((S32) (sizeof((x)) / sizeof((x)[0])))

//
// Instrument list manager class
//

#define INS_SET_DIM    1
#define INS_BLOCK_SIZE 128

struct InsListMgr
{
public:
   S32 current_size;
   INSTRUMENT*used;
   INSTRUMENT*avail;
   INSTRUMENT*table[INS_SET_DIM];
   INSTRUMENT*array;
};

extern InsListMgr *instrument_list;

extern void INS_init(INSTRUMENT *INS, void const *data);
extern void INS_cleanup(INSTRUMENT *INS);
extern S32 INS_is_match(INSTRUMENT *INS, void *data);
extern U32 INS_set(void const *data);

extern void ILM_construct(InsListMgr *ILM);
extern void ILM_dealloc(InsListMgr *ILM, INSTRUMENT *entry);
extern void ILM_dealloc(InsListMgr *ILM, S32 entry_num);
extern void ILM_cleanup(InsListMgr *ILM);
extern void ILM_destroy(InsListMgr *ILM);
extern INSTRUMENT *ILM_lookup(InsListMgr *ILM, void *seed);
extern INSTRUMENT *ILM_alloc(InsListMgr *ILM, void const *seed=NULL);

struct DLS_FILE
{
   DLS_FILE *next;
   DLS_FILE *prev;
   DLS_FILE *next_in_set;
   DLS_FILE *prev_in_set;
   S32       set_key;
   S32       entry_num;

   //
   // File data
   //

   C8        filename[256];
   void const *image;

   //
   // COLH data
   //

   S32 cInstruments;

   //
   // PTBL data
   //

   POOLTABLE *ptbl;
   POOLCUE   *cues;

   //
   // WAVE data
   //

   WAVEDESC *WAVE_list;
};

extern void DF_init(DLS_FILE *DF, void const *data);
extern void DF_cleanup(DLS_FILE *DF);
extern S32 DF_is_match(DLS_FILE *DF, void const *data);
extern U32 DF_set(void const *data);

//
// File list manager
//

#define FILE_SET_DIM    1
#define FILE_BLOCK_SIZE 4

struct FileListMgr
{
   S32 current_size;    
   DLS_FILE*used;           
   DLS_FILE*avail;
   DLS_FILE*table[FILE_SET_DIM];
   DLS_FILE*array;
};

extern void FLM_construct(FileListMgr *FLM);
extern void FLM_dealloc(FileListMgr *FLM, DLS_FILE *entry);
extern void FLM_dealloc(FileListMgr *FLM, S32 entry_num);
extern void FLM_destroy(FileListMgr *FLM);
extern void FLM_cleanup(FileListMgr *FLM);
extern DLS_FILE *FLM_lookup(FileListMgr *FLM, void const *seed);
extern DLS_FILE *FLM_alloc(FileListMgr *FLM, void const *seed=NULL);

extern FileListMgr *file_list;

extern S32 InstrumentsInit();
extern void InstrumentsDeinit();

extern S32 DLSFILE_parse(void const *data, DLS_FILE * *file_var, C8 const *lpFileName, U32 flags);

extern S32 DLS_init     (SINTa *lpdwHandle,
                         S32        dwFlags,
                         HMDIDRIVER MIDI_driver,
                         S32        output_format,
                         S32        output_sample_rate,
                         S32        buffer_size,
                         UINTa      user,
                         AILDLSPCB  buffer_poll_CB,
                         AILDLSLCB  buffer_lock_CB,
                         AILDLSUCB  buffer_unlock_CB);

extern S32 DLS_shutdown (SINTa      dwDLSHandle,
                         S32        dwFlags);

#endif


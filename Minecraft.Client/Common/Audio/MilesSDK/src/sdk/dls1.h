//==========================================================================;
//
// dls1.h
//
// Description:
//
// Interface defines and structures for the Instrument Collection Form
// RIFF DLS (Level 1).
//
// Written by Sonic Foundry 1996. Released for public use.
//
//==========================================================================;

#ifndef _INC_DLS1
#define _INC_DLS1

///////////////////////////////////////////////////////////////////////////
//
// Layout of an instrument collection:
//
// RIFF [] 'DLS ' [dlid,colh,INSTLIST,WAVEPOOL,INFOLIST]
//
// INSTLIST
// LIST [] 'lins'
// LIST [] 'ins ' [dlid,insh,RGNLIST,ARTLIST,INFOLIST]
// LIST [] 'ins ' [dlid,insh,RGNLIST,ARTLIST,INFOLIST]
// LIST [] 'ins ' [dlid,insh,RGNLIST,ARTLIST,INFOLIST]
//
// RGNLIST
// LIST [] 'lrgn'
// LIST [] 'rgn ' [rgnh,wsmp,wlnk,ARTLIST]
// LIST [] 'rgn ' [rgnh,wsmp,wlnk,ARTLIST]
// LIST [] 'rgn ' [rgnh,wsmp,wlnk,ARTLIST]
//
// ARTLIST
// LIST [] 'lart'
// 'art1' level 1 Articulation connection graph
// 'art2' level 2 Articulation connection graph
// '3rd1' Possible 3rd party articulation structure 1
// '3rd2' Possible 3rd party articulation structure 2 .... and so on
//
// WAVEPOOL
// ptbl [] [pool table]
// LIST [] 'wvpl'
// [path],
// [path],
// LIST [] 'wave' [dlid,RIFFWAVE]
// LIST [] 'wave' [dlid,RIFFWAVE]
// LIST [] 'wave' [dlid,RIFFWAVE]
// LIST [] 'wave' [dlid,RIFFWAVE]
// LIST [] 'wave' [dlid,RIFFWAVE]
//
// INFOLIST
// LIST [] 'INFO'
// 'icmt' 'One of those crazy comments.'
// 'icop' 'Copyright (C) 1996 Sonic Foundry'
//
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////
// FOURCC's used in the DLS file
///////////////////////////////////////////////////////////////////////////

#define MSSFOURCC_DLS   MSSmmioFOURCC('D','L','S',' ')
#define MSSFOURCC_DLID  MSSmmioFOURCC('d','l','i','d')
#define MSSFOURCC_COLH  MSSmmioFOURCC('c','o','l','h')
#define MSSFOURCC_WVPL  MSSmmioFOURCC('w','v','p','l')
#define MSSFOURCC_PTBL  MSSmmioFOURCC('p','t','b','l')
#define MSSFOURCC_PATH  MSSmmioFOURCC('p','a','t','h')
#define MSSFOURCC_wave  MSSmmioFOURCC('w','a','v','e')
#define MSSFOURCC_LINS  MSSmmioFOURCC('l','i','n','s')
#define MSSFOURCC_INS   MSSmmioFOURCC('i','n','s',' ')
#define MSSFOURCC_INSH  MSSmmioFOURCC('i','n','s','h')
#define MSSFOURCC_LRGN  MSSmmioFOURCC('l','r','g','n')
#define MSSFOURCC_RGN   MSSmmioFOURCC('r','g','n',' ')
#define MSSFOURCC_RGNH  MSSmmioFOURCC('r','g','n','h')
#define MSSFOURCC_LART  MSSmmioFOURCC('l','a','r','t')
#define MSSFOURCC_ART1  MSSmmioFOURCC('a','r','t','1')
#define MSSFOURCC_WLNK  MSSmmioFOURCC('w','l','n','k')
#define MSSFOURCC_WSMP  MSSmmioFOURCC('w','s','m','p')
#define MSSFOURCC_VERS  MSSmmioFOURCC('v','e','r','s')

///////////////////////////////////////////////////////////////////////////
// Articulation connection graph definitions
///////////////////////////////////////////////////////////////////////////

//
// Generic sources
//

#define CONN_SRC_NONE             0x0000
#define CONN_SRC_LFO              0x0001
#define CONN_SRC_KEYONVELOCITY    0x0002
#define CONN_SRC_KEYNUMBER        0x0003
#define CONN_SRC_EG1              0x0004
#define CONN_SRC_EG2              0x0005
#define CONN_SRC_PITCHWHEEL       0x0006

//
// Midi controllers 0-127
//

#define CONN_SRC_CC1              0x0081
#define CONN_SRC_CC7              0x0087
#define CONN_SRC_CC10             0x008a
#define CONN_SRC_CC11             0x008b

//
// Generic destinations
//

#define CONN_DST_NONE             0x0000
#define CONN_DST_ATTENUATION      0x0001
#define CONN_DST_RESERVED         0x0002
#define CONN_DST_PITCH            0x0003
#define CONN_DST_PAN              0x0004

//
// LFO destinations
//

#define CONN_DST_LFO_FREQUENCY    0x0104
#define CONN_DST_LFO_STARTDELAY   0x0105

//
// EG1 destinations
//

#define CONN_DST_EG1_ATTACKTIME   0x0206
#define CONN_DST_EG1_DECAYTIME    0x0207
#define CONN_DST_EG1_RESERVED     0x0208
#define CONN_DST_EG1_RELEASETIME  0x0209
#define CONN_DST_EG1_SUSTAINLEVEL 0x020a

//
// EG2 destinations
//

#define CONN_DST_EG2_ATTACKTIME   0x030a
#define CONN_DST_EG2_DECAYTIME    0x030b
#define CONN_DST_EG2_RESERVED     0x030c
#define CONN_DST_EG2_RELEASETIME  0x030d
#define CONN_DST_EG2_SUSTAINLEVEL 0x030e

//
// Transforms
//

#define CONN_TRN_NONE             0x0000
#define CONN_TRN_CONCAVE          0x0001

///////////////////////////////////////////////////////////////////////////
// Structures
///////////////////////////////////////////////////////////////////////////

#if !defined(R5900) && !defined(__CELLOS_LV2__) && !defined(__psp__) && !defined(IS_3DS)

#if defined(_PUSHPOP_SUPPORTED) || PRAGMA_STRUCT_PACKPUSH
  #pragma pack(push,1)
#else
  #pragma pack(1)
#endif

#undef MSS_STRUCT
#define MSS_STRUCT struct 

#else

#undef MSS_STRUCT
#define MSS_STRUCT struct __attribute__((__packed__))

#endif


typedef struct _DLSID 
{
   U32 ulData1;
   U16 usData2;
   U16 usData3;
   S8  abData4[8];
} 
DLSID, *LPDLSID;

typedef struct _CONNECTION
{
   U16 usSource;
   U16 usControl;
   U16 usDestination;
   U16 usTransform;
   S32 lScale;
}
CONNECTION, *LPCONNECTION;

typedef struct _RGNRANGE 
{
   U16 usLow;
   U16 usHigh;
}
RGNRANGE, * LPRGNRANGE;

typedef struct _RGNHEADER 
{
   RGNRANGE RangeKey;      // Key range
   RGNRANGE RangeVelocity; // Velocity range
   U16      fusOptions;    // Synthesis options for this range
   U16      usKeyGroup;    // Key grouping for non simultaneous play

   //
   // 0 = no group, 1 up is group
   // for Level 1 only groups 1-15 are allowed
   //
}
RGNHEADER, *LPRGNHEADER;

//
// Any paths or links are stored right after struct _WAVELINK
//

typedef struct _WAVELINK 
{ 
   U16 fusOptions;   // options flags for this wave
   U16 usPhaseGroup; // phase grouping for locking channels
   U32 ulChannel;    // channel placement
   U32 ulTableIndex; // index into the wave pool table, 0-based
}
WAVELINK, *LPWAVELINK;

typedef struct _rwsmp 
{
   U32 cbSize;
   U16 usUnityNote;  // MIDI unity playback note
   S16 sFineTune;    // Fine tune in log tuning
   S32 lAttenuation; // Overall attenuation to be applied to data
   U32 fulOptions;   // Flag options
   U32 cSampleLoops; // Count of sample loops, 0 loops is one-shot
} 
WSMPL, *LPWSMPL;

#if !defined(R5900) && !defined(__CELLOS_LV2__) && !defined(__psp__) && !defined(IS_3DS)

#if defined(_PUSHPOP_SUPPORTED) || PRAGMA_STRUCT_PACKPUSH
  #pragma pack(pop)
#else
  #pragma pack()
#endif

#endif


typedef struct _DLSVERSION 
{
   S32 dwVersionMS;
   S32 dwVersionLS;
}  
DLSVERSION, *LPDLSVERSION;

//
// Level 1 Articulation Data
//

typedef struct _CONNECTIONLIST 
{
   U32 cbSize;        // size of the connection list structure
   U32 cConnections;  // count of connections in the list
} 
CONNECTIONLIST, *LPCONNECTIONLIST;

///////////////////////////////////////////////////////////////////////////
// Generic type defines for regions and instruments
///////////////////////////////////////////////////////////////////////////

#define F_INSTRUMENT_DRUMS 0x80000000

typedef struct _MIDILOCALE 
{
   U32 ulBank;
   U32 ulInstrument;
}
MIDILOCALE, *LPMIDILOCALE;

///////////////////////////////////////////////////////////////////////////
// Header structures found in an DLS file for collection, instruments, and
// regions
///////////////////////////////////////////////////////////////////////////

#define F_RGN_OPTION_SELFNONEXCLUSIVE 0x0001

typedef struct _INSTHEADER 
{
   U32        cRegions; // Count of regions in this instrument
   MIDILOCALE Locale;   // Intended MIDI locale of this instrument
}
INSTHEADER, *LPINSTHEADER;

typedef struct _DLSHEADER 
{
   U32 cInstruments; // Count of instruments in the collection
}
DLSHEADER, *LPDLSHEADER;

//////////////////////////////////////////////////////////////////////////////
// Definitions for the Wave link structure
//////////////////////////////////////////////////////////////////////////////

//
// **** For level 1 only WAVELINK_CHANNEL_MONO is valid ****
//
// ulChannel allows for up to 32 channels of audio with each bit position
// specifiying a channel of playback
//

#define WAVELINK_CHANNEL_LEFT    0x0001l
#define WAVELINK_CHANNEL_RIGHT   0x0002l
#define F_WAVELINK_PHASE_MASTER  0x0001


#define POOL_CUE_NULL 0xffffffffl

typedef struct _POOLCUE
{
   U32 ulOffset; // Offset to the entry in the list
}
POOLCUE, *LPPOOLCUE;

typedef struct _POOLTABLE 
{
   U32 cbSize;  // size of the pool table structure
   U32 cCues;   // count of cues in the list
} 
POOLTABLE, *LPPOOLTABLE;

//////////////////////////////////////////////////////////////////////////////
// Structures for the "wsmp" chunk
//////////////////////////////////////////////////////////////////////////////

#define F_WSMP_NO_TRUNCATION  0x0001l
#define F_WSMP_NO_COMPRESSION 0x0002l

//
// This loop type is a normal forward-playing loop which is continually
// played until the envelope reaches an off threshold in the release
// portion of the volume envelope
//

#define WLOOP_TYPE_FORWARD 0

typedef struct _rloop 
{
   U32 cbSize;
   U32 ulType;     // Loop type
   U32 ulStart;    // Start of loop in samples
   U32 ulLength;   // Length of loop in samples
} 
WLOOP, *LPWLOOP;

#endif


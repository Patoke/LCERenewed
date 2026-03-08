#ifndef HLRUNTIME_H
#define HLRUNTIME_H

//
// Parsing
//
F32 atof_with_len(const char* i_source, S32* o_count);
S32 atoi_with_len(const char* i_source, S32 hex, S32* o_count);

// Spatialization types
//-----------------------------------------------------------------------------
typedef enum
{
  MSS_2D_DEFAULT,
  MSS_3D,
  MSS_2D_USER_CHANMAT,

  MSS_SPAT_COUNT           // total number of spats
}
MSS_SPAT_TYPE;

// Control bits for SNDPRESET->update_mask, also used in property definition
//-----------------------------------------------------------------------------
enum SNDCTRL
{
   BIT_SND_SPAT_TYPE,       // 1 value
   BIT_SND_3D_POS,          // 1 vector
   BIT_SND_3D_CONE,         // 1 vector
   BIT_SND_3D_DISTANCES,    // 3 values (float, float, bool)
   BIT_SND_3D_ORIENT,       // 2 vectors
   BIT_SND_3D_VEL,          // 1 vector, 1 value
   BIT_SND_3D_ROLLOFF,      // 1 value
   BIT_SND_REVERB_LEVELS,   // 2 values
   BIT_SND_LPF_CUTOFF,      // 1 value
   BIT_SND_OCCLUSION,       // disabled by default 1 value
   BIT_SND_EXCLUSION,       // 1 value
   BIT_SND_OBSTRUCTION,     // 1 value
   BIT_SND_PLAYBACK_RATE,   // 1 value
   BIT_SND_VOLUME_PAN,      // 5 values
   BIT_SND_3D_FALLOFF_GRAPH,
  
   BIT_SND_FILTER_PROPERTY, // (filter properties aren't individually classified since they don't usually have redundant representations)
   BIT_SND_CHAN_MATRIX,     // crapton of "C "
   BIT_SND_SPKR_LEVELS      // crapton of "S "
};

enum SNDCTRLMASK
{
   SND_SPAT_TYPE = 1 << BIT_SND_SPAT_TYPE,
   SND_3D_POS = 1 << BIT_SND_3D_POS,
   SND_3D_CONE = 1 << BIT_SND_3D_CONE,
   SND_3D_DISTANCES = 1 << BIT_SND_3D_DISTANCES,
   SND_3D_ORIENT = 1 << BIT_SND_3D_ORIENT,
   SND_3D_VEL = 1 << BIT_SND_3D_VEL, 
   SND_3D_ROLLOFF = 1 << BIT_SND_3D_ROLLOFF,
   SND_REVERB_LEVELS = 1 << BIT_SND_REVERB_LEVELS, 
   SND_LPF_CUTOFF = 1 << BIT_SND_LPF_CUTOFF, 
   SND_OCCLUSION = 1 << BIT_SND_OCCLUSION, 
   SND_EXCLUSION = 1 << BIT_SND_EXCLUSION, 
   SND_OBSTRUCTION = 1 << BIT_SND_OBSTRUCTION, 
   SND_PLAYBACK_RATE = 1 << BIT_SND_PLAYBACK_RATE,
   SND_VOLUME_PAN = 1 << BIT_SND_VOLUME_PAN,
   SND_3D_FALLOFF_GRAPH = 1 << BIT_SND_3D_FALLOFF_GRAPH
};

// Control bits for ENVPRESET->update_mask, also used in property definition
//-----------------------------------------------------------------------------
enum ENVCTRL
{
   BIT_ENV_REVERB_ROOM_TYPE,
   BIT_ENV_REVERB_PARAMS,
   BIT_ENV_REVERB_MASTER_VOLUME,
   BIT_ENV_MASTER_VOLUME,
   BIT_ENV_ROLLOFF,
   BIT_ENV_DOPPLER,
   BIT_ENV_FALLOFF_POWER,
};

enum ENVCTRLMASK
{
   ENV_REVERB_ROOM_TYPE = 1 << BIT_ENV_REVERB_ROOM_TYPE,
   ENV_REVERB_PARAMS = 1 << BIT_ENV_REVERB_PARAMS,
   ENV_REVERB_MASTER_VOLUME = 1 << BIT_ENV_REVERB_MASTER_VOLUME,
   ENV_MASTER_VOLUME = 1 << BIT_ENV_MASTER_VOLUME,
   ENV_ROLLOFF = 1 << BIT_ENV_ROLLOFF,
   ENV_DOPPLER = 1 << BIT_ENV_DOPPLER,
   ENV_FALLOFF_POWER = 1 << BIT_ENV_FALLOFF_POWER,
};

//
// Property String Declarations
//
#define ENV_F_MASTER_VOLUME "Master volume"
#define ENV_F_REVERB_MASTER_DRY_VOL "Master dry volume"
#define ENV_F_REVERB_MASTER_WET_VOL "Master wet volume"

#define ENV_RTYPE_REVERB_ROOM_TYPE "Room type"
#define ENV_F_REVERB_TIME_SECS "Reverb time"
#define ENV_F_REVERB_PREDELAY_SECS "Reverb predelay"
#define ENV_F_REVERB_DAMPING "Reverb damping factor"
#define ENV_V_LISTENER_POS "3D listener position vector"
#define ENV_O_LISTENER_ORIENTATION "3D listener orientation"
#define ENV_V_LISTENER_VEL "3D listener velocity direction"
#define ENV_F_LISTENER_MAG "3D listener velocity magnitude"
#define ENV_F_ROLLOFF "3D rolloff factor"
#define ENV_F_DOPPLER "3D Doppler factor"
#define ENV_F_FALLOFF_POWER "3D speaker falloff power"


#define SOUND_F_PLAYBACK_RATE_FACTOR "Playback rate scale factor"
#define SOUND_F_VOL "Volume"
#define SOUND_F_LR_PAN "Left-right pan"
#define SOUND_F_FB_PAN "Front-back pan"
#define SOUND_F_CENTER_VOL "Center volume"
#define SOUND_F_LFE_VOL "LFE volume"
#define SOUND_F_WET_VOL "Reverb wet level"
#define SOUND_F_DRY_VOL "Reverb dry level"
#define SOUND_F_LPF_CUTOFF "Built-in LPF cutoff"
#define SOUND_F_OCCLUSION "Occlusion"
#define SOUND_F_OBSTRUCTION "Obstruction"
#define SOUND_F_EXCLUSION "Exclusion"
#define SOUND_STYPE_HOW_SPATIALIZE "Spatialization type"
#define SOUND_F_3D_MIN_DIST "3D minimum distance"
#define SOUND_F_3D_MAX_DIST "3D maximum distance"
#define SOUND_B_3D_AUTO_WET_ATTEN "3D wet reverb attenuation"
#define SOUND_F_3D_CONE_INNER "3D cone inner angle"
#define SOUND_F_3D_CONE_OUTER "3D cone outer angle"
#define SOUND_F_3D_CONE_OUTER_VOL "3D cone outer volume"
#define SOUND_F_3D_ROLLOFF "3D rolloff"
#define SOUND_3D_FALLOFF_GRAPH "Falloff Graph"

#define SOUND_CHANNEL_VOL "Channel level"
#define SOUND_SPEAKER_VOL "Speaker level"

#endif

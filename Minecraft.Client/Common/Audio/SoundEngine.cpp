#include "stdafx.h"

#include "SoundEngine.h"
#include "..\Consoles_App.h"
#include "..\..\MultiplayerLocalPlayer.h"
#include "..\..\..\Minecraft.World\net.minecraft.world.level.h"
#include "..\..\Minecraft.World\leveldata.h"
#include "..\..\Minecraft.World\mth.h"
#include "..\..\TexturePackRepository.h"
#include "..\..\DLCTexturePack.h"
#include "Common\DLC\DLCAudioFile.h"

#ifdef _WINDOWS64
char SoundEngine::m_szSoundPath[] = {"Windows64Media\\Sound\\"};
char SoundEngine::m_szMusicPath[] = {"music\\"};
#endif

char *SoundEngine::m_szStreamFileA[eStream_Max] =
{
	"calm1",
	"calm2",
	"calm3",
	"hal1",
	"hal2",
	"hal3",
	"hal4",
	"nuance1",
	"nuance2",
	// add the new music tracks
	"creative1",
	"creative2",
	"creative3",
	"creative4",
	"creative5",
	"creative6",
	"menu1",
	"menu2",
	"menu3",
	"menu4",
	"piano1",
	"piano2",
	"piano3",

	// Nether
	"nether1",
	"nether2",
	"nether3",
	"nether4",
	// The End
	"the_end_dragon_alive",
	"the_end_end",
	// CDs
	"11",
	"13",
	"blocks",
	"cat",
	"chirp",
	"far",
	"mall",
	"mellohi",
	"stal",
	"strad",
	"ward",
	"where_are_we_now"
};

/////////////////////////////////////////////
//
//	SoundEngine
//
/////////////////////////////////////////////
SoundEngine::SoundEngine()
{
	random = new Random();
	memset(&m_engine, 0, sizeof(ma_engine));
	memset(&m_engineConfig, 0, sizeof(ma_engine_config));
	m_musicStreamActive = false;
	m_StreamState = eMusicStreamState_Idle;
	m_iMusicDelay = 0;
	m_validListenerCount = 0;

	m_bHeardTrackA = NULL;

	SetStreamingSounds(eStream_Overworld_Calm1, eStream_Overworld_piano3,
		eStream_Nether1, eStream_Nether4,
		eStream_end_dragon, eStream_end_end,
		eStream_CD_1);

	m_musicID = getMusicID(LevelData::DIMENSION_OVERWORLD);

	m_StreamingAudioInfo.bIs3D  = false;
	m_StreamingAudioInfo.x      = 0;
	m_StreamingAudioInfo.y      = 0;
	m_StreamingAudioInfo.z      = 0;
	m_StreamingAudioInfo.volume = 1;
	m_StreamingAudioInfo.pitch  = 1;

	memset(CurrentSoundsPlaying, 0, sizeof(int) * (eSoundType_MAX + eSFX_MAX));
	memset(m_ListenerA, 0, sizeof(AUDIO_LISTENER) * XUSER_MAX_COUNT);
}

void SoundEngine::destroy() {}

#ifdef _DEBUG
void SoundEngine::GetSoundName(char *szSoundName, int iSound)
{
	strcpy((char *)szSoundName, "Minecraft/");
	wstring name     = wchSoundNames[iSound];
	char   *SoundName = (char *)ConvertSoundPathToName(name);
	strcat((char *)szSoundName, SoundName);
}
#endif

/////////////////////////////////////////////
//
//	init
//
/////////////////////////////////////////////
void SoundEngine::init(Options *pOptions)
{
	app.DebugPrintf("---SoundEngine::init\n");

	m_engineConfig                = ma_engine_config_init();
	m_engineConfig.listenerCount  = MAX_LOCAL_PLAYERS;

	if(ma_engine_init(&m_engineConfig, &m_engine) != MA_SUCCESS)
	{
		app.DebugPrintf("Failed to initialize miniaudio engine\n");
		return;
	}

	ma_engine_set_volume(&m_engine, 1.0f);

	m_MasterMusicVolume   = 1.0f;
	m_MasterEffectsVolume = 1.0f;
	m_validListenerCount  = 1;
	m_bSystemMusicPlaying = false;

	app.DebugPrintf("---SoundEngine::init - done\n");
}

/////////////////////////////////////////////
//
//	SetStreamingSounds
//
/////////////////////////////////////////////
void SoundEngine::SetStreamingSounds(int iOverworldMin, int iOverWorldMax, int iNetherMin, int iNetherMax, int iEndMin, int iEndMax, int iCD1)
{
	m_iStream_Overworld_Min = iOverworldMin;
	m_iStream_Overworld_Max = iOverWorldMax;
	m_iStream_Nether_Min    = iNetherMin;
	m_iStream_Nether_Max    = iNetherMax;
	m_iStream_End_Min       = iEndMin;
	m_iStream_End_Max       = iEndMax;
	m_iStream_CD_1          = iCD1;

	if(m_bHeardTrackA)
		delete[] m_bHeardTrackA;

	m_bHeardTrackA = new bool[iEndMax + 1];
	memset(m_bHeardTrackA, 0, sizeof(bool) * (iEndMax + 1));
}

/////////////////////////////////////////////
//
//	updateMiniAudio
//
/////////////////////////////////////////////
void SoundEngine::updateMiniAudio()
{
	if(m_validListenerCount == 1)
	{
		for(int i = 0; i < MAX_LOCAL_PLAYERS; i++)
		{
			if(m_ListenerA[i].bValid)
			{
				ma_engine_listener_set_position(&m_engine, 0,
					m_ListenerA[i].vPosition.x,
					m_ListenerA[i].vPosition.y,
					-m_ListenerA[i].vPosition.z);
				ma_engine_listener_set_direction(&m_engine, 0,
					m_ListenerA[i].vOrientFront.x,
					m_ListenerA[i].vOrientFront.y,
					-m_ListenerA[i].vOrientFront.z);
				ma_engine_listener_set_world_up(&m_engine, 0, 0.0f, 1.0f, 0.0f);
				break;
			}
		}
	}
	else
	{
		// splitscreen: listener at origin, sounds repositioned by distance down the z axis
		ma_engine_listener_set_position(&m_engine, 0, 0.0f, 0.0f, 0.0f);
		ma_engine_listener_set_direction(&m_engine, 0, 0.0f, 0.0f, 1.0f);
		ma_engine_listener_set_world_up(&m_engine, 0, 0.0f, 1.0f, 0.0f);
	}

	// iterate over active sounds and clean up completed ones
	for(auto it = m_activeSounds.begin(); it != m_activeSounds.end(); )
	{
		MiniAudioSound *s = *it;

		if(!ma_sound_is_playing(&s->sound))
		{
			ma_sound_uninit(&s->sound);
			delete s;
			it = m_activeSounds.erase(it);
			continue;
		}

		float finalVolume = s->info.volume * m_MasterEffectsVolume;
		if(finalVolume > 1.0f) finalVolume = 1.0f;

		ma_sound_set_volume(&s->sound, finalVolume);
		ma_sound_set_pitch(&s->sound, s->info.pitch);

		if(s->info.bIs3D)
		{
			if(m_validListenerCount > 1)
			{
				float fClosest  = 10000.0f;
				float fClosestX = 0.0f, fClosestY = 0.0f, fClosestZ = 0.0f, fDist;

				for(int i = 0; i < MAX_LOCAL_PLAYERS; i++)
				{
					if(m_ListenerA[i].bValid)
					{
						float x = fabs(m_ListenerA[i].vPosition.x - s->info.x);
						float y = fabs(m_ListenerA[i].vPosition.y - s->info.y);
						float z = fabs(m_ListenerA[i].vPosition.z - s->info.z);
						fDist   = x + y + z;

						if(fDist < fClosest)
						{
							fClosest  = fDist;
							fClosestX = x;
							fClosestY = y;
							fClosestZ = z;
						}
					}
				}

				float realDist = sqrtf((fClosestX * fClosestX) + (fClosestY * fClosestY) + (fClosestZ * fClosestZ));
				ma_sound_set_position(&s->sound, 0, 0, realDist);
			}
			else
			{
				ma_sound_set_position(&s->sound, s->info.x, s->info.y, -s->info.z);
			}
		}

		++it;
	}
}

/////////////////////////////////////////////
//
//	tick
//
/////////////////////////////////////////////
void SoundEngine::tick(shared_ptr<Mob> *players, float a)
{
	ConsoleSoundEngine::tick();

	int listenerCount = 0;

	if(players)
	{
		for(int i = 0; i < MAX_LOCAL_PLAYERS; i++)
		{
			if(players[i] != NULL)
			{
				m_ListenerA[i].bValid = true;

				F32 x = players[i]->xo + (players[i]->x - players[i]->xo) * a;
				F32 y = players[i]->yo + (players[i]->y - players[i]->yo) * a;
				F32 z = players[i]->zo + (players[i]->z - players[i]->zo) * a;

				float yRot = players[i]->yRotO + (players[i]->yRot - players[i]->yRotO) * a;
				float yCos = (float)cos(-yRot * Mth::RAD_TO_GRAD - PI);
				float ySin = (float)sin(-yRot * Mth::RAD_TO_GRAD - PI);

				m_ListenerA[i].vPosition.x    = x;
				m_ListenerA[i].vPosition.y    = y;
				m_ListenerA[i].vPosition.z    = z;
				m_ListenerA[i].vOrientFront.x = ySin;
				m_ListenerA[i].vOrientFront.y = 0;
				m_ListenerA[i].vOrientFront.z = yCos;

				listenerCount++;
			}
			else
			{
				m_ListenerA[i].bValid = false;
			}
		}
	}

	if(listenerCount == 0)
	{
		m_ListenerA[0].vPosition.x    = 0;
		m_ListenerA[0].vPosition.y    = 0;
		m_ListenerA[0].vPosition.z    = 0;
		m_ListenerA[0].vOrientFront.x = 0;
		m_ListenerA[0].vOrientFront.y = 0;
		m_ListenerA[0].vOrientFront.z = 1.0f;
		listenerCount++;
	}
	m_validListenerCount = listenerCount;

	updateMiniAudio();
}

/////////////////////////////////////////////
//
//	play
//
/////////////////////////////////////////////
void SoundEngine::play(int iSound, float x, float y, float z, float volume, float pitch)
{
	if(iSound == -1)
	{
		app.DebugPrintf(6, "PlaySound with sound of -1 !!!!!!!!!!!!!!!\n");
		return;
	}

	strcpy((char *)m_szSoundName, "Minecraft/");

	wstring name = wchSoundNames[iSound];
	char *SoundName = (char *)ConvertSoundPathToName(name, false);
	strcat((char *)m_szSoundName, SoundName);

	app.DebugPrintf(6, "PlaySound - %d - %s (%f %f %f, vol %f, pitch %f)\n",
		iSound, m_szSoundName, x, y, z, volume, pitch);

	char basePath[256];
	sprintf_s(basePath, "%s%s", m_szSoundPath, (char *)m_szSoundName);

	char finalPath[256];
	finalPath[0] = 0;

	static const char *extensions[] = {".ogg", ".wav", ".mp3"};

	for(int e = 0; e < 3 && !finalPath[0]; e++)
	{
		char candidate[256];
		sprintf_s(candidate, "%s%s", basePath, extensions[e]);
		DWORD attr = GetFileAttributesA(candidate);
		if(attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
			strcpy_s(finalPath, candidate);
	}

	if(!finalPath[0])
	{
		int count = 0;
		for(int e = 0; e < 3; e++)
		{
			for(int n = 1; n < 32; n++)
			{
				char candidate[256];
				sprintf_s(candidate, "%s%d%s", basePath, n, extensions[e]);
				DWORD attr = GetFileAttributesA(candidate);
				if(attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
					count = n;
				else
					break;
			}
		}

		if(count > 0)
		{
			int chosen = (rand() % count) + 1;
			for(int e = 0; e < 3 && !finalPath[0]; e++)
			{
				char candidate[256];
				sprintf_s(candidate, "%s%d%s", basePath, chosen, extensions[e]);
				DWORD attr = GetFileAttributesA(candidate);
				if(attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
					strcpy_s(finalPath, candidate);
			}
		}
	}

	if(!finalPath[0])
	{
		app.DebugPrintf("SoundEngine::play - no file found for %s\n", basePath);
		return;
	}

	MiniAudioSound *s = new MiniAudioSound();
	memset(&s->info, 0, sizeof(AUDIO_INFO));

	s->info.x      = x;
	s->info.y      = y;
	s->info.z      = z;
	s->info.volume = volume;
	s->info.pitch  = pitch;
	s->info.bIs3D  = true;
	s->info.bUseSoundsPitchVal = false;
	s->info.iSound = iSound + eSFX_MAX;

	if(ma_sound_init_from_file(&m_engine, finalPath, MA_SOUND_FLAG_ASYNC, NULL, NULL, &s->sound) != MA_SUCCESS)
	{
		app.DebugPrintf("SoundEngine::play - failed to init sound from file: %s\n", finalPath);
		delete s;
		return;
	}

	float finalVolume = volume * m_MasterEffectsVolume;
	if(finalVolume > 1.0f) finalVolume = 1.0f;

	float distanceMax = 16.0f;
	if(volume == 10000.0f)
	{
		distanceMax = 10000.0f;
		finalVolume = 1.0f;
	}

	switch(iSound)
	{
	case eSoundType_MOB_ENDERDRAGON_GROWL:
	case eSoundType_MOB_ENDERDRAGON_MOVE:
	case eSoundType_MOB_ENDERDRAGON_END:
	case eSoundType_MOB_ENDERDRAGON_HIT:
	case eSoundType_FIREWORKS_BLAST:
	case eSoundType_FIREWORKS_BLAST_FAR:
	case eSoundType_FIREWORKS_LARGE_BLAST:
	case eSoundType_FIREWORKS_LARGE_BLAST_FAR:
		distanceMax = 100.0f;
		break;
	case eSoundType_MOB_GHAST_MOAN:
	case eSoundType_MOB_GHAST_SCREAM:
	case eSoundType_MOB_GHAST_DEATH:
	case eSoundType_MOB_GHAST_CHARGE:
	case eSoundType_MOB_GHAST_FIREBALL:
		distanceMax = 30.0f;
		break;
	}

	ma_sound_set_spatialization_enabled(&s->sound, MA_TRUE);
	ma_sound_set_min_distance(&s->sound, 1.0f);
	ma_sound_set_max_distance(&s->sound, distanceMax);
	ma_sound_set_rolloff(&s->sound, 1.0f);
	ma_sound_set_attenuation_model(&s->sound, ma_attenuation_model_linear);
	ma_sound_set_volume(&s->sound, finalVolume);
	ma_sound_set_pitch(&s->sound, pitch);
	ma_sound_set_position(&s->sound, x, y, -z);

	ma_sound_start(&s->sound);

	m_activeSounds.push_back(s);
}

/////////////////////////////////////////////
//
//	playUI
//
/////////////////////////////////////////////
void SoundEngine::playUI(int iSound, float volume, float pitch)
{
	wstring name;

	if(iSound >= eSFX_MAX)
	{
		name = wchSoundNames[iSound];
	}
	else
	{
		name = wchUISoundNames[iSound];
	}

	char *SoundName = (char *)ConvertSoundPathToName(name, false);

	char basePath[256];
	sprintf_s(basePath, "%sMinecraft/UI/%s", m_szSoundPath, SoundName);

	char finalPath[256];
	finalPath[0] = 0;

	static const char *extensions[] = {".ogg", ".wav", ".mp3"};

	for(int e = 0; e < 3 && !finalPath[0]; e++)
	{
		char candidate[256];
		sprintf_s(candidate, "%s%s", basePath, extensions[e]);
		DWORD attr = GetFileAttributesA(candidate);
		if(attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
			strcpy_s(finalPath, candidate);
	}

	if(!finalPath[0])
	{
		app.DebugPrintf("SoundEngine::playUI - no file found for %s\n", basePath);
		return;
	}

	MiniAudioSound *s = new MiniAudioSound();
	memset(&s->info, 0, sizeof(AUDIO_INFO));

	s->info.volume           = volume;
	s->info.pitch            = pitch;
	s->info.bIs3D            = false;
	s->info.bUseSoundsPitchVal = true;

	if(ma_sound_init_from_file(&m_engine, finalPath, MA_SOUND_FLAG_ASYNC, NULL, NULL, &s->sound) != MA_SUCCESS)
	{
		app.DebugPrintf("SoundEngine::playUI - failed to init sound from file: %s\n", finalPath);
		delete s;
		return;
	}

	float finalVolume = volume * m_MasterEffectsVolume;
	if(finalVolume > 1.0f) finalVolume = 1.0f;

	ma_sound_set_spatialization_enabled(&s->sound, MA_FALSE);
	ma_sound_set_volume(&s->sound, finalVolume);
	ma_sound_set_pitch(&s->sound, pitch);

	ma_sound_start(&s->sound);

	m_activeSounds.push_back(s);
}

/////////////////////////////////////////////
//
//	playStreaming
//
/////////////////////////////////////////////
void SoundEngine::playStreaming(const wstring& name, float x, float y, float z, float volume, float pitch, bool bMusicDelay)
{
	// This function doesn't actually play a streaming sound, just sets states and an id for the music tick to play it
	// Level audio will be played when a play with an empty name comes in
	// CD audio will be played when a named stream comes in

	m_StreamingAudioInfo.x      = x;
	m_StreamingAudioInfo.y      = y;
	m_StreamingAudioInfo.z      = z;
	m_StreamingAudioInfo.volume = volume;
	m_StreamingAudioInfo.pitch  = pitch;

	if(m_StreamState == eMusicStreamState_Playing)
		m_StreamState = eMusicStreamState_Stop;
	else if(m_StreamState == eMusicStreamState_Opening)
		m_StreamState = eMusicStreamState_OpeningCancel;

	if(name.empty())
	{
		// music, or stop CD
		m_StreamingAudioInfo.bIs3D = false;

		// random delay of up to 3 minutes for music
		m_iMusicDelay = random->nextInt(20 * 60 * 3);

#ifdef _DEBUG
		m_iMusicDelay = 0;
#endif

		Minecraft *pMinecraft = Minecraft::GetInstance();

		bool playerInEnd    = false;
		bool playerInNether = false;

		for(unsigned int i = 0; i < MAX_LOCAL_PLAYERS; i++)
		{
			if(pMinecraft->localplayers[i] != NULL)
			{
				if(pMinecraft->localplayers[i]->dimension == LevelData::DIMENSION_END)
					playerInEnd = true;
				else if(pMinecraft->localplayers[i]->dimension == LevelData::DIMENSION_NETHER)
					playerInNether = true;
			}
		}

		if(playerInEnd)
			m_musicID = getMusicID(LevelData::DIMENSION_END);
		else if(playerInNether)
			m_musicID = getMusicID(LevelData::DIMENSION_NETHER);
		else
			m_musicID = getMusicID(LevelData::DIMENSION_OVERWORLD);
	}
	else
	{
		// jukebox
		m_StreamingAudioInfo.bIs3D = true;
		m_musicID                  = getMusicID(name);
		m_iMusicDelay              = 0;
	}
}

/////////////////////////////////////////////
//
//	OpenStreamThreadProc
//
//	Don't actually open in this thread, as it can block for ~300ms.
//
/////////////////////////////////////////////
int SoundEngine::OpenStreamThreadProc(void *lpParameter)
{
	SoundEngine *soundEngine = (SoundEngine *)lpParameter;

	if(soundEngine->m_musicStreamActive)
	{
		ma_sound_stop(&soundEngine->m_musicStream);
		ma_sound_uninit(&soundEngine->m_musicStream);
		soundEngine->m_musicStreamActive = false;
	}

	ma_result result = ma_sound_init_from_file(
		&soundEngine->m_engine,
		soundEngine->m_szStreamName,
		MA_SOUND_FLAG_STREAM,
		NULL, NULL,
		&soundEngine->m_musicStream);

	if(result != MA_SUCCESS)
	{
		app.DebugPrintf("SoundEngine::OpenStreamThreadProc - Could not open - %s\n", soundEngine->m_szStreamName);
		return 0;
	}

	ma_sound_set_spatialization_enabled(&soundEngine->m_musicStream, MA_FALSE);
	ma_sound_set_looping(&soundEngine->m_musicStream, MA_FALSE);

	soundEngine->m_musicStreamActive = true;

	return 0;
}

/////////////////////////////////////////////
//
//	GetRandomishTrack
//
/////////////////////////////////////////////
int SoundEngine::GetRandomishTrack(int iStart, int iEnd)
{
	// make it more likely that we'll get a track we've not heard for a while, although repeating tracks sometimes is fine

	bool bAllTracksHeard = true;
	int  iVal            = iStart;

	for(int i = iStart; i <= iEnd; i++)
	{
		if(m_bHeardTrackA[i] == false)
		{
			bAllTracksHeard = false;
			app.DebugPrintf("Not heard all tracks yet\n");
			break;
		}
	}

	if(bAllTracksHeard)
	{
		app.DebugPrintf("Heard all tracks - resetting the tracking array\n");
		for(int i = iStart; i <= iEnd; i++)
			m_bHeardTrackA[i] = false;
	}

	// trying to get a track we haven't heard, but not too hard
	for(int i = 0; i <= ((iEnd - iStart) / 2); i++)
	{
		// random->nextInt(1) will always return 0
		iVal = random->nextInt((iEnd - iStart) + 1) + iStart;
		if(m_bHeardTrackA[iVal] == false)
		{
			app.DebugPrintf("(%d) Not heard track %d yet, so playing it now\n", i, iVal);
			m_bHeardTrackA[iVal] = true;
			break;
		}
		else
		{
			app.DebugPrintf("(%d) Skipping track %d already heard it recently\n", i, iVal);
		}
	}

	app.DebugPrintf("Select track %d\n", iVal);
	return iVal;
}

/////////////////////////////////////////////
//
//	getMusicID
//
/////////////////////////////////////////////
int SoundEngine::getMusicID(int iDomain)
{
	Minecraft *pMinecraft = Minecraft::GetInstance();

	if(pMinecraft == NULL)
		return GetRandomishTrack(m_iStream_Overworld_Min, m_iStream_Overworld_Max);

	if(pMinecraft->skins->isUsingDefaultSkin())
	{
		switch(iDomain)
		{
		case LevelData::DIMENSION_END:
			// the end isn't random - it has different music depending on whether the dragon is alive or not, but we've not added the dead dragon music yet
			return m_iStream_End_Min;
		case LevelData::DIMENSION_NETHER:
			return GetRandomishTrack(m_iStream_Nether_Min, m_iStream_Nether_Max);
		default:
			return GetRandomishTrack(m_iStream_Overworld_Min, m_iStream_Overworld_Max);
		}
	}
	else
	{
		// using a texture pack - may have multiple End music tracks
		switch(iDomain)
		{
		case LevelData::DIMENSION_END:
			return GetRandomishTrack(m_iStream_End_Min, m_iStream_End_Max);
		case LevelData::DIMENSION_NETHER:
			return GetRandomishTrack(m_iStream_Nether_Min, m_iStream_Nether_Max);
		default:
			return GetRandomishTrack(m_iStream_Overworld_Min, m_iStream_Overworld_Max);
		}
	}
}

/////////////////////////////////////////////
//
//	getMusicID
//
/////////////////////////////////////////////
// check what the CD is
int SoundEngine::getMusicID(const wstring& name)
{
	int   iCD      = 0;
	char *SoundName = (char *)ConvertSoundPathToName(name, true);

	// these will always be the game cds, so use the m_szStreamFileA for this
	for(int i = 0; i < 12; i++)
	{
		if(strcmp(SoundName, m_szStreamFileA[i + eStream_CD_1]) == 0)
		{
			iCD = i;
			break;
		}
	}

	// adjust for cd start position on normal or mash-up pack
	return iCD + m_iStream_CD_1;
}

/////////////////////////////////////////////
//
//	getMasterMusicVolume
//
/////////////////////////////////////////////
float SoundEngine::getMasterMusicVolume()
{
	if(m_bSystemMusicPlaying)
		return 0.0f;
	else
		return m_MasterMusicVolume;
}

/////////////////////////////////////////////
//
//	updateMusicVolume
//
/////////////////////////////////////////////
void SoundEngine::updateMusicVolume(float fVal)
{
	m_MasterMusicVolume = fVal;
}

/////////////////////////////////////////////
//
//	updateSystemMusicPlaying
//
/////////////////////////////////////////////
void SoundEngine::updateSystemMusicPlaying(bool isPlaying)
{
	m_bSystemMusicPlaying = isPlaying;
}

/////////////////////////////////////////////
//
//	updateSoundEffectVolume
//
/////////////////////////////////////////////
void SoundEngine::updateSoundEffectVolume(float fVal)
{
	m_MasterEffectsVolume = fVal;
}

void SoundEngine::add(const wstring& name, File *file)          {}
void SoundEngine::addMusic(const wstring& name, File *file)     {}
void SoundEngine::addStreaming(const wstring& name, File *file) {}
bool SoundEngine::isStreamingWavebankReady()                    { return true; }

/////////////////////////////////////////////
//
//	playMusicTick
//
/////////////////////////////////////////////
void SoundEngine::playMusicTick()
{
	playMusicUpdate();
}

/////////////////////////////////////////////
//
//	playMusicUpdate
//
/////////////////////////////////////////////
void SoundEngine::playMusicUpdate()
{
	static float fMusicVol = 0.0f;
	fMusicVol = getMasterMusicVolume();

	switch(m_StreamState)
	{
	case eMusicStreamState_Idle:

		// start a stream playing
		if(m_iMusicDelay > 0)
		{
			m_iMusicDelay--;
			return;
		}

		if(m_musicID != -1)
		{
			strcpy((char *)m_szStreamName, m_szMusicPath);

			if(Minecraft::GetInstance()->skins->getSelected()->hasAudio())
			{
				// It's a mash-up - need to use the DLC path for the music
				TexturePack    *pTexPack    = Minecraft::GetInstance()->skins->getSelected();
				DLCTexturePack *pDLCTexPack = (DLCTexturePack *)pTexPack;
				DLCPack        *pack        = pDLCTexPack->getDLCInfoParentPack();
				DLCAudioFile   *dlcAudioFile = (DLCAudioFile *)pack->getFile(DLCManager::e_DLCType_Audio, 0);

				app.DebugPrintf("Mashup pack \n");

				if(m_musicID < m_iStream_CD_1)
				{
					SetIsPlayingStreamingGameMusic(true);
					SetIsPlayingStreamingCDMusic(false);
					m_MusicType                = eMusicType_Game;
					m_StreamingAudioInfo.bIs3D = false;

					wstring &wstrSoundName = dlcAudioFile->GetSoundName(m_musicID);
					char     szName[255];
					wcstombs(szName, wstrSoundName.c_str(), 255);
					string strFile    = "TPACK:\\Data\\" + string(szName) + ".wav";
					string mountedPath = StorageManager.GetMountedPath(strFile);
					strcpy(m_szStreamName, mountedPath.c_str());
				}
				else
				{
					SetIsPlayingStreamingGameMusic(false);
					SetIsPlayingStreamingCDMusic(true);
					m_MusicType                = eMusicType_CD;
					m_StreamingAudioInfo.bIs3D = true;

					// Need to adjust to index into the cds in the game's m_szStreamFileA
					strcat((char *)m_szStreamName, "cds/");
					strcat((char *)m_szStreamName, m_szStreamFileA[m_musicID - m_iStream_CD_1 + eStream_CD_1]);
					strcat((char *)m_szStreamName, ".wav");
				}
			}
			else
			{
				if(m_musicID < m_iStream_CD_1)
				{
					SetIsPlayingStreamingGameMusic(true);
					SetIsPlayingStreamingCDMusic(false);
					m_MusicType                = eMusicType_Game;
					m_StreamingAudioInfo.bIs3D = false;
					strcat((char *)m_szStreamName, "music/");
				}
				else
				{
					SetIsPlayingStreamingGameMusic(false);
					SetIsPlayingStreamingCDMusic(true);
					m_MusicType                = eMusicType_CD;
					m_StreamingAudioInfo.bIs3D = true;
					strcat((char *)m_szStreamName, "cds/");
				}
				strcat((char *)m_szStreamName, m_szStreamFileA[m_musicID]);
				strcat((char *)m_szStreamName, ".wav");
			}

			// try alternate extensions if the default doesn't exist
			{
				FILE *pFile = nullptr;
				if(fopen_s(&pFile, m_szStreamName, "rb") == 0 && pFile)
				{
					fclose(pFile);
				}
				else
				{
					static const char *extensions[] = {".ogg", ".wav", ".mp3"};
					char *dot  = strrchr(m_szStreamName, '.');
					bool  found = false;
					if(dot)
					{
						for(int e = 0; e < 3 && !found; e++)
						{
							strcpy_s(dot, 5, extensions[e]);
							if(fopen_s(&pFile, m_szStreamName, "rb") == 0 && pFile)
							{
								fclose(pFile);
								found = true;
							}
						}
					}
					if(!found)
					{
						app.DebugPrintf("WARNING: No audio file found for music ID %d (tried .ogg, .mp3, .wav)\n", m_musicID);
						m_StreamState = eMusicStreamState_Idle;
						m_iMusicDelay = 20 * 60;
						break;
					}
				}
			}

			app.DebugPrintf("Starting streaming - %s\n", m_szStreamName);

			// Don't actually open in this thread, as it can block for ~300ms.
			m_openStreamThread = new C4JThread(OpenStreamThreadProc, this, "OpenStreamThreadProc");
			m_openStreamThread->Run();
			m_StreamState = eMusicStreamState_Opening;
		}
		break;

	case eMusicStreamState_Opening:
		// if the open stream thread is complete, then we are ready to proceed to actually playing
		if(!m_openStreamThread->isRunning())
		{
			delete m_openStreamThread;
			m_openStreamThread = NULL;

			if(!m_musicStreamActive)
			{
				m_StreamState = eMusicStreamState_Idle;
				m_iMusicDelay = 20 * 60;
				break;
			}

			if(m_StreamingAudioInfo.bIs3D)
			{
				ma_sound_set_spatialization_enabled(&m_musicStream, MA_TRUE);
				ma_sound_set_min_distance(&m_musicStream, 1.0f);
				ma_sound_set_max_distance(&m_musicStream, 64.0f);
				ma_sound_set_attenuation_model(&m_musicStream, ma_attenuation_model_linear);

				if(m_validListenerCount > 1)
				{
					float fClosest  = 10000.0f;
					float fClosestX = 0.0f, fClosestY = 0.0f, fClosestZ = 0.0f, fDist;

					for(int i = 0; i < MAX_LOCAL_PLAYERS; i++)
					{
						if(m_ListenerA[i].bValid)
						{
							float x = fabs(m_ListenerA[i].vPosition.x - m_StreamingAudioInfo.x);
							float y = fabs(m_ListenerA[i].vPosition.y - m_StreamingAudioInfo.y);
							float z = fabs(m_ListenerA[i].vPosition.z - m_StreamingAudioInfo.z);
							fDist   = x + y + z;

							if(fDist < fClosest)
							{
								fClosest  = fDist;
								fClosestX = x;
								fClosestY = y;
								fClosestZ = z;
							}
						}
					}

					float realDist = sqrtf((fClosestX * fClosestX) + (fClosestY * fClosestY) + (fClosestZ * fClosestZ));
					ma_sound_set_position(&m_musicStream, 0, 0, realDist);
				}
				else
				{
					ma_sound_set_position(&m_musicStream,
						m_StreamingAudioInfo.x,
						m_StreamingAudioInfo.y,
						-m_StreamingAudioInfo.z);
				}
			}
			else
			{
				ma_sound_set_spatialization_enabled(&m_musicStream, MA_FALSE);
			}

			ma_sound_set_pitch(&m_musicStream, m_StreamingAudioInfo.pitch);
			ma_sound_set_volume(&m_musicStream, m_StreamingAudioInfo.volume * getMasterMusicVolume());
			ma_sound_start(&m_musicStream);

			m_StreamState = eMusicStreamState_Playing;
		}
		break;

	case eMusicStreamState_OpeningCancel:
		if(!m_openStreamThread->isRunning())
		{
			delete m_openStreamThread;
			m_openStreamThread = NULL;
			m_StreamState      = eMusicStreamState_Stop;
		}
		break;

	case eMusicStreamState_Stop:
		if(m_musicStreamActive)
		{
			ma_sound_stop(&m_musicStream);
			ma_sound_uninit(&m_musicStream);
			m_musicStreamActive = false;
		}
		SetIsPlayingStreamingCDMusic(false);
		SetIsPlayingStreamingGameMusic(false);
		m_StreamState = eMusicStreamState_Idle;
		break;

	case eMusicStreamState_Stopping:
		break;

	case eMusicStreamState_Play:
		break;

	case eMusicStreamState_Playing:
		if(GetIsPlayingStreamingGameMusic())
		{
			bool playerInEnd    = false;
			bool playerInNether = false;
			Minecraft *pMinecraft = Minecraft::GetInstance();

			for(unsigned int i = 0; i < MAX_LOCAL_PLAYERS; ++i)
			{
				if(pMinecraft->localplayers[i] != NULL)
				{
					if(pMinecraft->localplayers[i]->dimension == LevelData::DIMENSION_END)
						playerInEnd = true;
					else if(pMinecraft->localplayers[i]->dimension == LevelData::DIMENSION_NETHER)
						playerInNether = true;
				}
			}

			if(playerInEnd && !GetIsPlayingEndMusic())
			{
				m_StreamState = eMusicStreamState_Stop;
				m_musicID     = getMusicID(LevelData::DIMENSION_END);
				SetIsPlayingEndMusic(true);
				SetIsPlayingNetherMusic(false);
			}
			else if(!playerInEnd && GetIsPlayingEndMusic())
			{
				m_StreamState = eMusicStreamState_Stop;
				if(playerInNether)
				{
					m_musicID = getMusicID(LevelData::DIMENSION_NETHER);
					SetIsPlayingEndMusic(false);
					SetIsPlayingNetherMusic(true);
				}
				else
				{
					m_musicID = getMusicID(LevelData::DIMENSION_OVERWORLD);
					SetIsPlayingEndMusic(false);
					SetIsPlayingNetherMusic(false);
				}
			}
			else if(playerInNether && !GetIsPlayingNetherMusic())
			{
				m_StreamState = eMusicStreamState_Stop;
				m_musicID     = getMusicID(LevelData::DIMENSION_NETHER);
				SetIsPlayingNetherMusic(true);
				SetIsPlayingEndMusic(false);
			}
			else if(!playerInNether && GetIsPlayingNetherMusic())
			{
				m_StreamState = eMusicStreamState_Stop;
				if(playerInEnd)
				{
					m_musicID = getMusicID(LevelData::DIMENSION_END);
					SetIsPlayingNetherMusic(false);
					SetIsPlayingEndMusic(true);
				}
				else
				{
					m_musicID = getMusicID(LevelData::DIMENSION_OVERWORLD);
					SetIsPlayingNetherMusic(false);
					SetIsPlayingEndMusic(false);
				}
			}

			// volume change required?
			if(m_musicStreamActive)
				ma_sound_set_volume(&m_musicStream, m_StreamingAudioInfo.volume * fMusicVol);
		}
		else
		{
			// Music disc playing - if it's a 3D stream, then set the position - we don't have any streaming audio in the world that moves, so this isn't
			// required unless we have more than one listener, and are setting the listening position to the origin and setting a fake position
			// for the sound down the z axis
			if(m_StreamingAudioInfo.bIs3D && m_validListenerCount > 1 && m_musicStreamActive)
			{
				float fClosest  = 10000.0f;
				float fClosestX = 0.0f, fClosestY = 0.0f, fClosestZ = 0.0f, fDist;

				for(int i = 0; i < MAX_LOCAL_PLAYERS; i++)
				{
					if(m_ListenerA[i].bValid)
					{
						float x = fabs(m_ListenerA[i].vPosition.x - m_StreamingAudioInfo.x);
						float y = fabs(m_ListenerA[i].vPosition.y - m_StreamingAudioInfo.y);
						float z = fabs(m_ListenerA[i].vPosition.z - m_StreamingAudioInfo.z);
						fDist   = x + y + z;

						if(fDist < fClosest)
						{
							fClosest  = fDist;
							fClosestX = x;
							fClosestY = y;
							fClosestZ = z;
						}
					}
				}

				float realDist = sqrtf((fClosestX * fClosestX) + (fClosestY * fClosestY) + (fClosestZ * fClosestZ));
				ma_sound_set_position(&m_musicStream, 0, 0, realDist);
			}
		}
		break;

	case eMusicStreamState_Completed:
		{
			// random delay of up to 3 minutes for music
			m_iMusicDelay = random->nextInt(20 * 60 * 3);

			Minecraft *pMinecraft = Minecraft::GetInstance();
			bool playerInEnd    = false;
			bool playerInNether = false;

			for(unsigned int i = 0; i < MAX_LOCAL_PLAYERS; i++)
			{
				if(pMinecraft->localplayers[i] != NULL)
				{
					if(pMinecraft->localplayers[i]->dimension == LevelData::DIMENSION_END)
						playerInEnd = true;
					else if(pMinecraft->localplayers[i]->dimension == LevelData::DIMENSION_NETHER)
						playerInNether = true;
				}
			}

			if(playerInEnd)
			{
				m_musicID = getMusicID(LevelData::DIMENSION_END);
				SetIsPlayingEndMusic(true);
				SetIsPlayingNetherMusic(false);
			}
			else if(playerInNether)
			{
				m_musicID = getMusicID(LevelData::DIMENSION_NETHER);
				SetIsPlayingNetherMusic(true);
				SetIsPlayingEndMusic(false);
			}
			else
			{
				m_musicID = getMusicID(LevelData::DIMENSION_OVERWORLD);
				SetIsPlayingNetherMusic(false);
				SetIsPlayingEndMusic(false);
			}

			m_StreamState = eMusicStreamState_Idle;
		}
		break;
	}

	// check the status of the stream - this is for when a track completes rather than is stopped by the user action
	if(m_musicStreamActive)
	{
		if(!ma_sound_is_playing(&m_musicStream) && ma_sound_at_end(&m_musicStream))
		{
			ma_sound_uninit(&m_musicStream);
			m_musicStreamActive = false;
			SetIsPlayingStreamingCDMusic(false);
			SetIsPlayingStreamingGameMusic(false);
			m_StreamState = eMusicStreamState_Completed;
		}
	}
}

/////////////////////////////////////////////
//
//	ConvertSoundPathToName
//
/////////////////////////////////////////////
char *SoundEngine::ConvertSoundPathToName(const wstring& name, bool bConvertSpaces)
{
	static char buf[256];
	assert(name.length() < 256);
	for(unsigned int i = 0; i < name.length(); i++)
	{
		wchar_t c = name[i];
		if(c == '.') c = '/';
		if(bConvertSpaces && c == ' ') c = '_';
		buf[i] = (char)c;
	}
	buf[name.length()] = 0;
	return buf;
}
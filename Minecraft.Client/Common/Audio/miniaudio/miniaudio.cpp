// @3UR: keep this! 
// dont add any of this to SoundEngine.h precompiled headers fucks us up and will complain all of the miniaudio stuff is already defined...
#include "Common/Audio/miniaudio/stb_vorbis.h"

#define MA_NO_DSOUND
#define MA_NO_WINMM
#define MINIAUDIO_IMPLEMENTATION

#include "Common/Audio/miniaudio/miniaudio.h"
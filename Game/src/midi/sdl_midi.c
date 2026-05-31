//
//  sdl_midi.c
//  Duke3D
//
//  Created by fabien sanglard on 12-12-15.
//  Copyright (c) 2012 fabien sanglard. All rights reserved.
//
//  SDL3 port: SDL_mixer 3.x dropped the legacy Mix_* API (Mix_PlayMusic, etc.)
//  in favour of an all-new MIX_* API (MIX_Mixer / MIX_Track / MIX_Audio). The
//  music here is MIDI, so it is rendered through SDL_mixer's FluidSynth (or
//  TiMidity) backend. FluidSynth needs a SoundFont; we look one up from the
//  DUKE_SOUNDFONT environment variable first, then a few well-known locations.
//  Set DUKE_SOUNDFONT=/path/to/file.sf2 (or TIMIDITY_CFG for the TiMidity
//  backend) if music is silent on your system.
//

#include <stdio.h>
#include "../audiolib/music.h"
#include "SDL.h"
#include "SDL_mixer.h"
#include "build.h"

/*
 Because the music is stored in a GRP file that is never fully loaded in RAM
 (the full version of Duke Nukem 3D is a 43MB GRP) we need to extract the music
 from it and store it in RAM.
*/
#define KILOBYTE (1024*1024)
uint8_t musicDataBuffer[100 * KILOBYTE];

/* Defined in SDL_mixer's internal header; it is a stable public property name. */
#define DUKE_FLUIDSYNTH_SOUNDFONT_PATH "SDL_mixer.decoder.fluidsynth.soundfont_path"

static MIX_Mixer *musicMixer = NULL;
static MIX_Track *musicTrack = NULL;
static MIX_Audio *musicAudio = NULL;
static const char *soundfontPath = NULL;
static float musicGain = 1.0f;

char  *MUSIC_ErrorString(int ErrorNumber)
{
	return "";
}

/* Find a General-MIDI SoundFont for the FluidSynth backend. */
static const char *find_soundfont(void)
{
    static const char *candidates[] = {
        "/opt/homebrew/share/fluid-synth/sf2/VintageDreamsWaves-v2.sf2",
        "/opt/homebrew/Cellar/fluid-synth/2.5.4/share/fluid-synth/sf2/VintageDreamsWaves-v2.sf2",
        "/usr/local/share/fluid-synth/sf2/VintageDreamsWaves-v2.sf2",
        "/usr/share/sounds/sf2/FluidR3_GM.sf2",
        "/usr/share/sounds/sf2/default-GM.sf2",
        "/usr/share/soundfonts/default.sf2",
    };
    const char *env = SDL_getenv("DUKE_SOUNDFONT");
    int i;

    if (env && *env)
        return env;

    for (i = 0; i < (int)SDL_arraysize(candidates); i++) {
        SDL_PathInfo info;
        if (SDL_GetPathInfo(candidates[i], &info))
            return candidates[i];
    }
    return NULL;
}

int MUSIC_Init(int SoundCard, int Address)
{
    if (!MIX_Init()) {
        printf("MIX_Init failed: %s\n", SDL_GetError());
        return MUSIC_Ok;   /* run silently rather than abort the game */
    }

    musicMixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (musicMixer == NULL) {
        printf("MIX_CreateMixerDevice failed: %s\n", SDL_GetError());
        return MUSIC_Ok;
    }

    musicTrack = MIX_CreateTrack(musicMixer);

    soundfontPath = find_soundfont();
    if (soundfontPath)
        printf("MIDI: using SoundFont '%s'.\n", soundfontPath);
    else
        printf("MIDI: no SoundFont found; music may be silent. "
               "Set DUKE_SOUNDFONT=/path/to/file.sf2\n");

    return MUSIC_Ok;
}

int MUSIC_Shutdown(void)
{
	MUSIC_StopSong();
    if (musicTrack) { MIX_DestroyTrack(musicTrack); musicTrack = NULL; }
    if (musicMixer) { MIX_DestroyMixer(musicMixer); musicMixer = NULL; }
    MIX_Quit();
    return(MUSIC_Ok);
}

void MUSIC_SetMaxFMMidiChannel(int channel)
{
}

void MUSIC_SetVolume(int volume)
{
    /* Duke passes roughly 0..255; MIX gain is a 0..1 float. */
    musicGain = volume / 255.0f;
    if (musicGain < 0.0f) musicGain = 0.0f;
    if (musicGain > 1.0f) musicGain = 1.0f;
    if (musicTrack)
        MIX_SetTrackGain(musicTrack, musicGain);
}

void MUSIC_SetMidiChannelVolume(int channel, int volume)
{
}

void MUSIC_ResetMidiChannelVolumes(void)
{
}

int MUSIC_GetVolume(void)
{
	return (int)(musicGain * 255.0f);
}

void MUSIC_SetLoopFlag(int loopflag)
{
}

int MUSIC_SongPlaying(void)
{
	return (musicTrack && MIX_TrackPlaying(musicTrack)) ? 1 : 0;
}

void MUSIC_Continue(void)
{
    if (musicTrack)
        MIX_ResumeTrack(musicTrack);
}

void MUSIC_Pause(void)
{
    if (musicTrack)
        MIX_PauseTrack(musicTrack);
}

int MUSIC_StopSong(void)
{
    if (musicTrack)
        MIX_StopTrack(musicTrack, 0);
    if (musicAudio) {
        MIX_DestroyAudio(musicAudio);
        musicAudio = NULL;
    }
    return(MUSIC_Ok);
}



int MUSIC_PlaySong(char  *songFilename, int loopflag)
{
    int32_t fd =  0;
    int fileSize;
    SDL_IOStream *rw;
    SDL_PropertiesID loadProps, playProps;

    if (musicMixer == NULL || musicTrack == NULL)
        return 0;   /* music subsystem unavailable */

    fd = kopen4load(songFilename,0);

	if(fd == -1){
        printf("The music '%s' cannot be found in the GRP or the filesystem.\n",songFilename);
        return 0;
    }

    fileSize = kfilelength( fd );
    if(fileSize >= sizeof(musicDataBuffer))
    {
        printf("The music '%s' was found but is too big (%dKB)to fit in the buffer (%luKB).\n",songFilename,fileSize/1024,sizeof(musicDataBuffer)/1024);
        kclose(fd);
        return 0;
    }

    /* Stop and free any currently playing song before we overwrite the buffer
     * the streaming decoder reads from. */
    MUSIC_StopSong();

    kread( fd, musicDataBuffer, fileSize);
    kclose( fd );

    /* Wrap the in-memory MIDI and load it, telling FluidSynth which SoundFont
     * to use. */
    rw = SDL_IOFromMem((void *) musicDataBuffer, fileSize);
    if (rw == NULL)
        return 0;

    loadProps = SDL_CreateProperties();
    SDL_SetPointerProperty(loadProps, MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, rw);
    SDL_SetBooleanProperty(loadProps, MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, true);
    if (soundfontPath)
        SDL_SetStringProperty(loadProps, DUKE_FLUIDSYNTH_SOUNDFONT_PATH, soundfontPath);

    musicAudio = MIX_LoadAudioWithProperties(loadProps);
    SDL_DestroyProperties(loadProps);

    if (musicAudio == NULL) {
        printf("Failed to load music '%s': %s\n", songFilename, SDL_GetError());
        SDL_CloseIO(rw);
        return 0;
    }

    MIX_SetTrackAudio(musicTrack, musicAudio);
    MIX_SetTrackGain(musicTrack, musicGain);

    /* loopflag == MUSIC_PlayOnce plays once; otherwise loop forever (-1). */
    playProps = SDL_CreateProperties();
    SDL_SetNumberProperty(playProps, MIX_PROP_PLAY_LOOPS_NUMBER,
                          (loopflag == MUSIC_PlayOnce) ? 0 : -1);
    MIX_PlayTrack(musicTrack, playProps);
    SDL_DestroyProperties(playProps);

    return 1;
}


void MUSIC_SetContext(int context)
{
}

int MUSIC_GetContext(void)
{
	return 0;
}

void MUSIC_SetSongTick(uint32_t PositionInTicks)
{
}

void MUSIC_SetSongTime(uint32_t milliseconds)
{
}

void MUSIC_SetSongPosition(int measure, int beat, int tick)
{
}

void MUSIC_GetSongPosition(songposition *pos)
{
}

void MUSIC_GetSongLength(songposition *pos)
{
}

int MUSIC_FadeVolume(int tovolume, int milliseconds)
{
	return(MUSIC_Ok);
}

int MUSIC_FadeActive(void)
{
	return 0;
}

void MUSIC_StopFade(void)

{
}

void MUSIC_RerouteMidiChannel(int channel, int cdecl function( int event, int c1, int c2 ))
{
}

void MUSIC_RegisterTimbreBank(uint8_t  *timbres)
{
}

// This is the method called from the Game Module.
void PlayMusic(char  *fileName){
    MUSIC_PlaySong(fileName,1);
}

#include <stdlib.h>
#include <string.h>

#include "dsl.h"
#include "util.h"

#include "SDL.h"

/*
 * SDL3 port: the original used SDL_mixer's effect callback to feed the device.
 * SDL_mixer 3.x dropped that legacy API entirely, so we now talk to the SDL3
 * core audio API directly. MultiVoc already does all the software mixing into
 * `_BufferStart`; here we just stream finished mix pages to the audio device.
 */

extern volatile int MV_MixPage;

static int DSL_ErrorCode = DSL_Ok;

static int mixer_initialized;

static void ( *_CallBackFunc )( void );
static volatile char *_BufferStart;
static int _BufferSize;
static int _NumDivisions;
static int _SampleRate;
static int _remainder;

static SDL_AudioStream *audioStream = NULL;

/*
possible todo ideas: cache sdl/sdl mixer error messages.
*/

char *DSL_ErrorString( int ErrorNumber )
{
	char *ErrorString;
	
	switch (ErrorNumber) {
		case DSL_Warning:
		case DSL_Error:
			ErrorString = DSL_ErrorString(DSL_ErrorCode);
			break;
		
		case DSL_Ok:
			ErrorString = "SDL Driver ok.";
			break;
		
		case DSL_SDLInitFailure:
			ErrorString = "SDL Audio initialization failed.";
			break;
		
		case DSL_MixerActive:
			ErrorString = "SDL Mixer already initialized.";
			break;	
	
		case DSL_MixerInitFailure:
			ErrorString = "SDL Mixer initialization failed.";
			break;
			
		default:
			ErrorString = "Unknown SDL Driver error.";
			break;
	}
	
	return ErrorString;
}

static void DSL_SetErrorCode(int ErrorCode)
{
	DSL_ErrorCode = ErrorCode;
}

int DSL_Init( void )
{
	DSL_SetErrorCode(DSL_Ok);
	
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		DSL_SetErrorCode(DSL_SDLInitFailure);
		
		return DSL_Error;
	}
	
	return DSL_Ok;
}

void DSL_Shutdown( void )
{
	DSL_StopPlayback();
}

/*
 * SDL3 audio stream callback. SDL asks us for `additional_amount` more bytes;
 * we satisfy it one finished MultiVoc mix page at a time. SDL_AudioStream
 * queues whatever we hand it, so we no longer need the old partial-page
 * (`_remainder`) bookkeeping the SDL_mixer effect callback required.
 */
static void SDLCALL audio_callback(void *udata, SDL_AudioStream *stream,
                                   int additional_amount, int total_amount)
{
	(void)udata; (void)total_amount;

	while (additional_amount > 0) {
		Uint8 *fxptr;

		/* ask MultiVoc to mix the next page, then stream it out */
		_CallBackFunc();
		fxptr = (Uint8 *)(&_BufferStart[MV_MixPage * _BufferSize]);

		SDL_PutAudioStreamData(stream, fxptr, _BufferSize);
		additional_amount -= _BufferSize;
	}
}

int   DSL_BeginBufferedPlayback( char *BufferStart,
      int BufferSize, int NumDivisions, unsigned SampleRate,
      int MixMode, void ( *CallBackFunc )( void ) )
{
	SDL_AudioFormat format;
	SDL_AudioSpec spec;
	int channels;
	int chunksize;
	int blah;
		
	if (mixer_initialized) {
		DSL_SetErrorCode(DSL_MixerActive);
		
		return DSL_Error;
	}
	
	_CallBackFunc = CallBackFunc;
	_BufferStart = BufferStart;
	_BufferSize = (BufferSize / NumDivisions);
	_NumDivisions = NumDivisions;
	_SampleRate = SampleRate;

	_remainder = 0;
	
	format = (MixMode & SIXTEEN_BIT) ? SDL_AUDIO_S16LE : SDL_AUDIO_U8;
	channels = (MixMode & STEREO) ? 2 : 1;

 /*
	I find 50ms to be ideal, at least with my hardware. This clamping mechanism
	was added because it seems the above remainder handling isn't so nice --kode54
 */
	chunksize = (5 * SampleRate) / 100;

	blah = _BufferSize;
	if (MixMode & SIXTEEN_BIT) blah >>= 1;
	if (MixMode & STEREO) blah >>= 1;

	if (chunksize % blah) chunksize += blah - (chunksize % blah);

	(void)chunksize;  /* SDL3 audio streams buffer for us; no chunk size needed */

	SDL_zero(spec);
	spec.freq = SampleRate;
	spec.format = format;
	spec.channels = channels;

	/* Opens the default playback device and binds a stream to it. SDL calls
	 * audio_callback whenever it needs more samples. */
	audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
	                                        &spec, audio_callback, NULL);
	if (audioStream == NULL) {
		DSL_SetErrorCode(DSL_MixerInitFailure);

		return DSL_Error;
	}

	SDL_ResumeAudioStreamDevice(audioStream);

	mixer_initialized = 1;

	return DSL_Ok;
}

void DSL_StopPlayback( void )
{
	if (audioStream != NULL) {
		/* Also closes the audio device opened by SDL_OpenAudioDeviceStream. */
		SDL_DestroyAudioStream(audioStream);
		audioStream = NULL;
	}

	mixer_initialized = 0;
}

unsigned DSL_GetPlaybackRate( void )
{
	return _SampleRate;
}

uint32_t DisableInterrupts( void )
{
	return 0;
}

void RestoreInterrupts( uint32_t flags )
{
}

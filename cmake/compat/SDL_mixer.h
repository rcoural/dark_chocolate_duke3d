/*
 * Compatibility shim: redirect the original "SDL_mixer.h" include to the
 * SDL3_mixer location <SDL3_mixer/SDL_mixer.h>.
 */
#ifndef CHOCO_SDL_COMPAT_SDL_MIXER_H
#define CHOCO_SDL_COMPAT_SDL_MIXER_H
#include <SDL3_mixer/SDL_mixer.h>
#endif

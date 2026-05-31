/*
 * Compatibility shim: the original Chocolate Duke3D sources include the
 * SDL 1.2 style header <SDL.h> / "SDL.h". SDL3 installs its umbrella header
 * as <SDL3/SDL.h>. This one-line redirect lets the original include lines
 * stay untouched while we build against SDL3.
 */
#ifndef CHOCO_SDL_COMPAT_SDL_H
#define CHOCO_SDL_COMPAT_SDL_H
#include <SDL3/SDL.h>
#endif

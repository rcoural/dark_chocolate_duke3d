cmake_minimum_required(VERSION 3.24)
project(chocolate_duke3d C CXX)

# -----------------------------------------------------------------------------
# Chocolate Duke3D - Apple Silicon / SDL3 port
#
# Faithful port of Fabien Sanglard's chocolate_duke3D, brought up to:
#   * a CMake build (replacing the old autotools / Xcode 4 projects)
#   * SDL3 + SDL3_mixer (replacing SDL 1.2)
#   * 64-bit arm64 (Apple Silicon) and x86_64
#
# SDL3 and SDL3_mixer are pulled via FetchContent, so no system SDL install
# is required. The original `#include "SDL.h"` / `#include "SDL_mixer.h"`
# lines are kept verbatim and redirected by the shims in cmake/compat.
# -----------------------------------------------------------------------------

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# ---- SDL3 -------------------------------------------------------------------
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON  CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.8
    GIT_SHALLOW    TRUE
)

# ---- SDL3_mixer -------------------------------------------------------------
# SDL_mixer 3.x is a complete API redesign (MIX_Mixer/MIX_Track/...), not a port
# of the SDL2_mixer Mix_* API. Sound EFFECTS go through the SDL3 core audio API
# directly (audiolib/dsl.c); SDL3_mixer is used only for MIDI MUSIC (via its
# FluidSynth/TiMidity backends). Keep the build light: no extra codecs needed.
set(SDLMIXER_SAMPLES  OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLMIXER_FLAC     OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MP3      OFF CACHE BOOL "" FORCE)
set(SDLMIXER_OPUS     OFF CACHE BOOL "" FORCE)
set(SDLMIXER_WAVPACK  OFF CACHE BOOL "" FORCE)
set(SDLMIXER_GME      OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MOD      OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)  # not needed; avoids a libvorbisfile dlopen
# Link FluidSynth directly instead of dlopen()ing "libfluidsynth.3.dylib": on
# Homebrew the dylib has an absolute install_name, so the dynamic linker finds
# it at runtime with no DYLD_LIBRARY_PATH. (dlopen of a bare leaf name does NOT
# search /opt/homebrew/lib, which is why MIDI was silent otherwise.)
set(SDLMIXER_MIDI_FLUIDSYNTH_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG        release-3.2.2
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(SDL3 SDL3_mixer)

# -----------------------------------------------------------------------------
# Source lists (mirrors the original Makefile.am files)
# -----------------------------------------------------------------------------
set(ENGINE_SRC
    Engine/src/cache.c
    Engine/src/display.c
    Engine/src/draw.c
    Engine/src/dummy_multi.c
    Engine/src/engine.c
    Engine/src/filesystem.c
    Engine/src/fixedPoint_math.c
    Engine/src/mmulti.c
    Engine/src/network.c
    Engine/src/tiles.c
)

set(GAME_SRC
    Game/src/actors.c
    Game/src/animlib.c
    Game/src/config.c
    Game/src/console.c
    Game/src/control.c
    Game/src/cvar_defs.c
    Game/src/cvars.c
    Game/src/dummy_audiolib.c
    Game/src/game.c
    Game/src/gamedef.c
    Game/src/global.c
    Game/src/keyboard.c
    Game/src/menues.c
    Game/src/player.c
    Game/src/premap.c
    Game/src/rts.c
    Game/src/scriplib.c
    Game/src/sector.c
    Game/src/sounds.c
)

set(AUDIO_SRC
    Game/src/audiolib/fx_man.c
    Game/src/audiolib/dsl.c
    Game/src/audiolib/ll_man.c
    Game/src/audiolib/multivoc.c
    Game/src/audiolib/mv_mix.c
    Game/src/audiolib/mvreverb.c
    Game/src/audiolib/nodpmi.c
    Game/src/audiolib/pitch.c
    Game/src/audiolib/user.c
    Game/src/audiolib/usrhooks.c
)

set(MIDI_SRC
    Game/src/midi/sdl_midi.c
)

# Build as separate static libraries, exactly like the original autotools build
# (libEngine.a / libaudio.a / libmidi.a + the Game objects). This matters: the
# Engine lib contains BOTH dummy_multi.c and mmulti.c (which define the same
# networking globals), and sounds.c duplicates USRHOOKS_* from audiolib. Static
# archive linking pulls only the first definition of each symbol, so the unused
# duplicates (mmulti.o, usrhooks.o) are simply never loaded. A single flat
# executable would instead see them as duplicate-symbol link errors.
add_library(duke_engine STATIC ${ENGINE_SRC})
add_library(duke_audio  STATIC ${AUDIO_SRC})
add_library(duke_midi   STATIC ${MIDI_SRC})

# The Game sources (including game.c's main() and sounds.c) live directly in the
# executable so they dominate the archives the way libGame.a did in the original.
add_executable(chocolate-duke3d ${GAME_SRC})

# The original code is Watcom/DOS-era C with a lot of intentional sloppiness.
# Silence the noise rather than touch every line; -Wno-unknown keeps the
# GCC-only flags from the old configure.ac from breaking clang.
set(CHOCO_C_FLAGS
    -Wno-unknown-warning-option
    -Wno-pointer-sign
    -Wno-int-conversion
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
    -Wno-parentheses
    -Wno-unused-function
    -Wno-unused-but-set-variable
    -Wno-unused-result
    -Wno-incompatible-pointer-types
    -Wno-incompatible-function-pointer-types
    -fno-strict-aliasing
)

# NOTE: deliberately do NOT add Game/src/audiolib to the include path — it ships
# its own assert.h that would shadow the system <assert.h>. The audiolib sources
# reference siblings via the "audiolib/" prefix (from Game/src) or same-directory
# quoted includes, so it is unnecessary anyway. Linking SDL3::SDL3 to every
# target is just to propagate the SDL3 headers at compile time.
foreach(t duke_engine duke_audio duke_midi chocolate-duke3d)
    target_include_directories(${t} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compat   # SDL.h / SDL_mixer.h redirects
        ${CMAKE_CURRENT_SOURCE_DIR}/Engine/src
        ${CMAKE_CURRENT_SOURCE_DIR}/Game/src)
    target_compile_options(${t} PRIVATE ${CHOCO_C_FLAGS})
    target_link_libraries(${t} PRIVATE SDL3::SDL3)
endforeach()

# Only the MIDI music layer needs SDL3_mixer (its headers + the MIX_* symbols).
target_link_libraries(duke_midi PRIVATE SDL3_mixer::SDL3_mixer)

# premap.c miscompiles with optimisation (documented in the original Makefile.am).
set_source_files_properties(Game/src/premap.c PROPERTIES COMPILE_OPTIONS "-O0")

# Link order mirrors the original LDADD (Game objects, then audio, midi, engine).
target_link_libraries(chocolate-duke3d PRIVATE duke_audio duke_midi duke_engine SDL3_mixer::SDL3_mixer SDL3::SDL3)

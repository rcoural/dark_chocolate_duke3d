Chocolate Duke3D
================

Chocolate Duke Nukem 3D is the equivalent of [Chocolate Doom](http://www.chocolate-doom.org/wiki/index.php/Chocolate_Doom).

A **port** that:

1. Remains as faithful as possible to the original source code.
2. Portable and compiling in one click on Windows, OS X and Linux.
3. Aimed at education, with lots of comments and documentation added in order to help programmers to understand and learn.


Requirements
============

[SDL](http://libsdl.org) and [SDL_mixer](http://www.libsdl.org/projects/SDL_mixer) to compile and run the code.

An original copy of [Duke Nukem 3D](https://3drealms.com/catalog/duke-nukem-3d_27/) (specifically the DUKE3D.GRP file from the original CD in binary working directory (and with rw permissions?)).

Apple Silicon / SDL3 port
=========================

This fork brings Chocolate Duke3D up to **Apple Silicon (arm64)** with a modern
toolchain:

* **CMake** build (replaces the old autotools / Xcode 4 projects).
* **SDL3** for video and input, fetched automatically via CMake `FetchContent`
  (pinned to `release-3.4.8`) — no system SDL install needed.
* Sound effects use the **SDL3 core audio API** directly (MultiVoc still does the
  software mixing).
* **MIDI music** plays through **SDL3_mixer** (fetched via CMake) using its
  FluidSynth backend, ported to the new `MIX_*` API. FluidSynth needs a
  SoundFont: the build links FluidSynth directly (so no `DYLD_LIBRARY_PATH` is
  needed) and the game auto-detects Homebrew's `VintageDreamsWaves` SoundFont.
  For better-sounding music, install a fuller General-MIDI SoundFont and point
  to it with `DUKE_SOUNDFONT=/path/to/FluidR3_GM.sf2` (or set `TIMIDITY_CFG` to
  use the TiMidity backend instead).
* 64-bit fixes: `FP_OFF` and several Build-engine framebuffer pointers no longer
  truncate on a 64-bit address space.

Build on macOS (Apple Silicon or Intel):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run it from a directory containing your `DUKE3D.GRP`:

```sh
cd /path/to/duke3d-data && /path/to/build/chocolate-duke3d
```

The game renders at 320x200 but the window opens at 3x (960x600) and is
resizable; the framebuffer is scaled to fit (letterboxed). Override the factor
with `DUKE_SCALE` (e.g. `DUKE_SCALE=4`), and toggle fullscreen at runtime with
**Alt+Enter** or **Ctrl+Enter**.

> **Status:** builds and boots on arm64 (loads the GRP, initialises the SDL3
> "cocoa" video driver, compiles the CON scripts). The remaining work is a
> careful 64-bit audit of the `Engine/src/draw.c` software rasteriser, whose
> x86-register-emulation helpers still stash palette/texture/framebuffer
> pointers in 32-bit integers.

Original build systems (upstream)
---------------------------------

* **Linux**: Use [Autoconf/Automake](https://www.gnu.org/software/autoconf/manual/autoconf.html#Basic-Installation)
* **Windows**: Use Visual Studio 2005 or Visual Studio 2012 or [Autoconf/Automake](https://www.gnu.org/software/autoconf/manual/autoconf.html#Basic-Installation)
* **OS X**: Use Xcode 4.0


Contributors
============

* **Project Initiator:** [Fabien Sanglard](https://github.com/fabiensanglard)
* **Linux Integration:** [Juan Manuel Borges Caño](https://github.com/juanmabc)
* **Autoconf/Automake Build System:** [darealshinji](https://github.com/darealshinji)


More Information
================

* **[Review of the Duke 3D source code](http://fabiensanglard.net/duke3d/)**
* [Simple DirectMedia Layer](https://wiki.libsdl.org/FrontPage), [SDL_Mixer](http://www.libsdl.org/projects/SDL_mixer/)
* [Duke Nukem 3D](https://3drealms.com/catalog/duke-nukem-3d_27/)

# Changelog

All notable changes to this fork are documented in this file.
This project follows [Semantic Versioning](https://semver.org/) (`0.x` = early,
in-development releases).

## [0.1.0] - 2026-05-31

Initial **SDL3 / Apple Silicon (arm64)** port of Fabien Sanglard's
[chocolate_duke3D](https://github.com/fabiensanglard/chocolate_duke3D).

### Added
- New **CMake** build that fetches **SDL3** (and **SDL3_mixer**) via
  `FetchContent` — no system SDL install required.
- Resizable window: renders at 320x200 and scales to the window (3x by default,
  override with the `DUKE_SCALE` environment variable). **Alt+Enter** toggles
  fullscreen.
- Headless framebuffer capture for testing via the `DUKE_AUTOSHOT` environment
  variable (writes a BMP of the current frame).

### Changed
- Ported the video, input, palette and presentation layers from **SDL 1.2 to
  SDL3** (window + renderer + streaming texture over an 8-bit indexed
  framebuffer; keyboard input via `SDL_Scancode`).
- Sound effects now use the **SDL3 core audio API** directly
  (`SDL_OpenAudioDeviceStream`); MultiVoc still performs the software mixing.
- **MIDI music** ported to SDL3_mixer's new `MIX_*` API and rendered with
  **FluidSynth**. FluidSynth is linked directly (so no `DYLD_LIBRARY_PATH` is
  needed); the SoundFont is auto-detected or set via `DUKE_SOUNDFONT`.
- **64-bit (arm64) correctness** fixes across the renderer, the CON script VM
  and the gameplay code (`FP_OFF` and framebuffer / palette / script /
  `temp_data` pointers now use `intptr_t`).

### Removed
- Dead DOS-era sources (audiolib hardware drivers, unused multiplayer code).
- Superseded autotools / Visual Studio / Xcode build files (replaced by CMake).

### Known limitations
- Interactive play (keyboard/mouse) and the full game content have not been
  thoroughly verified; some untested code paths may still hit 64-bit issues.
- MIDI playback requires a SoundFont; audio quality depends on the one used.

[0.1.0]: https://github.com/rcoural/dark_chocolate_duke3d/releases/tag/v0.1.0

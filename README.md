# SDLDoom-SDL3

A modernized fork of **SDLDoom 1.10** — Sam Lantinga's SDL port of id Software's
Linux DOOM (the Jan 10 1997 source drop) — brought up to **SDL 3**, made
**64-bit clean**, and given a **true hi-resolution software renderer** with
in-game Video and Keys menus.

This continues where the long-dormant `sdldoom-1.10-mod` left off.

![SDLDoom-SDL3 running DOOM II at 1280×960 hi-res](docs/screenshot.png)

## What's new vs. the original SDLDoom

- **SDL 1.x → SDL 3.** Window/renderer/streaming-texture display, `SDL_AudioStream`
  audio, `SDL_MAIN_HANDLED` startup. The SDL-specific code stays confined to the
  `i_*` modules.
- **64-bit clean.** Builds and runs as a Win64 binary (MinGW-w64) and natively on
  64-bit Linux. Fixed the classic vanilla-DOOM 32-bit pointer/`int` truncation
  traps (see `CLAUDE.md` for the gory details).
- **True hi-res internal rendering.** The 3D view renders at a real higher
  internal resolution (1×–4× of 320×200), not an upscale. All 2D HUD/menu drawing
  stays authored in 320×200 and is scaled up.
- **In-game Video menu** — resolution, aspect ratio, fullscreen toggle, all
  persisted across runs.
- **In-game Keys menu** — rebind the keyboard controls.
- **Always-Run toggle** — the Run key now toggles persistent run instead of
  needing to be held.
- **Mouse grab** in windowed mode (relative-motion turning; releases on alt-tab).

## Building

### Windows (MinGW-w64 + SDL3 SDK)

Point `SDL3` at the SDL3 SDK (headers in `$SDL3/include/SDL3`, import lib + DLL in
`$SDL3/lib/<arch>`), then:

```sh
./build.sh        # SDL3=../SDL3 ARCH=x64 by default; produces doom.exe + copies SDL3.dll
```

### Linux (native, SDL3 from the distro)

Install SDL3 development files (e.g. `libsdl3-dev`), then:

```sh
gcc -O2 -o doom *.c $(pkg-config --cflags --libs sdl3) -DSDL_MAIN_HANDLED \
    -Dalloca=__builtin_alloca \
    -Wno-error=implicit-function-declaration -Wno-error=implicit-int \
    -Wno-error=int-conversion -Wno-error=incompatible-pointer-types -lm
```

(The `-Wno-error=` flags relax K&R-isms in the vintage id source that modern gcc
otherwise rejects; `-Dalloca` papers over a glibc/MinGW declaration difference.)

## Running

DOOM needs an **IWAD** (`doom2.wad`, `doom.wad`, `doom1.wad`, …). None ships here —
the game data is commercial and is **not** distributed in this repository. Provide
your own copy:

```sh
./doom -iwad /path/to/doom2.wad
# or set DOOMWADDIR, or drop the WAD next to the binary
```

Useful flags: `-fullscreen`, `-warp <map>`, `-skill N`, `-render N` / `-aspect`,
`-nograbmouse`, `-file <pwad>`. See `CLAUDE.md` and `README.b` for more.

## License

The engine is id Software's DOOM source under the **DOOM Source Code License**
(`DOOMLIC.TXT`). The SDL port layer follows the original SDLDoom terms
(`README.SDL`). No artwork, levels, or IWADs are included.

See `README.b` for the original Linux DOOM drop notes and what was stripped, and
`CLAUDE.md` for an architecture overview and the porting notes.

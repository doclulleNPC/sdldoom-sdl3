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
- **Optional HD / mod assets** — terrain-dependent footstep sounds, a truecolor
  ("fullcolor") 3D view, and HD sprite / wall-texture replacements. Each is an
  opt-in toggle under **Options → Mod**, persisted across runs. The assets are
  built from third-party mod source — see [Optional HD / mod assets](#optional-hd--mod-assets).

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

## Optional HD / mod assets

The footstep, fullcolor and HD sprite/texture features are **opt-in** (enable
them under **Options → Mod**). They depend on asset WADs that are **not**
distributed here — they are generated from third-party GZDoom mod packs whose
content is under the mod authors' own terms. Both the source `.pk3`s and the
built `.wad`s are gitignored; the game loads the `.wad`s from the working
directory (i.e. next to `doom.exe`, the `run/` folder).

The generator scripts live in `tools_footsteps/` and expect the source packs in
`run/`. They require **Python 3**; the footsteps generator additionally needs
**`ffmpeg`** on `PATH` (it transcodes the source audio to DMX format).

| Feature      | Source (see links below)        | Place in `run/` as                | Build with                                  | Produces            |
|--------------|---------------------------------|-----------------------------------|---------------------------------------------|---------------------|
| Footsteps    | zk-resources (DaZombieKiller)   | `footsteps.pk3`                   | `python tools_footsteps/gen_footsteps.py`   | `run/footsteps.wad` (and regenerates `footstep_tables.h`) |
| HD textures  | DHTP (KuriKai)                  | `hd_textures.pk3`                 | `python tools_footsteps/gen_hdtextures.py`  | `run/hdtextures.wad` |
| HD sprites   | HD weapons / items (Marcelus)   | `hd_weapons.pk3`, `hd_items.pk3`  | `python tools_footsteps/gen_hdsprites.py`   | `run/hdsprites.wad`  |

Sources:

- **Footsteps** — [DaZombieKiller/zk-resources](https://github.com/DaZombieKiller/zk-resources).
- **HD textures** — based on the DHTP (Doom High-resolution Texture Project) by
  KuriKai: [github.com/KuriKai/DHTP](https://github.com/KuriKai/DHTP/). The exact
  pack used here is the pk3 mirrored on the Wad Archive:
  [a9555fd5…pk3.gz](https://archive.org/download/wadarchive/DATA/a9.zip/a9%2F555fd5230d6010a408927837fcfcd6b3ae1eb8%2Fa9555fd5230d6010a408927837fcfcd6b3ae1eb8.pk3.gz)
  (decompress the `.gz` to get the `.pk3`).
- **HD sprites** — HD weapon and item sprites by **Marcelus**. _(TODO: add the
  download URL — the link didn't come through.)_

Notes:

- **Footsteps** also regenerates the in-tree `footstep_tables.h` (flat→terrain
  map and per-terrain sound variants) from the pack's `sndinfo.txt` /
  `language.txt`, so rerun it if you update the source pack.
- The scripts apply the relevant GZDoom filter precedence (`filter/doom` then
  `filter/doom.doom2`) and only pack DOOM II–relevant, ≤8-char lump names.
- Once the `.wad`s are in `run/`, launch the game and enable each feature in
  **Options → Mod**; the toggles are saved to `.doomrc`.

## License

The engine is id Software's DOOM source under the **DOOM Source Code License**
(`DOOMLIC.TXT`). The SDL port layer follows the original SDLDoom terms
(`README.SDL`). No artwork, levels, or IWADs are included.

See `README.b` for the original Linux DOOM drop notes and what was stripped, and
`CLAUDE.md` for an architecture overview and the porting notes.

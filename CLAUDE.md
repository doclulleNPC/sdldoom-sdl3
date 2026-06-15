# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

SDLDoom 1.10 — originally Sam Lantinga's port of id Software's Linux DOOM (the Jan 10 1997 "Boom"/`b.` source drop) to SDL. The engine logic is the original id source; the system layer is confined almost entirely to the `i_*` (interface/system) modules.

**This tree has been ported from SDL 1.x to SDL 3 and made 64-bit clean** (it builds and runs as a Win64 binary with MinGW-w64). The SDL1→SDL3 work lives in `i_video.c` (window/renderer/streaming-texture instead of a palettized display surface), `i_sound.c` (`SDL_AudioStream` callback instead of the removed `SDL_OpenAudio`), `i_system.c`, and `i_main.c` (`SDL_MAIN_HANDLED`). `i_net.c` was reduced to a portable single-player stub (the old BSD-socket multiplayer is not supported on this build).

Requires a DOOM IWAD (`doom2.wad`, `doom.wad`, `doom1.wad`, …) at runtime — no artwork ships here. The registered/commercial detection and the disabling of homebrew (non-id) maps are intentionally left in the source for licensing reasons (see `README.b`).

## Build

Current build: MinGW-w64 gcc + the prebuilt SDL3 SDK (headers under `$SDL3/include/SDL3`, import lib/DLL under `$SDL3/lib/<arch>`):

```sh
./build.sh                 # SDL3=../SDL3 ARCH=x64 by default; produces doom.exe + copies SDL3.dll
```

which is just:

```sh
gcc -O2 -o doom.exe *.c -I../SDL3/include -DSDL_MAIN_HANDLED ../SDL3/lib/x64/SDL3.lib -lm
```

There is no test suite, no linter, and no CI — "running it" means launching `doom.exe` against an IWAD. `SDL3.dll` must sit next to the exe (build.sh copies it).

The legacy Unix autotools build (`configure.in`, `Makefile.am`, SDL 1.x via `AM_PATH_SDL`) is left in place but is **stale** — it predates the SDL3 port and won't link against SDL3.

### Variable internal resolution (hi-res renderer)

The engine renders the 3D view at a true higher internal resolution (not an
upscale of 320x200). Key design:
- `SCREENWIDTH`/`SCREENHEIGHT` are **runtime variables** (in `doomdef.c`), equal
  to `BASE_WIDTH*hires` x `BASE_HEIGHT*hires`. `hires` is the integer scale
  (1..4). Renderer static tables are sized for `MAXWIDTH`x`MAXHEIGHT`.
- **All 2D drawing is authored in 320x200 (`BASE_*`) coordinates** and scaled up
  by `hires` inside the `V_*` functions (`v_video.c`: `V_DrawPatch`,
  `V_DrawPatchFlipped`, `V_CopyRect`). The 3D view, automap and screen wipe draw
  natively at `SCREENWIDTH`/`SCREENHEIGHT`. So when editing HUD/menu/status-bar/
  intermission/finale code, positions must use `BASE_WIDTH`/`BASE_HEIGHT`, not
  `SCREENWIDTH`/`SCREENHEIGHT`. The view-border code (`r_draw.c`) is the one place
  that divides its (hi-res) view rect back to base coords before calling `V_*`.
- `V_SetRes(scale)` (`i_video.c`) changes resolution at runtime: updates the
  globals, recreates the SDL texture, reallocates the status bar buffer
  (`ST_SetRes`), and flags a renderer rebuild via `R_SetViewSize` (`D_Display`
  then repaints at the new size). Reached from the menu: **Options -> Video**
  (`m_menu.c`, `M_DrawVideo`/`M_VideoRes`/`M_VideoAspect`), or `-render N` /
  `-aspect` at startup. Aspect ratio is purely an SDL window-shape choice
  (`screen_aspect`, `I_ApplyAspect`); the frame is stretched to fill the window.

### 64-bit porting notes (important when touching old DOOM code)

Vanilla DOOM assumes 32-bit (ILP32) and stores pointers in `int`/aligns via `(int)`. On Win64 (LLP64: `long` is 32-bit, pointers 64-bit) several idioms are silent corruption — fixed here, watch for more:
- Pointer arrays allocated as `Z_Malloc(count*4)` underallocate (pointer = 8 bytes) → heap overflow. Use `count*sizeof(*ptr)`.
- On-disk WAD structs must keep their 32-bit field widths. `maptexture_t.columndirectory` was `void**` (8 bytes) and shifted every following field — it must be a 4-byte `int`. The historic `boolean` (int, 4 bytes) is also load-bearing for these layouts, which is why `doomtype.h` keeps `typedef int boolean` rather than 1-byte `bool`.
- Pointer↔int alignment math (`(int)p + 255 & ~255`) truncates — use `uintptr_t`.
- `lumpinfo_t.handle` holds a `FILE*`, not an `int`.
- `p_saveg.c` still archives pointers as 32-bit ints, so **savegames are not 64-bit-correct** (load/save will misbehave); normal play is unaffected. Left as a known limitation.

## Running

`main()` in `i_main.c` just stashes argv into the globals `myargc`/`myargv` and calls `D_DoomMain()` (`d_main.c`), which parses all command-line params, determines game mode, loads WADs, and enters `D_DoomLoop()`. Params are read with `M_CheckParm` (`m_argv.c`), not getopt. Useful flags:

- `-fullscreen` — added by this SDL port (`i_video.c`).
- `-iwad`/implicit IWAD search, `-file <pwad>`, `-warp <ep> <map>`, `-skill N`, `-devparm`, `-nomonsters`, `-respawn`, `-fast`, `-turbo`, `-record`/`-playdemo`, `-2`/`-3`/`-4` (screen multiply / blocky scaling in `i_video.c`).

## Architecture

The codebase follows DOOM's canonical **single-letter-prefix module convention**. The prefix tells you the subsystem; understanding the prefixes is the fastest way to navigate:

- **`d_*` — DOOM main / top level.** `d_main.c` owns startup and the master game loop (`D_DoomLoop`), which each tic calls the `*_Responder` (input), `*_Ticker` (sim advance), and `*_Drawer` (render) of every subsystem. `d_net.c` runs the tic/command buffering and (loopback) netcode. `doomdef`/`doomstat` hold global game state and enums.
- **`g_*` — game control.** `g_game.c` drives skill, episode/map progression, save/load orchestration, demo record/playback, and dispatches player ticcmds.
- **`p_*` — playsim (gameplay logic).** The largest cluster. `p_tick` runs the thinker list; `p_mobj` map objects; `p_enemy` AI; `p_map`/`p_maputl`/`p_sight` movement, collision and line-of-sight; `p_inter` damage/pickups; `p_user`/`p_pspr` player + weapon sprites; `p_spec`/`p_doors`/`p_floor`/`p_ceilng`/`p_plats`/`p_lights`/`p_switch`/`p_telept` sector special effects; `p_setup` loads a level's lumps; `p_saveg` serializes savegames. `p_local.h` is the shared internal header.
- **`r_*` — renderer (software BSP).** `r_main` frame setup; `r_bsp` walks the level BSP tree; `r_segs`/`r_plane` draw walls and flats; `r_things` sprites; `r_draw` the low-level column/span rasterizers; `r_data` texture/flat/colormap composition; `r_sky`. Output is an 8-bit 320x200 indexed framebuffer.
- **`i_*` — interface / OS layer (THE SDL PORT).** This is where almost all platform code lives and where most port work happens. `i_video.c` (SDL surface, palette, key/mouse translation, fullscreen, blocky multiply), `i_sound.c` (SDL audio mixing of SFX + music), `i_system.c` (timing `I_GetTime`, `I_Error`, zone base alloc), `i_net.c` (UDP). When fixing platform bugs, look here first — the original filenames in the `$Id$` headers (e.g. `i_x.c`) reveal these are the rewritten Linux/X equivalents.
- **`m_*` — misc / menu / math.** `m_menu` the interactive menus; `m_fixed` 16.16 fixed-point math; `m_random` the deterministic RNG table (critical: demo/net sync depends on it); `m_argv` cmdline; `m_swap` endianness byte-swapping; `m_cheat`, `m_bbox`, `m_misc` (config file, screenshots).
- **`w_*` / `z_*` — resources & memory.** `w_wad` loads WAD lumps by name/number (everything game data flows through here). `z_zone` is DOOM's custom tagged zone allocator (`Z_Malloc` with `PU_*` purge tags) — game code allocates from the zone, not raw `malloc`.
- **`v_*` — video software framebuffer** primitives (patch blitting into screens[]); distinct from `i_video` which pushes to the actual SDL display.
- **HUD/screens:** `hu_*` heads-up text/messages (`hu_lib` widgets), `st_*` status bar (`st_lib` widgets), `wi_*` intermission "you are here" screens, `f_*` finale text + screen `f_wipe` melt transition, `am_map` the automap.
- **`s_*` — sound logic** (channel allocation, distance/volume) sitting above `i_sound`. `sounds.c`/`info.c` are generated data tables (sound defs; the state/mobjinfo/sprite LUTs that *define* DOOM as a game) — see `README.b`; they were produced by id tools not included here, so edit with care.

### Cross-cutting invariants

- **Determinism for demos/netplay:** game logic advances in fixed 35Hz tics driven by ticcmds. `m_random`'s table-based RNG and 16.16 fixed-point math must stay bit-exact or demos desync. Don't "modernize" `m_fixed`/`m_random` casually (the TODO muses about floats — that would break demo compatibility).
- **All game data is in WAD lumps**, fetched via `w_wad` by lump name; there are no loose asset files.
- **Memory comes from `z_zone`** with purge tags, not the C heap — match the surrounding allocation idiom.
- Source is C with `// -*- C++ -*-` headers and id's `$Id$`/`$Log$` RCS banners. Match the existing K&R-ish brace/indentation style of the file you edit.

## Reference docs in-tree

`README.SDL` (the port note), `README.b` (original Linux DOOM drop notes, licensing, what was stripped), `README.gl`/`README.asm` (historical GL/asm notes — not built here), `TODO`/`Changelog` (id/Boom-era worklog), `DOOMLIC.TXT` (DOOM Source Code License — the governing license).

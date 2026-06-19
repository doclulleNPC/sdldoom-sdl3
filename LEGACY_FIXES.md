# Legacy fixes — bugs that stem from the age of this code

This tree is id Software's January 1997 Linux DOOM source. It was written for a
specific world: **32-bit ILP32** (pointers and `int` and `long` all 4 bytes), a
**fixed 320×200** 8-bit framebuffer, **static worst-case arrays**, a few **MB of
RAM**, and **K&R / pre-C89** habits. None of those assumptions hold on the modern
target this port runs on (64-bit LLP64, a true hi-res internal renderer, modern
multi-MB art, `-O2` compilers). Most of the non-feature bugs we've fixed are not
"mistakes" — they are *correct 1997 code* meeting a world it never anticipated.

This file catalogs those fixes so the **pattern** is recognizable: when something
breaks, ask first "is this a 1997 assumption?" The four recurring classes are
64-bit, hi-res/320×200, static limits, and modern-compiler/data exposure.

See also CLAUDE.md ("64-bit porting notes", "Variable internal resolution") for
the day-to-day rules; this file is the *why*, with commit references.

---

## A. 64-bit: ILP32 → LLP64 (pointers became 8 bytes, `long` stayed 4)

DOOM stores pointers in `int`, sizes allocations with literal `4`, and aligns
with `(int)` casts. On LLP64 every one of those is silent corruption.

- **Heap overflow from `count*4` allocations.** Pointer arrays were sized with a
  hardcoded `4` bytes/element; a pointer is 8 → under-allocation and heap
  smash. Use `count*sizeof(*p)`. *(73174aa)*
- **On-disk WAD struct width.** `maptexture_t.columndirectory` was `void**` (8
  bytes), shifting every later field and misparsing the texture lumps. It must
  be a 4-byte `int`. This is *also why `doomtype.h` keeps `typedef int boolean`*
  — the 4-byte boolean is load-bearing for on-disk struct layouts; a 1-byte
  `bool` would corrupt them. *(73174aa)*
- **Pointer↔int alignment math** (`(int)p + 255 & ~255`) truncates the high 32
  bits → use `uintptr_t`. `lumpinfo_t.handle` holds a `FILE*`, not an `int`.
  Config `default_t` string defaults use `intptr_t`. *(73174aa)*
- **Savegames (two distinct 64-bit bugs).** *Symptom:* on a 64-bit build,
  saving and reloading produced wrong world state and could crash on busy maps;
  it was long carried in the docs as a known limitation. *Root cause + fix:*
  - **Index↔pointer swizzle truncation.** To make pointers serializable,
    `p_saveg.c` stores object **indices in the pointer fields** themselves
    (e.g. a `sector_t*` field temporarily holds the sector number) and restores
    them on load. Both directions cast through `(int)`, which on LP64/LLP64
    truncates the 8-byte pointer slot to 32 bits → restored references pointed
    at garbage. Now uses `intptr_t`/`uintptr_t` so the full pointer width round-
    trips. *(Note: this stores native pointer-width values, so a save file is
    correct for the build that wrote it, not portable between 32- and 64-bit
    builds — acceptable since this is a 64-bit-only port.)*
  - **Fixed save-buffer overrun.** `G_DoSaveGame` serialized into `screens[1]`
    with a hardcoded `0x2c000` (176 KB) cap sized for 32-bit structs. With
    8-byte pointers the archived `mobj_t`/thinker records are larger, so busy
    maps overran the buffer (heap corruption → crash). Now a dedicated
    `Z_Malloc` buffer sized to the actual level (headers + players + world +
    one `mobj_t`-sized bound per live thinker), bounds-checked and freed.

  Verified: save (`doomsav0.dsg`) and reload both work in-game. **Savegames are
  64-bit-correct on this build.** *(148b89f)*
- **`boolean` vs winsock.** Win32 `<rpcndr.h>` (pulled in by `<winsock2.h>`)
  typedefs `boolean` as `unsigned char`, colliding with DOOM's load-bearing
  `int` boolean. Rename Windows' `boolean` across the system-header includes.
  *(852885a)*

## B. Hi-res: hardcoded 320×200 → variable internal resolution

The 3D view now renders at a true higher internal resolution (`SCREENWIDTH`/
`SCREENHEIGHT` are runtime variables, up to 1920×1200), not an upscaled 320×200.
Two failure modes recur: **byte-width fields that assumed values < 256**, and
**2D code that used screen coords where it must use base coords**.

- **The core discipline (49062bf).** *All 2D drawing* (HUD, status bar, menus,
  intermission, finale, view border) is authored in **`BASE_WIDTH`×`BASE_HEIGHT`
  (320×200)** and scaled up *inside* the `V_*` functions. The 3D view, automap
  and screen wipe draw natively at `SCREENWIDTH`/`SCREENHEIGHT`. `V_DrawPatch`
  range-checks against `BASE_WIDTH/BASE_HEIGHT` (RANGECHECK is on), so any 2D
  position computed from `SCREENWIDTH` overflows and spams *"Patch at x,y exceeds
  LFB"* (and draws in the wrong place). **Rule: HUD/menu/status-bar/intermission/
  finale positions use `BASE_*`, never `SCREEN*`.**
- **`byte` row fields capped at 255.** `visplane_t.top[]/bottom[]` (per-column
  floor/ceiling clip rows) were `byte`. Fine when rows < 200; at hi-res rows
  exceed 255 → floors/ceilings couldn't draw below row 255 (black bands at the
  bottom of the view) and the overflow corrupted span math (an intermittent
  crash). Widen to `unsigned short`, sentinel `0xffff`. *(3d52c03)*
- **`SCREENWIDTH` where `BASE_WIDTH` was meant.** The weapon sprite scaled by
  `viewwidth/SCREENWIDTH` but the art is 320×200 → drawn at `1/hires` size and
  floating; use `/BASE_WIDTH`. And `d_main.c` detected fullscreen view with a
  hardcoded `viewheight == 200`; at hi-res it's `SCREENHEIGHT` → compare against
  `SCREENHEIGHT`. *(aaf0b6c)*
- **More hardcoded base-res in `D_Display`.** The border-update test used
  `scaledviewwidth != 320`, but `scaledviewwidth` is in *screen* units
  (`r_main.c` sets it to `SCREENWIDTH` or `setblocks*32*hires`; the renderer's
  own border code keys off `== SCREENWIDTH`). At hi-res it was true even at full
  width → the border block ran every frame. Compare against `SCREENWIDTH`. Also:
  nothing forced a full status-bar repaint when the menu closed, so 2× text menu
  items (e.g. "VIDEO") that bleed below base `y=168` into the bar stayed baked
  into the HUD over HEALTH/ARMS — force `redrawsbar` when the menu was active
  (`menuactivestate`). *(16b7214)*
- **Zone heap too small for hi-res buffers.** `f_wipe`'s screen-melt does
  `Z_Malloc(SCREENWIDTH*SCREENHEIGHT)` (~1 MB at hires 4), and the intermission
  + renderer tables also scale with resolution. The vanilla 6 MB zone could
  overflow once hi-res level data + textures were loaded (crash at the
  intermission wipe). Raised to 64 MB. *(63050f2)*

## C. Static limits: fixed worst-case arrays → dynamic

1997 sized render lists for 320×200 and simple maps. Hi-res and modern/complex
maps blow past them — as an `I_Error`, dropped geometry (HOM), or corruption.

- **Render lists made dynamic** *(5c5fd1d)*: visplanes (drop `MAXVISPLANES=128`
  → hash pool + free list), drawsegs and vissprites (realloc-grow instead of
  dropping segs/sprites), and solidsegs bounded to the provable worst case
  (`MAXSEGS 32` → `MAXWIDTH/2+8`). All render-only → no demo/netsync impact.

## D. Latent bugs exposed by modern compilers / data

Code that "worked" only because of 1997 toolchains, memory layout, or asset
sizes.

- **Missing array terminator.** `sprnames[]` had no NULL terminator, but
  `R_InitSpriteDefs` scans for one — an out-of-bounds read. It survived on
  `-O0`/Win64 (adjacent memory happened to read as zero) but at `-O2`/Linux it
  over-counts and dereferences garbage → segfault in `P_Init`. Size
  `NUMSPRITES+1` and add the terminator. *(6f70b2c)*
- **Unbounded asset cache.** The HD texture/sprite caches decoded every PNG once
  and kept the full ARGB image forever — modern multi-MB art accumulated to
  >1 GB over a session (decode stalls, gradual slowdown). Now box-downscaled to
  ≤512px, LRU-capped (256 MB tex / 128 MB sprite), and freed per level.
  *(a4ff6b9)*
- **Missing-data crash.** Footstep playback faulted when the sound lumps weren't
  present; guard and disable quietly. *(fb9a2d6)*
- **Enum constant used as a boolean.** `wi_stuff.c`/`hu_stuff.c` test `if
  (french)` — but `french` is a *value* of the `Language_t` enum (= 1), not a
  flag, so the test is *always true*. In single-player the intermission takes the
  harmless `else` (loads `WIOSTI`), so it never showed; but in co-op (`netgame &&
  !deathmatch`) the always-true branch loads `WIOBJ` (the French "items" lump),
  which is absent from non-French IWADs → `W_GetNumForName: WIOBJ not found`
  crash. The real test is `language == french` (and `language` is only ever set
  to `french` for the actual `doom2f.wad`). Fixed all three `if (french)` sites.

---

## Recognizing the pattern (checklist for new bugs)

When something crashes, corrupts, or renders wrong, before assuming it's *our*
new code, check whether it's a 1997 assumption surfacing:

1. **A `byte`/`short` field holding a value that can now exceed 255/32767?**
   (hi-res rows, large counts) → widen it.
2. **A pointer stored in / cast to `int` or `long`?** → `intptr_t`/`uintptr_t`,
   and check any on-disk/over-the-wire struct keeps its original field widths.
3. **A 2D screen position using `SCREENWIDTH/SCREENHEIGHT`?** → it must be
   `BASE_WIDTH/BASE_HEIGHT` and scaled in `V_*` (watch for "exceeds LFB").
4. **A fixed `MAX…` array or `*4`-style allocation?** → check it against the
   hi-res / 64-bit worst case; make it dynamic or `sizeof`-based.
5. **Works at `-O0` but not `-O2`, or on Windows but not Linux?** → suspect an
   out-of-bounds read / uninitialized value the old layout masked.

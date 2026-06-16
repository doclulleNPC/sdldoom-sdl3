# Carmack's TODO — roadmap & status

Every item John Carmack raised in the 1997 source-release note (`../TODO.TXT`),
turned into concrete tasks against this fork — now scored against what the
**SDLDoom-SDL3** base already implements. File paths are relative to this repo root.

**Status legend:** ✅ done · 🟡 partial · ⬜ open · ➕ bonus (done, beyond Carmack's list)

**Rough state:** groups 0–1 done · groups 2–3 partial · groups 4–6 open
(group 4 networking has actually *regressed* — see below).

---

## 0. Housekeeping ("make it work" prerequisites) — ✅ done

- [x] **Build on modern 64-bit Linux.** ✅ 64-bit clean (commit `73174aa`); the classic
      vanilla pointer/`int` truncation traps are fixed. Builds native Linux and Win64
      (MinGW-w64). Known limitation: `p_saveg.c` still archives pointers as 32-bit ints,
      so **savegames are not 64-bit-correct** — fixing this is a loose end.
- [x] **Modernize the build/SDL detection.** ✅ SDL3 via `pkg-config` (Linux) and
      `build.sh` + SDL3 SDK (Windows). The old `-m32` autotools path is left in place but
      **stale** — candidate for deletion once nobody needs it.
- [ ] **Endianness sanity pass.** 🟡 WAD reads still go through `SHORT`/`LONG` (`m_swap.c`);
      worth a quick re-verify on the 64-bit targets, but no known issue.

## 1. "Port it to your favorite operating system" — ✅ done (macOS open)

- [x] **SDL platform layer ported to SDL 3.** ✅ `i_video.c` (window/renderer/streaming
      texture), `i_sound.c` (`SDL_AudioStream`), `i_system.c`, `i_main.c`
      (`SDL_MAIN_HANDLED`).
- [x] **Windows (Win64, MinGW-w64).** ✅ via `build.sh` + bundled `SDL3.dll`.
- [ ] **macOS build.** ⬜ untouched. Engine is portable; only `i_*` + build need it.

## 2. "Add some rendering features" — 🟡 partial

- [ ] **Transparency** — ⬜ open. Translucent walls/sprites. Classic route: a Boom-style
      `TRANMAP` blend LUT + new column drawer in `r_draw.c`, drawer select in
      `r_things.c` / `r_segs.c`. The new truecolor framebuffer may allow real alpha
      blending instead of a LUT.
- [x] **Look up / down (free-look)** — ✅ `mod_freelook` + `players[].lookdir`, view shear;
      weapon position corrected under free-look (commits `0751329`, `03a2cc1`).
      Menu: Options → Mod.
- [ ] **Sloped floors/ceilings** — ⬜ open (the hard one). Per-span plane equations in
      `r_plane.c` + matching collision in `p_map.c`. Schedule last.
- [ ] **(Carmack's bigger idea) Single front-to-back BSP-walk render rewrite** — ⬜ open.
      **Recommended: do NOT do this as a standalone software rewrite.** See scope below.
- [x] **Dynamic (Boom-style) visplanes** — ✅ done. `r_plane.c` now allocates visplanes on
      demand into a hash table keyed by (picnum, lightlevel, height), with retired planes
      parked on a free list and reused (`R_NewVisPlane`, `visplane_hash`, rewritten
      `R_ClearPlanes`/`R_FindPlane`/`R_CheckPlane`/`R_DrawPlanes`); `visplane_t` gained a
      `next` link (`r_defs.h`). The `MAXVISPLANES` array cap and its
      `I_Error("visplane overflow")` are gone. Builds clean (SDL3), smoke-tested headless
      (DOOM2 MAP07, dummy drivers) rendering frames without crash. Render-only → no demo-sync
      impact. `MAXVISPLANES` is now just the (power-of-two) hash-bucket count.
- [x] **Dynamic drawsegs / vissprites + bounded solidsegs** — ✅ done. The sibling render
      limits are no longer hard caps:
      - `drawsegs` (`r_bsp.c`/`r_segs.c`) now `realloc`-grows from `MAXDRAWSEGS` in
        `R_StoreWallRange` (was a silent `return` that dropped segments → HOM).
      - `vissprites` (`r_things.c`) now `realloc`-grows from `MAXVISSPRITES` in
        `R_NewVisSprite` (removed the shared `overflowsprite` dummy that made distant things
        vanish). The pointer-linked `R_SortVisSprites` is safe because all growth happens
        during projection, before the sort/draw.
      - `solidsegs` (`r_bsp.c`) `MAXSEGS` raised from 32 to the provable worst case
        `MAXWIDTH/2 + 8` — vanilla's 32 could silently overrun the array on complex views.
      Externs updated (`r_bsp.h`, `r_things.h`); the `drawsegs`/`visplane` overflow `I_Error`s
      in `R_DrawPlanes` removed. Builds clean; smoke-tested headless on DOOM2 MAP01/15/28 at
      `-render 4` (1280-wide view, monsters) with no crash/overflow. Render-only → no
      demo-sync impact.
- [x] ➕ **True hi-res internal rendering** — ✅ real 1×–4× of 320×200 (not an upscale),
      in-game Video menu (resolution/aspect/fullscreen, persisted). *Beyond Carmack's list.*
- [x] ➕ **Truecolor ("fullcolor") 32-bit framebuffer** — ✅ smooth, non-palette-snapped
      lighting (`r_draw.c` `colormap32`, `i_video.c` `screen32`). *Bonus.*
- [x] ➕ **"Antialiasing" via bilinear upscale** — ✅ `mod_smooth` → `SDL_SCALEMODE_LINEAR`
      (`i_video.c`). The 8-bit renderer's closest thing to AA.
- [x] ➕ **HD sprite / wall-texture replacement** — ✅ opt-in (`hd_sprite.c`, `hd_texture.c`,
      `stb_image.h`). *Bonus.*

## 3. "Add some game features" — 🟡 partial

- [x] **Jumping** — ✅ `mod_jump` + `key_jump`, Quake-style; plays `sfx_oof` grunt
      (commits `0751329`, `e5eaaf0`). Flag-gated for demo compatibility.
- [ ] **Ducking** — ⬜ open. Variable player height/viewheight: `p_user.c`, clipping in
      `p_map.c`, view height in `r_main.c`.
- [ ] **Flying / noclip movement modes** — ⬜ open. Extend movement flags in `p_user.c` /
      `p_mobj.c`.
- [ ] **New weapons** — ⬜ open. Extend `info.c`/`info.h` states, `p_pspr.c`, `d_items.c`,
      ammo/pickups in `p_inter.c`.
- [x] ➕ **Crosshair** — ✅ `mod_crosshair` (Options → Mod). *Bonus.*
- [x] ➕ **Terrain-dependent footstep sounds** — ✅ opt-in (`p_footstep.c`,
      `footstep_tables.h`, `tools/`). *Bonus.*
- [x] ➕ **Quake-style drop-down console** — ✅ (`c_console.c`). *Bonus.*

## 4. Networking — ⬜ open (REGRESSED)

⚠️ The SDL3 port reduced `i_net.c` to a portable **single-player stub** — the original
BSD-socket multiplayer no longer builds. So this group is further from done than vanilla.

- [ ] **Restore basic peer-to-peer netgame** on a modern socket API (`d_net.c`, `i_net.c`)
      — lockstep tic-based.
- [ ] **Packet-server model** — one node collects/rebroadcasts all tics (`d_net.c`).
- [ ] **Client/server model** — authoritative server; larger tic-loop / `g_game.c`
      rearchitecture.

## 5. 3D-accelerated renderer — ⬜ open

- [ ] **GPU render backend (OpenGL or SDL3 GPU API)** — brute-force the level behind the
      `r_*` interface, reusing `p_*` simulation. *(The renderer is still software — hi-res
      + truecolor, but no GPU 3D backend yet.)*
- [ ] **Handle Carmack's stated hurdles** — non-power-of-two textures + multi-texture walls
      (`r_data.c` composition / atlases), and reconciling the 35 Hz timebase with uncapped
      framerate (interpolate render state between tics).

## 6. Code-quality cleanups Carmack flagged as regrets — 🟡 (one already done)

- [x] **Line-of-sight via the BSP tree** — ✅ already shipped in this 1.10 source, *not*
      a TODO. `p_sight.c` does exactly the simplification Carmack wished for:
      `P_CheckSight` → `P_CrossBSPNode` (`p_sight.c:257`, descends the tree, recursing only
      into nodes the sight line crosses) → `P_CrossSubsector` (`:135`, final seg-crossing
      test). No work needed.
- [ ] **Replace polar-coordinate clipping** in the renderer — ⬜ open. `r_bsp.c R_AddLine`
      (`:259`) turns each seg into world angles via `R_PointToAngle` (`:271`), clips against
      `clipangle`/`2*clipangle` (`:287–306`), then maps to screen columns through
      `viewangletox[]`. This is the "polar coordinates for clipping" he called silly — clip
      in projected screen/clip space instead. ⚠️ The angle path also feeds texture mapping
      and is demo-sync-sensitive: flag-gate or prove bit-identical.
- [ ] **Clean up movement/collision** (`p_map.c`, `p_maputl.c`) — ⬜ open. Still blockmap +
      per-line checks, not the BSP. Carmack's BSP-based volume sweeping is the acknowledged
      hard part (Quake edge-bevel territory). Lowest priority.

### Related BSP item (cross-ref from group 2)

- [ ] **Unified front-to-back BSP walk** (the "Right Thing" render rewrite) — ⬜ open. Walls
      already walk the tree front-to-back (`R_RenderBSPNode`/`R_Subsector`/`R_AddLine`), but
      floors/ceilings (`r_plane.c`) and sprites (`r_things.c`) are separate later passes.
      Collapse into one walk that collects per-subsector info and draws flats-as-polygons +
      subsector-clipped sprite fragments on the way back up. Architectural rewrite — see
      group 2 and the scoping notes below.

## Scope: unified front-to-back BSP-walk rewrite (group 2 "bigger idea")

**Current pipeline (classic deferred Doom).** During the BSP walk
(`R_RenderBSPNode` → `R_Subsector`): walls draw *immediately* (`R_StoreWallRange`,
`r_segs.c`) while floors/ceilings are accumulated as visplanes and sprites are queued into
a global `vissprites[]`. After the walk (`R_RenderPlayerView`, `r_main.c`): `R_DrawPlanes`
reconstructs flats as horizontal spans **from the wall-edge top/bottom arrays** ("the gaps
between walls" — Carmack's exact complaint), then `R_DrawMasked` does a **global**
back-to-front sprite sort and clips each sprite per-column against `drawsegs[]`. So walls
are already front-to-back ✓; floors/ceilings and sprites are the two deferred passes to fold
in ✗.

**What a real rewrite touches:**
- `r_plane.c` — gutted: visplanes / `R_FindPlane` / `R_CheckPlane` / `R_MakeSpans` /
  `R_DrawPlanes` / `floorclip`/`ceilingclip` replaced by rasterizing each subsector's
  floor/ceiling **polygon** inline.
- **Subsector geometry (the hard part)** — subsectors store segs but no closed polygon; the
  open BSP-partition side has no seg, so the convex boundary must be reconstructed from segs
  + partition lines.
- `r_segs.c` — remove plane-marking side effects (`markfloor`/`markceiling`, span recording
  in `R_RenderSegLoop`); walls themselves mostly stay.
- `r_things.c` — draw sprites *during* the walk with per-subsector ordering instead of global
  `R_SortVisSprites`/`R_DrawMasked`; re-entangle masked midtextures.
- `r_main.c` — drop the `R_DrawPlanes()` / `R_DrawMasked()` calls.

**Calculus:**
- ✅ **Demo-safe** — rendering doesn't feed the sim; only visual correctness (HOM, sprite
  clipping) is at stake.
- ⚠️ **Double rasterizer surface** — every new rasterizer must carry *both* the 8-bit and the
  truecolor (`colormap32`/`screen32`) write paths plus hi-res scaling. `r_plane.c` was already
  hi-res-bug-fixed once (`3d52c03`).
- ⚠️ **The GPU backend (group 5) subsumes ~90% of the payoff** — a GPU renderer draws flats as
  polygons and depth-buffers sprite/wall ordering for free.

**Decision:** don't do the standalone software rewrite. Either fold "flats-as-polygons +
depth-buffered sprites" into the **group-5 GPU backend**, or leave the deferred software
renderer as-is. Do the **dynamic-visplanes** task (group 2) separately for the one real bug
(overflow `I_Error`s) it would otherwise fix.

## Cross-cutting constraint

Anything that changes simulation results (jumping, slopes, float math) **breaks
demo/netgame compatibility**. Gameplay-affecting changes stay flag-gated (as jump and
free-look already are) and the fixed-point 35 Hz tic path + `m_random` table stay bit-exact
when the flags are off.

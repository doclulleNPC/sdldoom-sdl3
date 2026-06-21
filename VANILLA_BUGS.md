# Vanilla DOOM bugs — catalog & status

Known bugs in the **original id DOOM engine** that this 1997 source still inherits,
scored against *this* fork. Distinct from `LEGACY_FIXES.md` (those are *port* bugs —
64-bit/hi-res/compiler rot); this file is the *gameplay/render* bugs that shipped in
DOOM itself and that demo/speedrun communities documented.

**Status:** ✅ fixed here · ⬜ present (vanilla) · ➖ deliberate limit / not really a bug ·
N/A not applicable.

**The compatibility rule** (see `CLAUDE.md` cross-cutting constraint): anything that
changes simulation results breaks demo/netgame sync. So bugs split into:
- **Render-only** — safe to fix outright (no sim impact).
- **Sim-affecting** — fixing changes playsim → must be limit-removing/Boom-style and, for
  strict vanilla demos, flag-gated. We already removed the *render-list* limits this way
  (`5c5fd1d`); the playsim overflows below are the remaining ones.

---

## A. Overflows & crashes (sim-affecting)

| Bug | Status | Where | Symptom |
|---|---|---|---|
| **Intercepts overflow** | ⬜ present | `p_maputl.c` `intercepts[MAXINTERCEPTS]` (128), `intercept_p++` with **no bound** (`:606`,`:671`) | `P_PathTraverse` (hitscan/LOS/use across complex geometry) runs `intercept_p` past the array → overwrites adjacent globals → wrong shots / crash. The classic "intercepts overflow". |
| **Spechit overflow** ("Donut overrun") | ⬜ present | `p_map.c` `spechit[MAXSPECIALCROSS]` (8), `spechit[numspechit++]` with **no bound** (`:242`) | Crossing >8 special lines in one move overruns `spechit[]` into neighbouring memory — the famous corruption used by some speedruns; crash on others. |
| **Activeplats overflow** | ⬜ present | `p_plats.c` `activeplats[MAXPLATS]` (30); `P_AddActivePlat` → `I_Error("no more plats!")` | >30 simultaneously-moving floors/lifts → hard `I_Error` abort. |
| **Button overflow** | ⬜ present | `p_switch.c` `buttonlist[MAXBUTTONS]` (16) | >16 switch animations queued → the new switch silently never reverts (stuck texture). |
| Visplane / drawseg / vissprite / solidseg caps | ✅ fixed | `r_plane.c`/`r_bsp.c`/`r_things.c` | dynamic/grown (`5c5fd1d`) — render-only, no demo impact. |
| Savegame buffer overrun | ✅ fixed | `g_game.c`/`p_saveg.c` | level-sized `Z_Malloc` + 64-bit swizzle (`148b89f`). |

*Fix approach for the present ones:* grow the arrays (realloc) like the render lists, or
bound + drop. Removes the crash and is bit-compatible with every demo that doesn't
actually overflow (i.e. all legit demos); only pathological "overflow-exploit" demos
diverge. → group **(C)**.

## B. Rendering / visual bugs (render-only → safe to fix)

| Bug | Status | Where | Symptom |
|---|---|---|---|
| **Tutti-frutti** | ⬜ present | texture composite (`r_data.c`) + column mask `&127` (`r_draw.c`) | A wall texture shorter than the drawn span (or non-power-of-2) repeats undefined cache rows → garbage stripe. Mapper-triggered. |
| **Medusa** | ⬜ present | `r_data.c` composite columns used as 2-sided **masked** midtextures | A multi-patch texture on a see-through line makes the masked drawer read a composite where it expects single posts → colour vomit + heavy slowdown. |
| **Long wall error** | ⬜ present | `r_main.c` `R_PointToAngle` (16-bit BAM table) used in `r_bsp.c R_AddLine` | Walls longer than ~2048 units exceed the angle table's precision → wobbling/skewed textures on very long walls (common in modern maps). Render-only. |
| **Fuzz past screen edge** | ⬜ check | `r_draw.c` `R_DrawFuzzColumn` `fuzzoffset` at `dc_yh==viewheight-1` | The spectre fuzz samples `±SCREENWIDTH`; at the bottom view row it can read/write one row out of the view rect. |
| **Sprite "missing rotations"** crash | ✅ fixed | `info.c` `sprnames[]` | needs a terminator + `-fno-strict-aliasing` (`6f70b2c`, `LEGACY_FIXES`). |
| Floor/ceiling black bands at hi-res | ✅ fixed | `r_plane.c` (`3d52c03`) | visplane rows were `byte`. |
| Slime trails | N/A | (node-builder precision, baked into the WAD) | not an engine bug. |

## C. HUD / status bar

| Bug | Status | Where | Symptom |
|---|---|---|---|
| **Ouch face** | ✅ fixed | `st_stuff.c:811` `st_oldhealth - health > ST_MUCHPAIN` | vanilla had the subtraction backwards → the "ouch" face only showed when *gaining* health. Fixed (`facce11`). |
| Picked-up-weapon "evil grin" edge cases | ➖ | `st_stuff.c` | minor; vanilla quirk, left as-is. |

## D. Gameplay quirks (sim-affecting — deliberate or demo-load-bearing)

| Bug | Status | Where | Note |
|---|---|---|---|
| **"All ghosts"** — a crushed (height-0) monster that revives is unkillable & passes walls | ⬜ present | `p_map.c` `P_CheckPosition` / `p_enemy.c` arch-vile | Demo-load-bearing; leave unless a Boom-compat flag is added. |
| **Lost Soul / Pain Elemental 20-skull limit** | ➖ deliberate | `p_enemy.c A_PainShootSkull` (`:1476`) | Intentional cap; lifting it (Boom) changes gameplay → flag-gated only. |
| **Lost soul / skull spawned inside a wall** | ⬜ present | `p_enemy.c A_PainShootSkull` | No position check on the new skull. Demo-sensitive. |
| **Blockmap "wallrunning" / linedef-trigger skip** | ⬜ present | `p_maputl.c` blockmap iteration | Movement/precision; demo-load-bearing. |
| **Self-referencing sector "deep water"** | N/A | — | a *feature* (mapping trick), not a bug. |

---

## What we fix here

- **(B) render-only visual bugs** — safe, no demo impact. Targeting the contained ones
  (long-wall, fuzz-edge) and the texture ones (tutti-frutti/medusa) where a standard
  source-port fix applies. Needs visual spot-check.
- **(C) the overflow crashes** — intercepts, spechit, activeplats, buttons — limit-removed
  (grown/bounded). Compatible with all non-overflowing demos.

Deliberate limits (D, lost-soul cap etc.) and demo-load-bearing sim quirks are **left
vanilla** unless a future Boom-compatibility mode flag-gates them.

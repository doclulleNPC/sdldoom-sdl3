// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Terrain-dependent player footstep sounds.
//
//	Ported from the GZDoom "zk-resources" footsteps mod by DaZombieKiller
//	(GDCC/ACS source in zk-resources-1.0/project_footsteps/source/src/main.c).
//	The flat->terrain table comes from that mod's language.txt and the
//	sound variants from its sndinfo.txt; both are regenerated, together with
//	footsteps.wad, by tools_footsteps/gen_footsteps.py into footstep_tables.h.
//
//	The original drives the sim from an ACS ENTER script; here we run the
//	same logic from P_PlayerThink.  It is purely cosmetic and deliberately
//	uses a private RNG (never P_Random) so demo/net determinism is untouched.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_player.h"
#include "p_local.h"
#include "w_wad.h"
#include "s_sound.h"
#include "sounds.h"
#include "p_footstep.h"

// Generated tables: terrain_t, terrain_sounds[], flat_terrain[],
// NUM_FLAT_TERRAIN, TER_DEFAULT_GROUP.
#include "footstep_tables.h"

extern int		firstflat;	// r_data.c

// MOD: master enable (m_menu.c / config), and the user's sfx volume.
extern int		mod_footsteps;
extern int		snd_SfxVolume;

// Per-player step countdown (cosmetic; not archived).
static int		footstep_timer[MAXPLAYERS];

// Private RNG -- must NOT be the playsim P_Random table, or demos desync.
static unsigned int	fs_rndseed;

static int FS_Rand (void)
{
    fs_rndseed = fs_rndseed * 1103515245u + 12345u;
    return (fs_rndseed >> 16) & 0x7fff;
}

//
// Map a sector floor flat to a terrain type.  Flat lump names are upper-case
// and NUL-padded to 8 bytes, exactly like the generated table, so a plain
// 8-byte compare is enough.
//
static terrain_t P_FlatTerrain (int floorpic)
{
    const char* name = lumpinfo[firstflat + floorpic].name;
    int		i;

    for (i = 0 ; i < (int)NUM_FLAT_TERRAIN ; i++)
	if (!strncmp(flat_terrain[i].name, name, 8))
	    return flat_terrain[i].terrain;

    return TER_DEFAULT_GROUP;
}

//
// P_PlayerFootsteps
//
void P_PlayerFootsteps (player_t* player)
{
    mobj_t*	mo = player->mo;
    int		pi = player - players;
    fixed_t	mag;
    int		units;
    int		speed;		// 0..256, where 256 == full run (|v| ~16/tic)
    int		interval;	// tics between steps: 35 (crawl) .. 10 (run)
    int		vol;		// 0..snd_SfxVolume
    terrain_t	ter;

    if (!mod_footsteps || !mo)
	return;

    // Only the living, and only while on the ground.
    if (player->playerstate != PST_LIVE || mo->z > mo->floorz)
    {
	footstep_timer[pi] = 0;
	return;
    }

    // Approximate horizontal speed in whole map units per tic.
    mag = P_AproxDistance(mo->momx, mo->momy);
    units = mag >> FRACBITS;
    if (units < 2)			// effectively standing still
    {
	footstep_timer[pi] = 0;
	return;
    }

    if (footstep_timer[pi] > 0)
    {
	footstep_timer[pi]--;
	return;
    }

    // Same curve as the GDCC source: speed = min(1, |v|/16) (here scaled by
    //  256, so 256 == full run), delay = 35 - 25*speed tics.  Float the whole
    //  expression before the single truncation, exactly like the ACS does
    //  (e.g. half speed -> 22 tics, not 23).
    speed = units * 16;
    if (speed > 256)
	speed = 256;

    interval = (35 * 256 - 25 * speed) / 256;	// 10 (run) .. 35 (crawl)
    if (interval < 1)
	interval = 1;

    // The original plays, then ACS_Delay(interval), so the play-to-play
    //  period is exactly `interval` tics.  We spend the current tic playing,
    //  so wait interval-1 more before the next step.
    footstep_timer[pi] = interval - 1;

    ter = P_FlatTerrain(mo->subsector->sector->floorpic);
    if (ter == TER_SILENT || terrain_sounds[ter].count == 0)
	return;

    vol = (snd_SfxVolume * speed) / 256;
    if (vol < 1)
	vol = 1;

    S_StartSoundAtVolume(mo,
	terrain_sounds[ter].sfx[FS_Rand() % terrain_sounds[ter].count],
	vol);
}

// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Terrain-dependent player footstep sounds.
//	Ported from the GZDoom "zk-resources" footsteps mod (see
//	zk-resources-1.0/project_footsteps).  Cosmetic only: it never touches
//	the playsim RNG, so demos/netplay stay deterministic.
//
//-----------------------------------------------------------------------------

#ifndef __P_FOOTSTEP__
#define __P_FOOTSTEP__

#include "d_player.h"

// Called once per player per tic from P_PlayerThink.
void P_PlayerFootsteps (player_t* player);

#endif

// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Voxel (KVX) model replacements for sprites.
//	Reads run/voxels.wad (built from VoxelDoom_v2.4.pk3 by
//	tools/gen_voxels.py) -- one raw KVX lump per model plus a VOXELDEF
//	lump mapping sprite frames to models -- and renders the model in place
//	of the 8-bit sprite into the truecolor framebuffer (screen32).  Has no
//	effect unless Fullcolor and Voxels are both enabled (Options -> Mod).
//
//-----------------------------------------------------------------------------

#ifndef __HD_VOXEL__
#define __HD_VOXEL__

struct vissprite_s;

// Draw the voxel model for this sprite-lump name (e.g. "MEDIA0") in place of
// the sprite, into screen32, clipped per-column by mfloorclip/mceilingclip.
// Returns 1 if a voxel existed and was drawn, 0 if the caller should fall back
// to the normal sprite/HD-sprite path.  Lazily opens voxels.wad on first use.
int HD_DrawVoxel (struct vissprite_s* vis, const char* name8);

// Free all decoded voxel models (called at level load).
void HD_VoxelReset (void);

#endif

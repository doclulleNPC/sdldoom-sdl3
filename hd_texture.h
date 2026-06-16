// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	HD (truecolor) wall-texture and flat replacements.
//	Reads run/hdtextures.wad (built from the DHTP texture pack by
//	tools/gen_hdtextures.py), streaming lumps from disk and
//	decoding on demand via stb_image.  Results are cached per texture/flat
//	index for the renderer (r_segs.c / r_plane.c).
//
//-----------------------------------------------------------------------------

#ifndef __HD_TEXTURE__
#define __HD_TEXTURE__

#include "hd_sprite.h"		// hdimage_t

// Allocate the per-index caches (called once after textures/flats are loaded).
void HD_TexCacheInit (int numtextures, int numflats);

// Resolve an HD wall texture / flat by engine index (+ its 8-char lump name).
// Returns NULL if there is no HD replacement.  Cached after first lookup.
hdimage_t* HD_GetTexture (int texnum, const char* name8);
hdimage_t* HD_GetFlat (int flatnum, const char* name8);

// Free all decoded HD textures/flats (called at level load so maps don't
// accumulate decoded images across a session).
void HD_LevelReset (void);

#endif

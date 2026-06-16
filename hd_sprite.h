// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	HD (truecolor) sprite replacements.
//	Reads run/hdsprites.wad (built from marcelus_hd_soft.pk3 by
//	tools/gen_hdsprites.py) -- one PNG lump per sprite frame --
//	and decodes frames on demand via stb_image into ARGB8888.  Used by the
//	fullcolor sprite path in r_things.c; has no effect unless Fullcolor and
//	HD Sprites are both enabled.
//
//-----------------------------------------------------------------------------

#ifndef __HD_SPRITE__
#define __HD_SPRITE__

typedef struct
{
    int		w;
    int		h;
    unsigned*	rgba;		// ARGB8888, w*h, or NULL if no/failed decode
} hdimage_t;

// Look up an HD frame by sprite-lump name (up to 8 chars, e.g. "SHTGA0").
// Returns NULL if there is no HD replacement or it could not be decoded.
// Lazily opens hdsprites.wad and decodes on first use.
hdimage_t* HD_Get (const char* name8);

// Free all decoded HD sprite frames (called at level load).
void HD_SpriteReset (void);

#endif

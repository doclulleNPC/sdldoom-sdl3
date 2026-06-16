// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	HD wall-texture / flat replacement registry + PNG decoder.  See
//	hd_texture.h.  The WAD (~200 MB) is NOT held in memory; lumps are read
//	from disk on first use and the decoded ARGB image is cached.  The
//	stb_image implementation lives in hd_sprite.c, so we only declare here.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "stb_image.h"		// implementation is in hd_sprite.c

#include "hd_sprite.h"		// hdimage_t
#include "hd_texture.h"

typedef struct
{
    char	name[9];
    int		filepos;
    int		size;
    hdimage_t	img;		// img.rgba == NULL until decoded
    int		tried;
} hdtex_t;

static hdtex_t*		reg;
static int		numreg;
static FILE*		texwad;		// kept open for on-demand reads
static int		tex_inited;	// 0=untried, 1=ready, -1=unavailable

// Per-index caches (resolved name -> image, or the NONE sentinel).
static hdimage_t**	texslot;
static signed char*	texstate;
static int		ntex;
static hdimage_t**	flatslot;
static signed char*	flatstate;
static int		nflat;

static int rd32 (unsigned char* p)
{
    return (int)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24));
}

static void HD_TexInit (void)
{
    unsigned char	hdr[12];
    int			numlumps, infotableofs, i;
    unsigned char*	dir;

    tex_inited = -1;

    texwad = fopen ("hdtextures.wad", "rb");
    if (!texwad)
	return;
    if (fread (hdr, 1, 12, texwad) != 12)
	return;
    if (memcmp (hdr, "PWAD", 4) && memcmp (hdr, "IWAD", 4))
	return;

    numlumps     = rd32 (hdr+4);
    infotableofs = rd32 (hdr+8);
    if (numlumps <= 0 || infotableofs < 12)
	return;

    dir = malloc (numlumps*16);
    if (!dir)
	return;
    fseek (texwad, infotableofs, SEEK_SET);
    if (fread (dir, 1, numlumps*16, texwad) != (size_t)numlumps*16)
    { free (dir); return; }

    reg = calloc (numlumps, sizeof(*reg));
    numreg = numlumps;
    for (i=0 ; i<numlumps ; i++)
    {
	reg[i].filepos = rd32 (dir + i*16);
	reg[i].size    = rd32 (dir + i*16 + 4);
	memcpy (reg[i].name, dir + i*16 + 8, 8);
	reg[i].name[8] = 0;
    }
    free (dir);
    tex_inited = 1;
}

static hdimage_t* HD_Decode (hdtex_t* e)
{
    unsigned char*	comp;
    unsigned char*	px;
    int			w=0, h=0, c=0, n;

    if (e->tried)
	return e->img.rgba ? &e->img : NULL;
    e->tried = 1;

    comp = malloc (e->size);
    if (!comp)
	return NULL;
    fseek (texwad, e->filepos, SEEK_SET);
    if (fread (comp, 1, e->size, texwad) != (size_t)e->size)
    { free (comp); return NULL; }

    px = stbi_load_from_memory (comp, e->size, &w, &h, &c, 4);
    free (comp);
    if (!px)
	return NULL;

    e->img.w = w;
    e->img.h = h;
    e->img.rgba = malloc (w*h*sizeof(unsigned));
    if (e->img.rgba)
	for (n=0 ; n<w*h ; n++)
	{
	    unsigned char* s = px + n*4;
	    e->img.rgba[n] = ((unsigned)s[3]<<24) | ((unsigned)s[0]<<16)
			     | ((unsigned)s[1]<<8) | (unsigned)s[2];
	}
    stbi_image_free (px);
    return e->img.rgba ? &e->img : NULL;
}

static hdimage_t* HD_GetByName (const char* name8)
{
    char	key[9];
    int		i;

    if (!tex_inited)
	HD_TexInit ();
    if (tex_inited != 1)
	return NULL;

    for (i=0 ; i<8 && name8[i] ; i++)
	key[i] = toupper ((unsigned char)name8[i]);
    for (; i<8 ; i++)
	key[i] = 0;

    for (i=0 ; i<numreg ; i++)
	if (!memcmp (reg[i].name, key, 8))
	    return HD_Decode (&reg[i]);
    return NULL;
}

void HD_TexCacheInit (int numtextures, int numflats)
{
    ntex  = numtextures;
    nflat = numflats;
    texslot   = calloc (ntex,  sizeof(*texslot));
    texstate  = calloc (ntex,  sizeof(*texstate));
    flatslot  = calloc (nflat, sizeof(*flatslot));
    flatstate = calloc (nflat, sizeof(*flatstate));
}

hdimage_t* HD_GetTexture (int texnum, const char* name8)
{
    if (texnum < 0 || texnum >= ntex || !texslot)
	return HD_GetByName (name8);
    if (!texstate[texnum])
    {
	texstate[texnum] = 1;
	texslot[texnum] = HD_GetByName (name8);
    }
    return texslot[texnum];
}

hdimage_t* HD_GetFlat (int flatnum, const char* name8)
{
    if (flatnum < 0 || flatnum >= nflat || !flatslot)
	return HD_GetByName (name8);
    if (!flatstate[flatnum])
    {
	flatstate[flatnum] = 1;
	flatslot[flatnum] = HD_GetByName (name8);
    }
    return flatslot[flatnum];
}

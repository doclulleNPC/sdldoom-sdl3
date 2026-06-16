// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	HD wall-texture / flat replacement registry + PNG decoder.  See
//	hd_texture.h.  The WAD (~200 MB) is NOT held in memory; lumps are read
//	from disk on first use and the decoded ARGB image is cached.  The
//	stb_image implementation lives in hd_sprite.c, so we only declare here.
//
//	Decoded images are bounded three ways so a long session can't bloat:
//	  * oversized PNGs are box-downscaled to <= HD_MAXDIM (also cuts decode
//	    cost and the per-texture frame hitch),
//	  * total decoded bytes are kept under HD_TEX_BUDGET via LRU eviction,
//	  * HD_LevelReset() frees all decodes at level load (called by p_setup),
//	    so textures from earlier maps don't accumulate.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "stb_image.h"		// implementation is in hd_sprite.c

#include "hd_sprite.h"		// hdimage_t
#include "hd_texture.h"

#define HD_MAXDIM	512			// downscale PNGs larger than this
#define HD_TEX_BUDGET	(256u * 1024 * 1024)	// decoded-image memory cap

typedef struct
{
    char	name[9];
    int		filepos;
    int		size;
    hdimage_t	img;		// img.rgba == NULL until decoded (or evicted)
    int		tried;		// 1 = decode attempted (and not since evicted)
    unsigned	lastused;	// LRU stamp
} hdtex_t;

static hdtex_t*		reg;
static int		numreg;
static FILE*		texwad;		// kept open for on-demand reads
static int		tex_inited;	// 0=untried, 1=ready, -1=unavailable

static size_t		hd_bytes;	// total decoded image bytes
static unsigned		hd_clock;	// LRU tick

// Per-index caches: engine texture/flat index -> registry entry (resolved
// once; the decode itself is reused/evicted/re-decoded by HD_Decode).
static hdtex_t**	texent;
static signed char*	texstate;
static int		ntex;
static hdtex_t**	flatent;
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

// Box-average `px` (w*h RGBA) down by integer factor f, in place-ish; returns a
// new malloc'd buffer of size nw*nh*4 and updates *w,*h.
static unsigned char* HD_Downscale (unsigned char* px, int* w, int* h, int f)
{
    int			ow=*w, oh=*h, nw=ow/f, nh=oh/f, x, y, k;
    unsigned char*	out;

    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    out = malloc (nw*nh*4);
    if (!out)
	return px;			// keep original on OOM

    for (y=0 ; y<nh ; y++)
	for (x=0 ; x<nw ; x++)
	{
	    int sum[4] = {0,0,0,0}, cnt=0, sy, sx;
	    for (sy=0 ; sy<f ; sy++)
		for (sx=0 ; sx<f ; sx++)
		{
		    int ix = x*f+sx, iy = y*f+sy;
		    if (ix<ow && iy<oh)
		    {
			unsigned char* s = px + (iy*ow+ix)*4;
			for (k=0;k<4;k++) sum[k]+=s[k];
			cnt++;
		    }
		}
	    {
		unsigned char* d = out + (y*nw+x)*4;
		for (k=0;k<4;k++) d[k] = cnt ? sum[k]/cnt : 0;
	    }
	}
    free (px);
    *w = nw; *h = nh;
    return out;
}

// Drop the least-recently-used decoded image(s) until under budget.  Evicted
// entries keep their registry slot but reset tried=0 so they can re-decode.
static void HD_Evict (hdtex_t* keep)
{
    while (hd_bytes > HD_TEX_BUDGET)
    {
	hdtex_t*	victim = NULL;
	unsigned	best = 0xffffffffu;
	int		i;

	for (i=0 ; i<numreg ; i++)
	    if (reg[i].img.rgba && &reg[i] != keep && reg[i].lastused < best)
	    { best = reg[i].lastused; victim = &reg[i]; }

	if (!victim)
	    break;
	hd_bytes -= (size_t)victim->img.w * victim->img.h * sizeof(unsigned);
	free (victim->img.rgba);
	victim->img.rgba = NULL;
	victim->tried = 0;		// allow re-decode if seen again
    }
}

static hdimage_t* HD_Decode (hdtex_t* e)
{
    unsigned char*	comp;
    unsigned char*	px;
    int			w=0, h=0, c=0, n, maxd;

    if (e->img.rgba)			// already decoded -- just touch LRU
    {
	e->lastused = ++hd_clock;
	return &e->img;
    }
    if (e->tried)			// decode previously failed
	return NULL;
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

    maxd = w>h ? w : h;
    if (maxd > HD_MAXDIM)
	px = HD_Downscale (px, &w, &h, (maxd + HD_MAXDIM-1) / HD_MAXDIM);

    e->img.w = w;
    e->img.h = h;
    e->img.rgba = malloc ((size_t)w*h*sizeof(unsigned));
    if (e->img.rgba)
    {
	for (n=0 ; n<w*h ; n++)
	{
	    unsigned char* s = px + n*4;
	    e->img.rgba[n] = ((unsigned)s[3]<<24) | ((unsigned)s[0]<<16)
			     | ((unsigned)s[1]<<8) | (unsigned)s[2];
	}
	hd_bytes += (size_t)w*h*sizeof(unsigned);
	e->lastused = ++hd_clock;
	if (hd_bytes > HD_TEX_BUDGET)
	    HD_Evict (e);
    }
    stbi_image_free (px);
    return e->img.rgba ? &e->img : NULL;
}

static hdtex_t* HD_ResolveByName (const char* name8)
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
	    return &reg[i];
    return NULL;
}

void HD_TexCacheInit (int numtextures, int numflats)
{
    ntex  = numtextures;
    nflat = numflats;
    texent    = calloc (ntex,  sizeof(*texent));
    texstate  = calloc (ntex,  sizeof(*texstate));
    flatent   = calloc (nflat, sizeof(*flatent));
    flatstate = calloc (nflat, sizeof(*flatstate));
}

// Free all decoded images (e.g. at level load) so maps don't accumulate.
void HD_LevelReset (void)
{
    int	i;
    for (i=0 ; i<numreg ; i++)
	if (reg[i].img.rgba)
	{
	    free (reg[i].img.rgba);
	    reg[i].img.rgba = NULL;
	    reg[i].tried = 0;
	}
    hd_bytes = 0;
}

hdimage_t* HD_GetTexture (int texnum, const char* name8)
{
    hdtex_t*	e;
    if (texnum < 0 || texnum >= ntex || !texent)
    {
	e = HD_ResolveByName (name8);
	return e ? HD_Decode (e) : NULL;
    }
    if (!texstate[texnum])
    {
	texstate[texnum] = 1;
	texent[texnum] = HD_ResolveByName (name8);
    }
    e = texent[texnum];
    return e ? HD_Decode (e) : NULL;
}

hdimage_t* HD_GetFlat (int flatnum, const char* name8)
{
    hdtex_t*	e;
    if (flatnum < 0 || flatnum >= nflat || !flatent)
    {
	e = HD_ResolveByName (name8);
	return e ? HD_Decode (e) : NULL;
    }
    if (!flatstate[flatnum])
    {
	flatstate[flatnum] = 1;
	flatent[flatnum] = HD_ResolveByName (name8);
    }
    e = flatent[flatnum];
    return e ? HD_Decode (e) : NULL;
}

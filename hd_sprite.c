// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	HD (truecolor) sprite replacement registry + PNG decoder.
//	See hd_sprite.h.  Reads run/hdsprites.wad directly (NOT through w_wad,
//	so the PNG lumps never clash with the IWAD's 8-bit sprite lumps of the
//	same name) and decodes frames on demand into ARGB8888 via stb_image.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

#include "hd_sprite.h"

#define HD_MAXDIM	512			// downscale frames larger than this
#define HD_SPR_BUDGET	(128u * 1024 * 1024)	// decoded-frame memory cap

typedef struct
{
    char	name[9];	// upper-case, NUL-terminated
    int		filepos;
    int		size;
    hdimage_t	img;		// img.rgba == NULL until decoded (or evicted)
    int		tried;		// decode attempted (and not since evicted)
    unsigned	lastused;	// LRU stamp
} hdentry_t;

static hdentry_t*	entries;
static int		numentries;
static unsigned char*	waddata;	// whole hdsprites.wad in memory
static int		hd_inited;	// 0=untried, 1=ready, -1=unavailable
static size_t		hd_bytes;	// total decoded frame bytes
static unsigned		hd_clock;	// LRU tick

static int rd32 (unsigned char* p)
{
    return (int)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24));
}

// Box-average px (w*h RGBA) down by integer factor f; returns new buffer.
static unsigned char* HD_Downscale (unsigned char* px, int* w, int* h, int f)
{
    int			ow=*w, oh=*h, nw=ow/f, nh=oh/f, x, y, k;
    unsigned char*	out;

    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    out = malloc (nw*nh*4);
    if (!out)
	return px;
    for (y=0 ; y<nh ; y++)
	for (x=0 ; x<nw ; x++)
	{
	    int sum[4]={0,0,0,0}, cnt=0, sy, sx;
	    for (sy=0 ; sy<f ; sy++)
		for (sx=0 ; sx<f ; sx++)
		{
		    int ix=x*f+sx, iy=y*f+sy;
		    if (ix<ow && iy<oh)
		    { unsigned char* s=px+(iy*ow+ix)*4; for(k=0;k<4;k++) sum[k]+=s[k]; cnt++; }
		}
	    { unsigned char* d=out+(y*nw+x)*4; for(k=0;k<4;k++) d[k]=cnt?sum[k]/cnt:0; }
	}
    free (px);
    *w=nw; *h=nh;
    return out;
}

static void HD_Evict (hdentry_t* keep)
{
    while (hd_bytes > HD_SPR_BUDGET)
    {
	hdentry_t*	victim = NULL;
	unsigned	best = 0xffffffffu;
	int		i;
	for (i=0 ; i<numentries ; i++)
	    if (entries[i].img.rgba && &entries[i]!=keep && entries[i].lastused<best)
	    { best=entries[i].lastused; victim=&entries[i]; }
	if (!victim) break;
	hd_bytes -= (size_t)victim->img.w * victim->img.h * sizeof(unsigned);
	free (victim->img.rgba);
	victim->img.rgba = NULL;
	victim->tried = 0;
    }
}

// Free all decoded frames at level load so maps don't accumulate them.
void HD_SpriteReset (void)
{
    int i;
    for (i=0 ; i<numentries ; i++)
	if (entries[i].img.rgba)
	{ free (entries[i].img.rgba); entries[i].img.rgba=NULL; entries[i].tried=0; }
    hd_bytes = 0;
}

static void HD_Init (void)
{
    FILE*		f;
    long		len;
    int			numlumps, infotableofs, i;
    unsigned char*	dir;

    hd_inited = -1;				// assume failure until proven

    f = fopen ("hdsprites.wad", "rb");
    if (!f)
	return;
    fseek (f, 0, SEEK_END);
    len = ftell (f);
    fseek (f, 0, SEEK_SET);
    if (len < 12)
    { fclose (f); return; }

    waddata = malloc (len);
    if (!waddata || fread (waddata, 1, len, f) != (size_t)len)
    { fclose (f); free (waddata); waddata = NULL; return; }
    fclose (f);

    if (memcmp (waddata, "PWAD", 4) && memcmp (waddata, "IWAD", 4))
    { free (waddata); waddata = NULL; return; }

    numlumps     = rd32 (waddata+4);
    infotableofs = rd32 (waddata+8);
    if (numlumps <= 0 || infotableofs < 0 || infotableofs + numlumps*16 > len)
    { free (waddata); waddata = NULL; return; }

    entries = calloc (numlumps, sizeof(*entries));
    numentries = numlumps;
    dir = waddata + infotableofs;
    for (i=0 ; i<numlumps ; i++)
    {
	entries[i].filepos = rd32 (dir + i*16);
	entries[i].size    = rd32 (dir + i*16 + 4);
	memcpy (entries[i].name, dir + i*16 + 8, 8);
	entries[i].name[8] = 0;
    }
    hd_inited = 1;
}

hdimage_t* HD_Get (const char* name8)
{
    char	key[9];
    int		i;

    if (!hd_inited)
	HD_Init ();
    if (hd_inited != 1)
	return NULL;

    for (i=0 ; i<8 && name8[i] ; i++)
	key[i] = toupper ((unsigned char)name8[i]);
    for (; i<8 ; i++)
	key[i] = 0;
    key[8] = 0;

    for (i=0 ; i<numentries ; i++)
    {
	hdentry_t* e = &entries[i];
	if (memcmp (e->name, key, 8))		// name[] is NUL-padded to 8
	    continue;

	if (e->img.rgba)			// already decoded -- touch LRU
	{
	    e->lastused = ++hd_clock;
	    return &e->img;
	}
	if (e->tried)				// previously failed
	    return NULL;

	{
	    int w=0, h=0, comp=0, n, maxd;
	    unsigned char* px;
	    e->tried = 1;
	    px = stbi_load_from_memory (waddata + e->filepos, e->size,
					&w, &h, &comp, 4);
	    if (px)
	    {
		maxd = w>h ? w : h;
		if (maxd > HD_MAXDIM)
		    px = HD_Downscale (px, &w, &h, (maxd + HD_MAXDIM-1)/HD_MAXDIM);
		e->img.w = w;
		e->img.h = h;
		e->img.rgba = malloc ((size_t)w*h*sizeof(unsigned));
		if (e->img.rgba)
		{
		    // stb gives R,G,B,A bytes; pack to ARGB8888 (screen32 order)
		    for (n=0 ; n<w*h ; n++)
		    {
			unsigned char* s = px + n*4;
			e->img.rgba[n] = ((unsigned)s[3]<<24) | ((unsigned)s[0]<<16)
					 | ((unsigned)s[1]<<8) | (unsigned)s[2];
		    }
		    hd_bytes += (size_t)w*h*sizeof(unsigned);
		    e->lastused = ++hd_clock;
		    if (hd_bytes > HD_SPR_BUDGET)
			HD_Evict (e);
		}
		stbi_image_free (px);
	    }
	}
	return e->img.rgba ? &e->img : NULL;
    }
    return NULL;
}

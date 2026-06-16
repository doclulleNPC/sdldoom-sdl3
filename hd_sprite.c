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

typedef struct
{
    char	name[9];	// upper-case, NUL-terminated
    int		filepos;
    int		size;
    hdimage_t	img;		// img.rgba == NULL until decoded
    int		tried;		// decode attempted (success or failure)
} hdentry_t;

static hdentry_t*	entries;
static int		numentries;
static unsigned char*	waddata;	// whole hdsprites.wad in memory
static int		hd_inited;	// 0=untried, 1=ready, -1=unavailable

static int rd32 (unsigned char* p)
{
    return (int)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24));
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

	if (!e->tried)
	{
	    int w=0, h=0, comp=0, n;
	    unsigned char* px;
	    e->tried = 1;
	    px = stbi_load_from_memory (waddata + e->filepos, e->size,
					&w, &h, &comp, 4);
	    if (px)
	    {
		e->img.w = w;
		e->img.h = h;
		e->img.rgba = malloc (w*h*sizeof(unsigned));
		if (e->img.rgba)
		{
		    // stb gives R,G,B,A bytes; pack to ARGB8888 (screen32 order)
		    for (n=0 ; n<w*h ; n++)
		    {
			unsigned char* s = px + n*4;
			e->img.rgba[n] = ((unsigned)s[3]<<24) | ((unsigned)s[0]<<16)
					 | ((unsigned)s[1]<<8) | (unsigned)s[2];
		    }
		}
		stbi_image_free (px);
	    }
	}
	return e->img.rgba ? &e->img : NULL;
    }
    return NULL;
}

// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Voxel (KVX) model rendering -- see hd_voxel.h.
//
//	Loads run/voxels.wad (raw KVX lumps + a VOXELDEF lump) directly, decodes
//	mip 0 of each model on demand into a flat list of coloured surface voxels,
//	and rasterizes it as depth-sorted voxel splats into the truecolor
//	framebuffer (screen32), reusing the vissprite's world position, scale,
//	light and the per-column wall clip arrays (mfloorclip/mceilingclip) so the
//	model is occluded exactly like the sprite it replaces.
//
//	First pass: one mip level, no back-face culling (KVX stores only surface
//	voxels, so a back-to-front painter's splat is correct), each voxel drawn
//	as a screen-aligned square sized to its projected world extent.
//
//	The KVX models come from "Voxel Doom" by Daniel Peterson ("cheello") and
//	Nash Muhandes; run/voxels.wad is built from their pk3 by tools/gen_voxels.py
//	(see tools/VOXELS.md).  https://doomwiki.org/wiki/Voxel_Doom
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "doomdef.h"
#include "m_fixed.h"
#include "tables.h"
#include "r_defs.h"
#include "r_state.h"
#include "r_main.h"
#include "hd_voxel.h"

// --- renderer globals we draw against (defined elsewhere) ---
extern fixed_t		viewx, viewy, viewz;
extern fixed_t		viewcos, viewsin;
extern unsigned int*	screen32;	// i_video.c truecolor framebuffer (ARGB8888)
extern double		fc_lightdim[32];// i_video.c per-light-level brightness
extern int		viewwindowx;	// r_draw.c view rect origin
extern int		viewwindowy;
extern short*		mfloorclip;	// r_things.c per-column clip (set by R_DrawSprite)
extern short*		mceilingclip;

#define VOX_MINZ	(FRACUNIT*4)

// ---------------------------------------------------------------------------
// Decoded model
// ---------------------------------------------------------------------------
typedef struct
{
    short	vx, vy, vz;	// voxel grid coordinate (z grows downward)
    unsigned	argb;		// opaque ARGB8888 colour
} voxcell_t;

typedef struct
{
    int		sizex, sizey, sizez;
    fixed_t	pivx, pivy, pivz;	// pivot, voxel units in 16.16
    int		ncells;
    voxcell_t*	cells;
} voxmodel_t;

// ---------------------------------------------------------------------------
// VOXELDEF binding: sprite-frame name -> kvx lump + orientation
// ---------------------------------------------------------------------------
typedef struct
{
    char	spr[9];		// sprite+frame key, upper-case (e.g. "MEDIA")
    int		sprlen;		// 4 (all frames) or 5 (one frame)
    char	kvx[9];		// kvx lump name, upper-case
    angle_t	angle;		// model yaw fixup
    fixed_t	scale;		// map units per voxel
    int		lumpidx;	// index into entries[], -1 = unresolved
    voxmodel_t*	model;		// decoded, NULL until first use
    int		tried;		// decode attempted
} voxdef_t;

typedef struct
{
    char	name[9];
    int		filepos;
    int		size;
} voxentry_t;

static voxentry_t*	entries;
static int		numentries;
static unsigned char*	waddata;
static int		vox_inited;	// 0=untried, 1=ready, -1=unavailable

static voxdef_t*	defs;
static int		numdefs;

// projected-splat scratch (reused across frames)
typedef struct { int x0, y0, vsz; unsigned argb; fixed_t tz; } voxsplat_t;
static voxsplat_t*	splats;
static int		maxsplats;

static int rd32 (unsigned char* p)
{ return (int)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24)); }

static unsigned rd16 (unsigned char* p)
{ return (unsigned)(p[0] | (p[1]<<8)); }


// ---------------------------------------------------------------------------
// VOXELDEF parsing (minimal scanner)
// ---------------------------------------------------------------------------
static angle_t deg2ang (double deg)
{
    // wraps naturally through the unsigned cast
    return (angle_t)(deg * (4294967296.0 / 360.0));
}

// Strip /* */ and // comments in place (replace with spaces, keep newlines).
static void VOX_Decomment (char* s, int len)
{
    int i;
    for (i=0 ; i<len ; i++)
    {
	if (s[i]=='/' && i+1<len && s[i+1]=='*')
	{
	    s[i]=s[i+1]=' ';
	    for (i+=2 ; i<len && !(s[i-1]=='*' && s[i]=='/') ; i++)
		if (s[i]!='\n') s[i]=' ';
	    if (i<len) s[i]=' ';
	}
	else if (s[i]=='/' && i+1<len && s[i+1]=='/')
	{
	    for ( ; i<len && s[i]!='\n' ; i++) s[i]=' ';
	}
    }
}

static void VOX_ParseDef (char* text, int len)
{
    char*	p = text;
    char*	end = text + len;
    char	keys[16][9];
    int		nkeys = 0;
    int		cap = 64;

    VOX_Decomment (text, len);
    defs = malloc (cap * sizeof(*defs));
    numdefs = 0;

    while (p < end)
    {
	// skip whitespace
	while (p<end && (unsigned char)*p<=' ') p++;
	if (p>=end) break;

	if (*p == '=')
	{
	    char	kvx[9];
	    int		kn = 0;
	    angle_t	ang = deg2ang (90.0);	// gzdoom default fixup
	    fixed_t	scl = FRACUNIT;
	    int		k;

	    p++;
	    while (p<end && (unsigned char)*p<=' ') p++;
	    if (p<end && *p=='"')			// "kvxname"
	    {
		p++;
		while (p<end && *p!='"' && kn<8)
		    kvx[kn++] = toupper((unsigned char)*p++);
		while (p<end && *p!='"') p++;
		if (p<end) p++;
	    }
	    kvx[kn] = 0;

	    // optional { options }
	    while (p<end && (unsigned char)*p<=' ') p++;
	    if (p<end && *p=='{')
	    {
		p++;
		while (p<end && *p!='}')
		{
		    while (p<end && (unsigned char)*p<=' ') p++;
		    if (p<end && *p=='}') break;
		    if (p<end && (isalpha((unsigned char)*p)))
		    {
			char	opt[24];
			int	on = 0;
			while (p<end && (isalnum((unsigned char)*p)) && on<23)
			    opt[on++] = tolower((unsigned char)*p++);
			opt[on] = 0;
			while (p<end && (unsigned char)*p<=' ') p++;
			if (p<end && *p=='=')
			{
			    double v;
			    p++;
			    v = strtod (p, &p);
			    if (!strcmp(opt,"angleoffset"))
				ang = deg2ang (v + 90.0);
			    else if (!strcmp(opt,"scale"))
				scl = (fixed_t)(v * FRACUNIT);
			}
			// value-less flags (useactorpitch, ...) just fall through
		    }
		    else if (p<end) p++;
		}
		if (p<end) p++;				// consume '}'
	    }

	    // commit one def per collected key
	    for (k=0 ; k<nkeys ; k++)
	    {
		voxdef_t* d;
		if (numdefs >= cap)
		{ cap *= 2; defs = realloc (defs, cap*sizeof(*defs)); }
		d = &defs[numdefs++];
		memset (d, 0, sizeof(*d));
		strcpy (d->spr, keys[k]);
		d->sprlen = (int)strlen (keys[k]);
		strcpy (d->kvx, kvx);
		d->angle = ang;
		d->scale = scl;
		d->lumpidx = -1;
	    }
	    nkeys = 0;
	}
	else if (isalnum((unsigned char)*p))
	{
	    // a sprite-frame key token
	    int n = 0;
	    if (nkeys < 16)
	    {
		while (p<end && (isalnum((unsigned char)*p)) && n<8)
		    keys[nkeys][n++] = toupper((unsigned char)*p++);
		keys[nkeys][n] = 0;
		nkeys++;
	    }
	    else
		while (p<end && (isalnum((unsigned char)*p))) p++;
	}
	else
	    p++;
    }
}


// ---------------------------------------------------------------------------
// WAD load + def resolution
// ---------------------------------------------------------------------------
static void VOX_Init (void)
{
    FILE*		f;
    long		len;
    int			numlumps, infotableofs, i, j;
    unsigned char*	dir;

    vox_inited = -1;

    f = fopen ("voxels.wad", "rb");
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

    // parse the VOXELDEF lump
    for (i=0 ; i<numentries ; i++)
	if (!strncmp (entries[i].name, "VOXELDEF", 8))
	{
	    char* txt = malloc (entries[i].size + 1);
	    if (txt)
	    {
		memcpy (txt, waddata + entries[i].filepos, entries[i].size);
		txt[entries[i].size] = 0;
		VOX_ParseDef (txt, entries[i].size);
		free (txt);
	    }
	    break;
	}

    // resolve each def's kvx name to a lump index
    for (i=0 ; i<numdefs ; i++)
	for (j=0 ; j<numentries ; j++)
	    if (!strncmp (entries[j].name, defs[i].kvx, 8)
		&& strlen(defs[i].kvx) == strlen(entries[j].name))
	    { defs[i].lumpidx = j; break; }

    vox_inited = 1;
}


// ---------------------------------------------------------------------------
// KVX mip-0 decode
// ---------------------------------------------------------------------------
static voxmodel_t* VOX_Decode (unsigned char* buf, int size)
{
    voxmodel_t*		m;
    unsigned char*	base = buf + 4;	// skip mip0 numbytes
    int			sx, sy, sz, offsetsize, voxdatasize;
    int*		xoff;
    unsigned char*	vox;
    unsigned char*	pal;
    int			x, y, cap, n;
    voxcell_t*		cells;

    if (size < 24 + 768 + 8)
	return NULL;

    sx = rd32 (base+0);  sy = rd32 (base+4);  sz = rd32 (base+8);
    if (sx<=0 || sy<=0 || sz<=0 || sx>256 || sy>256 || sz>256)
	return NULL;

    offsetsize  = (sx+1)*4 + sx*(sy+1)*2;
    voxdatasize = rd32(buf+0) - 24 - offsetsize;
    if (voxdatasize < 0 || 4 + 24 + offsetsize + voxdatasize > size - 768)
	return NULL;

    pal = buf + size - 768;
    vox = base + 24 + offsetsize;

    xoff = malloc ((sx+1) * sizeof(int));
    for (x=0 ; x<=sx ; x++)
	xoff[x] = rd32 (base + 24 + x*4) - offsetsize;

    m = malloc (sizeof(*m));
    m->sizex=sx; m->sizey=sy; m->sizez=sz;
    m->pivx = rd32(base+12) << 8;	// 8.8 -> 16.16
    m->pivy = rd32(base+16) << 8;
    m->pivz = rd32(base+20) << 8;

    cap = 4096; n = 0;
    cells = malloc (cap * sizeof(*cells));

    for (x=0 ; x<sx ; x++)
    {
	unsigned char* xybase = base + 24 + (sx+1)*4 + x*(sy+1)*2;
	for (y=0 ; y<sy ; y++)
	{
	    int s0 = xoff[x] + (int)rd16 (xybase + y*2);
	    int s1 = xoff[x] + (int)rd16 (xybase + (y+1)*2);
	    unsigned char* p;
	    unsigned char* pe;

	    if (s0 < 0 || s1 > voxdatasize || s1 <= s0)
		continue;
	    p  = vox + s0;
	    pe = vox + s1;
	    while (p + 3 <= pe)
	    {
		int ztop = p[0], zleng = p[1], i;
		if (p + 3 + zleng > pe)
		    break;
		for (i=0 ; i<zleng ; i++)
		{
		    int idx = p[3+i];
		    int r6 = pal[idx*3+0], g6 = pal[idx*3+1], b6 = pal[idx*3+2];
		    voxcell_t* c;
		    if (n >= cap)
		    { cap *= 2; cells = realloc (cells, cap*sizeof(*cells)); }
		    c = &cells[n++];
		    c->vx = (short)x; c->vy = (short)y; c->vz = (short)(ztop+i);
		    c->argb = 0xff000000u
			| ((unsigned)((r6<<2)|(r6>>4))<<16)
			| ((unsigned)((g6<<2)|(g6>>4))<<8)
			|  (unsigned)((b6<<2)|(b6>>4));
		}
		p += 3 + zleng;
	    }
	}
    }

    free (xoff);
    m->ncells = n;
    m->cells = cells;
    return m;
}


// ---------------------------------------------------------------------------
// Lookup by sprite-lump name (e.g. "MEDIA0")
// ---------------------------------------------------------------------------
static voxdef_t* VOX_Lookup (const char* name8)
{
    char	key[9];
    int		i, n;

    for (n=0 ; n<8 && name8[n] ; n++)
	key[n] = toupper ((unsigned char)name8[n]);
    for (i=n ; i<8 ; i++) key[i]=0;
    key[8]=0;

    for (i=0 ; i<numdefs ; i++)
	if (!strncmp (defs[i].spr, key, defs[i].sprlen))
	    return &defs[i];
    return NULL;
}

static voxmodel_t* VOX_Model (voxdef_t* d)
{
    if (d->model)
	return d->model;
    if (d->tried || d->lumpidx < 0)
	return NULL;
    d->tried = 1;
    d->model = VOX_Decode (waddata + entries[d->lumpidx].filepos,
			   entries[d->lumpidx].size);
    return d->model;
}


// ---------------------------------------------------------------------------
// Painter's-order splat comparator (far first)
// ---------------------------------------------------------------------------
static int splatcmp (const void* a, const void* b)
{
    fixed_t za = ((const voxsplat_t*)a)->tz;
    fixed_t zb = ((const voxsplat_t*)b)->tz;
    return (za<zb) ? 1 : (za>zb) ? -1 : 0;	// descending tz
}


// ---------------------------------------------------------------------------
// Public: draw the voxel for this sprite frame
// ---------------------------------------------------------------------------
int HD_DrawVoxel (vissprite_t* vis, const char* name8)
{
    voxdef_t*	d;
    voxmodel_t*	m;
    fixed_t	mupv, cosv, sinv;
    angle_t	yaw;
    double	dim = 1.0;
    int		i, ns = 0;

    if (!vox_inited)
	VOX_Init ();
    if (vox_inited != 1)
	return 0;

    d = VOX_Lookup (name8);
    if (!d)
	return 0;
    m = VOX_Model (d);
    if (!m || m->ncells <= 0)
	return 0;

    // light dim, mirroring R_BlitHDSprite
    if (vis->colormap)
    {
	int row = (int)((vis->colormap - colormaps) >> 8);
	if (row >= 0 && row < 32)
	    dim = fc_lightdim[row];
    }

    mupv = d->scale;				// map units per voxel
    yaw  = vis->mobjangle + d->angle;
    cosv = finecosine[yaw >> ANGLETOFINESHIFT];
    sinv = finesine  [yaw >> ANGLETOFINESHIFT];

    if (m->ncells > maxsplats)
    {
	maxsplats = m->ncells;
	splats = realloc (splats, maxsplats * sizeof(*splats));
    }

    // project every surface voxel to a screen-aligned square
    for (i=0 ; i<m->ncells ; i++)
    {
	voxcell_t*	c = &m->cells[i];
	fixed_t		lx = FixedMul (mupv, (c->vx<<FRACBITS) + (FRACUNIT>>1) - m->pivx);
	fixed_t		ly = FixedMul (mupv, (c->vy<<FRACBITS) + (FRACUNIT>>1) - m->pivy);
	fixed_t		az = vis->gz
			   + FixedMul (mupv, m->pivz - ((c->vz<<FRACBITS) + (FRACUNIT>>1)));
	fixed_t		dx = FixedMul (lx,cosv) - FixedMul (ly,sinv);
	fixed_t		dy = FixedMul (lx,sinv) + FixedMul (ly,cosv);
	fixed_t		ax = vis->gx + dx;
	fixed_t		ay = vis->gy + dy;
	fixed_t		tr_x = ax - viewx;
	fixed_t		tr_y = ay - viewy;
	fixed_t		tz = FixedMul(tr_x,viewcos) + FixedMul(tr_y,viewsin);
	fixed_t		xscale, txp, sxc, syc;
	int		vsz, half;
	unsigned	argb;
	voxsplat_t*	sp;

	if (tz < VOX_MINZ)
	    continue;
	xscale = FixedDiv (projection, tz);
	txp = FixedMul(tr_x,viewsin) - FixedMul(tr_y,viewcos);
	sxc = (centerxfrac + FixedMul (txp, xscale)) >> FRACBITS;
	syc = (centeryfrac - FixedMul (az - viewz, xscale)) >> FRACBITS;

	vsz = FixedMul (mupv, xscale) >> FRACBITS;
	if (vsz < 1) vsz = 1;
	half = vsz >> 1;

	argb = c->argb;
	if (dim < 0.999)
	{
	    unsigned r=(argb>>16)&0xff, g=(argb>>8)&0xff, b=argb&0xff;
	    argb = 0xff000000u | (((unsigned)(r*dim))<<16)
		 | (((unsigned)(g*dim))<<8) | (unsigned)(b*dim);
	}

	sp = &splats[ns++];
	sp->x0 = (int)sxc - half;
	sp->y0 = (int)syc - half;
	sp->vsz = vsz;
	sp->argb = argb;
	sp->tz = tz;
    }

    if (ns <= 0)
	return 1;	// voxel existed but projected to nothing; still skip sprite

    qsort (splats, ns, sizeof(*splats), splatcmp);

    // splat back-to-front, clipped per column by the wall/sprite clip arrays
    for (i=0 ; i<ns ; i++)
    {
	voxsplat_t*	sp = &splats[i];
	int		x, y;
	int		xl = sp->x0, xr = sp->x0 + sp->vsz;
	int		yt = sp->y0, yb = sp->y0 + sp->vsz;

	if (xl < vis->x1) xl = vis->x1;
	if (xr > vis->x2+1) xr = vis->x2+1;
	if (yt < 0) yt = 0;
	if (yb > viewheight) yb = viewheight;

	for (x=xl ; x<xr ; x++)
	{
	    int	cy0 = yt, cy1 = yb;
	    unsigned* col;

	    if (cy1 > mfloorclip[x])   cy1 = mfloorclip[x];
	    if (cy0 <= mceilingclip[x]) cy0 = mceilingclip[x]+1;
	    if (cy0 >= cy1)
		continue;

	    col = &screen32[(cy0+viewwindowy)*SCREENWIDTH + viewwindowx + x];
	    for (y=cy0 ; y<cy1 ; y++, col += SCREENWIDTH)
		*col = sp->argb;
	}
    }
    return 1;
}


// ---------------------------------------------------------------------------
// Free decoded models at level load
// ---------------------------------------------------------------------------
void HD_VoxelReset (void)
{
    int i;
    for (i=0 ; i<numdefs ; i++)
	if (defs[i].model)
	{
	    free (defs[i].model->cells);
	    free (defs[i].model);
	    defs[i].model = NULL;
	    defs[i].tried = 0;
	}
}

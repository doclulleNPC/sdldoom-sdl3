// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for the SDL3 library.
//
//	The DOOM software renderer produces an 8-bit, palettized 320x200
//	framebuffer in screens[0].  SDL3 no longer exposes palettized display
//	surfaces, so each frame we expand that 8-bit buffer through a 32-bit
//	palette into a streaming texture and let the renderer scale it to the
//	window.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>

#include <SDL3/SDL.h>

#include "m_swap.h"
#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"


static SDL_Window*	window = NULL;
static SDL_Renderer*	renderer = NULL;
static SDL_Texture*	texture = NULL;

// Expanded 32-bit (ARGB8888) palette, rebuilt by I_SetPalette.
static Uint32		palette[256];

// --- Fullcolor (truecolor) mode (Options -> Mod -> Fullcolor) -------------
// The 3D view drawers (r_draw.c) optionally dual-write a parallel truecolor
// framebuffer (screen32) using colormap32 -- a smooth, non-palette-snapped
// version of the 8-bit light colormaps.  I_FinishUpdate then composites the
// truecolor view into the final image wherever 2D (HUD/menu) did not draw
// over it.  See R_RenderPlayerView / I_CaptureTrueColorView.
#define NUMCMAPS	34		// COLORMAP lump = 32 light + invuln + extra
Uint32			colormap32[NUMCMAPS*256];
Uint32*			screen32;	// truecolor 3D-view framebuffer
int			truecolor;	// drawers dual-write while this is set
static byte*		view8snap;	// 8-bit view snapshot (2D-overdraw detect)
static int		view_truecolor;	// a truecolor view was captured this frame
double		fc_lightdim[32];	// per-light-level brightness (exported for HD sprite shading)
static int		cm_dim_ready;
static int		cm_built;	// colormap32 has been built at least once

extern byte*		colormaps;	// r_data.c (8-bit light tables)
extern int		viewwindowx, viewwindowy, scaledviewwidth, viewheight;

// Rebuild colormap32 from the current (possibly tinted) palette.  Called from
// I_SetPalette so damage/pickup/radsuit palette flashes tint the view too.
static void I_BuildTrueColormaps (void)
{
    int row, i;

    if (!colormaps)
	return;

    // Derive each light level's brightness once, from the 8-bit colormap.
    if (!cm_dim_ready)
    {
	for (row=0 ; row<32 ; row++)
	{
	    double num=0, den=0;
	    for (i=0 ; i<256 ; i++)
	    {
		Uint32 b = palette[i], l = palette[colormaps[row*256+i]];
		den += ((b>>16)&0xff)+((b>>8)&0xff)+(b&0xff);
		num += ((l>>16)&0xff)+((l>>8)&0xff)+(l&0xff);
	    }
	    fc_lightdim[row] = den>0 ? num/den : 1.0;
	}
	cm_dim_ready = 1;
    }

    // Light levels 0..31: smooth-dim the true palette colour (no snapping).
    for (row=0 ; row<32 ; row++)
    {
	double d = fc_lightdim[row];
	for (i=0 ; i<256 ; i++)
	{
	    Uint32 c = palette[i];
	    Uint32 r = (Uint32)(((c>>16)&0xff)*d);
	    Uint32 g = (Uint32)(((c>>8)&0xff)*d);
	    Uint32 b = (Uint32)((c&0xff)*d);
	    colormap32[row*256+i] = 0xff000000u | (r<<16) | (g<<8) | b;
	}
    }
    // Special maps (invuln, extra): keep the stylised 8-bit-mapped colour.
    for (row=32 ; row<NUMCMAPS ; row++)
	for (i=0 ; i<256 ; i++)
	    colormap32[row*256+i] = palette[colormaps[row*256+i]];

    cm_built = 1;
}

// Snapshot the 8-bit view rect right after R_RenderPlayerView, before any HUD
// or menu draws over it, so the composite can tell view pixels from overlays.
void I_CaptureTrueColorView (void)
{
    int y;

    if (!truecolor || !screen32 || !view8snap)
    {
	view_truecolor = 0;
	return;
    }
    // Ensure the truecolor tables exist (colormaps may not have been loaded
    // yet at the first I_SetPalette during startup).
    if (!cm_built)
	I_BuildTrueColormaps();
    for (y=0 ; y<viewheight ; y++)
    {
	int off = (viewwindowy+y)*SCREENWIDTH + viewwindowx;
	memcpy (view8snap+off, screens[0]+off, scaledviewwidth);
    }
    view_truecolor = 1;
}

// Fake mouse handling.
boolean		grabMouse;


//
//  Translates the key
//
int xlatekey(SDL_Keycode sym)
{

    int rc;

    switch(sym)
    {
      case SDLK_LEFT:	rc = KEY_LEFTARROW;	break;
      case SDLK_RIGHT:	rc = KEY_RIGHTARROW;	break;
      case SDLK_DOWN:	rc = KEY_DOWNARROW;	break;
      case SDLK_UP:	rc = KEY_UPARROW;	break;
      case SDLK_ESCAPE:	rc = KEY_ESCAPE;	break;
      case SDLK_RETURN:	rc = KEY_ENTER;		break;
      case SDLK_TAB:	rc = KEY_TAB;		break;
      case SDLK_F1:	rc = KEY_F1;		break;
      case SDLK_F2:	rc = KEY_F2;		break;
      case SDLK_F3:	rc = KEY_F3;		break;
      case SDLK_F4:	rc = KEY_F4;		break;
      case SDLK_F5:	rc = KEY_F5;		break;
      case SDLK_F6:	rc = KEY_F6;		break;
      case SDLK_F7:	rc = KEY_F7;		break;
      case SDLK_F8:	rc = KEY_F8;		break;
      case SDLK_F9:	rc = KEY_F9;		break;
      case SDLK_F10:	rc = KEY_F10;		break;
      case SDLK_F11:	rc = KEY_F11;		break;
      case SDLK_F12:	rc = KEY_F12;		break;

      case SDLK_BACKSPACE:
      case SDLK_DELETE:	rc = KEY_BACKSPACE;	break;

      case SDLK_PAUSE:	rc = KEY_PAUSE;		break;

      case SDLK_EQUALS:	rc = KEY_EQUALS;	break;

      case SDLK_KP_MINUS:
      case SDLK_MINUS:	rc = KEY_MINUS;		break;

      case SDLK_LSHIFT:
      case SDLK_RSHIFT:
	rc = KEY_RSHIFT;
	break;

      case SDLK_LCTRL:
      case SDLK_RCTRL:
	rc = KEY_RCTRL;
	break;

      case SDLK_LALT:
      case SDLK_LGUI:
      case SDLK_RALT:
      case SDLK_RGUI:
	rc = KEY_RALT;
	break;

      default:
        rc = sym;
	break;
    }

    return rc;

}

void I_ShutdownGraphics(void)
{
  if (texture)  { SDL_DestroyTexture(texture);   texture = NULL;  }
  if (renderer) { SDL_DestroyRenderer(renderer); renderer = NULL; }
  if (window)   { SDL_DestroyWindow(window);     window = NULL;   }
  SDL_Quit();
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

//
// Build a DOOM mouse event from the current SDL mouse button mask.
//
static int I_MouseButtons(SDL_MouseButtonFlags state)
{
    return 0
	| ((state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))   ? 1 : 0)
	| ((state & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) ? 2 : 0)
	| ((state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))  ? 4 : 0);
}

/* This processes SDL events */
void I_GetEvent(SDL_Event *Event)
{
    event_t event;

    switch (Event->type)
    {
      case SDL_EVENT_KEY_DOWN:
	event.type = ev_keydown;
	event.data1 = xlatekey(Event->key.key);
	D_PostEvent(&event);
        break;

      case SDL_EVENT_KEY_UP:
	event.type = ev_keyup;
	event.data1 = xlatekey(Event->key.key);
	D_PostEvent(&event);
	break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP:
	event.type = ev_mouse;
	event.data1 = I_MouseButtons(SDL_GetMouseState(NULL, NULL));
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	break;

      case SDL_EVENT_MOUSE_MOTION:
	event.type = ev_mouse;
	event.data1 = I_MouseButtons(Event->motion.state);
	event.data2 =  ((int)Event->motion.xrel) << 2;
	event.data3 = -((int)Event->motion.yrel) << 2;
	D_PostEvent(&event);
	break;

      case SDL_EVENT_WINDOW_FOCUS_GAINED:
	// Re-grab when we regain focus (e.g. after alt-tab).
	if (grabMouse)
	    SDL_SetWindowRelativeMouseMode(window, true);
	break;

      case SDL_EVENT_WINDOW_FOCUS_LOST:
	// Let go of the mouse so the user can leave the window.
	SDL_SetWindowRelativeMouseMode(window, false);
	break;

      case SDL_EVENT_QUIT:
	I_Quit();
    }

}

//
// I_StartTic
//
void I_StartTic (void)
{
    SDL_Event Event;

    while ( SDL_PollEvent(&Event) )
	I_GetEvent(&Event);
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{

    static int	lasttic;
    int		tics;
    int		i;
    int		x, y;
    void*	pixels;
    int		pitch;

    // draws little dots on the bottom of the screen
    if (devparm)
    {

	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    }

    // Expand the 8-bit palettized frame into the 32-bit streaming texture.
    if ( !SDL_LockTexture(texture, NULL, &pixels, &pitch) )
	return;

    for (y=0 ; y<SCREENHEIGHT ; y++)
    {
	Uint32*		dst = (Uint32 *)((Uint8 *)pixels + y*pitch);
	unsigned char*	src = screens[0] + y*SCREENWIDTH;

	// Fullcolor: inside the 3D view, use the truecolor framebuffer wherever
	// the 8-bit pixel still matches the pre-HUD snapshot (i.e. no overlay
	// drew there); everywhere else fall back to the palette expansion.
	if (view_truecolor
	    && y >= viewwindowy && y < viewwindowy+viewheight)
	{
	    int		vx0 = viewwindowx;
	    int		vx1 = viewwindowx + scaledviewwidth;
	    Uint32*	s32  = screen32   + y*SCREENWIDTH;
	    byte*	snap = view8snap  + y*SCREENWIDTH;

	    for (x=0 ; x<SCREENWIDTH ; x++)
	    {
		if (x >= vx0 && x < vx1 && src[x] == snap[x])
		    dst[x] = s32[x];
		else
		    dst[x] = palette[src[x]];
	    }
	}
	else
	{
	    for (x=0 ; x<SCREENWIDTH ; x++)
		dst[x] = palette[src[x]];
	}
    }
    view_truecolor = 0;		// consumed; next frame must re-capture

    SDL_UnlockTexture(texture);

    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}


//
// I_SetPalette
//
void I_SetPalette (byte* pal)
{
    int i;

    for ( i=0; i<256; ++i ) {
	Uint8 r = gammatable[usegamma][*pal++];
	Uint8 g = gammatable[usegamma][*pal++];
	Uint8 b = gammatable[usegamma][*pal++];
	palette[i] = ((Uint32)0xff << 24) | (r << 16) | (g << 8) | b;
    }

    // Keep the truecolor light tables in sync (so palette flashes tint the
    // fullcolor view as well).
    I_BuildTrueColormaps();
}


// Desired display aspect ratio (controls window shape; the frame is stretched
// to fill the window).  0 = 4:3 (classic), 1 = 16:10 (square pixels),
// 2 = 16:9, 3 = stretch/free.
int	screen_aspect = 0;

// MOD: bilinear filtering on the upscale.  This is the closest the 8-bit
// software renderer can get to "antialiasing": it smooths the magnified frame
// (no true MSAA -- there is no geometry/truecolour stage to sample).
int	mod_smooth = 0;

static const struct { int num, den; } aspects[4] =
{
    { 4, 3 }, { 16, 10 }, { 16, 9 }, { 0, 0 }	// {0,0} = free
};

// Non-zero when displaying fullscreen (don't resize the window then).
// Saved/restored via the config file (see m_misc.c defaults).
int	fullscreen_mode = 0;


//
// (Re)create the streaming texture + logical presentation to match the
// current internal resolution (SCREENWIDTH x SCREENHEIGHT).
//
static void I_CreateTexture(void)
{
    if (texture)
	SDL_DestroyTexture(texture);

    // The frame is presented stretched to fill the window; the window shape
    // (set by I_ApplyAspect) therefore determines the displayed aspect ratio.
    SDL_SetRenderLogicalPresentation(renderer, SCREENWIDTH, SCREENHEIGHT,
				     SDL_LOGICAL_PRESENTATION_STRETCH);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_STREAMING,
				SCREENWIDTH, SCREENHEIGHT);
    if ( texture == NULL )
	I_Error("Could not create texture: %s", SDL_GetError());
    SDL_SetTextureScaleMode(texture,
	mod_smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
}

// MOD: toggle bilinear smoothing at runtime (Options -> Mod).
void I_SetSmoothing(int on)
{
    mod_smooth = on ? 1 : 0;
    if (texture)
	SDL_SetTextureScaleMode(texture,
	    mod_smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
}

//
// Size the window to match the internal resolution and the chosen aspect
// ratio, clamped to fit the display.  Called whenever the resolution or the
// aspect ratio changes, so picking a higher resolution makes a bigger window.
//
void I_ApplyAspect(void)
{
    int		w, h;
    SDL_Rect	bounds;
    SDL_DisplayID disp;

    if (!window || fullscreen_mode)
	return;

    // Window is the internal-resolution width, with the height giving the
    // requested aspect ratio (free mode keeps the native 16:10 frame shape).
    w = SCREENWIDTH;
    if (aspects[screen_aspect].num)
	h = w * aspects[screen_aspect].den / aspects[screen_aspect].num;
    else
	h = SCREENHEIGHT;

    // Don't let the window grow past the usable desktop area.
    disp = SDL_GetDisplayForWindow(window);
    if (disp && SDL_GetDisplayUsableBounds(disp, &bounds))
    {
	int maxw = bounds.w - 40;
	int maxh = bounds.h - 80;
	if (w > maxw) { h = h * maxw / w; w = maxw; }
	if (h > maxh) { w = w * maxh / h; h = maxh; }
    }

    SDL_SetWindowSize(window, w, h);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

//
// Toggle borderless-desktop fullscreen at runtime (from the Video menu).
//
void I_SetFullscreen(int on)
{
    if (!window)
	return;

    fullscreen_mode = on ? 1 : 0;
    SDL_SetWindowFullscreen(window, fullscreen_mode ? true : false);

    if (!fullscreen_mode)
	I_ApplyAspect();		// restore the windowed size
}

int I_GetFullscreen(void)
{
    return fullscreen_mode;
}

// Cross-module hooks used when changing resolution.
extern int	screenblocks;
extern int	detailLevel;
void		R_SetViewSize (int blocks, int detail);
void		ST_SetRes (void);

//
// V_SetRes
// Change the internal rendering resolution at runtime (scale = 1..4).
// Rebuilds the renderer tables, status bar buffer and SDL texture.
//
void V_SetRes(int scale)
{
    if (scale < 1) scale = 1;
    if (scale > 6) scale = 6;
    if (BASE_WIDTH*scale > MAXWIDTH || BASE_HEIGHT*scale > MAXHEIGHT)
	return;

    hires        = scale;
    SCREENWIDTH  = BASE_WIDTH  * scale;
    SCREENHEIGHT = BASE_HEIGHT * scale;

    if (renderer)
	I_CreateTexture();

    // Rebuild resolution-dependent state.  R_SetViewSize sets setsizeneeded,
    // so the next D_Display rebuilds the view tables and repaints the border
    // background at the new resolution.
    ST_SetRes();				// status bar background buffer + refresh
    R_SetViewSize (screenblocks, detailLevel);

    // Grow/shrink the window to match the new resolution.
    I_ApplyAspect();
}


void I_InitGraphics(void)
{

    static int	firsttime=1;
    Uint32	window_flags = 0;
    int		w, h, startscale;

    if (!firsttime)
	return;
    firsttime = 0;

    // Fullcolor framebuffers, sized for the maximum internal resolution so
    // they never need reallocating on a resolution change.
    screen32  = malloc (MAXWIDTH*MAXHEIGHT*sizeof(Uint32));
    view8snap = malloc (MAXWIDTH*MAXHEIGHT);

    // Grab the mouse by default: relative-motion mode drives turning and keeps
    // the cursor confined to the window (essential in windowed mode).  Focus
    // changes release/re-grab it (see I_GetEvent) so alt-tab still works.
    // -nograbmouse opts out.
    grabMouse = !M_CheckParm("-nograbmouse");

    // screen_aspect / hires / fullscreen_mode come from the config file
    // (loaded by M_LoadDefaults before this runs).  Optional -aspect overrides.
    screen_aspect &= 3;
    {
	int p = M_CheckParm("-aspect");
	if (p && p < myargc-1)
	    screen_aspect = atoi(myargv[p+1]) & 3;
    }

    // Initial resolution scale (also drives the window size).  Defaults to the
    // saved value; -1..-4 / -render N override.  Changeable later in the menu.
    startscale = hires;
    if (M_CheckParm("-1")) startscale = 1;
    if (M_CheckParm("-2")) startscale = 2;
    if (M_CheckParm("-3")) startscale = 3;
    if (M_CheckParm("-4")) startscale = 4;
    {
	int p = M_CheckParm("-render");
	if (p && p < myargc-1)
	    startscale = atoi(myargv[p+1]);
    }
    if (startscale < 1) startscale = 1;
    if (startscale > 6) startscale = 6;

    // Initial window size = resolution width, height for the chosen aspect.
    w = BASE_WIDTH * startscale;
    if (aspects[screen_aspect].num)
	h = w * aspects[screen_aspect].den / aspects[screen_aspect].num;
    else
	h = BASE_HEIGHT * startscale;

    window_flags |= SDL_WINDOW_RESIZABLE;
    if (fullscreen_mode || M_CheckParm("-fullscreen"))
    {
        window_flags |= SDL_WINDOW_FULLSCREEN;
	fullscreen_mode = 1;
    }

    window = SDL_CreateWindow("SDLDoom-SDL3", w, h, window_flags);
    if ( window == NULL )
	I_Error("Could not create %dx%d window: %s", w, h, SDL_GetError());

    renderer = SDL_CreateRenderer(window, NULL);
    if ( renderer == NULL )
	I_Error("Could not create renderer: %s", SDL_GetError());

    I_CreateTexture();

    SDL_HideCursor();
    if (grabMouse)
	SDL_SetWindowRelativeMouseMode(window, true);

    // screens[0..3] are allocated by V_Init at the maximum resolution; the
    // 3D view, HUD etc. render into screens[0] at the current SCREENWIDTH.
    // V_SetRes also sizes the window to match (via I_ApplyAspect).
    V_SetRes(startscale);
}

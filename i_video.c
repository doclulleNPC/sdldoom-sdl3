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

// Fake mouse handling.
boolean		grabMouse;

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply pixels.
// (SDL3 scales the texture to the window for us; multiply only sizes the
//  initial window.)
static int	multiply=1;


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

	for (x=0 ; x<SCREENWIDTH ; x++)
	    dst[x] = palette[src[x]];
    }

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
}


void I_InitGraphics(void)
{

    static int	firsttime=1;
    Uint32	window_flags = 0;
    int		w, h;

    if (!firsttime)
	return;
    firsttime = 0;

    if (M_CheckParm("-2"))
	multiply = 2;

    if (M_CheckParm("-3"))
	multiply = 3;

    // check if the user wants to grab the mouse (quite unnice)
    grabMouse = !!M_CheckParm("-grabmouse");

    w = SCREENWIDTH * multiply;
    h = SCREENHEIGHT * multiply;

    if (!!M_CheckParm("-fullscreen"))
        window_flags |= SDL_WINDOW_FULLSCREEN;

    window = SDL_CreateWindow("SDL DOOM! v1.10", w, h, window_flags);
    if ( window == NULL )
	I_Error("Could not create %dx%d window: %s", w, h, SDL_GetError());

    renderer = SDL_CreateRenderer(window, NULL);
    if ( renderer == NULL )
	I_Error("Could not create renderer: %s", SDL_GetError());

    // Always present at the native DOOM resolution, letterboxed and
    // pixel-doubled by the renderer to fill the window.
    SDL_SetRenderLogicalPresentation(renderer, SCREENWIDTH, SCREENHEIGHT,
				     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_STREAMING,
				SCREENWIDTH, SCREENHEIGHT);
    if ( texture == NULL )
	I_Error("Could not create texture: %s", SDL_GetError());
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    SDL_HideCursor();
    if (grabMouse)
	SDL_SetWindowRelativeMouseMode(window, true);

    // The DOOM! code expects screens[0] to be valid at all times, so always
    // give it its own 8-bit 320x200 buffer.
    screens[0] = (unsigned char *) malloc (SCREENWIDTH * SCREENHEIGHT);
    if ( screens[0] == NULL )
	I_Error("Couldn't allocate screen memory");
}

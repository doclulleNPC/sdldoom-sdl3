// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Quake-style drop-down console (MOD addition, not part of vanilla DOOM).
//
//-----------------------------------------------------------------------------

#ifndef __C_CONSOLE__
#define __C_CONSOLE__

#include "d_event.h"

extern boolean	consoleactive;

void	C_Init (void);
boolean	C_Responder (event_t* ev);	// true if the event was consumed
void	C_Drawer (void);		// translucent backdrop into the 8-bit LFB
void	C_Printf (const char* fmt, ...);

// Accessors for the SDL3-font text overlay (I_DrawConsole in i_video.c).  The
// backdrop is drawn by C_Drawer into the framebuffer; the text is drawn on top
// by the platform layer so it can use SDL's font instead of the bitmap hu_font.
int		C_BaseHeight (void);	// drop height in 320x200 rows, 0 = closed
const char*	C_InputLine (void);	// "]input_" with blinking cursor (static buf)
int		C_ScrollCount (void);	// number of scrollback lines
const char*	C_ScrollLine (int i);	// i=0 newest; NULL if out of range

#endif

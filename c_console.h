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
void	C_Drawer (void);		// overlay, drawn last in D_Display
void	C_Printf (const char* fmt, ...);

#endif

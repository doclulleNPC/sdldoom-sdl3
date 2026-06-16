// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Quake-style drop-down console.  Toggled with the backquote/tilde key,
//	drops over the top of the screen, shows a scrollback log and an input
//	line, and runs a small set of commands.  This is a MOD addition; none
//	of it exists in vanilla DOOM.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_player.h"
#include "v_video.h"
#include "g_game.h"
#include "i_system.h"

#include "c_console.h"

// from m_menu.c -- draws hu_font text in BASE (320x200) coords, hires-aware.
extern void	M_WriteText (int x, int y, char* string);
// from r_data.c -- 34 light/dark levels of the palette (index*256 + colour).
extern byte*	colormaps;

#define CON_TOGGLE	'`'		// backquote / tilde
#define CON_HEIGHT	120		// console height in BASE (200-tall) rows
#define CON_DARK	23		// colormap level used to dim the backdrop
#define CON_LINES	256		// scrollback ring size
#define CON_LINELEN	100
#define CON_INPUTLEN	78
#define CON_STEP	12		// drop/raise speed (base rows per frame)

boolean		consoleactive = false;

static char	conlines[CON_LINES][CON_LINELEN];
static int	conhead;		// index of next slot to write
static int	concount;		// number of stored lines (<= CON_LINES)

static char	coninput[CON_INPUTLEN+1];
static int	coninlen;

static int	conanim;		// current drop height in base rows (0..CON_HEIGHT)
static int	conblink;		// cursor blink counter


//
// Scrollback
//
static void C_AddLine (const char* s)
{
    strncpy (conlines[conhead], s, CON_LINELEN-1);
    conlines[conhead][CON_LINELEN-1] = 0;
    conhead = (conhead+1) % CON_LINES;
    if (concount < CON_LINES)
	concount++;
}

void C_Printf (const char* fmt, ...)
{
    char	buf[CON_LINELEN];
    va_list	ap;

    va_start (ap, fmt);
    vsnprintf (buf, sizeof(buf), fmt, ap);
    va_end (ap);
    C_AddLine (buf);
}

void C_Init (void)
{
    conhead = concount = 0;
    coninput[0] = 0;
    coninlen = 0;
    conanim = 0;
    C_Printf ("SDLDoom-SDL3 console.  Type 'help'.");
}


//
// Commands
//
static boolean InLevel (void)
{
    return gamestate == GS_LEVEL && players[consoleplayer].mo != NULL;
}

static void C_Warp (char* arg)
{
    int	e = 1, m = 0;

    if (!arg || !arg[0])
    {
	C_Printf ("usage: map MAPxx  |  map ExMy  |  warp <n>");
	return;
    }

    if (arg[0] == 'm' || arg[0] == 'M')		// "MAP07"
	m = atoi (arg+3);
    else if (arg[0] == 'e' || arg[0] == 'E')	// "E2M4"
	sscanf (arg, "%*c%d%*c%d", &e, &m);
    else					// bare number
	m = atoi (arg);

    if (gamemode == commercial)
	e = 1;

    if (m < 1)
    {
	C_Printf ("bad map '%s'", arg);
	return;
    }

    C_Printf ("warping to %s...", arg);
    consoleactive = false;
    G_DeferedInitNew (gameskill, e, m);
}

static void C_Execute (char* line)
{
    char	cmd[32];
    char	arg[64];
    int		n, i;
    player_t*	plyr = &players[consoleplayer];

    arg[0] = 0;
    n = sscanf (line, "%31s %63s", cmd, arg);
    if (n < 1)
	return;

    for (i = 0; cmd[i]; i++)
	cmd[i] = tolower (cmd[i]);

    if (!strcmp (cmd, "help"))
    {
	C_Printf ("commands:");
	C_Printf ("  god  noclip  give  kill");
	C_Printf ("  map <MAPxx|ExMy>  warp <n>");
	C_Printf ("  clear  quit");
    }
    else if (!strcmp (cmd, "clear"))
    {
	conhead = concount = 0;
    }
    else if (!strcmp (cmd, "quit") || !strcmp (cmd, "exit"))
    {
	I_Quit ();
    }
    else if (!strcmp (cmd, "map") || !strcmp (cmd, "warp"))
    {
	C_Warp (arg);
    }
    else if (!InLevel ())
    {
	C_Printf ("'%s' needs an active game", cmd);
    }
    else if (!strcmp (cmd, "god"))
    {
	plyr->cheats ^= CF_GODMODE;
	if (plyr->cheats & CF_GODMODE)
	{
	    plyr->mo->health = 100;
	    plyr->health = 100;
	}
	C_Printf ("god mode %s", (plyr->cheats & CF_GODMODE) ? "ON" : "OFF");
    }
    else if (!strcmp (cmd, "noclip"))
    {
	plyr->cheats ^= CF_NOCLIP;
	C_Printf ("noclip %s", (plyr->cheats & CF_NOCLIP) ? "ON" : "OFF");
    }
    else if (!strcmp (cmd, "give"))
    {
	plyr->armorpoints = 200;
	plyr->armortype = 2;
	for (i = 0; i < NUMWEAPONS; i++)
	    plyr->weaponowned[i] = true;
	for (i = 0; i < NUMAMMO; i++)
	    plyr->ammo[i] = plyr->maxammo[i];
	for (i = 0; i < NUMCARDS; i++)
	    plyr->cards[i] = true;
	C_Printf ("gave weapons, ammo, keys and armor");
    }
    else if (!strcmp (cmd, "kill"))
    {
	plyr->mo->health = 0;
	plyr->health = 0;
	C_Printf ("ouch.");
    }
    else
    {
	C_Printf ("unknown command '%s'", cmd);
    }
}


//
// Input
//
boolean C_Responder (event_t* ev)
{
    int	c;

    if (ev->type != ev_keydown && ev->type != ev_keyup)
	return false;

    // Toggle works from anywhere.
    if (ev->type == ev_keydown && ev->data1 == CON_TOGGLE)
    {
	consoleactive = !consoleactive;
	return true;
    }

    if (!consoleactive)
	return false;

    if (ev->type != ev_keydown)
	return true;			// swallow key-ups while open

    c = ev->data1;
    switch (c)
    {
      case KEY_ESCAPE:
	consoleactive = false;
	break;

      case KEY_ENTER:
	C_Printf ("]%s", coninput);
	C_Execute (coninput);
	coninput[0] = 0;
	coninlen = 0;
	break;

      case KEY_BACKSPACE:
	if (coninlen > 0)
	    coninput[--coninlen] = 0;
	break;

      default:
	if (c >= ' ' && c < 127 && coninlen < CON_INPUTLEN)
	{
	    coninput[coninlen++] = c;
	    coninput[coninlen] = 0;
	}
	break;
    }
    return true;
}


//
// Render (called last in D_Display, on top of everything)
//
void C_Drawer (void)
{
    int		target;
    int		hpix;
    int		x, y, i;
    char	buf[CON_INPUTLEN+4];

    // slide toward the open/closed position
    target = consoleactive ? CON_HEIGHT : 0;
    if (conanim < target)
	conanim = (conanim+CON_STEP > target) ? target : conanim+CON_STEP;
    else if (conanim > target)
	conanim = (conanim-CON_STEP < target) ? target : conanim-CON_STEP;

    if (conanim <= 0)
	return;

    // Dim the area the console covers (translucent backdrop via the colormaps).
    hpix = conanim * hires;
    for (y = 0; y < hpix; y++)
    {
	byte* row = screens[0] + y*SCREENWIDTH;
	for (x = 0; x < SCREENWIDTH; x++)
	    row[x] = colormaps[CON_DARK*256 + row[x]];
    }
    // bottom edge
    {
	byte* row = screens[0] + (hpix-1)*SCREENWIDTH;
	for (x = 0; x < SCREENWIDTH; x++)
	    row[x] = colormaps[31*256 + row[x]];
    }

    // input line at the bottom of the console (base coords)
    conblink++;
    snprintf (buf, sizeof(buf), "]%s%s", coninput, (conblink & 8) ? "_" : "");
    y = conanim - 10;
    M_WriteText (4, y, buf);

    // scrollback above it, newest first
    y -= 10;
    for (i = 0; i < concount && y > -8; i++)
    {
	int idx = (conhead-1-i+CON_LINES) % CON_LINES;
	M_WriteText (4, y, conlines[idx]);
	y -= 8;
    }
}

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
//	System specific network interface stuff.
//
//	SDL3 / Win32 port note:
//	The original implementation used BSD/Unix sockets (sys/socket.h,
//	netinet/in.h, gethostbyname, ...) which are not available on a plain
//	MinGW build.  Networked multiplayer is not supported by this port, so
//	this file is reduced to a portable single-player stub.  Single player
//	never enters the send/get path (D_net skips the local node), so the
//	packet routines simply I_Error if they are ever reached.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "m_argv.h"

#include "doomstat.h"

#include "i_net.h"


//
// I_InitNetwork
//
void I_InitNetwork (void)
{
    doomcom = malloc (sizeof (*doomcom) );
    memset (doomcom, 0, sizeof(*doomcom) );

    if (M_CheckParm ("-net"))
	I_Error ("Networked multiplayer is not supported by this SDL3 port.");

    // single player game
    netgame = false;
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes = 1;
    doomcom->deathmatch = false;
    doomcom->consoleplayer = 0;
    doomcom->ticdup = 1;
    doomcom->extratics = 0;
}


void I_NetCmd (void)
{
    // Only reached in a netgame, which this port does not support.
    I_Error ("I_NetCmd: networking not supported in this build");
}

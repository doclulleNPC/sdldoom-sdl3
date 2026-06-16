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
// DESCRIPTION:
//	Created by the sound utility written by Dave Taylor.
//	Kept as a sample, DOOM2  sounds. Frozen.
//
//-----------------------------------------------------------------------------

#ifndef __SOUNDS__
#define __SOUNDS__


//
// SoundFX struct.
//
typedef struct sfxinfo_struct	sfxinfo_t;

struct sfxinfo_struct
{
    // up to 6-character name
    char*	name;

    // Sfx singularity (only one at a time)
    int		singularity;

    // Sfx priority
    int		priority;

    // referenced sound if a link
    sfxinfo_t*	link;

    // pitch if a link
    int		pitch;

    // volume if a link
    int		volume;

    // sound data
    void*	data;

    // this is checked every second to see if sound
    // can be thrown out (if 0, then decrement, if -1,
    // then throw out, if > 0, then it is in use)
    int		usefulness;

    // lump number of sfx
    int		lumpnum;		
};




//
// MusicInfo struct.
//
typedef struct
{
    // up to 6-character name
    char*	name;

    // lump number of music
    int		lumpnum;
    
    // music data
    void*	data;

    // music handle once registered
    int handle;
    
} musicinfo_t;




// the complete set of sound effects
extern sfxinfo_t	S_sfx[];

// the complete set of music
extern musicinfo_t	S_music[];

//
// Identifiers for all music in game.
//

typedef enum
{
    mus_None,
    mus_e1m1,
    mus_e1m2,
    mus_e1m3,
    mus_e1m4,
    mus_e1m5,
    mus_e1m6,
    mus_e1m7,
    mus_e1m8,
    mus_e1m9,
    mus_e2m1,
    mus_e2m2,
    mus_e2m3,
    mus_e2m4,
    mus_e2m5,
    mus_e2m6,
    mus_e2m7,
    mus_e2m8,
    mus_e2m9,
    mus_e3m1,
    mus_e3m2,
    mus_e3m3,
    mus_e3m4,
    mus_e3m5,
    mus_e3m6,
    mus_e3m7,
    mus_e3m8,
    mus_e3m9,
    mus_inter,
    mus_intro,
    mus_bunny,
    mus_victor,
    mus_introa,
    mus_runnin,
    mus_stalks,
    mus_countd,
    mus_betwee,
    mus_doom,
    mus_the_da,
    mus_shawn,
    mus_ddtblu,
    mus_in_cit,
    mus_dead,
    mus_stlks2,
    mus_theda2,
    mus_doom2,
    mus_ddtbl2,
    mus_runni2,
    mus_dead2,
    mus_stlks3,
    mus_romero,
    mus_shawn2,
    mus_messag,
    mus_count2,
    mus_ddtbl3,
    mus_ampie,
    mus_theda3,
    mus_adrian,
    mus_messg2,
    mus_romer2,
    mus_tense,
    mus_shawn3,
    mus_openin,
    mus_evil,
    mus_ultima,
    mus_read_m,
    mus_dm2ttl,
    mus_dm2int,
    NUMMUSIC
} musicenum_t;


//
// Identifiers for all sfx in game.
//

typedef enum
{
    sfx_None,
    sfx_pistol,
    sfx_shotgn,
    sfx_sgcock,
    sfx_dshtgn,
    sfx_dbopn,
    sfx_dbcls,
    sfx_dbload,
    sfx_plasma,
    sfx_bfg,
    sfx_sawup,
    sfx_sawidl,
    sfx_sawful,
    sfx_sawhit,
    sfx_rlaunc,
    sfx_rxplod,
    sfx_firsht,
    sfx_firxpl,
    sfx_pstart,
    sfx_pstop,
    sfx_doropn,
    sfx_dorcls,
    sfx_stnmov,
    sfx_swtchn,
    sfx_swtchx,
    sfx_plpain,
    sfx_dmpain,
    sfx_popain,
    sfx_vipain,
    sfx_mnpain,
    sfx_pepain,
    sfx_slop,
    sfx_itemup,
    sfx_wpnup,
    sfx_oof,
    sfx_telept,
    sfx_posit1,
    sfx_posit2,
    sfx_posit3,
    sfx_bgsit1,
    sfx_bgsit2,
    sfx_sgtsit,
    sfx_cacsit,
    sfx_brssit,
    sfx_cybsit,
    sfx_spisit,
    sfx_bspsit,
    sfx_kntsit,
    sfx_vilsit,
    sfx_mansit,
    sfx_pesit,
    sfx_sklatk,
    sfx_sgtatk,
    sfx_skepch,
    sfx_vilatk,
    sfx_claw,
    sfx_skeswg,
    sfx_pldeth,
    sfx_pdiehi,
    sfx_podth1,
    sfx_podth2,
    sfx_podth3,
    sfx_bgdth1,
    sfx_bgdth2,
    sfx_sgtdth,
    sfx_cacdth,
    sfx_skldth,
    sfx_brsdth,
    sfx_cybdth,
    sfx_spidth,
    sfx_bspdth,
    sfx_vildth,
    sfx_kntdth,
    sfx_pedth,
    sfx_skedth,
    sfx_posact,
    sfx_bgact,
    sfx_dmact,
    sfx_bspact,
    sfx_bspwlk,
    sfx_vilact,
    sfx_noway,
    sfx_barexp,
    sfx_punch,
    sfx_hoof,
    sfx_metal,
    sfx_chgun,
    sfx_tink,
    sfx_bdopn,
    sfx_bdcls,
    sfx_itmbk,
    sfx_flame,
    sfx_flamst,
    sfx_getpow,
    sfx_bospit,
    sfx_boscub,
    sfx_bossit,
    sfx_bospn,
    sfx_bosdth,
    sfx_manatk,
    sfx_mandth,
    sfx_sssit,
    sfx_ssdth,
    sfx_keenpn,
    sfx_keendt,
    sfx_skeact,
    sfx_skesit,
    sfx_skeatk,
    sfx_radio,
    // --- footstep sounds (footsteps.wad) ---
    sfx_stp1,
    sfx_stp2,
    sfx_stp3,
    sfx_stp4,
    sfx_wtr1,
    sfx_wtr2,
    sfx_wtr3,
    sfx_wtr4,
    sfx_carp1,
    sfx_carp2,
    sfx_carp3,
    sfx_dirt1,
    sfx_dirt2,
    sfx_dirt3,
    sfx_dirt4,
    sfx_dirt5,
    sfx_dirt6,
    sfx_grav1,
    sfx_grav2,
    sfx_grav3,
    sfx_grav4,
    sfx_hard1,
    sfx_hard2,
    sfx_hard3,
    sfx_hard4,
    sfx_hard5,
    sfx_hard6,
    sfx_met01,
    sfx_met02,
    sfx_met03,
    sfx_met04,
    sfx_met05,
    sfx_met06,
    sfx_met201,
    sfx_met202,
    sfx_met203,
    sfx_met204,
    sfx_rock1,
    sfx_rock2,
    sfx_rock3,
    sfx_rock4,
    sfx_rock5,
    sfx_rock6,
    sfx_rock7,
    sfx_rock8,
    sfx_snow1,
    sfx_snow2,
    sfx_snow3,
    sfx_snow4,
    sfx_snow5,
    sfx_snow6,
    sfx_tile01,
    sfx_tile02,
    sfx_tile03,
    sfx_tile04,
    sfx_tile05,
    sfx_tile06,
    sfx_tile21,
    sfx_tile22,
    sfx_tile23,
    sfx_tile24,
    sfx_tile25,
    sfx_tile26,
    sfx_tile27,
    sfx_tile28,
    sfx_wood1,
    sfx_wood2,
    sfx_wood3,
    sfx_wood4,
    sfx_slime1,
    sfx_slime2,
    sfx_slime3,
    sfx_slime4,
    sfx_slimy1,
    sfx_slimy2,
    sfx_slimy3,
    sfx_slimy4,
    sfx_lava1,
    sfx_lava2,
    sfx_lava3,
    sfx_lava4,
    NUMSFX
} sfxenum_t;

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------


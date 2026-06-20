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
//	System interface for sound.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_unix.c,v 1.5 1997/02/03 22:45:10 b1 Exp $";

#include <math.h>
#include <stdlib.h>

#include <SDL3/SDL.h>

#include "z_zone.h"

#include "m_swap.h"
#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "doomdef.h"

// MOD: OGG music decoding (declarations only; stb_vorbis.c is its own TU).
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#include "i_mus.h"	// MOD: native MUS playback (OPL-style FM synth)


// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.


// Needed for calling the actual sound output.
static int SAMPLECOUNT=		512;
#define NUM_CHANNELS		8

#define SAMPLERATE		11025	// Hz

// The actual lengths of all sound effects.
int 		lengths[NUMSFX];

// The actual output device.
int	audio_fd;

// SDL3 audio output: a stream bound to the default playback device, fed by
// I_SDLAudioCallback below.  (SDL3 replaced the SDL1/2 push/callback audio API.)
static SDL_AudioStream*	audiostream = NULL;
static Uint8*		mixbuffer = NULL;	// one slice of mixed S16 stereo
static int		mixbufferbytes = 0;


// The channel step amount...
unsigned int	channelstep[NUM_CHANNELS];
// ... and a 0.16 bit remainder of last step.
unsigned int	channelstepremainder[NUM_CHANNELS];


// The channel data pointers, start and end.
unsigned char*	channels[NUM_CHANNELS];
unsigned char*	channelsend[NUM_CHANNELS];


// Time/gametic that the channel started playing,
//  used to determine oldest, which automatically
//  has lowest priority.
// In case number of active sounds exceeds
//  available channels.
int		channelstart[NUM_CHANNELS];

// The sound in channel handles,
//  determined on registration,
//  might be used to unregister/stop/modify,
//  currently unused.
int 		channelhandles[NUM_CHANNELS];

// SFX id of the playing sound effect.
// Used to catch duplicates (like chainsaw).
int		channelids[NUM_CHANNELS];			

// Pitch to stepping lookup, unused.
int		steptable[256];

// Volume lookups.
int		vol_lookup[128*256];

// Hardware left and right channel volume lookup.
int*		channelleftvol_lookup[NUM_CHANNELS];
int*		channelrightvol_lookup[NUM_CHANNELS];



//
// This function loads the sound data from the WAD lump,
//  for single sound.
//
void*
getsfx
( char*         sfxname,
  int*          len )
{
    unsigned char*      sfx;
    unsigned char*      paddedsfx;
    int                 i;
    int                 size;
    int                 paddedsize;
    char                name[20];
    int                 sfxlump;

    
    // Get the sound data from the WAD, allocate lump
    //  in zone memory.
    sprintf(name, "ds%s", sfxname);

    // Now, there is a severe problem with the
    //  sound handling, in it is not (yet/anymore)
    //  gamemode aware. That means, sounds from
    //  DOOM II will be requested even with DOOM
    //  shareware.
    // The sound list is wired into sounds.c,
    //  which sets the external variable.
    // I do not do runtime patches to that
    //  variable. Instead, we will use a
    //  default sound for replacement.
    if ( W_CheckNumForName(name) == -1 )
      sfxlump = W_GetNumForName("dspistol");
    else
      sfxlump = W_GetNumForName(name);
    
    size = W_LumpLength( sfxlump );

    // Debug.
    // fprintf( stderr, "." );
    //fprintf( stderr, " -loading  %s (lump %d, %d bytes)\n",
    //	     sfxname, sfxlump, size );
    //fflush( stderr );
    
    sfx = (unsigned char*)W_CacheLumpNum( sfxlump, PU_STATIC );

    // Pads the sound effect out to the mixing buffer size.
    // The original realloc would interfere with zone memory.
    paddedsize = ((size-8 + (SAMPLECOUNT-1)) / SAMPLECOUNT) * SAMPLECOUNT;

    // Allocate from zone memory.
    paddedsfx = (unsigned char*)Z_Malloc( paddedsize+8, PU_STATIC, 0 );
    // ddt: (unsigned char *) realloc(sfx, paddedsize+8);
    // This should interfere with zone memory handling,
    //  which does not kick in in the soundserver.

    // Now copy and pad.
    memcpy(  paddedsfx, sfx, size );
    for (i=size ; i<paddedsize+8 ; i++)
        paddedsfx[i] = 128;

    // Remove the cached lump.
    Z_Free( sfx );
    
    // Preserve padded length.
    *len = paddedsize;

    // Return allocated padded data.
    return (void *) (paddedsfx + 8);
}





//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
int
addsfx
( int		sfxid,
  int		volume,
  int		step,
  int		seperation )
{
    static unsigned short	handlenums = 0;
 
    int		i;
    int		rc = -1;
    
    int		oldest = gametic;
    int		oldestnum = 0;
    int		slot;

    int		rightvol;
    int		leftvol;

    // Chainsaw troubles.
    // Play these sound effects only one at a time.
    if ( sfxid == sfx_sawup
	 || sfxid == sfx_sawidl
	 || sfxid == sfx_sawful
	 || sfxid == sfx_sawhit
	 || sfxid == sfx_stnmov
	 || sfxid == sfx_pistol	 )
    {
	// Loop all channels, check.
	for (i=0 ; i<NUM_CHANNELS ; i++)
	{
	    // Active, and using the same SFX?
	    if ( (channels[i])
		 && (channelids[i] == sfxid) )
	    {
		// Reset.
		channels[i] = 0;
		// We are sure that iff,
		//  there will only be one.
		break;
	    }
	}
    }

    // Loop all channels to find oldest SFX.
    for (i=0; (i<NUM_CHANNELS) && (channels[i]); i++)
    {
	if (channelstart[i] < oldest)
	{
	    oldestnum = i;
	    oldest = channelstart[i];
	}
    }

    // Tales from the cryptic.
    // If we found a channel, fine.
    // If not, we simply overwrite the first one, 0.
    // Probably only happens at startup.
    if (i == NUM_CHANNELS)
	slot = oldestnum;
    else
	slot = i;

    // Okay, in the less recent channel,
    //  we will handle the new SFX.
    // Set pointer to raw data.
    channels[slot] = (unsigned char *) S_sfx[sfxid].data;
    // Set pointer to end of raw data.
    channelsend[slot] = channels[slot] + lengths[sfxid];

    // Reset current handle number, limited to 0..100.
    if (!handlenums)
	handlenums = 100;

    // Assign current handle number.
    // Preserved so sounds could be stopped (unused).
    channelhandles[slot] = rc = handlenums++;

    // Set stepping???
    // Kinda getting the impression this is never used.
    channelstep[slot] = step;
    // ???
    channelstepremainder[slot] = 0;
    // Should be gametic, I presume.
    channelstart[slot] = gametic;

    // Separation, that is, orientation/stereo.
    //  range is: 1 - 256
    seperation += 1;

    // Per left/right channel.
    //  x^2 seperation,
    //  adjust volume properly.
    volume *= 8;
    leftvol =
	volume - ((volume*seperation*seperation) >> 16); ///(256*256);
    seperation = seperation - 257;
    rightvol =
	volume - ((volume*seperation*seperation) >> 16);	

    // Sanity check, clamp volume.
    if (rightvol < 0 || rightvol > 127)
	I_Error("rightvol out of bounds");
    
    if (leftvol < 0 || leftvol > 127)
	I_Error("leftvol out of bounds");
    
    // Get the proper lookup table piece
    //  for this volume level???
    channelleftvol_lookup[slot] = &vol_lookup[leftvol*256];
    channelrightvol_lookup[slot] = &vol_lookup[rightvol*256];

    // Preserve sound SFX id,
    //  e.g. for avoiding duplicates of chainsaw.
    channelids[slot] = sfxid;

    // You tell me.
    return rc;
}





//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels()
{
  // Init internal lookups (raw data, mixing buffer, channels).
  // This function sets up internal lookups used during
  //  the mixing process. 
  int		i;
  int		j;
    
  int*	steptablemid = steptable + 128;
  
  // Okay, reset internal mixing channels to zero.
  /*for (i=0; i<NUM_CHANNELS; i++)
  {
    channels[i] = 0;
  }*/

  // This table provides step widths for pitch parameters.
  // I fail to see that this is currently used.
  for (i=-128 ; i<128 ; i++)
    steptablemid[i] = (int)(pow(2.0, (i/64.0))*65536.0);
  
  
  // Generates volume lookup tables
  //  which also turn the unsigned samples
  //  into signed samples.
  for (i=0 ; i<128 ; i++)
    for (j=0 ; j<256 ; j++) {
      vol_lookup[i*256+j] = (i*(j-128)*256)/127;
//fprintf(stderr, "vol_lookup[%d*256+%d] = %d\n", i, j, vol_lookup[i*256+j]);
    }
}	

 
void I_SetSfxVolume(int volume)
{
  // Identical to DOS.
  // Basically, this should propagate
  //  the menu/config file setting
  //  to the state variable used in
  //  the mixing.
  snd_SfxVolume = volume;
}

// Music volume (0..15 from the menu); applied to the OGG mix in the callback.
extern int	i_music_gain;		// defined with the music code below
void I_SetMusicVolume(int volume)
{
  snd_MusicVolume = volume;
  i_music_gain = volume;		// 0..15
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    // Don't hard-crash on a missing sound lump (e.g. an optional add-on sound
    // whose WAD wasn't loaded); fall back to the pistol, like getsfx() does.
    if (W_CheckNumForName(namebuf) == -1)
	return W_GetNumForName("dspistol");
    return W_GetNumForName(namebuf);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{

  // UNUSED
  priority = 0;
  
    // Debug.
    //fprintf( stderr, "starting sound %d", id );
    
    // Returns a handle (not used).
    if (audiostream) SDL_LockAudioStream(audiostream);
    id = addsfx( id, vol, steptable[pitch], sep );
    if (audiostream) SDL_UnlockAudioStream(audiostream);

    // fprintf( stderr, "/handle is %d\n", id );
    
    return id;
}



void I_StopSound (int handle)
{
  // You need the handle returned by StartSound.
  // Would be looping all channels,
  //  tracking down the handle,
  //  an setting the channel to zero.
  
  // UNUSED.
  handle = 0;
}


int I_SoundIsPlaying(int handle)
{
    // Ouch.
    return gametic < handle;
}


//
// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the given
//  mixing buffer, and clamping it to the allowed
//  range.
//
// This function currently supports only 16bit.
//
void I_UpdateSound(void *unused, Uint8 *stream, int len);

//
// SDL3 audio stream callback.
// SDL asks for `additional_amount` more bytes; we mix one slice at a time
// (I_UpdateSound always produces exactly mixbufferbytes) and push it.
//
static void SDLCALL
I_SDLAudioCallback
( void			*userdata,
  SDL_AudioStream	*stream,
  int			additional_amount,
  int			total_amount )
{
    while (additional_amount > 0)
    {
	I_UpdateSound(NULL, mixbuffer, mixbufferbytes);
	SDL_PutAudioStreamData(stream, mixbuffer, mixbufferbytes);
	additional_amount -= mixbufferbytes;
    }
}

void I_UpdateSound(void *unused, Uint8 *stream, int len)
{
  // Mix current sound data.
  // Data, from raw sound, for right and left.
  register unsigned int	sample;
  register int		dl;
  register int		dr;
  
  // Pointers in audio stream, left, right, end.
  signed short*		leftout;
  signed short*		rightout;
  signed short*		leftend;
  // Step in stream, left and right, thus two.
  int				step;

  // Mixing channel index.
  int				chan;
    
    // Left and right channel
    //  are in audio stream, alternating.
    leftout = (signed short *)stream;
    rightout = ((signed short *)stream)+1;
    step = 2;

    // Determine end, for left channel only
    //  (right channel is implicit).
    leftend = leftout + SAMPLECOUNT*step;

    // Mix sounds into the mixing buffer.
    // Loop over step*SAMPLECOUNT,
    //  that is 512 values for two channels.
    while (leftout != leftend)
    {
	// Reset left/right value. 
	dl = 0;
	dr = 0;

	// Love thy L2 chache - made this a loop.
	// Now more channels could be set at compile time
	//  as well. Thus loop those  channels.
	for ( chan = 0; chan < NUM_CHANNELS; chan++ )
	{
	    // Check channel, if active.
	    if (channels[ chan ])
	    {
		// Get the raw data from the channel. 
		sample = *channels[ chan ];
		// Add left and right part
		//  for this channel (sound)
		//  to the current data.
		// Adjust volume accordingly.
		dl += channelleftvol_lookup[ chan ][sample];
		dr += channelrightvol_lookup[ chan ][sample];
		// Increment index ???
		channelstepremainder[ chan ] += channelstep[ chan ];
		// MSB is next sample???
		channels[ chan ] += channelstepremainder[ chan ] >> 16;
		// Limit to LSB???
		channelstepremainder[ chan ] &= 65536-1;

		// Check whether we are done.
		if (channels[ chan ] >= channelsend[ chan ])
		    channels[ chan ] = 0;
	    }
	}
	
	// Clamp to range. Left hardware channel.
	// Has been char instead of short.
	// if (dl > 127) *leftout = 127;
	// else if (dl < -128) *leftout = -128;
	// else *leftout = dl;

	if (dl > 0x7fff)
	    *leftout = 0x7fff;
	else if (dl < -0x8000)
	    *leftout = -0x8000;
	else
	    *leftout = dl;

	// Same for right hardware channel.
	if (dr > 0x7fff)
	    *rightout = 0x7fff;
	else if (dr < -0x8000)
	    *rightout = -0x8000;
	else
	    *rightout = dr;

	// Increment current pointers in stream
	leftout += step;
	rightout += step;
    }
}

void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
  // I fail too see that this is used.
  // Would be using the handle to identify
  //  on which channel the sound might be active,
  //  and resetting the channel parameters.

  // UNUSED.
  handle = vol = sep = pitch = 0;
}


void I_ShutdownSound(void)
{
  if (audiostream)
  {
    SDL_DestroyAudioStream(audiostream);	// also closes the bound device
    audiostream = NULL;
  }
  if (mixbuffer)
  {
    free(mixbuffer);
    mixbuffer = NULL;
  }
}


void
I_InitSound()
{ 
  SDL_AudioSpec wanted;
  int i;

  // Secure and configure sound device first.
  fprintf( stderr, "I_InitSound: ");

  // Open the audio device (SDL3: signed 16-bit, native endian, stereo).
  SDL_zero(wanted);
  wanted.freq = SAMPLERATE;
  wanted.format = SDL_AUDIO_S16;
  wanted.channels = 2;

  // One mixed slice = SAMPLECOUNT stereo frames of 16-bit samples.
  mixbufferbytes = SAMPLECOUNT * wanted.channels * (int)sizeof(Sint16);
  mixbuffer = (Uint8 *) malloc(mixbufferbytes);
  if ( mixbuffer == NULL ) {
    fprintf(stderr, "couldn't allocate audio mixing buffer\n");
    return;
  }

  audiostream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                          &wanted, I_SDLAudioCallback, NULL);
  if ( audiostream == NULL ) {
    fprintf(stderr, "couldn't open audio: %s\n", SDL_GetError());
    free(mixbuffer);
    mixbuffer = NULL;
    return;
  }
  fprintf(stderr, " configured audio device with %d samples/slice\n", SAMPLECOUNT);

    
  // Initialize external data (all sounds) at start, keep static.
  fprintf( stderr, "I_InitSound: ");
  
  for (i=1 ; i<NUMSFX ; i++)
  { 
    // Alias? Example is the chaingun sound linked to pistol.
    if (!S_sfx[i].link)
    {
      // Load data from WAD file.
      S_sfx[i].data = getsfx( S_sfx[i].name, &lengths[i] );
    }	
    else
    {
      // Previously loaded already?
      S_sfx[i].data = S_sfx[i].link->data;
      lengths[i] = lengths[(S_sfx[i].link - S_sfx)/sizeof(sfxinfo_t)];
    }
  }

  fprintf( stderr, " pre-cached all sound data\n");
  
  // Finished initialization.
  fprintf(stderr, "I_InitSound: sound module ready\n");
  SDL_ResumeAudioStreamDevice(audiostream);
}




//
// MUSIC API -- OGG playback via stb_vorbis (MOD).
//
// DOOM's own music lumps are MUS/MIDI, which we don't synthesize; only OGG
// replacement lumps (a music pack) actually play.  A non-OGG song registers as
// "no music" and is silently skipped.  Decoded OGG is fed to a second SDL audio
// stream bound to the same device as the SFX, so SDL mixes + resamples it.
//
int			i_music_gain = 15;	// 0..15 (set by I_SetMusicVolume)

static stb_vorbis*	mus_vorbis;	// current OGG decoder, or NULL
static SDL_AudioStream*	mus_stream;	// music stream bound to the SFX device
static int		mus_channels = 2;
static int		mus_loop;
static int		mus_paused;
static int		mus_kind;	// 0 none, 1 OGG (stb_vorbis), 2 MUS (synth)
static int		mus_geninit;	// MUS_Init() done?

void I_InitMusic(void)		{ }
void I_ShutdownMusic(void)	{ }

// SDL pulls more music PCM through here (runs on the audio thread).
static void SDLCALL
I_MusicStreamCallback (void* ud, SDL_AudioStream* s, int additional, int total)
{
    float	gain = mus_paused ? 0.0f : (i_music_gain / 15.0f);

    (void)ud; (void)total;

    if (mus_kind == 2)			// MUS: native synth, S16 stereo
    {
	short	buf[1024];		// 512 stereo frames
	while (additional > 0)
	{
	    int	frames = additional / (int)(2*sizeof(short));
	    int	i;
	    if (frames > 512) frames = 512;
	    if (frames <= 0) break;
	    MUS_Render (buf, frames);
	    for (i = 0 ; i < frames*2 ; i++)
		buf[i] = (short)(buf[i] * gain);
	    SDL_PutAudioStreamData (s, buf, frames*2*(int)sizeof(short));
	    additional -= frames*2*(int)sizeof(short);
	}
	return;
    }

    // OGG: stb_vorbis, float interleaved
    {
	float	buf[2048];
	int	framecap = (int)(sizeof(buf)/sizeof(float)) / mus_channels;
	while (additional > 0)
	{
	    int	frames = additional / (int)(mus_channels*sizeof(float));
	    int	got, i, n;

	    if (frames > framecap) frames = framecap;
	    if (frames <= 0) break;

	    got = mus_vorbis ? stb_vorbis_get_samples_float_interleaved
			       (mus_vorbis, mus_channels, buf, frames*mus_channels) : 0;
	    if (got <= 0)
	    {
		if (mus_vorbis && mus_loop)
		{
		    stb_vorbis_seek_start (mus_vorbis);
		    got = stb_vorbis_get_samples_float_interleaved
			    (mus_vorbis, mus_channels, buf, frames*mus_channels);
		}
		if (got <= 0)
		{
		    memset (buf, 0, frames*mus_channels*sizeof(float));
		    got = frames;
		}
	    }
	    n = got*mus_channels;
	    for (i = 0 ; i < n ; i++)
		buf[i] *= gain;
	    SDL_PutAudioStreamData (s, buf, n*(int)sizeof(float));
	    additional -= n*(int)sizeof(float);
	}
    }
}

int I_RegisterSong(void* data, int length)
{
    const unsigned char*	p = data;
    int				err;

    mus_kind = 0;

    // OGG replacement music?
    if (length >= 4 && p[0]=='O' && p[1]=='g' && p[2]=='g' && p[3]=='S')
    {
	mus_vorbis = stb_vorbis_open_memory (p, length, &err, NULL);
	if (mus_vorbis) { mus_kind = 1; return 1; }
	return 0;
    }

    // Native MUS via the FM synth.
    if (!mus_geninit) { MUS_Init (); mus_geninit = 1; }
    if (MUS_Register (p, length)) { mus_kind = 2; return 2; }

    return 0;	// e.g. raw MIDI -- unsupported
}

void I_PlaySong(int handle, int looping)
{
    SDL_AudioSpec	src, dst;

    if (!handle)
	return;

    dst.format = SDL_AUDIO_S16;   dst.channels = 2;   dst.freq = SAMPLERATE;
    mus_paused = 0;

    if (mus_kind == 1)			// OGG
    {
	stb_vorbis_info info = stb_vorbis_get_info (mus_vorbis);
	mus_channels = info.channels;
	mus_loop = looping;
	src.format = SDL_AUDIO_F32; src.channels = mus_channels; src.freq = info.sample_rate;
    }
    else if (mus_kind == 2)		// MUS
    {
	MUS_Start (looping);
	src.format = SDL_AUDIO_S16; src.channels = 2; src.freq = SAMPLERATE;
    }
    else
	return;

    mus_stream = SDL_CreateAudioStream (&src, &dst);
    if (!mus_stream)
	return;
    SDL_SetAudioStreamGetCallback (mus_stream, I_MusicStreamCallback, NULL);
    SDL_BindAudioStream (SDL_GetAudioStreamDevice(audiostream), mus_stream);
}

void I_PauseSong (int handle)	{ (void)handle; mus_paused = 1; }
void I_ResumeSong (int handle)	{ (void)handle; mus_paused = 0; }

void I_StopSong(int handle)
{
    (void)handle;
    if (mus_stream)			// destroying the stream stops the callback
    {
	SDL_DestroyAudioStream (mus_stream);
	mus_stream = NULL;
    }
    if (mus_kind == 2)
	MUS_Stop ();
}

void I_UnRegisterSong(int handle)
{
    (void)handle;
    if (mus_stream)
    {
	SDL_DestroyAudioStream (mus_stream);
	mus_stream = NULL;
    }
    if (mus_kind == 2)
	MUS_Stop ();
    if (mus_vorbis)
    {
	stb_vorbis_close (mus_vorbis);
	mus_vorbis = NULL;
    }
    mus_kind = 0;
}

int I_QrySongPlaying(int handle)
{
    (void)handle;
    return mus_kind != 0;
}


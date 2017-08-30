#ifndef __GCMODPLAY_H__
#define __GCMODPLAY_H__

#include <gctypes.h>
#include "modplay/modplay.h"

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _modsndbuf {
	u32 freq;
	u16 fmt;
	u32 chans;
	f32 samples;
	void *usr_data;
	void (*callback)(void *,u8 *,u32);
} MODSNDBUF;

typedef struct _modplay {
	MOD mod;
	BOOL playing,paused;
	BOOL bits,stereo,manual_polling;
	u32 playfreq,numSFXChans;
	MODSNDBUF soundBuf;
} MODPlay;

void MODPlay_Init(MODPlay *mod);
s32 MODPlay_SetFrequency(MODPlay *mod,u32 freq);
void MODPlay_SetStereo(MODPlay *mod,BOOL stereo);
s32 MODPlay_SetMOD(MODPlay *mod,const void *mem);
void MODPlay_Unload(MODPlay *mod);
s32 MODPlay_AllocSFXChannels(MODPlay *mod,u32 sfxchans);
s32 MODPlay_Start(MODPlay *mod);
s32 MODPlay_Stop(MODPlay *mod);
s32 MODPlay_TriggerNote(MODPlay *mod,u32 chan,u8 inst,u16 freq,u8 vol);
s32 MODPlay_Pause(MODPlay *mod,BOOL);
void MODPlay_SetVolume(MODPlay * mod, s32 musicvolume, s32 sfxvolume);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif

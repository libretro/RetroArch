#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <unistd.h>
#endif
#ifdef GEKKO
#include <ogcsys.h>
#endif
#include "definitions.h"
#include "wiiuse_internal.h"
#include "speaker.h"

#define WENCMIN(a,b)		((a)>(b)?(b):(a))
#define ABS(x)				((s32)(x)>0?(s32)(x):-((s32)(x)))

static const int yamaha_indexscale[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	230, 230, 230, 230, 307, 409, 512, 614
};

static const int yamaha_difflookup[] = {
	1, 3, 5, 7, 9, 11, 13, 15,
	-1, -3, -5, -7, -9, -11, -13, -15
};

static ubyte __wiiuse_speaker_vol = 0x40;
static ubyte __wiiuse_speaker_defconf[7] = { 0x00,0x00,0xD0,0x07,0x40,0x0C,0x0E };

static __inline__ short wenc_clip_short(int a)
{
	if((a+32768)&~65535) return (a>>31)^32767;
	else return a;
}

static __inline__ int wenc_clip(int a,int amin,int amax)
{
	if(a<amin) return amin;
	else if(a>amax) return amax;
	else return a;
}

ubyte wencdata(WENCStatus *info,short sample)
{
	int nibble,delta;

	if(!info->step) {
		info->predictor = 0;
		info->step = 127;
	}

	delta = sample - info->predictor;
	nibble = WENCMIN(7,(ABS(delta)*4)/info->step) + ((delta<0)*8);

	info->predictor += ((info->step*yamaha_difflookup[nibble])/8);
	info->predictor = wenc_clip_short(info->predictor);
	info->step = (info->step*yamaha_indexscale[nibble])>>8;
	info->step = wenc_clip(info->step,127,24576);

	return nibble;
}

void wiiuse_set_speaker(struct wiimote_t *wm,int status)
{
	ubyte conf[7];
	ubyte buf = 0x00;

	if(!wm) return;

	if(!WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) {
		WIIUSE_DEBUG("Tried to enable speaker, will wait until handshake finishes.\n");
		if(status)
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_SPEAKER_INIT);
		else
			WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_SPEAKER_INIT);
		return;
	}

	if(status) {
		if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER)) {
			wiiuse_status(wm,NULL);
			return;
		}
	} else {
		if(!WIIMOTE_IS_SET(wm,WIIMOTE_STATE_SPEAKER)) {
			wiiuse_status(wm,NULL);
			return;
		}
	}


	buf = 0x04;
	wiiuse_sendcmd(wm,WM_CMD_SPEAKER_MUTE,&buf,1,NULL);

	if (!status) {
		WIIUSE_DEBUG("Disabled speaker for wiimote id %i.", wm->unid);

		buf = 0x01;
		wiiuse_write_data(wm,WM_REG_SPEAKER_REG1,&buf,1,NULL);

		buf = 0x00;
		wiiuse_write_data(wm,WM_REG_SPEAKER_REG3,&buf,1,NULL);

		buf = 0x00;
		wiiuse_sendcmd(wm,WM_CMD_SPEAKER_ENABLE,&buf,1,NULL);

		wiiuse_status(wm,NULL);
		return;
	}

	memcpy(conf,__wiiuse_speaker_defconf,7);

	buf = 0x04;
	wiiuse_sendcmd(wm,WM_CMD_SPEAKER_ENABLE,&buf,1,NULL);

	buf = 0x01;
	wiiuse_write_data(wm,WM_REG_SPEAKER_REG3,&buf,1,NULL);

	buf = 0x08;
	wiiuse_write_data(wm,WM_REG_SPEAKER_REG1,&buf,1,NULL);

	conf[2] = 0xd0;
	conf[3] = 0x07;
	conf[4] = __wiiuse_speaker_vol;
	wiiuse_write_data(wm,WM_REG_SPEAKER_BLOCK,conf,7,NULL);

	buf = 0x01;
	wiiuse_write_data(wm,WM_REG_SPEAKER_REG2,&buf,1,NULL);

	buf = 0x00;
	wiiuse_sendcmd(wm,WM_CMD_SPEAKER_MUTE,&buf,1,NULL);

	wiiuse_status(wm,NULL);
	return;
}

void set_speakervol(struct wiimote_t *wm,ubyte vol)
{
	__wiiuse_speaker_vol = vol;
}

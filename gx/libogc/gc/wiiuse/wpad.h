/*-------------------------------------------------------------

wpad.h -- Wiimote Application Programmers Interface

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __WPAD_H__
#define __WPAD_H__

#include <gctypes.h>
#include <wiiuse/wiiuse.h>

#define WPAD_MAX_IR_DOTS						4
											
enum {
	WPAD_CHAN_ALL = -1,
	WPAD_CHAN_0,
	WPAD_CHAN_1,
	WPAD_CHAN_2,
	WPAD_CHAN_3,
	WPAD_BALANCE_BOARD,
	WPAD_MAX_WIIMOTES,
};
											
#define WPAD_BUTTON_2							0x0001
#define WPAD_BUTTON_1							0x0002
#define WPAD_BUTTON_B							0x0004
#define WPAD_BUTTON_A							0x0008
#define WPAD_BUTTON_MINUS						0x0010
#define WPAD_BUTTON_HOME						0x0080
#define WPAD_BUTTON_LEFT						0x0100
#define WPAD_BUTTON_RIGHT						0x0200
#define WPAD_BUTTON_DOWN						0x0400
#define WPAD_BUTTON_UP							0x0800
#define WPAD_BUTTON_PLUS						0x1000
											
#define WPAD_NUNCHUK_BUTTON_Z					(0x0001<<16)
#define WPAD_NUNCHUK_BUTTON_C					(0x0002<<16)
											
#define WPAD_CLASSIC_BUTTON_UP					(0x0001<<16)
#define WPAD_CLASSIC_BUTTON_LEFT				(0x0002<<16)
#define WPAD_CLASSIC_BUTTON_ZR					(0x0004<<16)
#define WPAD_CLASSIC_BUTTON_X					(0x0008<<16)
#define WPAD_CLASSIC_BUTTON_A					(0x0010<<16)
#define WPAD_CLASSIC_BUTTON_Y					(0x0020<<16)
#define WPAD_CLASSIC_BUTTON_B					(0x0040<<16)
#define WPAD_CLASSIC_BUTTON_ZL					(0x0080<<16)
#define WPAD_CLASSIC_BUTTON_FULL_R				(0x0200<<16)
#define WPAD_CLASSIC_BUTTON_PLUS				(0x0400<<16)
#define WPAD_CLASSIC_BUTTON_HOME				(0x0800<<16)
#define WPAD_CLASSIC_BUTTON_MINUS				(0x1000<<16)
#define WPAD_CLASSIC_BUTTON_FULL_L				(0x2000<<16)
#define WPAD_CLASSIC_BUTTON_DOWN				(0x4000<<16)
#define WPAD_CLASSIC_BUTTON_RIGHT				(0x8000<<16)

#define WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP		(0x0001<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_YELLOW		(0x0008<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_GREEN			(0x0010<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_BLUE			(0x0020<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_RED			(0x0040<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_ORANGE		(0x0080<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_PLUS			(0x0400<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_MINUS			(0x1000<<16)
#define WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN	(0x4000<<16)

enum {
	WPAD_EXP_NONE = 0,
	WPAD_EXP_NUNCHUK,
	WPAD_EXP_CLASSIC,
	WPAD_EXP_GUITARHERO3,
 	WPAD_EXP_WIIBOARD,
	WPAD_EXP_UNKNOWN = 255
};

enum {
	WPAD_FMT_BTNS = 0,
	WPAD_FMT_BTNS_ACC,
	WPAD_FMT_BTNS_ACC_IR
};

enum {
	WPAD_STATE_DISABLED,
	WPAD_STATE_ENABLING,
	WPAD_STATE_ENABLED
};

#define WPAD_ERR_NONE							0
#define WPAD_ERR_NO_CONTROLLER					-1
#define WPAD_ERR_NOT_READY						-2
#define WPAD_ERR_TRANSFER						-3
#define WPAD_ERR_NONEREGISTERED					-4
#define WPAD_ERR_UNKNOWN						-5
#define WPAD_ERR_BAD_CHANNEL					-6
#define WPAD_ERR_QUEUE_EMPTY					-7
#define WPAD_ERR_BADVALUE						-8
#define WPAD_ERR_BADCONF                        -9

#define WPAD_DATA_BUTTONS						0x01
#define WPAD_DATA_ACCEL							0x02
#define WPAD_DATA_EXPANSION						0x04
#define WPAD_DATA_IR							0x08

#define WPAD_ENC_FIRST							0x00
#define WPAD_ENC_CONT							0x01

#define WPAD_THRESH_IGNORE						-1
#define WPAD_THRESH_ANY							0
#define WPAD_THRESH_DEFAULT_BUTTONS				0
#define WPAD_THRESH_DEFAULT_IR					WPAD_THRESH_IGNORE
#define WPAD_THRESH_DEFAULT_ACCEL				20
#define WPAD_THRESH_DEFAULT_JOYSTICK			2
#define WPAD_THRESH_DEFAULT_BALANCEBOARD		60
#define WPAD_THRESH_DEFAULT_MOTION_PLUS			100

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _wpad_data
{
	s16 err;

	u32 data_present;
	u8 battery_level;

	u32 btns_h;
	u32 btns_l;
	u32 btns_d;
	u32 btns_u;

	struct ir_t ir;
	struct vec3w_t accel;
	struct orient_t orient;
	struct gforce_t gforce;
	struct expansion_t exp;
} WPADData;

typedef struct _wpad_encstatus
{
	u8 data[32];
}WPADEncStatus;

typedef void (*WPADDataCallback)(s32 chan, const WPADData *data);
typedef void (*WPADShutdownCallback)(s32 chan);

s32 WPAD_Init();
s32 WPAD_ControlSpeaker(s32 chan,s32 enable);
s32 WPAD_ReadEvent(s32 chan, WPADData *data);
s32 WPAD_DroppedEvents(s32 chan);
s32 WPAD_Flush(s32 chan);
s32 WPAD_ReadPending(s32 chan, WPADDataCallback datacb);
s32 WPAD_SetDataFormat(s32 chan, s32 fmt);
s32 WPAD_SetMotionPlus(s32 chan, u8 enable);
s32 WPAD_SetVRes(s32 chan,u32 xres,u32 yres);
s32 WPAD_GetStatus();
s32 WPAD_Probe(s32 chan,u32 *type);
s32 WPAD_SetEventBufs(s32 chan, WPADData *bufs, u32 cnt);
s32 WPAD_Disconnect(s32 chan);
s32 WPAD_IsSpeakerEnabled(s32 chan);
s32 WPAD_SendStreamData(s32 chan,void *buf,u32 len);
void WPAD_Shutdown();
void WPAD_SetIdleTimeout(u32 seconds);
void WPAD_SetPowerButtonCallback(WPADShutdownCallback cb);
void WPAD_SetBatteryDeadCallback(WPADShutdownCallback cb);
s32 WPAD_ScanPads();
s32 WPAD_Rumble(s32 chan, int status);
s32 WPAD_SetIdleThresholds(s32 chan, s32 btns, s32 ir, s32 accel, s32 js, s32 wb, s32 mp);
void WPAD_EncodeData(WPADEncStatus *info,u32 flag,const s16 *pcmSamples,s32 numSamples,u8 *encData);
WPADData *WPAD_Data(int chan);
u8 WPAD_BatteryLevel(int chan);
u32 WPAD_ButtonsUp(int chan);
u32 WPAD_ButtonsDown(int chan);
u32 WPAD_ButtonsHeld(int chan);
void WPAD_IR(int chan, struct ir_t *ir);
void WPAD_Orientation(int chan, struct orient_t *orient);
void WPAD_GForce(int chan, struct gforce_t *gforce);
void WPAD_Accel(int chan, struct vec3w_t *accel);
void WPAD_Expansion(int chan, struct expansion_t *exp);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif

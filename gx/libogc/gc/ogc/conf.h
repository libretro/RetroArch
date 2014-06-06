/*-------------------------------------------------------------

conf.h -- SYSCONF support

Copyright (C) 2008
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

#ifndef __CONF_H__
#define __CONF_H__

#if defined(HW_RVL)

#include <gctypes.h>
#include <gcutil.h>

#define CONF_EBADFILE	-0x6001
#define CONF_ENOENT		-0x6002
#define CONF_ETOOBIG	-0x6003
#define CONF_ENOTINIT	-0x6004
#define CONF_ENOTIMPL	-0x6005
#define CONF_EBADVALUE	-0x6006
#define CONF_ENOMEM		-0x6007
#define	CONF_ERR_OK		0

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

enum {
	CONF_BIGARRAY = 1,
	CONF_SMALLARRAY,
	CONF_BYTE,
	CONF_SHORT,
	CONF_LONG,
	CONF_BOOL = 7
};

enum {
	CONF_VIDEO_NTSC = 0,
	CONF_VIDEO_PAL,
	CONF_VIDEO_MPAL
};

enum {
	CONF_REGION_JP = 0,
	CONF_REGION_US = 1,
	CONF_REGION_EU = 2,
	CONF_REGION_KR = 4,
	CONF_REGION_CN = 5
};

enum {
	CONF_AREA_JPN = 0,
	CONF_AREA_USA,
	CONF_AREA_EUR,
	CONF_AREA_AUS,
	CONF_AREA_BRA,
	CONF_AREA_TWN,
	CONF_AREA_ROC,
	CONF_AREA_KOR,
	CONF_AREA_HKG,
	CONF_AREA_ASI,
	CONF_AREA_LTN,
	CONF_AREA_SAF,
	CONF_AREA_CHN
};

enum {
	CONF_SHUTDOWN_STANDBY = 0,
	CONF_SHUTDOWN_IDLE
};

enum {
	CONF_LED_OFF = 0,
	CONF_LED_DIM,
	CONF_LED_BRIGHT
};

enum {
	CONF_SOUND_MONO = 0,
	CONF_SOUND_STEREO,
	CONF_SOUND_SURROUND
};

enum {
	CONF_LANG_JAPANESE = 0,
	CONF_LANG_ENGLISH,
	CONF_LANG_GERMAN,
	CONF_LANG_FRENCH,
	CONF_LANG_SPANISH,
	CONF_LANG_ITALIAN,
	CONF_LANG_DUTCH,
	CONF_LANG_SIMP_CHINESE,
	CONF_LANG_TRAD_CHINESE,
	CONF_LANG_KOREAN
};

enum {
	CONF_ASPECT_4_3 = 0,
	CONF_ASPECT_16_9
};

enum {
	CONF_SENSORBAR_BOTTOM = 0,
	CONF_SENSORBAR_TOP
};

#define CONF_PAD_MAX_REGISTERED 10
#define CONF_PAD_MAX_ACTIVE 4

typedef struct _conf_pad_device conf_pad_device;

struct _conf_pad_device {
	u8 bdaddr[6];
	char name[0x40];
} ATTRIBUTE_PACKED;

typedef struct _conf_pads conf_pads;

struct _conf_pads {
	u8 num_registered;
	conf_pad_device registered[CONF_PAD_MAX_REGISTERED];
	conf_pad_device active[CONF_PAD_MAX_ACTIVE];
	conf_pad_device balance_board;
	conf_pad_device unknown;
} ATTRIBUTE_PACKED;

s32 CONF_Init(void);
s32 CONF_GetLength(const char *name);
s32 CONF_GetType(const char *name);
s32 CONF_Get(const char *name, void *buffer, u32 length);
s32 CONF_GetShutdownMode(void);
s32 CONF_GetIdleLedMode(void);
s32 CONF_GetProgressiveScan(void);
s32 CONF_GetEuRGB60(void);
s32 CONF_GetIRSensitivity(void);
s32 CONF_GetSensorBarPosition(void);
s32 CONF_GetPadSpeakerVolume(void);
s32 CONF_GetPadMotorMode(void);
s32 CONF_GetSoundMode(void);
s32 CONF_GetLanguage(void);
s32 CONF_GetCounterBias(u32 *bias);
s32 CONF_GetScreenSaverMode(void);
s32 CONF_GetDisplayOffsetH(s8 *offset);
s32 CONF_GetPadDevices(conf_pads *pads);
s32 CONF_GetNickName(u8 *nickname);
s32 CONF_GetAspectRatio(void);
s32 CONF_GetEULA(void);
s32 CONF_GetParentalPassword(s8 *password);
s32 CONF_GetParentalAnswer(s8 *answer);
s32 CONF_GetWiiConnect24(void);
s32 CONF_GetRegion(void);
s32 CONF_GetArea(void);
s32 CONF_GetVideo(void);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif

#endif

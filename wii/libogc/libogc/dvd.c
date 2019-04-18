/*-------------------------------------------------------------

dvd.h -- DVD subsystem

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

Additionally following copyrights apply for the patching system:
 * Copyright (C) 2005 The GameCube Linux Team
 * Copyright (C) 2005 Albert Herranz

Thanks alot guys for that incredible patch!!

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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "lwp.h"
#include "irq.h"
#include "ogcsys.h"
#include "system.h"
#include "dvd.h"

#define DVD_BRK							(1<<0)
#define DVD_DE_MSK						(1<<1)
#define DVD_DE_INT						(1<<2)
#define DVD_TC_MSK						(1<<3)
#define DVD_TC_INT						(1<<4)
#define DVD_BRK_MSK						(1<<5)
#define DVD_BRK_INT						(1<<6)

#define DVD_CVR_INT						(1<<2)
#define DVD_CVR_MSK						(1<<1)
#define DVD_CVR_STATE					(1<<0)

#define DVD_DI_MODE						(1<<2)
#define DVD_DI_DMA						(1<<1)
#define DVD_DI_START					(1<<0)

#define DVD_DISKIDSIZE					0x20
#define DVD_DRVINFSIZE					0x20
#define DVD_MAXCOMMANDS					0x12

#define DVD_DVDINQUIRY					0x12000000
#define DVD_FWSETOFFSET					0x32000000
#define DVD_FWENABLEEXT					0x55000000
#define DVD_READSECTOR					0xA8000000
#define DVD_READDISKID					0xA8000040
#define DVD_SEEKSECTOR					0xAB000000
#define DVD_REQUESTERROR				0xE0000000
#define DVD_AUDIOSTREAM					0xE1000000
#define DVD_AUDIOSTATUS					0xE2000000
#define DVD_STOPMOTOR					0xE3000000
#define DVD_AUDIOCONFIG					0xE4000000
#define DVD_FWSETSTATUS					0xEE000000
#define DVD_FWWRITEMEM					0xFE010100
#define DVD_FWREADMEM					0xFE010000
#define DVD_FWCTRLMOTOR					0xFE110000
#define DVD_FWFUNCCALL					0xFE120000

#define DVD_MODEL04						0x20020402
#define DVD_MODEL06						0x20010608
#define DVD_MODEL08						0x20020823
#define DVD_MODEL08Q					0x20010831

#define DVD_FWIRQVECTOR					0x00804c

#define DVD_DRIVERESET					0x00000001
#define DVD_CHIPPRESENT					0x00000002
#define DVD_INTEROPER					0x00000004

/* drive status, status */
#define DVD_STATUS(s)					((u8)((s)>>24))

#define DVD_STATUS_READY				0x00
#define DVD_STATUS_COVER_OPENED			0x01
#define DVD_STATUS_DISK_CHANGE			0x02
#define DVD_STATUS_NO_DISK				0x03
#define DVD_STATUS_MOTOR_STOP			0x04
#define DVD_STATUS_DISK_ID_NOT_READ		0x05

/* drive status, error */
#define DVD_ERROR(s)					((u32)((s)&0x00ffffff))

#define DVD_ERROR_NO_ERROR				0x000000
#define DVD_ERROR_MOTOR_STOPPED			0x020400
#define DVD_ERROR_DISK_ID_NOT_READ		0x020401
#define DVD_ERROR_MEDIUM_NOT_PRESENT	0x023a00
#define DVD_ERROR_SEEK_INCOMPLETE		0x030200
#define DVD_ERROR_UNRECOVERABLE_READ	0x031100
#define DVD_ERROR_TRANSFER_PROTOCOL		0x040800
#define DVD_ERROR_INVALID_COMMAND		0x052000
#define DVD_ERROR_AUDIOBUFFER_NOTSET	0x052001
#define DVD_ERROR_BLOCK_OUT_OF_RANGE	0x052100
#define DVD_ERROR_INVALID_FIELD			0x052400
#define DVD_ERROR_INVALID_AUDIO_CMD		0x052401
#define DVD_ERROR_INVALID_CONF_PERIOD	0x052402
#define DVD_ERROR_END_OF_USER_AREA		0x056300
#define DVD_ERROR_MEDIUM_CHANGED		0x062800
#define DVD_ERROR_MEDIUM_CHANGE_REQ		0x0B5A01

#define DVD_SPINMOTOR_MASK				0x0000ff00

#define cpu_to_le32(x)					(((x>>24)&0x000000ff) | ((x>>8)&0x0000ff00) | ((x<<8)&0x00ff0000) | ((x<<24)&0xff000000))
#define dvd_may_retry(s)				(DVD_STATUS(s) == DVD_STATUS_READY || DVD_STATUS(s) == DVD_STATUS_DISK_ID_NOT_READ)

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

typedef void (*dvdcallbacklow)(s32);
typedef void (*dvdstatecb)(dvdcmdblk *);

typedef struct _dvdcmdl {
	s32 cmd;
	void *buf;
	u32 len;
	s64 offset;
	dvdcallbacklow cb;
} dvdcmdl;

typedef struct _dvdcmds {
	void *buf;
	u32 len;
	s64 offset;
} dvdcmds;

static u32 __dvd_initflag = 0;
static u32 __dvd_stopnextint = 0;
static vu32 __dvd_resetoccured = 0;
static u32 __dvd_waitcoverclose = 0;
static u32 __dvd_breaking = 0;
static vu32 __dvd_resetrequired = 0;
static u32 __dvd_canceling = 0;
static u32 __dvd_pauseflag = 0;
static u32 __dvd_pausingflag = 0;
static u64 __dvd_lastresetend = 0;
static u32 __dvd_ready = 0;
static u32 __dvd_resumefromhere = 0;
static u32 __dvd_fatalerror = 0;
static u32 __dvd_lasterror = 0;
static u32 __dvd_internalretries = 0;
static u32 __dvd_autofinishing = 0;
static u32 __dvd_autoinvalidation = 1;
static u32 __dvd_cancellasterror = 0;
static u32 __dvd_drivechecked = 0;
static u32 __dvd_drivestate = 0;
static u32 __dvd_extensionsenabled = TRUE;
static u32 __dvd_lastlen;
static u32 __dvd_nextcmdnum;
static u32 __dvd_workaround;
static u32 __dvd_workaroundseek;
static u32 __dvd_lastcmdwasread;
static u32 __dvd_currcmd;
static u32 __dvd_motorcntrl;
static lwpq_t __dvd_wait_queue;
static syswd_t __dvd_timeoutalarm;
static dvdcmdblk __dvd_block$15;
static dvdcmdblk __dvd_dummycmdblk;
static dvddiskid __dvd_tmpid0 ATTRIBUTE_ALIGN(32);
static dvddrvinfo __dvd_driveinfo ATTRIBUTE_ALIGN(32);
static dvdcallbacklow __dvd_callback = NULL;
static dvdcallbacklow __dvd_resetcovercb = NULL;
static dvdcallbacklow __dvd_finalunlockcb = NULL;
static dvdcallbacklow __dvd_finalreadmemcb = NULL;
static dvdcallbacklow __dvd_finalsudcb = NULL;
static dvdcallbacklow __dvd_finalstatuscb = NULL;
static dvdcallbacklow __dvd_finaladdoncb = NULL;
static dvdcallbacklow __dvd_finalpatchcb = NULL;
static dvdcallbacklow __dvd_finaloffsetcb = NULL;
static dvdcbcallback __dvd_cancelcallback = NULL;
static dvdcbcallback __dvd_mountusrcb = NULL;
static dvdstatecb __dvd_laststate = NULL;
static dvdcmdblk *__dvd_executing = NULL;
static void *__dvd_usrdata = NULL;
static dvddiskid *__dvd_diskID = (dvddiskid*)0x80000000;

static lwp_queue __dvd_waitingqueue[4];
static dvdcmdl __dvd_cmdlist[4];
static dvdcmds __dvd_cmd_curr,__dvd_cmd_prev;

static u32 __dvdpatchcode_size = 0;
static const u8 *__dvdpatchcode = NULL;

static const u32 __dvd_patchcode04_size = 448;
static const u8 __dvd_patchcode04[] =
{
	0xf7,0x10,0xff,0xf7,0xf4,0x74,0x25,0xd0,0x40,0xf7,0x20,0x4c,0x80,0xf4,0x74,0xd6,
	0x9c,0x08,0xf7,0x20,0xd6,0xfc,0xf4,0x74,0x28,0xae,0x08,0xf7,0x20,0xd2,0xfc,0x80,
	0x0c,0xc4,0xda,0xfc,0xfe,0xc8,0xda,0xfc,0xf5,0x00,0x01,0xe8,0x03,0xfc,0xc1,0x00,
	0xa0,0xf4,0x74,0x09,0xec,0x40,0x10,0xc8,0xda,0xfc,0xf5,0x00,0x02,0xe8,0x03,0xfc,
	0xbc,0x00,0xf4,0x74,0xf9,0xec,0x40,0x80,0x02,0xf0,0x20,0xc8,0x84,0x80,0xc0,0x9c,
	0x81,0xdc,0xb4,0x80,0xf5,0x30,0x00,0xf4,0x44,0xa1,0xd1,0x40,0xf8,0xaa,0x00,0x10,
	0xf4,0xd0,0x9c,0xd1,0x40,0xf0,0x01,0xdc,0xb4,0x80,0xf5,0x30,0x00,0xf7,0x48,0xaa,
	0x00,0xe9,0x07,0xf4,0xc4,0xa1,0xd1,0x40,0x10,0xfe,0xd8,0x32,0xe8,0x1d,0xf7,0x48,
	0xa8,0x00,0xe8,0x28,0xf7,0x48,0xab,0x00,0xe8,0x22,0xf7,0x48,0xe1,0x00,0xe8,0x1c,
	0xf7,0x48,0xee,0x00,0xe8,0x3d,0xd8,0x55,0xe8,0x31,0xfe,0x71,0x04,0xfd,0x22,0x00,
	0xf4,0x51,0xb0,0xd1,0x40,0xa0,0x40,0x04,0x40,0x06,0xea,0x33,0xf2,0xf9,0xf4,0xd2,
	0xb0,0xd1,0x40,0x71,0x04,0xfd,0x0a,0x00,0xf2,0x49,0xfd,0x05,0x00,0x51,0x04,0xf2,
	0x36,0xfe,0xf7,0x21,0xbc,0xff,0xf7,0x31,0xbc,0xff,0xfe,0xf5,0x30,0x01,0xfd,0x7e,
	0x00,0xea,0x0c,0xf5,0x30,0x01,0xc4,0xb0,0x81,0xf5,0x30,0x02,0xc4,0x94,0x81,0xdc,
	0xb4,0x80,0xf8,0xe0,0x00,0x10,0xa0,0xf5,0x10,0x01,0xf5,0x10,0x02,0xf5,0x10,0x03,
	0xfe,0xc8,0xda,0xfc,0xf7,0x00,0xfe,0xff,0xf7,0x31,0xd2,0xfc,0xea,0x0b,0xc8,0xda,
	0xfc,0xf7,0x00,0xfd,0xff,0xf7,0x31,0xd6,0xfc,0xc4,0xda,0xfc,0xcc,0x44,0xfc,0xf7,
	0x00,0xfe,0xff,0xc4,0x44,0xfc,0xf4,0x7d,0x28,0xae,0x08,0xe9,0x07,0xf4,0x75,0x60,
	0xd1,0x40,0xea,0x0c,0xf4,0x7d,0xd6,0x9c,0x08,0xe9,0x05,0xf4,0x75,0x94,0xd1,0x40,
	0xf2,0x7c,0xd0,0x04,0xcc,0x5b,0x80,0xd8,0x01,0xe9,0x02,0x7c,0x04,0x51,0x20,0x71,
	0x34,0xf4,0x7d,0xc1,0x85,0x08,0xe8,0x05,0xfe,0x80,0x01,0xea,0x02,0x80,0x00,0xa5,
	0xd8,0x00,0xe8,0x02,0x85,0x0c,0xc5,0xda,0xfc,0xf4,0x75,0xa0,0xd1,0x40,0x14,0xfe,
	0xf7,0x10,0xff,0xf7,0xf4,0xc9,0xa0,0xd1,0x40,0xd9,0x00,0xe8,0x22,0x21,0xf7,0x49,
	0x08,0x06,0xe9,0x05,0x85,0x02,0xf5,0x10,0x01,0xf4,0x79,0x00,0xf0,0x00,0xe9,0x05,
	0x80,0x00,0xf5,0x10,0x09,0xd9,0x06,0xe9,0x06,0x61,0x06,0xd5,0x06,0x41,0x06,0xf4,
	0xe0,0x9f,0xdc,0xc7,0xf4,0xe0,0xb5,0xcb,0xc7,0x00,0x00,0x00,0x74,0x0a,0x08,0x00,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const u32 __dvd_patchcode06_size = 448;
static const u8 __dvd_patchcode06[] =
{
	0xf7,0x10,0xff,0xf7,0xf4,0x74,0x25,0xd0,0x40,0xf7,0x20,0x4c,0x80,0xf4,0x74,0x42,
	0x9d,0x08,0xf7,0x20,0xd6,0xfc,0xf4,0x74,0x45,0xb1,0x08,0xf7,0x20,0xd2,0xfc,0x80,
	0x0c,0xc4,0xda,0xfc,0xfe,0xc8,0xda,0xfc,0xf5,0x00,0x01,0xe8,0x03,0xfc,0xc1,0x00,
	0xa0,0xf4,0x74,0x09,0xec,0x40,0x10,0xc8,0xda,0xfc,0xf5,0x00,0x02,0xe8,0x03,0xfc,
	0xbc,0x00,0xf4,0x74,0x02,0xed,0x40,0x80,0x02,0xf0,0x20,0xc8,0x78,0x80,0xc0,0x90,
	0x81,0xdc,0xa8,0x80,0xf5,0x30,0x00,0xf4,0x44,0xa1,0xd1,0x40,0xf8,0xaa,0x00,0x10,
	0xf4,0xd0,0x9c,0xd1,0x40,0xf0,0x01,0xdc,0xa8,0x80,0xf5,0x30,0x00,0xf7,0x48,0xaa,
	0x00,0xe9,0x07,0xf4,0xc4,0xa1,0xd1,0x40,0x10,0xfe,0xd8,0x32,0xe8,0x1d,0xf7,0x48,
	0xa8,0x00,0xe8,0x28,0xf7,0x48,0xab,0x00,0xe8,0x22,0xf7,0x48,0xe1,0x00,0xe8,0x1c,
	0xf7,0x48,0xee,0x00,0xe8,0x3d,0xd8,0x55,0xe8,0x31,0xfe,0x71,0x04,0xfd,0x22,0x00,
	0xf4,0x51,0xb0,0xd1,0x40,0xa0,0x40,0x04,0x40,0x06,0xea,0x33,0xf2,0xf9,0xf4,0xd2,
	0xb0,0xd1,0x40,0x71,0x04,0xfd,0x0a,0x00,0xf2,0x49,0xfd,0x05,0x00,0x51,0x04,0xf2,
	0x36,0xfe,0xf7,0x21,0xbc,0xff,0xf7,0x31,0xbc,0xff,0xfe,0xf5,0x30,0x01,0xfd,0x7e,
	0x00,0xea,0x0c,0xf5,0x30,0x01,0xc4,0xa4,0x81,0xf5,0x30,0x02,0xc4,0x88,0x81,0xdc,
	0xa8,0x80,0xf8,0xe0,0x00,0x10,0xa0,0xf5,0x10,0x01,0xf5,0x10,0x02,0xf5,0x10,0x03,
	0xfe,0xc8,0xda,0xfc,0xf7,0x00,0xfe,0xff,0xf7,0x31,0xd2,0xfc,0xea,0x0b,0xc8,0xda,
	0xfc,0xf7,0x00,0xfd,0xff,0xf7,0x31,0xd6,0xfc,0xc4,0xda,0xfc,0xcc,0x44,0xfc,0xf7,
	0x00,0xfe,0xff,0xc4,0x44,0xfc,0xf4,0x7d,0x45,0xb1,0x08,0xe9,0x07,0xf4,0x75,0x60,
	0xd1,0x40,0xea,0x0c,0xf4,0x7d,0x42,0x9d,0x08,0xe9,0x05,0xf4,0x75,0x94,0xd1,0x40,
	0xf2,0x7c,0xd0,0x04,0xcc,0x5b,0x80,0xd8,0x01,0xe9,0x02,0x7c,0x04,0x51,0x20,0x71,
	0x34,0xf4,0x7d,0xb9,0x85,0x08,0xe8,0x05,0xfe,0x80,0x01,0xea,0x02,0x80,0x00,0xa5,
	0xd8,0x00,0xe8,0x02,0x85,0x0c,0xc5,0xda,0xfc,0xf4,0x75,0xa0,0xd1,0x40,0x14,0xfe,
	0xf7,0x10,0xff,0xf7,0xf4,0xc9,0xa0,0xd1,0x40,0xd9,0x00,0xe8,0x22,0x21,0xf7,0x49,
	0x08,0x06,0xe9,0x05,0x85,0x02,0xf5,0x10,0x01,0xf4,0x79,0x00,0xf0,0x00,0xe9,0x05,
	0x80,0x00,0xf5,0x10,0x09,0xd9,0x06,0xe9,0x06,0x61,0x06,0xd5,0x06,0x41,0x06,0xf4,
	0xe0,0xbc,0xdf,0xc7,0xf4,0xe0,0x37,0xcc,0xc7,0x00,0x00,0x00,0x74,0x0a,0x08,0x00,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const u32 __dvd_patchcode08_size = 448;
static const u8 __dvd_patchcode08[] =
{
	0xf7,0x10,0xff,0xf7,0xf4,0x74,0x25,0xd0,0x40,0xf7,0x20,0x4c,0x80,0xf4,0x74,0x32,
	0x9d,0x08,0xf7,0x20,0xd6,0xfc,0xf4,0x74,0x75,0xae,0x08,0xf7,0x20,0xd2,0xfc,0x80,
	0x0c,0xc4,0xda,0xfc,0xfe,0xc8,0xda,0xfc,0xf5,0x00,0x01,0xe8,0x03,0xfc,0xc1,0x00,
	0xa0,0xf4,0x74,0x09,0xec,0x40,0x10,0xc8,0xda,0xfc,0xf5,0x00,0x02,0xe8,0x03,0xfc,
	0xbc,0x00,0xf4,0x74,0xf5,0xec,0x40,0x80,0x02,0xf0,0x20,0xc8,0x80,0x80,0xc0,0x98,
	0x81,0xdc,0xb0,0x80,0xf5,0x30,0x00,0xf4,0x44,0xa1,0xd1,0x40,0xf8,0xaa,0x00,0x10,
	0xf4,0xd0,0x9c,0xd1,0x40,0xf0,0x01,0xdc,0xb0,0x80,0xf5,0x30,0x00,0xf7,0x48,0xaa,
	0x00,0xe9,0x07,0xf4,0xc4,0xa1,0xd1,0x40,0x10,0xfe,0xd8,0x32,0xe8,0x1d,0xf7,0x48,
	0xa8,0x00,0xe8,0x28,0xf7,0x48,0xab,0x00,0xe8,0x22,0xf7,0x48,0xe1,0x00,0xe8,0x1c,
	0xf7,0x48,0xee,0x00,0xe8,0x3d,0xd8,0x55,0xe8,0x31,0xfe,0x71,0x04,0xfd,0x22,0x00,
	0xf4,0x51,0xb0,0xd1,0x40,0xa0,0x40,0x04,0x40,0x06,0xea,0x33,0xf2,0xf9,0xf4,0xd2,
	0xb0,0xd1,0x40,0x71,0x04,0xfd,0x0a,0x00,0xf2,0x49,0xfd,0x05,0x00,0x51,0x04,0xf2,
	0x36,0xfe,0xf7,0x21,0xbc,0xff,0xf7,0x31,0xbc,0xff,0xfe,0xf5,0x30,0x01,0xfd,0x7e,
	0x00,0xea,0x0c,0xf5,0x30,0x01,0xc4,0xac,0x81,0xf5,0x30,0x02,0xc4,0x90,0x81,0xdc,
	0xb0,0x80,0xf8,0xe0,0x00,0x10,0xa0,0xf5,0x10,0x01,0xf5,0x10,0x02,0xf5,0x10,0x03,
	0xfe,0xc8,0xda,0xfc,0xf7,0x00,0xfe,0xff,0xf7,0x31,0xd2,0xfc,0xea,0x0b,0xc8,0xda,
	0xfc,0xf7,0x00,0xfd,0xff,0xf7,0x31,0xd6,0xfc,0xc4,0xda,0xfc,0xcc,0x44,0xfc,0xf7,
	0x00,0xfe,0xff,0xc4,0x44,0xfc,0xf4,0x7d,0x75,0xae,0x08,0xe9,0x07,0xf4,0x75,0x60,
	0xd1,0x40,0xea,0x0c,0xf4,0x7d,0x32,0x9d,0x08,0xe9,0x05,0xf4,0x75,0x94,0xd1,0x40,
	0xf2,0x7c,0xd0,0x04,0xcc,0x5b,0x80,0xd8,0x01,0xe9,0x02,0x7c,0x04,0x51,0x20,0x71,
	0x34,0xf4,0x7d,0xc1,0x85,0x08,0xe8,0x05,0xfe,0x80,0x01,0xea,0x02,0x80,0x00,0xa5,
	0xd8,0x00,0xe8,0x02,0x85,0x0c,0xc5,0xda,0xfc,0xf4,0x75,0xa0,0xd1,0x40,0x14,0xfe,
	0xf7,0x10,0xff,0xf7,0xf4,0xc9,0xa0,0xd1,0x40,0xd9,0x00,0xe8,0x22,0x21,0xf7,0x49,
	0x08,0x06,0xe9,0x05,0x85,0x02,0xf5,0x10,0x01,0xf4,0x79,0x00,0xf0,0x00,0xe9,0x05,
	0x80,0x00,0xf5,0x10,0x09,0xd9,0x06,0xe9,0x06,0x61,0x06,0xd5,0x06,0x41,0x06,0xf4,
	0xe0,0xec,0xdc,0xc7,0xf4,0xe0,0x0e,0xcc,0xc7,0x00,0x00,0x00,0x74,0x0a,0x08,0x00,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const u32 __dvd_patchcodeQ08_size = 448;
static const u8 __dvd_patchcodeQ08[] =
{
	0xf7,0x10,0xff,0xf7,0xf4,0x74,0x25,0xd0,0x40,0xf7,0x20,0x4c,0x80,0xf4,0x74,0x39,
	0x9e,0x08,0xf7,0x20,0xd6,0xfc,0xf4,0x74,0x02,0xb3,0x08,0xf7,0x20,0xd2,0xfc,0x80,
	0x0c,0xc4,0xda,0xfc,0xfe,0xc8,0xda,0xfc,0xf5,0x00,0x01,0xe8,0x03,0xfc,0xc1,0x00,
	0xa0,0xf4,0x74,0x09,0xec,0x40,0x10,0xc8,0xda,0xfc,0xf5,0x00,0x02,0xe8,0x03,0xfc,
	0xbc,0x00,0xf4,0x74,0x02,0xed,0x40,0x80,0x02,0xf0,0x20,0xc8,0x78,0x80,0xc0,0x92,
	0x81,0xdc,0xaa,0x80,0xf5,0x30,0x00,0xf4,0x44,0xa1,0xd1,0x40,0xf8,0xaa,0x00,0x10,
	0xf4,0xd0,0x9c,0xd1,0x40,0xf0,0x01,0xdc,0xaa,0x80,0xf5,0x30,0x00,0xf7,0x48,0xaa,
	0x00,0xe9,0x07,0xf4,0xc4,0xa1,0xd1,0x40,0x10,0xfe,0xd8,0x32,0xe8,0x1d,0xf7,0x48,
	0xa8,0x00,0xe8,0x28,0xf7,0x48,0xab,0x00,0xe8,0x22,0xf7,0x48,0xe1,0x00,0xe8,0x1c,
	0xf7,0x48,0xee,0x00,0xe8,0x3d,0xd8,0x55,0xe8,0x31,0xfe,0x71,0x04,0xfd,0x22,0x00,
	0xf4,0x51,0xb0,0xd1,0x40,0xa0,0x40,0x04,0x40,0x06,0xea,0x33,0xf2,0xf9,0xf4,0xd2,
	0xb0,0xd1,0x40,0x71,0x04,0xfd,0x0a,0x00,0xf2,0x49,0xfd,0x05,0x00,0x51,0x04,0xf2,
	0x36,0xfe,0xf7,0x21,0xbc,0xff,0xf7,0x31,0xbc,0xff,0xfe,0xf5,0x30,0x01,0xfd,0x7e,
	0x00,0xea,0x0c,0xf5,0x30,0x01,0xc4,0xa6,0x81,0xf5,0x30,0x02,0xc4,0x8a,0x81,0xdc,
	0xaa,0x80,0xf8,0xe0,0x00,0x10,0xa0,0xf5,0x10,0x01,0xf5,0x10,0x02,0xf5,0x10,0x03,
	0xfe,0xc8,0xda,0xfc,0xf7,0x00,0xfe,0xff,0xf7,0x31,0xd2,0xfc,0xea,0x0b,0xc8,0xda,
	0xfc,0xf7,0x00,0xfd,0xff,0xf7,0x31,0xd6,0xfc,0xc4,0xda,0xfc,0xcc,0x44,0xfc,0xf7,
	0x00,0xfe,0xff,0xc4,0x44,0xfc,0xf4,0x7d,0x02,0xb3,0x08,0xe9,0x07,0xf4,0x75,0x60,
	0xd1,0x40,0xea,0x0c,0xf4,0x7d,0x39,0x9e,0x08,0xe9,0x05,0xf4,0x75,0x94,0xd1,0x40,
	0xf2,0x7c,0xd0,0x04,0xcc,0x5b,0x80,0xd8,0x01,0xe9,0x02,0x7c,0x04,0x51,0x20,0x71,
	0x34,0xf4,0x7d,0x7f,0x86,0x08,0xe8,0x05,0xfe,0x80,0x01,0xea,0x02,0x80,0x00,0xa5,
	0xd8,0x00,0xe8,0x02,0x85,0x0c,0xc5,0xda,0xfc,0xf4,0x75,0xa0,0xd1,0x40,0x14,0xfe,
	0xf7,0x10,0xff,0xf7,0xf4,0xc9,0xa0,0xd1,0x40,0xd9,0x00,0xe8,0x22,0x21,0xf7,0x49,
	0x08,0x06,0xe9,0x05,0x85,0x02,0xf5,0x10,0x01,0xf4,0x79,0x00,0xf0,0x00,0xe9,0x05,
	0x80,0x00,0xf5,0x10,0x09,0xd9,0x06,0xe9,0x06,0x61,0x06,0xd5,0x06,0x41,0x06,0xf4,
	0xe0,0x79,0xe1,0xc7,0xf4,0xe0,0xeb,0xcd,0xc7,0x00,0x00,0x00,0xa4,0x0a,0x08,0x00,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static vu32* const _piReg = (u32*)0xCC003000;

#if defined(HW_RVL)
	static vu32* const _diReg = (u32*)0xCD006000;
#elif defined(HW_DOL)
	static vu32* const _diReg = (u32*)0xCC006000;
#endif

static u8 __dvd_unlockcmd$221[12] = {0xff,0x01,'M','A','T','S','H','I','T','A',0x02,0x00};
static u8 __dvd_unlockcmd$222[12] = {0xff,0x00,'D','V','D','-','G','A','M','E',0x03,0x00};
static u32 __dvd_errortable[] = {
	0x00000000, 0x00023a00, 0x00062800, 0x00030200,
	0x00031100, 0x00052000, 0x00052001, 0x00052100,
	0x00052400, 0x00052401, 0x00052402, 0x000B5A01,
	0x00056300, 0x00020401, 0x00020400, 0x00040800,
	0x00100007, 0x00000000
};

void __dvd_statecheckid(void);
void __dvd_stategettingerror(void);
void __dvd_statecoverclosed(void);
void __dvd_stateready(void);
void __dvd_statemotorstopped(void);
void __dvd_statetimeout(void);
void __dvd_stategotoretry(void);
void __dvd_stateerror(s32 result);
void __dvd_statecoverclosed_cmd(dvdcmdblk *block);
void __dvd_statebusy(dvdcmdblk *block);
void DVD_LowReset(u32 reset_mode);
s32 DVD_LowSeek(s64 offset,dvdcallbacklow cb);
s32 DVD_LowRead(void *buf,u32 len,s64 offset,dvdcallbacklow cb);
s32 DVD_LowReadId(dvddiskid *diskID,dvdcallbacklow cb);
s32 DVD_LowRequestError(dvdcallbacklow cb);
s32 DVD_LowStopMotor(dvdcallbacklow cb);
s32 DVD_LowInquiry(dvddrvinfo *info,dvdcallbacklow cb);
s32 DVD_LowWaitCoverClose(dvdcallbacklow cb);
s32 DVD_LowAudioStream(u32 subcmd,u32 len,s64 offset,dvdcallbacklow cb);
s32 DVD_LowAudioBufferConfig(s32 enable,u32 size,dvdcallbacklow cb);
s32 DVD_LowRequestAudioStatus(u32 subcmd,dvdcallbacklow cb);
s32 DVD_LowEnableExtensions(u8 enable,dvdcallbacklow cb);
s32 DVD_LowSpinMotor(u32 mode,dvdcallbacklow cb);
s32 DVD_LowSetStatus(u32 status,dvdcallbacklow cb);
s32 DVD_LowUnlockDrive(dvdcallbacklow cb);
s32 DVD_LowPatchDriveCode(dvdcallbacklow cb);
s32 DVD_LowSpinUpDrive(dvdcallbacklow cb);
s32 DVD_LowControlMotor(u32 mode,dvdcallbacklow cb);
s32 DVD_LowFuncCall(u32 address,dvdcallbacklow cb);
s32 DVD_LowReadmem(u32 address,void *buffer,dvdcallbacklow cb);
s32 DVD_LowSetGCMOffset(s64 offset,dvdcallbacklow cb);
s32 DVD_LowSetOffset(s64 offset,dvdcallbacklow cb);
s32 DVD_ReadAbsAsyncPrio(dvdcmdblk *block,void *buf,u32 len,s64 offset,dvdcbcallback cb,s32 prio);
s32 DVD_ReadDiskID(dvdcmdblk *block,dvddiskid *id,dvdcbcallback cb);
s32 __issuecommand(s32 prio,dvdcmdblk *block);

extern void udelay(int us);
extern u32 diff_msec(unsigned long long start,unsigned long long end);
extern long long gettime(void);
extern void __MaskIrq(u32);
extern void __UnmaskIrq(u32);
extern syssramex* __SYS_LockSramEx(void);
extern u32 __SYS_UnlockSramEx(u32 write);

static u8 err2num(u32 errorcode)
{
	u32 i;

	i=0;
	while(i<18) {
		if(errorcode==__dvd_errortable[i]) return i;
		i++;
	}
	if(errorcode<0x00100000 || errorcode>0x00100008) return 29;

	return 17;
}

static u8 convert(u32 errorcode)
{
	u8 err,err_num;

	if((errorcode-0x01230000)==0x4567) return 255;
	else if((errorcode-0x01230000)==0x4568) return 254;

	err = _SHIFTR(errorcode,24,8);
	err_num = err2num((errorcode&0x00ffffff));
	if(err>0x06) err = 0x06;

	return err_num+(err*30);
}

static void __dvd_clearwaitingqueue()
{
	u32 i;

	for(i=0;i<4;i++)
		__lwp_queue_init_empty(&__dvd_waitingqueue[i]);
}

static s32 __dvd_checkwaitingqueue()
{
	u32 i;
	u32 level;

	_CPU_ISR_Disable(level);
	for(i=0;i<4;i++) {
		if(!__lwp_queue_isempty(&__dvd_waitingqueue[i])) break;
	}
	_CPU_ISR_Restore(level);
	return (i<4);
}

static s32 __dvd_pushwaitingqueue(s32 prio,dvdcmdblk *block)
{
	u32 level;
	_CPU_ISR_Disable(level);
	__lwp_queue_appendI(&__dvd_waitingqueue[prio],&block->node);
	_CPU_ISR_Restore(level);
	return 1;
}

static dvdcmdblk* __dvd_popwaitingqueueprio(s32 prio)
{
	u32 level;
	dvdcmdblk *ret = NULL;
	_CPU_ISR_Disable(level);
	ret = (dvdcmdblk*)__lwp_queue_firstnodeI(&__dvd_waitingqueue[prio]);
	_CPU_ISR_Restore(level);
	return ret;
}

static dvdcmdblk* __dvd_popwaitingqueue()
{
	u32 i,level;
	dvdcmdblk *ret = NULL;
	_CPU_ISR_Disable(level);
	for(i=0;i<4;i++) {
		if(!__lwp_queue_isempty(&__dvd_waitingqueue[i])) {
			_CPU_ISR_Restore(level);
			ret = __dvd_popwaitingqueueprio(i);
			return ret;
		}
	}
	_CPU_ISR_Restore(level);
	return NULL;
}

static void __dvd_timeouthandler(syswd_t alarm,void *cbarg)
{
	dvdcallbacklow cb;

	__MaskIrq(IRQMASK(IRQ_PI_DI));
	cb = __dvd_callback;
	if(cb) cb(0x10);
}

static void __dvd_storeerror(u32 errorcode)
{
	u8 err;
	syssramex *ptr;

	err = convert(errorcode);
	ptr = __SYS_LockSramEx();
	ptr->dvderr_code = err;
	__SYS_UnlockSramEx(1);
}

static u32 __dvd_categorizeerror(u32 errorcode)
{
	if((errorcode-0x20000)==0x0400) {
		__dvd_lasterror = errorcode;
		return 1;
	}

	if(DVD_ERROR(errorcode)==DVD_ERROR_MEDIUM_CHANGED
		|| DVD_ERROR(errorcode)==DVD_ERROR_MEDIUM_NOT_PRESENT
		|| DVD_ERROR(errorcode)==DVD_ERROR_MEDIUM_CHANGE_REQ
		|| (DVD_ERROR(errorcode)-0x40000)==0x3100) return 0;

	__dvd_internalretries++;
	if(__dvd_internalretries==2) {
		if(__dvd_lasterror==DVD_ERROR(errorcode)) {
			__dvd_lasterror = DVD_ERROR(errorcode);
			return 1;
		}
		__dvd_lasterror = DVD_ERROR(errorcode);
		return 2;
	}

	__dvd_lasterror = DVD_ERROR(errorcode);
	if(DVD_ERROR(errorcode)!=DVD_ERROR_UNRECOVERABLE_READ) {
		if(__dvd_executing->cmd!=0x0005) return 3;
	}
	return 2;
}

static void __SetupTimeoutAlarm(const struct timespec *tp)
{
	SYS_SetAlarm(__dvd_timeoutalarm,tp,__dvd_timeouthandler,NULL);
}

static void __Read(void *buffer,u32 len,s64 offset,dvdcallbacklow cb)
{
	u32 val;
	struct timespec tb;
	__dvd_callback = cb;
	__dvd_stopnextint = 0;
	__dvd_lastcmdwasread = 1;

	_diReg[2] = DVD_READSECTOR;
	_diReg[3] = (u32)(offset>>2);
	_diReg[4] = len;
	_diReg[5] = (u32)buffer;
	_diReg[6] = len;

	__dvd_lastlen = len;

	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);
	val = _diReg[7];
	if(val>0x00a00000) {
		tb.tv_sec = 20;
		tb.tv_nsec = 0;
		__SetupTimeoutAlarm(&tb);
	} else {
		tb.tv_sec = 10;
		tb.tv_nsec = 0;
		__SetupTimeoutAlarm(&tb);
	}
}

static void __DoRead(void *buffer,u32 len,s64 offset,dvdcallbacklow cb)
{
	__dvd_nextcmdnum = 0;
	__dvd_cmdlist[0].cmd = -1;
	__Read(buffer,len,offset,cb);
}

static u32 __ProcessNextCmd()
{
	u32 cmd_num = __dvd_nextcmdnum;
	if(__dvd_cmdlist[cmd_num].cmd==0x0001) {
		__dvd_nextcmdnum++;
		__Read(__dvd_cmdlist[cmd_num].buf,__dvd_cmdlist[cmd_num].len,__dvd_cmdlist[cmd_num].offset,__dvd_cmdlist[cmd_num].cb);
		return 1;
	}

	if(__dvd_cmdlist[cmd_num].cmd==0x0002) {
		__dvd_nextcmdnum++;
		DVD_LowSeek(__dvd_cmdlist[cmd_num].offset,__dvd_cmdlist[cmd_num].cb);
		return 1;
	}
	return 0;
}

static void __DVDLowWATypeSet(u32 workaround,u32 workaroundseek)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__dvd_workaround = workaround;
	__dvd_workaroundseek = workaroundseek;
	_CPU_ISR_Restore(level);
}

static void __DVDInitWA()
{
	__dvd_nextcmdnum = 0;
	__DVDLowWATypeSet(0,0);
}

static s32 __dvd_checkcancel(u32 cancelpt)
{
	dvdcmdblk *block;

	if(__dvd_canceling) {
		__dvd_resumefromhere = cancelpt;
		__dvd_canceling = 0;

		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;

		block->state = 10;
		if(block->cb) block->cb(-3,block);
		if(__dvd_cancelcallback) __dvd_cancelcallback(0,block);

		__dvd_stateready();
		return 1;
	}
	return 0;
}

static void __dvd_stateretrycb(s32 result)
{
	if(result==0x0010) {
		__dvd_executing->state = -1;
		__dvd_statetimeout();
		return;
	}

	if(result&0x0002) __dvd_stateerror(0x01234567);
	if(result==0x0001) {
		__dvd_statebusy(__dvd_executing);
		return;
	}
}

static void __dvd_unrecoverederrorretrycb(s32 result)
{
	u32 val;

	if(result==0x0010) {
		__dvd_executing->state = -1;
		__dvd_statetimeout();
		return;
	}

	__dvd_executing->state = -1;
	if(result&0x0002) __dvd_stateerror(0x01234567);
	else {
		val = _diReg[8];
		__dvd_stateerror(val);
	}
}

static void __dvd_unrecoverederrorcb(s32 result)
{
	if(result==0x0010) {
		__dvd_executing->state = -1;
		__dvd_statetimeout();
		return;
	}
	if(result&0x0001) {
		__dvd_stategotoretry();
		return;
	}
	if(!(result==0x0002)) DVD_LowRequestError(__dvd_unrecoverederrorretrycb);
}

static void __dvd_stateerrorcb(s32 result)
{
	dvdcmdblk *block;
	if(result==0x0010) {
		__dvd_executing->state = -1;
		__dvd_statetimeout();
		return;
	}

	__dvd_fatalerror = 1;
	block = __dvd_executing;
	__dvd_executing = &__dvd_dummycmdblk;
	if(block->cb) block->cb(-1,block);
	if(__dvd_canceling) {
		__dvd_canceling = 0;
		if(__dvd_cancelcallback) __dvd_cancelcallback(0,block);
	}
	__dvd_stateready();
}

static void __dvd_stategettingerrorcb(s32 result)
{
	s32 ret;
	u32 val,cnclpt;
	if(result==0x0010) {
		__dvd_executing->state = -1;
		__dvd_statetimeout();
		return;
	}
	if(result&0x0002) {
		__dvd_executing->state = -1;
		__dvd_stateerror(0x01234567);
		return;
	}
	if(result==0x0001) {
		val = _diReg[8];
		ret = __dvd_categorizeerror(val);
		if(ret==1) {
			__dvd_executing->state = -1;
			__dvd_stateerror(val);
			return;
		} else if(ret==2 || ret==3) cnclpt = 0;
		else {
			if(DVD_STATUS(val)==DVD_STATUS_COVER_OPENED) cnclpt = 4;
			else if(DVD_STATUS(val)==DVD_STATUS_DISK_CHANGE) cnclpt = 6;
			else if(DVD_STATUS(val)==DVD_STATUS_NO_DISK) cnclpt = 3;
			else cnclpt = 5;
		}
		if(__dvd_checkcancel(cnclpt)) return;

		if(ret==2) {
			if(dvd_may_retry(val)) {
				// disable the extensions iff they're enabled and we were trying to read the disc ID
				if(__dvd_executing->cmd==0x05) {
					__dvd_lasterror = 0;
					__dvd_extensionsenabled = FALSE;
					DVD_LowSpinUpDrive(__dvd_stateretrycb);
					return;
				}
				__dvd_statebusy(__dvd_executing);
			} else {
				__dvd_storeerror(val);
				__dvd_stategotoretry();
			}
			return;
		} else if(ret==3) {
			if(DVD_ERROR(val)==DVD_ERROR_UNRECOVERABLE_READ) {
				DVD_LowSeek(__dvd_executing->offset,__dvd_unrecoverederrorcb);
				return;
			} else {
				__dvd_laststate(__dvd_executing);
				return;
			}
		} else if(DVD_STATUS(val)==DVD_STATUS_COVER_OPENED) {
			__dvd_executing->state = 5;
			__dvd_statemotorstopped();
			return;
		} else if(DVD_STATUS(val)==DVD_STATUS_DISK_CHANGE) {
			__dvd_executing->state = 3;
			__dvd_statecoverclosed();
			return;
		} else if(DVD_STATUS(val)==DVD_STATUS_NO_DISK) {
			__dvd_executing->state = 4;
			__dvd_statemotorstopped();
			return;
		}
		__dvd_executing->state = -1;
		__dvd_stateerror(0x01234567);
		return;
	}
}

static void __dvd_statebusycb(s32 result)
{
	u32 val;
	dvdcmdblk *block;
	if(result==0x0010) {
		__dvd_executing->state = -1;
		__dvd_statetimeout();
		return;
	}
	if(__dvd_currcmd==0x0003 || __dvd_currcmd==0x000f) {
		if(result&0x0002) {
			__dvd_executing->state = -1;
			__dvd_stateerror(0x01234567);
			return;
		}
		if(result==0x0001) {
			__dvd_internalretries = 0;
			if(__dvd_currcmd==0x000f) __dvd_resetrequired = 1;
			if(__dvd_checkcancel(7)) return;

			__dvd_executing->state = 7;
			__dvd_statemotorstopped();
			return;
		}
	}
	if(result&0x0004) {

	}
	if(__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004
		|| __dvd_currcmd==0x0005 || __dvd_currcmd==0x000e) {
		__dvd_executing->txdsize += (__dvd_executing->currtxsize-_diReg[6]);
	}
	if(result&0x0008) {
		__dvd_canceling = 0;
		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		__dvd_executing->state = 10;
		if(block->cb) block->cb(-3,block);
		if(__dvd_cancelcallback) __dvd_cancelcallback(0,block);
		__dvd_stateready();
		return;
	}
	if(result&0x0001) {
		__dvd_internalretries = 0;
		if(__dvd_checkcancel(0)) return;

		if(__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004
			|| __dvd_currcmd==0x0005 || __dvd_currcmd==0x000e) {
			if(__dvd_executing->txdsize!=__dvd_executing->len) {
				__dvd_statebusy(__dvd_executing);
				return;
			}
			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = 0;
			if(block->cb) block->cb(block->txdsize,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0009 || __dvd_currcmd==0x000a
			|| __dvd_currcmd==0x000b || __dvd_currcmd==0x000c) {

			val = _diReg[8];
			if(__dvd_currcmd==0x000a || __dvd_currcmd==0x000b) val <<= 2;

			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = 0;
			if(block->cb) block->cb(val,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0006) {
			if(!__dvd_executing->currtxsize) {
				if(_diReg[8]&0x0001) {
					block = __dvd_executing;
					__dvd_executing = &__dvd_dummycmdblk;
					block->state = 9;
					if(block->cb) block->cb(-2,block);
					__dvd_stateready();
					return;
				}
				__dvd_autofinishing = 0;
				__dvd_executing->currtxsize = 1;
				DVD_LowAudioStream(0,__dvd_executing->len,__dvd_executing->offset,__dvd_statebusycb);
				return;
			}

			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = 0;
			if(block->cb) block->cb(0,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0010) {
			if(__dvd_drivestate&DVD_CHIPPRESENT) {
				block = __dvd_executing;
				__dvd_executing = &__dvd_dummycmdblk;
				block->state = 0;
				if(block->cb) block->cb(DVD_DISKIDSIZE,block);
				__dvd_stateready();
				return;
			}
		}

		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		block->state = 0;
		if(block->cb) block->cb(0,block);
		__dvd_stateready();
		return;
	}
	if(result==0x0002) {
		if(__dvd_currcmd==0x000e) {
			__dvd_executing->state = -1;
			__dvd_stateerror(0x01234567);
			return;
		}
		if((__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004
			|| __dvd_currcmd==0x0005 || __dvd_currcmd==0x000e)
			&& __dvd_executing->txdsize==__dvd_executing->len) {
				if(__dvd_checkcancel(0)) return;

				block = __dvd_executing;
				__dvd_executing = &__dvd_dummycmdblk;
				block->state = 0;
				if(block->cb) block->cb(block->txdsize,block);
				__dvd_stateready();
				return;
		}
	}
	__dvd_stategettingerror();
}

static void __dvd_mountsynccb(s32 result,dvdcmdblk *block)
{
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_inquirysynccb(s32 result,dvdcmdblk *block)
{
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_readsynccb(s32 result,dvdcmdblk *block)
{
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_streamatendsynccb(s32 result,dvdcmdblk *block)
{
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_seeksynccb(s32 result,dvdcmdblk *block)
{
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_spinupdrivesynccb(s32 result,dvdcmdblk *block)
{
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_motorcntrlsynccb(s32 result,dvdcmdblk *block)
{
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_setgcmsynccb(s32 result,dvdcmdblk *block)
{
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_statemotorstoppedcb(s32 result)
{
	_diReg[1] = 0;
	__dvd_executing->state = 3;
	__dvd_statecoverclosed();
}

static void __dvd_statecoverclosedcb(s32 result)
{
	if(result==0x0010) {
		__dvd_executing->state = -1;
		__dvd_statetimeout();
		return;
	}

	if(!(result&0x0006)) {
		__dvd_internalretries = 0;
		__dvd_statecheckid();
		return;
	}

	if(result==0x0002) __dvd_stategettingerror();
}

static void __dvd_statecheckid1cb(s32 result)
{
	__dvd_ready = 1;
	if(memcmp(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE)) memcpy(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE);
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_stategotoretrycb(s32 result)
{
	if(result==0x0010) {
		__dvd_executing->state = -1;
		__dvd_statetimeout();
		return;
	}
	if(result&0x0002) {
		__dvd_executing->state = -1;
		__dvd_stateerror(0x01234567);
		return;
	}
	if(result==0x0001) {
		__dvd_internalretries = 0;
		if(__dvd_currcmd==0x0004 || __dvd_currcmd==0x0005
			|| __dvd_currcmd==0x000d || __dvd_currcmd==0x000f) {
			__dvd_resetrequired = 1;
			if(__dvd_checkcancel(2)) return;

			__dvd_executing->state = 11;
			__dvd_statemotorstopped();
		}
	}
}

static void __dvd_getstatuscb(s32 result)
{
	u32 val,*pn_data;
	dvdcallbacklow cb;
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		val = _diReg[8];

		pn_data = __dvd_usrdata;
		__dvd_usrdata = NULL;
		if(pn_data) pn_data[0] = val;

		cb = __dvd_finalstatuscb;
		__dvd_finalstatuscb = NULL;
		if(cb) cb(result);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_readmemcb(s32 result)
{
	u32 val,*pn_data;
	dvdcallbacklow cb;
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		val = _diReg[8];

		pn_data = __dvd_usrdata;
		__dvd_usrdata = NULL;
		if(pn_data) pn_data[0] = val;

		cb = __dvd_finalreadmemcb;
		__dvd_finalreadmemcb = NULL;
		if(cb) cb(result);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_cntrldrivecb(s32 result)
{
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		DVD_LowSpinMotor(__dvd_motorcntrl,__dvd_finalsudcb);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_setgcmoffsetcb(s32 result)
{
	s64 *pn_data,offset;
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		pn_data = (s64*)__dvd_usrdata;
		__dvd_usrdata = NULL;

		offset = 0;
		if(pn_data) offset = *pn_data;
		DVD_LowSetOffset(offset,__dvd_finaloffsetcb);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_handlespinupcb(s32 result)
{
	static u32 step = 0;
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		if(step==0x0000) {
			step++;
			_diReg[1] = _diReg[1];
			DVD_LowEnableExtensions(__dvd_extensionsenabled,__dvd_handlespinupcb);
			return;
		}
		if(step==0x0001) {
			step++;
			_diReg[1] = _diReg[1];
			DVD_LowSpinMotor((DVD_SPINMOTOR_ACCEPT|DVD_SPINMOTOR_UP),__dvd_handlespinupcb);
			return;
		}
		if(step==0x0002) {
			step = 0;
			if(!__dvd_lasterror) {
				_diReg[1] = _diReg[1];
				DVD_LowSetStatus((_SHIFTL((DVD_STATUS_DISK_ID_NOT_READ+1),16,8)|0x00000300),__dvd_finalsudcb);
				return;
			}
			__dvd_finalsudcb(result);
			return;
		}
	}

	step = 0;
	__dvd_stategettingerror();
}

static void __dvd_fwpatchcb(s32 result)
{
	static u32 step = 0;
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		if(step==0x0000) {
			step++;
			_diReg[1] = _diReg[1];
			DVD_LowUnlockDrive(__dvd_fwpatchcb);
			return;
		}
		if(step==0x0001) {
			step = 0;
			DVD_LowPatchDriveCode(__dvd_finalpatchcb);
			return;
		}
	}

	step = 0;
	__dvd_stategettingerror();
}

static void __dvd_checkaddonscb(s32 result)
{
	u32 txdsize;
	dvdcallbacklow cb;
	__dvd_drivechecked = 1;
	txdsize = (DVD_DISKIDSIZE-_diReg[6]);
	if(result&0x0001) {
		// if the read was successful but was interrupted by a break we issue the read again.
		if(txdsize!=DVD_DISKIDSIZE) {
			_diReg[1] = _diReg[1];
			DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
			DVD_LowReadId(&__dvd_tmpid0,__dvd_checkaddonscb);
			return;
		}
		__dvd_drivestate |= DVD_CHIPPRESENT;
	}

	cb = __dvd_finaladdoncb;
	__dvd_finaladdoncb = NULL;
	if(cb) cb(0x01);
}

static void __dvd_checkaddons(dvdcallbacklow cb)
{
	__dvd_finaladdoncb = cb;

	// try to read disc ID.
	_diReg[1] = _diReg[1];
	DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
	DVD_LowReadId(&__dvd_tmpid0,__dvd_checkaddonscb);
	return;
}

static void __dvd_fwpatchmem(dvdcallbacklow cb)
{
	__dvd_finalpatchcb = cb;

	_diReg[1] = _diReg[1];
	DCInvalidateRange(&__dvd_driveinfo,DVD_DRVINFSIZE);
	DVD_LowInquiry(&__dvd_driveinfo,__dvd_fwpatchcb);
	return;
}

static void __dvd_handlespinup()
{
	_diReg[1] = _diReg[1];
	DVD_LowUnlockDrive(__dvd_handlespinupcb);
	return;
}

static void __dvd_spinupdrivecb(s32 result)
{
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		if(!__dvd_drivechecked) {
			__dvd_checkaddons(__dvd_spinupdrivecb);
			return;
		}
		if(!(__dvd_drivestate&DVD_CHIPPRESENT)) {
			if(!(__dvd_drivestate&DVD_INTEROPER)) {
				if(!(__dvd_drivestate&DVD_DRIVERESET)) {
					DVD_Reset(DVD_RESETHARD);
					udelay(1150*1000);
				}
				__dvd_fwpatchmem(__dvd_spinupdrivecb);
				return;
			}
			__dvd_handlespinup();
			return;
		}

		__dvd_finalsudcb(result);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_statecoverclosed_spinupcb(s32 result)
{
	DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
	__dvd_laststate = __dvd_statecoverclosed_cmd;
	__dvd_statecoverclosed_cmd(__dvd_executing);
}

static void __DVDInterruptHandler(u32 nIrq,void *pCtx)
{
	s64 now;
	u32 status,ir,irm,irmm,diff;
	dvdcallbacklow cb;

	SYS_CancelAlarm(__dvd_timeoutalarm);

	irmm = 0;
	if(__dvd_lastcmdwasread) {
		__dvd_cmd_prev.buf = __dvd_cmd_curr.buf;
		__dvd_cmd_prev.len = __dvd_cmd_curr.len;
		__dvd_cmd_prev.offset = __dvd_cmd_curr.offset;
		if(__dvd_stopnextint) irmm |= 0x0008;
	}
	__dvd_lastcmdwasread = 0;
	__dvd_stopnextint = 0;

	status = _diReg[0];
	irm = (status&(DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK));
	ir = ((status&(DVD_DE_INT|DVD_TC_INT|DVD_BRK_INT))&(irm<<1));
	if(ir&DVD_BRK_INT) irmm |= 0x0008;
	if(ir&DVD_TC_INT) irmm |= 0x0001;
	if(ir&DVD_DE_INT) irmm |= 0x0002;

	if(irmm) __dvd_resetoccured = 0;

	_diReg[0] = (ir|irm);

	now = gettime();
	diff = diff_msec(__dvd_lastresetend,now);
	if(__dvd_resetoccured && diff>200) {
		status = _diReg[1];
		irm = status&DVD_CVR_MSK;
		ir = (status&DVD_CVR_INT)&(irm<<1);
		if(ir&0x0004) {
			cb = __dvd_resetcovercb;
			__dvd_resetcovercb = NULL;
			if(cb) {
				cb(0x0004);
			}
		}
		_diReg[1] = _diReg[1];
	} else {
		if(__dvd_waitcoverclose) {
			status = _diReg[1];
			irm = status&DVD_CVR_MSK;
			ir = (status&DVD_CVR_INT)&(irm<<1);
			if(ir&DVD_CVR_INT) irmm |= 0x0004;
			_diReg[1] = (ir|irm);
			__dvd_waitcoverclose = 0;
		} else
			_diReg[1] = 0;
	}

	if(irmm&0x0008) {
		if(!__dvd_breaking) irmm &= ~0x0008;
	}

	if(irmm&0x0001) {
		if(__ProcessNextCmd()) return;
	} else {
		__dvd_cmdlist[0].cmd = -1;
		__dvd_nextcmdnum = 0;
	}

	cb = __dvd_callback;
	__dvd_callback = NULL;
	if(cb) cb(irmm);

	__dvd_breaking = 0;
}

static void __dvd_patchdrivecb(s32 result)
{
	u32 cnt = 0;
	static u32 cmd_buf[3];
	static u32 stage = 0;
	static u32 nPos = 0;
	static u32 drv_address = 0x0040d000;
	const u32 chunk_size = 3*sizeof(u32);

	if(__dvdpatchcode==NULL || __dvdpatchcode_size<=0) return;

	while(stage!=0x0003) {
		__dvd_callback = __dvd_patchdrivecb;
		if(stage==0x0000) {
			for(cnt=0;cnt<chunk_size && nPos<__dvdpatchcode_size;cnt++,nPos++) ((u8*)cmd_buf)[cnt] = __dvdpatchcode[nPos];

			if(nPos>=__dvdpatchcode_size) stage = 0x0002;
			else stage = 0x0001;

			_diReg[1] = _diReg[1];
			_diReg[2] = DVD_FWWRITEMEM;
			_diReg[3] = drv_address;
			_diReg[4] = _SHIFTL(cnt,16,16);
			_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

			drv_address += cnt;
			return;
		}

		if(stage>=0x0001) {
			if(stage>0x0001) stage = 0x0003;
			else stage = 0;

			_diReg[1] = _diReg[1];
			_diReg[2] = cmd_buf[0];
			_diReg[3] = cmd_buf[1];
			_diReg[4] = cmd_buf[2];
			_diReg[7] = DVD_DI_START;
			return;
		}
	}
	__dvd_callback = NULL;
	__dvdpatchcode = NULL;
	__dvd_drivestate |= DVD_INTEROPER;
	DVD_LowFuncCall(0x0040d000,__dvd_finalpatchcb);
}

static void __dvd_unlockdrivecb(s32 result)
{
	u32 i;
	__dvd_callback = __dvd_finalunlockcb;
	_diReg[1] = _diReg[1];
	for(i=0;i<3;i++) _diReg[2+i] = ((u32*)__dvd_unlockcmd$222)[i];
	_diReg[7] = DVD_DI_START;
}

void __dvd_resetasync(dvdcbcallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__dvd_clearwaitingqueue();
	if(__dvd_canceling) __dvd_cancelcallback = cb;
	else {
		if(__dvd_executing) __dvd_executing->cb = NULL;
		DVD_CancelAllAsync(cb);
	}
	_CPU_ISR_Restore(level);
}

void __dvd_statebusy(dvdcmdblk *block)
{
	u32 len;
	__dvd_laststate = __dvd_statebusy;
	if(block->cmd>DVD_MAXCOMMANDS) return;

	switch(block->cmd) {
		case 1:					//Read(Sector)
		case 4:
			_diReg[1] = _diReg[1];
			len = block->len-block->txdsize;
			if(len<0x80000) block->currtxsize = len;
			else block->currtxsize = 0x80000;
			DVD_LowRead(block->buf+block->txdsize,block->currtxsize,(block->offset+block->txdsize),__dvd_statebusycb);
			return;
		case 2:					//Seek(Sector)
			_diReg[1] = _diReg[1];
			DVD_LowSeek(block->offset,__dvd_statebusycb);
			return;
		case 3:
		case 15:
			DVD_LowStopMotor(__dvd_statebusycb);
			return;
		case 5:					//ReadDiskID
			_diReg[1] = _diReg[1];
			block->currtxsize = DVD_DISKIDSIZE;
			DVD_LowReadId(block->buf,__dvd_statebusycb);
			return;
		case 6:
			_diReg[1] = _diReg[1];
			if(__dvd_autofinishing) {
				__dvd_executing->currtxsize = 0;
				DVD_LowRequestAudioStatus(0,__dvd_statebusycb);
			} else {
				__dvd_executing->currtxsize = 1;
				DVD_LowAudioStream(0,__dvd_executing->len,__dvd_executing->offset,__dvd_statebusycb);
			}
			return;
		case 7:
			_diReg[1] = _diReg[1];
			DVD_LowAudioStream(0x00010000,0,0,__dvd_statebusycb);
			return;
		case 8:
			_diReg[1] = _diReg[1];
			__dvd_autofinishing = 1;
			DVD_LowAudioStream(0,0,0,__dvd_statebusycb);
			return;
		case 9:
			_diReg[1] = _diReg[1];
			DVD_LowRequestAudioStatus(0,__dvd_statebusycb);
			return;
		case 10:
			_diReg[1] = _diReg[1];
			DVD_LowRequestAudioStatus(0x00010000,__dvd_statebusycb);
			return;
		case 11:
			_diReg[1] = _diReg[1];
			DVD_LowRequestAudioStatus(0x00020000,__dvd_statebusycb);
			return;
		case 12:
			_diReg[1] = _diReg[1];
			DVD_LowRequestAudioStatus(0x00030000,__dvd_statebusycb);
			return;
		case 13:
			_diReg[1] = _diReg[1];
			DVD_LowAudioBufferConfig(__dvd_executing->offset,__dvd_executing->len,__dvd_statebusycb);
			return;
		case 14:				//Inquiry
			_diReg[1] = _diReg[1];
			block->currtxsize = 0x20;
			DVD_LowInquiry(block->buf,__dvd_statebusycb);
			return;
		case 16:
			__dvd_lasterror = 0;
			__dvd_extensionsenabled = TRUE;
			DVD_LowSpinUpDrive(__dvd_statebusycb);
			return;
		case 17:
			_diReg[1] = _diReg[1];
			DVD_LowControlMotor(block->offset,__dvd_statebusycb);
			return;
		case 18:
			_diReg[1] = _diReg[1];
			DVD_LowSetGCMOffset(block->offset,__dvd_statebusycb);
			return;
		default:
			return;
	}
}

void __dvd_stateready()
{
	dvdcmdblk *block;
	if(!__dvd_checkwaitingqueue()) {
		__dvd_executing = NULL;
		return;
	}

	__dvd_executing = __dvd_popwaitingqueue();

	if(__dvd_fatalerror) {
		__dvd_executing->state = -1;
		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		if(block->cb) block->cb(-1,block);
		__dvd_stateready();
		return;
	}

	__dvd_currcmd = __dvd_executing->cmd;

	if(__dvd_resumefromhere) {
		if(__dvd_resumefromhere<=7) {
			switch(__dvd_resumefromhere) {
				case 1:
					__dvd_executing->state = 6;
					__dvd_statemotorstopped();
					break;
				case 2:
					__dvd_executing->state = 11;
					__dvd_statemotorstopped();
					break;
				case 3:
					__dvd_executing->state = 4;
					__dvd_statemotorstopped();
					break;
				case 4:
					__dvd_executing->state = 5;
					__dvd_statemotorstopped();
					break;
				case 5:
					__dvd_executing->state = -1;
					__dvd_stateerror(__dvd_cancellasterror);
					break;
				case 6:
					__dvd_executing->state = 3;
					__dvd_statecoverclosed();
					break;
				case 7:
					__dvd_executing->state = 7;
					__dvd_statemotorstopped();
					break;
				default:
					break;

			}
		}
		__dvd_resumefromhere = 0;
		return;
	}
	__dvd_executing->state = 1;
	__dvd_statebusy(__dvd_executing);
}

void __dvd_statecoverclosed()
{
	dvdcmdblk *blk;
	if(__dvd_currcmd==0x0004 || __dvd_currcmd==0x0005
		|| __dvd_currcmd==0x000d || __dvd_currcmd==0x000f
		|| __dvd_currcmd==0x0010) {
		__dvd_clearwaitingqueue();
		blk = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		if(blk->cb) blk->cb(-4,blk);
		__dvd_stateready();
	} else {
		__dvd_extensionsenabled = TRUE;
		DVD_LowSpinUpDrive(__dvd_statecoverclosed_spinupcb);
	}
}

void __dvd_statecoverclosed_cmd(dvdcmdblk *block)
{
	DVD_LowReadId(&__dvd_tmpid0,__dvd_statecoverclosedcb);
}

void __dvd_statemotorstopped()
{
	DVD_LowWaitCoverClose(__dvd_statemotorstoppedcb);
}

void __dvd_stateerror(s32 result)
{
	__dvd_storeerror(result);
	DVD_LowStopMotor(__dvd_stateerrorcb);
}

void __dvd_stategettingerror()
{
	DVD_LowRequestError(__dvd_stategettingerrorcb);
}

void __dvd_statecheckid2(dvdcmdblk *block)
{

}

void __dvd_statecheckid()
{
	dvdcmdblk *blk;
	if(__dvd_currcmd==0x0003) {
		if(memcmp(&__dvd_dummycmdblk,&__dvd_executing,sizeof(dvdcmdblk))) {
			DVD_LowStopMotor(__dvd_statecheckid1cb);
			return;
		}
		memcpy(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE);

		__dvd_executing->state = 1;
		DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
		__dvd_laststate = __dvd_statecheckid2;
		__dvd_statecheckid2(__dvd_executing);
		return;
	}
	if(__dvd_currcmd==0x0010) {
		blk = __dvd_executing;
		blk->state = 0;
		__dvd_executing = &__dvd_dummycmdblk;
		if(blk->cb) blk->cb(0,blk);
		__dvd_stateready();
		return;
	}
	if(__dvd_currcmd!=0x0005 && memcmp(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE)) {
		blk = __dvd_executing;
		blk->state = -1;
		__dvd_executing = &__dvd_dummycmdblk;
		if(blk->cb) blk->cb(-1,blk);			//terminate current operation
		if(__dvd_mountusrcb) __dvd_mountusrcb(DVD_DISKIDSIZE,blk);		//if we came across here, notify user callback of successful remount
		__dvd_stateready();
		return;
	}
	__dvd_statebusy(__dvd_executing);
}

void __dvd_statetimeout()
{
	__dvd_storeerror(0x01234568);
	DVD_Reset(DVD_RESETSOFT);
	__dvd_stateerrorcb(0);
}

void __dvd_stategotoretry()
{
	DVD_LowStopMotor(__dvd_stategotoretrycb);
}

s32 __issuecommand(s32 prio,dvdcmdblk *block)
{
	s32 ret;
	u32 level;
	if(__dvd_autoinvalidation &&
		(block->cmd==0x0001 || block->cmd==0x00004
		|| block->cmd==0x0005 || block->cmd==0x000e)) DCInvalidateRange(block->buf,block->len);

	_CPU_ISR_Disable(level);
	block->state = 0x0002;
	ret = __dvd_pushwaitingqueue(prio,block);
	if(!__dvd_executing) __dvd_stateready();
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_LowUnlockDrive(dvdcallbacklow cb)
{
	u32 i;

	__dvd_callback = __dvd_unlockdrivecb;
	__dvd_finalunlockcb = cb;
	__dvd_stopnextint = 0;

	for(i=0;i<3;i++) _diReg[2+i] = ((u32*)__dvd_unlockcmd$221)[i];
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowPatchDriveCode(dvdcallbacklow cb)
{
	__dvd_finalpatchcb = cb;
	__dvd_stopnextint = 0;

	if(__dvd_driveinfo.rel_date==DVD_MODEL04) {
		__dvdpatchcode = __dvd_patchcode04;
		__dvdpatchcode_size = __dvd_patchcode04_size;
	} else if((__dvd_driveinfo.rel_date&0x0000ff00)==0x00000500) {		// for wii: since i don't know the real date i have to mask & compare.
	} else if(__dvd_driveinfo.rel_date==DVD_MODEL06) {
		__dvdpatchcode = __dvd_patchcode06;
		__dvdpatchcode_size = __dvd_patchcode06_size;
	} else if(__dvd_driveinfo.rel_date==DVD_MODEL08) {
		__dvdpatchcode = __dvd_patchcode08;
		__dvdpatchcode_size = __dvd_patchcode08_size;
	} else if(__dvd_driveinfo.rel_date==DVD_MODEL08Q) {
		__dvdpatchcode = __dvd_patchcodeQ08;
		__dvdpatchcode_size = __dvd_patchcodeQ08_size;
	} else if((__dvd_driveinfo.rel_date&0x0000ff00)==0x00000900) {		// for wii: since i don't know the real date i have to mask & compare.
	} else {
		__dvdpatchcode = NULL;
		__dvdpatchcode_size = 0;
	}

	__dvd_patchdrivecb(0);
	return 1;
}

s32 DVD_LowSetOffset(s64 offset,dvdcallbacklow cb)
{
	__dvd_stopnextint = 0;
	__dvd_callback = cb;

	_diReg[2] = DVD_FWSETOFFSET;
	_diReg[3] = (u32)(offset>>2);
	_diReg[4] = 0;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowSetGCMOffset(s64 offset,dvdcallbacklow cb)
{
	static s64 loc_offset = 0;
	loc_offset = offset;

	__dvd_finaloffsetcb = cb;
	__dvd_stopnextint = 0;
	__dvd_usrdata = &loc_offset;

	DVD_LowUnlockDrive(__dvd_setgcmoffsetcb);
	return 1;
}

s32 DVD_LowReadmem(u32 address,void *buffer,dvdcallbacklow cb)
{
	__dvd_finalreadmemcb = cb;
	__dvd_usrdata = buffer;
	__dvd_callback = __dvd_readmemcb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_FWREADMEM;
	_diReg[3] = address;
	_diReg[4] = 0x00010000;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowFuncCall(u32 address,dvdcallbacklow cb)
{
	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_FWFUNCCALL;
	_diReg[3] = address;
	_diReg[4] = 0x66756E63;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowSpinMotor(u32 mode,dvdcallbacklow cb)
{
	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_FWCTRLMOTOR|(mode&0x0000ff00);
	_diReg[3] = 0;
	_diReg[4] = 0;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowEnableExtensions(u8 enable,dvdcallbacklow cb)
{
	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = (DVD_FWENABLEEXT|_SHIFTL(enable,16,8));
	_diReg[3] = 0;
	_diReg[4] = 0;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowSetStatus(u32 status,dvdcallbacklow cb)
{
	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = (DVD_FWSETSTATUS|(status&0x00ffffff));
	_diReg[3] = 0;
	_diReg[4] = 0;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowGetStatus(u32 *status,dvdcallbacklow cb)
{
	__dvd_finalstatuscb = cb;
	__dvd_usrdata = status;

	DVD_LowRequestError(__dvd_getstatuscb);
	return 1;
}

s32 DVD_LowControlMotor(u32 mode,dvdcallbacklow cb)
{
	__dvd_stopnextint = 0;
	__dvd_motorcntrl = mode;
	__dvd_finalsudcb = cb;
	DVD_LowUnlockDrive(__dvd_cntrldrivecb);

	return 1;

}

s32 DVD_LowSpinUpDrive(dvdcallbacklow cb)
{
	__dvd_finalsudcb = cb;
	__dvd_spinupdrivecb(1);

	return 1;
}

s32 DVD_LowRead(void *buf,u32 len,s64 offset,dvdcallbacklow cb)
{
	_diReg[6] = len;

	__dvd_cmd_curr.buf = buf;
	__dvd_cmd_curr.len = len;
	__dvd_cmd_curr.offset = offset;
	__DoRead(buf,len,offset,cb);
	return 1;
}

s32 DVD_LowSeek(s64 offset,dvdcallbacklow cb)
{
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_SEEKSECTOR;
	_diReg[3] = (u32)(offset>>2);
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowReadId(dvddiskid *diskID,dvdcallbacklow cb)
{
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_READDISKID;
	_diReg[3] = 0;
	_diReg[4] = DVD_DISKIDSIZE;
	_diReg[5] = (u32)diskID;
	_diReg[6] = DVD_DISKIDSIZE;
	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowRequestError(dvdcallbacklow cb)
{
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_REQUESTERROR;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowStopMotor(dvdcallbacklow cb)
{
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_STOPMOTOR;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowInquiry(dvddrvinfo *info,dvdcallbacklow cb)
{
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_DVDINQUIRY;
	_diReg[4] = 0x20;
	_diReg[5] = (u32)info;
	_diReg[6] = 0x20;
	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowWaitCoverClose(dvdcallbacklow cb)
{
	__dvd_callback = cb;
	__dvd_waitcoverclose = 1;
	__dvd_stopnextint = 0;
	_diReg[1] = DVD_CVR_MSK;
	return 1;
}

void DVD_LowReset(u32 reset_mode)
{
	u32 val;

	_diReg[1] = DVD_CVR_MSK;
	val = _piReg[9];
	_piReg[9] = ((val&~0x0004)|0x0001);

	udelay(12);

	if(reset_mode==DVD_RESETHARD) val |= 0x0004;
	val |= 0x0001;
	_piReg[9] = val;

	__dvd_resetoccured = 1;
	__dvd_lastresetend = gettime();
	__dvd_drivestate |= DVD_DRIVERESET;
}

s32 DVD_LowAudioStream(u32 subcmd,u32 len,s64 offset,dvdcallbacklow cb)
{
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_AUDIOSTREAM|subcmd;
	_diReg[3] = (u32)(offset>>2);
	_diReg[4] = len;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowAudioBufferConfig(s32 enable,u32 size,dvdcallbacklow cb)
{
	u32 val;
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	val = 0;
	if(enable) {
		val |= 0x00010000;
		if(!size) val |= 0x0000000a;
	}

	_diReg[2] = DVD_AUDIOCONFIG|val;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowRequestAudioStatus(u32 subcmd,dvdcallbacklow cb)
{
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_AUDIOSTATUS|subcmd;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

//special, only used in bios replacement. therefor only there extern'd
s32 __DVDAudioBufferConfig(dvdcmdblk *block,u32 enable,u32 size,dvdcbcallback cb)
{
	if(!block) return 0;

	block->cmd = 0x000d;
	block->offset = enable;
	block->len = size;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_ReadDiskID(dvdcmdblk *block,dvddiskid *id,dvdcbcallback cb)
{
	if(!block || !id) return 0;

	block->cmd = 0x0005;
	block->buf = id;
	block->len = DVD_DISKIDSIZE;
	block->offset = 0;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_ReadAbsAsyncPrio(dvdcmdblk *block,void *buf,u32 len,s64 offset,dvdcbcallback cb,s32 prio)
{
	block->cmd = 0x0001;
	block->buf = buf;
	block->len = len;
	block->offset = offset;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(prio,block);
}

s32 DVD_ReadAbsAsyncForBS(dvdcmdblk *block,void *buf,u32 len,s64 offset,dvdcbcallback cb)
{
	block->cmd = 0x0004;
	block->buf = buf;
	block->len = len;
	block->offset = offset;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_SeekAbsAsyncPrio(dvdcmdblk *block,s64 offset,dvdcbcallback cb,s32 prio)
{
	block->cmd = 0x0002;
	block->offset = offset;
	block->cb = cb;

	return __issuecommand(prio,block);
}

s32 DVD_InquiryAsync(dvdcmdblk *block,dvddrvinfo *info,dvdcbcallback cb)
{
	block->cmd = 0x000e;
	block->buf = info;
	block->len = 0x20;
	block->txdsize = 0;
	block->cb = cb;
	return __issuecommand(2,block);
}

s32 DVD_Inquiry(dvdcmdblk *block,dvddrvinfo *info)
{
	u32 level;
	s32 state,ret;
	ret = DVD_InquiryAsync(block,info,__dvd_inquirysynccb);
	if(!ret) return -1;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==0) ret = block->txdsize;
		else if(state==-1) ret = -1;
		else if(state==10) ret = -3;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=0 && state!=-1 && state!=10);
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_ReadPrio(dvdcmdblk *block,void *buf,u32 len,s64 offset,s32 prio)
{
	u32 level;
	s32 state,ret;
	if(offset>=0 && offset<8511160320LL) {
		ret = DVD_ReadAbsAsyncPrio(block,buf,len,offset,__dvd_readsynccb,prio);
		if(!ret) return -1;

		_CPU_ISR_Disable(level);
		do {
			state = block->state;
			if(state==0) ret = block->txdsize;
			else if(state==-1) ret = -1;
			else if(state==10) ret = -3;
			else LWP_ThreadSleep(__dvd_wait_queue);
		} while(state!=0 && state!=-1 && state!=10);
		_CPU_ISR_Restore(level);
		return ret;
	}
	return -1;
}

s32 DVD_SeekPrio(dvdcmdblk *block,s64 offset,s32 prio)
{
	u32 level;
	s32 state,ret;
	if(offset>0 && offset<8511160320LL) {
		ret = DVD_SeekAbsAsyncPrio(block,offset,__dvd_seeksynccb,prio);
		if(!ret) return -1;

		_CPU_ISR_Disable(level);
		do {
			state = block->state;
			if(state==0) ret = 0;
			else if(state==-1) ret = -1;
			else if(state==10) ret = -3;
			else LWP_ThreadSleep(__dvd_wait_queue);
		} while(state!=0 && state!=-1 && state!=10);
		_CPU_ISR_Restore(level);

		return ret;
	}
	return -1;
}

s32 DVD_CancelAllAsync(dvdcbcallback cb)
{
	u32 level;
	_CPU_ISR_Disable(level);
	DVD_Pause();
	_CPU_ISR_Restore(level);
	return 1;
}

s32 DVD_StopStreamAtEndAsync(dvdcmdblk *block,dvdcbcallback cb)
{
	block->cmd = 0x08;
	block->cb = cb;
	return __issuecommand(1,block);
}

s32 DVD_StopStreamAtEnd(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
	ret = DVD_StopStreamAtEndAsync(block,__dvd_streamatendsynccb);
	if(!ret) return -1;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==0 || state==-1) ret = -1;
		else if(state==10) ret = block->txdsize;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=0 && state!=-1 && state!=10);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_SpinUpDriveAsync(dvdcmdblk *block,dvdcbcallback cb)
{
	DVD_Reset(DVD_RESETNONE);

	block->cmd = 0x10;
	block->cb = cb;
	return __issuecommand(1,block);
}

s32 DVD_SpinUpDrive(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
	ret = DVD_SpinUpDriveAsync(block,__dvd_spinupdrivesynccb);
	if(!ret) return -1;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==0 || state==-1) ret = -1;
		else if(state==10) ret = block->txdsize;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=0 && state!=-1 && state!=10);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_ControlDriveAsync(dvdcmdblk *block,u32 cmd,dvdcbcallback cb)
{
	block->cmd = 0x11;
	block->cb = cb;
	block->offset = cmd;
	return __issuecommand(1,block);
}

s32 DVD_ControlDrive(dvdcmdblk *block,u32 cmd)
{
	s32 ret,state;
	u32 level;
	ret = DVD_ControlDriveAsync(block,cmd,__dvd_motorcntrlsynccb);

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==0 || state==-1) ret = -1;
		else if(state==10) ret = block->txdsize;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=0 && state!=-1 && state!=10);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_SetGCMOffsetAsync(dvdcmdblk *block,s64 offset,dvdcbcallback cb)
{
	block->cmd = 0x12;
	block->cb = cb;
	block->offset = offset;
	return __issuecommand(1,block);
}

s32 DVD_SetGCMOffset(dvdcmdblk *block,s64 offset)
{
	s32 ret,state;
	u32 level;
	ret = DVD_SetGCMOffsetAsync(block,offset,__dvd_setgcmsynccb);

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==0 || state==-1) ret = -1;
		else if(state==10) ret = block->txdsize;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=0 && state!=-1 && state!=10);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_GetCmdBlockStatus(dvdcmdblk *block)
{
	u32 level;
	s32 ret = -1;

	_CPU_ISR_Disable(level);
	if(block) {
		if((ret=block->state)==0x0003) ret = 1;
	}
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_GetDriveStatus()
{
	s32 ret;
	u32 level;

	_CPU_ISR_Disable(level);
	if(__dvd_fatalerror) ret = -1;
	else {
		if(__dvd_pausingflag) ret = 8;
		else {
			if(!__dvd_executing || __dvd_executing==&__dvd_dummycmdblk) ret = 0;
			else ret = DVD_GetCmdBlockStatus(__dvd_executing);
		}
	}
	_CPU_ISR_Restore(level);
	return ret;
}

void DVD_Pause()
{
	u32 level;

	_CPU_ISR_Disable(level);
	__dvd_pauseflag = 1;
	if(__dvd_executing==NULL) __dvd_pausingflag = 1;
	_CPU_ISR_Restore(level);
}

void DVD_Reset(u32 reset_mode)
{
	__dvd_drivestate &= ~(DVD_INTEROPER|DVD_CHIPPRESENT|DVD_DRIVERESET);

	if(reset_mode!=DVD_RESETNONE)
		DVD_LowReset(reset_mode);

	_diReg[0] = (DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK);
	_diReg[1] = _diReg[1];

	__dvd_resetrequired = 0;
	__dvd_internalretries = 0;
}

void callback(s32 result,dvdcmdblk *block)
{
	if(result==0x00) {
		DVD_ReadDiskID(block,&__dvd_tmpid0,callback);
		return;
	}
	else if(result>=DVD_DISKIDSIZE) {
		memcpy(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE);
	} else if(result==-4) {
		DVD_SpinUpDriveAsync(block,callback);
		return;
	}
	if(__dvd_mountusrcb) __dvd_mountusrcb(result,block);
}

s32 DVD_MountAsync(dvdcmdblk *block,dvdcbcallback cb)
{
	__dvd_mountusrcb = cb;
	DVD_Reset(DVD_RESETHARD);
	udelay(1150*1000);
	return DVD_SpinUpDriveAsync(block,callback);
}

s32 DVD_Mount()
{
	s32 ret = 0;
	s32 state;
	u32 level;

	ret = DVD_MountAsync(&__dvd_block$15,__dvd_mountsynccb);
	if(!ret) return -1;

	_CPU_ISR_Disable(level);
	do {
		state = __dvd_block$15.state;
		if(state==0) ret = 0;
		else if(state==-1) ret = -1;
		else if(state==10) ret = -3;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=0 && state!=-1 && state!=10);
	__dvd_mountusrcb = NULL;		//set to zero coz this is only used to sync for this function.
	_CPU_ISR_Restore(level);

	return ret;
}

dvddiskid* DVD_GetCurrentDiskID()
{
	return __dvd_diskID;
}

dvddrvinfo* DVD_GetDriveInfo()
{
	return &__dvd_driveinfo;
}

void DVD_Init()
{
	if(!__dvd_initflag) {
		__dvd_initflag = 1;
		__dvd_clearwaitingqueue();
		__DVDInitWA();

		IRQ_Request(IRQ_PI_DI,__DVDInterruptHandler,NULL);
		__UnmaskIrq(IRQMASK(IRQ_PI_DI));

		SYS_CreateAlarm(&__dvd_timeoutalarm);
		LWP_InitQueue(&__dvd_wait_queue);
	}
}

u32 DVD_SetAutoInvalidation(u32 auto_inv)
{
	u32 ret = __dvd_autoinvalidation;
	__dvd_autoinvalidation= auto_inv;
	return ret;
}

static bool dvdio_Startup()
{
	DVD_Init();

	if (mfpvr() == 0x00083214) // GameCube
	{
		DVD_Mount();
	}
	else
	{
		DVD_Reset(DVD_RESETHARD);
		DVD_ReadDiskID(&__dvd_block$15, &__dvd_tmpid0, callback);
	}
	return true;
}

static bool dvdio_IsInserted()
{
	u32 status = 0;
	DVD_LowGetStatus(&status, NULL);

	if(DVD_STATUS(status) == DVD_STATUS_READY)
		return true;

	return false;
}

static bool dvdio_ReadSectors(sec_t sector,sec_t numSectors,void *buffer)
{
	dvdcmdblk blk;

	if(DVD_ReadPrio(&blk, buffer, numSectors<<11, sector << 11, 2) <= 0)
		return false;

	return true;
}

static bool dvdio_WriteSectors(sec_t sector,sec_t numSectors,const void *buffer)
{
	return true;
}

static bool dvdio_ClearStatus()
{
	return true;
}

static bool dvdio_Shutdown()
{
	return true;
}

const DISC_INTERFACE __io_gcdvd = {
	DEVICE_TYPE_GAMECUBE_DVD,
	FEATURE_MEDIUM_CANREAD | FEATURE_GAMECUBE_DVD,
	(FN_MEDIUM_STARTUP)&dvdio_Startup,
	(FN_MEDIUM_ISINSERTED)&dvdio_IsInserted,
	(FN_MEDIUM_READSECTORS)&dvdio_ReadSectors,
	(FN_MEDIUM_WRITESECTORS)&dvdio_WriteSectors,
	(FN_MEDIUM_CLEARSTATUS)&dvdio_ClearStatus,
	(FN_MEDIUM_SHUTDOWN)&dvdio_Shutdown
};

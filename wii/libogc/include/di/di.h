/*-------------------------------------------------------------

di.h -- Drive Interface library

Team Twiizers
Copyright (C) 2008

Erant
marcan

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

/*
All buffers in this document need to be 32-byte aligned!
*/

#ifndef __DI_H__
#define __DI_H__

#include <stdint.h>
#include <ogc/ipc.h>
#include <ogc/disc_io.h>

#define DVD_IDENTIFY			0x12
#define DVD_READ_DISCID			0x70
#define	DVD_LOW_READ			0x71
#define DVD_WAITFORCOVERCLOSE	0x79
#define	DVD_READ_PHYSICAL		0x80
#define	DVD_READ_COPYRIGHT		0x81
#define DVD_READ_DISCKEY		0x82
#define DVD_GETCOVER			0x88
#define DVD_RESET				0x8A
#define	DVD_OPEN_PARTITION		0x8B
#define	DVD_CLOSE_PARTITION		0x8C
#define DVD_READ_UNENCRYPTED	0x8D
#define DVD_REPORTKEY			0xA4
#define	DVD_READ				0xD0
#define DVD_READ_CONFIG			0xD1
#define DVD_READ_BCA			0xDA
#define DVD_GET_ERROR 			0xE0
#define	DVD_SET_MOTOR			0xE3

#define DVD_READY				0x1
#define DVD_INIT				0x2
#define DVD_UNKNOWN				0x4
#define DVD_NO_DISC				0x8
#define DVD_IOS_ERROR			0x10
#define DVD_D0					0x20
#define DVD_A8					0x40

#define DVD_COVER_DISC_INSERTED 0x02

#define LIBDI_MAX_RETRIES		16

#define DEVICE_TYPE_WII_DVD		(('W'<<24)|('D'<<16)|('V'<<8)|'D')

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
	uint16_t	rev;
	uint16_t	dev_code;
	uint32_t	rel_date;
}DI_DriveID;

typedef int(*di_callback)(uint32_t status, uint32_t error);
typedef int(*read_func)(void*,uint32_t,uint32_t);
typedef int(*read_func_async)(void*,uint32_t,uint32_t,ipccallback);

extern int di_fd;
extern const DISC_INTERFACE __io_wiidvd;

int DI_Init();
void DI_LoadDVDX(bool load);
void DI_UseCache(bool use);
void DI_SetInitCallback(di_callback cb);
void DI_Mount();
void DI_Close();
int DI_GetStatus();

int DI_Identify(DI_DriveID* id);
int DI_CheckDVDSupport();
int DI_ReadDiscID(u64 *id);
int DI_GetError(uint32_t* error);
int DI_GetCoverRegister(uint32_t* status);
int DI_Reset();

int DI_StopMotor();
int DI_Eject();
int DI_KillDrive();

int DI_ReadDVD(void* buf, uint32_t len, uint32_t lba);
int DI_ReadDVDAsync(void* buf, uint32_t len, uint32_t lba, ipccallback ipc_cb);

int DI_Read(void *buf, u32 size, u32 offset);
int DI_UnencryptedRead(void *buf, u32 size, u32 offset);

int DI_ReadDVDConfig(uint32_t* val, uint32_t flag);
int DI_ReadDVDCopyright(uint32_t* copyright);
int DI_ReadDVDDiscKey(void *buf);
int DI_ReadDVDPhysical(void *buf);
int DI_Read_BCA(void *buf);
int DI_ReportKey(int keytype, uint32_t lba, void* buf);

int DI_OpenPartition(u32 offset);
int DI_ClosePartition(void);

#ifdef __cplusplus
}
#endif

#endif

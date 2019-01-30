/*-------------------------------------------------------------

wiilaunch.c -- Wii NAND title launching and argument passing

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

#if defined(HW_RVL)

#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ipc.h"
#include "asm.h"
#include "processor.h"
#include "es.h"
#include "video.h"
#include "network.h"
#include "wiilaunch.h"

static char __nandbootinfo[] ATTRIBUTE_ALIGN(32) = "/shared2/sys/NANDBOOTINFO";
static char __stateflags[] ATTRIBUTE_ALIGN(32) = "/title/00000001/00000002/data/state.dat";

static int __initialized = 0;
static char args_set = 0;

typedef struct {
	u32 checksum;
	u32 argsoff;
	u8 unk1;
	u8 unk2;
	u8 apptype;
	u8 titletype;
	u32 launchcode;
	u32 unknown[2];
	u64 launcher;
	u8 argbuf[0x1000];
} NANDBootInfo;

typedef struct {
	u32 checksum;
	u8 flags;
	u8 type;
	u8 discstate;
	u8 returnto;
	u32 unknown[6];
} StateFlags;

#define TYPE_RETURN 3
#define TYPE_NANDBOOT 4
#define TYPE_SHUTDOWNSYSTEM 5
#define RETURN_TO_MENU 0
#define RETURN_TO_SETTINGS 1
#define RETURN_TO_ARGS 2

static NANDBootInfo nandboot ATTRIBUTE_ALIGN(32);

static StateFlags stateflags ATTRIBUTE_ALIGN(32);

static u32 __CalcChecksum(u32 *buf, int len)
{
	u32 sum = 0;
	int i;
	len = (len/4);

	for(i=1; i<len; i++)
		sum += buf[i];

	return sum;
}

static void __SetChecksum(void *buf, int len)
{
	u32 *p = (u32*)buf;
	p[0] = __CalcChecksum(p, len);
}

static int __ValidChecksum(void *buf, int len)
{
	u32 *p = (u32*)buf;
	return p[0] == __CalcChecksum(p, len);
}

static s32 __WII_ReadStateFlags(void)
{
	int fd;
	int ret;

	fd = IOS_Open(__stateflags,IPC_OPEN_READ);
	if(fd < 0) {
		memset(&stateflags,0,sizeof(stateflags));
		return WII_EINTERNAL;
	}

	ret = IOS_Read(fd, &stateflags, sizeof(stateflags));
	IOS_Close(fd);
	if(ret != sizeof(stateflags)) {
		memset(&stateflags,0,sizeof(stateflags));
		return WII_EINTERNAL;
	}
	if(!__ValidChecksum(&stateflags, sizeof(stateflags))) {
		memset(&stateflags,0,sizeof(stateflags));
		return WII_ECHECKSUM;
	}
	return 0;
}

static s32 __WII_ReadNANDBootInfo(void)
{
	int fd;
	int ret;

	fd = IOS_Open(__nandbootinfo,IPC_OPEN_READ);
	if(fd < 0) {
		memset(&nandboot,0,sizeof(nandboot));
		return WII_EINTERNAL;
	}

	ret = IOS_Read(fd, &nandboot, sizeof(nandboot));
	IOS_Close(fd);
	if(ret != sizeof(nandboot)) {
		memset(&nandboot,0,sizeof(nandboot));
		return WII_EINTERNAL;
	}
	if(!__ValidChecksum(&nandboot, sizeof(nandboot))) {
		memset(&nandboot,0,sizeof(nandboot));
		return WII_ECHECKSUM;
	}
	return 0;
}

static s32 __WII_WriteStateFlags(void)
{
	int fd;
	int ret;

	__SetChecksum(&stateflags, sizeof(stateflags));

	fd = IOS_Open(__stateflags,IPC_OPEN_READ|IPC_OPEN_WRITE);
	if(fd < 0) {
		return WII_EINTERNAL;
	}

	ret = IOS_Write(fd, &stateflags, sizeof(stateflags));
	IOS_Close(fd);
	if(ret != sizeof(stateflags)) {
		return WII_EINTERNAL;
	}
	return 0;
}

static s32 __WII_WriteNANDBootInfo(void)
{
	int fd;
	int ret;

	__SetChecksum(&nandboot, sizeof(nandboot));

	fd = IOS_Open(__nandbootinfo,IPC_OPEN_READ|IPC_OPEN_WRITE);
	if(fd < 0) {
		return WII_EINTERNAL;
	}

	ret = IOS_Write(fd, &nandboot, sizeof(nandboot));
	IOS_Close(fd);
	if(ret != sizeof(nandboot)) {
		return WII_EINTERNAL;
	}
	return 0;
}

s32 WII_Initialize(void)
{
	__WII_ReadStateFlags();
	__WII_ReadNANDBootInfo();
	__initialized = 1;
	return 0;
}

s32 __WII_SetArgs(const char **argv)
{
	int argslen = 0;
	int buflen = 0;
	int argvp, argp;
	int argc = 0;
	int i;

	if(!__initialized)
		return WII_ENOTINIT;

	for(i=0; argv[i] != NULL; i++) {
		argslen += strlen(argv[i]) + 1;
		argc++;
	}
	buflen = (argslen + 3) & ~3;
	buflen += 4 * argc;
	buflen += 8;

	if(buflen > 0x1000)
		return WII_E2BIG;

	argp = 0x1000 - argslen;
	argvp = 0x1000 - buflen;

	memset(&nandboot.argbuf, 0, sizeof(nandboot.argbuf));

	nandboot.argsoff = 0x1000 + argvp;
	*((u32*)&nandboot.argbuf[argvp]) = argc;
	argvp += 4;

	for(i=0; i<argc; i++) {
		strcpy((char*)&nandboot.argbuf[argp], argv[i]);
		*((u32*)&nandboot.argbuf[argvp]) = argp + 0x1000;
		argvp += 4;
		argp += strlen(argv[i]) + 1;
	}
	*((u32*)&nandboot.argbuf[argvp]) = 0;

	return 0;
}

s32 WII_LaunchTitle(u64 titleID)
{
	s32 res;
	u32 numviews;
	STACK_ALIGN(tikview,views,4,32);

	if(!__initialized)
		return WII_ENOTINIT;

	res = ES_GetNumTicketViews(titleID, &numviews);
	if(res < 0) {
		return res;
	}
	if(numviews > 4) {
		return WII_EINTERNAL;
	}
	res = ES_GetTicketViews(titleID, views, numviews);
	if(res < 0)
		return res;

	net_wc24cleanup();

	if (args_set == 0)
	{
		memset(&nandboot,0,sizeof(NANDBootInfo));
		nandboot.apptype = 0x81;
		if(titleID == 0x100000002LL)
			nandboot.titletype = 4;
		else
			nandboot.titletype = 2;
		if(ES_GetTitleID(&nandboot.launcher) < 0)
			nandboot.launcher = 0x0000000100000002LL;
		nandboot.checksum = __CalcChecksum((u32*)&nandboot,sizeof(NANDBootInfo));
		__WII_WriteNANDBootInfo();
	}
	VIDEO_SetBlack(1);
	VIDEO_Flush();

	res = ES_LaunchTitle(titleID, &views[0]);
	if(res < 0)
		return res;
	return WII_EINTERNAL;
}

s32 WII_LaunchTitleWithArgs(u64 titleID, int launchcode, ...)
{
	char argv0[20];
	const char *argv[256];
	int argc = 1;
	va_list args;
	int ret = 0;

	if(!__initialized)
		return WII_ENOTINIT;

	sprintf(argv0, "%016llx", titleID);

	argv[0] = argv0;

	va_start(args, launchcode);

	do {
		argv[argc++] = va_arg(args, const char *);
	} while(argv[argc - 1] != NULL);

	va_end(args);

	if(ES_GetTitleID(&nandboot.launcher) < 0)
		nandboot.launcher = 0x100000002LL;

	if(titleID == 0x100000002LL)
		nandboot.titletype = 4;
	else
		nandboot.titletype = 2;
	nandboot.apptype = 0x81;
	nandboot.launchcode = launchcode;

	stateflags.type = TYPE_RETURN;
	stateflags.returnto = RETURN_TO_ARGS;

	__WII_SetArgs(argv);

	__WII_WriteStateFlags();
	__WII_WriteNANDBootInfo();

	args_set = 1;

	ret = WII_LaunchTitle(titleID);
	if(ret < 0)
		args_set = 0;

	return ret;
}

s32 WII_ReturnToMenu(void)
{
	if(!__initialized)
		return WII_ENOTINIT;
	stateflags.type = TYPE_RETURN;
	stateflags.returnto = RETURN_TO_MENU;
	__WII_WriteStateFlags();
	return WII_LaunchTitle(0x100000002LL);
}

s32 WII_ReturnToSettings(void)
{
	if(!__initialized)
		return WII_ENOTINIT;
	stateflags.type = TYPE_RETURN;
	stateflags.returnto = RETURN_TO_SETTINGS;
	__WII_WriteStateFlags();
	return WII_LaunchTitle(0x100000002LL);
}

s32 WII_ReturnToSettingsPage(const char *page)
{
	if(!__initialized)
		return WII_ENOTINIT;

	return WII_LaunchTitleWithArgs(0x100000002LL, 1, page, NULL);
}

s32 WII_OpenURL(const char *url)
{
	u32 tmdsize;
	u64 tid = 0;
	u64 *list;
	u32 titlecount;
	s32 ret;
	u32 i;

	if(!__initialized)
		return WII_ENOTINIT;

	ret = ES_GetNumTitles(&titlecount);
	if(ret < 0)
		return WII_EINTERNAL;

	list = memalign(32, titlecount * sizeof(u64) + 32);

	ret = ES_GetTitles(list, titlecount);
	if(ret < 0) {
		free(list);
		return WII_EINTERNAL;
	}

	for(i=0; i<titlecount; i++) {
		if((list[i] & ~0xFF) == 0x1000148414400LL) {
			tid = list[i];
			break;
		}
	}
	free(list);

	if(!tid)
		return WII_EINSTALL;

	if(ES_GetStoredTMDSize(tid, &tmdsize) < 0)
		return WII_EINSTALL;

	return WII_LaunchTitleWithArgs(tid, 0, url, NULL);
}

#endif

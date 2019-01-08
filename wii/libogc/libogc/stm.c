/*-------------------------------------------------------------

stm.c - System and miscellaneous hardware control functions

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

#if defined(HW_RVL)

#include <stdio.h>
#include "ipc.h"
#include "system.h"
#include "asm.h"
#include "processor.h"
#include "stm.h"

#define IOCTL_STM_EVENTHOOK			0x1000
#define IOCTL_STM_GET_IDLEMODE		0x3001
#define IOCTL_STM_RELEASE_EH		0x3002
#define IOCTL_STM_HOTRESET			0x2001
#define IOCTL_STM_HOTRESET_FOR_PD	0x2002
#define IOCTL_STM_SHUTDOWN			0x2003
#define IOCTL_STM_IDLE				0x2004
#define IOCTL_STM_WAKEUP			0x2005
#define IOCTL_STM_VIDIMMING			0x5001
#define IOCTL_STM_LEDFLASH			0x6001
#define IOCTL_STM_LEDMODE			0x6002
#define IOCTL_STM_READVER			0x7001
#define IOCTL_STM_READDDRREG		0x4001
#define IOCTL_STM_READDDRREG2		0x4002

static s32 __stm_eh_fd = -1;
static s32 __stm_imm_fd = -1;
static u32 __stm_vdinuse = 0;
static u32 __stm_initialized= 0;
static u32 __stm_ehregistered= 0;
static u32 __stm_ehclear= 0;

static u32 __stm_ehbufin[0x08] ATTRIBUTE_ALIGN(32) = {0,0,0,0,0,0,0,0};
static u32 __stm_ehbufout[0x08] ATTRIBUTE_ALIGN(32) = {0,0,0,0,0,0,0,0};
static u32 __stm_immbufin[0x08] ATTRIBUTE_ALIGN(32) = {0,0,0,0,0,0,0,0};
static u32 __stm_immbufout[0x08] ATTRIBUTE_ALIGN(32) = {0,0,0,0,0,0,0,0};

static char __stm_eh_fs[] ATTRIBUTE_ALIGN(32) = "/dev/stm/eventhook";
static char __stm_imm_fs[] ATTRIBUTE_ALIGN(32) = "/dev/stm/immediate";

s32 __STM_SetEventHook();
s32 __STM_ReleaseEventHook();
static s32 __STMEventHandler(s32 result,void *usrdata);

stmcallback __stm_eventcb = NULL;

static vu16* const _viReg = (u16*)0xCC002000;

s32 __STM_Init()
{
	if(__stm_initialized==1) return 1;

	__stm_vdinuse = 0;
	__stm_imm_fd = IOS_Open(__stm_imm_fs,0);
	if(__stm_imm_fd<0) return 0;

	__stm_eh_fd = IOS_Open(__stm_eh_fs,0);
	if(__stm_eh_fd<0) return 0;

	__stm_initialized = 1;
	__STM_SetEventHook();
	return 1;
}

s32 __STM_Close()
{
	s32 res;
	s32 ret = 0;
	__STM_ReleaseEventHook();

	if(__stm_imm_fd >= 0) {
		res = IOS_Close(__stm_imm_fd);
		if(res < 0) ret = res;
		__stm_imm_fd = -1;
	}
	if(__stm_eh_fd >= 0) {
		res = IOS_Close(__stm_eh_fd);
		if(res < 0) ret = res;
		__stm_eh_fd = -1;
	}
	__stm_initialized = 0;
	return ret;
}

s32 __STM_SetEventHook()
{
	s32 ret;
	u32 level;

	if(__stm_initialized==0) return STM_ENOTINIT;

	__stm_ehclear = 0;

	_CPU_ISR_Disable(level);
	ret = IOS_IoctlAsync(__stm_eh_fd,IOCTL_STM_EVENTHOOK,__stm_ehbufin,0x20,__stm_ehbufout,0x20,__STMEventHandler,NULL);
	if(ret<0) __stm_ehregistered = 0;
	else __stm_ehregistered = 1;
	_CPU_ISR_Restore(level);

	return ret;
}

s32 __STM_ReleaseEventHook()
{
	s32 ret;

	if(__stm_initialized==0) return STM_ENOTINIT;
	if(__stm_ehregistered==0) return STM_ENOHANDLER;

	__stm_ehclear = 1;

	ret = IOS_Ioctl(__stm_imm_fd,IOCTL_STM_RELEASE_EH,__stm_immbufin,0x20,__stm_immbufout,0x20);
	if(ret>=0) __stm_ehregistered = 0;

	return ret;
}

static s32 __STMEventHandler(s32 result,void *usrdata)
{
	__stm_ehregistered = 0;

	if(result < 0) { // shouldn't happen
		return result;
	}

	if(__stm_ehclear) { //release
		return 0;
	}

	if(__stm_eventcb) {
		__stm_eventcb(__stm_ehbufout[0]);
	}

	__STM_SetEventHook();

	return 0;
}

stmcallback STM_RegisterEventHandler(stmcallback newhandler)
{
	stmcallback old;
	old = __stm_eventcb;
	__stm_eventcb = newhandler;
	return old;
}

s32 STM_ShutdownToStandby()
{
	int res;

	_viReg[1] = 0;
	if(__stm_initialized==0) {
		return STM_ENOTINIT;
	}
	__stm_immbufin[0] = 0;
	res= IOS_Ioctl(__stm_imm_fd,IOCTL_STM_SHUTDOWN,__stm_immbufin,0x20,__stm_immbufout,0x20);
	if(res<0) {
	}
	return res;
}

s32 STM_ShutdownToIdle()
{
	int res;

	_viReg[1] = 0;
	if(__stm_initialized==0) {
		return STM_ENOTINIT;
	}
	switch(SYS_GetHollywoodRevision()) {
		case 0:
		case 1:
		case 2:
			__stm_immbufin[0] = 0xFCA08280;
		default:
			__stm_immbufin[0] = 0xFCE082C0;
	}
	res= IOS_Ioctl(__stm_imm_fd,IOCTL_STM_IDLE,__stm_immbufin,0x20,__stm_immbufout,0x20);
	if(res<0) {
	}
	return res;
}

s32 STM_SetLedMode(u32 mode)
{
	int res;
	if(__stm_initialized==0) {
		return STM_ENOTINIT;
	}
	__stm_immbufin[0] = mode;
	res= IOS_Ioctl(__stm_imm_fd,IOCTL_STM_LEDMODE,__stm_immbufin,0x20,__stm_immbufout,0x20);
	if(res<0) {
	}
	return res;
}

s32 STM_RebootSystem()
{
	int res;

	_viReg[1] = 0;
	if(__stm_initialized==0) {
		return STM_ENOTINIT;
	}
	__stm_immbufin[0] = 0;
	res= IOS_Ioctl(__stm_imm_fd,IOCTL_STM_HOTRESET,__stm_immbufin,0x20,__stm_immbufout,0x20);
	if(res<0) {
	}
	return res;
}

#endif /* defined(HW_RVL) */

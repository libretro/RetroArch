/*-------------------------------------------------------------

arqmgr.c -- ARAM task request queue management

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

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

#include "asm.h"
#include "processor.h"
#include "arqueue.h"
#include "arqmgr.h"

#define ARQM_STACKENTRIES		16
#define ARQM_ZEROBYTES			256
#define ROUNDUP32(x)			(((u32)(x)+0x1f)&~0x1f)

typedef struct _arqm_info {
	ARQRequest arqhandle;
	ARQMCallback callback;
	void *buffer;
	u32 file_len;
	u32 read_len;
	u32 aram_start;
	u32 curr_read_offset;
	u32 curr_aram_offset;
	volatile BOOL polled;
} ARQM_Info;

static u32 __ARQMStackLocation;
static u32 __ARQMFreeBytes;
static u32 __ARQMStackPointer[ARQM_STACKENTRIES];
static ARQM_Info __ARQMInfo[ARQM_STACKENTRIES];
static u8 __ARQMZeroBuffer[ARQM_ZEROBYTES] ATTRIBUTE_ALIGN(32);

static void __ARQMPollCallback(ARQRequest *req)
{
	u32 i;
	ARQM_Info *ptr = NULL;

	for(i=0;i<ARQM_STACKENTRIES;i++) {
		ptr = &__ARQMInfo[i];
		if(req==&ptr->arqhandle) break;
	}
	if(i>=ARQM_STACKENTRIES) return;

	ptr->callback = NULL;
	ptr->polled = TRUE;
}

void ARQM_Init(u32 arambase,s32 len)
{
	u32 i;

	if(len<=0) return;

	__ARQMStackLocation = 0;
	__ARQMStackPointer[0] = arambase;
	__ARQMFreeBytes = len;

	for(i=0;i<ARQM_ZEROBYTES/sizeof(u32);i++) ((u32*)__ARQMZeroBuffer)[i] = 0;
	ARQM_PushData(__ARQMZeroBuffer,ARQM_ZEROBYTES);

}

u32 ARQM_PushData(void *buffer,s32 len)
{
	u32 rlen,level;
	ARQM_Info *ptr;

	if(((u32)buffer)&0x1f || len<=0) return 0;

	rlen = ROUNDUP32(len);
	if(__ARQMFreeBytes>=rlen && __ARQMStackLocation<(ARQM_STACKENTRIES-1)) {
		ptr = &__ARQMInfo[__ARQMStackLocation];

		_CPU_ISR_Disable(level);
		ptr->polled = FALSE;
		ptr->aram_start = __ARQMStackPointer[__ARQMStackLocation++];
		__ARQMStackPointer[__ARQMStackLocation] = ptr->aram_start+rlen;
		__ARQMFreeBytes -= rlen;

		ARQ_PostRequestAsync(&ptr->arqhandle,__ARQMStackLocation-1,ARQ_MRAMTOARAM,ARQ_PRIO_HI,ptr->aram_start,(u32)buffer,rlen,__ARQMPollCallback);
		_CPU_ISR_Restore(level);

		while(ptr->polled==FALSE);
		return (ptr->aram_start);
	}
	return 0;
}

void ARQM_Pop()
{
	u32 level;

	_CPU_ISR_Disable(level);

	if(__ARQMStackLocation>1) {
		__ARQMFreeBytes += (__ARQMStackPointer[__ARQMStackLocation]-__ARQMStackPointer[__ARQMStackLocation-1]);
		__ARQMStackLocation--;
	}
	_CPU_ISR_Restore(level);
}

u32 ARQM_GetZeroBuffer()
{
	return __ARQMStackPointer[0];
}

u32 ARQM_GetStackPointer()
{
	u32 level,tmp;

	_CPU_ISR_Disable(level)
	tmp = __ARQMStackPointer[__ARQMStackLocation];
	_CPU_ISR_Restore(level);

	return tmp;
}

u32 ARQM_GetFreeSize()
{
	u32 level,tmp;

	_CPU_ISR_Disable(level)
	tmp = __ARQMFreeBytes;
	_CPU_ISR_Restore(level);

	return tmp;
}

/*-------------------------------------------------------------

arqueue.c -- ARAM task request queue implementation

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
#include <stdio.h>
#include <lwp.h>

#include "asm.h"
#include "processor.h"
#include "arqueue.h"

static u32 __ARQChunkSize;
static u32 __ARQInitFlag = 0;
static lwpq_t __ARQSyncQueue;

static lwp_queue __ARQReqQueueLo;
static lwp_queue __ARQReqQueueHi;
static ARQRequest *__ARQReqPendingLo;
static ARQRequest *__ARQReqPendingHi;
static ARQCallback __ARQCallbackLo = NULL;
static ARQCallback __ARQCallbackHi = NULL;

static __inline__ void __ARQPopTaskQueueHi()
{
	ARQRequest *req;

	req = (ARQRequest*)__lwp_queue_getI(&__ARQReqQueueHi);
	if(!req) return;

	req->state = ARQ_TASK_RUNNING;
	AR_StartDMA(req->dir,req->mram_addr,req->aram_addr,req->len);
	__ARQCallbackHi = req->callback;
	__ARQReqPendingHi = req;
}

static void __ARQCallbackDummy(ARQRequest *req)
{
}

static void __ARQCallbackSync(ARQRequest *req)
{
	LWP_ThreadBroadcast(__ARQSyncQueue);
}

static void __ARQServiceQueueLo()
{
	ARQRequest *req;

	if(!__ARQReqPendingLo) {
		req = (ARQRequest*)__lwp_queue_getI(&__ARQReqQueueLo);
		__ARQReqPendingLo = req;
	}

	req = __ARQReqPendingLo;
	if(req) {
		req->state = ARQ_TASK_RUNNING;
		if(req->len<=__ARQChunkSize) {
			AR_StartDMA(req->dir,req->mram_addr,req->aram_addr,req->len);
			__ARQCallbackLo = __ARQReqPendingLo->callback;
		} else {
			AR_StartDMA(req->dir,req->mram_addr,req->aram_addr,__ARQChunkSize);
			__ARQReqPendingLo->len -= __ARQChunkSize;
			__ARQReqPendingLo->aram_addr += __ARQChunkSize;
			__ARQReqPendingLo->mram_addr += __ARQChunkSize;
		}
	}
}

static void __ARInterruptServiceRoutine()
{
	if(__ARQCallbackHi) {
		__ARQReqPendingHi->state = ARQ_TASK_FINISHED;
		__ARQCallbackHi(__ARQReqPendingHi);
		__ARQReqPendingHi = NULL;
		__ARQCallbackHi = NULL;
	} else if(__ARQCallbackLo) {
		__ARQReqPendingLo->state = ARQ_TASK_FINISHED;
		__ARQCallbackLo(__ARQReqPendingLo);
		__ARQReqPendingLo = NULL;
		__ARQCallbackLo = NULL;
	}
	__ARQPopTaskQueueHi();
	if(!__ARQReqPendingHi) __ARQServiceQueueLo();
}

void ARQ_Init()
{
	u32 level;
	if(__ARQInitFlag) return;

	_CPU_ISR_Disable(level);

	__ARQReqPendingLo = NULL;
	__ARQReqPendingHi = NULL;
	__ARQCallbackLo = NULL;
	__ARQCallbackHi = NULL;

	__ARQChunkSize = ARQ_DEF_CHUNK_SIZE;

	LWP_InitQueue(&__ARQSyncQueue);

	__lwp_queue_init_empty(&__ARQReqQueueLo);
	__lwp_queue_init_empty(&__ARQReqQueueHi);

	AR_RegisterCallback(__ARInterruptServiceRoutine);

	__ARQInitFlag = 1;
	_CPU_ISR_Restore(level);
}

void ARQ_Reset()
{
	u32 level;
	_CPU_ISR_Disable(level);
	__ARQInitFlag = 0;
	_CPU_ISR_Restore(level);
}

void ARQ_SetChunkSize(u32 size)
{
	u32 level;
	_CPU_ISR_Disable(level);
	__ARQChunkSize = (size+31)&~31;
	_CPU_ISR_Restore(level);
}

u32 ARQ_GetChunkSize()
{
	return __ARQChunkSize;
}

void ARQ_FlushQueue()
{
	u32 level;

	_CPU_ISR_Disable(level);

	__lwp_queue_init_empty(&__ARQReqQueueLo);
	__lwp_queue_init_empty(&__ARQReqQueueHi);
	if(!__ARQCallbackLo) __ARQReqPendingLo = NULL;

	_CPU_ISR_Restore(level);
}

void ARQ_PostRequestAsync(ARQRequest *req,u32 owner,u32 dir,u32 prio,u32 aram_addr,u32 mram_addr,u32 len,ARQCallback cb)
{
	u32 level;
	ARQRequest *p;

	req->state = ARQ_TASK_READY;
	req->dir = dir;
	req->owner = owner;
	req->aram_addr = aram_addr;
	req->mram_addr = mram_addr;
	req->len = len;
	req->prio = prio;
	req->callback = (cb==NULL) ? __ARQCallbackDummy : cb;

	_CPU_ISR_Disable(level);

	if(prio==ARQ_PRIO_LO) __lwp_queue_appendI(&__ARQReqQueueLo,&req->node);
	else __lwp_queue_appendI(&__ARQReqQueueHi,&req->node);

	if(!__ARQReqPendingLo && !__ARQReqPendingHi) {
		p = (ARQRequest*)__lwp_queue_getI(&__ARQReqQueueHi);
		if(p) {
			p->state = ARQ_TASK_RUNNING;
			AR_StartDMA(p->dir,p->mram_addr,p->aram_addr,p->len);
			__ARQCallbackHi = p->callback;
			__ARQReqPendingHi = p;
		}
		if(!__ARQReqPendingHi) __ARQServiceQueueLo();
	}
	_CPU_ISR_Restore(level);
}

void ARQ_PostRequest(ARQRequest *req,u32 owner,u32 dir,u32 prio,u32 aram_addr,u32 mram_addr,u32 len)
{
	u32 level;

	ARQ_PostRequestAsync(req,owner,dir,prio,aram_addr,mram_addr,len,__ARQCallbackSync);

	_CPU_ISR_Disable(level);
	while(req->state!=ARQ_TASK_FINISHED) {
		LWP_ThreadSleep(__ARQSyncQueue);
	}
	_CPU_ISR_Restore(level);
}

void ARQ_RemoveRequest(ARQRequest *req)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_queue_extractI(&req->node);
	if(__ARQReqPendingLo && __ARQReqPendingLo==req && __ARQCallbackLo==NULL) __ARQReqPendingLo = NULL;
	_CPU_ISR_Restore(level);
}

u32 ARQ_RemoveOwnerRequest(u32 owner)
{
	u32 level,cnt;
	ARQRequest *req;

	_CPU_ISR_Disable(level);

	cnt = 0;
	req = (ARQRequest*)__ARQReqQueueHi.first;
	while(req!=(ARQRequest*)__lwp_queue_tail(&__ARQReqQueueHi)) {
		if(req->owner==owner) {
			__lwp_queue_extractI(&req->node);
			cnt++;
		}
		req = (ARQRequest*)req->node.next;
	}

	req = (ARQRequest*)__ARQReqQueueLo.first;
	while(req!=(ARQRequest*)__lwp_queue_tail(&__ARQReqQueueLo)) {
		if(req->owner==owner) {
			__lwp_queue_extractI(&req->node);
			cnt++;
		}
		req = (ARQRequest*)req->node.next;
	}
	if(__ARQReqPendingLo && __ARQReqPendingLo==req && __ARQCallbackLo==NULL) __ARQReqPendingLo = NULL;
	_CPU_ISR_Restore(level);

	return cnt;
}

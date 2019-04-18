/*-------------------------------------------------------------

dsp.c -- DSP subsystem

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
#include "asm.h"
#include "processor.h"
#include "irq.h"
#include "dsp.h"

// DSPCR bits
#define DSPCR_DSPRESET      0x0800        // Reset DSP
#define DSPCR_DSPDMA        0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINTMSK     0x0100        // * interrupt mask   (RW)
#define DSPCR_DSPINT        0x0080        // * interrupt active (RWC)
#define DSPCR_ARINTMSK      0x0040
#define DSPCR_ARINT         0x0020
#define DSPCR_AIINTMSK      0x0010
#define DSPCR_AIINT         0x0008
#define DSPCR_HALT          0x0004        // halt DSP
#define DSPCR_PIINT         0x0002        // assert DSP PI interrupt
#define DSPCR_RES           0x0001        // reset DSP

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static u32 __dsp_inited = FALSE;
static u32 __dsp_rudetask_pend = FALSE;
static DSPCallback __dsp_intcb = NULL;
static dsptask_t *__dsp_currtask,*__dsp_lasttask,*__dsp_firsttask,*__dsp_rudetask,*tmp_task;

static vu16* const _dspReg = (u16*)0xCC005000;

static void __dsp_inserttask(dsptask_t *task)
{
	dsptask_t *t;

	if(!__dsp_firsttask) {
		__dsp_currtask = task;
		__dsp_lasttask = task;
		__dsp_firsttask = task;
		task->next = NULL;
		task->prev = NULL;
		return;
	}

	t = __dsp_firsttask;
	while(t) {
		if(task->prio<t->prio) {
			task->prev = t->prev;
			t->prev = task;
			task->next = t;
			if(!task->prev) {
				__dsp_firsttask = task;
				break;
			} else {
				task->prev->next = task;
				break;
			}
		}
		t = t->next;
	}
	if(t) return;

	__dsp_lasttask->next = task;
	task->next = NULL;
	task->prev = __dsp_lasttask;
	__dsp_lasttask = task;
}

static void __dsp_removetask(dsptask_t *task)
{
	task->flags = DSPTASK_CLEARALL;
	task->state = DSPTASK_DONE;
	if(__dsp_firsttask==task) {
		if(task->next) {
			__dsp_firsttask = task->next;
			__dsp_firsttask->prev = NULL;
			return;
		}
		__dsp_currtask = NULL;
		__dsp_lasttask = NULL;
		__dsp_firsttask = NULL;
		return;
	}
	if(__dsp_lasttask==task) {
		__dsp_lasttask = task->prev;
		__dsp_lasttask->next = NULL;
		__dsp_currtask = __dsp_firsttask;
		return;
	}
	__dsp_currtask = __dsp_currtask->next;
}

static void __dsp_boottask(dsptask_t *task)
{
	u32 mail;
	while(!DSP_CheckMailFrom());
	mail = DSP_ReadMailFrom();
	DSP_SendMailTo(0x80F3A001);
	while(DSP_CheckMailTo());
	DSP_SendMailTo((u32)task->iram_maddr);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3C002);
	while(DSP_CheckMailTo());
	DSP_SendMailTo((task->iram_addr&0xffff));
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3A002);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(task->iram_len);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3B002);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(0x80F3D001);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(task->init_vec);
	while(DSP_CheckMailTo());
}

static void __dsp_exectask(dsptask_t *exec,dsptask_t *hire)
{
	if(!exec) {
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
	} else {
		DSP_SendMailTo((u32)exec->dram_maddr);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(exec->dram_len);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(exec->dram_addr);
		while(DSP_CheckMailTo());
	}

	DSP_SendMailTo((u32)hire->iram_maddr);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(hire->iram_len);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(hire->iram_addr);
	while(DSP_CheckMailTo());
	if(hire->state==DSPTASK_INIT) {
		DSP_SendMailTo(hire->init_vec);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		DSP_SendMailTo(0);
		while(DSP_CheckMailTo());
		return;
	}

	DSP_SendMailTo(hire->resume_vec);
	while(DSP_CheckMailTo());

	DSP_SendMailTo((u32)hire->dram_maddr);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(hire->dram_len);
	while(DSP_CheckMailTo());
	DSP_SendMailTo(hire->dram_addr);
	while(DSP_CheckMailTo());
}

static void __dsp_def_taskcb()
{
	u32 mail;
	while(!DSP_CheckMailFrom());

	mail = DSP_ReadMailFrom();
	if(__dsp_currtask->flags&DSPTASK_CANCEL) {
		if(mail==0xDCD10002) mail = 0xDCD10003;
	}

	switch(mail) {
		case 0xDCD10000:
			__dsp_currtask->state = DSPTASK_RUN;
			if(__dsp_currtask->init_cb) __dsp_currtask->init_cb(__dsp_currtask);
			break;
		case 0xDCD10001:
			__dsp_currtask->state = DSPTASK_RUN;
			if(__dsp_currtask->res_cb) __dsp_currtask->res_cb(__dsp_currtask);
			break;
		case 0xDCD10002:
			if(__dsp_rudetask_pend==TRUE) {
				if(__dsp_rudetask==__dsp_currtask) {
					DSP_SendMailTo(0xCDD10003);
					while(DSP_CheckMailTo());

					__dsp_rudetask = NULL;
					__dsp_rudetask_pend = FALSE;
					if(__dsp_currtask->res_cb) __dsp_currtask->res_cb(__dsp_currtask);
				} else {
					DSP_SendMailTo(0xCDD10001);
					while(DSP_CheckMailTo());

					__dsp_exectask(__dsp_currtask,__dsp_rudetask);
					__dsp_currtask->flags = DSPTASK_YIELD;
					__dsp_currtask = __dsp_rudetask;
					__dsp_rudetask = NULL;
					__dsp_rudetask_pend = FALSE;
				}
			} else if(__dsp_currtask->next==NULL) {
				if(__dsp_firsttask==__dsp_currtask) {
					DSP_SendMailTo(0xCDD10003);
					while(DSP_CheckMailTo());

					if(__dsp_currtask->res_cb) __dsp_currtask->res_cb(__dsp_currtask);
				} else {
					DSP_SendMailTo(0xCDD10001);
					while(DSP_CheckMailTo());

					__dsp_exectask(__dsp_currtask,__dsp_firsttask);
					__dsp_currtask->state = DSPTASK_YIELD;
					__dsp_currtask = __dsp_firsttask;
				}
			} else {
				DSP_SendMailTo(0xCDD10001);
				while(DSP_CheckMailTo());

				__dsp_exectask(__dsp_currtask,__dsp_currtask->next);
				__dsp_currtask->state = DSPTASK_YIELD;
				__dsp_currtask = __dsp_currtask->next;
			}
			break;
		case 0xDCD10003:
			if(__dsp_rudetask_pend==TRUE) {
				if(__dsp_currtask->done_cb) __dsp_currtask->done_cb(__dsp_currtask);
				DSP_SendMailTo(0xCDD10001);
				while(DSP_CheckMailTo());

				__dsp_exectask(NULL,__dsp_rudetask);
				__dsp_removetask(__dsp_currtask);

				__dsp_currtask = __dsp_rudetask;
				__dsp_rudetask_pend = FALSE;
				__dsp_rudetask = NULL;
			} else if(__dsp_currtask->next==NULL) {
				if(__dsp_firsttask==__dsp_currtask) {
					if(__dsp_currtask->done_cb) __dsp_currtask->done_cb(__dsp_currtask);
					DSP_SendMailTo(0xCDD10002);
					while(DSP_CheckMailTo());

					__dsp_currtask->state = DSPTASK_DONE;
					__dsp_removetask(__dsp_currtask);
				}
			} else {
				if(__dsp_currtask->done_cb) __dsp_currtask->done_cb(__dsp_currtask);

				DSP_SendMailTo(0xCDD10001);
				while(DSP_CheckMailTo());

				__dsp_currtask->state = DSPTASK_DONE;
				__dsp_exectask(NULL,__dsp_firsttask);
				__dsp_currtask = __dsp_firsttask;
				__dsp_removetask(__dsp_lasttask);
			}
			break;
		case 0xDCD10004:
			if(__dsp_currtask->req_cb) __dsp_currtask->req_cb(__dsp_currtask);
			break;
	}

}

static void __dsp_inthandler(u32 nIrq,void *pCtx)
{
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT))|DSPCR_DSPINT;
	if(__dsp_intcb) __dsp_intcb();
}

void DSP_Init()
{
	u32 level;
	_CPU_ISR_Disable(level);
	if(__dsp_inited==FALSE) {
		__dsp_intcb= __dsp_def_taskcb;

		IRQ_Request(IRQ_DSP_DSP,__dsp_inthandler,NULL);
		__UnmaskIrq(IRQMASK(IRQ_DSP_DSP));

		_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_DSPRESET;
		_dspReg[5] = (_dspReg[5]&~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));

		__dsp_currtask = NULL;
		__dsp_firsttask = NULL;
		__dsp_lasttask = NULL;
		tmp_task = NULL;
		__dsp_inited = TRUE;
	}
	_CPU_ISR_Restore(level);
}

DSPCallback DSP_RegisterCallback(DSPCallback usr_cb)
{
	u32 level;
	DSPCallback ret;
	_CPU_ISR_Disable(level);
	ret = __dsp_intcb;
	if(usr_cb)
		__dsp_intcb = usr_cb;
	else
		__dsp_intcb = __dsp_def_taskcb;
	_CPU_ISR_Restore(level);

	return ret;
}

u32 DSP_CheckMailTo()
{
	return _SHIFTR(_dspReg[0],15,1);
}

u32 DSP_CheckMailFrom()
{
	return _SHIFTR(_dspReg[2],15,1);
}

u32 DSP_ReadMailFrom()
{
	return (_SHIFTL(_dspReg[2],16,16)|(_dspReg[3]&0xffff));
}

void DSP_SendMailTo(u32 mail)
{
	_dspReg[0] = _SHIFTR(mail,16,16);
	_dspReg[1] = (mail&0xffff);
}

u32 DSP_ReadCPUtoDSP()
{
	u32 cpu_dsp;
	cpu_dsp = (_SHIFTL(_dspReg[0],16,16)|(_dspReg[1]&0xffff));
	return cpu_dsp;
}

void DSP_AssertInt()
{
	u32 level;
	_CPU_ISR_Disable(level);
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_PIINT;
	_CPU_ISR_Restore(level);
}

void DSP_Reset()
{
	u16 old;
	u32 level;

	_CPU_ISR_Disable(level);
	old = _dspReg[5];
	_dspReg[5] = (old&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|(DSPCR_DSPRESET|DSPCR_RES);
	_CPU_ISR_Restore(level);
}

void DSP_Halt()
{
	u32 level,old;

	_CPU_ISR_Disable(level);
	old = _dspReg[5];
	_dspReg[5] = (old&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT))|DSPCR_HALT;
	_CPU_ISR_Restore(level);
}

void DSP_Unhalt()
{
	u32 level;

	_CPU_ISR_Disable(level);
	_dspReg[5] = (_dspReg[5]&~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT|DSPCR_HALT));
	_CPU_ISR_Restore(level);
}

u32 DSP_GetDMAStatus()
{
	return _dspReg[5]&DSPCR_DSPDMA;
}

dsptask_t* DSP_AddTask(dsptask_t *task)
{
	u32 level;
	_CPU_ISR_Disable(level);
	__dsp_inserttask(task);
	task->state = DSPTASK_INIT;
	task->flags = DSPTASK_ATTACH;
	_CPU_ISR_Restore(level);

	if(__dsp_firsttask==task) __dsp_boottask(task);
	return task;
}

void DSP_CancelTask(dsptask_t *task)
{
	u32 level;

	_CPU_ISR_Disable(level);
	task->flags |= DSPTASK_CANCEL;
	_CPU_ISR_Restore(level);
}

dsptask_t* DSP_AssertTask(dsptask_t *task)
{
	u32 level;
	dsptask_t *ret = NULL;

	_CPU_ISR_Disable(level);
	if(task==__dsp_currtask) {
		__dsp_rudetask = task;
		__dsp_rudetask_pend = TRUE;
		ret = task;
	} else {
		if(task->prio<__dsp_currtask->prio) {
			__dsp_rudetask = task;
			__dsp_rudetask_pend = TRUE;
			if(__dsp_currtask->state==DSPTASK_RUN)
				_dspReg[5] = ((_dspReg[5]&~(DSPCR_DSPINT|DSPCR_ARINT|DSPCR_AIINT))|DSPCR_PIINT);

			ret = task;
		}
	}
	_CPU_ISR_Restore(level);

	return ret;
}

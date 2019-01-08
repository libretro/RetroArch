/*-------------------------------------------------------------

aram.c -- ARAM subsystem

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
#include <string.h>
#include <stdio.h>
#include "asm.h"
#include "processor.h"
#include "aram.h"
#include "irq.h"
#include "cache.h"

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

#define AR_ARAMEXPANSION	2

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

static vu16* const _dspReg = (u16*)0xCC005000;

static ARCallback __ARDmaCallback = NULL;
static u32 __ARInit_Flag = 0;
static u32 __ARStackPointer = 0;
static u32 __ARFreeBlocks = 0;
static u32 *__ARBlockLen = NULL;

static u32 __ARInternalSize = 0;
static u32 __ARExpansionSize = 0;
static u32 __ARSize = 0;

static void __ARHandler(u32 irq,void *ctx);
static void __ARCheckSize(void);
static void __ARClearArea(u32 aramaddr,u32 len);

ARCallback AR_RegisterCallback(ARCallback callback)
{
	u32 level;
	ARCallback old;

	_CPU_ISR_Disable(level);
	old = __ARDmaCallback;
	__ARDmaCallback = callback;
	_CPU_ISR_Restore(level);
	return old;
}

u32 AR_GetDMAStatus()
{
	u32 level,ret;
	_CPU_ISR_Disable(level);
	ret = ((_dspReg[5]&DSPCR_DSPDMA)==DSPCR_DSPDMA);
	_CPU_ISR_Restore(level);
	return ret;
}

u32 AR_Init(u32 *stack_idx_array,u32 num_entries)
{
	u32 level;
	u32 aram_base = 0x4000;

	if(__ARInit_Flag) return aram_base;

	_CPU_ISR_Disable(level);

	__ARDmaCallback = NULL;

	IRQ_Request(IRQ_DSP_ARAM,__ARHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_DSP_ARAM));

	__ARStackPointer = aram_base;
	__ARFreeBlocks = num_entries;
	__ARBlockLen = stack_idx_array;
	_dspReg[13] = (_dspReg[13]&~0xff)|(_dspReg[13]&0xff);

	__ARCheckSize();
	__ARInit_Flag = 1;

	_CPU_ISR_Restore(level);
	return __ARStackPointer;
}

void AR_StartDMA(u32 dir,u32 memaddr,u32 aramaddr,u32 len)
{
	u32 level;

	_CPU_ISR_Disable(level);

	// set main memory address
	_dspReg[16] = (_dspReg[16]&~0x03ff)|_SHIFTR(memaddr,16,16);
	_dspReg[17] = (_dspReg[17]&~0xffe0)|_SHIFTR(memaddr, 0,16);

	// set aram address
	_dspReg[18] = (_dspReg[18]&~0x03ff)|_SHIFTR(aramaddr,16,16);
	_dspReg[19] = (_dspReg[19]&~0xffe0)|_SHIFTR(aramaddr, 0,16);

	// set cntrl bits
	_dspReg[20] = (_dspReg[20]&~0x8000)|_SHIFTL(dir,15,1);
	_dspReg[20] = (_dspReg[20]&~0x03ff)|_SHIFTR(len,16,16);
	_dspReg[21] = (_dspReg[21]&~0xffe0)|_SHIFTR(len, 0,16);

	_CPU_ISR_Restore(level);
}

u32 AR_Alloc(u32 len)
{
	u32 level;
	u32 curraddr;

	_CPU_ISR_Disable(level);
	curraddr = __ARStackPointer;
	__ARStackPointer += len;
	*__ARBlockLen++ = len;
	__ARFreeBlocks--;
	_CPU_ISR_Restore(level);

	return curraddr;
}

u32 AR_Free(u32 *len)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__ARBlockLen--;
	if(len) *len = *__ARBlockLen;
	__ARStackPointer -= *__ARBlockLen;
	__ARFreeBlocks++;
	_CPU_ISR_Restore(level);

	return __ARStackPointer;
}

void AR_Clear(u32 flag)
{
	switch(flag) {
		case AR_ARAMINTALL:
			if(__ARInternalSize)
				__ARClearArea(0,__ARInternalSize);
			break;
		case AR_ARAMINTUSER:
			if(__ARInternalSize)
				__ARClearArea(0x4000,__ARInternalSize-0x4000);
			break;
		case AR_ARAMEXPANSION:
			if(__ARInternalSize && __ARExpansionSize)
				__ARClearArea(__ARInternalSize,__ARExpansionSize);
			break;
		default:
			break;
	}
}

BOOL AR_CheckInit()
{
	return __ARInit_Flag;
}

void AR_Reset()
{
	__ARInit_Flag = 0;
}

u32 AR_GetSize()
{
	return __ARSize;
}

u32 AR_GetBaseAddress()
{
	return 0x4000;
}

u32 AR_GetInternalSize()
{
	return __ARInternalSize;
}

static __inline__ void __ARClearInterrupt()
{
	u16 cause;

	cause = _dspReg[5]&~(DSPCR_DSPINT|DSPCR_AIINT);
	_dspReg[5] = (cause|DSPCR_ARINT);
}

static __inline__ void __ARWaitDma()
{
	while(_dspReg[5]&DSPCR_DSPDMA);
}

static void __ARReadDMA(u32 memaddr,u32 aramaddr,u32 len)
{
	// set main memory address
	_dspReg[16] = (_dspReg[16]&~0x03ff)|_SHIFTR(memaddr,16,16);
	_dspReg[17] = (_dspReg[17]&~0xffe0)|_SHIFTR(memaddr, 0,16);

	// set aram address
	_dspReg[18] = (_dspReg[18]&~0x03ff)|_SHIFTR(aramaddr,16,16);
	_dspReg[19] = (_dspReg[19]&~0xffe0)|_SHIFTR(aramaddr, 0,16);

	// set cntrl bits
	_dspReg[20] = (_dspReg[20]&~0x8000)|0x8000;
	_dspReg[20] = (_dspReg[20]&~0x03ff)|_SHIFTR(len,16,16);
	_dspReg[21] = (_dspReg[21]&~0xffe0)|_SHIFTR(len, 0,16);

	__ARWaitDma();
	__ARClearInterrupt();

}

static void __ARWriteDMA(u32 memaddr,u32 aramaddr,u32 len)
{
	// set main memory address
	_dspReg[16] = (_dspReg[16]&~0x03ff)|_SHIFTR(memaddr,16,16);
	_dspReg[17] = (_dspReg[17]&~0xffe0)|_SHIFTR(memaddr, 0,16);

	// set aram address
	_dspReg[18] = (_dspReg[18]&~0x03ff)|_SHIFTR(aramaddr,16,16);
	_dspReg[19] = (_dspReg[19]&~0xffe0)|_SHIFTR(aramaddr, 0,16);

	// set cntrl bits
	_dspReg[20] = (_dspReg[20]&~0x8000);
	_dspReg[20] = (_dspReg[20]&~0x03ff)|_SHIFTR(len,16,16);
	_dspReg[21] = (_dspReg[21]&~0xffe0)|_SHIFTR(len, 0,16);

	__ARWaitDma();
	__ARClearInterrupt();
}

static void __ARClearArea(u32 aramaddr,u32 len)
{
	u32 currlen,curraddr,endaddr;
	static u8 zero_buffer[2048] ATTRIBUTE_ALIGN(32);

	while(!(_dspReg[11]&0x0001));

	memset(zero_buffer,0,2048);
	DCFlushRange(zero_buffer,2048);

	curraddr = aramaddr;
	endaddr = aramaddr+len;

	currlen = 2048;
	while(curraddr<endaddr) {
		if((endaddr-curraddr)<currlen) currlen = ((endaddr-curraddr)+31)&~31;
		__ARWriteDMA((u32)zero_buffer,curraddr,currlen);
		curraddr += currlen;
	}
}

static void __ARCheckSize()
{
	u32 i,arsize,arszflag;
	static u32 test_data[8] ATTRIBUTE_ALIGN(32);
	static u32 dummy_data[8] ATTRIBUTE_ALIGN(32);
	static u32 buffer[8] ATTRIBUTE_ALIGN(32);

	while(!(_dspReg[11]&0x0001));

	__ARSize = __ARInternalSize = arsize  = 0x1000000;
	_dspReg[9] = (_dspReg[9]&~0x3f)|0x23;

	for(i=0;i<8;i++) {
		test_data[i] = 0xBAD1BAD0;
		dummy_data[i] = 0xDEADBEEF;
	}
	DCFlushRange(test_data,32);
	DCFlushRange(dummy_data,32);

	__ARExpansionSize = 0;
	__ARWriteDMA((u32)test_data,0x1000000,32);
	__ARWriteDMA((u32)test_data,0x1200000,32);
	__ARWriteDMA((u32)test_data,0x2000000,32);
	__ARWriteDMA((u32)test_data,0x1000200,32);
	__ARWriteDMA((u32)test_data,0x1400000,32);

	memset(buffer,0,32);
	DCFlushRange(buffer,32);

	__ARWriteDMA((u32)dummy_data,0x1000000,32);

	DCInvalidateRange(buffer,32);
	__ARReadDMA((u32)buffer,0x1000000,32);
	_nop();

	arszflag = 0x03;
	if(buffer[0]==dummy_data[0]) {
		memset(buffer,0,32);
		DCFlushRange(buffer,32);
		__ARReadDMA((u32)buffer,0x1200000,32);
		_nop();
		if(buffer[0]==dummy_data[0]) {
			__ARExpansionSize = 0x200000;
			arsize +=  0x200000;
			goto end_check;				//not nice but fast
		}

		memset(buffer,0,32);
		DCFlushRange(buffer,32);
		__ARReadDMA((u32)buffer,0x2000000,32);
		_nop();
		if(buffer[0]==dummy_data[0]) {
			__ARExpansionSize = 0x400000;
			arsize +=  0x400000;
			arszflag |= 0x08;
			goto end_check;				//not nice but fast
		}

		memset(buffer,0,32);
		DCFlushRange(buffer,32);
		__ARReadDMA((u32)buffer,0x1400000,32);
		_nop();
		if(buffer[0]==dummy_data[0]) {
			__ARExpansionSize = 0x1000000;
			arsize +=  0x1000000;
			arszflag |= 0x18;
		} else {
			__ARExpansionSize = 0x2000000;
			arsize +=  0x2000000;
			arszflag |= 0x20;
		}
end_check:
		_dspReg[9] = (_dspReg[9]&~0x3f)|arszflag;
	}

	*(u32*)0x800000d0 = arsize;
	__ARSize = arsize;
}

static void __ARHandler(u32 irq,void *ctx)
{
	__ARClearInterrupt();

	if(__ARDmaCallback)
		__ARDmaCallback();
}

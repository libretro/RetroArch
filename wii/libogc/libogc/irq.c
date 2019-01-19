/*-------------------------------------------------------------

irq.h -- Interrupt subsystem

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
#include "asm.h"
#include "cache.h"
#include "context.h"
#include "processor.h"
#include "lwp_threads.h"
#include "irq.h"
#include "console.h"

#define CPU_STACK_ALIGNMENT				8
#define CPU_MINIMUM_STACK_FRAME_SIZE	16

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

struct irq_handler_s {
	raw_irq_handler_t pHndl;
	void *pCtx;
};

static u64 spuriousIrq = 0;
static u32 prevIrqMask = 0;
static u32 currIrqMask = 0;
static struct irq_handler_s g_IRQHandler[32];

static vu32* const _piReg = (u32*)0xCC003000;
static vu16* const _memReg = (u16*)0xCC004000;
static vu16* const _dspReg = (u16*)0xCC005000;

#if defined(HW_DOL)
static vu32* const _exiReg = (u32*)0xCC006800;
static vu32* const _aiReg = (u32*)0xCC006C00;
#elif defined(HW_RVL)
static vu32* const _exiReg = (u32*)0xCD006800;
static vu32* const _aiReg = (u32*)0xCD006C00;
#endif

static u32 const _irqPrio[] = {IM_PI_ERROR,IM_PI_DEBUG,IM_MEM,IM_PI_RSW,
							   IM_PI_VI,(IM_PI_PETOKEN|IM_PI_PEFINISH),
							   IM_PI_HSP,
							   (IM_DSP_ARAM|IM_DSP_DSP|IM_AI|IM_EXI|IM_PI_SI|IM_PI_DI),
							   IM_DSP_AI,IM_PI_CP,
#if defined(HW_RVL)
							   IM_PI_ACR,
#endif
							   0xffffffff};

extern void __exception_load(u32,void *,u32,void *);

extern s8 irqhandler_start[],irqhandler_end[];
extern u8 __intrstack_addr[],__intrstack_end[];

void c_irqdispatcher(frame_context *ctx)
{
	u32 i,icause,intmask,irq = 0;
	u32 cause,mask;

	cause = _piReg[0]&~0x10000;
	mask = _piReg[1];

	if(!cause || !(cause&mask)) {
		spuriousIrq++;
		return;
	}

	intmask = 0;
	if(cause&0x00000080) {		//Memory Interface
		icause = _memReg[15];
		if(icause&0x00000001) {
			intmask |= IRQMASK(IRQ_MEM0);
		}
		if(icause&0x00000002) {
			intmask |= IRQMASK(IRQ_MEM1);
		}
		if(icause&0x00000004) {
			intmask |= IRQMASK(IRQ_MEM2);
		}
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_MEM3);
		}
		if(icause&0x00000010) {
			intmask |= IRQMASK(IRQ_MEMADDRESS);
		}
	}
	if(cause&0x00000040) {		//DSP
		icause = _dspReg[5];
		if(icause&0x00000008){
			intmask |= IRQMASK(IRQ_DSP_AI);
		}
		if(icause&0x00000020){
			intmask |= IRQMASK(IRQ_DSP_ARAM);
		}
		if(icause&0x00000080){
			intmask |= IRQMASK(IRQ_DSP_DSP);
		}
	}
	if(cause&0x00000020) {		//Streaming
		icause = _aiReg[0];
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_AI);
		}
	}
	if(cause&0x00000010) {		//EXI
		//EXI 0
		icause = _exiReg[0];
		if(icause&0x00000002) {
			intmask |= IRQMASK(IRQ_EXI0_EXI);
		}
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_EXI0_TC);
		}
		if(icause&0x00000800) {
			intmask |= IRQMASK(IRQ_EXI0_EXT);
		}
		//EXI 1
		icause = _exiReg[5];
		if(icause&0x00000002) {
			intmask |= IRQMASK(IRQ_EXI1_EXI);
		}
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_EXI1_TC);
		}
		if(icause&0x00000800) {
			intmask |= IRQMASK(IRQ_EXI1_EXT);
		}
		//EXI 2
		icause = _exiReg[10];
		if(icause&0x00000002) {
			intmask |= IRQMASK(IRQ_EXI2_EXI);
		}
		if(icause&0x00000008) {
			intmask |= IRQMASK(IRQ_EXI2_TC);
		}
	}
	if(cause&0x00002000) {		//High Speed Port
		intmask |= IRQMASK(IRQ_PI_HSP);
	}
	if(cause&0x00001000) {		//External Debugger
		intmask |= IRQMASK(IRQ_PI_DEBUG);
	}
	if(cause&0x00000400) {		//Frame Ready (PE_FINISH)
		intmask |= IRQMASK(IRQ_PI_PEFINISH);
	}
	if(cause&0x00000200) {		//Token Assertion (PE_TOKEN)
		intmask |= IRQMASK(IRQ_PI_PETOKEN);
	}
	if(cause&0x00000100) {		//Video Interface
		intmask |= IRQMASK(IRQ_PI_VI);
	}
	if(cause&0x00000008) {		//Serial
		intmask |= IRQMASK(IRQ_PI_SI);
	}
	if(cause&0x00000004) {		//DVD
		intmask |= IRQMASK(IRQ_PI_DI);
	}
	if(cause&0x00000002) {		//Reset Switch
		intmask |= IRQMASK(IRQ_PI_RSW);
	}
	if(cause&0x00000800) {		//Command FIFO
		intmask |= IRQMASK(IRQ_PI_CP);
	}
	if(cause&0x00000001) {		//GP Runtime Error
		intmask |= IRQMASK(IRQ_PI_ERROR);
	}
#if defined(HW_RVL)
	if(cause&0x00004000) {
		intmask |= IRQMASK(IRQ_PI_ACR);
	}
#endif
	mask = intmask&~(prevIrqMask|currIrqMask);
	if(mask) {
		i=0;
		irq = 0;
		while(i<(sizeof(_irqPrio)/sizeof(u32))) {
			if((irq=(mask&_irqPrio[i]))) {
				irq = cntlzw(irq);
				break;
			}
			i++;
		}

		if(g_IRQHandler[irq].pHndl) g_IRQHandler[irq].pHndl(irq,g_IRQHandler[irq].pCtx);
	}
}

static u32 __SetInterrupts(u32 iMask,u32 nMask)
{
	u32 imask;
	u32 irq = cntlzw(iMask);

	if(irq<=IRQ_MEMADDRESS)
   {
      imask = 0;
      if(!(nMask&IM_MEM0)) imask |= 0x0001;
      if(!(nMask&IM_MEM1)) imask |= 0x0002;
      if(!(nMask&IM_MEM2)) imask |= 0x0004;
      if(!(nMask&IM_MEM3)) imask |= 0x0008;
      if(!(nMask&IM_MEMADDRESS)) imask |= 0x0010;
      _memReg[14] = (u16)imask;
      return (iMask&~IM_MEM);
   }

	if(irq>=IRQ_DSP_AI && irq<=IRQ_DSP_DSP) {
		imask = _dspReg[5]&~0x1f8;
		if(!(nMask&IM_DSP_AI)) imask |= 0x0010;
		if(!(nMask&IM_DSP_ARAM)) imask |= 0x0040;
		if(!(nMask&IM_DSP_DSP)) imask |= 0x0100;
		_dspReg[5] = (u16)imask;
		return (iMask&~IM_DSP);
	}

	if(irq==IRQ_AI) {
		imask = _aiReg[0]&~0x2c;
		if(!(nMask&IM_AI)) imask |= 0x0004;
		_aiReg[0] = imask;
		return (iMask&~IM_AI);
	}
	if(irq>=IRQ_EXI0_EXI && irq<=IRQ_EXI0_EXT) {
		imask = _exiReg[0]&~0x2c0f;
		if(!(nMask&IM_EXI0_EXI)) imask |= 0x0001;
		if(!(nMask&IM_EXI0_TC)) imask |= 0x0004;
		if(!(nMask&IM_EXI0_EXT)) imask |= 0x0400;
		_exiReg[0] = imask;
		return (iMask&~IM_EXI0);
	}

	if(irq>=IRQ_EXI1_EXI && irq<=IRQ_EXI1_EXT) {
		imask = _exiReg[5]&~0x0c0f;
		if(!(nMask&IM_EXI1_EXI)) imask |= 0x0001;
		if(!(nMask&IM_EXI1_TC)) imask |= 0x0004;
		if(!(nMask&IM_EXI1_EXT)) imask |= 0x0400;
		_exiReg[5] = imask;
		return (iMask&~IM_EXI1);
	}

	if(irq>=IRQ_EXI2_EXI && irq<=IRQ_EXI2_TC) {
		imask = _exiReg[10]&~0x000f;
		if(!(nMask&IM_EXI2_EXI)) imask |= 0x0001;
		if(!(nMask&IM_EXI2_TC)) imask |= 0x0004;
		_exiReg[10] = imask;
		return (iMask&~IM_EXI2);
	}

#if defined(HW_DOL)
	if(irq>=IRQ_PI_CP && irq<=IRQ_PI_HSP) {
#elif defined(HW_RVL)
	if(irq>=IRQ_PI_CP && irq<=IRQ_PI_ACR) {
#endif
		imask = 0xf0;
		if(!(nMask&IM_PI_ERROR)) {
			imask |= 0x00000001;
		}
		if(!(nMask&IM_PI_RSW)) {
			imask |= 0x00000002;
		}
		if(!(nMask&IM_PI_DI)) {
			imask |= 0x00000004;
		}
		if(!(nMask&IM_PI_SI)) {
			imask |= 0x00000008;
		}
		if(!(nMask&IM_PI_VI)) {
			imask |= 0x00000100;
		}
		if(!(nMask&IM_PI_PETOKEN)) {
			imask |= 0x00000200;
		}
		if(!(nMask&IM_PI_PEFINISH)) {
			imask |= 0x00000400;
		}
		if(!(nMask&IM_PI_CP)) {
			imask |= 0x00000800;
		}
		if(!(nMask&IM_PI_DEBUG)) {
			imask |= 0x00001000;
		}
		if(!(nMask&IM_PI_HSP)) {
			imask |= 0x00002000;
		}
#if defined(HW_RVL)
		if(!(nMask&IM_PI_ACR)) {
			imask |= 0x00004000;
		}
#endif
		_piReg[1] = imask;
		return (iMask&~IM_PI);
	}
	return 0;
}

void __UnmaskIrq(u32 nMask)
{
	u32 level;
	u32 mask;

	_CPU_ISR_Disable(level);
	mask = (nMask&(prevIrqMask|currIrqMask));
	nMask = (prevIrqMask&~nMask);
	prevIrqMask = nMask;
	while((mask=__SetInterrupts(mask,(nMask|currIrqMask)))!=0);
	_CPU_ISR_Restore(level);
}

void __MaskIrq(u32 nMask)
{
	u32 level;
	u32 mask;

	_CPU_ISR_Disable(level);
	mask = (nMask&~(prevIrqMask|currIrqMask));
	nMask = (nMask|prevIrqMask);
	prevIrqMask = nMask;
	while((mask=__SetInterrupts(mask,(nMask|currIrqMask)))!=0);
	_CPU_ISR_Restore(level);
}

void __irq_init()
{
	register u32 intrStack = (u32)__intrstack_addr;
	register u32 intrStack_end = (u32)__intrstack_end;
	register u32 irqNestingLevel = 0;

	memset(g_IRQHandler,0,32*sizeof(struct irq_handler_s));

	*((u32*)intrStack_end) = 0xDEADBEEF;
	intrStack = intrStack - CPU_MINIMUM_STACK_FRAME_SIZE;
	intrStack &= ~(CPU_STACK_ALIGNMENT-1);
	*((u32*)intrStack) = 0;

	mtspr(272,irqNestingLevel);
	mtspr(273,intrStack);

	prevIrqMask = 0;
	currIrqMask = 0;
	_piReg[1] = 0xf0;

	__MaskIrq(-32);

	_piReg[0] = 0x01;
	__UnmaskIrq(IM_PI_ERROR);
}

raw_irq_handler_t IRQ_Request(u32 nIrq,raw_irq_handler_t pHndl,void *pCtx)
{
	u32 level;

	_CPU_ISR_Disable(level);
	raw_irq_handler_t old = g_IRQHandler[nIrq].pHndl;
	g_IRQHandler[nIrq].pHndl = pHndl;
	g_IRQHandler[nIrq].pCtx = pCtx;
	_CPU_ISR_Restore(level);
	return old;
}

raw_irq_handler_t IRQ_GetHandler(u32 nIrq)
{
	u32 level;
	raw_irq_handler_t ret;

	_CPU_ISR_Disable(level);
	ret = g_IRQHandler[nIrq].pHndl;
	_CPU_ISR_Restore(level);
	return ret;
}

raw_irq_handler_t IRQ_Free(u32 nIrq)
{
	u32 level;

	_CPU_ISR_Disable(level);
	raw_irq_handler_t old = g_IRQHandler[nIrq].pHndl;
	g_IRQHandler[nIrq].pHndl = NULL;
	g_IRQHandler[nIrq].pCtx = NULL;
	_CPU_ISR_Restore(level);
	return old;
}

u32 IRQ_Disable()
{
	u32 level;
	_CPU_ISR_Disable(level);
	return level;
}

void IRQ_Restore(u32 level)
{
	_CPU_ISR_Restore(level);
}

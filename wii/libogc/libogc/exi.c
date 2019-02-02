/*-------------------------------------------------------------

exi.c -- EXI subsystem

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
#include "irq.h"
#include "processor.h"
#include "spinlock.h"
#include "exi.h"

#define EXI_LOCK_DEVS				32

#define EXI_MAX_CHANNELS			3
#define EXI_MAX_DEVICES				3

#define EXI_DEVICE0					0x0080
#define EXI_DEVICE1					0x0100
#define EXI_DEVICE2					0x0200

#define EXI_EXI_IRQ					0x0002
#define EXI_TC_IRQ					0x0008
#define EXI_EXT_IRQ					0x0800
#define EXI_EXT_BIT					0x1000

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

struct _lck_dev {
	lwp_node node;
	u32 dev;
	EXICallback unlockcb;
};

typedef struct _exibus_priv {
	EXICallback CallbackEXI;
	EXICallback CallbackTC;
	EXICallback CallbackEXT;

	u32 imm_len;
	void *imm_buff;
	u32 lockeddev;
	u32 flags;
	u32 lck_cnt;
	u32 exi_id;
	u64 exi_idtime;
	lwp_queue lckd_dev;
	u32 lckd_dev_bits;
} exibus_priv;

static lwp_queue _lckdev_queue;
static struct _lck_dev lckdevs[EXI_LOCK_DEVS];
static exibus_priv eximap[EXI_MAX_CHANNELS];
static u64 last_exi_idtime[EXI_MAX_CHANNELS];

static u32 exi_id_serport1 = 0;

static u32 exi_uart_chan = EXI_CHANNEL_0;
static u32 exi_uart_dev = EXI_DEVICE_0;
static u32 exi_uart_barnacle_enabled = 0;
static u32 exi_uart_enabled = 0;

static void __exi_irq_handler(u32,void *);
static void __tc_irq_handler(u32,void *);
static void __ext_irq_handler(u32,void *);

#if defined(HW_DOL)
	static vu32* const _exiReg = (u32*)0xCC006800;
#elif defined(HW_RVL)
	static vu32* const _exiReg = (u32*)0xCD006800;
#else
	#error HW model unknown.
#endif

static __inline__ void __exi_clearirqs(s32 nChn,u32 nEXIIrq,u32 nTCIrq,u32 nEXTIrq)
{
	u32 d;
	d = (_exiReg[nChn*5]&~(EXI_EXI_IRQ|EXI_TC_IRQ|EXI_EXT_IRQ));
	if(nEXIIrq) d |= EXI_EXI_IRQ;
	if(nTCIrq) d |= EXI_TC_IRQ;
	if(nEXTIrq) d |= EXI_EXT_IRQ;
	_exiReg[nChn*5] = d;
}

static __inline__ void __exi_setinterrupts(s32 nChn,exibus_priv *exi)
{
	exibus_priv *pexi = &eximap[EXI_CHANNEL_2];
	if(nChn==EXI_CHANNEL_0) {
		__MaskIrq((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI2_EXI)));
		if(!(exi->flags&EXI_FLAG_LOCKED) && (exi->CallbackEXI || pexi->CallbackEXI))
			__UnmaskIrq((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI2_EXI)));
	} else if(nChn==EXI_CHANNEL_1) {
		__MaskIrq(IRQMASK(IRQ_EXI1_EXI));
		if(!(exi->flags&EXI_FLAG_LOCKED) && exi->CallbackEXI) __UnmaskIrq(IRQMASK(IRQ_EXI1_EXI));
	} else if(nChn==EXI_CHANNEL_2) {				//explicitly use of channel 2 only if debugger is attached.
		__MaskIrq(IRQMASK(IRQ_EXI0_EXI));
		if(!(exi->flags&EXI_FLAG_LOCKED) && IRQ_GetHandler(IRQ_PI_DEBUG)) __UnmaskIrq(IRQMASK(IRQ_EXI2_EXI));
	}
}

static void __exi_initmap(exibus_priv *exim)
{
	s32 i;
	exibus_priv *m;

	__lwp_queue_initialize(&_lckdev_queue,lckdevs,EXI_LOCK_DEVS,sizeof(struct _lck_dev));

	for(i=0;i<EXI_MAX_CHANNELS;i++) {
		m = &exim[i];
		m->CallbackEXI = NULL;
		m->CallbackEXT = NULL;
		m->CallbackTC = NULL;
		m->imm_buff = NULL;
		m->exi_id = 0;
		m->exi_idtime = 0;
		m->flags = 0;
		m->imm_len = 0;
		m->lck_cnt = 0;
		m->lockeddev = 0;
		m->lckd_dev_bits = 0;
		__lwp_queue_init_empty(&m->lckd_dev);
	}
}

static s32 __exi_probe(s32 nChn)
{
	u64 time;
	s32 ret = 1;
	u32 level;
	u32 val;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);
	val = _exiReg[nChn*5];
	if(!(exi->flags&EXI_FLAG_ATTACH)) {
		if(val&EXI_EXT_IRQ) {
			__exi_clearirqs(nChn,0,0,1);
			exi->exi_idtime = 0;
			last_exi_idtime[nChn] = 0;
		}
		if(_exiReg[nChn*5]&EXI_EXT_BIT) {
			time = gettime();
			if(last_exi_idtime[nChn]==0) last_exi_idtime[nChn] = time;
			if((val=diff_usec(last_exi_idtime[nChn],time)+10)<30) ret = 0;
			else ret = 1;
			_CPU_ISR_Restore(level);
			return ret;
		} else {
			exi->exi_idtime = 0;
			last_exi_idtime[nChn] = 0;
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	if(!(_exiReg[nChn*5]&EXI_EXT_BIT) || (_exiReg[nChn*5]&EXI_EXT_IRQ)) {
		exi->exi_idtime = 0;
		last_exi_idtime[nChn] = 0;
		ret = 0;
	}
	_CPU_ISR_Restore(level);
	return ret;
}

static s32 __exi_attach(s32 nChn,EXICallback ext_cb)
{
	s32 ret;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);
	ret = 0;
	if(!(exi->flags&EXI_FLAG_ATTACH)) {
		if(__exi_probe(nChn)==1) {
			__exi_clearirqs(nChn,1,0,0);
			exi->CallbackEXT = ext_cb;
			__UnmaskIrq(((IRQMASK(IRQ_EXI0_EXT))>>(nChn*3)));
			exi->flags |= EXI_FLAG_ATTACH;
			ret = 1;
		}
	}
	_CPU_ISR_Restore(level);
	return ret;
}

s32 EXI_Lock(s32 nChn,s32 nDev,EXICallback unlockCB)
{
	u32 level;
	struct _lck_dev *lckd;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);
	if(exi->flags&EXI_FLAG_LOCKED) {
		if(unlockCB && !(exi->lckd_dev_bits&(1<<nDev))) {
			lckd = (struct _lck_dev*)__lwp_queue_getI(&_lckdev_queue);
			if(lckd) {
				exi->lck_cnt++;
				exi->lckd_dev_bits |= (1<<nDev);
				lckd->dev = nDev;
				lckd->unlockcb = unlockCB;
				__lwp_queue_appendI(&exi->lckd_dev,&lckd->node);
			}
		}
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->lockeddev = nDev;
	exi->flags |= EXI_FLAG_LOCKED;
	__exi_setinterrupts(nChn,exi);

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_Unlock(s32 nChn)
{
	u32 level,dev;
	EXICallback cb;
	struct _lck_dev *lckd;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);
	if(!(exi->flags&EXI_FLAG_LOCKED)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->flags &= ~EXI_FLAG_LOCKED;
	__exi_setinterrupts(nChn,exi);

	if(!exi->lck_cnt) {
		_CPU_ISR_Restore(level);
		return 1;
	}

	exi->lck_cnt--;
	lckd = (struct _lck_dev*)__lwp_queue_getI(&exi->lckd_dev);
	__lwp_queue_appendI(&_lckdev_queue,&lckd->node);

	cb = lckd->unlockcb;
	dev = lckd->dev;
	exi->lckd_dev_bits &= ~(1<<dev);
	if(cb) cb(nChn,dev);

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_Select(s32 nChn,s32 nDev,s32 nFrq)
{
	u32 val;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);

	if(exi->flags&EXI_FLAG_SELECT) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	if(nChn!=EXI_CHANNEL_2) {
		if(nDev==EXI_DEVICE_0 && !(exi->flags&EXI_FLAG_ATTACH)) {
			if(__exi_probe(nChn)==0) {
				_CPU_ISR_Restore(level);
				return 0;
			}
		}
		if(!(exi->flags&EXI_FLAG_LOCKED) || exi->lockeddev!=nDev) {
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	exi->flags |= EXI_FLAG_SELECT;
	val = _exiReg[nChn*5];
	val = (val&0x405)|(0x80<<nDev)|(nFrq<<4);
	_exiReg[nChn*5] = val;

	if(exi->flags&EXI_FLAG_ATTACH) {
		if(nChn==EXI_CHANNEL_0) __MaskIrq(IRQMASK(IRQ_EXI0_EXT));
		else if(nChn==EXI_CHANNEL_1) __MaskIrq(IRQMASK(IRQ_EXI1_EXT));
	}

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_SelectSD(s32 nChn,s32 nDev,s32 nFrq)
{
	u32 val,id;
	s32 ret;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);

	if(exi->flags&EXI_FLAG_SELECT) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	if(nChn!=EXI_CHANNEL_2) {
		if(nDev==EXI_DEVICE_0 && !(exi->flags&EXI_FLAG_ATTACH)) {
			if((ret=__exi_probe(nChn))==1) {
				if(!exi->exi_idtime) ret = EXI_GetID(nChn,EXI_DEVICE_0,&id);
			}
			if(ret==0) {
				_CPU_ISR_Restore(level);
				return 0;
			}
		}
		if(!(exi->flags&EXI_FLAG_LOCKED) || exi->lockeddev!=nDev) {
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	exi->flags |= EXI_FLAG_SELECT;
	val = _exiReg[nChn*5];
	val = (val&0x405)|(nFrq<<4);
	_exiReg[nChn*5] = val;

	if(exi->flags&EXI_FLAG_ATTACH) {
		if(nChn==EXI_CHANNEL_0) __MaskIrq(IRQMASK(IRQ_EXI0_EXT));
		else if(nChn==EXI_CHANNEL_1) __MaskIrq(IRQMASK(IRQ_EXI1_EXT));
	}

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_Deselect(s32 nChn)
{
	u32 val;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);

	if(!(exi->flags&EXI_FLAG_SELECT)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->flags &= ~EXI_FLAG_SELECT;
	val = _exiReg[nChn*5];
	_exiReg[nChn*5] = (val&0x405);

	if(exi->flags&EXI_FLAG_ATTACH) {
		if(nChn==EXI_CHANNEL_0) __UnmaskIrq(IRQMASK(IRQ_EXI0_EXT));
		else if(nChn==EXI_CHANNEL_1) __UnmaskIrq(IRQMASK(IRQ_EXI1_EXT));
	}

	if(nChn!=EXI_CHANNEL_2 && val&EXI_DEVICE0) {
		if(__exi_probe(nChn)==0) {
			_CPU_ISR_Restore(level);
			return 0;
		}
	}
	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_Sync(s32 nChn)
{
	u8 *buf;
	s32 ret;
	u32 level,i,cnt,val;
	exibus_priv *exi = &eximap[nChn];
	while(_exiReg[nChn*5+3]&0x0001);

	_CPU_ISR_Disable(level);

	ret = 0;
	if(exi->flags&EXI_FLAG_SELECT && exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM)) {
		if(exi->flags&EXI_FLAG_IMM) {
			cnt = exi->imm_len;
			buf = exi->imm_buff;
			if(buf && cnt>0) {
				val = _exiReg[nChn*5+4];
				for(i=0;i<cnt;i++) ((u8*)buf)[i] = (val>>((3-i)*8))&0xFF;
			}
		}
		exi->flags &= ~(EXI_FLAG_DMA|EXI_FLAG_IMM);
		ret = 1;
	}
	_CPU_ISR_Restore(level);
	return ret;
}

s32 EXI_Imm(s32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb)
{
	u32 level;
	u32 value,i;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);

	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM) || !(exi->flags&EXI_FLAG_SELECT)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->CallbackTC = tc_cb;
	if(tc_cb) {
		__exi_clearirqs(nChn,0,1,0);
		__UnmaskIrq(IRQMASK((IRQ_EXI0_TC+(nChn*3))));
	}
	exi->flags |= EXI_FLAG_IMM;

	exi->imm_buff = pData;
	exi->imm_len = nLen;
	if(nMode!=EXI_READ) {
		for(i=0,value=0;i<nLen;i++) value |= (((u8*)pData)[i])<<((3-i)*8);
		_exiReg[nChn*5+4] = value;
	}
	if(nMode==EXI_WRITE) exi->imm_len = 0;

	_exiReg[nChn*5+3] = (((nLen-1)&0x03)<<4)|((nMode&0x03)<<2)|0x01;

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_ImmEx(s32 nChn,void *pData,u32 nLen,u32 nMode)
{
	u8 *buf = pData;
	u32 tc;
	s32 ret = 0;
	while(nLen) {
		ret = 0;
		tc = nLen;
		if(tc>4) tc = 4;

		if(!EXI_Imm(nChn,buf,tc,nMode,NULL)) break;
		if(!EXI_Sync(nChn)) break;
		nLen -= tc;
		buf += tc;

		ret = 1;
	}
	return ret;
}

s32 EXI_Dma(s32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb)
{
	u32 level;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);

	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM) || !(exi->flags&EXI_FLAG_SELECT)) {
		_CPU_ISR_Restore(level);
		return 0;
	}
	exi->CallbackTC = tc_cb;
	if(tc_cb) {
		__exi_clearirqs(nChn,0,1,0);
		__UnmaskIrq((IRQMASK((IRQ_EXI0_TC+(nChn*3)))));
	}

	exi->imm_buff = NULL;
	exi->imm_len = 0;
	exi->flags |= EXI_FLAG_DMA;

	_exiReg[nChn*5+1] = (u32)pData&0x03FFFFE0;
	_exiReg[nChn*5+2] = nLen;
	_exiReg[nChn*5+3] = ((nMode&0x03)<<2)|0x03;

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_GetState(s32 nChn)
{
	exibus_priv *exi = &eximap[nChn];
	return exi->flags;
}

static s32 __unlocked_handler(s32 nChn,s32 nDev)
{
	u32 nId;
	EXI_GetID(nChn,nDev,&nId);
	return 1;
}

s32 EXI_GetID(s32 nChn,s32 nDev,u32 *nId)
{
	u64 idtime = 0;
	s32 ret,lck;
	u32 reg,level;
	exibus_priv *exi = &eximap[nChn];

	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) {
		if(__exi_probe(nChn)==0) return 0;
		if(exi->exi_idtime==last_exi_idtime[nChn]) {
			*nId = exi->exi_id;
			return 1;
		}
		if(__exi_attach(nChn,NULL)==0) return 0;
		idtime = last_exi_idtime[nChn];
	}
	lck = 0;
	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) lck = 1;

	if(lck)  ret = EXI_Lock(nChn,nDev,__unlocked_handler);
	else ret = EXI_Lock(nChn,nDev,NULL);

	if(ret) {
		if(EXI_Select(nChn,nDev,EXI_SPEED1MHZ)==1) {
			reg = 0;
			EXI_Imm(nChn,&reg,2,EXI_WRITE,NULL);
			EXI_Sync(nChn);
			EXI_Imm(nChn,nId,4,EXI_READ,NULL);
			EXI_Sync(nChn);
			EXI_Deselect(nChn);
			EXI_Unlock(nChn);
		}
	}

	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) {
		ret = 0;
		EXI_Detach(nChn);

		_CPU_ISR_Disable(level);
		if(idtime==last_exi_idtime[nChn]) {
			exi->exi_idtime = idtime;
			exi->exi_id = *nId;
			ret = 1;
		}
		_CPU_ISR_Restore(level);
	}
	return ret;
}

s32 EXI_Attach(s32 nChn,EXICallback ext_cb)
{
	s32 ret;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
	EXI_Probe(nChn);

	_CPU_ISR_Disable(level);
	if(exi->exi_idtime) {
		ret = __exi_attach(nChn,ext_cb);
	} else
		ret = 0;
	_CPU_ISR_Restore(level);
	return ret;
}

s32 EXI_Detach(s32 nChn)
{
	u32 level;
	s32 ret = 1;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);
	if(exi->flags&EXI_FLAG_ATTACH) {
		if(exi->flags&EXI_FLAG_LOCKED && exi->lockeddev!=EXI_DEVICE_0) ret = 0;
		else {
			exi->flags &= ~EXI_FLAG_ATTACH;
			__MaskIrq(((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI0_TC)|IRQMASK(IRQ_EXI0_EXT))>>(nChn*3)));
		}
	}
	_CPU_ISR_Restore(level);
	return ret;
}

EXICallback EXI_RegisterEXICallback(s32 nChn,EXICallback exi_cb)
{
	u32 level;
	EXICallback old = NULL;
	exibus_priv *exi = &eximap[nChn];
	_CPU_ISR_Disable(level);
	old = exi->CallbackEXI;
	exi->CallbackEXI = exi_cb;
	if(nChn==EXI_CHANNEL_2) __exi_setinterrupts(EXI_CHANNEL_0,&eximap[EXI_CHANNEL_0]);
	else __exi_setinterrupts(nChn,exi);
	_CPU_ISR_Restore(level);
	return old;
}

s32 EXI_Probe(s32 nChn)
{
	s32 ret;
	u32 id;
	exibus_priv *exi = &eximap[nChn];
	if((ret=__exi_probe(nChn))==1) {
		if(exi->exi_idtime==0) {
			if(EXI_GetID(nChn,EXI_DEVICE_0,&id)==0) ret = 0;
		}
	}
	return ret;
}

s32 EXI_ProbeEx(s32 nChn)
{
	if(EXI_Probe(nChn)==1) return 1;
	if(last_exi_idtime[nChn]==0) return -1;
	return 0;
}

void EXI_ProbeReset()
{
	last_exi_idtime[0] = 0;
	last_exi_idtime[1] = 0;

	eximap[0].exi_idtime = 0;
	eximap[1].exi_idtime = 0;

	__exi_probe(0);
	__exi_probe(1);
	EXI_GetID(EXI_CHANNEL_0,EXI_DEVICE_2,&exi_id_serport1);
}

void __exi_init()
{
	__MaskIrq(IM_EXI);

	_exiReg[0] = 0;
	_exiReg[5] = 0;
	_exiReg[10] = 0;

	_exiReg[0] = 0x2000;

	__exi_initmap(eximap);

	IRQ_Request(IRQ_EXI0_EXI,__exi_irq_handler,NULL);
	IRQ_Request(IRQ_EXI0_TC,__tc_irq_handler,NULL);
	IRQ_Request(IRQ_EXI0_EXT,__ext_irq_handler,NULL);
	IRQ_Request(IRQ_EXI1_EXI,__exi_irq_handler,NULL);
	IRQ_Request(IRQ_EXI1_TC,__tc_irq_handler,NULL);
	IRQ_Request(IRQ_EXI1_EXT,__ext_irq_handler,NULL);
	IRQ_Request(IRQ_EXI2_EXI,__exi_irq_handler,NULL);
	IRQ_Request(IRQ_EXI2_TC,__tc_irq_handler,NULL);

	EXI_ProbeReset();
}

void __exi_irq_handler(u32 nIrq,void *pCtx)
{
	u32 chan,dev;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_EXI))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan*5]&0x380),8,2);

	exi = &eximap[chan];
	__exi_clearirqs(chan,1,0,0);

	if(!exi->CallbackEXI) return;
	exi->CallbackEXI(chan,dev);
}

void __tc_irq_handler(u32 nIrq,void *pCtx)
{
	u32 cnt,len,d,chan,dev;
	EXICallback tccb;
	void *buf = NULL;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_TC))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan*5]&0x380),8,2);

	exi = &eximap[chan];
	__MaskIrq(IRQMASK(nIrq));
	__exi_clearirqs(chan,0,1,0);

	tccb = exi->CallbackTC;
	if(!tccb) return;

	exi->CallbackTC = NULL;
	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM)) {
		if(exi->flags&EXI_FLAG_IMM) {
			len = exi->imm_len;
			buf = exi->imm_buff;
			if(len>0 && buf) {
				d = _exiReg[chan*5+4];
				if(d>0) {
					for(cnt=0;cnt<len;cnt++) ((u8*)buf)[cnt] = (d>>((3-cnt)*8))&0xFF;
				}
			}
		}
		exi->flags &= ~(EXI_FLAG_DMA|EXI_FLAG_IMM);
	}
	tccb(chan,dev);
}

void __ext_irq_handler(u32 nIrq,void *pCtx)
{

	u32 chan,dev;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_EXT))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan*5]&0x380),8,2);

	exi = &eximap[chan];
	__MaskIrq(IRQMASK(nIrq));
	__exi_clearirqs(chan,0,0,1);

	exi->flags &= ~EXI_FLAG_ATTACH;
	if(exi->CallbackEXT) exi->CallbackEXT(chan,dev);
}

/* EXI UART stuff */
static s32 __probebarnacle(s32 chn,u32 dev,u32 *rev)
{
	u32 ret,reg;

	if(chn!=EXI_CHANNEL_2 && dev==EXI_DEVICE_0) {
		if(EXI_Attach(chn,NULL)==0) return 0;
	}

	ret = 0;
	if(EXI_Lock(chn,dev,NULL)==1) {
		if(EXI_Select(chn,dev,EXI_SPEED1MHZ)==1) {
			reg = 0x20011300;
			if(EXI_Imm(chn,&reg,sizeof(u32),EXI_WRITE,NULL)==0) ret |= 0x0001;
			if(EXI_Sync(chn)==0) ret |= 0x0002;
			if(EXI_Imm(chn,rev,sizeof(u32),EXI_READ,NULL)==0) ret |= 0x0004;
			if(EXI_Sync(chn)==0) ret |= 0x0008;
			if(EXI_Deselect(chn)==0) ret |= 0x0010;

		}
		EXI_Unlock(chn);
	}

	if(chn!=EXI_CHANNEL_2 && dev==EXI_DEVICE_0) EXI_Detach(chn);

	if(ret) return 0;
	if((*rev+0x00010000)==0xffff) return 0;

	return 1;
}

static s32 __queuelength()
{
	u32 reg;
	u8 len = 0;

	if(EXI_Select(exi_uart_chan,exi_uart_dev,EXI_SPEED8MHZ)==0) return -1;

	reg = 0x20010000;
	EXI_Imm(exi_uart_chan,&reg,sizeof(u32),EXI_WRITE,NULL);
	EXI_Sync(exi_uart_chan);
	EXI_Imm(exi_uart_chan,&len,sizeof(u8),EXI_READ,NULL);
	EXI_Sync(exi_uart_chan);

	EXI_Deselect(exi_uart_chan);

	return (16-len);
}

void __SYS_EnableBarnacle(s32 chn,u32 dev)
{
	u32 id,rev;

	if(EXI_GetID(chn,dev,&id)==0) return;

	if(id==0x01020000 || id==0x0004 || id==0x80000010 || id==0x80000008
		|| id==0x80000004 || id==0xffff || id==0x80000020 || id==0x0020
		|| id==0x0010 || id==0x0008 || id==0x01010000 || id==0x04040404
		|| id==0x04021000 || id==0x03010000 || id==0x02020000
		|| id==0x04020300 || id==0x04020200 || id==0x04130000
		|| id==0x04120000 || id==0x04060000 || id==0x04220000) return;

	if(__probebarnacle(chn,dev,&rev)==0) return;

	exi_uart_chan = chn;
	exi_uart_dev = dev;
	exi_uart_barnacle_enabled = 0xa5ff005a;
	exi_uart_enabled = 0xa5ff005a;
}

s32 InitializeUART()
{
	if((exi_uart_enabled+0x5a010000)==0x005a) return 0;

	exi_uart_chan = EXI_CHANNEL_0;
	exi_uart_dev = EXI_DEVICE_1;

	exi_uart_enabled = 0xa5ff005a;
	return 0;
}

s32 WriteUARTN(void *buf,u32 len)
{
	u8 *ptr;
	u32 reg;
	s32 ret,qlen,cnt;

	if((exi_uart_enabled+0x5a010000)!=0x005a) return 2;
	if(EXI_Lock(exi_uart_chan,exi_uart_dev,NULL)==0) return 0;

	ptr = buf;
	while((ptr-(u8*)buf)<len) {
		if(*ptr=='\n') *ptr = '\r';
		ptr++;
	}

	ret = 0;
	ptr = buf;
	while(len) {
		if((qlen=__queuelength())<0) {
			ret = 3;
			break;
		} else if(qlen>=12 || qlen>=len) {
			if(EXI_Select(exi_uart_chan,exi_uart_dev,EXI_SPEED8MHZ)==0) {
				ret = 3;
				break;
			}

			reg = 0xa0010000;
			EXI_Imm(exi_uart_chan,&reg,sizeof(u32),EXI_WRITE,NULL);
			EXI_Sync(exi_uart_chan);

			while(qlen>0 && len>0) {
				cnt = 4;
				if(qlen>=0x0004) {
					if(len<4) cnt = len;
					if(qlen<len) break;

					EXI_Imm(exi_uart_chan,ptr,cnt,EXI_WRITE,NULL);
					EXI_Sync(exi_uart_chan);
					qlen -= cnt;
					len -= cnt;
				}
			}

		}
		EXI_Deselect(exi_uart_chan);
	}

	EXI_Unlock(exi_uart_chan);
	return ret;
}

/*-------------------------------------------------------------

system.c -- OS functions and initialization

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
#include <malloc.h>
#include <sys/iosupport.h>

#include "asm.h"
#include "irq.h"
#include "exi.h"
#if defined(HW_RVL)
#include "ipc.h"
#include "ios.h"
#include "stm.h"
#include "es.h"
#include "conf.h"
#include "wiilaunch.h"
#endif
#include "cache.h"
#include "video.h"
#include "system.h"
#include "sys_state.h"
#include "lwp_threads.h"
#include "lwp_priority.h"
#include "lwp_watchdog.h"
#include "lwp_wkspace.h"
#include "lwp_objmgr.h"
#include "lwp_config.h"
#include "libversion.h"

#define SYSMEM1_SIZE				0x01800000
#if defined(HW_RVL)
#define SYSMEM2_SIZE				0x04000000
#endif
#define KERNEL_HEAP					(1*1024*1024)

// DSPCR bits
#define DSPCR_DSPRESET			    0x0800        // Reset DSP
#define DSPCR_DSPDMA				    0x0200        // ARAM dma in progress, if set
#define DSPCR_DSPINTMSK			    0x0100        // * interrupt mask   (RW)
#define DSPCR_DSPINT			    0x0080        // * interrupt active (RWC)
#define DSPCR_ARINTMSK			    0x0040
#define DSPCR_ARINT				    0x0020
#define DSPCR_AIINTMSK			    0x0010
#define DSPCR_AIINT				    0x0008
#define DSPCR_HALT				    0x0004        // halt DSP
#define DSPCR_PIINT				    0x0002        // assert DSP PI interrupt
#define DSPCR_RES				    0x0001        // reset DSP

#define LWP_OBJTYPE_SYSWD			7

#define LWP_CHECK_SYSWD(hndl)		\
{									\
	if(((hndl)==SYS_WD_NULL) || (LWP_OBJTYPE(hndl)!=LWP_OBJTYPE_SYSWD))	\
		return NULL;				\
}

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

struct _sramcntrl {
	u8 srambuf[64];
	u32 offset;
	s32 enabled;
	s32 locked;
	s32 sync;
} sramcntrl ATTRIBUTE_ALIGN(32);

typedef struct _alarm_st
{
	lwp_obj object;
	wd_cntrl alarm;
	u64 ticks;
	u64 periodic;
	u64 start_per;
	alarmcallback alarmhandler;
	void *cb_arg;
} alarm_st;

typedef struct _yay0header {
	unsigned int id ATTRIBUTE_PACKED;
	unsigned int dec_size ATTRIBUTE_PACKED;
	unsigned int links_offset ATTRIBUTE_PACKED;
	unsigned int chunks_offset ATTRIBUTE_PACKED;
} yay0header;

static u16 sys_fontenc = 0xffff;
static u32 sys_fontcharsinsheet = 0;
static u8 *sys_fontwidthtab = NULL;
static u8 *sys_fontimage = NULL;
static sys_fontheader *sys_fontdata = NULL;

static lwp_queue sys_reset_func_queue;
static u32 system_initialized = 0;
static lwp_objinfo sys_alarm_objects;

static void *__sysarena1lo = NULL;
static void *__sysarena1hi = NULL;

#if defined(HW_RVL)
static void *__sysarena2lo = NULL;
static void *__sysarena2hi = NULL;
static void *__ipcbufferlo = NULL;
static void *__ipcbufferhi = NULL;
#endif

static void __RSWDefaultHandler();
static resetcallback __RSWCallback = NULL;
#if defined(HW_RVL)
static void __POWDefaultHandler();
static powercallback __POWCallback = NULL;

static u32 __sys_resetdown = 0;
#endif

static vu16* const _viReg = (u16*)0xCC002000;
static vu32* const _piReg = (u32*)0xCC003000;
static vu16* const _memReg = (u16*)0xCC004000;
static vu16* const _dspReg = (u16*)0xCC005000;

void __SYS_ReadROM(void *buf,u32 len,u32 offset);
void* SYS_AllocArena1MemLo(u32 size,u32 align);

static s32 __sram_sync(void);
static s32 __sram_writecallback(s32 chn,s32 dev);
static s32 __mem_onreset(s32 final);

extern void	__lwp_thread_coreinit(void);
extern void	__lwp_sysinit(void);
extern void __heap_init(void);
extern void __exception_init(void);
extern void __exception_closeall(void);
extern void __systemcall_init(void);
extern void __decrementer_init(void);
extern void __lwp_mutex_init(void);
extern void __lwp_cond_init(void);
extern void __lwp_mqbox_init(void);
extern void __lwp_sema_init(void);
extern void __exi_init(void);
extern void __si_init(void);
extern void __irq_init(void);
extern void __lwp_start_multitasking(void);
extern void __timesystem_init(void);
extern void __memlock_init(void);
extern void __libc_init(int);

extern void __libogc_malloc_lock( struct _reent *ptr );
extern void __libogc_malloc_unlock( struct _reent *ptr );

extern void __exception_console(void);
extern void __exception_printf(const char *str, ...);

extern void __realmode(void*);
extern void __configMEM1_24Mb(void);
extern void __configMEM1_48Mb(void);
extern void __configMEM2_64Mb(void);
extern void __configMEM2_128Mb(void);
extern void __reset(u32 reset_code);

extern u32 __IPC_ClntInit(void);
extern u32 __PADDisableRecalibration(s32 disable);

extern void __console_init_ex(void *conbuffer,int tgt_xstart,int tgt_ystart,int tgt_stride,int con_xres,int con_yres,int con_stride);

extern int clock_gettime(struct timespec *tp);
extern void timespec_subtract(const struct timespec *tp_start,const struct timespec *tp_end,struct timespec *result);

extern int __libogc_lock_init(int *lock,int recursive);
extern int __libogc_lock_close(int *lock);
extern int __libogc_lock_release(int *lock);
extern int __libogc_lock_acquire(int *lock);
extern void __libogc_exit(int status);
extern void * __libogc_sbrk_r(struct _reent *ptr, ptrdiff_t incr);
extern int __libogc_gettod_r(struct _reent *ptr, struct timeval *tp, struct timezone *tz);

extern u8 __gxregs[];
extern u8 __text_start[];
extern u8 __isIPL[];
extern u8 __Arena1Lo[], __Arena1Hi[];
#if defined(HW_RVL)
extern u8 __Arena2Lo[], __Arena2Hi[];
extern u8 __ipcbufferLo[], __ipcbufferHi[];
#endif

u8 *__argvArena1Lo = (u8*)0xdeadbeef;

static u32 __sys_inIPL = (u32)__isIPL;

static u32 _dsp_initcode[] =
{
	0x029F0010,0x029F0033,0x029F0034,0x029F0035,
	0x029F0036,0x029F0037,0x029F0038,0x029F0039,
	0x12061203,0x12041205,0x00808000,0x0088FFFF,
	0x00841000,0x0064001D,0x02180000,0x81001C1E,
	0x00441B1E,0x00840800,0x00640027,0x191E0000,
	0x00DEFFFC,0x02A08000,0x029C0028,0x16FC0054,
	0x16FD4348,0x002102FF,0x02FF02FF,0x02FF02FF,
	0x02FF02FF,0x00000000,0x00000000,0x00000000
};

static sys_resetinfo mem_resetinfo = {
	{},
	__mem_onreset,
	127
};

static const char *__sys_versiondate;
static const char *__sys_versionbuild;

static __inline__ alarm_st* __lwp_syswd_open(syswd_t wd)
{
	LWP_CHECK_SYSWD(wd);
	return (alarm_st*)__lwp_objmgr_get(&sys_alarm_objects,LWP_OBJMASKID(wd));
}

static __inline__ void __lwp_syswd_free(alarm_st *alarm)
{
	__lwp_objmgr_close(&sys_alarm_objects,&alarm->object);
	__lwp_objmgr_free(&sys_alarm_objects,&alarm->object);
}

#ifdef HW_DOL
#define SOFTRESET_ADR *((vu32*)0xCC003024)
void __reload() { SOFTRESET_ADR=0; }

void __libogc_exit(int status)
{
	SYS_ResetSystem(SYS_SHUTDOWN,0,0);
	__lwp_thread_stopmultitasking(__reload);
}
#else
static void (*reload)() = (void(*)())0x80001800;

static bool __stub_found()
{
	u64 sig = ((u64)(*(u32*)0x80001804) << 32) + *(u32*)0x80001808;
	if (sig == 0x5354554248415858ULL) // 'STUBHAXX'
		return true;
	return false;
}

void __reload()
{
	if(__stub_found()) {
		__exception_closeall();
		reload();
	}
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

void __libogc_exit(int status)
{
	if(__stub_found()) {
		SYS_ResetSystem(SYS_SHUTDOWN,0,0);
		__lwp_thread_stopmultitasking(reload);
	}
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

#endif

static void __init_syscall_array() {
	__syscalls.sbrk_r = __libogc_sbrk_r;
	__syscalls.lock_init = __libogc_lock_init;
	__syscalls.lock_close = __libogc_lock_close;
	__syscalls.lock_release = __libogc_lock_release;
	__syscalls.lock_acquire = __libogc_lock_acquire;
	__syscalls.malloc_lock = __libogc_malloc_lock;
	__syscalls.malloc_unlock = __libogc_malloc_unlock;
	__syscalls.exit = __libogc_exit;
	__syscalls.gettod_r = __libogc_gettod_r;

}

static alarm_st* __lwp_syswd_allocate()
{
	alarm_st *alarm;

	__lwp_thread_dispatchdisable();
	alarm = (alarm_st*)__lwp_objmgr_allocate(&sys_alarm_objects);
	if(alarm) {
		__lwp_objmgr_open(&sys_alarm_objects,&alarm->object);
		return alarm;
	}
	__lwp_thread_dispatchenable();
	return NULL;
}

static s32 __mem_onreset(s32 final)
{
	if(final==TRUE) {
		_memReg[8] = 255;
		__UnmaskIrq(IM_MEM0|IM_MEM1|IM_MEM2|IM_MEM3);
	}
	return 1;
}

static void __sys_alarmhandler(void *arg)
{
	alarm_st *alarm;
	syswd_t thealarm = (syswd_t)arg;

	if(thealarm==SYS_WD_NULL || LWP_OBJTYPE(thealarm)!=LWP_OBJTYPE_SYSWD) return;

	__lwp_thread_dispatchdisable();
	alarm = (alarm_st*)__lwp_objmgr_getnoprotection(&sys_alarm_objects,LWP_OBJMASKID(thealarm));
	if(alarm) {
		if(alarm->alarmhandler) alarm->alarmhandler(thealarm,alarm->cb_arg);
		if(alarm->periodic) __lwp_wd_insert_ticks(&alarm->alarm,alarm->periodic);
	}
	__lwp_thread_dispatchunnest();
}

#if defined(HW_DOL)
static void __dohotreset(u32 resetcode)
{
	u32 level;

	_CPU_ISR_Disable(level);
	_viReg[1] = 0;
	ICFlashInvalidate();
	__reset(resetcode<<3);
}
#endif

static s32 __call_resetfuncs(s32 final)
{
	s32 ret;
	sys_resetinfo *info;
	lwp_queue *header = &sys_reset_func_queue;

	ret = 1;
	info = (sys_resetinfo*)header->first;
	while(info!=(sys_resetinfo*)__lwp_queue_tail(header)) {
		if(info->func && info->func(final)==0) ret |= (ret<<1);
		info = (sys_resetinfo*)info->node.next;
	}
	if(__sram_sync()==0) ret |= (ret<<1);

	if(ret&~0x01) return 0;
	return 1;
}

#if defined(HW_DOL)
static void __doreboot(u32 resetcode,s32 force_menu)
{
	u32 level;

	_CPU_ISR_Disable(level);

	*((u32*)0x817ffffc) = 0;
	*((u32*)0x817ffff8) = 0;
	*((u32*)0x800030e2) = 1;
}
#endif

static void __MEMInterruptHandler()
{
	_memReg[16] = 0;
}

static void __RSWDefaultHandler()
{

}

#if defined(HW_RVL)
static void __POWDefaultHandler()
{
}
#endif

#if defined(HW_DOL)
static void __RSWHandler()
{
	s64 now;
	static s64 hold_down = 0;

	hold_down = gettime();
	do  {
		now = gettime();
		if(diff_usec(hold_down,now)>=100) break;
	} while(!(_piReg[0]&0x10000));

	if(_piReg[0]&0x10000) {
		__MaskIrq(IRQMASK(IRQ_PI_RSW));

		if(__RSWCallback) {
			__RSWCallback();
		}
	}
	_piReg[0] = 2;
}
#endif

#if defined(HW_RVL)
static void __STMEventHandler(u32 event)
{
	s32 ret;
	u32 level;

	if(event==STM_EVENT_RESET) {
		ret = SYS_ResetButtonDown();
		if(ret) {
			_CPU_ISR_Disable(level);
			__sys_resetdown = 1;
			__RSWCallback();
			_CPU_ISR_Restore(level);
		}
	}

	if(event==STM_EVENT_POWER) {
		_CPU_ISR_Disable(level);
		__POWCallback();
		_CPU_ISR_Restore(level);
	}
}
#endif

void * __attribute__ ((weak)) __myArena1Lo = 0;
void * __attribute__ ((weak)) __myArena1Hi = 0;

static void __lowmem_init()
{
	u32 *_gx = (u32*)__gxregs;

#if defined(HW_DOL)
	void *ram_start = (void*)0x80000000;
	void *ram_end = (void*)(0x80000000|SYSMEM1_SIZE);
	void *arena_start = (void*)0x80003000;
#elif defined(HW_RVL)
	void *arena_start = (void*)0x80003F00;
#endif

	memset(_gx,0,2048);
	memset(arena_start,0,0x100);
	if ( __argvArena1Lo == (u8*)0xdeadbeef ) __argvArena1Lo = __Arena1Lo;
	if (__myArena1Lo == 0) __myArena1Lo = __argvArena1Lo;
	if (__myArena1Hi == 0) __myArena1Hi = __Arena1Hi;

#if defined(HW_DOL)
	memset(ram_start,0,0x100);
	*((u32*)(ram_start+0x20))	= 0x0d15ea5e;   // magic word "disease"
	*((u32*)(ram_start+0x24))	= 1;            // version
	*((u32*)(ram_start+0x28))	= SYSMEM1_SIZE;	// physical memory size
	*((u32*)(ram_start+0x2C))	= 1 + ((*(u32*)0xCC00302c)>>28);

	*((u32*)(ram_start+0x30))	= (u32)__myArena1Lo;
	*((u32*)(ram_start+0x34))	= (u32)__myArena1Hi;

	*((u32*)(ram_start+0xEC))	= (u32)ram_end;	// ram_end (??)
	*((u32*)(ram_start+0xF0))	= SYSMEM1_SIZE;	// simulated memory size
	*((u32*)(ram_start+0xF8))	= TB_BUS_CLOCK;	// bus speed: 162 MHz
	*((u32*)(ram_start+0xFC))	= TB_CORE_CLOCK;	// cpu speed: 486 Mhz

	*((u16*)(arena_start+0xE0))	= 6; // production pads
	*((u32*)(arena_start+0xE4))	= 0xC0008000;

	DCFlushRangeNoSync(ram_start, 0x100);
#endif

	DCFlushRangeNoSync(arena_start, 0x100);
	DCFlushRangeNoSync(_gx, 2048);
	_sync();

	SYS_SetArenaLo((void*)__myArena1Lo);
	SYS_SetArenaHi((void*)__myArena1Hi);
#if defined(HW_RVL)
	SYS_SetArena2Lo((void*)__Arena2Lo);
	SYS_SetArena2Hi((void*)__Arena2Hi);
#endif
}

#if defined(HW_RVL)
static void __ipcbuffer_init()
{
	__ipcbufferlo = (void*)__ipcbufferLo;
	__ipcbufferhi = (void*)__ipcbufferHi;
}
#endif

static void __memprotect_init()
{
	u32 level;

	_CPU_ISR_Disable(level);

	__MaskIrq((IM_MEM0|IM_MEM1|IM_MEM2|IM_MEM3));

	_memReg[16] = 0;
	_memReg[8] = 255;

	IRQ_Request(IRQ_MEM0,__MEMInterruptHandler,NULL);
	IRQ_Request(IRQ_MEM1,__MEMInterruptHandler,NULL);
	IRQ_Request(IRQ_MEM2,__MEMInterruptHandler,NULL);
	IRQ_Request(IRQ_MEM3,__MEMInterruptHandler,NULL);
	IRQ_Request(IRQ_MEMADDRESS,__MEMInterruptHandler,NULL);

	SYS_RegisterResetFunc(&mem_resetinfo);
	__UnmaskIrq(IM_MEMADDRESS);		//only enable memaddress irq atm

	_CPU_ISR_Restore(level);
}

static __inline__ u32 __get_fontsize(void *buffer)
{
	u8 *ptr = (u8*)buffer;

	if(ptr[0]=='Y' && ptr[1]=='a' && ptr[2]=='y') return (((u32*)ptr)[1]);
	else return 0;
}

static u32 __read_rom(void *buf,u32 len,u32 offset)
{
	u32 ret;
	u32 loff;

	DCInvalidateRange(buf,len);

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,NULL)==0) return 0;
	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		EXI_Unlock(EXI_CHANNEL_0);
		return 0;
	}

	ret = 0;
	loff = offset<<6;
	if(EXI_Imm(EXI_CHANNEL_0,&loff,4,EXI_WRITE,NULL)==0) ret |= 0x0001;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x0002;
	if(EXI_Dma(EXI_CHANNEL_0,buf,len,EXI_READ,NULL)==0) ret |= 0x0004;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x0008;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x0010;
	if(EXI_Unlock(EXI_CHANNEL_0)==0) ret |= 0x00020;

	if(ret) return 0;
	return 1;
}

static u32 __getrtc(u32 *gctime)
{
	u32 ret;
	u32 cmd;
	u32 time;

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,NULL)==0) return 0;
	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		EXI_Unlock(EXI_CHANNEL_0);
		return 0;
	}

	ret = 0;
	time = 0;
	cmd = 0x20000000;
	if(EXI_Imm(EXI_CHANNEL_0,&cmd,4,EXI_WRITE,NULL)==0) ret |= 0x01;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
	if(EXI_Imm(EXI_CHANNEL_0,&time,4,EXI_READ,NULL)==0) ret |= 0x04;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x08;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x10;

	EXI_Unlock(EXI_CHANNEL_0);
	*gctime = time;
	if(ret) return 0;

	return 1;
}

static u32 __sram_read(void *buffer)
{
	u32 command,ret;

	DCInvalidateRange(buffer,64);

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,NULL)==0) return 0;
	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		EXI_Unlock(EXI_CHANNEL_0);
		return 0;
	}

	ret = 0;
	command = 0x20000100;
	if(EXI_Imm(EXI_CHANNEL_0,&command,4,EXI_WRITE,NULL)==0) ret |= 0x01;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
	if(EXI_Dma(EXI_CHANNEL_0,buffer,64,EXI_READ,NULL)==0) ret |= 0x04;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x08;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x10;
	if(EXI_Unlock(EXI_CHANNEL_0)==0) ret |= 0x20;

	if(ret) return 0;
	return 1;
}

static u32 __sram_write(void *buffer,u32 loc,u32 len)
{
	u32 cmd,ret;

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,__sram_writecallback)==0) return 0;
	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		EXI_Unlock(EXI_CHANNEL_0);
		return 0;
	}

	ret = 0;
	cmd = 0xa0000100+(loc<<6);
	if(EXI_Imm(EXI_CHANNEL_0,&cmd,4,EXI_WRITE,NULL)==0) ret |= 0x01;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
	if(EXI_ImmEx(EXI_CHANNEL_0,buffer,len,EXI_WRITE)==0) ret |= 0x04;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x08;
	if(EXI_Unlock(EXI_CHANNEL_0)==0) ret |= 0x10;

	if(ret) return 0;
	return 1;
}

static s32 __sram_writecallback(s32 chn,s32 dev)
{
	sramcntrl.sync = __sram_write(sramcntrl.srambuf+sramcntrl.offset,sramcntrl.offset,(64-sramcntrl.offset));
	if(sramcntrl.sync) sramcntrl.offset = 64;

	return 1;
}

static s32 __sram_sync()
{
	return sramcntrl.sync;
}

void __sram_init()
{
	sramcntrl.enabled = 0;
	sramcntrl.locked = 0;
	sramcntrl.sync = __sram_read(sramcntrl.srambuf);

	sramcntrl.offset = 64;
}

static void DisableWriteGatherPipe()
{
	mtspr(920,(mfspr(920)&~0x40000000));
}

static void __buildchecksum(u16 *buffer,u16 *c1,u16 *c2)
{
	u32 i;

	*c1 = 0;
	*c2 = 0;
	for(i=0;i<4;i++) {
		*c1 += buffer[6+i];
		*c2 += buffer[6+i]^-1;
	}
}

static void* __locksram(u32 loc)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(!sramcntrl.locked) {
		sramcntrl.enabled = level;
		sramcntrl.locked = 1;
		return (void*)((u32)sramcntrl.srambuf+loc);
	}
	_CPU_ISR_Restore(level);
	return NULL;
}

static u32 __unlocksram(u32 write,u32 loc)
{
	syssram *sram = (syssram*)sramcntrl.srambuf;

	if(write) {
		if(!loc) {
			if((sram->flags&0x03)>0x02) sram->flags = (sram->flags&~0x03);
			__buildchecksum((u16*)sramcntrl.srambuf,&sram->checksum,&sram->checksum_inv);
		}
		if(loc<sramcntrl.offset) sramcntrl.offset = loc;

		sramcntrl.sync = __sram_write(sramcntrl.srambuf+sramcntrl.offset,sramcntrl.offset,(64-sramcntrl.offset));
		if(sramcntrl.sync) sramcntrl.offset = 64;
	}
	sramcntrl.locked = 0;
	_CPU_ISR_Restore(sramcntrl.enabled);
	return sramcntrl.sync;
}

//returns the size of font
static u32 __read_font(void *buffer)
{
	if(SYS_GetFontEncoding()==1) __SYS_ReadROM(buffer,315392,1769216);
	else __SYS_ReadROM(buffer,12288,2084608);
	return __get_fontsize(buffer);
}

static void __expand_font(const u8 *src,u8 *dest)
{
	s32 cnt;
	u32 idx;
	u8 val1,val2;
	u8 *data = (u8*)sys_fontdata+44;

	if(sys_fontdata->sheet_format==0x0000) {
		cnt = (sys_fontdata->sheet_fullsize/2)-1;

		while(cnt>=0) {
			idx = _SHIFTR(src[cnt],6,2);
			val1 = data[idx];

			idx = _SHIFTR(src[cnt],4,2);
			val2 = data[idx];

			dest[(cnt<<1)+0] =((val1&0xf0)|(val2&0x0f));

			idx = _SHIFTR(src[cnt],2,2);
			val1 = data[idx];

			idx = _SHIFTR(src[cnt],0,2);
			val2 = data[idx];

			dest[(cnt<<1)+1] =((val1&0xf0)|(val2&0x0f));

			cnt--;
		}
	}
	DCStoreRange(dest,sys_fontdata->sheet_fullsize);
}

static void __dsp_bootstrap()
{
	u16 status;
	u32 tick;

	memcpy(SYS_GetArenaHi()-128,(void*)0x81000000,128);
	memcpy((void*)0x81000000,_dsp_initcode,128);
	DCFlushRange((void*)0x81000000,128);

	_dspReg[9] = 67;
	_dspReg[5] = (DSPCR_DSPRESET|DSPCR_DSPINT|DSPCR_ARINT|DSPCR_AIINT|DSPCR_HALT);
	_dspReg[5] |= DSPCR_RES;
	while(_dspReg[5]&DSPCR_RES);

	_dspReg[0] = 0;
	while((_SHIFTL(_dspReg[2],16,16)|(_dspReg[3]&0xffff))&0x80000000);

	((u32*)_dspReg)[8] = 0x01000000;
	((u32*)_dspReg)[9] = 0;
	((u32*)_dspReg)[10] = 32;

	status = _dspReg[5];
	while(!(status&DSPCR_ARINT)) status = _dspReg[5];
	_dspReg[5] = status;

	tick = gettick();
	while((gettick()-tick)<2194);

	((u32*)_dspReg)[8] = 0x01000000;
	((u32*)_dspReg)[9] = 0;
	((u32*)_dspReg)[10] = 32;

	status = _dspReg[5];
	while(!(status&DSPCR_ARINT)) status = _dspReg[5];
	_dspReg[5] = status;

	_dspReg[5] &= ~DSPCR_DSPRESET;
	while(_dspReg[5]&0x400);

	_dspReg[5] &= ~DSPCR_HALT;
	while(!(_dspReg[2]&0x8000));
	status = _dspReg[3];

	_dspReg[5] |= DSPCR_HALT;
	_dspReg[5] = (DSPCR_DSPRESET|DSPCR_DSPINT|DSPCR_ARINT|DSPCR_AIINT|DSPCR_HALT);
	_dspReg[5] |= DSPCR_RES;
	while(_dspReg[5]&DSPCR_RES);

	memcpy((void*)0x81000000,SYS_GetArenaHi()-128,128);
#ifdef _SYS_DEBUG
	printf("__audiosystem_init(finish)\n");
#endif
}

static void __dsp_shutdown()
{
	u32 tick;

	_dspReg[5] = (DSPCR_DSPRESET|DSPCR_HALT);
	_dspReg[27] &= ~0x8000;
	while(_dspReg[5]&0x400);
	while(_dspReg[5]&0x200);

	_dspReg[5] = (DSPCR_DSPRESET|DSPCR_DSPINT|DSPCR_ARINT|DSPCR_AIINT|DSPCR_HALT);
	_dspReg[0] = 0;
	while((_SHIFTL(_dspReg[2],16,16)|(_dspReg[3]&0xffff))&0x80000000);

	tick = gettick();
	while((gettick()-tick)<44);

	_dspReg[5] |= DSPCR_RES;
	while(_dspReg[5]&DSPCR_RES);
}

static void decode_szp(void *src,void *dest)
{
	u32 i,k,link;
	u8 *dest8,*tmp;
	u32 loff,coff,roff;
	u32 size,cnt,cmask,bcnt;
	yay0header *header;

	dest8 = (u8*)dest;
	header = (yay0header*)src;
	size = header->dec_size;
	loff = header->links_offset;
	coff = header->chunks_offset;

	roff = sizeof(yay0header);
	cmask = 0;
	cnt = 0;
	bcnt = 0;

	do {
		if(!bcnt) {
			cmask = *(u32*)(src+roff);
			roff += 4;
			bcnt = 32;
		}

		if(cmask&0x80000000) {
			dest8[cnt++] = *(u8*)(src+coff);
			coff++;
		} else {
			link = *(u16*)(src+loff);
			loff += 2;

			tmp = dest8+(cnt-(link&0x0fff)-1);
			k = link>>12;
			if(k==0) {
				k = (*(u8*)(src+coff))+18;
				coff++;
			} else k += 2;

			for(i=0;i<k;i++) {
				dest8[cnt++] = tmp[i];
			}
		}
		cmask <<= 1;
		bcnt--;
	} while(cnt<size);
}

syssram* __SYS_LockSram()
{
	return (syssram*)__locksram(0);
}

syssramex* __SYS_LockSramEx()
{
	return (syssramex*)__locksram(sizeof(syssram));
}

u32 __SYS_UnlockSram(u32 write)
{
	return __unlocksram(write,0);
}

u32 __SYS_UnlockSramEx(u32 write)
{
	return __unlocksram(write,sizeof(syssram));
}

u32 __SYS_SyncSram()
{
	return __sram_sync();
}

void __SYS_ReadROM(void *buf,u32 len,u32 offset)
{
	u32 cpy_cnt;

	while(len>0) {
		cpy_cnt = (len>256)?256:len;
		while(__read_rom(buf,cpy_cnt,offset)==0);
		offset += cpy_cnt;
		buf += cpy_cnt;
		len -= cpy_cnt;
	}
}

u32 __SYS_GetRTC(u32 *gctime)
{
	u32 cnt,ret;
	u32 time1,time2;

	cnt = 0;
	ret = 0;
	while(cnt<16) {
		if(__getrtc(&time1)==0) ret |= 0x01;
		if(__getrtc(&time2)==0) ret |= 0x02;
		if(ret) return 0;
		if(time1==time2) {
			*gctime = time1;
			return 1;
		}
		cnt++;
	}
	return 0;
}

void __SYS_SetTime(s64 time)
{
	u32 level;
	s64 now;
	s64 *pBootTime = (s64*)0x800030d8;

	_CPU_ISR_Disable(level);
	now = gettime();
	now -= time;
	now += *pBootTime;
	*pBootTime = now;
	settime(now);
	EXI_ProbeReset();
	_CPU_ISR_Restore(level);
}

s64 __SYS_GetSystemTime()
{
	u32 level;
	s64 now;
	s64 *pBootTime = (s64*)0x800030d8;

	_CPU_ISR_Disable(level);
	now = gettime();
	now += *pBootTime;
	_CPU_ISR_Restore(level);
	return now;
}

void __SYS_SetBootTime()
{
	u32 gctime;

	__SYS_LockSram();
	__SYS_GetRTC(&gctime);
	__SYS_SetTime(secs_to_ticks(gctime));
	__SYS_UnlockSram(0);
}

u32 __SYS_LoadFont(void *src,void *dest)
{
	if(__read_font(src)==0) return 0;

	decode_szp(src,dest);

	sys_fontdata = (sys_fontheader*)dest;
	sys_fontwidthtab = (u8*)dest+sys_fontdata->width_table;
	sys_fontcharsinsheet = sys_fontdata->sheet_column*sys_fontdata->sheet_row;

	/* TODO: implement SJIS handling */
	return 1;
}

#if defined(HW_RVL)
void* __SYS_GetIPCBufferLo()
{
	return __ipcbufferlo;
}

void* __SYS_GetIPCBufferHi()
{
	return __ipcbufferhi;
}

#endif

void _V_EXPORTNAME(void)
{ __sys_versionbuild = _V_STRING; __sys_versiondate = _V_DATE_; }

#if defined(HW_RVL)
void __SYS_DoPowerCB(void)
{
	u32 level;
	powercallback powcb;

	_CPU_ISR_Disable(level);
	powcb = __POWCallback;
	__POWCallback = __POWDefaultHandler;
	powcb();
	_CPU_ISR_Restore(level);
}
#endif

void __SYS_InitCallbacks()
{
#if defined(HW_RVL)
	__POWCallback = __POWDefaultHandler;
	__sys_resetdown = 0;
#endif
	__RSWCallback = __RSWDefaultHandler;
}

void __attribute__((weak)) __SYS_PreInit()
{

}

void SYS_Init()
{
	u32 level;

	_CPU_ISR_Disable(level);

	__SYS_PreInit();

	if(system_initialized) return;
	system_initialized = 1;

	_V_EXPORTNAME();

	__init_syscall_array();
	__lowmem_init();
#if defined(HW_RVL)
	__ipcbuffer_init();
#endif
	__lwp_wkspace_init(KERNEL_HEAP);
	__lwp_queue_init_empty(&sys_reset_func_queue);
	__lwp_objmgr_initinfo(&sys_alarm_objects,LWP_MAX_WATCHDOGS,sizeof(alarm_st));
	__sys_state_init();
	__lwp_priority_init();
	__lwp_watchdog_init();
	__exception_init();
	__systemcall_init();
	__decrementer_init();
	__irq_init();
	__exi_init();
	__sram_init();
	__si_init();
	__lwp_thread_coreinit();
	__lwp_sysinit();
	__memlock_init();
	__lwp_mqbox_init();
	__lwp_sema_init();
	__lwp_mutex_init();
	__lwp_cond_init();
	__timesystem_init();
	__dsp_bootstrap();

	if(!__sys_inIPL)
		__memprotect_init();

#ifdef SDLOADER_FIX
	__SYS_SetBootTime();
#endif
	DisableWriteGatherPipe();
	__SYS_InitCallbacks();
#if defined(HW_RVL)
	__IPC_ClntInit();
#elif defined(HW_DOL)
	IRQ_Request(IRQ_PI_RSW,__RSWHandler,NULL);
	__MaskIrq(IRQMASK(IRQ_PI_RSW));
#endif
	__libc_init(1);
	__lwp_thread_startmultitasking();
	_CPU_ISR_Restore(level);
}

// This function gets called inside the main thread, prior to the application's main() function
void SYS_PreMain()
{
#if defined(HW_RVL)
	u32 i;

	for (i = 0; i < 32; ++i)
		IOS_Close(i);

	__IOS_LoadStartupIOS();
	__IOS_InitializeSubsystems();
	STM_RegisterEventHandler(__STMEventHandler);
	CONF_Init();
	WII_Initialize();
#endif
}

u32 SYS_ResetButtonDown()
{
	return (!(_piReg[0]&0x00010000));
}

#if defined(HW_DOL)
void SYS_ResetSystem(s32 reset,u32 reset_code,s32 force_menu)
{
	u32 ret = 0;
	syssram *sram;

	__dsp_shutdown();

	if(reset==SYS_SHUTDOWN) {
		ret = __PADDisableRecalibration(TRUE);
	}

	while(__call_resetfuncs(FALSE)==0);

	if(reset==SYS_HOTRESET && force_menu==TRUE) {
		sram = __SYS_LockSram();
		sram->flags |= 0x40;
		__SYS_UnlockSram(TRUE);
		while(!__SYS_SyncSram());
	}

	__exception_closeall();
	__call_resetfuncs(TRUE);

	LCDisable();

	__lwp_thread_dispatchdisable();
	if(reset==SYS_HOTRESET) {
		__dohotreset(reset_code);
	} else if(reset==SYS_RESTART) {
		__lwp_thread_closeall();
		__lwp_thread_dispatchunnest();
		__doreboot(reset_code,force_menu);
	}

	__lwp_thread_closeall();

	memset((void*)0x80000040,0,140);
	memset((void*)0x800000D4,0,20);
	memset((void*)0x800000F4,0,4);
	memset((void*)0x80003000,0,192);
	memset((void*)0x800030C8,0,12);
	memset((void*)0x800030E2,0,1);

	__PADDisableRecalibration(ret);
}
#endif

#if defined(HW_RVL)

void SYS_ResetSystem(s32 reset,u32 reset_code,s32 force_menu)
{
	u32 ret = 0;

	__dsp_shutdown();

	if(reset==SYS_SHUTDOWN) {
		ret = __PADDisableRecalibration(TRUE);
	}

	while(__call_resetfuncs(FALSE)==0);

	switch(reset) {
		case SYS_RESTART:
			STM_RebootSystem();
			break;
		case SYS_POWEROFF:
			if(CONF_GetShutdownMode() == CONF_SHUTDOWN_IDLE) {
				ret = CONF_GetIdleLedMode();
				if(ret <= 2) STM_SetLedMode(ret);
				STM_ShutdownToIdle();
			} else {
				STM_ShutdownToStandby();
			}
			break;
		case SYS_POWEROFF_STANDBY:
			STM_ShutdownToStandby();
			break;
		case SYS_POWEROFF_IDLE:
			ret = CONF_GetIdleLedMode();
			if(ret >= 0 && ret <= 2) STM_SetLedMode(ret);
			STM_ShutdownToIdle();
			break;
		case SYS_RETURNTOMENU:
			WII_ReturnToMenu();
			break;
	}

	//TODO: implement SYS_HOTRESET
	// either restart failed or this is SYS_SHUTDOWN

	__IOS_ShutdownSubsystems();

	__exception_closeall();
	__call_resetfuncs(TRUE);

	LCDisable();

	__lwp_thread_dispatchdisable();
	__lwp_thread_closeall();

	memset((void*)0x80000040,0,140);
	memset((void*)0x800000D4,0,20);
	memset((void*)0x800000F4,0,4);
	memset((void*)0x80003000,0,192);
	memset((void*)0x800030C8,0,12);
	memset((void*)0x800030E2,0,1);

	__PADDisableRecalibration(ret);
}
#endif

void SYS_RegisterResetFunc(sys_resetinfo *info)
{
	u32 level;
	sys_resetinfo *after;
	lwp_queue *header = &sys_reset_func_queue;

	_CPU_ISR_Disable(level);
	for(after=(sys_resetinfo*)header->first;after->node.next!=NULL && info->prio>=after->prio;after=(sys_resetinfo*)after->node.next);
	__lwp_queue_insertI(after->node.prev,&info->node);
	_CPU_ISR_Restore(level);
}

void SYS_UnregisterResetFunc(sys_resetinfo *info) {
	u32 level;
	lwp_node *n;

	_CPU_ISR_Disable(level);
	for (n = sys_reset_func_queue.first; n->next; n = n->next) {
		if (n == &info->node) {
			__lwp_queue_extractI(n);
			break;
		}
	}
	_CPU_ISR_Restore(level);
}

void SYS_SetArena1Lo(void *newLo)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__sysarena1lo = newLo;
	_CPU_ISR_Restore(level);
}

void* SYS_GetArena1Lo()
{
	u32 level;
	void *arenalo;

	_CPU_ISR_Disable(level);
	arenalo = __sysarena1lo;
	_CPU_ISR_Restore(level);

	return arenalo;
}

void SYS_SetArena1Hi(void *newHi)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__sysarena1hi = newHi;
	_CPU_ISR_Restore(level);
}

void* SYS_GetArena1Hi()
{
	u32 level;
	void *arenahi;

	_CPU_ISR_Disable(level);
	arenahi = __sysarena1hi;
	_CPU_ISR_Restore(level);

	return arenahi;
}

u32 SYS_GetArena1Size()
{
	u32 level,size;

	_CPU_ISR_Disable(level);
	size = ((u32)__sysarena1hi - (u32)__sysarena1lo);
	_CPU_ISR_Restore(level);

	return size;
}

void* SYS_AllocArena1MemLo(u32 size,u32 align)
{
	u32 mem1lo;
	void *ptr = NULL;

	mem1lo = (u32)SYS_GetArena1Lo();
	ptr = (void*)((mem1lo+(align-1))&~(align-1));
	mem1lo = ((((u32)ptr+size+align)-1)&~(align-1));
	SYS_SetArena1Lo((void*)mem1lo);

	return ptr;
}

#if defined(HW_RVL)
void SYS_SetArena2Lo(void *newLo)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__sysarena2lo = newLo;
	_CPU_ISR_Restore(level);
}

void* SYS_GetArena2Lo()
{
	u32 level;
	void *arenalo;

	_CPU_ISR_Disable(level);
	arenalo = __sysarena2lo;
	_CPU_ISR_Restore(level);

	return arenalo;
}

void SYS_SetArena2Hi(void *newHi)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__sysarena2hi = newHi;
	_CPU_ISR_Restore(level);
}

void* SYS_GetArena2Hi()
{
	u32 level;
	void *arenahi;

	_CPU_ISR_Disable(level);
	arenahi = __sysarena2hi;
	_CPU_ISR_Restore(level);

	return arenahi;
}

u32 SYS_GetArena2Size()
{
	u32 level,size;

	_CPU_ISR_Disable(level);
	size = ((u32)__sysarena2hi - (u32)__sysarena2lo);
	_CPU_ISR_Restore(level);

	return size;
}

void* SYS_AllocArena2MemLo(u32 size,u32 align)
{
	u32 mem2lo;
	void *ptr = NULL;

	mem2lo = (u32)SYS_GetArena2Lo();
	ptr = (void*)((mem2lo+(align-1))&~(align-1));
	mem2lo = ((((u32)ptr+size+align)-1)&~(align-1));
	SYS_SetArena2Lo((void*)mem2lo);

	return ptr;
}
#endif

void SYS_ProtectRange(u32 chan,void *addr,u32 bytes,u32 cntrl)
{
	u16 rcntrl;
	u32 pstart,pend,level;

	if(chan<SYS_PROTECTCHANMAX) {
		pstart = ((u32)addr)&~0x3ff;
		pend = ((((u32)addr)+bytes)+1023)&~0x3ff;
		DCFlushRange((void*)pstart,(pend-pstart));

		_CPU_ISR_Disable(level);

		__UnmaskIrq(IRQMASK(chan));
		_memReg[chan<<2] = _SHIFTR(pstart,10,16);
		_memReg[(chan<<2)+1] = _SHIFTR(pend,10,16);

		rcntrl = _memReg[8];
		rcntrl = (rcntrl&~(_SHIFTL(3,(chan<<1),2)))|(_SHIFTL(cntrl,(chan<<1),2));
		_memReg[8] = rcntrl;

		if(cntrl==SYS_PROTECTRDWR)
			__MaskIrq(IRQMASK(chan));

		_CPU_ISR_Restore(level);
	}
}

void* SYS_AllocateFramebuffer(GXRModeObj *rmode)
{
	return memalign(32, VIDEO_GetFrameBufferSize(rmode));
}

u32 SYS_GetFontEncoding()
{
	u32 ret,tv_mode;

	if(sys_fontenc<=0x0001) return sys_fontenc;

	ret = 0;
	tv_mode = VIDEO_GetCurrentTvMode();
	if(tv_mode==VI_NTSC && _viReg[55]&0x0002) ret = 1;
	sys_fontenc = ret;
	return ret;
}

u32 SYS_InitFont(sys_fontheader *font_data)
{
	void *packed_data = NULL;

	if(!font_data) return 0;

	if(SYS_GetFontEncoding()==1) {
		memset(font_data,0,SYS_FONTSIZE_SJIS);
		packed_data = (void*)((u32)font_data+868096);
	} else {
		memset(font_data,0,SYS_FONTSIZE_ANSI);
		packed_data = (void*)((u32)font_data+119072);
	}

	if(__SYS_LoadFont(packed_data,font_data)==1) {
		sys_fontimage = (u8*)((((u32)font_data+font_data->sheet_image)+31)&~31);
		__expand_font((u8*)font_data+font_data->sheet_image,sys_fontimage);
		return 1;
	}

	return 0;
}

void SYS_GetFontTexture(s32 c,void **image,s32 *xpos,s32 *ypos,s32 *width)
{
	u32 sheets,rem;

	*xpos = 0;
	*ypos = 0;
	*image = NULL;
	if(!sys_fontwidthtab || ! sys_fontimage) return;

	if(c<sys_fontdata->first_char || c>sys_fontdata->last_char) c = sys_fontdata->inval_char;
	else c -= sys_fontdata->first_char;

	sheets = c/sys_fontcharsinsheet;
	rem = c%sys_fontcharsinsheet;
	*image = sys_fontimage+(sys_fontdata->sheet_size*sheets);
	*xpos = (rem%sys_fontdata->sheet_column)*sys_fontdata->cell_width;
	*ypos = (rem/sys_fontdata->sheet_column)*sys_fontdata->cell_height;
	*width = sys_fontwidthtab[c];
}

void SYS_GetFontTexel(s32 c,void *image,s32 pos,s32 stride,s32 *width)
{
	u32 sheets,rem;
	u32 xoff,yoff;
	u32 xpos,ypos;
	u8 *img_start;
	u8 *ptr1,*ptr2;

	if(!sys_fontwidthtab || ! sys_fontimage) return;

	if(c<sys_fontdata->first_char || c>sys_fontdata->last_char) c = sys_fontdata->inval_char;
	else c -= sys_fontdata->first_char;

	sheets = c/sys_fontcharsinsheet;
	rem = c%sys_fontcharsinsheet;
	xoff = (rem%sys_fontdata->sheet_column)*sys_fontdata->cell_width;
	yoff = (rem/sys_fontdata->sheet_column)*sys_fontdata->cell_height;
	img_start = sys_fontimage+(sys_fontdata->sheet_size*sheets);

	ypos = 0;
	while(ypos<sys_fontdata->cell_height) {
		xpos = 0;
		while(xpos<sys_fontdata->cell_width) {
			ptr1 = img_start+(((sys_fontdata->sheet_width/8)<<5)*((ypos+yoff)/8));
			ptr1 = ptr1+(((xpos+xoff)/8)<<5);
			ptr1 = ptr1+(((ypos+yoff)%8)<<2);
			ptr1 = ptr1+(((xpos+xoff)%8)/2);

			ptr2 = image+((ypos/8)*(((stride<<1)/8)<<5));
			ptr2 = ptr2+(((xpos+pos)/8)<<5);
			ptr2 = ptr2+(((xpos+pos)%8)/2);
			ptr2 = ptr2+((ypos%8)<<2);

			*ptr2 = *ptr1;

			xpos += 2;
		}
		ypos++;
	}
	*width = sys_fontwidthtab[c];
}

s32 SYS_CreateAlarm(syswd_t *thealarm)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_allocate();
	if(!alarm) return -1;

	alarm->alarmhandler = NULL;
	alarm->ticks = 0;
	alarm->start_per = 0;
	alarm->periodic = 0;

	*thealarm = (LWP_OBJMASKTYPE(LWP_OBJTYPE_SYSWD)|LWP_OBJMASKID(alarm->object.id));
	__lwp_thread_dispatchenable();
	return 0;
}

s32 SYS_SetAlarm(syswd_t thealarm,const struct timespec *tp,alarmcallback cb,void *cbarg)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_open(thealarm);
	if(!alarm) return -1;

	alarm->cb_arg = cbarg;
	alarm->alarmhandler = cb;
	alarm->ticks = __lwp_wd_calc_ticks(tp);

	alarm->periodic = 0;
	alarm->start_per = 0;

	__lwp_wd_initialize(&alarm->alarm,__sys_alarmhandler,alarm->object.id,(void*)thealarm);
	__lwp_wd_insert_ticks(&alarm->alarm,alarm->ticks);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 SYS_SetPeriodicAlarm(syswd_t thealarm,const struct timespec *tp_start,const struct timespec *tp_period,alarmcallback cb,void *cbarg)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_open(thealarm);
	if(!alarm) return -1;

	alarm->start_per = __lwp_wd_calc_ticks(tp_start);
	alarm->periodic = __lwp_wd_calc_ticks(tp_period);
	alarm->alarmhandler = cb;
	alarm->cb_arg = cbarg;

	alarm->ticks = 0;

	__lwp_wd_initialize(&alarm->alarm,__sys_alarmhandler,alarm->object.id,(void*)thealarm);
	__lwp_wd_insert_ticks(&alarm->alarm,alarm->start_per);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 SYS_RemoveAlarm(syswd_t thealarm)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_open(thealarm);
	if(!alarm) return -1;

	alarm->alarmhandler = NULL;
	alarm->ticks = 0;
	alarm->periodic = 0;
	alarm->start_per = 0;

	__lwp_wd_remove_ticks(&alarm->alarm);
	__lwp_syswd_free(alarm);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 SYS_CancelAlarm(syswd_t thealarm)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_open(thealarm);
	if(!alarm) return -1;

	alarm->alarmhandler = NULL;
	alarm->ticks = 0;
	alarm->periodic = 0;
	alarm->start_per = 0;

	__lwp_wd_remove_ticks(&alarm->alarm);
	__lwp_thread_dispatchenable();
	return 0;
}

resetcallback SYS_SetResetCallback(resetcallback cb)
{
	u32 level;
	resetcallback old;

	_CPU_ISR_Disable(level);
	old = __RSWCallback;
	__RSWCallback = cb;
#if defined(HW_DOL)
	if(__RSWCallback) {
		_piReg[0] = 2;
		__UnmaskIrq(IRQMASK(IRQ_PI_RSW));
	} else
		__MaskIrq(IRQMASK(IRQ_PI_RSW));
#endif
	_CPU_ISR_Restore(level);
	return old;
}

#if defined(HW_RVL)
powercallback SYS_SetPowerCallback(powercallback cb)
{
	u32 level;
	powercallback old;

	_CPU_ISR_Disable(level);
	old = __POWCallback;
	__POWCallback = cb;
	_CPU_ISR_Restore(level);
	return old;
}
#endif

void SYS_StartPMC(u32 mcr0val,u32 mcr1val)
{
	mtmmcr0(mcr0val);
	mtmmcr1(mcr1val);
}

void SYS_StopPMC()
{
	mtmmcr0(0);
	mtmmcr1(0);
}

void SYS_ResetPMC()
{
	mtpmc1(0);
	mtpmc2(0);
	mtpmc3(0);
	mtpmc4(0);
}

void SYS_DumpPMC()
{
	printf("<%lu load/stores / %lu miss cycles / %lu cycles / %lu instructions>\n",mfpmc1(),mfpmc2(),mfpmc3(),mfpmc4());
}

void SYS_SetWirelessID(u32 chan,u32 id)
{
	u32 write;
	syssramex *sram;

	write = 0;
	sram = __SYS_LockSramEx();
	if(sram->wirelessPad_id[chan]!=(u16)id) {
		sram->wirelessPad_id[chan] = (u16)id;
		write = 1;
	}
	__SYS_UnlockSramEx(write);
}

u32 SYS_GetWirelessID(u32 chan)
{
	u16 id;
	syssramex *sram;

	id = 0;
	sram = __SYS_LockSramEx();
	id = sram->wirelessPad_id[chan];
	__SYS_UnlockSramEx(0);
	return id;
}

#if defined(HW_RVL)
u32 SYS_GetHollywoodRevision()
{
	u32 rev;
	DCInvalidateRange((void*)0x80003138,8);
	rev = *((u32*)0x80003138);
	return rev;
}
#endif

u64 SYS_Time()
{
	u64 current_time = 0;
    u32 gmtime =0;
    __SYS_GetRTC(&gmtime);
    current_time = gmtime;
#ifdef HW_RVL
	u32 bias;
	if (CONF_GetCounterBias(&bias) >= 0)
		current_time += bias;
#else
	syssram* sram = __SYS_LockSram();
	current_time += sram->counter_bias;
	__SYS_UnlockSram(0);
#endif
	return (TB_TIMER_CLOCK * 1000) * current_time;
}

#ifndef _GX_DEFINES_H
#define _GX_DEFINES_H

#define _SHIFTL(v, s, w)	((uint32_t) (((uint32_t)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	((uint32_t)(((uint32_t)(v) >> (s)) & ((0x01 << (w)) - 1)))

#ifndef __lhbrx
#define __lhbrx(base,index)			\
({	register u16 res;				\
	__asm__ volatile ("lhbrx	%0,%1,%2" : "=r"(res) : "b%"(index), "r"(base) : "memory"); \
	res; })
#endif

#ifndef __sthbrx
#define __sthbrx(base,index,value)	\
	__asm__ volatile ("sthbrx	%0,%1,%2" : : "r"(value), "b%"(index), "r"(base) : "memory")
#endif

#ifndef __stwbrx
#define __stwbrx(base,index,value)	\
	__asm__ volatile ("stwbrx	%0,%1,%2" : : "r"(value), "b%"(index), "r"(base) : "memory")
#endif

#ifndef _sync
#define _sync() asm volatile("sync")
#endif

#ifndef _nop
#define _nop() asm volatile("nop")
#endif

#ifndef ppcsync
#define ppcsync() asm volatile("sc")
#endif

#ifndef ppchalt
#define ppchalt() ({					\
      _sync(); \
	while(1) {							\
      _nop(); \
		asm volatile("li 3,0");			\
      _nop(); \
	}									\
})
#endif

#ifndef _CPU_ISR_Enable
#define _CPU_ISR_Enable() \
	{ register u32 _val = 0; \
	  __asm__ __volatile__ ( \
		"mfmsr %0\n" \
		"ori %0,%0,0x8000\n" \
		"mtmsr %0" \
		: "=&r" ((_val)) : "0" ((_val)) \
	  ); \
	}
#endif

#ifndef _CPU_ISR_Disable
#define _CPU_ISR_Disable( _isr_cookie ) \
  { register u32 _disable_mask = 0; \
	_isr_cookie = 0; \
    __asm__ __volatile__ ( \
	  "mfmsr %0\n" \
	  "rlwinm %1,%0,0,17,15\n" \
	  "mtmsr %1\n" \
	  "extrwi %0,%0,1,16" \
	  : "=&r" ((_isr_cookie)), "=&r" ((_disable_mask)) \
	  : "0" ((_isr_cookie)), "1" ((_disable_mask)) \
	); \
  }
#endif

#ifndef _CPU_ISR_Restore
#define _CPU_ISR_Restore( _isr_cookie )  \
  { register u32 _enable_mask = 0; \
	__asm__ __volatile__ ( \
    "    cmpwi %0,0\n" \
	"    beq 1f\n" \
	"    mfmsr %1\n" \
	"    ori %1,%1,0x8000\n" \
	"    mtmsr %1\n" \
	"1:" \
	: "=r"((_isr_cookie)),"=&r" ((_enable_mask)) \
	: "0"((_isr_cookie)),"1" ((_enable_mask)) \
	); \
  }
#endif

#ifdef GEKKO

extern void __lwp_thread_stopmultitasking(void (*exitfunc)());

#define OSThread lwp_t
#define OSCond lwpq_t
#define OSThreadQueue lwpq_t

#define OSInitMutex(mutex) LWP_MutexInit(mutex, 0)
#define OSLockMutex(mutex) LWP_MutexLock(mutex)
#define OSUnlockMutex(mutex) LWP_MutexUnlock(mutex)
#define OSTryLockMutex(mutex) LWP_MutexTryLock(mutex)

#define OSInitCond(cond) LWP_CondInit(cond)
#define OSSignalCond(cond) LWP_ThreadSignal(cond)
#define OSWaitCond(cond, mutex) LWP_CondWait(cond, mutex)

#define OSInitThreadQueue(queue) LWP_InitQueue(queue)
#define OSSleepThread(queue) LWP_ThreadSleep(queue)
#define OSJoinThread(thread, val) LWP_JoinThread(thread, val)

#define OSCreateThread(thread, func, intarg, ptrarg, stackbase, stacksize, priority, attrs) LWP_CreateThread(thread, func, ptrarg, stackbase, stacksize, priority)

#define VISetPostRetraceCallback(cb) VIDEO_SetPostRetraceCallback(cb)
#define VISetBlack(black) VIDEO_SetBlack(black)
#define VIFlush() VIDEO_Flush()
#define VISetNextFrameBuffer(fb) VIDEO_SetNextFramebuffer(fb)
#define VIWaitForRetrace() VIDEO_WaitVSync()
#define VIConfigure(rm) VIDEO_Configure(rm)
#define VIInit() VIDEO_Init()
#define VIPadFrameBufferWidth(width) VIDEO_PadFramebufferWidth(width)

#endif

#endif

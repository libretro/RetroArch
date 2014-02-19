#ifndef __PPC_ASM_H
#define __PPC_ASM_H

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

#ifdef GEKKO

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

extern void __lwp_thread_stopmultitasking(void (*exitfunc)());
#endif

#endif

#endif

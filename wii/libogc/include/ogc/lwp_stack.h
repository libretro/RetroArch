#ifndef __LWP_STACK_H__
#define __LWP_STACK_H__

#include <gctypes.h>
#include <lwp_threads.h>

#define CPU_STACK_ALIGNMENT				8
#define CPU_MINIMUM_STACK_SIZE			1024*8
#define CPU_MINIMUM_STACK_FRAME_SIZE	16
#define CPU_MODES_INTERRUPT_MASK		0x00000001 /* interrupt level in mode */

#ifdef __cplusplus
extern "C" {
#endif

u32 __lwp_stack_allocate(lwp_cntrl *,u32);
void __lwp_stack_free(lwp_cntrl *);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_stack.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif

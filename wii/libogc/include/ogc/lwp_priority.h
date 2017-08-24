#ifndef __LWP_PRIORITY_H__
#define __LWP_PRIORITY_H__

#include <gctypes.h>
#include "machine/processor.h"

#define LWP_PRIO_MIN				0
#define LWP_PRIO_MAX				255

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _priocntrl {
	u32 *minor;
	u32 ready_minor,ready_major;
	u32 block_minor,block_major;
} prio_cntrl;

extern vu32 _prio_major_bitmap;
extern u32 _prio_bitmap[];

void __lwp_priority_init();

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_priority.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif

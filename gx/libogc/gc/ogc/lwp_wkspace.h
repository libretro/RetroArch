#ifndef __LWP_WKSPACE_H__
#define __LWP_WKSPACE_H__

#include <gctypes.h>
#include <lwp_heap.h>

#ifdef __cplusplus
extern "C" {
#endif

extern heap_cntrl __wkspace_heap;

void __lwp_wkspace_init(u32 size);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_wkspace.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif

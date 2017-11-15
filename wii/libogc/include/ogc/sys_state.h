#ifndef __SYS_STATE_H__
#define __SYS_STATE_H__

#define SYS_STATE_BEFORE_INIT		0
#define SYS_STATE_BEFORE_MT			1
#define SYS_STATE_BEGIN_MT			2
#define SYS_STATE_UP				3
#define SYS_STATE_SHUTDOWN			4
#define SYS_STATE_FAILED			5

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

extern u32 _sys_state_curr;

#ifdef LIBOGC_INTERNAL
#include <libogc/sys_state.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif

/* Stand-in for libnx's switch/kernel/svc.h: the two priority calls,
 * which fifo_svc_shim.c supplies over SCHED_FIFO. */
#ifndef HOSTNX_SWITCH_KERNEL_SVC_H
#define HOSTNX_SWITCH_KERNEL_SVC_H

#include "../types.h"

#define CUR_THREAD_HANDLE 0xFFFF8000

Result svcGetThreadPriority(s32 *priority, Handle handle);
Result svcSetThreadPriority(Handle handle, u32 priority);

#endif

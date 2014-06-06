#ifndef __LWP_TQDATA_H__
#define __LWP_TQDATA_H__

#define LWP_THREADQ_NUM_PRIOHEADERS		4
#define LWP_THREADQ_PRIOPERHEADER		64
#define LWP_THREADQ_REVERSESEARCHMASK	0x20

#define LWP_THREADQ_SYNCHRONIZED		0
#define LWP_THREADQ_NOTHINGHAPPEND		1
#define LWP_THREADQ_TIMEOUT				2
#define LWP_THREADQ_SATISFIED			3

#define LWP_THREADQ_MODEFIFO			0
#define LWP_THREADQ_MODEPRIORITY		1

#ifdef __cplusplus
extern "C" {
#endif

#include "lwp_queue.h"
#include "lwp_priority.h"

typedef struct _lwpthrqueue {
	union {
		lwp_queue fifo;
		lwp_queue priority[LWP_THREADQ_NUM_PRIOHEADERS];
	} queues;
	u32 sync_state;
	u32 mode;
	u32 state;
	u32 timeout_state;
} lwp_thrqueue;

#ifdef __cplusplus
	}
#endif

#endif

#ifndef __LWP_MESSAGES_H__
#define __LWP_MESSAGES_H__

#include <gctypes.h>
#include <limits.h>
#include <string.h>
#include <lwp_threadq.h>

//#define _LWPMQ_DEBUG

#define LWP_MQ_FIFO							0
#define LWP_MQ_PRIORITY						1

#define LWP_MQ_STATUS_SUCCESSFUL			0
#define LWP_MQ_STATUS_INVALID_SIZE			1
#define LWP_MQ_STATUS_TOO_MANY				2
#define LWP_MQ_STATUS_UNSATISFIED			3
#define LWP_MQ_STATUS_UNSATISFIED_NOWAIT	4
#define LWP_MQ_STATUS_DELETED				5
#define LWP_MQ_STATUS_TIMEOUT				6
#define LWP_MQ_STATUS_UNSATISFIED_WAIT		7

#define LWP_MQ_SEND_REQUEST					INT_MAX
#define LWP_MQ_SEND_URGENT					INT_MIN

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mq_notifyhandler)(void *);

typedef struct _mqbuffer {
	u32 size;
	u32 buffer[1];
} mq_buffer;

typedef struct _mqbuffercntrl {
	lwp_node node;
	u32 prio;
	mq_buffer contents;
} mq_buffercntrl;

//the following struct is extensible
typedef struct _mqattr {
	u32 mode;
} mq_attr;

typedef struct _mqcntrl {
	lwp_thrqueue wait_queue;
	mq_attr attr;
	u32 max_pendingmsgs;
	u32 num_pendingmsgs;
	u32 max_msgsize;
	lwp_queue pending_msgs;
	mq_buffer *msq_buffers;
	mq_notifyhandler notify_handler;
	void *notify_arg;
	lwp_queue inactive_msgs;
} mq_cntrl;

u32 __lwpmq_initialize(mq_cntrl *mqueue,mq_attr *attrs,u32 max_pendingmsgs,u32 max_msgsize);
void __lwpmq_close(mq_cntrl *mqueue,u32 status);
u32 __lwpmq_seize(mq_cntrl *mqueue,u32 id,void *buffer,u32 *size,u32 wait,u64 timeout);
u32 __lwpmq_submit(mq_cntrl *mqueue,u32 id,void *buffer,u32 size,u32 type,u32 wait,u64 timeout);
u32 __lwpmq_broadcast(mq_cntrl *mqueue,void *buffer,u32 size,u32 id,u32 *count);
void __lwpmq_msg_insert(mq_cntrl *mqueue,mq_buffercntrl *msg,u32 type);
u32 __lwpmq_flush(mq_cntrl *mqueue);
u32 __lwpmq_flush_support(mq_cntrl *mqueue);
void __lwpmq_flush_waitthreads(mq_cntrl *mqueue);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_messages.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif

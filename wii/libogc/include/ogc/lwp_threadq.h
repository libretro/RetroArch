#ifndef __LWP_THREADQ_H__
#define __LWP_THREADQ_H__

#include <gctypes.h>
#include <lwp_tqdata.h>
#include <lwp_threads.h>
#include <lwp_watchdog.h>

#define LWP_THREADQ_NOTIMEOUT		LWP_WD_NOTIMEOUT

#ifdef __cplusplus
extern "C" {
#endif

lwp_cntrl* __lwp_threadqueue_firstfifo(lwp_thrqueue *queue);
lwp_cntrl* __lwp_threadqueue_firstpriority(lwp_thrqueue *queue);
void __lwp_threadqueue_enqueuefifo(lwp_thrqueue *queue,lwp_cntrl *thethread,u64 timeout);
lwp_cntrl* __lwp_threadqueue_dequeuefifo(lwp_thrqueue *queue);
void __lwp_threadqueue_enqueuepriority(lwp_thrqueue *queue,lwp_cntrl *thethread,u64 timeout);
lwp_cntrl* __lwp_threadqueue_dequeuepriority(lwp_thrqueue *queue);
void __lwp_threadqueue_init(lwp_thrqueue *queue,u32 mode,u32 state,u32 timeout_state);
lwp_cntrl* __lwp_threadqueue_first(lwp_thrqueue *queue);
void __lwp_threadqueue_enqueue(lwp_thrqueue *queue,u64 timeout);
lwp_cntrl* __lwp_threadqueue_dequeue(lwp_thrqueue *queue);
void __lwp_threadqueue_flush(lwp_thrqueue *queue,u32 status);
void __lwp_threadqueue_extract(lwp_thrqueue *queue,lwp_cntrl *thethread);
void __lwp_threadqueue_extractfifo(lwp_thrqueue *queue,lwp_cntrl *thethread);
void __lwp_threadqueue_extractpriority(lwp_thrqueue *queue,lwp_cntrl *thethread);
u32 __lwp_threadqueue_extractproxy(lwp_cntrl *thethread);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_threadq.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif

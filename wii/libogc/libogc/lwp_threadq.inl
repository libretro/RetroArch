#ifndef __LWP_THREADQ_INL__
#define __LWP_THREADQ_INL__

static __inline__ void __lwp_threadqueue_csenter(lwp_thrqueue *queue)
{
	queue->sync_state = LWP_THREADQ_NOTHINGHAPPEND;
}

#endif

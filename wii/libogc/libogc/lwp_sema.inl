#ifndef __LWP_SEMA_INL__
#define __LWP_SEMA_INL__

static __inline__ u32 __lwp_sema_ispriority(lwp_semattr *attr)
{
	return (attr->mode==LWP_SEMA_MODEPRIORITY);
}

static __inline__ void __lwp_sema_seize_isrdisable(lwp_sema *sema,u32 id,u32 wait,u32 *isrlevel)
{
	lwp_cntrl *exec;
	u32 level = *isrlevel;

	exec = _thr_executing;
	exec->wait.ret_code = LWP_SEMA_SUCCESSFUL;
	if(sema->count!=0) {
		--sema->count;
		_CPU_ISR_Restore(level);
		return;
	}

	if(!wait) {
		_CPU_ISR_Restore(level);
		exec->wait.ret_code = LWP_SEMA_UNSATISFIED_NOWAIT;
		return;
	}

	__lwp_thread_dispatchdisable();
	__lwp_threadqueue_csenter(&sema->wait_queue);
	exec->wait.queue = &sema->wait_queue;
	exec->wait.id = id;
	_CPU_ISR_Restore(level);

	__lwp_threadqueue_enqueue(&sema->wait_queue,0);
	__lwp_thread_dispatchenable();
}

#endif

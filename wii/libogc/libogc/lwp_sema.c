#include "asm.h"
#include "lwp_sema.h"

void __lwp_sema_initialize(lwp_sema *sema,lwp_semattr *attrs,u32 init_count)
{
	sema->attrs = *attrs;
	sema->count = init_count;

	__lwp_threadqueue_init(&sema->wait_queue,__lwp_sema_ispriority(attrs)?LWP_THREADQ_MODEPRIORITY:LWP_THREADQ_MODEFIFO,LWP_STATES_WAITING_FOR_SEMAPHORE,LWP_SEMA_TIMEOUT);
}

u32 __lwp_sema_surrender(lwp_sema *sema,u32 id)
{
	u32 level,ret;
	lwp_cntrl *thethread;

	ret = LWP_SEMA_SUCCESSFUL;
	if((thethread=__lwp_threadqueue_dequeue(&sema->wait_queue))) return ret;
	else {
		_CPU_ISR_Disable(level);
		if(sema->count<=sema->attrs.max_cnt)
			++sema->count;
		else
			ret = LWP_SEMA_MAXCNT_EXCEEDED;
		_CPU_ISR_Restore(level);
	}
	return ret;
}

u32 __lwp_sema_seize(lwp_sema *sema,u32 id,u32 wait,u64 timeout)
{
	u32 level;
	lwp_cntrl *exec;

	exec = _thr_executing;
	exec->wait.ret_code = LWP_SEMA_SUCCESSFUL;

	_CPU_ISR_Disable(level);
	if(sema->count!=0) {
		--sema->count;
		_CPU_ISR_Restore(level);
		return LWP_SEMA_SUCCESSFUL;
	}

	if(!wait) {
		_CPU_ISR_Restore(level);
		exec->wait.ret_code = LWP_SEMA_UNSATISFIED_NOWAIT;
		return LWP_SEMA_UNSATISFIED_NOWAIT;
	}

	__lwp_threadqueue_csenter(&sema->wait_queue);
	exec->wait.queue = &sema->wait_queue;
	exec->wait.id = id;
	_CPU_ISR_Restore(level);

	__lwp_threadqueue_enqueue(&sema->wait_queue,timeout);
	return LWP_SEMA_SUCCESSFUL;
}

void __lwp_sema_flush(lwp_sema *sema,u32 status)
{
	__lwp_threadqueue_flush(&sema->wait_queue,status);
}

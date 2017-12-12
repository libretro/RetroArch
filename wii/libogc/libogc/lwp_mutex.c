#include "asm.h"
#include "lwp_mutex.h"

void __lwp_mutex_initialize(lwp_mutex *mutex,lwp_mutex_attr *attrs,u32 init_lock)
{
	mutex->atrrs = *attrs;
	mutex->lock = init_lock;
	mutex->blocked_cnt = 0;

	if(init_lock==LWP_MUTEX_LOCKED) {
		mutex->nest_cnt = 1;
		mutex->holder = _thr_executing;
		if(__lwp_mutex_isinheritprio(attrs) || __lwp_mutex_isprioceiling(attrs))
			_thr_executing->res_cnt++;
	} else {
		mutex->nest_cnt = 0;
		mutex->holder = NULL;
	}

	__lwp_threadqueue_init(&mutex->wait_queue,__lwp_mutex_isfifo(attrs)?LWP_THREADQ_MODEFIFO:LWP_THREADQ_MODEPRIORITY,LWP_STATES_WAITING_FOR_MUTEX,LWP_MUTEX_TIMEOUT);
}

u32 __lwp_mutex_surrender(lwp_mutex *mutex)
{
	lwp_cntrl *thethread;
	lwp_cntrl *holder;

	holder = mutex->holder;

	if(mutex->atrrs.onlyownerrelease) {
		if(!__lwp_thread_isexec(holder))
			return LWP_MUTEX_NOTOWNER;
	}

	if(!mutex->nest_cnt)
		return LWP_MUTEX_SUCCESSFUL;

	mutex->nest_cnt--;
	if(mutex->nest_cnt!=0) {
		switch(mutex->atrrs.nest_behavior) {
			case LWP_MUTEX_NEST_ACQUIRE:
				return LWP_MUTEX_SUCCESSFUL;
			case LWP_MUTEX_NEST_ERROR:
				return LWP_MUTEX_NEST_NOTALLOWED;
			case LWP_MUTEX_NEST_BLOCK:
				break;
		}
	}

	if(__lwp_mutex_isinheritprio(&mutex->atrrs) || __lwp_mutex_isprioceiling(&mutex->atrrs))
		holder->res_cnt--;

	mutex->holder = NULL;
	if(__lwp_mutex_isinheritprio(&mutex->atrrs) || __lwp_mutex_isprioceiling(&mutex->atrrs)) {
		if(holder->res_cnt==0 && holder->real_prio!=holder->cur_prio)
			__lwp_thread_changepriority(holder,holder->real_prio,TRUE);
	}

	if((thethread=__lwp_threadqueue_dequeue(&mutex->wait_queue))) {
		mutex->nest_cnt = 1;
		mutex->holder = thethread;
		if(__lwp_mutex_isinheritprio(&mutex->atrrs) || __lwp_mutex_isprioceiling(&mutex->atrrs))
			thethread->res_cnt++;
	} else
		mutex->lock = LWP_MUTEX_UNLOCKED;

	return LWP_MUTEX_SUCCESSFUL;
}

void __lwp_mutex_seize_irq_blocking(lwp_mutex *mutex,u64 timeout)
{
	lwp_cntrl *exec;

	exec = _thr_executing;
	if(__lwp_mutex_isinheritprio(&mutex->atrrs)){
		if(mutex->holder->cur_prio>exec->cur_prio)
			__lwp_thread_changepriority(mutex->holder,exec->cur_prio,FALSE);
	}

	mutex->blocked_cnt++;
	__lwp_threadqueue_enqueue(&mutex->wait_queue,timeout);

	if(_thr_executing->wait.ret_code==LWP_MUTEX_SUCCESSFUL) {
		if(__lwp_mutex_isprioceiling(&mutex->atrrs)) {
			if(mutex->atrrs.prioceil<exec->cur_prio)
				__lwp_thread_changepriority(exec,mutex->atrrs.prioceil,FALSE);
		}
	}
	__lwp_thread_dispatchenable();
}

void __lwp_mutex_flush(lwp_mutex *mutex,u32 status)
{
	__lwp_threadqueue_flush(&mutex->wait_queue,status);
}

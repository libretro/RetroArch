#include <stdio.h>
#include <lwp_watchdog.h>
#include "asm.h"
#include "lwp_threadq.h"

//#define _LWPTHRQ_DEBUG

static void __lwp_threadqueue_timeout(void *usr_data)
{
	lwp_cntrl *thethread;
	lwp_thrqueue *thequeue;

	__lwp_thread_dispatchdisable();
	thethread = (lwp_cntrl*)usr_data;
	thequeue = thethread->wait.queue;
	if(thequeue->sync_state!=LWP_THREADQ_SYNCHRONIZED && __lwp_thread_isexec(thethread)) {
		if(thequeue->sync_state!=LWP_THREADQ_SATISFIED) thequeue->sync_state = LWP_THREADQ_TIMEOUT;
	} else {
		thethread->wait.ret_code = thethread->wait.queue->timeout_state;
		__lwp_threadqueue_extract(thethread->wait.queue,thethread);
	}
	__lwp_thread_dispatchunnest();
}

lwp_cntrl* __lwp_threadqueue_firstfifo(lwp_thrqueue *queue)
{
	if(!__lwp_queue_isempty(&queue->queues.fifo))
		return (lwp_cntrl*)queue->queues.fifo.first;

	return NULL;
}

lwp_cntrl* __lwp_threadqueue_firstpriority(lwp_thrqueue *queue)
{
	u32 index;

	for(index=0;index<LWP_THREADQ_NUM_PRIOHEADERS;index++) {
		if(!__lwp_queue_isempty(&queue->queues.priority[index]))
			return (lwp_cntrl*)queue->queues.priority[index].first;
	}
	return NULL;
}

void __lwp_threadqueue_enqueuefifo(lwp_thrqueue *queue,lwp_cntrl *thethread,u64 timeout)
{
	u32 level,sync_state;

	_CPU_ISR_Disable(level);

	sync_state = queue->sync_state;
	queue->sync_state = LWP_THREADQ_SYNCHRONIZED;
#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_enqueuefifo(%p,%d)\n",thethread,sync_state);
#endif
	switch(sync_state) {
		case LWP_THREADQ_SYNCHRONIZED:
			break;
		case LWP_THREADQ_NOTHINGHAPPEND:
			__lwp_queue_appendI(&queue->queues.fifo,&thethread->object.node);
			_CPU_ISR_Restore(level);
			return;
		case LWP_THREADQ_TIMEOUT:
			thethread->wait.ret_code = thethread->wait.queue->timeout_state;
			_CPU_ISR_Restore(level);
			break;
		case LWP_THREADQ_SATISFIED:
			if(__lwp_wd_isactive(&thethread->timer)) {
				__lwp_wd_deactivate(&thethread->timer);
				_CPU_ISR_Restore(level);
				__lwp_wd_remove_ticks(&thethread->timer);
			} else
				_CPU_ISR_Restore(level);

			break;
	}
	__lwp_thread_unblock(thethread);
}

lwp_cntrl* __lwp_threadqueue_dequeuefifo(lwp_thrqueue *queue)
{
	u32 level;
	lwp_cntrl *ret;

	_CPU_ISR_Disable(level);
	if(!__lwp_queue_isempty(&queue->queues.fifo)) {
		ret = (lwp_cntrl*)__lwp_queue_firstnodeI(&queue->queues.fifo);
		if(!__lwp_wd_isactive(&ret->timer)) {
			_CPU_ISR_Restore(level);
			__lwp_thread_unblock(ret);
		} else {
			__lwp_wd_deactivate(&ret->timer);
			_CPU_ISR_Restore(level);
			__lwp_wd_remove_ticks(&ret->timer);
			__lwp_thread_unblock(ret);
		}
		return ret;
	}

	switch(queue->sync_state) {
		case LWP_THREADQ_SYNCHRONIZED:
		case LWP_THREADQ_SATISFIED:
			_CPU_ISR_Restore(level);
			return NULL;
		case LWP_THREADQ_NOTHINGHAPPEND:
		case LWP_THREADQ_TIMEOUT:
			queue->sync_state = LWP_THREADQ_SATISFIED;
			_CPU_ISR_Restore(level);
			return _thr_executing;
	}
	return NULL;
}

void __lwp_threadqueue_enqueuepriority(lwp_thrqueue *queue,lwp_cntrl *thethread,u64 timeout)
{
	u32 level,search_prio,header_idx,prio,block_state,sync_state;
	lwp_cntrl *search_thread;
	lwp_queue *header;
	lwp_node *cur_node,*next_node,*prev_node,*search_node;

	__lwp_queue_init_empty(&thethread->wait.block2n);

	prio = thethread->cur_prio;
	header_idx = prio/LWP_THREADQ_PRIOPERHEADER;
	header = &queue->queues.priority[header_idx];
	block_state = queue->state;

	if(prio&LWP_THREADQ_REVERSESEARCHMASK) {
#ifdef _LWPTHRQ_DEBUG
		printf("__lwp_threadqueue_enqueuepriority(%p,reverse_search)\n",thethread);
#endif
		goto reverse_search;
	}

#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_enqueuepriority(%p,forward_search)\n",thethread);
#endif
forward_search:
	search_prio = LWP_PRIO_MIN - 1;
	_CPU_ISR_Disable(level);
	search_thread = (lwp_cntrl*)header->first;
	while(!__lwp_queue_istail(header,(lwp_node*)search_thread)) {
		search_prio = search_thread->cur_prio;
		if(prio<=search_prio) break;
		_CPU_ISR_Flash(level);

		if(!__lwp_statesset(search_thread->cur_state,block_state)) {
			_CPU_ISR_Restore(level);
			goto forward_search;
		}
		search_thread = (lwp_cntrl*)search_thread->object.node.next;
	}
	if(queue->sync_state!=LWP_THREADQ_NOTHINGHAPPEND) goto synchronize;
	queue->sync_state = LWP_THREADQ_SYNCHRONIZED;
	if(prio==search_prio) goto equal_prio;

	search_node = (lwp_node*)search_thread;
	prev_node = search_node->prev;
	cur_node = (lwp_node*)thethread;

	cur_node->next = search_node;
	cur_node->prev = prev_node;
	prev_node->next = cur_node;
	search_node->prev = cur_node;
	_CPU_ISR_Restore(level);
	return;

reverse_search:
	search_prio = LWP_PRIO_MAX + 1;
	_CPU_ISR_Disable(level);
	search_thread = (lwp_cntrl*)header->last;
	while(!__lwp_queue_ishead(header,(lwp_node*)search_thread)) {
		search_prio = search_thread->cur_prio;
		if(prio>=search_prio) break;
		_CPU_ISR_Flash(level);

		if(!__lwp_statesset(search_thread->cur_state,block_state)) {
			_CPU_ISR_Restore(level);
			goto reverse_search;
		}
		search_thread = (lwp_cntrl*)search_thread->object.node.prev;
	}
	if(queue->sync_state!=LWP_THREADQ_NOTHINGHAPPEND) goto synchronize;
	queue->sync_state = LWP_THREADQ_SYNCHRONIZED;
	if(prio==search_prio) goto equal_prio;

	search_node = (lwp_node*)search_thread;
	next_node = search_node->next;
	cur_node = (lwp_node*)thethread;

	cur_node->next = next_node;
	cur_node->prev = search_node;
	search_node->next = cur_node;
	next_node->prev = cur_node;
	_CPU_ISR_Restore(level);
	return;

equal_prio:
#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_enqueuepriority(%p,equal_prio)\n",thethread);
#endif
	search_node = __lwp_queue_tail(&search_thread->wait.block2n);
	prev_node = search_node->prev;
	cur_node = (lwp_node*)thethread;

	cur_node->next = search_node;
	cur_node->prev = prev_node;
	prev_node->next = cur_node;
	search_node->prev = cur_node;
	_CPU_ISR_Restore(level);
	return;

synchronize:
	sync_state = queue->sync_state;
	queue->sync_state = LWP_THREADQ_SYNCHRONIZED;

#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_enqueuepriority(%p,sync_state = %d)\n",thethread,sync_state);
#endif
	switch(sync_state) {
		case LWP_THREADQ_SYNCHRONIZED:
			break;
		case LWP_THREADQ_NOTHINGHAPPEND:
			break;
		case LWP_THREADQ_TIMEOUT:
			thethread->wait.ret_code = thethread->wait.queue->timeout_state;
			_CPU_ISR_Restore(level);
			break;
		case LWP_THREADQ_SATISFIED:
			if(__lwp_wd_isactive(&thethread->timer)) {
				__lwp_wd_deactivate(&thethread->timer);
				_CPU_ISR_Restore(level);
				__lwp_wd_remove_ticks(&thethread->timer);
			} else
				_CPU_ISR_Restore(level);
			break;
	}
	__lwp_thread_unblock(thethread);
}

lwp_cntrl* __lwp_threadqueue_dequeuepriority(lwp_thrqueue *queue)
{
	u32 level,idx;
	lwp_cntrl *newfirstthr,*ret = NULL;
	lwp_node *newfirstnode,*newsecnode,*last_node,*next_node,*prev_node;

	_CPU_ISR_Disable(level);
	for(idx=0;idx<LWP_THREADQ_NUM_PRIOHEADERS;idx++) {
		if(!__lwp_queue_isempty(&queue->queues.priority[idx])) {
			ret	 = (lwp_cntrl*)queue->queues.priority[idx].first;
			goto dequeue;
		}
	}

#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_dequeuepriority(%p,sync_state = %d)\n",ret,queue->sync_state);
#endif
	switch(queue->sync_state) {
		case LWP_THREADQ_SYNCHRONIZED:
		case LWP_THREADQ_SATISFIED:
			_CPU_ISR_Restore(level);
			return NULL;
		case LWP_THREADQ_NOTHINGHAPPEND:
		case LWP_THREADQ_TIMEOUT:
			queue->sync_state = LWP_THREADQ_SATISFIED;
			_CPU_ISR_Restore(level);
			return _thr_executing;
	}

dequeue:
#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_dequeuepriority(%p,dequeue)\n",ret);
#endif
	newfirstnode = ret->wait.block2n.first;
	newfirstthr = (lwp_cntrl*)newfirstnode;
	next_node = ret->object.node.next;
	prev_node = ret->object.node.prev;
	if(!__lwp_queue_isempty(&ret->wait.block2n)) {
		last_node = ret->wait.block2n.last;
		newsecnode = newfirstnode->next;
		prev_node->next = newfirstnode;
		next_node->prev = newfirstnode;
		newfirstnode->next = next_node;
		newfirstnode->prev = prev_node;

		if(!__lwp_queue_onenode(&ret->wait.block2n)) {
			newsecnode->prev = __lwp_queue_head(&newfirstthr->wait.block2n);
			newfirstthr->wait.block2n.first = newsecnode;
			newfirstthr->wait.block2n.last = last_node;
			last_node->next = __lwp_queue_tail(&newfirstthr->wait.block2n);
		}
	} else {
		prev_node->next = next_node;
		next_node->prev = prev_node;
	}

	if(!__lwp_wd_isactive(&ret->timer)) {
		_CPU_ISR_Restore(level);
		__lwp_thread_unblock(ret);
	} else {
		__lwp_wd_deactivate(&ret->timer);
		_CPU_ISR_Restore(level);
		__lwp_wd_remove_ticks(&ret->timer);
		__lwp_thread_unblock(ret);
	}
	return ret;
}

void __lwp_threadqueue_init(lwp_thrqueue *queue,u32 mode,u32 state,u32 timeout_state)
{
	u32 index;

	queue->state = state;
	queue->mode = mode;
	queue->timeout_state = timeout_state;
	queue->sync_state = LWP_THREADQ_SYNCHRONIZED;
#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_init(%p,%08x,%d,%d)\n",queue,state,timeout_state,mode);
#endif
	switch(mode) {
		case LWP_THREADQ_MODEFIFO:
			__lwp_queue_init_empty(&queue->queues.fifo);
			break;
		case LWP_THREADQ_MODEPRIORITY:
			for(index=0;index<LWP_THREADQ_NUM_PRIOHEADERS;index++)
				__lwp_queue_init_empty(&queue->queues.priority[index]);
			break;
	}
}

lwp_cntrl* __lwp_threadqueue_first(lwp_thrqueue *queue)
{
	lwp_cntrl *ret;

	switch(queue->mode) {
		case LWP_THREADQ_MODEFIFO:
			ret = __lwp_threadqueue_firstfifo(queue);
			break;
		case LWP_THREADQ_MODEPRIORITY:
			ret = __lwp_threadqueue_firstpriority(queue);
			break;
		default:
			ret = NULL;
			break;
	}

	return ret;
}

void __lwp_threadqueue_enqueue(lwp_thrqueue *queue,u64 timeout)
{
	lwp_cntrl *thethread;

	thethread = _thr_executing;
	__lwp_thread_setstate(thethread,queue->state);

	if(timeout) {
		__lwp_wd_initialize(&thethread->timer,__lwp_threadqueue_timeout,thethread->object.id,thethread);
		__lwp_wd_insert_ticks(&thethread->timer,timeout);
	}

#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_enqueue(%p,%p,%d)\n",queue,thethread,queue->mode);
#endif
	switch(queue->mode) {
		case LWP_THREADQ_MODEFIFO:
			__lwp_threadqueue_enqueuefifo(queue,thethread,timeout);
			break;
		case LWP_THREADQ_MODEPRIORITY:
			__lwp_threadqueue_enqueuepriority(queue,thethread,timeout);
			break;
	}
}

lwp_cntrl* __lwp_threadqueue_dequeue(lwp_thrqueue *queue)
{
	lwp_cntrl *ret = NULL;

#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_dequeue(%p,%p,%d,%d)\n",queue,_thr_executing,queue->mode,queue->sync_state);
#endif
	switch(queue->mode) {
		case LWP_THREADQ_MODEFIFO:
			ret = __lwp_threadqueue_dequeuefifo(queue);
			break;
		case LWP_THREADQ_MODEPRIORITY:
			ret = __lwp_threadqueue_dequeuepriority(queue);
			break;
		default:
			ret = NULL;
			break;
	}
#ifdef _LWPTHRQ_DEBUG
	printf("__lwp_threadqueue_dequeue(%p,%p,%d,%d)\n",queue,ret,queue->mode,queue->sync_state);
#endif
	return ret;
}

void __lwp_threadqueue_flush(lwp_thrqueue *queue,u32 status)
{
	lwp_cntrl *thethread;
	while((thethread=__lwp_threadqueue_dequeue(queue))) {
		thethread->wait.ret_code = status;
	}
}

void __lwp_threadqueue_extract(lwp_thrqueue *queue,lwp_cntrl *thethread)
{
	switch(queue->mode) {
		case LWP_THREADQ_MODEFIFO:
			__lwp_threadqueue_extractfifo(queue,thethread);
			break;
		case LWP_THREADQ_MODEPRIORITY:
			__lwp_threadqueue_extractpriority(queue,thethread);
			break;
	}

}

void __lwp_threadqueue_extractfifo(lwp_thrqueue *queue,lwp_cntrl *thethread)
{
	u32 level;

	_CPU_ISR_Disable(level);
	if(!__lwp_statewaitthreadqueue(thethread->cur_state)) {
		_CPU_ISR_Restore(level);
		return;
	}

	__lwp_queue_extractI(&thethread->object.node);
	if(!__lwp_wd_isactive(&thethread->timer)) {
		_CPU_ISR_Restore(level);
	} else {
		__lwp_wd_deactivate(&thethread->timer);
		_CPU_ISR_Restore(level);
		__lwp_wd_remove_ticks(&thethread->timer);
	}
	__lwp_thread_unblock(thethread);
}

void __lwp_threadqueue_extractpriority(lwp_thrqueue *queue,lwp_cntrl *thethread)
{
	u32 level;
	lwp_cntrl *first;
	lwp_node *curr,*next,*prev,*new_first,*new_sec,*last;

	curr = (lwp_node*)thethread;

	_CPU_ISR_Disable(level);
	if(__lwp_statewaitthreadqueue(thethread->cur_state)) {
		next = curr->next;
		prev = curr->prev;

		if(!__lwp_queue_isempty(&thethread->wait.block2n)) {
			new_first = thethread->wait.block2n.first;
			first = (lwp_cntrl*)new_first;
			last = thethread->wait.block2n.last;
			new_sec = new_first->next;

			prev->next = new_first;
			next->prev = new_first;
			new_first->next = next;
			new_first->prev = prev;

			if(!__lwp_queue_onenode(&thethread->wait.block2n)) {
				new_sec->prev = __lwp_queue_head(&first->wait.block2n);
				first->wait.block2n.first = new_sec;
				first->wait.block2n.last = last;
				last->next = __lwp_queue_tail(&first->wait.block2n);
			}
		} else {
			prev->next = next;
			next->prev = prev;
		}
		if(!__lwp_wd_isactive(&thethread->timer)) {
			_CPU_ISR_Restore(level);
			__lwp_thread_unblock(thethread);
		} else {
			__lwp_wd_deactivate(&thethread->timer);
			_CPU_ISR_Restore(level);
			__lwp_wd_remove_ticks(&thethread->timer);
			__lwp_thread_unblock(thethread);
		}
	} else
		_CPU_ISR_Restore(level);
}

u32 __lwp_threadqueue_extractproxy(lwp_cntrl *thethread)
{
	u32 state;

	state = thethread->cur_state;
	if(__lwp_statewaitthreadqueue(state)) {
		__lwp_threadqueue_extract(thethread->wait.queue,thethread);
		return TRUE;
	}
	return FALSE;
}

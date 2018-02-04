#include <stdlib.h>
#include "asm.h"
#include "lwp_messages.h"
#include "lwp_wkspace.h"

void __lwpmq_msg_insert(mq_cntrl *mqueue,mq_buffercntrl *msg,u32 type)
{
	++mqueue->num_pendingmsgs;
	msg->prio = type;

#ifdef _LWPMQ_DEBUG
	printf("__lwpmq_msg_insert(%p,%p,%d)\n",mqueue,msg,type);
#endif

	switch(type) {
		case LWP_MQ_SEND_REQUEST:
			__lwpmq_msg_append(mqueue,msg);
			break;
		case LWP_MQ_SEND_URGENT:
			__lwpmq_msg_prepend(mqueue,msg);
			break;
		default:
		{
			mq_buffercntrl *tmsg;
			lwp_node *node;
			lwp_queue *header;

			header = &mqueue->pending_msgs;
			node = header->first;
			while(!__lwp_queue_istail(header,node)) {
				tmsg = (mq_buffercntrl*)node;
				if(tmsg->prio<=msg->prio) {
					node = node->next;
					continue;
				}
				break;
			}
			__lwp_queue_insert(node->prev,&msg->node);
		}
		break;
	}

	if(mqueue->num_pendingmsgs==1 && mqueue->notify_handler)
		mqueue->notify_handler(mqueue->notify_arg);
}

u32 __lwpmq_initialize(mq_cntrl *mqueue,mq_attr *attrs,u32 max_pendingmsgs,u32 max_msgsize)
{
	u32 alloc_msgsize;
	u32 buffering_req;

#ifdef _LWPMQ_DEBUG
	printf("__lwpmq_initialize(%p,%p,%d,%d)\n",mqueue,attrs,max_pendingmsgs,max_msgsize);
#endif
	mqueue->max_pendingmsgs = max_pendingmsgs;
	mqueue->num_pendingmsgs = 0;
	mqueue->max_msgsize = max_msgsize;
	__lwpmq_set_notify(mqueue,NULL,NULL);

	alloc_msgsize = max_msgsize;
	if(alloc_msgsize&(sizeof(u32)-1))
		alloc_msgsize = (alloc_msgsize+sizeof(u32))&~(sizeof(u32)-1);

	buffering_req = max_pendingmsgs*(alloc_msgsize+sizeof(mq_buffercntrl));
	mqueue->msq_buffers = (mq_buffer*)__lwp_wkspace_allocate(buffering_req);

	if(!mqueue->msq_buffers) return 0;

	__lwp_queue_initialize(&mqueue->inactive_msgs,mqueue->msq_buffers,max_pendingmsgs,(alloc_msgsize+sizeof(mq_buffercntrl)));
	__lwp_queue_init_empty(&mqueue->pending_msgs);
	__lwp_threadqueue_init(&mqueue->wait_queue,__lwpmq_is_priority(attrs)?LWP_THREADQ_MODEPRIORITY:LWP_THREADQ_MODEFIFO,LWP_STATES_WAITING_FOR_MESSAGE,LWP_MQ_STATUS_TIMEOUT);

	return 1;
}

u32 __lwpmq_seize(mq_cntrl *mqueue,u32 id,void *buffer,u32 *size,u32 wait,u64 timeout)
{
	u32 level;
	mq_buffercntrl *msg;
	lwp_cntrl *exec,*thread;

	exec = _thr_executing;
	exec->wait.ret_code = LWP_MQ_STATUS_SUCCESSFUL;
#ifdef _LWPMQ_DEBUG
	printf("__lwpmq_seize(%p,%d,%p,%p,%d,%d)\n",mqueue,id,buffer,size,wait,mqueue->num_pendingmsgs);
#endif

	_CPU_ISR_Disable(level);
	if(mqueue->num_pendingmsgs!=0) {
		--mqueue->num_pendingmsgs;
		msg = __lwpmq_get_pendingmsg(mqueue);
		_CPU_ISR_Restore(level);

		*size = msg->contents.size;
		exec->wait.cnt = msg->prio;
		__lwpmq_buffer_copy(buffer,msg->contents.buffer,*size);

		thread = __lwp_threadqueue_dequeue(&mqueue->wait_queue);
		if(!thread) {
			__lwpmq_free_msg(mqueue,msg);
			return LWP_MQ_STATUS_SUCCESSFUL;
		}

		msg->prio = thread->wait.cnt;
		msg->contents.size = (u32)thread->wait.ret_arg_1;
		__lwpmq_buffer_copy(msg->contents.buffer,thread->wait.ret_arg,msg->contents.size);

		__lwpmq_msg_insert(mqueue,msg,msg->prio);
		return LWP_MQ_STATUS_SUCCESSFUL;
	}

	if(!wait) {
		_CPU_ISR_Restore(level);
		exec->wait.ret_code = LWP_MQ_STATUS_UNSATISFIED_NOWAIT;
		return LWP_MQ_STATUS_UNSATISFIED_NOWAIT;
	}

	__lwp_threadqueue_csenter(&mqueue->wait_queue);
	exec->wait.queue = &mqueue->wait_queue;
	exec->wait.id = id;
	exec->wait.ret_arg = (void*)buffer;
	exec->wait.ret_arg_1 = (void*)size;
	_CPU_ISR_Restore(level);

	__lwp_threadqueue_enqueue(&mqueue->wait_queue,timeout);
	return LWP_MQ_STATUS_SUCCESSFUL;
}

u32 __lwpmq_submit(mq_cntrl *mqueue,u32 id,void *buffer,u32 size,u32 type,u32 wait,u64 timeout)
{
	u32 level;
	lwp_cntrl *thread;
	mq_buffercntrl *msg;

#ifdef _LWPMQ_DEBUG
	printf("__lwpmq_submit(%p,%p,%d,%d,%d,%d)\n",mqueue,buffer,size,id,type,wait);
#endif
	if(size>mqueue->max_msgsize)
		return LWP_MQ_STATUS_INVALID_SIZE;

	if(mqueue->num_pendingmsgs==0) {
		thread = __lwp_threadqueue_dequeue(&mqueue->wait_queue);
		if(thread) {
			__lwpmq_buffer_copy(thread->wait.ret_arg,buffer,size);
			*(u32*)thread->wait.ret_arg_1 = size;
			thread->wait.cnt = type;
			return LWP_MQ_STATUS_SUCCESSFUL;
		}
	}

	if(mqueue->num_pendingmsgs<mqueue->max_pendingmsgs) {
		msg = __lwpmq_allocate_msg(mqueue);
		if(!msg) return LWP_MQ_STATUS_UNSATISFIED;

		__lwpmq_buffer_copy(msg->contents.buffer,buffer,size);
		msg->contents.size = size;
		msg->prio = type;
		__lwpmq_msg_insert(mqueue,msg,type);
		return LWP_MQ_STATUS_SUCCESSFUL;
	}

	if(!wait) return LWP_MQ_STATUS_TOO_MANY;
	if(__lwp_isr_in_progress()) return LWP_MQ_STATUS_UNSATISFIED;

	{
		lwp_cntrl *exec = _thr_executing;

		_CPU_ISR_Disable(level);
		__lwp_threadqueue_csenter(&mqueue->wait_queue);
		exec->wait.queue = &mqueue->wait_queue;
		exec->wait.id = id;
		exec->wait.ret_arg = (void*)buffer;
		exec->wait.ret_arg_1 = (void*)size;
		exec->wait.cnt = type;
		_CPU_ISR_Restore(level);

		__lwp_threadqueue_enqueue(&mqueue->wait_queue,timeout);
	}
	return LWP_MQ_STATUS_UNSATISFIED_WAIT;
}

u32 __lwpmq_broadcast(mq_cntrl *mqueue,void *buffer,u32 size,u32 id,u32 *count)
{
	lwp_cntrl *thread;
	u32 num_broadcast;
	lwp_waitinfo *waitp;
	u32 rsize;
#ifdef _LWPMQ_DEBUG
	printf("__lwpmq_broadcast(%p,%p,%d,%d,%p)\n",mqueue,buffer,size,id,count);
#endif
	if(mqueue->num_pendingmsgs!=0) {
		*count = 0;
		return LWP_MQ_STATUS_SUCCESSFUL;
	}

	num_broadcast = 0;
	while((thread=__lwp_threadqueue_dequeue(&mqueue->wait_queue))) {
		waitp = &thread->wait;
		++num_broadcast;

		rsize = size;
		if(size>mqueue->max_msgsize)
			rsize = mqueue->max_msgsize;

		__lwpmq_buffer_copy(waitp->ret_arg,buffer,rsize);
		*(u32*)waitp->ret_arg_1 = size;
	}
	*count = num_broadcast;
	return LWP_MQ_STATUS_SUCCESSFUL;
}

void __lwpmq_close(mq_cntrl *mqueue,u32 status)
{
	__lwp_threadqueue_flush(&mqueue->wait_queue,status);
	__lwpmq_flush_support(mqueue);
	__lwp_wkspace_free(mqueue->msq_buffers);
}

u32 __lwpmq_flush(mq_cntrl *mqueue)
{
	if(mqueue->num_pendingmsgs!=0)
		return __lwpmq_flush_support(mqueue);
	else
		return 0;
}

u32 __lwpmq_flush_support(mq_cntrl *mqueue)
{
	u32 level;
	lwp_node *inactive;
	lwp_node *mqueue_first;
	lwp_node *mqueue_last;
	u32 cnt;

	_CPU_ISR_Disable(level);

	inactive = mqueue->inactive_msgs.first;
	mqueue_first = mqueue->pending_msgs.first;
	mqueue_last = mqueue->pending_msgs.last;

	mqueue->inactive_msgs.first = mqueue_first;
	mqueue_last->next = inactive;
	inactive->prev = mqueue_last;
	mqueue_first->prev = __lwp_queue_head(&mqueue->inactive_msgs);

	__lwp_queue_init_empty(&mqueue->pending_msgs);

	cnt = mqueue->num_pendingmsgs;
	mqueue->num_pendingmsgs = 0;

	_CPU_ISR_Restore(level);
	return cnt;
}

void __lwpmq_flush_waitthreads(mq_cntrl *mqueue)
{
	__lwp_threadqueue_flush(&mqueue->wait_queue,LWP_MQ_STATUS_UNSATISFIED_NOWAIT);
}

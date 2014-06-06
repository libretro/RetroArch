#ifndef __MESSAGE_INL__
#define __MESSAGE_INL__

static __inline__ void __lwpmq_set_notify(mq_cntrl *mqueue,mq_notifyhandler handler,void *arg)
{
	mqueue->notify_handler = handler;
	mqueue->notify_arg = arg;
}

static __inline__ u32 __lwpmq_is_priority(mq_attr *attr)
{
	return (attr->mode==LWP_MQ_PRIORITY);
}

static __inline__ mq_buffercntrl* __lwpmq_allocate_msg(mq_cntrl *mqueue)
{
	return (mq_buffercntrl*)__lwp_queue_get(&mqueue->inactive_msgs);
}

static __inline__ void __lwpmq_free_msg(mq_cntrl *mqueue,mq_buffercntrl *msg)
{
	__lwp_queue_append(&mqueue->inactive_msgs,&msg->node);
}

static __inline__ void __lwpmq_msg_append(mq_cntrl *mqueue,mq_buffercntrl *msg)
{
#ifdef _LWPMQ_DEBUG
	printf("__lwpmq_msq_append(%p,%p,%p)\n",mqueue,&mqueue->inactive_msgs,msg);
#endif
	__lwp_queue_append(&mqueue->pending_msgs,&msg->node);
}

static __inline__ void __lwpmq_msg_prepend(mq_cntrl *mqueue,mq_buffercntrl *msg)
{
#ifdef _LWPMQ_DEBUG
	printf("__lwpmq_msq_prepend(%p,%p,%p)\n",mqueue,&mqueue->inactive_msgs,msg);
#endif
	__lwp_queue_prepend(&mqueue->pending_msgs,&msg->node);
}

static __inline__ u32 __lwpmq_send(mq_cntrl *mqueue,u32 id,void *buffer,u32 size,u32 wait,u32 timeout)
{
	return __lwpmq_submit(mqueue,id,buffer,size,LWP_MQ_SEND_REQUEST,wait,timeout);
}

static __inline__ u32 __lwpmq_urgent(mq_cntrl *mqueue,void *buffer,u32 size,u32 id,u32 wait,u32 timeout)
{
	return __lwpmq_submit(mqueue,id,buffer,size,LWP_MQ_SEND_URGENT,wait,timeout);
}

static __inline__ mq_buffercntrl* __lwpmq_get_pendingmsg(mq_cntrl *mqueue)
{
	return (mq_buffercntrl*)__lwp_queue_getI(&mqueue->pending_msgs);
}

static __inline__ void __lwpmq_buffer_copy(void *dest,const void *src,u32 size)
{
	if(size==sizeof(u32)) *(u32*)dest = *(u32*)src;
	else memcpy(dest,src,size);
}

#endif

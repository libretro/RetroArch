#ifndef __LWP_QUEUE_INL__
#define __LWP_QUEUE_INL__

static __inline__ lwp_node* __lwp_queue_head(lwp_queue *queue)
{
	return (lwp_node*)queue;
}

static __inline__ lwp_node* __lwp_queue_tail(lwp_queue *queue)
{
	return (lwp_node*)&queue->perm_null;
}

static __inline__ u32 __lwp_queue_istail(lwp_queue *queue,lwp_node *node)
{
	return (node==__lwp_queue_tail(queue));
}

static __inline__ u32 __lwp_queue_ishead(lwp_queue *queue,lwp_node *node)
{
	return (node==__lwp_queue_head(queue));
}

static __inline__ lwp_node* __lwp_queue_firstnodeI(lwp_queue *queue)
{
	lwp_node *ret = queue->first;
	lwp_node *new_first = ret->next;
	queue->first = new_first;
	new_first->prev = __lwp_queue_head(queue);
	return ret;
}

static __inline__ void __lwp_queue_init_empty(lwp_queue *queue)
{
	queue->first = __lwp_queue_tail(queue);
	queue->perm_null = NULL;
	queue->last = __lwp_queue_head(queue);
}

static __inline__ u32 __lwp_queue_isempty(lwp_queue *queue)
{
	return (queue->first==__lwp_queue_tail(queue));
}

static __inline__ u32 __lwp_queue_onenode(lwp_queue *queue)
{
	return (queue->first==queue->last);
}

static __inline__ void __lwp_queue_appendI(lwp_queue *queue,lwp_node *node)
{
	lwp_node *old;
	node->next = __lwp_queue_tail(queue);
	old = queue->last;
	queue->last = node;
	old->next = node;
	node->prev = old;
}

static __inline__ void __lwp_queue_extractI(lwp_node *node)
{
	lwp_node *next = node->next;
	lwp_node *prev = node->prev;
	next->prev = prev;
	prev->next = next;
}

static __inline__ void __lwp_queue_insertI(lwp_node *after,lwp_node *node)
{
	lwp_node *before;

	node->prev = after;
	before = after->next;
	after->next = node;
	node->next = before;
	before->prev = node;
}

static __inline__ void __lwp_queue_prepend(lwp_queue *queue,lwp_node *node)
{
	__lwp_queue_insert(__lwp_queue_head(queue),node);
}

static __inline__ void __lwp_queue_prependI(lwp_queue *queue,lwp_node *node)
{
	__lwp_queue_insertI(__lwp_queue_head(queue),node);
}

static __inline__ lwp_node* __lwp_queue_getI(lwp_queue *queue)
{
	if(!__lwp_queue_isempty(queue))
		return __lwp_queue_firstnodeI(queue);
   return NULL;
}

#endif

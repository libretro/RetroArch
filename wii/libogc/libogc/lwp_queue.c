#include <stdlib.h>
#include "asm.h"
#include "processor.h"
#include "lwp_queue.h"

void __lwp_queue_initialize(lwp_queue *queue,void *start_addr,u32 num_nodes,u32 node_size)
{
	u32 count;
	lwp_node *curr;
	lwp_node *next;

#ifdef _LWPQ_DEBUG
	printf("__lwp_queue_initialize(%p,%p,%d,%d)\n",queue,start_addr,num_nodes,node_size);
#endif
	count = num_nodes;
	curr = __lwp_queue_head(queue);
	queue->perm_null = NULL;
	next = (lwp_node*)start_addr;

	while(count--) {
		curr->next = next;
		next->prev = curr;
		curr = next;
		next = (lwp_node*)(((void*)next)+node_size);
	}
	curr->next = __lwp_queue_tail(queue);
	queue->last = curr;
}

lwp_node* __lwp_queue_get(lwp_queue *queue)
{
	u32 level;
	lwp_node *ret = NULL;

	_CPU_ISR_Disable(level);
	if(!__lwp_queue_isempty(queue)) {
		ret	 = __lwp_queue_firstnodeI(queue);
	}
	_CPU_ISR_Restore(level);
	return ret;
}

void __lwp_queue_append(lwp_queue *queue,lwp_node *node)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_queue_appendI(queue,node);
	_CPU_ISR_Restore(level);
}

void __lwp_queue_extract(lwp_node *node)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_queue_extractI(node);
	_CPU_ISR_Restore(level);
}

void __lwp_queue_insert(lwp_node *after,lwp_node *node)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_queue_insertI(after,node);
	_CPU_ISR_Restore(level);
}

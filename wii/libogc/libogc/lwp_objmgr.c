#include <processor.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lwp_threads.h>
#include <lwp_wkspace.h>
#include <lwp_config.h>

#include "lwp_objmgr.h"

static u32 _lwp_objmgr_memsize = 0;
static lwp_obj *null_local_table = NULL;

u32 __lwp_objmgr_memsize()
{
	return _lwp_objmgr_memsize;
}

void __lwp_objmgr_initinfo(lwp_objinfo *info,u32 max_nodes,u32 node_size)
{
	u32 idx,i,size;
	lwp_obj *object;
	lwp_queue inactives;
	void **local_table;

	info->min_id = 0;
	info->max_id = 0;
	info->inactives_cnt = 0;
	info->node_size = node_size;
	info->max_nodes = max_nodes;
	info->obj_blocks = NULL;
	info->local_table = &null_local_table;

	__lwp_queue_init_empty(&info->inactives);

	size = ((info->max_nodes*sizeof(lwp_obj*))+(info->max_nodes*info->node_size));
	local_table = (void**)__lwp_wkspace_allocate(info->max_nodes*sizeof(lwp_obj*));
	if(!local_table) return;

	info->local_table = (lwp_obj**)local_table;
	for(i=0;i<info->max_nodes;i++) {
		local_table[i] = NULL;
	}

	info->obj_blocks = __lwp_wkspace_allocate(info->max_nodes*info->node_size);
	if(!info->obj_blocks) {
		__lwp_wkspace_free(local_table);
		return;
	}

	__lwp_queue_initialize(&inactives,info->obj_blocks,info->max_nodes,info->node_size);

	idx = info->min_id;
	while((object=(lwp_obj*)__lwp_queue_get(&inactives))!=NULL) {
		object->id = idx;
		object->information = NULL;
		__lwp_queue_append(&info->inactives,&object->node);
		idx++;
	}

	info->max_id += info->max_nodes;
	info->inactives_cnt += info->max_nodes;
	_lwp_objmgr_memsize += size;
}

lwp_obj* __lwp_objmgr_getisrdisable(lwp_objinfo *info,u32 id,u32 *p_level)
{
	u32 level;
	lwp_obj *object = NULL;

	_CPU_ISR_Disable(level);
	if(info->max_id>=id) {
		if((object=info->local_table[id])!=NULL) {
			*p_level = level;
			return object;
		}
	}
	_CPU_ISR_Restore(level);
	return NULL;
}

lwp_obj* __lwp_objmgr_getnoprotection(lwp_objinfo *info,u32 id)
{
	lwp_obj *object = NULL;

	if(info->max_id>=id) {
		if((object=info->local_table[id])!=NULL) return object;
	}
	return NULL;
}

lwp_obj* __lwp_objmgr_get(lwp_objinfo *info,u32 id)
{
	lwp_obj *object = NULL;

	if(info->max_id>=id) {
		__lwp_thread_dispatchdisable();
		if((object=info->local_table[id])!=NULL) return object;
		__lwp_thread_dispatchenable();
	}
	return NULL;
}

lwp_obj* __lwp_objmgr_allocate(lwp_objinfo *info)
{
	u32 level;
	lwp_obj* object;

	_CPU_ISR_Disable(level);
	 object = (lwp_obj*)__lwp_queue_getI(&info->inactives);
	 if(object) {
		 object->information = info;
		 info->inactives_cnt--;
	 }
	_CPU_ISR_Restore(level);

	return object;
}

void __lwp_objmgr_free(lwp_objinfo *info,lwp_obj *object)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_queue_appendI(&info->inactives,&object->node);
	object->information	= NULL;
	info->inactives_cnt++;
	_CPU_ISR_Restore(level);
}

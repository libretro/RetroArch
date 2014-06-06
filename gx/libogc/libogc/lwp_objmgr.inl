#ifndef __LWP_OBJMGR_INL__
#define __LWP_OBJMGR_INL__

static __inline__ void __lwp_objmgr_setlocal(lwp_objinfo *info,u32 idx,lwp_obj *object)
{
	if(idx<info->max_nodes) info->local_table[idx] = object;
}

static __inline__ void __lwp_objmgr_open(lwp_objinfo *info,lwp_obj *object)
{
	__lwp_objmgr_setlocal(info,object->id,object);
}

static __inline__ void __lwp_objmgr_close(lwp_objinfo *info,lwp_obj *object)
{
	__lwp_objmgr_setlocal(info,object->id,NULL);
}

#endif

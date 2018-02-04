#ifndef __LWP_OBJMGR_H__
#define __LWP_OBJMGR_H__

#include <gctypes.h>
#include "lwp_queue.h"

#define LWP_OBJMASKTYPE(type)		((type)<<16)
#define LWP_OBJMASKID(id)			((id)&0xffff)
#define LWP_OBJTYPE(id)				((id)>>16)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lwp_objinfo lwp_objinfo;

typedef struct _lwp_obj {
	lwp_node node;
	s32 id;
	lwp_objinfo *information;
} lwp_obj;

struct _lwp_objinfo {
	u32 min_id;
	u32 max_id;
	u32 max_nodes;
	u32 node_size;
	lwp_obj **local_table;
	void *obj_blocks;
	lwp_queue inactives;
	u32 inactives_cnt;
};

void __lwp_objmgr_initinfo(lwp_objinfo *info,u32 max_nodes,u32 node_size);
void __lwp_objmgr_free(lwp_objinfo *info,lwp_obj *object);
lwp_obj* __lwp_objmgr_allocate(lwp_objinfo *info);
lwp_obj* __lwp_objmgr_get(lwp_objinfo *info,u32 id);
lwp_obj* __lwp_objmgr_getisrdisable(lwp_objinfo *info,u32 id,u32 *p_level);
lwp_obj* __lwp_objmgr_getnoprotection(lwp_objinfo *info,u32 id);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_objmgr.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif

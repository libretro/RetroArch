#ifndef __LWP_QUEUE_H__
#define __LWP_QUEUE_H__

#include <gctypes.h>

//#define _LWPQ_DEBUG

#ifdef _LWPQ_DEBUG
extern int printk(const char *fmt,...);
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lwpnode {
	struct _lwpnode *next;
	struct _lwpnode *prev;
} lwp_node;

typedef struct _lwpqueue {
	lwp_node *first;
	lwp_node *perm_null;
	lwp_node *last;
} lwp_queue;

void __lwp_queue_initialize(lwp_queue *,void *,u32,u32);
lwp_node* __lwp_queue_get(lwp_queue *);
void __lwp_queue_append(lwp_queue *,lwp_node *);
void __lwp_queue_extract(lwp_node *);
void __lwp_queue_insert(lwp_node *,lwp_node *);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_queue.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif

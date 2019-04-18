/*-------------------------------------------------------------

semaphore.c -- Thread subsystem IV

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <stdlib.h>
#include <errno.h>
#include <asm.h>
#include "lwp_sema.h"
#include "lwp_objmgr.h"
#include "lwp_config.h"
#include "semaphore.h"

#define LWP_OBJTYPE_SEM				4

#define LWP_CHECK_SEM(hndl)		\
{									\
	if(((hndl)==LWP_SEM_NULL) || (LWP_OBJTYPE(hndl)!=LWP_OBJTYPE_SEM))	\
		return NULL;				\
}

typedef struct _sema_st
{
	lwp_obj object;
	lwp_sema sema;
} sema_st;

lwp_objinfo _lwp_sema_objects;

void __lwp_sema_init()
{
	__lwp_objmgr_initinfo(&_lwp_sema_objects,LWP_MAX_SEMAS,sizeof(sema_st));
}

static __inline__ sema_st* __lwp_sema_open(sem_t sem)
{
	LWP_CHECK_SEM(sem);
	return (sema_st*)__lwp_objmgr_get(&_lwp_sema_objects,LWP_OBJMASKID(sem));
}

static __inline__ void __lwp_sema_free(sema_st *sema)
{
	__lwp_objmgr_close(&_lwp_sema_objects,&sema->object);
	__lwp_objmgr_free(&_lwp_sema_objects,&sema->object);
}

static sema_st* __lwp_sema_allocate()
{
	sema_st *sema;

	__lwp_thread_dispatchdisable();
	sema = (sema_st*)__lwp_objmgr_allocate(&_lwp_sema_objects);
	if(sema) {
		__lwp_objmgr_open(&_lwp_sema_objects,&sema->object);
		return sema;
	}
	__lwp_thread_dispatchenable();
	return NULL;
}

s32 LWP_SemInit(sem_t *sem,u32 start,u32 max)
{
	lwp_semattr attr;
	sema_st *ret;

	if(!sem) return -1;

	ret = __lwp_sema_allocate();
	if(!ret) return -1;

	attr.max_cnt = max;
	attr.mode = LWP_SEMA_MODEFIFO;
	__lwp_sema_initialize(&ret->sema,&attr,start);

	*sem = (sem_t)(LWP_OBJMASKTYPE(LWP_OBJTYPE_SEM)|LWP_OBJMASKID(ret->object.id));
	__lwp_thread_dispatchenable();
	return 0;
}

s32 LWP_SemWait(sem_t sem)
{
	sema_st *lwp_sem = __lwp_sema_open(sem);
	if(!lwp_sem) return -1;

	__lwp_sema_seize(&lwp_sem->sema,lwp_sem->object.id,TRUE,LWP_THREADQ_NOTIMEOUT);
	__lwp_thread_dispatchenable();

	switch(_thr_executing->wait.ret_code) {
		case LWP_SEMA_SUCCESSFUL:
			break;
		case LWP_SEMA_UNSATISFIED_NOWAIT:
			return EAGAIN;
		case LWP_SEMA_DELETED:
			return EAGAIN;
		case LWP_SEMA_TIMEOUT:
			return ETIMEDOUT;

	}
	return 0;
}

s32 LWP_SemPost(sem_t sem)
{
	sema_st *lwp_sem = __lwp_sema_open(sem);
	if(!lwp_sem) return -1;

	__lwp_sema_surrender(&lwp_sem->sema,lwp_sem->object.id);
	__lwp_thread_dispatchenable();

	return 0;
}

s32 LWP_SemDestroy(sem_t sem)
{
	sema_st *lwp_sem = __lwp_sema_open(sem);
	if(!lwp_sem) return -1;

	__lwp_sema_flush(&lwp_sem->sema,-1);
	__lwp_thread_dispatchenable();

	__lwp_sema_free(lwp_sem);
	return 0;
}

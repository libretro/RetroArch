/*-------------------------------------------------------------

mutex.c -- Thread subsystem III

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
#include "asm.h"
#include "lwp_mutex.h"
#include "lwp_objmgr.h"
#include "lwp_config.h"
#include "mutex.h"

#define LWP_OBJTYPE_MUTEX			3

#define LWP_CHECK_MUTEX(hndl)		\
{									\
	if(((hndl)==LWP_MUTEX_NULL) || (LWP_OBJTYPE(hndl)!=LWP_OBJTYPE_MUTEX))	\
		return NULL;				\
}

typedef struct _mutex_st
{
	lwp_obj object;
	lwp_mutex mutex;
} mutex_st;

lwp_objinfo _lwp_mutex_objects;

static s32 __lwp_mutex_locksupp(mutex_t lock,u32 timeout,u8 block)
{
	u32 level;
	mutex_st *p;

	if(lock==LWP_MUTEX_NULL || LWP_OBJTYPE(lock)!=LWP_OBJTYPE_MUTEX) return -1;

	p = (mutex_st*)__lwp_objmgr_getisrdisable(&_lwp_mutex_objects,LWP_OBJMASKID(lock),&level);
	if(!p) return -1;

	__lwp_mutex_seize(&p->mutex,p->object.id,block,timeout,level);
	return _thr_executing->wait.ret_code;
}

void __lwp_mutex_init()
{
	__lwp_objmgr_initinfo(&_lwp_mutex_objects,LWP_MAX_MUTEXES,sizeof(mutex_st));
}

static __inline__ mutex_st* __lwp_mutex_open(mutex_t lock)
{
	LWP_CHECK_MUTEX(lock);
	return (mutex_st*)__lwp_objmgr_get(&_lwp_mutex_objects,LWP_OBJMASKID(lock));
}

static __inline__ void __lwp_mutex_free(mutex_st *lock)
{
	__lwp_objmgr_close(&_lwp_mutex_objects,&lock->object);
	__lwp_objmgr_free(&_lwp_mutex_objects,&lock->object);
}

static mutex_st* __lwp_mutex_allocate()
{
	mutex_st *lock;

	__lwp_thread_dispatchdisable();
	lock = (mutex_st*)__lwp_objmgr_allocate(&_lwp_mutex_objects);
	if(lock) {
		__lwp_objmgr_open(&_lwp_mutex_objects,&lock->object);
		return lock;
	}
	__lwp_thread_dispatchunnest();
	return NULL;
}

s32 LWP_MutexInit(mutex_t *mutex,bool use_recursive)
{
	lwp_mutex_attr attr;
	mutex_st *ret;

	if(!mutex) return -1;

	ret = __lwp_mutex_allocate();
	if(!ret) return -1;

	attr.mode = LWP_MUTEX_FIFO;
	attr.nest_behavior = use_recursive?LWP_MUTEX_NEST_ACQUIRE:LWP_MUTEX_NEST_ERROR;
	attr.onlyownerrelease = TRUE;
	attr.prioceil = 1; //__lwp_priotocore(LWP_PRIO_MAX-1);
	__lwp_mutex_initialize(&ret->mutex,&attr,LWP_MUTEX_UNLOCKED);

	*mutex = (mutex_t)(LWP_OBJMASKTYPE(LWP_OBJTYPE_MUTEX)|LWP_OBJMASKID(ret->object.id));
	__lwp_thread_dispatchunnest();
	return 0;
}

s32 LWP_MutexDestroy(mutex_t mutex)
{
	mutex_st *p = __lwp_mutex_open(mutex);
	if(!p) return 0;

	if(__lwp_mutex_locked(&p->mutex)) {
		__lwp_thread_dispatchenable();
		return EBUSY;
	}
	__lwp_mutex_flush(&p->mutex,EINVAL);
	__lwp_thread_dispatchenable();

	__lwp_mutex_free(p);
	return 0;
}

s32 LWP_MutexLock(mutex_t mutex)
{
	return __lwp_mutex_locksupp(mutex,LWP_THREADQ_NOTIMEOUT,TRUE);
}

s32 LWP_MutexTryLock(mutex_t mutex)
{
	return __lwp_mutex_locksupp(mutex,LWP_THREADQ_NOTIMEOUT,FALSE);
}

s32 LWP_MutexUnlock(mutex_t mutex)
{
	u32 ret;
	mutex_st *lock;

	lock = __lwp_mutex_open(mutex);
	if(!lock) return -1;

	ret = __lwp_mutex_surrender(&lock->mutex);
	__lwp_thread_dispatchenable();

	return ret;
}

#include <_ansi.h>
#include <_syslist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>

#include "asm.h"
#include "processor.h"
#include "mutex.h"

int __libogc_lock_init(int *lock,int recursive)
{
	s32 ret;
	mutex_t retlck = LWP_MUTEX_NULL;

	if(!lock) return -1;

	*lock = 0;
	ret = LWP_MutexInit(&retlck,(recursive?TRUE:FALSE));
	if(ret==0) *lock = (int)retlck;

	return ret;
}

int __libogc_lock_close(int *lock)
{
	s32 ret;
	mutex_t plock;

	if(!lock || *lock==0) return -1;

	plock = (mutex_t)*lock;
	ret = LWP_MutexDestroy(plock);
	if(ret==0) *lock = 0;

	return ret;
}

int __libogc_lock_acquire(int *lock)
{
	mutex_t plock;

	if(!lock || *lock==0) return -1;

	plock = (mutex_t)*lock;
	return LWP_MutexLock(plock);
}

int __libogc_lock_release(int *lock)
{
	mutex_t plock;

	if(!lock || *lock==0) return -1;

	plock = (mutex_t)*lock;
	return LWP_MutexUnlock(plock);
}

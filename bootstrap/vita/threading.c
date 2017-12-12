// This provides support for __getreent() as well as implementation of our thread-related wrappers

#include <reent.h>
#include <stdio.h>
#include <string.h>

#include <vitasdk/utils.h>
#include <psp2/kernel/threadmgr.h>

#define MAX_THREADS 256

typedef struct reent_for_thread {
	int thread_id;
	int needs_reclaim;
	void *tls_data_ext;
	void *pthread_data_ext;
	struct _reent reent;
} reent_for_thread;

static reent_for_thread reent_list[MAX_THREADS];
static int _newlib_reent_mutex;
static struct _reent _newlib_global_reent;

#define TLS_REENT_THID_PTR(thid)	sceKernelGetThreadTLSAddr(thid, 0x88)
#define TLS_REENT_PTR				sceKernelGetTLSAddr(0x88)

#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

int __vita_delete_thread_reent(int thid)
{
	struct reent_for_thread *for_thread;

	// We only need to cleanup if reent is allocated, i.e. if it's on our TLS
	// We also don't need to clean up the global reent
	struct _reent **on_tls = NULL;

	if (thid == 0)
		on_tls = TLS_REENT_PTR;
	else
		on_tls = TLS_REENT_THID_PTR(thid);

	if (!*on_tls || *on_tls == &_newlib_global_reent)
		return 0;

	for_thread = list_entry(*on_tls, struct reent_for_thread, reent);

	// Remove from TLS
	*on_tls = 0;

	// Set thread id to zero, which means the reent is free
	for_thread->thread_id = 0;

	// We can't reclaim it here, will be done later in __getreent
	for_thread->needs_reclaim = 1;

	return 1;
}

int vitasdk_delete_thread_reent(int thid)
{
	int res = 0;
	// Lock the list because we'll be modifying it
	sceKernelLockMutex(_newlib_reent_mutex, 1, NULL);

	res = __vita_delete_thread_reent(thid);

	sceKernelUnlockMutex(_newlib_reent_mutex, 1);
	return res;
}

int _exit_thread_common(int exit_status, int (*exit_func)(int)) {
	int res = 0;
	int ret = 0;
	int thid = sceKernelGetThreadId();

	// Lock the list because we'll be modifying it
	sceKernelLockMutex(_newlib_reent_mutex, 1, NULL);

	res = __vita_delete_thread_reent(0);

	ret = exit_func(exit_status);

	if (res)
	{
		struct _reent **on_tls = TLS_REENT_PTR;
		struct reent_for_thread *for_thread = list_entry(*on_tls, struct reent_for_thread, reent);

		for_thread->thread_id = thid;

		// And put it back on TLS
		*on_tls = &for_thread->reent;
	}

	sceKernelUnlockMutex(_newlib_reent_mutex, 1);
	return ret;
}

int vita_exit_thread(int exit_status) {
	return _exit_thread_common(exit_status, sceKernelExitThread);
}

int vita_exit_delete_thread(int exit_status) {
	return _exit_thread_common(exit_status, sceKernelExitDeleteThread);
}

static inline void __vita_clean_reent(void)
{
	int i;
	SceKernelThreadInfo info;

	for (i = 0; i < MAX_THREADS; ++i)
	{
		info.size = sizeof(SceKernelThreadInfo);

		if (sceKernelGetThreadInfo(reent_list[i].thread_id, &info) < 0)
		{
			reent_list[i].thread_id = 0;
			reent_list[i].needs_reclaim = 1;
		}
	}
}

static inline struct reent_for_thread *__vita_allocate_reent(void)
{
	int i;
	struct reent_for_thread *free_reent = 0;

	for (i = 0; i < MAX_THREADS; ++i)
		if (reent_list[i].thread_id == 0) {
			free_reent = &reent_list[i];
			break;
		}

	return free_reent;
}

struct _reent *__getreent_for_thread(int thid) {
	struct reent_for_thread *free_reent = 0;
	struct _reent *returned_reent = 0;

	// A pointer to our reent should be on the TLS
	struct _reent **on_tls = NULL;

	if (thid == 0)
		on_tls = TLS_REENT_PTR;
	else
		on_tls = TLS_REENT_THID_PTR(thid);

	if (*on_tls) {
		return *on_tls;
	}

  	sceKernelLockMutex(_newlib_reent_mutex, 1, 0);

	// If it's not on the TLS this means the thread doesn't have a reent allocated yet
	// We allocate one and put a pointer to it on the TLS
	free_reent = __vita_allocate_reent();

	if (!free_reent) {
		// clean any hanging thread references
		__vita_clean_reent();

		free_reent = __vita_allocate_reent();

		if (!free_reent) {
			// we've exhausted all our resources
			__builtin_trap();
		}
	} else {
		// First, check if it needs to be cleaned up (if it came from another thread)
		if (free_reent->needs_reclaim) {
			_reclaim_reent(&free_reent->reent);
			free_reent->needs_reclaim = 0;
		}

		memset(free_reent, 0, sizeof(struct reent_for_thread));

		// Set it up
		if(thid==0){
			thid = sceKernelGetThreadId();
		}
		free_reent->thread_id = thid;
		_REENT_INIT_PTR(&free_reent->reent);
		returned_reent = &free_reent->reent;
	}

	// Put it on TLS for faster access time
	*on_tls = returned_reent;

	sceKernelUnlockMutex(_newlib_reent_mutex, 1);
	return returned_reent;
}

struct _reent *__getreent(void) {
	return  __getreent_for_thread(0);
}

void *vitasdk_get_tls_data(SceUID thid)
{
	struct reent_for_thread *for_thread;
	struct _reent *reent = __getreent_for_thread(thid);

	for_thread = list_entry(reent, struct reent_for_thread, reent);
	return &for_thread->tls_data_ext;
}

void *vitasdk_get_pthread_data(SceUID thid)
{
	struct reent_for_thread *for_thread;
	struct _reent *reent = __getreent_for_thread(thid);

	for_thread = list_entry(reent, struct reent_for_thread, reent);
	return &for_thread->pthread_data_ext;
}

// Called from _start to set up the main thread reentrancy structure
void _init_vita_reent(void) {
	memset(reent_list, 0, sizeof(reent_list));
	_newlib_reent_mutex = sceKernelCreateMutex("reent list access mutex", 0, 0, 0);
	reent_list[0].thread_id = sceKernelGetThreadId();
	_REENT_INIT_PTR(&reent_list[0].reent);
	*(struct _reent **)(TLS_REENT_PTR) = &reent_list[0].reent;
	_REENT_INIT_PTR(&_newlib_global_reent);
}

void _free_vita_reent(void) {
	sceKernelDeleteMutex(_newlib_reent_mutex);
}

#include <stdlib.h>
#include <string.h>
#include <sys/reent.h>
#include "sys_state.h"
#include "lwp_threads.h"

int libc_reentrant;
struct _reent libc_globl_reent;

extern void _wrapup_reent(struct _reent *);
extern void _reclaim_reent(struct _reent *);

int __libc_create_hook(lwp_cntrl *curr_thr,lwp_cntrl *create_thr)
{
	create_thr->libc_reent = NULL;
	return 1;
}

int __libc_start_hook(lwp_cntrl *curr_thr,lwp_cntrl *start_thr)
{
	struct _reent *ptr;

	ptr = (struct _reent*)calloc(1,sizeof(struct _reent));
	if(!ptr) abort();

	_REENT_INIT_PTR((ptr));

	start_thr->libc_reent = ptr;
	return 1;
}

int __libc_delete_hook(lwp_cntrl *curr_thr, lwp_cntrl *delete_thr)
{
	struct _reent *ptr;

	if(curr_thr==delete_thr)
		ptr = _REENT;
	else
		ptr = (struct _reent*)delete_thr->libc_reent;

	if(ptr && ptr!=&libc_globl_reent) {
		_reclaim_reent(ptr);
		free(ptr);
	}
	delete_thr->libc_reent = 0;

	if(curr_thr==delete_thr) _REENT = 0;

	return 1;
}

void __libc_init(int reentrant)
{
	libc_globl_reent = (struct _reent)_REENT_INIT((libc_globl_reent));
	_REENT = &libc_globl_reent;

	if(reentrant) {
		__lwp_thread_setlibcreent((void*)&_REENT);
		libc_reentrant = reentrant;
	}
}

void __libc_wrapup()
{
	if(!__sys_state_up(__sys_state_get())) return;
	if(_REENT!=&libc_globl_reent) {
		_wrapup_reent(&libc_globl_reent);
		_REENT = &libc_globl_reent;
	}
}

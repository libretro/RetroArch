#ifndef __LWP_INL__
#define __LWP_INL__

static __inline__ u32 __lwp_thread_isexec(lwp_cntrl *thethread)
{
	return (thethread==_thr_executing);
}

static __inline__ u32 __lwp_thread_isheir(lwp_cntrl *thethread)
{
	return (thethread==_thr_heir);
}

static __inline__ void __lwp_thread_calcheir()
{
	_thr_heir = (lwp_cntrl*)_lwp_thr_ready[__lwp_priomap_highest()].first;
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_calcheir(%p)\n",_thr_heir);
#endif
}

static __inline__ u32 __lwp_thread_isallocatedfp(lwp_cntrl *thethread)
{
	return (thethread==_thr_allocated_fp);
}

static __inline__ void __lwp_thread_deallocatefp()
{
	_thr_allocated_fp = NULL;
}

static __inline__ void __lwp_thread_dispatchinitialize()
{
	_thread_dispatch_disable_level = 1;
}

static __inline__ void __lwp_thread_dispatchenable()
{
	if((--_thread_dispatch_disable_level)==0)
		__thread_dispatch();
}

static __inline__ void __lwp_thread_dispatchdisable()
{
	++_thread_dispatch_disable_level;
}

static __inline__ void __lwp_thread_dispatchunnest()
{
	--_thread_dispatch_disable_level;
}

static __inline__ void __lwp_thread_unblock(lwp_cntrl *thethread)
{
	__lwp_thread_clearstate(thethread,LWP_STATES_BLOCKED);
}

static __inline__ void** __lwp_thread_getlibcreent()
{
	return __lwp_thr_libc_reent;
}

static __inline__ void __lwp_thread_setlibcreent(void **libc_reent)
{
	__lwp_thr_libc_reent = libc_reent;
}

static __inline__ bool __lwp_thread_isswitchwant()
{

	return _context_switch_want;
}

static __inline__ bool __lwp_thread_isdispatchenabled()
{
	return (_thread_dispatch_disable_level==0);
}

static __inline__ void __lwp_thread_inittimeslice()
{
	__lwp_wd_initialize(&_lwp_wd_timeslice,__lwp_thread_tickle_timeslice,LWP_TIMESLICE_TIMER_ID,NULL);
}

static __inline__ void __lwp_thread_starttimeslice()
{
	__lwp_wd_insert_ticks(&_lwp_wd_timeslice,millisecs_to_ticks(1));
}

static __inline__ void __lwp_thread_stoptimeslice()
{
	__lwp_wd_remove_ticks(&_lwp_wd_timeslice);
}
#endif

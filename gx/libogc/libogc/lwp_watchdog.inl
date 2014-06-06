#ifndef __LWP_WATCHDOG_INL__
#define __LWP_WATCHDOG_INL__

static __inline__ void __lwp_wd_initialize(wd_cntrl *wd,wd_service_routine routine,u32 id,void *usr_data)
{
	wd->state = LWP_WD_INACTIVE;
	wd->id = id;
	wd->routine = routine;
	wd->usr_data = usr_data;
}

static __inline__ wd_cntrl* __lwp_wd_first(lwp_queue *queue)
{
	return (wd_cntrl*)queue->first;
}

static __inline__ wd_cntrl* __lwp_wd_last(lwp_queue *queue)
{
	return (wd_cntrl*)queue->last;
}

static __inline__ wd_cntrl* __lwp_wd_next(wd_cntrl *wd)
{
	return (wd_cntrl*)wd->node.next;
}

static __inline__ wd_cntrl* __lwp_wd_prev(wd_cntrl *wd)
{
	return (wd_cntrl*)wd->node.prev;
}

static __inline__ void __lwp_wd_activate(wd_cntrl *wd)
{
	wd->state = LWP_WD_ACTIVE;
}

static __inline__ void __lwp_wd_deactivate(wd_cntrl *wd)
{
	wd->state = LWP_WD_REMOVE;
}

static __inline__ u32 __lwp_wd_isactive(wd_cntrl *wd)
{
	return (wd->state==LWP_WD_ACTIVE);
}

static __inline__ u64 __lwp_wd_calc_ticks(const struct timespec *time)
{
	u64 ticks;

	ticks = secs_to_ticks(time->tv_sec);
	ticks += nanosecs_to_ticks(time->tv_nsec);

	return ticks;
}

static __inline__ void __lwp_wd_tickle_ticks()
{
	__lwp_wd_tickle(&_wd_ticks_queue);
}

static __inline__ void __lwp_wd_insert_ticks(wd_cntrl *wd,s64 interval)
{
	wd->start = gettime();
	wd->fire = (wd->start+LWP_WD_ABS(interval));
	__lwp_wd_insert(&_wd_ticks_queue,wd);
}

static __inline__ void __lwp_wd_adjust_ticks(u32 dir,s64 interval)
{
	__lwp_wd_adjust(&_wd_ticks_queue,dir,interval);
}

static __inline__ void __lwp_wd_remove_ticks(wd_cntrl *wd)
{
	__lwp_wd_remove(&_wd_ticks_queue,wd);
}

static __inline__ void __lwp_wd_reset(wd_cntrl *wd)
{
	__lwp_wd_remove(&_wd_ticks_queue,wd);
	__lwp_wd_insert(&_wd_ticks_queue,wd);
}
#endif

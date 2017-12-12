#include <stdlib.h>
#include <limits.h>
#include "asm.h"
#include "lwp_threads.h"
#include "lwp_watchdog.h"

//#define _LWPWD_DEBUG

#ifdef _LWPWD_DEBUG
#include <stdio.h>
#endif

vu32 _wd_sync_level;
vu32 _wd_sync_count;
u32 _wd_ticks_since_boot;

lwp_queue _wd_ticks_queue;

static void __lwp_wd_settimer(wd_cntrl *wd)
{
	u64 now;
	s64 diff;
	union uulc {
		u64 ull;
		u32 ul[2];
	} v;

	now = gettime();
	v.ull = diff = diff_ticks(now,wd->fire);
#ifdef _LWPWD_DEBUG
	printf("__lwp_wd_settimer(%p,%llu,%lld)\n",wd,wd->fire,diff);
#endif
	if(diff<=0) {
#ifdef _LWPWD_DEBUG
		printf(" __lwp_wd_settimer(0): %lld<=0\n",diff);
#endif
		wd->fire = 0;
		mtdec(0);
	} else if(diff<0x0000000080000000LL) {
#ifdef _LWPWD_DEBUG
		printf("__lwp_wd_settimer(%d): %lld<0x0000000080000000LL\n",v.ul[1],diff);
#endif
		mtdec(v.ul[1]);
	} else {
#ifdef _LWPWD_DEBUG
		printf("__lwp_wd_settimer(0x7fffffff)\n");
#endif
		mtdec(0x7fffffff);
	}
}

void __lwp_watchdog_init()
{
	_wd_sync_level = 0;
	_wd_sync_count = 0;
	_wd_ticks_since_boot = 0;

	__lwp_queue_init_empty(&_wd_ticks_queue);
}

void __lwp_wd_insert(lwp_queue *header,wd_cntrl *wd)
{
	u32 level;
	u64 fire;
	u32 isr_nest_level;
	wd_cntrl *after;
#ifdef _LWPWD_DEBUG
	printf("__lwp_wd_insert(%p,%llu,%llu)\n",wd,wd->start,wd->fire);
#endif
	isr_nest_level = __lwp_isr_in_progress();
	wd->state = LWP_WD_INSERTED;

	_wd_sync_count++;
restart:
	_CPU_ISR_Disable(level);
	fire = wd->fire;
	for(after=__lwp_wd_first(header);;after=__lwp_wd_next(after)) {
		if(fire==0 || !__lwp_wd_next(after)) break;
		if(fire<after->fire) break;

		_CPU_ISR_Flash(level);
		if(wd->state!=LWP_WD_INSERTED) goto exit_insert;
		if(_wd_sync_level>isr_nest_level) {
			_wd_sync_level = isr_nest_level;
			_CPU_ISR_Restore(level);
			goto restart;
		}
	}
	__lwp_wd_activate(wd);
	wd->fire = fire;
	__lwp_queue_insertI(after->node.prev,&wd->node);
	if(__lwp_wd_first(header)==wd) __lwp_wd_settimer(wd);

exit_insert:
	_wd_sync_level = isr_nest_level;
	_wd_sync_count--;
	_CPU_ISR_Restore(level);
	return;
}

u32 __lwp_wd_remove(lwp_queue *header,wd_cntrl *wd)
{
	u32 level;
	u32 prev_state;
	wd_cntrl *next;
#ifdef _LWPWD_DEBUG
	printf("__lwp_wd_remove(%p)\n",wd);
#endif
	_CPU_ISR_Disable(level);
	prev_state = wd->state;
	switch(prev_state) {
		case LWP_WD_INACTIVE:
			break;
		case  LWP_WD_INSERTED:
			wd->state = LWP_WD_INACTIVE;
			break;
		case LWP_WD_ACTIVE:
		case LWP_WD_REMOVE:
			wd->state = LWP_WD_INACTIVE;
			next = __lwp_wd_next(wd);
			if(_wd_sync_count) _wd_sync_level = __lwp_isr_in_progress();
			__lwp_queue_extractI(&wd->node);
			if(!__lwp_queue_isempty(header) && __lwp_wd_first(header)==next) __lwp_wd_settimer(next);
			break;
	}
	_CPU_ISR_Restore(level);
	return prev_state;
}

void __lwp_wd_tickle(lwp_queue *queue)
{
	wd_cntrl *wd;
	u64 now;
	s64 diff;

	if(__lwp_queue_isempty(queue)) return;

	wd = __lwp_wd_first(queue);
	now = gettime();
	diff = diff_ticks(now,wd->fire);
#ifdef _LWPWD_DEBUG
	printf("__lwp_wd_tickle(%p,%08x%08x,%08x%08x,%08x%08x,%08x%08x)\n",wd,(u32)(now>>32),(u32)now,(u32)(wd->start>>32),(u32)wd->start,(u32)(wd->fire>>32),(u32)wd->fire,(u32)(diff>>32),(u32)diff);
#endif
	if(diff<=0) {
		do {
			switch(__lwp_wd_remove(queue,wd)) {
				case LWP_WD_ACTIVE:
					wd->routine(wd->usr_data);
					break;
				case LWP_WD_INACTIVE:
					break;
				case LWP_WD_INSERTED:
					break;
				case LWP_WD_REMOVE:
					break;
			}
			wd = __lwp_wd_first(queue);
		} while(!__lwp_queue_isempty(queue) && wd->fire==0);
	} else {
		__lwp_wd_reset(wd);
	}
}

void __lwp_wd_adjust(lwp_queue *queue,u32 dir,s64 interval)
{
	u32 level;
	u64 abs_int;

	_CPU_ISR_Disable(level);
	abs_int = gettime()+LWP_WD_ABS(interval);
	if(!__lwp_queue_isempty(queue)) {
		switch(dir) {
			case LWP_WD_BACKWARD:
				__lwp_wd_first(queue)->fire += LWP_WD_ABS(interval);
				break;
			case LWP_WD_FORWARD:
				while(abs_int) {
					if(abs_int<__lwp_wd_first(queue)->fire) {
						__lwp_wd_first(queue)->fire -= LWP_WD_ABS(interval);
						break;
					} else {
						abs_int -= __lwp_wd_first(queue)->fire;
						__lwp_wd_first(queue)->fire = gettime();
						__lwp_wd_tickle(queue);
						if(__lwp_queue_isempty(queue)) break;
					}
				}
				break;
		}
	}
	_CPU_ISR_Restore(level);
}

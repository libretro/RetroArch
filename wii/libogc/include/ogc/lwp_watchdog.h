#ifndef __LWP_WATCHDOG_H__
#define __LWP_WATCHDOG_H__

#include <gctypes.h>
#include "lwp_queue.h"
#include <time.h>

#if defined(HW_RVL)
	#define TB_BUS_CLOCK				243000000u
	#define TB_CORE_CLOCK				729000000u
#elif defined(HW_DOL)
	#define TB_BUS_CLOCK				162000000u
	#define TB_CORE_CLOCK				486000000u
#endif
#define TB_TIMER_CLOCK				(TB_BUS_CLOCK/4000)			//4th of the bus frequency

#define TB_SECSPERMIN				60
#define TB_MINSPERHR				60
#define TB_MONSPERYR				12
#define TB_DAYSPERYR				365
#define TB_HRSPERDAY				24
#define TB_SECSPERDAY				(TB_SECSPERMIN*TB_MINSPERHR*TB_HRSPERDAY)
#define TB_SECSPERNYR				(365*TB_SECSPERDAY)

#define TB_MSPERSEC					1000
#define TB_USPERSEC					1000000
#define TB_NSPERSEC					1000000000
#define TB_NSPERMS					1000000
#define TB_NSPERUS					1000
#define TB_USPERTICK				10000

#define ticks_to_cycles(ticks)		((((u64)(ticks)*(u64)((TB_CORE_CLOCK*2)/TB_TIMER_CLOCK))/2))
#define ticks_to_secs(ticks)		(((u64)(ticks)/(u64)(TB_TIMER_CLOCK*1000)))
#define ticks_to_millisecs(ticks)	(((u64)(ticks)/(u64)(TB_TIMER_CLOCK)))
#define ticks_to_microsecs(ticks)	((((u64)(ticks)*8)/(u64)(TB_TIMER_CLOCK/125)))
#define ticks_to_nanosecs(ticks)	((((u64)(ticks)*8000)/(u64)(TB_TIMER_CLOCK/125)))

#define tick_microsecs(ticks)		((((u64)(ticks)*8)%(u64)(TB_TIMER_CLOCK/125)))
#define tick_nanosecs(ticks)		((((u64)(ticks)*8000)%(u64)(TB_TIMER_CLOCK/125)))

#define secs_to_ticks(sec)			((u64)(sec)*(TB_TIMER_CLOCK*1000))
#define millisecs_to_ticks(msec)	((u64)(msec)*(TB_TIMER_CLOCK))
#define microsecs_to_ticks(usec)	(((u64)(usec)*(TB_TIMER_CLOCK/125))/8)
#define nanosecs_to_ticks(nsec)		(((u64)(nsec)*(TB_TIMER_CLOCK/125))/8000)

#define diff_ticks(tick0,tick1)		(((u64)(tick1)<(u64)(tick0))?((u64)-1-(u64)(tick0)+(u64)(tick1)):((u64)(tick1)-(u64)(tick0)))

#define LWP_WD_INACTIVE				0
#define LWP_WD_INSERTED				1
#define LWP_WD_ACTIVE				2
#define LWP_WD_REMOVE				3

#define LWP_WD_FORWARD				0
#define LWP_WD_BACKWARD				1

#define LWP_WD_NOTIMEOUT			0

#define LWP_WD_ABS(x)				((s64)(x)>0?(s64)(x):-((s64)(x)))

#ifdef __cplusplus
extern "C" {
#endif

extern vu32 _wd_sync_level;
extern vu32 _wd_sync_count;
extern u32 _wd_ticks_since_boot;

extern lwp_queue _wd_ticks_queue;

extern u32 gettick();
extern u64 gettime();
extern void settime(u64);

u32 diff_sec(u64 start,u64 end);
u32 diff_msec(u64 start,u64 end);
u32 diff_usec(u64 start,u64 end);
u32 diff_nsec(u64 start,u64 end);

typedef void (*wd_service_routine)(void *);

typedef struct _wdcntrl {
	lwp_node node;
	u64 start;
	u32 id;
	u32 state;
	u64 fire;
	wd_service_routine routine;
	void *usr_data;
} wd_cntrl;

void __lwp_watchdog_init();
void __lwp_watchdog_settimer(wd_cntrl *wd);
void __lwp_wd_insert(lwp_queue *header,wd_cntrl *wd);
u32 __lwp_wd_remove(lwp_queue *header,wd_cntrl *wd);
void __lwp_wd_tickle(lwp_queue *queue);
void __lwp_wd_adjust(lwp_queue *queue,u32 dir,s64 interval);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_watchdog.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif

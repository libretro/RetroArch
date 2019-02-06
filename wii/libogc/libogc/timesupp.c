#include <_ansi.h>
#include <_syslist.h>

#include "asm.h"
#include "processor.h"
#include "lwp.h"
#include "lwp_threadq.h"
#include "timesupp.h"
#include "exi.h"
#include "system.h"
#include "conf.h"

#include <stdio.h>
#include <sys/time.h>

/* time variables */
static u32 exi_wait_inited = 0;
static lwpq_t time_exi_wait;

extern u32 __SYS_GetRTC(u32 *gctime);
extern syssram* __SYS_LockSram();
extern u32 __SYS_UnlockSram(u32 write);

u32 _DEFUN(gettick,(),
	_NOARGS)

{
	u32 result;
	__asm__ __volatile__ (
		"mftb	%0\n"
		: "=r" (result)
	);
	return result;
}

u64 _DEFUN(gettime,(),
						  _NOARGS)
{
	u32 tmp;
	union uulc {
		u64 ull;
		u32 ul[2];
	} v;

	__asm__ __volatile__(
		"1:	mftbu	%0\n\
		    mftb	%1\n\
		    mftbu	%2\n\
			cmpw	%0,%2\n\
			bne		1b\n"
		: "=r" (v.ul[0]), "=r" (v.ul[1]), "=&r" (tmp)
	);
	return v.ull;
}

void _DEFUN(settime,(t),
			u64 t)
{
	u32 tmp;
	union uulc {
		u64 ull;
		u32 ul[2];
	} v;

	v.ull = t;
	__asm__ __volatile__ (
		"li		%0,0\n\
		 mttbl  %0\n\
		 mttbu  %1\n\
		 mttbl  %2\n"
		 : "=&r" (tmp)
		 : "r" (v.ul[0]), "r" (v.ul[1])
	);
}

u32 diff_sec(u64 start,u64 end)
{
	u64 diff;

	diff = diff_ticks(start,end);
	return ticks_to_secs(diff);
}

u32 diff_msec(u64 start,u64 end)
{
	u64 diff = diff_ticks(start,end);
	return ticks_to_millisecs(diff);
}

u32 diff_usec(u64 start,u64 end)
{
	u64 diff = diff_ticks(start,end);
	return ticks_to_microsecs(diff);
}

u32 diff_nsec(u64 start,u64 end)
{
	u64 diff = diff_ticks(start,end);
	return ticks_to_nanosecs(diff);
}

void __timesystem_init()
{
	if(!exi_wait_inited) {
		exi_wait_inited = 1;
		LWP_InitQueue(&time_exi_wait);
	}
}

void timespec_subtract(const struct timespec *tp_start,const struct timespec *tp_end,struct timespec *result)
{
	struct timespec start_st = *tp_start;
	struct timespec *start = &start_st;
	u32 nsecpersec = TB_NSPERSEC;

	if(tp_end->tv_nsec<start->tv_nsec) {
		int secs = (start->tv_nsec - tp_end->tv_nsec)/nsecpersec+1;
		start->tv_nsec -= nsecpersec * secs;
		start->tv_sec += secs;
	}
	if((tp_end->tv_nsec - start->tv_nsec)>nsecpersec) {
		int secs = (start->tv_nsec - tp_end->tv_nsec)/nsecpersec;
		start->tv_nsec += nsecpersec * secs;
		start->tv_sec -= secs;
	}

	result->tv_sec = (tp_end->tv_sec - start->tv_sec);
	result->tv_nsec = (tp_end->tv_nsec - start->tv_nsec);
}

unsigned long long timespec_to_ticks(const struct timespec *tp)
{
	return __lwp_wd_calc_ticks(tp);
}

int clock_gettime(struct timespec *tp)
{
	u32 gctime;
#if defined(HW_RVL)
	u32 wii_bias = 0;
#endif

	if(!tp) return -1;

	if(!__SYS_GetRTC(&gctime)) return -1;

#if defined(HW_DOL)
	syssram* sram = __SYS_LockSram();
	gctime += sram->counter_bias;
	__SYS_UnlockSram(0);
#else
	if(CONF_GetCounterBias(&wii_bias)>=0) gctime += wii_bias;
#endif
	gctime += 946684800;

	tp->tv_sec = gctime;
	tp->tv_nsec = ticks_to_nanosecs(gettick())%1000000000;

	return 0;
}

// this function spins till timeout is reached
void _DEFUN(udelay,(us),
			unsigned us)
{
	unsigned long long start, end;
	start = gettime();
	while (1)
	{
		end = gettime();
		if (diff_usec(start,end) >= us)
			break;
	}
}

unsigned int _DEFUN(nanosleep,(tb),
           struct timespec *tb)
{
	u64 timeout;

	__lwp_thread_dispatchdisable();

	timeout = __lwp_wd_calc_ticks(tb);
	__lwp_thread_setstate(_thr_executing,LWP_STATES_DELAYING|LWP_STATES_INTERRUPTIBLE_BY_SIGNAL);
	__lwp_wd_initialize(&_thr_executing->timer,__lwp_thread_delayended,_thr_executing->object.id,_thr_executing);
	__lwp_wd_insert_ticks(&_thr_executing->timer,timeout);

	__lwp_thread_dispatchenable();
	return TB_SUCCESSFUL;
}

static u32 __getrtc(u32 *gctime)
{
	u32 ret;
	u32 cmd;
	u32 time;

	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
		return 0;
	}

	ret = 0;
	time = 0;
	cmd = 0x20000000;
	if(EXI_Imm(EXI_CHANNEL_0,&cmd,4,EXI_WRITE,NULL)==0) ret |= 0x01;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
	if(EXI_Imm(EXI_CHANNEL_0,&time,4,EXI_READ,NULL)==0) ret |= 0x04;
	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x08;
	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x10;

	*gctime = time;
	if(ret) return 0;

	return 1;
}

static s32 __time_exi_unlock(s32 chn,s32 dev)
{
	LWP_ThreadBroadcast(time_exi_wait);
	return 1;
}

static void __time_exi_wait()
{
	u32 ret;

	do {
		if((ret=EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,__time_exi_unlock))==1) break;
		LWP_ThreadSleep(time_exi_wait);
	}while(ret==0);
}

static u32 __getRTC(u32 *gctime)
{
	u32 cnt,time1,time2;

	__time_exi_wait();

	cnt = 0;

	while(cnt<16) {
		if(__getrtc(&time1)==0
			|| __getrtc(&time2)==0) {
			EXI_Unlock(EXI_CHANNEL_0);
			break;
		}
		if(time1==time2) {
			*gctime = time1;
			EXI_Unlock(EXI_CHANNEL_0);
			return 1;
		}
		cnt++;
	}
	return 0;
}

time_t _DEFUN(time,(timer),
			  time_t *timer)
{
	time_t gctime = 0;
#if defined(HW_RVL)
	u32 wii_bias = 0;
#endif

	if(__getRTC((u32*)&gctime)==0) return (time_t)0;

#if defined(HW_DOL)
	syssram* sram = __SYS_LockSram();
	gctime += sram->counter_bias;
	__SYS_UnlockSram(0);
#else
	if(CONF_GetCounterBias(&wii_bias)>=0) gctime += wii_bias;
#endif

	gctime += 946684800;

	if(timer) *timer = gctime;
	return gctime;
}

unsigned int _DEFUN(sleep,(s),
		   unsigned int s)
{
	struct timespec tb;

	tb.tv_sec = s;
	tb.tv_nsec = 0;
	return nanosleep(&tb);
}

unsigned int _DEFUN(usleep,(us),
           unsigned int us)
{
	struct timespec tb;
	u32 sec = us/TB_USPERSEC;
	u32 rem = us - (sec*TB_USPERSEC);

	tb.tv_sec = sec;
	tb.tv_nsec = rem*TB_NSPERUS;
	return nanosleep(&tb);
}

clock_t clock(void) {
	return -1;
}

int __libogc_gettod_r(struct _reent *ptr, struct timeval *tp, struct timezone *tz) {

	if (tp != NULL) {
		tp->tv_sec = time(NULL);
		tp->tv_usec = ticks_to_microsecs(gettick())%1000000;
	}
	if (tz != NULL) {
		tz->tz_minuteswest = 0;
		tz->tz_dsttime = 0;

	}
	return 0;
}

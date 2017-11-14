#ifndef __SYS_STATE_INL__
#define __SYS_STATE_INL__

static __inline__ void __sys_state_init()
{
	_sys_state_curr = SYS_STATE_BEFORE_INIT;
}

static __inline__ void __sys_state_set(u32 sys_state)
{
	_sys_state_curr = sys_state;
}

static __inline__ u32 __sys_state_get()
{
	return _sys_state_curr;
}

static __inline__ u32 __sys_state_beforeinit(u32 statecode)
{
	return (statecode==SYS_STATE_BEFORE_INIT);
}

static __inline__ u32 __sys_state_beforemultitasking(u32 statecode)
{
	return (statecode==SYS_STATE_BEFORE_MT);
}

static __inline__ u32 __sys_state_beginmultitasking(u32 statecode)
{
	return (statecode==SYS_STATE_BEGIN_MT);
}

static __inline__ u32 __sys_state_up(u32 statecode)
{
	return (statecode==SYS_STATE_UP);
}

static __inline__ u32 __sys_state_failed(u32 statecode)
{
	return (statecode==SYS_STATE_FAILED);
}

#endif

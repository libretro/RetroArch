#ifndef __LWP_STATES_INL__
#define __LWP_STATES_INL__

static __inline__ u32 __lwp_setstate(u32 curr_state,u32 stateset)
{
	return (curr_state|stateset);
}

static __inline__ u32 __lwp_clearstate(u32 curr_state,u32 stateclear)
{
	return (curr_state&~stateclear);
}

static __inline__ u32 __lwp_stateready(u32 curr_state)
{
	return (curr_state==LWP_STATES_READY);
}

static __inline__ u32 __lwp_stateonlydormant(u32 curr_state)
{
	return (curr_state==LWP_STATES_DORMANT);
}

static __inline__ u32 __lwp_statedormant(u32 curr_state)
{
	return (curr_state&LWP_STATES_DORMANT);
}

static __inline__ u32 __lwp_statesuspended(u32 curr_state)
{
	return (curr_state&LWP_STATES_SUSPENDED);
}

static __inline__ u32 __lwp_statetransient(u32 curr_state)
{
	return (curr_state&LWP_STATES_TRANSIENT);
}

static __inline__ u32 __lwp_statedelaying(u32 curr_state)
{
	return (curr_state&LWP_STATES_DELAYING);
}

static __inline__ u32 __lwp_statewaitbuffer(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_BUFFER);
}

static __inline__ u32 __lwp_statewaitsegment(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_SEGMENT);
}

static __inline__ u32 __lwp_statewaitmessage(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_MESSAGE);
}

static __inline__ u32 __lwp_statewaitevent(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_EVENT);
}

static __inline__ u32 __lwp_statewaitmutex(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_MUTEX);
}

static __inline__ u32 __lwp_statewaitsemaphore(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_SEMAPHORE);
}

static __inline__ u32 __lwp_statewaittime(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_TIME);
}

static __inline__ u32 __lwp_statewaitrpcreply(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_RPCREPLAY);
}

static __inline__ u32 __lwp_statewaitperiod(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_PERIOD);
}

static __inline__ u32 __lwp_statewaitlocallyblocked(u32 curr_state)
{
	return (curr_state&LWP_STATES_LOCALLY_BLOCKED);
}

static __inline__ u32 __lwp_statewaitthreadqueue(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_ON_THREADQ);
}

static __inline__ u32 __lwp_stateblocked(u32 curr_state)
{
	return (curr_state&LWP_STATES_BLOCKED);
}

static __inline__ u32 __lwp_statesset(u32 curr_state,u32 mask)
{
	return ((curr_state&mask)!=LWP_STATES_READY);
}

#endif

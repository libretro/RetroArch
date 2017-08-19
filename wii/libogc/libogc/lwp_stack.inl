#ifndef __LWP_STACK_INL__
#define __LWP_STACK_INL__

static __inline__ u32 __lwp_stack_isenough(u32 size)
{
	return (size>=CPU_MINIMUM_STACK_SIZE);
}

static __inline__ u32 __lwp_stack_adjust(u32 size)
{
	return (size+CPU_STACK_ALIGNMENT);
}

#endif

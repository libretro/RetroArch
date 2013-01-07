#ifndef __LWP_PRIORITY_INL__
#define __LWP_PRIORITY_INL__

static __inline__ void __lwp_priomap_init(prio_cntrl *theprio,u32 prio)
{
	u32 major,minor,mask;
	
	major = prio/16;
	minor = prio%16;
	
	theprio->minor = &_prio_bitmap[major];
	
	mask = 0x80000000>>major;
	theprio->ready_major = mask;
	theprio->block_major = ~mask;
	
	mask = 0x80000000>>minor;
	theprio->ready_minor = mask;
	theprio->block_minor = ~mask;
#ifdef _LWPPRIO_DEBUG
	printf("__lwp_priomap_init(%p,%d,%p,%d,%d,%d,%d)\n",theprio,prio,theprio->minor,theprio->ready_major,theprio->ready_minor,theprio->block_major,theprio->block_minor);
#endif
}

static __inline__ void __lwp_priomap_addto(prio_cntrl *theprio)
{
	*theprio->minor |= theprio->ready_minor;
	_prio_major_bitmap |= theprio->ready_major;
}

static __inline__ void __lwp_priomap_removefrom(prio_cntrl *theprio)
{
	*theprio->minor &= theprio->block_minor;
	if(*theprio->minor==0)
		_prio_major_bitmap &= theprio->block_major;
}

static __inline__ u32 __lwp_priomap_highest()
{
	u32 major,minor;
	major = cntlzw(_prio_major_bitmap);
	minor = cntlzw(_prio_bitmap[major]);
#ifdef _LWPPRIO_DEBUG
	printf("__lwp_priomap_highest(%d)\n",((major<<4)+minor));
#endif
	return ((major<<4)+minor);
}

#endif

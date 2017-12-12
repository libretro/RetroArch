#include <gctypes.h>

vu32 _prio_major_bitmap;
u32 _prio_bitmap[16] __attribute__((aligned(32)));

void __lwp_priority_init()
{
	u32 index;

	_prio_major_bitmap = 0;
	for(index=0;index<16;index++)
		_prio_bitmap[index] = 0;

}

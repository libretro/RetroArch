#include <stdlib.h>
#include <string.h>

#include "asm.h"
#include "processor.h"

#include "bt.h"
#include "btmemb.h"

void btmemb_init(struct memb_blks *blk)
{
	MEMSET(blk->mem,0,(MEM_ALIGN_SIZE(blk->size)+sizeof(u32))*blk->num);
}

void* btmemb_alloc(struct memb_blks *blk)
{
	s32 i;
	u32 *ptr;
	u32 level;
	void *p;

	_CPU_ISR_Disable(level);
	ptr = (u32*)blk->mem;
	for(i=0;i<blk->num;i++) {
		if(*ptr==0) {
			++(*ptr);
			p = (ptr+1);
			_CPU_ISR_Restore(level);
			return p;
		}
		ptr = (u32*)((u8*)ptr+(MEM_ALIGN_SIZE(blk->size)+sizeof(u32)));
	}
	_CPU_ISR_Restore(level);
	return NULL;
}

u8 btmemb_free(struct memb_blks *blk,void *ptr)
{
	u8 ref;
	s32 i;
	u32 level;
	u32 *ptr2,*ptr1;

	_CPU_ISR_Disable(level);
	ptr1 = ptr;
	ptr2 = (u32*)blk->mem;
	for(i=0;i<blk->num;i++) {
		if(ptr2==(ptr1 - 1)) {
			ref = --(*ptr2);
			_CPU_ISR_Restore(level);
			return ref;
		}
		ptr2 = (u32*)((u8*)ptr2+(MEM_ALIGN_SIZE(blk->size)+sizeof(u32)));
	}
	_CPU_ISR_Restore(level);
	return -1;
}

u8 btmemb_ref(struct memb_blks *blk,void *ptr)
{
	u8 ref;
	u32 *pref;
	u32 level;

	_CPU_ISR_Disable(level);
	pref = ptr-sizeof(u32);
	ref = ++(*pref);
	_CPU_ISR_Restore(level);
	return ref;
}

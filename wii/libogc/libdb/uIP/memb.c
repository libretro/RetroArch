#include <stdlib.h>
#include <string.h>

#include "uip.h"
#include "memb.h"

void memb_init(struct memb_blks *blk)
{
	UIP_MEMSET(blk->mem,0,(MEM_ALIGN_SIZE(blk->size)+sizeof(u32))*blk->num);
}

void* memb_alloc(struct memb_blks *blk)
{
	s32 i;
	u32 *ptr;

	ptr = (u32*)blk->mem;
	for(i=0;i<blk->num;i++) {
		if(*ptr==0) {
			++(*ptr);
			return (void*)(ptr+1);
		}
		ptr = (u32*)(u8*)ptr+(MEM_ALIGN_SIZE(blk->size)+sizeof(u32));
	}
	return NULL;
}

u8 memb_free(struct memb_blks *blk,void *ptr)
{
	s32 i;
	u32 *ptr2,*ptr1;

	ptr1 = ptr;
	ptr2 = (u32*)blk->mem;
	for(i=0;i<blk->num;i++) {
		if(ptr2==(ptr1 - 1)) {
			return --(*ptr2);
		}
		ptr2 = (u32*)(u8*)ptr2+(MEM_ALIGN_SIZE(blk->size)+sizeof(u32));
	}
	return -1;
}

u8 memb_ref(struct memb_blks *blk,void *ptr)
{
	u32 *pref = ptr-sizeof(u32);
	return ++(*pref);
}

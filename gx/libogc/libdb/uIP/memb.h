#ifndef __MEMB_H__
#define __MEMB_H__

#include <gctypes.h>

#define MEMB(name,size,num)										\
	static u8 memb_mem_##name[(MEM_ALIGN_SIZE(size)+sizeof(u32))*num];		\
	static struct memb_blks name = {size,num,memb_mem_##name}

struct memb_blks {
	u16 size;
	u16 num;
	u8 *mem;
};

void memb_init(struct memb_blks *blk);
void* memb_alloc(struct memb_blks *blk);
u8 memb_free(struct memb_blks *blk,void *ptr);
u8 memb_ref(struct memb_blks *blk,void *ptr);

#endif

#include <stdlib.h>
#include <string.h>

#include "asm.h"
#include "processor.h"

#include "bt.h"
#include "btmemr.h"

#define MIN_SIZE				12
#define SIZEOF_STRUCT_MEM		(sizeof(struct mem)+(((sizeof(struct mem)%MEM_ALIGNMENT)==0)?0:(4-(sizeof(struct mem)%MEM_ALIGNMENT))))

struct mem {
	u32 next,prev;
	u32 used;
};

static struct mem *ram_free;
static struct mem *ram_end;
static u8 ram_block[sizeof(struct mem)+MEM_SIZE+MEM_ALIGNMENT];

static void plug_holes(struct mem *rmem)
{
	struct mem *nmem;
	struct mem *pmem;

	nmem = (struct mem*)&ram_block[rmem->next];
	if(rmem!=nmem && nmem->used==0 && (u8_t*)nmem!=(u8_t*)ram_end) {
		if(ram_free==nmem) ram_free = rmem;

		rmem->next = nmem->next;
		((struct mem*)&ram_block[nmem->next])->prev = (u8_t*)rmem -	ram_block;
	}

	pmem = (struct mem*)&ram_block[rmem->prev];
	if(pmem!=rmem && pmem->used==0) {
		if(ram_free==rmem) ram_free = pmem;
		pmem->next = rmem->next;
		((struct mem*)&ram_block[rmem->next])->prev = (u8_t*)pmem - ram_block;
	}
}

void btmemr_init()
{
	u32 level;
	struct mem *rmem;

	MEMSET(ram_block,0,MEM_SIZE);

	_CPU_ISR_Disable(level);
	rmem = (struct mem*)ram_block;
	rmem->next = MEM_SIZE;
	rmem->prev = 0;
	rmem->used = 0;

	ram_end = (struct mem*)&ram_block[MEM_SIZE];
	ram_end->used = 1;
	ram_end->prev = MEM_SIZE;
	ram_end->next = MEM_SIZE;

	ram_free = (struct mem*)ram_block;
	_CPU_ISR_Restore(level);
}

void* btmemr_malloc(u32 size)
{
	u32 level;
	u32 ptr,ptr2;
	struct mem *rmem,*rmem2;

	if(size==0) return NULL;

	if(size%MEM_ALIGNMENT) size += MEM_ALIGNMENT - ((size+SIZEOF_STRUCT_MEM)%SIZEOF_STRUCT_MEM);
	if(size>MEM_SIZE) return NULL;

	_CPU_ISR_Disable(level);
	for(ptr = (u8_t*)ram_free - ram_block;ptr<MEM_SIZE;ptr=((struct mem*)&ram_block[ptr])->next) {
		rmem = (struct mem*)&ram_block[ptr];
		if(!rmem->used && rmem->next - (ptr + SIZEOF_STRUCT_MEM)>=size + SIZEOF_STRUCT_MEM) {
			ptr2 = ptr + SIZEOF_STRUCT_MEM + size;
			rmem2 = (struct mem*)&ram_block[ptr2];

			rmem2->prev = ptr;
			rmem2->next = rmem->next;
			rmem->next = ptr2;
			if(rmem->next!=MEM_SIZE) ((struct mem*)&ram_block[rmem2->next])->prev = ptr2;

			rmem2->used = 0;
			rmem->used = 1;

			if(rmem==ram_free) {
				while(ram_free->used && ram_free!=ram_end) ram_free = (struct mem*)&ram_block[ram_free->next];
			}

			_CPU_ISR_Restore(level);
			return (u8_t*)rmem+SIZEOF_STRUCT_MEM;
		}
	}
	_CPU_ISR_Restore(level);
	return NULL;
}

void btmemr_free(void *ptr)
{
	u32 level;
	struct mem *rmem;

	if(ptr==NULL) return;
	if((u8_t*)ptr<(u8_t*)ram_block || (u8_t*)ptr>=(u8_t*)ram_end) return;

	_CPU_ISR_Disable(level);
	rmem = (struct mem*)((u8_t*)ptr - SIZEOF_STRUCT_MEM);
	rmem->used = 0;

	if(rmem<ram_free) ram_free = rmem;

	plug_holes(rmem);
	_CPU_ISR_Restore(level);
}

void* btmemr_realloc(void *ptr,u32 newsize)
{
	u32 level;
	u32 size,ptr1,ptr2;
	struct mem *rmem,*rmem2;

	if(newsize%MEM_ALIGNMENT) newsize += MEM_ALIGNMENT - ((newsize + SIZEOF_STRUCT_MEM)%MEM_ALIGNMENT);
	if(newsize>MEM_SIZE) return NULL;
	if((u8_t*)ptr<(u8_t*)ram_block || (u8_t*)ptr>=(u8_t*)ram_end) {
		ERROR("memr_realloc: illegal memory.\n");
		return ptr;
	}

	_CPU_ISR_Disable(level);
	rmem = (struct mem*)((u8_t*)ptr - SIZEOF_STRUCT_MEM);
	ptr1 = (u8_t*)rmem - ram_block;
	size = rmem->next - ptr1 - SIZEOF_STRUCT_MEM;

	if(newsize+SIZEOF_STRUCT_MEM+MIN_SIZE<size) {
		ptr2 = ptr1 + SIZEOF_STRUCT_MEM + newsize;
		rmem2 = (struct mem*)&ram_block[ptr2];
		rmem2->used = 0;
		rmem2->next = rmem->next;
		rmem2->prev = ptr1;
		rmem->next = ptr2;
		if(rmem2->next!=MEM_SIZE) ((struct mem*)&ram_block[rmem2->next])->prev = ptr2;

		plug_holes(rmem2);
	}
	_CPU_ISR_Restore(level);

	return ptr;
}

void* btmemr_reallocm(void *ptr,u32 newsize)
{
	void *nmem;

	nmem = btmemr_malloc(newsize);
	if(nmem==NULL) return btmemr_realloc(ptr,newsize);

	MEMCPY(nmem,ptr,newsize);
	btmemr_free(ptr);

	return nmem;
}

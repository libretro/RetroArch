#ifndef __LWP_HEAP_INL__
#define __LWP_HEAP_INL__

static __inline__ heap_block* __lwp_heap_head(heap_cntrl *theheap)
{
	return (heap_block*)&theheap->start;
}

static __inline__ heap_block* __lwp_heap_tail(heap_cntrl *heap)
{
	return (heap_block*)&heap->final;
}

static __inline__ heap_block* __lwp_heap_prevblock(heap_block *block)
{
	return (heap_block*)((char*)block - (block->back_flag&~HEAP_BLOCK_USED));
}

static __inline__ heap_block* __lwp_heap_nextblock(heap_block *block)
{
	return (heap_block*)((char*)block + (block->front_flag&~HEAP_BLOCK_USED));
}

static __inline__ heap_block* __lwp_heap_blockat(heap_block *block,u32 offset)
{
	return (heap_block*)((char*)block + offset);
}

static __inline__ heap_block* __lwp_heap_usrblockat(void *ptr)
{
	u32 offset = *(((u32*)ptr)-1);
	return __lwp_heap_blockat(ptr,-offset+-HEAP_BLOCK_USED_OVERHEAD);
}

static __inline__ bool __lwp_heap_prev_blockfree(heap_block *block)
{
	return !(block->back_flag&HEAP_BLOCK_USED);
}

static __inline__ bool __lwp_heap_blockfree(heap_block *block)
{
	return !(block->front_flag&HEAP_BLOCK_USED);
}

static __inline__ bool __lwp_heap_blockused(heap_block *block)
{
	return (block->front_flag&HEAP_BLOCK_USED);
}

static __inline__ u32 __lwp_heap_blocksize(heap_block *block)
{
	return (block->front_flag&~HEAP_BLOCK_USED);
}

static __inline__ void* __lwp_heap_startuser(heap_block *block)
{
	return (void*)&block->next;
}

static __inline__ bool __lwp_heap_blockin(heap_cntrl *heap,heap_block *block)
{
	return ((u32)block>=(u32)heap->start && (u32)block<=(u32)heap->final);
}

static __inline__ bool __lwp_heap_pgsize_valid(u32 pgsize)
{
	return (pgsize!=0 && ((pgsize%PPC_ALIGNMENT)==0));
}

static __inline__ u32 __lwp_heap_buildflag(u32 size,u32 flag)
{
	return (size|flag);
}

#endif

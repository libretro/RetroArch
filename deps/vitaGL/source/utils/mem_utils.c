/* 
 * mem_utils.c:
 * Utilities for memory management
 */

#include "../shared.h"

#define MEM_ALIGNMENT 8 // seems to be enough, set to 16 if something explodes

typedef struct tm_block_s {
	struct tm_block_s *next; // next block in list (either free or allocated)
	int32_t type; // one of vglMemType (VGL_MEM_ALL when unused)
	uintptr_t base; // block start address
	uint32_t offset; // offset for USSE stuff (unused)
	uint32_t size; // block size
} tm_block_t;

static void *mempool_addr[3] = { NULL, NULL, NULL }; // addresses of heap memblocks (VRAM, RAM, PHYCONT RAM)
static SceUID mempool_id[3] = { 0, 0, 0 }; // UIDs of heap memblocks (VRAM, RAM, PHYCONT RAM)
static size_t mempool_size[3] = { 0, 0, 0 }; // sizes of heap memlbocks (VRAM, RAM, PHYCONT RAM)

static int tm_initialized;

static tm_block_t *tm_alloclist; // list of allocated blocks
static tm_block_t *tm_freelist; // list of free blocks

static uint32_t tm_free[VGL_MEM_TYPE_COUNT]; // see enum vglMemType

// heap funcs //

// get new block header
static inline tm_block_t *heap_blk_new(void) {
	return calloc(1, sizeof(tm_block_t));
}

// release block header
static inline void heap_blk_release(tm_block_t *block) {
	free(block);
}

// determine if two blocks can be merged into one
// blocks of different types can't be merged,
// blocks of same type can only be merged if they're next to each other
// in memory and have matching offsets
static inline int heap_blk_mergeable(tm_block_t *a, tm_block_t *b) {
	return a->type == b->type
		&& a->base + a->size == b->base
		&& a->offset + a->size == b->offset;
}

// inserts a block into the free list and merges with neighboring
// free blocks if possible
static void heap_blk_insert_free(tm_block_t *block) {
	tm_block_t *curblk = tm_freelist;
	tm_block_t *prevblk = NULL;
	while (curblk && curblk->base < block->base) {
		prevblk = curblk;
		curblk = curblk->next;
	}

	if (prevblk)
		prevblk->next = block;
	else
		tm_freelist = block;

	block->next = curblk;
	tm_free[block->type] += block->size;
	tm_free[0] += block->size;

	if (curblk && heap_blk_mergeable(block, curblk)) {
		block->size += curblk->size;
		block->next = curblk->next;
		heap_blk_release(curblk);
	}

	if (prevblk && heap_blk_mergeable(prevblk, block)) {
		prevblk->size += block->size;
		prevblk->next = block->next;
		heap_blk_release(block);
	}
}

// allocates a block from the heap
// (removes it from free list and adds to alloc list)
static tm_block_t *heap_blk_alloc(int32_t type, uint32_t size, uint32_t alignment) {
	tm_block_t *curblk = tm_freelist;
	tm_block_t *prevblk = NULL;

	while (curblk) {
		const uint32_t skip = ALIGN(curblk->base, alignment) - curblk->base;

		if (curblk->type == type && skip + size <= curblk->size) {
			tm_block_t *skipblk = NULL;
			tm_block_t *unusedblk = NULL;

			if (skip != 0) {
				skipblk = heap_blk_new();
				if (!skipblk)
					return NULL;
			}

			if (skip + size != curblk->size) {
				unusedblk = heap_blk_new();
				if (!unusedblk) {
					if (skipblk)
						heap_blk_release(skipblk);
					return NULL;
				}
			}

			if (skip != 0) {
				if (prevblk)
					prevblk->next = skipblk;
				else
					tm_freelist = skipblk;

				skipblk->next = curblk;
				skipblk->type = curblk->type;
				skipblk->base = curblk->base;
				skipblk->offset = curblk->offset;
				skipblk->size = skip;

				curblk->base += skip;
				curblk->offset += skip;
				curblk->size -= skip;

				prevblk = skipblk;
			}

			if (size != curblk->size) {
				unusedblk->next = curblk->next;
				curblk->next = unusedblk;
				unusedblk->type = curblk->type;
				unusedblk->base = curblk->base + size;
				unusedblk->offset = curblk->offset + size;
				unusedblk->size = curblk->size - size;
				curblk->size = size;
			}

			if (prevblk)
				prevblk->next = curblk->next;
			else
				tm_freelist = curblk->next;

			curblk->next = tm_alloclist;
			tm_alloclist = curblk;
			tm_free[type] -= size;
			tm_free[0] -= size;
			return curblk;
		}

		prevblk = curblk;
		curblk = curblk->next;
	}

	return NULL;
}

// frees a previously allocated heap block
// (removes from alloc list and inserts into free list)
static void heap_blk_free(uintptr_t base) {
	tm_block_t *curblk = tm_alloclist;
	tm_block_t *prevblk = NULL;

	while (curblk && curblk->base != base) {
		prevblk = curblk;
		curblk = curblk->next;
	}

	if (!curblk)
		return;

	if (prevblk)
		prevblk->next = curblk->next;
	else
		tm_alloclist = curblk->next;

	curblk->next = NULL;

	heap_blk_insert_free(curblk);
}

// initializes heap variables and blockpool
static void heap_init(void) {
	tm_alloclist = NULL;
	tm_freelist = NULL;

	for (int i = 0; i < VGL_MEM_TYPE_COUNT; ++i)
		tm_free[i] = 0;

	tm_initialized = 1;
}

// resets heap state and frees allocated block headers
static void heap_destroy(void) {
	tm_block_t *n;

	tm_block_t *p = tm_alloclist;
	while (p) {
		n = p->next;
		heap_blk_release(p);
		p = n;
	}

	p = tm_freelist;
	while (p) {
		n = p->next;
		heap_blk_release(p);
		p = n;
	}

	tm_initialized = 0;
}

// adds a memblock to the heap
static void heap_extend(int32_t type, void *base, uint32_t size) {
	tm_block_t *block = heap_blk_new();
	block->next = NULL;
	block->type = type;
	block->base = (uintptr_t)base;
	block->offset = 0;
	block->size = size;
	heap_blk_insert_free(block);
}

// allocates memory from the heap (basically malloc())
static void *heap_alloc(int32_t type, uint32_t size, uint32_t alignment) {
	tm_block_t *block = heap_blk_alloc(type, size, alignment);

	if (!block)
		return NULL;

	return (void *)block->base;
}

// frees previously allocated heap memory (basically free())
static void heap_free(void *addr) {
	heap_blk_free((uintptr_t)addr);
}

void vitagl_mem_term(void) {
	heap_destroy();
	if (mempool_addr[0] != NULL) {
		sceKernelFreeMemBlock(mempool_id[0]);
		sceKernelFreeMemBlock(mempool_id[1]);
		mempool_addr[0] = NULL;
		mempool_addr[1] = NULL;
		mempool_id[0] = 0;
		mempool_id[1] = 0;
	}
}

int vitagl_mem_init(size_t size_ram, size_t size_cdram, size_t size_phycont) {
	if (mempool_addr[0] != NULL)
		vitagl_mem_term();

	mempool_size[0] = ALIGN(size_cdram, 256 * 1024);
	mempool_size[1] = ALIGN(size_ram, 4 * 1024);
	mempool_size[2] = ALIGN(size_phycont, 256 * 1024);
	mempool_id[0] = sceKernelAllocMemBlock("cdram_mempool", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, mempool_size[0], NULL);
	mempool_id[1] = sceKernelAllocMemBlock("ram_mempool", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, mempool_size[1], NULL);
	mempool_id[2] = sceKernelAllocMemBlock("phycont_mempool", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW, mempool_size[2], NULL);

	for (int i = 0; i < VGL_MEM_TYPE_COUNT - 2; i++) {
		sceKernelGetMemBlockBase(mempool_id[i], &mempool_addr[i]);
		sceGxmMapMemory(mempool_addr[i], mempool_size[i], SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);
	}

	// Initialize heap
	heap_init();

	// Add memblocks to heap
	heap_extend(VGL_MEM_VRAM, mempool_addr[0], mempool_size[0]);
	heap_extend(VGL_MEM_RAM, mempool_addr[1], mempool_size[1]);
	heap_extend(VGL_MEM_SLOW, mempool_addr[2], mempool_size[2]);

	return 1;
}

void vitagl_mempool_free(void *ptr, vglMemType type) {
	if (type == VGL_MEM_EXTERNAL)
		free(ptr);
	else
		heap_free(ptr); // type is already stored in heap for alloc'd blocks
}

void *vitagl_mempool_alloc(size_t size, vglMemType type) {
	void *res = NULL;
	if (size <= tm_free[type])
		res = heap_alloc(type, size, MEM_ALIGNMENT);
	return res;
}

// Returns currently free space on mempool
size_t vitagl_mempool_get_free_space(vglMemType type) {
	return tm_free[type];
}

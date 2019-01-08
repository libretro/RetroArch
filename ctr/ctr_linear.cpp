/* from https://github.com/smealum/ctrulib
 * modified to allow reducing __linear_heap_size at runtime */

#include <3ds.h>
#include <stdlib.h>
#include <3ds/util/rbtree.h>
#include "ctr_debug.h"

struct MemChunk
{
	u8* addr;
	u32 size;
};

struct MemBlock
{
	MemBlock *prev, *next;
	u8* base;
	u32 size;

	static MemBlock* Create(u8* base, u32 size)
	{
		auto b = (MemBlock*)malloc(sizeof(MemBlock));
		if (!b) return nullptr;
		b->prev = nullptr;
		b->next = nullptr;
		b->base = base;
		b->size = size;
		return b;
	}
};

struct MemPool
{
	MemBlock *first, *last;

	bool Ready() { return first != nullptr; }

	void AddBlock(MemBlock* blk)
	{
		blk->prev = last;
		if (last) last->next = blk;
		if (!first) first = blk;
		last = blk;
	}

	void DelBlock(MemBlock* b)
	{
		auto prev = b->prev, &pNext = prev ? prev->next : first;
		auto next = b->next, &nPrev = next ? next->prev : last;
		pNext = next;
		nPrev = prev;
		free(b);
	}

	void InsertBefore(MemBlock* b, MemBlock* p)
	{
		auto prev = b->prev, &pNext = prev ? prev->next : first;
		b->prev = p;
		p->next = b;
		p->prev = prev;
		pNext = p;
	}

	void InsertAfter(MemBlock* b, MemBlock* n)
	{
		auto next = b->next, &nPrev = next ? next->prev : last;
		b->next = n;
		n->prev = b;
		n->next = next;
		nPrev = n;
	}

	void CoalesceRight(MemBlock* b);

	bool Allocate(MemChunk& chunk, u32 size, int align);
	void Deallocate(const MemChunk& chunk);

	void Destroy()
	{
		MemBlock* next = nullptr;
		for (auto b = first; b; b = next)
		{
			next = b->next;
			free(b);
		}
		first = nullptr;
		last = nullptr;
	}

	//void Dump(const char* title);
	u32 GetFreeSpace();
};

static rbtree_t sAddrMap;

struct addrMapNode
{
	rbtree_node node;
	MemChunk chunk;
};

#define getAddrMapNode(x) rbtree_item((x), addrMapNode, node)

static int addrMapNodeComparator(const rbtree_node_t* _lhs, const rbtree_node_t* _rhs)
{
	auto lhs = getAddrMapNode(_lhs)->chunk.addr;
	auto rhs = getAddrMapNode(_rhs)->chunk.addr;
	if (lhs < rhs)
		return -1;
	if (lhs > rhs)
		return 1;
	return 0;
}

static void addrMapNodeDestructor(rbtree_node_t* a)
{
	free(getAddrMapNode(a));
}

static addrMapNode* getNode(void* addr)
{
	addrMapNode n;
	n.chunk.addr = (u8*)addr;
	auto p = rbtree_find(&sAddrMap, &n.node);
	return p ? getAddrMapNode(p) : nullptr;
}

static addrMapNode* newNode(const MemChunk& chunk)
{
	auto p = (addrMapNode*)malloc(sizeof(addrMapNode));
	if (!p) return nullptr;
	p->chunk = chunk;
	return p;
}

static void delNode(addrMapNode* node)
{
	rbtree_remove(&sAddrMap, &node->node, addrMapNodeDestructor);
}

extern u32 __linear_heap, __linear_heap_size;

static MemPool sLinearPool;
static u32 sLinearPool_maxaddr;

static bool linearInit(void)
{
	auto blk = MemBlock::Create((u8*)__linear_heap, __linear_heap_size);
	if (blk)
	{
		sLinearPool.AddBlock(blk);
      sLinearPool_maxaddr = __linear_heap;
		rbtree_init(&sAddrMap, addrMapNodeComparator);
		return true;
	}
	return false;
}

void* linearMemAlign(size_t size, size_t alignment)
{
	// Enforce minimum alignment
	if (alignment < 16)
		alignment = 16;

	// Convert alignment to shift amount
	int shift;
	for (shift = 4; shift < 32; shift ++)
	{
		if ((1U<<shift) == alignment)
			break;
	}
	if (shift == 32) // Invalid alignment
		return nullptr;

	// Initialize the pool if it is not ready
	if (!sLinearPool.Ready() && !linearInit())
		return nullptr;

	// Allocate the chunk
	MemChunk chunk;
	if (!sLinearPool.Allocate(chunk, size, shift))
		return nullptr;

	auto node = newNode(chunk);
	if (!node)
	{
		sLinearPool.Deallocate(chunk);
		return nullptr;
	}
	if (rbtree_insert(&sAddrMap, &node->node));

   if (sLinearPool_maxaddr < (u32)sLinearPool.last->base)
      sLinearPool_maxaddr = (u32)sLinearPool.last->base;

	return chunk.addr;
}

void* linearAlloc(size_t size)
{
#if 0
   extern PrintConsole* currentConsole;
   if(currentConsole->consoleInitialised)
   {
      printf("linearAlloc : 0x%08X\n", size);
      DEBUG_HOLD();
   }
#endif
	return linearMemAlign(size, 0x80);
}

void* linearRealloc(void* mem, size_t size)
{
	// TODO
	return NULL;
}

void linearFree(void* mem)
{
	auto node = getNode(mem);
	if (!node) return;

	// Free the chunk
	sLinearPool.Deallocate(node->chunk);

	// Free the node
	delNode(node);
}

u32 linearSpaceFree()
{
	return sLinearPool.GetFreeSpace();
}

extern "C" u32 ctr_get_linear_free(void)
{
   if(sLinearPool.last->base + sLinearPool.last->size != (u8*)__linear_heap + __linear_heap_size)
      return 0;
   return sLinearPool.last->size;
}

extern "C" u32 ctr_get_linear_unused(void)
{
   return __linear_heap + __linear_heap_size - sLinearPool_maxaddr;
}

extern "C" void ctr_linear_free_pages(u32 pages)
{
   if(sLinearPool.last->base + sLinearPool.last->size != (u8*)__linear_heap + __linear_heap_size)
      return;

   u32 size = pages << 12;
   if(size > sLinearPool.last->size)
      return;

   sLinearPool.last->size -= size;
   __linear_heap_size -= size;
   u32 tmp;
   svcControlMemory(&tmp, __linear_heap + __linear_heap_size, 0x0, size,
         MEMOP_FREE, (MemPerm)(MEMPERM_READ | MEMPERM_WRITE));

#if 0
   printf("l:0x%08X-->0x%08X(-0x%08X) \n", sLinearPool.last->size + size, sLinearPool.last->size, size);
   DEBUG_HOLD();
#endif
}

extern "C" void ctr_linear_get_stats(void)
{
   printf("last:\n");
   printf("0x%08X --> 0x%08X (0x%08X) \n", sLinearPool.last->base,
          sLinearPool.last->base + sLinearPool.last->size, sLinearPool.last->size);
   printf("free: 0x%08X unused: 0x%08X \n", ctr_get_linear_unused(), ctr_get_linear_free());
}

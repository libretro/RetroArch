#pragma once
#include <wiiu/types.h>
#include "memheap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MEMExpandedHeapMode
{
   MEM_EXP_HEAP_MODE_FIRST_FREE     = 0,
   MEM_EXP_HEAP_MODE_NEAREST_SIZE   = 1,
} MEMExpandedHeapMode;

typedef enum MEMExpandedHeapDirection
{
   MEM_EXP_HEAP_DIR_FROM_TOP        = 0,
   MEM_EXP_HEAP_DIR_FROM_BOTTOM     = 1,
} MEMExpandedHeapDirection;

typedef struct MEMExpandedHeapBlock MEMExpandedHeapBlock;
struct MEMExpandedHeapBlock
{
   uint32_t attribs;
   uint32_t blockSize;
   MEMExpandedHeapBlock *prev;
   MEMExpandedHeapBlock *next;
   uint16_t tag;
   uint16_t __unknown;
};

typedef struct MEMExpandedHeapBlockList
{
   MEMExpandedHeapBlock *head;
   MEMExpandedHeapBlock *tail;
}MEMExpandedHeapBlockList;

typedef struct MEMExpandedHeap
{
   MEMHeapHeader header;
   MEMExpandedHeapBlockList freeList;
   MEMExpandedHeapBlockList usedList;
   uint16_t groupId;
   uint16_t attribs;
}MEMExpandedHeap;

MEMExpandedHeap *MEMCreateExpHeapEx(MEMExpandedHeap *heap, uint32_t size, uint16_t flags);
MEMExpandedHeap *MEMDestroyExpHeap(MEMExpandedHeap *heap);
void *MEMAllocFromExpHeapEx(MEMExpandedHeap *heap, uint32_t size, int alignment);
void MEMFreeToExpHeap(MEMExpandedHeap *heap, uint8_t *block);
MEMExpandedHeapMode MEMSetAllocModeForExpHeap(MEMExpandedHeap *heap, MEMExpandedHeapMode mode);
MEMExpandedHeapMode MEMGetAllocModeForExpHeap(MEMExpandedHeap *heap);
uint32_t MEMAdjustExpHeap(MEMExpandedHeap *heap);
uint32_t MEMResizeForMBlockExpHeap(MEMExpandedHeap *heap, uint8_t *address, uint32_t size);
uint32_t MEMGetTotalFreeSizeForExpHeap(MEMExpandedHeap *heap);
uint32_t MEMGetAllocatableSizeForExpHeapEx(MEMExpandedHeap *heap, int alignment);
uint16_t MEMSetGroupIDForExpHeap(MEMExpandedHeap *heap, uint16_t id);
uint16_t MEMGetGroupIDForExpHeap(MEMExpandedHeap *heap);
uint32_t MEMGetSizeForMBlockExpHeap(uint8_t *addr);
uint16_t MEMGetGroupIDForMBlockExpHeap(uint8_t *addr);
MEMExpandedHeapDirection MEMGetAllocDirForMBlockExpHeap(uint8_t *addr);

#ifdef __cplusplus
}
#endif

#pragma once
#include <wiiu/types.h>
#include "memheap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMUnitHeapFreeBlock
{
   struct MEMUnitHeapFreeBlock *next;
} MEMUnitHeapFreeBlock;

typedef struct
{
   MEMHeapHeader header;
   MEMUnitHeapFreeBlock *freeBlocks;
   uint32_t blockSize;
} MEMUnitHeap;

MEMUnitHeap *MEMCreateUnitHeapEx(MEMUnitHeap *heap, uint32_t size, uint32_t blockSize,
                                 int32_t alignment, uint16_t flags);
void *MEMDestroyUnitHeap(MEMUnitHeap *heap);
void *MEMAllocFromUnitHeap(MEMUnitHeap *heap);
void MEMFreeToUnitHeap(MEMUnitHeap *heap, void *block);
void MEMiDumpUnitHeap(MEMUnitHeap *heap);
uint32_t MEMCountFreeBlockForUnitHeap(MEMUnitHeap *heap);
uint32_t MEMCalcHeapSizeForUnitHeap(uint32_t blockSize, uint32_t count, int32_t alignment);

#ifdef __cplusplus
}
#endif

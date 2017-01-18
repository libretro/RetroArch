#pragma once
#include <wut.h>
#include "memheap.h"

/**
 * \defgroup coreinit_unitheap Unit Heap
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMUnitHeap MEMUnitHeap;
typedef struct MEMUnitHeapFreeBlock MEMUnitHeapFreeBlock;

struct MEMUnitHeapFreeBlock
{
   MEMUnitHeapFreeBlock *next;
};
CHECK_OFFSET(MEMUnitHeapFreeBlock, 0x00, next);
CHECK_SIZE(MEMUnitHeapFreeBlock, 0x04);

struct MEMUnitHeap
{
   MEMHeapHeader header;
   MEMUnitHeapFreeBlock *freeBlocks;
   uint32_t blockSize;
};
CHECK_OFFSET(MEMUnitHeap, 0x00, header);
CHECK_OFFSET(MEMUnitHeap, 0x40, freeBlocks);
CHECK_OFFSET(MEMUnitHeap, 0x44, blockSize);
CHECK_SIZE(MEMUnitHeap, 0x48);

MEMUnitHeap *
MEMCreateUnitHeapEx(MEMUnitHeap *heap,
                    uint32_t size,
                    uint32_t blockSize,
                    int32_t alignment,
                    uint16_t flags);

void *
MEMDestroyUnitHeap(MEMUnitHeap *heap);

void *
MEMAllocFromUnitHeap(MEMUnitHeap *heap);

void
MEMFreeToUnitHeap(MEMUnitHeap *heap,
                  void *block);

void
MEMiDumpUnitHeap(MEMUnitHeap *heap);

uint32_t
MEMCountFreeBlockForUnitHeap(MEMUnitHeap *heap);

uint32_t
MEMCalcHeapSizeForUnitHeap(uint32_t blockSize,
                           uint32_t count,
                           int32_t alignment);

#ifdef __cplusplus
}
#endif

/** @} */

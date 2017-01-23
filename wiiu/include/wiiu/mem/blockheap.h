#pragma once
#include <wiiu/types.h>
#include "memheap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMBlockHeapBlock MEMBlockHeapBlock;
typedef struct MEMBlockHeapTracking MEMBlockHeapTracking;
typedef struct MEMBlockHeap MEMBlockHeap;

typedef struct MEMBlockHeapTracking
{
   uint32_t __unknown0;
   uint32_t __unknown1;
   MEMBlockHeapBlock *blocks;
   uint32_t blockCount;
} MEMBlockHeapTracking;

typedef struct MEMBlockHeapBlock
{
   void *start;
   void *end;
   BOOL isFree;
   MEMBlockHeapBlock *prev;
   MEMBlockHeapBlock *next;
} MEMBlockHeapBlock;

typedef struct MEMBlockHeap
{
   MEMHeapHeader header;
   MEMBlockHeapTracking defaultTrack;
   MEMBlockHeapBlock defaultBlock;
   MEMBlockHeapBlock *firstBlock;
   MEMBlockHeapBlock *lastBlock;
   MEMBlockHeapBlock *firstFreeBlock;
   uint32_t numFreeBlocks;
} MEMBlockHeap;

MEMBlockHeap *MEMInitBlockHeap(MEMBlockHeap *heap,
                               void *start,
                               void *end,
                               MEMBlockHeapTracking *blocks,
                               uint32_t size,
                               uint32_t flags);

void *MEMDestroyBlockHeap(MEMBlockHeap *heap);

int MEMAddBlockHeapTracking(MEMBlockHeap *heap,
                            MEMBlockHeapTracking *tracking,
                            uint32_t size);

void *MEMAllocFromBlockHeapAt(MEMBlockHeap *heap,
                              void *addr,
                              uint32_t size);

void *MEMAllocFromBlockHeapEx(MEMBlockHeap *heap,
                              uint32_t size,
                              int32_t align);

void MEMFreeToBlockHeap(MEMBlockHeap *heap,
                        void *data);

uint32_t MEMGetAllocatableSizeForBlockHeapEx(MEMBlockHeap *heap,
      int32_t align);

uint32_t MEMGetTrackingLeftInBlockHeap(MEMBlockHeap *heap);

uint32_t MEMGetTotalFreeSizeForBlockHeap(MEMBlockHeap *heap);

#ifdef __cplusplus
}
#endif

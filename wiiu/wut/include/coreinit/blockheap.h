#pragma once
#include <wut.h>
#include "memheap.h"

/**
 * \defgroup coreinit_blockheap Block Heap
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMBlockHeapBlock MEMBlockHeapBlock;
typedef struct MEMBlockHeapTracking MEMBlockHeapTracking;
typedef struct MEMBlockHeap MEMBlockHeap;

struct MEMBlockHeapTracking
{
   UNKNOWN(0x8);

   //! Pointer to first memory block
   MEMBlockHeapBlock *blocks;

   //! Number of blocks in this tracking heap
   uint32_t blockCount;
};
CHECK_OFFSET(MEMBlockHeapTracking, 0x08, blocks);
CHECK_OFFSET(MEMBlockHeapTracking, 0x0C, blockCount);
CHECK_SIZE(MEMBlockHeapTracking, 0x10);

struct MEMBlockHeapBlock
{
   //! First address of the data region this block has allocated
   void *start;

   //! End address of the data region this block has allocated
   void *end;

   //! TRUE if the block is free, FALSE if allocated
   BOOL isFree;

   //! Link to previous block, note that this is only set for allocated blocks
   MEMBlockHeapBlock *prev;

   //! Link to next block, always set
   MEMBlockHeapBlock *next;
};
CHECK_OFFSET(MEMBlockHeapBlock, 0x00, start);
CHECK_OFFSET(MEMBlockHeapBlock, 0x04, end);
CHECK_OFFSET(MEMBlockHeapBlock, 0x08, isFree);
CHECK_OFFSET(MEMBlockHeapBlock, 0x0c, prev);
CHECK_OFFSET(MEMBlockHeapBlock, 0x10, next);
CHECK_SIZE(MEMBlockHeapBlock, 0x14);

struct MEMBlockHeap
{
   MEMHeapHeader header;

   //! Default tracking heap, tracks only defaultBlock
   MEMBlockHeapTracking defaultTrack;

   //! Default block, used so we don't have an empty block list
   MEMBlockHeapBlock defaultBlock;

   //! First block in this heap
   MEMBlockHeapBlock *firstBlock;

   //! Last block in this heap
   MEMBlockHeapBlock *lastBlock;

   //! First free block
   MEMBlockHeapBlock *firstFreeBlock;

   //! Free block count
   uint32_t numFreeBlocks;
};
CHECK_OFFSET(MEMBlockHeap, 0x00, header);
CHECK_OFFSET(MEMBlockHeap, 0x40, defaultTrack);
CHECK_OFFSET(MEMBlockHeap, 0x50, defaultBlock);
CHECK_OFFSET(MEMBlockHeap, 0x64, firstBlock);
CHECK_OFFSET(MEMBlockHeap, 0x68, lastBlock);
CHECK_OFFSET(MEMBlockHeap, 0x6C, firstFreeBlock);
CHECK_OFFSET(MEMBlockHeap, 0x70, numFreeBlocks);
CHECK_SIZE(MEMBlockHeap, 0x74);

MEMBlockHeap *
MEMInitBlockHeap(MEMBlockHeap *heap,
                 void *start,
                 void *end,
                 MEMBlockHeapTracking *blocks,
                 uint32_t size,
                 uint32_t flags);

void *
MEMDestroyBlockHeap(MEMBlockHeap *heap);

int
MEMAddBlockHeapTracking(MEMBlockHeap *heap,
                        MEMBlockHeapTracking *tracking,
                        uint32_t size);

void *
MEMAllocFromBlockHeapAt(MEMBlockHeap *heap,
                        void *addr,
                        uint32_t size);

void *
MEMAllocFromBlockHeapEx(MEMBlockHeap *heap,
                        uint32_t size,
                        int32_t align);

void
MEMFreeToBlockHeap(MEMBlockHeap *heap,
                   void *data);

uint32_t
MEMGetAllocatableSizeForBlockHeapEx(MEMBlockHeap *heap,
                                    int32_t align);

uint32_t
MEMGetTrackingLeftInBlockHeap(MEMBlockHeap *heap);

uint32_t
MEMGetTotalFreeSizeForBlockHeap(MEMBlockHeap *heap);

#ifdef __cplusplus
}
#endif

/** @} */

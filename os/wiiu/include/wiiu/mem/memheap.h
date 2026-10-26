#pragma once
#include <wiiu/types.h>
#include <wiiu/os/spinlock.h>
#include "memlist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MEMHeapFillType
{
   MEM_HEAP_FILL_TYPE_UNUSED    = 0,
   MEM_HEAP_FILL_TYPE_ALLOCATED = 1,
   MEM_HEAP_FILL_TYPE_FREED     = 2,
} MEMHeapFillType;

typedef enum MEMHeapTag
{
   MEM_BLOCK_HEAP_TAG      = 0x424C4B48u,
   MEM_EXPANDED_HEAP_TAG   = 0x45585048u,
   MEM_FRAME_HEAP_TAG      = 0x46524D48u,
   MEM_UNIT_HEAP_TAG       = 0x554E5448u,
   MEM_USER_HEAP_TAG       = 0x55535248u,
} MEMHeapTag;

typedef enum MEMHeapFlags
{
   MEM_HEAP_FLAG_ZERO_ALLOCATED  = 1 << 0,
   MEM_HEAP_FLAG_DEBUG_MODE      = 1 << 1,
   MEM_HEAP_FLAG_USE_LOCK        = 1 << 2,
} MEMHeapFlags;

typedef struct MEMHeapHeader
{
   /*! Tag indicating which type of heap this is */
   MEMHeapTag tag;

   /*! Link for list this heap is in */
   MEMMemoryLink link;

   /*! List of all child heaps in this heap */
   MEMMemoryList list;

   /*! Pointer to start of allocatable memory */
   void *dataStart;

   /*! Pointer to end of allocatable memory */
   void *dataEnd;

   /*! Lock used when MEM_HEAP_FLAG_USE_LOCK is set. */
   OSSpinLock lock;

   /*! Flags set during heap creation. */
   uint32_t flags;

   uint32_t __unknown[0x3];
} MEMHeapHeader;

/**
 * Print details about heap to COSWarn
 */
void MEMDumpHeap(MEMHeapHeader *heap);

/**
 * Find heap which contains a memory block.
 */
MEMHeapHeader *MEMFindContainHeap(void *block);

/**
 * Get the data fill value used when MEM_HEAP_FLAG_DEBUG_MODE is set.
 */
uint32_t MEMGetFillValForHeap(MEMHeapFillType type);

/**
 * Set the data fill value used when MEM_HEAP_FLAG_DEBUG_MODE is set.
 */
void MEMSetFillValForHeap(MEMHeapFillType type, uint32_t value);

#ifdef __cplusplus
}
#endif

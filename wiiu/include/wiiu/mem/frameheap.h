#pragma once
#include <wiiu/types.h>
#include "memheap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MEMFrameHeapFreeMode
{
   MEM_FRAME_HEAP_FREE_HEAD   = 1 << 0,
   MEM_FRAME_HEAP_FREE_TAIL   = 1 << 1,
   MEM_FRAME_HEAP_FREE_ALL    = MEM_FRAME_HEAP_FREE_HEAD | MEM_FRAME_HEAP_FREE_TAIL,
} MEMFrameHeapFreeMode;

typedef struct MEMFrameHeapState
{
   uint32_t tag;
   void *head;
   void *tail;
   struct MEMFrameHeapState *previous;
} MEMFrameHeapState;

typedef struct MEMFrameHeap
{
   MEMHeapHeader header;
   void *head;
   void *tail;
   MEMFrameHeapState *previousState;
} MEMFrameHeap;

MEMFrameHeap *MEMCreateFrmHeapEx(void *heap, uint32_t size, uint32_t flags);
void *MEMDestroyFrmHeap(MEMFrameHeap *heap);
void *MEMAllocFromFrmHeapEx(MEMFrameHeap *heap, uint32_t size, int alignment);
void MEMFreeToFrmHeap(MEMFrameHeap *heap, MEMFrameHeapFreeMode mode);
BOOL MEMRecordStateForFrmHeap(MEMFrameHeap *heap, uint32_t tag);
BOOL MEMFreeByStateToFrmHeap(MEMFrameHeap *heap, uint32_t tag);
uint32_t MEMAdjustFrmHeap(MEMFrameHeap *heap);
uint32_t MEMResizeForMBlockFrmHeap(MEMFrameHeap *heap, uint32_t addr, uint32_t size);
uint32_t MEMGetAllocatableSizeForFrmHeapEx(MEMFrameHeap *heap, int alignment);

#ifdef __cplusplus
}
#endif

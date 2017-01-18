#pragma once
#include <wut.h>
#include "memheap.h"

/**
 * \defgroup coreinit_frameheap Frame Heap
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MEMFrameHeapFreeMode
{
   MEM_FRAME_HEAP_FREE_HEAD   = 1 << 0,
   MEM_FRAME_HEAP_FREE_TAIL   = 1 << 1,
   MEM_FRAME_HEAP_FREE_ALL    = MEM_FRAME_HEAP_FREE_HEAD | MEM_FRAME_HEAP_FREE_TAIL,
} MEMFrameHeapFreeMode;

typedef struct MEMFrameHeap MEMFrameHeap;
typedef struct MEMFrameHeapState MEMFrameHeapState;

struct MEMFrameHeapState
{
   uint32_t tag;
   void *head;
   void *tail;
   MEMFrameHeapState *previous;
};
CHECK_OFFSET(MEMFrameHeapState, 0x00, tag);
CHECK_OFFSET(MEMFrameHeapState, 0x04, head);
CHECK_OFFSET(MEMFrameHeapState, 0x08, tail);
CHECK_OFFSET(MEMFrameHeapState, 0x0C, previous);
CHECK_SIZE(MEMFrameHeapState, 0x10);

struct MEMFrameHeap
{
   MEMHeapHeader header;
   void *head;
   void *tail;
   MEMFrameHeapState *previousState;
};
CHECK_OFFSET(MEMFrameHeap, 0x00, header);
CHECK_OFFSET(MEMFrameHeap, 0x40, head);
CHECK_OFFSET(MEMFrameHeap, 0x44, tail);
CHECK_OFFSET(MEMFrameHeap, 0x48, previousState);
CHECK_SIZE(MEMFrameHeap, 0x4C);

MEMFrameHeap *
MEMCreateFrmHeapEx(void *heap,
                   uint32_t size,
                   uint32_t flags);

void *
MEMDestroyFrmHeap(MEMFrameHeap *heap);

void *
MEMAllocFromFrmHeapEx(MEMFrameHeap *heap,
                      uint32_t size,
                      int alignment);

void
MEMFreeToFrmHeap(MEMFrameHeap *heap,
                 MEMFrameHeapFreeMode mode);

BOOL
MEMRecordStateForFrmHeap(MEMFrameHeap *heap,
                         uint32_t tag);

BOOL
MEMFreeByStateToFrmHeap(MEMFrameHeap *heap,
                        uint32_t tag);

uint32_t
MEMAdjustFrmHeap(MEMFrameHeap *heap);

uint32_t
MEMResizeForMBlockFrmHeap(MEMFrameHeap *heap,
                          uint32_t addr,
                          uint32_t size);

uint32_t
MEMGetAllocatableSizeForFrmHeapEx(MEMFrameHeap *heap,
                                  int alignment);

#ifdef __cplusplus
}
#endif

/** @} */

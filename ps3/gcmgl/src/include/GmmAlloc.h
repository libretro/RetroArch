#ifndef _GMM_ALLOC_H_
#define _GMM_ALLOC_H_

void gmmPrintState(); // print out all blocks and their current states

#define GMM_ASSERT(cond) ((void)0)

#define GMM_ERROR                   0xFFFFFFFF
#define GMM_TILE_ALIGNMENT          0x10000 // 16K
#define GMM_ALIGNMENT               128		// non-tile is 128 byte
#define GMM_RSX_WAIT_INDEX          254		// label index
#define GMM_PPU_WAIT_INDEX          255		// label index
#define GMM_BLOCK_COUNT             512		// initial memory block pool
#define GMM_TILE_BLOCK_COUNT        16		// initial tile memory block pool

#define GMM_NUM_FREE_BINS           22
#define GMM_FREE_BIN_0              0x80	// 0x00 - 0x80
#define GMM_FREE_BIN_1              0x100	// 0x80 - 0x100
#define GMM_FREE_BIN_2              0x180	// ...
#define GMM_FREE_BIN_3              0x200
#define GMM_FREE_BIN_4              0x280
#define GMM_FREE_BIN_5              0x300
#define GMM_FREE_BIN_6              0x380
#define GMM_FREE_BIN_7              0x400
#define GMM_FREE_BIN_8              0x800
#define GMM_FREE_BIN_9              0x1000
#define GMM_FREE_BIN_10             0x2000
#define GMM_FREE_BIN_11             0x4000
#define GMM_FREE_BIN_12             0x8000
#define GMM_FREE_BIN_13             0x10000
#define GMM_FREE_BIN_14             0x20000
#define GMM_FREE_BIN_15             0x40000
#define GMM_FREE_BIN_16             0x80000
#define GMM_FREE_BIN_17             0x100000
#define GMM_FREE_BIN_18             0x200000
#define GMM_FREE_BIN_19             0x400000
#define GMM_FREE_BIN_20             0x800000
#define GMM_FREE_BIN_21             0x1000000

// data structure for the fixed allocater 
typedef struct GmmFixedAllocData{
   char        **ppBlockList[2];		// pre-allocated list of block descriptors
   uint16_t    **ppFreeBlockList[2];
   uint16_t    *pBlocksUsed[2];
   uint16_t    BlockListCount[2];
}GmmFixedAllocData;

// common shared block descriptor for tile and non-tile block
// "base class" for GmmBlock and GmmTileBlock
typedef struct GmmBaseBlock{

   uint8_t     isTile;
   uint32_t    address;
   uint32_t    size;
}GmmBaseBlock;

typedef struct GmmBlock{
   GmmBaseBlock    base;	// inheritence
   struct GmmBlock *pPrev;
   struct GmmBlock *pNext;

   uint8_t     isPinned;

   // these would only be valid if the block is in 
   // pending free list or free list
   struct GmmBlock *pPrevFree;
   struct GmmBlock *pNextFree;

   uint32_t    fence;
}GmmBlock;

typedef struct GmmTileBlock
{
   GmmBaseBlock        base;	// inheritence
   struct GmmTileBlock *pPrev;
   struct GmmTileBlock *pNext;

   uint32_t    tileTag;
   void        *pData;
} GmmTileBlock;

typedef struct GmmAllocator
{
   uint32_t    memoryBase;

   uint32_t    startAddress;
   uint32_t    size;
   uint32_t    freeAddress;

   GmmBlock    *pHead;
   GmmBlock    *pTail;
   GmmBlock    *pSweepHead;
   uint32_t    freedSinceSweep;

   uint32_t    tileStartAddress;
   uint32_t    tileSize;

   GmmTileBlock    *pTileHead;
   GmmTileBlock    *pTileTail;

   GmmBlock    *pPendingFreeHead;
   GmmBlock    *pPendingFreeTail;

   // Acceleration data structure for free blocks
   GmmBlock    *pFreeHead[GMM_NUM_FREE_BINS];
   GmmBlock    *pFreeTail[GMM_NUM_FREE_BINS];

   uint32_t    totalSize;		// == size + tileSize
} GmmAllocator;

uint32_t gmmDestroy(void);
char *gmmIdToAddress(const uint32_t id);
uint32_t gmmFree (const uint32_t freeId);
uint32_t gmmAlloc(const uint32_t size);
uint32_t gmmAllocTiled(const uint32_t size);

extern GmmAllocator         *pGmmLocalAllocator;

#define GMM_ADDRESS_TO_OFFSET(address) (address - pGmmLocalAllocator->memoryBase)

#define gmmGetBlockSize(id) (((GmmBaseBlock*)id)->size)
#define gmmGetTileData(id) (((GmmTileBlock*)id)->pData)
#define gmmIdToOffset(id)  (GMM_ADDRESS_TO_OFFSET(((GmmBaseBlock*)id)->address))
#define gmmAllocFixedTileBlock() ((GmmTileBlock*)gmmAllocFixed(1))
#define gmmFreeFixedTileBlock(data)  (gmmFreeFixed(1, (GmmTileBlock*)data))
#define gmmFreeFixedBlock(data)  (gmmFreeFixed(0, (GmmBlock*)data))
#define gmmAllocTileBlock(pAllocator, size, pBlock) ((pBlock == NULL) ? gmmCreateTileBlock(pAllocator, size) : pBlock)

#define gmmSetTileAttrib(id, tag, data) \
   ((GmmTileBlock*)id)->tileTag = tag; \
   ((GmmTileBlock*)id)->pData = data;

#define gmmPinId(id) \
   if (!((GmmBaseBlock*)id)->isTile) \
      ((GmmBlock *)id)->isPinned = 1;

#define gmmUnpinId(id) \
   if (!((GmmBaseBlock*)id)->isTile) \
      ((GmmBlock *)id)->isPinned = 0;

#endif

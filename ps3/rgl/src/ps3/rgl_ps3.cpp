/*  RetroArch - A frontend for libretro.
 *  RGL - An OpenGL subset wrapper library.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../rgl.h"
#include "../rglp.h"

#include <sdk_version.h>

#include "include/GmmAlloc.h"
#include "include/rgl-constants.h"
#include "include/rgl-typedefs.h"
#include "include/rgl-externs.h"
#include "include/rgl-inline.h"

#include <Cg/CgCommon.h>

#include <ppu_intrinsics.h>
#include <sys/memory.h>
#include <sys/sys_time.h>
#include <sys/timer.h>
#include <sysutil/sysutil_sysparam.h>
#include <sys/synchronization.h>

#include <cell/sysmodule.h>
#include <cell/gcm.h>
#include <cell/gcm/gcm_method_data.h>
#include <cell/resc.h>
using namespace cell::Gcm;

static GLuint nvFenceCounter = 0;

// the global instance of the rglGcmState_i structure
rglGcmState rglGcmState_i;

#define NAME_INCREMENT 4
#define CAPACITY_INCREMENT 16

static void rglInitNameSpace(void *data)
{
   rglNameSpace *name = (rglNameSpace*)data;
   name->data = NULL;
   name->firstFree = NULL;
   name->capacity = 0;
}

static void rglFreeNameSpace(void *data)
{
   rglNameSpace *name = (rglNameSpace*)data;

   if (name->data)
      free(name->data);

   name->data = NULL;
   name->capacity = 0;
   name->firstFree = NULL;
}

unsigned int rglCreateName(void *data, void* object)
{
   rglNameSpace *name = (rglNameSpace*)data;
   // NULL is reserved for the guard of the linked list.
   if (name->firstFree == NULL)
   {
      // need to allocate more pointer space
      int newCapacity = name->capacity + NAME_INCREMENT;

      // realloc the block of pointers
      void** newData = ( void** )malloc( newCapacity * sizeof( void* ) );
      if ( newData == NULL )
      {
         // XXX what should we generally do here ?
         rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
         return 0;
      }
      memcpy( newData, name->data, name->capacity * sizeof( void* ) );

      if (name->data != NULL)
         free (name->data);

      name->data = newData;

      // initialize the pointers to the next free elements.
      // (effectively build a linked list of free elements in place)
      // treat the last item differently, by linking it to NULL
      for ( int index = name->capacity; index < newCapacity - 1; ++index )
         name->data[index] = name->data + index + 1;

      name->data[newCapacity - 1] = NULL;
      // update the first free element to the new data pointer.
      name->firstFree = name->data + name->capacity;
      // update the new capacity.
      name->capacity = newCapacity;
   }
   // firstFree is a pointer, compute the index of it
   unsigned int result = name->firstFree - name->data;

   // update the first free to the next free element.
   name->firstFree = (void**)*name->firstFree;

   // store the object in data.
   name->data[result] = object;

   // offset the index by 1 to avoid the name 0
   return result + 1;
}

unsigned int rglIsName (void *data, unsigned int name )
{
   rglNameSpace *ns = (rglNameSpace*)data;
   // there should always be a namespace
   // 0 is never valid.
   if (RGL_UNLIKELY(name == 0))
      return 0;

   // names start numbering from 1, so convert from a name to an index
   --name;

   // test whether it is in the namespace range
   if ( RGL_UNLIKELY( name >= ns->capacity ) )
      return 0;

   // test whether the pointer is inside the data block.
   // if so, it means it is free.
   // if it points to NULL, it means it is the last free name in the linked list.
   void** value = ( void** )ns->data[name];

   if ( RGL_UNLIKELY(value == NULL ||
            ( value >= ns->data && value < ns->data + ns->capacity ) ) )
      return 0;

   // The pointer is not free and allocated, so name is a real name.
   return 1;
}

void rglEraseName(void *data, unsigned int name )
{
   rglNameSpace *ns = (rglNameSpace*)data;
   if (rglIsName(ns, name))
   {
      --name;
      ns->data[name] = ns->firstFree;
      ns->firstFree = ns->data + name;
   }
}

// Initialize texture namespace ns with creation and destruction functions
static void rglTexNameSpaceInit(void *data,
      rglTexNameSpaceCreateFunction create,
      rglTexNameSpaceDestroyFunction destroy)
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   ns->capacity = CAPACITY_INCREMENT;
   ns->data = (void **)malloc( ns->capacity * sizeof( void* ) );
   memset( ns->data, 0, ns->capacity*sizeof( void* ) );
   ns->create = create;
   ns->destroy = destroy;
}

// Free texture namespace ns
static void rglTexNameSpaceFree(void *data)
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   for (GLuint i = 1;i < ns->capacity; ++i)
      if (ns->data[i])
         ns->destroy( ns->data[i] );

   free( ns->data );
   ns->data = NULL;
}

// Reset all names in namespace ns to NULL
void rglTexNameSpaceResetNames(void *data)
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   for ( GLuint i = 1;i < ns->capacity;++i )
   {
      if ( ns->data[i] )
      {
         ns->destroy( ns->data[i] );
         ns->data[i] = NULL;
      }
   }
}

// Get an index of the first free name in namespace ns
static GLuint rglTexNameSpaceGetFree(void *data)
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;
   GLuint i;

   for (i = 1;i < ns->capacity;++i)
      if (!ns->data[i])
         break;
   return i;
}

// Add name to namespace by increasing capacity and calling creation call back function
// Return GL_TRUE for success, GL_FALSE for failure
GLboolean rglTexNameSpaceCreateNameLazy(void *data, GLuint name )
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   if (name >= ns->capacity)
   {
      int newCapacity = name >= ns->capacity + CAPACITY_INCREMENT ? name + 1 : ns->capacity + CAPACITY_INCREMENT;
      void **newData = ( void ** )realloc( ns->data, newCapacity * sizeof( void * ) );
      memset( newData + ns->capacity, 0, ( newCapacity - ns->capacity )*sizeof( void * ) );
      ns->data = newData;
      ns->capacity = newCapacity;
   }
   if ( !ns->data[name] )
   {
      ns->data[name] = ns->create();
      if ( ns->data[name] ) return GL_TRUE;
   }
   return GL_FALSE;
}

// Check if name is a valid name in namespace ns
// Return GL_TRUE if so, GL_FALSE otherwise
GLboolean rglTexNameSpaceIsName(void *data, GLuint name )
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;
   
   if ((name > 0) && (name < ns->capacity))
      return( ns->data[name] != 0 );
   else return GL_FALSE;
}

// Generate new n names in namespace ns
static void rglTexNameSpaceGenNames(void *data, GLsizei n, GLuint *names )
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   for ( int i = 0;i < n;++i )
   {
      GLuint name = rglTexNameSpaceGetFree( ns );
      names[i] = name;
      if (name)
         rglTexNameSpaceCreateNameLazy( ns, name );
   }
}

// Delete n names from namespace ns
void rglTexNameSpaceDeleteNames(void *data, GLsizei n, const GLuint *names )
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   for ( int i = 0;i < n;++i )
   {
      GLuint name = names[i];
      if (!rglTexNameSpaceIsName(ns, name))
         continue;
      ns->destroy( ns->data[name] );
      ns->data[name] = NULL;
   }
}

/*============================================================
  MEMORY MANAGER
  ============================================================ */

static GmmAllocator         *pGmmLocalAllocator = NULL;
static volatile uint32_t    *pLock = NULL;
static uint32_t             cachedLockValue = 0;
static GmmFixedAllocData    *pGmmFixedAllocData = NULL;

#define PAD(x, pad) ((x + pad - 1) / pad * pad)
#define GMM_ADDRESS_TO_OFFSET(address) (address - pGmmLocalAllocator->memoryBase)

static uint32_t gmmInitFixedAllocator(void)
{

   pGmmFixedAllocData = (GmmFixedAllocData *)malloc(sizeof(GmmFixedAllocData));

   if (pGmmFixedAllocData == NULL)
      return GMM_ERROR;

   memset(pGmmFixedAllocData, 0, sizeof(GmmFixedAllocData));

   for (int i=0; i<2; i++)
   {
      int blockCount = (i==0) ? GMM_BLOCK_COUNT : GMM_TILE_BLOCK_COUNT;
      int blockSize  = (i==0) ? sizeof(GmmBlock): sizeof(GmmTileBlock);

      pGmmFixedAllocData->ppBlockList[i] = (char **)malloc(sizeof(char *));
      if (pGmmFixedAllocData->ppBlockList[i] == NULL)
         return GMM_ERROR;

      pGmmFixedAllocData->ppBlockList[i][0] = (char *)malloc(blockSize * blockCount);
      if (pGmmFixedAllocData->ppBlockList[i][0] == NULL)
         return GMM_ERROR;

      pGmmFixedAllocData->ppFreeBlockList[i] = (uint16_t **)malloc(sizeof(uint16_t *));
      if (pGmmFixedAllocData->ppFreeBlockList[i] == NULL)
         return GMM_ERROR;

      pGmmFixedAllocData->ppFreeBlockList[i][0] = (uint16_t *)malloc(sizeof(uint16_t) * blockCount);
      if (pGmmFixedAllocData->ppFreeBlockList[i][0] == NULL)
         return GMM_ERROR;

      pGmmFixedAllocData->pBlocksUsed[i] = (uint16_t *)malloc(sizeof(uint16_t));
      if (pGmmFixedAllocData->pBlocksUsed[i] == NULL)
         return GMM_ERROR;

      for (int j=0; j<blockCount; j++)
         pGmmFixedAllocData->ppFreeBlockList[i][0][j] = j; 

      pGmmFixedAllocData->pBlocksUsed[i][0] = 0;
      pGmmFixedAllocData->BlockListCount[i] = 1;
   }

   return CELL_OK;
}

static void *gmmAllocFixed(uint8_t isTile)
{
   int blockCount = isTile ? GMM_TILE_BLOCK_COUNT : GMM_BLOCK_COUNT;
   int blockSize  = isTile ? sizeof(GmmTileBlock) : sizeof(GmmBlock);
   int listCount  = pGmmFixedAllocData->BlockListCount[isTile];

   for (int i=0; i<listCount; i++)
   {
      if (pGmmFixedAllocData->pBlocksUsed[isTile][i] < blockCount)
      {
         return pGmmFixedAllocData->ppBlockList[isTile][i] + 
            (pGmmFixedAllocData->ppFreeBlockList[isTile][i][pGmmFixedAllocData->pBlocksUsed[isTile][i]++] * 
             blockSize);
      }
   }

   char **ppBlockList = 
      (char **)realloc(pGmmFixedAllocData->ppBlockList[isTile],
            (listCount + 1) * sizeof(char *));

   if (ppBlockList == NULL)
      return NULL;

   pGmmFixedAllocData->ppBlockList[isTile] = ppBlockList;

   pGmmFixedAllocData->ppBlockList[isTile][listCount] = 
      (char *)malloc(blockSize * blockCount);

   if (pGmmFixedAllocData->ppBlockList[isTile][listCount] == NULL)
      return NULL;

   uint16_t **ppFreeBlockList = 
      (uint16_t **)realloc(pGmmFixedAllocData->ppFreeBlockList[isTile],
            (listCount + 1) * sizeof(uint16_t *));

   if (ppFreeBlockList == NULL)
      return NULL;

   pGmmFixedAllocData->ppFreeBlockList[isTile] = ppFreeBlockList;

   pGmmFixedAllocData->ppFreeBlockList[isTile][listCount] = 
      (uint16_t *)malloc(sizeof(uint16_t) * blockCount);

   if (pGmmFixedAllocData->ppFreeBlockList[isTile][listCount] == NULL)
      return NULL;

   uint16_t *pBlocksUsed = 
      (uint16_t *)realloc(pGmmFixedAllocData->pBlocksUsed[isTile],
            (listCount + 1) * sizeof(uint16_t));

   if (pBlocksUsed == NULL)
      return NULL;

   pGmmFixedAllocData->pBlocksUsed[isTile] = pBlocksUsed;

   for (int i=0; i<blockCount; i++)
      pGmmFixedAllocData->ppFreeBlockList[isTile][listCount][i] = i; 

   pGmmFixedAllocData->pBlocksUsed[isTile][listCount] = 0;
   pGmmFixedAllocData->BlockListCount[isTile]++;

   return pGmmFixedAllocData->ppBlockList[isTile][listCount] + 
      (pGmmFixedAllocData->ppFreeBlockList[isTile][listCount][pGmmFixedAllocData->pBlocksUsed[isTile][listCount]++] * 
       blockSize);
}

static void gmmFreeFixed(uint8_t isTile, void *pBlock)
{
   int blockCount = isTile ? GMM_TILE_BLOCK_COUNT : GMM_BLOCK_COUNT;
   int blockSize  = isTile ? sizeof(GmmTileBlock) : sizeof(GmmBlock);
   uint8_t found = 0;

   for (int i=0; i<pGmmFixedAllocData->BlockListCount[isTile]; i++)
   {
      if (pBlock >= pGmmFixedAllocData->ppBlockList[isTile][i] &&
            pBlock < (pGmmFixedAllocData->ppBlockList[isTile][i] + blockSize * blockCount))
      {
         int index = ((char *)pBlock - pGmmFixedAllocData->ppBlockList[isTile][i]) / blockSize;
         pGmmFixedAllocData->ppFreeBlockList[isTile][i][--pGmmFixedAllocData->pBlocksUsed[isTile][i]] = index;
         found = 1;
      }
   }
}

static void gmmDestroyFixedAllocator (void)
{
   if (pGmmFixedAllocData)
   {
      for (int i=0; i<2; i++) 
      {
         for(int j=0; j<pGmmFixedAllocData->BlockListCount[i]; j++)
         {
            free(pGmmFixedAllocData->ppBlockList[i][j]);
            free(pGmmFixedAllocData->ppFreeBlockList[i][j]);
         }
         free(pGmmFixedAllocData->ppBlockList[i]);
         free(pGmmFixedAllocData->ppFreeBlockList[i]);
         free(pGmmFixedAllocData->pBlocksUsed[i]);
      }

      free(pGmmFixedAllocData);
      pGmmFixedAllocData = NULL;
   }
}

#define GMM_ALLOC_FIXED_BLOCK() ((GmmBlock*)gmmAllocFixed(0))

static void gmmFreeFixedBlock (void *data)
{
   GmmBlock *pBlock = (GmmBlock*)data;
   gmmFreeFixed(0, pBlock);
}

static GmmTileBlock *gmmAllocFixedTileBlock (void)
{
   return (GmmTileBlock *)gmmAllocFixed(1);
}

static void gmmFreeFixedTileBlock (void *data)
{
   GmmTileBlock *pTileBlock = (GmmTileBlock*)data;
   gmmFreeFixed(1, pTileBlock);
}

uint32_t gmmInit(
      const void *localMemoryBase,
      const void *localStartAddress,
      const uint32_t localSize
      )
{
   GmmAllocator *pAllocator;
   uint32_t alignedLocalSize, alignedMainSize;
   uint32_t localEndAddress = (uint32_t)localStartAddress + localSize;

   localEndAddress = (localEndAddress / GMM_TILE_ALIGNMENT) * GMM_TILE_ALIGNMENT;

   alignedLocalSize = localEndAddress - (uint32_t)localStartAddress;

   pAllocator = (GmmAllocator *)malloc(2*sizeof(GmmAllocator));

   if (pAllocator == NULL)
      return GMM_ERROR;

   memset(pAllocator, 0, 1 * sizeof(GmmAllocator));

   if (pAllocator)
   {
      pAllocator->memoryBase = (uint32_t)localMemoryBase;
      pAllocator->startAddress = (uint32_t)localStartAddress;
      pAllocator->size = alignedLocalSize;
      pAllocator->freeAddress = pAllocator->startAddress;
      pAllocator->tileStartAddress = ((uint32_t)localStartAddress) + alignedLocalSize;
      pAllocator->totalSize = alignedLocalSize;

      pGmmLocalAllocator = pAllocator;
   }
   else
      return GMM_ERROR;

   pLock = cellGcmGetLabelAddress(GMM_PPU_WAIT_INDEX);
   *pLock = 0;
   cachedLockValue = 0;

   return gmmInitFixedAllocator();
}

static inline void gmmWaitForSweep (void)
{
   while (cachedLockValue != 0)
   {
      sys_timer_usleep(30);
      cachedLockValue = *pLock;
   }
}

uint32_t gmmDestroy (void)
{
   GmmBlock *pBlock, *pTmpBlock;
   GmmTileBlock *pTileBlock, *pTmpTileBlock;
   GmmAllocator *pAllocator;

   pAllocator = pGmmLocalAllocator;

   {
      if (pAllocator)
      {
         pBlock = pAllocator->pHead;

         while (pBlock)
         {
            pTmpBlock = pBlock;
            pBlock = pBlock->pNext;
            gmmFreeFixedBlock(pTmpBlock);
         }

         pTileBlock = pAllocator->pTileHead;

         while (pTileBlock)
         {
            pTmpTileBlock = pTileBlock;
            pTileBlock = pTileBlock->pNext;
            gmmFreeFixedTileBlock(pTmpTileBlock);
         }

         free(pAllocator);
      }
   }

   pGmmLocalAllocator = NULL;

   gmmDestroyFixedAllocator();

   return CELL_OK;
}

uint32_t gmmGetBlockSize(const uint32_t id)
{
   return ((GmmBaseBlock *)id)->size;
}

void *gmmGetTileData(const uint32_t id)
{
   return ((GmmTileBlock *)id)->pData;
}

void gmmSetTileAttrib(const uint32_t id, const uint32_t tag,
      void *pData)
{
   GmmTileBlock    *pTileBlock = (GmmTileBlock *)id;

   pTileBlock->tileTag = tag;
   pTileBlock->pData = pData;
}

uint32_t gmmIdToOffset(const uint32_t id)
{
   GmmBaseBlock    *pBaseBlock = (GmmBaseBlock *)id;
   return GMM_ADDRESS_TO_OFFSET(pBaseBlock->address);
}

char *gmmIdToAddress (const uint32_t id)
{
   GmmBaseBlock    *pBaseBlock = (GmmBaseBlock *)id;

   gmmWaitForSweep();

   return (char *)pBaseBlock->address;
}

void gmmPinId(const uint32_t id)
{
   GmmBaseBlock    *pBaseBlock = (GmmBaseBlock *)id;

   if (!pBaseBlock->isTile)
   {
      GmmBlock *pBlock = (GmmBlock *)id;

      pBlock->isPinned = 1;
   }
}

void gmmUnpinId(const uint32_t id)
{
   GmmBaseBlock    *pBaseBlock = (GmmBaseBlock *)id;

   if (!pBaseBlock->isTile)
   {
      GmmBlock *pBlock = (GmmBlock *)id;

      pBlock->isPinned = 0;
   }
}

static GmmBlock *gmmAllocBlock(
      GmmAllocator *pAllocator,
      uint32_t size
      )
{
   uint32_t    address;
   GmmBlock    *pNewBlock = NULL;
   GmmBlock    *pBlock = pAllocator->pTail;

   address = pAllocator->freeAddress;

   if (UINT_MAX - address >= size && 
         address + size <= pAllocator->startAddress + pAllocator->size)
   {
      pNewBlock = GMM_ALLOC_FIXED_BLOCK();
      if (pNewBlock == NULL)
      {
         return NULL;
      }

      memset(pNewBlock, 0, sizeof(GmmBlock));

      pNewBlock->base.address = address;
      pNewBlock->base.size = size;
      pAllocator->freeAddress = address + size;

      if (pBlock)
      {
         pNewBlock->pPrev = pBlock;
         pBlock->pNext = pNewBlock;
         pAllocator->pTail = pNewBlock;
      }
      else
      {
         pAllocator->pHead = pNewBlock;
         pAllocator->pTail = pNewBlock;
      }
   }

   return pNewBlock;
}

static GmmTileBlock *gmmFindFreeTileBlock(
      GmmAllocator *pAllocator,
      const uint32_t size
      )
{
   GmmTileBlock    *pBlock = pAllocator->pTileHead;
   GmmTileBlock    *pBestAfterBlock = NULL;
   GmmTileBlock    *pNewBlock = NULL;
   uint32_t        bestSize = 0;
   uint32_t        freeSize = 0;

   while (pBlock && pBlock->pNext)
   {
      freeSize = pBlock->pNext->base.address - pBlock->base.address - pBlock->base.size;

      if (freeSize >= size &&
            (pBestAfterBlock == NULL || freeSize < bestSize) &&
            (pBlock->pNext == NULL ||
             pBlock->pData != pBlock->pNext->pData))
      {
         pBestAfterBlock = pBlock;
         bestSize = freeSize;
      }

      pBlock = pBlock->pNext;
   }

   if (pBestAfterBlock)
   {
      pNewBlock = gmmAllocFixedTileBlock();
      if (pNewBlock == NULL)
         return NULL;

      memset(pNewBlock, 0, sizeof(GmmTileBlock));

      pNewBlock->base.address = pBestAfterBlock->base.address + pBestAfterBlock->base.size;
      pNewBlock->base.isTile = 1;
      pNewBlock->base.size = size;

      pNewBlock->pNext = pBestAfterBlock->pNext;
      pNewBlock->pPrev = pBestAfterBlock;
      pNewBlock->pPrev->pNext = pNewBlock;
      pNewBlock->pNext->pPrev = pNewBlock;

      return pNewBlock;
   }
   else
      return NULL;
}

static GmmTileBlock *gmmCreateTileBlock(
      GmmAllocator *pAllocator,
      const uint32_t size
      )
{
   GmmTileBlock    *pNewBlock;
   uint32_t        address;

   address = pAllocator->tileStartAddress - size;

   if (address > pAllocator->startAddress + pAllocator->size)
      return NULL;

   if (pAllocator->pTail &&
         pAllocator->pTail->base.address + pAllocator->pTail->base.size > address)
      return NULL;

   pAllocator->size = address - pAllocator->startAddress;
   pAllocator->tileSize = pAllocator->tileStartAddress + pAllocator->tileSize - address;
   pAllocator->tileStartAddress = address;

   pNewBlock = gmmAllocFixedTileBlock();
   if (pNewBlock == NULL)
      return NULL;

   memset(pNewBlock, 0, sizeof(GmmTileBlock));

   pNewBlock->base.address = address;
   pNewBlock->base.isTile = 1;
   pNewBlock->base.size = size;
   pNewBlock->pNext = pAllocator->pTileHead;

   if (pAllocator->pTileHead)
   {
      pAllocator->pTileHead->pPrev = pNewBlock;
      pAllocator->pTileHead = pNewBlock;
   }
   else
   {
      pAllocator->pTileHead = pNewBlock;
      pAllocator->pTileTail = pNewBlock;
   }

   return pNewBlock;
}

static void gmmFreeTileBlock (void *data)
{
   GmmTileBlock    *pTileBlock = (GmmTileBlock*)data;
   GmmAllocator    *pAllocator;

   if (pTileBlock->pPrev)
      pTileBlock->pPrev->pNext = pTileBlock->pNext;

   if (pTileBlock->pNext)
      pTileBlock->pNext->pPrev = pTileBlock->pPrev;

   pAllocator = pGmmLocalAllocator;

   if (pAllocator->pTileHead == pTileBlock)
   {
      pAllocator->pTileHead = pTileBlock->pNext;

      if (pAllocator->pTileHead)
         pAllocator->pTileHead->pPrev = NULL;

      uint32_t prevSize;
      prevSize = pAllocator->size;
      pAllocator->size = pAllocator->pTileHead ? 
         pAllocator->pTileHead->base.address - pAllocator->startAddress :
         pAllocator->totalSize;
      pAllocator->tileSize = pAllocator->totalSize - pAllocator->size;
      pAllocator->tileStartAddress = pAllocator->pTileHead ?
         pAllocator->pTileHead->base.address :
         pAllocator->startAddress + pAllocator->size;
   }

   if (pAllocator->pTileTail == pTileBlock)
   {
      pAllocator->pTileTail = pTileBlock->pPrev;

      if (pAllocator->pTileTail)
         pAllocator->pTileTail->pNext = NULL;
   }

   gmmFreeFixedTileBlock(pTileBlock);
}

static uint32_t gmmAllocExtendedTileBlock(const uint32_t size, const uint32_t tag)
{
   GmmAllocator    *pAllocator;
   uint32_t        retId = 0;
   uint32_t        newSize;
   uint8_t         resizeSucceed = 1;


   pAllocator = pGmmLocalAllocator;


   newSize = PAD(size, GMM_TILE_ALIGNMENT);

   GmmTileBlock    *pBlock = pAllocator->pTileTail;

   while (pBlock)
   {
      if (pBlock->tileTag == tag)
      {
         GLuint address, tileSize;
         rglGcmGetTileRegionInfo(pBlock->pData, &address, &tileSize);

         if ((pBlock->pNext && pBlock->pNext->base.address-pBlock->base.address-pBlock->base.size >= newSize) ||
               (pBlock->pPrev && pBlock->base.address-pBlock->pPrev->base.address-pBlock->pPrev->base.size >= newSize))
         {
            GmmTileBlock *pNewBlock = gmmAllocFixedTileBlock();
            if (pNewBlock == NULL)
               break;

            retId = (uint32_t)pNewBlock;

            memset(pNewBlock, 0, sizeof(GmmTileBlock));

            pNewBlock->base.isTile = 1;
            pNewBlock->base.size = newSize;

            if (pBlock->pNext && pBlock->pNext->base.address-pBlock->base.address-pBlock->base.size >= newSize)
            {
               pNewBlock->base.address = pBlock->base.address+pBlock->base.size;
               pNewBlock->pNext = pBlock->pNext;
               pNewBlock->pPrev = pBlock;
               pBlock->pNext->pPrev = pNewBlock;
               pBlock->pNext = pNewBlock;

               if (pNewBlock->pPrev->pData != pNewBlock->pNext->pData)
                  resizeSucceed = rglGcmTryResizeTileRegion( address, tileSize+newSize, pBlock->pData );
            }
            else
            {
               pNewBlock->base.address = pBlock->base.address-newSize;
               pNewBlock->pNext = pBlock;
               pNewBlock->pPrev = pBlock->pPrev;
               pBlock->pPrev->pNext = pNewBlock;
               pBlock->pPrev = pNewBlock;

               if (pNewBlock->pPrev->pData != pNewBlock->pNext->pData)
                  resizeSucceed = rglGcmTryResizeTileRegion( (GLuint)gmmIdToOffset((uint32_t)pNewBlock), tileSize+newSize, pBlock->pData );
            }
            gmmSetTileAttrib( retId, tag, pBlock->pData );
            break;
         }

         if (pBlock == pAllocator->pTileHead)
         {
            retId = (uint32_t)gmmCreateTileBlock(pAllocator, newSize); 
            if (retId == 0)
               break;

            resizeSucceed = rglGcmTryResizeTileRegion( (GLuint)gmmIdToOffset(retId), tileSize+newSize, pBlock->pData );
            gmmSetTileAttrib( retId, tag, pBlock->pData );
            break;
         }
      }

      pBlock = pBlock->pPrev;
   }

   if (retId == 0)
      return GMM_ERROR;

   if (!resizeSucceed)
   {
      gmmFreeTileBlock((GmmTileBlock *)retId);
      return GMM_ERROR;
   }

   return retId;
}

static GmmTileBlock *gmmAllocTileBlock(GmmAllocator *pAllocator,
      const uint32_t size)
{
   GmmTileBlock    *pBlock = gmmFindFreeTileBlock(pAllocator, size); 

   if (pBlock == NULL)
      pBlock = gmmCreateTileBlock(pAllocator, size);

   return pBlock;
}

static void gmmFreeBlock (void *data)
{
   GmmBlock *pBlock = (GmmBlock*)data;
   GmmAllocator    *pAllocator;

   if (pBlock->pPrev)
      pBlock->pPrev->pNext = pBlock->pNext;

   if (pBlock->pNext)
      pBlock->pNext->pPrev = pBlock->pPrev;

   pAllocator = pGmmLocalAllocator;

   if (pAllocator->pHead == pBlock)
   {
      pAllocator->pHead = pBlock->pNext;

      if (pAllocator->pHead)
         pAllocator->pHead->pPrev = NULL;
   }

   if (pAllocator->pTail == pBlock)
   {
      pAllocator->pTail = pBlock->pPrev;

      if (pAllocator->pTail)
         pAllocator->pTail->pNext = NULL;
   }

   if (pBlock->pPrev == NULL)
      pAllocator->pSweepHead = pAllocator->pHead;
   else if (pBlock->pPrev &&
         (pAllocator->pSweepHead == NULL || 
          (pAllocator->pSweepHead &&
           pAllocator->pSweepHead->base.address > pBlock->pPrev->base.address)))
      pAllocator->pSweepHead = pBlock->pPrev;

   pAllocator->freedSinceSweep += pBlock->base.size;

   gmmFreeFixedBlock(pBlock);
}

static void gmmAddPendingFree (void *data)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   GmmBlock *pBlock = (GmmBlock*)data;
   GmmAllocator    *pAllocator;

   pAllocator = pGmmLocalAllocator;

   if (pAllocator->pPendingFreeTail)
   {
      pBlock->pNextFree = NULL;
      pBlock->pPrevFree = pAllocator->pPendingFreeTail;
      pAllocator->pPendingFreeTail->pNextFree = pBlock;
      pAllocator->pPendingFreeTail = pBlock;
   }
   else
   {
      pBlock->pNextFree = NULL;
      pBlock->pPrevFree = NULL;
      pAllocator->pPendingFreeHead = pBlock;
      pAllocator->pPendingFreeTail = pBlock;
   }

   pBlock->isPinned = 0;

   ++nvFenceCounter;

   /* semaphore ID : RGLGCM_SEMA_FENCE, new fence value: nvFenceCounter */
   rglGcmSetWriteBackEndLabel(thisContext, RGLGCM_SEMA_FENCE, nvFenceCounter );

   pBlock->fence = nvFenceCounter;
}

static uint8_t gmmSizeToFreeIndex(uint32_t size)
{
   if (size >= GMM_FREE_BIN_0 && size < GMM_FREE_BIN_1)
      return 0;
   else if (size >= GMM_FREE_BIN_1 && size < GMM_FREE_BIN_2)
      return 1;
   else if (size >= GMM_FREE_BIN_2 && size < GMM_FREE_BIN_3)
      return 2;
   else if (size >= GMM_FREE_BIN_3 && size < GMM_FREE_BIN_4)
      return 3;
   else if (size >= GMM_FREE_BIN_4 && size < GMM_FREE_BIN_5)
      return 4;
   else if (size >= GMM_FREE_BIN_5 && size < GMM_FREE_BIN_6)
      return 5;
   else if (size >= GMM_FREE_BIN_6 && size < GMM_FREE_BIN_7)
      return 6;
   else if (size >= GMM_FREE_BIN_7 && size < GMM_FREE_BIN_8)
      return 7;
   else if (size >= GMM_FREE_BIN_8 && size < GMM_FREE_BIN_9)
      return 8;
   else if (size >= GMM_FREE_BIN_9 && size < GMM_FREE_BIN_10)
      return 9;
   else if (size >= GMM_FREE_BIN_10 && size < GMM_FREE_BIN_11)
      return 10;
   else if (size >= GMM_FREE_BIN_11 && size < GMM_FREE_BIN_12)
      return 11;
   else if (size >= GMM_FREE_BIN_12 && size < GMM_FREE_BIN_13)
      return 12;
   else if (size >= GMM_FREE_BIN_13 && size < GMM_FREE_BIN_14)
      return 13;
   else if (size >= GMM_FREE_BIN_14 && size < GMM_FREE_BIN_15)
      return 14;
   else if (size >= GMM_FREE_BIN_15 && size < GMM_FREE_BIN_16)
      return 15;
   else if (size >= GMM_FREE_BIN_16 && size < GMM_FREE_BIN_17)
      return 16;
   else if (size >= GMM_FREE_BIN_17 && size < GMM_FREE_BIN_18)
      return 17;
   else if (size >= GMM_FREE_BIN_18 && size < GMM_FREE_BIN_19)
      return 18;
   else if (size >= GMM_FREE_BIN_19 && size < GMM_FREE_BIN_20)
      return 19;
   else if (size >= GMM_FREE_BIN_20 && size < GMM_FREE_BIN_21)
      return 20;
   else
      return 21;
}

static void gmmAddFree(
      GmmAllocator *pAllocator,
      GmmBlock *pBlock
      )
{
   uint8_t freeIndex = gmmSizeToFreeIndex(pBlock->base.size);

   if (pAllocator->pFreeHead[freeIndex])
   {
      GmmBlock *pInsertBefore = pAllocator->pFreeHead[freeIndex];

      while (pInsertBefore && pInsertBefore->base.size < pBlock->base.size)
         pInsertBefore = pInsertBefore->pNextFree;

      if (pInsertBefore == NULL)
      {
         pBlock->pNextFree = NULL;
         pBlock->pPrevFree = pAllocator->pFreeTail[freeIndex];
         pAllocator->pFreeTail[freeIndex]->pNextFree = pBlock;
         pAllocator->pFreeTail[freeIndex] = pBlock;
      }
      else if (pInsertBefore == pAllocator->pFreeHead[freeIndex])
      {
         pBlock->pNextFree = pInsertBefore;
         pBlock->pPrevFree = pInsertBefore->pPrevFree;
         pInsertBefore->pPrevFree = pBlock;
         pAllocator->pFreeHead[freeIndex] = pBlock;
      }
      else
      {
         pBlock->pNextFree = pInsertBefore;
         pBlock->pPrevFree = pInsertBefore->pPrevFree;
         pInsertBefore->pPrevFree->pNextFree = pBlock;
         pInsertBefore->pPrevFree = pBlock;
      }
   }
   else
   {
      pBlock->pNextFree = NULL;
      pBlock->pPrevFree = NULL;
      pAllocator->pFreeHead[freeIndex] = pBlock;
      pAllocator->pFreeTail[freeIndex] = pBlock;
   }
}

uint32_t gmmFree(const uint32_t freeId)
{
   GmmBaseBlock    *pBaseBlock = (GmmBaseBlock *)freeId;

   if (pBaseBlock->isTile)
   {
      GmmTileBlock *pTileBlock = (GmmTileBlock *)pBaseBlock;

      if (pTileBlock->pPrev &&
            pTileBlock->pNext &&
            pTileBlock->pPrev->pData == pTileBlock->pNext->pData)
      {
      }
      else if (pTileBlock->pPrev && pTileBlock->pPrev->pData == pTileBlock->pData)
      {
         GLuint address, size;

         rglGcmGetTileRegionInfo(pTileBlock->pData, &address, &size);
         if ( !rglGcmTryResizeTileRegion(address, (size-pTileBlock->base.size), pTileBlock->pData) )
         {
            rglGcmTryResizeTileRegion(address, 0, pTileBlock->pData);
            if ( !rglGcmTryResizeTileRegion(address, (size-pTileBlock->base.size), pTileBlock->pData) )
            {
            }
         }
      }
      else if (pTileBlock->pNext && pTileBlock->pNext->pData == pTileBlock->pData)
      {
         GLuint address, size;

         rglGcmGetTileRegionInfo(pTileBlock->pData, &address, &size);
         if ( !rglGcmTryResizeTileRegion((address+pTileBlock->base.size), (size-pTileBlock->base.size), pTileBlock->pData) )
         {
            rglGcmTryResizeTileRegion(address, 0, pTileBlock->pData);
            if ( !rglGcmTryResizeTileRegion((address+pTileBlock->base.size), (size-pTileBlock->base.size), pTileBlock->pData) )
            {
            }
         }
      }
      else
      {
         if ( !rglGcmTryResizeTileRegion( (GLuint)gmmIdToOffset(freeId), 0, gmmGetTileData(freeId) ) )
         {
         }
      }

      gmmFreeTileBlock(pTileBlock);
   }
   else
   {
      GmmBlock *pBlock = (GmmBlock *)pBaseBlock;

      gmmAddPendingFree(pBlock);
   }

   return CELL_OK;
}

static inline void gmmLocalMemcpy(void *data, const uint32_t dstOffset,
      const uint32_t srcOffset, const uint32_t moveSize)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)data;
   int32_t offset = 0;
   int32_t sizeLeft = moveSize;
   int32_t dimension = 4096;

   while (sizeLeft > 0)
   {
      while(sizeLeft >= dimension*dimension*4)
      {
         rglGcmSetTransferImage(gCellGcmCurrentContext,
               CELL_GCM_TRANSFER_LOCAL_TO_LOCAL,
               dstOffset+offset,
               dimension*4,
               0,
               0,
               srcOffset+offset,
               dimension*4,
               0,
               0,
               dimension,
               dimension,
               4);

         offset = offset + dimension*dimension*4;
         sizeLeft = sizeLeft - (dimension*dimension*4);
      }

      dimension = dimension >> 1;

      if (dimension == 32)
         break;
   }

   if (sizeLeft > 0)
   {
      rglGcmSetTransferImage(gCellGcmCurrentContext, 
            CELL_GCM_TRANSFER_LOCAL_TO_LOCAL,
            dstOffset+offset,
            sizeLeft,
            0,
            0,
            srcOffset+offset,
            sizeLeft,
            0,
            0,
            sizeLeft/4,
            1,
            4);
   }
}

static inline void gmmMemcpy(void *data, const uint8_t mode,
      const uint32_t dstOffset, const uint32_t srcOffset,
      const uint32_t moveSize)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)data;

   if (dstOffset + moveSize <= srcOffset)
   {
      gmmLocalMemcpy(thisContext,
            dstOffset,
            srcOffset,
            moveSize);
   }
   else
   {
      uint32_t moveBlockSize = srcOffset-dstOffset;
      uint32_t iterations = (moveSize+moveBlockSize-1)/moveBlockSize;

      for (uint32_t i=0; i<iterations; i++)
      {
         gmmLocalMemcpy(thisContext,
               dstOffset+(i*moveBlockSize),
               srcOffset+(i*moveBlockSize),
               moveBlockSize);
      }
   }
}

static uint8_t gmmInternalSweep(void *data)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)data;
   GmmAllocator    *pAllocator = pGmmLocalAllocator;
   GmmBlock        *pBlock = pAllocator->pSweepHead;
   GmmBlock        *pSrcBlock = pBlock;
   GmmBlock        *pTempBlock;
   GmmBlock        *pTempBlockNext;
   uint32_t        dstAddress = 0;
   uint32_t        srcAddress = 0;
   uint32_t        srcOffset, dstOffset;
   uint32_t        prevEndAddress = 0;
   uint32_t        moveSize, moveDistance;
   uint8_t         mode = CELL_GCM_TRANSFER_LOCAL_TO_LOCAL;
   uint8_t         ret = 0;
   uint32_t        totalMoveSize = 0;

   while (pBlock != NULL)
   {
      if (pBlock->isPinned == 0)
      {
         if (pBlock->pPrev)
            prevEndAddress = pBlock->pPrev->base.address + pBlock->pPrev->base.size;
         else
            prevEndAddress = pAllocator->startAddress;

         if (pBlock->base.address > prevEndAddress)
         {
            dstAddress = prevEndAddress;
            srcAddress = pBlock->base.address;
            pSrcBlock = pBlock;
         }

         moveSize = pBlock->base.address + pBlock->base.size - srcAddress;

         if (srcAddress > dstAddress &&
               (pBlock->pNext == NULL ||
                pBlock->pNext->base.address > pBlock->base.address + pBlock->base.size ||
                pBlock->pNext->isPinned))
         {
            dstOffset = GMM_ADDRESS_TO_OFFSET(dstAddress);
            srcOffset = GMM_ADDRESS_TO_OFFSET(srcAddress);

            totalMoveSize += moveSize;

            gmmMemcpy(thisContext,
                  mode,
                  dstOffset,
                  srcOffset,
                  moveSize);

            pTempBlock = pSrcBlock;

            moveDistance = srcOffset - dstOffset;

            while (pTempBlock != pBlock->pNext)
            {
               pTempBlock->base.address -= moveDistance;
               pTempBlock = pTempBlock->pNext;
            }
         }
      }
      else
      {
         uint32_t availableSize;

         srcAddress = 0;
         dstAddress = 0;

         if (pBlock->pPrev == NULL)
            availableSize = pBlock->base.address - pAllocator->startAddress;
         else
            availableSize = pBlock->base.address - (pBlock->pPrev->base.address + pBlock->pPrev->base.size);

         pTempBlock = pBlock->pNext;

         while (availableSize >= GMM_ALIGNMENT &&
               pTempBlock)
         {
            pTempBlockNext = pTempBlock->pNext;

            if (pTempBlock->isPinned == 0 &&
                  pTempBlock->base.size <= availableSize)
            {
               uint32_t pinDstAddress = (pBlock->pPrev == NULL) ?
                  pAllocator->startAddress :
                  pBlock->pPrev->base.address + pBlock->pPrev->base.size;
               uint32_t pinSrcAddress = pTempBlock->base.address;

               dstOffset = GMM_ADDRESS_TO_OFFSET(pinDstAddress);
               srcOffset = GMM_ADDRESS_TO_OFFSET(pinSrcAddress);

               totalMoveSize += pTempBlock->base.size;

               gmmMemcpy(thisContext,
                     mode,
                     dstOffset,
                     srcOffset,
                     pTempBlock->base.size);

               pTempBlock->base.address = pinDstAddress;

               if (pTempBlock == pAllocator->pTail)
               {
                  if (pTempBlock->pNext)
                     pAllocator->pTail = pTempBlock->pNext;
                  else
                     pAllocator->pTail = pTempBlock->pPrev;
               }

               if (pTempBlock->pNext)
                  pTempBlock->pNext->pPrev = pTempBlock->pPrev;
               if (pTempBlock->pPrev)
                  pTempBlock->pPrev->pNext = pTempBlock->pNext;

               if (pBlock->pPrev)
                  pBlock->pPrev->pNext = pTempBlock;
               else
                  pAllocator->pHead = pTempBlock;
               pTempBlock->pPrev = pBlock->pPrev;
               pTempBlock->pNext = pBlock;
               pBlock->pPrev = pTempBlock;
            }

            if (pBlock->pPrev)
            {
               availableSize = pBlock->base.address - (pBlock->pPrev->base.address + pBlock->pPrev->base.size);
            }

            pTempBlock = pTempBlockNext;
         }

         if (availableSize > 0)
         {
            GmmBlock *pNewBlock = GMM_ALLOC_FIXED_BLOCK();

            if (pNewBlock)
            {
               memset(pNewBlock, 0, sizeof(GmmBlock));
               pNewBlock->base.address = pBlock->base.address - availableSize;
               pNewBlock->base.size = availableSize;
               pNewBlock->pNext = pBlock;
               pNewBlock->pPrev = pBlock->pPrev;
               if (pBlock->pPrev)
                  pBlock->pPrev->pNext = pNewBlock;

               pBlock->pPrev = pNewBlock;

               if (pBlock == pAllocator->pHead)
                  pAllocator->pHead = pNewBlock;

               gmmAddFree(pAllocator, pNewBlock);

               ret = 1;
            }
         }
      }

      pBlock = pBlock->pNext;
   }

   uint32_t newFreeAddress = pAllocator->pTail ? 
      pAllocator->pTail->base.address + pAllocator->pTail->base.size :
      pAllocator->startAddress;

   if (pAllocator->freeAddress != newFreeAddress)
   {
      pAllocator->freeAddress = newFreeAddress;
      ret = 1;
   }

   pAllocator->freedSinceSweep = 0;
   pAllocator->pSweepHead = NULL;

   return ret;
}

static void gmmRemovePendingFree (GmmAllocator *pAllocator,
      GmmBlock *pBlock)
{
   if (pBlock == pAllocator->pPendingFreeHead)
      pAllocator->pPendingFreeHead = pBlock->pNextFree;

   if (pBlock == pAllocator->pPendingFreeTail)
      pAllocator->pPendingFreeTail = pBlock->pPrevFree;

   if (pBlock->pNextFree)
      pBlock->pNextFree->pPrevFree = pBlock->pPrevFree;

   if (pBlock->pPrevFree)
      pBlock->pPrevFree->pNextFree = pBlock->pNextFree;
}

static void gmmFreeAll(void)
{
   GmmBlock        *pBlock;
   GmmBlock        *pTemp;
   GmmAllocator *pAllocator =  pGmmLocalAllocator;

   pBlock = pAllocator->pPendingFreeHead;
   while (pBlock)
   {
      pTemp = pBlock->pNextFree;
      gmmFreeBlock(pBlock);
      pBlock = pTemp;
   }
   pAllocator->pPendingFreeHead = NULL;
   pAllocator->pPendingFreeTail = NULL;

   for (int i=0; i<GMM_NUM_FREE_BINS; i++)
   {
      pBlock = pAllocator->pFreeHead[i];
      while (pBlock)
      {
         pTemp = pBlock->pNextFree;
         gmmFreeBlock(pBlock);
         pBlock = pTemp;
      }
      pAllocator->pFreeHead[i] = NULL;
      pAllocator->pFreeTail[i] = NULL;
   }
}

static void gmmAllocSweep(void *data)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   gmmFreeAll();

   if (gmmInternalSweep(thisContext))
   {
      *pLock = 1;
      cachedLockValue = 1;
      rglGcmSetWriteBackEndLabel(thisContext, GMM_PPU_WAIT_INDEX, 0);

      cellGcmFlush(thisContext);
   }
}

static uint32_t gmmInternalAlloc(
      GmmAllocator *pAllocator,
      const uint8_t isTile,
      const uint32_t size
      )
{
   uint32_t        retId;

   if (isTile)
      retId = (uint32_t)gmmAllocTileBlock(pAllocator, size);
   else
      retId = (uint32_t)gmmAllocBlock(pAllocator, size);

   if (retId == 0)
      return GMM_ERROR;

   return retId;
}

static void gmmRemoveFree(
      GmmAllocator *pAllocator,
      GmmBlock *pBlock,
      uint8_t freeIndex
      )
{
   if (pBlock == pAllocator->pFreeHead[freeIndex])
      pAllocator->pFreeHead[freeIndex] = pBlock->pNextFree;

   if (pBlock == pAllocator->pFreeTail[freeIndex])
      pAllocator->pFreeTail[freeIndex] = pBlock->pPrevFree;

   if (pBlock->pNextFree)
      pBlock->pNextFree->pPrevFree = pBlock->pPrevFree;

   if (pBlock->pPrevFree)
      pBlock->pPrevFree->pNextFree = pBlock->pNextFree;
}

static uint32_t gmmFindFreeBlock(
      GmmAllocator    *pAllocator,
      uint32_t        size
      )
{
   uint32_t        retId = GMM_ERROR;
   GmmBlock        *pBlock;
   uint8_t         found = 0;
   uint8_t         freeIndex = gmmSizeToFreeIndex(size);

   pBlock = pAllocator->pFreeHead[freeIndex];

   while (freeIndex < GMM_NUM_FREE_BINS)
   {
      if (pBlock)
      {
         if (pBlock->base.size >= size)
         {
            found = 1;
            break;
         }
         pBlock = pBlock->pNextFree;
      }
      else if (++freeIndex < GMM_NUM_FREE_BINS)
         pBlock = pAllocator->pFreeHead[freeIndex];
   }

   if (found)
   {
      if (pBlock->base.size != size)
      {
         GmmBlock *pNewBlock = GMM_ALLOC_FIXED_BLOCK();
         if (pNewBlock == NULL)
            return GMM_ERROR;

         memset(pNewBlock, 0, sizeof(GmmBlock));
         pNewBlock->base.address = pBlock->base.address + size;
         pNewBlock->base.size = pBlock->base.size - size;
         pNewBlock->pNext = pBlock->pNext;
         pNewBlock->pPrev = pBlock;
         if (pBlock->pNext)
            pBlock->pNext->pPrev = pNewBlock;
         pBlock->pNext = pNewBlock;

         if (pBlock == pAllocator->pTail)
            pAllocator->pTail = pNewBlock;

         gmmAddFree(pAllocator, pNewBlock);
      }
      pBlock->base.size = size;
      gmmRemoveFree(pAllocator, pBlock, freeIndex);
      retId = (uint32_t)pBlock;
   }

   return retId;
}

uint32_t gmmAlloc(void *data,
      const uint8_t isTile, const uint32_t size)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)data;
   GmmAllocator    *pAllocator;
   uint32_t        retId;
   uint32_t        newSize;

   if (__builtin_expect((size == 0),0))
      return GMM_ERROR;

   pAllocator =  pGmmLocalAllocator;

   if (!isTile)
   {
      newSize = PAD(size, GMM_ALIGNMENT);

      retId = gmmFindFreeBlock(pAllocator, newSize);
   }
   else
   {
      newSize = PAD(size, GMM_TILE_ALIGNMENT);
      retId = GMM_ERROR;
   }

   if (retId == GMM_ERROR)
   {
      retId = gmmInternalAlloc(pAllocator,
            isTile,
            newSize);

      if (retId == GMM_ERROR)
      {
         gmmAllocSweep(thisContext);

         retId = gmmInternalAlloc(pAllocator,
               isTile,
               newSize);

         if (!isTile &&
               retId == GMM_ERROR)
            retId = gmmFindFreeBlock(pAllocator, newSize);
      }
   }

   return retId;
}

/*============================================================
  FIFO BUFFER
  ============================================================ */

void rglGcmFifoFinish (void *data)
{
   rglGcmFifo *fifo = (rglGcmFifo*)data;
   GLuint ref = rglGcmFifoPutReference( fifo );

   rglGcmFifoFlush( fifo );

   while (rglGcmFifoReferenceInUse(fifo, ref))
      sys_timer_usleep(10);
}

void rglGcmFifoFlush (void *data)
{
   rglGcmFifo *fifo = (rglGcmFifo*)data;
   unsigned int offsetInBytes = 0;

   cellGcmAddressToOffset( fifo->current, ( uint32_t * )&offsetInBytes );

   cellGcmFlush();

   fifo->dmaControl->Put = offsetInBytes;
   fifo->lastPutWritten = fifo->current;

   fifo->lastSWReferenceFlushed = fifo->lastSWReferenceWritten;
}

GLuint rglGcmFifoPutReference (void *data)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   rglGcmFifo *fifo = (rglGcmFifo*)data;
   fifo->lastSWReferenceWritten++;

   rglGcmSetReferenceCommand(thisContext, fifo->lastSWReferenceWritten );

   if (( fifo->lastSWReferenceWritten & 0x7fffffff ) == 0 )
      rglGcmFifoFinish( fifo );

   return fifo->lastSWReferenceWritten;
}

GLuint rglGcmFifoReadReference (void *data)
{
   rglGcmFifo *fifo = (rglGcmFifo*)data;
   GLuint ref = *((volatile GLuint *)&fifo->dmaControl->Reference);
   fifo->lastHWReferenceRead = ref;
   return ref;
}

GLboolean rglGcmFifoReferenceInUse (void *data, GLuint reference)
{
   rglGcmFifo *fifo = (rglGcmFifo*)data;
   // compare against cached hw ref value (accounting wrap)
   if ( !(( fifo->lastHWReferenceRead - reference ) & 0x80000000 ) )
      return GL_FALSE;

   // has the reference already been flushed out ?
   if (( fifo->lastSWReferenceFlushed - reference ) & 0x80000000 )
      rglGcmFifoFlush( fifo );

   // read current hw reference
   rglGcmFifoReadReference( fifo );

   // compare against hw ref value (accounting wrap)
   if ( !(( fifo->lastHWReferenceRead - reference ) & 0x80000000 ) )
      return GL_FALSE;

   return GL_TRUE;
}

void rglGcmFifoInit (void *data, void *dmaControl, unsigned long dmaPushBufferOffset, uint32_t*dmaPushBuffer,
      GLuint dmaPushBufferSize )
{
   rglGcmFifo *fifo = (rglGcmFifo*)data;

   // init fifoBlockSize
   fifo->fifoBlockSize = DEFAULT_FIFO_BLOCK_SIZE;

   // init fifo context pointers to first fifo block which will be set at the the dmaPushPuffer position
   fifo->begin     = (uint32_t*) dmaPushBuffer;
   fifo->end       = fifo->begin + ( fifo->fifoBlockSize / sizeof( uint32_t ) ) - 1;
   // init rest of context
   fifo->current        = fifo->begin;
   fifo->lastGetRead    = fifo->current;
   fifo->lastPutWritten = fifo->current;

   // store fifo values
   fifo->dmaPushBufferBegin       = dmaPushBuffer;
   fifo->dmaPushBufferEnd         = (uint32_t*)(( size_t )dmaPushBuffer + dmaPushBufferSize ) - 1;
   fifo->dmaControl               = (rglGcmControlDma*)dmaControl;
   fifo->dmaPushBufferOffset      = dmaPushBufferOffset;
   fifo->dmaPushBufferSizeInWords = dmaPushBufferSize / sizeof(uint32_t);

   fifo->lastHWReferenceRead    = 0;
   fifo->lastSWReferenceWritten = 0;
   fifo->lastSWReferenceFlushed = 0;

   // note that rglGcmFifo is-a CellGcmContextData
   gCellGcmCurrentContext = fifo;
   // setting our own out of space callback here to handle our fifo
   gCellGcmCurrentContext->callback = ( CellGcmContextCallback )rglOutOfSpaceCallback;

   // ensure the ref is initted to 0.
   if ( rglGcmFifoReadReference( fifo ) != 0 )
   {
      GCM_FUNC( cellGcmSetReferenceCommand, 0 );
      rglGcmFifoFlush( fifo ); // Here, we jump to this new buffer

      // a finish that waits for 0 specifically.
      while (rglGcmFifoReadReference(fifo) != 0)
         sys_timer_usleep(10);
   }
   fifo->dmaPushBufferGPU = dmaPushBuffer;
   fifo->spuid = 0;

}

/*============================================================
  GL INITIALIZATION
  ============================================================ */

void rglGcmSetOpenGLState (void *data)
{
   rglGcmState *rglGcmSt = (rglGcmState*)data;
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   GLuint i;

   // initialize the default OpenGL state
   GCM_FUNC( cellGcmSetBlendColor, 0, 0);
   GCM_FUNC( cellGcmSetBlendEquation, RGLGCM_FUNC_ADD, RGLGCM_FUNC_ADD );
   GCM_FUNC( cellGcmSetBlendFunc, RGLGCM_ONE, RGLGCM_ZERO, RGLGCM_ONE, RGLGCM_ZERO );
   rglGcmSetClearColor(thisContext, 0 );
   GCM_FUNC( cellGcmSetBlendEnable, RGLGCM_FALSE );
   GCM_FUNC( cellGcmSetBlendEnableMrt, RGLGCM_FALSE, RGLGCM_FALSE, RGLGCM_FALSE );
   GCM_FUNC( cellGcmSetFragmentProgramGammaEnable, RGLGCM_FALSE );

   for ( i = 0; i < RGLGCM_ATTRIB_COUNT; i++ )
   {
      GCM_FUNC( cellGcmSetVertexDataArray, i, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
   }

   rglGcmSetDitherEnable(thisContext, RGLGCM_TRUE );

   for ( i = 0; i < RGLGCM_MAX_TEXIMAGE_COUNT; i++ )
   {
      static const GLuint borderColor = 0;

      // update the setTextureAddress Portion
      GCM_FUNC( cellGcmSetTextureAddress, i, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_NEVER, 0 );

      // update the setTextureFilter Portion
      GCM_FUNC( cellGcmSetTextureFilter, i, 0, CELL_GCM_TEXTURE_NEAREST_LINEAR, CELL_GCM_TEXTURE_LINEAR,
            CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );

      // update the texture control to setup anisotropic settings
      rglGcmSetTextureControl(thisContext, i, CELL_GCM_TRUE, 0, 12 << 8, CELL_GCM_TEXTURE_MAX_ANISO_1 );

      // update border color
      GCM_FUNC( cellGcmSetTextureBorderColor, i, borderColor );
   }

   // Set zNear and zFar to the default 0.0f and 1.0f here
   rglGcmViewportState *v = &rglGcmState_i.state.viewport;
   v->x = 0;
   v->y = 0;
   v->w = RGLGCM_MAX_RT_DIMENSION;
   v->h = RGLGCM_MAX_RT_DIMENSION;
   rglGcmFifoGlViewport(v, 0.0f, 1.0f );
}

GLboolean rglGcmInitFromRM( rglGcmResource *rmResource )
{
   rglGcmState *rglGcmSt = &rglGcmState_i;
   memset( rglGcmSt, 0, sizeof( *rglGcmSt ) );

   rglGcmSt->localAddress = rmResource->localAddress;
   rglGcmSt->hostMemoryBase = rmResource->hostMemoryBase;
   rglGcmSt->hostMemorySize = rmResource->hostMemorySize;

   rglGcmSt->hostNotifierBuffer = NULL; //rmResource->hostNotifierBuffer;
   rglGcmSt->semaphores = rmResource->semaphores;

   rglGcmFifoInit( &rglGcmSt->fifo, rmResource->dmaControl, rmResource->dmaPushBufferOffset, (uint32_t*)rmResource->dmaPushBuffer, rmResource->dmaPushBufferSize );

   rglGcmFifoFinish( &rglGcmSt->fifo );

   // Set the GPU to a known state
   rglGcmSetOpenGLState(rglGcmSt);

   // wait for setup to complete
   rglGcmFifoFinish(&rglGcmSt->fifo);

   return GL_TRUE;
}

void rglGcmDestroy(void)
{
   rglGcmState *rglGcmSt = &rglGcmState_i;
   memset( rglGcmSt, 0, sizeof( *rglGcmSt ) );
}

/*============================================================
  GCM UTILITIES
  ============================================================ */

static GLuint MemoryClock = 0;
static GLuint GraphicsClock = 0;

GLuint rglGcmGetMemoryClock(void)
{
   return MemoryClock;
}

GLuint rglGcmGetGraphicsClock(void)
{
   return GraphicsClock;
}

GLboolean rglGcmInit (void *opt_data, void *res_data)
{
   rglGcmResource *resource = (rglGcmResource*)res_data;
   RGLinitOptions *options = (RGLinitOptions*)opt_data;
   if ( !rglGcmInitFromRM( resource ) )
   {
      fprintf( stderr, "RGL GCM failed initialisation" );
      return GL_FALSE;
   }
   MemoryClock = resource->MemoryClock;
   GraphicsClock = resource->GraphicsClock;

   if ( gmmInit( resource->localAddress, // pass in the base address, which "could" diff from start address
            resource->localAddress,
            resource->localSize) == GMM_ERROR )
   {
      fprintf( stderr, "Could not init GPU memory manager" );
      rglGcmDestroy();
      return GL_FALSE;
   }

   // initialize DMA sync mechanism
   // use one semaphore to implement fence
   rglGcmSemaphoreMemory *semaphores = rglGcmState_i.semaphores;
   semaphores->userSemaphores[RGLGCM_SEMA_FENCE].val = nvFenceCounter;

   // also need to init the labelValue for waiting for idle 
   rglGcmState_i.labelValue = 1; 

   return GL_TRUE;
}

void rglGcmAllocDestroy()
{
   gmmDestroy();

   rglGcmDestroy();
}

void rglGcmSend( unsigned int dstId, unsigned dstOffset, unsigned int pitch,
      const char *src, unsigned int size )
{
   // try allocating the whole block in the bounce buffer
   GLuint id = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
         0, size);

   memcpy( gmmIdToAddress(id), src, size );
   rglGcmTransferData( dstId, dstOffset, size, id, 0, size, size, 1 );

   gmmFree( id );
}

/*============================================================
  PLATFORM INITIALIZATION
  ============================================================ */

// resc is enabled by setting ANY of the resc related device parameters (using the enable mask)
static inline int rescIsEnabled (void *data)
{
   RGLdeviceParameters *params = (RGLdeviceParameters*)data;
   return params->enable & ( RGL_DEVICE_PARAMETERS_RESC_RENDER_WIDTH_HEIGHT |
         RGL_DEVICE_PARAMETERS_RESC_RATIO_MODE |
         RGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE |
         RGL_DEVICE_PARAMETERS_RESC_INTERLACE_MODE |
         RGL_DEVICE_PARAMETERS_RESC_ADJUST_ASPECT_RATIO );
}

/*============================================================
  DEVICE CONTEXT CREATION
  ============================================================ */

static const RGLdeviceParameters defaultParameters =
{
enable: 0,
        colorFormat: GL_ARGB_SCE,
        depthFormat: GL_NONE,
        multisamplingMode: GL_MULTISAMPLING_NONE_SCE,
        TVStandard: RGL_TV_STANDARD_NONE,
        connector: RGL_DEVICE_CONNECTOR_NONE,
        bufferingMode: RGL_BUFFERING_MODE_DOUBLE,
        width: 0,
        height: 0,
        renderWidth: 0,
        renderHeight: 0,
        rescRatioMode: RESC_RATIO_MODE_LETTERBOX,
        rescPalTemporalMode: RESC_PAL_TEMPORAL_MODE_50_NONE,
        rescInterlaceMode: RESC_INTERLACE_MODE_NORMAL_BILINEAR,
        horizontalScale: 1.0f,
        verticalScale: 1.0f
};

static int rglInitCompleted = 0;

void rglPsglPlatformInit (void *data)
{
   RGLinitOptions *options = (RGLinitOptions*)data;

   if ( !rglInitCompleted )
   {
      cellSysmoduleLoadModule( CELL_SYSMODULE_GCM_SYS );
      cellSysmoduleLoadModule( CELL_SYSMODULE_RESC );

      rglDeviceInit( options );
      _CurrentContext = NULL;
      _CurrentDevice = NULL;
   }

   rglInitCompleted = 1;
}

void rglPsglPlatformExit(void)
{
   RGLcontext* LContext = _CurrentContext;
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;

   if ( LContext )
   {
      rglGcmSetInvalidateVertexCache(thisContext);
      rglGcmFifoFlush( &rglGcmState_i.fifo );

      psglMakeCurrent( NULL, NULL );
      rglDeviceExit();

      _CurrentContext = NULL; 

      rglInitCompleted = 0;
   }
}

RGL_EXPORT RGLdevice*	rglPlatformCreateDeviceAuto( GLenum colorFormat, GLenum depthFormat, GLenum multisamplingMode )
{
   RGLdeviceParameters parameters;
   parameters.enable = RGL_DEVICE_PARAMETERS_COLOR_FORMAT | RGL_DEVICE_PARAMETERS_DEPTH_FORMAT | RGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
   parameters.colorFormat = colorFormat;
   parameters.depthFormat = depthFormat;
   parameters.multisamplingMode = multisamplingMode;
   return psglCreateDeviceExtended( &parameters );
}

RGL_EXPORT RGLdevice*	rglPlatformCreateDeviceExtended (const void *data)
{
   RGLdeviceParameters *parameters = (RGLdeviceParameters*)data;
   RGLdevice *device = (RGLdevice*)malloc( sizeof( RGLdevice ) + sizeof( rglGcmDevice ) );
   if ( !device )
   {
      rglSetError( GL_OUT_OF_MEMORY );
      return NULL;
   }
   memset( device, 0, sizeof( RGLdevice ) + sizeof(rglGcmDevice) );

   // initialize fields
   memcpy( &device->deviceParameters, parameters, sizeof( RGLdeviceParameters ) );

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_COLOR_FORMAT ) == 0 )
      device->deviceParameters.colorFormat = defaultParameters.colorFormat;

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_DEPTH_FORMAT ) == 0 )
      device->deviceParameters.depthFormat = defaultParameters.depthFormat;

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE ) == 0 )
      device->deviceParameters.multisamplingMode = defaultParameters.multisamplingMode;

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_TV_STANDARD ) == 0 )
      device->deviceParameters.TVStandard = defaultParameters.TVStandard;

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_CONNECTOR ) == 0 )
      device->deviceParameters.connector = defaultParameters.connector;

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_BUFFERING_MODE ) == 0 )
      device->deviceParameters.bufferingMode = defaultParameters.bufferingMode;

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_WIDTH_HEIGHT ) == 0 )
   {
      device->deviceParameters.width = defaultParameters.width;
      device->deviceParameters.height = defaultParameters.height;
   }

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_RESC_RENDER_WIDTH_HEIGHT ) == 0 )
   {
      device->deviceParameters.renderWidth = defaultParameters.renderWidth;
      device->deviceParameters.renderHeight = defaultParameters.renderHeight;
   }
   
   if (( parameters->enable & RGL_DEVICE_PARAMETERS_RESC_RATIO_MODE ) == 0 )
      device->deviceParameters.rescRatioMode = defaultParameters.rescRatioMode;

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE ) == 0 )
      device->deviceParameters.rescPalTemporalMode = defaultParameters.rescPalTemporalMode;

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_RESC_INTERLACE_MODE ) == 0 )
      device->deviceParameters.rescInterlaceMode = defaultParameters.rescInterlaceMode;

   if (( parameters->enable & RGL_DEVICE_PARAMETERS_RESC_ADJUST_ASPECT_RATIO ) == 0 )
   {
      device->deviceParameters.horizontalScale = defaultParameters.horizontalScale;
      device->deviceParameters.verticalScale = defaultParameters.verticalScale;
   }

   device->rasterDriver = NULL;

   // platform-specific initialization
   //  This creates the default framebuffer.
   if ( rglPlatformCreateDevice(device) < 0 )
   {
      free( device );
      return NULL;
   }
   return device;
}

RGL_EXPORT GLfloat rglPlatformGetDeviceAspectRatio (const void *data)
{
   const RGLdevice *device = (const RGLdevice*)data;
   CellVideoOutState videoState;
   cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState);

   switch (videoState.displayMode.aspect){
      case CELL_VIDEO_OUT_ASPECT_4_3:  return 4.0f/3.0f;
      case CELL_VIDEO_OUT_ASPECT_16_9: return 16.0f/9.0f;
   };

   return 16.0f/9.0f;
}

/*============================================================
  DEVICE CONTEXT INITIALIZATION
  ============================================================ */

#define RGLGCM_DMA_PUSH_BUFFER_PREFETCH_PADDING 0x1000 // 4KB
#define RGLGCM_FIFO_SIZE (64<<10) // 64 kb

// allocation handles
#define RGLGCM_CHANNEL_HANDLE_ID                     0xFACE0001
#define RGLGCM_FRAME_BUFFER_OBJECT_HANDLE_ID         0xFACE0002
#define RGLGCM_HOST_BUFFER_OBJECT_HANDLE_ID          0xFACE0003

#define RGLGCM_PUSHBUF_MEMORY_HANDLE_ID              0xBEEF1000
#define RGLGCM_HOST_NOTIFIER_MEMORY_HANDLE_ID        0xBEEF1001
#define RGLGCM_VID_NOTIFIER_MEMORY_HANDLE_ID         0xBEEF1002
#define RGLGCM_SEMAPHORE_MEMORY_HANDLE_ID            0xBEEF1003

// dma handles
#define RGLGCM_CHANNEL_DMA_SCRATCH_NOTIFIER          0xBEEF2000
#define RGLGCM_CHANNEL_DMA_ERROR_NOTIFIER            0xBEEF2001
#define RGLGCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER       0xBEEF2002
#define RGLGCM_CONTEXT_DMA_FROM_MEMORY_PUSHBUF       0xBEEF2003
#define RGLGCM_CONTEXT_DMA_TO_MEMORY_GET_REPORT      0xBEEF2004
#define RGLGCM_CONTEXT_DMA_MEMORY_HOST_BUFFER        0xBEEF2005
#define RGLGCM_CONTEXT_DMA_MEMORY_SEMAPHORE_RW       0xBEEF2006
#define RGLGCM_CONTEXT_DMA_MEMORY_SEMAPHORE_RO       0xBEEF2007

// clas contexts
#define RGLGCM_CURIE_PRIMITIVE                       0xBEEF4097
#define RGLGCM_MEM2MEM_HOST_TO_VIDEO                 0xBBBB0000
#define RGLGCM_IMAGEFROMCPU                          0xBBBB1000
#define RGLGCM_SCALEDIMAGE                           0xBBBB1001
#define RGLGCM_CONTEXT_2D_SURFACE                    0xBBBB2000
#define RGLGCM_CONTEXT_SWIZ_SURFACE                  0xBBBB2001

int32_t rglOutOfSpaceCallback (void *data, uint32_t spaceInWords)
{
   struct CellGcmContextData *fifoContext = (struct CellGcmContextData*)data;
   rglGcmFifo * fifo = &rglGcmState_i.fifo;

   // make sure that the space requested will actually fit in to
   // a single fifo block! 

   // auto flush
   cellGcmFlushUnsafeInline((CellGcmContextData*)fifo);

   uint32_t *nextbegin, *nextend, nextbeginoffset, nextendoffset;

   fifo->updateLastGetRead(); 

   // If the current end isn't the same as the full fifo end we 
   // aren't at the end.  Just go ahead and set the next begin and end 
   if(fifo->end != fifo->dmaPushBufferEnd)
      nextbegin = (uint32_t *)fifo->end + 1; 
   else
      nextbegin = (uint32_t *)fifo->dmaPushBufferBegin;
   nextend = nextbegin + (fifo->fifoBlockSize)/sizeof(uint32_t) - 1;

   cellGcmAddressToOffset(nextbegin, &nextbeginoffset);
   cellGcmAddressToOffset(nextend, &nextendoffset);

   //use this version so as not to trigger another callback
   cellGcmSetJumpCommandUnsafeInline((CellGcmContextData*)fifo, nextbeginoffset);


   //set up new context
   fifo->begin = nextbegin;
   fifo->current = nextbegin;
   fifo->end = nextend;

   //if Gpu busy with the new area, stall and flush
   uint32_t get = fifo->dmaControl->Get;

   void * getEA = NULL; 

   cellGcmIoOffsetToAddress( get, &getEA ); 

   // We are going to stall on 3 things 
   // 1. If the get is still with in the new fifo block area
   // 2. If the get is in gcm's initiazation fifo block 
   // 3. If the user stall call back returns RGL_STALL/true that 
   // 3A. the get is in one of their called SCB fifos AND 3B. the last call 
   // position in RGL's fifo is in the next block we want to jump to
   // we have to stall... few!  [RSTENSON] 
   while(((get >= nextbeginoffset) && (get <= nextendoffset)) 
         || ((0 <= get) && (get < 0x10000))) 
   {
      get = fifo->dmaControl->Get;
      cellGcmIoOffsetToAddress( get, &getEA ); 
   }

   // need to add some nops here at the beginning for a issue with the get and the put being at the 
   // same position when the fifo is in GPU memory. 
   for ( GLuint i = 0; i < 8; i++ )
   {
      fifo->current[0] = 0;
      fifo->current++;
   }

   return CELL_OK;
}

void rglGcmDestroyRM( rglGcmResource* gcmResource )
{
   if ( gcmResource->hostMemoryBase ) 
      free( gcmResource->hostMemoryBase );

   memset(( void* )gcmResource, 0, sizeof( rglGcmResource ) );

   return;
}

int rglGcmInitRM( rglGcmResource *gcmResource, unsigned int hostMemorySize, int inSysMem, unsigned int dmaPushBufferSize )
{
   memset( gcmResource, 0, sizeof( rglGcmResource ) );

   // XXX currently we need to decide how much host memory is needed before we know the GPU type
   // It sucks because we don't know if the push buffer is in host memory or not.
   // So, assume that it is...
   dmaPushBufferSize = rglPad( dmaPushBufferSize, RGLGCM_HOST_BUFFER_ALIGNMENT );

   // in case of host push buffer we need to add padding to avoid GPU push buffer prefetch to
   // cause a problem fetching invalid addresses at the end of the push buffer.
   gcmResource->hostMemorySize = rglPad( RGLGCM_FIFO_SIZE + hostMemorySize + dmaPushBufferSize + RGLGCM_DMA_PUSH_BUFFER_PREFETCH_PADDING + (RGLGCM_LM_MAX_TOTAL_QUERIES * sizeof( GLuint )), 1 << 20 );

   if ( gcmResource->hostMemorySize > 0 )
      gcmResource->hostMemoryBase = (char *)memalign( 1 << 20, gcmResource->hostMemorySize  );

   // Initialize RSXIF
   // 64 KB is minimum fifo size for libgpu (for now)
   if (cellGcmInit( RGLGCM_FIFO_SIZE, gcmResource->hostMemorySize, gcmResource->hostMemoryBase ) != 0)
   {
      fprintf( stderr, "RSXIF failed initialization\n" );
      return GL_FALSE;
   }

   // Get Gpu configuration
   CellGcmConfig config;
   cellGcmGetConfiguration( &config );

   gcmResource->localAddress = ( char * )config.localAddress;
   gcmResource->localSize = config.localSize;
   gcmResource->MemoryClock = config.memoryFrequency;
   gcmResource->GraphicsClock = config.coreFrequency;

   gcmResource->semaphores = ( rglGcmSemaphoreMemory * )cellGcmGetLabelAddress( 0 );
   gcmResource->dmaControl = ( char* ) cellGcmGetControlRegister() - (( char * ) & (( rglGcmControlDma* )0 )->Put - ( char * )0 );
   int hostPushBuffer = 0;
   hostPushBuffer = 1;

   // the IOIF mapping don't work. work-around here.
   for ( GLuint i = 0;i < 32;++i ) gcmResource->ioifMappings[i] = ( unsigned long long )( unsigned long )( gcmResource->localAddress + ( 64 << 20 ) * ( i / 4 ) );

   cellGcmFinish( 1 ); // added just a constant value for now to adjust to the inline libgcm interface change

   if ( hostPushBuffer )
   {
      gcmResource->hostMemorySize -= dmaPushBufferSize + RGLGCM_DMA_PUSH_BUFFER_PREFETCH_PADDING;
      gcmResource->dmaPushBuffer = gcmResource->hostMemoryBase + gcmResource->hostMemorySize;
      gcmResource->dmaPushBufferOffset = ( char * )gcmResource->dmaPushBuffer - ( char * )gcmResource->hostMemoryBase;
      gcmResource->linearMemory = ( char* )0x0;
      gcmResource->persistentMemorySize = gcmResource->localSize;
   }
   else
   {
      // Allocate Fifo at begining of vmem map
      gcmResource->dmaPushBuffer = gcmResource->localAddress;
      gcmResource->dmaPushBufferOffset = ( char * )gcmResource->dmaPushBuffer - ( char * )gcmResource->localAddress;
      gcmResource->linearMemory = ( char* )0x0 + dmaPushBufferSize;
      gcmResource->persistentMemorySize = gcmResource->localSize - dmaPushBufferSize;
   }
   gcmResource->dmaPushBufferSize = dmaPushBufferSize;
   gcmResource->hostMemoryReserved = RGLGCM_FIFO_SIZE;

   // Set Jump command to our fifo structure
   cellGcmSetJumpCommand(( char * )gcmResource->dmaPushBuffer - ( char * )gcmResource->hostMemoryBase );

   // Set our Fifo functions
   gCellGcmCurrentContext->callback = ( CellGcmContextCallback )rglOutOfSpaceCallback;

   fprintf(stderr, "RGLGCM resource: MClk: %f Mhz NVClk: %f Mhz\n", ( float )gcmResource->MemoryClock / 1E6, ( float )gcmResource->GraphicsClock / 1E6 );
   fprintf(stderr, "RGLGCM resource: Video Memory: %i MB\n", gcmResource->localSize / ( 1024*1024 ) );
   fprintf(stderr, "RGLGCM resource: localAddress mapped at %p\n", gcmResource->localAddress );
   fprintf(stderr, "RGLGCM resource: push buffer at %p - %p (size = 0x%X), offset=0x%lx\n",
         gcmResource->dmaPushBuffer, ( char* )gcmResource->dmaPushBuffer + gcmResource->dmaPushBufferSize, gcmResource->dmaPushBufferSize, gcmResource->dmaPushBufferOffset );
   fprintf(stderr, "RGLGCM resource: dma control at %p\n", gcmResource->dmaControl );

   return 1;
}

/*============================================================
  DEVICE CONTEXT IMPLEMENTATION
  ============================================================ */

// tiled memory manager
typedef struct
{
   int id;
   GLuint offset;
   GLuint size;        // 0 size indicates an unused tile
   GLuint pitch;       // in bytes
   GLuint bank;
} rglTiledRegion;

typedef struct
{
   rglTiledRegion region[RGLGCM_MAX_TILED_REGIONS];
}
rglTiledMemoryManager;


// TODO: put in device state?
static rglTiledMemoryManager rglGcmTiledMemoryManager;

static rglGcmResource rglGcmResource;


void rglGcmTiledMemoryInit( void )
{
   rglTiledMemoryManager* mm = &rglGcmTiledMemoryManager;
   int32_t retVal;

   memset( mm->region, 0, sizeof( mm->region ) );
   for ( int i = 0;i < RGLGCM_MAX_TILED_REGIONS;++i )
      retVal = cellGcmUnbindTile( i );
}

GLboolean rglPlatformDeviceInit (void *data)
{
   RGLinitOptions *options = (RGLinitOptions*)data;
   GLuint fifoSize = RGLGCM_FIFO_SIZE_DEFAULT;
   GLuint hostSize = RGLGCM_HOST_SIZE_DEFAULT;

   if ( options != NULL )
   {
      if ( options->enable & RGL_INIT_FIFO_SIZE )
         fifoSize = options->fifoSize;
      if ( options->enable & RGL_INIT_HOST_MEMORY_SIZE )
         hostSize = options->hostMemorySize;
   }

   if ( !rglGcmInitRM( &rglGcmResource, hostSize, 0, fifoSize ) )
   {
      fprintf( stderr, "RM resource failed initialisation\n" );
      return GL_FALSE;
   }

   return rglGcmInit( options, &rglGcmResource );
}


void rglPlatformDeviceExit (void)
{
   rglGcmDestroy();
   rglGcmDestroyRM( &rglGcmResource );
}

/////////////////////////////////////////////////////////////////////////////

static unsigned int validPitch[] =
{
   0x0200,
   0x0300,
   0x0400,
   0x0500,
   0x0600,
   0x0700,
   0x0800,
   0x0A00,
   0x0C00,
   0x0D00,
   0x0E00,
   0x1000,
   0x1400,
   0x1800,
   0x1A00,
   0x1C00,
   0x2000,
   0x2800,
   0x3000,
   0x3400,
   0x3800,
   0x4000,
   0x5000,
   0x6000,
   0x6800,
   0x7000,
   0x8000,
   0xA000,
   0xC000,
   0xD000,
   0xE000,
   0x10000,
};
static const unsigned int validPitchCount = sizeof( validPitch ) / sizeof( validPitch[0] );

static unsigned int findValidPitch( unsigned int pitch )
{
   if (pitch <= validPitch[0])
      return validPitch[0];
   else
   {
      // dummy linear search
      for ( GLuint i = 0;i < validPitchCount - 1;++i )
         if (( pitch > validPitch[i] ) && ( pitch <= validPitch[i+1] ) )
            return validPitch[i+1];

      return validPitch[validPitchCount-1];
   }
}

static GLboolean rglDuringDestroyDevice = GL_FALSE;

// region update callback
//  This callback is passed to rglGcmAllocCreateRegion to notify when the
//  region is resized or deleted.
GLboolean rglGcmTryResizeTileRegion( GLuint address, GLuint size, void* data )
{
   rglTiledRegion* region = ( rglTiledRegion* )data;
   int32_t retVal = 0;

   // delete always succeeds
   if ( size == 0 )
   {
      region->offset = 0;
      region->size = 0;
      region->pitch = 0;

      if ( ! rglDuringDestroyDevice ) 
      {
         // must wait until RSX is completely idle before calling cellGcmUnbindTile 
         rglGcmUtilWaitForIdle(); 

         retVal = cellGcmUnbindTile( region->id );
         rglGcmFifoFinish( &rglGcmState_i.fifo );
      }
      return GL_TRUE;
   }
   region->offset = address;
   region->size = size;

   // must wait until RSX is completely idle before calling cellGcmSetTileInfo 
   rglGcmUtilWaitForIdle(); 

   retVal = cellGcmSetTileInfo(
         region->id,
         CELL_GCM_LOCATION_LOCAL,
         region->offset,
         region->size,
         region->pitch,
         CELL_GCM_COMPMODE_DISABLED, // if no tag bits, disable compression
         0,
         region->bank );

   retVal = cellGcmBindTile( region->id ); 

   rglGcmFifoFinish( &rglGcmState_i.fifo );
   return GL_TRUE;
}

void rglGcmGetTileRegionInfo( void* data, GLuint *address, GLuint *size )
{
   rglTiledRegion* region = ( rglTiledRegion* )data;

   *address = region->offset;
   *size = region->size;
}

#define RGLGCM_TILED_BUFFER_ALIGNMENT 0x10000 // 64KB
#define RGLGCM_TILED_BUFFER_HEIGHT_ALIGNMENT 64

GLuint rglGcmAllocCreateRegion(
      GLuint size,
      GLint tag,
      void* data )
{
   uint32_t id = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo, 1, size);

   if ( id != GMM_ERROR )
   {
      if ( rglGcmTryResizeTileRegion( (GLuint)gmmIdToOffset(id), gmmGetBlockSize(id), data ) )
         gmmSetTileAttrib( id, tag, data );
      else
      {
         gmmFree( id );
         id = GMM_ERROR;
      }
   }

   return id; 
}

/////////////////////////////////////////////////////////////////////////////
// tiled surface allocation

static void rglGcmAllocateTiledSurface(
      rglTiledMemoryManager* mm,
      GLuint width,
      GLuint height,
      GLuint bitsPerPixel,
      GLuint antiAliasing,
      GLuint* id,
      GLuint* pitchAllocated,
      GLuint* bytesAllocated )
{
   // determine pitch (in bytes)
   const unsigned int pitch = width * bitsPerPixel / 8;
   const unsigned int tiledPitch = findValidPitch( pitch );
   if ( tiledPitch < pitch )
      *pitchAllocated = rglPad( pitch, tiledPitch );
   else
      *pitchAllocated = tiledPitch;

   // fix alignment
   //  Render targets must be aligned to 8*pitch from the start of their
   //  region.  In addition, tiled regions must be aligned to 65536.  In
   //  order to keep both requirements satisfied as the region is extended
   //  or shrunken, the allocation size is padded to the smallest common
   //  multiple.
   //
   //  This can result in a fairly large percentage of wasted memory for
   //  certain dimension combinations, but this is simple and may conserve
   //  tiled region usage over some alternatives.
   GLuint padSize = RGLGCM_TILED_BUFFER_ALIGNMENT; // 64KB

   while (( padSize % ( tiledPitch*8 ) ) != 0 )
      padSize += RGLGCM_TILED_BUFFER_ALIGNMENT;

   // determine allocation size
   height = rglPad( height, RGLGCM_TILED_BUFFER_HEIGHT_ALIGNMENT );
   *bytesAllocated = rglPad(( *pitchAllocated ) * height, padSize );

   // attempt to extend an existing region
   //  The region tag is a hash of the pitch, compression, and isZBuffer.
   const GLuint tag = *pitchAllocated | 0;

   *id = gmmAllocExtendedTileBlock(*bytesAllocated, tag);

   if ( *id == GMM_ERROR )
   {
      // find an unused region
      for ( int i = 0; i < RGLGCM_MAX_TILED_REGIONS; ++i )
      {
         if ( mm->region[i].size == 0 )
         {
            // assign a region
            //  Address and size will be set in the callback.
            mm->region[i].id = i;
            mm->region[i].pitch = *pitchAllocated;
            mm->region[i].bank = 0x0; // XXX experiment

            // allocate space for our region
            *id = rglGcmAllocCreateRegion(
                  *bytesAllocated,
                  tag,
                  &mm->region[i] );

            break;
         }
      } // loop to find an unused region
   }

   // if we don't have a valid id, give up
   if ( *id == GMM_ERROR )
   {
      *bytesAllocated = 0;
      *pitchAllocated = 0;
   }
   else
   {
      //RGL_REPORT_EXTRA( RGL_REPORT_GPU_MEMORY_ALLOC, "Allocating GPU memory (tiled): %d bytes allocated at id 0x%08x", *bytesAllocated, *id );
   }
}

/////////////////////////////////////////////////////////////////////////////
// color surface allocation

GLboolean rglGcmAllocateColorSurface(
      GLuint width,
      GLuint height,
      GLuint bitsPerPixel,
      GLuint scanoutSupported,
      GLuint antiAliasing,
      GLuint *id,
      GLuint *pitchAllocated,
      GLuint *bytesAllocated )
{
   rglTiledMemoryManager* mm = &rglGcmTiledMemoryManager;

   rglGcmAllocateTiledSurface(
         mm,
         width, height, bitsPerPixel,
         antiAliasing,
         id,
         pitchAllocated,
         bytesAllocated );

   return *bytesAllocated > 0;
}

void rglGcmFreeTiledSurface( GLuint bufferId )
{
   gmmFree( bufferId );
}

/////////////////////////////////////////////////////////////////////////////
// video mode selection

typedef struct
{
   int width;
   int height;
   unsigned char hwMode;
}
VideoMode;

static const VideoMode sysutilModes[] =
{
   {720, 480, CELL_VIDEO_OUT_RESOLUTION_480},
   {720, 576, CELL_VIDEO_OUT_RESOLUTION_576},
   {1280, 720, CELL_VIDEO_OUT_RESOLUTION_720},
   {1920, 1080, CELL_VIDEO_OUT_RESOLUTION_1080},
#if (OS_VERSION_NUMERIC >= 0x150)
   {1600, 1080, CELL_VIDEO_OUT_RESOLUTION_1600x1080}, // hardware scales to 1920x1080 from 1600x1080 buffer
   {1440, 1080, CELL_VIDEO_OUT_RESOLUTION_1440x1080}, // hardware scales to 1920x1080 from 1440x1080 buffer
   {1280, 1080, CELL_VIDEO_OUT_RESOLUTION_1280x1080}, // hardware scales to 1920x1080 from 1280x1080 buffer
   {960, 1080, CELL_VIDEO_OUT_RESOLUTION_960x1080},   // hardware scales to 1920x1080 from 960x1080 buffer
#endif
};
static const int sysutilModeCount = sizeof( sysutilModes ) / sizeof( sysutilModes[0] );

static const VideoMode *findModeByResolutionInTable( int width, int height, const VideoMode *table, int modeCount )
{
   for ( int i = 0;i < modeCount;++i )
   {
      const VideoMode *vm = table + i;
      if (( vm->width == width ) && ( vm->height  == height ) ) return vm;
   }
   return NULL;
}

static inline const VideoMode *findModeByResolution( int width, int height )
{
   return findModeByResolutionInTable( width, height, sysutilModes, sysutilModeCount );
}

static const VideoMode *findModeByEnum( GLenum TVStandard )
{
   const VideoMode *vm = NULL;
   switch ( TVStandard )
   {
      case RGL_TV_STANDARD_NTSC_M:
      case RGL_TV_STANDARD_NTSC_J:
      case RGL_TV_STANDARD_HD480P:
      case RGL_TV_STANDARD_HD480I:
         vm = &(sysutilModes[0]);
         break;
      case RGL_TV_STANDARD_PAL_M:
      case RGL_TV_STANDARD_PAL_B:
      case RGL_TV_STANDARD_PAL_D:
      case RGL_TV_STANDARD_PAL_G:
      case RGL_TV_STANDARD_PAL_H:
      case RGL_TV_STANDARD_PAL_I:
      case RGL_TV_STANDARD_PAL_N:
      case RGL_TV_STANDARD_PAL_NC:
      case RGL_TV_STANDARD_HD576I:
      case RGL_TV_STANDARD_HD576P:
         vm = &(sysutilModes[1]);
         break;
      case RGL_TV_STANDARD_HD720P:
      case RGL_TV_STANDARD_1280x720_ON_VESA_1280x768:
      case RGL_TV_STANDARD_1280x720_ON_VESA_1280x1024:
         vm = &(sysutilModes[2]);
         break;
      case RGL_TV_STANDARD_HD1080I:
      case RGL_TV_STANDARD_HD1080P:
      case RGL_TV_STANDARD_1920x1080_ON_VESA_1920x1200:
         vm = &(sysutilModes[3]);
         break;
      default:
         vm = &(sysutilModes[2]);
         break; // do nothing
   }

   return vm;
}

// XXX ugly global to be returned by the function
static VideoMode _sysutilDetectedVideoMode;

const VideoMode *rglDetectVideoMode(void)
{
   CellVideoOutState videoState;
   int ret = cellVideoOutGetState( CELL_VIDEO_OUT_PRIMARY, 0, &videoState );
   if ( ret < 0 )
   {
      //RGL_REPORT_EXTRA( RGL_REPORT_ASSERT, "couldn't read the video configuration, using a default 720p resolution" );
      videoState.displayMode.scanMode = CELL_VIDEO_OUT_SCAN_MODE_PROGRESSIVE;
      videoState.displayMode.resolutionId = CELL_VIDEO_OUT_RESOLUTION_720;
   }
   CellVideoOutResolution resolution;
   cellVideoOutGetResolution( videoState.displayMode.resolutionId, &resolution );

   _sysutilDetectedVideoMode.width = resolution.width;
   _sysutilDetectedVideoMode.height = resolution.height;
   _sysutilDetectedVideoMode.hwMode = videoState.displayMode.resolutionId;
   return &_sysutilDetectedVideoMode;
}

static void rescInit( const RGLdeviceParameters* params, rglGcmDevice *gcmDevice )
{
   //RGL_REPORT_EXTRA(RGL_REPORT_RESC,"WARNING: RESC is enabled.");
   GLboolean result = 0;

   CellRescBufferMode dstBufferMode;
   if ( params->width == 720  && params->height == 480 )  dstBufferMode = CELL_RESC_720x480;
   else if ( params->width == 720  && params->height == 576 )  dstBufferMode = CELL_RESC_720x576;
   else if ( params->width == 1280 && params->height == 720 )  dstBufferMode = CELL_RESC_1280x720;
   else if ( params->width == 1920 && params->height == 1080 ) dstBufferMode = CELL_RESC_1920x1080;
   else
   {
      dstBufferMode = CELL_RESC_720x480;
      fprintf( stderr, "Invalid display resolution for resolution conversion: %ux%u. Defaulting to 720x480...\n", params->width, params->height );
   }

   CellRescInitConfig conf;
   memset( &conf, 0, sizeof( CellRescInitConfig ) );
   conf.size            = sizeof( CellRescInitConfig );
   conf.resourcePolicy  = CELL_RESC_MINIMUM_GPU_LOAD | CELL_RESC_CONSTANT_VRAM;
   conf.supportModes    = CELL_RESC_720x480 | CELL_RESC_720x576 | CELL_RESC_1280x720 | CELL_RESC_1920x1080;
   conf.ratioMode       = ( params->rescRatioMode == RESC_RATIO_MODE_FULLSCREEN ) ? CELL_RESC_FULLSCREEN :
      ( params->rescRatioMode == RESC_RATIO_MODE_PANSCAN ) ? CELL_RESC_PANSCAN :
      CELL_RESC_LETTERBOX;
   conf.palTemporalMode = ( params->rescPalTemporalMode == RESC_PAL_TEMPORAL_MODE_60_DROP ) ? CELL_RESC_PAL_60_DROP :
      ( params->rescPalTemporalMode == RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE ) ? CELL_RESC_PAL_60_INTERPOLATE :
      ( params->rescPalTemporalMode == RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE_30_DROP ) ? CELL_RESC_PAL_60_INTERPOLATE_30_DROP :
      ( params->rescPalTemporalMode == RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE_DROP_FLEXIBLE ) ? CELL_RESC_PAL_60_INTERPOLATE_DROP_FLEXIBLE :
      CELL_RESC_PAL_50;
   conf.interlaceMode   = ( params->rescInterlaceMode == RESC_INTERLACE_MODE_INTERLACE_FILTER ) ? CELL_RESC_INTERLACE_FILTER :
      CELL_RESC_NORMAL_BILINEAR;
   cellRescInit( &conf );

   // allocate all the destination scanout buffers using the RGL memory manager
   GLuint size;
   GLuint colorBuffersPitch;
   uint32_t numColorBuffers = cellRescGetNumColorBuffers( dstBufferMode, ( CellRescPalTemporalMode )conf.palTemporalMode, 0 );
   result = rglGcmAllocateColorSurface(params->width, params->height * numColorBuffers,
         4*8, RGLGCM_TRUE, 1, &(gcmDevice->RescColorBuffersId), &colorBuffersPitch, &size );

   // set the destination buffer format and pitch
   CellRescDsts dsts = { CELL_RESC_SURFACE_A8R8G8B8, colorBuffersPitch, 1 };
   cellRescSetDsts( dstBufferMode, &dsts );

   // set the resc output display mode (destination format)
   cellRescSetDisplayMode( dstBufferMode );

   // allocate space for vertex array and fragment shader for drawing the rescaling texture-mapped quad
   int32_t colorBuffersSize, vertexArraySize, fragmentShaderSize;
   cellRescGetBufferSize( &colorBuffersSize, &vertexArraySize, &fragmentShaderSize );
   gcmDevice->RescVertexArrayId    = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
         0, vertexArraySize);
   gcmDevice->RescFragmentShaderId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
         0, fragmentShaderSize);


   // tell resc how to access the destination (scanout) buffer
   cellRescSetBufferAddress( gmmIdToAddress(gcmDevice->RescColorBuffersId),
         gmmIdToAddress(gcmDevice->RescVertexArrayId),
         gmmIdToAddress(gcmDevice->RescFragmentShaderId) );

   // scale to adjust for overscan
   cellRescAdjustAspectRatio( params->horizontalScale, params->verticalScale );

   // allocate an interlace table if interlace filtering is used
   if ((params->enable & RGL_DEVICE_PARAMETERS_RESC_INTERLACE_MODE) &&
         (params->rescInterlaceMode == RESC_INTERLACE_MODE_INTERLACE_FILTER))
   {
      const unsigned int tableLength = 32; // this was based on the guidelines in the resc reference guide
      unsigned int tableSize = sizeof(uint16_t) * 4 * tableLength; // 2 bytes per FLOAT16 * 4 values per entry * length of table
      void *interlaceTable = gmmIdToAddress(gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
               0, tableSize));
      int32_t errorCode = cellRescCreateInterlaceTable(interlaceTable,params->renderHeight,CELL_RESC_ELEMENT_HALF,tableLength);
      (void)errorCode;
   }
}

// Semaphore for PPU wait
static sys_semaphore_t FlipSem;

// A flip callback function to release semaphore and write a label to lock the GPU
static void rglFlipCallbackFunction(const uint32_t head)
{
   //printf("Flip callback: label value: %d -> 0\n", *labelAddress);
   int res = sys_semaphore_post(FlipSem,1);
   (void)res;  // unused variable is ok
}

// A label for GPU to skip VSYNC
static volatile uint32_t *labelAddress = NULL;
static const uint32_t WaitLabelIndex = 111;

// VBlank callback function to write a label to release GPU
static void rglVblankCallbackFunction(const uint32_t head)
{   
   (void)head;
   int status = *labelAddress;
   switch(status){
      case 2:
         if (cellGcmGetFlipStatus()==0){
            cellGcmResetFlipStatus();
            *labelAddress=1;
         }
         break;
      case 1:
         *labelAddress = 0;
         break;
      default:
         break;
         // wait until rsx set the status to 2
   }
}

// Resc version of VBlank callback function to write a label to release GPU
static void rglRescVblankCallbackFunction(const uint32_t head)
{   
   (void)head;
   int status = *labelAddress;
   switch(status){
      case 2:
         if (cellRescGetFlipStatus()==0){
            cellRescResetFlipStatus();
            *labelAddress=1;
         }
         break;
      case 1:
         *labelAddress = 0;
         break;
      default:
         break;
         // wait until rsx set the status to 2
   }
}

/////////////////////////////////////////////////////////////////////////////
// create device

static void rglSetDisplayMode( const VideoMode *vm, GLushort bitsPerPixel, GLuint pitch )
{
   CellVideoOutConfiguration videocfg;
   memset( &videocfg, 0, sizeof( videocfg ) );
   videocfg.resolutionId = vm->hwMode;
   videocfg.format = ( bitsPerPixel == 32 ) ? CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8 : CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_R16G16B16X16_FLOAT;
   videocfg.pitch = pitch;
   videocfg.aspect = CELL_VIDEO_OUT_ASPECT_AUTO;
   cellVideoOutConfigure( CELL_VIDEO_OUT_PRIMARY, &videocfg, NULL, 0 );
}

int rglPlatformCreateDevice (void *data)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   RGLdevice *device = (RGLdevice*)data;
   rglGcmDevice *gcmDevice = ( rglGcmDevice * )device->platformDevice;
   RGLdeviceParameters* params = &device->deviceParameters;
   rglDuringDestroyDevice = GL_FALSE;
   GLboolean result = 0;

   // Tile memory manager init
   rglGcmTiledMemoryInit();

   const VideoMode *vm = NULL;
   if ( params->enable & RGL_DEVICE_PARAMETERS_TV_STANDARD )
   {
      vm = findModeByEnum( params->TVStandard );
      if ( !vm ) return -1;
      params->width = vm->width;
      params->height = vm->height;
   }
   else if ( params->enable & RGL_DEVICE_PARAMETERS_WIDTH_HEIGHT )
   {
      vm = findModeByResolution( params->width, params->height );
      if ( !vm ) return -1;
   }
   else
   {
      vm = rglDetectVideoMode();
      if ( !vm ) return -1;
      params->width = vm->width;
      params->height = vm->height;
   }

   // set render width and height to match the display width and height, unless resolution conversion is specified
   if ( !(params->enable & RGL_DEVICE_PARAMETERS_RESC_RENDER_WIDTH_HEIGHT) )
   {
      params->renderWidth = params->width;
      params->renderHeight = params->height;
   }

   if (rescIsEnabled(params))
      rescInit( params, gcmDevice );

   gcmDevice->deviceType = 0;
   gcmDevice->TVStandard = params->TVStandard;

   // if resc enabled, vsync is always enabled
   gcmDevice->vsync = rescIsEnabled( params ) ? GL_TRUE : GL_FALSE;

   gcmDevice->skipFirstVsync = GL_FALSE;

   gcmDevice->ms = NULL;

   const GLuint width = params->renderWidth;
   const GLuint height = params->renderHeight;

   GLboolean fpColor = GL_FALSE;
   switch ( params->colorFormat )
   {
      case GL_RGBA16F_ARB:
         fpColor = GL_TRUE;
         break;
      case GL_ARGB_SCE:
         break;
         // GL_RGBA ?
      default:
         return -1;
   }

   GLuint antiAliasingMode=1;

   // create color buffers
   //  The default color buffer format is currently always ARGB8.
   // single, double, or triple buffering
   for ( int i = 0; i < params->bufferingMode; ++i )
   {
      gcmDevice->color[i].source = RGLGCM_SURFACE_SOURCE_DEVICE;
      gcmDevice->color[i].width = width;
      gcmDevice->color[i].height = height;
      gcmDevice->color[i].bpp = fpColor ? 8 : 4;
      gcmDevice->color[i].format = RGLGCM_ARGB8;
      gcmDevice->color[i].pool = RGLGCM_SURFACE_POOL_LINEAR;

      // allocate tiled memory
      GLuint size;
      result = rglGcmAllocateColorSurface(
            width, height,           // dimensions
            gcmDevice->color[i].bpp*8,  // bits per sample
            RGLGCM_TRUE,               // scan out enable
            antiAliasingMode,                // antiAliasing
            &gcmDevice->color[i].dataId,    // returned buffer
            &gcmDevice->color[i].pitch,
            &size );
   }

   memset( &gcmDevice->rt, 0, sizeof( rglGcmRenderTargetEx ) );
   gcmDevice->rt.colorBufferCount = 1;
   gcmDevice->rt.yInverted = GL_TRUE;
   gcmDevice->rt.width = width;
   gcmDevice->rt.height = height;

   rglGcmViewportState *v = &rglGcmState_i.state.viewport;
   v->x = 0;
   v->y = 0;
   v->w = width;
   v->h = height;
   rglGcmFifoGlViewport(v, 0.0f, 1.0f);
   rglGcmSetClearColor(thisContext, 0 );

   if ( fpColor )
   {
      // we don't yet have a fragment program to clear the buffer with a quad.
      // so we'll cheat pretending we have a RGBA buffer with twice the width.
      // Since we clear with 0 (which is the same in fp16), it works.

      gcmDevice->rt.width = 2 * width;
      gcmDevice->rt.colorFormat = RGLGCM_ARGB8;
      for ( int i = 0; i < params->bufferingMode; ++i )
      {
         gcmDevice->rt.colorId[0] = gcmDevice->color[i].dataId;
         gcmDevice->rt.colorPitch[0] = gcmDevice->color[i].pitch;
         rglGcmFifoGlSetRenderTarget( &gcmDevice->rt );

         if (rglGcmState_i.renderTarget.colorFormat)
            rglGcmSetClearSurface(thisContext, CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | 
                  CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A );
      }
      // restore parameters
      gcmDevice->rt.width = width;
      gcmDevice->rt.colorFormat = gcmDevice->color[0].format;
   }
   else
   {
      // clear the buffers for compression to work best
      gcmDevice->rt.colorFormat = RGLGCM_ARGB8;
      for ( int i = 0; i < params->bufferingMode; ++i )
      {
         gcmDevice->rt.colorId[0] = gcmDevice->color[i].dataId;
         gcmDevice->rt.colorPitch[0] = gcmDevice->color[i].pitch;
         rglGcmFifoGlSetRenderTarget( &gcmDevice->rt );
         
         if (rglGcmState_i.renderTarget.colorFormat)
            rglGcmSetClearSurface(thisContext, CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | 
                  CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A );
      }
   }

   gcmDevice->scanBuffer = 0;
   if ( params->bufferingMode == RGL_BUFFERING_MODE_SINGLE )
      gcmDevice->drawBuffer = 0;
   else if ( params->bufferingMode == RGL_BUFFERING_MODE_DOUBLE )
      gcmDevice->drawBuffer = 1;
   else if ( params->bufferingMode == RGL_BUFFERING_MODE_TRIPLE )
      gcmDevice->drawBuffer = 2;


   // Create semaphore
   sys_semaphore_attribute_t attr;//todo: configure
   sys_semaphore_attribute_initialize(attr);

   // Set initial and max count of semaphore based on buffering mode
   sys_semaphore_value_t initial_val = 0;
   sys_semaphore_value_t max_val = 1;
   switch (device->deviceParameters.bufferingMode)
   {
      case RGL_BUFFERING_MODE_SINGLE:
         initial_val = 0;
         max_val = 1;
         break;
      case RGL_BUFFERING_MODE_DOUBLE:
         initial_val = 1;
         max_val = 2;
         break;
      case RGL_BUFFERING_MODE_TRIPLE:
         initial_val = 2;
         max_val = 3;
         break;
      default:
         break;
   }

   int res = sys_semaphore_create(&FlipSem, &attr, initial_val, max_val);
   (void)res;  // unused variable is ok

   // Register flip callback
   if ( rescIsEnabled( params ) )
      cellRescSetFlipHandler(rglFlipCallbackFunction);
   else
      cellGcmSetFlipHandler(rglFlipCallbackFunction);

   // Initialize label by getting address
   labelAddress = (volatile uint32_t *)cellGcmGetLabelAddress(WaitLabelIndex);	
   *labelAddress = 0;

   // Regiter VBlank callback
   if ( rescIsEnabled( params ) )
      cellRescSetVBlankHandler(rglRescVblankCallbackFunction);
   else
      cellGcmSetVBlankHandler(rglVblankCallbackFunction);

   if ( rescIsEnabled( params ) )
   {
      for ( int i = 0; i < params->bufferingMode; ++i ) // bufferingMode should always be single, but for now....
      {
         // Set the RESC CellRescSrc buffer (source buffer to scale - psgl's render target):
         // **NOT SURE WHAT TO DO ABOUT FP BUFFERS YET** case CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT for fp16

         CellRescSrc rescSrc = { CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN | CELL_GCM_TEXTURE_NR, // uint8_t format
            gcmDevice->color[i].pitch,                           // uint32_t pitch
            width,                                                     // uint16_t width
            height,                                                    // uint16_t height
            gmmIdToOffset( gcmDevice->color[i].dataId ) }; // uint32_t offset

         if ( cellRescSetSrc( i, &rescSrc ) != CELL_OK )
         {
            fprintf( stderr, "Registering display buffer %d failed\n", i );
            return -1;
         }
      }
   }
   else
   {
      rglSetDisplayMode( vm, gcmDevice->color[0].bpp*8, gcmDevice->color[0].pitch );

      cellGcmSetFlipMode( gcmDevice->vsync ? CELL_GCM_DISPLAY_VSYNC : CELL_GCM_DISPLAY_HSYNC );
      rglGcmSetInvalidateVertexCache(thisContext);
      rglGcmFifoFinish( &rglGcmState_i.fifo );

      for ( int i = 0; i < params->bufferingMode; ++i )
      {
         if ( cellGcmSetDisplayBuffer( i, gmmIdToOffset( gcmDevice->color[i].dataId ), gcmDevice->color[i].pitch , width, height ) != CELL_OK )
         {
            fprintf( stderr, "Registering display buffer %d failed\n", i );
            return -1;
         }
      }
   }

   rglGcmFifoGlIncFenceRef( &gcmDevice->swapFifoRef );

   //swapFifoRef2 used for triple buffering
   gcmDevice->swapFifoRef2 = gcmDevice->swapFifoRef;

   return 0;
}

void rglPlatformDestroyDevice (void *data)
{
   RGLdevice *device = (RGLdevice*)data;
   rglGcmDevice *gcmDevice = ( rglGcmDevice * )device->platformDevice;
   RGLdeviceParameters *params = &device->deviceParameters;
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;

   rglGcmSetInvalidateVertexCache(thisContext);
   rglGcmFifoFinish( &rglGcmState_i.fifo );

   // Stop flip callback
   if ( rescIsEnabled( params ) )
      cellRescSetFlipHandler(NULL);
   else
      cellGcmSetFlipHandler(NULL);

   // Stop VBlank callback
   if ( rescIsEnabled( &device->deviceParameters ) )
      cellRescSetVBlankHandler(NULL);
   else
      cellGcmSetVBlankHandler(NULL);

   // Destroy semaphore
   int res = sys_semaphore_destroy(FlipSem);
   (void)res;  // prevent unused variable warning in opt build

   if ( rescIsEnabled( params ) )
   {
      cellRescExit();
      rglGcmFreeTiledSurface(gcmDevice->RescColorBuffersId);
      gmmFree(gcmDevice->RescVertexArrayId);
      gmmFree(gcmDevice->RescFragmentShaderId);
   }

   rglDuringDestroyDevice = GL_TRUE;
   for ( int i = 0; i < params->bufferingMode; ++i )
   {
      if ( gcmDevice->color[i].pool != RGLGCM_SURFACE_POOL_NONE )
         rglGcmFreeTiledSurface( gcmDevice->color[i].dataId );
   }
   rglDuringDestroyDevice = GL_FALSE;
}

GLAPI void RGL_EXPORT psglSwap (void)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   const uint32_t  fence = rglGcmState_i.semaphores->userSemaphores[RGLGCM_SEMA_FENCE].val;
   GmmBlock        *pBlock = NULL;
   GmmBlock        *pTemp = NULL;
   GmmAllocator *pAllocator =  pGmmLocalAllocator;

   pBlock = pAllocator->pPendingFreeHead;

   while (pBlock)
   {
      pTemp = pBlock->pNextFree;

      if ( !(( fence - pBlock->fence ) & 0x80000000 ) )
      {
         gmmRemovePendingFree(pAllocator, pBlock);
         gmmAddFree(pAllocator, pBlock);
      }

      pBlock = pTemp;
   }

   RGLdevice *device = (RGLdevice*)_CurrentDevice;
   rglGcmDevice *gcmDevice = (rglGcmDevice *)device->platformDevice;

   const GLuint drawBuffer = gcmDevice->drawBuffer;

   GLboolean vsync = _CurrentContext->VSync;
   if ( vsync != gcmDevice->vsync )
   {
      if ( ! rescIsEnabled( &device->deviceParameters ) )
      {
         cellGcmSetFlipMode( vsync ? CELL_GCM_DISPLAY_VSYNC : CELL_GCM_DISPLAY_HSYNC );
         gcmDevice->vsync = vsync;
      }
   }

   if ( device->deviceParameters.bufferingMode == RGL_BUFFERING_MODE_TRIPLE )
   {
      if ( rescIsEnabled( &device->deviceParameters ) )
         cellRescSetWaitFlip(); // GPU will wait until flip actually occurs
      else
      {
         // GPU will wait until flip actually occurs
         rglGcmSetWaitLabel(gCellGcmCurrentContext, 1, 0);
      }
   }

   if ( rescIsEnabled( &device->deviceParameters ) )
   {
      int32_t res = cellRescSetConvertAndFlip(( uint8_t ) drawBuffer ); 
      if ( res != CELL_OK )
      {
         //RGL_REPORT_EXTRA(RGL_REPORT_RESC_FLIP_ERROR, "WARNING: RESC cellRescSetConvertAndFlip returned error code %d.\n", res);
         if ( _CurrentContext )
            _CurrentContext->needValidate |= RGL_VALIDATE_FRAMEBUFFER;
         return;
      }
   }
   else
      cellGcmSetFlipUnsafe(gCellGcmCurrentContext, (uint8_t)drawBuffer);

   if ( device->deviceParameters.bufferingMode != RGL_BUFFERING_MODE_TRIPLE )
   {
      if ( rescIsEnabled( &device->deviceParameters ) )
         cellRescSetWaitFlip(); // GPU will wait until flip actually occurs
      else
      {
         // GPU will wait until flip actually occurs
         rglGcmSetWaitLabel(gCellGcmCurrentContext, 1, 0);
      }
   }

   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
   const char * __restrict v = driver->sharedVPConstants;
   GCM_FUNC( cellGcmSetVertexProgramParameterBlock, 0, 8, ( float* )v ); // GCM_PORT_UNTESTED [KHOFF]

   rglGcmSetDitherEnable(thisContext, RGLGCM_TRUE );

   RGLcontext *context = (RGLcontext*)_CurrentContext;
   context->needValidate = RGL_VALIDATE_ALL;
   context->attribs->DirtyMask = ( 1 << RGL_MAX_VERTEX_ATTRIBS ) - 1;

   rglGcmSetInvalidateVertexCache(thisContext);
   rglGcmFifoFlush( &rglGcmState_i.fifo );

   while (sys_semaphore_wait(FlipSem,1000) != CELL_OK);

   rglGcmSetInvalidateVertexCache(thisContext);
   rglGcmFifoFlush( &rglGcmState_i.fifo );

   if ( device->deviceParameters.bufferingMode == RGL_BUFFERING_MODE_DOUBLE )
   {
      gcmDevice->drawBuffer = gcmDevice->scanBuffer;
      gcmDevice->scanBuffer = drawBuffer;

      gcmDevice->rt.colorId[0] = gcmDevice->color[gcmDevice->drawBuffer].dataId;
      gcmDevice->rt.colorPitch[0] = gcmDevice->color[gcmDevice->drawBuffer].pitch;
   }
   else if ( device->deviceParameters.bufferingMode == RGL_BUFFERING_MODE_TRIPLE )
   {
      gcmDevice->drawBuffer = gcmDevice->scanBuffer;

      if ( gcmDevice->scanBuffer == 2 )
         gcmDevice->scanBuffer = 0;
      else
         gcmDevice->scanBuffer++;

      gcmDevice->rt.colorId[0] = gcmDevice->color[gcmDevice->drawBuffer].dataId;
      gcmDevice->rt.colorPitch[0] = gcmDevice->color[gcmDevice->drawBuffer].pitch;
   }
}

/*============================================================
  BUFFERS
  ============================================================ */

static rglBufferObject *rglCreateBufferObject (void)
{
   GLuint size = sizeof( rglBufferObject ) + sizeof(rglGcmBufferObject);
   rglBufferObject *buffer = (rglBufferObject*)malloc(size);

   if(!buffer )
      return NULL;

   memset(buffer, 0, size); 
   buffer->refCount = 1;
   new( &buffer->textureReferences ) RGL::Vector<rglTexture *>();

   return buffer;
}

static void rglFreeBufferObject (void *data)
{
   rglBufferObject *buffer = (rglBufferObject*)data;

   if ( --buffer->refCount == 0 )
   {
      rglPlatformDestroyBufferObject( buffer );
      buffer->textureReferences.~Vector<rglTexture *>();
      free( buffer );
   }
}

GLAPI void APIENTRY glBindBuffer( GLenum target, GLuint name )
{
   RGLcontext *LContext = _CurrentContext;

   if (name)
      rglTexNameSpaceCreateNameLazy( &LContext->bufferObjectNameSpace, name );

   switch ( target )
   {
      case GL_ARRAY_BUFFER:
         LContext->ArrayBuffer = name;
         break;
      case GL_PIXEL_UNPACK_BUFFER_ARB:
         LContext->PixelUnpackBuffer = name;
         break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
         LContext->TextureBuffer = name;
         break;
      default:
         break;
   }
}

GLAPI GLvoid* APIENTRY glMapBuffer( GLenum target, GLenum access )
{
   RGLcontext *LContext = _CurrentContext;
   GLuint name = 0;

   switch ( target )
   {
      case GL_ARRAY_BUFFER:
         name = LContext->ArrayBuffer;
         break;
      case GL_PIXEL_UNPACK_BUFFER_ARB:
         name = LContext->PixelUnpackBuffer;
         break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
         name = LContext->TextureBuffer;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return NULL;
   }

   rglBufferObject* bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];
   bufferObject->mapped = GL_TRUE;

   return rglPlatformBufferObjectMap( bufferObject, access );
}

GLAPI GLboolean APIENTRY glUnmapBuffer( GLenum target )
{
   RGLcontext *LContext = _CurrentContext;
   GLuint name = 0;

   switch ( target )
   {
      case GL_ARRAY_BUFFER:
         name = LContext->ArrayBuffer;
         break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
         name = LContext->TextureBuffer;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return GL_FALSE;
   }
   rglBufferObject* bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];

   bufferObject->mapped = GL_FALSE;

   GLboolean result = rglPlatformBufferObjectUnmap( bufferObject );

   return result;
}

GLAPI void APIENTRY glDeleteBuffers( GLsizei n, const GLuint *buffers )
{
   RGLcontext *LContext = _CurrentContext;
   for (int i = 0; i < n; ++i)
   {
      if(!rglTexNameSpaceIsName(&LContext->bufferObjectNameSpace, buffers[i]))
         continue;
      if (buffers[i])
      {
         GLuint name = buffers[i];

         if (LContext->ArrayBuffer == name)
            LContext->ArrayBuffer = 0;
         if (LContext->PixelUnpackBuffer == name)
            LContext->PixelUnpackBuffer = 0;

         for ( int i = 0;i < RGL_MAX_VERTEX_ATTRIBS;++i )
         {
            if ( LContext->attribs->attrib[i].arrayBuffer == name )
            {
               LContext->attribs->attrib[i].arrayBuffer = 0;
               LContext->attribs->HasVBOMask &= ~( 1 << i );
            }
         }
      }
   }
   rglTexNameSpaceDeleteNames( &LContext->bufferObjectNameSpace, n, buffers );
}

GLAPI void APIENTRY glGenBuffers( GLsizei n, GLuint *buffers )
{
   RGLcontext *LContext = _CurrentContext;
   rglTexNameSpaceGenNames( &LContext->bufferObjectNameSpace, n, buffers );
}

GLAPI void APIENTRY glBufferData( GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage )
{
   RGLcontext *LContext = _CurrentContext;

   GLuint name = 0;

   switch ( target )
   {
      case GL_ARRAY_BUFFER:
         name = LContext->ArrayBuffer;
         break;
      case GL_PIXEL_UNPACK_BUFFER_ARB:
         name = LContext->PixelUnpackBuffer;
         break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
         name = LContext->TextureBuffer;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
   rglBufferObject* bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];

   if ( bufferObject->refCount > 1 )
   {
      rglTexNameSpaceDeleteNames( &LContext->bufferObjectNameSpace, 1, &name );
      rglTexNameSpaceCreateNameLazy( &LContext->bufferObjectNameSpace, name );

      bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];
   }

   if (bufferObject->size > 0)
      rglPlatformDestroyBufferObject( bufferObject );

   bufferObject->size = size;
   bufferObject->width = 0;
   bufferObject->height = 0;
   bufferObject->internalFormat = GL_NONE;

   if ( size > 0 )
   {
      GLboolean created = rglpCreateBufferObject(bufferObject);
      if ( !created )
      {
         rglSetError( GL_OUT_OF_MEMORY );
         return;
      }
      if (data)
         rglPlatformBufferObjectSetData( bufferObject, 0, size, data, GL_TRUE );
   }
}

/*============================================================
  FRAMEBUFFER
  ============================================================ */

GLAPI void APIENTRY glClearColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
}

GLAPI void APIENTRY glBlendColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
   RGLcontext*	LContext = _CurrentContext;
   LContext->BlendColor.R = rglClampf( red );
   LContext->BlendColor.G = rglClampf( green );
   LContext->BlendColor.B = rglClampf( blue );
   LContext->BlendColor.A = rglClampf( alpha );

   LContext->needValidate |= RGL_VALIDATE_BLENDING;
}

GLAPI void APIENTRY glBlendFunc( GLenum sfactor, GLenum dfactor )
{
   RGLcontext*	LContext = _CurrentContext;

   LContext->BlendFactorSrcRGB = sfactor;
   LContext->BlendFactorSrcAlpha = sfactor;
   LContext->BlendFactorDestRGB = dfactor;
   LContext->BlendFactorDestAlpha = dfactor;
   LContext->needValidate |= RGL_VALIDATE_BLENDING;
}

/*============================================================
  FRAMEBUFFER OBJECTS
  ============================================================ */

void rglFramebufferGetAttachmentTexture(
      RGLcontext* LContext,
      const rglFramebufferAttachment* attachment,
      rglTexture** texture,
      GLuint* face )
{
   switch ( attachment->type )
   {
      case RGL_FRAMEBUFFER_ATTACHMENT_NONE:
         *texture = NULL;
         *face = 0;
         break;
      case RGL_FRAMEBUFFER_ATTACHMENT_RENDERBUFFER:
         break;
      case RGL_FRAMEBUFFER_ATTACHMENT_TEXTURE:
         *texture = rglGetTextureSafe( LContext, attachment->name );
         *face = 0;
         if ( *texture )
         {
            switch ( attachment->textureTarget )
            {
               case GL_TEXTURE_2D:
                  break;
               default:
                  *texture = NULL;
            }
         }
         break;
      default:
         *face = 0;
         *texture = NULL;
   }
}

rglFramebufferAttachment* rglFramebufferGetAttachment(void *data, GLenum attachment)
{
   rglFramebuffer *framebuffer = (rglFramebuffer*)data;

   switch ( attachment )
   {
      case GL_COLOR_ATTACHMENT0_EXT:
      case GL_COLOR_ATTACHMENT1_EXT:
      case GL_COLOR_ATTACHMENT2_EXT:
      case GL_COLOR_ATTACHMENT3_EXT:
         return &framebuffer->color[attachment - GL_COLOR_ATTACHMENT0_EXT];
      default:
         rglSetError( GL_INVALID_ENUM );
         return NULL;
   }
}

GLAPI GLboolean APIENTRY glIsFramebufferOES( GLuint framebuffer )
{
   RGLcontext* LContext = _CurrentContext;

   if ( !rglTexNameSpaceIsName( &LContext->framebufferNameSpace, framebuffer ) )
      return GL_FALSE;

   return GL_TRUE;
}

GLAPI void APIENTRY glBindFramebufferOES( GLenum target, GLuint framebuffer )
{
   RGLcontext* LContext = _CurrentContext;

   if ( framebuffer )
      rglTexNameSpaceCreateNameLazy( &LContext->framebufferNameSpace, framebuffer );

   LContext->framebuffer = framebuffer;
   LContext->needValidate |= RGL_VALIDATE_FRAMEBUFFER;
}

GLAPI void APIENTRY glDeleteFramebuffersOES( GLsizei n, const GLuint *framebuffers )
{
   RGLcontext *LContext = _CurrentContext;

   for ( int i = 0; i < n; ++i )
   {
      if ( framebuffers[i] && framebuffers[i] == LContext->framebuffer )
         glBindFramebufferOES( GL_FRAMEBUFFER_OES, 0 );
   }

   rglTexNameSpaceDeleteNames( &LContext->framebufferNameSpace, n, framebuffers );
}

GLAPI void APIENTRY glGenFramebuffersOES( GLsizei n, GLuint *framebuffers )
{
   RGLcontext *LContext = _CurrentContext;
   rglTexNameSpaceGenNames( &LContext->framebufferNameSpace, n, framebuffers );
}

GLAPI GLenum APIENTRY glCheckFramebufferStatusOES( GLenum target )
{
   RGLcontext* LContext = _CurrentContext;

   if (LContext->framebuffer)
   {
      rglFramebuffer* framebuffer = rglGetFramebuffer( LContext, LContext->framebuffer );

      return rglPlatformFramebufferCheckStatus( framebuffer );
   }

   return GL_FRAMEBUFFER_COMPLETE_OES;
}

GLAPI void APIENTRY glFramebufferTexture2DOES( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level )
{
   RGLcontext* LContext = _CurrentContext;

   rglFramebuffer* framebuffer = rglGetFramebuffer( LContext, LContext->framebuffer );
   rglFramebufferAttachment* attach = rglFramebufferGetAttachment( framebuffer, attachment );

   if (!attach)
      return;

   rglTexture *textureObject = NULL;
   GLuint face;
   rglFramebufferGetAttachmentTexture( LContext, attach, &textureObject, &face );

   if (textureObject)
      textureObject->framebuffers.removeElement( framebuffer );

   if (texture)
   {
      attach->type = RGL_FRAMEBUFFER_ATTACHMENT_TEXTURE;
      textureObject = (rglTexture*)LContext->textureNameSpace.data[texture];
      textureObject->framebuffers.pushBack( framebuffer );
   }
   else
      attach->type = RGL_FRAMEBUFFER_ATTACHMENT_NONE;
   attach->name = texture;
   attach->textureTarget = textarget;

   framebuffer->needValidate = GL_TRUE;
   LContext->needValidate |= RGL_VALIDATE_FRAMEBUFFER;
}


/*============================================================
  IMAGE CONVERSION
  ============================================================ */

#define GL_UNSIGNED_INT_24_8      GL_UNSIGNED_INT_24_8_SCE
#define GL_UNSIGNED_INT_8_24_REV  GL_UNSIGNED_INT_8_24_REV_SCE

#define GL_UNSIGNED_SHORT_8_8		GL_UNSIGNED_SHORT_8_8_SCE
#define GL_UNSIGNED_SHORT_8_8_REV	GL_UNSIGNED_SHORT_8_8_REV_SCE
#define GL_UNSIGNED_INT_16_16		GL_UNSIGNED_INT_16_16_SCE
#define GL_UNSIGNED_INT_16_16_REV	GL_UNSIGNED_INT_16_16_REV_SCE

#define DECLARE_C_TYPES \
   DECLARE_TYPE(GL_BYTE,GLbyte,127.f) \
DECLARE_TYPE(GL_UNSIGNED_BYTE,GLubyte,255.f) \
DECLARE_TYPE(GL_SHORT,GLshort,32767.f) \
DECLARE_TYPE(GL_UNSIGNED_SHORT,GLushort,65535.f) \
DECLARE_TYPE(GL_INT,GLint,2147483647.f) \
DECLARE_TYPE(GL_UNSIGNED_INT,GLuint,4294967295.0) \
DECLARE_TYPE(GL_FIXED,GLfixed,65535.f)

#define DECLARE_UNPACKED_TYPES \
   DECLARE_UNPACKED_TYPE(GL_BYTE) \
DECLARE_UNPACKED_TYPE(GL_UNSIGNED_BYTE) \
DECLARE_UNPACKED_TYPE(GL_SHORT) \
DECLARE_UNPACKED_TYPE(GL_UNSIGNED_SHORT) \
DECLARE_UNPACKED_TYPE(GL_INT) \
DECLARE_UNPACKED_TYPE(GL_UNSIGNED_INT) \
DECLARE_UNPACKED_TYPE(GL_HALF_FLOAT_ARB) \
DECLARE_UNPACKED_TYPE(GL_FLOAT) \
DECLARE_UNPACKED_TYPE(GL_FIXED)


#define DECLARE_PACKED_TYPES \
   DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_BYTE,4,4) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_BYTE,6,2) \
DECLARE_PACKED_TYPE_AND_REV_3(UNSIGNED_BYTE,3,3,2) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_BYTE,2,2,2,2) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_SHORT,12,4) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_SHORT,8,8) \
DECLARE_PACKED_TYPE_AND_REV_3(UNSIGNED_SHORT,5,6,5) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_SHORT,4,4,4,4) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_SHORT,5,5,5,1) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_INT,16,16) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_INT,24,8) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_INT,8,8,8,8) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_INT,10,10,10,2)

#define DECLARE_TYPE(TYPE,CTYPE,MAXVAL) \
   typedef CTYPE type_##TYPE; \
static inline type_##TYPE rglFloatTo_##TYPE(float v) { return (type_##TYPE)(rglClampf(v)*MAXVAL); } \
static inline float rglFloatFrom_##TYPE(type_##TYPE v) { return ((float)v)/MAXVAL; }
DECLARE_C_TYPES
#undef DECLARE_TYPE

typedef GLfloat type_GL_FLOAT;
typedef GLhalfARB type_GL_HALF_FLOAT_ARB;

#define rglFloatTo_GL_FLOAT(v) (v)
#define rglFloatFrom_GL_FLOAT(v) (v)
#define rglFloatTo_GL_HALF_FLOAT_ARB(x) (rglFloatToHalf(x))
#define rglFloatFrom_GL_HALF_FLOAT_ARB(x) (rglHalfToFloat(x))

#define DECLARE_PACKED_TYPE_AND_REV_2(REALTYPE,S1,S2) \
   DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S1##_##S2,2,S1,S2,0,0,) \
DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S2##_##S1##_REV,2,S2,S1,0,0,_REV)

#define DECLARE_PACKED_TYPE_AND_REV_3(REALTYPE,S1,S2,S3) \
   DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S1##_##S2##_##S3,3,S1,S2,S3,0,) \
DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S3##_##S2##_##S1##_REV,3,S3,S2,S1,0,_REV)

#define DECLARE_PACKED_TYPE_AND_REV_4(REALTYPE,S1,S2,S3,S4) \
   DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S1##_##S2##_##S3##_##S4,4,S1,S2,S3,S4,) \
DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S4##_##S3##_##S2##_##S1##_REV,4,S4,S3,S2,S1,_REV)

#define DECLARE_PACKED_TYPE_AND_REALTYPE(REALTYPE,N,S1,S2,S3,S4,REV) \
   DECLARE_PACKED_TYPE(GL_##REALTYPE,PACKED_TYPE(REALTYPE,N,S1,S2,S3,S4,REV),N,S1,S2,S3,S4,REV)

#define INDEX(N,X) (X)
#define INDEX_REV(N,X) (N-1-X)

#define GET_BITS(to,from,first,count) if ((count)>0) to=((GLfloat)(((from)>>(first))&((1<<(count))-1)))/(GLfloat)((1<<((count==0)?1:count))-1)
#define PUT_BITS(from,to,first,count) if ((count)>0) to|=((unsigned int)((from)*((GLfloat)((1<<((count==0)?1:count))-1))))<<(first);

static inline int rglGetComponentCount (GLenum format)
{
   switch ( format )
   {
      case GL_RGB:
      case GL_BGR:
         return 3;
      case GL_RGBA:
      case GL_BGRA:
      case GL_ABGR:
      case GL_ARGB_SCE:
         return 4;
      case GL_RED:
      case GL_GREEN:
      case GL_BLUE:
      case GL_ALPHA:
         return 1;
      default:
         return 0;
   }
}

static inline int rglGetTypeSize( GLenum type )
{
   switch ( type )
   {

#define DECLARE_PACKED_TYPE(REALTYPE,TYPE,N,S1,S2,S3,S4,REV) \
      case TYPE: \
                 return sizeof(type_##REALTYPE);
      DECLARE_PACKED_TYPES
#undef DECLARE_PACKED_TYPE

#define DECLARE_UNPACKED_TYPE(TYPE) \
      case TYPE: \
                 return sizeof(type_##TYPE);
         DECLARE_UNPACKED_TYPES
#undef DECLARE_UNPACKED_TYPE

      default:
         return 0;
   }
}

int rglGetPixelSize( GLenum format, GLenum type )
{
   int componentSize;
   switch ( type )
   {

#define DECLARE_PACKED_TYPE(REALTYPE,TYPE,N,S1,S2,S3,S4,REV) \
      case TYPE: \
                 return sizeof(type_##REALTYPE);
      DECLARE_PACKED_TYPES
#undef DECLARE_PACKED_TYPE

#define DECLARE_UNPACKED_TYPE(TYPE) \
      case TYPE: \
                 componentSize=sizeof(type_##TYPE); \
         break;
         DECLARE_UNPACKED_TYPES
#undef DECLARE_UNPACKED_TYPE

      default:
         return 0;
   }
   return rglGetComponentCount(format) * componentSize;
}

static void rglRawRasterToImage(const void *in_data,
      void *out_data, GLuint x, GLuint y, GLuint z )
{
   const rglRaster* raster = (const rglRaster*)in_data;
   rglImage *image = (rglImage*)out_data;
   const int pixelBits = rglGetPixelSize( image->format, image->type ) * 8;

   const GLuint size = pixelBits / 8;

   if ( raster->xstride == image->xstride &&
         raster->ystride == image->ystride &&
         raster->zstride == image->zstride )
   {
      memcpy((char*)image->data +
            x*image->xstride + y*image->ystride + z*image->zstride,
            raster->data, raster->depth*raster->zstride );

      return;
   }
   else if ( raster->xstride == image->xstride )
   {
      const GLuint lineBytes = raster->width * raster->xstride;
      for ( int i = 0; i < raster->depth; ++i )
      {
         for ( int j = 0; j < raster->height; ++j )
         {
            const char *src = ( const char * )raster->data +
               i * raster->zstride + j * raster->ystride;
            char *dst = ( char * )image->data +
               ( i + z ) * image->zstride +
               ( j + y ) * image->ystride +
               x * image->xstride;
            memcpy( dst, src, lineBytes );
         }
      }

      return;
   }

   for ( int i = 0; i < raster->depth; ++i )
   {
      for ( int j = 0; j < raster->height; ++j )
      {
         const char *src = ( const char * )raster->data +
            i * raster->zstride + j * raster->ystride;
         char *dst = ( char * )image->data +
            ( i + z ) * image->zstride +
            ( j + y ) * image->ystride +
            x * image->xstride;

         for ( int k = 0; k < raster->width; ++k )
         {
            memcpy( dst, src, size );

            src += raster->xstride;
            dst += image->xstride;
         }
      }
   }
}

void rglImageAllocCPUStorage (void *data)
{
   rglImage *image = (rglImage*)data;

   if (( image->storageSize > image->mallocStorageSize ) || ( !image->mallocData ) )
   {
      if (image->mallocData)
         free( image->mallocData );

      image->mallocData = (char *)malloc( image->storageSize + 128 );
      image->mallocStorageSize = image->storageSize;
   }
   image->data = rglPadPtr( image->mallocData, 128 );
}

void rglImageFreeCPUStorage(void *data)
{
   rglImage *image = (rglImage*)data;

   if (!image->mallocData)
      return;

   free( image->mallocData );
   image->mallocStorageSize = 0;
   image->data = NULL;
   image->mallocData = NULL;
   image->dataState &= ~RGL_IMAGE_DATASTATE_HOST;
}

static void rglSetImage(void *data, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLsizei alignment, GLenum format, GLenum type, const void *pixels )
{
   rglImage *image = (rglImage*)data;

   image->width = width;
   image->height = height;
   image->depth = depth;
   image->alignment = alignment;

   image->xblk = 0;
   image->yblk = 0;

   image->xstride = 0;
   image->ystride = 0;
   image->zstride = 0;

   image->format = 0;
   image->type = 0;
   image->internalFormat = 0;
   const GLenum status = rglPlatformChooseInternalStorage( image, internalFormat );
   (( void )status );

   image->data = NULL;
   image->mallocData = NULL;
   image->mallocStorageSize = 0;

   image->isSet = GL_TRUE;

   {
      if ( image->xstride == 0 )
         image->xstride = rglGetPixelSize( image->format, image->type );
      if ( image->ystride == 0 )
         image->ystride = image->width * image->xstride;
      if ( image->zstride == 0 )
         image->zstride = image->height * image->ystride;
   }

   if (pixels)
   {
      rglImageAllocCPUStorage( image );
      if ( !image->data )
         return;

      rglRaster raster;
      raster.format = format;
      raster.type = type;
      raster.width = width;
      raster.height = height;
      raster.depth = depth;
      raster.data = (void*)pixels;

      raster.xstride = rglGetPixelSize( raster.format, raster.type );
      raster.ystride = (raster.width * raster.xstride + alignment - 1) / alignment * alignment;
      raster.zstride = raster.height * raster.ystride;

      rglRawRasterToImage( &raster, image, 0, 0, 0 );
      image->dataState = RGL_IMAGE_DATASTATE_HOST;
   }
   else
      image->dataState = RGL_IMAGE_DATASTATE_UNSET;
}

/*============================================================
  ENGINE
  ============================================================ */

static char* rglVendorString = "RetroArch";

static char* rglRendererString = "RGL";
static char* rglExtensionsString = "";

static char* rglVersionNumber = "2.00";
char* rglVersion = "2.00";

RGLcontext* _CurrentContext = NULL;

RGL_EXPORT RGLcontextHookFunction rglContextCreateHook = NULL;
RGL_EXPORT RGLcontextHookFunction rglContextDestroyHook = NULL;

void rglSetError( GLenum error )
{
}

GLAPI GLenum APIENTRY glGetError(void)
{
   if (!_CurrentContext )
      return GL_INVALID_OPERATION;
   else
   {
      GLenum error = _CurrentContext->error;

      _CurrentContext->error = GL_NO_ERROR;
      return error;
   }
}

GLAPI void APIENTRY glGetIntegerv(GLenum pname, GLint* params)
{
   switch (pname)
   {
      case GL_MAX_TEXTURE_SIZE:
         params[0] = RGLP_MAX_TEXTURE_SIZE;
         break;
      default:
         fprintf(stderr, "glGetIntegerv: enum not supported.\n");
         break;
   }
}

static void rglResetContext (void *data)
{
   RGLcontext *LContext = (RGLcontext*)data;

   rglTexNameSpaceResetNames( &LContext->textureNameSpace );
   rglTexNameSpaceResetNames( &LContext->bufferObjectNameSpace );
   rglTexNameSpaceResetNames( &LContext->framebufferNameSpace );
   rglTexNameSpaceResetNames( &LContext->fenceObjectNameSpace );

   LContext->ViewPort.X = 0;
   LContext->ViewPort.Y = 0;
   LContext->ViewPort.XSize = 0;
   LContext->ViewPort.YSize = 0;

   LContext->DepthNear = 0.f;
   LContext->DepthFar = 1.f;

   LContext->DrawBuffer = LContext->ReadBuffer = GL_COLOR_ATTACHMENT0_EXT;

   LContext->ShaderSRGBRemap = GL_FALSE;

   LContext->Blending = GL_FALSE;
   LContext->BlendColor.R = 0.0f;
   LContext->BlendColor.G = 0.0f;
   LContext->BlendColor.B = 0.0f;
   LContext->BlendColor.A = 0.0f;
   LContext->BlendEquationRGB = GL_FUNC_ADD;
   LContext->BlendEquationAlpha = GL_FUNC_ADD;
   LContext->BlendFactorSrcRGB = GL_ONE;
   LContext->BlendFactorDestRGB = GL_ZERO;
   LContext->BlendFactorSrcAlpha = GL_ONE;
   LContext->BlendFactorDestAlpha = GL_ZERO;

   LContext->ColorLogicOp = GL_FALSE;
   LContext->LogicOp = GL_COPY;

   LContext->Dithering = GL_TRUE;

   LContext->TexCoordReplaceMask = 0;

   for ( int i = 0;i < RGL_MAX_TEXTURE_IMAGE_UNITS;++i )
   {
      rglTextureImageUnit *tu = LContext->TextureImageUnits + i;
      tu->bound2D = 0;

      tu->enable2D = GL_FALSE;
      tu->fragmentTarget = 0;

      tu->lodBias = 0.f;
      tu->currentTexture = NULL;
   }

   LContext->ActiveTexture = 0;
   LContext->CurrentImageUnit = LContext->TextureImageUnits;

   LContext->packAlignment = 4;
   LContext->unpackAlignment = 4;

   rglAttributeState *as = (rglAttributeState*)&LContext->defaultAttribs0;

   for ( int i = 0; i < RGL_MAX_VERTEX_ATTRIBS; ++i )
   {
      as->attrib[i].clientSize = 4;
      as->attrib[i].clientType = GL_FLOAT;
      as->attrib[i].clientStride = 16;
      as->attrib[i].clientData = NULL;

      as->attrib[i].value[0] = 0.0f;
      as->attrib[i].value[1] = 0.0f;
      as->attrib[i].value[2] = 0.0f;
      as->attrib[i].value[3] = 1.0f;

      as->attrib[i].normalized = GL_FALSE;
      as->attrib[i].frequency = 1;

      as->attrib[i].arrayBuffer = 0;
   }

   as->attrib[RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[0] = 1.0f;
   as->attrib[RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[1] = 1.0f;
   as->attrib[RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[2] = 1.0f;
   as->attrib[RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[3] = 1.0f;

   as->attrib[RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[0] = 1.0f;
   as->attrib[RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[1] = 1.0f;
   as->attrib[RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[2] = 1.0f;
   as->attrib[RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[3] = 1.0f;

   as->attrib[RGL_ATTRIB_NORMAL_INDEX].value[0] = 0.f;
   as->attrib[RGL_ATTRIB_NORMAL_INDEX].value[1] = 0.f;
   as->attrib[RGL_ATTRIB_NORMAL_INDEX].value[2] = 1.f;

   as->DirtyMask = ( 1 << RGL_MAX_VERTEX_ATTRIBS ) - 1;
   as->EnabledMask = 0;
   as->HasVBOMask = 0;

   LContext->attribs = &LContext->defaultAttribs0;

   LContext->framebuffer = 0;

   LContext->VertexProgram = GL_FALSE;
   LContext->BoundVertexProgram = 0;

   LContext->FragmentProgram = GL_FALSE;
   LContext->BoundFragmentProgram = 0;

   LContext->ArrayBuffer = 0;
   LContext->PixelUnpackBuffer = 0;
   LContext->TextureBuffer = 0;

   LContext->VSync = GL_FALSE;
}

static rglTexture *rglAllocateTexture(void)
{
   GLuint size = sizeof(rglTexture) + sizeof(rglGcmTexture);
   rglTexture *texture = (rglTexture*)malloc(size);
   memset( texture, 0, size );
   texture->target = 0;
   texture->minFilter = GL_NEAREST_MIPMAP_LINEAR;
   texture->magFilter = GL_LINEAR;
   texture->minLod = -1000.f;
   texture->maxLod = 1000.f;
   texture->maxLevel = 1000;
   texture->wrapS = GL_REPEAT;
   texture->wrapT = GL_REPEAT;
   texture->wrapR = GL_REPEAT;
   texture->lodBias = 0.f;
   texture->maxAnisotropy = 1.f;
   texture->compareMode = GL_NONE;
   texture->compareFunc = GL_LEQUAL;
   texture->gammaRemap = 0;
   texture->vertexEnable = GL_FALSE;
   texture->usage = 0;
   texture->isRenderTarget = GL_FALSE;
   texture->image = NULL;
   texture->isComplete = GL_FALSE;
   texture->imageCount = 0;
   texture->revalidate = 0;
   texture->referenceBuffer = NULL;
   new( &texture->framebuffers ) RGL::Vector<rglFramebuffer *>();
   rglPlatformCreateTexture( texture );
   return texture;
}

static void rglFreeTexture (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   rglTextureTouchFBOs(texture);
   texture->framebuffers.~Vector<rglFramebuffer *>();

   if ( texture->image )
   {
      rglImage *image = texture->image;
      rglImageFreeCPUStorage( image );
      free( texture->image );
   }
   if ( texture->referenceBuffer )
      texture->referenceBuffer->textureReferences.removeElement( texture );
   rglPlatformDestroyTexture( texture );
   free( texture );
}

RGLcontext* psglCreateContext(void)
{
   RGLcontext* LContext = (RGLcontext*)malloc(sizeof(RGLcontext));

   if (!LContext)
      return NULL;

   memset(LContext, 0, sizeof(RGLcontext));

   LContext->error = GL_NO_ERROR;

   rglTexNameSpaceInit( &LContext->textureNameSpace, ( rglTexNameSpaceCreateFunction )rglAllocateTexture, ( rglTexNameSpaceDestroyFunction )rglFreeTexture );

   for ( int i = 0;i < RGL_MAX_TEXTURE_IMAGE_UNITS;++i )
   {
      rglTextureImageUnit *tu = LContext->TextureImageUnits + i;

      tu->default2D = rglAllocateTexture();
      if ( !tu->default2D )
      {
         psglDestroyContext( LContext );
         return NULL;
      }
      tu->default2D->target = GL_TEXTURE_2D;
   }

   rglTexNameSpaceInit( &LContext->bufferObjectNameSpace, ( rglTexNameSpaceCreateFunction )rglCreateBufferObject, ( rglTexNameSpaceDestroyFunction )rglFreeBufferObject );
   rglTexNameSpaceInit( &LContext->framebufferNameSpace, ( rglTexNameSpaceCreateFunction )rglCreateFramebuffer, ( rglTexNameSpaceDestroyFunction )rglDestroyFramebuffer );

   LContext->needValidate = 0;
   LContext->everAttached = 0;

   LContext->RGLcgLastError = CG_NO_ERROR;
   LContext->RGLcgErrorCallbackFunction = NULL;
   LContext->RGLcgContextHead = ( CGcontext )NULL;

   rglInitNameSpace( &LContext->cgProgramNameSpace );
   rglInitNameSpace( &LContext->cgParameterNameSpace );
   rglInitNameSpace( &LContext->cgContextNameSpace );

   rglResetContext( LContext );

   if (rglContextCreateHook)
      rglContextCreateHook( LContext );

   return( LContext );
}

void RGL_EXPORT psglResetCurrentContext(void)
{
   RGLcontext *context = _CurrentContext;
   rglResetContext(context);
   context->needValidate |= RGL_VALIDATE_ALL;
}

RGLcontext *psglGetCurrentContext(void)
{
   return _CurrentContext;
}

void RGL_EXPORT psglDestroyContext (void *data)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   RGLcontext *LContext = (RGLcontext*)data;
   if ( _CurrentContext == LContext )
   {
      rglGcmSetInvalidateVertexCache(thisContext);
      rglGcmFifoFinish( &rglGcmState_i.fifo );
   }

   while ( LContext->RGLcgContextHead != ( CGcontext )NULL )
   {
      RGLcontext* current = _CurrentContext;
      _CurrentContext = LContext;
      cgDestroyContext( LContext->RGLcgContextHead );
      _CurrentContext = current;
   }
   rglFreeNameSpace( &LContext->cgProgramNameSpace );
   rglFreeNameSpace( &LContext->cgParameterNameSpace );
   rglFreeNameSpace( &LContext->cgContextNameSpace );

   if ( rglContextDestroyHook ) rglContextDestroyHook( LContext );

   for ( int i = 0; i < RGL_MAX_TEXTURE_IMAGE_UNITS; ++i )
   {
      rglTextureImageUnit* tu = LContext->TextureImageUnits + i;
      if ( tu->default2D ) rglFreeTexture( tu->default2D );
   }

   rglTexNameSpaceFree( &LContext->textureNameSpace );
   rglTexNameSpaceFree( &LContext->bufferObjectNameSpace );
   rglTexNameSpaceFree( &LContext->fenceObjectNameSpace );
   rglTexNameSpaceFree( &LContext->framebufferNameSpace );

   if ( _CurrentContext == LContext )
      psglMakeCurrent( NULL, NULL );

   free( LContext );
}

GLAPI void APIENTRY glEnable( GLenum cap )
{
   RGLcontext* LContext = _CurrentContext;

   switch ( cap )
   {
      case GL_TEXTURE_2D:
         LContext->CurrentImageUnit->enable2D = GL_TRUE;
         LContext->CurrentImageUnit->currentTexture = rglGetCurrentTexture( LContext->CurrentImageUnit,
               LContext->CurrentImageUnit->fragmentTarget );
         LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
         break;
      case GL_SHADER_SRGB_REMAP_SCE:
         LContext->ShaderSRGBRemap = GL_TRUE;
         LContext->needValidate |= RGL_VALIDATE_SHADER_SRGB_REMAP;
         break;
      case GL_BLEND:
         LContext->Blending = GL_TRUE;
         LContext->needValidate |= RGL_VALIDATE_BLENDING;
         break;
      case GL_DITHER:
         LContext->Dithering = GL_TRUE;
         break;
      case GL_VSYNC_SCE:
         LContext->VSync = GL_TRUE;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
}

GLAPI void APIENTRY glDisable( GLenum cap )
{
   RGLcontext* LContext = _CurrentContext;

   switch ( cap )
   {
      case GL_TEXTURE_2D:
         LContext->CurrentImageUnit->enable2D = GL_FALSE;
         LContext->CurrentImageUnit->currentTexture = rglGetCurrentTexture( LContext->CurrentImageUnit,
               LContext->CurrentImageUnit->fragmentTarget );
         LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
         break;
      case GL_SHADER_SRGB_REMAP_SCE:
         LContext->ShaderSRGBRemap = GL_FALSE;
         LContext->needValidate |= RGL_VALIDATE_SHADER_SRGB_REMAP;
         break;
      case GL_BLEND:
         LContext->Blending = GL_FALSE;
         LContext->needValidate |= RGL_VALIDATE_BLENDING;
         break;
      case GL_DITHER:
         LContext->Dithering = GL_FALSE;
         break;
      case GL_VSYNC_SCE:
         LContext->VSync = GL_FALSE;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
}

GLAPI void APIENTRY glEnableClientState( GLenum array )
{
   switch ( array )
   {
      case GL_VERTEX_ARRAY:
         rglEnableVertexAttribArrayNV( RGL_ATTRIB_POSITION_INDEX );
         break;
      case GL_COLOR_ARRAY:
         rglEnableVertexAttribArrayNV( RGL_ATTRIB_PRIMARY_COLOR_INDEX );
         break;
      case GL_NORMAL_ARRAY:
         rglEnableVertexAttribArrayNV( RGL_ATTRIB_NORMAL_INDEX );
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
}

GLAPI void APIENTRY glDisableClientState( GLenum array )
{
   switch ( array )
   {
      case GL_VERTEX_ARRAY:
         rglDisableVertexAttribArrayNV( RGL_ATTRIB_POSITION_INDEX );
         break;
      case GL_COLOR_ARRAY:
         rglDisableVertexAttribArrayNV( RGL_ATTRIB_PRIMARY_COLOR_INDEX );
         break;
      case GL_NORMAL_ARRAY:
         rglDisableVertexAttribArrayNV( RGL_ATTRIB_NORMAL_INDEX );
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
}

GLAPI const GLubyte* APIENTRY glGetString( GLenum name )
{
   switch ( name )
   {
      case GL_VENDOR:
         return(( GLubyte* )rglVendorString );
      case GL_RENDERER:
         return(( GLubyte* )rglRendererString );
      case GL_VERSION:
         return(( GLubyte* )rglVersionNumber );
      case GL_EXTENSIONS:
         return(( GLubyte* )rglExtensionsString );
      default:
         {
            rglSetError( GL_INVALID_ENUM );
            return(( GLubyte* )NULL );
         }
   }
}

void psglInit (void *data)
{
   RGLinitOptions *options = (RGLinitOptions*)data;
   rglPsglPlatformInit(options);
}

void psglExit(void)
{
   rglPsglPlatformExit();
}

/*============================================================
  RASTER
  ============================================================ */

GLAPI void APIENTRY glViewport( GLint x, GLint y, GLsizei width, GLsizei height )
{
   RGLcontext*	LContext = _CurrentContext;

   LContext->ViewPort.X = x;
   LContext->ViewPort.Y = y;
   LContext->ViewPort.XSize = width;
   LContext->ViewPort.YSize = height;
   LContext->needValidate |= RGL_VALIDATE_VIEWPORT;
}

/*============================================================
  TEXTURES (INTERNAL)
  ============================================================ */

// Reallocate images held by a texture
void rglReallocateImages (void *data, GLint level, GLsizei dimension )
{
   rglTexture *texture = (rglTexture*)data;
   GLuint oldCount = texture->imageCount;

   if (dimension <= 0)
      dimension = 1;

   GLuint n = level + 1 + rglLog2( dimension );
   n = MAX( n, oldCount );

   rglImage *image = ( rglImage * )realloc( texture->image, n * sizeof( rglImage ) );
   memset( image + oldCount, 0, ( n - oldCount )*sizeof( rglImage ) );

   texture->image = image;
   texture->imageCount = n;
}

rglTexture *rglGetCurrentTexture (const void *data, GLenum target)
{
   const rglTextureImageUnit *unit = (const rglTextureImageUnit*)data;
   RGLcontext*	LContext = _CurrentContext;
   GLuint name = unit->bound2D;
   rglTexture *defaultTexture = unit->default2D;

   if (name)
      return ( rglTexture * )LContext->textureNameSpace.data[name];
   else
      return defaultTexture;
}

static void rglGetImage( GLenum target, GLint level, rglTexture **texture, rglImage **image, GLsizei reallocateSize )
{
   RGLcontext*	LContext = _CurrentContext;
   rglTextureImageUnit *unit = LContext->CurrentImageUnit;

   GLenum expectedTarget = GL_TEXTURE_2D;

   rglTexture *tex = rglGetCurrentTexture( unit, expectedTarget );

   if (level >= ( int )tex->imageCount)
      rglReallocateImages( tex, level, reallocateSize );

   *image = tex->image + level;
   *texture = tex;
}

void rglBindTextureInternal (void *data, GLuint name, GLenum target )
{
   rglTextureImageUnit *unit = (rglTextureImageUnit*)data;
   RGLcontext*	LContext = _CurrentContext;
   rglTexture *texture = NULL;

   if (name)
   {
      rglTexNameSpaceCreateNameLazy( &LContext->textureNameSpace, name );
      texture = ( rglTexture * )LContext->textureNameSpace.data[name];

      if (!texture->target)
         texture->target = target;
   }

   unit->bound2D = name;
   unit->currentTexture = rglGetCurrentTexture( unit, unit->fragmentTarget );

   LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
}

/*============================================================
  TEXTURES
  ============================================================ */

GLAPI void APIENTRY glGenTextures( GLsizei n, GLuint *textures )
{
   RGLcontext*	LContext = _CurrentContext;
   rglTexNameSpaceGenNames( &LContext->textureNameSpace, n, textures );
}

GLAPI void APIENTRY glDeleteTextures( GLsizei n, const GLuint *textures )
{
   RGLcontext*	LContext = _CurrentContext;

   for ( int i = 0;i < n;++i )
   {
      if (textures[i])
      {
         GLuint name = textures[i];
         int unit;

         for ( unit = 0; unit < RGL_MAX_TEXTURE_IMAGE_UNITS; ++unit)
         {
            rglTextureImageUnit *tu = LContext->TextureImageUnits + unit;
            GLboolean dirty = GL_FALSE;
            if ( tu->bound2D == name )
            {
               tu->bound2D = 0;
               dirty = GL_TRUE;
            }
            if ( dirty )
            {
               tu->currentTexture = rglGetCurrentTexture( tu, tu->fragmentTarget );
               LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
            }
         }
      }
   }

   rglTexNameSpaceDeleteNames( &LContext->textureNameSpace, n, textures );
}

GLAPI void APIENTRY glTexParameteri( GLenum target, GLenum pname, GLint param )
{
   RGLcontext*	LContext = _CurrentContext;
   rglTexture *texture = rglGetCurrentTexture( LContext->CurrentImageUnit, target );
   switch ( pname )
   {
      case GL_TEXTURE_MIN_FILTER:
         texture->minFilter = param;
         if ( texture->referenceBuffer == 0 )
            texture->revalidate |= RGL_TEXTURE_REVALIDATE_LAYOUT;
         break;
      case GL_TEXTURE_MAG_FILTER:
         texture->magFilter = param;
         break;
      case GL_TEXTURE_MAX_LEVEL:
         texture->maxLevel = param;
         break;
      case GL_TEXTURE_WRAP_S:
         texture->wrapS = param;
         break;
      case GL_TEXTURE_WRAP_T:
         texture->wrapT = param;
         break;
      case GL_TEXTURE_WRAP_R:
         texture->wrapR = param;
         break;
      case GL_TEXTURE_FROM_VERTEX_PROGRAM_SCE:
         if ( param != 0 )
            texture->vertexEnable = GL_TRUE;
         else
            texture->vertexEnable = GL_FALSE;
         texture->revalidate |= RGL_TEXTURE_REVALIDATE_LAYOUT;
         break;
      case GL_TEXTURE_COMPARE_MODE_ARB:
         texture->compareMode = param;
         break;
      case GL_TEXTURE_COMPARE_FUNC_ARB:
         texture->compareFunc = param;
         break;
      case GL_TEXTURE_GAMMA_REMAP_R_SCE:
      case GL_TEXTURE_GAMMA_REMAP_G_SCE:
      case GL_TEXTURE_GAMMA_REMAP_B_SCE:
      case GL_TEXTURE_GAMMA_REMAP_A_SCE:
         {
            GLuint bit = 1 << ( pname - GL_TEXTURE_GAMMA_REMAP_R_SCE );
            if ( param ) texture->gammaRemap |= bit;
            else texture->gammaRemap &= ~bit;
         }
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
   texture->revalidate |= RGL_TEXTURE_REVALIDATE_PARAMETERS;
   LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
}

GLAPI void APIENTRY glBindTexture( GLenum target, GLuint name )
{
   RGLcontext*	LContext = _CurrentContext;
   rglTextureImageUnit *unit = LContext->CurrentImageUnit;

   rglBindTextureInternal( unit, name, target );
}

GLAPI void APIENTRY glTexImage2D( GLenum target, GLint level, GLint internalFormat,
      GLsizei width, GLsizei height, GLint border, GLenum format,
      GLenum type, const GLvoid *pixels )
{
   RGLcontext*	LContext = _CurrentContext;

   rglTexture *texture;
   rglImage *image;
   rglGetImage(target, level, &texture, &image, MAX(width, height));

   image->dataState = RGL_IMAGE_DATASTATE_UNSET;

   rglBufferObject* bufferObject = NULL;
   if ( LContext->PixelUnpackBuffer != 0 )
   {
      bufferObject = 
         (rglBufferObject*)LContext->bufferObjectNameSpace.data[LContext->PixelUnpackBuffer];
      pixels = rglPlatformBufferObjectMap( bufferObject, GL_READ_ONLY ) +
         (( const GLubyte* )pixels - ( const GLubyte* )NULL );
   }

   rglSetImage(
         image,
         internalFormat,
         width, height, 1,
         LContext->unpackAlignment,
         format, type,
         pixels );


   if ( LContext->PixelUnpackBuffer != 0 )
      rglPlatformBufferObjectUnmap( bufferObject );

   texture->revalidate |= RGL_TEXTURE_REVALIDATE_IMAGES;

   rglTextureTouchFBOs( texture );

   LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
}

GLAPI void APIENTRY glActiveTexture( GLenum texture )
{
   RGLcontext*	LContext = _CurrentContext;

   int unit = texture - GL_TEXTURE0;
   LContext->ActiveTexture = unit;

   if(unit < RGL_MAX_TEXTURE_IMAGE_UNITS)
      LContext->CurrentImageUnit = LContext->TextureImageUnits + unit;
   else
      LContext->CurrentImageUnit = NULL;
}

/*============================================================
  VERTEX ARRAYS
  ============================================================ */

GLAPI void APIENTRY glVertexPointer( GLint size, GLenum type, GLsizei stride, const GLvoid* pointer )
{
   rglVertexAttribPointerNV( RGL_ATTRIB_POSITION_INDEX, size, type, GL_FALSE, stride, pointer );
}

void rglVertexAttribPointerNV(
      GLuint index,
      GLint fsize,
      GLenum type,
      GLboolean normalized,
      GLsizei stride,
      const void* pointer)
{
   RGLcontext*	LContext = _CurrentContext;

   GLsizei defaultStride = 0;
   switch (type)
   {
      case GL_FLOAT:
      case GL_HALF_FLOAT_ARB:
      case GL_BYTE:
      case GL_UNSIGNED_BYTE:
      case GL_SHORT:
      case GL_FIXED:
         defaultStride = fsize * rglGetTypeSize(type);
         break;
      case GL_FIXED_11_11_10_SCE:
         defaultStride = 4;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }

   rglAttributeState* as = LContext->attribs;
   rglAttribute* attrib = as->attrib + index;
   attrib->clientSize = fsize;
   attrib->clientType = type;
   attrib->clientStride = stride ? stride : defaultStride;
   attrib->clientData = (void*)pointer;
   attrib->arrayBuffer = LContext->ArrayBuffer;
   attrib->normalized = normalized;
   RGLBIT_ASSIGN( as->HasVBOMask, index, attrib->arrayBuffer != 0 );

   RGLBIT_TRUE( as->DirtyMask, index );
}

void rglEnableVertexAttribArrayNV (GLuint index)
{
   RGLcontext *LContext = _CurrentContext;

   RGLBIT_TRUE( LContext->attribs->EnabledMask, index );
   RGLBIT_TRUE( LContext->attribs->DirtyMask, index );
}

void rglDisableVertexAttribArrayNV (GLuint index)
{
   RGLcontext *LContext = _CurrentContext;

   RGLBIT_FALSE( LContext->attribs->EnabledMask, index );
   RGLBIT_TRUE( LContext->attribs->DirtyMask, index );
}

/*============================================================
  DEVICE CONTEXT CREATION
  ============================================================ */

RGLdevice *_CurrentDevice = NULL;

void rglDeviceInit (void *data)
{
   RGLinitOptions *options = (RGLinitOptions*)data;
   rglPlatformDeviceInit(options);
}

void rglDeviceExit(void)
{
   rglPlatformDeviceExit();
}

RGL_EXPORT RGLdevice*	psglCreateDeviceAuto( GLenum colorFormat, GLenum depthFormat, GLenum multisamplingMode )
{
   return rglPlatformCreateDeviceAuto(colorFormat, depthFormat, multisamplingMode);
}

RGL_EXPORT RGLdevice*	psglCreateDeviceExtended (const void *data)
{
   const RGLdeviceParameters *parameters = (const RGLdeviceParameters*)data;
   return rglPlatformCreateDeviceExtended(parameters);
}

RGL_EXPORT GLfloat psglGetDeviceAspectRatio (const void *data)
{
   const RGLdevice *device = (const RGLdevice*)data;
   return rglPlatformGetDeviceAspectRatio(device);
}

RGL_EXPORT void psglGetDeviceDimensions (const RGLdevice * device, GLuint *width, GLuint *height)
{
   *width = device->deviceParameters.width;
   *height = device->deviceParameters.height;
}

RGL_EXPORT void psglGetRenderBufferDimensions (const RGLdevice * device, GLuint *width, GLuint *height)
{
   *width = device->deviceParameters.renderWidth;
   *height = device->deviceParameters.renderHeight;
}

RGL_EXPORT void psglDestroyDevice (void *data)
{
   RGLdevice *device = (RGLdevice*)data;
   if (_CurrentDevice == device)
      psglMakeCurrent( NULL, NULL );

   if (device->rasterDriver)
      rglPlatformRasterExit( device->rasterDriver );

   rglPlatformDestroyDevice( device );

   free( device );
}

void RGL_EXPORT psglMakeCurrent (RGLcontext *context, RGLdevice *device)
{
   _CurrentContext = NULL;
   _CurrentDevice = NULL;

   if ( context && device )
   {
      _CurrentContext = context;
      _CurrentDevice = device;

      if ( !device->rasterDriver )
         device->rasterDriver = rglPlatformRasterInit();

      //attach context
      if (!context->everAttached)
      {
         context->ViewPort.XSize = device->deviceParameters.width;
         context->ViewPort.YSize = device->deviceParameters.height;
         context->needValidate |= RGL_VALIDATE_VIEWPORT;
         context->everAttached = GL_TRUE;
      }

      context->needValidate = RGL_VALIDATE_ALL;
      context->attribs->DirtyMask = ( 1 << RGL_MAX_VERTEX_ATTRIBS ) - 1;
   }
}

RGLdevice *psglGetCurrentDevice (void)
{
   return _CurrentDevice;
}

#include "rgl_ps3_cg.cpp"

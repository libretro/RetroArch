/*
 file_allocation_table.c
 Reading, writing and manipulation of the FAT structure on
 a FAT partition

 Copyright (c) 2006 Michael "Chishm" Chisholm

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "file_allocation_table.h"
#include "partition.h"
#include "mem_allocate.h"
#include <string.h>

/*
Gets the cluster linked from input cluster
*/
uint32_t _FAT_fat_nextCluster(PARTITION* partition, uint32_t cluster)
{
	uint32_t nextCluster = CLUSTER_FREE;
	sec_t sector;
	int offset;

	if (cluster == CLUSTER_FREE)
		return CLUSTER_FREE;

	switch (partition->filesysType)
	{
		case FS_UNKNOWN:
			return CLUSTER_ERROR;
			break;

		case FS_FAT12:
		{
			u32 nextCluster_h;
			sector = partition->fat.fatStart + (((cluster * 3) / 2) / partition->bytesPerSector);
			offset = ((cluster * 3) / 2) % partition->bytesPerSector;


			_FAT_cache_readLittleEndianValue (partition->cache, &nextCluster, sector, offset, sizeof(u8));

			offset++;

			if (offset >= partition->bytesPerSector)
         {
				offset = 0;
				sector++;
			}
			nextCluster_h = 0;

			_FAT_cache_readLittleEndianValue (partition->cache, &nextCluster_h, sector, offset, sizeof(u8));
			nextCluster |= (nextCluster_h << 8);

			if (cluster & 0x01)
				nextCluster = nextCluster >> 4;
         else
				nextCluster &= 0x0FFF;

			if (nextCluster >= 0x0FF7)
				nextCluster = CLUSTER_EOF;

			break;
		}
		case FS_FAT16:
			sector = partition->fat.fatStart + ((cluster << 1) / partition->bytesPerSector);
			offset = (cluster % (partition->bytesPerSector >> 1)) << 1;

			_FAT_cache_readLittleEndianValue (partition->cache, &nextCluster, sector, offset, sizeof(u16));

			if (nextCluster >= 0xFFF7)
				nextCluster = CLUSTER_EOF;
			break;

		case FS_FAT32:
			sector = partition->fat.fatStart + ((cluster << 2) / partition->bytesPerSector);
			offset = (cluster % (partition->bytesPerSector >> 2)) << 2;

			_FAT_cache_readLittleEndianValue (partition->cache, &nextCluster, sector, offset, sizeof(u32));

			if (nextCluster >= 0x0FFFFFF7)
				nextCluster = CLUSTER_EOF;
			break;

		default:
			return CLUSTER_ERROR;
			break;
	}

	return nextCluster;
}

/*
writes value into the correct offset within a partition's FAT, based
on the cluster number.
*/
static bool _FAT_fat_writeFatEntry (PARTITION* partition, uint32_t cluster, uint32_t value)
{
	sec_t sector;
	int offset;
	uint32_t oldValue;

	if ((cluster < CLUSTER_FIRST) || (cluster > partition->fat.lastCluster /* This will catch CLUSTER_ERROR */))
	{
		return false;
	}

	switch (partition->filesysType)
	{
		case FS_UNKNOWN:
			return false;
			break;

		case FS_FAT12:
			sector = partition->fat.fatStart + (((cluster * 3) / 2) / partition->bytesPerSector);
			offset = ((cluster * 3) / 2) % partition->bytesPerSector;

			if (cluster & 0x01)
         {

            _FAT_cache_readLittleEndianValue (partition->cache, &oldValue, sector, offset, sizeof(u8));

            value = (value << 4) | (oldValue & 0x0F);

            _FAT_cache_writeLittleEndianValue (partition->cache, value & 0xFF, sector, offset, sizeof(u8));

            offset++;
            if (offset >= partition->bytesPerSector)
            {
               offset = 0;
               sector++;
            }

            _FAT_cache_writeLittleEndianValue (partition->cache, (value >> 8) & 0xFF, sector, offset, sizeof(u8));

         } else {

				_FAT_cache_writeLittleEndianValue (partition->cache, value, sector, offset, sizeof(u8));

				offset++;
				if (offset >= partition->bytesPerSector)
            {
					offset = 0;
					sector++;
				}

				_FAT_cache_readLittleEndianValue (partition->cache, &oldValue, sector, offset, sizeof(u8));

				value = ((value >> 8) & 0x0F) | (oldValue & 0xF0);

				_FAT_cache_writeLittleEndianValue (partition->cache, value, sector, offset, sizeof(u8));
			}

			break;

		case FS_FAT16:
			sector = partition->fat.fatStart + ((cluster << 1) / partition->bytesPerSector);
			offset = (cluster % (partition->bytesPerSector >> 1)) << 1;

			_FAT_cache_writeLittleEndianValue (partition->cache, value, sector, offset, sizeof(u16));

			break;

		case FS_FAT32:
			sector = partition->fat.fatStart + ((cluster << 2) / partition->bytesPerSector);
			offset = (cluster % (partition->bytesPerSector >> 2)) << 2;

			_FAT_cache_writeLittleEndianValue (partition->cache, value, sector, offset, sizeof(u32));

			break;

		default:
			return false;
			break;
	}

	return true;
}

/*-----------------------------------------------------------------
gets the first available free cluster, sets it
to end of file, links the input cluster to it then returns the
cluster number
If an error occurs, return CLUSTER_ERROR
-----------------------------------------------------------------*/
uint32_t _FAT_fat_linkFreeCluster(PARTITION* partition, uint32_t cluster)
{
	uint32_t firstFree;
	uint32_t curLink;
	uint32_t lastCluster;
	bool loopedAroundFAT = false;

	lastCluster =  partition->fat.lastCluster;

	if (cluster > lastCluster)
		return CLUSTER_ERROR;

	/* Check if the cluster already has a link, and return it if so */
	curLink = _FAT_fat_nextCluster(partition, cluster);
	if ((curLink >= CLUSTER_FIRST) && (curLink <= lastCluster))
		return curLink;	/* Return the current link - don't allocate a new one */

	/* Get a free cluster */
	firstFree = partition->fat.firstFree;
	/* Start at first valid cluster */
	if (firstFree < CLUSTER_FIRST)
		firstFree = CLUSTER_FIRST;

	/* Search until a free cluster is found */
	while (_FAT_fat_nextCluster(partition, firstFree) != CLUSTER_FREE)
   {
		firstFree++;
		if (firstFree > lastCluster)
      {
         if (loopedAroundFAT)
         {
            /* If couldn't get a free cluster then return an error */
            partition->fat.firstFree = firstFree;
            return CLUSTER_ERROR;
         }

         /* Try looping back to the beginning of the FAT
          * This was suggested by loopy */
         firstFree = CLUSTER_FIRST;
         loopedAroundFAT = true;
      }
	}
	partition->fat.firstFree = firstFree;
	if(partition->fat.numberFreeCluster)
		partition->fat.numberFreeCluster--;
	partition->fat.numberLastAllocCluster = firstFree;

   /* Update the linked from FAT entry */
	if ((cluster >= CLUSTER_FIRST) && (cluster <= lastCluster))
		_FAT_fat_writeFatEntry (partition, cluster, firstFree);
	/* Create the linked to FAT entry */
	_FAT_fat_writeFatEntry (partition, firstFree, CLUSTER_EOF);

	return firstFree;
}

/*-----------------------------------------------------------------
gets the first available free cluster, sets it
to end of file, links the input cluster to it, clears the new
cluster to 0 valued bytes, then returns the cluster number
If an error occurs, return CLUSTER_ERROR
-----------------------------------------------------------------*/
uint32_t _FAT_fat_linkFreeClusterCleared (PARTITION* partition, uint32_t cluster)
{
	uint32_t i;
	uint8_t *emptySector;

	/* Link the cluster */
	uint32_t newCluster = _FAT_fat_linkFreeCluster(partition, cluster);

	if (newCluster == CLUSTER_FREE || newCluster == CLUSTER_ERROR)
		return CLUSTER_ERROR;

	emptySector = (uint8_t*) _FAT_mem_allocate(partition->bytesPerSector);

	/* Clear all the sectors within the cluster */
	memset (emptySector, 0, partition->bytesPerSector);
	for (i = 0; i < partition->sectorsPerCluster; i++)
   {
		_FAT_cache_writeSectors (partition->cache,
			_FAT_fat_clusterToSector (partition, newCluster) + i,
			1, emptySector);
	}

	_FAT_mem_free(emptySector);

	return newCluster;
}


/*-----------------------------------------------------------------
_FAT_fat_clearLinks
frees any cluster used by a file
-----------------------------------------------------------------*/
bool _FAT_fat_clearLinks (PARTITION* partition, uint32_t cluster)
{
	uint32_t nextCluster;

	if ((cluster < CLUSTER_FIRST) || (cluster > partition->fat.lastCluster /* This will catch CLUSTER_ERROR */))
		return false;

	/* If this clears up more space in the FAT before the current free pointer, move it backwards */
	if (cluster < partition->fat.firstFree)
		partition->fat.firstFree = cluster;

	while ((cluster != CLUSTER_EOF) && (cluster != CLUSTER_FREE) && (cluster != CLUSTER_ERROR))
   {
		/* Store next cluster before erasing the link */
		nextCluster = _FAT_fat_nextCluster (partition, cluster);

		/* Erase the link */
		_FAT_fat_writeFatEntry (partition, cluster, CLUSTER_FREE);

		if(partition->fat.numberFreeCluster < (partition->numberOfSectors/partition->sectorsPerCluster))
			partition->fat.numberFreeCluster++;
		/* Move onto next cluster */
		cluster = nextCluster;
	}

	return true;
}

/*-----------------------------------------------------------------
_FAT_fat_trimChain
Drop all clusters past the chainLength.
If chainLength is 0, all clusters are dropped.
If chainLength is 1, the first cluster is kept and the rest are
dropped, and so on.
Return the last cluster left in the chain.
-----------------------------------------------------------------*/
uint32_t _FAT_fat_trimChain (PARTITION* partition, uint32_t startCluster, unsigned int chainLength)
{
   uint32_t nextCluster;

   if (chainLength == 0)
   {
      /* Drop the entire chain */
      _FAT_fat_clearLinks (partition, startCluster);
      return CLUSTER_FREE;
   }

   /* Find the last cluster in the chain, and the one after it */
   chainLength--;
   nextCluster = _FAT_fat_nextCluster (partition, startCluster);
   while ((chainLength > 0) && (nextCluster != CLUSTER_FREE) && (nextCluster != CLUSTER_EOF))
   {
      chainLength--;
      startCluster = nextCluster;
      nextCluster = _FAT_fat_nextCluster (partition, startCluster);
   }

   /* Drop all clusters after the last in the chain */
   if (nextCluster != CLUSTER_FREE && nextCluster != CLUSTER_EOF)
      _FAT_fat_clearLinks (partition, nextCluster);

   /* Mark the last cluster in the chain as the end of the file */
   _FAT_fat_writeFatEntry (partition, startCluster, CLUSTER_EOF);

   return startCluster;
}

/*-----------------------------------------------------------------
_FAT_fat_lastCluster
Trace the cluster links until the last one is found
-----------------------------------------------------------------*/
uint32_t _FAT_fat_lastCluster (PARTITION* partition, uint32_t cluster)
{
	while ((_FAT_fat_nextCluster(partition, cluster) != CLUSTER_FREE) && (_FAT_fat_nextCluster(partition, cluster) != CLUSTER_EOF))
		cluster = _FAT_fat_nextCluster(partition, cluster);
	return cluster;
}

/*-----------------------------------------------------------------
_FAT_fat_freeClusterCount
Return the number of free clusters available
-----------------------------------------------------------------*/
unsigned int _FAT_fat_freeClusterCount (PARTITION* partition)
{
	unsigned int count = 0;
	uint32_t curCluster;

	for (curCluster = CLUSTER_FIRST; curCluster <= partition->fat.lastCluster; curCluster++)
   {
		if (_FAT_fat_nextCluster(partition, curCluster) == CLUSTER_FREE)
			count++;
	}

	return count;
}


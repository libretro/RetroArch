/*
 partition.c
 Functions for mounting and dismounting partitions
 on various block devices.

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

#include "partition.h"
#include "bit_ops.h"
#include "file_allocation_table.h"
#include "directory.h"
#include "mem_allocate.h"
#include "fatfile.h"

#include <string.h>
#include <ctype.h>
#include <sys/iosupport.h>

/*
Data offsets
*/

// BIOS Parameter Block offsets
enum BPB {
	BPB_jmpBoot = 0x00,
	BPB_OEMName = 0x03,
	// BIOS Parameter Block
	BPB_bytesPerSector = 0x0B,
	BPB_sectorsPerCluster = 0x0D,
	BPB_reservedSectors = 0x0E,
	BPB_numFATs = 0x10,
	BPB_rootEntries = 0x11,
	BPB_numSectorsSmall = 0x13,
	BPB_mediaDesc = 0x15,
	BPB_sectorsPerFAT = 0x16,
	BPB_sectorsPerTrk = 0x18,
	BPB_numHeads = 0x1A,
	BPB_numHiddenSectors = 0x1C,
	BPB_numSectors = 0x20,
	// Ext BIOS Parameter Block for FAT16
	BPB_FAT16_driveNumber = 0x24,
	BPB_FAT16_reserved1 = 0x25,
	BPB_FAT16_extBootSig = 0x26,
	BPB_FAT16_volumeID = 0x27,
	BPB_FAT16_volumeLabel = 0x2B,
	BPB_FAT16_fileSysType = 0x36,
	// Bootcode
	BPB_FAT16_bootCode = 0x3E,
	// FAT32 extended block
	BPB_FAT32_sectorsPerFAT32 = 0x24,
	BPB_FAT32_extFlags = 0x28,
	BPB_FAT32_fsVer = 0x2A,
	BPB_FAT32_rootClus = 0x2C,
	BPB_FAT32_fsInfo = 0x30,
	BPB_FAT32_bkBootSec = 0x32,
	// Ext BIOS Parameter Block for FAT32
	BPB_FAT32_driveNumber = 0x40,
	BPB_FAT32_reserved1 = 0x41,
	BPB_FAT32_extBootSig = 0x42,
	BPB_FAT32_volumeID = 0x43,
	BPB_FAT32_volumeLabel = 0x47,
	BPB_FAT32_fileSysType = 0x52,
	// Bootcode
	BPB_FAT32_bootCode = 0x5A,
	BPB_bootSig_55 = 0x1FE,
	BPB_bootSig_AA = 0x1FF
};

// File system information block offsets
enum FSIB
{
	FSIB_SIG1 = 0x00,
	FSIB_SIG2 = 0x1e4,
	FSIB_numberOfFreeCluster = 0x1e8,
	FSIB_numberLastAllocCluster = 0x1ec,
	FSIB_bootSig_55 = 0x1FE,
	FSIB_bootSig_AA = 0x1FF
};

static const char FAT_SIG[3] = {'F', 'A', 'T'};
static const char FS_INFO_SIG1[4] = {'R', 'R', 'a', 'A'};
static const char FS_INFO_SIG2[4] = {'r', 'r', 'A', 'a'};

sec_t FindFirstValidPartition_buf(const DISC_INTERFACE* disc, uint8_t *sectorBuffer)
{
	uint8_t part_table[16*4];
	uint8_t *ptr;
	int i;

	// Read first sector of disc
	if (!_FAT_disc_readSectors (disc, 0, 1, sectorBuffer)) {
		return 0;
	}

	memcpy(part_table,sectorBuffer+0x1BE,16*4);
	ptr = part_table;

	for(i=0;i<4;i++,ptr+=16) {
		sec_t part_lba = u8array_to_u32(ptr, 0x8);

		if (!memcmp(sectorBuffer + BPB_FAT16_fileSysType, FAT_SIG, sizeof(FAT_SIG)) ||
			!memcmp(sectorBuffer + BPB_FAT32_fileSysType, FAT_SIG, sizeof(FAT_SIG))) {
			return part_lba;
		}

		if(ptr[4]==0) continue;

		if(ptr[4]==0x0F) {
			sec_t part_lba2=part_lba;
			sec_t next_lba2=0;
			int n;

			for(n=0;n<8;n++) // max 8 logic partitions
			{
				if(!_FAT_disc_readSectors (disc, part_lba+next_lba2, 1, sectorBuffer)) return 0;

				part_lba2 = part_lba + next_lba2 + u8array_to_u32(sectorBuffer, 0x1C6) ;
				next_lba2 = u8array_to_u32(sectorBuffer, 0x1D6);

				if(!_FAT_disc_readSectors (disc, part_lba2, 1, sectorBuffer)) return 0;

				if (!memcmp(sectorBuffer + BPB_FAT16_fileSysType, FAT_SIG, sizeof(FAT_SIG)) ||
					!memcmp(sectorBuffer + BPB_FAT32_fileSysType, FAT_SIG, sizeof(FAT_SIG)))
				{
					return part_lba2;
				}

				if(next_lba2==0) break;
			}
		} else {
			if(!_FAT_disc_readSectors (disc, part_lba, 1, sectorBuffer)) return 0;
			if (!memcmp(sectorBuffer + BPB_FAT16_fileSysType, FAT_SIG, sizeof(FAT_SIG)) ||
				!memcmp(sectorBuffer + BPB_FAT32_fileSysType, FAT_SIG, sizeof(FAT_SIG))) {
				return part_lba;
			}
		}
	}
	return 0;
}

sec_t FindFirstValidPartition(const DISC_INTERFACE* disc)
{
	uint8_t *sectorBuffer = (uint8_t*) _FAT_mem_align(MAX_SECTOR_SIZE);
	if (!sectorBuffer) return 0;
	sec_t ret = FindFirstValidPartition_buf(disc, sectorBuffer);
	_FAT_mem_free(sectorBuffer);
	return ret;
}

PARTITION* _FAT_partition_constructor_buf (const DISC_INTERFACE* disc, uint32_t cacheSize, uint32_t sectorsPerPage, sec_t startSector, uint8_t *sectorBuffer)
{
	PARTITION* partition;

	// Read first sector of disc
	if (!_FAT_disc_readSectors (disc, startSector, 1, sectorBuffer)) {
		return NULL;
	}

	// Make sure it is a valid MBR or boot sector
	if ( (sectorBuffer[BPB_bootSig_55] != 0x55) || (sectorBuffer[BPB_bootSig_AA] != 0xAA)) {
		return NULL;
	}

	if (startSector != 0) {
		// We're told where to start the partition, so just accept it
	} else if (!memcmp(sectorBuffer + BPB_FAT16_fileSysType, FAT_SIG, sizeof(FAT_SIG))) {
		// Check if there is a FAT string, which indicates this is a boot sector
		startSector = 0;
	} else if (!memcmp(sectorBuffer + BPB_FAT32_fileSysType, FAT_SIG, sizeof(FAT_SIG))) {
		// Check for FAT32
		startSector = 0;
	} else {
		startSector = FindFirstValidPartition_buf(disc, sectorBuffer);
		if (!_FAT_disc_readSectors (disc, startSector, 1, sectorBuffer)) {
			return NULL;
		}
	}

	// Now verify that this is indeed a FAT partition
	if (memcmp(sectorBuffer + BPB_FAT16_fileSysType, FAT_SIG, sizeof(FAT_SIG)) &&
		memcmp(sectorBuffer + BPB_FAT32_fileSysType, FAT_SIG, sizeof(FAT_SIG)))
	{
		return NULL;
	}

	partition = (PARTITION*) _FAT_mem_allocate (sizeof(PARTITION));
	if (partition == NULL) {
		return NULL;
	}

	// Init the partition lock
	_FAT_lock_init(&partition->lock);

	if (!memcmp(sectorBuffer + BPB_FAT16_fileSysType, FAT_SIG, sizeof(FAT_SIG)))
		strncpy(partition->label, (char*)(sectorBuffer + BPB_FAT16_volumeLabel), 11);
	else
		strncpy(partition->label, (char*)(sectorBuffer + BPB_FAT32_volumeLabel), 11);
	partition->label[11] = '\0';

	// Set partition's disc interface
	partition->disc = disc;

	// Store required information about the file system
	partition->fat.sectorsPerFat = u8array_to_u16(sectorBuffer, BPB_sectorsPerFAT);
	if (partition->fat.sectorsPerFat == 0) {
		partition->fat.sectorsPerFat = u8array_to_u32( sectorBuffer, BPB_FAT32_sectorsPerFAT32);
	}

	partition->numberOfSectors = u8array_to_u16( sectorBuffer, BPB_numSectorsSmall);
	if (partition->numberOfSectors == 0) {
		partition->numberOfSectors = u8array_to_u32( sectorBuffer, BPB_numSectors);
	}

	partition->bytesPerSector = u8array_to_u16(sectorBuffer, BPB_bytesPerSector);
	if(partition->bytesPerSector < MIN_SECTOR_SIZE || partition->bytesPerSector > MAX_SECTOR_SIZE) {
		// Unsupported sector size
		_FAT_mem_free(partition);
		return NULL;
	}

	partition->sectorsPerCluster = sectorBuffer[BPB_sectorsPerCluster];
	partition->bytesPerCluster = partition->bytesPerSector * partition->sectorsPerCluster;
	partition->fat.fatStart = startSector + u8array_to_u16(sectorBuffer, BPB_reservedSectors);

	partition->rootDirStart = partition->fat.fatStart + (sectorBuffer[BPB_numFATs] * partition->fat.sectorsPerFat);
	partition->dataStart = partition->rootDirStart +
		(( u8array_to_u16(sectorBuffer, BPB_rootEntries) * DIR_ENTRY_DATA_SIZE) / partition->bytesPerSector);

	partition->totalSize = ((uint64_t)partition->numberOfSectors - (partition->dataStart - startSector)) * (uint64_t)partition->bytesPerSector;

	//FS info sector
	partition->fsInfoSector = startSector + (u8array_to_u16(sectorBuffer, BPB_FAT32_fsInfo) ? u8array_to_u16(sectorBuffer, BPB_FAT32_fsInfo) : 1);

	// Store info about FAT
	uint32_t clusterCount = (partition->numberOfSectors - (uint32_t)(partition->dataStart - startSector)) / partition->sectorsPerCluster;
	partition->fat.lastCluster = clusterCount + CLUSTER_FIRST - 1;
	partition->fat.firstFree = CLUSTER_FIRST;
	partition->fat.numberFreeCluster = 0;
	partition->fat.numberLastAllocCluster = 0;

	if (clusterCount < CLUSTERS_PER_FAT12) {
		partition->filesysType = FS_FAT12;	// FAT12 volume
	} else if (clusterCount < CLUSTERS_PER_FAT16) {
		partition->filesysType = FS_FAT16;	// FAT16 volume
	} else {
		partition->filesysType = FS_FAT32;	// FAT32 volume
	}

	if (partition->filesysType != FS_FAT32) {
		partition->rootDirCluster = FAT16_ROOT_DIR_CLUSTER;
	} else {
		// Set up for the FAT32 way
		partition->rootDirCluster = u8array_to_u32(sectorBuffer, BPB_FAT32_rootClus);
		// Check if FAT mirroring is enabled
		if (!(sectorBuffer[BPB_FAT32_extFlags] & 0x80)) {
			// Use the active FAT
			partition->fat.fatStart = partition->fat.fatStart + ( partition->fat.sectorsPerFat * (sectorBuffer[BPB_FAT32_extFlags] & 0x0F));
		}
	}

	// Create a cache to use
	partition->cache = _FAT_cache_constructor (cacheSize, sectorsPerPage, partition->disc, startSector+partition->numberOfSectors, partition->bytesPerSector);

	// Set current directory to the root
	partition->cwdCluster = partition->rootDirCluster;

	// Check if this disc is writable, and set the readOnly property appropriately
	partition->readOnly = !(_FAT_disc_features(disc) & FEATURE_MEDIUM_CANWRITE);

	// There are currently no open files on this partition
	partition->openFileCount = 0;
	partition->firstOpenFile = NULL;

	_FAT_partition_readFSinfo(partition);

	return partition;
}

PARTITION* _FAT_partition_constructor (const DISC_INTERFACE* disc, uint32_t cacheSize, uint32_t sectorsPerPage, sec_t startSector)
{
	uint8_t *sectorBuffer = (uint8_t*) _FAT_mem_align(MAX_SECTOR_SIZE);
	if (!sectorBuffer) return NULL;
	PARTITION *ret = _FAT_partition_constructor_buf(disc, cacheSize,
			sectorsPerPage, startSector, sectorBuffer);
	_FAT_mem_free(sectorBuffer);
	return ret;
}

void _FAT_partition_destructor (PARTITION* partition) {
	FILE_STRUCT* nextFile;

	_FAT_lock(&partition->lock);

	// Synchronize open files
	nextFile = partition->firstOpenFile;
	while (nextFile) {
		_FAT_syncToDisc (nextFile);
		nextFile = nextFile->nextOpenFile;
	}

	// Write out the fs info sector
	_FAT_partition_writeFSinfo(partition);

	// Free memory used by the cache, writing it to disc at the same time
	_FAT_cache_destructor (partition->cache);

	// Unlock the partition and destroy the lock
	_FAT_unlock(&partition->lock);
	_FAT_lock_deinit(&partition->lock);

	// Free memory used by the partition
	_FAT_mem_free (partition);
}

PARTITION* _FAT_partition_getPartitionFromPath (const char* path) {
	const devoptab_t *devops;

	devops = GetDeviceOpTab (path);

	if (!devops) {
		return NULL;
	}

	return (PARTITION*)devops->deviceData;
}

static void _FAT_updateFS_INFO(PARTITION * partition, uint8_t *sectorBuffer) {
	partition->fat.numberFreeCluster = _FAT_fat_freeClusterCount(partition);
	u32_to_u8array(sectorBuffer, FSIB_numberOfFreeCluster, partition->fat.numberFreeCluster);
	u32_to_u8array(sectorBuffer, FSIB_numberLastAllocCluster, partition->fat.numberLastAllocCluster);
	_FAT_disc_writeSectors (partition->disc, partition->fsInfoSector, 1, sectorBuffer);
}

void _FAT_partition_createFSinfo(PARTITION * partition)
{
	if(partition->readOnly || partition->filesysType != FS_FAT32)
		return;

	uint8_t *sectorBuffer = (uint8_t*) _FAT_mem_align(partition->bytesPerSector);
	if (!sectorBuffer) return;
	memset(sectorBuffer, 0, partition->bytesPerSector);

	int i;
	for(i = 0; i < 4; ++i)
	{
		sectorBuffer[FSIB_SIG1+i] = FS_INFO_SIG1[i];
		sectorBuffer[FSIB_SIG2+i] = FS_INFO_SIG2[i];
	}

	sectorBuffer[FSIB_bootSig_55] = 0x55;
	sectorBuffer[FSIB_bootSig_AA] = 0xAA;

	_FAT_updateFS_INFO(partition,sectorBuffer);

	_FAT_mem_free(sectorBuffer);
}

void _FAT_partition_readFSinfo(PARTITION * partition)
{
	if(partition->filesysType != FS_FAT32)
		return;

	uint8_t *sectorBuffer = (uint8_t*) _FAT_mem_align(partition->bytesPerSector);
	if (!sectorBuffer) return;
	memset(sectorBuffer, 0, partition->bytesPerSector);
	// Read first sector of disc
	if (!_FAT_disc_readSectors (partition->disc, partition->fsInfoSector, 1, sectorBuffer)) {
		_FAT_mem_free(sectorBuffer);
		return;
	}

	if(memcmp(sectorBuffer+FSIB_SIG1, FS_INFO_SIG1, 4) != 0 ||
		memcmp(sectorBuffer+FSIB_SIG2, FS_INFO_SIG2, 4) != 0 ||
		u8array_to_u32(sectorBuffer, FSIB_numberOfFreeCluster) == 0)
	{
		//sector does not yet exist, create one!
		_FAT_partition_createFSinfo(partition);
	} else {
		partition->fat.numberFreeCluster = u8array_to_u32(sectorBuffer, FSIB_numberOfFreeCluster);
		if(partition->fat.numberFreeCluster == 0xffffffff) {
			_FAT_updateFS_INFO(partition,sectorBuffer);
			partition->fat.numberFreeCluster = u8array_to_u32(sectorBuffer, FSIB_numberOfFreeCluster);
		}
		partition->fat.numberLastAllocCluster = u8array_to_u32(sectorBuffer, FSIB_numberLastAllocCluster);
	}
	_FAT_mem_free(sectorBuffer);
}

void _FAT_partition_writeFSinfo(PARTITION * partition)
{
	if(partition->filesysType != FS_FAT32)
		return;

	uint8_t *sectorBuffer = (uint8_t*) _FAT_mem_align(partition->bytesPerSector);
	if (!sectorBuffer) return;
	memset(sectorBuffer, 0, partition->bytesPerSector);
	// Read first sector of disc
	if (!_FAT_disc_readSectors (partition->disc, partition->fsInfoSector, 1, sectorBuffer)) {
		_FAT_mem_free(sectorBuffer);
		return;
	}

	if(memcmp(sectorBuffer+FSIB_SIG1, FS_INFO_SIG1, 4) || memcmp(sectorBuffer+FSIB_SIG2, FS_INFO_SIG2, 4)) {
		_FAT_mem_free(sectorBuffer);
		return;
	}

	u32_to_u8array(sectorBuffer, FSIB_numberOfFreeCluster, partition->fat.numberFreeCluster);
	u32_to_u8array(sectorBuffer, FSIB_numberLastAllocCluster, partition->fat.numberLastAllocCluster);

	// Write first sector of disc
	_FAT_disc_writeSectors (partition->disc, partition->fsInfoSector, 1, sectorBuffer);
	_FAT_mem_free(sectorBuffer);
}

uint32_t* _FAT_getCwdClusterPtr(const char* name) {
	PARTITION *partition = _FAT_partition_getPartitionFromPath(name);

	if (!partition) {
		return NULL;
	}

	return &partition->cwdCluster;
}

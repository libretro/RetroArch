/*
 fatfile.c

 Functions used by the newlib disc stubs to interface with
 this library

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

 2009-10-23 oggzee: fixes for cluster aligned file size (write, truncate, seek)
*/

#include "fatfile.h"

#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

#include "cache.h"
#include "file_allocation_table.h"
#include "bit_ops.h"
#include "filetime.h"
#include "lock.h"

bool _FAT_findEntry(const char *path, DIR_ENTRY *dirEntry) {
	PARTITION *partition = _FAT_partition_getPartitionFromPath(path);

	// Check Partition
	if( !partition )
		return false;

	// Move the path pointer to the start of the actual path
	if (strchr (path, ':') != NULL) {
		path = strchr (path, ':') + 1;
	}
	if (strchr (path, ':') != NULL) {
		return false;
	}

	// Search for the file on the disc
	return _FAT_directory_entryFromPath (partition, dirEntry, path, NULL);

}

int	FAT_getAttr(const char *file) {
	DIR_ENTRY dirEntry;
	if (!_FAT_findEntry(file,&dirEntry)) return -1;

	return dirEntry.entryData[DIR_ENTRY_attributes];
}

int FAT_setAttr(const char *file, uint8_t attr) {

	// Defines...
	DIR_ENTRY_POSITION entryEnd;
	PARTITION *partition = NULL;
	DIR_ENTRY dirEntry;

	// Get Partition
	partition = _FAT_partition_getPartitionFromPath( file );

	// Check Partition
	if( !partition )
		return -1;

	// Move the path pointer to the start of the actual path
	if (strchr (file, ':') != NULL)
		file = strchr (file, ':') + 1;
	if (strchr (file, ':') != NULL)
		return -1;

	// Get DIR_ENTRY
	if( !_FAT_directory_entryFromPath (partition, &dirEntry, file, NULL) )
		return -1;

	// Get Entry-End
	entryEnd = dirEntry.dataEnd;

	// Lock Partition
	_FAT_lock(&partition->lock);

	// Write Data
	_FAT_cache_writePartialSector (
		partition->cache // Cache to write
		, &attr // Value to be written
		, _FAT_fat_clusterToSector( partition , entryEnd.cluster ) + entryEnd.sector // cluster
		, entryEnd.offset * DIR_ENTRY_DATA_SIZE + DIR_ENTRY_attributes // offset
		, 1 // Size in bytes
	);

	// Flush any sectors in the disc cache
	if ( !_FAT_cache_flush( partition->cache ) ) {
		_FAT_unlock(&partition->lock); // Unlock Partition
		return -1;
	}

	// Unlock Partition
	_FAT_unlock(&partition->lock);

	return 0;
}

int _FAT_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode) {
	PARTITION* partition = NULL;
	bool fileExists;
	DIR_ENTRY dirEntry;
	const char* pathEnd;
	uint32_t dirCluster;
	FILE_STRUCT* file = (FILE_STRUCT*) fileStruct;
	partition = _FAT_partition_getPartitionFromPath (path);

	if (partition == NULL) {
		r->_errno = ENODEV;
		return -1;
	}

	// Move the path pointer to the start of the actual path
	if (strchr (path, ':') != NULL) {
		path = strchr (path, ':') + 1;
	}
	if (strchr (path, ':') != NULL) {
		r->_errno = EINVAL;
		return -1;
	}

	// Determine which mode the file is openned for
	if ((flags & 0x03) == O_RDONLY) {
		// Open the file for read-only access
		file->read = true;
		file->write = false;
		file->append = false;
	} else if ((flags & 0x03) == O_WRONLY) {
		// Open file for write only access
		file->read = false;
		file->write = true;
		file->append = false;
	} else if ((flags & 0x03) == O_RDWR) {
		// Open file for read/write access
		file->read = true;
		file->write = true;
		file->append = false;
	} else {
		r->_errno = EACCES;
		return -1;
	}

	// Make sure we aren't trying to write to a read-only disc
	if (file->write && partition->readOnly) {
		r->_errno = EROFS;
		return -1;
	}

	// Search for the file on the disc
	_FAT_lock(&partition->lock);
	fileExists = _FAT_directory_entryFromPath (partition, &dirEntry, path, NULL);

	// The file shouldn't exist if we are trying to create it
	if ((flags & O_CREAT) && (flags & O_EXCL) && fileExists) {
		_FAT_unlock(&partition->lock);
		r->_errno = EEXIST;
		return -1;
	}

	// It should not be a directory if we're openning a file,
	if (fileExists && _FAT_directory_isDirectory(&dirEntry)) {
		_FAT_unlock(&partition->lock);
		r->_errno = EISDIR;
		return -1;
	}

	// We haven't modified the file yet
	file->modified = false;

	// If the file doesn't exist, create it if we're allowed to
	if (!fileExists) {
		if (flags & O_CREAT) {
			if (partition->readOnly) {
				// We can't write to a read-only partition
				_FAT_unlock(&partition->lock);
				r->_errno = EROFS;
				return -1;
			}
			// Create the file
			// Get the directory it has to go in
			pathEnd = strrchr (path, DIR_SEPARATOR);
			if (pathEnd == NULL) {
				// No path was specified
				dirCluster = partition->cwdCluster;
				pathEnd = path;
			} else {
				// Path was specified -- get the right dirCluster
				// Recycling dirEntry, since it needs to be recreated anyway
				if (!_FAT_directory_entryFromPath (partition, &dirEntry, path, pathEnd) ||
					!_FAT_directory_isDirectory(&dirEntry)) {
					_FAT_unlock(&partition->lock);
					r->_errno = ENOTDIR;
					return -1;
				}
				dirCluster = _FAT_directory_entryGetCluster (partition, dirEntry.entryData);
				// Move the pathEnd past the last DIR_SEPARATOR
				pathEnd += 1;
			}
			// Create the entry data
			strncpy (dirEntry.filename, pathEnd, NAME_MAX - 1);
			memset (dirEntry.entryData, 0, DIR_ENTRY_DATA_SIZE);

			// Set the creation time and date
			dirEntry.entryData[DIR_ENTRY_cTime_ms] = 0;
			u16_to_u8array (dirEntry.entryData, DIR_ENTRY_cTime, _FAT_filetime_getTimeFromRTC());
			u16_to_u8array (dirEntry.entryData, DIR_ENTRY_cDate, _FAT_filetime_getDateFromRTC());

			if (!_FAT_directory_addEntry (partition, &dirEntry, dirCluster)) {
				_FAT_unlock(&partition->lock);
				r->_errno = ENOSPC;
				return -1;
			}

			// File entry is modified
			file->modified = true;
		} else {
			// file doesn't exist, and we aren't creating it
			_FAT_unlock(&partition->lock);
			r->_errno = ENOENT;
			return -1;
		}
	}

	file->filesize = u8array_to_u32 (dirEntry.entryData, DIR_ENTRY_fileSize);

	/* Allow LARGEFILEs with undefined results
	// Make sure that the file size can fit in the available space
	if (!(flags & O_LARGEFILE) && (file->filesize >= (1<<31))) {
		r->_errno = EFBIG;
		return -1;
	}
	*/

	// Make sure we aren't trying to write to a read-only file
	if (file->write && !_FAT_directory_isWritable(&dirEntry)) {
		_FAT_unlock(&partition->lock);
		r->_errno = EROFS;
		return -1;
	}

	// Associate this file with a particular partition
	file->partition = partition;

	file->startCluster = _FAT_directory_entryGetCluster (partition, dirEntry.entryData);

	// Truncate the file if requested
	if ((flags & O_TRUNC) && file->write && (file->startCluster != 0)) {
		_FAT_fat_clearLinks (partition, file->startCluster);
		file->startCluster = CLUSTER_FREE;
		file->filesize = 0;
		// File is modified since we just cut it all off
		file->modified = true;
	}

	// Remember the position of this file's directory entry
	file->dirEntryStart = dirEntry.dataStart;		// Points to the start of the LFN entries of a file, or the alias for no LFN
	file->dirEntryEnd = dirEntry.dataEnd;

	// Reset read/write pointer
	file->currentPosition = 0;
	file->rwPosition.cluster = file->startCluster;
	file->rwPosition.sector =  0;
	file->rwPosition.byte = 0;

	if (flags & O_APPEND) {
		file->append = true;

		// Set append pointer to the end of the file
		file->appendPosition.cluster = _FAT_fat_lastCluster (partition, file->startCluster);
		file->appendPosition.sector = (file->filesize % partition->bytesPerCluster) / partition->bytesPerSector;
		file->appendPosition.byte = file->filesize % partition->bytesPerSector;

		// Check if the end of the file is on the end of a cluster
		if ( (file->filesize > 0) && ((file->filesize % partition->bytesPerCluster)==0) ){
			// Set flag to allocate a new cluster
			file->appendPosition.sector = partition->sectorsPerCluster;
			file->appendPosition.byte = 0;
		}
	} else {
		file->append = false;
		// Use something sane for the append pointer, so the whole file struct contains known values
		file->appendPosition = file->rwPosition;
	}

	file->inUse = true;

	// Insert this file into the double-linked list of open files
	partition->openFileCount += 1;
	if (partition->firstOpenFile) {
		file->nextOpenFile = partition->firstOpenFile;
		partition->firstOpenFile->prevOpenFile = file;
	} else {
		file->nextOpenFile = NULL;
	}
	file->prevOpenFile = NULL;
	partition->firstOpenFile = file;

	_FAT_unlock(&partition->lock);

	return (int) file;
}

/*
Synchronizes the file data to disc.
Does no locking of its own -- lock the partition before calling.
Returns 0 on success, an error code on failure.
*/
int _FAT_syncToDisc (FILE_STRUCT* file) {
	uint8_t dirEntryData[DIR_ENTRY_DATA_SIZE];

	if (!file || !file->inUse) {
		return EBADF;
	}

	if (file->write && file->modified) {
		// Load the old entry
		_FAT_cache_readPartialSector (file->partition->cache, dirEntryData,
			_FAT_fat_clusterToSector(file->partition, file->dirEntryEnd.cluster) + file->dirEntryEnd.sector,
			file->dirEntryEnd.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);

		// Write new data to the directory entry
		// File size
		u32_to_u8array (dirEntryData, DIR_ENTRY_fileSize, file->filesize);

		// Start cluster
		u16_to_u8array (dirEntryData, DIR_ENTRY_cluster, file->startCluster);
		u16_to_u8array (dirEntryData, DIR_ENTRY_clusterHigh, file->startCluster >> 16);

		// Modification time and date
		u16_to_u8array (dirEntryData, DIR_ENTRY_mTime, _FAT_filetime_getTimeFromRTC());
		u16_to_u8array (dirEntryData, DIR_ENTRY_mDate, _FAT_filetime_getDateFromRTC());

		// Access date
		u16_to_u8array (dirEntryData, DIR_ENTRY_aDate, _FAT_filetime_getDateFromRTC());

		// Set archive attribute
		dirEntryData[DIR_ENTRY_attributes] |= ATTRIB_ARCH;

		// Write the new entry
		_FAT_cache_writePartialSector (file->partition->cache, dirEntryData,
			_FAT_fat_clusterToSector(file->partition, file->dirEntryEnd.cluster) + file->dirEntryEnd.sector,
			file->dirEntryEnd.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);

		// Flush any sectors in the disc cache
		if (!_FAT_cache_flush(file->partition->cache)) {
			return EIO;
		}
	}

	file->modified = false;

	return 0;
}

int _FAT_close_r (struct _reent *r, void *fd) {
	FILE_STRUCT* file = (FILE_STRUCT*)  fd;
	int ret = 0;

	if (!file->inUse) {
		r->_errno = EBADF;
		return -1;
	}

	_FAT_lock(&file->partition->lock);

	if (file->write) {
		ret = _FAT_syncToDisc (file);
		if (ret != 0) {
			r->_errno = ret;
			ret = -1;
		}
	}

	file->inUse = false;

	// Remove this file from the double-linked list of open files
	file->partition->openFileCount -= 1;
	if (file->nextOpenFile) {
		file->nextOpenFile->prevOpenFile = file->prevOpenFile;
	}
	if (file->prevOpenFile) {
		file->prevOpenFile->nextOpenFile = file->nextOpenFile;
	} else {
		file->partition->firstOpenFile = file->nextOpenFile;
	}

	_FAT_unlock(&file->partition->lock);

	return ret;
}

ssize_t _FAT_read_r (struct _reent *r, void *fd, char *ptr, size_t len) {
	FILE_STRUCT* file = (FILE_STRUCT*)  fd;
	PARTITION* partition;
	CACHE* cache;
	FILE_POSITION position;
	uint32_t tempNextCluster;
	unsigned int tempVar;
	size_t remain;
	bool flagNoError = true;

	// Short circuit cases where len is 0 (or less)
	if (len <= 0) {
		return 0;
	}

	// Make sure we can actually read from the file
	if ((file == NULL) || !file->inUse || !file->read) {
		r->_errno = EBADF;
		return -1;
	}

	partition = file->partition;
	_FAT_lock(&partition->lock);

	// Don't try to read if the read pointer is past the end of file
	if (file->currentPosition >= file->filesize || file->startCluster == CLUSTER_FREE) {
		r->_errno = EOVERFLOW;
		_FAT_unlock(&partition->lock);
		return 0;
	}

	// Don't read past end of file
	if (len + file->currentPosition > file->filesize) {
		r->_errno = EOVERFLOW;
		len = file->filesize - file->currentPosition;
	}

	remain = len;
	position = file->rwPosition;
	cache = file->partition->cache;

	// Align to sector
	tempVar = partition->bytesPerSector - position.byte;
	if (tempVar > remain) {
		tempVar = remain;
	}

	if ((tempVar < partition->bytesPerSector) && flagNoError)
	{
		_FAT_cache_readPartialSector ( cache, ptr, _FAT_fat_clusterToSector (partition, position.cluster) + position.sector,
			position.byte, tempVar);

		remain -= tempVar;
		ptr += tempVar;

		position.byte += tempVar;
		if (position.byte >= partition->bytesPerSector) {
			position.byte = 0;
			position.sector++;
		}
	}

	// align to cluster
	// tempVar is number of sectors to read
	if (remain > (partition->sectorsPerCluster - position.sector) * partition->bytesPerSector) {
		tempVar = partition->sectorsPerCluster - position.sector;
	} else {
		tempVar = remain / partition->bytesPerSector;
	}

	if ((tempVar > 0) && flagNoError) {
		if (! _FAT_cache_readSectors (cache, _FAT_fat_clusterToSector (partition, position.cluster) + position.sector,
			tempVar, ptr))
		{
			flagNoError = false;
			r->_errno = EIO;
		} else {
			ptr += tempVar * partition->bytesPerSector;
			remain -= tempVar * partition->bytesPerSector;
			position.sector += tempVar;
		}
	}

	// Move onto next cluster
	// It should get to here without reading anything if a cluster is due to be allocated
	if ((position.sector >= partition->sectorsPerCluster) && flagNoError) {
		tempNextCluster = _FAT_fat_nextCluster(partition, position.cluster);
		if ((remain == 0) && (tempNextCluster == CLUSTER_EOF)) {
			position.sector = partition->sectorsPerCluster;
		} else if (!_FAT_fat_isValidCluster(partition, tempNextCluster)) {
			r->_errno = EIO;
			flagNoError = false;
		} else {
			position.sector = 0;
			position.cluster = tempNextCluster;
		}
	}

	// Read in whole clusters, contiguous blocks at a time
	while ((remain >= partition->bytesPerCluster) && flagNoError) {
		uint32_t chunkEnd;
		uint32_t nextChunkStart = position.cluster;
		size_t chunkSize = 0;

		do {
			chunkEnd = nextChunkStart;
			nextChunkStart = _FAT_fat_nextCluster (partition, chunkEnd);
			chunkSize += partition->bytesPerCluster;
		} while ((nextChunkStart == chunkEnd + 1) &&
#ifdef LIMIT_SECTORS
		 	(chunkSize + partition->bytesPerCluster <= LIMIT_SECTORS * partition->bytesPerSector) &&
#endif
			(chunkSize + partition->bytesPerCluster <= remain));

		if (!_FAT_cache_readSectors (cache, _FAT_fat_clusterToSector (partition, position.cluster),
				chunkSize / partition->bytesPerSector, ptr))
		{
			flagNoError = false;
			r->_errno = EIO;
			break;
		}
		ptr += chunkSize;
		remain -= chunkSize;

		// Advance to next cluster
		if ((remain == 0) && (nextChunkStart == CLUSTER_EOF)) {
			position.sector = partition->sectorsPerCluster;
			position.cluster = chunkEnd;
		} else if (!_FAT_fat_isValidCluster(partition, nextChunkStart)) {
			r->_errno = EIO;
			flagNoError = false;
		} else {
			position.sector = 0;
			position.cluster = nextChunkStart;
		}
	}

	// Read remaining sectors
	tempVar = remain / partition->bytesPerSector; // Number of sectors left
	if ((tempVar > 0) && flagNoError) {
		if (!_FAT_cache_readSectors (cache, _FAT_fat_clusterToSector (partition, position.cluster),
			tempVar, ptr))
		{
			flagNoError = false;
			r->_errno = EIO;
		} else {
			ptr += tempVar * partition->bytesPerSector;
			remain -= tempVar * partition->bytesPerSector;
			position.sector += tempVar;
		}
	}

	// Last remaining sector
	// Check if anything is left
	if ((remain > 0) && flagNoError) {
		_FAT_cache_readPartialSector ( cache, ptr,
			_FAT_fat_clusterToSector (partition, position.cluster) + position.sector, 0, remain);
		position.byte += remain;
		remain = 0;
	}

	// Length read is the wanted length minus the stuff not read
	len = len - remain;

	// Update file information
	file->rwPosition = position;
	file->currentPosition += len;

	_FAT_unlock(&partition->lock);
	return len;
}

// if current position is on the cluster border and more data has to be written
// then get next cluster or allocate next cluster
// this solves the over-allocation problems when file size is aligned to cluster size
// return true on succes, false on error
static bool _FAT_check_position_for_next_cluster(struct _reent *r,
		FILE_POSITION *position, PARTITION* partition, size_t remain, bool *flagNoError)
{
	uint32_t tempNextCluster;
	// do nothing if no more data to write
	if (remain == 0) return true;
	if (flagNoError && *flagNoError == false) return false;
	if (position->sector > partition->sectorsPerCluster) {
		// invalid arguments - internal error
		r->_errno = EINVAL;
		goto err;
	}
	if (position->sector == partition->sectorsPerCluster) {
		// need to advance to next cluster
		tempNextCluster = _FAT_fat_nextCluster(partition, position->cluster);
		if ((tempNextCluster == CLUSTER_EOF) || (tempNextCluster == CLUSTER_FREE)) {
			// Ran out of clusters so get a new one
			tempNextCluster = _FAT_fat_linkFreeCluster(partition, position->cluster);
		}
		if (!_FAT_fat_isValidCluster(partition, tempNextCluster)) {
			// Couldn't get a cluster, so abort
			r->_errno = ENOSPC;
			goto err;
		}
		position->sector = 0;
		position->cluster = tempNextCluster;
	}
	return true;
err:
	if (flagNoError) *flagNoError = false;
	return false;
}

/*
Extend a file so that the size is the same as the rwPosition
*/
static bool _FAT_file_extend_r (struct _reent *r, FILE_STRUCT* file) {
	PARTITION* partition = file->partition;
	CACHE* cache = file->partition->cache;
	FILE_POSITION position;
	uint8_t zeroBuffer [partition->bytesPerSector];
	memset(zeroBuffer, 0, partition->bytesPerSector);
	uint32_t remain;
	uint32_t tempNextCluster;
	unsigned int sector;

	position.byte = file->filesize % partition->bytesPerSector;
	position.sector = (file->filesize % partition->bytesPerCluster) / partition->bytesPerSector;
	// It is assumed that there is always a startCluster
	// This will be true when _FAT_file_extend_r is called from _FAT_write_r
	position.cluster = _FAT_fat_lastCluster (partition, file->startCluster);

	remain = file->currentPosition - file->filesize;

	if ((remain > 0) && (file->filesize > 0) && (position.sector == 0) && (position.byte  == 0)) {
		// Get a new cluster on the edge of a cluster boundary
		tempNextCluster = _FAT_fat_linkFreeCluster(partition, position.cluster);
		if (!_FAT_fat_isValidCluster(partition, tempNextCluster)) {
			// Couldn't get a cluster, so abort
			r->_errno = ENOSPC;
			return false;
		}
		position.cluster = tempNextCluster;
		position.sector = 0;
	}

	if (remain + position.byte < partition->bytesPerSector) {
		// Only need to clear to the end of the sector
		_FAT_cache_writePartialSector (cache, zeroBuffer,
			_FAT_fat_clusterToSector (partition, position.cluster) + position.sector, position.byte, remain);
		position.byte += remain;
	} else {
		if (position.byte > 0) {
			_FAT_cache_writePartialSector (cache, zeroBuffer,
				_FAT_fat_clusterToSector (partition, position.cluster) + position.sector, position.byte,
				partition->bytesPerSector - position.byte);
			remain -= (partition->bytesPerSector - position.byte);
			position.byte = 0;
			position.sector ++;
		}

		while (remain >= partition->bytesPerSector) {
			if (position.sector >= partition->sectorsPerCluster) {
				position.sector = 0;
				// Ran out of clusters so get a new one
				tempNextCluster = _FAT_fat_linkFreeCluster(partition, position.cluster);
				if (!_FAT_fat_isValidCluster(partition, tempNextCluster)) {
					// Couldn't get a cluster, so abort
					r->_errno = ENOSPC;
					return false;
				}
				position.cluster = tempNextCluster;
			}

			sector = _FAT_fat_clusterToSector (partition, position.cluster) + position.sector;
			_FAT_cache_writeSectors (cache, sector, 1, zeroBuffer);

			remain -= partition->bytesPerSector;
			position.sector ++;
		}

		if (!_FAT_check_position_for_next_cluster(r, &position, partition, remain, NULL)) {
			// error already marked
			return false;
		}

		if (remain > 0) {
			_FAT_cache_writePartialSector (cache, zeroBuffer,
				_FAT_fat_clusterToSector (partition, position.cluster) + position.sector, 0, remain);
			position.byte = remain;
		}
	}

	file->rwPosition = position;
	file->filesize = file->currentPosition;
	return true;
}

ssize_t _FAT_write_r (struct _reent *r, void *fd, const char *ptr, size_t len) {
	FILE_STRUCT* file = (FILE_STRUCT*)  fd;
	PARTITION* partition;
	CACHE* cache;
	FILE_POSITION position;
	uint32_t tempNextCluster;
	unsigned int tempVar;
	size_t remain;
	bool flagNoError = true;
	bool flagAppending = false;

	// Make sure we can actually write to the file
	if ((file == NULL) || !file->inUse || !file->write) {
		r->_errno = EBADF;
		return -1;
	}

	partition = file->partition;
	cache = file->partition->cache;
	_FAT_lock(&partition->lock);

	// Only write up to the maximum file size, taking into account wrap-around of ints
	if (len + file->filesize > FILE_MAX_SIZE || len + file->filesize < file->filesize) {
		len = FILE_MAX_SIZE - file->filesize;
	}

	// Short circuit cases where len is 0 (or less)
	if (len <= 0) {
		_FAT_unlock(&partition->lock);
		return 0;
	}

	remain = len;

	// Get a new cluster for the start of the file if required
	if (file->startCluster == CLUSTER_FREE) {
		tempNextCluster = _FAT_fat_linkFreeCluster (partition, CLUSTER_FREE);
		if (!_FAT_fat_isValidCluster(partition, tempNextCluster)) {
			// Couldn't get a cluster, so abort immediately
			_FAT_unlock(&partition->lock);
			r->_errno = ENOSPC;
			return -1;
		}
		file->startCluster = tempNextCluster;

		// Appending starts at the begining for a 0 byte file
		file->appendPosition.cluster = file->startCluster;
		file->appendPosition.sector = 0;
		file->appendPosition.byte = 0;

		file->rwPosition.cluster = file->startCluster;
		file->rwPosition.sector =  0;
		file->rwPosition.byte = 0;
	}

	if (file->append) {
		position = file->appendPosition;
		flagAppending = true;
	} else {
		// If the write pointer is past the end of the file, extend the file to that size
		if (file->currentPosition > file->filesize) {
			if (!_FAT_file_extend_r (r, file)) {
				_FAT_unlock(&partition->lock);
				return -1;
			}
		}

		// Write at current read pointer
		position = file->rwPosition;

		// If it is writing past the current end of file, set appending flag
		if (len + file->currentPosition > file->filesize) {
			flagAppending = true;
		}
	}

	// Move onto next cluster if needed
	_FAT_check_position_for_next_cluster(r, &position, partition, remain, &flagNoError);

	// Align to sector
	tempVar = partition->bytesPerSector - position.byte;
	if (tempVar > remain) {
		tempVar = remain;
	}

	if ((tempVar < partition->bytesPerSector) && flagNoError) {
		// Write partial sector to disk
		_FAT_cache_writePartialSector (cache, ptr,
			_FAT_fat_clusterToSector (partition, position.cluster) + position.sector, position.byte, tempVar);

		remain -= tempVar;
		ptr += tempVar;
		position.byte += tempVar;

		// Move onto next sector
		if (position.byte >= partition->bytesPerSector) {
			position.byte = 0;
			position.sector ++;
		}
	}

	// Align to cluster
	// tempVar is number of sectors to write
	if (remain > (partition->sectorsPerCluster - position.sector) * partition->bytesPerSector) {
		tempVar = partition->sectorsPerCluster - position.sector;
	} else {
		tempVar = remain / partition->bytesPerSector;
	}

	if ((tempVar > 0 && tempVar < partition->sectorsPerCluster) && flagNoError) {
		if (!_FAT_cache_writeSectors (cache,
			_FAT_fat_clusterToSector (partition, position.cluster) + position.sector, tempVar, ptr))
		{
			flagNoError = false;
			r->_errno = EIO;
		} else {
			ptr += tempVar * partition->bytesPerSector;
			remain -= tempVar * partition->bytesPerSector;
			position.sector += tempVar;
		}
	}

	// Write whole clusters
	while ((remain >= partition->bytesPerCluster) && flagNoError) {
		// allocate next cluster
		_FAT_check_position_for_next_cluster(r, &position, partition, remain, &flagNoError);
		if (!flagNoError) break;
		// set indexes to the current position
		uint32_t chunkEnd = position.cluster;
		uint32_t nextChunkStart = position.cluster;
		size_t chunkSize = partition->bytesPerCluster;
		FILE_POSITION next_position = position;

		// group consecutive clusters
		while (flagNoError &&
#ifdef LIMIT_SECTORS
				(chunkSize + partition->bytesPerCluster <= LIMIT_SECTORS * partition->bytesPerSector) &&
#endif
				(chunkSize + partition->bytesPerCluster < remain))
		{
			// pretend to use up all sectors in next_position
			next_position.sector = partition->sectorsPerCluster;
			// get or allocate next cluster
			_FAT_check_position_for_next_cluster(r, &next_position, partition,
					remain - chunkSize, &flagNoError);
			if (!flagNoError) break; // exit loop on error
			nextChunkStart = next_position.cluster;
			if (nextChunkStart != chunkEnd + 1) break; // exit loop if not consecutive
			chunkEnd = nextChunkStart;
			chunkSize += partition->bytesPerCluster;
		}

		if ( !_FAT_cache_writeSectors (cache,
				_FAT_fat_clusterToSector(partition, position.cluster), chunkSize / partition->bytesPerSector, ptr))
		{
			flagNoError = false;
			r->_errno = EIO;
			break;
		}
		ptr += chunkSize;
		remain -= chunkSize;

		if ((chunkEnd != nextChunkStart) && _FAT_fat_isValidCluster(partition, nextChunkStart)) {
			// new cluster is already allocated (because it was not consecutive)
			position.cluster = nextChunkStart;
			position.sector = 0;
		} else {
			// Allocate a new cluster when next writing the file
			position.cluster = chunkEnd;
			position.sector = partition->sectorsPerCluster;
		}
	}

	// allocate next cluster if needed
	_FAT_check_position_for_next_cluster(r, &position, partition, remain, &flagNoError);

	// Write remaining sectors
	tempVar = remain / partition->bytesPerSector; // Number of sectors left
	if ((tempVar > 0) && flagNoError) {
		if (!_FAT_cache_writeSectors (cache, _FAT_fat_clusterToSector (partition, position.cluster), tempVar, ptr))
		{
			flagNoError = false;
			r->_errno = EIO;
		} else {
			ptr += tempVar * partition->bytesPerSector;
			remain -= tempVar * partition->bytesPerSector;
			position.sector += tempVar;
		}
	}

	// Last remaining sector
	if ((remain > 0) && flagNoError) {
		if (flagAppending) {
			_FAT_cache_eraseWritePartialSector ( cache, ptr,
				_FAT_fat_clusterToSector (partition, position.cluster) + position.sector, 0, remain);
		} else {
			_FAT_cache_writePartialSector ( cache, ptr,
				_FAT_fat_clusterToSector (partition, position.cluster) + position.sector, 0, remain);
		}
		position.byte += remain;
		remain = 0;
	}

	// Amount written is the originally requested amount minus stuff remaining
	len = len - remain;

	// Update file information
	file->modified = true;
	if (file->append) {
		// Appending doesn't affect the read pointer
		file->appendPosition = position;
		file->filesize += len;
	} else {
		// Writing also shifts the read pointer
		file->rwPosition = position;
		file->currentPosition += len;
		if (file->filesize < file->currentPosition) {
			file->filesize = file->currentPosition;
		}
	}
	_FAT_unlock(&partition->lock);

	return len;
}

off_t _FAT_seek_r (struct _reent *r, void *fd, off_t pos, int dir) {
	FILE_STRUCT* file = (FILE_STRUCT*)  fd;
	PARTITION* partition;
	uint32_t cluster, nextCluster;
	int clusCount;
	off_t newPosition;
	uint32_t position;

	if ((file == NULL) || (file->inUse == false))	 {
		// invalid file
		r->_errno = EBADF;
		return -1;
	}

	partition = file->partition;
	_FAT_lock(&partition->lock);

	switch (dir) {
		case SEEK_SET:
			newPosition = pos;
			break;
		case SEEK_CUR:
			newPosition = (off_t)file->currentPosition + pos;
			break;
		case SEEK_END:
			newPosition = (off_t)file->filesize + pos;
			break;
		default:
			_FAT_unlock(&partition->lock);
			r->_errno = EINVAL;
			return -1;
	}

	if ((pos > 0) && (newPosition < 0)) {
		_FAT_unlock(&partition->lock);
		r->_errno = EOVERFLOW;
		return -1;
	}

	// newPosition can only be larger than the FILE_MAX_SIZE on platforms where
	// off_t is larger than 32 bits.
	if (newPosition < 0 || ((sizeof(newPosition) > 4) && newPosition > (off_t)FILE_MAX_SIZE)) {
		_FAT_unlock(&partition->lock);
		r->_errno = EINVAL;
		return -1;
	}

	position = (uint32_t)newPosition;

	// Only change the read/write position if it is within the bounds of the current filesize,
	// or at the very edge of the file
	if (position <= file->filesize && file->startCluster != CLUSTER_FREE) {
		// Calculate where the correct cluster is
		// how many clusters from start of file
		clusCount = position / partition->bytesPerCluster;
		cluster = file->startCluster;
		if (position >= file->currentPosition) {
			// start from current cluster
			int currentCount = file->currentPosition / partition->bytesPerCluster;
			if (file->rwPosition.sector == partition->sectorsPerCluster) {
				currentCount--;
			}
			clusCount -= currentCount;
			cluster = file->rwPosition.cluster;
		}
		// Calculate the sector and byte of the current position,
		// and store them
		file->rwPosition.sector = (position % partition->bytesPerCluster) / partition->bytesPerSector;
		file->rwPosition.byte = position % partition->bytesPerSector;

		nextCluster = _FAT_fat_nextCluster (partition, cluster);
		while ((clusCount > 0) && (nextCluster != CLUSTER_FREE) && (nextCluster != CLUSTER_EOF)) {
			clusCount--;
			cluster = nextCluster;
			nextCluster = _FAT_fat_nextCluster (partition, cluster);
		}

		// Check if ran out of clusters and it needs to allocate a new one
		if (clusCount > 0) {
			if ((clusCount == 1) && (file->filesize == position) && (file->rwPosition.sector == 0)) {
				// Set flag to allocate a new cluster
				file->rwPosition.sector = partition->sectorsPerCluster;
				file->rwPosition.byte = 0;
			} else {
				_FAT_unlock(&partition->lock);
				r->_errno = EINVAL;
				return -1;
			}
		}

		file->rwPosition.cluster = cluster;
	}

	// Save position
	file->currentPosition = position;

	_FAT_unlock(&partition->lock);
	return position;
}

int _FAT_fstat_r (struct _reent *r, void *fd, struct stat *st) {
	FILE_STRUCT* file = (FILE_STRUCT*)  fd;
	PARTITION* partition;
	DIR_ENTRY fileEntry;

	if ((file == NULL) || (file->inUse == false))	 {
		// invalid file
		r->_errno = EBADF;
		return -1;
	}

	partition = file->partition;
	_FAT_lock(&partition->lock);

	// Get the file's entry data
	fileEntry.dataStart = file->dirEntryStart;
	fileEntry.dataEnd = file->dirEntryEnd;

	if (!_FAT_directory_entryFromPosition (partition, &fileEntry)) {
		_FAT_unlock(&partition->lock);
		r->_errno = EIO;
		return -1;
	}

	// Fill in the stat struct
	_FAT_directory_entryStat (partition, &fileEntry, st);

	// Fix stats that have changed since the file was openned
  	st->st_ino = (ino_t)(file->startCluster);		// The file serial number is the start cluster
	st->st_size = file->filesize;					// File size

	_FAT_unlock(&partition->lock);
	return 0;
}

int _FAT_ftruncate_r (struct _reent *r, void *fd, off_t len) {
	FILE_STRUCT* file = (FILE_STRUCT*)  fd;
	PARTITION* partition;
	int ret=0;
	uint32_t newSize = (uint32_t)len;

	if (len < 0) {
		// Trying to truncate to a negative size
		r->_errno = EINVAL;
		return -1;
	}

	if ((sizeof(len) > 4) && len > (off_t)FILE_MAX_SIZE) {
		// Trying to extend the file beyond what FAT supports
		r->_errno = EFBIG;
		return -1;
	}

	if (!file || !file->inUse) {
		// invalid file
		r->_errno = EBADF;
		return -1;
	}

	if (!file->write) {
		// Read-only file
		r->_errno = EINVAL;
		return -1;
	}

	partition = file->partition;
	_FAT_lock(&partition->lock);

	if (newSize > file->filesize) {
		// Expanding the file
		FILE_POSITION savedPosition;
		uint32_t savedOffset;
		// Get a new cluster for the start of the file if required
		if (file->startCluster == CLUSTER_FREE) {
			uint32_t tempNextCluster = _FAT_fat_linkFreeCluster (partition, CLUSTER_FREE);
			if (!_FAT_fat_isValidCluster(partition, tempNextCluster)) {
				// Couldn't get a cluster, so abort immediately
				_FAT_unlock(&partition->lock);
				r->_errno = ENOSPC;
				return -1;
			}
			file->startCluster = tempNextCluster;

			file->rwPosition.cluster = file->startCluster;
			file->rwPosition.sector =  0;
			file->rwPosition.byte = 0;
		}
		// Save the read/write pointer
		savedPosition = file->rwPosition;
		savedOffset = file->currentPosition;
		// Set the position to the new size
		file->currentPosition = newSize;
		// Extend the file to the new position
		if (!_FAT_file_extend_r (r, file)) {
			ret = -1;
		}
		// Set the append position to the new rwPointer
		if (file->append) {
			file->appendPosition = file->rwPosition;
		}
		// Restore the old rwPointer;
		file->rwPosition = savedPosition;
		file->currentPosition = savedOffset;
	} else if (newSize < file->filesize){
		// Shrinking the file
		if (len == 0) {
			// Cutting the file down to nothing, clear all clusters used
			_FAT_fat_clearLinks (partition, file->startCluster);
			file->startCluster = CLUSTER_FREE;

			file->appendPosition.cluster = CLUSTER_FREE;
			file->appendPosition.sector = 0;
			file->appendPosition.byte = 0;
		} else {
			// Trimming the file down to the required size
			unsigned int chainLength;
			uint32_t lastCluster;

			// Drop the unneeded end of the cluster chain.
			// If the end falls on a cluster boundary, drop that cluster too,
			// then set a flag to allocate a cluster as needed
			chainLength = ((newSize-1) / partition->bytesPerCluster) + 1;
			lastCluster = _FAT_fat_trimChain (partition, file->startCluster, chainLength);

			if (file->append) {
				file->appendPosition.byte = newSize % partition->bytesPerSector;
				// Does the end of the file fall on the edge of a cluster?
				if (newSize % partition->bytesPerCluster == 0) {
					// Set a flag to allocate a new cluster
					file->appendPosition.sector = partition->sectorsPerCluster;
				} else {
					file->appendPosition.sector = (newSize % partition->bytesPerCluster) / partition->bytesPerSector;
				}
				file->appendPosition.cluster = lastCluster;
			}
		}
	} else {
		// Truncating to same length, so don't do anything
	}

	file->filesize = newSize;
	file->modified = true;

	_FAT_unlock(&partition->lock);
	return ret;
}

int _FAT_fsync_r (struct _reent *r, void *fd) {
	FILE_STRUCT* file = (FILE_STRUCT*)  fd;
	int ret = 0;

	if (!file->inUse) {
		r->_errno = EBADF;
		return -1;
	}

	_FAT_lock(&file->partition->lock);

	ret = _FAT_syncToDisc (file);
	if (ret != 0) {
		r->_errno = ret;
		ret = -1;
	}

	_FAT_unlock(&file->partition->lock);

	return ret;
}

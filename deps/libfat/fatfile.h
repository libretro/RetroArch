/*
 fatfile.h
 
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
*/


#ifndef _FATFILE_H
#define _FATFILE_H

#include <sys/reent.h>
#include <sys/stat.h>

#include "common.h"
#include "partition.h"
#include "directory.h"

#define FILE_MAX_SIZE ((uint32_t)0xFFFFFFFF)	/* 4GiB - 1B */

typedef struct {
	u32   cluster;
	sec_t sector;
	s32   byte;
} FILE_POSITION;

struct _FILE_STRUCT;

struct _FILE_STRUCT {
	uint32_t             filesize;
	uint32_t             startCluster;
	uint32_t             currentPosition;
	FILE_POSITION        rwPosition;
	FILE_POSITION        appendPosition;
	DIR_ENTRY_POSITION   dirEntryStart;		/* Points to the start of the LFN entries of a file, or the alias for no LFN */
	DIR_ENTRY_POSITION   dirEntryEnd;		/* Always points to the file's alias entry */
	PARTITION*           partition;
	struct _FILE_STRUCT* prevOpenFile;		/* The previous entry in a double-linked list of open files */
	struct _FILE_STRUCT* nextOpenFile;		/* The next entry in a double-linked list of open files */
	bool                 read;
	bool                 write;
	bool                 append;
	bool                 inUse;
	bool                 modified;
};

typedef struct _FILE_STRUCT FILE_STRUCT;

int _FAT_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode);

int _FAT_close_r (struct _reent *r, void *fd);

ssize_t _FAT_write_r (struct _reent *r,void *fd, const char *ptr, size_t len);

ssize_t _FAT_read_r (struct _reent *r, void *fd, char *ptr, size_t len);

off_t _FAT_seek_r (struct _reent *r, void *fd, off_t pos, int dir);

int _FAT_fstat_r (struct _reent *r, void *fd, struct stat *st);

int _FAT_stat_r (struct _reent *r, const char *path, struct stat *st);

int _FAT_link_r (struct _reent *r, const char *existing, const char *newLink);

int _FAT_unlink_r (struct _reent *r, const char *name);

int _FAT_chdir_r (struct _reent *r, const char *name);

int _FAT_rename_r (struct _reent *r, const char *oldName, const char *newName);

int _FAT_ftruncate_r (struct _reent *r, void *fd, off_t len);

int _FAT_fsync_r (struct _reent *r, void *fd);

/*
Synchronizes the file data to disc.
Does no locking of its own -- lock the partition before calling.
Returns 0 on success, an error code on failure.
*/
extern int _FAT_syncToDisc (FILE_STRUCT* file);

#endif /* _FATFILE_H */

/*
 directory.h
 Reading, writing and manipulation of the directory structure on
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

#ifndef _DIRECTORY_H
#define _DIRECTORY_H

#include <sys/stat.h>
#include <sys/syslimits.h>

#include "common.h"
#include "partition.h"

#define DIR_ENTRY_DATA_SIZE 0x20
#define MAX_LFN_LENGTH	256
#define MAX_ALIAS_LENGTH 13
#define LFN_ENTRY_LENGTH 13
#define ALIAS_ENTRY_LENGTH 11
#define MAX_ALIAS_EXT_LENGTH 3
#define MAX_ALIAS_PRI_LENGTH 8
#define MAX_NUMERIC_TAIL 999999
#define FAT16_ROOT_DIR_CLUSTER 0

#define DIR_SEPARATOR '/'

// File attributes
#define ATTRIB_ARCH	0x20			// Archive
#define ATTRIB_DIR	0x10			// Directory
#define ATTRIB_LFN	0x0F			// Long file name
#define ATTRIB_VOL	0x08			// Volume
#define ATTRIB_SYS	0x04			// System
#define ATTRIB_HID	0x02			// Hidden
#define ATTRIB_RO	0x01			// Read only

#define CASE_LOWER_EXT  0x10		// WinNT lowercase extension
#define CASE_LOWER_BASE 0x08		// WinNT lowercase basename

typedef enum {FT_DIRECTORY, FT_FILE} FILE_TYPE;

typedef struct {
	uint32_t cluster;
	sec_t    sector;
	int32_t  offset;
} DIR_ENTRY_POSITION;

typedef struct {
	uint8_t            entryData[DIR_ENTRY_DATA_SIZE];
	DIR_ENTRY_POSITION dataStart;		// Points to the start of the LFN entries of a file, or the alias for no LFN
	DIR_ENTRY_POSITION dataEnd;			// Always points to the file/directory's alias entry
	char               filename[NAME_MAX];
} DIR_ENTRY;

// Directory entry offsets
enum DIR_ENTRY_offset {
	DIR_ENTRY_name = 0x00,
	DIR_ENTRY_extension = 0x08,
	DIR_ENTRY_attributes = 0x0B,
	DIR_ENTRY_caseInfo = 0x0C,
	DIR_ENTRY_cTime_ms = 0x0D,
	DIR_ENTRY_cTime = 0x0E,
	DIR_ENTRY_cDate = 0x10,
	DIR_ENTRY_aDate = 0x12,
	DIR_ENTRY_clusterHigh = 0x14,
	DIR_ENTRY_mTime = 0x16,
	DIR_ENTRY_mDate = 0x18,
	DIR_ENTRY_cluster = 0x1A,
	DIR_ENTRY_fileSize = 0x1C
};

/*
Returns true if the file specified by entry is a directory
*/
static inline bool _FAT_directory_isDirectory (DIR_ENTRY* entry) {
	return ((entry->entryData[DIR_ENTRY_attributes] & ATTRIB_DIR) != 0);
}

static inline bool _FAT_directory_isWritable (DIR_ENTRY* entry) {
	return ((entry->entryData[DIR_ENTRY_attributes] & ATTRIB_RO) == 0);
}

static inline bool _FAT_directory_isDot (DIR_ENTRY* entry) {
	return ((entry->filename[0] == '.') && ((entry->filename[1] == '\0') ||
		((entry->filename[1] == '.') && entry->filename[2] == '\0')));
}

/*
Reads the first directory entry from the directory starting at dirCluster
Places result in entry
entry will be destroyed even if no directory entry is found
Returns true on success, false on failure
*/
bool _FAT_directory_getFirstEntry (PARTITION* partition, DIR_ENTRY* entry, uint32_t dirCluster);

/*
Reads the next directory entry after the one already pointed to by entry
Places result in entry
entry will be destroyed even if no directory entry is found
Returns true on success, false on failure
*/
bool _FAT_directory_getNextEntry (PARTITION* partition, DIR_ENTRY* entry);

/*
Gets the directory entry corrsponding to the supplied path
entry will be destroyed even if no directory entry is found
pathEnd specifies the end of the path string, for cutting strings short if needed
 specify NULL to use the full length of path
 pathEnd is only a suggestion, and the path string will be searched up until the next PATH_SEPARATOR
 after pathEND.
Returns true on success, false on failure
*/
bool _FAT_directory_entryFromPath (PARTITION* partition, DIR_ENTRY* entry, const char* path, const char* pathEnd);

/*
Changes the current directory to the one specified by path
Returns true on success, false on failure
*/
bool _FAT_directory_chdir (PARTITION* partition, const char* path);

/*
Removes the directory entry specified by entry
Assumes that entry is valid
Returns true on success, false on failure
*/
bool _FAT_directory_removeEntry (PARTITION* partition, DIR_ENTRY* entry);

/*
Add a directory entry to the directory specified by dirCluster
The fileData, dataStart and dataEnd elements of the DIR_ENTRY struct are
updated with the new directory entry position and alias.
Returns true on success, false on failure
*/
bool _FAT_directory_addEntry (PARTITION* partition, DIR_ENTRY* entry, uint32_t dirCluster);

/*
Get the start cluster of a file from it's entry data
*/
uint32_t _FAT_directory_entryGetCluster (PARTITION* partition, const uint8_t* entryData);

/*
Fill in the file name and entry data of DIR_ENTRY* entry.
Assumes that the entry's dataStart and dataEnd are correct
Returns true on success, false on failure
*/
bool _FAT_directory_entryFromPosition (PARTITION* partition, DIR_ENTRY* entry);

/*
Fill in a stat struct based on a file entry
*/
void _FAT_directory_entryStat (PARTITION* partition, DIR_ENTRY* entry, struct stat *st);

/*
Get volume label
*/
bool _FAT_directory_getVolumeLabel (PARTITION* partition, char *label);

#endif // _DIRECTORY_H

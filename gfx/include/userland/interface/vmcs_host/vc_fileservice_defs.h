/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// File service command enumeration.

#ifndef VC_FILESERVICE_DEFS_H
#define VC_FILESERVICE_DEFS_H

#define VC_FILESERV_VER   1
/* Definitions (not used by API) */
#define FS_MAX_DATA 8192 //4096

/* Protocol (not used by API) version 1.2 */

enum {
   /* Over-the-wire file open flags */
   VC_O_RDONLY     = 0x01,
   VC_O_WRONLY     = 0x02,
   VC_O_RDWR            = 0x04,
   VC_O_APPEND     = 0x08,
   VC_O_CREAT           = 0x10,
   VC_O_TRUNC           = 0x20,
   VC_O_EXCL            = 0x40,

   /* Request Commands (VC->Host->VC) */

   /* These commands don't require a pathname */
   VC_FILESYS_RESET      = 64,
   VC_FILESYS_CLOSE      = 65,
   VC_FILESYS_CLOSEDIR   = 66,
   VC_FILESYS_LSEEK      = 67,
   VC_FILESYS_READ       = 68,
   VC_FILESYS_READDIR    = 69,
   VC_FILESYS_SETEND     = 70,
   VC_FILESYS_WRITE      = 71,

   /* These commands require a pathname */
   VC_FILESYS_FORMAT     = 72,
   VC_FILESYS_FREESPACE  = 73,
   VC_FILESYS_GET_ATTR   = 74,
   VC_FILESYS_MKDIR      = 75,
   VC_FILESYS_OPEN       = 76,
   VC_FILESYS_OPENDIR    = 77,
   VC_FILESYS_REMOVE     = 78,
   VC_FILESYS_RENAME     = 79,
   VC_FILESYS_SET_ATTR   = 80,
   VC_FILESYS_SCANDISK   = 81,
   VC_FILESYS_TOTALSPACE = 82,
   VC_FILESYS_DISKWRITABLE=83,
   VC_FILESYS_OPEN_DISK_RAW  = 84,
   VC_FILESYS_CLOSE_DISK_RAW = 85,
   VC_FILESYS_NUMSECTORS     = 86,
   VC_FILESYS_READ_SECTORS   = 87,
   VC_FILESYS_WRITE_SECTORS  = 88,

   VC_FILESYS_MOUNT      = 89,
   VC_FILESYS_UMOUNT     = 90,
   VC_FILESYS_FSTYPE     = 91,

   VC_FILESYS_READ_DIRECT = 92,

   VC_FILESYS_LSEEK64     = 93,
   VC_FILESYS_FREESPACE64 = 94,
   VC_FILESYS_TOTALSPACE64= 95,
   VC_FILESYS_OPEN_DISK   = 96,
   VC_FILESYS_CLOSE_DISK  = 97,

   /* extra simple functions for mass storage testing */
   VC_FILESYS_READ_SECTOR = 98, //1sect
   VC_FILESYS_STREAM_SECTOR_BEGIN = 99,
   VC_FILESYS_STREAM_SECTOR_END = 100,
   VC_FILESYS_WRITE_SECTOR = 101,
   VC_FILESYS_FSTAT      = 102,
   VC_FILESYS_DIRSIZE     = 103,
   VC_FILESYS_LIST_DIRS   = 104,
   VC_FILESYS_LIST_FILES  = 105,
   VC_FILESYS_NUM_DIRS    = 106,
   VC_FILESYS_NUM_FILES   = 107,
   VC_FILESYS_MAX_FILESIZE = 108,
   VC_FILESYS_CHKDSK       = 109,
};

/* Parameters for lseek */

#define  VC_FILESYS_SEEK_SET  0    /* Set file pointer to "offset" */
#define  VC_FILESYS_SEEK_CUR  1    /* Set file pointer to current plus "offset" */
#define  VC_FILESYS_SEEK_END  2    /* Set file pointer to EOF plus "offset" */

/* Return values of vc_filesys_type */
#define VC_FILESYS_FS_UNKNOWN 0
#define VC_FILESYS_FS_FAT12 1
#define VC_FILESYS_FS_FAT16 2
#define VC_FILESYS_FS_FAT32 3

#endif

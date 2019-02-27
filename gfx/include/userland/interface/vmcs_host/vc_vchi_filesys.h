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

#ifndef VC_VCHI_VCFILESYS_H_
#define VC_VCHI_VCFILESYS_H_

#include "vchost_platform_config.h"
#include "vcfilesys_defs.h"
#include "vc_fileservice_defs.h"
#include "interface/vchi/vchi.h"

#ifndef _DIRENT_H  // This should really be in a dirent.h header to avoid conflicts
typedef struct DIR_tag DIR;
#endif // ifndef _DIRENT_H

typedef struct {
   int64_t  st_size;    /* total size, in bytes  (off_t)*/
   uint32_t st_modtime;   /* time of last modification (time_t)*/
} FSTAT_T;

VCHPRE_ int VCHPOST_  vc_vchi_filesys_init (VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections );

// Stop it to prevent the functions from trying to use it.
VCHPRE_ void VCHPOST_ vc_filesys_stop(void);

// Return the service number (-1 if not running).
VCHPRE_ int VCHPOST_ vc_filesys_inum(void);

// Low level file system functions equivalent to close(), lseek(), open(), read() and write()
VCHPRE_ int VCHPOST_ vc_filesys_close(int fildes);

VCHPRE_ long VCHPOST_ vc_filesys_lseek(int fildes, long offset, int whence);

VCHPRE_ int64_t VCHPOST_ vc_filesys_lseek64(int fildes, int64_t offset, int whence);

VCHPRE_ int VCHPOST_ vc_filesys_open(const char *path, int vc_oflag);

VCHPRE_ int VCHPOST_ vc_filesys_read(int fildes, void *buf, unsigned int nbyte);

VCHPRE_ int VCHPOST_ vc_filesys_write(int fildes, const void *buf, unsigned int nbyte);

VCHPRE_ int VCHPOST_ vc_filesys_mount(const char *device, const char *mountpoint, const char *options);
VCHPRE_ int VCHPOST_ vc_filesys_umount(const char *mountpoint);

// Ends a directory listing iteration
VCHPRE_ int VCHPOST_ vc_filesys_closedir(void *dhandle);

// Formats the drive that contains the given path
VCHPRE_ int VCHPOST_ vc_filesys_format(const char *path);

// Returns the amount of free space on the drive that contains the given path
VCHPRE_ int VCHPOST_ vc_filesys_freespace(const char *path);
VCHPRE_ int64_t VCHPOST_ vc_filesys_freespace64(const char *path);

// Gets the attributes of the named file
VCHPRE_ int VCHPOST_ vc_filesys_get_attr(const char *path, fattributes_t *attr);

// Get the file stat info struct for the specified file.
VCHPRE_ int VCHPOST_ vc_filesys_fstat(int filedes, FSTAT_T *buf);

// Creates a new directory
VCHPRE_ int VCHPOST_ vc_filesys_mkdir(const char *path);

// Starts a directory listing iteration
VCHPRE_ void * VCHPOST_ vc_filesys_opendir(const char *dirname);

// Directory listing iterator
VCHPRE_ struct dirent * VCHPOST_ vc_filesys_readdir_r(void *dhandle, struct dirent *result);

// Get the sum of the filesizes, and the number of files under the specified directory path.
VCHPRE_ int64_t VCHPOST_ vc_filesys_dirsize(const char *path, uint32_t *num_files, uint32_t *num_dirs);

// Deletes a file or (empty) directory
VCHPRE_ int VCHPOST_ vc_filesys_remove(const char *path);

// Renames a file, provided the new name is on the same file system as the old
VCHPRE_ int VCHPOST_ vc_filesys_rename(const char *oldfile, const char *newfile);

// Resets the co-processor side file system
VCHPRE_ int VCHPOST_ vc_filesys_reset(void);

// Sets the attributes of the named file
VCHPRE_ int VCHPOST_ vc_filesys_set_attr(const char *path, fattributes_t attr);

// Truncates a file at its current position
VCHPRE_ int VCHPOST_ vc_filesys_setend(int fildes);

// Returns the size of a file in bytes.
VCHPRE_ int VCHPOST_ vc_filesys_size(const char *path);

// Checks whether there are any messages in the incoming message fifo and responds to any such messages
VCHPRE_ int VCHPOST_ vc_filesys_poll_message_fifo(void);

// Return the event used to wait for reads.
VCHPRE_ void * VCHPOST_ vc_filesys_read_event(void);

// Sends a command for VC01 to reset the file system
VCHPRE_ void VCHPOST_ vc_filesys_sendreset(void);

// Return the error code of the last file system error
VCHPRE_ int VCHPOST_ vc_filesys_errno(void);

// Invalidates any cluster chains in the FAT that are not referenced in any directory structures
VCHPRE_ void VCHPOST_ vc_filesys_scandisk(const char *path);

// Checks whether or not a FAT filesystem is corrupt or not. If fix_errors is TRUE behaves exactly as vc_filesys_scandisk.
VCHPRE_ int VCHPOST_ vc_filesys_chkdsk(const char *path, int fix_errors);

// Return whether a disk is writeable or not.
VCHPRE_ int VCHPOST_ vc_filesys_diskwritable(const char *path);

// Return file system type of a disk.
VCHPRE_ int VCHPOST_ vc_filesys_fstype(const char *path);

// Returns the toatl amount of space on the drive that contains the given path
VCHPRE_ int VCHPOST_ vc_filesys_totalspace(const char *path);
VCHPRE_ int64_t VCHPOST_ vc_filesys_totalspace64(const char *path);

// Open disk for block level access
VCHPRE_ int VCHPOST_ vc_filesys_open_disk_raw(const char *path);

// Close disk from block level access mode
VCHPRE_ int VCHPOST_ vc_filesys_close_disk_raw(const char *path);

// Open disk for normal access
VCHPRE_ int VCHPOST_ vc_filesys_open_disk(const char *path);

// Close disk for normal access
VCHPRE_ int VCHPOST_ vc_filesys_close_disk(const char *path);

// Return number of sectors.
VCHPRE_ int VCHPOST_ vc_filesys_numsectors(const char *path);
VCHPRE_ int64_t VCHPOST_ vc_filesys_numsectors64(const char *path);

// Read/Write sectors
VCHPRE_ int VCHPOST_ vc_filesys_read_sectors(const char *path, uint32_t sector_num, char *sectors, uint32_t num_sectors, uint32_t *sectors_read);
VCHPRE_ int VCHPOST_ vc_filesys_write_sectors(const char *path, uint32_t sector_num, char *sectors, uint32_t num_sectors, uint32_t *sectors_written);

// Begin reading sectors from VideoCore.
VCHPRE_ int VCHPOST_ vc_filesys_read_sectors_begin(const char *path, uint32_t sector, uint32_t count);

// Read the next sector.
VCHPRE_ int VCHPOST_ vc_filesys_read_sector(char *buf);

// End streaming sectors.
VCHPRE_ int VCHPOST_ vc_filesys_read_sectors_end(uint32_t *sectors_read);

// Begin writing sectors from VideoCore.
VCHPRE_ int VCHPOST_ vc_filesys_write_sectors_begin(const char *path, uint32_t sector, uint32_t count);

// Write the next sector.
VCHPRE_ int VCHPOST_ vc_filesys_write_sector(const char *buf);

// End streaming sectors.
VCHPRE_ int VCHPOST_ vc_filesys_write_sectors_end(uint32_t *sectors_written);

#endif //VCFILESYS_H_

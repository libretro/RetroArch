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

#ifndef VCHOST_H
#define VCHOST_H

#include "vchost_platform_config.h"
#include "vcfilesys_defs.h"
#include "interface/vcos/vcos.h" //for VCHPRE_ abd VCHPOST_ macro's for func declaration
#include "interface/vmcs_host/vc_fileservice_defs.h" // for VC_O_XXX file definitions
#include "interface/vchi/vchi.h"

#define UNUSED_PARAMETER(x) ((void)(x))/* macro to suppress not use warning */

/*---------------------------------------------------------------------------*/
/* Byte-swapping, dependent on host's orientation */
/*---------------------------------------------------------------------------*/

#ifndef VC_HOST_IS_BIG_ENDIAN
#define VC_HTOV32(val) (val)
#define VC_HTOV16(val) (val)
#define VC_VTOH32(val) (val)
#define VC_VTOH16(val) (val)
#else
static unsigned long  VC_HTOV32(unsigned long val)  {
   return ((val<<24) | ((val&0xff00)<<8) | ((val>>8)&0xff00) | ((val>>24)&0xff)); }
static unsigned short VC_HTOV16(unsigned short val) {
   return ((val<<8)|(val>>8)); }
static unsigned long  VC_VTOH32(unsigned long val)  {
   return ((val<<24) | ((val&0xff00)<<8) | ((val>>8)&0xff00) | ((val>>24)&0xff)); }
static unsigned short VC_VTOH16(unsigned short val) {
   return ((val<<8)|(val>>8)); }
#endif

/*---------------------------------------------------------------------------*/
/* Host port related functions */
/*---------------------------------------------------------------------------*/

/* Boot a bin file from flash into RAM. Returns the id of the application running */

VCHPRE_ int VCHPOST_ vc_host_boot(char *cmd_line, void *binimg, int nbytes, int bootloader);

/* Perform any platform specific initialisations. */

VCHPRE_ int VCHPOST_ vc_host_init(void);

/* Read a multiple of 16 bytes from VideoCore. host_addr has no particular alignment,
   but it is important that it transfers the data in 16-bit chunks if this is possible. */

VCHPRE_ int VCHPOST_ vc_host_read_consecutive(void *host_addr, uint32_t vc_addr, int nbytes, int channel);

#ifdef VC_HOST_IS_BIG_ENDIAN
// Reads from VideoCore with an implicit swap of each pair of bytes.
VCHPRE_ int VCHPOST_ vc_host_read_byteswapped(void *host_addr, uint32_t vc_addr, int nbytes, int channel);
#endif

/* Write a multiple of 16 bytes to VideoCore. host_addr has no particular alignment,
   but it is important that it transfers the data in 16-bit chunks if this is possible. */

VCHPRE_ int VCHPOST_ vc_host_write_consecutive(uint32_t vc_addr, void *host_addr, int nbytes, int channel);

#ifdef VC_HOST_IS_BIG_ENDIAN
// Write to VideoCore with an implicit swap of each pair of bytes.
VCHPRE_ int VCHPOST_ vc_host_write_byteswapped(uint32_t vc_addr, void *host_addr, int nbytes, int channel);
#endif

/* Send an interrupt to VideoCore. */

VCHPRE_ int VCHPOST_ vc_host_send_interrupt(int channel);

/* Wait for an interrupt from VideoCore. This can return immediately if applications
   are happy to busy-wait. */

VCHPRE_ int VCHPOST_ vc_host_wait_interrupt(void);

/* Tell the host to act on or ignore interrupts. */

VCHPRE_ void VCHPOST_ vc_host_interrupts(int on);

/* Function called when there is some kind of internal error. Breakpoints can be set on
   this for debugging. */

VCHPRE_ void VCHPOST_ vc_error(void);

/*---------------------------------------------------------------------------*/
/* Event (interrupt) related functions */
/*---------------------------------------------------------------------------*/

// Minimum number of event objects an implementation should support.
// Sufficient for 2 per 8 interfaces/services + 4 others
#define VC_EVENT_MAX_NUM  20

/* Create (and clear) an event.  Returns a pointer to the event object. */
VCHPRE_ void * VCHPOST_ vc_event_create(void);

/* Wait for an event to be set, blocking until it is set.
   Only one thread may be waiting at any one time.
   The event is automatically cleared on leaving this function. */
VCHPRE_ void VCHPOST_ vc_event_wait(void *sig);

/* Reads the state of an event (for polling systems) */
VCHPRE_ int VCHPOST_ vc_event_status(void *sig);

/* Forcibly clears any pending event */
VCHPRE_ void VCHPOST_ vc_event_clear(void *sig);

/* Sets an event - can be called from any thread */
VCHPRE_ void VCHPOST_ vc_event_set(void *sig);

/* Register the calling task to be notified of an event. */
VCHPRE_ void VCHPOST_ vc_event_register(void *ievent);

/* Set events to block, stopping polling mode. */
VCHPRE_ void VCHPOST_ vc_event_blocking(void);

/*---------------------------------------------------------------------------*/
/* Semaphore related functions */
/*---------------------------------------------------------------------------*/

// Minimum number of locks an implementation should support.

#define VC_LOCK_MAX_NUM 32

// Create a lock. Returns a pointer to the lock object. A lock is initially available
// just once.

VCHPRE_ void * VCHPOST_ vc_lock_create(void);

// Obtain a lock. Block until we have it. Locks are not re-entrant for the same thread.

VCHPRE_ void VCHPOST_ vc_lock_obtain(void *lock);

// Release a lock. Anyone can call this, even if they didn't obtain the lock first.

VCHPRE_ void VCHPOST_ vc_lock_release(void *lock);

/*---------------------------------------------------------------------------*/
/* File system related functions */
/*---------------------------------------------------------------------------*/

// Initialises the host dependent file system functions for use
VCHPRE_ void VCHPOST_ vc_hostfs_init(void);
VCHPRE_ void VCHPOST_ vc_hostfs_exit(void);

// Low level file system functions equivalent to close(), lseek(), open(), read() and write()
VCHPRE_ int VCHPOST_ vc_hostfs_close(int fildes);

VCHPRE_ long VCHPOST_ vc_hostfs_lseek(int fildes, long offset, int whence);

VCHPRE_ int64_t VCHPOST_ vc_hostfs_lseek64(int fildes, int64_t offset, int whence);

VCHPRE_ int VCHPOST_ vc_hostfs_open(const char *path, int vc_oflag);

VCHPRE_ int VCHPOST_ vc_hostfs_read(int fildes, void *buf, unsigned int nbyte);

VCHPRE_ int VCHPOST_ vc_hostfs_write(int fildes, const void *buf, unsigned int nbyte);

// Ends a directory listing iteration
VCHPRE_ int VCHPOST_ vc_hostfs_closedir(void *dhandle);

// Formats the drive that contains the given path
VCHPRE_ int VCHPOST_ vc_hostfs_format(const char *path);

// Returns the amount of free space on the drive that contains the given path
VCHPRE_ int VCHPOST_ vc_hostfs_freespace(const char *path);
VCHPRE_ int64_t VCHPOST_ vc_hostfs_freespace64(const char *path);

// Gets the attributes of the named file
VCHPRE_ int VCHPOST_ vc_hostfs_get_attr(const char *path, fattributes_t *attr);

// Creates a new directory
VCHPRE_ int VCHPOST_ vc_hostfs_mkdir(const char *path);

// Starts a directory listing iteration
VCHPRE_ void * VCHPOST_ vc_hostfs_opendir(const char *dirname);

// Directory listing iterator
VCHPRE_ struct dirent * VCHPOST_ vc_hostfs_readdir_r(void *dhandle, struct dirent *result);

// Deletes a file or (empty) directory
VCHPRE_ int VCHPOST_ vc_hostfs_remove(const char *path);

// Renames a file, provided the new name is on the same file system as the old
VCHPRE_ int VCHPOST_ vc_hostfs_rename(const char *oldfile, const char *newfile);

// Sets the attributes of the named file
VCHPRE_ int VCHPOST_ vc_hostfs_set_attr(const char *path, fattributes_t attr);

// Truncates a file at its current position
VCHPRE_ int VCHPOST_ vc_hostfs_setend(int fildes);

// Returns the total amount of space on the drive that contains the given path
VCHPRE_ int VCHPOST_ vc_hostfs_totalspace(const char *path);
VCHPRE_ int64_t VCHPOST_ vc_hostfs_totalspace64(const char *path);

// Return millisecond resolution system time, only used for differences
VCHPRE_ int VCHPOST_ vc_millitime(void);

// Invalidates any cluster chains in the FAT that are not referenced in any directory structures
VCHPRE_ void VCHPOST_ vc_hostfs_scandisk(const char *path);

// Checks whether or not a FAT filesystem is corrupt or not. If fix_errors is TRUE behaves exactly as vc_filesys_scandisk.
VCHPRE_ int VCHPOST_ vc_hostfs_chkdsk(const char *path, int fix_errors);

/*---------------------------------------------------------------------------*/
/* These functions only need to be implemented for the test system. */
/*---------------------------------------------------------------------------*/

// Open a log file.
VCHPRE_ void VCHPOST_ vc_log_open(const char *fname);

// Flush any pending data to the log file.
VCHPRE_ void VCHPOST_ vc_log_flush(void);

// Close the log file.
VCHPRE_ void VCHPOST_ vc_log_close(void);

// Log an error.
VCHPRE_ void VCHPOST_ vc_log_error(const char *format, ...);

// Log a warning.
VCHPRE_ void VCHPOST_ vc_log_warning(const char *format, ...);

// Write a message to the log.
VCHPRE_ void VCHPOST_ vc_log_msg(const char *format, ...);

// Flush the log.
VCHPRE_ void VCHPOST_ vc_log_flush(void);

// Return the total number of warnings and errors logged.
VCHPRE_ void VCHPOST_ vc_log_counts(int *warnings, int *errors);

// Wait for the specified number of microseconds. Used in test system only.
VCHPRE_ void VCHPOST_ vc_sleep(int ms);

// Get a time value in milliseconds. Used for measuring time differences
VCHPRE_ uint32_t VCHPOST_ vc_time(void);

// Check timing functions are available. Use in calibrating tests.
VCHPRE_ int VCHPOST_ calibrate_sleep (const char *data_dir);

/*---------------------------------------------------------------------------*/
/* Functions to allow dynamic service creation */
/*---------------------------------------------------------------------------*/

VCHPRE_ void VCHPOST_ vc_host_get_vchi_state(VCHI_INSTANCE_T *initialise_instance, VCHI_CONNECTION_T **connection);

#endif

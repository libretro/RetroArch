/* Copyright (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro_vfs.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRETRO_VFS_H__
#define LIBRETRO_VFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <libretro.h>

/* Opaque file handle
 * Introduced in VFS API v1 */
struct retro_vfs_file_handle;

/* File open flags
 * Introduced in VFS API v1 */
#define RETRO_VFS_FILE_ACCESS_READ            (1 << 0) /* Read only mode */
#define RETRO_VFS_FILE_ACCESS_WRITE           (1 << 1) /* Write only mode, discard contents and overwrites existing file unless RETRO_VFS_FILE_ACCESS_UPDATE is also specified */
#define RETRO_VFS_FILE_ACCESS_READ_WRITE      (RETRO_VFS_FILE_ACCESS_READ | RETRO_VFS_FILE_ACCESS_WRITE) /* Read-write mode, discard contents and overwrites existing file unless RETRO_VFS_FILE_ACCESS_UPDATE is also specified*/
#define RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING (1 << 2) /* Prevents discarding content of existing files opened for writing */

#define RETRO_VFS_FILE_ACCESS_HINT_NONE            (0)
/* Indicate that we would want to map the file into memory if possible. Requires RETRO_VFS_FILE_ACCESS_READ. This is only a hint and it is up to the frontend to honor and implement it. */
#define RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP      (1 << 0)


/* Get path from opaque handle. Returns the exact same path passed to file_open when getting the handle
 * Introduced in VFS API v1 */
typedef const char *(RETRO_CALLCONV *retro_vfs_file_get_path_t)(struct retro_vfs_file_handle *stream);

/* Open a file for reading or writing. If path points to a directory, this will
 * fail. Returns the opaque file handle, or NULL for error.
 * Introduced in VFS API v1 */
typedef struct retro_vfs_file_handle *(RETRO_CALLCONV *retro_vfs_file_open_t)(const char *path, uint64_t flags);

/* Close the file and release its resources. Must be called if open_file returns non-NULL. Returns 0 on succes, -1 on failure.
 * Whether the call succeeds ot not, the handle passed as parameter becomes invalid and should no longer be used.
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_file_close_t)(struct retro_vfs_file_handle *stream);

/* Return the size of the file in bytes, or -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_file_size_t)(struct retro_vfs_file_handle *stream);

/* Get the current read / write position for the file. Returns - 1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_file_tell_t)(struct retro_vfs_file_handle *stream);

/* Set the current read/write position for the file. Returns the new position, -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_file_seek_t)(struct retro_vfs_file_handle *stream, int64_t offset);

/* Read data from a file. Returns the number of bytes read, or -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_file_read_t)(struct retro_vfs_file_handle *stream, void *s, uint64_t len);

/* Write data to a file. Returns the number of bytes written, or -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_file_write_t)(struct retro_vfs_file_handle *stream, const void *s, uint64_t len);

/* Flush pending writes to file, if using buffered IO. Returns 0 on sucess, or -1 on failure.
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_file_flush_t)(struct retro_vfs_file_handle *stream);

/* Delete the specified file. Returns 0 on success, -1 on failure
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_file_delete_t)(const char *path);

struct retro_vfs_interface
{
	retro_vfs_file_get_path_t file_get_path;
	retro_vfs_file_open_t file_open;
	retro_vfs_file_close_t file_close;
	retro_vfs_file_size_t file_size;
	retro_vfs_file_tell_t file_tell;
	retro_vfs_file_seek_t file_seek;
	retro_vfs_file_read_t file_read;
	retro_vfs_file_write_t file_write;
	retro_vfs_file_flush_t file_flush;
	retro_vfs_file_delete_t file_delete;
};

struct retro_vfs_interface_info
{
   /* Set by core: should this be higher than the version the front end supports,
    * front end will return false in the RETRO_ENVIRONMENT_GET_VFS_INTERFACE call
    * Introduced in VFS API v1 */
   uint32_t required_interface_version;

   /* Frontend writes interface pointer here. The frontend also sets the actual
    * version, must be at least required_interface_version.
    * Introduced in VFS API v1 */
   struct retro_vfs_interface *iface;
};

#define RETRO_ENVIRONMENT_GET_VFS_INTERFACE (45 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_vfs_interface_info * --
                                            * Gets access to the VFS interface.
                                            * VFS presence needs to be queried prior to load_game or any
                                            * get_system/save/other_directory being called to let front end know
                                            * core supports VFS before it starts handing out paths.
                                            * It is recomended to do so in retro_set_environment */

#ifdef __cplusplus
}
#endif

#endif

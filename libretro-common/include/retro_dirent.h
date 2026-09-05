/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_dirent.h).
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

#ifndef __RETRO_DIRENT_H
#define __RETRO_DIRENT_H

#include <libretro.h>
#include <retro_common_api.h>

#include <boolean.h>

/** @defgroup dirent Directory Entries
 * @{
 */

RETRO_BEGIN_DECLS

/**
 * The minimum VFS version (as defined in \c retro_vfs_interface_info::required_interface_version)
 * required by the \c dirent functions.
 * If no acceptable VFS interface is provided,
 * all dirent functions will fall back to libretro-common's implementations.
 * @see retro_vfs_interface_info
 */
#define DIRENT_REQUIRED_VFS_VERSION 3

/**
 * Installs a frontend-provided VFS interface for the dirent functions to use
 * instead of libretro-common's built-in implementations.
 *
 * @param vfs_info The VFS interface returned by the frontend.
 * The dirent functions will fall back to libretro-common's implementations
 * if \c vfs_info::required_interface_version is too low.
 * @see retro_vfs_interface_info
 * @see RETRO_ENVIRONMENT_GET_VFS_INTERFACE
 */
void dirent_vfs_init(const struct retro_vfs_interface_info* vfs_info);

/**
 * Opaque handle to a directory entry (aka "dirent").
 * It may name a file, directory, or other filesystem object.
 * @see retro_opendir
 */
typedef struct RDIR RDIR;

/**
 * Opens a directory for reading.
 *
 * @param name Path to a directory to open.
 * @return An \c RDIR representing the given directory if successful.
 * Returns \c NULL if \c name is \c NULL, the empty string, or does not name a directory.
 * @note The returned \c RDIR must be closed with \c retro_closedir.
 * @see retro_opendir_include_hidden
 * @see retro_closedir
 */
struct RDIR *retro_opendir(const char *name);

/**
 * @copybrief retro_opendir
 *
 * @param name Path to the directory to open.
 * @param include_hidden Whether to include hidden files and directories
 * when iterating over this directory with \c retro_readdir.
 * Platforms and filesystems have different notions of "hidden" files.
 * Setting this to \c false will not prevent this function from opening \c name.
 * @return An \c RDIR representing the given directory if successful.
 * Returns \c NULL if \c name is \c NULL, the empty string, or does not name a directory.
 * @note The returned \c RDIR must be closed with \c retro_closedir.
 * @see retro_opendir
 * @see retro_closedir
 */
struct RDIR *retro_opendir_include_hidden(const char *name, bool include_hidden);

/**
 * Reads the next entry in the given directory.
 *
 * Here's a usage example that prints the names of all files in the current directory:
 * @code
 * struct RDIR *rdir = retro_opendir(".");
 * if (rdir)
 * {
 *    while (retro_readdir(rdir))
 *    {
 *       const char *name = retro_dirent_get_name(rdir);
 *       printf("%s\n", name);
 *    }
 *    retro_closedir(rdir);
 *    rdir = NULL;
 * }
 * @endcode
 *
 * @param rdir The directory to iterate over.
 * Behavior is undefined if \c NULL.
 * @return \c true if the next entry was read successfully,
 * \c false if there are no more entries to read or if there was an error.
 * @note This may include "." and ".." on Unix-like platforms.
 * @see retro_dirent_get_name
 * @see retro_dirent_is_dir
 */
int retro_readdir(struct RDIR *rdir);

/**
 * @deprecated Left for compatibility.
 * @param rdir Ignored.
 * @return \c false.
 */
bool retro_dirent_error(struct RDIR *rdir);

/**
 * Gets the name of the dirent's current file or directory.
 *
 * @param rdir The dirent to get the name of.
 * Behavior is undefined if \c NULL.
 * @return The name of the directory entry (file, directory, etc.) that the dirent points to.
 * Will return \c NULL if there was an error,
 * \c retro_readdir has not been called on \c rdir,
 * or if there are no more entries to read.
 * @note This returns only a name, not a full path.
 * @warning The returned string is managed by the VFS implementation
 * and must not be modified or freed by the caller.
 * @warning The returned string is only valid until the next call to \c retro_readdir.
 * @see retro_readdir
 */
const char *retro_dirent_get_name(struct RDIR *rdir);

/**
 * Checks if the given \c RDIR's current dirent names a directory.
 *
 * @param rdir The directory entry to check.
 * Behavior is undefined if \c NULL.
 * @param unused Ignored for compatibility reasons. Pass \c NULL.
 * @return \c true if \c rdir refers to a directory, otherwise \c false.
 * @see retro_readdir
 */
bool retro_dirent_is_dir(struct RDIR *rdir, const char *unused);

/**
 * Closes an opened \c RDIR that was returned by \c retro_opendir.
 *
 * @param rdir The directory entry to close.
 * If \c NULL, this function does nothing.
 * @see retro_vfs_closedir_t
 */
void retro_closedir(struct RDIR *rdir);

RETRO_END_DECLS

/** @} */

#endif

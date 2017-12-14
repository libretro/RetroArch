/* Copyright  (C) 2010-2017 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (vfs_implementation.h).
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

#ifndef __LIBRETRO_SDK_VFS_IMPLEMENTATION_H
#define __LIBRETRO_SDK_VFS_IMPLEMENTATION_H

#include <stdint.h>
#include <libretro.h>

/* Replace the following symbol with something appropriate
 * to signify the file is being compiled for a front end instead of a core.
 * This allows the same code to act as reference implementation
 * for VFS and as fallbacks for when the front end does not provide VFS functionality.
 */

void *retro_vfs_file_open_impl(const char *path, unsigned mode, unsigned hints);

int retro_vfs_file_close_impl(void *data);

int retro_vfs_file_error_impl(void *data);

int64_t retro_vfs_file_size_impl(void *data);

int64_t retro_vfs_file_tell_impl(void *data);

int64_t retro_vfs_file_seek_impl(void *data, int64_t offset, int whence);

int64_t retro_vfs_file_read_impl(void *data, void *s, uint64_t len);

int64_t retro_vfs_file_write_impl(void *data, const void *s, uint64_t len);

int retro_vfs_file_flush_impl(void *data);

int retro_vfs_file_delete_impl(const char *path);

const char *retro_vfs_file_get_path_impl(void *data);

#endif

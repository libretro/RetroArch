/* Copyright  (C) 2010-2020 The RetroArch team
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

#include <stdio.h>
#include <stdint.h>
#include <libretro.h>
#include <retro_environment.h>
#include <vfs/vfs.h>

RETRO_BEGIN_DECLS

libretro_vfs_implementation_file *retro_vfs_file_open_impl(const char *path, unsigned mode, unsigned hints);

/* Deallocate a range of a file without changing its length; see
 * filestream_punch_hole. Returns 0 on success, -1 when the platform or
 * filesystem cannot do it. */
int retro_vfs_file_punch_hole_impl(libretro_vfs_implementation_file *stream, int64_t offset, int64_t len);

int retro_vfs_file_close_impl(libretro_vfs_implementation_file *stream);

int retro_vfs_file_error_impl(libretro_vfs_implementation_file *stream);

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream);

int64_t retro_vfs_file_truncate_impl(libretro_vfs_implementation_file *stream, int64_t length);

int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file *stream);

int64_t retro_vfs_file_seek_impl(libretro_vfs_implementation_file *stream, int64_t offset, int seek_position);

int64_t retro_vfs_file_read_impl(libretro_vfs_implementation_file *stream, void *s, uint64_t len);

int64_t retro_vfs_file_write_impl(libretro_vfs_implementation_file *stream, const void *s, uint64_t len);

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream);

int retro_vfs_file_remove_impl(const char *path);

int retro_vfs_file_rename_impl(const char *old_path, const char *new_path);

const char *retro_vfs_file_get_path_impl(libretro_vfs_implementation_file *stream);

/* Borrowed pointer to the whole file, when this stream is backed by a
 * memory map, else NULL.  Valid until the stream is closed; read-only;
 * never freed by the caller.
 *
 * RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS already maps the file,
 * but the mapping was reachable only from inside the read path, which
 * memcpy's out of it - so every consumer of the hint paid for a copy
 * of data that was already addressable, and the point of asking for
 * the map was lost.  This hands the map itself to callers that want
 * to look rather than own: comparing a file against a buffer, hashing
 * it, or passing it to a parser that takes a base pointer.
 *
 * NULL is a normal answer, not an error - no HAVE_MMAP, hint not
 * given, a write mode, a scheme with no mapping, or a frontend-
 * supplied VFS - so callers need the copying path anyway and should
 * treat this as a fast lane, never as the only one. */
const uint8_t *retro_vfs_file_get_mapped_ptr_impl(
      libretro_vfs_implementation_file *stream, int64_t *len);

int retro_vfs_stat_impl(const char *path, int32_t *size);

int retro_vfs_stat_64_impl(const char *path, int64_t *size);

int retro_vfs_mkdir_impl(const char *dir);

libretro_vfs_implementation_dir *retro_vfs_opendir_impl(const char *dir, bool include_hidden);

bool retro_vfs_readdir_impl(libretro_vfs_implementation_dir *dirstream);

const char *retro_vfs_dirent_get_name_impl(libretro_vfs_implementation_dir *dirstream);

bool retro_vfs_dirent_is_dir_impl(libretro_vfs_implementation_dir *dirstream);

int retro_vfs_closedir_impl(libretro_vfs_implementation_dir *dirstream);

#ifdef __WINRT__

void uwp_set_acl(const wchar_t* path, const wchar_t* AccessString);

#endif

RETRO_END_DECLS

#endif

/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_implementation_cdrom.h).
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

#ifndef __LIBRETRO_SDK_VFS_IMPLEMENTATION_CDROM_H
#define __LIBRETRO_SDK_VFS_IMPLEMENTATION_CDROM_H

#include <vfs/vfs.h>
#include <cdrom/cdrom.h>

RETRO_BEGIN_DECLS

int64_t retro_vfs_file_seek_cdrom(libretro_vfs_implementation_file *stream, int64_t offset, int whence);

/* Linux: cdrom://driveN.iso (sg device N), Windows: cdrom://D:/drive.iso.
 * Read-only cooked 2048-byte image; capacity is probed per handle.
 * No UDF parsing, DVD navigation or decryption is performed here.
 * macOS cooked images are not yet supported. A .device URI is for metadata
 * queries only and does not require readable CD geometry. */
void retro_vfs_file_open_cdrom(
      libretro_vfs_implementation_file *stream,
      const char *path, unsigned mode, unsigned hints);

int retro_vfs_file_close_cdrom(libretro_vfs_implementation_file *stream);

int64_t retro_vfs_file_tell_cdrom(libretro_vfs_implementation_file *stream);

int64_t retro_vfs_file_read_cdrom(libretro_vfs_implementation_file *stream,
      void *s, uint64_t len);

int retro_vfs_file_error_cdrom(libretro_vfs_implementation_file *stream);

/* Borrowed immutable geometry, valid until this handle closes. NULL for
 * metadata-only probes and cooked images. BIN opens obtain their own TOC;
 * no preceding CUE open is required. Drive-reported medium changes, resets
 * or removal make bound data handles unreadable until reopened. Other
 * handles and changes not reported to this handle are not tracked here. */
const cdrom_toc_t* retro_vfs_file_get_cdrom_toc(
      const libretro_vfs_implementation_file *stream);

const vfs_cdrom_t* retro_vfs_file_get_cdrom_position(const libretro_vfs_implementation_file *stream);

RETRO_END_DECLS

#endif

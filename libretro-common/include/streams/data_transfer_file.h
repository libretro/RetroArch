/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer_file.h).
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


#ifndef __LIBRETRO_SDK_STREAMS_DATA_TRANSFER_FILE_H__
#define __LIBRETRO_SDK_STREAMS_DATA_TRANSFER_FILE_H__

#include <stddef.h>

#include <retro_common_api.h>
#include <formats/data_transfer.h>

RETRO_BEGIN_DECLS

/* data_transfer over a file opened by path through filestream, so
 * 64-bit offsets and VFS-only paths (archive members, Android
 * content:// documents, frontend overrides) work like any other.
 * The transfer owns the file and closes it when freed. */

/* data_transfer_open_prefix_reader over the file at @path */
data_transfer_t *data_transfer_open_prefix(const char *path,
      size_t commit_cap);

/* data_transfer_open_window_reader over the file at @path */
data_transfer_t *data_transfer_open_window(const char *path, size_t keep);

RETRO_END_DECLS

#endif

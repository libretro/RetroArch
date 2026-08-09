/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (interface_stream.h).
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

#ifndef _LIBRETRO_SDK_INTERFACE_STREAM_H
#define _LIBRETRO_SDK_INTERFACE_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

enum intfstream_type
{
   INTFSTREAM_FILE = 0,
   INTFSTREAM_MEMORY,
   INTFSTREAM_CHD,
   INTFSTREAM_RZIP,
   /* Read-only file access through a sliding buffer - see
    * intfstream_open_buffered(). */
   INTFSTREAM_BUFFERED
};

typedef struct intfstream_internal intfstream_internal_t, intfstream_t;

typedef struct intfstream_info
{
   struct
   {
      struct
      {
         uint8_t *data;
         uint64_t size;
      } buf;
      bool writable;
   } memory;
   struct
   {
      void *handle;
      int32_t track;
   } chd;
   enum intfstream_type type;
} intfstream_info_t;

void *intfstream_init(intfstream_info_t *info);

bool intfstream_resize(intfstream_internal_t *intf,
      intfstream_info_t *info);

bool intfstream_open(intfstream_internal_t *intf,
      const char *path, unsigned mode, unsigned hints);

int64_t intfstream_read(intfstream_internal_t *intf,
      void *s, uint64_t len);

int64_t intfstream_write(intfstream_internal_t *intf,
      const void *s, uint64_t len);

int intfstream_printf(intfstream_internal_t *intf,
      const char* format, ...);

int64_t intfstream_get_ptr(intfstream_internal_t *intf);

char *intfstream_gets(intfstream_internal_t *intf,
      char *buffer, uint64_t len);

int intfstream_getc(intfstream_internal_t *intf);

int64_t intfstream_seek(intfstream_internal_t *intf,
      int64_t offset, int whence);

int64_t intfstream_truncate(intfstream_internal_t *intf,
      uint64_t len);

void intfstream_rewind(intfstream_internal_t *intf);

int64_t intfstream_tell(intfstream_internal_t *intf);

int intfstream_eof(intfstream_internal_t *intf);

void intfstream_putc(intfstream_internal_t *intf, int c);

int intfstream_close(intfstream_internal_t *intf);

int64_t intfstream_get_size(intfstream_internal_t *intf);

int intfstream_flush(intfstream_internal_t *intf);

uint32_t intfstream_get_offset_to_start(intfstream_internal_t *intf);

uint32_t intfstream_get_frame_size(intfstream_internal_t *intf);

uint32_t intfstream_get_first_sector(intfstream_internal_t* intf);

bool intfstream_is_compressed(intfstream_internal_t *intf);

/**
 * intfstream_crc_step:
 *
 * Hash at most @max_bytes more of @intf into @accumulator, resuming
 * from where the previous call stopped.  Returns bytes hashed, 0 at
 * end of stream, -1 on error.
 *
 * For callers inside a task handler that must bound how long one tick
 * spends hashing; intfstream_get_crc() consumes the whole stream in
 * one call, making the tick cost a function of file size.  Timing
 * policy stays with the caller, which owns the deadline and the
 * quantum.  Position the stream and zero @accumulator before the
 * first step.
 **/
int64_t intfstream_crc_step(intfstream_internal_t *intf,
      uint32_t *accumulator, size_t max_bytes);

bool intfstream_get_crc(intfstream_internal_t *intf, uint32_t *crc);

intfstream_t *intfstream_open_file(const char *path,
      unsigned mode, unsigned hints);

/* Open @path read-only behind a sliding window of @window bytes.
 *
 * Reads that fall inside the window are a memcpy; the window is
 * refilled by seek+read when one does not.  For a consumer issuing
 * very many small reads - a MsgPack walk over an .rdb spends its time
 * there - this removes the per-read trip through
 * filestream/VFS/fread while keeping the resident cost fixed at
 * @window rather than the size of the file.
 *
 * A backwards seek inside the window is free; one outside it costs a
 * refill, so @window should comfortably exceed the caller's largest
 * backwards jump.
 *
 * Returns NULL if the file cannot be opened or the window cannot be
 * allocated; callers should fall back to intfstream_open_file(). */
intfstream_t *intfstream_open_buffered(const char *path, uint64_t window);

intfstream_t *intfstream_open_memory(void *data,
      unsigned mode, unsigned hints, uint64_t size);

/* Deprecated.  Has the same effect as `intfstream_open_memory` with
   a mode including `RETRO_VFS_FILE_ACCESS_WRITE`. */
intfstream_t *intfstream_open_writable_memory(void *data,
      unsigned mode, unsigned hints, uint64_t size);

intfstream_t *intfstream_open_chd_track(const char *path,
      unsigned mode, unsigned hints, int32_t track);

intfstream_t *intfstream_open_rzip_file(const char *path,
      unsigned mode);

RETRO_END_DECLS

#endif

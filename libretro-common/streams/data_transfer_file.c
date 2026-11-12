/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer_file.c).
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


/* The file side of data_transfer: formats/data_transfer.c reads only
 * through its reader callback, and this opens the file, supplies the
 * reader and closes the file with the transfer. */

#include <stdint.h>

#include <streams/file_stream.h>
#include <streams/data_transfer_file.h>

/* @bytes at @offset: one read at the offset, or a seek and a read
 * where the file has none */
static int64_t data_transfer_file_read_at(void *ud, uint64_t offset,
      uint8_t *dst, size_t bytes)
{
   RFILE  *f = (RFILE*)ud;
   int64_t got;
   if (offset > (uint64_t)INT64_MAX)
      return -1;
   if ((got = filestream_read_at(f, dst, (int64_t)bytes,
               (int64_t)offset)) >= 0)
      return got;
   if (filestream_seek(f, (int64_t)offset,
            RETRO_VFS_SEEK_POSITION_START) < 0)
      return -1;
   return filestream_read(f, dst, (int64_t)bytes);
}

static void data_transfer_file_release(void *ud)
{
   filestream_close((RFILE*)ud);
}

/* The file at @path and its length, or NULL */
static RFILE *data_transfer_file_open(const char *path, size_t *len)
{
   int64_t l;
   RFILE  *f;
   if (!path || !(f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return NULL;
   l = filestream_get_size(f);
   if (l <= 0 || (uint64_t)l > SIZE_MAX)
   {
      filestream_close(f);
      return NULL;
   }
   *len = (size_t)l;
   return f;
}

data_transfer_t *data_transfer_open_prefix(const char *path,
      size_t commit_cap)
{
   data_transfer_t *dt;
   size_t           len = 0;
   RFILE           *f   = data_transfer_file_open(path, &len);
   if (!f)
      return NULL;
   if (!(dt = data_transfer_open_prefix_reader(len, commit_cap,
               data_transfer_file_read_at, f)))
   {
      filestream_close(f);
      return NULL;
   }
   data_transfer_set_release(dt, data_transfer_file_release);
   return dt;
}

data_transfer_t *data_transfer_open_window(const char *path, size_t keep)
{
   data_transfer_t *dt;
   size_t           len = 0;
   RFILE           *f   = data_transfer_file_open(path, &len);
   if (!f)
      return NULL;
   if (!(dt = data_transfer_open_window_reader(len, keep,
               data_transfer_file_read_at, f)))
   {
      filestream_close(f);
      return NULL;
   }
   data_transfer_set_release(dt, data_transfer_file_release);
   return dt;
}

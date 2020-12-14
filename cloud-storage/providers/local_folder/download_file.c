/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (download_file.c).
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

#include <boolean.h>

#include <streams/file_stream.h>

#include "../../cloud_storage.h"
#include "local_folder_internal.h"

#define BUFFER_SIZE 8192

bool cloud_storage_local_folder_download_file(
   cloud_storage_item_t *file_to_download,
   char *local_file)
{
   RFILE *src_file;
   RFILE *dest_file;
   uint8_t buffer[BUFFER_SIZE];
   uint8_t bytes_read;

   src_file = filestream_open(file_to_download->id, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!src_file)
   {
      return false;
   }

   dest_file = filestream_open(local_file, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!dest_file) {
      filestream_close(src_file);
      return false;
   }

   bytes_read = filestream_read(src_file, buffer, BUFFER_SIZE);
   while (bytes_read > 0) {
      filestream_write(dest_file, buffer, bytes_read);
      bytes_read = filestream_read(src_file, buffer, BUFFER_SIZE);
   }

   filestream_close(src_file);
   filestream_close(dest_file);

   return true;
}

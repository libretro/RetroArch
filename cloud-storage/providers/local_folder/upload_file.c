/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (upload_file.c).
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

#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../cloud_storage.h"
#include "local_folder_internal.h"

#define BUFFER_SIZE 8192

bool cloud_storage_local_folder_upload_file(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file,
   char *local_file)
{
   RFILE *src_file;
   char *dest_filename;
   RFILE *dest_file;
   uint8_t buffer[BUFFER_SIZE];
   uint8_t bytes_read;

   src_file = filestream_open(local_file, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!src_file)
   {
      return false;
   }

   dest_filename = pathname_join(remote_dir->id, remote_file->name);
   dest_file = filestream_open(dest_filename, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!dest_file) {
      free(dest_filename);
      filestream_close(src_file);
      return false;
   }
   free(dest_filename);

   bytes_read = filestream_read(src_file, buffer, BUFFER_SIZE);
   while (bytes_read > 0) {
      filestream_write(dest_file, buffer, bytes_read);
      bytes_read = filestream_read(src_file, buffer, BUFFER_SIZE);
   }

   filestream_close(src_file);
   filestream_close(dest_file);

   return true;
}

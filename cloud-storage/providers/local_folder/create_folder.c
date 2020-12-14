/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (create_folder.c).
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

#include <stdlib.h>
#include <time.h>

#include <file/file_path.h>

#include "../../cloud_storage.h"
#include "local_folder_internal.h"

cloud_storage_item_t *cloud_storage_local_folder_create_folder(const char *folder_name)
{
   char *absolute_path;
   const char *base_dir;
   cloud_storage_item_t *metadata;

   base_dir = cloud_storage_local_folder_get_base_dir();
   absolute_path = pathname_join(base_dir, folder_name);

   if (!path_is_directory(absolute_path) && !path_mkdir(absolute_path)) {
      free(absolute_path);
      return NULL;
   }

   metadata = (cloud_storage_item_t *)calloc(sizeof(cloud_storage_item_t), 1);
   metadata->id = absolute_path;
   metadata->item_type = CLOUD_STORAGE_FOLDER;
   metadata->last_sync_time = time(NULL);
   metadata->name = strdup(folder_name);

   return metadata;
}

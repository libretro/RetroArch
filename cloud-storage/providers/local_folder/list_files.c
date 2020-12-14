/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (list_files.c).
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
#include <string.h>
#include <time.h>

#include <file/file_path.h>
#include <retro_dirent.h>

#include "../../provider_common.h"
#include "local_folder_internal.h"

static cloud_storage_item_t *_get_file_metadata(const char *folder_path, const char *filename)
{
   char *absolute_path;
   cloud_storage_item_t *metadata;

   absolute_path = pathname_join(folder_path, filename);

   metadata = (cloud_storage_item_t *)calloc(sizeof(cloud_storage_item_t), 1);
   metadata->id = absolute_path;
   metadata->name = strdup(filename);
   metadata->item_type = CLOUD_STORAGE_FILE;
   metadata->type_data.file.hash_type = CLOUD_STORAGE_HASH_MD5;
   metadata->type_data.file.hash_value = cloud_storage_get_md5_hash(absolute_path);
   metadata->last_sync_time = time(NULL);

   return metadata;
}

void cloud_storage_local_folder_list_files(cloud_storage_item_t *folder)
{
   RDIR *dir;
   cloud_storage_item_t *last_item = NULL;

   if (folder->type_data.folder.children)
   {
      cloud_storage_item_free(folder->type_data.folder.children);
      folder->type_data.folder.children = NULL;
   }

   dir = retro_opendir(folder->id);
   if (!dir)
   {
      return;
   }

   while (retro_readdir(dir))
   {
      const char *filename;
      cloud_storage_item_t *new_item;

      filename = retro_dirent_get_name(dir);
      if (strcmp(filename, ".") && strcmp(filename, ".."))
      {
         new_item = _get_file_metadata(folder->id, filename);

         if (!folder->type_data.folder.children)
         {
            folder->type_data.folder.children = new_item;
            last_item = new_item;
         } else
         {
            last_item->next = new_item;
            last_item = new_item;
         }
      }
   }

   retro_closedir(dir);
}

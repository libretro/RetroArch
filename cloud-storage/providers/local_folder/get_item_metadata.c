/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (get_item_metadata.c).
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
#include <lists/linked_list.h>

#include "../../provider_common.h"
#include "local_folder_internal.h"

/* Get the metadata for a folder in the local storage folder. Return NULL if it
 * doesn't exist.
 * id attribute is the absolute path
 */
cloud_storage_item_t *cloud_storage_local_folder_get_folder_metadata(const char *folder_name)
{
   char *absolute_path;
   const char *base_dir;
   cloud_storage_item_t *metadata;

   base_dir = cloud_storage_local_folder_get_base_dir();
   absolute_path = pathname_join(base_dir, folder_name);

   if (!path_is_directory(absolute_path)) {
      free(absolute_path);
      return NULL;
   }

   /* Create the metadata */
   metadata = (cloud_storage_item_t *)calloc(sizeof(cloud_storage_item_t), 1);
   metadata->id = absolute_path;
   metadata->item_type = CLOUD_STORAGE_FOLDER;
   metadata->name = strdup(folder_name);
   metadata->last_sync_time = time(NULL);
   metadata->type_data.folder.children = linked_list_new();

   return metadata;
}

/* Get the metadata for a file in the local storage folder. Return NULL if it
 * doesn't exist. The file is identified from its metadata.
 * id attribute is the absolute path
 */
cloud_storage_item_t *cloud_storage_local_folder_get_file_metadata(cloud_storage_item_t *file)
{
   cloud_storage_item_t *metadata;

   if (!path_is_valid(file->id)) {
      return NULL;
   }

   /* Create the metadata */
   metadata = (cloud_storage_item_t *)calloc(sizeof(cloud_storage_item_t), 1);
   metadata->id = strdup(file->id);
   metadata->name = strdup(file->name);
   metadata->item_type = CLOUD_STORAGE_FILE;
   metadata->type_data.file.hash_type = CLOUD_STORAGE_HASH_MD5;
   metadata->type_data.file.hash_value = cloud_storage_get_md5_hash(metadata->id);
   metadata->last_sync_time = time(NULL);

   return metadata;
}

/* Get the metadata for a file in the local storage folder. Return NULL if it
 * doesn't exist. The file is identified from its parent folder's metadata
 * and filename.
 * id attribute is the absolute path
 */
cloud_storage_item_t *cloud_storage_local_folder_get_file_metadata_by_name(
   cloud_storage_item_t *folder,
   char *filename)
{
   char *absolute_path;
   const char *base_dir;
   cloud_storage_item_t *metadata;
   base_dir = cloud_storage_local_folder_get_base_dir();
   absolute_path = pathname_join(base_dir, filename);

   if (!path_is_valid(absolute_path)) {
      free(absolute_path);
      return NULL;
   }

   /* Create the metadata */
   metadata = (cloud_storage_item_t *)calloc(sizeof(cloud_storage_item_t), 1);
   metadata->id = absolute_path;
   metadata->item_type = CLOUD_STORAGE_FILE;
   metadata->name = strdup(filename);
   metadata->last_sync_time = time(NULL);

   metadata->type_data.file.hash_type = CLOUD_STORAGE_HASH_MD5;
   metadata->type_data.file.hash_value= cloud_storage_get_md5_hash(metadata->id);

   return metadata;
}

/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (local_folder.c).
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

#include <file/file_path.h>
#include <lrc_hash.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>

#include "../cloud_storage.h"
#include "local_folder.h"

#define BUFFER_SIZE 8192

static char *_base_dir = NULL;

/* Ready if the base folder for local folder provider exists */
static bool _ready_for_request(void)
{
   return _base_dir != NULL;
}

/* Get the base directory for the local folder provider. */
static const char *_cloud_storage_local_folder_get_base_dir()
{
   return _base_dir;
}

/* Create a new metadata structure for file in the local folder directory. */
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
   MD5_calculate(absolute_path, metadata->type_data.file.hash_value);
   metadata->last_sync_time = time(NULL);

   return metadata;
}

/* Release the memory for a cloud_storage_item_t. Use this function with
 * linked_list_free().
 */
static void _free_file_list(void *file)
{
   cloud_storage_item_free((cloud_storage_item_t *)file);
}

/* Add the metadata for the files in the local folder directory to the
 * folder metadata.
 */
static void _cloud_storage_local_folder_list_files(cloud_storage_item_t *folder)
{
   RDIR *dir;
   cloud_storage_item_t *last_item = NULL;

   if (folder->type_data.folder.children)
   {
      linked_list_free(folder->type_data.folder.children, &_free_file_list);
      folder->type_data.folder.children = linked_list_new();
   }

   dir = retro_opendir(folder->id);
   if (!dir)
   {
      return;
   }

   /* Read the directory and add each file as a child of the folder */
   while (retro_readdir(dir))
   {
      const char *filename;
      cloud_storage_item_t *new_item;

      filename = retro_dirent_get_name(dir);
      if (strcmp(filename, ".") && strcmp(filename, ".."))
      {
         linked_list_add(folder->type_data.folder.children, _get_file_metadata(folder->id, filename));
      }
   }

   retro_closedir(dir);
}

/* Copy a file from local storage folder to the corresponding RetroArch folder. */
static bool _cloud_storage_local_folder_download_file(
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

   /* Copy the file 8k bytes at a time */
   bytes_read = filestream_read(src_file, buffer, BUFFER_SIZE);
   while (bytes_read > 0) {
      filestream_write(dest_file, buffer, bytes_read);
      bytes_read = filestream_read(src_file, buffer, BUFFER_SIZE);
   }

   filestream_close(src_file);
   filestream_close(dest_file);

   return true;
}

/* Copy a file from a RetroArch directory to a folder in the local folder provider
 * base directory. Tries to create the destination file if it doesn't exist.
 */
static bool _cloud_storage_local_folder_upload_file(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file,
   char *local_file)
{
   RFILE *src_file;
   char *dest_filename;
   RFILE *dest_file;
   uint8_t buffer[BUFFER_SIZE];
   uint32_t bytes_read;

   src_file = filestream_open(local_file, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!src_file)
   {
      return false;
   }

   /* Try to open the destination file, creating if needed */
   dest_filename = pathname_join(remote_dir->id, remote_file->name);
   dest_file = filestream_open(dest_filename, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!dest_file) {
      free(dest_filename);
      filestream_close(src_file);
      return false;
   }

   /* Copy the file contents over 8k at a time */
   do {
      bytes_read = filestream_read(src_file, buffer, BUFFER_SIZE);
      if (bytes_read > 0) {
         filestream_write(dest_file, buffer, bytes_read);
      }
   } while (bytes_read > 0);

   filestream_close(src_file);
   filestream_close(dest_file);

   if (remote_file->id)
      free(remote_file->id);
   remote_file->id = dest_filename;

   return true;
}

/* Get the metadata for a folder in the local storage folder. Return NULL if it
 * doesn't exist.
 * id attribute is the absolute path
 */
static cloud_storage_item_t *_cloud_storage_local_folder_get_folder_metadata(const char *folder_name)
{
   char *absolute_path;
   const char *base_dir;
   cloud_storage_item_t *metadata;

   base_dir = _cloud_storage_local_folder_get_base_dir();
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
static cloud_storage_item_t *_cloud_storage_local_folder_get_file_metadata(cloud_storage_item_t *file)
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
   MD5_calculate(metadata->id, metadata->type_data.file.hash_value);
   metadata->last_sync_time = time(NULL);

   return metadata;
}

/* Get the metadata for a file in the local storage folder. Return NULL if it
 * doesn't exist. The file is identified from its parent folder's metadata
 * and filename.
 * id attribute is the absolute path
 */
static cloud_storage_item_t *_cloud_storage_local_folder_get_file_metadata_by_name(
   cloud_storage_item_t *folder,
   char *filename)
{
   char *absolute_path;
   const char *base_dir;
   cloud_storage_item_t *metadata;
   base_dir = _cloud_storage_local_folder_get_base_dir();
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
   MD5_calculate(metadata->id, metadata->type_data.file.hash_value);

   return metadata;
}

/* Delete the local file. */
static bool _cloud_storage_local_folder_delete_file(cloud_storage_item_t *file)
{
   filestream_delete(file->id);

   return true;
}

/* Create the metadata for a local folder. If the doesn't exist create it.
 * Return NULL if unable create the folder.
 * id attribute is the absolute path.
 */
static cloud_storage_item_t *_cloud_storage_local_folder_create_folder(const char *folder_name)
{
   char *absolute_path;
   const char *base_dir;
   cloud_storage_item_t *metadata;

   base_dir = _cloud_storage_local_folder_get_base_dir();
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
   metadata->type_data.folder.children = linked_list_new();

   return metadata;
}

/* Create the storage provider structure for the local folder provider. */
cloud_storage_provider_t *cloud_storage_local_folder_create(void)
{
   cloud_storage_provider_t *provider;

   provider = (cloud_storage_provider_t *)malloc(sizeof(cloud_storage_provider_t));

   /* Assign the function pointers */
   provider->ready_for_request = _ready_for_request;
   provider->list_files = _cloud_storage_local_folder_list_files;
   provider->download_file = _cloud_storage_local_folder_download_file;
   provider->upload_file = _cloud_storage_local_folder_upload_file;
   provider->get_folder_metadata = _cloud_storage_local_folder_get_folder_metadata;
   provider->get_file_metadata = _cloud_storage_local_folder_get_file_metadata;
   provider->get_file_metadata_by_name = _cloud_storage_local_folder_get_file_metadata_by_name;
   provider->delete_file = _cloud_storage_local_folder_delete_file;
   provider->create_folder = _cloud_storage_local_folder_create_folder;

   /* Create the base directory if it doesn't exist */
   if (!_base_dir)
   {
      _base_dir = getenv("RETRO_CLOUD_STORAGE_TEST");
      if (!_base_dir)
      {
         char *tmpdir;
         size_t tmpdir_len;

         /* Will create in the temp directory. Try to identify an appropriate
          * directory to use for the temp directory.
          */
         tmpdir = getenv("TMPDIR");
         if (!tmpdir){
            tmpdir = getenv("TMP");
         }

         if (!tmpdir)
         {
            if (path_is_directory("/tmp"))
            {
               tmpdir = (char *)"/tmp";
            }
         } else if (!path_is_directory(tmpdir))
         {
            tmpdir = NULL;
         }

         if (tmpdir)
         {
            /* Found a temp directory to use */
            _base_dir = pathname_join(tmpdir, "retro_cloud_storage_test");
            if (!path_is_directory(_base_dir) && !path_mkdir(_base_dir)) {
               free(_base_dir);
               _base_dir = NULL;
            }
         }
      }
   }

   return provider;
}

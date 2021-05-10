/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cloud_storage.c).
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

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include <file/file_path.h>
#include <retroarch.h>
#include <retro_dirent.h>
#include <utils/md5.h>
#include <lists/linked_list.h>
#include <queues/generic_queue.h>
#include <rthreads/rthreads.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#include "cloud_storage.h"
#include "provider_common.h"
#include "providers/local_folder/local_folder.h"

#define BUFFER_SIZE 8192

enum _operation_type_t
{
   SYNC_FILES,
   UPLOAD_FILE,
   DOWNLOAD_FILE
};

struct _operation_t
{
   enum _operation_type_t operation_type;

   union
   {
      struct
      {
         folder_type_t folder_type;
      } sync;

      struct
      {
         folder_type_t folder_type;
         char *local_file;
      } upload;

      struct
      {
         folder_type_t folder_type;
         cloud_storage_item_t *remote_file;
      } download;
   } parameters;
};

static bool _shutdown = true;
static cloud_storage_provider_t *_provider;

static char _game_states_config_value[8192] = "";

static bool _need_sync_runtime_logs = false;

cloud_storage_item_t *_game_states = NULL;

static generic_queue_t *_operation_queue = NULL;
static slock_t *_operation_queue_mutex;

static scond_t *_operation_condition;
static sthread_t *_operation_thread;

static void _sync_files_execute(folder_type_t folder_type);

static void _upload_file_execute(folder_type_t folder_type, char *file_name);

void _cloud_storage_item_free_fn(void *item)
{
   cloud_storage_item_free((cloud_storage_item_t *) item);
}

void cloud_storage_item_free(cloud_storage_item_t *item)
{
   while (item)
   {
      free(item->id);
      free(item->name);

      if (item->item_type == CLOUD_STORAGE_FILE)
      {
         if (item->type_data.file.hash_value)
         {
            free(item->type_data.file.hash_value);
         }
         if (item->type_data.file.download_url)
         {
            free(item->type_data.file.download_url);
         }
      }

      if (item->item_type == CLOUD_STORAGE_FOLDER)
      {
         linked_list_free(item->type_data.folder.children, &_cloud_storage_item_free_fn);
      }

      free(item);
   }
}

static void _free_operation(struct _operation_t *operation)
{
   switch (operation->operation_type)
   {
      case UPLOAD_FILE:
         if (operation->parameters.upload.local_file)
         {
            free(operation->parameters.upload.local_file);
         }
         break;
      case DOWNLOAD_FILE:
         if (operation->parameters.download.remote_file)
         {
            cloud_storage_item_free(operation->parameters.download.remote_file);
         }
         break;
      default:
         break;
   }

   free(operation);
}

static void _free_operation_fn(void *operation)
{
   _free_operation((struct _operation_t *) operation);
}

static void _operation_thread_loop(void *user_data)
{
   while (!_shutdown)
   {
      struct _operation_t *operation;

      /* In the critical section:
       * - Go back to waiting if the opertion queue is empty
       * - Break out of the loop and shutdown the operation thread if
       *   _shutdown is true
       * - Pop an operation off of the queue and release the lock
       */
      slock_lock(_operation_queue_mutex);
      if (generic_queue_length(_operation_queue) == 0)
      {
         scond_wait(_operation_condition, _operation_queue_mutex);
      }

      if (_shutdown)
      {
         slock_unlock(_operation_queue_mutex);
         continue;
      }

      operation = (struct _operation_t *)generic_queue_pop(_operation_queue);
      slock_unlock(_operation_queue_mutex);

      if (operation)
      {
         switch (operation->operation_type)
         {
            case SYNC_FILES:
               _sync_files_execute(operation->parameters.sync.folder_type);
               break;
            case UPLOAD_FILE:
               _upload_file_execute(operation->parameters.upload.folder_type, operation->parameters.upload.local_file);
               break;
            case DOWNLOAD_FILE:
               break;
         }

         _free_operation(operation);
      }
   }

   /* Broke out of the operation loop, will shutdown the operation thread */
   if (_operation_queue)
   {
      generic_queue_free(_operation_queue, &_free_operation_fn);
   }
}

void cloud_storage_init(void)
{
   struct _operation_queue_item *sync_item;

   if (!_shutdown)
   {
      return;
   }

   _provider = cloud_storage_local_folder_create();

   _operation_queue = generic_queue_new();
   _operation_queue_mutex = slock_new();
   _operation_condition = scond_new();

   _shutdown = false;
   _operation_thread = sthread_create(_operation_thread_loop, NULL);

   if (_provider->ready_for_request())
   {
      cloud_storage_sync_files();
   }
}

void cloud_storage_shutdown(void)
{
   if (_shutdown)
   {
      return;
   }

   /* Signal the operation thread to shutdown in a critical section */
   slock_lock(_operation_queue_mutex);
   _shutdown = true;
   scond_signal(_operation_condition);
   slock_unlock(_operation_queue_mutex);

   sthread_join(_operation_thread);
}

char *cloud_storage_get_md5_hash(char *absolute_filename)
{
   RFILE *file;
   uint8_t buffer[BUFFER_SIZE];
   MD5_CTX md5_ctx;
   int64_t bytes_read;
   unsigned char *checksum;
   char *checksum_str;

   file = filestream_open(absolute_filename, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
   {
      return NULL;
   }

   MD5_Init(&md5_ctx);

   /* Pass the contents of the local file through the MD5 hash function */
   bytes_read = filestream_read(file, buffer, BUFFER_SIZE);
   while (bytes_read > 0)
   {
      MD5_Update(&md5_ctx, buffer, bytes_read);
      bytes_read = filestream_read(file, buffer, BUFFER_SIZE);
   }

   filestream_close(file);
   checksum = (unsigned char *)calloc(16, sizeof(char));
   MD5_Final(checksum, &md5_ctx);

   /* Convert the MD5 checksum to a hex string */
   checksum_str = bytes_to_hex_str(checksum, 16);
   free(checksum);

   return checksum_str;
}

/* Converts a value (game save/state directory setting) to an absolute directory path */
static char *_config_value_to_directory(const char *value)
{
   char *dir;
   int i;

   dir = strdup(value);
   path_basedir(dir);
   for (i = strlen(dir) - 1; i > 0; i++)
   {
      if (dir[i] == '/')
      {
         dir[i] = '\0';
      } else {
         break;
      }
   }

   return dir;
}

/* Generate the local absolute filename from the folder category and filename */
static char *_get_absolute_filename(folder_type_t folder_type, char *filename)
{
   global_t *global;
   char *raw_folder;
   char *folder;
   char *absolute_filename;

   global = global_get_ptr();
   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_STATES:
         folder = _config_value_to_directory(global->name.savestate);
         break;
      default:
         return NULL;
   }

   absolute_filename = pathname_join(folder, filename);

   free(folder);

   return absolute_filename;
}

/**
 * Retrieve the metadata for a storage provider file that corresponds to
 * a local file.
 *
 * @param folder_type folder category for the local file
 * @param local_file name of the local file
 * @return metadata for the storage provider file
 */
static cloud_storage_item_t *_get_remote_file_for_local_file(
   folder_type_t folder_type,
   char *local_file)
{
   cloud_storage_item_t *folder;
   linked_list_iterator_t *iterator;
   cloud_storage_item_t *file = NULL;

   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_STATES:
         folder = _game_states;
         break;
      default:
         return NULL;
   }

   if (!folder)
   {
      return NULL;
   }

   /* Look for the file among the immediate children */
   iterator = linked_list_iterator(folder->type_data.folder.children, true);
   while (iterator)
   {
      file = (cloud_storage_item_t *)linked_list_iterator_value(iterator);
      if (file->item_type == CLOUD_STORAGE_FILE && !strcmp(local_file, file->name))
      {
         break;
      }

      iterator = linked_list_iterator_next(iterator);
   }

   if (iterator)
   {
      linked_list_iterator_free(iterator);
   }

   return file;
}

/* Check if a file needs to be uploaded
 * - Storage provider does not have a corresponding file
 * - The checksums are different
 */
static bool _file_need_upload(folder_type_t folder_type, char *local_file, cloud_storage_item_t *remote_file)
{
   char *checksum_str = NULL;
   bool need_upload;

   if (!remote_file)
   {
      return true;
   }

   if (!local_file || path_is_directory(local_file))
   {
      return false;
   }

   switch (remote_file->type_data.file.hash_type)
   {
      case CLOUD_STORAGE_HASH_MD5:
         checksum_str = cloud_storage_get_md5_hash(local_file);
         break;
   }

   need_upload = strcmp(checksum_str, remote_file->type_data.file.hash_value) != 0;
   free(checksum_str);
   return need_upload;
}

/**
 * @brief Copies a local file to the storage provider
 *
 * Copy a local file to a corresponding file in the storage provider.
 *
 * @param folder_type folder category
 * @param local_file absolute path of the local file to copy
 * @param remote_file metadata for the file in the storage provider
 */
static void _upload_file(folder_type_t folder_type, char *local_file, cloud_storage_item_t *remote_file)
{
   cloud_storage_item_t *remote_folder;
   bool new_file = false;

   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_STATES:
         remote_folder = _game_states;
         break;
      default:
         return;
   }

   if (!local_file || path_is_directory(local_file))
   {
      return;
   }

   /* If not in storage provider, make a new metadata structure */
   if (!remote_file)
   {
      remote_file = (cloud_storage_item_t *)calloc(1, sizeof(cloud_storage_item_t));
      remote_file->item_type = CLOUD_STORAGE_FILE;
      remote_file->name = strdup(find_last_slash(local_file) + 1);
      new_file = true;
   }

   /* If the upload succeeded, update teh checksum and last sync time */
   if (_provider->upload_file(
      remote_folder,
      remote_file,
      local_file))
   {
      if (!remote_file->type_data.file.hash_value) {
         switch (remote_file->type_data.file.hash_type)
         {
            case CLOUD_STORAGE_HASH_MD5:
               remote_file->type_data.file.hash_value = (char *)cloud_storage_get_md5_hash(local_file);
               break;
         }
      }

      remote_file->last_sync_time = time(NULL);

      /* If new, add to the parent folder */
      if (new_file)
      {
         linked_list_add(remote_folder->type_data.folder.children, remote_file);
      }
   }
}

/* Copy the metadata from one structure to another */
static void _update_existing_item(cloud_storage_item_t *item_to_update, cloud_storage_item_t *other)
{
   if (item_to_update->name)
   {
      free(item_to_update->name);
      item_to_update->name = NULL;
   }

   if (other->name)
   {
      item_to_update->name = strdup(other->name);
   }

   /* Copy the file attributes */
   if (item_to_update->item_type == CLOUD_STORAGE_FILE)
   {
      if (item_to_update->type_data.file.download_url)
      {
         free(item_to_update->type_data.file.download_url);
         item_to_update->type_data.file.download_url = NULL;
      }

      if (item_to_update->type_data.file.hash_value)
      {
         free(item_to_update->type_data.file.hash_value);
         item_to_update->type_data.file.hash_value = NULL;
      }
   }

   item_to_update->item_type = other->item_type;

   /* Copy the folder attributes */
   if (other->item_type == CLOUD_STORAGE_FILE)
   {
      if (other->type_data.file.download_url)
      {
         item_to_update->type_data.file.download_url = strdup(other->type_data.file.download_url);
      }

      item_to_update->type_data.file.hash_type = other->type_data.file.hash_type;

      if (other->type_data.file.hash_value)
      {
         item_to_update->type_data.file.hash_value = strdup(other->type_data.file.hash_value);
      }
   } else
   {
      item_to_update->type_data.folder.children = other->type_data.folder.children;
      /* Set other to have a new empty list of children so that it can be freed */
      other->type_data.folder.children = linked_list_new();
   }
}

/* Iterate over the files in a local folder category. Find any files that
 * need to be uploaded and upload them to the storage provider.
 */
static void _sync_files_upload(folder_type_t folder_type)
{
   global_t *global;
   cloud_storage_item_t *remote_folder;
   char *raw_dir;
   char *dir_name;
   struct RDIR *dir;
   char *filename;

   global = global_get_ptr();
   if (!global)
   {
      return;
   }

   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_STATES:
         remote_folder = _game_states;
         raw_dir = global->name.savestate;
         break;
      default:
         return;
   }

   if (!remote_folder || strlen(raw_dir) == 0)
   {
      return;
   }

   dir_name = strdup(raw_dir);
   if (strrchr(dir_name, '.') > strrchr(dir_name, '/'))
   {
      path_basedir(dir_name);
   }

   /* Iterate over the local files */
   dir = retro_opendir(dir_name);
   while (dir != NULL && retro_readdir(dir))
   {
      char *filename = (char *)retro_dirent_get_name(dir);
      char *absolute_filename;

      absolute_filename = pathname_join(dir_name, filename);

      if (strcmp(filename, ".") && strcmp(filename, ".."))
      {
         cloud_storage_item_t *remote_file;

         /* Look for a matching file in the storage provider */
         remote_file = _get_remote_file_for_local_file(folder_type, filename);
         if (remote_file && remote_file->item_type == CLOUD_STORAGE_FOLDER)
         {
            continue;
         } else if (remote_file)
         {
            cloud_storage_item_t *current_metadata;

            /* If the metadata is at least 30s old, consider it stale and refresh */
            if (remote_file->last_sync_time < time(NULL) - 30)
            {
               current_metadata = _provider->get_file_metadata(remote_file);
               _update_existing_item(remote_file, current_metadata);
               cloud_storage_item_free(current_metadata);
            }
         }

         /* Upload the file if necessary */
         if (_file_need_upload(folder_type, absolute_filename, remote_file))
         {
            _upload_file(folder_type, absolute_filename, remote_file);
         }
      }

      free(absolute_filename);
   }

   free(dir_name);
}

/* Check for a local file that corresponds to a file from the storage provider */
static bool _have_local_file_for_remote_file(
   folder_type_t folder_type,
   cloud_storage_item_t *remote_file)
{
   global_t *global;
   char *dir_name = NULL;
   struct RDIR *dir;
   bool found = false;

   global = global_get_ptr();
   if (!global)
   {
      return false;
   }

   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_STATES:
         if (strlen(global->name.savestate) > 0)
         {
            dir_name = strdup(global->name.savestate);
         }
         break;
      default:
         return false;
   }

   if (!dir_name)
   {
      return false;
   }

   path_basedir(dir_name);

   /* Check the local files in the folder category for a matching file */
   dir = retro_opendir(dir_name);
   while (dir != NULL && retro_readdir(dir))
   {
      char *filename = (char *)retro_dirent_get_name(dir);
      if (strcmp(filename, ".") && strcmp(filename, "..") && !strcmp(filename, remote_file->name))
      {
         found = true;
         break;
      }
   }

   free(dir_name);
   return found;
}

/* Check if a file in the storage provider needs to be copied to a local file
 * - Local file does not exist
 * - Checksums are different
 */
static bool _file_need_download(char *local_file, cloud_storage_item_t *remote_file)
{
   char *checksum_str = NULL;
   bool need_download;
   char *raw_dir;

   if (!path_is_valid(local_file))
   {
      return true;
   }

   if (path_is_directory(local_file))
   {
      return false;
   }

   switch (remote_file->type_data.file.hash_type)
   {
      case CLOUD_STORAGE_HASH_MD5:
         checksum_str = cloud_storage_get_md5_hash(local_file);
         break;
   }

   need_download = strcmp(checksum_str, remote_file->type_data.file.hash_value) != 0;
   free(checksum_str);
   return need_download;
}

/* Copy a file from the storage provider to a local file */
static void _download_file(char *local_file, cloud_storage_item_t *remote_file)
{
   _provider->download_file(
      remote_file,
      local_file
   );
}

/* Iterate over the files in the storage provider for a given folder category
 * and download any files that need downloading.
 */
static void _sync_files_download(folder_type_t folder_type)
{
   cloud_storage_item_t *remote_folder;
   cloud_storage_item_t *remote_file;
   linked_list_iterator_t *iterator;

   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_STATES:
         remote_folder = _game_states;
         break;
      default:
         return;
   }

   if (!remote_folder)
   {
      return;
   }

   /* Iterate over the remote files */
   iterator = linked_list_iterator(remote_folder->type_data.folder.children, true);
   while (iterator)
   {
      char *local_file;
      bool need_download = false;
      cloud_storage_item_t *current_metadata;

      remote_file = (cloud_storage_item_t *)linked_list_iterator_value(iterator);

      /* If the metadata is at least 30s old, consider it stale and refresh */
      if (remote_file->last_sync_time < time(NULL) - 30)
      {
         current_metadata = _provider->get_file_metadata(remote_file);
         if (!current_metadata)
         {
            iterator = linked_list_iterator_next(iterator);
            continue;
         }
         _update_existing_item(remote_file, current_metadata);
         cloud_storage_item_free(current_metadata);
      }

      local_file = _get_absolute_filename(folder_type, remote_file->name);

      /* Check if storage provider file needs to be downloaded */
      if (!local_file)
      {
         need_download = false;
      } else if (_have_local_file_for_remote_file(folder_type, remote_file))
      {
         need_download = _file_need_download(local_file, remote_file);
      } else
      {
         need_download = true;
      }

      /* Download the file */
      if (need_download)
      {
         _download_file(local_file, remote_file);
      }

      iterator = linked_list_iterator_next(iterator);
   }
}

/* For a folder category, create the folder in the storage provider if missing.
 * If it was present already, get the list of files in the folder.
 */
static bool _prepare_folder(folder_type_t folder_type)
{
   const char *folder_name;
   cloud_storage_item_t **folder;

   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_STATES:
         folder_name = GAME_STATES_FOLDER_NAME;
         folder = &_game_states;
         break;
      default:
         return false;
   }

   if (*folder == NULL)
   {
      /* Need to try to get the metadata from the storage provider */
      *folder = _provider->get_folder_metadata(folder_name);
      if (*folder)
      {
         _provider->list_files(*folder);
         return true;
      } else
      {
         /* Folder didn't exist, so create it */
         *folder = _provider->create_folder(folder_name);
         return *folder != NULL;
      }
   } else
   {
      return true;
   }
}

/* Peforms the file sync operations (upload and download). */
static void _sync_files_execute(folder_type_t folder_type)
{
   if (!_prepare_folder(folder_type))
   {
      return;
   }

   _sync_files_upload(folder_type);
   _sync_files_download(folder_type);
}

/* Adds a new file sync operation to the operation queue. */
void cloud_storage_sync_files(void)
{
   bool sync_states;
   global_t *global = global_get_ptr();
   char *raw_folder;
   int i;
   generic_queue_iterator_t *iterator;
   struct _operation_t *operation;

   raw_folder = _config_value_to_directory(global->name.savestate);
   sync_states = strcmp(_game_states_config_value, raw_folder) != 0;

   if (!sync_states)
   {
      /* Only want to sync if the folder has changed */
      return;
   } else
   {
      strcpy(_game_states_config_value, global->name.savestate);
   }

   slock_lock(_operation_queue_mutex);

   /* Ignore the sync operation if the storage provider is not ready */
   if (!_provider->ready_for_request())
   {
      slock_unlock(_operation_queue_mutex);
      return;
   }

   /* Remove any pending upload and download operations, since a sync will upload and
    * download all files/
    */
   iterator = generic_queue_iterator(_operation_queue, true);
   while (iterator) {
      struct _operation_t *operation;
      
      operation = (struct _operation_t *)generic_queue_iterator_value(iterator);
      if (operation->operation_type == UPLOAD_FILE || operation->operation_type == DOWNLOAD_FILE)
      {
         iterator = generic_queue_iterator_remove(iterator);
         _free_operation(operation);
      } else
      {
         iterator = generic_queue_iterator_next(iterator);
      }
   }

   /* Create the new sync operation and add it to the operation queue */
   operation = (struct _operation_t *)calloc(1, sizeof(struct _operation_t));
   operation->operation_type = SYNC_FILES;
   operation->parameters.sync.folder_type = CLOUD_STORAGE_GAME_STATES;
   generic_queue_push(_operation_queue, operation);

   scond_signal(_operation_condition);
   slock_unlock(_operation_queue_mutex);
}

/* Performs a copy of a local file to the storage provider */
static void _upload_file_execute(folder_type_t folder_type, char *file_name)
{
   cloud_storage_item_t *remote_file;

   if (!_prepare_folder(folder_type))
   {
      return;
   }

   remote_file = _get_remote_file_for_local_file(folder_type, find_last_slash(file_name) + 1);
   if (_file_need_upload(folder_type, file_name, remote_file))
   {
      _upload_file(folder_type, file_name, remote_file);
   }
}

/* Adds a new file upload operation to the operation queue. */
void cloud_storage_upload_file(folder_type_t folder_type, char *file_name)
{
   struct _operation_t *operation;
   generic_queue_iterator_t *iterator;

   slock_lock(_operation_queue_mutex);

   if (!_provider->ready_for_request())
   {
      slock_unlock(_operation_queue_mutex);
      return;
   }

   /* Look for any pending sync or matching upload operations. If any are found, do not add this operation. */
   iterator = generic_queue_iterator(_operation_queue, true);
   while (iterator)
   {
      operation = (struct _operation_t *)generic_queue_iterator_value(iterator);
      switch (operation->operation_type)
      {
         case SYNC_FILES:
            generic_queue_iterator_free(iterator);
            goto complete;
         case UPLOAD_FILE:
            if (operation->parameters.upload.folder_type == folder_type && !strcmp(file_name, operation->parameters.upload.local_file))
            {
               generic_queue_iterator_free(iterator);
               goto complete;
            }
            break;
         default:
            break;
      }

      iterator = generic_queue_iterator_next(iterator);
   }

   /* Create the new upload operation and add it to the operation queue */
   operation = (struct _operation_t *)calloc(1, sizeof(struct _operation_t));
   operation->operation_type = UPLOAD_FILE;
   operation->parameters.sync.folder_type = folder_type;
   operation->parameters.upload.local_file = strdup(file_name);
   generic_queue_push(_operation_queue, operation);

   scond_signal(_operation_condition);

complete:
   slock_unlock(_operation_queue_mutex);
}

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
#include <rthreads/rthreads.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#include "cloud_storage.h"
#include "provider_common.h"
#include "providers/local_folder/local_folder.h"

#define BUFFER_SIZE 8192

enum _operation_queue_item_type_t
{
   SYNC_FILES,
   UPLOAD_FILE,
   DOWNLOAD_FILE
};

struct _operation_queue_item;
struct _operation_queue_item
{
   enum _operation_queue_item_type_t operation;

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
   } operation_parameters;

   struct _operation_queue_item *next;
};

static bool _shutdown = true;
static cloud_storage_provider_t *_provider;

static char _game_states_config_value[8192] = "";

static bool _need_sync_runtime_logs = false;

cloud_storage_item_t *_game_states = NULL;

static struct _operation_queue_item *_operation_queue = NULL;
static slock_t *_operation_queue_mutex;

static scond_t *_operation_condition;
static sthread_t *_operation_thread;

static void _sync_files_execute(folder_type_t folder_type);

static void _upload_file_execute(folder_type_t folder_type, char *file_name);

void cloud_storage_item_free(cloud_storage_item_t *item)
{
   cloud_storage_item_t *next;

   while (item)
   {
      next = item->next;

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

      if (item->item_type == CLOUD_STORAGE_FOLDER && item->type_data.folder.children)
      {
         cloud_storage_item_free(item->type_data.folder.children);
      }

      free(item);

      item = next;
   }
}

static void _free_operation_queue_item(struct _operation_queue_item *queue_item)
{
   switch (queue_item->operation)
   {
      case UPLOAD_FILE:
         if (queue_item->operation_parameters.upload.local_file)
         {
            free(queue_item->operation_parameters.upload.local_file);
         }
         break;
      case DOWNLOAD_FILE:
         if (queue_item->operation_parameters.download.remote_file)
         {
            cloud_storage_item_free(queue_item->operation_parameters.download.remote_file);
         }
         break;
      default:
         break;
   }

   free(queue_item);
}

static void _free_operation_queue(struct _operation_queue_item *queue)
{
   struct _operation_queue_item *next_item;

   while (queue)
   {
      next_item = queue->next;
      _free_operation_queue_item(queue);
      queue = next_item;
   }
}

static void _operation_thread_loop(void *user_data)
{
   while (!_shutdown)
   {
      struct _operation_queue_item *task;

      slock_lock(_operation_queue_mutex);

      if (!_operation_queue)
      {
         scond_wait(_operation_condition, _operation_queue_mutex);
      }

      if (_shutdown)
      {
         slock_unlock(_operation_queue_mutex);
         continue;
      }

      task = _operation_queue;
      if (_operation_queue)
      {
         _operation_queue = _operation_queue->next;
         task->next = NULL;
      }

      slock_unlock(_operation_queue_mutex);

      if (task)
      {
         switch (task->operation)
         {
            case SYNC_FILES:
               _sync_files_execute(task->operation_parameters.sync.folder_type);
               break;
            case UPLOAD_FILE:
               _upload_file_execute(task->operation_parameters.upload.folder_type, task->operation_parameters.upload.local_file);
               break;
            case DOWNLOAD_FILE:
               break;
         }

         _free_operation_queue_item(task);
      }
   }

   if (_operation_queue)
   {
      _free_operation_queue(_operation_queue);
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

   bytes_read = filestream_read(file, buffer, BUFFER_SIZE);
   while (bytes_read > 0)
   {
      MD5_Update(&md5_ctx, buffer, bytes_read);
      bytes_read = filestream_read(file, buffer, BUFFER_SIZE);
   }

   filestream_close(file);
   checksum = (unsigned char *)calloc(16, sizeof(char));
   MD5_Final(checksum, &md5_ctx);

   checksum_str = bytes_to_hex_str(checksum, 16);
   free(checksum);

   return checksum_str;
}

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

static cloud_storage_item_t *_get_remote_file_for_local_file(
   folder_type_t folder_type,
   char *local_file)
{
   cloud_storage_item_t *folder;
   cloud_storage_item_t *file;

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

   for (file = folder->type_data.folder.children; file != NULL; file = file->next)
   {
      if (file->item_type == CLOUD_STORAGE_FILE && !strcmp(local_file, file->name))
      {
         return file;
      }
   }

   return NULL;
}

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

   if (!remote_file)
   {
      remote_file = (cloud_storage_item_t *)calloc(1, sizeof(cloud_storage_item_t));
      remote_file->item_type = CLOUD_STORAGE_FILE;
      remote_file->name = strdup(find_last_slash(local_file) + 1);
      remote_file->next = NULL;
      new_file = true;
   }

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

      if (new_file)
      {
         remote_file->next = remote_folder->type_data.folder.children;
         remote_folder->type_data.folder.children = remote_file;
      }
   }
}

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
   }
}

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

   dir = retro_opendir(dir_name);
   while (dir != NULL && retro_readdir(dir))
   {
      char *filename = (char *)retro_dirent_get_name(dir);
      char *absolute_filename;

      absolute_filename = pathname_join(dir_name, filename);

      if (strcmp(filename, ".") && strcmp(filename, ".."))
      {
         cloud_storage_item_t *remote_file;

         remote_file = _get_remote_file_for_local_file(folder_type, filename);
         if (remote_file && remote_file->item_type == CLOUD_STORAGE_FOLDER)
         {
            continue;
         } else if (remote_file)
         {
            cloud_storage_item_t *current_metadata;

            if (remote_file->last_sync_time < time(NULL) - 30)
            {
               current_metadata = _provider->get_file_metadata(remote_file);
               _update_existing_item(remote_file, current_metadata);
               cloud_storage_item_free(current_metadata);
            }
         }

         if (_file_need_upload(folder_type, absolute_filename, remote_file))
         {
            _upload_file(folder_type, absolute_filename, remote_file);
         }
      }

      free(absolute_filename);
   }

   free(dir_name);
}

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

static void _download_file(char *local_file, cloud_storage_item_t *remote_file)
{
   _provider->download_file(
      remote_file,
      local_file
   );
}

static void _sync_files_download(folder_type_t folder_type)
{
   cloud_storage_item_t *remote_folder;
   cloud_storage_item_t *remote_file;

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

   for (remote_file = remote_folder->type_data.folder.children; remote_file != NULL; remote_file = remote_file->next)
   {
      char *local_file;
      bool need_download = false;
      cloud_storage_item_t *current_metadata;

      if (remote_file->last_sync_time < time(NULL) - 30)
      {
         current_metadata = _provider->get_file_metadata(remote_file);
         if (!current_metadata)
         {
            continue;
         }
         _update_existing_item(remote_file, current_metadata);
         cloud_storage_item_free(current_metadata);
      }

      local_file = _get_absolute_filename(folder_type, remote_file->name);

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

      if (need_download)
      {
         _download_file(local_file, remote_file);
      }
   }
}

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
      *folder = _provider->get_folder_metadata(folder_name);
      if (*folder)
      {
         _provider->list_files(*folder);
         return true;
      } else
      {
         *folder = _provider->create_folder(folder_name);
         return *folder != NULL;
      }
   } else
   {
      return true;
   }
}

static void _sync_files_execute(folder_type_t folder_type)
{
   if (!_prepare_folder(folder_type))
   {
      return;
   }

   _sync_files_upload(folder_type);
   _sync_files_download(folder_type);
}

void cloud_storage_sync_files(void)
{
   bool sync_states;
   global_t *global = global_get_ptr();
   char *raw_folder;
   int i;
   struct _operation_queue_item *queue_item = NULL;
   struct _operation_queue_item *previous_queue_item = NULL;

   raw_folder = _config_value_to_directory(global->name.savestate);
   sync_states = strcmp(_game_states_config_value, raw_folder) != 0;

   if (!sync_states)
   {
      return;
   } else
   {
      strcpy(_game_states_config_value, global->name.savestate);
   }

   slock_lock(_operation_queue_mutex);

   if (!_provider->ready_for_request())
   {
      slock_unlock(_operation_queue_mutex);
      return;
   }

   for (queue_item = _operation_queue; queue_item != NULL; queue_item = queue_item->next)
   {
      if (queue_item->operation == UPLOAD_FILE || queue_item->operation == DOWNLOAD_FILE)
      {
         if (!previous_queue_item)
         {
            _free_operation_queue_item(queue_item);
         } else
         {
            struct _operation_queue_item *next_item;

            next_item = queue_item->next;
            _free_operation_queue_item(queue_item);
            previous_queue_item->next = next_item;
            queue_item = next_item;
         }

         continue;
      }

      previous_queue_item = queue_item;
   }

   if (sync_states)
   {
      queue_item = (struct _operation_queue_item *)calloc(1, sizeof(struct _operation_queue_item));
      queue_item->operation = SYNC_FILES;
      queue_item->operation_parameters.sync.folder_type = CLOUD_STORAGE_GAME_STATES;
      if (previous_queue_item)
      {
         previous_queue_item->next = queue_item;
      } else
      {
         _operation_queue = queue_item;
      }
      previous_queue_item = queue_item;
   }

   scond_signal(_operation_condition);

   slock_unlock(_operation_queue_mutex);
}

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

void cloud_storage_upload_file(folder_type_t folder_type, char *file_name)
{
   struct _operation_queue_item *queue_item;
   struct _operation_queue_item *previous_queue_item = NULL;

   slock_lock(_operation_queue_mutex);

   if (!_provider->ready_for_request())
   {
      slock_unlock(_operation_queue_mutex);
      return;
   }

   for (queue_item = _operation_queue; queue_item != NULL; queue_item = queue_item->next)
   {
      switch (queue_item->operation)
      {
         case SYNC_FILES:
            goto complete;
         case UPLOAD_FILE:
            if (queue_item->operation_parameters.upload.folder_type && !strcmp(file_name, queue_item->operation_parameters.upload.local_file))
            {
               goto complete;
            }
            break;
         case DOWNLOAD_FILE:
            continue;
      }

      previous_queue_item = queue_item;
   }

   queue_item = (struct _operation_queue_item *)calloc(1, sizeof(struct _operation_queue_item));
   queue_item->operation = UPLOAD_FILE;
   queue_item->operation_parameters.sync.folder_type = folder_type;
   queue_item->operation_parameters.upload.local_file = strdup(file_name);
   if (previous_queue_item)
   {
      previous_queue_item->next = queue_item;
   } else
   {
      _operation_queue = queue_item;
   }

   scond_signal(_operation_condition);

complete:
   slock_unlock(_operation_queue_mutex);
}

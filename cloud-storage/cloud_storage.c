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

#include <configuration.h>
#include <file/file_path.h>
#include <retroarch.h>
#include <retro_dirent.h>
#include <lrc_hash.h>
#include <rthreads/rthreads.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#include "cloud_storage.h"
#include "hex.h"
#include "driver_utils.h"
#include "google/google.h"
#include "onedrive/onedrive.h"

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
static cloud_storage_provider_t *_providers[3];

static char _game_saves_config_value[8192] = "";
static char _game_states_config_value[8192] = "";
static char _runtime_logs_dir[8192] = "";
static char _screenshots_config_value[8192] = "";

static bool _need_sync_runtime_logs = false;

cloud_storage_item_t *_game_saves = NULL;
cloud_storage_item_t *_game_states = NULL;
cloud_storage_item_t *_runtime_logs = NULL;
cloud_storage_item_t *_screenshots = NULL;

static struct _operation_queue_item *_operation_queue = NULL;
static slock_t *_operation_queue_mutex;

static scond_t *_operation_condition;
static sthread_t *_operation_thread;

static void _sync_files_execute(folder_type_t folder_type);

static void _upload_file_execute(folder_type_t folder_type, char *file_name);

static void _free_operation_queue(struct _operation_queue_item *queue)
{
   struct _operation_queue_item *next_item;

   while (queue)
   {
      switch (queue->operation)
      {
         case UPLOAD_FILE:
            if (queue->operation_parameters.upload.local_file)
            {
               free(queue->operation_parameters.upload.local_file);
            }
            break;
         case DOWNLOAD_FILE:
            if (queue->operation_parameters.download.remote_file)
            {
               cloud_storage_item_free(queue->operation_parameters.download.remote_file);
            }
            break;
         default:
            break;
      }

      next_item = queue->next;
      free(queue);
      queue = next_item;
   }
}

static void _operation_thread_loop(void *user_data)
{
   while (!_shutdown)
   {
      struct _operation_queue_item *task;

      slock_lock(_operation_queue_mutex);

      scond_wait_timeout(_operation_condition, _operation_queue_mutex, 500);
      if (_shutdown)
      {
         slock_unlock(_operation_queue_mutex);
         continue;
      }

      task = _operation_queue;
      if (_operation_queue)
      {
         _operation_queue = NULL;
      }

      slock_unlock(_operation_queue_mutex);

      while (task)
      {
         struct _operation_queue_item *next_task;

         next_task = task->next;
         task->next = NULL;

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

         _free_operation_queue(task);
         task = next_task;
      }
   }

   if (_operation_queue)
   {
      _free_operation_queue(_operation_queue);
   }
}

static cloud_storage_provider_t *_get_active_provider(void)
{
   settings_t *settings;

   settings = config_get_ptr();

   if (!_providers[settings->uints.cloud_storage_provider])
   {
      cloud_storage_init();
   }

   return _providers[settings->uints.cloud_storage_provider];
}

void cloud_storage_init(void)
{
   settings_t *settings;
   struct _operation_queue_item *sync_item;

   if (!_shutdown)
   {
      return;
   }

   _providers[0] = cloud_storage_google_create();
   _providers[1] = cloud_storage_onedrive_create();
   _providers[2] = NULL;

   settings = config_get_ptr();
   cloud_storage_set_active_provider(settings->uints.cloud_storage_provider);

   _operation_queue_mutex = slock_new();
   _operation_condition = scond_new();

   _shutdown = false;
   _operation_thread = sthread_create(_operation_thread_loop, NULL);

   if (_get_active_provider()->ready_for_request())
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

static char *_get_md5_hash(char *absolute_filename)
{
   RFILE *file;
   uint8_t *buffer;
   MD5_CTX md5_ctx;
   int64_t bytes_read;
   unsigned char *checksum;
   char *checksum_str;

   file = filestream_open(absolute_filename, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
   {
      return NULL;
   }

   buffer = (uint8_t *)malloc(4096);
   MD5_Init(&md5_ctx);

   bytes_read = filestream_read(file, buffer, 4096);
   while (bytes_read > 0)
   {
      MD5_Update(&md5_ctx, buffer, bytes_read);
      bytes_read = filestream_read(file, buffer, 4096);
   }

   free(buffer);
   filestream_close(file);
   checksum = (unsigned char *)calloc(16, sizeof(char));
   MD5_Final(checksum, &md5_ctx);

   checksum_str = bytes_to_hex_str(checksum, 16);
   free(checksum);

   return checksum_str;
}

static char *_get_sha1_hash(char *absolute_filename)
{
   char *checksum_str;

   checksum_str = (char *)malloc(41 * sizeof(char));
   if (!sha1_calculate(absolute_filename, checksum_str))
   {
      return checksum_str;
   } else
   {
      free(checksum_str);
      return NULL;
   }
}

static char *_get_sha256_hash(char *absolute_filename)
{
   RFILE *file;
   struct sha256_ctx sha;
   int64_t bytes_read;
   char *checksum_str;
   int i;
   uint8_t buffer[4096];

   union
   {
      uint32_t u32[8];
      uint8_t u8[32];
   } shahash;

   file = filestream_open(absolute_filename, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
   {
      return NULL;
   }

   sha256_init(&sha);

   bytes_read = filestream_read(file, buffer, 4096);
   while (bytes_read > 0)
   {
      sha256_chunk(&sha, buffer, bytes_read);
      bytes_read = filestream_read(file, buffer, 4096);
   }

   filestream_close(file);

   sha256_final(&sha);
   sha256_subhash(&sha, shahash.u32);

   checksum_str = (char *)calloc(65, sizeof(char));
   for (i = 0; i < 32; i++)
   {
      snprintf(checksum_str + 2 * i, 3, "%02X", (unsigned)shahash.u8[i]);
   }

   return checksum_str;
}

static char *_clean_directory_path(char *dir_name)
{
   size_t i;

   dir_name = path_remove_extension(dir_name);
   for (i = strlen(dir_name) - 1; i >= 0 && dir_name[i] == '/'; i--)
   {
      dir_name[i] = '\0';
   }

   return dir_name;
}

static char *_get_absolute_filename(folder_type_t folder_type, char *filename)
{
   global_t *global;
   char *raw_folder;
   char *folder;
   char *absolute_filename;
   size_t absolute_filename_len;

   global = global_get_ptr();
   raw_folder = folder_type == CLOUD_STORAGE_GAME_STATES ? global->name.savestate : global->name.savefile;
   if (strlen(raw_folder) == 0)
   {
      return NULL;
   }

   folder = (char *)malloc(strlen(raw_folder) + 1);
   strcpy(folder, raw_folder);
   folder = _clean_directory_path(folder);

   absolute_filename_len = strlen(folder) + strlen(filename) + 2;
   absolute_filename = (char *)calloc(strlen(folder) + strlen(filename) + 2, sizeof(char));
   fill_pathname_join(absolute_filename, folder, filename, absolute_filename_len);

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
      case CLOUD_STORAGE_GAME_SAVES:
         folder = _game_saves;
         break;
      case CLOUD_STORAGE_GAME_STATES:
         folder = _game_states;
         break;
      case CLOUD_STORAGE_RUNTIME_LOGS:
         folder = _runtime_logs;
         break;
      case CLOUD_STORAGE_SCREENSHOTS:
         folder = _screenshots;
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
   char *absolute_filename;

   if (!remote_file)
   {
      return true;
   }

   absolute_filename = _get_absolute_filename(folder_type, local_file);

   if (!absolute_filename)
   {
      return false;
   } else if (path_is_directory(absolute_filename))
   {
      free(absolute_filename);
      return false;
   }

   switch (remote_file->type_data.file.hash_type)
   {
      case MD5:
         checksum_str = _get_md5_hash(absolute_filename);
         break;
      case SHA1:
         checksum_str = _get_sha1_hash(absolute_filename);
         break;
      case SHA256:
         checksum_str = _get_sha256_hash(absolute_filename);
         break;
   }

   need_upload = strcmp(checksum_str, remote_file->type_data.file.hash_value) != 0;
   free(checksum_str);
   return need_upload;
}

static void _upload_file(folder_type_t folder_type, char *local_file, cloud_storage_item_t *remote_file)
{
   char *absolute_filename;
   cloud_storage_item_t *remote_folder;

   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_SAVES:
         remote_folder = _game_saves;
         break;
      case CLOUD_STORAGE_GAME_STATES:
         remote_folder = _game_states;
         break;
      case CLOUD_STORAGE_RUNTIME_LOGS:
         remote_folder = _runtime_logs;
         break;
      case CLOUD_STORAGE_SCREENSHOTS:
         remote_folder = _screenshots;
         break;
      default:
         return;
   }

   absolute_filename = _get_absolute_filename(folder_type, local_file);
   if (!absolute_filename)
   {
      return;
   } else if (path_is_directory(absolute_filename))
   {
      free(absolute_filename);
      return;
   }

   if (_get_active_provider()->upload_file(
      remote_folder,
      remote_file,
      absolute_filename))
   {
      if (!remote_file->type_data.file.hash_value) {
         switch (remote_file->type_data.file.hash_type)
         {
            case MD5:
               remote_file->type_data.file.hash_value = _get_md5_hash(absolute_filename);
               break;
            case SHA1:
               remote_file->type_data.file.hash_value = _get_sha1_hash(absolute_filename);
               break;
            case SHA256:
               remote_file->type_data.file.hash_value = _get_sha256_hash(absolute_filename);
               break;
         }
      }

      remote_file->last_sync_time = time(NULL);
   }

   free(absolute_filename);
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
      item_to_update->name = (char *)calloc(strlen(other->name) + 1, sizeof(char));
      strcpy(item_to_update->name, other->name);
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
         item_to_update->type_data.file.download_url = (char *)calloc(strlen(other->type_data.file.download_url) + 1, sizeof(char));
         strcpy(item_to_update->type_data.file.download_url, other->type_data.file.download_url);
      }

      item_to_update->type_data.file.hash_type = other->type_data.file.hash_type;

      if (other->type_data.file.hash_value)
      {
         item_to_update->type_data.file.hash_value = (char *)calloc(strlen(other->type_data.file.hash_value) + 1, sizeof(char));
         strcpy(item_to_update->type_data.file.hash_value, other->type_data.file.hash_value);
      }
   } else
   {
      item_to_update->type_data.folder.children = other->type_data.folder.children;
   }
}

static void _sync_files_upload(folder_type_t folder_type)
{
   global_t *global;
   settings_t *settings;
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
   settings = config_get_ptr();
   if (!settings)
   {
      return;
   }

   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_SAVES:
         remote_folder = _game_saves;
         raw_dir = global->name.savefile;
         break;
      case CLOUD_STORAGE_GAME_STATES:
         remote_folder = _game_states;
         raw_dir = global->name.savestate;
         break;
      case CLOUD_STORAGE_RUNTIME_LOGS:
         remote_folder = _runtime_logs;
         raw_dir = _runtime_logs_dir;
         break;
      case CLOUD_STORAGE_SCREENSHOTS:
         remote_folder = _screenshots;
         raw_dir = settings->paths.directory_screenshot;
         break;
      default:
         return;
   }

   if (!remote_folder || strlen(raw_dir) == 0)
   {
      return;
   }

   dir_name = (char *)calloc(strlen(raw_dir) + 1, sizeof(char));
   strcpy(dir_name, raw_dir);
   dir_name = _clean_directory_path(dir_name);

   dir = retro_opendir(dir_name);
   while (dir != NULL && retro_readdir(dir))
   {
      char *filename = (char *)retro_dirent_get_name(dir);
      if (strcmp(filename, ".") && strcmp(filename, ".."))
      {
         cloud_storage_item_t *remote_file;
         bool new_file = false;

         remote_file = _get_remote_file_for_local_file(folder_type, filename);
         if (remote_file && remote_file->item_type == CLOUD_STORAGE_FOLDER)
         {
            continue;
         } else if (remote_file)
         {
            cloud_storage_item_t *current_metadata;

            if (remote_file->last_sync_time < time(NULL) - 30)
            {
               current_metadata = _get_active_provider()->get_file_metadata(remote_file);
               _update_existing_item(remote_file, current_metadata);
               cloud_storage_item_free(current_metadata);
            }
         } else
         {
            remote_file = (cloud_storage_item_t *)calloc(1, sizeof(cloud_storage_item_t));
            remote_file->item_type = CLOUD_STORAGE_FILE;
            remote_file->name = (char *)calloc(strlen(filename) + 1, sizeof(char));
            strcpy(remote_file->name, filename);

            new_file = true;
         }

         if (new_file || _file_need_upload(folder_type, filename, remote_file))
         {
            _upload_file(folder_type, filename, remote_file);

            if (new_file)
            {
               if (!remote_folder->type_data.folder.children)
               {
                  remote_folder->type_data.folder.children = remote_file;
               } else
               {
                  cloud_storage_item_t *last_item;

                  for (last_item = remote_folder->type_data.folder.children; last_item->next != NULL; last_item = last_item->next);

                  last_item->next = remote_file;
               }
               
            }
         }
      }
   }

   free(dir_name);
}

static bool _have_local_file_for_remote_file(
   folder_type_t folder_type,
   cloud_storage_item_t *remote_file)
{
   global_t *global;
   settings_t *settings;
   char *dir_name = NULL;
   struct RDIR *dir;
   bool found = false;

   global = global_get_ptr();
   if (!global)
   {
      return false;
   }
   settings = config_get_ptr();
   if (!settings)
   {
      return false;
   }

   switch (folder_type)
   {
      case CLOUD_STORAGE_GAME_SAVES:
         if (strlen(global->name.savefile) > 0)
         {
            dir_name = (char *)calloc(strlen(global->name.savefile) + 1, sizeof(char));
            strcpy(dir_name, global->name.savefile);
         }
         break;
      case CLOUD_STORAGE_GAME_STATES:
         if (strlen(global->name.savestate) > 0)
         {
            dir_name = (char *)calloc(strlen(global->name.savestate) + 1, sizeof(char));
            strcpy(dir_name, global->name.savestate);
         }
         break;
      case CLOUD_STORAGE_RUNTIME_LOGS:
         if (strlen(_runtime_logs_dir) > 0)
         {
            dir_name = (char *)calloc(strlen(_runtime_logs_dir) + 1, sizeof(char));
            strcpy(dir_name, _runtime_logs_dir);
         }
         break;
      case CLOUD_STORAGE_SCREENSHOTS:
         if (strlen(settings->paths.directory_screenshot) > 0)
         {
            dir_name = (char *)calloc(strlen(settings->paths.directory_screenshot) + 1, sizeof(char));
            strcpy(dir_name, settings->paths.directory_screenshot);
         }
         break;
      default:
         return false;
   }

   if (!dir_name)
   {
      return false;
   }

   dir_name = _clean_directory_path(dir_name);

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
      case MD5:
         checksum_str = _get_md5_hash(local_file);
         break;
      case SHA1:
         checksum_str = _get_sha1_hash(local_file);
         break;
      case SHA256:
         checksum_str = _get_sha256_hash(local_file);
         break;
   }

   need_download = strcmp(checksum_str, remote_file->type_data.file.hash_value) != 0;
   free(checksum_str);
   return need_download;
}

static void _download_file(char *local_file, cloud_storage_item_t *remote_file)
{
   _get_active_provider()->download_file(
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
      case CLOUD_STORAGE_GAME_SAVES:
         remote_folder = _game_saves;
         break;
      case CLOUD_STORAGE_GAME_STATES:
         remote_folder = _game_states;
         break;
      case CLOUD_STORAGE_RUNTIME_LOGS:
         remote_folder = _runtime_logs;
         break;
      case CLOUD_STORAGE_SCREENSHOTS:
         remote_folder = _screenshots;
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
         current_metadata = _get_active_provider()->get_file_metadata(remote_file);
         if (!current_metadata)
         {
            continue;
         }
         _update_existing_item(remote_file, current_metadata);
      }

      local_file = _get_absolute_filename(folder_type, remote_file->name);

      if (_have_local_file_for_remote_file(folder_type, remote_file))
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
      case CLOUD_STORAGE_GAME_SAVES:
         folder_name = GAME_SAVES_FOLDER_NAME;
         folder = &_game_saves;
         break;
      case CLOUD_STORAGE_GAME_STATES:
         folder_name = GAME_STATES_FOLDER_NAME;
         folder = &_game_states;
         break;
      case CLOUD_STORAGE_RUNTIME_LOGS:
         folder_name = RUNTIME_LOGS_FOLDER_NAME;
         folder = &_runtime_logs;
         break;
      case CLOUD_STORAGE_SCREENSHOTS:
         folder_name = SCREENSHOTS_FOLDER_NAME;
         folder = &_screenshots;
         break;
      default:
         return false;
   }

   if (*folder == NULL)
   {
      *folder = _get_active_provider()->get_folder_metadata(folder_name);
      if (*folder)
      {
         _get_active_provider()->list_files(*folder);
         return true;
      } else
      {
         *folder = _get_active_provider()->create_folder(folder_name);
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

static void _free_operation_queue_item(struct _operation_queue_item *queue_item)
{
   switch (queue_item->operation)
   {
      case UPLOAD_FILE:
         free(queue_item->operation_parameters.upload.local_file);
         break;
      default:
         break;
   }

   free(queue_item);
}

void cloud_storage_sync_files(void)
{
   bool sync_saves;
   bool sync_states;
   bool sync_runtime_logs;
   bool sync_screenshots;
   global_t *global = global_get_ptr();
   settings_t *settings = config_get_ptr();
   struct _operation_queue_item *queue_item = NULL;
   struct _operation_queue_item *previous_queue_item = NULL;

   sync_saves = strcmp(_game_saves_config_value, global->name.savefile) != 0;
   sync_states = strcmp(_game_states_config_value, global->name.savestate) != 0;
   sync_screenshots = strcmp(_screenshots_config_value, settings->paths.directory_screenshot) != 0;

   if (!sync_saves && !sync_states && !_need_sync_runtime_logs)
   {
      return;
   }

   if (sync_saves)
   {
      strcpy(_game_saves_config_value, global->name.savefile);
   }
   if (sync_states)
   {
      strcpy(_game_states_config_value, global->name.savestate);
   }
   if (sync_screenshots)
   {
      strcpy(_screenshots_config_value, settings->paths.directory_screenshot);
   }

   slock_lock(_operation_queue_mutex);

   if (!_get_active_provider()->ready_for_request())
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

   if (sync_saves)
   {
      queue_item = (struct _operation_queue_item *)calloc(1, sizeof(struct _operation_queue_item));
      queue_item->operation = SYNC_FILES;
      queue_item->operation_parameters.sync.folder_type = CLOUD_STORAGE_GAME_SAVES;
      if (previous_queue_item)
      {
         previous_queue_item->next = queue_item;
      } else
      {
         _operation_queue = queue_item;
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

   if (_need_sync_runtime_logs)
   {
      queue_item = (struct _operation_queue_item *)calloc(1, sizeof(struct _operation_queue_item));
      queue_item->operation = SYNC_FILES;
      queue_item->operation_parameters.sync.folder_type = CLOUD_STORAGE_RUNTIME_LOGS;
      if (previous_queue_item)
      {
         previous_queue_item = queue_item;
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

   remote_file = _get_remote_file_for_local_file(folder_type, file_name);
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

   if (!_get_active_provider()->ready_for_request())
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
   queue_item->operation_parameters.sync.folder_type = CLOUD_STORAGE_GAME_SAVES;
   queue_item->operation_parameters.upload.local_file = (char *)calloc(strlen(file_name) + 1, sizeof(char));
   strcpy(queue_item->operation_parameters.upload.local_file, file_name);
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

void cloud_storage_set_active_provider(unsigned provider_id)
{
   if (_game_saves)
   {
      cloud_storage_item_free(_game_saves);
      _game_saves = NULL;
   }

   if (_game_states)
   {
      cloud_storage_item_free(_game_states);
      _game_states = NULL;
   }
}

bool cloud_storage_need_authorization(void)
{
   return _get_active_provider()->need_authorization;
}

bool cloud_storage_have_default_credentials(void)
{
   return _get_active_provider()->have_default_credentials();
}

void cloud_storage_authorize(void (*callback)(bool success))
{
   _get_active_provider()->authorize(callback);
}

void cloud_storage_set_logfile_dir(const char *logfile_dir)
{
   if (logfile_dir)
   {
      if (strcmp(_runtime_logs_dir, logfile_dir) != 0)
      {
         _need_sync_runtime_logs = true;
      }

      strcpy(_runtime_logs_dir, logfile_dir);
   } else
   {
      _runtime_logs_dir[0] = '\0';
   }
}
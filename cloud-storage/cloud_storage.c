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
#include <retroarch.h>
#include <retro_dirent.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <rhash.h>

#include "cloud_storage.h"
#include "hex.h"
#include "driver_utils.h"
#include "google/google.h"
#include "onedrive/onedrive.h"

static cloud_storage_provider_t *providers[1];
static size_t cloud_storage_providers_count;
static cloud_storage_provider_state_t *active_provider_state;

static bool initialized = false;

#define VERIFY_INITIALIZED { \
   if (!initialized) { \
      cloud_storage_init(); \
      initialized = true; \
   } \
}

static void sync_files_callback(
   cloud_storage_operation_state_t *operation_state,
   bool succeeded);

static void free_continuation_data(cloud_storage_continuation_data_t *continuation_data)
{
   free(continuation_data);
}

static void free_operation_state(cloud_storage_operation_state_t *operation_state)
{
   free(operation_state);
}

void cloud_storage_init(void)
{
   settings_t *settings;

   providers[0] = cloud_storage_google_create();
   providers[1] = cloud_storage_onedrive_create();
   providers[2] = NULL;
   //providers[2] = cloud_storage_aws_create();

   initialized = true;

   settings = config_get_ptr();
   cloud_storage_set_active_provider(settings->uints.cloud_storage_provider);
}

static cloud_storage_provider_t *get_active_provider(settings_t *settings)
{
   return providers[settings->uints.cloud_storage_provider];
}

static bool have_local_dir(folder_type_t folder_type)
{
   global_t *global = global_get_ptr();
   char *config_value;

   if (!global)
   {
      return false;
   }

   if (folder_type == CLOUD_STORAGE_GAME_SAVES)
   {
      config_value = global->name.savefile;
   } else if (folder_type == CLOUD_STORAGE_GAME_STATES)
   {
      config_value = global->name.savestate;
   } else
   {
      return false;
   }

   if (config_value)
   {
      char *directory;
      struct RDIR *dir;

      directory = (char *)calloc(strlen(config_value) + 1, sizeof(char));
      strcpy(directory, config_value);
      path_remove_extension(directory);

      dir = retro_opendir(directory);
      if (!dir)
      {
         free(directory);
         return false;
      } else
      {
         retro_closedir(dir);
         free(directory);
         return true;
      }
   } else
   {
      return false;
   }
}

static char *get_md5_hash(char *absolute_filename)
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

static char *get_sha1_hash(char *absolute_filename)
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

static char *get_sha256_hash(char *absolute_filename)
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

   checksum_str = (char *)calloc(41, sizeof(char));
   for (i = 0; i < 32; i++)
   {
      snprintf(checksum_str + 2 * i, 3, "%02x", (unsigned)shahash.u8[i]);
   }

   return checksum_str;
}

static bool file_need_download(
   folder_type_t folder_type,
   cloud_storage_item_t *item)
{
   cloud_storage_item_t *remote_file;
   char *raw_dir;
   char *local_dir;
   char *absolute_filename;
   size_t absolute_filename_len;
   char *checksum_str = NULL;
   global_t *global;

   global = global_get_ptr();

   if (item->item_type == CLOUD_STORAGE_FOLDER)
   {
      return false;
   }

   if (folder_type == CLOUD_STORAGE_GAME_STATES)
   {
      raw_dir = global->name.savestate;
   } else
   {
      raw_dir = global->name.savefile;
   }
   local_dir = (char *)calloc(strlen(raw_dir) + 1, sizeof(char));
   strcpy(local_dir, raw_dir);
   path_remove_extension(local_dir);

   absolute_filename_len = strlen(local_dir) + strlen(item->name) + 2;
   absolute_filename = (char *)calloc(absolute_filename_len, sizeof(char));
   fill_pathname_join(absolute_filename, local_dir, item->name, absolute_filename_len);
   free(local_dir);

   switch (item->type_data.file.hash_type)
   {
      case MD5:
         checksum_str = get_md5_hash(absolute_filename);
         break;
      case SHA1:
         checksum_str = get_sha1_hash(absolute_filename);
         break;
      case SHA256:
         checksum_str = get_sha256_hash(absolute_filename);
         break;
   }

   if (!checksum_str)
   {
      return true;
   } else if (!strcmp(checksum_str, item->type_data.file.hash_value))
   {
      free(checksum_str);
      return false;
   } else
   {
      free(checksum_str);
      return true;
   }
}

static cloud_storage_upload_item_t *get_upload_list(
   cloud_storage_provider_state_t *provider_state,
   folder_type_t folder_type)
{
   global_t *global = global_get_ptr();
   char *dir_name;
   struct RDIR *dir;
   char *filename;
   cloud_storage_upload_item_t *upload_list = NULL;
   cloud_storage_upload_item_t *current_item;

   if (!global)
   {
      return NULL;
   }

   if (folder_type == CLOUD_STORAGE_GAME_SAVES && strlen(global->name.savefile) > 0)
   {
      dir_name = (char *)calloc(strlen(global->name.savefile) + 1, sizeof(char));
      strcpy(dir_name, global->name.savefile);
   } else if (folder_type == CLOUD_STORAGE_GAME_STATES && strlen(global->name.savestate) > 0)
   {
      dir_name = (char *)calloc(strlen(global->name.savestate) + 1, sizeof(char));
      strcpy(dir_name, global->name.savestate);
   } else
   {
      return NULL;
   }

   dir_name = path_remove_extension(dir_name);

   dir = retro_opendir(dir_name);
   while (dir != NULL && retro_readdir(dir))
   {
      char *filename = (char *)retro_dirent_get_name(dir);
      if (strcmp(filename, ".") && strcmp(filename, ".."))
      {
         current_item = (cloud_storage_upload_item_t *)malloc(sizeof(cloud_storage_upload_item_t));
         current_item->local_file = (char *)calloc(strlen(filename) + 1, sizeof(char));
         strcpy(current_item->local_file, filename);
         current_item->next = upload_list;
         upload_list = current_item;
      }
   }

   free(dir_name);

   return upload_list;
}

static bool file_need_upload(folder_type_t folder_type, cloud_storage_item_t *folder, char *local_file)
{
   cloud_storage_item_t *remote_file;
   char *checksum_str = NULL;
   bool need_upload;
   global_t *global;
   char *raw_dir;
   char *dir_name;
   char *absolute_filename;
   size_t absolute_filename_len;

   for (remote_file = folder->type_data.folder.children;remote_file;remote_file = remote_file->next)
   {
      if (!strcmp(local_file, remote_file->name))
      {
         break;
      }
   }

   if (!remote_file)
   {
      return true;
   }

   global = global_get_ptr();

   if (folder->item_type == CLOUD_STORAGE_FOLDER)
   {
      return false;
   }

   if (folder_type == CLOUD_STORAGE_GAME_STATES)
   {
      raw_dir = global->name.savestate;
   } else
   {
      raw_dir = global->name.savefile;
   }
   dir_name = (char *)calloc(strlen(raw_dir) + 1, sizeof(char));
   strcpy(dir_name, raw_dir);
   path_remove_extension(dir_name);

   absolute_filename_len = strlen(dir_name) + strlen(local_file) + 2;
   absolute_filename = (char *)calloc(absolute_filename_len, sizeof(char));
   fill_pathname_join(absolute_filename, dir_name, local_file, absolute_filename_len);
   free(dir_name);

   switch (remote_file->type_data.file.hash_type)
   {
      case MD5:
         checksum_str = get_md5_hash(absolute_filename);
         break;
      case SHA1:
         checksum_str = get_sha1_hash(absolute_filename);
         break;
      case SHA256:
         checksum_str = get_sha256_hash(absolute_filename);
         break;
   }

   need_upload = strcmp(checksum_str, remote_file->type_data.file.hash_value) != 0;
   free(checksum_str);
   return need_upload;
}

static void sync_files_get_folder_metadata(
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_provider_t *provider,
   folder_type_t folder_type)
{
   char *folder_name;

   continuation_data->operation_data.sync_files.phase = GET_FOLDER_METADATA;

   provider->get_folder_metadata(
      folder_type == CLOUD_STORAGE_GAME_STATES ? GAME_STATES_FOLDER_NAME : GAME_SAVES_FOLDER_NAME,
      continuation_data,
      sync_files_callback
   );
}

static void sync_files_create_folder(
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_provider_t *provider,
   folder_type_t folder_type)
{
   continuation_data->operation_data.sync_files.phase = CREATE_FOLDER;

   provider->create_folder(
      folder_type == CLOUD_STORAGE_GAME_STATES ? GAME_STATES_FOLDER_NAME : GAME_SAVES_FOLDER_NAME,
      continuation_data,
      sync_files_callback);
}

static void sync_files_list_folder(
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_provider_t *provider,
   cloud_storage_provider_state_t *provider_state,
   folder_type_t folder_type)
{
   cloud_storage_item_t *folder;

   if (provider_state->game_states->type_data.folder.children)
   {
      cloud_storage_item_free(provider_state->game_states->type_data.folder.children);
      provider_state->game_states->type_data.folder.children = NULL;
   }

   continuation_data->operation_data.sync_files.phase = LISTING_FOLDER;

   folder = folder_type == CLOUD_STORAGE_GAME_STATES ? provider_state->game_states : provider_state->game_saves;

   provider->list_files(
      folder,
      continuation_data,
      sync_files_callback
   );
}

static void sync_files_download_file(
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_provider_t *provider,
   cloud_storage_provider_state_t *provider_state,
   folder_type_t folder_type)
{
   global_t *global;
   char *local_file;
   char *local_folder;
   char *config_value;

   global = global_get_ptr();

   continuation_data->operation_data.sync_files.phase = DOWNLOADING_ITEMS;
   config_value = folder_type == CLOUD_STORAGE_GAME_STATES ? global->name.savestate : global->name.savefile;
   local_folder = (char *)calloc(strlen(config_value) + 1, sizeof(char));
   strcpy(local_folder, config_value);
   path_remove_extension(local_folder);

   local_file = (char *)calloc(strlen(local_folder) + strlen(continuation_data->operation_data.sync_files.files_to_download->name) + 2, sizeof(char));
   strcpy(local_file, local_folder);
   local_file[strlen(local_file)] = '/';
   strcpy(local_file + strlen(local_folder) + 1, continuation_data->operation_data.sync_files.files_to_download->name);

   free(local_folder);

   provider->download_file(
      continuation_data->operation_data.sync_files.files_to_download,
      local_file,
      continuation_data,
      sync_files_callback
   );
}

static char *get_absolute_filename(folder_type_t folder_type, char *filename)
{
   global_t *global = global_get_ptr();
   char *raw_folder;
   char *folder;
   char *absolute_filename;

   global = global_get_ptr();
   raw_folder = folder_type == CLOUD_STORAGE_GAME_STATES ? global->name.savestate : global->name.savefile;
   folder = (char *)malloc(strlen(raw_folder) + 1);
   strcpy(folder, raw_folder);
   path_remove_extension(folder);

   absolute_filename = (char *)calloc(strlen(folder) + strlen(filename) + 2, sizeof(char));
   strcpy(absolute_filename, folder);
   absolute_filename[strlen(folder)] = '/';
   strcpy(absolute_filename + strlen(folder) + 1, filename);

   free(folder);

   return absolute_filename;
}

static void sync_files_upload_item(
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_provider_t *provider,
   cloud_storage_provider_state_t *provider_state,
   folder_type_t folder_type)
{
   cloud_storage_item_t *folder;
   cloud_storage_item_t *remote_file;
   char *file_name;

   folder = folder_type == CLOUD_STORAGE_GAME_STATES ?
      continuation_data->provider_state->game_states : continuation_data->provider_state->game_saves;

   file_name = continuation_data->operation_data.sync_files.upload_items->local_file;
   for (remote_file = folder->type_data.folder.children;remote_file;remote_file = remote_file->next)
   {
      if (!strcmp(remote_file->name, file_name))
      {
         break;
      }
   }

   if (!remote_file)
   {
      remote_file = (cloud_storage_item_t *)calloc(1, sizeof(cloud_storage_item_t));
      remote_file->name = (char *)calloc(strlen(file_name) + 1, sizeof(char));
      strcpy(remote_file->name, file_name);
      remote_file->item_type = CLOUD_STORAGE_FILE;
   }

   continuation_data->operation_data.sync_files.phase = UPLOADING_ITEMS;
   provider->upload_file(
      folder,
      remote_file,
      get_absolute_filename(folder_type, file_name),
      continuation_data,
      sync_files_callback
   );
}

static void sync_files_auth_callback(
   cloud_storage_continuation_data_t *continuation_data,
   bool success,
   void *data)
{
   if (!success)
   {
      free_continuation_data(continuation_data);
   } else
   {
      cloud_storage_operation_state_t *new_operation_state;

      new_operation_state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
      new_operation_state->continuation_data = continuation_data;

      sync_files_callback(new_operation_state, true);
   }
}

static cloud_storage_provider_status_t sync_files_preprocess(
   cloud_storage_operation_state_t *operation_state,
   bool succeeded)
{
   settings_t *settings;
   cloud_storage_provider_t *provider;

   settings = config_get_ptr();
   provider = get_active_provider(settings);

   switch (provider->get_status(operation_state->continuation_data->provider_state))
   {
      case CLOUD_STORAGE_STATUS_NOT_READY:
         free_continuation_data(operation_state->continuation_data);
         free_operation_state(operation_state);
         return CLOUD_STORAGE_STATUS_NOT_READY;
      case CLOUD_STORAGE_STATUS_NEED_AUTHENTICATION:
         {
            cloud_storage_continuation_data_t *continuation_data;

            continuation_data = operation_state->continuation_data;
            free_operation_state(operation_state);
            provider->authenticate(continuation_data, sync_files_auth_callback, continuation_data);
            return CLOUD_STORAGE_STATUS_NEED_AUTHENTICATION;
         }
      default:
         return CLOUD_STORAGE_STATUS_READY;
   }
}

static bool sync_files_process_response(
   cloud_storage_operation_state_t *operation_state,
   bool succeeded,
   folder_type_t folder_type,
   bool folder_exists)
{
   cloud_storage_continuation_data_t *continuation_data;

   if (!succeeded)
   {
      return false;
   }

   continuation_data = operation_state->continuation_data;

   switch (continuation_data->operation_data.sync_files.phase)
   {
      case NOT_STARTED:
         break;
      case GET_FOLDER_METADATA:
      case CREATE_FOLDER:
         if (operation_state->result)
         {
            if (folder_type == CLOUD_STORAGE_GAME_STATES)
            {
               continuation_data->provider_state->game_states = operation_state->result;
            } else
            {
               continuation_data->provider_state->game_saves = operation_state->result;
            }
         }
         break;
      case LISTING_FOLDER:
         if (operation_state->result)
         {
            if (folder_type == CLOUD_STORAGE_GAME_STATES)
            {
               continuation_data->provider_state->game_states->type_data.folder.children = operation_state->result;
            } else
            {
               continuation_data->provider_state->game_saves->type_data.folder.children = operation_state->result;
            }
         }
         break;
      case DOWNLOADING_ITEMS:
         if (continuation_data->operation_data.sync_files.files_to_download)
         {
            cloud_storage_item_t *item_to_delete;

            item_to_delete = continuation_data->operation_data.sync_files.files_to_download;
            continuation_data->operation_data.sync_files.files_to_download =
               continuation_data->operation_data.sync_files.files_to_download->next;
            item_to_delete->next = NULL;
            cloud_storage_item_free(item_to_delete);
         }
         break;
      case UPLOADING_ITEMS:
         if (continuation_data->operation_data.sync_files.upload_items)
         {
            cloud_storage_upload_item_t *item_to_delete;

            item_to_delete = continuation_data->operation_data.sync_files.upload_items;
            continuation_data->operation_data.sync_files.upload_items =
               continuation_data->operation_data.sync_files.upload_items->next;
            free(item_to_delete->local_file);
            free(item_to_delete);
         }
         break;
   }

   return true;
}

static void sync_files_next_download(cloud_storage_continuation_data_t *continuation_data)
{
   while (continuation_data->operation_data.sync_files.files_to_download)
   {
      if (file_need_download(
         continuation_data->operation_data.sync_files.current_folder_type,
         continuation_data->operation_data.sync_files.files_to_download))
      {
         return;
      } else
      {
         cloud_storage_item_t *item_to_remove;

         item_to_remove = continuation_data->operation_data.sync_files.files_to_download;
         continuation_data->operation_data.sync_files.files_to_download =
            continuation_data->operation_data.sync_files.files_to_download->next;
         item_to_remove->next = NULL;
         cloud_storage_item_free(item_to_remove);
      }
   }
}

static void sync_files_next_upload(cloud_storage_continuation_data_t *continuation_data)
{
   while (continuation_data->operation_data.sync_files.upload_items)
   {
      folder_type_t folder_type;
      cloud_storage_item_t *folder;

      folder_type = continuation_data->operation_data.sync_files.current_folder_type;
      folder = folder_type == CLOUD_STORAGE_GAME_STATES ?
         continuation_data->provider_state->game_states : continuation_data->provider_state->game_saves;

      if (file_need_upload(folder_type, folder, continuation_data->operation_data.sync_files.upload_items->local_file))
      {
         return;
      } else
      {
         cloud_storage_upload_item_t *item_to_remove;

         item_to_remove = continuation_data->operation_data.sync_files.upload_items;
         continuation_data->operation_data.sync_files.upload_items =
            continuation_data->operation_data.sync_files.upload_items->next;
         free(item_to_remove->local_file);
         free(item_to_remove);
      }
   }
}

static void sync_files_update_phase(cloud_storage_continuation_data_t *continuation_data, bool previous_operation_success)
{
   switch (continuation_data->operation_data.sync_files.phase)
   {
      case NOT_STARTED:
         continuation_data->operation_data.sync_files.phase = GET_FOLDER_METADATA;
         return;
      case GET_FOLDER_METADATA:
         {
            folder_type_t folder_type;
            cloud_storage_item_t *folder;

            folder_type = continuation_data->operation_data.sync_files.current_folder_type;
            folder = folder_type == CLOUD_STORAGE_GAME_STATES ?
               continuation_data->provider_state->game_states : continuation_data->provider_state->game_saves;
            if (folder)
            {
               continuation_data->operation_data.sync_files.phase = LISTING_FOLDER;
            } else
            {
               continuation_data->operation_data.sync_files.phase = CREATE_FOLDER;
            }
            return;
         }
      default:
         break;
   }

   switch (continuation_data->operation_data.sync_files.phase)
   {
      case CREATE_FOLDER:
         if (previous_operation_success)
         {
            continuation_data->operation_data.sync_files.phase = UPLOADING_ITEMS;
         } else
         {
            continuation_data->operation_data.sync_files.phase = NOT_STARTED;
         }

         return;
      case LISTING_FOLDER:
         {
            folder_type_t folder_type;
            cloud_storage_item_t *folder;

            folder_type = continuation_data->operation_data.sync_files.current_folder_type;
            folder = folder_type == CLOUD_STORAGE_GAME_STATES ?
               continuation_data->provider_state->game_states : continuation_data->provider_state->game_saves;

            continuation_data->operation_data.sync_files.files_to_download = cloud_storage_clone_item_list(folder->type_data.folder.children);
            sync_files_next_download(continuation_data);

            if (continuation_data->operation_data.sync_files.files_to_download)
            {
               continuation_data->operation_data.sync_files.phase = DOWNLOADING_ITEMS;
               return;
            }

            continuation_data->operation_data.sync_files.upload_items = get_upload_list(
               continuation_data->provider_state,
               continuation_data->operation_data.sync_files.current_folder_type
            );
            sync_files_next_upload(continuation_data);

            if (continuation_data->operation_data.sync_files.upload_items)
            {
               continuation_data->operation_data.sync_files.phase = UPLOADING_ITEMS;
               return;
            }

            continuation_data->operation_data.sync_files.phase = NOT_STARTED;
            return;
         }
      case DOWNLOADING_ITEMS:
         {
            cloud_storage_item_t *item_to_remove;

            item_to_remove = continuation_data->operation_data.sync_files.files_to_download;
            continuation_data->operation_data.sync_files.files_to_download = continuation_data->operation_data.sync_files.files_to_download->next;
            item_to_remove->next = NULL;
            cloud_storage_item_free(item_to_remove);
            sync_files_next_download(continuation_data);

            if (continuation_data->operation_data.sync_files.files_to_download)
            {
               return;
            }

            continuation_data->operation_data.sync_files.upload_items = get_upload_list(
               continuation_data->provider_state,
               continuation_data->operation_data.sync_files.current_folder_type
            );
            sync_files_next_upload(continuation_data);

            if (continuation_data->operation_data.sync_files.upload_items)
            {
               continuation_data->operation_data.sync_files.phase = UPLOADING_ITEMS;
               return;
            }

            continuation_data->operation_data.sync_files.phase = NOT_STARTED;
            return;
         }
      case UPLOADING_ITEMS:
         {
            cloud_storage_upload_item_t * item_to_remove;

            item_to_remove = continuation_data->operation_data.sync_files.upload_items;
            continuation_data->operation_data.sync_files.upload_items = continuation_data->operation_data.sync_files.upload_items->next;
            free(item_to_remove->local_file);
            free(item_to_remove);
            sync_files_next_upload(continuation_data);

            if (continuation_data->operation_data.sync_files.upload_items)
            {
               continuation_data->operation_data.sync_files.phase = UPLOADING_ITEMS;
               return;
            }

            continuation_data->operation_data.sync_files.phase = NOT_STARTED;
            return;
         }
      default:
         return;
   }
}

static void sync_files_callback(
   cloud_storage_operation_state_t *operation_state,
   bool succeeded)
{
   folder_type_t folder_type;
   settings_t *settings;
   cloud_storage_provider_t *provider;
   bool previous_operation_success;

   switch (sync_files_preprocess(operation_state, succeeded))
   {
      case CLOUD_STORAGE_STATUS_NOT_READY:
         return;
      case CLOUD_STORAGE_STATUS_NEED_AUTHENTICATION:
         return;
      case CLOUD_STORAGE_STATUS_READY:
         break;
   }

   folder_type = operation_state->continuation_data->operation_data.sync_files.current_folder_type;

   previous_operation_success = sync_files_process_response(operation_state, succeeded, folder_type, have_local_dir(folder_type));
   sync_files_update_phase(operation_state->continuation_data, previous_operation_success);

   if (operation_state->continuation_data->operation_data.sync_files.phase == NOT_STARTED)
   {
      if (folder_type == CLOUD_STORAGE_GAME_STATES && have_local_dir(CLOUD_STORAGE_GAME_SAVES))
      {
         folder_type = CLOUD_STORAGE_GAME_SAVES;
         operation_state->continuation_data->operation_data.sync_files.current_folder_type = folder_type;
         sync_files_update_phase(operation_state->continuation_data, true);
      }
   }

   settings = config_get_ptr();
   provider = get_active_provider(settings);

   switch (operation_state->continuation_data->operation_data.sync_files.phase)
   {
      case NOT_STARTED:
         break;
      case GET_FOLDER_METADATA:
         sync_files_get_folder_metadata(
            operation_state->continuation_data,
            provider,
            folder_type
         );
         return;
      case CREATE_FOLDER:
         sync_files_create_folder(
            operation_state->continuation_data,
            provider,
            folder_type
         );
         return;
      case LISTING_FOLDER:
         sync_files_list_folder(
            operation_state->continuation_data,
            provider,
            operation_state->continuation_data->provider_state,
            folder_type
         );
         return;
      case DOWNLOADING_ITEMS:
         sync_files_download_file(
            operation_state->continuation_data,
            provider,
            operation_state->continuation_data->provider_state,
            folder_type
         );
         return;
      case UPLOADING_ITEMS:
         sync_files_upload_item(
            operation_state->continuation_data,
            provider,
            operation_state->continuation_data->provider_state,
            folder_type
         );
         return;
   }

   free_continuation_data(operation_state->continuation_data);
}

void cloud_storage_authorize(
   unsigned provider_id,
   void (*callback)(bool success))
{
   cloud_storage_provider_t *provider;
   global_t *global;
   settings_t *settings;

   VERIFY_INITIALIZED

   global = global_get_ptr();
   settings = config_get_ptr();

   providers[provider_id]->authorize(settings, global, callback);
}

void cloud_storage_sync_files(void)
{
   cloud_storage_continuation_data_t *continuation_data;
   cloud_storage_operation_state_t *operation_state;

   VERIFY_INITIALIZED

   continuation_data = malloc(sizeof(cloud_storage_continuation_data_t));
   continuation_data->provider_state = active_provider_state;
   continuation_data->operation_type = CLOUD_STORAGE_SYNC;
   continuation_data->operation_data.sync_files.current_folder_type = CLOUD_STORAGE_GAME_STATES;
   continuation_data->operation_data.sync_files.upload_items = NULL;
   continuation_data->provider_state->game_states = NULL;
   continuation_data->provider_state->game_saves = NULL;
   continuation_data->operation_data.sync_files.phase = NOT_STARTED;
   continuation_data->operation_data.sync_files.files_to_download = NULL;

   operation_state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   operation_state->continuation_data = continuation_data;

   sync_files_callback(operation_state, true);
}

static cloud_storage_item_t *get_existing_item_metadata(
   cloud_storage_item_t *folder,
   char *name)
{
   cloud_storage_item_t *item;

   if (!folder)
   {
      return NULL;
   }

   for (item = folder->type_data.folder.children;item;item = item->next)
   {
      if (!strcmp(name, item->name))
      {
         return item;
      }
   }

   return NULL;
}

static void upload_file_callback(
   cloud_storage_operation_state_t *operation_state,
   bool succeeded)
{
}

void cloud_storage_upload_file(folder_type_t folder_type, char *filename)
{
   global_t *global;
   char *config_value;
   char *dir_name;
   char *absolute_filename;
   size_t absolute_filename_len;
   settings_t *settings;
   cloud_storage_provider_t *provider;
   cloud_storage_status_response_t provider_status;
   cloud_storage_continuation_data_t *continuation_data;
   cloud_storage_item_t *folder;
   cloud_storage_item_t *remote_item;

   VERIFY_INITIALIZED

   global = global_get_ptr();
   config_value = folder_type == CLOUD_STORAGE_GAME_STATES ? global->name.savestate : global->name.savefile;
   dir_name = (char *)calloc(strlen(config_value) + 1, sizeof(char));
   strcpy(dir_name, config_value);
   path_remove_extension(dir_name);

   absolute_filename_len = strlen(dir_name) + strlen(filename) + 2;
   absolute_filename = (char *)calloc(absolute_filename_len, sizeof(char));
   fill_pathname_join(absolute_filename, dir_name, filename, absolute_filename_len);

   free(dir_name);

   settings = config_get_ptr();
   provider = get_active_provider(settings);

   provider = get_active_provider(settings);
   provider_status = provider->get_status(active_provider_state);

   if (provider_status != CLOUD_STORAGE_STATUS_READY)
   {
      return;
   }

   continuation_data = malloc(sizeof(cloud_storage_continuation_data_t));
   continuation_data->provider_state = active_provider_state;
   continuation_data->operation_type = CLOUD_STORAGE_UPLOAD_FILE;

   folder = folder_type == CLOUD_STORAGE_GAME_STATES ? continuation_data->provider_state->game_states : continuation_data->provider_state->game_saves;

   continuation_data->operation_type = CLOUD_STORAGE_UPLOAD;
   continuation_data->operation_data.upload_file.file_to_upload = get_existing_item_metadata(
      folder,
      filename
   );

   if (!continuation_data->operation_data.upload_file.file_to_upload)
   {
      continuation_data->operation_data.upload_file.file_to_upload = (cloud_storage_item_t *)calloc(1, sizeof(cloud_storage_item_t));
      continuation_data->operation_data.upload_file.file_to_upload->name = filename;
      continuation_data->operation_data.upload_file.file_to_upload->item_type = CLOUD_STORAGE_FILE;
   }

   for (remote_item = folder->type_data.folder.children;remote_item;remote_item = remote_item->next)
   {
      if (!strcmp(filename, remote_item->name))
      {
         break;
      }
   }

   if (!remote_item)
   {
      remote_item = (cloud_storage_item_t *)calloc(1, sizeof(cloud_storage_item_t));
      remote_item->item_type = CLOUD_STORAGE_FILE;
      remote_item->name = (char *)calloc(strlen(filename) + 1, sizeof(char));
      strcpy(remote_item->name, filename);

      remote_item->next = folder->type_data.folder.children;
      folder->type_data.folder.children = remote_item;
   }

   provider->upload_file(
      folder_type == CLOUD_STORAGE_GAME_STATES ? continuation_data->provider_state->game_states : continuation_data->provider_state->game_saves,
      remote_item,
      dir_name,
      continuation_data,
      upload_file_callback
   );
}

void cloud_storage_set_active_provider(unsigned provider_id)
{
   settings_t *settings;
   cloud_storage_provider_t *provider;

   VERIFY_INITIALIZED

   settings = config_get_ptr();
   provider = get_active_provider(settings);

   if (active_provider_state)
   {
      cloud_storage_item_t *item;
      cloud_storage_item_t *next;

      active_provider_state->free_data(active_provider_state->provider_data);

      item = active_provider_state->game_states;
      while (item)
      {
         next = item->next;
         cloud_storage_item_free(item);
         item = next;
      }

      item = active_provider_state->game_saves;
      while (item)
      {
         next = item->next;
         cloud_storage_item_free(item);
         item = next;
      }
   } else
   {
      active_provider_state = (cloud_storage_provider_state_t *)malloc(sizeof(cloud_storage_provider_state_t));
   }

   active_provider_state->backoff_end = 0;
   active_provider_state->retry_count = 0;
   active_provider_state->status = CLOUD_STORAGE_PROVIDER_READY;
   active_provider_state->game_states = NULL;
   active_provider_state->game_saves = NULL;
   active_provider_state->provider_data = provider->initialize(settings);
   active_provider_state->free_data = provider->free_provider_data;
}

bool cloud_storage_need_authorization(void)
{
   settings_t *settings;
   cloud_storage_provider_t *provider;

   VERIFY_INITIALIZED

   settings = config_get_ptr();
   provider = get_active_provider(settings);

   return provider->need_authorization;
}
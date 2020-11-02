/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (driver_utils.c).
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

#include <encodings/utf.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "driver_utils.h"
#include "cloud_storage.h"
#include "json.h"

char *cloud_storage_join_strings(size_t *length, ...)
{
   va_list valist;
   int count = 0;
   char *result;
   size_t *lengths;
   size_t current_pos = 0;
   int i;

   va_start(valist, length);
   while (va_arg(valist, char *))
   {
      count++;
   }
   va_end(valist);

   if (count == 0)
   {
      return NULL;
   }

   lengths = (size_t *)malloc(count * sizeof(size_t));

   va_start(valist, length);
   for (i = 0;i < count;i++)
   {
      lengths[i] = strlen(va_arg(valist, char *));
      current_pos += lengths[i];
   }
   va_end(valist);

   result = (char *)calloc(current_pos + 1, sizeof(char));

   current_pos = 0;
   va_start(valist, length);
   for (i = 0;i < count;i++)
   {
      strcpy(result + current_pos, va_arg(valist, char *));
      current_pos += lengths[i];
   }
   va_end(valist);

   if (length)
   {
      *length = current_pos;
   }

   return result;
}

cloud_storage_item_t *cloud_storage_clone_item_list(cloud_storage_item_t *items)
{
   cloud_storage_item_t *new_list = NULL;
   cloud_storage_item_t *current_item = NULL;

   while (items)
   {
      cloud_storage_item_t *new_item;

      new_item = (cloud_storage_item_t *)malloc(sizeof(cloud_storage_item_t));

      if (items->id)
      {
         size_t length;

         length = strlen(items->id) + 1;
         new_item->id = (char *)malloc(length);
         strcpy(new_item->id, items->id);
      } else
      {
         new_item->id = NULL;
      }

      new_item->item_type = items->item_type;

      if (items->name)
      {
         size_t length;

         length = strlen(items->name) + 1;
         new_item->name = (char *)malloc(length);
         strcpy(new_item->name, items->name);
      } else
      {
         new_item->name = NULL;
      }

      if (items->item_type == CLOUD_STORAGE_FILE)
      {
         if (items->type_data.file.download_url)
         {
            size_t length;

            length = strlen(items->type_data.file.download_url) + 1;
            new_item->type_data.file.download_url = (char *)malloc(length);
            strcpy(new_item->type_data.file.download_url, items->type_data.file.download_url);
         } else
         {
            new_item->type_data.file.download_url = NULL;
         }

         new_item->type_data.file.hash_type = items->type_data.file.hash_type;

         if (items->type_data.file.hash_value)
         {
            size_t length;

            length = strlen(items->type_data.file.hash_value) + 1;
            new_item->type_data.file.hash_value = (char *)malloc(length);
            strcpy(new_item->type_data.file.hash_value, items->type_data.file.hash_value);
         } else
         {
            new_item->type_data.file.hash_value = NULL;
         }
      } else
      {
         new_item->type_data.folder.children = NULL;
      }

      new_item->next = NULL;

      if (!new_list)
      {
         new_list = new_item;
         current_item = new_item;
      } else
      {
         current_item->next = new_item;
      }

      current_item = new_item;
      items = items->next;
   }

   return new_list;
}

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

char *get_temp_directory_alloc(void)
{
   char *path             = NULL;
#ifdef _WIN32
#ifdef LEGACY_WIN32
   DWORD path_length      = GetTempPath(0, NULL) + 1;
   path                   = (char*)malloc(path_length * sizeof(char));

   if (!path)
      return NULL;

   path[path_length - 1]   = 0;
   GetTempPath(path_length, path);
#else
   DWORD path_length = GetTempPathW(0, NULL) + 1;
   wchar_t *wide_str = (wchar_t*)malloc(path_length * sizeof(wchar_t));

   if (!wide_str)
      return NULL;

   wide_str[path_length - 1] = 0;
   GetTempPathW(path_length, wide_str);

   path = utf16_to_utf8_string_alloc(wide_str);
   free(wide_str);
#endif
#elif defined ANDROID
   {
      settings_t *settings = configuration_settings;
      path = (char *)calloc(strlen(settings->paths.directory_libretro) + 1, size(char));
      strcpy(path, settings->paths.directory_libretro);
   }
#else
   if (getenv("TMPDIR"))
   {
      path = (char *)calloc(strlen(getenv("TMPDIR")) + 1, sizeof(char));
      strcpy(path, getenv("TMPDIR"));
   } else
   {
      path = (char *)calloc(5, sizeof(char));
      strcpy(path, "/tmp");
   }

#endif
   return path;
}

bool cloud_storage_save_access_token(char *driver_name, char *new_access_token, time_t expiration_time)
{
   settings_t *settings;
   char *tmp_dir = NULL;
   char *dir;
   char *file_name;
   char *file_path = NULL;
   size_t file_path_len;
   RFILE *file = NULL;
   bool access_token_saved = false;
   int x;

   settings = config_get_ptr();

   if (strlen(settings->paths.directory_cache) == 0)
   {
      tmp_dir = get_temp_directory_alloc();
      dir = tmp_dir;
   } else
   {
      dir = settings->paths.directory_cache;
   }

   file_name = cloud_storage_join_strings(
      NULL,
      driver_name,
      "_access_token.json",
      NULL
   );
   file_path_len = strlen(dir) + strlen(file_name) + 2;
   file_path = (char *)calloc(file_path_len, sizeof(char));
   fill_pathname_join(file_path, dir, file_name, file_path_len);
   free(file_name);

   file = filestream_open(file_path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
   {
      goto cleanup;
   }

   if (!filestream_printf(file, "{\n  \"access_token\": \"%s\",\n  \"expiration_time\": %ld\n}",
      new_access_token, expiration_time))
   {
      goto cleanup;
   }
   if (filestream_flush(file))
   {
      goto cleanup;
   }

   access_token_saved = true;

cleanup:
   if (tmp_dir)
   {
      free(tmp_dir);
   }

   if (file_path)
   {
      free(file_path);
   }

   if (file)
   {
      filestream_close(file);
   }

   return access_token_saved;
}

bool cloud_storage_save_file(char *file_name, uint8_t *data, size_t data_len)
{
   RFILE *file;
   char *local_folder;
   int64_t offset = 0;
   int64_t bytes_written;
   bool result = false;

   file = filestream_open(file_name, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
   {
      return false;
   }

   while (!filestream_error(file) && offset < data_len)
   {
      bytes_written = filestream_write(file, data + offset, data_len - offset);
      if (!filestream_error(file))
      {
         offset += bytes_written;
      }
   }

   filestream_flush(file);
   filestream_close(file);

   return true;
}

void cloud_storage_load_access_token(char *driver_name, char **access_token, int64_t *expiration_time)
{
   settings_t *settings;
   char *tmp_dir = NULL;
   char *dir;
   char *file_name;
   char *file_path = NULL;
   size_t file_path_len;
   RFILE *file = NULL;
   int64_t file_size;
   char *file_contents = NULL;
   int64_t offset;
   int64_t bytes_read;
   struct json_node_t *json = NULL;
   char *parsed_access_token = NULL;
   size_t token_length;
   int64_t parsed_expiration_time;

   settings = config_get_ptr();

   *access_token = NULL;
   *expiration_time = 0;

   if (strlen(settings->paths.directory_cache) == 0)
   {
      tmp_dir = get_temp_directory_alloc();
      dir = tmp_dir;
   } else
   {
      dir = settings->paths.directory_cache;
   }

   file_name = cloud_storage_join_strings(
      NULL,
      driver_name,
      "_access_token.json",
      NULL
   );
   file_path_len = strlen(dir) + strlen(file_name) + 2;
   file_path = (char *)calloc(file_path_len, sizeof(char));
   fill_pathname_join(file_path, dir, file_name, file_path_len);
   free(file_name);

   file = filestream_open(file_path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
   {
      goto cleanup;
   }

   file_size = filestream_get_size(file);
   file_contents = (char *)malloc(file_size + 1);
   file_contents[file_size] = '\0';

   offset = 0;
   bytes_read = filestream_read(file, file_contents, file_size);
   while (bytes_read + offset < file_size)
   {
      bytes_read = filestream_read(file, file_contents + offset, file_size + offset);
   }
   filestream_close(file);

   json = string_to_json(file_contents);

   if (!json)
   {
      goto cleanup;
   }

   if (json->node_type != OBJECT_VALUE)
   {
      goto cleanup;
   }

   if (!json_map_get_value_string(json->value.map_value, "access_token", &parsed_access_token, &token_length))
   {
      goto cleanup;
   }

   if (!json_map_get_value_int(json->value.map_value, "expiration_time", &parsed_expiration_time))
   {
      goto cleanup;
   }
   if (time(NULL) > parsed_expiration_time)
   {
      goto cleanup;
   }

   *access_token = (char *)calloc(token_length + 1, sizeof(char));
   strncpy(*access_token, parsed_access_token, token_length);
   *expiration_time = parsed_expiration_time;

cleanup:
   if (file_contents)
   {
      free(file_contents);
   }
   if (tmp_dir)
   {
      free(tmp_dir);
   }

   if (file_path)
   {
      free(file_path);
   }
   if (json)
   {
      json_node_free(json);
   }
}

int64_t cloud_storage_get_file_size(char *local_file)
{
   RFILE *file;
   int64_t file_size;

   file = filestream_open(local_file, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
   {
      return -1;
   }

   file_size = filestream_get_size(file);
   filestream_close(file);

   return file_size;
}

void cloud_storage_add_request_body_data(
   char *filename,
   size_t offset,
   size_t segment_length,
   struct http_request_t *request)
{
   RFILE *file;
   uint8_t *body;

   file = filestream_open(filename, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (offset > 0)
   {
      filestream_seek(file, offset, SEEK_SET);
   }

   body = malloc(segment_length);
   filestream_read(file, body, segment_length);
   filestream_close(file);

   net_http_request_set_body(request, body, segment_length);
}

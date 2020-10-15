/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (onedrive.c).
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

#include <configuration.h>
#include <net/net_http.h>
#include <rest/rest.h>
#include <string/stdstring.h>

#include "../cloud_storage.h"
#include "../json.h"
#include "../driver_utils.h"
#include "onedrive.h"
#include "onedrive_internal.h"

#define ACCESS_TOKEN_FILE "onedrive_access_token.json"

char *_onedrive_access_token = NULL;
time_t _onedrive_access_token_expiration_time = 0;

static bool _ready_for_request()
{
   settings_t *settings;

   settings = config_get_ptr();

   return strlen(settings->arrays.cloud_storage_onedrive_refresh_token) > 0;
}

cloud_storage_item_t *cloud_storage_onedrive_parse_file_from_json(struct json_map_t file_json)
{
   cloud_storage_item_t *file;
   char *temp_string;
   size_t temp_string_length;
   struct json_map_t *file_resource;

   file = (cloud_storage_item_t *)malloc(sizeof(cloud_storage_item_t));
   file->next = NULL;

   if (!json_map_get_value_string(file_json, "id", &temp_string, &temp_string_length))
   {
      goto cleanup;
   }
   file->id = (char *)calloc(temp_string_length + 1, sizeof(char));
   strncpy(file->id, temp_string, temp_string_length);

   if (!json_map_get_value_string(file_json, "name", &temp_string, &temp_string_length))
   {
      goto cleanup;
   }
   file->name = (char *)calloc(temp_string_length + 1, sizeof(char));
   strncpy(file->name, temp_string, temp_string_length);

   if (json_map_get_value_map(file_json, "file", &file_resource))
   {
      struct json_map_t *hashes;

      file->item_type = CLOUD_STORAGE_FILE;
      file->type_data.file.hash_type = SHA256;

      if (!json_map_get_value_string(file_json, "@microsoft.graph.downloadUrl", &temp_string, &temp_string_length))
      {
         goto cleanup;
      }
      file->type_data.file.download_url = (char *)calloc(temp_string_length + 1, sizeof(char));
      strncpy(file->type_data.file.download_url, temp_string, temp_string_length);

      if (json_map_get_value_map(*file_resource, "hashes", &hashes))
      {
         if (json_map_get_value_string(*hashes, "sha256Hash", &temp_string, &temp_string_length))
         {
            file->type_data.file.hash_value = (char *)calloc(temp_string_length + 1, sizeof(char));
            strncpy(file->type_data.file.hash_value, temp_string, temp_string_length);
         }
      }
   } else if (json_map_get_value_map(file_json, "folder", &file_resource))
   {
      file->item_type = CLOUD_STORAGE_FOLDER;
      file->type_data.folder.children = NULL;
   }

   file->last_sync_time = time(NULL);
   file->next = NULL;

   return file;

cleanup:
   if (file)
   {
      if (file->name)
      {
         free(file->name);
      }
      if (file->id)
      {
         free(file->id);
      }
   }

   return NULL;
}

cloud_storage_provider_t *cloud_storage_onedrive_create()
{
   cloud_storage_provider_t *provider;

   provider = (cloud_storage_provider_t *)malloc(sizeof(cloud_storage_provider_t));

   provider->need_authorization = true;
   provider->ready_for_request = _ready_for_request;
   provider->authenticate = cloud_storage_onedrive_authenticate;
   provider->authorize = cloud_storage_onedrive_authorize;
   provider->list_files = cloud_storage_onedrive_list_files;
   provider->download_file = cloud_storage_onedrive_download_file;
   provider->upload_file = cloud_storage_onedrive_upload_file;
   provider->get_folder_metadata = cloud_storage_onedrive_get_folder_metadata;
   provider->get_file_metadata = cloud_storage_onedrive_get_file_metadata;
   provider->get_file_metadata_by_name = cloud_storage_onedrive_get_file_metadata_by_name;
   provider->delete_file = cloud_storage_onedrive_delete_file;
   provider->create_folder = cloud_storage_onedrive_create_folder;

   return provider;
}

bool cloud_storage_onedrive_add_authorization_header(
   rest_request_t *rest_request)
{
   char *value;

   if (_onedrive_access_token && time(NULL) - 30 > _onedrive_access_token_expiration_time)
   {
      free(_onedrive_access_token);
   }

   if (!_onedrive_access_token && !cloud_storage_onedrive_authenticate())
   {
      return false;
   }

   value = (char *)calloc(8 + strlen(_onedrive_access_token), sizeof(char));
   strcpy(value, "Bearer ");
   strcpy(value + 7, _onedrive_access_token);

   rest_request_set_header(rest_request, "Authorization", value, true);
   free(value);

   return true;
}

struct http_response_t *onedrive_rest_execute_request(rest_request_t *rest_request)
{
   if (!cloud_storage_onedrive_add_authorization_header(rest_request))
   {
      return NULL;
   }

   return rest_request_execute(rest_request);
}
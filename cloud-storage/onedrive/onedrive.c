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
#include <formats/rjson.h>
#include <net/net_http.h>
#include <rest/rest.h>
#include <string/stdstring.h>

#include "../cloud_storage.h"
#include "../driver_utils.h"
#include "onedrive.h"
#include "onedrive_internal.h"

#define CLOUD_STORAGE_ONEDRIVE_DEFAULT_CLIENT_ID ""

char *_onedrive_access_token = NULL;
time_t _onedrive_access_token_expiration_time = 0;

static bool _ready_for_request(void)
{
   settings_t *settings;

   settings = config_get_ptr();

   return strlen(settings->arrays.cloud_storage_onedrive_refresh_token) > 0;
}

bool cloud_storage_onedrive_have_default_credentials(void)
{
   return strlen(CLOUD_STORAGE_ONEDRIVE_DEFAULT_CLIENT_ID) > 0;
}

const char *cloud_storage_onedrive_get_client_id(void)
{
   settings_t *settings;

   settings = config_get_ptr();

   if (settings->bools.cloud_storage_onedrive_default_creds)
   {
      return CLOUD_STORAGE_ONEDRIVE_DEFAULT_CLIENT_ID;
   } else
   {
      return settings->arrays.cloud_storage_onedrive_client_id;
   }
}

static void _parse_hashes_data(cloud_storage_item_t *file, rjson_t *json)
{
   const char *key_name;
   size_t key_name_len;
   int depth = 0;

   for (;;)
   {
      switch (rjson_next(json))
      {
         case RJSON_STRING:
            if (depth > 0)
            {
               break;
            }

            if ((rjson_get_context_count(json) & 1) == 1)
            {
               key_name = rjson_get_string(json, &key_name_len);
            } else if (strcmp("sha256Hash", key_name) == 0)
            {
               const char *value;
               size_t value_len;

               value = rjson_get_string(json, &value_len);
               file->type_data.file.hash_value = (char *)malloc(value_len + 1);
               strcpy(file->type_data.file.hash_value, value);
               file->type_data.file.hash_type = SHA256;
            }

            break;
         case RJSON_OBJECT:
         case RJSON_ARRAY:
            depth++;
            break;
         case RJSON_OBJECT_END:
            if (depth == 0)
            {
               return;
            }
         case RJSON_ARRAY_END:
            depth--;
            break;
         default:
            break;
      }
   }
}

static void _parse_file_data(cloud_storage_item_t *file, rjson_t *json)
{
   const char *key_name;
   size_t key_name_len;
   int depth = 0;

   file->item_type = CLOUD_STORAGE_FILE;
   for (;;)
   {
      switch (rjson_next(json))
      {
         case RJSON_STRING:
            if (depth > 0)
            {
               break;
            }

            if ((rjson_get_context_count(json) & 1) == 1)
            {
               key_name = rjson_get_string(json, &key_name_len);
            } else if (strcmp("@microsoft.graph.downloadUrl", key_name) == 0)
            {
               const char *value;
               size_t value_len;

               value = rjson_get_string(json, &value_len);
               file->type_data.file.download_url = (char *)malloc(value_len + 1);
               strcpy(file->type_data.file.download_url, value);
            }
         case RJSON_OBJECT:
            if (strcmp("hashes", key_name) == 0)
            {
               _parse_hashes_data(file, json);
               break;
            }
         case RJSON_ARRAY:
            depth++;
            break;
         case RJSON_OBJECT_END:
            if (depth == 0)
            {
               return;
            }
         case RJSON_ARRAY_END:
            depth--;
            break;
         default:
            break;
      }
   }
}

cloud_storage_item_t *cloud_storage_onedrive_parse_file_from_json(rjson_t *json)
{
   cloud_storage_item_t *file;
   int depth = 0;
   const char *temp_string;
   size_t temp_string_length;
   const char *key_name;
   size_t key_name_len;

   file = (cloud_storage_item_t *)calloc(1, sizeof(cloud_storage_item_t));

   for (;;)
   {
      switch (rjson_next(json))
      {
         case RJSON_STRING:
            if (depth == 0)
            {
               if ((rjson_get_context_count(json) & 1) == 1)
               {
                  key_name = rjson_get_string(json, &key_name_len);
               } else if (strcmp("id", key_name) == 0)
               {
                  temp_string = rjson_get_string(json, &temp_string_length);
                  file->id = (char *)malloc(temp_string_length + 1);
                  strcpy(file->id, temp_string);
               } else if (strcmp("name", key_name) == 0)
               {
                  temp_string = rjson_get_string(json, &temp_string_length);
                  file->name = (char *)malloc(temp_string_length + 1);
                  strcpy(file->name, temp_string);
               }
            }

            break;
         case RJSON_OBJECT:
            if (depth == 0)
            {
               if (strcmp("file", key_name) == 0)
               {
                  file->item_type = CLOUD_STORAGE_FILE;
                  _parse_file_data(file, json);
                  break;
               } else if (strcmp("folder", key_name) == 0)
               {
                  file->item_type = CLOUD_STORAGE_FOLDER;
               }
            }
         case RJSON_ARRAY:
            depth++;
            break;
         case RJSON_OBJECT_END:
         case RJSON_ARRAY_END:
            depth--;
            break;
         default:
            break;
      }
   }

   rjson_free(json);

   if (!file->name || !file->id || (file->item_type == CLOUD_STORAGE_FILE &&
      (!file->type_data.file.download_url || !file->type_data.file.hash_value)))
   {
      if (file->name)
      {
         free(file->name);
      }
      if (file->id)
      {
         free(file->id);
      }
      if (file->item_type == CLOUD_STORAGE_FILE)
      {
         if (file->type_data.file.download_url)
         {
            free(file->type_data.file.download_url);
         }
         if (file->type_data.file.hash_value)
         {
            free(file->type_data.file.hash_value);
         }
      }

      free(file);
      return NULL;
   }

   file->last_sync_time = time(NULL);
   file->next = NULL;

   return file;
}

cloud_storage_provider_t *cloud_storage_onedrive_create(void)
{
   cloud_storage_provider_t *provider;

   provider = (cloud_storage_provider_t *)malloc(sizeof(cloud_storage_provider_t));

   provider->need_authorization = true;
   provider->have_default_credentials = cloud_storage_onedrive_have_default_credentials;
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

   if (!_onedrive_access_token)
   {
      cloud_storage_load_access_token("onedrive", &_onedrive_access_token, &_onedrive_access_token_expiration_time);
      if (!_onedrive_access_token && !cloud_storage_onedrive_authenticate())
      {
         return false;
      }
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
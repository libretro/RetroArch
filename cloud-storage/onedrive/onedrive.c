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

#include <string/stdstring.h>

#include "../cloud_storage.h"
#include "../rest-lib/rest_api.h"
#include "../json.h"
#include "../driver_utils.h"
#include "onedrive.h"
#include "onedrive_internal.h"

#define ACCESS_TOKEN_FILE "onedrive_access_token.json"

static void free_provider_data(void *provider_data)
{
   struct cloud_storage_onedrive_provider_data_t *onedrive_provider_data;

   onedrive_provider_data = (struct cloud_storage_onedrive_provider_data_t *)provider_data;

   if (onedrive_provider_data->access_token)
   {
      free(onedrive_provider_data->access_token);
   }

   free(onedrive_provider_data);
}

static void *initialize(settings_t *settings)
{
   struct cloud_storage_onedrive_provider_data_t *provider_data;

   provider_data = (struct cloud_storage_onedrive_provider_data_t *)calloc(1, sizeof(struct cloud_storage_onedrive_provider_data_t));
   cloud_storage_load_access_token("onedrive", &(provider_data->access_token), &(provider_data->access_token_expiration_time));

   if (strlen(settings->arrays.cloud_storage_onedrive_refresh_token) > 0)
   {
      provider_data->refresh_token = settings->arrays.cloud_storage_onedrive_refresh_token;
   }

   return provider_data;
}

static cloud_storage_status_response_t get_status(cloud_storage_provider_state_t *provider_state)
{
   struct cloud_storage_onedrive_provider_data_t *provider_data;

   provider_data = (struct cloud_storage_onedrive_provider_data_t *)provider_state->provider_data;
   if (!provider_data->refresh_token)
   {
      return CLOUD_STORAGE_STATUS_NOT_READY;
   } else if (!provider_data->access_token || time(NULL) >= provider_data->access_token_expiration_time - 5)
   {
      return CLOUD_STORAGE_STATUS_NEED_AUTHENTICATION;
   } else
   {
      return CLOUD_STORAGE_STATUS_READY;
   }
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
      file->type_data.file.hash_type = SHA1;

      if (!json_map_get_value_string(file_json, "@microsoft.graph.downloadUrl", &temp_string, &temp_string_length))
      {
         goto cleanup;
      }
      file->type_data.file.download_url = (char *)calloc(temp_string_length + 1, sizeof(char));
      strncpy(file->type_data.file.download_url, temp_string, temp_string_length);

      if (json_map_get_value_map(*file_resource, "hashes", &hashes))
      {
         if (json_map_get_value_string(*hashes, "sha1Hash", &temp_string, &temp_string_length))
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

   provider->max_retries = 2;
   provider->retry_delays = (time_t *)malloc(2 * sizeof(time_t));
   provider->retry_delays[0] = 1;
   provider->retry_delays[1] = 10;

   provider->initialize = initialize;
   provider->need_authorization = true;
   provider->get_status = get_status;
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

static void internal_server_error_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *operation_state)
{
   operation_state->complete = true;

   operation_state->callback(
      operation_state,
      false);
}

static void intermediate_auth_callback(
   cloud_storage_continuation_data_t *continuation_data,
   bool success,
   void *data)
{
   rest_api_request_t *request;

   request = (rest_api_request_t *)data;
   rest_api_request_execute(request);
}

static void clear_access_token(struct cloud_storage_onedrive_provider_data_t *provider_data)
{
   if (provider_data->access_token)
   {
      free(provider_data->access_token);
   }

   provider_data->access_token_expiration_time = 0;
}

static void auth_failed_response_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   struct cloud_storage_onedrive_provider_data_t *provider_data;

   provider_data = (struct cloud_storage_onedrive_provider_data_t *)state->continuation_data->provider_state->provider_data;

   clear_access_token(provider_data);
   cloud_storage_onedrive_authenticate(
      state->continuation_data,
      intermediate_auth_callback,
      request
   );
}

static void default_response_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   state->complete = true;

   state->callback(
      state,
      false);
}

void cloud_storage_onedrive_add_authorization_header(
   struct cloud_storage_onedrive_provider_data_t *provider_data,
   struct http_request_t *request)
{
   char *value;

   value = (char *)calloc(8 + strlen(provider_data->access_token), sizeof(char));
   strcpy(value, "Bearer ");
   strcpy(value + 7, provider_data->access_token);

   net_http_request_set_header(request, "Authorization", value, true);
   free(value);
}

void onedrive_rest_execute_request(
   rest_api_request_t *rest_request,
   cloud_storage_operation_state_t *state)
{
   struct http_request_t *http_request;
   struct cloud_storage_onedrive_provider_data_t *provider_data;

   provider_data = (struct cloud_storage_onedrive_provider_data_t *)state->continuation_data->provider_state->provider_data;

   cloud_storage_onedrive_add_authorization_header(
      provider_data,
      rest_api_get_http_request(rest_request));

   rest_api_request_set_response_handler(rest_request, 500, true, internal_server_error_handler, state);
   rest_api_request_set_response_handler(rest_request, 401, false, auth_failed_response_handler, state);
   if (!rest_api_request_get_default_callback(rest_request))
   {
      rest_api_request_set_default_response_handler(rest_request, true, default_response_handler, state);
   }

   rest_api_request_execute(rest_request);
}
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

#include "../cloud_storage.h"
#include "../rest-lib/rest_api.h"
#include "../json.h"
#include "../driver_utils.h"
#include "onedrive_internal.h"

#define LIST_FILES_URL "https://graph.microsoft.com/v1.0/me/drive/special/approot"
#define BY_ID_URL "https://graph.microsoft.com/v1.0/me/drive/items/"

static struct http_request_t *create_folder_http_request(char *folder_name)
{
   struct http_request_t *request;
   char *parent_folder_id;
   char *url;

   request = net_http_request_new();

   url = cloud_storage_join_strings(
      NULL,
      LIST_FILES_URL,
      ":/",
      folder_name,
      ":",
      NULL
   );

   net_http_request_set_url(request, url);
   net_http_request_set_method(request, "GET");

   net_http_request_set_log_request_body(request, true);
   net_http_request_set_log_response_body(request, true);

   return request;
}

static struct http_request_t *create_file_by_id_http_request(cloud_storage_item_t *file)
{
   struct http_request_t *request;
   char *url;
   char *parent_folder_id;

   request = net_http_request_new();

   url = cloud_storage_join_strings(
      NULL,
      BY_ID_URL,
      file->id,
      NULL
   );

   net_http_request_set_url(request, url);
   free(url);
   net_http_request_set_method(request, "GET");

   net_http_request_set_log_request_body(request, true);
   net_http_request_set_log_response_body(request, true);

   return request;
}

static struct http_request_t *create_file_by_name_http_request(
   char *filename,
   cloud_storage_item_t *folder)
{
   struct http_request_t *request;
   char *parent_folder_id;
   char *url;

   request = net_http_request_new();

   url = cloud_storage_join_strings(
      NULL,
      LIST_FILES_URL,
      ":/",
      folder->name,
      ":/",
      filename,
      NULL
   );
   net_http_request_set_url(request, url);
   free(url);

   net_http_request_set_log_request_body(request, true);
   net_http_request_set_log_response_body(request, true);

   return request;
}

static void not_found_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   state->callback(state, true);
   state->complete = true;
}

static void success_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   char *json_text;
   struct json_node_t *json;
   cloud_storage_item_t *metadata = NULL;
   uint8_t *data;
   size_t data_len;

   data = net_http_response_get_data(response, &data_len, false);

   json_text = (char *)malloc(data_len + 1);
   strncpy(json_text, (char *)data, data_len);
   json_text[data_len] = '\0';

   json = string_to_json(json_text);

   if (json)
   {
      struct json_array_t *files_list;
      struct json_map_t *file;

      if (json->node_type == OBJECT_VALUE)
      {
         metadata = cloud_storage_onedrive_parse_file_from_json(json->value.map_value);
      }

      json_node_free(json);
   }

   free(json_text);

   state->result = metadata;
   state->callback(state, true);

   state->complete = true;
}

void cloud_storage_onedrive_get_folder_metadata(
   char *folder_name,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback)
{
   cloud_storage_operation_state_t *state;
   struct http_request_t *http_request;
   rest_api_request_t *rest_request;

   state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   state->continuation_data = continuation_data;
   state->callback = callback;

   http_request = create_folder_http_request(folder_name);

   rest_request = rest_api_request_new(http_request);
   rest_api_request_set_response_handler(rest_request, 200, false, success_handler, state);
   rest_api_request_set_response_handler(rest_request, 404, false, not_found_handler, state);

   onedrive_rest_execute_request(rest_request, state);
}

void cloud_storage_onedrive_get_file_metadata(
   cloud_storage_item_t *file,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback)
{
   cloud_storage_operation_state_t *state;
   struct http_request_t *http_request;
   rest_api_request_t *rest_request;

   state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   state->continuation_data = continuation_data;
   state->callback = callback;

   http_request = create_file_by_id_http_request(file);

   rest_request = rest_api_request_new(http_request);
   rest_api_request_set_response_handler(rest_request, 200, false, success_handler, state);
   rest_api_request_set_response_handler(rest_request, 404, false, not_found_handler, state);

   onedrive_rest_execute_request(rest_request, state);
}

void cloud_storage_onedrive_get_file_metadata_by_name(
   cloud_storage_item_t *folder,
   char *filename,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback)
{
   cloud_storage_operation_state_t *state;
   struct http_request_t *http_request;
   rest_api_request_t *rest_request;

   state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   state->continuation_data = continuation_data;
   state->callback = callback;

   http_request = create_file_by_name_http_request(filename, folder);

   rest_request = rest_api_request_new(http_request);
   rest_api_request_set_response_handler(rest_request, 200, false, success_handler, state);
   rest_api_request_set_response_handler(rest_request, 404, false, not_found_handler, state);

   onedrive_rest_execute_request(rest_request, state);
}

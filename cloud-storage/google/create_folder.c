/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (create_folder.c).
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
#include "google_internal.h"

#define CREATE_FILE_URL "https://www.googleapis.com/drive/v3/files"

static cloud_storage_item_t *parse_create_folder_response(
   char *folder_name,
   uint8_t *response,
   size_t response_len)
{
   char *json_text;
   struct json_node_t *json;
   struct json_map_t item;
   char *id;
   size_t id_length;
   cloud_storage_item_t *metadata = NULL;

   json_text = (char *)malloc(response_len + 1);
   strncpy(json_text, (char *)response, response_len);
   json_text[response_len] = '\0';

   json = string_to_json(json_text);
   json_text = NULL;

   if (!json || json->node_type != OBJECT_VALUE)
   {
      goto cleanup;
   }

   metadata = (cloud_storage_item_t *)calloc(1, sizeof(cloud_storage_item_t));
   metadata->name = folder_name;
   metadata->item_type = CLOUD_STORAGE_FOLDER;

   item = json->value.map_value;

   if (json_map_get_value_string(item, "id", &id, &id_length))
   {
      metadata->id = (char *)calloc(id_length + 1, sizeof(char));
      strncpy(metadata->id, id, id_length);
   }

cleanup:
   free(json_text);
   if (json)
   {
      json_node_free(json);
   }

   if (metadata && !metadata->id)
   {
      free(metadata);
   }

   return metadata;
}

static void success_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   uint8_t *response_data;
   size_t response_len;

   response_data = net_http_response_get_data(response, &response_len, false);
   state->result = parse_create_folder_response((char *)state->extra_state, response_data, response_len);

   if (state->result)
   {
      state->callback(state, true);
   } else
   {
      state->callback(state, false);
   }

   state->complete = true;
}

static void free_extra_state(void *extra_state)
{
   free(extra_state);
}

void cloud_storage_google_create_folder(
   char *folder_name,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback)
{
   char *body;
   size_t body_len;
   struct http_request_t *http_request;
   rest_api_request_t *rest_request;
   cloud_storage_operation_state_t *state;

   http_request = net_http_request_new();

   net_http_request_set_url(http_request, CREATE_FILE_URL);
   net_http_request_set_method(http_request, "POST");

   net_http_request_set_header(http_request, "Content-Type", "application/json", true);

   body = cloud_storage_join_strings(
      &body_len,
      "{\"name\": \"",
      folder_name,
      "\", \"mimeType\": \"application/vnd.google-apps.folder\", \"parents\": [\"appDataFolder\"]}",
      NULL
   );
   net_http_request_set_body(http_request, (uint8_t *)body, body_len);

   net_http_request_set_log_request_body(http_request, true);
   net_http_request_set_log_response_body(http_request, true);

   state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   state->continuation_data = continuation_data;
   state->callback = callback;
   state->extra_state = folder_name;
   state->free_extra_state = free_extra_state;

   rest_request = rest_api_request_new(http_request);
   rest_api_request_set_response_handler(rest_request, 200, false, success_handler, state);
   rest_api_request_set_response_handler(rest_request, 201, false, success_handler, state);

   google_rest_execute_request(rest_request, state);
}
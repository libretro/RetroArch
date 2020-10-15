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

#include <net/net_http.h>
#include <rest/rest.h>

#include "../cloud_storage.h"
#include "../json.h"
#include "../driver_utils.h"
#include "google_internal.h"

#define CREATE_FILE_URL "https://www.googleapis.com/drive/v3/files"

static cloud_storage_item_t *_parse_create_folder_response(
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

cloud_storage_item_t *cloud_storage_google_create_folder(char *folder_name)
{
   char *body;
   size_t body_len;
   struct http_request_t *http_request;
   rest_request_t *rest_request;
   struct http_response_t *http_response = NULL;
   cloud_storage_item_t *new_folder = NULL;

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

   rest_request = rest_request_new(http_request);
   http_response = google_rest_execute_request(rest_request);
   if (!http_response)
   {
      goto complete;
   }

   switch (net_http_response_get_status(http_response))
   {
      case 200:
      case 201:
         {
            uint8_t *response_data;
            size_t response_len;

            response_data = net_http_response_get_data(http_response, &response_len, false);
            new_folder = _parse_create_folder_response(folder_name, response_data, response_len);
            break;
         }
      default:
         break;
   }

complete:
   if (http_response)
   {
      net_http_response_free(http_response);
   }
   rest_request_free(rest_request);

   return new_folder;
}
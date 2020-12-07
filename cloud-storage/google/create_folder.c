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

#include <formats/rjson.h>
#include <net/net_http.h>
#include <rest/rest.h>

#include "../cloud_storage.h"
#include "../driver_utils.h"
#include "google_internal.h"

#define CREATE_FILE_URL "https://www.googleapis.com/drive/v3/files"

static cloud_storage_item_t *_parse_create_folder_response(
   const char *folder_name,
   uint8_t *response,
   size_t response_len)
{
   rjson_t *json;
   const char *id;
   size_t id_length;
   bool in_object = false;
   bool in_id = false;
   cloud_storage_item_t *metadata = NULL;

   json = rjson_open_buffer(response, response_len);
   for (;;)
   {
      switch (rjson_next(json))
      {
         case RJSON_ERROR:
            goto cleanup;
         case RJSON_OBJECT:
            if (in_object)
            {
               goto cleanup;
            } else
            {
               in_object = true;
            }
            break;
         case RJSON_STRING:
            if (!in_object)
            {
               goto cleanup;
            }

            if ((rjson_get_context_count(json) & 1) == 1)
            {
               size_t value_len;

               if (strcmp("id", rjson_get_string(json, &value_len)) == 0)
               {
                  in_id = true;
               }
            } else if (in_id)
            {
               id = rjson_get_string(json, &id_length);
               in_id = false;
               goto id_found;
            }

            break;
         default:
            goto cleanup;
      }
   }

id_found:
   metadata = (cloud_storage_item_t *)calloc(sizeof(cloud_storage_item_t), 1);
   metadata->name = (char *)calloc(strlen(folder_name) + 1, sizeof(char));
   strcpy(metadata->name, folder_name);
   metadata->item_type = CLOUD_STORAGE_FOLDER;
   metadata->id = (char *)calloc(id_length + 1, sizeof(char));
   strncpy(metadata->id, id, id_length);
   metadata->last_sync_time = time(NULL);

cleanup:
   if (json)
   {
      rjson_free(json);
   }

   if (metadata && !metadata->id)
   {
      free(metadata);
   }

   return metadata;
}

cloud_storage_item_t *cloud_storage_google_create_folder(const char *folder_name)
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
   net_http_request_set_body_raw(http_request, (uint8_t *)body, body_len);

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
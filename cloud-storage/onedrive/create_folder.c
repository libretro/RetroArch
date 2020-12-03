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
#include "onedrive_internal.h"

#define CREATE_FILE_URL "https://graph.microsoft.com/v1.0/me/drive/special/approot/children"

static cloud_storage_item_t *_parse_create_folder_response(
   const char *folder_name,
   uint8_t *response,
   size_t response_len)
{
   rjson_t *json;
   cloud_storage_item_t *metadata = NULL;
   bool in_object = false;
   const char *key_name;
   size_t key_name_len;

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
               key_name = rjson_get_string(json, &key_name_len);
            } else if (strcmp("id", key_name) == 0)
            {
               const char *id;
               size_t id_length;

               metadata = (cloud_storage_item_t *)calloc(1, sizeof(cloud_storage_item_t));
               metadata->name = (char *)malloc(strlen(folder_name) + 1);
               strcpy(metadata->name, folder_name);
               metadata->item_type = CLOUD_STORAGE_FOLDER;

               id = rjson_get_string(json, &id_length);
               metadata->id = (char *)malloc(id_length + 1);
               strcpy(metadata->id, id);
            }

            break;
         default:
            break;
      }
   }

cleanup:
   rjson_free(json);
   return metadata;
}

cloud_storage_item_t *cloud_storage_onedrive_create_folder(const char *folder_name)
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
      "\", \"folder\": { }, \"@microsoft.graph.conflictBehavior\": \"fail\"}",
      NULL
   );
   net_http_request_set_body_raw(http_request, (uint8_t *)body, body_len);

   rest_request = rest_request_new(http_request);
   http_response = onedrive_rest_execute_request(rest_request);
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
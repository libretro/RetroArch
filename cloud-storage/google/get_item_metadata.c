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

#include <net/net_http.h>
#include <rest/rest.h>

#include "../cloud_storage.h"
#include "../json.h"
#include "../driver_utils.h"
#include "google_internal.h"

#define LIST_FILES_URL "https://www.googleapis.com/drive/v3/files"

static struct http_request_t *_create_folder_http_request(char *folder_name)
{
   struct http_request_t *request;
   char *parent_folder_id;
   char *q;

   request = net_http_request_new();
   net_http_request_set_url(request, LIST_FILES_URL);
   net_http_request_set_method(request, "GET");

   q = cloud_storage_join_strings(
      NULL,
      "name=\"",
      folder_name,
      "\" and mimeType=\"application/vnd.google-apps.folder\" and \"appDataFolder\" in parents",
      NULL);
   net_http_request_set_url_param(request, "q", q, true);
   free(q);

   net_http_request_set_url_param(request, "spaces", "appDataFolder", true);
   net_http_request_set_url_param(request, "fields", "files(id,name,mimeType,md5Checksum)", true);

   net_http_request_set_log_request_body(request, true);
   net_http_request_set_log_response_body(request, true);

   return request;
}

static struct http_request_t *_create_file_by_id_http_request(cloud_storage_item_t *file)
{
   struct http_request_t *request;
   char *url;
   char *parent_folder_id;

   url = cloud_storage_join_strings(
      NULL,
      LIST_FILES_URL,
      "/",
      file->id,
      NULL
   );

   request = net_http_request_new();
   net_http_request_set_url(request, url);
   net_http_request_set_method(request, "GET");

   net_http_request_set_url_param(request, "spaces", "appDataFolder", true);
   net_http_request_set_url_param(request, "fields", "id,name,mimeType,md5Checksum", true);

   net_http_request_set_log_request_body(request, true);
   net_http_request_set_log_response_body(request, true);

   return request;
}

static struct http_request_t *_create_file_by_name_http_request(
   char *filename,
   cloud_storage_item_t *folder)
{
   struct http_request_t *request;
   char *parent_folder_id;
   char *q;

   request = net_http_request_new();
   net_http_request_set_url(request, LIST_FILES_URL);
   net_http_request_set_method(request, "GET");

   q = cloud_storage_join_strings(
      NULL,
      "name=\"",
      filename,
      "\" and \"",
      folder->id,
      "\" in parents",
      NULL
   );
   net_http_request_set_url_param(request, "q", q, true);
   free(q);

   net_http_request_set_url_param(request, "spaces", "appDataFolder", true);
   net_http_request_set_url_param(request, "fields", "files(id,name,mimeType,md5Checksum)", true);

   net_http_request_set_log_request_body(request, true);
   net_http_request_set_log_response_body(request, true);

   return request;
}

static cloud_storage_item_t *_process_metadata_response(
   struct http_response_t *response,
   bool single)
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
      if (single)
      {
         struct json_map_t *file;

         if (json->node_type == OBJECT_VALUE)
         {
            metadata = cloud_storage_google_parse_file_from_json(json->value.map_value);
         }
      } else
      {
         struct json_array_t *files_list;
         struct json_map_t *file;

         if (json->node_type == OBJECT_VALUE && json_map_get_value_array(json->value.map_value, "files", &files_list))
         {
            if (files_list->element && files_list->element->value->node_type == OBJECT_VALUE)
            {
               metadata = cloud_storage_google_parse_file_from_json(files_list->element->value->value.map_value);
            }
         }
      }

      json_node_free(json);
   }

   free(json_text);

   return metadata;
}

static cloud_storage_item_t *_execute_get_metadata_request(struct http_request_t *http_request, bool single)
{
   rest_request_t *rest_request;
   struct http_response_t *http_response = NULL;
   cloud_storage_item_t *metadata = NULL;

   rest_request = rest_request_new(http_request);
   http_response = google_rest_execute_request(rest_request);
   if (!http_response)
   {
      goto complete;
   }

   switch (net_http_response_get_status(http_response))
   {
      case 200:
         metadata = _process_metadata_response(http_response, single);
         break;
      default:
         break;
   }

complete:
   if (http_response)
   {
      net_http_response_free(http_response);
   }
   rest_request_free(rest_request);

   return metadata;
}

cloud_storage_item_t *cloud_storage_google_get_folder_metadata(char *folder_name)
{
   struct http_request_t *http_request;

   http_request = _create_folder_http_request(folder_name);
   return _execute_get_metadata_request(http_request, false);
}

cloud_storage_item_t *cloud_storage_google_get_file_metadata(cloud_storage_item_t *file)
{
   struct http_request_t *http_request;

   http_request = _create_file_by_id_http_request(file);
   return _execute_get_metadata_request(http_request, true);
}

cloud_storage_item_t *cloud_storage_google_get_file_metadata_by_name(
   cloud_storage_item_t *folder,
   char *filename)
{
   struct http_request_t *http_request;

   http_request = _create_file_by_name_http_request(filename, folder);
   return _execute_get_metadata_request(http_request, false);
}

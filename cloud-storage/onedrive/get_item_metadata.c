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

#include <formats/rjson.h>
#include <net/net_http.h>
#include <rest/rest.h>

#include "../cloud_storage.h"
#include "../driver_utils.h"
#include "onedrive_internal.h"

#define LIST_FILES_URL "https://graph.microsoft.com/v1.0/me/drive/special/approot"
#define BY_ID_URL "https://graph.microsoft.com/v1.0/me/drive/items/"

static struct http_request_t *_create_folder_http_request(const char *folder_name)
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

   return request;
}

static struct http_request_t *_create_file_by_id_http_request(cloud_storage_item_t *file)
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

   return request;
}

static struct http_request_t *_create_file_by_name_http_request(
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

   return request;
}

static cloud_storage_item_t *_process_metadata_response(struct http_response_t *response)
{
   rjson_t *json;
   cloud_storage_item_t *metadata = NULL;
   uint8_t *data;
   size_t data_len;

   data = net_http_response_get_data(response, &data_len, false);
   json = rjson_open_buffer(data, data_len);

   for (;;)
   {
      switch (rjson_next(json))
      {
         case RJSON_ERROR:
            goto cleanup;
         case RJSON_OBJECT:
            metadata = cloud_storage_onedrive_parse_file_from_json(json);
            goto cleanup;
         default:
            goto cleanup;
      }
   }

cleanup:
   rjson_free(json);
   return metadata;
}

static cloud_storage_item_t *_execute_get_metadata_request(struct http_request_t *http_request)
{
   rest_request_t *rest_request;
   struct http_response_t *http_response = NULL;
   cloud_storage_item_t *metadata = NULL;

   rest_request = rest_request_new(http_request);
   http_response = onedrive_rest_execute_request(rest_request);
   if (!http_response)
   {
      goto complete;
   }

   switch (net_http_response_get_status(http_response))
   {
      case 200:
         metadata = _process_metadata_response(http_response);
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

cloud_storage_item_t *cloud_storage_onedrive_get_folder_metadata(const char *folder_name)
{
   struct http_request_t *http_request;

   http_request = _create_folder_http_request(folder_name);
   return _execute_get_metadata_request(http_request);
}

cloud_storage_item_t *cloud_storage_onedrive_get_file_metadata(cloud_storage_item_t *file)
{
   struct http_request_t *http_request;

   http_request = _create_file_by_id_http_request(file);
   return _execute_get_metadata_request(http_request);
}

cloud_storage_item_t *cloud_storage_onedrive_get_file_metadata_by_name(
   cloud_storage_item_t *folder,
   char *filename)
{
   struct http_request_t *http_request;

   http_request = _create_file_by_name_http_request(filename, folder);
   return _execute_get_metadata_request(http_request);
}

/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (upload_file.c).
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

#include <streams/file_stream.h>
#include <string/stdstring.h>

#include <net/net_http.h>
#include <rest/rest.h>

#include "../cloud_storage.h"
#include "../json.h"
#include "../driver_utils.h"
#include "onedrive_internal.h"

#define DRIVE_ITEMS_URL "https://graph.microsoft.com/v1.0/me/drive/items/"
#define MAX_SIZE_FOR_SIMPLE_UPLOAD (1 << 22)
#define MAX_BYTES_PER_SEGMENT (320 * (1 << 10))
#define CONTENT_TYPE "application/octet-stream"
#define UPLOAD_URL_KEY "uploadUrl"

static void _process_metadata_response(
   struct http_response_t *response,
   cloud_storage_item_t *remote_file)
{
   uint8_t *data;
   size_t data_len;
   char *json_str;
   struct json_node_t *json;

   data = net_http_response_get_data(response, &data_len, false);

   json_str = (char *)malloc(data_len + 1);
   json_str[data_len] = '\0';
   memcpy(json_str, data, data_len);

   json = string_to_json(json_str);
   if (json && json->node_type == OBJECT_VALUE)
   {
      cloud_storage_item_t *uploaded_item;
      char *temp_str;
      size_t temp_str_len;

      uploaded_item = cloud_storage_onedrive_parse_file_from_json(json->value.map_value);
      if (uploaded_item)
      {
         if (!remote_file->id && uploaded_item->id)
         {
            remote_file->id = (char *)calloc(strlen(uploaded_item->id) + 1, sizeof(char));
            strcpy(remote_file->id, uploaded_item->id);
         }

         if (remote_file->type_data.file.download_url)
         {
            free(remote_file->type_data.file.download_url);

            if (uploaded_item->type_data.file.download_url)
            {
               remote_file->type_data.file.download_url = (char *)calloc(
                  strlen(uploaded_item->type_data.file.download_url) + 1,
                  sizeof(char)
               );
               strcpy(remote_file->type_data.file.download_url, uploaded_item->type_data.file.download_url);
            }
         }

         remote_file->type_data.file.hash_type = SHA1;
         if (remote_file->type_data.file.hash_value)
         {
            free(remote_file->type_data.file.hash_value);

            if (uploaded_item->type_data.file.hash_value)
            {
               remote_file->type_data.file.hash_value = (char *)calloc(
                  strlen(uploaded_item->type_data.file.hash_value) + 1,
                  sizeof(char)
               );
               strcpy(remote_file->type_data.file.hash_value, uploaded_item->type_data.file.hash_value);
            }
         }

         cloud_storage_item_free(uploaded_item);
      }
   }

   if (json)
   {
      json_node_free(json);
   }
   free(json_str);
}

static void _add_http_headers_data(int64_t file_size, int64_t offset, int64_t upload_segment_length, struct http_request_t *request)
{
   char *range_str;
   char *content_len_str;

   range_str = (char *)calloc(105, sizeof(char));
   content_len_str = (char *)calloc(33, sizeof(char));

#if defined(_WIN32) || (_WIN64)
   snprintf(range_str, 105, "bytes %" PRIuPTR "-%" PRIuPTR "/%" PRIuPTR, (long long)offset,
      (long long)(offset + upload_segment_length - 1), (long long)file_size);
   snprintf(content_len_str, 33, "%" PRIuPTR, (long long)upload_segment_length);
#else
   snprintf(range_str, 105, "bytes %lld-%lld/%lld", (long long)offset,
      (long long)(offset + upload_segment_length - 1), (long long)file_size);
   snprintf(content_len_str, 33, "%lld", (long long)upload_segment_length);
#endif

   net_http_request_set_header(request, "Content-Range", range_str, true);

   free(range_str);
   free(content_len_str);
}

static struct http_request_t *_create_multipart_upload_part_request(
   char *upload_url,
   char *local_file,
   int64_t file_size,
   int64_t offset)
{
   struct http_request_t *http_request;
   int64_t upload_segment_length;

   if (file_size - offset > MAX_BYTES_PER_SEGMENT)
   {
      upload_segment_length = MAX_BYTES_PER_SEGMENT;
   } else
   {
      upload_segment_length = file_size - offset;
   }

   http_request = net_http_request_new();
   net_http_request_set_url(http_request, upload_url);
   net_http_request_set_method(http_request, "PUT");
   _add_http_headers_data(file_size, offset, upload_segment_length, http_request);
   cloud_storage_add_request_body_data(local_file, offset, upload_segment_length, http_request);

   net_http_request_set_log_request_body(http_request, false);
   net_http_request_set_log_response_body(http_request, true);

   return http_request;
}

static bool _upload_simple(cloud_storage_item_t *remote_dir, cloud_storage_item_t *remote_file, char *local_file, int64_t file_size)
{
   char *url;
   struct http_request_t *http_request;
   rest_request_t *rest_request;
   struct http_response_t *http_response;
   bool uploaded = false;

   http_request = net_http_request_new();
   if (remote_file->id)
   {
      url = cloud_storage_join_strings(
         NULL,
         DRIVE_ITEMS_URL,
         remote_file->id,
         "/content",
         NULL
      );
   } else
   {
      url = cloud_storage_join_strings(
         NULL,
         DRIVE_ITEMS_URL,
         remote_dir->id,
         ":/",
         remote_file->name,
         ":/content",
         NULL
      );
   }

   net_http_request_set_url(http_request, url);
   free(url);

   net_http_request_set_method(http_request, "PUT");
   net_http_request_set_header(http_request, "Content-Type", CONTENT_TYPE, true);
   cloud_storage_add_request_body_data(local_file, 0, file_size, http_request);

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
         _process_metadata_response(http_response, remote_file);
         uploaded = true;
      default:
         break;
   }

complete:
   if (http_response)
   {
      net_http_response_free(http_response);
   }
   rest_request_free(rest_request);

   return uploaded;
}

static struct http_request_t *_create_start_upload_multipart_request(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file)
{
   struct http_request_t *http_request;
   char *url;
   char *body;
   size_t body_len;

   http_request = net_http_request_new();
   url = cloud_storage_join_strings(
      NULL,
      "https://graph.microsoft.com/v1.0/me/drive/special/approot",
      ":/",
      remote_dir->name,
      "/",
      remote_file->name,
      ":/createUploadSession",
      NULL
   );

   net_http_request_set_url(http_request, url);
   free(url);

   net_http_request_set_method(http_request, "POST");
   body = cloud_storage_join_strings(
      &body_len,
      "{\"item\": {\"@microsoft.graph.conflictBehavior\": \"fail\", \"name\": \"",
      remote_file->name,
      "\"}}",
      NULL
   );
   net_http_request_set_header(http_request, "Content-Type", "application/json", true);
   net_http_request_set_body(http_request, (uint8_t *)body, body_len);

   net_http_request_set_log_request_body(http_request, true);
   net_http_request_set_log_response_body(http_request, true);

   return http_request;
}

static char *_process_start_multipart_response(
   struct http_response_t *response)
{
   uint8_t *data;
   size_t data_len;
   struct json_node_t *json;
   char *json_str;
   char *tmp_str;
   size_t tmp_str_len;
   char *upload_url;

   data = net_http_response_get_data(response, &data_len, false);

   json_str = (char *)malloc(data_len + 1);
   json_str[data_len] = '\0';
   memcpy(json_str, data, data_len);

   json = string_to_json(json_str);
   if (json && json->node_type == OBJECT_VALUE && json_map_get_value_string(json->value.map_value, UPLOAD_URL_KEY, &tmp_str, &tmp_str_len))
   {
      upload_url = (char *)calloc(tmp_str_len + 1, sizeof(char));
      strncpy(upload_url, tmp_str, tmp_str_len);
   }

   if (json)
   {
      json_node_free(json);
   }
   free(json_str);

   return upload_url;
}

static bool _upload_multipart(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file,
   char *local_file,
   int64_t file_size)
{
   char *parent_folder_id;
   char *url;
   char *body;
   size_t body_len;
   struct http_request_t *http_request;
   rest_request_t *rest_request;
   struct http_response_t *http_response;
   char *upload_url;
   int64_t offset = 0;
   bool uploaded = false;

   http_request = _create_start_upload_multipart_request(remote_dir, remote_file);
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
         upload_url = _process_start_multipart_response(http_response);
         break;
      default:
         goto complete;
   }

   while (offset < file_size)
   {
      int64_t upload_segment_length;

      net_http_response_free(http_response);
      rest_request_free(rest_request);

      if (offset + MAX_BYTES_PER_SEGMENT <= file_size)
      {
         upload_segment_length = MAX_BYTES_PER_SEGMENT;
      } else
      {
         upload_segment_length = file_size - offset;
      }

      http_request = _create_multipart_upload_part_request(upload_url, local_file, file_size, offset);
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
         case 202:
            break;
         default:
            goto complete;
      }

      offset += upload_segment_length;
   }

   _process_metadata_response(http_response, remote_file);

   uploaded = true;

complete:
   if (http_response)
   {
      net_http_response_free(http_response);
   }
   rest_request_free(rest_request);

   return uploaded;
}

bool cloud_storage_onedrive_upload_file(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file,
   char *local_file)
{
   struct http_request_t *http_request;
   rest_request_t *rest_request;
   size_t filename_len;
   int64_t file_size;

   file_size = cloud_storage_get_file_size(local_file);

   if (file_size > MAX_BYTES_PER_SEGMENT)
   {
      return _upload_multipart(remote_dir, remote_file, local_file, file_size);
   } else
   {
      return _upload_simple(remote_dir, remote_file, local_file, file_size);
   }
}
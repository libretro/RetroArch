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

#include <rest/rest.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include <net/net_http.h>
#include <rest/rest.h>

#include "../cloud_storage.h"
#include "../json.h"
#include "../driver_utils.h"
#include "google_internal.h"

#define UPLOAD_FILE_URL "https://www.googleapis.com/upload/drive/v3/files"
#define MAX_BYTES_PER_SEGMENT 262144

static void _process_upload_file_part_response(
   struct http_response_t *response,
   cloud_storage_item_t *remote_file)
{
   struct json_node_t *json;
   char *json_str;

   char *file_id;
   size_t file_id_length;
   char *md5;
   size_t md5_length;
   uint8_t *data;
   size_t data_len;

   data = net_http_response_get_data(response, &data_len, false);

   json_str = (char *)malloc(data_len + 1);
   json_str[data_len] = '\0';
   memcpy(json_str, data, data_len);

   json = string_to_json(json_str);
   if (json)
   {
      if (!remote_file->id && json_map_get_value_string(json->value.map_value, "id", &file_id, &file_id_length))
      {
         remote_file->id = (char *)calloc(file_id_length + 1, sizeof(char));
         strncpy(remote_file->id, file_id, file_id_length);
      }

      if (remote_file->type_data.file.hash_value)
      {
         free(remote_file->type_data.file.hash_value);
      }

      remote_file->type_data.file.hash_type = MD5;
      if (json_map_get_value_string(json->value.map_value, "md5Checksum", &md5, &md5_length))
      {
         remote_file->type_data.file.hash_value = (char *)calloc(md5_length + 1, sizeof(char));
         strncpy(remote_file->type_data.file.hash_value, md5, md5_length);
      }

      json_node_free(json);
   }

   free(json_str);
}

static void _add_http_headers_data(
   int64_t file_size,
   int64_t offset,
   int64_t upload_segment_length,
   struct http_request_t *request)
{
   char *range_str;

   range_str = (char *)calloc(105, sizeof(char));

#if defined(_WIN32) || (_WIN64)
   snprintf(range_str, 105, "bytes %" PRIuPTR "-%" PRIuPTR "/%" PRIuPTR, (long long)offset,
      (long long)(offset + upload_segment_length - 1), (long long)file_size);
#else
   snprintf(range_str, 105, "bytes %lld-%lld/%lld", (long long)offset,
      (long long)(offset + upload_segment_length - 1), (long long)file_size);
#endif

   net_http_request_set_header(request, "Content-Range", range_str, true);
   free(range_str);
}

static rest_request_t *_create_upload_file_part_request(
   char *upload_url,
   int64_t file_size,
   int64_t offset,
   int64_t upload_segment_length,
   char *local_file)
{
   struct http_request_t *http_request;

   http_request = net_http_request_new();
   net_http_request_set_url(http_request, upload_url);
   net_http_request_set_method(http_request, "PUT");
   _add_http_headers_data(file_size, offset, upload_segment_length, http_request);
   cloud_storage_add_request_body_data(local_file, offset, upload_segment_length, http_request);

   return rest_request_new(http_request);
}

static char *_process_start_file_upload(struct http_response_t *http_response)
{
   char *location;
   char *upload_url;

   location = net_http_response_get_header_first_value(http_response, "Location");
   upload_url = (char *)calloc(strlen(location) + 1, sizeof(char));
   strcpy(upload_url, location);

   return upload_url;
}

static rest_request_t *_create_start_file_upload_request(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file,
   int64_t file_size,
   char *local_file
)
{
   char *file_size_str;
   char *body;
   size_t body_len;
   struct http_request_t *http_request;
   rest_request_t *rest_request;
   struct http_response_t *http_response;

   http_request = net_http_request_new();
   if (remote_file->id)
   {
      char *url;

      url = cloud_storage_join_strings(
         NULL,
         UPLOAD_FILE_URL,
         "/",
         remote_file->id,
         NULL
      );
      net_http_request_set_url(http_request, url);
      free(url);

      net_http_request_set_method(http_request, "PATCH");
   } else
   {
      net_http_request_set_url(http_request, UPLOAD_FILE_URL);
      net_http_request_set_method(http_request, "POST");
   }

   file_size = cloud_storage_get_file_size(local_file);

   net_http_request_set_url_param(http_request, "uploadType", "resumable", true);

   net_http_request_set_header(http_request, "X-Upload-Content-Type", "application/octet-stream", true);
   file_size_str = (char *)calloc(33, sizeof(char));
#if defined(_WIN32) || (_WIN64)
   snprintf(file_size_str, 33, "%" PRIuPTR, (long long)file_size);
#else
   snprintf(file_size_str, 33, "%lld", (long long)file_size);
#endif
   net_http_request_set_header(http_request, "X-Upload-Content-Length", file_size_str, true);
   net_http_request_set_header(http_request, "Content-Type", "application/json; charset=UTF-8", true);
   free(file_size_str);

   if (remote_file->id == NULL)
   {
      body = cloud_storage_join_strings(
         &body_len,
         "{\"name\": \"",
         remote_file->name,
         "\", \"parents\": [\"",
         remote_dir->id,
         "\"]}",
         NULL);
   } else
   {
      body = cloud_storage_join_strings(
         &body_len,
         "{\"name\":\"",
         remote_file->name,
         "\"}",
         NULL);
   }

   net_http_request_set_body_raw(http_request, (uint8_t *)body, body_len);
   free(body);

   return rest_request_new(http_request);
}

bool cloud_storage_google_upload_file(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file,
   char *local_file)
{
   rest_request_t *rest_request;
   struct http_response_t *http_response;
   int64_t file_size;
   int64_t offset = 0;
   char *upload_url;
   bool uploaded = false;

   file_size = cloud_storage_get_file_size(local_file);

   rest_request = _create_start_file_upload_request(remote_dir, remote_file, file_size, local_file);
   http_response = google_rest_execute_request(rest_request);
   if (!http_response)
   {
      goto complete;
   }

   switch (net_http_response_get_status(http_response))
   {
      case 200:
      case 201:
         upload_url = _process_start_file_upload(http_response);
         break;
      default:
         goto complete;
   }
   net_http_response_free(http_response);
   rest_request_free(rest_request);

   while (offset < file_size)
   {
      int64_t upload_segment_length;

      if (file_size - offset > MAX_BYTES_PER_SEGMENT)
      {
         upload_segment_length = MAX_BYTES_PER_SEGMENT;
      } else
      {
         upload_segment_length = file_size - offset;
      }

      rest_request = _create_upload_file_part_request(
         upload_url,
         file_size,
         offset,
         upload_segment_length,
         local_file);

      offset += upload_segment_length;

      http_response = google_rest_execute_request(rest_request);
      if (!http_response)
      {
         goto complete;
      }

      switch (net_http_response_get_status(http_response))
      {
         case 200:
         case 201:
         case 308:
            if (offset == file_size)
            {
               _process_upload_file_part_response(http_response, remote_file);
               uploaded = true;
            }
         default:
            break;
      }
   }

complete:
   if (http_response)
   {
      net_http_response_free(http_response);
   }
   rest_request_free(rest_request);

   return uploaded;
}
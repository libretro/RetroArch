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

#include "../cloud_storage.h"
#include "../rest-lib/rest_api.h"
#include "../json.h"
#include "../driver_utils.h"
#include "google_internal.h"

#define UPLOAD_FILE_URL "https://www.googleapis.com/upload/drive/v3/files"
#define MAX_BYTES_PER_SEGMENT 262144

struct cloud_storage_google_upload_file_state_t
{
   cloud_storage_item_t *remote_dir;
   cloud_storage_item_t *remote_file;
   char *local_file;
   char *upload_url;
   int64_t file_size;
   int64_t offset;
};

static void upload_file_part(struct cloud_storage_operation_state_t *state);

static void upload_file_part_success_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   struct json_node_t *json;
   char *json_str;
   struct cloud_storage_google_upload_file_state_t *extra_state;

   extra_state = (struct cloud_storage_google_upload_file_state_t *)state->extra_state;

   if (extra_state->offset + MAX_BYTES_PER_SEGMENT < extra_state->file_size)
   {
      extra_state->offset += MAX_BYTES_PER_SEGMENT;
      upload_file_part(state);
   } else
   {
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
         if (!extra_state->remote_file->id && json_map_get_value_string(json->value.map_value, "id", &file_id, &file_id_length))
         {
            extra_state->remote_file->id = (char *)calloc(file_id_length + 1, sizeof(char));
            strncpy(extra_state->remote_file->id, file_id, file_id_length);
         }

         if (extra_state->remote_file->type_data.file.hash_value)
         {
            free(extra_state->remote_file->type_data.file.hash_value);
         }

         extra_state->remote_file->type_data.file.hash_type = MD5;
         if (json_map_get_value_string(json->value.map_value, "md5Checksum", &md5, &md5_length))
         {
            extra_state->remote_file->type_data.file.hash_value = (char *)calloc(md5_length + 1, sizeof(char));
            strncpy(extra_state->remote_file->type_data.file.hash_value, md5, md5_length);
         }
      }

      if (json)
      {
         json_node_free(json);
      }
      free(json_str);

      state->result = extra_state->remote_file;
      state->callback(state, true);

      state->complete = true;
   }
}

static void add_http_headers_data(int64_t file_size, int64_t offset, struct http_request_t *request)
{
   int64_t upload_segment_length;
   char *range_str;

   if (file_size - offset > MAX_BYTES_PER_SEGMENT)
   {
      upload_segment_length = MAX_BYTES_PER_SEGMENT;
   } else
   {
      upload_segment_length = file_size - offset;
   }

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

static void upload_file_part(struct cloud_storage_operation_state_t *state)
{
   struct http_request_t *http_request;
   rest_api_request_t *rest_request;
   struct cloud_storage_google_upload_file_state_t *extra_state;

   extra_state = (struct cloud_storage_google_upload_file_state_t *)state->extra_state;

   http_request = net_http_request_new();
   net_http_request_set_url(http_request, extra_state->upload_url);
   net_http_request_set_method(http_request, "PUT");
   add_http_headers_data(extra_state->file_size, extra_state->offset, http_request);
   cloud_storage_add_request_body_data(extra_state->local_file, extra_state->offset, MAX_BYTES_PER_SEGMENT, http_request);

   net_http_request_set_log_request_body(http_request, false);
   net_http_request_set_log_response_body(http_request, true);

   rest_request = rest_api_request_new(http_request);
   rest_api_request_set_response_handler(rest_request, 200, false, upload_file_part_success_handler, state);
   rest_api_request_set_response_handler(rest_request, 201, false, upload_file_part_success_handler, state);

   google_rest_execute_request(rest_request, state);
}

static void start_file_upload_success_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   char *location;
   struct cloud_storage_google_upload_file_state_t *extra_state;
   rest_api_request_t *rest_request;

   extra_state = (struct cloud_storage_google_upload_file_state_t *)state->extra_state;

   location = net_http_response_get_header_first_value(response, "Location");
   extra_state->upload_url = (char *)calloc(strlen(location) + 1, sizeof(char));
   strcpy(extra_state->upload_url, location);

   upload_file_part(state);
}

static void start_file_upload(cloud_storage_operation_state_t *state)
{
   char *file_size_str;
   char *body;
   size_t body_len;
   struct http_request_t *http_request;
   rest_api_request_t *rest_request;
   struct cloud_storage_google_upload_file_state_t *extra_state;

   extra_state = (struct cloud_storage_google_upload_file_state_t *)state->extra_state;

   http_request = net_http_request_new();
   if (extra_state->remote_file->id)
   {
      char *url;

      url = cloud_storage_join_strings(
         NULL,
         UPLOAD_FILE_URL,
         "/",
         extra_state->remote_file->id,
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

   extra_state->file_size = cloud_storage_get_file_size(extra_state->local_file);
   extra_state->offset = 0;

   net_http_request_set_url_param(http_request, "uploadType", "resumable", true);

   net_http_request_set_header(http_request, "X-Upload-Content-Type", "application/octet-stream", true);
   file_size_str = (char *)calloc(33, sizeof(char));
#if defined(_WIN32) || (_WIN64)
   snprintf(file_size_str, 33, "%" PRIuPTR, (long long)extra_state->file_size);
#else
   snprintf(file_size_str, 33, "%lld", (long long)extra_state->file_size);
#endif
   net_http_request_set_header(http_request, "X-Upload-Content-Length", file_size_str, true);
   net_http_request_set_header(http_request, "Content-Type", "application/json; charset=UTF-8", true);
   free(file_size_str);

   if (extra_state->remote_file->id == NULL)
   {
      body = cloud_storage_join_strings(
         &body_len,
         "{\"name\": \"",
         extra_state->remote_file->name,
         "\", \"parents\": [\"",
         extra_state->remote_dir->id,
         "\"]}",
         NULL);
   } else
   {
      body = cloud_storage_join_strings(
         &body_len,
         "{\"name\":\"",
         extra_state->remote_file->name,
         "\"}",
         NULL);
   }

   net_http_request_set_body(http_request, (uint8_t *)body, body_len);

   net_http_request_set_log_request_body(http_request, true);
   net_http_request_set_log_response_body(http_request, true);

   rest_request = rest_api_request_new(http_request);
   rest_api_request_set_response_handler(rest_request, 200, false, start_file_upload_success_handler, state);
   rest_api_request_set_response_handler(rest_request, 201, false, start_file_upload_success_handler, state);

   google_rest_execute_request(rest_request, state);
}

static void free_extra_state(void *extra_state_ptr)
{
   struct cloud_storage_google_upload_file_state_t *extra_state;

   extra_state = (struct cloud_storage_google_upload_file_state_t *)extra_state_ptr;

   free(extra_state->local_file);

   if (extra_state->upload_url)
   {
      free(extra_state->upload_url);
   }

   if (extra_state->remote_file && !extra_state->remote_file->id)
   {
      cloud_storage_item_free(extra_state->remote_file);
   }

   free(extra_state);
}

void cloud_storage_google_upload_file(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file,
   char *local_file,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback)
{
   struct http_request_t *http_request;
   cloud_storage_operation_state_t *state;
   struct cloud_storage_google_upload_file_state_t *extra_state;
   rest_api_request_t *rest_request;
   size_t filename_len;
   int64_t file_size;

   extra_state = (struct cloud_storage_google_upload_file_state_t *)calloc(1, sizeof(struct cloud_storage_google_upload_file_state_t));
   extra_state->remote_dir = remote_dir;
   extra_state->remote_file = remote_file;
   extra_state->local_file = local_file;

   state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   state->continuation_data = continuation_data;
   state->callback = callback;
   state->extra_state = extra_state;
   state->free_extra_state = free_extra_state;

   start_file_upload(state);
}
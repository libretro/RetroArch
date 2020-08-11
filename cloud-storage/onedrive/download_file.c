/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (download_file.c).
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

#include <boolean.h>

#include <streams/file_stream.h>

#include "../cloud_storage.h"
#include "../rest-lib/rest_api.h"
#include "../driver_utils.h"
#include "onedrive_internal.h"

#define DOWNLOAD_FILES_URL "https://graph.microsoft.com/v1.0/me/drive/"

static struct http_request_t *create_http_request(char *download_url)
{
   struct http_request_t *http_request;

   http_request = net_http_request_new();

   net_http_request_set_url(http_request, download_url);
   net_http_request_set_method(http_request, "GET");

   net_http_request_set_log_request_body(http_request, true);
   net_http_request_set_log_response_body(http_request, false);

   return http_request;
}

static void download_files_success_callback(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   uint8_t *data;
   size_t data_len;

   data = net_http_response_get_data(response, &data_len, false);
   cloud_storage_save_file((char *)state->extra_state, data, data_len);

   state->callback(state, true);
   state->complete = true;
}

static void failure_callback(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   state->callback(state, false);
   state->complete = true;
}

static void free_extra_state(void *extra_state)
{
   free(extra_state);
}

void cloud_storage_onedrive_download_file(
   cloud_storage_item_t *file_to_download,
   char *local_file,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback)
{
   char *url;
   cloud_storage_operation_state_t *state;
   struct http_request_t *http_request;
   rest_api_request_t *rest_request;

   state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   state->continuation_data = continuation_data;
   state->callback = callback;
   state->extra_state = local_file;
   state->free_extra_state = free_extra_state;

   http_request = create_http_request(file_to_download->type_data.file.download_url);

   rest_request = rest_api_request_new(http_request);
   rest_api_request_set_response_handler(rest_request, 200, false, download_files_success_callback, state);
   rest_api_request_set_response_handler(rest_request, 400, false, failure_callback, state);
   rest_api_request_set_response_handler(rest_request, 404, false, failure_callback, state);

   onedrive_rest_execute_request(rest_request, state);
}

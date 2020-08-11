/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (delete_file.c).
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
#include "../driver_utils.h"
#include "onedrive_internal.h"

#define DELETE_FILE_URL_BASE "https://graph.microsoft.com/v1.0/me/drive/special/items/"

static struct http_request_t *get_http_request(char *file_id)
{
   char *url;
   struct http_request_t *request;

   request = net_http_request_new();

   url = cloud_storage_join_strings(NULL, DELETE_FILE_URL_BASE, file_id, NULL);
   net_http_request_set_url(request, url);
   free(url);

   net_http_request_set_method(request, "DELETE");

   net_http_request_set_log_request_body(request, true);
   net_http_request_set_log_response_body(request, true);

   return request;
}

static void success_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   cloud_storage_file_t *deleted_file;

   state->callback(state, true);
   state->complete = true;
}

void cloud_storage_onedrive_delete_file(
   cloud_storage_item_t *file,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback)
{
   struct http_request_t *http_request;
   cloud_storage_operation_state_t *state;
   rest_api_request_t *rest_request;

   state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   state->continuation_data = continuation_data;
   state->callback = callback;

   http_request = get_http_request(file->id);

   rest_request = rest_api_request_new(http_request);
   rest_api_request_set_response_handler(rest_request, 200, false, success_handler, state);

   onedrive_rest_execute_request(rest_request, state);
}

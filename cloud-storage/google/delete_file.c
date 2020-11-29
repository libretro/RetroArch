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

#include <net/net_http.h>
#include <rest/rest.h>

#include "../cloud_storage.h"
#include "../driver_utils.h"
#include "google_internal.h"

#define DELETE_FILE_URL_BASE "https://www.googleapis.com/drive/v3/files/"

static struct http_request_t *_get_http_request(char *file_id)
{
   char *url;
   struct http_request_t *request;

   request = net_http_request_new();

   url = cloud_storage_join_strings(NULL, DELETE_FILE_URL_BASE, file_id, NULL);
   net_http_request_set_url(request, url);
   free(url);

   net_http_request_set_method(request, "DELETE");

   return request;
}

bool cloud_storage_google_delete_file(cloud_storage_item_t *file)
{
   struct http_request_t *http_request;
   rest_request_t *rest_request;
   struct http_response_t *http_response = NULL;
   bool deleted = false;

   http_request = _get_http_request(file->id);
   rest_request = rest_request_new(http_request);

   http_response = google_rest_execute_request(rest_request);
   if (!http_response)
   {
      goto complete;
   }

   switch (net_http_response_get_status(http_response))
   {
      case 200:
         deleted = true;
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

   return deleted;
}

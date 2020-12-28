/* Copyright  (C) 2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rest_api.c).
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
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <rest/rest.h>
#include <rthreads/rthreads.h>

struct rest_request_t
{
   struct http_request_t *http_request;
   struct http_connection_t *http_conn;
   struct http_t *http;
   rest_retry_policy_t *retry_policy;
   int retry_count;
   time_t delay_end;
   uint16_t timeout_seconds;
};

struct rest_retry_policy_t
{
   int *status_codes;
   size_t status_codes_len;
   time_t *delays;
   size_t delays_len;
};

#define REST_DEFAULT_TIMEOUT 30

rest_retry_policy_t *rest_retry_policy_new(
   int *status_codes,
   size_t status_codes_len,
   time_t *delays,
   size_t delays_len)
{
   rest_retry_policy_t *retry_policy;

   retry_policy = (rest_retry_policy_t *)calloc(1, sizeof(rest_retry_policy_t));
   retry_policy->status_codes = status_codes;
   retry_policy->status_codes_len = status_codes_len;
   retry_policy->delays = delays;
   retry_policy->delays_len = delays_len;

   return retry_policy;
}

void rest_request_free(rest_request_t *request)
{
   if (!request)
   {
      return;
   }

   if (request->http)
      net_http_delete(request->http);

   if (request->http_conn)
      net_http_connection_free(request->http_conn, false);
   else if (request->http_request)
   {
      net_http_request_free(request->http_request);
      request->http_request = NULL;
   }

   if (request->http_request)
   {
      net_http_request_free(request->http_request);
   }

   free(request);
}

rest_request_t *rest_request_new(struct http_request_t *http_request)
{
   struct rest_request_t *request;

   request = (rest_request_t *)calloc(1, sizeof(rest_request_t));
   request->http_request = http_request;
   request->timeout_seconds = REST_DEFAULT_TIMEOUT;

   return request;
}

void rest_request_set_timeout(rest_request_t *request, uint16_t timeout_seconds)
{
   request->timeout_seconds = timeout_seconds;
}

void rest_request_set_header(rest_request_t *request, const char *name, const char *value, bool replace)
{
   net_http_request_set_header(request->http_request, name, value, replace);
}

void rest_request_set_retry_policy(
   rest_request_t *request,
   rest_retry_policy_t *retry_policy)
{
   if (request)
   {
      request->retry_policy = retry_policy;
   }
}

static void _start_request(rest_request_t *request)
{
   request->http_conn = net_http_connection_new(request->http_request);
   if (!request->http_conn || !net_http_connection_iterate(request->http_conn) || !net_http_connection_done(request->http_conn))
   {
      net_http_connection_free(request->http_conn, true);
      request->http_conn = NULL;
      return;
   }

   request->http = net_http_new(request->http_conn);
   if (!request->http)
   {
      net_http_connection_free(request->http_conn, true);
      request->http_conn = NULL;
      request->http = NULL;
   }
}

struct http_response_t *rest_request_execute(rest_request_t *request)
{
   time_t timeout;
   size_t progress;
   size_t total;

   timeout = time(NULL) + request->timeout_seconds;

   do {
      if (!request->http_conn)
      {
         _start_request(request);
         if (!request->http)
         {
            break;
         }
      }

      if (net_http_update(request->http, &progress, &total))
      {
         struct http_response_t *response;

         response = net_http_get_response(request->http);

         if (request->retry_policy)
         {
            int status_code;
            int i;
            bool should_retry = false;
            struct rest_api_response_callback_with_status_t *current_entry;

            status_code = net_http_response_get_status(response);

            for (i = 0; i < request->retry_policy->status_codes_len; i++)
            {
               if (status_code == request->retry_policy->status_codes[i])
               {
                  should_retry = true;
                  break;
               }
            }

            if (should_retry)
            {
               net_http_delete(request->http);
               net_http_connection_free(request->http_conn, false);
               request->http = NULL;
               request->http_conn = NULL;

               if (request->retry_count < request->retry_policy->delays_len - 1)
               {
                  request->delay_end = time(NULL) + request->retry_policy->delays[request->retry_count];
                  request->retry_count++;
               } else
               {
                  return NULL;
               }
            } else
            {
               return response;
            }
         } else
         {
            return response;
         }
      } else
      {
         usleep(100);
      }
   } while (time(NULL) < timeout);

   return NULL;
}

/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (authenticate.c).
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
#include <string/stdstring.h>

#include "../cloud_storage.h"
#include "../driver_utils.h"
#include "onedrive_internal.h"

#define CLIENT_ID_PARAM_NAME "client_id"
#define CLIENT_SECRET_PARAM_NAME "client_secret"
#define REDIRECT_URI_NAME "redirect_uri"
#define REFRESH_TOKEN_URL "https://login.microsoftonline.com/common/oauth2/v2.0/token"
#define REFRESH_TOKEN_PARAM_NAME "refresh_token"
#define GRANT_TYPE_PARAM_NAME "grant_type"
#define GRANT_TYPE_PARAM_VALUE "refresh_token"

extern char *_onedrive_access_token;
extern time_t _onedrive_access_token_expiration_time;

static void _parse_access_token_response(uint8_t *json_text, size_t json_length, char **new_access_token, time_t *expiration_time)
{
   const char *parsed_access_token = NULL;
   size_t token_length;
   int64_t expires_in = -1;
   rjson_t *json;
   bool in_object = false;
   const char *key_name;
   size_t key_name_len;

   json = rjson_open_buffer(json_text, json_length);
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
            } else if (strcmp("access_token", key_name) == 0)
            {
               parsed_access_token = rjson_get_string(json, &token_length);
            }

            break;
         case RJSON_NUMBER:
            if (!in_object)
            {
               goto cleanup;
            }

            if ((rjson_get_context_count(json) & 1) == 0 && strcmp("expires_in", key_name) == 0)
            {
               expires_in = rjson_get_int(json);
            }

            break;
         case RJSON_OBJECT_END:
            if (in_object && parsed_access_token && expires_in > -1)
            {
               *new_access_token = (char *)calloc(token_length + 1, sizeof(char));
               strncpy(*new_access_token, parsed_access_token, token_length);
               *expiration_time = time(NULL) + expires_in;
            }
            goto cleanup;
         default:
            goto cleanup;
      }
   }

cleanup:
   rjson_free(json);
}

static uint8_t *_refresh_token_request_body(size_t *request_body_len)
{
   size_t name_lengths[5];
   size_t value_lengths[5];
   char *names[5];
   char *values[5];
   char *request_body;
   char *encoded_client_id;
   char *encoded_redirect_uri;
   char *encoded_refresh_token;
   size_t cur_pos = 0;
   int i;
   settings_t *settings;
   const char *client_id;

   settings = config_get_ptr();

   client_id = cloud_storage_onedrive_get_client_id();

   net_http_urlencode(&encoded_client_id, client_id);
   net_http_urlencode(&encoded_redirect_uri, "https://login.microsoftonline.com/common/oauth2/nativeclient");
   net_http_urlencode(&encoded_refresh_token, settings->arrays.cloud_storage_onedrive_refresh_token);

   request_body = cloud_storage_join_strings(
      request_body_len,
      CLIENT_ID_PARAM_NAME,
      "=",
      encoded_client_id,
      "&",
      REDIRECT_URI_NAME,
      "=",
      encoded_redirect_uri,
      "&",
      REFRESH_TOKEN_PARAM_NAME,
      "=",
      encoded_refresh_token,
      "&",
      GRANT_TYPE_PARAM_NAME,
      "=",
      GRANT_TYPE_PARAM_VALUE,
      NULL
   );

   free(encoded_client_id);
   free(encoded_redirect_uri);
   free(encoded_refresh_token);

   return (uint8_t *)request_body;
}

static bool _process_response(struct http_response_t *response)
{
   uint8_t *data;
   size_t data_len;
   char *json_text;
   struct json_node_t *json;
   char *new_access_token = NULL;
   time_t expiration_time;

   data = net_http_response_get_data(response, &data_len, false);
   _parse_access_token_response(data, data_len, &new_access_token, &expiration_time);

   if (new_access_token)
   {
      _onedrive_access_token = new_access_token;
      _onedrive_access_token_expiration_time = expiration_time;
      cloud_storage_save_access_token("onedrive", new_access_token, expiration_time);
      return true;
   }

   return false;
}

static struct http_request_t *_create_http_request(void)
{
   struct http_request_t *http_request;
   uint8_t *body;
   size_t body_len;

   http_request = net_http_request_new();
   net_http_request_set_url(http_request, REFRESH_TOKEN_URL);
   net_http_request_set_method(http_request, "POST");
   net_http_request_set_header(http_request, "Content-Type", "application/x-www-form-urlencoded", true);

   body = _refresh_token_request_body(&body_len);
   net_http_request_set_body_raw(http_request, body, body_len);

   return http_request;
}

bool cloud_storage_onedrive_authenticate(void)
{
   struct http_request_t *http_request;
   struct http_response_t *http_response;
   uint8_t *body;
   size_t body_len;
   rest_request_t *rest_request;
   bool authenticated = false;

   http_request = _create_http_request();

   rest_request = rest_request_new(http_request);
   http_response = rest_request_execute(rest_request);
   if (!http_response)
   {
      goto complete;
   }

   switch (net_http_response_get_status(http_response))
   {
      case 200:
         authenticated = _process_response(http_response);
         break;
      default:
         goto complete;
   }

complete:
   if (http_response)
   {
      net_http_response_free(http_response);
   }

   rest_request_free(rest_request);

   return authenticated;
}

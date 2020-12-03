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

#include <file/file_path.h>
#include <net/net_http.h>
#include <rest/rest.h>
#include <formats/rjson.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../cloud_storage.h"
#include "../driver_utils.h"
#include "google_internal.h"

#define CLIENT_ID_PARAM_NAME "client_id"
#define CLIENT_SECRET_PARAM_NAME "client_secret"
#define REFRESH_TOKEN_URL "https://oauth2.googleapis.com/token"
#define REFRESH_TOKEN_PARAM_NAME "refresh_token"
#define GRANT_TYPE_PARAM_NAME "grant_type"
#define GRANT_TYPE_PARAM_VALUE "refresh_token"

extern char *_google_access_token;
extern time_t _google_access_token_expiration_time;

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
   size_t name_lengths[4];
   size_t value_lengths[4];
   const char *names[4];
   char *values[4];
   char *request_body;
   size_t cur_pos = 0;
   int i;
   settings_t *settings;
   const char *client_id;
   const char *client_secret;

   settings = config_get_ptr();

   client_id = cloud_storage_google_get_client_id();
   client_secret = cloud_storage_google_get_client_secret();

   names[0] = CLIENT_ID_PARAM_NAME;
   net_http_urlencode(&(values[0]), client_id);
   names[1] = CLIENT_SECRET_PARAM_NAME;
   net_http_urlencode(&(values[1]), client_secret);
   names[2] = REFRESH_TOKEN_PARAM_NAME;
   net_http_urlencode(&(values[2]), settings->arrays.cloud_storage_google_refresh_token);
   names[3] = GRANT_TYPE_PARAM_NAME;
   values[3] = (char *)malloc(strlen(GRANT_TYPE_PARAM_VALUE) + 1);
   strcpy(values[3], GRANT_TYPE_PARAM_VALUE);

   *request_body_len = 0;
   for (i = 0;i < 4;i++)
   {
      name_lengths[i] = strlen(names[i]);
      *request_body_len += name_lengths[i] + 1;
      value_lengths[i] = strlen(values[i]);
      *request_body_len += value_lengths[i] + 1;
   }

   request_body = (char *)calloc(*request_body_len, sizeof(char));

   for (i = 0;i < 4;i++)
   {
      strcpy(request_body + cur_pos, names[i]);
      cur_pos += name_lengths[i];
      request_body[cur_pos++] = '=';
      strcpy(request_body + cur_pos, values[i]);
      cur_pos += value_lengths[i];

      if (i < 3)
      {
         request_body[cur_pos++] = '&';
         free(values[i]);
      }
   }

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
   struct cloud_storage_google_provider_data_t *provider_data;
   struct authenticate_extra_state_t *extra_state;

   data = net_http_response_get_data(response, &data_len, false);
   _parse_access_token_response(data, data_len, &new_access_token, &expiration_time);

   if (new_access_token)
   {
      _google_access_token = new_access_token;
      _google_access_token_expiration_time = expiration_time;
      cloud_storage_save_access_token("google", new_access_token, expiration_time);
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

bool cloud_storage_google_authenticate(void)
{
   struct http_request_t *http_request;
   rest_request_t *rest_request;
   struct http_response_t *http_response = NULL;
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

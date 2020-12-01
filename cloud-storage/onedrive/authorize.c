/* Copyright  (C) 2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (authorize.c).
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
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#include <net/open_browser.h>
#include <rest/rest.h>

#include <configuration.h>

#include "../../command.h"

#include "../cloud_storage.h"
#include "../json.h"
#include "../driver_utils.h"
#include "onedrive_internal.h"

#define URL_MAX_LENGTH 2048

static rest_retry_policy_t *retry_policy = NULL;
extern char *_onedrive_access_token;
extern time_t _onedrive_access_token_expiration_time;

static bool _parse_request(char *request, size_t len, char **code)
{
   char *path;
   char *line_end = NULL;
   char *space_pos = NULL;
   char *current_pos = NULL;
   char *param_end = NULL;

   if (request == NULL)
   {
      return false;
   }

   if (strncmp("GET ", request, 4))
   {
      return false;
   }

   path = request + 4;
   for (current_pos = request + 4;current_pos < request + len;current_pos++)
   {
      if (*current_pos == ' ')
      {
         space_pos = current_pos;
      } else if (*current_pos == '\n')
      {
         if (!space_pos)
         {
            return false;
         } else
         {
            line_end = current_pos;
            break;
         }
      }
   }

   if (!space_pos || !line_end)
   {
      return false;
   }

   if (strncmp(space_pos, " HTTP/", 6))
   {
      return false;
   }

   for (current_pos = path;current_pos < space_pos;current_pos++)
   {
      if (*current_pos == '?')
      {
         break;
      }
   }
   if (current_pos >= space_pos - 1)
   {
      return false;
   }

   current_pos++;
   while (current_pos < space_pos)
   {
      char *equals_pos = NULL;

      path = current_pos;
      for (;current_pos < space_pos;current_pos++)
      {
         if (*current_pos == '&')
         {
            break;
         }
      }
      param_end = current_pos;

      for (current_pos = path;current_pos < param_end;current_pos++)
      {
         if (*current_pos == '=')
         {
            equals_pos = current_pos;
            break;
         }
      }

      if (!equals_pos)
      {
         return false;
      }

      if (!strncmp("code=", path, equals_pos - path + 1))
      {
         current_pos = equals_pos + 1;
         *code = (char *)calloc(param_end - current_pos + 1, sizeof(char));
         strncpy(*code, current_pos, param_end - current_pos);

         return true;
      }

      current_pos = param_end + 1;
   }

   return false;
}

static void _process_get_tokens_response(struct http_response_t *response)
{
   char *json_text;
   uint8_t *bytes;
   size_t bytes_len;
   struct json_node_t *json;
   char *access_token = NULL;
   char *refresh_token = NULL;
   time_t access_token_expiration_time = 0;

   bytes = net_http_response_get_data(response, &bytes_len, false);

   json_text = (char *)malloc(bytes_len + 1);
   memcpy(json_text, bytes, bytes_len);
   json_text[bytes_len] = '\0';

   json = string_to_json(json_text);
   if (json)
   {
      char *value;
      size_t value_len;
      int64_t int_value;
      char expiration_time[16];
      settings_t *settings;

      if (json->node_type != OBJECT_VALUE)
      {
         goto cleanup;
      }

      if (!json_map_get_value_string(json->value.map_value, "access_token", &value, &value_len))
      {
         goto cleanup;
      }
      access_token = (char *)malloc(value_len + 1);
      access_token[value_len] = '\0';
      strncpy(access_token, value, value_len);

      if (!json_map_get_value_int(json->value.map_value, "expires_in", &int_value))
      {
         goto cleanup;
      }
      access_token_expiration_time = time(NULL) + (int)int_value;

      if (!json_map_get_value_string(json->value.map_value, "refresh_token", &value, &value_len))
      {
         goto cleanup;
      }
      refresh_token = (char *)malloc(value_len + 1);
      refresh_token[value_len] = '\0';
      strncpy(refresh_token, value, value_len);

      command_event(CMD_EVENT_MENU_SAVE_CONFIG, NULL);

      settings = config_get_ptr();
      strcpy(settings->arrays.cloud_storage_onedrive_refresh_token, refresh_token);
      free(refresh_token);
      refresh_token = NULL;

      cloud_storage_save_access_token("onedrive", access_token, access_token_expiration_time);
      access_token = NULL;
   }

cleanup:
   if (json)
   {
      json_node_free(json);
   }
   if (json_text)
   {
      free(json_text);
   }

   if (access_token)
   {
      free(access_token);
   }

   if (refresh_token)
   {
      free(refresh_token);
   }
}

static rest_retry_policy_t *_get_retry_policy(void)
{
   if (!retry_policy)
   {
      int *status_codes;
      time_t *delays;

      status_codes = (int *)calloc(1, sizeof(int));
      status_codes[0] = 500;
      delays = (time_t *)calloc(3, sizeof(time_t));
      delays[0] = 1;
      delays[1] = 2;
      delays[2] = 5;

      retry_policy = rest_retry_policy_new(status_codes, 1, delays, 3);
   }

   return retry_policy;
}

static void _get_tokens(char *code, char *code_verifier, int port)
{
   rest_request_t *rest_request;
   struct http_request_t *http_request;
   struct http_response_t *http_response;
   const char *client_id;
   char *body;
   size_t body_len;

   http_request = net_http_request_new();
   net_http_request_set_url(http_request, "https://login.live.com/oauth20_token.srf");
   net_http_request_set_method(http_request, "POST");
   net_http_request_set_header(http_request, "Content-Type", "application/x-www-form-urlencoded", true);

   client_id = cloud_storage_onedrive_get_client_id();

   body = (char *)malloc(2048);
   body_len = snprintf(
      body,
      2048,
      "code=%s&client_id=%s&grant_type=authorization_code&redirect_uri=http%%3A//localhost%%3A%d/auth_code",
      code,
      client_id,
      port
   );

   net_http_request_set_body_raw(http_request, (uint8_t *)body, body_len);

   rest_request = rest_request_new(http_request);
   rest_request_set_retry_policy(rest_request, _get_retry_policy());

   http_response = rest_request_execute(rest_request);
   if (!http_response)
   {
      goto complete;
   }

   switch (net_http_response_get_status(http_response))
   {
      case 200:
         _process_get_tokens_response(http_response);
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
}

static bool _process_request(char *code_verifier, int port, uint8_t *request, size_t request_len)
{
   char *code;

   if (_parse_request((char *)request, request_len, &code))
   {
      _get_tokens(code, code_verifier, port);

      if (code)
      {
         free(code);
      }

      return true;
   }

   return false;
}

authorization_status_t cloud_storage_onedrive_authorize(void (*callback)(bool success))
{
   settings_t *settings;
   const char *client_id;
   int port;
   char url[URL_MAX_LENGTH];
   int i;
   struct authorize_state_t *authorize_state;
   authorization_status_t result;

   settings = config_get_ptr();

   if (strlen(settings->arrays.cloud_storage_onedrive_refresh_token) > 0)
   {
      return AUTH_COMPLETE;
   }

   client_id = cloud_storage_onedrive_get_client_id();
   if (strlen(client_id) == 0) {
      return AUTH_FAILED;
   }

   authorize_state = (struct authorize_state_t *)calloc(1, sizeof(struct authorize_state_t));
   authorize_state->port = -1;
   authorize_state->mutex = slock_new();
   authorize_state->condition = scond_new();
   authorize_state->callback = callback;

   slock_lock(authorize_state->mutex);
   if (!cloud_storage_oauth_receive_browser_request(
      authorize_state,
      NULL,
      _process_request))
   {
      goto failed;
   }

   if (!scond_wait_timeout(authorize_state->condition, authorize_state->mutex, 30000000))
   {
      slock_unlock(authorize_state->mutex);
      goto failed;
   }
   slock_unlock(authorize_state->mutex);

   snprintf(
      url,
      URL_MAX_LENGTH,
      "https://login.live.com/oauth20_authorize.srf?scope=offline_access%%20Files.ReadWrite.AppFolder&response_type=code&redirect_uri=http%%3A//localhost%%3A%d/auth_code&client_id=%s",
      authorize_state->port,
      client_id
   );

   if (!open_browser(url))
   {
      goto failed;
   }

   return AUTH_PENDING_ASYNC;

failed:
   slock_free(authorize_state->mutex);
   scond_free(authorize_state->condition);
   free(authorize_state);
   return AUTH_FAILED;
}

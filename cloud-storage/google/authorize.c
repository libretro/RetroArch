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

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <process.h>
#include <winsock2.h>
#else
#include <pthread.h>
#endif

#include <sys/types.h>
#include <net/net_socket.h>
#include <net/net_compat.h>
#include <net/open_browser.h>

#include <configuration.h>

#include "../../command.h"

#include "../cloud_storage.h"
#include "../rest-lib/rest_api.h"
#include "../json.h"
#include "../driver_utils.h"
#include "google_internal.h"

#define URL_MAX_LENGTH 2048

#if !defined(_WIN32) && !defined(_WIN64)
typedef int SOCKET;
#endif

struct authorize_state_t
{
   char *code_verifier;
   int port;
   SOCKET sockfd;
   void (*callback)(bool success);
   struct cloud_storage_google_provider_data_t *provider_data;
};

static bool listen_for_response_thread(struct authorize_state_t *authorize_state);

authorization_status_t cloud_storage_google_authorize(
   settings_t *settings,
   void *google_state_ptr,
   void (*callback)(bool success)
)
{
   int port;
   char url[URL_MAX_LENGTH];
   const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";
   char *code_verifier;
   int i;
   struct authorize_state_t *authorize_state;

   if (strlen(settings->arrays.cloud_storage_google_refresh_token) > 0)
   {
      return AUTH_COMPLETE;
   }

   if (strlen(settings->arrays.cloud_storage_google_client_id) == 0 ||
         strlen(settings->arrays.cloud_storage_google_client_secret) == 0) {
      return AUTH_FAILED;
   }

   code_verifier = (char *)malloc(65 * sizeof(char));
   code_verifier[64] = '\0';
   for (i = 0;i < 64;i++)
   {
      int key = rand() % (int)(sizeof(charset) - 1);
      code_verifier[i] = charset[key];
   }

   authorize_state = (struct authorize_state_t *)malloc(sizeof(struct authorize_state_t));
   authorize_state->code_verifier = code_verifier;
   authorize_state->port = -1;
   authorize_state->callback = callback;
   authorize_state->provider_data = (struct cloud_storage_google_provider_data_t *)google_state_ptr;

   if (!listen_for_response_thread(authorize_state))
   {
      free(code_verifier);
      return AUTH_FAILED;
   }

   snprintf(
      url,
      URL_MAX_LENGTH,
      "https://accounts.google.com/o/oauth2/v2/auth?scope=https%%3A//www.googleapis.com/auth/drive.appdata&code_challenge=%s&code_challenge_method=plain&response_type=code&state=authorization&redirect_uri=http%%3A//127.0.0.1%%3A%d/auth_code&client_id=%s",
      code_verifier,
      authorize_state->port,
      settings->arrays.cloud_storage_google_client_id
   );

   if (!open_browser(url))
   {
      free(code_verifier);
      return AUTH_FAILED;
   }

   return AUTH_PENDING_ASYNC;
}

static bool parse_request(char *request, size_t len, char **code)
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

static void get_tokens_failure_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   struct authorize_state_t *authorize_state;

   authorize_state = (struct authorize_state_t *)state->extra_state;

   authorize_state->callback(false);

   if (authorize_state->code_verifier)
   {
      free(authorize_state->code_verifier);
   }
   free(authorize_state);
   free(state);
}

static void get_tokens_success_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   struct authorize_state_t *authorize_state;
   char *json_text;
   uint8_t *bytes;
   size_t bytes_len;
   struct json_node_t *json;
   char *access_token = NULL;
   char *refresh_token = NULL;
   time_t access_token_expiration_time = 0;
   bool success = false;

   authorize_state = (struct authorize_state_t *)state->extra_state;

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

      success = true;

      settings = config_get_ptr();
      strcpy(settings->arrays.cloud_storage_google_refresh_token, refresh_token);
      free(refresh_token);
      refresh_token = NULL;

      cloud_storage_save_access_token("google", access_token, access_token_expiration_time);
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

   authorize_state->callback(success);

   if (authorize_state)
   {
      if (authorize_state->code_verifier)
      {
         free(authorize_state->code_verifier);
      }

      free(authorize_state);
   }

   if (state)
   {
      free(state);
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

static void get_tokens(char *code, struct authorize_state_t *authorize_state)
{
   rest_api_request_t *request;
   struct http_request_t *http_request;
   settings_t *settings;
   char *body;
   size_t body_len;
   cloud_storage_operation_state_t *state;

   http_request = net_http_request_new();
   net_http_request_set_url(http_request, "https://oauth2.googleapis.com/token");
   net_http_request_set_method(http_request, "POST");
   net_http_request_set_header(http_request, "Content-Type", "application/x-www-form-urlencoded", true);

   settings = config_get_ptr();

   body = (char *)malloc(2048);
   body_len = snprintf(
      body,
      2048,
      "code=%s&client_id=%s&client_secret=%s&code_verifier=%s&grant_type=authorization_code&redirect_uri=http%%3A//127.0.0.1%%3A%d/auth_code",
      code,
      settings->arrays.cloud_storage_google_client_id,
      settings->arrays.cloud_storage_google_client_secret,
      authorize_state->code_verifier,
      authorize_state->port
   );
   free(authorize_state->code_verifier);
   authorize_state->code_verifier = NULL;

   state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   state->extra_state = authorize_state;

   net_http_request_set_body(http_request, (uint8_t *)body, body_len);
   net_http_request_set_log_request_body(http_request, true);
   net_http_request_set_log_response_body(http_request, true);

   request = rest_api_request_new(http_request);
   rest_api_request_set_default_response_handler(request, false, get_tokens_failure_handler, state);
   rest_api_request_set_response_handler(request, 500, true, get_tokens_failure_handler, state);
   rest_api_request_set_response_handler(request, 200, false, get_tokens_success_handler, state);

   rest_api_request_execute(request);
}

#if defined(_WIN32) || defined(_Win64)
static void listen_for_response(void *data)
#else
static void *listen_for_response(void *data)
#endif
{
   int on;
   struct addrinfo hints;
   struct addrinfo *server_addr = NULL;
   int client_addr_size;
   char port_string[6];
   SOCKET clientfd = -1;
   bool have_client = false;
   char request[4096];
   size_t request_offset = 0;
   char *code = NULL;
   struct authorize_state_t *authorize_state;
   struct timeval timeout;

   authorize_state = (struct authorize_state_t *)data;

   memset(request, 0, sizeof(request));

   if (listen(authorize_state->sockfd, 2) < 0)
   {
      goto cleanup;
   }

   while (1)
   {
      int rc;
      int i;
      int current_nfds;
      fd_set read_set;
      fd_set write_set;

      FD_ZERO(&read_set);
      FD_SET(authorize_state->sockfd, &read_set);
      FD_ZERO(&write_set);
      FD_SET(authorize_state->sockfd, &write_set);
      if (have_client)
      {
         FD_SET(clientfd, &read_set);
         FD_SET(clientfd, &write_set);
      }

      timeout.tv_sec = 5 * 60;
      timeout.tv_usec = 0;
      rc = socket_select(have_client ? clientfd + 1 : authorize_state->sockfd + 1, &read_set, &write_set, (fd_set *)NULL, &timeout);
      if (rc < 0)
      {
         continue;
      } else if (rc == 0)
      {
         goto cleanup;
      }

      if (have_client && FD_ISSET(clientfd, &read_set))
      {
         while (1)
         {
            bool error = false;
            size_t bytes_read;

            bytes_read = socket_receive_all_nonblocking(clientfd, &error, request + request_offset, sizeof(request) - request_offset - 1);
            if (error)
            {
               if (errno != EWOULDBLOCK)
               {
                  goto cleanup;
               }
               break;
            } else if (bytes_read == 0)
            {
               goto process_request;
            } else
            {
               request_offset += bytes_read;
               break;
            }
         }

         goto process_request;
      } else if (FD_ISSET(authorize_state->sockfd, &read_set))
      {
         clientfd = accept(authorize_state->sockfd, NULL, NULL);
         if (clientfd < 0)
         {
            if (errno != EWOULDBLOCK)
            {
               goto cleanup;
            }
         } else if (!have_client)
         {
            have_client = true;
         }
      }
   }

process_request:
   if (have_client && clientfd > 0)
   {
      char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
      send(clientfd, response, strlen(response), 0);
      socket_close(clientfd);
      clientfd = 0;
   }

   socket_close(authorize_state->sockfd);
   authorize_state->sockfd = 0;

   if (parse_request(request, request_offset, &code))
   {
      get_tokens(code, authorize_state);

      if (code)
      {
         free(code);
      }

      if (have_client && clientfd > 0)
      {
#if defined(_WIN32) || defined(_WIN64)
         shutdown(clientfd, SD_BOTH);
#else
         shutdown(clientfd, SHUT_WR);
#endif
         socket_close(clientfd);
         clientfd = 0;
      }

      if (server_addr)
      {
         freeaddrinfo(server_addr);
      }

#if defined(_WIN32) || defined(_WIN64)
      return;
#else
      pthread_exit(NULL);
#endif
   }

cleanup:
   if (code)
   {
      free(code);
   }

   if (have_client && clientfd > 0)
   {
#if defined(_WIN32) || defined(_WIN64)
      shutdown(clientfd, SD_BOTH);
#else
      shutdown(clientfd, SHUT_WR);
#endif
      socket_close(clientfd);
      clientfd = 0;
   }

   if (authorize_state->sockfd != 0)
   {
      socket_close(authorize_state->sockfd);
   }

   if (server_addr)
   {
      freeaddrinfo_retro(server_addr);
   }

   if (authorize_state)
   {
      if (authorize_state->code_verifier)
      {
         free(authorize_state->code_verifier);
      }
      free(authorize_state);
   }
#if defined(_WIN32) || defined(_WIN64)
   return;
#else
   pthread_exit(NULL);
#endif
}

static bool listen_for_response_thread(struct authorize_state_t *authorize_state)
{
   u_long on = 1;
   struct addrinfo hints;
   struct addrinfo *server_addr = NULL;
   char port_string[6];

   authorize_state->port = 9000;

   if (!network_init())
   {
      return false;
   }

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;
   hints.ai_protocol = IPPROTO_TCP;

   for (;authorize_state->port < 9100;authorize_state->port++)
   {
      snprintf(port_string, 6, "%d", authorize_state->port);

      if (getaddrinfo_retro("127.0.0.1", port_string, &hints, &server_addr) != 0)
      {
         if (server_addr)
         {
            freeaddrinfo_retro(server_addr);
         }

         return false;
      }

      authorize_state->sockfd = socket_create("Google Authorization", SOCKET_DOMAIN_INET, SOCKET_TYPE_STREAM, SOCKET_PROTOCOL_TCP);
      if (authorize_state->sockfd == -1)
      {
         continue;
      }

      if (!socket_nonblock(authorize_state->sockfd))
      {
         socket_close(authorize_state->sockfd);
         continue;
      }

      if (socket_bind(authorize_state->sockfd, server_addr))
      {
         break;
      } else
      {
         socket_close(authorize_state->sockfd);
         break;
      }
   }

   if (authorize_state->port == 9100)
   {
      socket_close(authorize_state->sockfd);

      if (server_addr)
      {
         freeaddrinfo_retro(server_addr);
      }

      return false;
   } else
   {
#if defined(_WIN32) || defined(_WIN64)
      _beginthread(listen_for_response, 0, authorize_state);
#else
      pthread_t thread;
      pthread_create(&thread, NULL, listen_for_response, authorize_state);
#endif
      return true;
   }
}

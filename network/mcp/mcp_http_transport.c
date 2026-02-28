/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <compat/strcasestr.h>
#include <net/net_compat.h>
#include <net/net_socket.h>

#include "mcp_http_transport.h"
#include "mcp_defines.h"
#include "../../verbosity.h"

/* HTTP state machine for non-blocking I/O */
enum mcp_http_state
{
   MCP_HTTP_STATE_IDLE = 0,        /* listening, no client connected */
   MCP_HTTP_STATE_READING_HEADER,  /* client connected, reading headers */
   MCP_HTTP_STATE_READING_BODY,    /* headers parsed, reading body */
   MCP_HTTP_STATE_REQUEST_READY,   /* full request parsed, ready to dispatch */
   MCP_HTTP_STATE_SENDING          /* sending response */
};

enum mcp_http_method
{
   MCP_HTTP_METHOD_UNKNOWN = 0,
   MCP_HTTP_METHOD_OPTIONS,
   MCP_HTTP_METHOD_GET,
   MCP_HTTP_METHOD_POST
};

typedef struct mcp_http_state_data
{
   int server_fd;
   int client_fd;
   enum mcp_http_state state;

   /* receive buffer for current request */
   char recv_buf[MCP_HTTP_MAX_REQUEST];
   size_t recv_buf_len;

   /* parsed header info */
   int header_end;      /* offset past \r\n\r\n */
   int content_length;
   enum mcp_http_method method;

   /* config */
   char password[256];
   char bind_address[256];
   unsigned port;
} mcp_http_state_t;

/* ---- helpers ---- */

static bool mcp_http_extract_path(const char *request, char *path_buf, size_t path_buf_size)
{
   const char *method_end;
   const char *path_end;
   size_t path_len;

   method_end = strchr(request, ' ');
   if (!method_end)
      return false;

   method_end++;
   path_end = strchr(method_end, ' ');
   if (!path_end)
      return false;

   path_len = (size_t)(path_end - method_end);
   if (path_len >= path_buf_size)
      path_len = path_buf_size - 1;
   memcpy(path_buf, method_end, path_len);
   path_buf[path_len] = '\0';
   return true;
}

static bool mcp_http_check_auth(const char *request, const char *password)
{
   const char *auth_header;
   const char *value_start;
   const char *line_end;
   size_t pw_len;

   /* no password configured = no auth required */
   if (string_is_empty(password))
      return true;

   /* look for Authorization header (case-insensitive) */
   auth_header = strcasestr(request, "Authorization:");
   if (!auth_header)
      return false;

   value_start = auth_header + STRLEN_CONST("Authorization:");
   while (*value_start == ' ' || *value_start == '\t')
      value_start++;

   /* expect "Bearer <password>" */
   if (strncmp(value_start, "Bearer ", STRLEN_CONST("Bearer ")) != 0)
      return false;

   value_start += STRLEN_CONST("Bearer ");
   line_end     = strstr(value_start, "\r\n");
   pw_len       = strlen(password);

   if (line_end)
   {
      if ((size_t)(line_end - value_start) != pw_len)
         return false;
      return (memcmp(value_start, password, pw_len) == 0);
   }

   return string_is_equal(value_start, password);
}

static int mcp_http_parse_content_length(const char *headers)
{
   const char *cl;
   const char *val;

   cl = strcasestr(headers, "Content-Length:");
   if (!cl)
      return 0;

   val = cl + STRLEN_CONST("Content-Length:");
   while (*val == ' ' || *val == '\t')
      val++;

   return atoi(val);
}

static void mcp_http_send_raw(int fd, const char *data, size_t len)
{
   size_t sent = 0;
   while (sent < len)
   {
      ssize_t n = send(fd, data + sent, len - sent, 0);
      if (n <= 0)
         break;
      sent += (size_t)n;
   }
}

static void mcp_http_send_error(int fd, int status_code, const char *status_text, const char *body)
{
   char header[512];
   size_t body_len  = strlen(body);
   int header_len   = snprintf(header, sizeof(header),
         "HTTP/1.1 %d %s\r\n"
         "Content-Type: text/plain\r\n"
         "Content-Length: %u\r\n"
         "Access-Control-Allow-Origin: *\r\n"
         "Connection: close\r\n"
         "\r\n",
         status_code, status_text, (unsigned)body_len);
   mcp_http_send_raw(fd, header, (size_t)header_len);
   mcp_http_send_raw(fd, body, body_len);
}

static void mcp_http_send_options(int fd)
{
   const char *response =
      "HTTP/1.1 204 No Content\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
      "Access-Control-Allow-Headers: Content-Type, Accept, Authorization, "
      "MCP-Session-Id, MCP-Protocol-Version\r\n"
      "Connection: close\r\n"
      "\r\n";
   mcp_http_send_raw(fd, response, strlen(response));
}

static void mcp_http_close_client(mcp_http_state_t *st)
{
   if (st->client_fd >= 0)
   {
      socket_close(st->client_fd);
      st->client_fd = -1;
   }
   st->state          = MCP_HTTP_STATE_IDLE;
   st->recv_buf_len   = 0;
   st->header_end     = -1;
   st->content_length = -1;
   st->method         = MCP_HTTP_METHOD_UNKNOWN;
}

/* ---- interface implementation ---- */

static bool mcp_http_init(mcp_transport_t *transport, const char *bind_address, unsigned port, const char *password)
{
   mcp_http_state_t *st;
   struct sockaddr_in addr;
   int opt = 1;
   int fd;

   if (!network_init())
   {
      RARCH_ERR("[MCP] Failed to init network\n");
      return false;
   }

   st = (mcp_http_state_t *)calloc(1, sizeof(*st));
   if (!st)
      return false;

   st->server_fd      = -1;
   st->client_fd      = -1;
   st->state          = MCP_HTTP_STATE_IDLE;
   st->header_end     = -1;
   st->content_length = -1;
   st->port           = port;

   if (password && *password)
      strlcpy(st->password, password, sizeof(st->password));
   if (bind_address && *bind_address)
      strlcpy(st->bind_address, bind_address, sizeof(st->bind_address));
   else
      strlcpy(st->bind_address, "127.0.0.1", sizeof(st->bind_address));

   fd = socket(AF_INET, SOCK_STREAM, 0);
   if (fd < 0)
   {
      RARCH_ERR("[MCP] Failed to create socket\n");
      free(st);
      return false;
   }

   setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_port        = htons((uint16_t)port);
   addr.sin_addr.s_addr = inet_addr(st->bind_address);

   if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
   {
      RARCH_ERR("[MCP] Failed to bind to %s:%u\n", st->bind_address, port);
      socket_close(fd);
      free(st);
      return false;
   }

   if (listen(fd, 1) < 0)
   {
      RARCH_ERR("[MCP] Failed to listen on socket\n");
      socket_close(fd);
      free(st);
      return false;
   }

   /* set server socket to non-blocking */
   socket_nonblock(fd);

   st->server_fd     = fd;
   transport->state  = st;

   RARCH_LOG("[MCP] HTTP server listening on http://%s:%u%s\n",
         st->bind_address,
         port,
         MCP_HTTP_ENDPOINT_PATH);
   return true;
}

static bool mcp_http_poll(mcp_transport_t *transport, char *out_buf, size_t out_buf_size, size_t *out_len)
{
   mcp_http_state_t *st = (mcp_http_state_t *)transport->state;
   char path_buf[256];

   if (!st || st->server_fd < 0)
      return false;

   switch (st->state)
   {
      case MCP_HTTP_STATE_IDLE:
      {
         /* Try to accept a new connection */
         struct sockaddr_in client_addr;
         socklen_t client_len = sizeof(client_addr);
         int client = accept(st->server_fd,
               (struct sockaddr *)&client_addr, &client_len);

         if (client < 0)
            return false; /* no connection pending */

         socket_nonblock(client);
         st->client_fd    = client;
         st->state        = MCP_HTTP_STATE_READING_HEADER;
         st->recv_buf_len = 0;
         st->header_end   = -1;
         st->content_length = -1;
         st->method       = MCP_HTTP_METHOD_UNKNOWN;

         RARCH_DBG("[MCP] Client connected (fd=%d)\n", client);
         return false;
      }

      case MCP_HTTP_STATE_READING_HEADER:
      case MCP_HTTP_STATE_READING_BODY:
      {
         char tmp[MCP_HTTP_RECV_BUF_SIZE];
         ssize_t n = recv(st->client_fd, tmp, sizeof(tmp), 0);

         if (n == 0)
         {
            /* client disconnected */
            RARCH_DBG("[MCP] Client disconnected\n");
            mcp_http_close_client(st);
            return false;
         }

         if (n < 0)
         {
            if (isagain((int)n) || isagain(errno))
               return false; /* no data yet */
            mcp_http_close_client(st);
            return false;
         }

         /* check size limit */
         if (st->recv_buf_len + (size_t)n > MCP_HTTP_MAX_REQUEST)
         {
            mcp_http_send_error(st->client_fd, 413, "Payload Too Large", "Request too large");
            mcp_http_close_client(st);
            return false;
         }

         memcpy(st->recv_buf + st->recv_buf_len, tmp, (size_t)n);
         st->recv_buf_len += (size_t)n;
         st->recv_buf[st->recv_buf_len] = '\0';

         /* look for end of headers if not found yet */
         if (st->header_end < 0)
         {
            char *header_term = strstr(st->recv_buf, "\r\n\r\n");
            if (!header_term)
               return false; /* need more data */

            st->header_end = (int)(header_term - st->recv_buf) + 4;

            /* determine method */
            if (strncmp(st->recv_buf, "OPTIONS ", 8) == 0)
               st->method = MCP_HTTP_METHOD_OPTIONS;
            else if (strncmp(st->recv_buf, "GET ", 4) == 0)
               st->method = MCP_HTTP_METHOD_GET;
            else if (strncmp(st->recv_buf, "POST ", 5) == 0)
               st->method = MCP_HTTP_METHOD_POST;

            st->content_length = mcp_http_parse_content_length(st->recv_buf);

            RARCH_DBG("[MCP] Headers complete: method=%s, content_length=%d, path=%s\n",
                  st->method == MCP_HTTP_METHOD_POST ? "POST" :
                  st->method == MCP_HTTP_METHOD_GET ? "GET" :
                  st->method == MCP_HTTP_METHOD_OPTIONS ? "OPTIONS" : "UNKNOWN",
                  st->content_length,
                  mcp_http_extract_path(st->recv_buf, path_buf, sizeof(path_buf)) ? path_buf : "?");

            st->state = MCP_HTTP_STATE_READING_BODY;
         }

         /* check if we have full body */
         if (st->header_end >= 0 && st->content_length >= 0)
         {
            int body_len = (int)st->recv_buf_len - st->header_end;
            if (body_len < st->content_length)
               return false; /* need more data */
         }

         /* full request received - now process */

         /* CORS preflight */
         if (st->method == MCP_HTTP_METHOD_OPTIONS)
         {
            RARCH_DBG("[MCP] Handling OPTIONS (CORS preflight)\n");
            if (mcp_http_extract_path(st->recv_buf, path_buf, sizeof(path_buf))
                  && !string_is_equal(path_buf, MCP_HTTP_ENDPOINT_PATH))
            {
               mcp_http_send_error(st->client_fd, 404, "Not Found", "404 Not Found");
            }
            else
               mcp_http_send_options(st->client_fd);

            mcp_http_close_client(st);
            return false;
         }

         /* path validation */
         if (!mcp_http_extract_path(st->recv_buf, path_buf, sizeof(path_buf))
               || !string_is_equal(path_buf, MCP_HTTP_ENDPOINT_PATH))
         {
            RARCH_DBG("[MCP] Rejecting invalid path: %s\n", path_buf);
            mcp_http_send_error(st->client_fd, 404, "Not Found", "404 Not Found");
            mcp_http_close_client(st);
            return false;
         }

         /* auth check */
         if (!mcp_http_check_auth(st->recv_buf, st->password))
         {
            RARCH_DBG("[MCP] Auth failed\n");
            mcp_http_send_error(st->client_fd, 401, "Unauthorized", "401 Unauthorized");
            mcp_http_close_client(st);
            return false;
         }

         /* GET (SSE not supported) */
         if (st->method == MCP_HTTP_METHOD_GET)
         {
            mcp_http_send_error(st->client_fd, 405,
                  "Method Not Allowed",
                  "{\"error\":\"SSE streaming not supported\"}");
            mcp_http_close_client(st);
            return false;
         }

         /* POST with body */
         if (st->method == MCP_HTTP_METHOD_POST && st->content_length > 0)
         {
            size_t body_len = (size_t)st->content_length;
            RARCH_DBG("[MCP] POST body (%d bytes): %.*s\n",
                  st->content_length,
                  st->content_length > 200 ? 200 : st->content_length,
                  st->recv_buf + st->header_end);

            if (body_len >= out_buf_size)
               body_len = out_buf_size - 1;

            memcpy(out_buf, st->recv_buf + st->header_end, body_len);
            out_buf[body_len] = '\0';
            *out_len = body_len;
            st->state = MCP_HTTP_STATE_REQUEST_READY;
            return true;
         }

         /* POST without body or other */
         mcp_http_close_client(st);
         return false;
      }

      case MCP_HTTP_STATE_REQUEST_READY:
      case MCP_HTTP_STATE_SENDING:
         /* waiting for send() to be called */
         return false;

      default:
         break;
   }

   return false;
}

static bool mcp_http_send(mcp_transport_t *transport, const char *data, size_t len)
{
   mcp_http_state_t *st = (mcp_http_state_t *)transport->state;
   char header[512];
   int header_len;

   if (!st || st->client_fd < 0)
      return false;

   header_len = snprintf(header, sizeof(header),
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: application/json\r\n"
         "Content-Length: %u\r\n"
         "Access-Control-Allow-Origin: *\r\n"
         "Connection: close\r\n"
         "\r\n",
         (unsigned)len);

   RARCH_DBG("[MCP] Sending response (%u bytes): %.*s\n", (unsigned)len, len > 200 ? 200 : (int)len, data);

   mcp_http_send_raw(st->client_fd, header, (size_t)header_len);
   mcp_http_send_raw(st->client_fd, data, len);

   mcp_http_close_client(st);
   return true;
}

static void mcp_http_close(mcp_transport_t *transport)
{
   mcp_http_state_t *st = (mcp_http_state_t *)transport->state;
   if (!st)
      return;

   mcp_http_close_client(st);

   if (st->server_fd >= 0)
   {
      socket_close(st->server_fd);
      st->server_fd = -1;
   }

   free(st);
   transport->state = NULL;

   RARCH_LOG("[MCP] HTTP server closed\n");
}

const mcp_transport_interface_t mcp_http_transport = {
   mcp_http_init,
   mcp_http_poll,
   mcp_http_send,
   mcp_http_close
};

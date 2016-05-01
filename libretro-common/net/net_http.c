/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_http.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <net/net_http.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <compat/strl.h>

enum
{
   P_HEADER_TOP = 0,
   P_HEADER,
   P_BODY,
   P_BODY_CHUNKLEN,
   P_DONE,
   P_ERROR
};

enum
{
   T_FULL = 0,
   T_LEN,
   T_CHUNK
};

struct http_t
{
   int fd;
   int status;
   
   char part;
   char bodytype;
   bool error;
   
   size_t pos;
   size_t len;
   size_t buflen;
   char * data;
};

struct http_connection_t
{
   char *domain;
   char *location;
   char *urlcopy;
   char *scan;
   int port;
};


static int net_http_new_socket(const char *domain, int port)
{
   int ret;
   struct addrinfo *addr = NULL;
   int fd = socket_init((void**)&addr, port, domain, SOCKET_TYPE_STREAM);
   if (fd < 0)
      return -1;

   ret = socket_connect(fd, (void*)addr, true);

   freeaddrinfo_retro(addr);

   if (ret < 0)
      goto error;

   if (!socket_nonblock(fd))
      goto error;

   return fd;

error:
   socket_close(fd);
   return -1;
}

static void net_http_send_str(int fd, bool *error, const char *text)
{
   if (!*error)
   {
      if (!socket_send_all_blocking(fd, text, strlen(text), true))
         *error = true;
   }
}

static ssize_t net_http_recv(int fd, bool *error,
      uint8_t *data, size_t maxlen)
{
   ssize_t bytes;

   if (*error)
      return -1;

   bytes = recv(fd, (char*)data, maxlen, 0);

   if (bytes > 0)
      return bytes;
   else if (bytes == 0)
      return -1;
   else if (isagain(bytes))
      return 0;

   *error=true;
   return -1;
}

static char* urlencode(const char* url)
{
   unsigned i;
   unsigned outpos = 0;
   unsigned outlen = 0;
   char *ret  = NULL;

   for (i = 0; url[i] != '\0'; i++)
   {
      outlen++;
      if (url[i] == ' ')
         outlen += 2;
   }
   
   ret = (char*)malloc(outlen + 1);
   if (!ret)
      return NULL;
   
   for (i = 0; url[i]; i++)
   {
      if (url[i] == ' ')
      {
         ret[outpos++] = '%';
         ret[outpos++] = '2';
         ret[outpos++] = '0';
      }
      else
         ret[outpos++] = url[i];
   }
   ret[outpos] = '\0';
   
   return ret;
}

struct http_connection_t *net_http_connection_new(const char *url)
{
   char **domain = NULL;
   struct http_connection_t *conn = (struct http_connection_t*)calloc(1, 
         sizeof(struct http_connection_t));

   if (!conn)
      return NULL;

   conn->urlcopy = urlencode(url);

   if (!conn->urlcopy)
      goto error;

   if (strncmp(url, "http://", strlen("http://")) != 0)
      goto error;

   conn->scan    = conn->urlcopy + strlen("http://");
   domain        = &conn->domain;
   *domain       = conn->scan;

   return conn;

error:
   if (conn->urlcopy)
      free(conn->urlcopy);
   conn->urlcopy = NULL;
   free(conn);
   return NULL;
}

bool net_http_connection_iterate(struct http_connection_t *conn)
{
   if (!conn)
      return false;

   while (*conn->scan != '/' && *conn->scan != ':' && *conn->scan != '\0')
      conn->scan++;

   return true;
}

bool net_http_connection_done(struct http_connection_t *conn)
{
   char **location = NULL;

   if (!conn)
      return false;

   location     = &conn->location;

   if (*conn->scan == '\0')
      return false;

   *conn->scan  = '\0';
   conn->port   = 80;

   if (*conn->scan == ':')
   {
      if (!isdigit((int)conn->scan[1]))
         return false;

      conn->port = strtoul(conn->scan + 1, &conn->scan, 10);

      if (*conn->scan != '/')
         return false;
   }

   *location = conn->scan + 1;

   return true;
}

void net_http_connection_free(struct http_connection_t *conn)
{
   if (!conn)
      return;

   if (conn->urlcopy)
      free(conn->urlcopy);

   free(conn);
}

const char *net_http_connection_url(struct http_connection_t *conn)
{
   return conn->urlcopy;
}

struct http_t *net_http_new(struct http_connection_t *conn)
{
   bool error            = false;
   int fd                = -1;
   struct http_t *state  = NULL;

   if (!conn)
      goto error;

   fd = net_http_new_socket(conn->domain, conn->port);
   if (fd < 0)
      goto error;

   error = false;

   /* This is a bit lazy, but it works. */
   net_http_send_str(fd, &error, "GET /");
   net_http_send_str(fd, &error, conn->location);
   net_http_send_str(fd, &error, " HTTP/1.1\r\n");

   net_http_send_str(fd, &error, "Host: ");
   net_http_send_str(fd, &error, conn->domain);

   if (conn->port != 80)
   {
      char portstr[16] = {0};

      snprintf(portstr, sizeof(portstr), ":%i", conn->port);
      net_http_send_str(fd, &error, portstr);
   }

   net_http_send_str(fd, &error, "\r\n");
   net_http_send_str(fd, &error, "Connection: close\r\n");
   net_http_send_str(fd, &error, "\r\n");

   if (error)
      goto error;

   state          = (struct http_t*)malloc(sizeof(struct http_t));
   state->fd      = fd;
   state->status  = -1;
   state->data    = NULL;
   state->part    = P_HEADER_TOP;
   state->bodytype= T_FULL;
   state->error   = false;
   state->pos     = 0;
   state->len     = 0;
   state->buflen  = 512;
   state->data    = (char*)malloc(state->buflen);

   if (!state->data)
      goto error;

   return state;

error:
   if (fd >= 0)
      socket_close(fd);
   return NULL;
}

int net_http_fd(struct http_t *state)
{
   if (!state)
      return -1;
   return state->fd;
}

bool net_http_update(struct http_t *state, size_t* progress, size_t* total)
{
   ssize_t newlen = 0;

   if (!state || state->error)
      goto fail;

   if (state->part < P_BODY)
   {
      newlen = net_http_recv(state->fd, &state->error,
            (uint8_t*)state->data + state->pos, state->buflen - state->pos);

      if (newlen < 0)
         goto fail;

      if (state->pos + newlen >= state->buflen - 64)
      {
         state->buflen *= 2;
         state->data = (char*)realloc(state->data, state->buflen);
      }
      state->pos += newlen;

      while (state->part < P_BODY)
      {
         char *dataend = state->data + state->pos;
         char *lineend = (char*)memchr(state->data, '\n', state->pos);

         if (!lineend)
            break;
         *lineend='\0';
         if (lineend != state->data && lineend[-1]=='\r')
            lineend[-1]='\0';

         if (state->part == P_HEADER_TOP)
         {
            if (strncmp(state->data, "HTTP/1.", strlen("HTTP/1."))!=0)
               goto fail;
            state->status = strtoul(state->data + strlen("HTTP/1.1 "), NULL, 10);
            state->part   = P_HEADER;
         }
         else
         {
            if (!strncmp(state->data, "Content-Length: ",
                     strlen("Content-Length: ")))
            {
               state->bodytype = T_LEN;
               state->len = strtol(state->data + 
                     strlen("Content-Length: "), NULL, 10);
            }
            if (!strcmp(state->data, "Transfer-Encoding: chunked"))
               state->bodytype = T_CHUNK;

            /* TODO: save headers somewhere */
            if (state->data[0]=='\0')
            {
               state->part = P_BODY;
               if (state->bodytype == T_CHUNK)
                  state->part = P_BODY_CHUNKLEN;
            }
         }

         memmove(state->data, lineend + 1, dataend-(lineend+1));
         state->pos = (dataend-(lineend + 1));
      }
      if (state->part >= P_BODY)
      {
         newlen = state->pos;
         state->pos = 0;
      }
   }

   if (state->part >= P_BODY && state->part < P_DONE)
   {
      if (!newlen)
      {
         newlen = net_http_recv(state->fd, &state->error,
               (uint8_t*)state->data + state->pos,
               state->buflen - state->pos);

         if (newlen < 0)
         {
            if (state->bodytype == T_FULL)
            {
               state->part = P_DONE;
               state->data = (char*)realloc(state->data, state->len);
            }
            else
               goto fail;
            newlen=0;
         }

         if (state->pos + newlen >= state->buflen - 64)
         {
            state->buflen *= 2;
            state->data = (char*)realloc(state->data, state->buflen);
         }
      }

parse_again:
      if (state->bodytype == T_CHUNK)
      {
         if (state->part == P_BODY_CHUNKLEN)
         {
            state->pos += newlen;
            if (state->pos - state->len >= 2)
            {
               /*
                * len=start of chunk including \r\n
                * pos=end of data
                */

               char *fullend = state->data + state->pos;
               char *end     = (char*)memchr(state->data + state->len + 2, '\n',
                                             state->pos - state->len - 2);

               if (end)
               {
                  size_t chunklen = strtoul(state->data+state->len, NULL, 16);
                  state->pos      = state->len;
                  end++;

                  memmove(state->data+state->len, end, fullend-end);

                  state->len      = chunklen;
                  newlen          = (fullend - end);

                  /*
                     len=num bytes
                     newlen=unparsed bytes after \n
                     pos=start of chunk including \r\n
                     */

                  state->part = P_BODY;
                  if (state->len == 0)
                  {
                     state->part = P_DONE;
                     state->len  = state->pos;
                     state->data = (char*)realloc(state->data, state->len);
                  }
                  goto parse_again;
               }
            }
         }
         else if (state->part == P_BODY)
         {
            if ((size_t)newlen >= state->len)
            {
               state->pos += state->len;
               newlen     -= state->len;
               state->len  = state->pos;
               state->part = P_BODY_CHUNKLEN;
               goto parse_again;
            }
            else
            {
               state->pos += newlen;
               state->len -= newlen;
            }
         }
      }
      else
      {
         state->pos += newlen;

         if (state->pos == state->len)
         {
            state->part = P_DONE;
            state->data = (char*)realloc(state->data, state->len);
         }
         if (state->pos > state->len)
            goto fail;
      }
   }

   if (progress)
      *progress = state->pos;

   if (total)
   {
      if (state->bodytype == T_LEN)
         *total=state->len;
      else
         *total=0;
   }

   return (state->part == P_DONE);

fail:
   if (state)
   {
      state->error  = true;
      state->part   = P_ERROR;
      state->status = -1;
   }

   return true;
}

int net_http_status(struct http_t *state)
{
   if (!state)
      return -1;
   return state->status;
}

uint8_t* net_http_data(struct http_t *state, size_t* len, bool accept_error)
{
   if (!state)
      return NULL;

   if (!accept_error && net_http_error(state))
   {
      if (len)
         *len=0;
      return NULL;
   }

   if (len)
      *len=state->len;

   return (uint8_t*)state->data;
}

void net_http_delete(struct http_t *state)
{
   if (!state)
      return;

   if (state->fd >= 0)
      socket_close(state->fd);
   free(state);
}

bool net_http_error(struct http_t *state)
{
   return (state->error || state->status<200 || state->status>299);
}

/* Copyright  (C) 2010-2020 The RetroArch team
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
#ifdef HAVE_SSL
#include <net/net_socket_ssl.h>
#endif
#include <compat/strl.h>
#include <string/stdstring.h>
#include <string.h>
#include <lists/string_list.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

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

struct http_socket_state_t
{
   void *ssl_ctx;
   int fd;
   bool ssl;
};

struct http_t
{
   char *data;
   struct string_list *headers;
   struct http_socket_state_t sock_state; /* ptr alignment */
   size_t pos;
   size_t len;
   size_t buflen;
   int status;
   char part;
   char bodytype;
   bool error;
};

struct http_connection_t
{
   char *domain;
   char *location;
   char *urlcopy;
   char *scan;
   char *methodcopy;
   char *contenttypecopy;
   void *postdatacopy;
   char *useragentcopy;
   char *headerscopy;
   struct http_socket_state_t sock_state; /* ptr alignment */
   size_t contentlength;
   int port;
};

/**
 * net_http_urlencode:
 *
 * URL Encode a string
 * caller is responsible for deleting the destination buffer
 **/
void net_http_urlencode(char **dest, const char *source)
{
   static const char urlencode_lut[256] = 
   {
      0,       /* 0   */
      0,       /* 1   */
      0,       /* 2   */
      0,       /* 3   */
      0,       /* 4   */
      0,       /* 5   */
      0,       /* 6   */
      0,       /* 7   */
      0,       /* 8   */
      0,       /* 9   */
      0,       /* 10  */
      0,       /* 11  */
      0,       /* 12  */
      0,       /* 13  */
      0,       /* 14  */
      0,       /* 15  */
      0,       /* 16  */
      0,       /* 17  */
      0,       /* 18  */
      0,       /* 19  */
      0,       /* 20  */
      0,       /* 21  */
      0,       /* 22  */
      0,       /* 23  */
      0,       /* 24  */
      0,       /* 25  */
      0,       /* 26  */
      0,       /* 27  */
      0,       /* 28  */
      0,       /* 29  */
      0,       /* 30  */
      0,       /* 31  */
      0,       /* 32  */
      0,       /* 33  */
      0,       /* 34  */
      0,       /* 35  */
      0,       /* 36  */
      0,       /* 37  */
      0,       /* 38  */
      0,       /* 39  */
      0,       /* 40  */
      0,       /* 41  */
      '*',     /* 42  */
      0,       /* 43  */
      0,       /* 44  */
      '-',     /* 45  */
      '.',     /* 46  */
      '/',     /* 47  */
      '0',     /* 48  */
      '1',     /* 49  */
      '2',     /* 50  */
      '3',     /* 51  */
      '4',     /* 52  */
      '5',     /* 53  */
      '6',     /* 54  */
      '7',     /* 55  */
      '8',     /* 56  */
      '9',     /* 57  */
      0,       /* 58  */
      0,       /* 59  */
      0,       /* 60  */
      0,       /* 61  */
      0,       /* 62  */
      0,       /* 63  */
      0,       /* 64  */
      'A',     /* 65  */
      'B',     /* 66  */
      'C',     /* 67  */
      'D',     /* 68  */
      'E',     /* 69  */
      'F',     /* 70  */
      'G',     /* 71  */
      'H',     /* 72  */
      'I',     /* 73  */
      'J',     /* 74  */
      'K',     /* 75  */
      'L',     /* 76  */
      'M',     /* 77  */
      'N',     /* 78  */
      'O',     /* 79  */
      'P',     /* 80  */
      'Q',     /* 81  */
      'R',     /* 82  */
      'S',     /* 83  */
      'T',     /* 84  */
      'U',     /* 85  */
      'V',     /* 86  */
      'W',     /* 87  */
      'X',     /* 88  */
      'Y',     /* 89  */
      'Z',     /* 90  */
      0,       /* 91  */
      0,       /* 92  */
      0,       /* 93  */
      0,       /* 94  */
      '_',     /* 95  */
      0,       /* 96  */
      'a',     /* 97  */
      'b',     /* 98  */
      'c',     /* 99  */
      'd',     /* 100 */
      'e',     /* 101 */
      'f',     /* 102 */
      'g',     /* 103 */
      'h',     /* 104 */
      'i',     /* 105 */
      'j',     /* 106 */
      'k',     /* 107 */
      'l',     /* 108 */
      'm',     /* 109 */
      'n',     /* 110 */
      'o',     /* 111 */
      'p',     /* 112 */
      'q',     /* 113 */
      'r',     /* 114 */
      's',     /* 115 */
      't',     /* 116 */
      'u',     /* 117 */
      'v',     /* 118 */
      'w',     /* 119 */
      'x',     /* 120 */
      'y',     /* 121 */
      'z',     /* 122 */
      0,       /* 123 */
      0,       /* 124 */
      0,       /* 125 */
      0,       /* 126 */
      0,       /* 127 */
      0,       /* 128 */
      0,       /* 129 */
      0,       /* 130 */
      0,       /* 131 */
      0,       /* 132 */
      0,       /* 133 */
      0,       /* 134 */
      0,       /* 135 */
      0,       /* 136 */
      0,       /* 137 */
      0,       /* 138 */
      0,       /* 139 */
      0,       /* 140 */
      0,       /* 141 */
      0,       /* 142 */
      0,       /* 143 */
      0,       /* 144 */
      0,       /* 145 */
      0,       /* 146 */
      0,       /* 147 */
      0,       /* 148 */
      0,       /* 149 */
      0,       /* 150 */
      0,       /* 151 */
      0,       /* 152 */
      0,       /* 153 */
      0,       /* 154 */
      0,       /* 155 */
      0,       /* 156 */
      0,       /* 157 */
      0,       /* 158 */
      0,       /* 159 */
      0,       /* 160 */
      0,       /* 161 */
      0,       /* 162 */
      0,       /* 163 */
      0,       /* 164 */
      0,       /* 165 */
      0,       /* 166 */
      0,       /* 167 */
      0,       /* 168 */
      0,       /* 169 */
      0,       /* 170 */
      0,       /* 171 */
      0,       /* 172 */
      0,       /* 173 */
      0,       /* 174 */
      0,       /* 175 */
      0,       /* 176 */
      0,       /* 177 */
      0,       /* 178 */
      0,       /* 179 */
      0,       /* 180 */
      0,       /* 181 */
      0,       /* 182 */
      0,       /* 183 */
      0,       /* 184 */
      0,       /* 185 */
      0,       /* 186 */
      0,       /* 187 */
      0,       /* 188 */
      0,       /* 189 */
      0,       /* 190 */
      0,       /* 191 */
      0,       /* 192 */
      0,       /* 193 */
      0,       /* 194 */
      0,       /* 195 */
      0,       /* 196 */
      0,       /* 197 */
      0,       /* 198 */
      0,       /* 199 */
      0,       /* 200 */
      0,       /* 201 */
      0,       /* 202 */
      0,       /* 203 */
      0,       /* 204 */
      0,       /* 205 */
      0,       /* 206 */
      0,       /* 207 */
      0,       /* 208 */
      0,       /* 209 */
      0,       /* 210 */
      0,       /* 211 */
      0,       /* 212 */
      0,       /* 213 */
      0,       /* 214 */
      0,       /* 215 */
      0,       /* 216 */
      0,       /* 217 */
      0,       /* 218 */
      0,       /* 219 */
      0,       /* 220 */
      0,       /* 221 */
      0,       /* 222 */
      0,       /* 223 */
      0,       /* 224 */
      0,       /* 225 */
      0,       /* 226 */
      0,       /* 227 */
      0,       /* 228 */
      0,       /* 229 */
      0,       /* 230 */
      0,       /* 231 */
      0,       /* 232 */
      0,       /* 233 */
      0,       /* 234 */
      0,       /* 235 */
      0,       /* 236 */
      0,       /* 237 */
      0,       /* 238 */
      0,       /* 239 */
      0,       /* 240 */
      0,       /* 241 */
      0,       /* 242 */
      0,       /* 243 */
      0,       /* 244 */
      0,       /* 245 */
      0,       /* 246 */
      0,       /* 247 */
      0,       /* 248 */
      0,       /* 249 */
      0,       /* 250 */
      0,       /* 251 */
      0,       /* 252 */
      0,       /* 253 */
      0,       /* 254 */
      0        /* 255 */
   };

   /* Assume every character will be encoded, so we need 3 times the space. */
   size_t len                       = strlen(source) * 3 + 1;
   size_t count                     = len;
   char *enc                        = (char*)calloc(1, len);
   *dest                            = enc;

   for (; *source; source++)
   {
      int written = 0;

      /* any non-ASCII character will just be encoded without question */
      if ((unsigned)*source < sizeof(urlencode_lut) && urlencode_lut[(unsigned)*source])
         written = snprintf(enc, count, "%c", urlencode_lut[(unsigned)*source]);
      else
         written = snprintf(enc, count, "%%%02X", *source & 0xFF);

      if (written > 0)
         count -= written;

      while (*++enc);
   }

   (*dest)[len - 1] = '\0';
}

/**
 * net_http_urlencode_full:
 *
 * Re-encode a full URL
 **/
void net_http_urlencode_full(char *dest,
      const char *source, size_t size)
{
   size_t buf_pos;
   size_t tmp_len;
   char url_domain[256];
   char url_path[PATH_MAX_LENGTH];
   char *tmp                         = NULL;
   int count                         = 0;

   strlcpy(url_path, source, sizeof(url_path));
   tmp            = url_path;

   while (count < 3 && tmp[0] != '\0')
   {
      tmp = strchr(tmp, '/');
      count++;
      tmp++;
   }

   tmp_len        = strlen(tmp);
   buf_pos        = ((strlcpy(url_domain, source, tmp - url_path)) - tmp_len) - 1;
   strlcpy(url_path,
         source  + buf_pos + 1,
         tmp_len           + 1
         );

   tmp             = NULL;
   net_http_urlencode(&tmp, url_path);
   buf_pos         = strlcpy(dest, url_domain, size);
   dest[  buf_pos] = '/';
   dest[++buf_pos] = '\0';
   strlcpy(dest + buf_pos, tmp, size - buf_pos);
   free(tmp);
}

static int net_http_new_socket(struct http_connection_t *conn)
{
   struct addrinfo *addr = NULL, *next_addr = NULL;
   int fd                = socket_init(
         (void**)&addr, conn->port, conn->domain, SOCKET_TYPE_STREAM, 0);
#ifdef HAVE_SSL
   if (conn->sock_state.ssl)
   {
      if (fd < 0)
         goto done;

      if (!(conn->sock_state.ssl_ctx = ssl_socket_init(fd, conn->domain)))
      {
         socket_close(fd);
         fd = -1;
         goto done;
      }

      /* TODO: Properly figure out what's going wrong when the newer 
         timeout/poll code interacts with mbed and winsock
         https://github.com/libretro/RetroArch/issues/14742 */

      /* Temp fix, don't use new timeout/poll code for cheevos http requests */
         bool timeout = true;
#ifdef __WIN32
      if (!strcmp(conn->domain, "retroachievements.org"))
         timeout = false;
#endif

      if (ssl_socket_connect(conn->sock_state.ssl_ctx, addr, timeout, true)
            < 0)
      {
         fd = -1;
         goto done;
      }
   }
   else
#endif
   for (next_addr = addr; fd >= 0; fd = socket_next((void**)&next_addr))
   {
      if (socket_connect_with_timeout(fd, next_addr, 5000))
         break;

      socket_close(fd);
   }

#ifdef HAVE_SSL
done:
#endif
   if (addr)
      freeaddrinfo_retro(addr);

   conn->sock_state.fd = fd;

   return fd;
}

static void net_http_send_str(
      struct http_socket_state_t *sock_state, bool *error,
      const char *text, size_t text_size)
{
   if (*error)
      return;
#ifdef HAVE_SSL
   if (sock_state->ssl)
   {
      if (!ssl_socket_send_all_blocking(
               sock_state->ssl_ctx, text, text_size, true))
         *error = true;
   }
   else
#endif
   {
      if (!socket_send_all_blocking(
               sock_state->fd, text, text_size, true))
         *error = true;
   }
}

struct http_connection_t *net_http_connection_new(const char *url,
      const char *method, const char *data)
{
   struct http_connection_t *conn = NULL;

   if (!url)
      return NULL;
   if (!(conn = (struct http_connection_t*)malloc(
         sizeof(*conn))))
      return NULL;

   conn->domain            = NULL;
   conn->location          = NULL;
   conn->urlcopy           = NULL;
   conn->scan              = NULL;
   conn->methodcopy        = NULL;
   conn->contenttypecopy   = NULL;
   conn->postdatacopy      = NULL;
   conn->useragentcopy     = NULL;
   conn->headerscopy       = NULL;
   conn->port              = 0;
   conn->sock_state.fd     = 0;
   conn->sock_state.ssl    = false;
   conn->sock_state.ssl_ctx= NULL;

   if (method)
      conn->methodcopy     = strdup(method);

   if (data)
   {
      conn->postdatacopy   = strdup(data);
      conn->contentlength  = strlen(data);
   }

   if (!(conn->urlcopy = strdup(url)))
      goto error;

   if (!strncmp(url, "http://", STRLEN_CONST("http://")))
      conn->scan           = conn->urlcopy + STRLEN_CONST("http://");
   else if (!strncmp(url, "https://", STRLEN_CONST("https://")))
   {
      conn->scan           = conn->urlcopy + STRLEN_CONST("https://");
      conn->sock_state.ssl = true;
   }
   else
      goto error;

   if (string_is_empty(conn->scan))
      goto error;

   conn->domain = conn->scan;

   return conn;

error:
   if (conn->urlcopy)
      free(conn->urlcopy);
   if (conn->methodcopy)
      free(conn->methodcopy);
   if (conn->postdatacopy)
      free(conn->postdatacopy);
   conn->urlcopy      = NULL;
   conn->methodcopy   = NULL;
   conn->postdatacopy = NULL;
   free(conn);
   return NULL;
}

/**
 * net_http_connection_iterate:
 *
 * Leaf function.
 **/
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
   int has_port = 0;

   if (!conn || !conn->domain || !*conn->domain)
      return false;

   if (*conn->scan == ':')
   {
      /* domain followed by port, split off the port */
      *conn->scan++ = '\0';

      if (!isdigit((int)(*conn->scan)))
         return false;

      conn->port = (int)strtoul(conn->scan, &conn->scan, 10);
      has_port   = 1;
   }
   else if (conn->port == 0)
   {
      /* port not specified, default to standard HTTP or HTTPS port */
      if (conn->sock_state.ssl)
         conn->port = 443;
      else
         conn->port = 80;
   }

   if (*conn->scan == '/')
   {
      /* domain followed by location - split off the location */
      /*   site.com/path.html   or   site.com:80/path.html   */
      *conn->scan    = '\0';
      conn->location = conn->scan + 1;
      return true;
   }
   else if (!*conn->scan)
   {
      /* domain with no location - point location at empty string */
      /*   site.com   or   site.com:80   */
      conn->location = conn->scan;
      return true;
   }
   else if (*conn->scan == '?')
   {
      /* domain with no location, but still has query parms - point location at the query parms */
      /*   site.com?param=3   or  site.com:80?param=3   */
      if (!has_port)
      {
         /* if there wasn't a port, we have to expand the urlcopy so we can separate the two parts */
         size_t domain_len   = strlen(conn->domain);
         size_t location_len = strlen(conn->scan);
         char* urlcopy       = (char*)malloc(domain_len + location_len + 2);
         memcpy(urlcopy, conn->domain, domain_len);
         urlcopy[domain_len] = '\0';
         memcpy(urlcopy + domain_len + 1, conn->scan, location_len + 1);

         free(conn->urlcopy);
         conn->domain        = conn->urlcopy = urlcopy;
         conn->location      = conn->scan    = urlcopy + domain_len + 1;
      }
      else /* There was a port, so overwriting the : will terminate the domain and we can just point at the ? */
         conn->location      = conn->scan;

      return true;
   }

   /* invalid character after domain/port */
   return false;
}

void net_http_connection_free(struct http_connection_t *conn)
{
   if (!conn)
      return;

   if (conn->urlcopy)
      free(conn->urlcopy);

   if (conn->methodcopy)
      free(conn->methodcopy);

   if (conn->contenttypecopy)
      free(conn->contenttypecopy);

   if (conn->postdatacopy)
      free(conn->postdatacopy);

   if (conn->useragentcopy)
      free(conn->useragentcopy);

   if (conn->headerscopy)
      free(conn->headerscopy);

   conn->urlcopy         = NULL;
   conn->methodcopy      = NULL;
   conn->contenttypecopy = NULL;
   conn->postdatacopy    = NULL;
   conn->useragentcopy   = NULL;
   conn->headerscopy     = NULL;

   free(conn);
}

void net_http_connection_set_user_agent(
      struct http_connection_t *conn, const char *user_agent)
{
   if (conn->useragentcopy)
      free(conn->useragentcopy);

   conn->useragentcopy = user_agent ? strdup(user_agent) : NULL;
}

void net_http_connection_set_headers(
      struct http_connection_t *conn, const char *headers)
{
   if (conn->headerscopy)
      free(conn->headerscopy);

   conn->headerscopy = headers ? strdup(headers) : NULL;
}

void net_http_connection_set_content(
      struct http_connection_t *conn, const char *content_type,
      size_t content_length, const void *content)

{
   if (conn->contenttypecopy)
      free(conn->contenttypecopy);
   if (conn->postdatacopy)
      free(conn->postdatacopy);

   conn->contenttypecopy = content_type ? strdup(content_type) : NULL;
   conn->contentlength = content_length;
   if (content_length)
   {
      conn->postdatacopy = malloc(content_length);
      memcpy(conn->postdatacopy, content, content_length);
   }
}

const char *net_http_connection_url(struct http_connection_t *conn)
{
   return conn->urlcopy;
}

const char* net_http_connection_method(struct http_connection_t* conn)
{
   return conn->methodcopy;
}

struct http_t *net_http_new(struct http_connection_t *conn)
{
   bool error            = false;
#ifdef HAVE_SSL
   if (!conn || (net_http_new_socket(conn)) < 0)
      return NULL;
#else
   int fd                = -1;
   if (!conn || (fd = net_http_new_socket(conn)) < 0)
      return NULL;
#endif

   /* This is a bit lazy, but it works. */
   if (conn->methodcopy)
   {
      net_http_send_str(&conn->sock_state, &error, conn->methodcopy,
            strlen(conn->methodcopy));
      net_http_send_str(&conn->sock_state, &error, " /",
            STRLEN_CONST(" /"));
   }
   else
   {
      net_http_send_str(&conn->sock_state, &error, "GET /",
            STRLEN_CONST("GET /"));
   }

   net_http_send_str(&conn->sock_state, &error, conn->location,
         strlen(conn->location));
   net_http_send_str(&conn->sock_state, &error, " HTTP/1.1\r\n",
         STRLEN_CONST(" HTTP/1.1\r\n"));

   net_http_send_str(&conn->sock_state, &error, "Host: ",
         STRLEN_CONST("Host: "));
   net_http_send_str(&conn->sock_state, &error, conn->domain,
         strlen(conn->domain));

   if (conn->port)
   {
      char portstr[16];
      size_t _len     = 0;
      portstr[  _len] = ':';
      portstr[++_len] = '\0';
      _len           += snprintf(portstr + _len, sizeof(portstr) - _len,
            "%i", conn->port);
      net_http_send_str(&conn->sock_state, &error, portstr, _len);
   }

   net_http_send_str(&conn->sock_state, &error, "\r\n",
         STRLEN_CONST("\r\n"));

   /* Pre-formatted headers */
   if (conn->headerscopy)
      net_http_send_str(&conn->sock_state, &error, conn->headerscopy,
            strlen(conn->headerscopy));
   if (conn->contenttypecopy)
   {
      net_http_send_str(&conn->sock_state, &error, "Content-Type: ",
            STRLEN_CONST("Content-Type: "));
      net_http_send_str(&conn->sock_state, &error,
            conn->contenttypecopy, strlen(conn->contenttypecopy));
      net_http_send_str(&conn->sock_state, &error, "\r\n",
            STRLEN_CONST("\r\n"));
   }

   if (conn->methodcopy && (string_is_equal(conn->methodcopy, "POST") || string_is_equal(conn->methodcopy, "PUT")))
   {
      size_t post_len, len;
      char *len_str        = NULL;

      if (!conn->postdatacopy && !string_is_equal(conn->methodcopy, "PUT"))
         goto err;

      if (!conn->headerscopy)
      {
         if (!conn->contenttypecopy)
            net_http_send_str(&conn->sock_state, &error,
                  "Content-Type: application/x-www-form-urlencoded\r\n",
                  STRLEN_CONST(
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     ));
      }

      net_http_send_str(&conn->sock_state, &error, "Content-Length: ",
            STRLEN_CONST("Content-Length: "));

      post_len = conn->contentlength;
#ifdef _WIN32
      len     = snprintf(NULL, 0, "%" PRIuPTR, post_len);
      len_str = (char*)malloc(len + 1);
      snprintf(len_str, len + 1, "%" PRIuPTR, post_len);
#else
      len     = snprintf(NULL, 0, "%llu", (long long unsigned)post_len);
      len_str = (char*)malloc(len + 1);
      snprintf(len_str, len + 1, "%llu", (long long unsigned)post_len);
#endif

      len_str[len] = '\0';

      net_http_send_str(&conn->sock_state, &error, len_str,
            strlen(len_str));
      net_http_send_str(&conn->sock_state, &error, "\r\n",
            STRLEN_CONST("\r\n"));

      free(len_str);
   }

   net_http_send_str(&conn->sock_state, &error, "User-Agent: ",
         STRLEN_CONST("User-Agent: "));
   if (conn->useragentcopy)
      net_http_send_str(&conn->sock_state, &error,
            conn->useragentcopy, strlen(conn->useragentcopy));
   else
      net_http_send_str(&conn->sock_state, &error, "libretro",
            STRLEN_CONST("libretro"));
   net_http_send_str(&conn->sock_state, &error, "\r\n",
         STRLEN_CONST("\r\n"));

   net_http_send_str(&conn->sock_state, &error,
         "Connection: close\r\n", STRLEN_CONST("Connection: close\r\n"));
   net_http_send_str(&conn->sock_state, &error, "\r\n",
         STRLEN_CONST("\r\n"));

   if (conn->postdatacopy && conn->contentlength)
      net_http_send_str(&conn->sock_state, &error, conn->postdatacopy,
            conn->contentlength);

   if (!error)
   {
      struct http_t *state = (struct http_t*)malloc(sizeof(struct http_t));
      state->sock_state    = conn->sock_state;
      state->status        = -1;
      state->data          = NULL;
      state->part          = P_HEADER_TOP;
      state->bodytype      = T_FULL;
      state->error         = false;
      state->pos           = 0;
      state->len           = 0;
      state->buflen        = 512;

      if ((state->data = (char*)malloc(state->buflen)))
      {
         if ((state->headers = string_list_new()) &&
             string_list_initialize(state->headers))
            return state;
         string_list_free(state->headers);
      }
      free(state);
   }

err:
   if (conn)
   {
      if (conn->methodcopy)
         free(conn->methodcopy);
      if (conn->contenttypecopy)
         free(conn->contenttypecopy);
      if (conn->postdatacopy)
         free(conn->postdatacopy);
      conn->methodcopy           = NULL;
      conn->contenttypecopy      = NULL;
      conn->postdatacopy         = NULL;

#ifdef HAVE_SSL
      if (conn->sock_state.ssl_ctx)
      {
         ssl_socket_close(conn->sock_state.ssl_ctx);
         ssl_socket_free(conn->sock_state.ssl_ctx);
         conn->sock_state.ssl_ctx = NULL;
      }
#endif
   }

#ifndef HAVE_SSL
   socket_close(fd);
#endif
   return NULL;
}

/**
 * net_http_fd:
 *
 * Leaf function.
 *
 * You can use this to call net_http_update
 * only when something will happen; select() it for reading.
 **/
int net_http_fd(struct http_t *state)
{
   if (!state)
      return -1;
   return state->sock_state.fd;
}

/**
 * net_http_update:
 *
 * @return true if it's done, or if something broke.
 * @total will be 0 if it's not known.
 **/
bool net_http_update(struct http_t *state, size_t* progress, size_t* total)
{
   if (state)
   {
      ssize_t newlen   = 0;
      if (state->error)
         goto error;

      if (state->part < P_BODY)
      {
         if (state->error)
            goto error;

#ifdef HAVE_SSL
         if (state->sock_state.ssl && state->sock_state.ssl_ctx)
            newlen = ssl_socket_receive_all_nonblocking(state->sock_state.ssl_ctx, &state->error,
                  (uint8_t*)state->data + state->pos,
                  state->buflen - state->pos);
         else
#endif
            newlen = socket_receive_all_nonblocking(state->sock_state.fd, &state->error,
                  (uint8_t*)state->data + state->pos,
                  state->buflen - state->pos);

         if (newlen < 0)
         {
            state->error  = true;
            goto error;
         }

         if (state->pos + newlen >= state->buflen - 64)
         {
            state->buflen *= 2;
            state->data    = (char*)realloc(state->data, state->buflen);
         }
         state->pos       += newlen;

         while (state->part < P_BODY)
         {
            char *dataend  = state->data + state->pos;
            char *lineend  = (char*)memchr(state->data, '\n', state->pos);

            if (!lineend)
               break;

            *lineend       = '\0';

            if (lineend != state->data && lineend[-1]=='\r')
               lineend[-1] = '\0';

            if (state->part == P_HEADER_TOP)
            {
               if (strncmp(state->data, "HTTP/1.", STRLEN_CONST("HTTP/1."))!=0)
               {
                  state->error  = true;
                  goto error;
               }
               state->status    = (int)strtoul(state->data 
                     + STRLEN_CONST("HTTP/1.1 "), NULL, 10);
               state->part      = P_HEADER;
            }
            else
            {
               if (string_starts_with_case_insensitive(state->data, "Content-Length:"))
               {
                  char* ptr = state->data + STRLEN_CONST("Content-Length:");
                  while (ISSPACE(*ptr))
                     ++ptr;

                  state->bodytype = T_LEN;
                  state->len = strtol(ptr, NULL, 10);
               }
               if (string_is_equal_case_insensitive(state->data, "Transfer-Encoding: chunked"))
                  state->bodytype = T_CHUNK;

               if (state->data[0]=='\0')
               {
                  if (state->status == 100)
                  {
                     state->part = P_HEADER_TOP;
                  }
                  else
                  {
                     state->part = P_BODY;
                     if (state->bodytype == T_CHUNK)
                        state->part = P_BODY_CHUNKLEN;
                  }
               }
               else
               {
                  union string_list_elem_attr attr;
                  attr.i = 0;
                  string_list_append(state->headers, state->data, attr);
               }
            }

            memmove(state->data, lineend + 1, dataend-(lineend+1));
            state->pos = (dataend-(lineend + 1));
         }

         if (state->part >= P_BODY)
         {
            newlen     = state->pos;
            state->pos = 0;
         }
      }

      if (state->part >= P_BODY && state->part < P_DONE)
      {
         if (!newlen)
         {
            if (state->error)
               newlen = -1;
            else if (state->len)
            {
#ifdef HAVE_SSL
               if (state->sock_state.ssl && state->sock_state.ssl_ctx)
                  newlen = ssl_socket_receive_all_nonblocking(
                        state->sock_state.ssl_ctx,
                        &state->error,
                        (uint8_t*)state->data + state->pos,
                        state->buflen - state->pos);
               else
#endif
                  newlen = socket_receive_all_nonblocking(
                        state->sock_state.fd,
                        &state->error,
                        (uint8_t*)state->data + state->pos,
                        state->buflen - state->pos);
            }

            if (newlen < 0)
            {
               if (state->bodytype != T_FULL)
               {
                  state->error  = true;
                  goto error;
               }
               state->part      = P_DONE;
               state->data      = (char*)realloc(state->data, state->len);
               newlen           = 0;
            }

            if (state->pos + newlen >= state->buflen - 64)
            {
               state->buflen   *= 2;
               state->data      = (char*)realloc(state->data, state->buflen);
            }
         }

parse_again:
         if (state->bodytype == T_CHUNK)
         {
            if (state->part == P_BODY_CHUNKLEN)
            {
               state->pos      += newlen;

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
               state->pos += newlen;
               state->len -= newlen;
            }
         }
         else
         {
            state->pos += newlen;

            if (state->pos > state->len)
            {
               state->error  = true;
               goto error;
            }
            else if (state->pos == state->len)
            {
               state->part   = P_DONE;
               state->data   = (char*)realloc(state->data, state->len);
            }
         }
      }

      if (progress)
         *progress = state->pos;

      if (total)
      {
         if (state->bodytype == T_LEN)
            *total = state->len;
         else
            *total = 0;
      }

      if (state->part != P_DONE)
         return false;
   }
   return true;
error:
   state->part   = P_ERROR;
   state->status = -1;
   return true;
}

/**
 * net_http_status:
 *
 * Report HTTP status. 200, 404, or whatever.
 *
 * Leaf function.
 * 
 * @return HTTP status code.
 **/
int net_http_status(struct http_t *state)
{
   if (!state)
      return -1;
   return state->status;
}

/**
 * net_http_headers:
 *
 * Leaf function.
 *
 * @return the response headers. The returned buffer is owned by the
 * caller of net_http_new; it is not freed by net_http_delete().
 * If the status is not 20x and accept_error is false, it returns NULL.
 **/
struct string_list *net_http_headers(struct http_t *state)
{
   if (!state)
      return NULL;

   if (state->error)
      return NULL;

   return state->headers;
}

/**
 * net_http_data:
 *
 * Leaf function.
 *
 * @return the downloaded data. The returned buffer is owned by the
 * HTTP handler; it's freed by net_http_delete().
 * If the status is not 20x and accept_error is false, it returns NULL.
 **/
uint8_t* net_http_data(struct http_t *state, size_t* len, bool accept_error)
{
   if (!state)
      return NULL;

   if (!accept_error && (state->error || state->status < 200 || state->status > 299))
   {
      if (len)
         *len = 0;
      return NULL;
   }

   if (len)
      *len    = state->len;

   return (uint8_t*)state->data;
}

/**
 * net_http_delete:
 *
 * Cleans up all memory.
 **/
void net_http_delete(struct http_t *state)
{
   if (!state)
      return;

   if (state->sock_state.fd >= 0)
   {
#ifdef HAVE_SSL
      if (state->sock_state.ssl && state->sock_state.ssl_ctx)
      {
         ssl_socket_close(state->sock_state.ssl_ctx);
         ssl_socket_free(state->sock_state.ssl_ctx);
         state->sock_state.ssl_ctx = NULL;
      }
      else
#endif
      socket_close(state->sock_state.fd);
   }
   free(state);
}

/**
 * net_http_error:
 *
 * Leaf function
 **/
bool net_http_error(struct http_t *state)
{
   return (state->error || state->status < 200 || state->status > 299);
}

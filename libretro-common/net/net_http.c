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
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <string.h>
#include <lists/string_list.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

enum response_part
{
   P_HEADER_TOP = 0,
   P_HEADER,
   P_BODY,
   P_BODY_CHUNKLEN,
   P_DONE
};

enum bodytype
{
   T_FULL = 0,
   T_LEN,
   T_CHUNK
};

struct conn_pool_entry
{
   char *domain;
   int port;
   int fd;
   void *ssl_ctx;
   bool ssl;
   bool connected;
   bool in_use;
   struct conn_pool_entry *next;
};

static struct conn_pool_entry *conn_pool = NULL;
#ifdef HAVE_THREADS
static slock_t *conn_pool_lock = NULL;
#endif

struct http_t
{
   bool error;

   struct conn_pool_entry *conn;
   bool ssl;
   bool request_sent;

   struct request
   {
      char *domain;
      char *path;
      char *method;
      char *contenttype;
      void *postdata;
      char *useragent;
      char *headers;
      size_t contentlength;
      int port;
   } request;

   struct response
   {
      char *data;
      struct string_list *headers;
      size_t pos;
      size_t len;
      size_t buflen;
      int status;
      enum response_part part;
      enum bodytype bodytype;
   } response;
};

struct http_connection_t
{
   char *domain;
   char *path;
   char *url;
   char *scan;
   char *method;
   char *contenttype;
   void *postdata;
   char *useragent;
   char *headers;
   size_t contentlength; /* ptr alignment */
   int port;
   bool ssl;
};

struct dns_cache_entry
{
   char *domain;
   int port;
   struct addrinfo *addr;
   retro_time_t timestamp;
   bool valid;
   struct dns_cache_entry *next;
};

static struct dns_cache_entry *dns_cache = NULL;
/* 5 min timeout, in usec */
static const retro_time_t dns_cache_timeout = 1000 /* usec/ms */ * 1000 /* ms/s */ * 60 /* s/min */ * 5 /* min */;
/* only cache failures for 30 seconds */
static const retro_time_t dns_cache_fail_timeout = 1000 /* usec/ms */ * 1000 /* ms/s */ * 30 /* s */;
#ifdef HAVE_THREADS
static slock_t *dns_cache_lock = NULL;
#endif

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
   size_t _len                      = strlen(source) * 3 + 1;
   size_t count                     = _len;
   char *enc                        = (char*)calloc(1, _len);
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

   (*dest)[_len - 1] = '\0';
}

/**
 * net_http_urlencode_full:
 *
 * Re-encode a full URL
 **/
void net_http_urlencode_full(char *s, const char *source, size_t len)
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
   buf_pos         = strlcpy(s, url_domain, len);
   s[  buf_pos] = '/';
   s[++buf_pos] = '\0';
   strlcpy(s + buf_pos, tmp, len - buf_pos);
   free(tmp);
}

struct http_connection_t *net_http_connection_new(const char *url,
      const char *method, const char *data)
{
   struct http_connection_t *conn = NULL;

   if (!url)
      return NULL;
   if (!(conn = (struct http_connection_t*)calloc(1, sizeof(*conn))))
      return NULL;

   if (method)
      conn->method         = strdup(method);

   if (data)
   {
      conn->postdata       = strdup(data);
      conn->contentlength  = strlen(data);
   }

   if (!(conn->url = strdup(url)))
      goto error;

   if (!strncmp(url, "http://", STRLEN_CONST("http://")))
      conn->scan = conn->url + STRLEN_CONST("http://");
   else if (!strncmp(url, "https://", STRLEN_CONST("https://")))
   {
      conn->scan = conn->url + STRLEN_CONST("https://");
      conn->ssl  = true;
   }
   else
      goto error;

   if (string_is_empty(conn->scan))
      goto error;

   conn->domain = conn->scan;

   return conn;

error:
   if (conn->url)
      free(conn->url);
   if (conn->method)
      free(conn->method);
   if (conn->postdata)
      free(conn->postdata);
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
      if (conn->ssl)
         conn->port = 443;
      else
         conn->port = 80;
   }

   if (*conn->scan == '/')
   {
      /* domain followed by path - split off the path */
      /*   site.com/path.html   or   site.com:80/path.html   */
      *conn->scan    = '\0';
      conn->path = conn->scan + 1;
      return true;
   }
   else if (!*conn->scan)
   {
      /* domain with no path - point path at empty string */
      /*   site.com   or   site.com:80   */
      conn->path = conn->scan;
      return true;
   }
   else if (*conn->scan == '?')
   {
      /* domain with no path, but still has query parms - point path at the query parms */
      /*   site.com?param=3   or  site.com:80?param=3   */
      if (!has_port)
      {
         /* if there wasn't a port, we have to expand the urlcopy so we can separate the two parts */
         size_t domain_len   = strlen(conn->domain);
         size_t path_len     = strlen(conn->scan);
         char* urlcopy       = (char*)malloc(domain_len + path_len + 2);
         memcpy(urlcopy, conn->domain, domain_len);
         urlcopy[domain_len] = '\0';
         memcpy(urlcopy + domain_len + 1, conn->scan, path_len + 1);

         free(conn->url);
         conn->domain        = conn->url     = urlcopy;
         conn->path          = conn->scan    = urlcopy + domain_len + 1;
      }
      else /* There was a port, so overwriting the : will terminate the domain and we can just point at the ? */
         conn->path          = conn->scan;

      return true;
   }

   /* invalid character after domain/port */
   return false;
}

void net_http_connection_free(struct http_connection_t *conn)
{
   if (!conn)
      return;

   if (conn->url)
      free(conn->url);

   if (conn->method)
      free(conn->method);

   if (conn->contenttype)
      free(conn->contenttype);

   if (conn->postdata)
      free(conn->postdata);

   if (conn->useragent)
      free(conn->useragent);

   if (conn->headers)
      free(conn->headers);

   free(conn);
}

void net_http_connection_set_user_agent(
      struct http_connection_t *conn, const char *user_agent)
{
   if (conn->useragent)
      free(conn->useragent);

   conn->useragent = user_agent ? strdup(user_agent) : NULL;
}

void net_http_connection_set_headers(
      struct http_connection_t *conn, const char *headers)
{
   if (conn->headers)
      free(conn->headers);

   conn->headers = headers ? strdup(headers) : NULL;
}

void net_http_connection_set_content(
      struct http_connection_t *conn, const char *content_type,
      size_t content_length, const void *content)

{
   if (conn->contenttype)
      free(conn->contenttype);
   if (conn->postdata)
      free(conn->postdata);

   conn->contenttype = content_type ? strdup(content_type) : NULL;
   conn->contentlength = content_length;
   if (content_length)
   {
      conn->postdata = malloc(content_length);
      memcpy(conn->postdata, content, content_length);
   }
}

const char *net_http_connection_url(struct http_connection_t *conn)
{
   return conn->url;
}

const char* net_http_connection_method(struct http_connection_t* conn)
{
   return conn->method;
}

static void net_http_dns_cache_remove_expired(void)
{
   struct dns_cache_entry *entry = dns_cache;
   struct dns_cache_entry *prev = NULL;
   while (entry)
   {
      if (     (entry->addr && (entry->timestamp + dns_cache_timeout < cpu_features_get_time_usec()))
            || (!entry->addr && (entry->timestamp + dns_cache_fail_timeout < cpu_features_get_time_usec())))
      {
         if (prev)
            prev->next = entry->next;
         else
            dns_cache = entry->next;
         if (entry->addr)
            freeaddrinfo_retro(entry->addr);
         free(entry->domain);
         free(entry);
         entry = prev ? prev->next : dns_cache;
      }
      else
      {
         prev = entry;
         entry = entry->next;
      }
   }
}

static struct dns_cache_entry *net_http_dns_cache_find(const char *domain, int port)
{
   struct dns_cache_entry *entry;

   net_http_dns_cache_remove_expired();

   entry = dns_cache;
   while (entry)
   {
      if (port == entry->port && string_is_equal(entry->domain, domain))
      {
         /* don't bump timeestamp for failures */
         if (entry->addr)
            entry->timestamp = cpu_features_get_time_usec();
         return entry;
      }
      entry = entry->next;
   }
   return NULL;
}

static struct dns_cache_entry *net_http_dns_cache_add(const char *domain, int port, struct addrinfo *addr)
{
   struct dns_cache_entry *entry = (struct dns_cache_entry*)calloc(1, sizeof(*entry));
   if (!entry)
      return NULL;
   entry->domain = strdup(domain);
   entry->port = port;
   entry->addr = addr;
   entry->timestamp = cpu_features_get_time_usec();
   entry->valid = (addr != NULL);
   entry->next = dns_cache;
   dns_cache = entry;
   return entry;
}

static void net_http_conn_pool_free(struct conn_pool_entry *entry)
{
#ifdef HAVE_SSL
   if (entry->ssl && entry->ssl_ctx)
   {
      ssl_socket_close(entry->ssl_ctx);
      ssl_socket_free(entry->ssl_ctx);
   }
#endif
   if (entry->fd >= 0)
      socket_close(entry->fd);
   free(entry->domain);
   free(entry);
}

static void net_http_conn_pool_remove(struct conn_pool_entry *entry)
{
   struct conn_pool_entry *prev = NULL;
   struct conn_pool_entry *current;
   if (!entry)
      return;
#ifdef HAVE_THREADS
   slock_lock(conn_pool_lock);
#endif
   current = conn_pool;
   while (current)
   {
      if (current == entry)
      {
         if (prev)
            prev->next = current->next;
         else
            conn_pool = current->next;
         net_http_conn_pool_free(current);
#ifdef HAVE_THREADS
         slock_unlock(conn_pool_lock);
#endif
         return;
      }
      prev = current;
      current = current->next;
   }
#ifdef HAVE_THREADS
   slock_unlock(conn_pool_lock);
#endif
}

/* *NOT* thread safe, caller must lock */
static void net_http_conn_pool_remove_expired(void)
{
   struct conn_pool_entry *entry = conn_pool;
   struct conn_pool_entry *prev = NULL;
   struct timeval tv = { 0 };
   int max = 0;
   fd_set fds;
   FD_ZERO(&fds);
   entry = conn_pool;
   while (entry)
   {
      if (!entry->in_use)
      {
         FD_SET(entry->fd, &fds);
         if (entry->fd >= max)
            max = entry->fd + 1;
      }
      entry = entry->next;
   }
   if (select(max, &fds, NULL, NULL, &tv) <= 0)
      return;
   entry = conn_pool;
   while (entry)
   {
      if (!entry->in_use && FD_ISSET(entry->fd, &fds))
      {
         char buf[4096];
         bool error = false;
#ifdef HAVE_SSL
         if (entry->ssl && entry->ssl_ctx)
            ssl_socket_receive_all_nonblocking(entry->ssl_ctx, &error, buf, sizeof(buf));
         else
#endif
            socket_receive_all_nonblocking(entry->fd, &error, buf, sizeof(buf));

         if (!error)
            continue;

         if (prev)
            prev->next = entry->next;
         else
            conn_pool = entry->next;
         /* if it's not in use and it's reaadable we assume that means it's closed without checking recv */
         net_http_conn_pool_free(entry);
         entry = prev ? prev->next : conn_pool;
      }
      else
      {
         prev = entry;
         entry = entry->next;
      }
   }
}

/* if it's not already in the pool, will add to end.
   *NOT* thread safe, caller must lock */
static void net_http_conn_pool_move_to_end(struct conn_pool_entry *entry)
{
   struct conn_pool_entry *prev = NULL;
   struct conn_pool_entry *current = conn_pool;
   /* 0 items in pool */
   if (!conn_pool)
   {
      conn_pool = entry;
      entry->next = NULL;
      return;
   }
   /* already only item in pool */
   if (conn_pool == entry && !conn_pool->next)
      return;
   while (current)
   {
      if (current != entry)
         prev = current;
      else
      {
         /* need to remove current */
         if (prev)
            prev->next = current->next;
         else
            conn_pool = current->next;
      }
      current = current->next;
   }
   prev->next = entry;
   entry->next = NULL;
}

static struct conn_pool_entry *net_http_conn_pool_find(const char *domain, int port)
{
   struct conn_pool_entry *entry;

#ifdef HAVE_THREADS
   slock_lock(conn_pool_lock);
#endif

   net_http_conn_pool_remove_expired();

   entry = conn_pool;
   while (entry)
   {
      if (!entry->in_use && port == entry->port && string_is_equal(entry->domain, domain))
      {
         entry->in_use = true;
         net_http_conn_pool_move_to_end(entry);
#ifdef HAVE_THREADS
         slock_unlock(conn_pool_lock);
#endif
         return entry;
      }
      entry = entry->next;
   }
#ifdef HAVE_THREADS
   slock_unlock(conn_pool_lock);
#endif
   return NULL;
}

static struct conn_pool_entry *net_http_conn_pool_add(const char *domain, int port, int fd, bool ssl)
{
   struct conn_pool_entry *entry = (struct conn_pool_entry*)calloc(1, sizeof(*entry));
   if (!entry)
      return NULL;
   entry->domain = strdup(domain);
   entry->port = port;
   entry->fd = fd;
   entry->in_use = true;
   entry->ssl = ssl;
   entry->connected = false;
#ifdef HAVE_THREADS
   slock_lock(conn_pool_lock);
#endif
   net_http_conn_pool_move_to_end(entry);
#ifdef HAVE_THREADS
   slock_unlock(conn_pool_lock);
#endif
   return entry;
}

struct http_t *net_http_new(struct http_connection_t *conn)
{
   struct http_t *state;

   if (!conn)
      return NULL;

   state = (struct http_t*)calloc(1, sizeof(struct http_t));
   if (!state)
      return NULL;

   state->ssl  = conn->ssl;
   state->conn = NULL;

   state->request.domain        = strdup(conn->domain);
   state->request.path          = strdup(conn->path);
   state->request.method        = strdup(conn->method);
   state->request.contenttype   = conn->contenttype ? strdup(conn->contenttype) : NULL;
   state->request.contentlength = conn->contentlength;
   if (conn->postdata && conn->contentlength)
   {
      state->request.postdata   = malloc(conn->contentlength);
      memcpy(state->request.postdata, conn->postdata, conn->contentlength);
   }
   state->request.useragent     = conn->useragent ? strdup(conn->useragent) : NULL;
   state->request.headers       = conn->headers ? strdup(conn->headers) : NULL;
   state->request.port          = conn->port;

   state->response.status  = -1;
   state->response.buflen  = 16 * 1024;
   state->response.data    = (char*)malloc(state->response.buflen);
   state->response.headers = string_list_new();

   return state;
}

static void net_http_resolve(void *data)
{
   struct dns_cache_entry *entry = (struct dns_cache_entry*)data;
   struct addrinfo hints         = {0};
   struct addrinfo *addr         = NULL;
   char *domain;
   int port;
   char port_buf[6];
#if defined(HAVE_SOCKET_LEGACY) || defined(WIIU)
   int family                    = AF_INET;
#else
   int family                    = AF_UNSPEC;
#endif

   hints.ai_family = family;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags |= AI_NUMERICSERV;

#ifdef HAVE_THREADS
   slock_lock(dns_cache_lock);
#endif
   domain = strdup(entry->domain);
   port = entry->port;
#ifdef HAVE_THREADS
   slock_unlock(dns_cache_lock);
#endif

   if (!network_init())
   {
#ifdef HAVE_THREADS
      slock_lock(dns_cache_lock);
#endif
      entry->valid = true;
      entry->addr = NULL;
#ifdef HAVE_THREADS
      slock_unlock(dns_cache_lock);
#endif
      free(domain);
      return;
   }

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);

   getaddrinfo_retro(domain, port_buf, &hints, &addr);
   free(domain);

#ifdef HAVE_THREADS
   slock_lock(dns_cache_lock);
#endif
   entry->valid = true;
   entry->addr = addr;
#ifdef HAVE_THREADS
   slock_unlock(dns_cache_lock);
#endif
}

static bool net_http_new_socket(struct http_t *state)
{
   struct addrinfo *addr = NULL;
   struct dns_cache_entry *entry;

#ifdef HAVE_THREADS
   sthread_t *thread;

   if (!dns_cache_lock)
      dns_cache_lock = slock_new();
   slock_lock(dns_cache_lock);
#endif

   entry = net_http_dns_cache_find(state->request.domain, state->request.port);
   if (entry)
   {
      if (entry->valid)
      {
         int fd;
         if (!entry->addr)
         {
#ifdef HAVE_THREADS
            slock_unlock(dns_cache_lock);
#endif
            return false;
         }
         addr = entry->addr;
         fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
         if (fd >= 0)
            state->conn = net_http_conn_pool_add(state->request.domain, state->request.port, fd, state->ssl);
#ifdef HAVE_THREADS
         /* still waiting on thread */
         slock_unlock(dns_cache_lock);
#endif
         return (fd >= 0);
      }
      else
      {
#ifdef HAVE_THREADS
         /* still waiting on thread */
         slock_unlock(dns_cache_lock);
#endif
         return true;
      }
   }
   else
   {
      entry = net_http_dns_cache_add(state->request.domain, state->request.port, NULL);
#ifdef HAVE_THREADS
      /* create the entry for it as an indicator that the request is underway */
      thread = sthread_create(net_http_resolve, entry);
      sthread_detach(thread);
#else
      net_http_resolve(state);
#endif
   }

#ifdef HAVE_THREADS
   slock_unlock(dns_cache_lock);
#endif

   return true;
}

static bool net_http_connect(struct http_t *state)
{
   struct addrinfo *addr = NULL, *next_addr = NULL;
   struct conn_pool_entry *conn = state->conn;
   struct dns_cache_entry *dns_entry = net_http_dns_cache_find(state->request.domain, state->request.port);
   /* we just used/added this in _new_socket above, if it's not there it's a big bug */
   addr = dns_entry->addr;

#ifndef HAVE_SSL
   if (state->ssl)
      return false;
#else
   if (state->ssl)
   {
      if (!conn || conn->fd < 0)
         return false;

      if (!(conn->ssl_ctx = ssl_socket_init(conn->fd, state->request.domain)))
      {
         net_http_conn_pool_remove(conn);
         state->conn = NULL;
         state->error = true;
         return false;
      }

      /* TODO: Properly figure out what's going wrong when the newer
         timeout/poll code interacts with mbed and winsock
         https://github.com/libretro/RetroArch/issues/14742 */

      /* Temp fix, don't use new timeout/poll code for cheevos http requests */
         bool timeout = true;
#ifdef __WIN32
      if (!strcmp(state->request.domain, "retroachievements.org"))
         timeout = false;
#endif

      if (ssl_socket_connect(conn->ssl_ctx, addr, timeout, true) < 0)
      {
         net_http_conn_pool_remove(conn);
         state->conn = NULL;
         state->error = true;
         return false;
      }
      else
      {
         conn->connected = true;
         return true;
      }
   }
   else
#endif
   {
      for (next_addr = addr; conn->fd >= 0; conn->fd = socket_next((void**)&next_addr))
      {
         if (socket_connect_with_timeout(conn->fd, next_addr, 5000))
         {
            conn->connected = true;
            return true;
         }

         socket_close(conn->fd);
      }
      conn->fd = -1; /* already closed */
      net_http_conn_pool_remove(conn);
      state->conn = NULL;
      state->error = true;
      return false;
   }
}

static void net_http_send_str(
      struct http_t *state, const char *text, size_t text_size)
{
   if (state->error)
      return;
#ifdef HAVE_SSL
   if (state->ssl)
   {
      if (!ssl_socket_send_all_blocking(
                  state->conn->ssl_ctx, text, text_size, true))
         state->error = true;
   }
   else
#endif
   {
      if (!socket_send_all_blocking(
                  state->conn->fd, text, text_size, true))
         state->error = true;
   }
}

static bool net_http_send_request(struct http_t *state)
{
   struct request *request = &state->request;

   /* This is a bit lazy, but it works. */
   if (request->method)
   {
      net_http_send_str(state, request->method, strlen(request->method));
      net_http_send_str(state, " /", STRLEN_CONST(" /"));
   }
   else
      net_http_send_str(state, "GET /", STRLEN_CONST("GET /"));

   net_http_send_str(state, request->path, strlen(request->path));
   net_http_send_str(state, " HTTP/1.1\r\n", STRLEN_CONST(" HTTP/1.1\r\n"));

   net_http_send_str(state, "Host: ", STRLEN_CONST("Host: "));
   net_http_send_str(state, request->domain, strlen(request->domain));

   if (request->port)
   {
      char portstr[16];
      size_t _len     = 0;
      portstr[  _len] = ':';
      portstr[++_len] = '\0';
      _len           += snprintf(portstr + _len, sizeof(portstr) - _len,
            "%i", request->port);
      net_http_send_str(state, portstr, _len);
   }

   net_http_send_str(state, "\r\n", STRLEN_CONST("\r\n"));

   /* Pre-formatted headers */
   if (request->headers)
      net_http_send_str(state, request->headers, strlen(request->headers));
   if (request->contenttype)
   {
      net_http_send_str(state, "Content-Type: ", STRLEN_CONST("Content-Type: "));
      net_http_send_str(state, request->contenttype, strlen(request->contenttype));
      net_http_send_str(state, "\r\n", STRLEN_CONST("\r\n"));
   }

   if (request->method && (string_is_equal(request->method, "POST") || string_is_equal(request->method, "PUT")))
   {
      size_t post_len, len;
      char *len_str        = NULL;

      if (!request->postdata && !string_is_equal(request->method, "PUT"))
      {
         state->error = true;
         return true;
      }

      if (!request->headers && !request->contenttype)
         net_http_send_str(state,
               "Content-Type: application/x-www-form-urlencoded\r\n",
               STRLEN_CONST("Content-Type: application/x-www-form-urlencoded\r\n"));

      net_http_send_str(state, "Content-Length: ", STRLEN_CONST("Content-Length: "));

      post_len = request->contentlength;
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

      net_http_send_str(state, len_str, strlen(len_str));
      net_http_send_str(state, "\r\n", STRLEN_CONST("\r\n"));

      free(len_str);
   }

   net_http_send_str(state, "User-Agent: ", STRLEN_CONST("User-Agent: "));
   if (request->useragent)
      net_http_send_str(state, request->useragent, strlen(request->useragent));
   else
      net_http_send_str(state, "libretro", STRLEN_CONST("libretro"));
   net_http_send_str(state, "\r\n", STRLEN_CONST("\r\n"));

   net_http_send_str(state, "\r\n", STRLEN_CONST("\r\n"));

   if (request->postdata && request->contentlength)
      net_http_send_str(state, request->postdata, request->contentlength);

   state->request_sent = true;
   return state->error;
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
   if (!state || !state->conn)
      return -1;
   return state->conn->fd;
}

static ssize_t net_http_receive_header(struct http_t *state, ssize_t newlen)
{
   struct response *response = &state->response;

   response->pos       += newlen;

   while (response->part < P_BODY)
   {
      char *dataend  = response->data + response->pos;
      char *lineend  = (char*)memchr(response->data, '\n', response->pos);

      if (!lineend)
         break;

      *lineend       = '\0';

      if (lineend != response->data && lineend[-1]=='\r')
         lineend[-1] = '\0';

      if (response->part == P_HEADER_TOP)
      {
         if (strncmp(response->data, "HTTP/1.", STRLEN_CONST("HTTP/1."))!=0)
         {
            response->part = P_DONE;
            state->error = true;
            return -1;
         }
         response->status = (int)strtoul(response->data
               + STRLEN_CONST("HTTP/1.1 "), NULL, 10);
         response->part   = P_HEADER;
      }
      else
      {
         if (string_starts_with_case_insensitive(response->data, "Content-Length:"))
         {
            char* ptr = response->data + STRLEN_CONST("Content-Length:");
            while (ISSPACE(*ptr))
               ++ptr;

            response->bodytype = T_LEN;
            response->len      = strtol(ptr, NULL, 10);
         }
         else if (string_is_equal_case_insensitive(response->data, "Transfer-Encoding: chunked"))
            response->bodytype = T_CHUNK;

         if (response->data[0]=='\0')
         {
            if (response->status == 100)
            {
               response->part = P_HEADER_TOP;
            }
            else
            {
               response->part = P_BODY;
               if (response->bodytype == T_CHUNK)
                  response->part = P_BODY_CHUNKLEN;
            }
         }
         else
         {
            union string_list_elem_attr attr;
            attr.i = 0;
            string_list_append(response->headers, response->data, attr);
         }
      }

      memmove(response->data, lineend + 1, dataend-(lineend+1));
      response->pos = (dataend-(lineend + 1));
   }

   if (response->part >= P_BODY)
   {
      newlen        = response->pos;
      response->pos = 0;
      if (response->bodytype == T_LEN)
      {
         response->buflen = response->len;
         response->data   = (char*)realloc(response->data, response->buflen);
      }
   }
   else
   {
      if (response->pos >= response->buflen - 64)
      {
         response->buflen *= 2;
         response->data    = (char*)realloc(response->data, response->buflen);
      }
   }
   return newlen;
}

static bool net_http_receive_body(struct http_t *state, ssize_t newlen)
{
   struct response *response = &state->response;

   if (newlen < 0 || state->error)
   {
      if (response->bodytype != T_FULL)
         return false;
      response->part      = P_DONE;
      if (response->buflen != response->len)
         response->data      = (char*)realloc(response->data, response->len);
      return true;
   }

parse_again:
   if (response->bodytype == T_CHUNK)
   {
      if (response->part == P_BODY_CHUNKLEN)
      {
         response->pos      += newlen;

         if (response->pos - response->len >= 2)
         {
            /*
             * len=start of chunk including \r\n
             * pos=end of data
             */

            char *fullend = response->data + response->pos;
            char *end     = (char*)memchr(response->data + response->len + 2, '\n',
                  response->pos - response->len - 2);

            if (end)
            {
               size_t chunklen = strtoul(response->data + response->len, NULL, 16);
               response->pos   = response->len;
               end++;

               memmove(response->data + response->len, end, fullend-end);

               response->len   = chunklen;
               newlen          = (fullend - end);

               /*
                 len=num bytes
                 newlen=unparsed bytes after \n
                 pos=start of chunk including \r\n
               */

               response->part = P_BODY;
               if (response->len == 0)
               {
                  response->part = P_DONE;
                  response->len  = response->pos;
                  response->data = (char*)realloc(response->data, response->len);
                  return true;
               }
               goto parse_again;
            }
         }
      }
      else if (response->part == P_BODY)
      {
         if ((size_t)newlen >= response->len)
         {
            response->pos += response->len;
            newlen        -= response->len;
            response->len  = response->pos;
            response->part = P_BODY_CHUNKLEN;
            goto parse_again;
         }
         response->pos += newlen;
         response->len -= newlen;
      }
   }
   else
   {
      response->pos += newlen;

      if (response->pos > response->len)
         return false;
      else if (response->pos == response->len)
      {
         response->part = P_DONE;
         if (response->buflen != response->len)
            response->data = (char*)realloc(response->data, response->len);
         return true;
      }
   }

   if (response->pos >= response->buflen)
   {
      response->buflen *= 2;
      response->data    = (char*)realloc(response->data, response->buflen);
   }
   return true;
}

static bool net_http_redirect(struct http_t *state, const char *location)
{
   /* this reinitializes state based on the new location */

   /* url may be absolute or relative to the current url */
   bool absolute = (strstr(location, "://") != NULL);

   if (absolute)
   {
      /* this block is a little wasteful, memory-wise */
      struct http_connection_t *new_url = net_http_connection_new(location, NULL, NULL);
      net_http_connection_iterate(new_url);
      if (!net_http_connection_done(new_url))
      {
         net_http_connection_free(new_url);
         return true;
      }
      state->ssl = new_url->ssl;
      if (state->request.domain)
         free(state->request.domain);
      state->request.domain = strdup(new_url->domain);
      state->request.port = new_url->port;
      if (state->request.path)
         free(state->request.path);
      state->request.path = strdup(new_url->path);
      net_http_connection_free(new_url);
   }
   else
   {
      if (*location == '/')
      {
         if (state->request.path)
            free(state->request.path);
         state->request.path = strdup(location);
      }
      else
      {
         char *path = malloc(PATH_MAX_LENGTH);
         fill_pathname_resolve_relative(path, state->request.path, location, PATH_MAX_LENGTH);
         free(state->request.path);
         state->request.path = path;
      }
   }
   state->request_sent = false;
   state->response.part = P_HEADER_TOP;
   state->response.status = -1;
   state->response.buflen = 16 * 1024;
   state->response.data = realloc(state->response.data, state->response.buflen);
   state->response.pos = 0;
   state->response.len = 0;
   state->response.bodytype = T_FULL;
   /* after this, assume location is invalid */
   string_list_deinitialize(state->response.headers);
   string_list_initialize(state->response.headers);
   /* keep going */
   return false;
}

/**
 * net_http_update:
 *
 * @return true if it's done, or if something broke.
 * @total will be 0 if it's not known.
 **/
bool net_http_update(struct http_t *state, size_t* progress, size_t* total)
{
   struct response *response;
   ssize_t newlen   = 0;

   if (!state)
      return true;

   if (state->error)
      return true;

   if (!state->conn)
   {
      state->conn = net_http_conn_pool_find(state->request.domain, state->request.port);
      if (!state->conn)
      {
         if (!net_http_new_socket(state))
            state->error = true;
         return state->error;
      }
   }

   if (!state->conn->connected)
   {
      if (!net_http_connect(state))
         state->error = true;
      return state->error;
   }

   if (!state->request_sent)
      return net_http_send_request(state);

   response = &state->response;

#ifdef HAVE_SSL
   if (state->ssl && state->conn->ssl_ctx)
      newlen = ssl_socket_receive_all_nonblocking(state->conn->ssl_ctx, &state->error,
            (uint8_t*)response->data + response->pos,
            response->buflen - response->pos);
   else
#endif
      newlen = socket_receive_all_nonblocking(state->conn->fd, &state->error,
            (uint8_t*)response->data + response->pos,
            response->buflen - response->pos);

   if (response->part < P_BODY)
   {
      if (newlen < 0 || state->error)
         goto error;
      newlen = net_http_receive_header(state, newlen);
   }

   if (response->part >= P_BODY && response->part < P_DONE)
   {
      if (!net_http_receive_body(state, newlen))
         goto error;
   }

   if (progress)
      *progress = response->pos;

   if (total)
   {
      if (response->bodytype == T_LEN)
         *total = response->len;
      else
         *total = 0;
   }

   if (response->part != P_DONE)
      return false;

   for (newlen = 0; (size_t)newlen < response->headers->size; newlen++)
   {
      if (string_is_equal_case_insensitive(response->headers->elems[newlen].data, "connection: close"))
      {
         net_http_conn_pool_remove(state->conn);
         state->conn = NULL;
         break;
      }
   }

   if (state->conn)
      state->conn->in_use = false;
   state->conn = NULL;

   if (response->status >= 300 && response->status < 400)
   {
      for (newlen = 0; (size_t)newlen < response->headers->size; newlen++)
      {
         if (string_starts_with_case_insensitive(response->headers->elems[newlen].data, "Location: "))
         {
            return net_http_redirect(state, response->headers->elems[newlen].data + STRLEN_CONST("Location: "));
         }
      }
   }

   return true;

error:
   net_http_conn_pool_remove(state->conn);
   state->error     = true;
   response->part   = P_DONE;
   response->status = -1;
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
   return state->response.status;
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

   return state->response.headers;
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

   if (!accept_error && (state->error || state->response.status < 200 || state->response.status > 299))
   {
      if (len)
         *len = 0;
      return NULL;
   }

   if (len)
      *len    = state->response.len;

   return (uint8_t*)state->response.data;
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

   if (state->conn)
      net_http_conn_pool_remove(state->conn);
   if (state->request.domain)
      free(state->request.domain);
   if (state->request.path)
      free(state->request.path);
   if (state->request.method)
      free(state->request.method);
   if (state->request.contenttype)
      free(state->request.contenttype);
   if (state->request.postdata)
      free(state->request.postdata);
   if (state->request.useragent)
      free(state->request.useragent);
   if (state->request.headers)
      free(state->request.headers);
   free(state);
}

/**
 * net_http_error:
 *
 * Leaf function
 **/
bool net_http_error(struct http_t *state)
{
   return (state->error || state->response.status < 200 || state->response.status > 299);
}

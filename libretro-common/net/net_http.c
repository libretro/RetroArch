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
#include <errno.h>
#include <string.h>

#include <net/net_http.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#ifdef HAVE_SSL
#include <net/net_socket_ssl.h>
#endif
#include <compat/strl.h>
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
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
#define LOCK_POOL() slock_lock(conn_pool_lock)
#define UNLOCK_POOL() slock_unlock(conn_pool_lock)
#else
#define LOCK_POOL()
#define UNLOCK_POOL()
#endif

typedef struct response
{
   char *data;
   struct string_list *headers;
   size_t pos;
   size_t len;
   size_t buflen;
   int status;
   enum response_part part;
   enum bodytype bodytype;
} response_t;

typedef struct request
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
} request_t;

struct http_t
{
   bool err;

   struct conn_pool_entry *conn;
   bool ssl;
   bool request_sent;

   request_t request;
   response_t response;
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

static void net_http_log_transport_state(
      const struct http_t *state, const char *stage, ssize_t io_len)
{
#if defined(DEBUG)
   const char *method = "GET";
   const char *domain = "<null>";
   const char *path   = "<null>";
   int port           = 0;
   int fd             = -1;
   int connected      = 0;

   if (state)
   {
      method = state->request.method ? state->request.method : "GET";
      domain = state->request.domain ? state->request.domain : "<null>";
      path   = state->request.path ? state->request.path : "<null>";
      port   = state->request.port;

      if (state->conn)
      {
         fd        = state->conn->fd;
         connected = state->conn->connected ? 1 : 0;
      }
   }

   fprintf(stderr,
         "[net_http] %s: method=%s host=%s port=%d path=/%s ssl=%d fd=%d connected=%d request_sent=%d err=%d io_len=%ld errno=%d (%s)\n",
         stage ? stage : "unknown",
         method,
         domain,
         port,
         path,
         state ? (state->ssl ? 1 : 0) : 0,
         fd,
         connected,
         state ? (state->request_sent ? 1 : 0) : 0,
         state ? (state->err ? 1 : 0) : 0,
         (long)io_len,
         errno,
         strerror(errno));
   fflush(stderr);
#else
   (void)state;
   (void)stage;
   (void)io_len;
#endif
}

struct dns_cache_entry
{
   char *domain;
   int port;
   struct addrinfo *addr;
   retro_time_t timestamp;
   bool valid;
#ifdef HAVE_THREADS
   sthread_t *thread;
#endif
   struct dns_cache_entry *next;
};

static struct dns_cache_entry *dns_cache = NULL;
/* 5 min timeout, in usec */
static const retro_time_t dns_cache_timeout = 1000 /* usec/ms */ * 1000 /* ms/s */ * 60 /* s/min */ * 5 /* min */;
/* only cache failures for 30 seconds */
static const retro_time_t dns_cache_fail_timeout = 1000 /* usec/ms */ * 1000 /* ms/s */ * 30 /* s */;
#ifdef HAVE_THREADS
static slock_t *dns_cache_lock = NULL;
#define LOCK_DNS_CACHE() slock_lock(dns_cache_lock)
#define UNLOCK_DNS_CACHE() slock_unlock(dns_cache_lock)
#else
#define LOCK_DNS_CACHE()
#define UNLOCK_DNS_CACHE()
#endif

/**
 * net_http_urlencode:
 *
 * URL Encode a string
 * caller is responsible for deleting the destination buffer
 **/
void net_http_urlencode(char **dest, const char *source)
{
   /* Bitmask for unreserved chars: A-Z a-z 0-9 * - . / _ */
   static const uint32_t safe[4] = {
      0x00000000, /*  0-31:  none           */
      0x03FFE400, /* 32-63:  * - . / 0-9    */
      0x87FFFFFE, /* 64-95:  A-Z _          */
      0x07FFFFFE  /* 96-127: a-z            */
   };

   const char *s;
   char *enc;
   size_t len = 0;

   /* First pass: compute exact output length */
   for (s = source; *s; s++)
   {
      unsigned char c = (unsigned char)*s;
      if (c < 128 && (safe[c >> 5] & (1u << (c & 31))))
         len += 1;
      else
         len += 3;
   }

   enc   = (char*)malloc(len + 1);
   *dest = enc;

   /* Second pass: encode */
   for (s = source; *s; s++)
   {
      unsigned char c = (unsigned char)*s;
      if (c < 128 && (safe[c >> 5] & (1u << (c & 31))))
         *enc++ = (char)c;
      else
      {
         static const char hex[] = "0123456789ABCDEF";
         *enc++ = '%';
         *enc++ = hex[c >> 4];
         *enc++ = hex[c & 0x0F];
      }
   }

   *enc = '\0';
}

/**
 * net_http_urlencode_full:
 *
 * Re-encode a full URL
 **/
void net_http_urlencode_full(char *s, const char *source, size_t len)
{
   static const char hex[] = "0123456789ABCDEF";
   const char *path_start;
   const char *p;
   size_t domain_len;
   size_t pos;
   int slashes = 0;

   if (!s || !source || len == 0)
      return;

   /* Find the third '/' to locate the domain/path boundary */
   for (p = source; *p && slashes < 3; p++)
   {
      if (*p == '/')
         slashes++;
   }

   /* If fewer than 3 slashes, no path to encode — just copy as-is */
   if (slashes < 3)
   {
      strlcpy(s, source, len);
      return;
   }

   path_start = p; /* points just past the third '/' */
   domain_len = (size_t)(path_start - source);

   /* Copy domain (including trailing '/') */
   if (domain_len >= len)
   {
      strlcpy(s, source, len);
      return;
   }
   memcpy(s, source, domain_len);
   pos = domain_len;

   /* Encode path directly into output buffer */
   for (p = path_start; *p && pos + 1 < len; p++)
   {
      unsigned char c = (unsigned char)*p;

      if (   (c >= 'A' && c <= 'Z')
          || (c >= 'a' && c <= 'z')
          || (c >= '0' && c <= '9')
          || c == '-' || c == '_'
          || c == '.' || c == '~'
          || c == '/' || c == ':' 
          || c == '?' || c == '#'
          || c == '&' || c == '=')
      {
         s[pos++] = c;
      }
      else if (pos + 3 < len)
      {
         s[pos++] = '%';
         s[pos++] = hex[(c >> 4) & 0x0F];
         s[pos++] = hex[ c       & 0x0F];
      }
      else
         break; /* not enough space for encoded char */
   }

   s[pos] = '\0';
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
   {
      conn->method = strdup(method);
      if (!conn->method)
         goto error;
   }
   if (data)
   {
      conn->postdata = strdup(data);
      if (!conn->postdata)
         goto error;
      conn->contentlength = strlen(data);
   }
   conn->url = strdup(url);
   if (!conn->url)
      goto error;
   if (memcmp(url, "http://", 7) == 0)
      conn->scan = conn->url + 7;
   else if (memcmp(url, "https://", 8) == 0)
   {
      conn->scan = conn->url + 8;
      conn->ssl  = true;
   }
   else
      goto error;
   if (*conn->scan == '\0')
      goto error;
   conn->domain = conn->scan;
   return conn;
error:
   free(conn->url);
   free(conn->method);
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

   conn->contenttype   = content_type ? strdup(content_type) : NULL;
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
#ifdef HAVE_THREADS
         if (entry->thread)
         {
            sthread_join(entry->thread);
            entry->thread = NULL;
         }
#endif
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

static struct dns_cache_entry *net_http_dns_cache_find(
   const char *domain, int port)
{
   struct dns_cache_entry *entry;

   net_http_dns_cache_remove_expired();

   entry = dns_cache;
   while (entry)
   {
      if (port == entry->port && strcmp(entry->domain, domain) == 0)
      {
#ifdef HAVE_THREADS
         if (entry->thread && entry->valid)
         {
            sthread_join(entry->thread);
            entry->thread = NULL;
         }
#endif
         /* don't bump timeestamp for failures */
         if (entry->addr)
            entry->timestamp = cpu_features_get_time_usec();
         return entry;
      }
      entry = entry->next;
   }
   return NULL;
}

static struct dns_cache_entry *net_http_dns_cache_add(
   const char *domain, int port, struct addrinfo *addr)
{
   struct dns_cache_entry *entry = (struct dns_cache_entry*)
      calloc(1, sizeof(*entry));
   if (!entry)
      return NULL;
   entry->domain = strdup(domain);
   entry->port = port;
   entry->addr = addr;
   entry->timestamp = cpu_features_get_time_usec();
   entry->valid = (addr != NULL);
#ifdef HAVE_THREADS
   entry->thread = NULL;
#endif
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

   LOCK_POOL();
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
         UNLOCK_POOL();
         return;
      }
      prev = current;
      current = current->next;
   }
   UNLOCK_POOL();
}

/* *NOT* thread safe, caller must lock */
static void net_http_conn_pool_remove_expired(void)
{
   fd_set fds;
   struct conn_pool_entry *entry = NULL;
   struct conn_pool_entry *prev  = NULL;
   struct timeval tv             = { 0 };
   int max                       = 0;
   FD_ZERO(&fds);
   entry = conn_pool;
   while (entry)
   {
      if (!entry->in_use && entry->fd >= 0 && entry->fd < FD_SETSIZE)
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
         /* If it's not in use and it's readable,
          * we assume that means it's closed without checking recv */
         if (prev)
            prev->next = entry->next;
         else
            conn_pool = entry->next;
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
   struct conn_pool_entry *prev    = NULL;
   struct conn_pool_entry *current = conn_pool;
   /* 0 items in pool */
   if (!conn_pool)
   {
      conn_pool   = entry;
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

   if (prev)
      prev->next  = entry;
   if (entry)
      entry->next = NULL;
}

static struct conn_pool_entry *net_http_conn_pool_find(
   const char *domain, int port)
{
   struct conn_pool_entry *entry;

   LOCK_POOL();

   net_http_conn_pool_remove_expired();

   entry = conn_pool;
   while (entry)
   {
      if (  !entry->in_use 
          && port == entry->port
          && strcmp(entry->domain, domain) == 0)
      {
         entry->in_use = true;
         net_http_conn_pool_move_to_end(entry);
         UNLOCK_POOL();
         return entry;
      }
      entry = entry->next;
   }
   UNLOCK_POOL();
   return NULL;
}

static struct conn_pool_entry *net_http_conn_pool_add(const char *domain, int port, int fd, bool ssl)
{
   struct conn_pool_entry *entry = (struct conn_pool_entry*)
      calloc(1, sizeof(*entry));
   if (!entry)
      return NULL;
   entry->domain = strdup(domain);
   entry->port = port;
   entry->fd = fd;
   entry->in_use = true;
   entry->ssl = ssl;
   entry->connected = false;
   LOCK_POOL();
   net_http_conn_pool_move_to_end(entry);
   UNLOCK_POOL();
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
   state->request.useragent= conn->useragent ? strdup(conn->useragent) : NULL;
   state->request.headers  = conn->headers ? strdup(conn->headers) : NULL;
   state->request.port     = conn->port;

   state->response.status  = -1;
   state->response.buflen  = 64 * 1024;  /* Start with larger buffer to reduce reallocations */
   state->response.data    = (char*)malloc(state->response.buflen);
   state->response.headers = string_list_new();

   return state;
}

static void net_http_resolve(void *data)
{
   int port;
   char *domain;
   char port_buf[6];
   struct dns_cache_entry *entry = (struct dns_cache_entry*)data;
   struct addrinfo hints         = {0};
   struct addrinfo *addr         = NULL;
#if defined(HAVE_SOCKET_LEGACY) || defined(WIIU)
   int family                    = AF_INET;
#else
   int family                    = AF_UNSPEC;
#endif

   hints.ai_family               = family;
   hints.ai_socktype             = SOCK_STREAM;
   hints.ai_flags               |= AI_NUMERICSERV;

   LOCK_DNS_CACHE();
   domain = strdup(entry->domain);
   port = entry->port;
   UNLOCK_DNS_CACHE();

   if (!network_init())
   {
      LOCK_DNS_CACHE();
      entry->valid = true;
      entry->addr = NULL;
      UNLOCK_DNS_CACHE();
      free(domain);
      return;
   }

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);

   getaddrinfo_retro(domain, port_buf, &hints, &addr);
   free(domain);

   LOCK_DNS_CACHE();
   entry->valid = true;
   entry->addr = addr;
   UNLOCK_DNS_CACHE();
}

static bool net_http_new_socket(struct http_t *state)
{
   struct addrinfo *addr = NULL;
   struct dns_cache_entry *entry;

#ifdef HAVE_THREADS
   if (!dns_cache_lock)
      dns_cache_lock = slock_new();
   LOCK_DNS_CACHE();

   /* need some place to create this, I guess */
   if (!conn_pool_lock)
      conn_pool_lock = slock_new();
#endif

   entry = net_http_dns_cache_find(state->request.domain, state->request.port);
   if (entry)
   {
      if (entry->valid)
      {
         int fd;
         if (!entry->addr)
         {
            net_http_log_transport_state(state, "dns_lookup_failed", -1);
            UNLOCK_DNS_CACHE();
            return false;
         }
         addr = entry->addr;
         fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
         if (fd >= 0)
            state->conn = net_http_conn_pool_add(state->request.domain, state->request.port, fd, state->ssl);
         else
            net_http_log_transport_state(state, "socket_create_failed", -1);
         /* still waiting on thread */
         UNLOCK_DNS_CACHE();
         return (fd >= 0);
      }
      else
      {
         /* still waiting on thread */
         UNLOCK_DNS_CACHE();
         return true;
      }
   }
   else
   {
      entry = net_http_dns_cache_add(state->request.domain, state->request.port, NULL);
#ifdef HAVE_THREADS
      /* create the entry for it as an indicator that the request is underway */
      entry->thread = sthread_create(net_http_resolve, entry);
#else
      net_http_resolve(entry);
#endif
   }

   UNLOCK_DNS_CACHE();

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
      if (!conn)
      {
         net_http_log_transport_state(state, "connect_missing_dns_or_conn", -1);
         return false;
      }
      for (next_addr = addr; conn->fd >= 0; conn->fd = socket_next((void**)&next_addr))
      {
         if (!(conn->ssl_ctx = ssl_socket_init(conn->fd, state->request.domain)))
         {
            net_http_log_transport_state(state, "ssl_init_failed", -1);
            socket_close(conn->fd);
            break;
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

         if (ssl_socket_connect(conn->ssl_ctx, next_addr, timeout, true) < 0)
         {
            net_http_log_transport_state(state, "ssl_connect_failed", -1);
            ssl_socket_close(conn->ssl_ctx);
            ssl_socket_free(conn->ssl_ctx);
            conn->ssl_ctx = NULL;
         }
         else
         {
            conn->connected = true;
            return true;
         }
      }
      conn->fd    = -1; /* already closed */
      net_http_conn_pool_remove(conn);
      state->conn = NULL;
      state->err  = true;
      return false;
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

         net_http_log_transport_state(state, "socket_connect_failed", -1);
         socket_close(conn->fd);
      }
      conn->fd    = -1; /* already closed */
      net_http_conn_pool_remove(conn);
      state->conn = NULL;
      state->err  = true;
      return false;
   }
}

static void net_http_send_str(
      struct http_t *state, const char *text, size_t text_size)
{
   if (state->err)
      return;
#ifdef HAVE_SSL
   if (state->ssl)
   {
      if (!ssl_socket_send_all_blocking(
                  state->conn->ssl_ctx, text, text_size, true))
      {
         state->err = true;
         net_http_log_transport_state(state, "ssl_send_failed", -1);
      }
   }
   else
#endif
   {
      if (!socket_send_all_blocking(
                  state->conn->fd, text, text_size, true))
      {
         state->err = true;
         net_http_log_transport_state(state, "socket_send_failed", -1);
      }
   }
}

static bool net_http_send_request(struct http_t *state)
{
   struct request *request = (struct request*)&state->request;
   /* This is a bit lazy, but it works. */
   if (request->method)
   {
      net_http_send_str(state, request->method, strlen(request->method));
      net_http_send_str(state, " /", sizeof(" /")-1);
   }
   else
      net_http_send_str(state, "GET /", sizeof("GET /")-1);
   net_http_send_str(state, request->path, strlen(request->path));
   net_http_send_str(state, " HTTP/1.1\r\n", sizeof(" HTTP/1.1\r\n")-1);
   net_http_send_str(state, "Host: ", sizeof("Host: ")-1);
   net_http_send_str(state, request->domain, strlen(request->domain));
   if (request->port && request->port != 80 && request->port != 443)
   {
      char portstr[16];
      size_t _len     = 0;
      portstr[  _len] = ':';
      portstr[++_len] = '\0';
      _len           += snprintf(portstr + _len, sizeof(portstr) - _len,
            "%i", request->port);
      net_http_send_str(state, portstr, _len);
   }
   net_http_send_str(state, "\r\n", sizeof("\r\n")-1);
   /* Pre-formatted headers */
   if (request->headers)
      net_http_send_str(state, request->headers, strlen(request->headers));
   if (request->contenttype)
   {
      net_http_send_str(state, "Content-Type: ", sizeof("Content-Type: ")-1);
      net_http_send_str(state, request->contenttype, strlen(request->contenttype));
      net_http_send_str(state, "\r\n", sizeof("\r\n")-1);
   }
   if (request->method && request->method[0] == 'P')
   {
      size_t _len, len;
      char *len_str = NULL;
      if (     !request->postdata
            && request->method[1] == 'O' /* POST, not PUT */
            && request->contentlength > 0)
      {
         state->err = true;
         net_http_log_transport_state(state, "post_without_payload", -1);
         return true;
      }
      if (!request->headers && !request->contenttype)
         net_http_send_str(state,
               "Content-Type: application/x-www-form-urlencoded\r\n",
               sizeof("Content-Type: application/x-www-form-urlencoded\r\n")-1);
      net_http_send_str(state, "Content-Length: ", sizeof("Content-Length: ")-1);
      _len = request->contentlength;
#ifdef _WIN32
      len     = snprintf(NULL, 0, "%" PRIuPTR, _len);
      len_str = (char*)malloc(len + 1);
      snprintf(len_str, len + 1, "%" PRIuPTR, _len);
#else
      len     = snprintf(NULL, 0, "%llu", (long long unsigned)_len);
      len_str = (char*)malloc(len + 1);
      snprintf(len_str, len + 1, "%llu", (long long unsigned)_len);
#endif
      len_str[len] = '\0';
      net_http_send_str(state, len_str, strlen(len_str));
      net_http_send_str(state, "\r\n", sizeof("\r\n")-1);
      free(len_str);
   }
   net_http_send_str(state, "User-Agent: ", sizeof("User-Agent: ")-1);
   if (request->useragent)
      net_http_send_str(state, request->useragent, strlen(request->useragent));
   else
      net_http_send_str(state, "libretro", sizeof("libretro")-1);
   net_http_send_str(state, "\r\n", sizeof("\r\n")-1);
   net_http_send_str(state, "\r\n", sizeof("\r\n")-1);
   if (request->postdata && request->contentlength)
      net_http_send_str(state, (const char*)request->postdata,
            request->contentlength);
   state->request_sent = true;
   return state->err;
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

static ssize_t net_http_receive_header(struct http_t *state, ssize_t len)
{
   struct response *response = (struct response*)&state->response;
   char *scan;
   char *dataend;

   response->pos += len;
   scan    = response->data;
   dataend = response->data + response->pos;

   while (response->part < P_BODY)
   {
      ssize_t remaining = dataend - scan;
      char *lineend     = (char*)memchr(scan, '\n', remaining);
      if (!lineend)
         break;

      *lineend = '\0';
      if (lineend != scan && lineend[-1] == '\r')
         lineend[-1] = '\0';

      if (response->part == P_HEADER_TOP)
      {
         if (   scan[0] != 'H' || scan[1] != 'T' || scan[2] != 'T'
             || scan[3] != 'P' || scan[4] != '/' || scan[5] != '1'
             || scan[6] != '.')
         {
            response->part = P_DONE;
            state->err     = true;
            return -1;
         }
         {
            const char *p  = scan + 9;
            response->status = (p[0] - '0') * 100
                             + (p[1] - '0') * 10
                             + (p[2] - '0');
         }
         response->part = P_HEADER;
      }
      else
      {
         if (scan[0] == '\0')
         {
            if (response->status == 100)
               response->part = P_HEADER_TOP;
            else
            {
               response->part = P_BODY;
               if (response->bodytype == T_CHUNK)
                  response->part = P_BODY_CHUNKLEN;
            }
            scan = lineend + 1;
            continue;
         }

         switch (scan[0] | 0x20)
         {
            case 'c':
               if (strncasecmp(scan, "Content-Length:",
                     sizeof("Content-Length:") - 1) == 0)
               {
                  char *ptr      = scan + (sizeof("Content-Length:") - 1);
                  ssize_t val    = 0;
                  while (*ptr == ' ' || *ptr == '\t')
                     ++ptr;
                  while (*ptr >= '0' && *ptr <= '9')
                     val = val * 10 + (*ptr++ - '0');
                  response->bodytype = T_LEN;
                  response->len      = val;
               }
               break;
            case 't':
               if (strcasecmp(scan,
                     "Transfer-Encoding: chunked") == 0)
                  response->bodytype = T_CHUNK;
               break;
            default:
               break;
         }

         {
            union string_list_elem_attr attr;
            attr.i = 0;
            string_list_append(response->headers, scan, attr);
         }
      }

      scan = lineend + 1;
   }

   if (scan != response->data)
   {
      ssize_t leftover = dataend - scan;
      if (leftover > 0)
         memmove(response->data, scan, leftover);
      response->pos = leftover;
   }

   if (response->part >= P_BODY)
   {
      len           = response->pos;
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
   return len;
}

static bool net_http_receive_body(struct http_t *state, ssize_t newlen)
{
   struct response *response = (struct response*)&state->response;

   if (newlen < 0 || state->err)
   {
      if (response->bodytype != T_FULL)
         return false;
      response->part      = P_DONE;
      if (response->buflen != response->len)
         response->data   = (char*)realloc(response->data, response->len);
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
            char *end     = (char*)memchr(response->data + response->len + 2,
            '\n', response->pos - response->len - 2);

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
   /* This reinitializes state based on the new location */

   /* URL may be absolute or relative to the current URL */
   bool absolute = (!strncmp(location, "http://", sizeof("http://")-1)
                 || !strncmp(location, "https://", sizeof("https://")-1));

   if (absolute)
   {
      /* this block is a little wasteful, memory-wise */
      struct http_connection_t *new_url = net_http_connection_new(
      location, NULL, NULL);
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
         char *path = (char*)malloc(PATH_MAX_LENGTH);
         fill_pathname_resolve_relative(path, state->request.path,
         location, PATH_MAX_LENGTH);
         free(state->request.path);
         state->request.path = path;
      }
   }
   state->request_sent       = false;
   state->response.part      = P_HEADER_TOP;
   state->response.status    = -1;
   /* Start with larger buffer to reduce reallocations */
   state->response.buflen    = 64 * 1024;
   state->response.data      = (char*)realloc(state->response.data,
   state->response.buflen);
   state->response.pos       = 0;
   state->response.len       = 0;
   state->response.bodytype  = T_FULL;
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
   ssize_t _len = 0;

   if (!state || state->err)
      return true;

   if (!state->conn)
   {
      state->conn = net_http_conn_pool_find(state->request.domain, state->request.port);
      if (!state->conn)
      {
         if (!net_http_new_socket(state))
            state->err = true;
         return state->err;
      }
   }

   if (!state->conn->connected)
   {
      if (!net_http_connect(state))
         state->err = true;
      return state->err;
   }

   if (!state->request_sent)
      return net_http_send_request(state);

   response = (struct response*)&state->response;

#ifdef HAVE_SSL
   if (state->ssl && state->conn->ssl_ctx)
      _len = ssl_socket_receive_all_nonblocking(state->conn->ssl_ctx, &state->err,
            (uint8_t*)response->data + response->pos,
            response->buflen - response->pos);
   else
#endif
      _len = socket_receive_all_nonblocking(state->conn->fd, &state->err,
            (uint8_t*)response->data + response->pos,
            response->buflen - response->pos);

   if (response->part < P_BODY)
   {
      if (_len < 0 || state->err)
      {
         net_http_log_transport_state(state, "receive_header_failed", _len);
         net_http_conn_pool_remove(state->conn);
         state->conn      = NULL;
         state->err       = true;
         response->part   = P_DONE;
         response->status = -1;
         return true;
      }
      _len = net_http_receive_header(state, _len);
   }

   if (response->part >= P_BODY && response->part < P_DONE)
   {
      if (!net_http_receive_body(state, _len))
      {
         net_http_log_transport_state(state, "receive_body_failed", _len);
         net_http_conn_pool_remove(state->conn);
         state->conn      = NULL;
         state->err       = true;
         response->part   = P_DONE;
         response->status = -1;
         return true;
      }
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

   for (_len = 0; (size_t)_len < response->headers->size; _len++)
   {
      if (string_is_equal_case_insensitive(response->headers->elems[_len].data, "connection: close"))
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
      for (_len = 0; (size_t)_len < response->headers->size; _len++)
      {
         if (string_starts_with_case_insensitive(response->headers->elems[_len].data, "Location: "))
            return net_http_redirect(state, response->headers->elems[_len].data + (sizeof("Location: ")-1));
      }
   }

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
 * If the status is not 20x and accept_err is false, it returns NULL.
 **/
struct string_list *net_http_headers_ex(struct http_t *state, bool accept_err)
{
   if (!state)
      return NULL;
   if (!accept_err && !state->err)
      return NULL;
   return state->response.headers;
}

struct string_list *net_http_headers(struct http_t *state)
{
   return net_http_headers_ex(state, false);
}

/**
 * net_http_data:
 *
 * Leaf function.
 *
 * @return the downloaded data. The returned buffer is owned by the
 * HTTP handler; it's freed by net_http_delete().
 * If the status is not 20x and accept_err is false, it returns NULL.
 **/
uint8_t* net_http_data(struct http_t *state, size_t* len, bool accept_err)
{
   if (!state)
      return NULL;

   if (!accept_err && (state->err || state->response.status < 200 || state->response.status > 299))
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
   return (state->err || state->response.status < 200 || state->response.status > 299);
}

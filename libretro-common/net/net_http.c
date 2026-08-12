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

/* Maximum Content-Length we'll honour from a server, to bound the
 * realloc() that follows header parsing.  256 MiB is comfortably
 * larger than any single libretro HTTP payload (core downloads,
 * thumbnail images, assets bundles) and small enough that a
 * hostile server cannot drive the client toward OOM by lying in
 * the Content-Length header. */
#define NET_HTTP_MAX_CONTENT_LENGTH ((size_t)256 * 1024 * 1024)

/* Receive-window floor.  Below this a recv() costs a syscall and a
 * round trip to move almost nothing; see the drain loop in
 * net_http_update(). */
#define NET_HTTP_MIN_RECV_WINDOW    (32 * 1024)

/* Per-call drain bounds.  The budget keeps a saturated link from
 * stalling a frame; the iteration cap keeps a peer that dribbles
 * single bytes from spinning us.
 *
 * The budget is a stall cap, not a throughput knob.  It only binds
 * when the peer can deliver more than budget bytes per task-queue
 * tick -- roughly 16MB/s at the 16.7ms unthreaded cadence -- so for
 * any transfer slower than that the loop exits on EAGAIN long before
 * the budget is reached and its value is irrelevant.  Measured
 * against a rate-limited loopback server at 2MB/s, the largest single
 * call was 112KiB with the budget at 4MiB: never once reached.
 *
 * What the value does control is the worst case when the link *does*
 * outrun us, and there the previous 4MiB was too coarse.  Draining
 * 4MiB of TLS costs one AES-GCM pass over 4MiB: 24-57ms measured on
 * x86-64 with AES-NI (~170MB/s effective), which is 2-4 dropped
 * frames per call in an unthreaded build, on the video thread.  A
 * target doing software AES-GCM at 30-60MB/s lands at 70-140ms.
 * 256KiB holds that to 2-3ms here and stays inside a frame even an
 * order of magnitude slower, while 16MiB still completes in 1.29s
 * against 16.9s for the pre-drain one-recv-per-tick behaviour -- so
 * substantially all of the win survives. */
#define NET_HTTP_DRAIN_BUDGET       ((size_t)256 * 1024)
#define NET_HTTP_DRAIN_MAX_ITERS    256

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
   /* Ownership of data/headers transfers to the caller when it
    * retrieves them via net_http_data() / net_http_headers_ex().
    * Until then this handle owns them and net_http_delete() frees
    * them.  Previously net_http_delete() freed neither, so any path
    * that did not retrieve both leaked -- which is every cancelled
    * download (task_http.c's cancel branch never calls the headers
    * accessor) and, before the accessors were reached, every
    * transport failure. */
   bool owns_data;
   bool owns_headers;
   char *data;
   struct string_list *headers;
   size_t pos;
   size_t len;
   size_t buflen;
   /* Streaming sink bookkeeping.  Zero/NULL when no sink is set, in
    * which case none of the flush paths below run and behaviour is
    * byte-for-byte what it was: the whole body accumulates in `data`.
    *
    * With a sink, decoded body bytes are handed off and dropped from
    * the buffer as they arrive, so peak memory is the receive window
    * rather than the whole payload.  `flushed` is the running count
    * already handed off; `content_len` is the Content-Length as
    * advertised, kept separately because T_LEN's `len` is decremented
    * as bytes are flushed and can no longer answer progress queries. */
   size_t flushed;
   size_t content_len;
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
   net_http_sink_t sink;
   void *sink_data;
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
   net_http_sink_t sink;
   void *sink_data;
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

   /* Malloc failure: leave *dest NULL and bail.  Callers that
    * dereference the result (common in URL-builder flows) will
    * then hit a single deliberate NULL check instead of a random
    * crash in the encoding loop below. */
   if (!enc)
      return;

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
         /* Malloc failure: leave conn untouched and return false
          * so the caller does not use a partially-initialised
          * connection.  Without this check the following memcpy
          * would NULL-deref. */
         if (!urlcopy)
            return false;
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

/**
 * net_http_connection_set_sink:
 *
 * Stream the response body to @cb as it arrives instead of buffering
 * the whole thing.  net_http_data() then returns NULL with a length of
 * 0, since the handle keeps nothing.  Response headers and status are
 * unaffected.
 *
 * @cb returning false aborts the transfer with an error, which is how
 * a full disk or a failed write surfaces.
 **/
void net_http_connection_set_sink(struct http_connection_t *conn,
      net_http_sink_t cb, void *userdata)
{
   if (!conn)
      return;
   conn->sink      = cb;
   conn->sink_data = userdata;
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
      if (conn->postdata)
         memcpy(conn->postdata, content, content_length);
      else
      {
         /* Malloc failure: leave postdata NULL and reset
          * contentlength so net_http_send_request does not
          * advertise a Content-Length it cannot honour. */
         conn->contentlength = 0;
      }
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
         /* An entry whose resolver has not published a result yet
          * cannot be evicted here.  This function only ever runs with
          * the DNS cache lock held -- net_http_dns_cache_find() is
          * called under it from both net_http_new_socket() and
          * net_http_connect() -- and net_http_resolve() takes that
          * same lock, both on entry and again on completion.
          * sthread_join() on a thread blocked acquiring the lock we
          * are holding deadlocks outright, taking the task thread
          * with it (and in unthreaded builds, the frontend).
          *
          * The window is not theoretical.  entry->addr stays NULL for
          * the whole resolution, so the fail-timeout arm above fires
          * after dns_cache_fail_timeout (30s) -- and
          * getaddrinfo_retro() against a blackholed resolver
          * routinely blocks longer than that.  So the entry looks
          * expired precisely while its thread is still running.
          *
          * entry->valid is set by the resolver under the lock
          * immediately before it unlocks and returns, so once it is
          * set nothing that thread does can block on us and the join
          * below is safe.  Until then, leave the entry alone; the
          * next sweep collects it. */
         if (entry->thread && !entry->valid)
         {
            prev  = entry;
            entry = entry->next;
            continue;
         }

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
      /* Move ownership of postdata from conn to state->request rather
       * than malloc+memcpy.  conn is freed by the caller shortly after
       * this function returns (see task_http.c and the sample in
       * libretro-common/samples/net/net_http_test.c; both use conn
       * once with net_http_new then call net_http_connection_free),
       * and conn->postdata is only read by net_http_new and freed by
       * net_http_connection_free - no other code paths observe it.
       * Null the conn fields so net_http_connection_free does not
       * double-free.  Eliminates an O(body size) copy that materially
       * matters for multi-MB POST payloads (file uploads, netplay,
       * translation service requests). */
      state->request.postdata   = conn->postdata;
      conn->postdata            = NULL;
      conn->contentlength       = 0;
   }
   state->request.useragent= conn->useragent ? strdup(conn->useragent) : NULL;
   state->request.headers  = conn->headers ? strdup(conn->headers) : NULL;
   state->request.port     = conn->port;

   state->response.status  = -1;
   state->sink                  = conn->sink;
   state->sink_data             = conn->sink_data;
   state->response.owns_data    = true;
   state->response.owns_headers = true;
   state->response.flushed      = 0;
   state->response.content_len  = 0;
   state->response.buflen  = 64 * 1024;  /* Start with larger buffer to reduce reallocations */
   state->response.data    = (char*)malloc(state->response.buflen);
   state->response.headers = string_list_new();

   /* Any of the strdup / malloc calls above can return NULL on OOM.
    * The dispatch path in net_http_update() dereferences
    * request.domain and writes to response.data without guards; fail
    * the whole setup early here rather than stack up NULL derefs
    * later.
    *
    * Note on cleanup order: net_http_delete() intentionally does not
    * free response.data or response.headers because successful callers
    * take ownership of response.data via net_http_data() and of
    * response.headers via net_http_headers().  On the OOM failure path
    * those ownership transfers never happen, so we free both here
    * before calling net_http_delete() (which then cleans up the
    * request.* fields and the state struct itself). */
   if (   !state->response.data
       || !state->response.headers
       || !state->request.domain
       || !state->request.path
       || !state->request.method
       || (conn->contenttype && !state->request.contenttype)
       || (conn->useragent && !state->request.useragent)
       || (conn->headers   && !state->request.headers))
   {
      /* Note: no postdata OOM check here.  Ownership of postdata is
       * moved from conn (above), not copied, so the transfer cannot
       * fail.  Both conn->postdata and state->request.postdata are
       * correctly set (NULL on conn, the original pointer on
       * state->request) regardless of any OOM elsewhere in this
       * function. */
      if (state->response.data)
         free(state->response.data);
      if (state->response.headers)
         string_list_free(state->response.headers);
      state->response.data    = NULL;
      state->response.headers = NULL;
      net_http_delete(state);
      return NULL;
   }

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
   struct dns_cache_entry *dns_entry;

   /* net_http_dns_cache_find() is not a read-only lookup: it calls
    * net_http_dns_cache_remove_expired(), which unlinks entries,
    * freeaddrinfo()s their addrinfo and free()s the entry, and it
    * joins resolver threads and bumps timestamps.  Calling it here
    * without the lock (as this function used to) let one download
    * free a cache entry while another was walking the same list under
    * the lock -- a use-after-free of the entry and its addrinfo, not
    * merely a benign race.  ThreadSanitizer flags it as soon as two
    * transfers overlap. */
   LOCK_DNS_CACHE();
   dns_entry = net_http_dns_cache_find(state->request.domain,
         state->request.port);
   /* Normally populated by net_http_new_socket() just above, but the
    * entry can expire between the two calls, so this is not the
    * "big bug" the old comment claimed -- it is reachable, and
    * dereferencing NULL here crashed. */
   if (!dns_entry)
   {
      UNLOCK_DNS_CACHE();
      net_http_log_transport_state(state, "connect_missing_dns_entry", -1);
      state->err = true;
      return false;
   }
   addr = dns_entry->addr;
   UNLOCK_DNS_CACHE();

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
#ifdef _WIN32
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
      size_t _len;
      int    len;
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
      /* Use a stack buffer -- the maximum decimal representation
       * of a size_t is 20 digits (UINT64_MAX) + NUL, well within
       * 32 bytes.  Pre-patch this was a malloc/snprintf pair
       * whose malloc was never NULL-checked; a snprintf-into-NULL
       * crash on OOM was the tail end of that sequence. */
      {
         char len_buf[32];
#ifdef _WIN32
         len = snprintf(len_buf, sizeof(len_buf), "%" PRIuPTR, _len);
#else
         len = snprintf(len_buf, sizeof(len_buf), "%llu",
               (long long unsigned)_len);
#endif
         if (len < 0)
            len = 0;
         else if ((size_t)len >= sizeof(len_buf))
            len = sizeof(len_buf) - 1;
         len_buf[len] = '\0';
         net_http_send_str(state, len_buf, (size_t)len);
      }
      net_http_send_str(state, "\r\n", sizeof("\r\n")-1);
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
         /* Status line is "HTTP/1.x SSS <reason>\r\n".  The fixed
          * prefix is 8 bytes, then a space, then 3 status digits ->
          * minimum line length is 12 bytes excluding the NUL we just
          * wrote at lineend (lineend - scan >= 12).  Pre-patch this
          * was not checked and a short malicious line like
          * "HTTP/1.0\n" let the code read scan[9..11] past the
          * terminator into whatever followed in the receive buffer. */
         ssize_t line_len = lineend - scan;
         if (   line_len < 12
             || scan[0] != 'H' || scan[1] != 'T' || scan[2] != 'T'
             || scan[3] != 'P' || scan[4] != '/' || scan[5] != '1'
             || scan[6] != '.' || scan[8] != ' ')
         {
            response->part = P_DONE;
            state->err     = true;
            return -1;
         }
         {
            const char *p = scan + 9;
            /* Also verify the three status chars are digits -- a
             * non-digit would produce a negative or junk status. */
            if (   p[0] < '0' || p[0] > '9'
                || p[1] < '0' || p[1] > '9'
                || p[2] < '0' || p[2] > '9')
            {
               response->part = P_DONE;
               state->err     = true;
               return -1;
            }
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
               {
                  response->part = P_BODY_CHUNKLEN;
                  /* The chunked body parser uses response->len as
                   * the position of the current chunklen line --
                   * must start at 0.  A hostile server that sent
                   * both "Content-Length: N" and
                   * "Transfer-Encoding: chunked" would otherwise
                   * leave response->len set to N from the
                   * Content-Length pass, and the first chunked
                   * parse step computed "response->pos -
                   * response->len" as an unsigned wrap to a huge
                   * value.  memchr() at that offset is a wild
                   * OOB read. */
                  response->len = 0;
               }
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
                  /* Parse Content-Length as unsigned with an explicit
                   * cap.  Pre-patch the accumulator was a signed ssize_t
                   * that could overflow (UB) on a very long digit string
                   * and then sign-extend to a huge size_t when assigned
                   * to response->len, driving realloc() toward OOM.  Cap
                   * at NET_HTTP_MAX_CONTENT_LENGTH (256 MiB) which is
                   * larger than any legitimate single HTTP response in
                   * the libretro/RetroArch workflow (cores, thumbnails,
                   * ROM manifests) and leaves a safe headroom before
                   * buflen can wrap. */
                  char *ptr      = scan + (sizeof("Content-Length:") - 1);
                  size_t val     = 0;
                  int    any     = 0;
                  int    oflow   = 0;
                  while (*ptr == ' ' || *ptr == '\t')
                     ++ptr;
                  while (*ptr >= '0' && *ptr <= '9')
                  {
                     size_t digit = (size_t)(*ptr++ - '0');
                     any = 1;
                     /* Detect overflow against the cap rather than
                      * against SIZE_MAX, so the later realloc call
                      * never sees an attacker-chosen huge value. */
                     if (val > (NET_HTTP_MAX_CONTENT_LENGTH - digit) / 10)
                     {
                        oflow = 1;
                        break;
                     }
                     val = val * 10 + digit;
                  }
                  if (!any || oflow)
                  {
                     /* Malformed header: treat as protocol error. */
                     response->part = P_DONE;
                     state->err     = true;
                     return -1;
                  }
                  response->bodytype    = T_LEN;
                  response->len         = val;
                  response->content_len = val;
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
      /* With a sink the body never accumulates, so sizing the buffer
       * to Content-Length would allocate the very thing streaming
       * exists to avoid -- up to NET_HTTP_MAX_CONTENT_LENGTH of it,
       * on a server's say-so.  Keep the receive-window buffer. */
      if (response->bodytype == T_LEN && response->len > 0 && !state->sink)
      {
         /* Use a tmp pointer so a realloc failure does not leak the
          * original buffer AND leave response->data NULL for later
          * writes to dereference. */
         char *tmp;
         response->buflen = response->len;
         tmp              = (char*)realloc(response->data, response->buflen);
         if (!tmp)
         {
            response->part = P_DONE;
            state->err     = true;
            return -1;
         }
         response->data   = tmp;
      }
   }
   else
   {
      if (response->pos >= response->buflen - 64)
      {
         char *tmp;
         response->buflen *= 2;
         tmp               = (char*)realloc(response->data, response->buflen);
         if (!tmp)
         {
            response->part = P_DONE;
            state->err     = true;
            return -1;
         }
         response->data    = tmp;
      }
   }
   return len;
}

/* Hand @n decoded body bytes from the front of the buffer to the
 * sink.  Callers reset pos/len afterwards; this only moves the data
 * out and keeps the running total.
 *
 * A sink refusing the write (full disk, I/O error) aborts the
 * transfer the same way a transport failure would. */
static bool net_http_sink_flush(struct http_t *state, size_t n)
{
   struct response *response = (struct response*)&state->response;

   if (!n)
      return true;

   if (!state->sink(state->sink_data, response->data, n))
   {
      net_http_log_transport_state(state, "sink_write_failed", -1);
      state->err     = true;
      response->part = P_DONE;
      return false;
   }

   response->flushed += n;
   return true;
}

static bool net_http_receive_body(struct http_t *state, ssize_t newlen)
{
   struct response *response = (struct response*)&state->response;

   if (newlen < 0 || state->err)
   {
      if (response->bodytype != T_FULL)
         return false;
      response->part      = P_DONE;
      if (response->buflen != response->len && response->len > 0)
      {
         /* Shrink response->data from buflen bytes to len bytes.
          * Use a tmp pointer so a realloc() failure (rare on shrink
          * but not impossible) does not overwrite response->data
          * with NULL and leak the original buffer.  Sibling shrink
          * path at ~line 1528 already uses this pattern; this was
          * the lone holdout.  On failure we keep the oversized-
          * but-valid buffer - this is a terminal state (P_DONE)
          * and the caller tears down shortly afterwards. */
         char *tmp = (char*)realloc(response->data, response->len);
         if (tmp)
            response->data = tmp;
      }
      /* The peer closing the connection *is* the terminator for
       * T_FULL, but the transports report it as a failure:
       * socket_receive_all_nonblocking() sets *err on recv() == 0,
       * and both SSL backends do the same on a clean close.  So
       * state->err was left set through P_DONE and every accessor
       * that respects it refused the body -- net_http_data(state,
       * &len, false), which is exactly what task_http.c calls on the
       * success path, returned NULL for a complete 200 response with
       * the payload sitting in the buffer.  Measured on a 1MiB
       * close-delimited body: 0 bytes with accept_err false, 1048576
       * with it true, on both TLS and plain.  The preceding commit
       * got the bytes into the buffer; they were still unreachable.
       *
       * Clear it, having reached a terminal state we consider valid.
       * Note this cannot distinguish a clean EOF from a socket error
       * midway through the body -- close-delimited framing carries no
       * length to check a truncated body against, which is precisely
       * why HTTP/1.1 servers use Content-Length or chunked and why
       * every other client treats close as success here.  Truncation
       * of a framed body is unaffected: T_LEN and T_CHUNK take the
       * `return false` above and still fail the transfer. */
      state->err = false;
      return true;
   }

   if (response->bodytype == T_CHUNK)
   {
      char  *out;
      char  *in;
      char  *rawend;
      size_t leftover;

      /* De-chunking is a single pass over the raw region.
       *
       * Invariants, unchanged from before: in P_BODY_CHUNKLEN,
       * response->len is the number of decoded body bytes (i.e. the
       * offset of the current chunk-length line) and response->pos is
       * the end of the raw wire data; in P_BODY, response->len is the
       * number of bytes still outstanding in the current chunk and
       * response->pos is the decoded byte count.
       *
       * The previous implementation slid the entire remaining raw tail
       * down over each chunk-length line it consumed, so a byte near
       * the end of the receive window was moved once for every chunk
       * header preceding it.  That is O(window * chunks), and the
       * window is response->buflen - response->pos, which grows
       * without bound as buflen doubles.  A 16 MiB body delivered in
       * 1 KiB chunks moved 45.5 GB; the same body in 8 KiB chunks
       * moved 5.7 GB.  Chunk size is the server's choice, so this is
       * also a cheap way for a peer to burn client CPU and memory
       * bandwidth.
       *
       * Instead, walk the raw region once with separate read and write
       * cursors and copy each chunk payload down to the write cursor.
       * out <= in always holds (decoded data can only ever be shorter
       * than the wire bytes it came from), so the copies stay in
       * bounds; they may overlap, hence memmove.  Every payload byte
       * now moves exactly once per parse pass: O(window). */

      if (response->part == P_BODY)
      {
         /* Finish the chunk carried over from the previous call. */
         if ((size_t)newlen < response->len)
         {
            response->pos += newlen;
            response->len -= newlen;
            if (state->sink)
            {
               if (!net_http_sink_flush(state, response->pos))
                  return false;
               response->pos = 0;
            }
            goto check_grow;
         }
         response->pos += response->len;
         newlen        -= (ssize_t)response->len;
         response->len  = response->pos;
         response->part = P_BODY_CHUNKLEN;
      }

      response->pos += newlen;

      out    = response->data + response->len;
      in     = response->data + response->len;
      rawend = response->data + response->pos;

      for (;;)
      {
         char  *end;
         size_t chunklen;
         size_t avail = (size_t)(rawend - in);

         if (avail < 2)
            break;

         /* Skip the CRLF terminating the previous chunk, then find the
          * end of the chunk-length line.  Starting the scan at in + 2
          * matches the previous behaviour, including on the first
          * chunk where those two bytes are length digits. */
         end = (char*)memchr(in + 2, '\n', avail - 2);
         if (!end)
            break;

         chunklen = strtoul(in, NULL, 16);
         /* Cap the chunk length at the same Content-Length ceiling.
          * A hostile server sending a chunklen like ffffffffffffffff
          * drives the client into an effectively unbounded receive
          * loop. */
         if (chunklen > NET_HTTP_MAX_CONTENT_LENGTH)
         {
            response->part = P_DONE;
            state->err     = true;
            return false;
         }
         end++;

         if (chunklen == 0)
         {
            /* Terminal chunk.  Any trailers after it are discarded,
             * as before.  Shrink the buffer to the decoded length.
             *
             * The length guard matters: a legitimate zero-length
             * chunked body ("0\r\n\r\n") would otherwise reach
             * realloc(data, 0), which glibc answers by freeing the
             * block and returning NULL.  That failed the transfer and
             * left response->data dangling.  Both sibling shrink
             * sites (T_FULL, T_LEN) already carry this guard; this
             * was the only one without it. */
            response->pos  = (size_t)(out - response->data);
            response->part = P_DONE;
            response->len  = response->pos;
            if (state->sink)
            {
               if (!net_http_sink_flush(state, response->pos))
                  return false;
               response->pos = 0;
               response->len = response->flushed;
               return true;
            }
            if (   response->buflen != response->len
                && response->len > 0)
            {
               char *tmp = (char*)realloc(response->data,
                     response->len);
               if (!tmp)
               {
                  state->err = true;
                  return false;
               }
               response->data = tmp;
            }
            return true;
         }

         avail = (size_t)(rawend - end);
         if (avail < chunklen)
         {
            /* Chunk straddles the end of the raw region.  Move what
             * we have and remember the outstanding remainder. */
            if (avail && out != end)
               memmove(out, end, avail);
            out           += avail;
            response->pos  = (size_t)(out - response->data);
            response->len  = chunklen - avail;
            response->part = P_BODY;
            if (state->sink)
            {
               /* In P_BODY, pos is the decoded count and len is what
                * is still outstanding in this chunk, so the decoded
                * prefix can go and the next receive appends at 0. */
               if (!net_http_sink_flush(state, response->pos))
                  return false;
               response->pos = 0;
            }
            goto check_grow;
         }

         if (out != end)
            memmove(out, end, chunklen);
         out += chunklen;
         in   = end + chunklen;
      }

      /* Carry the unconsumed raw remainder down behind the decoded
       * data so the next receive appends to it as before. */
      leftover      = (size_t)(rawend - in);
      response->len = (size_t)(out - response->data);
      if (leftover && out != in)
         memmove(out, in, leftover);
      response->pos  = response->len + leftover;
      response->part = P_BODY_CHUNKLEN;
      if (state->sink)
      {
         /* Decoded bytes sit at data[0..len) with the unconsumed raw
          * tail behind them; hand off the former and slide the tail
          * to the front so the invariant (len == decoded offset of
          * the current chunklen line) still holds at 0. */
         if (!net_http_sink_flush(state, response->len))
            return false;
         if (leftover)
            memmove(response->data, response->data + response->len,
                  leftover);
         response->pos = leftover;
         response->len = 0;
      }
   }
   else if (response->bodytype == T_FULL)
   {
      /* Body is delimited by the peer closing the connection, so
       * there is no expected length to compare against.  response->len
       * stays 0 for T_FULL (it is only ever assigned by the
       * Content-Length parse or the chunked decoder), which is why the
       * shared "pos > len" check below cannot be used here: the first
       * body byte would trip it and the whole transfer would be
       * discarded with status -1.  Just accumulate; the terminal
       * condition is the newlen < 0 branch at the top of this
       * function, which sets P_DONE and shrinks the buffer. */
      response->pos += newlen;
      response->len  = response->pos;
      if (state->sink)
      {
         if (!net_http_sink_flush(state, response->pos))
            return false;
         response->pos = 0;
         response->len = 0;
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
         if (state->sink)
         {
            if (!net_http_sink_flush(state, response->pos))
               return false;
            response->pos = 0;
            response->len = response->flushed;
            return true;
         }
         if (response->buflen != response->len && response->len > 0)
         {
            char *tmp = (char*)realloc(response->data, response->len);
            if (!tmp)
            {
               state->err = true;
               return false;
            }
            response->data = tmp;
         }
         return true;
      }
      if (state->sink)
      {
         /* T_LEN's `len` is the outstanding count once streaming, so
          * decrement it by what we hand off; the "pos == len"
          * completion test above keeps working unchanged.  The
          * advertised total lives in content_len for progress. */
         if (!net_http_sink_flush(state, response->pos))
            return false;
         response->len -= response->pos;
         response->pos  = 0;
      }
   }

check_grow:
   if (response->pos >= response->buflen)
   {
      char *tmp;
      response->buflen *= 2;
      tmp               = (char*)realloc(response->data, response->buflen);
      if (!tmp)
      {
         state->err = true;
         return false;
      }
      response->data    = tmp;
   }
   return true;
}

static bool net_http_redirect(struct http_t *state, const char *location)
{
   /* This reinitializes state based on the new location.  Every
    * allocation below is checked; on any failure state->err is
    * set and we return true (the dispatch loop reads that as
    * "transfer finished with an error"), leaving the state in a
    * safe-to-delete shape. */

   /* URL may be absolute or relative to the current URL */
   char *new_domain = NULL;
   char *new_path   = NULL;
   char *tmp;
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
         state->err = true;
         return true;
      }
      new_domain = strdup(new_url->domain);
      new_path   = strdup(new_url->path);
      if (!new_domain || !new_path)
      {
         free(new_domain);
         free(new_path);
         net_http_connection_free(new_url);
         state->err = true;
         return true;
      }
      state->ssl  = new_url->ssl;
      state->request.port = new_url->port;
      if (state->request.domain)
         free(state->request.domain);
      state->request.domain = new_domain;
      if (state->request.path)
         free(state->request.path);
      state->request.path = new_path;
      net_http_connection_free(new_url);
   }
   else
   {
      if (*location == '/')
      {
         new_path = strdup(location);
         if (!new_path)
         {
            state->err = true;
            return true;
         }
         if (state->request.path)
            free(state->request.path);
         state->request.path = new_path;
      }
      else
      {
         new_path = (char*)malloc(PATH_MAX_LENGTH);
         if (!new_path)
         {
            state->err = true;
            return true;
         }
         fill_pathname_resolve_relative(new_path, state->request.path,
         location, PATH_MAX_LENGTH);
         free(state->request.path);
         state->request.path = new_path;
      }
   }
   state->request_sent       = false;
   state->response.part      = P_HEADER_TOP;
   state->response.status    = -1;
   /* Start with larger buffer to reduce reallocations */
   state->response.buflen    = 64 * 1024;
   tmp = (char*)realloc(state->response.data, state->response.buflen);
   if (!tmp)
   {
      /* Keep the existing buffer; state->data is still valid.
       * Mark the transfer as errored so the dispatch loop tears
       * it down. */
      state->err = true;
      return true;
   }
   state->response.data      = tmp;
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
 * net_http_init:
 *
 * Creates the locks guarding the process-global DNS cache and
 * connection pool.  Must be called once, before any thread can reach
 * net_http_update().
 *
 * These were created lazily on first use inside net_http_new_socket():
 *
 *     if (!dns_cache_lock) dns_cache_lock = slock_new();
 *
 * which is an unsynchronised first-use initialisation of the very
 * lock meant to serialise that cache.  Two threads arriving together
 * each create one, one store wins, and the loser goes on locking an
 * object nobody else holds -- so the cache is walked and mutated with
 * no mutual exclusion at all.  It does not reproduce once the locks
 * exist, which is why it survived a TSan run over concurrent
 * transfers; the window is only ever the first two requests of the
 * process.
 *
 * Idempotent, so callers that cannot easily order their startup can
 * call it more than once -- but not concurrently, which is the whole
 * point.
 **/
void net_http_init(void)
{
#ifdef HAVE_THREADS
   if (!dns_cache_lock)
      dns_cache_lock = slock_new();
   if (!conn_pool_lock)
      conn_pool_lock = slock_new();
#endif
}

/**
 * net_http_deinit:
 *
 * Tears down the DNS cache and connection pool.  Nothing did this
 * before: pooled sockets (and their SSL contexts), cached addrinfo,
 * the strdup'd domains and both mutexes simply lived until the
 * process exited.
 *
 * Both lists are detached under their lock and then drained with the
 * lock released.  That ordering matters for the DNS cache:
 * net_http_resolve() takes the same lock at the end of its run, so
 * joining a resolver thread while holding it deadlocks.
 **/
void net_http_deinit(void)
{
   struct conn_pool_entry *conns;
   struct dns_cache_entry *entries;

   LOCK_POOL();
   conns     = conn_pool;
   conn_pool = NULL;
   UNLOCK_POOL();

   while (conns)
   {
      struct conn_pool_entry *next = conns->next;
      net_http_conn_pool_free(conns);
      conns = next;
   }

   LOCK_DNS_CACHE();
   entries   = dns_cache;
   dns_cache = NULL;
   UNLOCK_DNS_CACHE();

   while (entries)
   {
      struct dns_cache_entry *next = entries->next;
#ifdef HAVE_THREADS
      if (entries->thread)
         sthread_join(entries->thread);
#endif
      if (entries->addr)
         freeaddrinfo_retro(entries->addr);
      free(entries->domain);
      free(entries);
      entries = next;
   }

#ifdef HAVE_THREADS
   if (dns_cache_lock)
   {
      slock_free(dns_cache_lock);
      dns_cache_lock = NULL;
   }
   if (conn_pool_lock)
   {
      slock_free(conn_pool_lock);
      conn_pool_lock = NULL;
   }
#endif
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

   /* Drain the socket, rather than taking a single bite out of it.
    *
    * socket_receive_all_nonblocking() is one recv() despite the name,
    * and this function used to issue exactly one per call.  Since
    * task_http_iterate_transfer() calls us once per task-queue tick,
    * transfer time was
    *
    *     (body_size / bytes_per_recv) * tick_period
    *
    * with bytes_per_recv capped by the kernel receive buffer.  The
    * tick period is ~1ms threaded and one frame (16.7ms at 60Hz)
    * unthreaded, so a 4MiB download over a 64KiB receive buffer cost
    * ~45 ticks -- around 0.8s of pure scheduling latency unthreaded,
    * against ~0.02s of actual transfer.  Nothing was slow except the
    * number of round trips.
    *
    * Looping until the socket reports EAGAIN collapses that to one
    * tick in the common case.  Two bounds keep a fast or hostile peer
    * from holding the calling thread (which is the video thread in
    * unthreaded builds):
    *
    *   - NET_HTTP_DRAIN_BUDGET caps bytes moved per call, so a
    *     saturated link yields rather than stalling a frame;
    *   - NET_HTTP_DRAIN_MAX_ITERS caps syscalls per call, so a peer
    *     dribbling one byte at a time cannot spin us.
    *
    * Exceeding either bound just returns false and we resume on the
    * next tick, which is the pre-existing behaviour. */
   {
      size_t drained = 0;
      int    iters   = 0;

      for (;;)
      {
         size_t window;

         /* Keep a floor under the receive window.  For the
          * doubling-growth body types (T_CHUNK, T_FULL) the window is
          * buflen - pos, which decays toward zero just before each
          * realloc; measured as low as 16 bytes with megabytes still
          * outstanding.  Those reads are round trips that move almost
          * nothing.  Grow early instead of waiting for pos to reach
          * buflen. */
         window = response->buflen - response->pos;
         if (     window < NET_HTTP_MIN_RECV_WINDOW
               && response->part != P_DONE)
         {
            char  *tmp;
            size_t want = response->buflen * 2;
            if (want < response->pos + NET_HTTP_MIN_RECV_WINDOW)
               want = response->pos + NET_HTTP_MIN_RECV_WINDOW;
            if (!(tmp = (char*)realloc(response->data, want)))
            {
               state->err = true;
               break;
            }
            response->data   = tmp;
            response->buflen = want;
            window           = response->buflen - response->pos;
         }

         /* Clamp the window to what is left of the budget, so that the
          * budget is an actual cap rather than a post-hoc check.
          *
          * It used to be tested only after the read had already
          * happened, and for T_LEN the window is the entire remaining
          * body -- so a single recv() off a large kernel receive
          * buffer could return far more than the budget and the check
          * would notice one read too late.  Measured on a 16MiB
          * Content-Length body over loopback: single calls of 6029153
          * and 5536467 bytes against a 4MiB budget, i.e. the bound
          * overshot by ~1.4x, and it scales with SO_RCVBUF rather
          * than with anything we control.
          *
          * drained < NET_HTTP_DRAIN_BUDGET is the loop invariant at
          * this point (the bottom of the loop breaks as soon as that
          * stops holding), so the subtraction cannot underflow.
          *
          * Stop rather than issue a read below the window floor: a
          * clamp to whatever happens to be left of the budget would
          * otherwise reintroduce exactly the tiny reads that
          * NET_HTTP_MIN_RECV_WINDOW exists to prevent, once per call.
          * The drained test guards the degenerate case where the
          * budget is configured below the floor, which would
          * otherwise break before reading anything and stall the
          * transfer outright. */
         if (     drained
               && NET_HTTP_DRAIN_BUDGET - drained < NET_HTTP_MIN_RECV_WINDOW)
            break;
         if (window > NET_HTTP_DRAIN_BUDGET - drained)
            window = NET_HTTP_DRAIN_BUDGET - drained;

#ifdef HAVE_SSL
         if (state->ssl && state->conn->ssl_ctx)
            _len = ssl_socket_receive_all_nonblocking(
                  state->conn->ssl_ctx, &state->err,
                  (uint8_t*)response->data + response->pos, window);
         else
#endif
            _len = socket_receive_all_nonblocking(state->conn->fd,
                  &state->err,
                  (uint8_t*)response->data + response->pos, window);

         if (response->part < P_BODY)
         {
            if (_len < 0 || state->err)
            {
               net_http_log_transport_state(state,
                     "receive_header_failed", _len);
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
               net_http_log_transport_state(state,
                     "receive_body_failed", _len);
               net_http_conn_pool_remove(state->conn);
               state->conn      = NULL;
               state->err       = true;
               response->part   = P_DONE;
               response->status = -1;
               return true;
            }
         }

         if (response->part == P_DONE || state->err)
            break;

         /* _len == 0 is EAGAIN: the socket is drained for now.
          * _len < 0 past the header stage is a close, which the body
          * parser above has already turned into P_DONE for T_FULL and
          * into an error otherwise; either way we are finished here. */
         if (_len <= 0)
            break;

         drained += (size_t)_len;
         if (     drained >= NET_HTTP_DRAIN_BUDGET
               || ++iters >= NET_HTTP_DRAIN_MAX_ITERS)
            break;
      }
   }

   if (progress)
      *progress = response->flushed + response->pos;

   if (total)
   {
      if (response->bodytype == T_LEN)
         /* content_len, not len: with a sink, len counts down as
          * bytes are handed off. */
         *total = response->content_len;
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
   {
      /* net_http_conn_pool_remove_expired() reads in_use under the
       * pool lock and feeds the fd to select(); writing it unlocked
       * raced with that walk. */
      LOCK_POOL();
      state->conn->in_use = false;
      UNLOCK_POOL();
   }
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
 * On a transport error, NULL is returned unless accept_err is true.
 * Headers are returned for any response that was parsed successfully,
 * including HTTP error statuses such as 401 (needed for auth challenges).
 **/
struct string_list *net_http_headers_ex(struct http_t *state, bool accept_err)
{
   if (!state)
      return NULL;
   /* Same predicate as net_http_data(): with no status line there is
    * no header set, only the empty (or half-filled) list allocated by
    * net_http_new(). */
   if (state->response.status < 0)
      return NULL;
   if (!accept_err && state->err)
      return NULL;
   state->response.owns_headers = false;
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

   /* No response was ever parsed, so there is no body to hand back.
    *
    * net_http_new() allocates response.data up front as the receive
    * buffer -- 64KiB of plain malloc().  On a transport failure
    * nothing is written into it and response.len stays 0, but the
    * pointer is non-NULL, and accept_err skipped the check below and
    * returned it: 64KiB of uninitialised, unterminated heap published
    * as a body.  A torn body is equally unusable, since response.len
    * is the T_LEN remainder rather than the bytes that landed.
    *
    * Every give-up path resets status to -1 (and net_http_new()
    * initialises it so), which is the exact predicate.  Returning NULL
    * leaves owns_data true, so net_http_delete() frees the buffer. */
   if (state->response.status < 0)
   {
      if (len)
         *len = 0;
      return NULL;
   }

   if (!accept_err && (state->err || state->response.status < 200 || state->response.status > 299))
   {
      if (len)
         *len = 0;
      return NULL;
   }

   /* Nothing was retained: the body went to the sink as it arrived. */
   if (state->sink)
   {
      if (len)
         *len = 0;
      return NULL;
   }

   if (len)
      *len    = state->response.len;

   /* Ownership moves to the caller.  The pointer is deliberately left
    * in place so repeated calls stay idempotent; only the ownership
    * flag changes, and net_http_delete() consults that. */
   state->response.owns_data = false;

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
   /* Free whatever the caller never took ownership of.  Without this
    * the doc comment on this function ("Cleans up all memory") was
    * simply false for the response side. */
   if (state->response.owns_data && state->response.data)
      free(state->response.data);
   if (state->response.owns_headers && state->response.headers)
      string_list_free(state->response.headers);
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

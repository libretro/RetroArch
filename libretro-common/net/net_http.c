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

#include <retro_posix_source.h>

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
#include <lists/string_list.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <retro_atomic.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
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

/* The receive buffer never grows past a maximal body plus one window.
 * Content-Length was already capped, but a close-delimited or chunked
 * body doubled the buffer for as long as the peer kept sending. */
#define NET_HTTP_MAX_BUFFER         (NET_HTTP_MAX_CONTENT_LENGTH + NET_HTTP_MIN_RECV_WINDOW)

/* Status line plus headers, summed over any 1xx blocks before the
 * final one.  Beyond this the peer is not sending a response we want;
 * failing is cheaper than doubling the buffer to find out. */
#define NET_HTTP_MAX_HEADER_BYTES   (64 * 1024)

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

/* Process-global DNS cache entries.  Every distinct host:port added an
 * entry that stayed for as long as it kept being hit; a session that
 * touches many hosts (thumbnail mirrors, cloud sync, netplay lobbies)
 * grew the list - walked on every connect - without bound.  Past this
 * the least recently used evictable entry goes. */
#define NET_HTTP_DNS_CACHE_MAX      32

/* Idle keep-alive connections kept, and for how long.  A socket idle
 * past the TTL is closed rather than reused: a NAT mapping or server
 * keep-alive timer that expired silently leaves a socket select()
 * never reports, and the reuse sends into a dead connection and waits
 * out the transfer's timeout. */
#define NET_HTTP_POOL_MAX_IDLE      8
#define NET_HTTP_POOL_IDLE_TTL      ((retro_time_t)30 * 1000000)

/* Redirects followed per request.  A server bouncing between two
 * URLs used to be followed forever. */
#define NET_HTTP_MAX_REDIRECTS      8

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
   retro_time_t idle_since;
   int port;
   int fd;
   int family;   /* of the address fd was created for */
   void *ssl_ctx;
   bool ssl;
   bool connected;
   bool in_use;
   struct conn_pool_entry *next;
};

/* Threading model.  The connection pool and the DNS cache belong to the
 * one thread that drives transfers (net_http_new/update/wait/delete);
 * all transfers sharing them must be driven from that thread, one call
 * at a time, and net_http_init()/net_http_deinit() run on it or with it
 * stopped.  In RetroArch that is the task queue's worker, or the main
 * thread with threaded tasks off; switching between the two stops the
 * queue first, so the handover is ordered by the thread join.
 *
 * The only other threads are the DNS resolvers.  Each touches exactly
 * one cache entry, whose domain and port are fixed before it starts: it
 * writes entry->addr, publishes entry->valid with a release store and
 * does not touch the entry again.  The owner reads valid with an
 * acquire load and frees or joins an entry only once it is set.  That
 * replaces the cache lock, the pool lock and the condition variable
 * the resolvers used to broadcast on: nothing here takes a lock. */
static struct conn_pool_entry *conn_pool = NULL;

typedef struct response
{
   /* Ownership of data/headers transfers to the caller when it
    * retrieves them via net_http_data() / net_http_headers_take().
    * Until then this handle owns them and net_http_delete() frees
    * them.  Previously net_http_delete() freed neither, so any path
    * that did not retrieve both leaked -- which is every cancelled
    * download (task_http.c's cancel branch never calls the headers
    * accessor) and, before the accessors were reached, every
    * transport failure. */
   bool owns_data;
   bool owns_headers;
   char *data;
   /* Response header lines in one block: each "Name: value" line
    * NUL-terminated, the block ending in an empty line.  One
    * allocation for the whole set, grown by doubling and bounded by
    * NET_HTTP_MAX_HEADER_BYTES; NULL until the first line.  This used
    * to be a string_list - its struct, a 32-slot array and a strdup
    * per line, for every transfer. */
   char *hdr;
   size_t hdr_len;   /* bytes used, final terminator excluded */
   size_t hdr_cap;
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
   /* Header bytes consumed so far, 1xx blocks included. */
   size_t hdr_bytes;
   int status;
   enum response_part part;
   enum bodytype bodytype;
   /* Set during the header walk, while each line is in cache, so
    * that nothing walks the block again at P_DONE.  location is the
    * offset of the Location value in hdr, plus one (0: none); an
    * offset because the block may move as it grows, and never a
    * pointer into the receive buffer, which is memmove'd after the
    * walk. */
   size_t location;
   /* The connection cannot carry another request once this response
    * is done: Connection: close, HTTP/1.0 without keep-alive, a
    * close-delimited body, a 101, or bytes left over after a response
    * that has no body, which can only be garbage or a response nobody
    * asked for. */
   bool conn_close;
   bool keep_alive;
   bool http10;
   /* A 3xx that will be followed: its body belongs to nobody and
    * must not reach the caller's sink. */
   bool discard;
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
   /* Streamed body: pulled from source in runs as the socket takes
    * them, instead of being held whole in postdata. */
   net_http_source_t source;
   net_http_source_rewind_t source_rewind;
   void *source_data;
   int port;
} request_t;

struct http_t
{
   net_http_sink_t sink;
   void *sink_data;
   /* The transport stage that failed (a literal such as
    * "ssl_connect_failed") and the library code that went with it,
    * for net_http_failure().  NULL/0 until something fails. */
   const char *fail_stage;
   int fail_code;
   bool err;

   struct conn_pool_entry *conn;
   /* The request in flight, sent a slice per net_http_update() as the
    * socket takes it (see net_http_send_request()).  out points at the
    * bytes being sent - the head, the caller's postdata, or a source
    * run in out_stage - and out_off at how far the transport got. */
   char       *out_head;
   char       *out_stage;
   const char *out;
   size_t      out_len;
   size_t      out_off;
   size_t      out_head_len;
   size_t      body_queued;   /* body bytes handed to out so far */
   int         send_phase;    /* enum net_http_send_phase */
   bool ssl;
   bool request_sent;
   /* The last net_http_update() stopped because the transport had
    * nothing more to give, rather than stopping on its own drain
    * budget with bytes still buffered. Only then is there anything for
    * net_http_wait() to wait on; stopping on the budget means the next
    * call has work waiting for it already. */
   bool blocked;
   /* conn came out of the pool rather than being opened for this
    * request; such a connection may have been closed by the peer
    * while idle. */
   bool conn_reused;
   /* The request has already been replayed once on a fresh
    * connection; it will not be replayed again. */
   bool retried;
   unsigned redirects;

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
   net_http_source_t source;
   net_http_source_rewind_t source_rewind;
   void *source_data;
   int port;
   bool ssl;
};

/* The OS error left by the socket call that just failed, for
 * net_http_failure().  Only where the platform keeps it in errno (or
 * WSAGetLastError on Windows); elsewhere 0, meaning "no code". */
static int net_http_socket_error(void)
{
#if defined(_WIN32)
   return WSAGetLastError();
#elif defined(__PS3__) || defined(VITA) || defined(WIIU) \
      || (defined(GEKKO) && !defined(GEKKO_NATIVE)) || defined(_3DS)
   return 0;
#else
   return errno;
#endif
}

/* Record the socket error for the first failing stage.  Must run
 * straight after the failed call, before anything can overwrite it. */
static void net_http_note_socket_error(struct http_t *state)
{
   if (state && !state->fail_stage)
      state->fail_code = net_http_socket_error();
}

static void net_http_log_transport_state(
      struct http_t *state, const char *stage, ssize_t io_len)
{
#if defined(DEBUG)
   int port           = 0;
   int fd             = -1;
   int connected      = 0;
   const char *method = "GET";
   const char *domain = "<null>";
   const char *path   = "<null>";
#endif
   /* Keep the first failure: a connect that fails on one address and
    * then another says the same thing twice, while a later stage
    * failing because of an earlier one says less. */
   if (state && !state->fail_stage)
      state->fail_stage = stage;
#if defined(DEBUG)
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
#endif
}

struct dns_cache_entry
{
   char *domain;
   int port;
   struct addrinfo *addr;
   retro_time_t timestamp;
   /* Lookup finished and addr is final; the resolver's only store
    * after creation.  Read through net_http_dns_entry_valid(). */
   retro_atomic_int_t valid;
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
/* Notified by net_http_resolve() once an entry carries a result, so
 * that a caller with nothing else to wait on can wait for the lookup
 * rather than spin over it.  See net_http_wait_dns(). */
static retro_eventcount_t dns_cache_ec;
static bool               dns_cache_ec_ready = false;
#endif

static bool net_http_dns_entry_valid(struct dns_cache_entry *entry)
{
   return retro_atomic_load_acquire_int(&entry->valid) != 0;
}

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

/* Length of "http://" or "https://" at the front of @s, 0 for anything
 * else. */
static size_t net_http_scheme_len(const char *s)
{
   if (!strncasecmp(s, "http://", sizeof("http://") - 1))
      return sizeof("http://") - 1;
   if (!strncasecmp(s, "https://", sizeof("https://") - 1))
      return sizeof("https://") - 1;
   return 0;
}

static bool net_http_join_put(char *dst, size_t dst_size, size_t *len,
      const char *src, size_t n)
{
   if (n >= dst_size - *len)
      return false;
   memcpy(dst + *len, src, n);
   *len      += n;
   dst[*len]  = '\0';
   return true;
}

/* RFC 3986 5.2.4, in place on a path that starts with '/' and ends at
 * the first '?' or '#'.  Output never outgrows input, so the write
 * cursor stays at or behind the read cursor. */
static void net_http_remove_dot_segments(char *p)
{
   char *r   = p;
   char *w   = p;
   char *end = p + strcspn(p, "?#");

   while (r < end)
   {
      char  *seg  = r + 1;
      char  *next = seg;
      size_t n;
      while (next < end && *next != '/')
         next++;
      n = (size_t)(next - seg);
      if (n == 1 && seg[0] == '.')
      {
         if (next == end)
            *w++ = '/';
      }
      else if (n == 2 && seg[0] == '.' && seg[1] == '.')
      {
         while (w > p && *--w != '/') { }
         if (next == end)
            *w++ = '/';
      }
      else
      {
         memmove(w, r, (size_t)(next - r));
         w += next - r;
      }
      r = next;
   }
   memmove(w, end, strlen(end) + 1);
}

int net_http_url_join(char *dst, size_t dst_size,
      const char *base, const char *ref)
{
   size_t len = 0;
   size_t scheme_len;
   const char *auth_end;
   const char *path_end;
   const char *r;

   if (!dst || !dst_size || !base)
      return -1;
   dst[0] = '\0';
   if (!ref)
      ref = "";
   if (!(scheme_len = net_http_scheme_len(base)))
      return -1;

   /* A reference with a scheme of its own replaces everything, but
    * only an http(s) one is ours to follow. */
   for (r = ref; (*r >= 'a' && *r <= 'z') || (*r >= 'A' && *r <= 'Z')
         || (r != ref && ((*r >= '0' && *r <= '9')
         || *r == '+' || *r == '-' || *r == '.')); r++) { }
   if (r != ref && *r == ':')
   {
      if (!net_http_scheme_len(ref))
         return -1;
      base       = ref;
      scheme_len = net_http_scheme_len(ref);
      ref        = "";
   }

   auth_end = base + scheme_len + strcspn(base + scheme_len, "/?#");
   path_end = auth_end + strcspn(auth_end, "?#");

   if (!*ref)
   {
      if (!net_http_join_put(dst, dst_size, &len, base, strlen(base)))
         goto overflow;
   }
   else if (ref[0] == '/' && ref[1] == '/')
   {
      /* network-path reference: keep "http:" / "https:" only */
      if (   !net_http_join_put(dst, dst_size, &len, base, scheme_len - 2)
          || !net_http_join_put(dst, dst_size, &len, ref, strlen(ref)))
         goto overflow;
   }
   else if (ref[0] == '/')
   {
      if (   !net_http_join_put(dst, dst_size, &len, base,
               (size_t)(auth_end - base))
          || !net_http_join_put(dst, dst_size, &len, ref, strlen(ref)))
         goto overflow;
   }
   else if (ref[0] == '?' || ref[0] == '#')
   {
      const char *keep = (ref[0] == '?')
         ? path_end
         : auth_end + strcspn(auth_end, "#");
      if (   !net_http_join_put(dst, dst_size, &len, base,
               (size_t)(keep - base))
          || !net_http_join_put(dst, dst_size, &len, ref, strlen(ref)))
         goto overflow;
   }
   else
   {
      /* Replace the last segment of the base path. */
      const char *slash = path_end;
      while (slash > auth_end && slash[-1] != '/')
         slash--;
      if (slash > auth_end)
      {
         if (!net_http_join_put(dst, dst_size, &len, base,
                  (size_t)(slash - base)))
            goto overflow;
      }
      else if (   !net_http_join_put(dst, dst_size, &len, base,
                     (size_t)(auth_end - base))
               || !net_http_join_put(dst, dst_size, &len, "/", 1))
         goto overflow;
      if (!net_http_join_put(dst, dst_size, &len, ref, strlen(ref)))
         goto overflow;
   }

   /* The path of the result starts after its own authority, which a
    * "//" reference may have replaced. */
   scheme_len = net_http_scheme_len(dst);
   {
      char *path = dst + scheme_len + strcspn(dst + scheme_len, "/?#");
      if (*path == '/')
      {
         net_http_remove_dot_segments(path);
         len = strlen(dst);
      }
   }
   return (int)len;

overflow:
   dst[0] = '\0';
   return -1;
}

int net_http_urldecode(char *dst, size_t dst_size, const char *src)
{
   return string_percent_decode(dst, dst_size, src);
}

int net_http_urldecode_inplace(char *s)
{
   return string_percent_decode(s, (size_t)-1, s);
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
   /* strncmp, not memcmp: a URL shorter than the scheme must not be
    * read past its terminator. */
   if (strncmp(url, "http://", 7) == 0)
      conn->scan = conn->url + 7;
   else if (strncmp(url, "https://", 8) == 0)
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

   /* An IPv6 literal is bracketed, and its colons are not the port's
    * (RFC 3986 3.2.2).  An unterminated one runs to the end, and
    * net_http_connection_done() refuses it. */
   if (*conn->scan == '[')
   {
      char *close = strchr(conn->scan, ']');
      conn->scan  = close ? close + 1 : conn->scan + strlen(conn->scan);
   }

   while (*conn->scan != '/' && *conn->scan != ':' && *conn->scan != '\0')
      conn->scan++;

   return true;
}

bool net_http_connection_done(struct http_connection_t *conn)
{
   int has_port = 0;

   if (!conn || !conn->domain || !*conn->domain)
      return false;

   /* "[" alone, "[::1" with no closing bracket, or "[::1]junk". */
   if (    *conn->domain == '['
       && (conn->scan < conn->domain + 3 || conn->scan[-1] != ']'))
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
   conn->source        = NULL;
   conn->source_rewind = NULL;
   conn->source_data   = NULL;
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

void net_http_connection_set_content_source(struct http_connection_t *conn,
      const char *content_type, size_t content_length,
      net_http_source_t source, net_http_source_rewind_t rewind,
      void *userdata)
{
   if (conn->contenttype)
      free(conn->contenttype);
   if (conn->postdata)
      free(conn->postdata);
   conn->postdata      = NULL;
   conn->contenttype   = content_type ? strdup(content_type) : NULL;
   conn->contentlength = content_length;
   conn->source        = source;
   conn->source_rewind = rewind;
   conn->source_data   = userdata;
}

const char *net_http_connection_url(struct http_connection_t *conn)
{
   return conn->url;
}

const char* net_http_connection_method(struct http_connection_t* conn)
{
   return conn->method;
}

/* Unlink and free @entry, @prev being the entry before it (NULL at the
 * head).  Only for an entry net_http_dns_cache_evictable() accepts. */
static void net_http_dns_cache_unlink(struct dns_cache_entry *prev,
      struct dns_cache_entry *entry)
{
#ifdef HAVE_THREADS
   /* The resolver has published (evictable() checked) and its last act
    * after that is a notify on the global eventcount, so this join
    * returns at once. */
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
}

/* An entry still being resolved stays: its thread reads entry->domain
 * and writes entry->addr until it publishes valid.  Its addr is NULL the
 * whole time, so it reads as a cached failure and its fail-timeout can
 * pass while a lookup against a blackholed resolver is still running;
 * the next sweep collects it.  (With the old cache lock, joining such a
 * thread here deadlocked, since it took the same lock to publish.) */
static bool net_http_dns_cache_evictable(struct dns_cache_entry *entry)
{
#ifdef HAVE_THREADS
   if (entry->thread && !net_http_dns_entry_valid(entry))
      return false;
#endif
   return true;
}

static void net_http_dns_cache_remove_expired(void)
{
   struct dns_cache_entry *entry = dns_cache;
   struct dns_cache_entry *prev  = NULL;
   /* One clock read for the whole walk, not two per entry. */
   retro_time_t now              = cpu_features_get_time_usec();
   while (entry)
   {
      struct dns_cache_entry *next = entry->next;
      if (     net_http_dns_cache_evictable(entry)
            && entry->timestamp + (entry->addr
               ? dns_cache_timeout : dns_cache_fail_timeout) < now)
         net_http_dns_cache_unlink(prev, entry);
      else
         prev = entry;
      entry = next;
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
         /* addr belongs to the resolver until valid is published. */
         if (net_http_dns_entry_valid(entry))
         {
#ifdef HAVE_THREADS
            if (entry->thread)
            {
               sthread_join(entry->thread);
               entry->thread = NULL;
            }
#endif
            /* don't bump timestamp for failures */
            if (entry->addr)
               entry->timestamp = cpu_features_get_time_usec();
         }
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
   if (!entry->domain)
   {
      free(entry);
      return NULL;
   }
   entry->port = port;
   entry->addr = addr;
   entry->timestamp = cpu_features_get_time_usec();
   retro_atomic_int_init(&entry->valid, addr != NULL);
#ifdef HAVE_THREADS
   entry->thread = NULL;
#endif
   entry->next = dns_cache;
   dns_cache = entry;

   /* Bound the cache.  Runs on insert only (a new host:port), never on
    * a hit or per transfer; the new entry is at the head and is never
    * the one evicted. */
   {
      struct dns_cache_entry *e, *p, *victim = NULL, *victim_prev = NULL;
      unsigned count = 0;
      for (p = NULL, e = dns_cache; e; p = e, e = e->next)
      {
         count++;
         if (   e != entry
             && net_http_dns_cache_evictable(e)
             && (!victim || e->timestamp < victim->timestamp))
         {
            victim      = e;
            victim_prev = p;
         }
      }
      if (count > NET_HTTP_DNS_CACHE_MAX && victim)
         net_http_dns_cache_unlink(victim_prev, victim);
   }
   return entry;
}

static void net_http_conn_pool_free(struct conn_pool_entry *entry)
{
#ifdef HAVE_SSL
   if (entry->ssl && entry->ssl_ctx)
   {
      /* ssl_socket_close() closes the underlying descriptor itself
       * (net_ctx.fd in net_socket_ssl_mbed.c, state->fd in
       * net_socket_ssl_bear.c -- both hold the descriptor in
       * entry->fd), so mark the fd consumed: a second close below
       * would race descriptor reuse and close an fd owned by another
       * thread -- on Android, fdsan aborts when Binder wins that
       * race. */
      ssl_socket_close(entry->ssl_ctx);
      ssl_socket_free(entry->ssl_ctx);
      entry->fd = -1;
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
         return;
      }
      prev = current;
      current = current->next;
   }
}

static void net_http_conn_pool_remove_expired(void)
{
   fd_set fds;
   struct conn_pool_entry *entry = NULL;
   struct conn_pool_entry *prev  = NULL;
   struct timeval tv             = { 0 };
   int max                       = 0;
   retro_time_t now              = cpu_features_get_time_usec();

   /* Idle past the TTL: closed without asking select(), which cannot
    * see a connection that died silently. */
   entry = conn_pool;
   while (entry)
   {
      struct conn_pool_entry *next = entry->next;
      if (!entry->in_use && entry->idle_since + NET_HTTP_POOL_IDLE_TTL < now)
      {
         if (prev)
            prev->next = next;
         else
            conn_pool  = next;
         net_http_conn_pool_free(entry);
      }
      else
         prev = entry;
      entry = next;
   }
   prev = NULL;

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

/* if it's not already in the pool, will add to end. */
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
         return entry;
      }
      entry = entry->next;
   }
   return NULL;
}

/* A finished transfer hands its connection back.  Past
 * NET_HTTP_POOL_MAX_IDLE idle sockets the longest-idle one is closed:
 * that costs some later request a handshake, where keeping every
 * socket a session ever opened costs descriptors and server slots. */
static void net_http_conn_pool_release(struct conn_pool_entry *conn)
{
   struct conn_pool_entry *e, *p, *oldest = NULL, *oldest_prev = NULL;
   unsigned idle = 0;

   conn->in_use     = false;
   conn->idle_since = cpu_features_get_time_usec();
   for (p = NULL, e = conn_pool; e; p = e, e = e->next)
   {
      if (e->in_use)
         continue;
      idle++;
      if (!oldest || e->idle_since < oldest->idle_since)
      {
         oldest      = e;
         oldest_prev = p;
      }
   }
   if (idle > NET_HTTP_POOL_MAX_IDLE && oldest)
   {
      if (oldest_prev)
         oldest_prev->next = oldest->next;
      else
         conn_pool         = oldest->next;
      net_http_conn_pool_free(oldest);
   }
}

static struct conn_pool_entry *net_http_conn_pool_add(const char *domain, int port, int fd, bool ssl)
{
   struct conn_pool_entry *entry = (struct conn_pool_entry*)
      calloc(1, sizeof(*entry));
   if (!entry)
      return NULL;
   entry->domain = strdup(domain);
   if (!entry->domain)
   {
      free(entry);
      return NULL;
   }
   entry->port = port;
   entry->fd = fd;
   entry->in_use = true;
   entry->ssl = ssl;
   entry->connected = false;
   net_http_conn_pool_move_to_end(entry);
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
   state->request.source        = conn->source;
   state->request.source_rewind = conn->source_rewind;
   state->request.source_data   = conn->source_data;
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

   /* Any of the strdup / malloc calls above can return NULL on OOM.
    * The dispatch path in net_http_update() dereferences
    * request.domain and writes to response.data without guards; fail
    * the whole setup early here rather than stack up NULL derefs
    * later.
    *
    * Note on cleanup order: response.data is freed here and nulled
    * before net_http_delete() (which then cleans up the request.*
    * fields and the state struct itself).  The header block does not
    * exist yet; it is allocated with the first header line. */
   if (   !state->response.data
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
      state->response.data    = NULL;
      net_http_delete(state);
      return NULL;
   }

   return state;
}

static void net_http_resolve(void *data)
{
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

   /* domain and port were set before this thread was created and the
    * owner leaves the entry alone until valid is published, so they are
    * read in place; the strdup taken under the old lock was a copy of
    * something nobody could change. */
   if (network_init())
   {
      /* The domain keeps the brackets of an IPv6 literal, since the
       * Host header needs them; the resolver must not see them. */
      const char *node = entry->domain;
      char        literal[64];
      size_t      n    = strlen(node);
      if (node[0] == '[' && n > 2 && node[n - 1] == ']' && n - 2 < sizeof(literal))
      {
         memcpy(literal, node + 1, n - 2);
         literal[n - 2] = '\0';
         node           = literal;
      }
      snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)entry->port);
      getaddrinfo_retro(node, port_buf, &hints, &addr);
   }

   entry->addr = addr;
   /* The entry is the owner's from here on: not one more access. */
   retro_atomic_store_release_int(&entry->valid, 1);
#ifdef HAVE_THREADS
   if (dns_cache_ec_ready)
      retro_eventcount_notify(&dns_cache_ec);
#endif
}

/* A socket for *@addr, or for the first address after it the platform
 * can create one for; *@addr is left on the address the descriptor
 * belongs to.  socket() fails for a family the system lacks - IPv6 on a
 * kernel or console stack built without it - and that used to end the
 * attempt: on the first address it failed the transfer outright, and in
 * the connect loop it hid every address after the failing one. */
static int net_http_socket_for(struct addrinfo **addr)
{
   struct addrinfo *a;
   for (a = *addr; a; a = a->ai_next)
   {
      int fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
      if (fd >= 0)
      {
         *addr = a;
         return fd;
      }
   }
   *addr = NULL;
   return -1;
}

static int net_http_socket_next(struct addrinfo **addr)
{
   if (!*addr)
      return -1;
   *addr = (*addr)->ai_next;
   return net_http_socket_for(addr);
}

static bool net_http_new_socket(struct http_t *state)
{
   struct addrinfo *addr = NULL;
   struct dns_cache_entry *entry;

   entry = net_http_dns_cache_find(state->request.domain, state->request.port);
   if (entry)
   {
      if (net_http_dns_entry_valid(entry))
      {
         int fd;
         if (!entry->addr)
         {
            net_http_log_transport_state(state, "dns_lookup_failed", -1);
            return false;
         }
         addr = entry->addr;
         fd   = net_http_socket_for(&addr);
         if (fd >= 0)
         {
            state->conn = net_http_conn_pool_add(state->request.domain, state->request.port, fd, state->ssl);
            if (state->conn)
               state->conn->family = addr->ai_family;
            else
            {
               socket_close(fd);
               fd = -1;
               net_http_log_transport_state(state, "conn_pool_alloc_failed", -1);
            }
         }
         else
         {
            net_http_note_socket_error(state);
            net_http_log_transport_state(state, "socket_create_failed", -1);
         }
         /* still waiting on thread */
         return (fd >= 0);
      }
      else
      {
         /* still waiting on thread */
         return true;
      }
   }
   else
   {
      entry = net_http_dns_cache_add(state->request.domain, state->request.port, NULL);
      if (!entry)
      {
         net_http_log_transport_state(state, "dns_cache_alloc_failed", -1);
         return false;
      }
#ifdef HAVE_THREADS
      /* create the entry for it as an indicator that the request is underway */
      entry->thread = sthread_create(net_http_resolve, entry);
      if (!entry->thread)
      {
         /* The new head never reached a resolver. */
         dns_cache = entry->next;
         free(entry->domain);
         free(entry);
         net_http_log_transport_state(state, "dns_thread_create_failed", -1);
         return false;
      }
#else
      net_http_resolve(entry);
#endif
   }

   return true;
}

static bool net_http_connect(struct http_t *state)
{
   struct addrinfo *addr = NULL, *next_addr = NULL;
   struct conn_pool_entry *conn = state->conn;
   struct dns_cache_entry *dns_entry;
   bool connected = false;
#ifdef HAVE_SSL
   bool timeout          = true;
#endif

   /* The cache is the driving thread's alone, so addr stays valid for
    * the whole connect: nothing else can expire the entry meanwhile.
    * (It used to be pinned with a user count, because another transfer
    * on another thread could run the expiry sweep mid-connect.) */
   dns_entry = net_http_dns_cache_find(state->request.domain,
         state->request.port);
   /* Normally populated by net_http_new_socket() just above, but the
    * entry can expire between the two calls, so this is not the
    * "big bug" the old comment claimed -- it is reachable, and
    * dereferencing NULL here crashed. */
   if (!dns_entry)
   {
      net_http_log_transport_state(state, "connect_missing_dns_entry", -1);
      state->err = true;
      return false;
   }
   addr = dns_entry->addr;

   /* Start at the address the socket was created for: earlier ones are
    * of a family socket() refused.  An entry that expired and was
    * re-resolved in between may not list that family at all, which is
    * a failed connect like any other. */
   if (conn)
   {
      while (addr && addr->ai_family != conn->family)
         addr = addr->ai_next;
      if (!addr && conn->fd >= 0)
      {
         socket_close(conn->fd);
         conn->fd = -1;
      }
   }

#ifndef HAVE_SSL
   if (state->ssl)
      goto release;
#else
   if (state->ssl)
   {
      /* The TLS layer gets the host as a server name, so an IPv6
       * literal goes without the brackets the URL and the Host header
       * need; the backend then checks the certificate against the
       * address rather than as a name. */
      char        tls_host[256];
      const char *tls_host_src = state->request.domain;
      size_t      dl           = strlen(tls_host_src);
      if (     tls_host_src[0] == '[' && dl > 2 && tls_host_src[dl - 1] == ']'
            && dl - 2 < sizeof(tls_host))
      {
         memcpy(tls_host, tls_host_src + 1, dl - 2);
         tls_host[dl - 2] = '\0';
      }
      else
         strlcpy(tls_host, tls_host_src, sizeof(tls_host));
      if (!conn)
      {
         net_http_log_transport_state(state, "connect_missing_dns_or_conn", -1);
         goto release;
      }
      for (next_addr = addr; next_addr && conn->fd >= 0;
            conn->fd = net_http_socket_next(&next_addr))
      {
         if (!(conn->ssl_ctx = ssl_socket_init(conn->fd, tls_host)))
         {
            net_http_log_transport_state(state, "ssl_init_failed", -1);
            socket_close(conn->fd);
            break;
         }

         /* TODO: Properly figure out what's going wrong when the newer
          timeout/poll code interacts with mbed and winsock
          https://github.com/libretro/RetroArch/issues/14742 */

         /* Temp fix, don't use new timeout/poll code for cheevos http requests */
         timeout = true;
#ifdef _WIN32
         if (!strcmp(state->request.domain, "retroachievements.org"))
            timeout = false;
#endif

         if (ssl_socket_connect(conn->ssl_ctx, next_addr, timeout, true) < 0)
         {
            if (!state->fail_stage)
               state->fail_code = ssl_socket_last_error(conn->ssl_ctx);
            net_http_log_transport_state(state, "ssl_connect_failed", -1);
            ssl_socket_close(conn->ssl_ctx);
            ssl_socket_free(conn->ssl_ctx);
            conn->ssl_ctx = NULL;
         }
         else
         {
            conn->connected = true;
            connected = true;
            goto release;
         }
      }
      conn->fd    = -1; /* already closed */
      net_http_conn_pool_remove(conn);
      state->conn = NULL;
      state->err  = true;
      goto release;
   }
   else
#endif
   {
      for (next_addr = addr; next_addr && conn->fd >= 0;
            conn->fd = net_http_socket_next(&next_addr))
      {
         if (socket_connect_with_timeout(conn->fd, next_addr, 5000))
         {
            conn->connected = true;
            connected = true;
            goto release;
         }

         net_http_note_socket_error(state);
         net_http_log_transport_state(state, "socket_connect_failed", -1);
         socket_close(conn->fd);
      }
      conn->fd    = -1; /* already closed */
      net_http_conn_pool_remove(conn);
      state->conn = NULL;
      state->err  = true;
      goto release;
   }
release:
   return connected;
}

/**
 * net_http_retry_fresh:
 *
 * A pooled connection that the peer closed while it sat idle fails
 * either on the first send or on the first recv, before a single
 * byte of the response has arrived.  Replay the request once on a
 * fresh connection in that case.  Nothing is retried once any
 * response byte has been seen, and a connection opened for this
 * request is never retried at all.
 *
 * @return true if the request has been rearmed and net_http_update()
 * should keep going, false if the failure stands.
 **/
static void net_http_send_reset(struct http_t *state);

static bool net_http_retry_fresh(struct http_t *state)
{
   /* pos alone misses a status line that was consumed whole before
    * the connection died: pos is back at 0 by then, and the replay
    * appended a second header set to the first. */
   if (     !state->conn_reused || state->retried
         || state->response.pos || state->response.hdr_bytes)
      return false;

   /* A streamed body may already be partly consumed; it has to be
    * restarted from its first byte, or the request cannot be replayed. */
   if (state->request.source)
   {
      if (     !state->request.source_rewind
            || !state->request.source_rewind(state->request.source_data))
         return false;
   }

   net_http_log_transport_state(state, "retry_on_fresh_connection", -1);

   if (state->conn)
      net_http_conn_pool_remove(state->conn);

   state->conn            = NULL;
   state->conn_reused     = false;
   state->retried         = true;
   state->err             = false;
   state->request_sent    = false;
   net_http_send_reset(state);
   state->fail_stage      = NULL;
   state->fail_code       = 0;
   state->response.part   = P_HEADER_TOP;
   state->response.pos    = 0;
   state->response.len    = 0;
   state->response.status = -1;
   return true;
}

enum net_http_send_phase
{
   SEND_NONE = 0,
   SEND_HEAD,
   SEND_BODY,
   SEND_FLUSH
};

/* Bytes the request may move per net_http_update(), the send side of
 * NET_HTTP_DRAIN_BUDGET and for the same reason: in an unthreaded build
 * this runs on the video thread, and over TLS every byte is encrypted
 * on the way.  The request used to go out in one blocking send, and
 * net_http's sockets are non-blocking, so a body larger than the socket
 * buffer spun on EAGAIN until the peer had taken all but the last
 * buffer's worth: a 16 MiB PUT at 2 MB/s was one 6 s call at 100% CPU. */
#define NET_HTTP_SEND_BUDGET ((size_t)256 * 1024)

/* A streamed body is pulled from its source one run at a time. */
#define NET_HTTP_SOURCE_RUN  (64 * 1024)

static void net_http_send_reset(struct http_t *state)
{
   free(state->out_head);
   state->out_head     = NULL;
   state->out_head_len = 0;
   state->out          = NULL;
   state->out_len      = 0;
   state->out_off      = 0;
   state->body_queued  = 0;
   state->send_phase   = SEND_NONE;
}

static size_t net_http_put(char *dst, size_t at, const char *src)
{
   size_t n = strlen(src);
   memcpy(dst + at, src, n);
   return at + n;
}

/* The request line and headers, byte for byte what the old sequence of
 * blocking sends wrote, in one buffer: one send where there were a
 * dozen. */
static bool net_http_build_head(struct http_t *state)
{
   struct request *request = &state->request;
   const char *method      = request->method ? request->method : "GET";
   bool body_method        = request->method && request->method[0] == 'P';
   size_t cap              = 192
      + strlen(method) + strlen(request->path) + strlen(request->domain)
      + (request->headers     ? strlen(request->headers)     : 0)
      + (request->contenttype ? strlen(request->contenttype) : 0)
      + (request->useragent   ? strlen(request->useragent)   : 0);
   char  *h;
   size_t n = 0;

   if (!(h = (char*)malloc(cap)))
      return false;
   n = net_http_put(h, n, method);
   n = net_http_put(h, n, " /");
   n = net_http_put(h, n, request->path);
   n = net_http_put(h, n, " HTTP/1.1\r\nHost: ");
   n = net_http_put(h, n, request->domain);
   if (request->port && request->port != 80 && request->port != 443)
      n += (size_t)snprintf(h + n, cap - n, ":%i", request->port);
   n = net_http_put(h, n, "\r\n");
   /* Pre-formatted headers */
   if (request->headers)
      n = net_http_put(h, n, request->headers);
   if (request->contenttype)
   {
      n = net_http_put(h, n, "Content-Type: ");
      n = net_http_put(h, n, request->contenttype);
      n = net_http_put(h, n, "\r\n");
   }
   if (body_method)
   {
      if (!request->headers && !request->contenttype)
         n = net_http_put(h, n,
               "Content-Type: application/x-www-form-urlencoded\r\n");
#ifdef _WIN32
      n += (size_t)snprintf(h + n, cap - n, "Content-Length: %" PRIuPTR "\r\n",
            request->contentlength);
#else
      n += (size_t)snprintf(h + n, cap - n, "Content-Length: %llu\r\n",
            (long long unsigned)request->contentlength);
#endif
   }
   n = net_http_put(h, n, "User-Agent: ");
   n = net_http_put(h, n, request->useragent ? request->useragent : "libretro");
   n = net_http_put(h, n, "\r\n\r\n");

   state->out_head     = h;
   state->out_head_len = n;
   return true;
}

/* As much of @len as the transport takes now: >0 taken, 0 full, -1
 * failed. */
static ssize_t net_http_send_some(struct http_t *state,
      const void *data, size_t len)
{
#ifdef HAVE_SSL
   if (state->ssl)
   {
      ssize_t n = ssl_socket_send_all_nonblocking(state->conn->ssl_ctx,
            data, len, true);
      if (n < 0)
         net_http_log_transport_state(state, "ssl_send_failed", -1);
      return n;
   }
#endif
   {
      ssize_t n = socket_send_all_nonblocking(state->conn->fd, data, len,
            true);
      if (n < 0)
      {
         net_http_note_socket_error(state);
         net_http_log_transport_state(state, "socket_send_failed", -1);
      }
      return n;
   }
}

/* Sends the request a slice at a time: as much as the socket takes,
 * at most NET_HTTP_SEND_BUDGET per call, then returns.  Stopping on a
 * full socket marks the transfer blocked, so net_http_wait() waits for
 * it to become writable; stopping on the budget does not.  Sets
 * request_sent once the last byte has left the TLS layer too.
 *
 * @return true on failure (state->err set), as before. */
static bool net_http_send_request(struct http_t *state)
{
   struct request *request = &state->request;
   size_t budget           = NET_HTTP_SEND_BUDGET;
   size_t body_total       = (request->source || request->postdata)
      ? request->contentlength : 0;

   if (state->send_phase == SEND_NONE)
   {
      if (     request->method
            && request->method[0] == 'P'
            && request->method[1] == 'O' /* POST, not PUT */
            && !request->postdata
            && !request->source
            && request->contentlength > 0)
      {
         state->err = true;
         net_http_log_transport_state(state, "post_without_payload", -1);
         return true;
      }
      if (!net_http_build_head(state))
      {
         state->err = true;
         net_http_log_transport_state(state, "request_oom", -1);
         return true;
      }
      state->out         = state->out_head;
      state->out_len     = state->out_head_len;
      state->out_off     = 0;
      state->body_queued = 0;
      state->send_phase  = SEND_HEAD;
   }

   for (;;)
   {
      ssize_t n;
      size_t  want;

      if (state->out_off == state->out_len)
      {
         if (state->send_phase == SEND_HEAD)
            state->send_phase = SEND_BODY;

         if (state->send_phase == SEND_BODY)
         {
            if (state->body_queued == body_total)
               state->send_phase = SEND_FLUSH;
            else if (request->source)
            {
               int64_t got;
               size_t  run = body_total - state->body_queued;
               if (run > NET_HTTP_SOURCE_RUN)
                  run = NET_HTTP_SOURCE_RUN;
               if (     !state->out_stage
                     && !(state->out_stage = (char*)malloc(NET_HTTP_SOURCE_RUN)))
               {
                  state->err = true;
                  net_http_log_transport_state(state, "source_oom", -1);
                  return true;
               }
               got = request->source(request->source_data,
                     state->out_stage, run);
               /* The head promised exactly Content-Length bytes; a
                * source that stops short, or claims more than the run
                * it was given, cannot be sent. */
               if (got <= 0 || got > (int64_t)run)
               {
                  state->err = true;
                  net_http_log_transport_state(state, "source_short", -1);
                  return true;
               }
               state->out          = state->out_stage;
               state->out_len      = (size_t)got;
               state->out_off      = 0;
               state->body_queued += (size_t)got;
               continue;
            }
            else
            {
               /* Postdata goes straight from the caller's buffer. */
               state->out          = (const char*)request->postdata;
               state->out_len      = body_total;
               state->out_off      = 0;
               state->body_queued  = body_total;
               continue;
            }
         }

         if (state->send_phase == SEND_FLUSH)
         {
#ifdef HAVE_SSL
            if (state->ssl)
            {
               int r = ssl_socket_flush_nonblocking(state->conn->ssl_ctx);
               if (r < 0)
               {
                  state->err = true;
                  net_http_log_transport_state(state, "ssl_send_failed", -1);
                  return true;
               }
               if (!r)
               {
                  state->blocked = true;
                  return false;
               }
            }
#endif
            net_http_send_reset(state);
            state->request_sent = true;
            return false;
         }
      }

      if (!budget)
         return false;
      want = state->out_len - state->out_off;
      if (want > budget)
         want = budget;
      if ((n = net_http_send_some(state, state->out + state->out_off,
                  want)) < 0)
      {
         state->err = true;
         return true;
      }
      if (!n)
      {
         state->blocked = true;
         return false;
      }
      state->out_off += (size_t)n;
      budget         -= (size_t)n;
   }
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

/* Drop the header lines and everything derived from them, keeping
 * the list's storage: for an interim (1xx) block, whose headers are
 * not the final response's, and for a redirect. */
static void net_http_headers_clear(struct response *response)
{
   response->hdr_len    = 0;
   if (response->hdr)
      response->hdr[0]  = '\0';
   response->location   = 0;
   response->discard    = false;
   response->conn_close = false;
   response->keep_alive = false;
   response->http10     = false;
}

/* Does the comma-separated header value @v list @tok?  Members are
 * compared whole and case-insensitively, with the optional whitespace
 * around them ignored, so "Keep-Alive, Close" lists close and
 * "gzip, chunked" lists chunked.  Only called on Connection and
 * Transfer-Encoding lines. */
static bool net_http_list_has(const char *v, const char *tok, size_t toklen)
{
   for (;;)
   {
      const char *e;
      const char *t;
      while (*v == ' ' || *v == '\t' || *v == ',')
         v++;
      if (!*v)
         return false;
      for (e = v; *e && *e != ','; e++) { }
      for (t = e; t > v && (t[-1] == ' ' || t[-1] == '\t'); t--) { }
      if ((size_t)(t - v) == toklen && !strncasecmp(v, tok, toklen))
         return true;
      v = e;
   }
}

/* The body is handed off as it arrives rather than kept: to the
 * caller's sink, or nowhere, for a 3xx whose Location will be followed.
 * Either way it never needs a buffer sized to Content-Length, and the
 * receive buffer is drained after every read. */
static bool net_http_body_streams(const struct http_t *state)
{
   return state->sink || state->response.discard;
}

static ssize_t net_http_receive_header(struct http_t *state, ssize_t len)
{
   struct response *response = (struct response*)&state->response;
   char *scan;
   char *dataend;
   const char *location;

   response->pos += len;
   scan    = response->data;
   dataend = response->data + response->pos;

   while (response->part < P_BODY)
   {
      ssize_t remaining = dataend - scan;
      char *lineend     = (char*)memchr(scan, '\n', remaining);
      if (!lineend)
         break;

      response->hdr_bytes += (size_t)(lineend - scan) + 1;
      if (response->hdr_bytes > NET_HTTP_MAX_HEADER_BYTES)
      {
         net_http_log_transport_state(state, "header_too_large", -1);
         response->part = P_DONE;
         state->err     = true;
         return -1;
      }

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
            /* HTTP/1.0 closes after the response unless the server
             * says keep-alive. */
            response->http10 = (scan[7] == '0');
         }
         response->part = P_HEADER;
      }
      else
      {
         if (scan[0] == '\0')
         {
            int status = response->status;
            if (response->http10 && !response->keep_alive)
               response->conn_close = true;
            if (   status >= 300 && status < 400
                && response->location
                && state->redirects < NET_HTTP_MAX_REDIRECTS)
               response->discard = true;
            if (status >= 100 && status < 200 && status != 101)
            {
               /* Interim response (100 Continue, 102 Processing, 103
                * Early Hints): the real status line follows on this
                * connection. */
               net_http_headers_clear(response);
               /* Framing too: a Content-Length or "chunked" on the
                * interim block is not the final response's, and
                * Transfer-Encoding winning over Content-Length would
                * otherwise carry it straight over. */
               response->bodytype    = T_FULL;
               response->len         = 0;
               response->content_len = 0;
               response->part        = P_HEADER_TOP;
            }
            else if (  status == 204
                    || status == 304
                    || status == 101
                    || (   state->request.method
                        && !strcmp(state->request.method, "HEAD")))
            {
               /* No body, whatever the framing headers say: a HEAD's
                * Content-Length describes the GET it stands in for and
                * a 304's the cached copy.  Waiting for that body, or
                * for a close, is waiting for something a keep-alive
                * server never sends. */
               response->part = P_DONE;
               if (status == 101)
                  response->conn_close = true;
            }
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

         location = NULL;
         switch (scan[0] | 0x20)
         {
            case 'c':
               if (strncasecmp(scan, "Connection:",
                     sizeof("Connection:") - 1) == 0)
               {
                  const char *v = scan + (sizeof("Connection:") - 1);
                  if (net_http_list_has(v, "close", sizeof("close") - 1))
                     response->conn_close = true;
                  else if (net_http_list_has(v, "keep-alive",
                           sizeof("keep-alive") - 1))
                     response->keep_alive = true;
               }
               else if (strncasecmp(scan, "Content-Length:",
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
                  /* Transfer-Encoding wins over Content-Length whichever
                   * comes first (RFC 9112 6.3); a length arriving after
                   * "chunked" used to reframe the body. */
                  if (response->bodytype != T_CHUNK)
                  {
                     response->bodytype    = T_LEN;
                     response->len         = val;
                     response->content_len = val;
                  }
               }
               break;
            case 't':
               if (   strncasecmp(scan, "Transfer-Encoding:",
                        sizeof("Transfer-Encoding:") - 1) == 0
                   && net_http_list_has(
                        scan + (sizeof("Transfer-Encoding:") - 1),
                        "chunked", sizeof("chunked") - 1))
                  response->bodytype = T_CHUNK;
               break;
            case 'l':
               if (strncasecmp(scan, "Location:",
                     sizeof("Location:") - 1) == 0)
               {
                  location = scan + (sizeof("Location:") - 1);
                  while (*location == ' ' || *location == '\t')
                     location++;
               }
               break;
            default:
               break;
         }

         {
            /* Line, its NUL, and the block's closing NUL.  The header
             * cap above bounds the growth. */
            size_t n = (size_t)(lineend - scan);
            if (n && !scan[n - 1])
               n--;   /* the CR, already overwritten */
            if (response->hdr_len + n + 2 > response->hdr_cap)
            {
               char  *tmp;
               size_t want = response->hdr_cap ? response->hdr_cap * 2 : 1024;
               while (want < response->hdr_len + n + 2)
                  want *= 2;
               if (!(tmp = (char*)realloc(response->hdr, want)))
               {
                  response->part = P_DONE;
                  state->err     = true;
                  return -1;
               }
               response->hdr     = tmp;
               response->hdr_cap = want;
            }
            memcpy(response->hdr + response->hdr_len, scan, n + 1);
            if (location)
               response->location = response->hdr_len
                  + (size_t)(location - scan) + 1;
            response->hdr_len += n + 1;
            response->hdr[response->hdr_len] = '\0';
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

   if (response->part == P_DONE)
   {
      /* Bodyless response.  Anything past it is not ours to read,
       * and the next request on this socket would read it first. */
      if (response->pos)
         response->conn_close = true;
      response->pos = 0;
      response->len = 0;
      return 0;
   }

   if (response->part >= P_BODY)
   {
      len           = response->pos;
      response->pos = 0;
      /* With a sink the body never accumulates, so sizing the buffer
       * to Content-Length would allocate the very thing streaming
       * exists to avoid -- up to NET_HTTP_MAX_CONTENT_LENGTH of it,
       * on a server's say-so.  Keep the receive-window buffer. */
      if (     response->bodytype == T_LEN && response->len > 0
            && !net_http_body_streams(state))
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
      /* A line still without its newline counts too, or one endless
       * header line would grow the buffer without bound. */
      if (response->hdr_bytes + response->pos > NET_HTTP_MAX_HEADER_BYTES)
      {
         net_http_log_transport_state(state, "header_too_large", -1);
         response->part = P_DONE;
         state->err     = true;
         return -1;
      }
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

   if (!n || response->discard)
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
      response->part       = P_DONE;
      /* The close that ended the body ended the connection too. */
      response->conn_close = true;
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
            if (net_http_body_streams(state))
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
         const char *digits;
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

         /* A size line with no hex digits used to parse as 0 - the
          * last chunk - so the transfer "succeeded" with whatever was
          * decoded so far and the connection went back to the pool
          * with the rest of the garbage unread. */
         for (digits = in; digits < end
               && (*digits == '\r' || *digits == '\n'
                || *digits == ' '  || *digits == '\t'); digits++) { }
         if (digits == end || !isxdigit((unsigned char)*digits))
         {
            response->part = P_DONE;
            state->err     = true;
            return false;
         }
         chunklen = strtoul(digits, NULL, 16);
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
            if (net_http_body_streams(state))
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
            if (net_http_body_streams(state))
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
      if (net_http_body_streams(state))
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
      if (net_http_body_streams(state))
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

      /* More than Content-Length arrived.  The body is complete; what
       * follows it is garbage or a response nobody asked for, and the
       * next request on this socket would read it first.  Keep the
       * body, drop the rest and the connection - the transfer used to
       * fail outright, with the whole body sitting in the buffer. */
      if (response->pos > response->len)
      {
         response->pos        = response->len;
         response->conn_close = true;
      }
      if (response->pos == response->len)
      {
         response->part = P_DONE;
         if (net_http_body_streams(state))
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
      if (net_http_body_streams(state))
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
      char  *tmp;
      size_t want = response->buflen * 2;
      if (want > NET_HTTP_MAX_BUFFER)
         want = NET_HTTP_MAX_BUFFER;
      if (want <= response->pos)
      {
         net_http_log_transport_state(state, "body_too_large", -1);
         state->err = true;
         return false;
      }
      if (!(tmp = (char*)realloc(response->data, want)))
      {
         state->err = true;
         return false;
      }
      response->data    = tmp;
      response->buflen  = want;
   }
   return true;
}

static bool net_http_redirect(struct http_t *state, const char *location)
{
   /* This reinitializes state based on the new location.  Every
    * allocation below is checked; on any failure state->err is
    * set and we return true (the dispatch loop reads that as
    * "transfer finished with an error"), leaving the state in a
    * safe-to-delete shape.
    *
    * @location aliases the header list, which is cleared further
    * down; it is only read before that, by the join. */
   char *new_domain = NULL;
   char *new_path   = NULL;
   char *tmp;
   char *base;
   char *url;
   char *host;
   char *auth_end;
   char *colon;
   size_t base_size, url_size, host_len;
   int port;
   bool ssl;

   /* The request goes out again, body included; a streamed body has
    * to start over from its first byte, the same contract as the
    * stale-pool replay.  Without a rewind there is no body to send,
    * which is a clean failure rather than a short one. */
   if (state->request.source && state->request.contentlength)
   {
      if (     !state->request.source_rewind
            || !state->request.source_rewind(state->request.source_data))
      {
         net_http_log_transport_state(state, "redirect_rewind_failed", -1);
         state->err = true;
         return true;
      }
   }

   /* One resolution path for every Location form: rebuild the current
    * URL and join the reference onto it.  This replaced an absolute /
    * absolute-path / relative split whose absolute branch allocated a
    * whole connection object to parse the URL and whose relative
    * branch used a filesystem path helper.  One scratch block holds
    * both strings; the join never outgrows base + ref + '/'. */
   base_size = sizeof("https://:65535/") + strlen(state->request.domain)
             + strlen(state->request.path);
   url_size  = base_size + strlen(location) + 1;
   if (!(base = (char*)malloc(base_size + url_size)))
   {
      state->err = true;
      return true;
   }
   url = base + base_size;
   snprintf(base, base_size, "%s://%s:%d/%s",
         state->ssl ? "https" : "http", state->request.domain,
         state->request.port, state->request.path);
   if (net_http_url_join(url, url_size, base, location) < 0)
      goto fail;

   ssl      = (url[4] == 's' || url[4] == 'S');
   host     = url + (ssl ? sizeof("https://") : sizeof("http://")) - 1;
   auth_end = host + strcspn(host, "/?#");
   /* The fragment is the client's, never part of the request. */
   auth_end[strcspn(auth_end, "#")] = '\0';

   if (*host == '[')
   {
      /* IPv6 literal: the port colon, if any, follows the bracket. */
      char *close = (char*)memchr(host, ']', (size_t)(auth_end - host));
      if (!close || close == host + 1)
         goto fail;
      colon = (close + 1 < auth_end) ? close + 1 : NULL;
      if (colon && *colon != ':')
         goto fail;
      host_len = (size_t)(close + 1 - host);
   }
   else
   {
      colon    = (char*)memchr(host, ':', (size_t)(auth_end - host));
      host_len = (size_t)((colon ? colon : auth_end) - host);
   }
   if (!host_len)
      goto fail;
   port = ssl ? 443 : 80;
   if (colon)
   {
      char *d;
      port = 0;
      for (d = colon + 1; d < auth_end; d++)
      {
         if (*d < '0' || *d > '9' || port > 6553)
            goto fail;
         port = port * 10 + (*d - '0');
      }
      if (port < 1 || port > 65535)
         goto fail;
   }

   /* request.path is kept without its leading slash; the request line
    * supplies it. */
   new_domain = (char*)malloc(host_len + 1);
   new_path   = strdup(*auth_end == '/' ? auth_end + 1 : auth_end);
   if (!new_domain || !new_path)
   {
      free(new_domain);
      free(new_path);
      goto fail;
   }
   memcpy(new_domain, host, host_len);
   new_domain[host_len] = '\0';
   free(base);

   free(state->request.domain);
   free(state->request.path);
   state->request.domain = new_domain;
   state->request.path   = new_path;
   state->request.port   = port;
   state->ssl            = ssl;
   state->request_sent       = false;
   net_http_send_reset(state);
   state->retried            = false;
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
   state->response.pos         = 0;
   state->response.len         = 0;
   state->response.flushed     = 0;
   state->response.content_len = 0;
   state->response.hdr_bytes   = 0;
   state->response.bodytype    = T_FULL;
   /* after this, assume location is invalid */
   net_http_headers_clear(&state->response);
   /* keep going */
   return false;

fail:
   free(base);
   state->err = true;
   return true;
}

/**
 * net_http_init:
 *
 * Creates the eventcount net_http_wait() parks on while a name is being
 * resolved.  Call once at startup, before the first transfer; nothing
 * creates it lazily.  Idempotent, but not safe to call concurrently,
 * and like everything here it belongs to the driving thread (see the
 * threading model at the top of this file).
 *
 * This used to create the DNS cache and pool locks, which in turn had
 * been created lazily on first use - an unsynchronised initialisation
 * of the very lock meant to serialise the cache.  There are no locks
 * left to create.
 **/
void net_http_init(void)
{
#ifdef HAVE_THREADS
   if (!dns_cache_ec_ready)
      dns_cache_ec_ready = retro_eventcount_init(&dns_cache_ec);
#endif
}

/**
 * net_http_deinit:
 *
 * Tears down the DNS cache and connection pool: pooled sockets (and
 * their SSL contexts), cached addrinfo and the strdup'd domains.
 *
 * Resolvers still running are joined.  None of them waits on anything
 * this thread holds - they publish with a store and leave - so the join
 * cannot deadlock; it lasts as long as an outstanding getaddrinfo().
 **/
void net_http_deinit(void)
{
   struct conn_pool_entry *conns   = conn_pool;
   struct dns_cache_entry *entries = dns_cache;

   conn_pool = NULL;
   dns_cache = NULL;

   while (conns)
   {
      struct conn_pool_entry *next = conns->next;
      net_http_conn_pool_free(conns);
      conns = next;
   }

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
   /* Every resolver has been joined, so none can notify any more. */
   if (dns_cache_ec_ready)
   {
      retro_eventcount_free(&dns_cache_ec);
      dns_cache_ec_ready = false;
   }
#endif
}

/**
 * net_http_update:
 *
 * @return true if it's done, or if something broke.
 * @total will be 0 if it's not known.
 **/
/**
 * net_http_wait:
 * @state            : transfer handle
 * @timeout_ms       : longest time to wait, in milliseconds
 *
 * Waits until @state's transport can make progress again, for callers
 * that drive a transfer from a thread of their own rather than once per
 * frame. Such a caller has nothing to pace it, and a fixed sleep
 * between passes costs its full duration whether the answer arrived in
 * a microsecond or not at all.
 *
 * Returns immediately when there is nothing to wait for: before a
 * socket exists, after an error, and - importantly - when the last pass
 * stopped on its own drain budget, which leaves bytes already buffered
 * for the next one.
 *
 * The wait is for writability while connecting or sending and for
 * readability once the request is out.
 *
 * Returns: true when the transport reported itself ready or no wait was
 * needed, false when @timeout_ms elapsed first.
 **/
#ifdef HAVE_THREADS
/* Is a lookup for @domain:@port still outstanding? */
static bool net_http_dns_pending(const char *domain, int port)
{
   struct dns_cache_entry *entry;

   for (entry = dns_cache; entry; entry = entry->next)
   {
      if (port == entry->port && strcmp(entry->domain, domain) == 0)
         return !net_http_dns_entry_valid(entry);
   }
   return false;
}

/* The wait for a transfer that has no socket yet because its name is
 * still being resolved. There is nothing to select() on in that state,
 * so the wait is on the eventcount the resolver notifies after
 * publishing, and until that lands the caller costs nothing. Without it
 * a threaded caller, which has nothing else pacing it, spins between
 * here and net_http_update() for the whole lookup.
 *
 * The predicate is re-checked after registering, so a publish between
 * the first check and the park is never missed. */
static bool net_http_wait_dns(struct http_t *state, int timeout_ms)
{
   retro_time_t deadline;

   if (!dns_cache_ec_ready)
      return true;

   deadline = cpu_features_get_time_usec() + (retro_time_t)timeout_ms * 1000;

   for (;;)
   {
      int key;
      retro_time_t now;

      if (!net_http_dns_pending(state->request.domain, state->request.port))
         return true;
      key = retro_eventcount_prepare_wait(&dns_cache_ec);
      if (!net_http_dns_pending(state->request.domain, state->request.port))
      {
         retro_eventcount_cancel_wait(&dns_cache_ec);
         return true;
      }
      now = cpu_features_get_time_usec();
      if (now >= deadline)
      {
         retro_eventcount_cancel_wait(&dns_cache_ec);
         return false;
      }
      retro_eventcount_commit_wait_timeout(&dns_cache_ec, key,
            (int64_t)(deadline - now));
   }
}
#endif

bool net_http_wait(struct http_t *state, int timeout_ms)
{
   bool rd = false;
   bool wr = false;

   if (!state || state->err || !state->blocked)
      return true;
   if (!state->conn)
   {
#ifdef HAVE_THREADS
      return net_http_wait_dns(state, timeout_ms);
#else
      /* Resolution runs inline, so the result is already published. */
      return true;
#endif
   }
   if (state->conn->fd < 0)
      return true;

   if (state->conn->connected && state->request_sent)
      rd = true;
   else
      wr = true;

   if (!socket_wait(state->conn->fd, &rd, &wr, timeout_ms))
      return false;

   return rd || wr;
}

bool net_http_update(struct http_t *state, size_t* progress, size_t* total)
{
   return net_http_update_budget(state, progress, total, NULL, NULL);
}

bool net_http_update_budget(struct http_t *state,
      size_t* progress, size_t* total,
      bool (*within_budget)(void *budget, size_t avail, size_t len),
      void *budget)
{
   struct response *response;
   ssize_t _len = 0;

   if (!state || state->err)
      return true;

   /* Re-established below wherever the pass ends without progress. */
   state->blocked = false;

   if (!state->conn)
   {
      /* A replayed request goes out on a connection of its own;
       * the pool is what it is recovering from. */
      if (!state->retried)
         state->conn = net_http_conn_pool_find(state->request.domain, state->request.port);
      state->conn_reused = (state->conn != NULL);
      if (!state->conn)
      {
         if (!net_http_new_socket(state))
            state->err = true;
         /* No progress this pass: either the resolver is still
          * running or the socket has only just been created. Both
          * are states net_http_wait() waits out. */
         state->blocked = !state->err;
         return state->err;
      }
   }

   if (!state->conn->connected)
   {
      if (!net_http_connect(state))
         state->err = true;
      state->blocked = !state->err;
      return state->err;
   }

   if (!state->request_sent)
   {
      if (net_http_send_request(state) && net_http_retry_fresh(state))
         return false;
      return state->err;
   }

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
    * next tick, which is the pre-existing behaviour.
    *
    * With @within_budget, the caller's time window replaces the byte
    * budget: each read, together with the TLS decryption behind it,
    * is one work item, the window decides how many a call gets, and
    * each read is still clamped to NET_HTTP_DRAIN_BUDGET so that no
    * single item can outgrow a frame.  A clock bounds the stall on
    * any link and any cipher speed; a byte count could only guess. */
   {
      size_t drained = 0;
      int    iters   = 0;

      for (;;)
      {
         size_t window;

         if (within_budget && !within_budget(budget, 0, 0))
            break;

         /* Keep a floor under the receive window.  For the
          * doubling-growth body types (T_CHUNK, T_FULL) the window is
          * buflen - pos, which decays toward zero just before each
          * realloc; measured as low as 16 bytes with megabytes still
          * outstanding.  Those reads are round trips that move almost
          * nothing.  Grow early instead of waiting for pos to reach
          * buflen. */
         window = response->buflen - response->pos;
         /* Not for a buffered Content-Length body: its buffer is
          * already exactly the body, so the window can only shrink
          * toward the last byte.  Growing there doubled a 16MiB
          * buffer to 32MiB for its final 32KiB and then shrank it back
          * at P_DONE - two reallocs, and on allocators without
          * mremap two whole-body copies, for nothing. */
         if (     window < NET_HTTP_MIN_RECV_WINDOW
               && response->part != P_DONE
               && !(   response->part     == P_BODY
                    && response->bodytype == T_LEN
                    && !net_http_body_streams(state)))
         {
            char  *tmp;
            size_t want = response->buflen * 2;
            if (want < response->pos + NET_HTTP_MIN_RECV_WINDOW)
               want = response->pos + NET_HTTP_MIN_RECV_WINDOW;
            if (want > NET_HTTP_MAX_BUFFER)
               want = NET_HTTP_MAX_BUFFER;
            if (want < response->pos + NET_HTTP_MIN_RECV_WINDOW)
            {
               net_http_log_transport_state(state, "body_too_large", -1);
               state->err = true;
               break;
            }
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
         if (within_budget)
         {
            /* One item: a read of at most the byte budget */
            if (window > NET_HTTP_DRAIN_BUDGET)
               window = NET_HTTP_DRAIN_BUDGET;
         }
         else
         {
            if (     drained
                  && NET_HTTP_DRAIN_BUDGET - drained < NET_HTTP_MIN_RECV_WINDOW)
               break;
            if (window > NET_HTTP_DRAIN_BUDGET - drained)
               window = NET_HTTP_DRAIN_BUDGET - drained;
         }

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
               if (net_http_retry_fresh(state))
                  return false;
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
          * into an error otherwise; either way we are finished here.
          *
          * This is the one exit that leaves nothing buffered anywhere,
          * including inside the TLS layer: the SSL read reports EAGAIN
          * only once it has handed over every decrypted byte it holds.
          * Breaking on the budget below leaves bytes waiting, so only
          * this exit marks the transfer as blocked. */
         if (_len <= 0)
         {
            state->blocked = (_len == 0);
            break;
         }

         drained += (size_t)_len;
         if (     (!within_budget && drained >= NET_HTTP_DRAIN_BUDGET)
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

   if (response->conn_close)
   {
      net_http_conn_pool_remove(state->conn);
      state->conn = NULL;
   }

   if (state->conn)
      net_http_conn_pool_release(state->conn);
   state->conn = NULL;

   if (   response->status >= 300 && response->status < 400
       && response->location)
   {
      if (state->redirects >= NET_HTTP_MAX_REDIRECTS)
      {
         net_http_log_transport_state(state, "redirect_limit", -1);
         state->err = true;
         return true;
      }
      state->redirects++;
      return net_http_redirect(state,
            response->hdr + response->location - 1);
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

/* The header block, or NULL when the accessors must refuse: no
 * status line was ever parsed (same predicate as net_http_data()), or
 * the transfer failed and the caller does not accept that. */
static const char *net_http_headers_block(struct http_t *state,
      bool accept_err)
{
   if (!state || state->response.status < 0)
      return NULL;
   if (!accept_err && state->err)
      return NULL;
   /* A response with no header lines still has a (empty) set; the
    * callers read NULL as "no response". */
   if (!state->response.hdr)
   {
      if (!(state->response.hdr = (char*)malloc(1)))
         return NULL;
      state->response.hdr[0]  = '\0';
      state->response.hdr_cap = 1;
   }
   return state->response.hdr;
}

size_t net_http_headers_compact(char *raw)
{
   char *p = raw;
   char *w = raw;

   if (!raw)
      return 0;
   while (*p)
   {
      char *eol  = strchr(p, '\n');
      char *end;
      char *next;

      if (!eol)
         eol = p + strlen(p);
      next = *eol ? eol + 1 : eol;
      end  = eol;
      while (end > p && (end[-1] == '\r' || end[-1] == ' '))
         end--;
      if (end > p)
      {
         size_t n = (size_t)(end - p);
         memmove(w, p, n);
         w   += n;
         *w++ = '\0';
      }
      p = next;
   }
   *w = '\0';
   return (size_t)(w - raw);
}

const char *net_http_header(struct http_t *state, const char *name)
{
   if (!state || state->response.status < 0 || !state->response.hdr)
      return NULL;
   return net_http_header_value(state->response.hdr, name);
}

char *net_http_headers_take(struct http_t *state, bool accept_err)
{
   const char *h = net_http_headers_block(state, accept_err);
   if (!h)
      return NULL;
   /* Pointer left in place so repeated calls stay idempotent, as with
    * net_http_data(); only ownership moves. */
   state->response.owns_headers = false;
   return (char*)h;
}

const char *net_http_header_next(const char *headers, const char *line)
{
   if (!headers)
      return NULL;
   line = line ? line + strlen(line) + 1 : headers;
   return *line ? line : NULL;
}

const char *net_http_header_value(const char *headers, const char *name)
{
   const char *h;
   size_t n;
   if (!headers || !name)
      return NULL;
   n = strlen(name);
   for (h = headers; *h; h += strlen(h) + 1)
   {
      if (!strncasecmp(h, name, n) && h[n] == ':')
      {
         const char *v = h + n + 1;
         while (*v == ' ' || *v == '\t')
            v++;
         return v;
      }
   }
   return NULL;
}

bool net_http_body_is_framed(const char *headers)
{
   const char *h;
   if (!headers)
      return false;
   for (h = headers; *h; h += strlen(h) + 1)
   {
      /* Same two tests as the header parser above. */
      if (strncasecmp(h, "Content-Length:", STRLEN_CONST("Content-Length:")) == 0)
         return true;
      if (   strncasecmp(h, "Transfer-Encoding:",
               STRLEN_CONST("Transfer-Encoding:")) == 0
          && net_http_list_has(h + STRLEN_CONST("Transfer-Encoding:"),
               "chunked", STRLEN_CONST("chunked")))
         return true;
   }
   return false;
}

/* Legacy form, for code outside RetroArch that still takes the headers
 * as a string_list.  Nothing in-tree calls it: it costs the per-line
 * allocations net_http_headers_take() exists to avoid.  The list is a
 * fresh copy each call, owned by the caller; the block stays with the
 * handle. */
struct string_list *net_http_headers_ex(struct http_t *state, bool accept_err)
{
   union string_list_elem_attr attr;
   struct string_list *list;
   const char *h = net_http_headers_block(state, accept_err);
   if (!h || !(list = string_list_new()))
      return NULL;
   attr.i = 0;
   for (; *h; h += strlen(h) + 1)
   {
      if (!string_list_append(list, h, attr))
      {
         string_list_free(list);
         return NULL;
      }
   }
   return list;
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
   net_http_send_reset(state);
   free(state->out_stage);
   /* Free whatever the caller never took ownership of.  Without this
    * the doc comment on this function ("Cleans up all memory") was
    * simply false for the response side. */
   if (state->response.owns_data && state->response.data)
      free(state->response.data);
   if (state->response.owns_headers)
      free(state->response.hdr);
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

const char *net_http_failure(struct http_t *state, int *code)
{
   if (code)
      *code = state ? state->fail_code : 0;
   return state ? state->fail_stage : NULL;
}

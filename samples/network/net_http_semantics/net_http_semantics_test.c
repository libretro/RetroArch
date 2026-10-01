/* HTTP/1.1 semantics oracle for libretro-common/net/net_http.c.
 *
 * Links the real net_http.c and plays the server in-process on
 * 127.0.0.1: a thread that accepts connections, logs every request it
 * receives (which connection, request line, Host, body) and answers
 * from a per-case script.  Connection identity is what the pool tests
 * look at: a request that should reuse a pooled socket must arrive on
 * the same server-side connection as the one before it, and one that
 * must not must arrive on a new one.
 *
 * Every case has a deadline.  The failures this guards against were
 * mostly hangs (HEAD and 204 waiting for a close that never came, a
 * 302 loop, a 1xx read as the final status), so a hang is a failure,
 * never a stuck job. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <pthread.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <net/net_http.h>
#include <net/net_compat.h>
#include <lists/string_list.h>

#define MAX_CONNS   16
#define CONN_BUF    (256 * 1024)
#define MAX_LOG     64
#define CASE_MS     3000

static int failures = 0;

static void check(int ok, const char *what)
{
   printf("[%s] %s\n", ok ? "pass" : "FAIL", what);
   if (!ok)
      failures++;
}

static int64_t now_ms(void)
{
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static unsigned char pat(size_t i)
{
   return (unsigned char)((i * 131u + (i >> 11)) & 0xff);
}

/* Largest realloc() request net_http made, for the buffer-growth
 * cases.  Linked with --wrap=realloc; only the client thread (this
 * one) reaches net_http's reallocs. */
static size_t realloc_max = 0;
static unsigned long allocs = 0;
void *__real_realloc(void *p, size_t n);
void *__wrap_realloc(void *p, size_t n)
{
   __atomic_fetch_add(&allocs, 1, __ATOMIC_RELAXED);
   if (n > realloc_max)
      realloc_max = n;
   return __real_realloc(p, n);
}

/* Every heap allocation made from the linked objects (the libc ones
 * are not wrapped).  Atomic: the DNS resolver thread allocates too. */
void *__real_malloc(size_t n);
void *__wrap_malloc(size_t n)
{
   __atomic_fetch_add(&allocs, 1, __ATOMIC_RELAXED);
   return __real_malloc(n);
}
void *__real_calloc(size_t n, size_t m);
void *__wrap_calloc(size_t n, size_t m)
{
   __atomic_fetch_add(&allocs, 1, __ATOMIC_RELAXED);
   return __real_calloc(n, m);
}
char *__real_strdup(const char *p);
char *__wrap_strdup(const char *p)
{
   __atomic_fetch_add(&allocs, 1, __ATOMIC_RELAXED);
   return __real_strdup(p);
}

/* ---- scripted server ---- */

enum body_kind { B_NONE = 0, B_RAW, B_CHUNKED };

struct step
{
   const char *head;    /* status line + headers + blank line, verbatim */
   size_t body_len;
   enum body_kind body;
   int close_after;
};

struct reqlog
{
   int conn_id;
   char line[256];
   char host[128];
   size_t body_len;
   int body_ok;
};

struct srv_conn
{
   int fd;
   int id;
   size_t len;
   char buf[CONN_BUF];
};

static pthread_mutex_t srv_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t       srv_thread;
static int             srv_fd   = -1;
static int             srv_port = 0;
static int             srv_stop = 0;
static int             srv_next_id = 0;
static struct srv_conn srv_conns[MAX_CONNS];
/* The script is copied in by case_begin(), heads included, and the
 * copies live until shutdown: the server can still be writing the last
 * response of a case after the client side has finished with it and
 * the case's own (often stack) step array is gone. */
#define MAX_STEPS   8
#define MAX_HEADS   128
static struct step     script[MAX_STEPS];
static char           *heads[MAX_HEADS];
static int             nheads = 0;
static int             script_n = 0;
static int             script_repeat = 0;
static struct reqlog   reqs[MAX_LOG];
static int             nreqs = 0;

static int send_all(int fd, const void *p, size_t n)
{
   const char *c = (const char*)p;
   while (n)
   {
      ssize_t w = send(fd, c, n, MSG_NOSIGNAL);
      if (w <= 0)
      {
         if (w < 0 && errno == EINTR)
            continue;
         return -1;
      }
      c += w;
      n -= (size_t)w;
   }
   return 0;
}

static void send_body(int fd, const struct step *st)
{
   static unsigned char blk[64 * 1024];
   size_t off = 0;

   if (st->body == B_RAW)
   {
      while (off < st->body_len)
      {
         size_t i, n = st->body_len - off;
         if (n > sizeof(blk))
            n = sizeof(blk);
         for (i = 0; i < n; i++)
            blk[i] = pat(off + i);
         if (send_all(fd, blk, n))
            return;
         off += n;
      }
   }
   else if (st->body == B_CHUNKED)
   {
      while (off < st->body_len)
      {
         char hdr[32];
         size_t i, n = st->body_len - off;
         if (n > 1024)
            n = 1024;
         snprintf(hdr, sizeof(hdr), "%lx\r\n", (unsigned long)n);
         for (i = 0; i < n; i++)
            blk[i] = pat(off + i);
         memcpy(blk + n, "\r\n", 2);
         if (send_all(fd, hdr, strlen(hdr)) || send_all(fd, blk, n + 2))
            return;
         off += n;
      }
      send_all(fd, "0\r\n\r\n", 5);
   }
}

static void srv_close(struct srv_conn *c)
{
   close(c->fd);
   c->fd  = -1;
   c->len = 0;
}

/* One complete request at the front of c->buf: log it, answer it,
 * drop it.  Returns 0 when more bytes are needed. */
static int srv_handle(struct srv_conn *c)
{
   char *end, *p;
   size_t head_len, clen = 0, i;
   struct step st_copy;
   const struct step *st;
   struct reqlog *r;

   if (c->len < 4)
      return 0;
   c->buf[c->len < CONN_BUF ? c->len : CONN_BUF - 1] = '\0';
   if (!(end = strstr(c->buf, "\r\n\r\n")))
      return 0;
   head_len = (size_t)(end - c->buf) + 4;

   for (p = c->buf; p < end; )
   {
      char *nl = strstr(p, "\r\n");
      if (!strncasecmp(p, "Content-Length:", 15))
         clen = strtoul(p + 15, NULL, 10);
      p = nl + 2;
   }
   if (c->len < head_len + clen)
      return 0;

   pthread_mutex_lock(&srv_lock);
   r = (nreqs < MAX_LOG) ? &reqs[nreqs] : NULL;
   if (r)
   {
      char *nl = strstr(c->buf, "\r\n");
      size_t ll = (size_t)(nl - c->buf);
      memset(r, 0, sizeof(*r));
      r->conn_id = c->id;
      if (ll >= sizeof(r->line))
         ll = sizeof(r->line) - 1;
      memcpy(r->line, c->buf, ll);
      for (p = c->buf; p < end; )
      {
         char *e = strstr(p, "\r\n");
         if (!strncasecmp(p, "Host: ", 6))
         {
            size_t hl = (size_t)(e - p - 6);
            if (hl >= sizeof(r->host))
               hl = sizeof(r->host) - 1;
            memcpy(r->host, p + 6, hl);
         }
         p = e + 2;
      }
      r->body_len = clen;
      r->body_ok  = 1;
      for (i = 0; i < clen; i++)
         if ((unsigned char)c->buf[head_len + i] != pat(i))
            r->body_ok = 0;
   }
   st = NULL;
   if (script_n)
   {
      if (nreqs < script_n)
         st = &script[nreqs];
      else if (script_repeat)
         st = &script[script_n - 1];
   }
   if (st)
   {
      st_copy = *st;
      st      = &st_copy;
   }
   nreqs++;
   pthread_mutex_unlock(&srv_lock);

   memmove(c->buf, c->buf + head_len + clen, c->len - head_len - clen);
   c->len -= head_len + clen;

   if (!st)
   {
      srv_close(c);
      return 1;
   }
   if (st->head && send_all(c->fd, st->head, strlen(st->head)) == 0)
      send_body(c->fd, st);
   if (st->close_after)
      srv_close(c);
   return 1;
}

static void *srv_main(void *arg)
{
   (void)arg;
   for (;;)
   {
      struct pollfd pfd[MAX_CONNS + 1];
      int map[MAX_CONNS + 1];
      int n = 0, i, stop;

      pthread_mutex_lock(&srv_lock);
      stop = srv_stop;
      pthread_mutex_unlock(&srv_lock);
      if (stop)
         break;

      pfd[n].fd = srv_fd; pfd[n].events = POLLIN; map[n++] = -1;
      for (i = 0; i < MAX_CONNS; i++)
      {
         if (srv_conns[i].fd < 0)
            continue;
         pfd[n].fd = srv_conns[i].fd; pfd[n].events = POLLIN; map[n++] = i;
      }
      if (poll(pfd, (nfds_t)n, 20) <= 0)
         continue;

      for (i = 0; i < n; i++)
      {
         if (!pfd[i].revents)
            continue;
         if (map[i] < 0)
         {
            int fd = accept(srv_fd, NULL, NULL), k;
            if (fd < 0)
               continue;
            for (k = 0; k < MAX_CONNS; k++)
               if (srv_conns[k].fd < 0)
                  break;
            if (k == MAX_CONNS)
            {
               close(fd);
               continue;
            }
            srv_conns[k].fd  = fd;
            srv_conns[k].len = 0;
            pthread_mutex_lock(&srv_lock);
            srv_conns[k].id  = srv_next_id++;
            pthread_mutex_unlock(&srv_lock);
         }
         else
         {
            struct srv_conn *c = &srv_conns[map[i]];
            ssize_t r = recv(c->fd, c->buf + c->len,
                  CONN_BUF - 1 - c->len, 0);
            if (r <= 0)
            {
               srv_close(c);
               continue;
            }
            c->len += (size_t)r;
            while (c->fd >= 0 && srv_handle(c)) { }
         }
      }
   }
   return NULL;
}

static int srv_start(void)
{
   struct sockaddr_in a;
   socklen_t al = sizeof(a);
   int i, one = 1;

   for (i = 0; i < MAX_CONNS; i++)
      srv_conns[i].fd = -1;
   srv_fd = socket(AF_INET, SOCK_STREAM, 0);
   setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
   memset(&a, 0, sizeof(a));
   a.sin_family      = AF_INET;
   a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (   bind(srv_fd, (struct sockaddr*)&a, sizeof(a))
       || listen(srv_fd, 16)
       || getsockname(srv_fd, (struct sockaddr*)&a, &al))
      return -1;
   srv_port = ntohs(a.sin_port);
   return pthread_create(&srv_thread, NULL, srv_main, NULL);
}

static void srv_shutdown(void)
{
   int i;
   pthread_mutex_lock(&srv_lock);
   srv_stop = 1;
   pthread_mutex_unlock(&srv_lock);
   pthread_join(srv_thread, NULL);
   for (i = 0; i < MAX_CONNS; i++)
      if (srv_conns[i].fd >= 0)
         srv_close(&srv_conns[i]);
   close(srv_fd);
   while (nheads)
      free(heads[--nheads]);
}

/* Fresh pool and DNS cache per case, so each case sees only its own
 * connections; then install the script.  The deinit closes whatever
 * the previous case left pooled, and the server drops those on EOF. */
static void case_begin(const struct step *steps, int n, int repeat)
{
   int i;
   net_http_deinit();
   net_http_init();
   pthread_mutex_lock(&srv_lock);
   if (n > MAX_STEPS)
      n = MAX_STEPS;
   for (i = 0; i < n; i++)
   {
      script[i] = steps[i];
      if (steps[i].head && nheads < MAX_HEADS)
      {
         heads[nheads] = strdup(steps[i].head);
         script[i].head = heads[nheads++];
      }
   }
   script_n      = n;
   script_repeat = repeat;
   nreqs         = 0;
   pthread_mutex_unlock(&srv_lock);
}

static int log_count(void)
{
   int n;
   pthread_mutex_lock(&srv_lock);
   n = nreqs;
   pthread_mutex_unlock(&srv_lock);
   return n;
}

static struct reqlog log_get(int i)
{
   struct reqlog r;
   memset(&r, 0, sizeof(r));
   r.conn_id = -1;
   pthread_mutex_lock(&srv_lock);
   if (i >= 0 && i < nreqs && i < MAX_LOG)
      r = reqs[i];
   pthread_mutex_unlock(&srv_lock);
   return r;
}

/* ---- client side ---- */

struct xfer
{
   const char *method;
   /* streamed request body */
   size_t source_len;
   int use_source;
   int source_can_rewind;
   /* streamed response body */
   int use_sink;
   /* results */
   int done;         /* net_http_update() reported completion */
   int err;
   int status;
   size_t len;
   uint8_t *data;
   char *headers;
};

static size_t source_pos = 0, source_total = 0;
static int    source_rewinds = 0;
static unsigned char sink_buf[64 * 1024];
static size_t sink_len = 0;

static int64_t test_source(void *ud, void *buf, size_t len)
{
   size_t i, n = source_total - source_pos;
   (void)ud;
   if (n > len)
      n = len;
   for (i = 0; i < n; i++)
      ((unsigned char*)buf)[i] = pat(source_pos + i);
   source_pos += n;
   return (int64_t)n;
}

static bool test_rewind(void *ud)
{
   (void)ud;
   source_pos = 0;
   source_rewinds++;
   return true;
}

static bool test_sink(void *ud, const void *data, size_t len)
{
   (void)ud;
   if (sink_len + len > sizeof(sink_buf))
      return false;
   memcpy(sink_buf + sink_len, data, len);
   sink_len += len;
   return true;
}

static void xfer_free(struct xfer *x)
{
   free(x->data);
   free(x->headers);
   x->data    = NULL;
   x->headers = NULL;
}

static void xfer_run_url(struct xfer *x, const char *url)
{
   struct http_connection_t *conn;
   struct http_t *h;
   int64_t deadline = now_ms() + CASE_MS;
   uint8_t *d;

   x->done = x->err = 0;
   x->status = -1;
   x->len = 0;
   x->data = NULL;
   x->headers = NULL;

   if (!(conn = net_http_connection_new(url,
               x->method ? x->method : "GET", NULL)))
      return;
   while (!net_http_connection_iterate(conn)) { }
   if (!net_http_connection_done(conn))
   {
      net_http_connection_free(conn);
      return;
   }
   if (x->use_source)
   {
      source_pos     = 0;
      source_total   = x->source_len;
      source_rewinds = 0;
      net_http_connection_set_content_source(conn,
            "application/octet-stream", x->source_len, test_source,
            x->source_can_rewind ? test_rewind : NULL, NULL);
   }
   if (x->use_sink)
   {
      sink_len = 0;
      net_http_connection_set_sink(conn, test_sink, NULL);
   }
   h = net_http_new(conn);
   net_http_connection_free(conn);
   if (!h)
      return;

   for (;;)
   {
      if (net_http_update(h, NULL, NULL))
      {
         x->done = 1;
         break;
      }
      if (now_ms() > deadline)
         break;
      net_http_wait(h, 20);
   }

   x->status = net_http_status(h);
   if (!x->done)
      x->err = 1;
   else
   {
      /* headers_ex() without accept_err refuses exactly when the
       * transfer ended on an error, whatever the status. */
      x->headers = net_http_headers_take(h, false);
      if (!x->headers)
      {
         x->err     = 1;
         x->headers = net_http_headers_take(h, true);
      }
      d = net_http_data(h, &x->len, true);
      if (d)
      {
         x->data = (uint8_t*)malloc(x->len + 1);
         if (x->data)
         {
            memcpy(x->data, d, x->len);
            x->data[x->len] = '\0';
         }
         free(d);
      }
   }
   net_http_delete(h);
}

static void xfer_run(struct xfer *x, const char *path)
{
   char url[256];
   snprintf(url, sizeof(url), "http://127.0.0.1:%d%s", srv_port, path);
   xfer_run_url(x, url);
}

static int body_is(const struct xfer *x, const char *s)
{
   return x->data && x->len == strlen(s) && !memcmp(x->data, s, x->len);
}

static int body_is_pattern(const struct xfer *x, size_t n)
{
   size_t i;
   if (!x->data || x->len != n)
      return 0;
   for (i = 0; i < n; i++)
      if (x->data[i] != pat(i))
         return 0;
   return 1;
}

static int has_header(const struct xfer *x, const char *prefix)
{
   const char *h;
   size_t n = strlen(prefix);
   for (h = net_http_header_next(x->headers, NULL); h;
         h = net_http_header_next(x->headers, h))
      if (!strncasecmp(h, prefix, n))
         return 1;
   return 0;
}

/* Two requests in a row; returns whether the second arrived on the
 * connection the first used. */
static int second_reused(void)
{
   return log_count() >= 2 && log_get(0).conn_id == log_get(1).conn_id;
}

#define OK_2 "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"

/* ---- baseline: GET 200 framing and pool reuse ---- */

static void run_section_baseline(void)
{
   struct xfer a, b;
   static const struct step s_len[] = {
      { "HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\n\r\n", 1048576, B_RAW, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_16m[] = {
      { "HTTP/1.1 200 OK\r\nContent-Length: 16777216\r\n\r\n", 16777216, B_RAW, 0 },
   };
   static const struct step s_chunk[] = {
      { "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n", 300000, B_CHUNKED, 0 },
      { OK_2, 0, B_NONE, 0 },
   };

   memset(&a, 0, sizeof(a));
   memset(&b, 0, sizeof(b));

   case_begin(s_len, 2, 0);
   xfer_run(&a, "/len");
   xfer_run(&b, "/next");
   check(a.done && a.status == 200 && body_is_pattern(&a, 1048576),
         "GET 200 Content-Length 1 MiB: full body");
   check(b.done && body_is(&b, "ok") && second_reused(),
         "GET 200 Content-Length: connection returned to the pool");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_16m, 1, 0);
   xfer_run(&a, "/16m");
   check(a.done && a.status == 200 && body_is_pattern(&a, 16777216),
         "GET 200 Content-Length 16 MiB: full body");
   xfer_free(&a);

   case_begin(s_chunk, 2, 0);
   xfer_run(&a, "/chunked");
   xfer_run(&b, "/next");
   check(a.done && a.status == 200 && body_is_pattern(&a, 300000),
         "GET 200 chunked, 1 KiB chunks: decoded body matches");
   check(b.done && body_is(&b, "ok") && second_reused(),
         "GET 200 chunked: connection returned to the pool");
   xfer_free(&a); xfer_free(&b);
}

/* ---- A1: responses without a body ---- */

static void run_section_a1(void)
{
   struct xfer a, b;
   static const struct step s_head[] = {
      { "HTTP/1.1 200 OK\r\nContent-Length: 1234\r\n\r\n", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_204[] = {
      { "HTTP/1.1 204 No Content\r\n\r\n", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_304[] = {
      { "HTTP/1.1 304 Not Modified\r\nETag: \"x\"\r\nContent-Length: 99\r\n\r\n", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_leftover[] = {
      { "HTTP/1.1 204 No Content\r\n\r\nGARBAGE", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_interim[] = {
      { "HTTP/1.1 102 Processing\r\nX-Interim: 1\r\n\r\n"
        "HTTP/1.1 103 Early Hints\r\nLink: </a>\r\n\r\n"
        OK_2, 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_101[] = {
      { "HTTP/1.1 101 Switching Protocols\r\nUpgrade: x\r\nConnection: Upgrade\r\n\r\n", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };

   memset(&a, 0, sizeof(a));
   memset(&b, 0, sizeof(b));

   case_begin(s_head, 2, 0);
   a.method = "HEAD";
   xfer_run(&a, "/head");
   a.method = NULL;
   xfer_run(&b, "/next");
   check(a.done && a.status == 200 && a.len == 0,
         "HEAD on keep-alive: finishes at the headers, empty body");
   check(b.done && body_is(&b, "ok") && second_reused(),
         "HEAD on keep-alive: next request reads a new status line on the same socket");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_204, 2, 0);
   xfer_run(&a, "/204");
   xfer_run(&b, "/next");
   check(a.done && a.status == 204 && a.len == 0,
         "204 on keep-alive: finishes at the headers");
   check(b.done && body_is(&b, "ok") && second_reused(),
         "204 on keep-alive: socket reused for the next request");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_304, 2, 0);
   xfer_run(&a, "/304");
   xfer_run(&b, "/next");
   check(a.done && a.status == 304 && a.len == 0,
         "304 with a Content-Length: finishes at the headers");
   check(b.done && body_is(&b, "ok") && second_reused(),
         "304 on keep-alive: socket reused for the next request");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_leftover, 2, 0);
   xfer_run(&a, "/204");
   xfer_run(&b, "/next");
   check(a.done && a.status == 204,
         "204 followed by stray bytes: transfer still completes");
   check(b.done && body_is(&b, "ok") && !second_reused(),
         "204 followed by stray bytes: socket not returned to the pool");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_interim, 2, 0);
   xfer_run(&a, "/interim");
   xfer_run(&b, "/next");
   check(a.done && a.status == 200 && body_is(&a, "ok"),
         "102 and 103 before the final status: read as interim header blocks");
   check(a.done && !has_header(&a, "X-Interim") && !has_header(&a, "Link:")
         && has_header(&a, "Content-Length"),
         "interim response headers are not reported as the final response's");
   check(b.done && body_is(&b, "ok") && second_reused(),
         "interim responses: socket reused for the next request");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_101, 2, 0);
   xfer_run(&a, "/upgrade");
   xfer_run(&b, "/next");
   check(a.done && a.status == 101, "101: transfer ends at the headers");
   check(b.done && body_is(&b, "ok") && !second_reused(),
         "101: socket not returned to the pool");
   xfer_free(&a); xfer_free(&b);
}

/* ---- A2: Connection / Transfer-Encoding as lists, HTTP/1.0 ---- */

static void run_section_a2(void)
{
   struct xfer a, b;
   static const struct step s_10[] = {
      { "HTTP/1.0 200 OK\r\nContent-Length: 2\r\n\r\nok", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_10ka[] = {
      { "HTTP/1.0 200 OK\r\nConnection: keep-alive\r\nContent-Length: 2\r\n\r\nok", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_list[] = {
      { "HTTP/1.1 200 OK\r\nConnection: Keep-Alive, Close\r\nContent-Length: 2\r\n\r\nok", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_nospace[] = {
      { "HTTP/1.1 200 OK\r\nConnection:close\r\nContent-Length: 2\r\n\r\nok", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_te[] = {
      { "HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip, chunked\r\n\r\n", 5000, B_CHUNKED, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_te_cl[] = {
      { "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n", 5000, B_CHUNKED, 0 },
      { OK_2, 0, B_NONE, 0 },
   };

   memset(&a, 0, sizeof(a));
   memset(&b, 0, sizeof(b));

   case_begin(s_10, 2, 0);
   xfer_run(&a, "/v10");
   xfer_run(&b, "/next");
   check(a.done && body_is(&a, "ok"), "HTTP/1.0 without Connection: body read");
   check(b.done && body_is(&b, "ok") && !second_reused(),
         "HTTP/1.0 without Connection: socket not returned to the pool");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_10ka, 2, 0);
   xfer_run(&a, "/v10ka");
   xfer_run(&b, "/next");
   check(a.done && body_is(&a, "ok") && b.done && second_reused(),
         "HTTP/1.0 with Connection: keep-alive: socket reused");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_list, 2, 0);
   xfer_run(&a, "/list");
   xfer_run(&b, "/next");
   check(a.done && body_is(&a, "ok") && b.done && !second_reused(),
         "Connection: Keep-Alive, Close: close honoured as a list member");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_nospace, 2, 0);
   xfer_run(&a, "/nospace");
   xfer_run(&b, "/next");
   check(a.done && body_is(&a, "ok") && b.done && !second_reused(),
         "Connection:close without a space: close honoured");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_te, 2, 0);
   xfer_run(&a, "/te");
   xfer_run(&b, "/next");
   check(a.done && body_is_pattern(&a, 5000),
         "Transfer-Encoding: gzip, chunked: body de-chunked without waiting for close");
   check(b.done && second_reused(),
         "Transfer-Encoding: gzip, chunked: socket reused");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_te_cl, 2, 0);
   xfer_run(&a, "/tecl");
   xfer_run(&b, "/next");
   check(a.done && body_is_pattern(&a, 5000) && b.done && body_is(&b, "ok"),
         "Content-Length after Transfer-Encoding: chunked does not reframe the body");
   xfer_free(&a); xfer_free(&b);

   check(net_http_body_is_framed("X-A: 1\0Transfer-Encoding: gzip, chunked\0"),
         "net_http_body_is_framed: chunked as a list member counts");
}

/* ---- A3: redirect policy ---- */

static void run_section_a3(void)
{
   struct xfer a;
   struct reqlog r;
   char abs_head[256];
   char want_host[64];
   struct step s_abs[2];
   static const struct step s_path[] = {
      { "HTTP/1.1 302 Found\r\nContent-Length: 0\r\nLocation: /foo\r\n\r\n", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_rel[] = {
      { "HTTP/1.1 302 Found\r\nLocation: bar\r\nContent-Length: 0\r\nX-After: 1\r\n\r\n", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_nospace[] = {
      { "HTTP/1.1 301 Moved\r\nLocation:/moved\r\nContent-Length: 0\r\n\r\n", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_loop[] = {
      { "HTTP/1.1 302 Found\r\nLocation: /loop\r\nContent-Length: 0\r\n\r\n", 0, B_NONE, 0 },
   };
   static const struct step s_put[] = {
      { "HTTP/1.1 307 Temporary Redirect\r\nLocation: /put2\r\nContent-Length: 0\r\n\r\n", 0, B_NONE, 0 },
      { "HTTP/1.1 201 Created\r\nContent-Length: 2\r\n\r\nok", 0, B_NONE, 0 },
   };
   static const struct step s_sink[] = {
      { "HTTP/1.1 302 Found\r\nLocation: /final\r\nContent-Length: 13\r\n\r\nredirect-body", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };

   memset(&a, 0, sizeof(a));

   case_begin(s_path, 2, 0);
   xfer_run(&a, "/start");
   r = log_get(1);
   check(a.done && body_is(&a, "ok") && !strcmp(r.line, "GET /foo HTTP/1.1"),
         "302 Location: /foo: request line in origin form, not //foo");
   check(second_reused(), "302 on keep-alive: redirect reuses the socket");
   xfer_free(&a);

   case_begin(s_rel, 2, 0);
   xfer_run(&a, "/dir/page");
   r = log_get(1);
   check(a.done && body_is(&a, "ok") && !strcmp(r.line, "GET /dir/bar HTTP/1.1"),
         "302 relative Location (followed by more headers): resolved under the base directory");
   xfer_free(&a);

   case_begin(s_nospace, 2, 0);
   xfer_run(&a, "/old");
   r = log_get(1);
   check(a.done && body_is(&a, "ok") && !strcmp(r.line, "GET /moved HTTP/1.1"),
         "301 Location:/moved without a space: followed");
   xfer_free(&a);

   snprintf(abs_head, sizeof(abs_head),
         "HTTP/1.1 302 Found\r\nContent-Length: 0\r\n"
         "Location: http://localhost:%d/abs?x=1\r\n\r\n", srv_port);
   snprintf(want_host, sizeof(want_host), "localhost:%d", srv_port);
   memset(s_abs, 0, sizeof(s_abs));
   s_abs[0].head = abs_head;
   s_abs[1].head = OK_2;
   case_begin(s_abs, 2, 0);
   xfer_run(&a, "/start");
   r = log_get(1);
   check(a.done && body_is(&a, "ok")
         && !strcmp(r.line, "GET /abs?x=1 HTTP/1.1")
         && !strcmp(r.host, want_host),
         "302 absolute Location: host, port and path taken from it");
   xfer_free(&a);

   case_begin(s_loop, 1, 1);
   xfer_run(&a, "/loop");
   check(a.done && a.err && log_count() == 9,
         "302 loop: stops with an error after 8 redirects (9 requests)");
   xfer_free(&a);

   case_begin(s_put, 2, 0);
   a.method            = "PUT";
   a.use_source        = 1;
   a.source_can_rewind = 1;
   a.source_len        = 100000;
   xfer_run(&a, "/put");
   check(a.done && a.status == 201 && log_count() == 2
         && log_get(0).body_len == 100000 && log_get(0).body_ok
         && log_get(1).body_len == 100000 && log_get(1).body_ok
         && source_rewinds == 1 && second_reused(),
         "streamed PUT then 307: source rewound, full body sent again on the same socket");
   xfer_free(&a);

   case_begin(s_put, 2, 0);
   a.source_can_rewind = 0;
   xfer_run(&a, "/put");
   check(a.done && a.err && log_count() == 1,
         "streamed PUT without a rewind then 307: clean error, nothing half-sent");
   xfer_free(&a);
   memset(&a, 0, sizeof(a));

   case_begin(s_sink, 2, 0);
   a.use_sink = 1;
   xfer_run(&a, "/start");
   check(a.done && !a.err && sink_len == 2 && !memcmp(sink_buf, "ok", 2),
         "302 with a body and a sink: only the final body reaches the sink");
   xfer_free(&a);
}

/* ---- A4: bounds ---- */

static char big_head[96 * 1024];

static void run_section_a4(void)
{
   struct xfer a, b;
   size_t off;
   int i;
   struct step s_big[1];
   static const struct step s_badchunk[] = {
      { "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nxyz\r\n", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   /* 16 KiB past a 64 KiB boundary, so the last read starts with less
    * than a window floor of body left. */
   static const struct step s_len[] = {
      { "HTTP/1.1 200 OK\r\nContent-Length: 1064960\r\n\r\n", 1064960, B_RAW, 0 },
   };
   static const struct step s_torn[] = {
      { OK_2, 0, B_NONE, 0 },
      { "HTTP/1.1 200 OK\r\n", 0, B_NONE, 1 },
      { OK_2, 0, B_NONE, 0 },
   };

   memset(&a, 0, sizeof(a));
   memset(&b, 0, sizeof(b));

   case_begin(s_badchunk, 2, 0);
   xfer_run(&a, "/badchunk");
   xfer_run(&b, "/next");
   check(a.done && a.err, "chunk size \"xyz\": error, not an empty success");
   check(b.done && body_is(&b, "ok") && !second_reused(),
         "chunk size \"xyz\": connection not pooled");
   xfer_free(&a); xfer_free(&b);

   /* ~72 KiB of header lines */
   off = (size_t)snprintf(big_head, sizeof(big_head), "HTTP/1.1 200 OK\r\n");
   for (i = 0; i < 70; i++)
   {
      off += (size_t)snprintf(big_head + off, sizeof(big_head) - off,
            "X-Pad-%02d: ", i);
      memset(big_head + off, 'a', 1000);
      off += 1000;
      memcpy(big_head + off, "\r\n", 2);
      off += 2;
   }
   memcpy(big_head + off, "Content-Length: 2\r\n\r\nok", 24);
   memset(s_big, 0, sizeof(s_big));
   s_big[0].head = big_head;
   case_begin(s_big, 1, 0);
   xfer_run(&a, "/bighead");
   check(a.done && a.err, "header block over 64 KiB: error");
   xfer_free(&a);

   /* ~90 KiB of 102 blocks: the cap counts interim blocks too */
   off = 0;
   while (off + 32 < sizeof(big_head) - 64)
   {
      memcpy(big_head + off, "HTTP/1.1 102 P\r\n\r\n", 18);
      off += 18;
   }
   memcpy(big_head + off, OK_2, sizeof(OK_2));
   case_begin(s_big, 1, 0);
   xfer_run(&a, "/flood");
   check(a.done && a.err, "endless 1xx blocks: error once past the header cap");
   xfer_free(&a);

   case_begin(s_len, 1, 0);
   realloc_max = 0;
   xfer_run(&a, "/len");
   check(a.done && body_is_pattern(&a, 1064960) && realloc_max <= 1064960,
         "GET 200 Content-Length: receive buffer never grows past the body");
   xfer_free(&a);

   /* A pooled connection that dies after the status line has already
    * given us part of a response: no replay. */
   case_begin(s_torn, 3, 0);
   xfer_run(&a, "/first");
   xfer_run(&b, "/torn");
   check(a.done && b.done && b.err && log_count() == 2,
         "pooled connection dying after the status line: not replayed");
   xfer_free(&a); xfer_free(&b);
}

/* ---- A5: scheme check ---- */

static void run_section_a5(void)
{
   /* Exactly sized, so a memcmp of the 7- or 8-byte scheme reads past
    * the allocation and ASan reports it. */
   char *u = (char*)malloc(3);
   struct http_connection_t *c;
   memcpy(u, "ht", 3);
   c = net_http_connection_new(u, "GET", NULL);
   check(c == NULL, "URL shorter than its scheme: rejected without reading past it");
   net_http_connection_free(c);
   free(u);
}

/* ---- B1: URL join and percent-decoding ---- */

static void join_case(const char *base, const char *ref, const char *want)
{
   char out[256], what[512];
   int  n = net_http_url_join(out, sizeof(out), base, ref);
   snprintf(what, sizeof(what), "url_join(\"%s\", \"%s\") = \"%s\"",
         base, ref ? ref : "(null)", want ? want : "(fail)");
   if (!want)
      check(n == -1 && out[0] == '\0', what);
   else
      check(n == (int)strlen(want) && !strcmp(out, want), what);
}

static void run_section_b1(void)
{
   const char *B = "http://h:8080/a/b/c?q=1#f";
   char buf[16];
   char guard[32];
   char *in;
   int n;

   join_case(B, "",               B);
   join_case(B, NULL,             B);
   join_case(B, "https://x/y",    "https://x/y");
   join_case(B, "HTTP://X/../y",  "HTTP://X/y");
   join_case(B, "//other:1/z",    "http://other:1/z");
   join_case(B, "/p?r",           "http://h:8080/p?r");
   join_case(B, "d",              "http://h:8080/a/b/d");
   join_case(B, "d?z#w",          "http://h:8080/a/b/d?z#w");
   join_case(B, "../d",           "http://h:8080/a/d");
   join_case(B, "../../../../d",  "http://h:8080/d");
   join_case(B, "./",             "http://h:8080/a/b/");
   join_case(B, "..",             "http://h:8080/a/");
   join_case(B, "?x=2",           "http://h:8080/a/b/c?x=2");
   join_case(B, "#g",             "http://h:8080/a/b/c?q=1#g");
   join_case("http://h",   "x",   "http://h/x");
   join_case("http://h?q", "x",   "http://h/x");
   join_case("https://h/dir/", "f%20g.zip", "https://h/dir/f%20g.zip");
   join_case(B, "mailto:x@y",     NULL);
   join_case("ftp://h/a", "b",    NULL);

   memset(guard, 'Z', sizeof(guard));
   n = net_http_url_join(guard, 8, "http://host/", "long/path");
   check(n == -1 && guard[0] == '\0' && guard[8] == 'Z',
         "url_join: too small a buffer fails without writing past it");

   n = net_http_urldecode(buf, sizeof(buf), "a%2Fb%20c");
   check(n == 5 && !strcmp(buf, "a/b c"), "urldecode: %2F and %20");
   n = net_http_urldecode(buf, sizeof(buf), "%zz%4+%41");
   check(n == 7 && !strcmp(buf, "%zz%4+A"), "urldecode: bad escapes and '+' copied as is");
   memset(guard, 'Z', sizeof(guard));
   n = net_http_urldecode(guard, 4, "abcdef");
   check(n == -1 && !strcmp(guard, "abc") && guard[4] == 'Z',
         "urldecode: truncates inside dst_size");

   in = (char*)malloc(16);
   memcpy(in, "%41%42%43%2", 12);
   n = net_http_urldecode_inplace(in);
   check(n == 5 && !strcmp(in, "ABC%2"), "urldecode_inplace: overlapping cursors");
   free(in);
}

/* ---- B2: redirects resolved through net_http_url_join ---- */

static void redirect_case(const char *from, const char *loc,
      const char *want_line, const char *want_host, const char *what)
{
   char head[256];
   struct step st[2];
   struct xfer a;
   struct reqlog r;

   snprintf(head, sizeof(head),
         "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n\r\n", loc);
   memset(st, 0, sizeof(st));
   memset(&a, 0, sizeof(a));
   st[0].head = head;
   st[1].head = OK_2;
   case_begin(st, 2, 0);
   xfer_run(&a, from);
   r = log_get(1);
   check(a.done && body_is(&a, "ok") && !strcmp(r.line, want_line)
         && (!want_host || !strcmp(r.host, want_host)), what);
   xfer_free(&a);
}

static void run_section_b2(void)
{
   char loc[128], host[64];

   redirect_case("/a/b/c", "../up",
         "GET /a/up HTTP/1.1", NULL,
         "302 Location ../up: dot segments resolved");
   redirect_case("/dir/page", "?q=1",
         "GET /dir/page?q=1 HTTP/1.1", NULL,
         "302 Location ?q=1: query replaces the base query on the same path");
   redirect_case("/x", "/y#frag",
         "GET /y HTTP/1.1", NULL,
         "302 Location with a fragment: fragment not sent");
   redirect_case("/x", "/files/My%20Game.zip",
         "GET /files/My%20Game.zip HTTP/1.1", NULL,
         "302 Location with %20: sent still encoded");
   snprintf(loc, sizeof(loc), "//localhost:%d/sr", srv_port);
   snprintf(host, sizeof(host), "localhost:%d", srv_port);
   redirect_case("/x", loc, "GET /sr HTTP/1.1", host,
         "302 Location //host:port/path: network-path reference changes host");
}

/* ---- header block ---- */

static char many_head[8192];

static void run_section_headers(void)
{
   struct xfer a;
   struct string_list *l;
   struct http_connection_t *conn;
   struct http_t *h;
   const char *line;
   unsigned long few, many;
   size_t off, i;
   int n;
   char url[128];
   struct step st[1];
   static const struct step s_one[] = {
      { "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok", 0, B_NONE, 0 },
   };
   static const struct step s_vals[] = {
      { "HTTP/1.1 200 OK\r\nX-Multi: a\r\nETag:   \"v1\"\r\nX-Multi: b\r\n"
        "X-Empty:\r\nContent-Length: 2\r\n\r\nok", 0, B_NONE, 0 },
   };

   memset(&a, 0, sizeof(a));

   case_begin(s_vals, 1, 0);
   xfer_run(&a, "/vals");
   check(a.done && a.headers
         && !strcmp(net_http_header_value(a.headers, "etag"), "\"v1\"")
         && !strcmp(net_http_header_value(a.headers, "X-Empty"), "")
         && !net_http_header_value(a.headers, "X-Mult")
         && !net_http_header_value(a.headers, "Missing"),
         "header_value: case-insensitive whole-name lookup, whitespace skipped");
   for (n = 0, line = net_http_header_next(a.headers, NULL); line;
         line = net_http_header_next(a.headers, line))
      n += !strncmp(line, "X-Multi:", 8);
   check(n == 2, "header_next: repeated fields all visited, in order");
   xfer_free(&a);

   /* Same request, 1 header line vs 64: the header block is one
    * allocation that doubles, so the count barely moves. A string_list
    * cost an allocation per line on top of its struct and array. */
   case_begin(s_one, 1, 0);
   allocs = 0;
   xfer_run(&a, "/few");
   few = __atomic_load_n(&allocs, __ATOMIC_RELAXED);
   xfer_free(&a);

   off = (size_t)snprintf(many_head, sizeof(many_head), "HTTP/1.1 200 OK\r\n");
   for (i = 0; i < 64; i++)
      off += (size_t)snprintf(many_head + off, sizeof(many_head) - off,
            "X-Header-%02u: value number %u\r\n", (unsigned)i, (unsigned)i);
   snprintf(many_head + off, sizeof(many_head) - off,
         "Content-Length: 2\r\n\r\nok");
   memset(st, 0, sizeof(st));
   st[0].head = many_head;
   case_begin(st, 1, 0);
   allocs = 0;
   xfer_run(&a, "/many");
   many = __atomic_load_n(&allocs, __ATOMIC_RELAXED);
   check(a.done && has_header(&a, "X-Header-63:") && many <= few + 3,
         "64 header lines cost no per-line allocations");
   xfer_free(&a);

   /* Legacy string_list accessor, for code outside RetroArch. */
   case_begin(s_vals, 1, 0);
   snprintf(url, sizeof(url), "http://127.0.0.1:%d/legacy", srv_port);
   conn = net_http_connection_new(url, "GET", NULL);
   net_http_connection_iterate(conn);
   net_http_connection_done(conn);
   h = net_http_new(conn);
   net_http_connection_free(conn);
   while (h && !net_http_update(h, NULL, NULL))
      net_http_wait(h, 20);
   l = net_http_headers_ex(h, false);
   check(l && l->size == 5 && !strcmp(l->elems[1].data, "ETag:   \"v1\""),
         "net_http_headers_ex: legacy string_list still built from the block");
   if (l)
      string_list_free(l);
   net_http_delete(h);
}

/* ---- interim responses do not frame the final one ---- */

static void run_section_interim_framing(void)
{
   struct xfer a;
   static const struct step s_te[] = {
      { "HTTP/1.1 100 Continue\r\nTransfer-Encoding: chunked\r\n\r\n"
        OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_cl[] = {
      { "HTTP/1.1 103 Early Hints\r\nContent-Length: 5\r\n\r\n"
        "HTTP/1.1 200 OK\r\n\r\nhello world", 0, B_NONE, 1 },
   };

   memset(&a, 0, sizeof(a));

   case_begin(s_te, 1, 0);
   xfer_run(&a, "/te");
   check(a.done && !a.err && body_is(&a, "ok"),
         "\"chunked\" on a 100 does not frame the final Content-Length body");
   xfer_free(&a);

   case_begin(s_cl, 1, 0);
   xfer_run(&a, "/cl");
   check(a.done && body_is(&a, "hello world"),
         "Content-Length on a 103 does not cut the final close-delimited body");
   xfer_free(&a);
}

/* ---- followed redirect bodies, and bytes past Content-Length ---- */

static void run_section_body_edges(void)
{
   struct xfer a, b;
   static const struct step s_big302[] = {
      { "HTTP/1.1 302 Found\r\nLocation: /final\r\nContent-Length: 8388608\r\n\r\n",
        8388608, B_RAW, 0 },
      { OK_2, 0, B_NONE, 0 },
   };
   static const struct step s_extra[] = {
      { "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nokEXTRA-BYTES", 0, B_NONE, 0 },
      { OK_2, 0, B_NONE, 0 },
   };

   memset(&a, 0, sizeof(a));
   memset(&b, 0, sizeof(b));

   /* A 302 with an 8 MiB body: read through and dropped, never held. */
   case_begin(s_big302, 2, 0);
   realloc_max = 0;
   xfer_run(&a, "/start");
   check(a.done && !a.err && body_is(&a, "ok") && realloc_max < 1048576,
         "followed 302 with an 8 MiB body: body dropped as it arrives, not buffered");
   check(second_reused(), "followed 302 with a body: socket reused for the redirect");
   xfer_free(&a);

   /* Content-Length 2, then more bytes: the body is complete. */
   case_begin(s_extra, 2, 0);
   xfer_run(&a, "/extra");
   xfer_run(&b, "/next");
   check(a.done && !a.err && a.status == 200 && body_is(&a, "ok"),
         "bytes past Content-Length: body kept, transfer succeeds");
   check(b.done && body_is(&b, "ok") && !second_reused(),
         "bytes past Content-Length: socket not returned to the pool");
   xfer_free(&a); xfer_free(&b);

   case_begin(s_extra, 2, 0);
   a.use_sink = 1;
   xfer_run(&a, "/extra");
   check(a.done && !a.err && sink_len == 2 && !memcmp(sink_buf, "ok", 2),
         "bytes past Content-Length with a sink: exactly the body reaches it");
   xfer_free(&a);
}

int main(void)
{
   if (!network_init() || srv_start())
   {
      fprintf(stderr, "cannot start loopback server\n");
      return 1;
   }
   net_http_init();

   run_section_baseline();
   run_section_a1();
   run_section_a2();
   run_section_a3();
   run_section_a4();
   run_section_a5();
   run_section_b1();
   run_section_b2();
   run_section_headers();
   run_section_interim_framing();
   run_section_body_edges();

   srv_shutdown();
   net_http_deinit();

   if (failures)
   {
      printf("%d check(s) failed\n", failures);
      return 1;
   }
   puts("net_http semantics: all checks passed");
   return 0;
}

/* WebDAV request headers of any length (task_push_webdav_move,
 * task_push_webdav_copy, task_push_webdav_put_stream).
 *
 * Drives the real task_http.c, net_http.c and task_queue.c against a
 * loopback server that records each request's header block.  A Digest
 * Authorization line carries the request URI, so a long path makes a
 * long header; MOVE and COPY put a Destination line ahead of it and PUT
 * an Expect line.  Every line must reach the server whole, however long
 * the caller's headers and the destination are.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_http.h>
#include <queues/task_queue.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include "../../../msg_hash.h"
#include "../../../tasks/tasks_internal.h"

#define NREQ     3
#define HEAD_MAX (64 * 1024)
/* Within what webdav.c can build: a destination under PATH_MAX_LENGTH,
 * and a Digest line whose uri= is a long save path. */
#define DEST_LEN 1900
#define AUTH_LEN 1500

static int failures;

/* What task_http.c needs from the rest of RetroArch */
const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return "msg"; }
void task_window_progress_cb(retro_task_t *task) { (void)task; }

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

/* Written by the server thread, read after it is joined. */
static char   g_head[NREQ][HEAD_MAX];
static char   g_body[NREQ][64];
static int    g_listen_fd = -1;
static int    g_port;

static void send_str(int fd, const char *s)
{
   socket_send_all_blocking(fd, s, strlen(s), true);
}

/* One request: its header block into g_head[idx], its body (when it
 * announces one) into g_body[idx]; answers 100 Continue to an Expect
 * and then 201 Created. */
static void serve_one(int cs, int idx)
{
   static char req[HEAD_MAX];
   size_t got = 0;
   char  *end = NULL;
   long   clen = 0;
   char  *cl;
   size_t have;

   while (got < sizeof(req) - 1)
   {
      int r = (int)recv(cs, req + got, (int)(sizeof(req) - 1 - got), 0);
      if (r <= 0)
         return;
      got     += (size_t)r;
      req[got] = '\0';
      if ((end = strstr(req, "\r\n\r\n")))
         break;
   }
   if (!end)
      return;

   memcpy(g_head[idx], req, (size_t)(end - req) + 2);
   g_head[idx][(end - req) + 2] = '\0';

   if ((cl = strstr(g_head[idx], "\r\nContent-Length: ")))
      clen = strtol(cl + STRLEN_CONST("\r\nContent-Length: "), NULL, 10);
   if (strstr(g_head[idx], "\r\nExpect: 100-continue\r\n"))
      send_str(cs, "HTTP/1.1 100 Continue\r\n\r\n");

   have = got - (size_t)(end + 4 - req);
   if (clen > 0 && clen < (long)sizeof(g_body[idx]))
   {
      memcpy(g_body[idx], end + 4, have);
      if (have < (size_t)clen)
         socket_receive_all_blocking(cs, g_body[idx] + have,
               (size_t)clen - have);
      g_body[idx][clen] = '\0';
   }

   send_str(cs, "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n"
         "Connection: close\r\n\r\n");
}

static void srv_main(void *data)
{
   int i;
   (void)data;
   for (i = 0; i < NREQ; i++)
   {
      int cs = (int)accept(g_listen_fd, NULL, NULL);
      if (cs < 0)
         break;
      serve_one(cs, i);
      socket_close(cs);
   }
}

static sthread_t *srv_start(void)
{
   struct sockaddr_in a;
   socklen_t          al = sizeof(a);

   g_listen_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
   memset(&a, 0, sizeof(a));
   a.sin_family      = AF_INET;
   a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (   g_listen_fd < 0
       || bind(g_listen_fd, (struct sockaddr*)&a, sizeof(a))
       || listen(g_listen_fd, 8)
       || getsockname(g_listen_fd, (struct sockaddr*)&a, &al))
      return NULL;
   g_port = ntohs(a.sin_port);
   return sthread_create(srv_main, NULL);
}

/* ---- task side ---- */

static int g_fired;
static int g_status;

static void done_cb(retro_task_t *task, void *task_data, void *user_data,
      const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   (void)task; (void)user_data; (void)err;
   g_fired  = 1;
   g_status = data ? data->status : 0;
}

static void finish(const char *label, void *t)
{
   int spins = 0;
   CHECK(t, "%s: push refused", label);
   if (!t)
      return;
   while (!g_fired && spins++ < 100000)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(g_fired, "%s: callback never ran", label);
   CHECK(g_status == 201, "%s: status %d", label, g_status);
   g_fired = 0;
}

static size_t g_put_off;
static const char g_put_body[] = "SAVEDATA";

static int64_t put_source(void *ud, void *buf, size_t len)
{
   size_t left = sizeof(g_put_body) - 1 - g_put_off;
   (void)ud;
   if (len > left)
      len = left;
   memcpy(buf, g_put_body + g_put_off, len);
   g_put_off += len;
   return (int64_t)len;
}

static bool put_rewind(void *ud)
{
   (void)ud;
   g_put_off = 0;
   return true;
}

/* @line, CRLF-terminated, is one of the lines of @head. */
static bool has_line(const char *head, const char *line)
{
   const char *p = strstr(head, line);
   return p && p > head && p[-1] == '\n';
}

int main(void)
{
   static char auth[AUTH_LEN + 64];
   static char dest[DEST_LEN + 64];
   static char line[DEST_LEN + 128];
   char        url[64];
   sthread_t  *srv;
   size_t      n;

   network_init();
   net_http_init();
   task_queue_init(true, NULL);

   if (!(srv = srv_start()))
   {
      printf("FAIL: loopback server\n");
      return 1;
   }
   snprintf(url, sizeof(url), "http://127.0.0.1:%d/dav/a.srm", g_port);

   n = (size_t)snprintf(auth, sizeof(auth),
         "Authorization: Digest username=\"u\", uri=\"/dav/");
   memset(auth + n, 'p', AUTH_LEN);
   strlcpy(auth + n + AUTH_LEN, "\", response=\"0\"\r\n",
         sizeof(auth) - n - AUTH_LEN);
   n = (size_t)snprintf(dest, sizeof(dest), "http://127.0.0.1:%d/dav/", g_port);
   memset(dest + n, 'd', DEST_LEN);
   dest[n + DEST_LEN] = '\0';

   finish("MOVE", task_push_webdav_move(url, dest, true, auth,
            done_cb, NULL));
   finish("COPY", task_push_webdav_copy(url, dest, true, auth,
            done_cb, NULL));
   finish("PUT", task_push_webdav_put_stream(url, put_source, put_rewind,
            NULL, sizeof(g_put_body) - 1, true, auth, done_cb, NULL));

   sthread_join(srv);
   socket_close(g_listen_fd);
   task_queue_deinit();
   net_http_deinit();

   snprintf(line, sizeof(line), "Destination: %s\r\n", dest);
   CHECK(!strncmp(g_head[0], "MOVE ", 5), "MOVE: method");
   CHECK(has_line(g_head[0], line), "MOVE: Destination cut short");
   CHECK(has_line(g_head[0], auth), "MOVE: Authorization cut short");
   CHECK(!strncmp(g_head[1], "COPY ", 5), "COPY: method");
   CHECK(has_line(g_head[1], line), "COPY: Destination cut short");
   CHECK(has_line(g_head[1], auth), "COPY: Authorization cut short");
   CHECK(!strncmp(g_head[2], "PUT ", 4), "PUT: method");
   CHECK(has_line(g_head[2], "Expect: 100-continue\r\n"), "PUT: Expect");
   CHECK(has_line(g_head[2], auth), "PUT: Authorization cut short");
   CHECK(!strcmp(g_body[2], g_put_body), "PUT: body \"%s\"", g_body[2]);

   if (failures)
   {
      printf("\n%d check(s) failed\n", failures);
      return 1;
   }
   printf("all WebDAV request header checks passed\n");
   return 0;
}

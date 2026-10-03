/* Resume of an interrupted download to a file (task_push_http_download_file).
 *
 * Drives the real task_http.c, net_http.c and task_queue.c against a
 * scripted loopback server.  The first response advertises byte ranges
 * and a strong ETag, then drops the connection part way through the
 * body; what the task must do next depends on what the server says to
 * the resume request:
 *
 *   206 from the right offset   -> the file continues, the task succeeds
 *                                  with status 200 and the whole file;
 *   200 (resource changed)      -> the file starts over with the new body;
 *   206 from the wrong offset   -> failure, partial file removed;
 *   no Accept-Ranges/validator  -> no resume at all (the old behaviour);
 *   drops again and again       -> at most three resumes, then failure.
 *
 * Before resume existed, every interrupted download failed outright and
 * the partial file was deleted. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/net_http.h>
#include <net/net_compat.h>
#include <queues/task_queue.h>

#include "../../../tasks/tasks_internal.h"
#include "../../../tasks/task_file_transfer.h"

#define BODY_LEN (1024u * 1024u)

static int failures;
static int checks;

#define CHECK(cond, ...) \
   do { \
      checks++; \
      if (!(cond)) \
      { \
         printf("    FAIL: "); printf(__VA_ARGS__); printf("\n"); \
         failures++; \
      } \
   } while (0)

const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return "msg"; }
void task_window_progress_cb(retro_task_t *task) { (void)task; }

static unsigned char pat(size_t i, int seed)
{
   return (unsigned char)((i * 7u + (unsigned)seed * 13u) & 0xff);
}

/* ---- scripted server ---- */

enum step_kind
{
   S_FULL_DROP = 0,     /* 200 + ranges + ETag "v1", body cut at .cut */
   S_FULL_DROP_BARE,    /* same, no Accept-Ranges and no validator */
   S_PARTIAL,           /* 206 from the requested offset to the end */
   S_PARTIAL_DROP,      /* 206 from the requested offset, cut at .cut */
   S_PARTIAL_WRONG,     /* 206 claiming to start 7 bytes later */
   S_FULL_NEW           /* 200, the whole resource, changed (seed 2) */
};

struct step { enum step_kind kind; size_t cut; };

#define MAX_REQS 8
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static const struct step *g_script;
static int    g_nsteps;
static int    g_nreqs;
static char   g_range[MAX_REQS][64];
static char   g_ifrange[MAX_REQS][64];
static int    g_listen_fd = -1, g_port;
static int    g_stop;
static pthread_t g_th;

static void send_all(int fd, const void *p, size_t n)
{
   const char *c = (const char*)p;
   while (n)
   {
      ssize_t w = send(fd, c, n, MSG_NOSIGNAL);
      if (w <= 0)
         return;
      c += w;
      n -= (size_t)w;
   }
}

static void send_body(int fd, size_t from, size_t to, int seed)
{
   static unsigned char blk[65536];
   while (from < to)
   {
      size_t i, n = to - from;
      if (n > sizeof(blk))
         n = sizeof(blk);
      for (i = 0; i < n; i++)
         blk[i] = pat(from + i, seed);
      send_all(fd, blk, n);
      from += n;
   }
}

static void header_value(const char *req, const char *name, char *out, size_t size)
{
   const char *p = req;
   size_t      n = strlen(name);
   out[0] = '\0';
   while ((p = strstr(p, "\r\n")))
   {
      p += 2;
      if (!strncasecmp(p, name, n) && p[n] == ':')
      {
         const char *v = p + n + 1, *e;
         while (*v == ' ')
            v++;
         e = strstr(v, "\r\n");
         if (e && (size_t)(e - v) < size)
         {
            memcpy(out, v, (size_t)(e - v));
            out[e - v] = '\0';
         }
         return;
      }
   }
}

static void serve_one(int cs)
{
   char   req[4096];
   char   head[512];
   size_t got = 0, from = 0;
   const struct step *st = NULL;
   int    idx;

   while (got < sizeof(req) - 1)
   {
      ssize_t r = recv(cs, req + got, sizeof(req) - 1 - got, 0);
      if (r <= 0)
         return;
      got += (size_t)r;
      req[got] = '\0';
      if (strstr(req, "\r\n\r\n"))
         break;
   }

   pthread_mutex_lock(&g_lock);
   idx = g_nreqs++;
   if (idx < MAX_REQS)
   {
      header_value(req, "Range", g_range[idx], sizeof(g_range[idx]));
      header_value(req, "If-Range", g_ifrange[idx], sizeof(g_ifrange[idx]));
      if (!strncmp(g_range[idx], "bytes=", 6))
         from = strtoul(g_range[idx] + 6, NULL, 10);
   }
   if (idx < g_nsteps)
      st = &g_script[idx];
   pthread_mutex_unlock(&g_lock);

   if (!st)
      return;

   switch (st->kind)
   {
      case S_FULL_DROP:
      case S_FULL_DROP_BARE:
         snprintf(head, sizeof(head),
               "HTTP/1.1 200 OK\r\nContent-Length: %u\r\n%s"
               "Connection: close\r\n\r\n", BODY_LEN,
               st->kind == S_FULL_DROP
               ? "Accept-Ranges: bytes\r\nETag: \"v1\"\r\n" : "");
         send_all(cs, head, strlen(head));
         send_body(cs, 0, st->cut, 1);
         break;
      case S_PARTIAL:
      case S_PARTIAL_DROP:
      case S_PARTIAL_WRONG:
      {
         size_t claim = from + (st->kind == S_PARTIAL_WRONG ? 7 : 0);
         snprintf(head, sizeof(head),
               "HTTP/1.1 206 Partial Content\r\nContent-Length: %lu\r\n"
               "Content-Range: bytes %lu-%u/%u\r\nAccept-Ranges: bytes\r\n"
               "ETag: \"v1\"\r\nConnection: close\r\n\r\n",
               (unsigned long)(BODY_LEN - claim), (unsigned long)claim,
               BODY_LEN - 1, BODY_LEN);
         send_all(cs, head, strlen(head));
         send_body(cs, claim, st->kind == S_PARTIAL_DROP ? st->cut : BODY_LEN, 1);
         break;
      }
      case S_FULL_NEW:
         snprintf(head, sizeof(head),
               "HTTP/1.1 200 OK\r\nContent-Length: %u\r\nAccept-Ranges: bytes\r\n"
               "ETag: \"v2\"\r\nConnection: close\r\n\r\n", BODY_LEN);
         send_all(cs, head, strlen(head));
         send_body(cs, 0, BODY_LEN, 2);
         break;
   }
}

static void *srv_main(void *a)
{
   (void)a;
   for (;;)
   {
      int cs, stop;
      pthread_mutex_lock(&g_lock);
      stop = g_stop;
      pthread_mutex_unlock(&g_lock);
      if (stop)
         break;
      if ((cs = accept(g_listen_fd, NULL, NULL)) < 0)
         continue;
      serve_one(cs);
      close(cs);
   }
   return NULL;
}

static int srv_start(void)
{
   struct sockaddr_in a;
   socklen_t al = sizeof(a);
   int one = 1;
   struct timeval tv = { 0, 100000 };

   g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
   setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
   /* accept() wakes every 100 ms so shutdown is noticed */
   setsockopt(g_listen_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
   memset(&a, 0, sizeof(a));
   a.sin_family      = AF_INET;
   a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (   bind(g_listen_fd, (struct sockaddr*)&a, sizeof(a))
       || listen(g_listen_fd, 8)
       || getsockname(g_listen_fd, (struct sockaddr*)&a, &al))
      return 0;
   g_port = ntohs(a.sin_port);
   return pthread_create(&g_th, NULL, srv_main, NULL) == 0;
}

/* ---- task side ---- */

struct obs { int fired; int had_error; int status; };
static struct obs g_obs;
static char g_path[128];

static void done_cb(retro_task_t *task, void *task_data, void *user_data,
      const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   (void)task; (void)user_data;
   pthread_mutex_lock(&g_lock);
   g_obs.fired     = 1;
   g_obs.had_error = (err && *err);
   g_obs.status    = data ? data->status : 0;
   pthread_mutex_unlock(&g_lock);
}

/* Bytes of the file at g_path, or -1 if it does not exist; checks the
 * content against pattern @seed. */
static long file_matches(int seed, int *ok)
{
   FILE *f = fopen(g_path, "rb");
   long  n = 0;
   int   c;
   *ok = 1;
   if (!f)
      return -1;
   while ((c = fgetc(f)) != EOF)
   {
      if ((unsigned char)c != pat((size_t)n, seed))
         *ok = 0;
      n++;
   }
   fclose(f);
   return n;
}

static void run(const char *label, const struct step *script, int n)
{
   char url[128];
   int  spins = 0;

   printf("  %s\n", label);
   pthread_mutex_lock(&g_lock);
   g_script = script;
   g_nsteps = n;
   g_nreqs  = 0;
   memset(g_range, 0, sizeof(g_range));
   memset(g_ifrange, 0, sizeof(g_ifrange));
   memset(&g_obs, 0, sizeof(g_obs));
   pthread_mutex_unlock(&g_lock);

   net_http_deinit();
   net_http_init();
   snprintf(url, sizeof(url), "http://127.0.0.1:%d/file.bin", g_port);
   if (!task_push_http_download_file(url, g_path, true, NULL, done_cb, NULL))
   {
      CHECK(0, "%s: push refused", label);
      return;
   }
   while (spins++ < 100000)
   {
      int fired;
      task_queue_check();
      pthread_mutex_lock(&g_lock);
      fired = g_obs.fired;
      pthread_mutex_unlock(&g_lock);
      if (fired)
         break;
      usleep(200);
   }
   task_queue_wait(NULL, NULL);
   task_queue_check();
   CHECK(g_obs.fired, "%s: callback never ran", label);
}

static int nreqs(void)
{
   int n;
   pthread_mutex_lock(&g_lock);
   n = g_nreqs;
   pthread_mutex_unlock(&g_lock);
   return n;
}

int main(void)
{
   int ok;
   long len;
   static const struct step s_resume[] = {
      { S_FULL_DROP, 300000 }, { S_PARTIAL, 0 } };
   static const struct step s_twice[] = {
      { S_FULL_DROP, 200000 }, { S_PARTIAL_DROP, 600000 }, { S_PARTIAL, 0 } };
   static const struct step s_changed[] = {
      { S_FULL_DROP, 300000 }, { S_FULL_NEW, 0 } };
   static const struct step s_wrong[] = {
      { S_FULL_DROP, 300000 }, { S_PARTIAL_WRONG, 0 } };
   static const struct step s_bare[] = {
      { S_FULL_DROP_BARE, 300000 }, { S_PARTIAL, 0 } };
   static const struct step s_flaky[] = {
      { S_FULL_DROP, 100000 }, { S_PARTIAL_DROP, 200000 },
      { S_PARTIAL_DROP, 300000 }, { S_PARTIAL_DROP, 400000 },
      { S_PARTIAL, 0 } };

   printf("task_http download resume\n\n");
   snprintf(g_path, sizeof(g_path), "/tmp/http_resume_test_%ld.bin", (long)getpid());
   network_init();
   net_http_init();
   if (!srv_start())
   {
      printf("cannot start loopback server\n");
      return 1;
   }
   task_queue_init(true, NULL);

   run("dropped at 300000, resumed with a 206", s_resume, 2);
   len = file_matches(1, &ok);
   CHECK(!g_obs.had_error && g_obs.status == 200,
         "resume: err=%d status=%d, expected success with 200",
         g_obs.had_error, g_obs.status);
   CHECK(len == (long)BODY_LEN && ok, "resume: file is %ld bytes, %s",
         len, ok ? "content ok" : "content wrong");
   CHECK(nreqs() == 2 && !strcmp(g_range[1], "bytes=300000-")
         && !strcmp(g_ifrange[1], "\"v1\""),
         "resume: second request Range '%s' If-Range '%s'",
         g_range[1], g_ifrange[1]);
   remove(g_path);

   run("dropped twice, resumed twice", s_twice, 3);
   len = file_matches(1, &ok);
   CHECK(!g_obs.had_error && len == (long)BODY_LEN && ok && nreqs() == 3
         && !strcmp(g_range[1], "bytes=200000-")
         && !strcmp(g_range[2], "bytes=600000-"),
         "two resumes: err=%d len=%ld ok=%d requests=%d ranges '%s' '%s'",
         g_obs.had_error, len, ok, nreqs(), g_range[1], g_range[2]);
   remove(g_path);

   run("resource changed: 200 to the resume", s_changed, 2);
   len = file_matches(2, &ok);
   CHECK(!g_obs.had_error && g_obs.status == 200 && len == (long)BODY_LEN && ok,
         "changed resource: err=%d status=%d len=%ld ok=%d (file must be the "
         "new body alone, not spliced)", g_obs.had_error, g_obs.status, len, ok);
   remove(g_path);

   run("206 from the wrong offset", s_wrong, 2);
   CHECK(g_obs.had_error && file_matches(1, &ok) < 0,
         "wrong Content-Range: err=%d, partial file %s", g_obs.had_error,
         file_matches(1, &ok) < 0 ? "removed" : "left behind");

   run("no Accept-Ranges, no validator", s_bare, 2);
   CHECK(g_obs.had_error && nreqs() == 1 && file_matches(1, &ok) < 0,
         "no validator: err=%d requests=%d (no resume may be tried)",
         g_obs.had_error, nreqs());

   run("drops on every attempt", s_flaky, 5);
   CHECK(g_obs.had_error && nreqs() == 4 && file_matches(1, &ok) < 0,
         "flaky: err=%d requests=%d, expected failure after 3 resumes",
         g_obs.had_error, nreqs());

   task_queue_deinit();
   pthread_mutex_lock(&g_lock);
   g_stop = 1;
   pthread_mutex_unlock(&g_lock);
   pthread_join(g_th, NULL);
   close(g_listen_fd);
   net_http_deinit();
   remove(g_path);

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}

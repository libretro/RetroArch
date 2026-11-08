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
 * the partial file was deleted.
 *
 * A file already at the path is replaced only by a download that
 * succeeds: a 404, a dropped or refused connection, a cancel and a
 * storage that cannot rename over it leave it as it was, and no
 * temporary file behind. */

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
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

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
   S_FULL_NEW,          /* 200, the whole resource, changed (seed 2) */
   S_NOT_FOUND,         /* 404 */
   S_HOLD               /* 200, body up to .cut, then waits for g_release */
};

struct step { enum step_kind kind; size_t cut; };

#define MAX_REQS 8
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static const struct step *g_script;
static int    g_nsteps;
static int    g_nreqs;
static char   g_range[MAX_REQS][64];
static char   g_ifrange[MAX_REQS][64];
static char   g_accenc[MAX_REQS][64];
static int    g_listen_fd = -1, g_port;
static int    g_stop;
static int    g_held, g_release;
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
      header_value(req, "Accept-Encoding", g_accenc[idx], sizeof(g_accenc[idx]));
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
      case S_NOT_FOUND:
         snprintf(head, sizeof(head), "HTTP/1.1 404 Not Found\r\n"
               "Content-Length: 9\r\nConnection: close\r\n\r\nnot found");
         send_all(cs, head, strlen(head));
         break;
      case S_HOLD:
         snprintf(head, sizeof(head),
               "HTTP/1.1 200 OK\r\nContent-Length: %u\r\n"
               "Connection: close\r\n\r\n", BODY_LEN);
         send_all(cs, head, strlen(head));
         send_body(cs, 0, st->cut, 1);
         pthread_mutex_lock(&g_lock);
         g_held = 1;
         while (!g_release)
         {
            pthread_mutex_unlock(&g_lock);
            usleep(1000);
            pthread_mutex_lock(&g_lock);
         }
         pthread_mutex_unlock(&g_lock);
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

struct obs { int fired; int had_error; int status; char err[128]; };
static struct obs g_obs;
static char g_path[128];
static char g_temp[160];
static const char g_seed[] = "art the user already had\n";

static void done_cb(retro_task_t *task, void *task_data, void *user_data,
      const char *err)
{
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   (void)task; (void)user_data;
   pthread_mutex_lock(&g_lock);
   g_obs.fired     = 1;
   g_obs.had_error = (err && *err);
   g_obs.status    = data ? data->status : 0;
   snprintf(g_obs.err, sizeof(g_obs.err), "%s", err ? err : "");
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

static void *push(const char *label, const struct step *script, int n,
      int port)
{
   char  url[128];
   void *t;

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
   snprintf(url, sizeof(url), "http://127.0.0.1:%d/file.bin", port);
   if (!(t = task_push_http_download_file(url, g_path, true, NULL, done_cb, NULL)))
      CHECK(0, "%s: push refused", label);
   return t;
}

static void wait_done(const char *label)
{
   int spins = 0;
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

static void run(const char *label, const struct step *script, int n)
{
   if (push(label, script, n, g_port))
      wait_done(label);
}

static void seed_target(void)
{
   FILE *f = fopen(g_path, "wb");
   if (f)
   {
      fwrite(g_seed, 1, sizeof(g_seed) - 1, f);
      fclose(f);
   }
}

/* g_path holds exactly the bytes seed_target() wrote */
static int seed_intact(void)
{
   char   buf[sizeof(g_seed)];
   size_t n;
   FILE  *f = fopen(g_path, "rb");
   if (!f)
      return 0;
   n = fread(buf, 1, sizeof(buf), f);
   fclose(f);
   return n == sizeof(g_seed) - 1 && !memcmp(buf, g_seed, n);
}

/* A failed download over an existing file: the file is unchanged right
 * after the push and after the failure. */
static void run_seeded_fail(const char *label, const struct step *script,
      int n, int port)
{
   int ok;
   seed_target();
   if (!push(label, script, n, port))
      return;
   CHECK(seed_intact(), "%s: existing file is %ld bytes after the push",
         label, file_matches(0, &ok));
   wait_done(label);
   CHECK(g_obs.had_error, "%s: no error reported", label);
   CHECK(seed_intact() && !path_is_valid(g_temp),
         "%s: existing file %s, temporary file %s", label,
         seed_intact() ? "kept" : "lost",
         path_is_valid(g_temp) ? "left behind" : "removed");
   remove(g_path);
}

static int RETRO_CALLCONV refuse_rename(const char *old_path,
      const char *new_path)
{
   (void)old_path;
   (void)new_path;
   return -1;
}

/* A refused frontend rename is retried after removing the file;
 * refusing that too leaves the file in place, as a refused built-in
 * rename does (Android SAF). */
static int RETRO_CALLCONV keep_target(const char *path)
{
   if (!strcmp(path, g_path))
      return -1;
   return remove(path) == 0 ? 0 : -1;
}

/* Writes to g_path stop short */
static int64_t RETRO_CALLCONV short_write(struct retro_vfs_file_handle *stream,
      const void *s, uint64_t len)
{
   libretro_vfs_implementation_file *f =
      (libretro_vfs_implementation_file*)stream;
   const char *path = retro_vfs_file_get_path_impl(f);
   if (path && !strcmp(path, g_path) && len > 1000)
      len = 1000;
   return retro_vfs_file_write_impl(f, s, len);
}

static void set_vfs(struct retro_vfs_interface *vfs)
{
   struct retro_vfs_interface_info info;
   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                      = vfs;
   filestream_vfs_init(&info);
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
   static const struct step s_full[]      = { { S_FULL_NEW, 0 } };
   static const struct step s_not_found[] = { { S_NOT_FOUND, 0 } };
   static const struct step s_hold[]      = { { S_HOLD, 200000 } };
   struct retro_vfs_interface      vfs;
   struct sockaddr_in a;
   socklen_t al = sizeof(a);
   int   dead_fd, dead_port = 0;
   void *t;
   FILE *f;

   printf("task_http download resume\n\n");
   snprintf(g_path, sizeof(g_path), "/tmp/http_resume_test_%ld.bin", (long)getpid());
   filestream_atomic_temp_name(g_temp, sizeof(g_temp), g_path);
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
   /* The body is written as received, so no content coding may be
    * accepted on either the first request or the resumed one. */
   CHECK(!strcmp(g_accenc[0], "identity") && !strcmp(g_accenc[1], "identity"),
         "resume: Accept-Encoding '%s' then '%s', expected identity",
         g_accenc[0], g_accenc[1]);
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
   CHECK(g_obs.had_error && file_matches(1, &ok) < 0 && !path_is_valid(g_temp),
         "wrong Content-Range: err=%d, partial file %s", g_obs.had_error,
         file_matches(1, &ok) < 0 && !path_is_valid(g_temp)
         ? "removed" : "left behind");

   run("no Accept-Ranges, no validator", s_bare, 2);
   CHECK(g_obs.had_error && nreqs() == 1 && file_matches(1, &ok) < 0
         && !path_is_valid(g_temp),
         "no validator: err=%d requests=%d (no resume may be tried)",
         g_obs.had_error, nreqs());

   run("drops on every attempt", s_flaky, 5);
   CHECK(g_obs.had_error && nreqs() == 4 && file_matches(1, &ok) < 0
         && !path_is_valid(g_temp),
         "flaky: err=%d requests=%d, expected failure after 3 resumes",
         g_obs.had_error, nreqs());

   run_seeded_fail("existing file, 404", s_not_found, 1, g_port);
   CHECK(strstr(g_obs.err, "404") != NULL,
         "404: error '%s' does not say why", g_obs.err);

   run_seeded_fail("existing file, dropped without resume", s_bare, 2, g_port);

   /* Bound but not listening: a connect is refused */
   dead_fd = socket(AF_INET, SOCK_STREAM, 0);
   memset(&a, 0, sizeof(a));
   a.sin_family      = AF_INET;
   a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (   !bind(dead_fd, (struct sockaddr*)&a, sizeof(a))
       && !getsockname(dead_fd, (struct sockaddr*)&a, &al))
      dead_port = ntohs(a.sin_port);
   CHECK(dead_port != 0, "cannot reserve a refusing port");
   if (dead_port)
      run_seeded_fail("existing file, connection refused", NULL, 0, dead_port);
   close(dead_fd);

   seed_target();
   if ((t = push("existing file, cancelled mid-download", s_hold, 1, g_port)))
   {
      int spins = 0, held = 0;
      CHECK(seed_intact(), "cancel: existing file is %ld bytes after the push",
            file_matches(0, &ok));
      while (!held && spins++ < 100000)
      {
         task_queue_check();
         pthread_mutex_lock(&g_lock);
         held = g_held;
         pthread_mutex_unlock(&g_lock);
         if (!held)
            usleep(200);
      }
      CHECK(held, "cancel: server never sent the first part");
      CHECK(seed_intact(), "cancel: existing file is %ld bytes mid-download",
            file_matches(0, &ok));
      task_queue_cancel_task(t);
      wait_done("cancel");
      CHECK(g_obs.had_error && seed_intact() && !path_is_valid(g_temp),
            "cancel: err=%d, existing file %s, temporary file %s",
            g_obs.had_error, seed_intact() ? "kept" : "lost",
            path_is_valid(g_temp) ? "left behind" : "removed");
   }
   pthread_mutex_lock(&g_lock);
   g_release = 1;
   pthread_mutex_unlock(&g_lock);
   remove(g_path);

   seed_target();
   run("existing file, replaced by a download", s_full, 1);
   len = file_matches(2, &ok);
   CHECK(!g_obs.had_error && len == (long)BODY_LEN && ok && !path_is_valid(g_temp),
         "replace: err=%d len=%ld ok=%d temporary file %s", g_obs.had_error,
         len, ok, path_is_valid(g_temp) ? "left behind" : "removed");
   remove(g_path);

   seed_target();
   run("existing file, replaced by a resumed download", s_resume, 2);
   len = file_matches(1, &ok);
   CHECK(!g_obs.had_error && len == (long)BODY_LEN && ok && !path_is_valid(g_temp),
         "replace by resume: err=%d len=%ld ok=%d temporary file %s",
         g_obs.had_error, len, ok,
         path_is_valid(g_temp) ? "left behind" : "removed");
   remove(g_path);

   /* A temporary file a crash left behind is written over */
   if ((f = fopen(g_temp, "wb")))
   {
      fputs("left by a crash", f);
      fclose(f);
   }
   seed_target();
   run("existing file, leftover temporary file", s_full, 1);
   len = file_matches(2, &ok);
   CHECK(!g_obs.had_error && len == (long)BODY_LEN && ok && !path_is_valid(g_temp),
         "leftover temporary file: err=%d len=%ld ok=%d temporary file %s",
         g_obs.had_error, len, ok,
         path_is_valid(g_temp) ? "left behind" : "removed");
   remove(g_temp);
   remove(g_path);

   /* A VFS that cannot rename over the file (Android SAF): copying
    * would truncate the file first, so it is kept */
   memset(&vfs, 0, sizeof(vfs));
   vfs.rename = refuse_rename;
   vfs.remove = keep_target;
   set_vfs(&vfs);
   seed_target();
   run("existing file, rename refused", s_full, 1);
   set_vfs(NULL);
   CHECK(g_obs.had_error && strstr(g_obs.err, "replace") && seed_intact()
         && !path_is_valid(g_temp),
         "rename refused: err=\"%s\", existing file %s, temporary file %s",
         g_obs.err, seed_intact() ? "kept" : "lost",
         path_is_valid(g_temp) ? "left behind" : "removed");
   remove(g_path);

   /* No file to lose: copied */
   set_vfs(&vfs);
   run("no existing file, rename refused", s_full, 1);
   set_vfs(NULL);
   len = file_matches(2, &ok);
   CHECK(!g_obs.had_error && len == (long)BODY_LEN && ok && !path_is_valid(g_temp),
         "rename refused, copied: err=%d len=%ld ok=%d temporary file %s",
         g_obs.had_error, len, ok,
         path_is_valid(g_temp) ? "left behind" : "removed");
   remove(g_path);

   /* The copy fails part way */
   memset(&vfs, 0, sizeof(vfs));
   vfs.rename = refuse_rename;
   vfs.write  = short_write;
   set_vfs(&vfs);
   run("no existing file, rename refused, copy fails", s_full, 1);
   set_vfs(NULL);
   CHECK(g_obs.had_error && file_matches(2, &ok) < 0 && !path_is_valid(g_temp),
         "copy failed: err=%d, file is %ld bytes, temporary file %s",
         g_obs.had_error, file_matches(2, &ok),
         path_is_valid(g_temp) ? "left behind" : "removed");
   remove(g_path);

   /* The temporary name cannot be opened (a directory holds it) */
   path_mkdir(g_temp);
   run("no existing file, temporary file unavailable", s_full, 1);
   len = file_matches(2, &ok);
   CHECK(!g_obs.had_error && len == (long)BODY_LEN && ok,
         "no temporary file: err=%d len=%ld ok=%d, expected a direct write",
         g_obs.had_error, len, ok);
   remove(g_path);
   seed_target();
   run("existing file, temporary file unavailable", s_full, 1);
   CHECK(g_obs.had_error && seed_intact() && path_is_directory(g_temp),
         "no temporary file: err=%d, existing file %s", g_obs.had_error,
         seed_intact() ? "kept" : "lost");
   path_rmdir(g_temp);
   remove(g_path);

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

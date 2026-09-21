/* Harness for the WebDAV Digest re-authentication paths.
 *
 * Every WebDAV request callback retries when the server answers 401
 * with a Digest challenge, rebuilding the Authorization header from
 * the new nonce.  That is right once: the nonce went stale, or the
 * request was the first to meet the challenge.  It is wrong forever:
 * with a wrong password the server answers every retry with another
 * challenge, and the driver used to retry every one of them, so a
 * mistyped password turned the first request of a sync into an
 * endless request loop that never reported anything.
 *
 * The test plays the server.  Each request the driver starts is
 * recorded by the stubs, and the test answers it with the response it
 * wants.  For each request type (OPTIONS, GET, PUT, DELETE, MOVE,
 * MKCOL) it checks that a server which keeps rejecting the credentials
 * gets exactly one retry and then a reported failure, and that a
 * single stale-nonce challenge is still retried and then succeeds.
 *
 * It also checks what a retried PUT sends.  The first attempt reads
 * the upload into memory and leaves the file at its end; the retry
 * read it again from there, got nothing, and sent a buffer of the
 * right length full of uninitialised memory - replacing the save on
 * the server with garbage whenever a nonce expired mid-upload.
 *
 * Includes the driver's translation unit so the paths are tested as
 * shipped rather than as a copy that can drift from them.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>

#include "../../../network/cloud_sync/webdav.c"

#include "stubs_retroarch.h"

/* Enough to catch a loop; a correct driver stops after one. */
#define MAX_ROUNDS 10

static unsigned failures = 0;

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

static const char *digest_header =
   "WWW-Authenticate: Digest realm=\"dav\", nonce=\"6f2a\", "
   "qop=\"auth\", algorithm=MD5";

typedef struct
{
   unsigned calls;
   bool     success;
   RFILE   *file;
} done_t;

static void on_done(void *user_data, const char *path, bool success,
      RFILE *file)
{
   done_t *done = (done_t*)user_data;
   (void)path;
   done->calls++;
   done->success = success;
   done->file    = file;
}

static webdav_cb_state_t *new_cb_state(const char *path, const char *file,
      done_t *done)
{
   webdav_cb_state_t *st = (webdav_cb_state_t*)calloc(1, sizeof(*st));
   strlcpy(st->path, path, sizeof(st->path));
   if (file)
      strlcpy(st->file, file, sizeof(st->file));
   st->cb        = on_done;
   st->user_data = done;
   return st;
}

static void make_response(http_transfer_data_t *resp, int status,
      struct string_list *headers, char *body, size_t len)
{
   memset(resp, 0, sizeof(*resp));
   resp->status  = status;
   resp->headers = headers;
   resp->data    = body;
   resp->len     = len;
}

/* Answers the request the driver just started with @resp, over and
 * over, and returns how many retries it made.  A driver still
 * retrying after MAX_ROUNDS is stopped with a 500 so the harness stays
 * leak-clean either way. */
static unsigned answer_every_retry(http_transfer_data_t *resp)
{
   unsigned retries = 0;

   while (stub_req.count && retries < MAX_ROUNDS)
   {
      retro_task_callback_t cb = stub_req.cb;
      void *user_data          = stub_req.user_data;
      retries++;
      stub_req.count           = 0;
      cb(NULL, resp, user_data, NULL);
   }

   if (stub_req.count)
   {
      http_transfer_data_t stop;
      make_response(&stop, 500, NULL, NULL, 0);
      stub_req.count = 0;
      stub_req.cb(NULL, &stop, stub_req.user_data, NULL);
   }

   return retries;
}

/* The first response to a request is fed in by hand (the request
 * itself went out before the harness took over); the rest go through
 * answer_every_retry(). */
static unsigned reject_credentials(retro_task_callback_t cb, void *st,
      http_transfer_data_t *challenge)
{
   stub_reset();
   cb(NULL, challenge, st, NULL);
   return answer_every_retry(challenge);
}

static void check_gives_up(const char *what, unsigned retries,
      const done_t *done)
{
   CHECK(retries == 1,
         "%s: wrong password must be retried once, was retried %u time(s)%s",
         what, retries, retries >= MAX_ROUNDS ? " (looping)" : "");
   CHECK(done->calls == 1,
         "%s: completion must be reported once, was reported %u time(s)",
         what, done->calls);
   CHECK(!done->success, "%s: rejected credentials must fail", what);
}

static void test_options(http_transfer_data_t *challenge)
{
   done_t done = {0};
   unsigned retries = reject_credentials(webdav_stat_cb,
         new_cb_state("", NULL, &done), challenge);
   check_gives_up("OPTIONS", retries, &done);
}

static void test_get(http_transfer_data_t *challenge, const char *tmp)
{
   done_t done = {0};
   unsigned retries = reject_credentials(webdav_read_cb,
         new_cb_state("saves/a.srm", tmp, &done), challenge);
   check_gives_up("GET", retries, &done);
   if (done.file)
      filestream_close(done.file);
}

static void test_delete(http_transfer_data_t *challenge)
{
   done_t done = {0};
   unsigned retries = reject_credentials(webdav_delete_cb,
         new_cb_state("saves/a.srm", NULL, &done), challenge);
   check_gives_up("DELETE", retries, &done);
}

static void test_move(http_transfer_data_t *challenge)
{
   done_t done = {0};
   unsigned retries = reject_credentials(webdav_backup_cb,
         new_cb_state("saves/a.srm", NULL, &done), challenge);
   check_gives_up("MOVE", retries, &done);
}

static unsigned mkdir_calls;
static bool     mkdir_success;

static void on_mkdir_done(bool success, webdav_cb_state_t *st)
{
   mkdir_calls++;
   mkdir_success = success;
   free(st);
}

static void test_mkcol(http_transfer_data_t *challenge)
{
   done_t done = {0};
   unsigned retries;
   webdav_mkdir_state_t *mk = (webdav_mkdir_state_t*)calloc(1, sizeof(*mk));

   strlcpy(mk->url, "http://127.0.0.1/dav/saves/", sizeof(mk->url));
   mk->last_slash = strrchr(mk->url, '/');
   mk->post_slash = mk->last_slash[1];
   mk->cb         = on_mkdir_done;
   mk->cb_st      = new_cb_state("saves/a.srm", NULL, &done);

   mkdir_calls   = 0;
   mkdir_success = true;
   retries       = reject_credentials(webdav_mkdir_cb, mk, challenge);

   CHECK(retries == 1,
         "MKCOL: wrong password must be retried once, was retried %u time(s)%s",
         retries, retries >= MAX_ROUNDS ? " (looping)" : "");
   CHECK(mkdir_calls == 1,
         "MKCOL: completion must be reported once, was reported %u time(s)",
         mkdir_calls);
   CHECK(!mkdir_success, "MKCOL: rejected credentials must fail");
}

static const char upload[] = "SAVEDATA-0123456789";

static RFILE *open_upload(const char *tmp)
{
   RFILE *f = filestream_open(tmp, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   filestream_write(f, upload, sizeof(upload) - 1);
   filestream_close(f);
   return filestream_open(tmp, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static bool put_sent_upload(void)
{
   return stub_req.put_len == sizeof(upload) - 1
       && stub_req.put_data
       && !memcmp(stub_req.put_data, upload, sizeof(upload) - 1);
}

/* PUT starts from webdav_do_update(), as webdav_update() does, so the
 * file is left where a real first attempt leaves it: at its end. */
static void test_put(http_transfer_data_t *challenge, const char *tmp)
{
   done_t   done  = {0};
   RFILE   *rfile = open_upload(tmp);
   unsigned retries = 0;
   webdav_cb_state_t *st = new_cb_state("saves/a.srm", NULL, &done);

   st->rfile = rfile;
   stub_reset();
   webdav_do_update(true, st);
   CHECK(stub_req.count == 1 && put_sent_upload(),
         "PUT: first attempt must send the file");

   while (stub_req.count && retries < MAX_ROUNDS)
   {
      retro_task_callback_t cb = stub_req.cb;
      void *user_data          = stub_req.user_data;
      stub_req.count           = 0;
      cb(NULL, challenge, user_data, NULL);
      if (!stub_req.count)
         break;
      retries++;
      CHECK(put_sent_upload(),
            "PUT: retry %u sent %u byte(s) that are not the file",
            retries, (unsigned)stub_req.put_len);
   }
   if (stub_req.count)
   {
      http_transfer_data_t stop;
      make_response(&stop, 500, NULL, NULL, 0);
      stub_req.count = 0;
      stub_req.cb(NULL, &stop, stub_req.user_data, NULL);
   }

   check_gives_up("PUT", retries, &done);
   filestream_close(rfile);
}

/* A stale nonce is one challenge, then success: still retried. */
static void test_stale_nonce_recovers(http_transfer_data_t *challenge,
      const char *tmp)
{
   static char body[] = "server-copy";
   done_t done = {0};
   http_transfer_data_t ok;
   char got[32] = {0};

   make_response(&ok, 200, NULL, body, sizeof(body) - 1);

   stub_reset();
   webdav_read_cb(NULL, challenge, new_cb_state("saves/a.srm", tmp, &done),
         NULL);
   CHECK(stub_req.count == 1 && !strcmp(stub_req.method, "GET")
         && stub_req.had_auth,
         "stale nonce: one authenticated retry expected");

   if (stub_req.count == 1)
   {
      stub_req.count = 0;
      stub_req.cb(NULL, &ok, stub_req.user_data, NULL);
   }

   CHECK(done.calls == 1 && done.success && done.file,
         "stale nonce: retry answered 200 must succeed with the file");
   if (done.file)
   {
      filestream_read(done.file, got, sizeof(got) - 1);
      CHECK(!strcmp(got, body), "stale nonce: got \"%s\"", got);
      filestream_close(done.file);
   }
}

int main(void)
{
   settings_t           *settings  = config_get_ptr();
   webdav_state_t       *webdav_st = webdav_state_get_ptr();
   struct string_list   *headers   = string_list_new();
   http_transfer_data_t  challenge;
   char tmp[64];

   snprintf(tmp, sizeof(tmp), "/tmp/webdav_reauth_%ld", (long)getpid());

   strlcpy(settings->arrays.webdav_username, "user",
         sizeof(settings->arrays.webdav_username));
   strlcpy(settings->arrays.webdav_password, "wrong",
         sizeof(settings->arrays.webdav_password));
   strlcpy(webdav_st->url, "http://127.0.0.1/dav/", sizeof(webdav_st->url));

   string_list_append(headers, digest_header,
         (union string_list_elem_attr){0});
   make_response(&challenge, 401, headers, NULL, 0);

   test_options(&challenge);
   test_get(&challenge, tmp);
   test_put(&challenge, tmp);
   test_delete(&challenge);
   test_move(&challenge);
   test_mkcol(&challenge);
   test_stale_nonce_recovers(&challenge, tmp);

   stub_reset();
   webdav_cleanup_digest();
   string_list_free(headers);
   remove(tmp);

   if (failures)
   {
      printf("\n%u check(s) failed\n", failures);
      return 1;
   }
   printf("\nall WebDAV re-authentication checks passed\n");
   return 0;
}

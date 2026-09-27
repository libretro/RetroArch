/* Harness for the WebDAV upload backup.
 *
 * With destructive sync off, cloud sync promises not to throw away the
 * server's copy of a file.  A delete kept that promise - WebDAV moved
 * the file into deleted/ - but an upload did not: it overwrote the
 * server's copy in place, so the one change most likely to lose a save
 * (an older or broken save from another device replacing a good one)
 * left nothing behind.
 *
 * An upload now copies the server's file to deleted/<path>-<time>
 * first, and only then PUTs.  COPY rather than MOVE, so the server
 * keeps its file until the PUT replaces it.  The test plays the
 * server and checks, request by request:
 *
 * - the backup is taken before the PUT, into deleted/, with the same
 *   naming a delete uses;
 * - a 404 (nothing on the server yet) is not an error;
 * - a backup that fails stops the upload, reported once, with no PUT;
 * - destructive sync, and the server manifest, which is rewritten
 *   every sync, are not backed up;
 * - the COPY and the PUT each get their own Digest retry.
 *
 * Includes the driver's translation unit so the paths are tested as
 * shipped rather than as a copy that can drift from them.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <boolean.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>

#include "../../../network/cloud_sync/webdav.c"

#include "stubs_retroarch.h"

#define BASE       "http://127.0.0.1/dav/"
#define MAX_STEPS  32

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

/* What the scripted server answers.  copy[] and put[] are consumed in
 * order, one per request; everything else gets 201. */
typedef struct
{
   int copy[4];
   int put[4];
} script_t;

typedef struct
{
   char     trace[MAX_STEPS][600];
   unsigned steps;
   unsigned calls;
   bool     success;
} run_t;

static struct string_list *challenge_headers;

static void on_done(void *user_data, const char *path, bool success,
      RFILE *file)
{
   run_t *run = (run_t*)user_data;
   (void)path; (void)file;
   run->calls++;
   run->success = success;
}

static const char *rel(const char *url)
{
   return strncmp(url, BASE, sizeof(BASE) - 1) ? url : url + sizeof(BASE) - 1;
}

static void run_update(const char *path, bool destructive,
      const script_t *script, run_t *run, const char *tmp)
{
   http_transfer_data_t resp;
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   unsigned ncopy = 0, nput = 0;
   RFILE *rfile;

   memset(run, 0, sizeof(*run));
   config_get_ptr()->bools.cloud_sync_destructive = destructive;

   /* A fresh sync, as webdav_sync_begin() leaves it: basic auth until
    * a Digest challenge says otherwise, no collections known yet. */
   webdav_cleanup_digest();
   webdav_st->basic = true;

   if (webdav_st->dirs)
      string_list_free(webdav_st->dirs);
   webdav_st->dirs = NULL;

   rfile = filestream_open(tmp, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   stub_has_pending       = false;
   stub_pending.overlapped = false;
   webdav_update(path, rfile, on_done, run);

   while (stub_has_pending && run->steps < MAX_STEPS)
   {
      stub_request_t req = stub_pending;
      int status         = 201;

      stub_has_pending = false;
      if (req.dest[0])
         snprintf(run->trace[run->steps], sizeof(run->trace[0]),
               "%s %s -> %s", req.method, rel(req.url), rel(req.dest));
      else
         snprintf(run->trace[run->steps], sizeof(run->trace[0]),
               "%s %s", req.method, rel(req.url));
      run->steps++;

      if (!strcmp(req.method, "COPY") && ncopy < 4 && script->copy[ncopy])
         status = script->copy[ncopy++];
      else if (!strcmp(req.method, "PUT") && nput < 4 && script->put[nput])
         status = script->put[nput++];

      memset(&resp, 0, sizeof(resp));
      resp.status  = status;
      resp.headers = (status == 401) ? challenge_headers : NULL;
      req.cb(NULL, &resp, req.user_data, NULL);
   }

   CHECK(!stub_pending.overlapped, "%s: requests overlapped", path);
   filestream_close(rfile);
}

static int find_step(const run_t *run, const char *prefix, unsigned from)
{
   unsigned i;
   for (i = from; i < run->steps; i++)
      if (!strncmp(run->trace[i], prefix, strlen(prefix)))
         return (int)i;
   return -1;
}

static unsigned count_steps(const run_t *run, const char *prefix)
{
   unsigned i, n = 0;
   for (i = 0; i < run->steps; i++)
      if (!strncmp(run->trace[i], prefix, strlen(prefix)))
         n++;
   return n;
}

static void dump(const char *what, const run_t *run)
{
   unsigned i;
   printf("  %s:\n", what);
   for (i = 0; i < run->steps; i++)
      printf("    %s\n", run->trace[i]);
}

/* "COPY saves/a.srm -> deleted/saves/a.srm-yymmdd-hhmmss" */
static bool backup_named_like_delete(const char *step)
{
   static const char want[] = "COPY saves/a.srm -> deleted/saves/a.srm-";
   const char *p = step + sizeof(want) - 1;
   int i;
   if (strncmp(step, want, sizeof(want) - 1))
      return false;
   for (i = 0; i < 13; i++)
      if (i == 6 ? p[i] != '-' : !isdigit((unsigned char)p[i]))
         return false;
   return p[13] == '\0';
}

static void test_backup_then_put(const char *tmp)
{
   script_t script = {{0}, {0}};
   run_t    run;
   int      copy, put;

   run_update("saves/a.srm", false, &script, &run, tmp);
   dump("non-destructive upload", &run);

   copy = find_step(&run, "COPY ", 0);
   put  = find_step(&run, "PUT saves/a.srm", 0);
   CHECK(copy >= 0, "non-destructive upload must back up the server copy");
   CHECK(put >= 0, "non-destructive upload must still upload");
   CHECK(copy < 0 || put < 0 || copy < put, "backup must come before the PUT");
   CHECK(copy < 0 || backup_named_like_delete(run.trace[copy]),
         "backup must go to deleted/<path>-<yymmdd-hhmmss>, got \"%s\"",
         copy < 0 ? "" : run.trace[copy]);
   CHECK(find_step(&run, "MKCOL deleted/saves/", 0) >= 0
         && (copy < 0 || find_step(&run, "MKCOL deleted/saves/", 0) < copy),
         "deleted/saves/ must exist before the COPY");
   CHECK(!count_steps(&run, "MOVE") && !count_steps(&run, "DELETE"),
         "the server copy must be copied, never moved or deleted");
   CHECK(run.calls == 1 && run.success,
         "non-destructive upload must report success once");
}

static void test_nothing_to_back_up(const char *tmp)
{
   script_t script = {{404}, {0}};
   run_t    run;

   run_update("saves/a.srm", false, &script, &run, tmp);
   CHECK(count_steps(&run, "PUT saves/a.srm") == 1,
         "404 on the backup (no server copy yet) must still upload");
   CHECK(run.calls == 1 && run.success,
         "404 on the backup must report success once");
}

static void test_backup_fails(const char *tmp)
{
   script_t script = {{500}, {0}};
   run_t    run;

   run_update("saves/a.srm", false, &script, &run, tmp);
   CHECK(!count_steps(&run, "PUT"),
         "a failed backup must stop the upload");
   CHECK(run.calls == 1 && !run.success,
         "a failed backup must report failure once");
}

static void test_destructive(const char *tmp)
{
   script_t script = {{0}, {0}};
   run_t    run;

   run_update("saves/a.srm", true, &script, &run, tmp);
   CHECK(!count_steps(&run, "COPY") && !count_steps(&run, "MKCOL deleted/"),
         "destructive sync must not back up");
   CHECK(count_steps(&run, "PUT saves/a.srm") == 1 && run.calls == 1
         && run.success, "destructive sync must upload once");
}

static void test_manifest(const char *tmp)
{
   script_t script = {{0}, {0}};
   run_t    run;

   run_update(CLOUD_SYNC_SERVER_MANIFEST, false, &script, &run, tmp);
   CHECK(!count_steps(&run, "COPY") && !count_steps(&run, "MKCOL"),
         "the server manifest must not be backed up");
   CHECK(count_steps(&run, "PUT " CLOUD_SYNC_SERVER_MANIFEST) == 1
         && run.calls == 1 && run.success,
         "the server manifest must upload once");
}

static void test_each_request_reauths(const char *tmp)
{
   script_t script = {{401, 201}, {401, 201}};
   run_t    run;

   run_update("saves/a.srm", false, &script, &run, tmp);
   dump("challenge on COPY and on PUT", &run);
   CHECK(count_steps(&run, "COPY") == 2,
         "COPY must get its own Digest retry");
   CHECK(count_steps(&run, "PUT") == 2,
         "PUT must get its own Digest retry after the COPY used one");
   CHECK(run.calls == 1 && run.success,
         "one challenge each must still succeed, once");
}

int main(void)
{
   settings_t     *settings  = config_get_ptr();
   webdav_state_t *webdav_st = webdav_state_get_ptr();
   char tmp[64];
   FILE *f;

   snprintf(tmp, sizeof(tmp), "/tmp/webdav_backup_%ld", (long)getpid());
   if ((f = fopen(tmp, "wb")))
   {
      fputs("SAVEDATA", f);
      fclose(f);
   }

   strlcpy(settings->arrays.webdav_username, "user",
         sizeof(settings->arrays.webdav_username));
   strlcpy(settings->arrays.webdav_password, "pass",
         sizeof(settings->arrays.webdav_password));
   strlcpy(webdav_st->url, BASE, sizeof(webdav_st->url));
   webdav_st->dav_verified = true;

   challenge_headers = string_list_new();
   string_list_append(challenge_headers,
         "WWW-Authenticate: Digest realm=\"dav\", nonce=\"6f2a\", "
         "qop=\"auth\", algorithm=MD5",
         (union string_list_elem_attr){0});

   test_backup_then_put(tmp);
   test_nothing_to_back_up(tmp);
   test_backup_fails(tmp);
   test_destructive(tmp);
   test_manifest(tmp);
   test_each_request_reauths(tmp);

   if (webdav_st->dirs)
      string_list_free(webdav_st->dirs);
   webdav_st->dirs = NULL;
   webdav_cleanup_digest();
   string_list_free(challenge_headers);
   remove(tmp);

   if (failures)
   {
      printf("\n%u check(s) failed\n", failures);
      return 1;
   }
   printf("\nall WebDAV upload backup checks passed\n");
   return 0;
}

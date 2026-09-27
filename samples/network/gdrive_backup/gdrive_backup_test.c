/* Harness for the Google Drive driver's uploads under non-destructive
 * sync.
 *
 * With destructive sync off, a Google Drive delete keeps the server's
 * copy, renamed in place to <name>-<yymmdd-hhmmss>, but an upload
 * replaced the file's content outright: the setting protected against
 * deletes and not against overwrites.  An upload to an existing file
 * now first asks Drive to copy it (files.copy) to that name in the same
 * folder, then PATCHes the new content.  The test plays the Drive API
 * and checks, request by request:
 *
 * - the copy comes before the PATCH, is made from the right file, into
 *   the same folder, under <name>-<yymmdd-hhmmss>;
 * - a failed copy stops the upload, reported once, with no PATCH;
 * - a 401 on the copy refreshes the token and goes round again, and the
 *   PATCH after a successful copy still has its own 401 retry;
 * - destructive sync, a file that does not exist yet (created, nothing
 *   to keep) and the server manifest skip the copy.
 *
 * Includes the driver's translation unit so it is tested as shipped.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <boolean.h>
#include <streams/file_stream.h>

#include "../../../network/cloud_sync/google_drive.c"

#include "stubs_retroarch.h"

#define MAX_STEPS 24

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

typedef enum { REQ_OTHER, REQ_SEARCH, REQ_COPY, REQ_PATCH, REQ_CREATE, REQ_TOKEN } req_kind_t;

typedef struct
{
   bool exists;          /* search finds the file */
   int  copy_status[4];  /* consumed in order; 0 = 200 */
   int  patch_status[4];
} script_t;

typedef struct
{
   stub_request_t req[MAX_STEPS];
   req_kind_t     kind[MAX_STEPS];
   unsigned       steps;
   unsigned       calls;
   bool           success;
} run_t;

static void on_done(void *user_data, const char *path, bool success,
      RFILE *file)
{
   run_t *run = (run_t*)user_data;
   (void)path; (void)file;
   run->calls++;
   run->success = success;
}

static req_kind_t classify(const stub_request_t *req)
{
   if (strstr(req->url, "oauth2.googleapis.com/token"))
      return REQ_TOKEN;
   if (strstr(req->url, "/files?q="))
      return REQ_SEARCH;
   if (strstr(req->url, "/copy"))
      return REQ_COPY;
   if (!strcmp(req->method, "PATCH") && strstr(req->url, "uploadType=media"))
      return REQ_PATCH;
   if (!strcmp(req->method, "POST") && !strcmp(req->url, GDRIVE_API_BASE "/files"))
      return REQ_CREATE;
   return REQ_OTHER;
}

static void run_update(const char *path, bool destructive,
      const script_t *script, run_t *run, const char *tmp)
{
   static char found[]   = "{\"files\":[{\"id\":\"FILE1\"}]}";
   static char none[]    = "{\"files\":[]}";
   static char copied[]  = "{\"id\":\"COPY1\"}";
   static char created[] = "{\"id\":\"NEW1\"}";
   static char token[]   = "{\"access_token\":\"token-2\",\"expires_in\":3599}";
   static char empty[]   = "{}";
   unsigned ncopy = 0, npatch = 0;
   RFILE *rfile;

   memset(run, 0, sizeof(*run));
   config_get_ptr()->bools.cloud_sync_destructive = destructive;
   strlcpy(gdrive_st.access_token, "token-1", sizeof(gdrive_st.access_token));
   stub_has_pending        = false;
   stub_pending.overlapped = false;

   rfile = filestream_open(tmp, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(gdrive_update(path, rfile, on_done, run), "%s: update must start", path);

   while (stub_has_pending && run->steps < MAX_STEPS)
   {
      http_transfer_data_t resp;
      stub_request_t req = stub_pending;
      req_kind_t kind    = classify(&req);

      stub_has_pending = false;
      run->kind[run->steps]  = kind;
      run->req[run->steps++] = req;

      memset(&resp, 0, sizeof(resp));
      resp.status = 200;
      resp.data   = empty;
      switch (kind)
      {
         case REQ_SEARCH: resp.data = script->exists ? found : none; break;
         case REQ_COPY:
            if (ncopy < 4 && script->copy_status[ncopy])
               resp.status = script->copy_status[ncopy];
            ncopy++;
            resp.data = copied;
            break;
         case REQ_PATCH:
            if (npatch < 4 && script->patch_status[npatch])
               resp.status = script->patch_status[npatch];
            npatch++;
            break;
         case REQ_CREATE: resp.data = created; break;
         case REQ_TOKEN:  resp.data = token;   break;
         default: break;
      }
      resp.len = strlen(resp.data);
      req.cb(NULL, &resp, req.user_data, NULL);
   }

   CHECK(!stub_pending.overlapped, "%s: requests overlapped", path);
   filestream_close(rfile);
}

static int find(const run_t *run, req_kind_t kind, unsigned from)
{
   unsigned i;
   for (i = from; i < run->steps; i++)
      if (run->kind[i] == kind)
         return (int)i;
   return -1;
}

static unsigned count(const run_t *run, req_kind_t kind)
{
   unsigned i, n = 0;
   for (i = 0; i < run->steps; i++)
      n += (run->kind[i] == kind);
   return n;
}

/* {"name":"a.srm-yymmdd-hhmmss","parents":["F_SAVES"]} */
static bool copy_body_ok(const char *body)
{
   static const char want[] = "{\"name\":\"a.srm-";
   const char *p = body + sizeof(want) - 1;
   int i;
   if (strncmp(body, want, sizeof(want) - 1))
      return false;
   for (i = 0; i < 13; i++)
      if (i == 6 ? p[i] != '-' : !isdigit((unsigned char)p[i]))
         return false;
   return !strcmp(p + 13, "\",\"parents\":[\"F_SAVES\"]}");
}

static void test_copy_then_patch(const char *tmp)
{
   script_t script = {true, {0}, {0}};
   run_t    run;
   int      copy, patch;

   run_update("saves/a.srm", false, &script, &run, tmp);
   copy  = find(&run, REQ_COPY, 0);
   patch = find(&run, REQ_PATCH, 0);

   CHECK(copy >= 0, "non-destructive upload must copy the existing file first");
   CHECK(patch >= 0, "non-destructive upload must still upload");
   CHECK(copy < 0 || patch < 0 || copy < patch, "the copy must come before the PATCH");
   if (copy >= 0)
   {
      CHECK(!strcmp(run.req[copy].url, GDRIVE_API_BASE "/files/FILE1/copy"),
            "the copy must be made from the file being replaced, got %s",
            run.req[copy].url);
      CHECK(copy_body_ok(run.req[copy].body),
            "the copy must be named a.srm-<yymmdd-hhmmss> in the same folder, got %s",
            run.req[copy].body);
   }
   CHECK(run.calls == 1 && run.success, "non-destructive upload must succeed once");
}

static void test_copy_fails(const char *tmp)
{
   script_t script = {true, {500}, {0}};
   run_t    run;

   run_update("saves/a.srm", false, &script, &run, tmp);
   CHECK(!count(&run, REQ_PATCH), "a failed copy must stop the upload");
   CHECK(run.calls == 1 && !run.success, "a failed copy must report failure once");
}

static void test_token_refresh(const char *tmp)
{
   script_t script = {true, {401}, {401}};
   run_t    run;

   run_update("saves/a.srm", false, &script, &run, tmp);
   CHECK(count(&run, REQ_TOKEN) == 2,
         "a 401 on the copy and a 401 on the PATCH must each refresh the token, saw %u",
         count(&run, REQ_TOKEN));
   CHECK(count(&run, REQ_COPY) == 2 && count(&run, REQ_PATCH) == 2,
         "copy and PATCH must each be retried once after the refresh");
   CHECK(run.calls == 1 && run.success, "one 401 each must still succeed, once");
}

static void test_no_backup_wanted(const char *tmp)
{
   script_t exists  = {true, {0}, {0}};
   script_t missing = {false, {0}, {0}};
   run_t    run;

   run_update("saves/a.srm", true, &exists, &run, tmp);
   CHECK(!count(&run, REQ_COPY) && count(&run, REQ_PATCH) == 1
         && run.calls == 1 && run.success,
         "destructive upload must PATCH without a copy");

   run_update("saves/a.srm", false, &missing, &run, tmp);
   CHECK(!count(&run, REQ_COPY) && count(&run, REQ_CREATE) == 1
         && count(&run, REQ_PATCH) == 1 && run.calls == 1 && run.success,
         "a file that does not exist yet has nothing to keep");

   run_update(CLOUD_SYNC_SERVER_MANIFEST, false, &exists, &run, tmp);
   CHECK(!count(&run, REQ_COPY) && count(&run, REQ_PATCH) == 1
         && run.calls == 1 && run.success,
         "the server manifest must be replaced without a copy");
}

int main(void)
{
   char  tmp[64];
   FILE *f;

   snprintf(tmp, sizeof(tmp), "/tmp/gdrive_backup_%ld", (long)getpid());
   if ((f = fopen(tmp, "wb")))
   {
      fputs("SAVEDATA", f);
      fclose(f);
   }

   strlcpy(gdrive_st.folder_id, "ROOT", sizeof(gdrive_st.folder_id));
   gdrive_folder_cache_add("saves", "F_SAVES");

   test_copy_then_patch(tmp);
   test_copy_fails(tmp);
   test_token_refresh(tmp);
   test_no_backup_wanted(tmp);

   free(gdrive_st.folders);
   gdrive_st.folders = NULL;
   remove(tmp);

   if (failures)
   {
      printf("\n%u check(s) failed\n", failures);
      return 1;
   }
   printf("\nall Google Drive upload backup checks passed\n");
   return 0;
}

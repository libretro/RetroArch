/* Harness for the S3 driver's backups under non-destructive sync.
 *
 * With destructive sync off, cloud sync promises not to throw away the
 * server's copy of a file.  The S3 driver never read the setting: a
 * delete removed the object for good and an upload overwrote it, so
 * for S3 users the setting did nothing at all.
 *
 * Both now copy the object to deleted/<path>-<yymmdd-hhmmss> first
 * (CopyObject: a PUT on the new key naming the source in
 * x-amz-copy-source), and only then delete or upload.  The test plays
 * the server and checks, request by request:
 *
 * - the copy comes first, targets deleted/, and names the right source
 *   for both addressing styles - /<bucket>/<key> whether the bucket is
 *   in the host (virtual-hosted) or the path (path-style);
 * - x-amz-copy-source is signed, as S3 requires of every x-amz-*
 *   header;
 * - a 404 (nothing to keep) is not an error;
 * - a failed copy - including a 200 carrying an <Error> body, which
 *   CopyObject can answer - stops the delete or upload, reported once;
 * - destructive sync and the server manifest skip the copy.
 *
 * Every upload body is streamed from the file and drained here the way
 * net_http does it, a replay from the start included: it must be all of
 * the file (or of a part's own range of it, for a multipart upload, and
 * for the single PUT it falls back to) and what was signed.
 *
 * It also calls the signer directly: query parameters sort as SigV4
 * requires, and out of memory the signer returns no signature rather
 * than a wrong one.
 *
 * The signature itself was cross-checked against botocore's SigV4 for
 * the same request when this was written; here the test checks the
 * header set that goes into it.
 *
 * Includes the driver's translation unit so it is tested as shipped.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <boolean.h>
#include <streams/file_stream.h>

/* Set to N to fail the driver's Nth allocation from here on, counting
 * malloc and strdup together.  Zero fails none. */
static unsigned fail_alloc = 0;

static bool alloc_fails(void)
{
   return fail_alloc && --fail_alloc == 0;
}

static void *test_malloc(size_t size)
{
   return alloc_fails() ? NULL : malloc(size);
}

static char *test_strdup(const char *s)
{
   return alloc_fails() ? NULL : strdup(s);
}

#define malloc test_malloc
#define strdup test_strdup

/* Parts of 5 MiB, the least S3 takes, so a multipart upload is 11 MiB
 * rather than the 64 the driver waits for */
#define S3_MULTIPART_NOMINAL_THRESHOLD_BYTES (5ULL * 1024ULL * 1024ULL)

#include "../../../network/cloud_sync/s3.c"

#include "stubs_retroarch.h"

#define MAX_STEPS 16

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

typedef struct
{
   const char *copy_body;
   int         copy_status;
   /* nonzero: what CreateMultipartUpload answers, -1 a transport error */
   int         initiate_status;
} script_t;

typedef struct
{
   stub_request_t req[MAX_STEPS];
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

static bool is_copy(const stub_request_t *req)
{
   return strstr(req->headers, "x-amz-copy-source:") != NULL;
}

static void use_endpoint(const char *url)
{
   s3_state_t *s3_st = s3_state_get_ptr();
   memset(s3_st, 0, sizeof(*s3_st));
   strlcpy(s3_st->url, url, sizeof(s3_st->url));
   strlcpy(s3_st->access_key_id, "AKIDEXAMPLE", sizeof(s3_st->access_key_id));
   strlcpy(s3_st->secret_access_key, "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY",
         sizeof(s3_st->secret_access_key));
   s3_parse_url(s3_st->url, s3_st->bucket, s3_st->region, s3_st->host, NULL);
}

static bool header_is(const stub_request_t *req, const char *name,
      const char *value);

static void serve(const script_t *script, run_t *run)
{
   static char initiated[] =
      "<InitiateMultipartUploadResult><UploadId>up1</UploadId>"
      "</InitiateMultipartUploadResult>";
   static char etag[] = "ETag: \"part\"\0";

   while (stub_has_pending && run->steps < MAX_STEPS)
   {
      http_transfer_data_t resp;
      stub_request_t req = stub_pending;

      stub_has_pending = false;
      run->req[run->steps++] = req;

      /* Every body is streamed from the file, all of it, and is what
       * was signed */
      if (!strcmp(req.method, "PUT") && req.body_len)
         CHECK(req.body_streamed, "%s: the body was handed over whole, not streamed",
               req.url);
      if (req.body_streamed)
      {
         CHECK(req.body_whole, "%s: the body did not stream %u bytes, replay included",
               req.url, (unsigned)req.body_len);
         CHECK(header_is(&req, "x-amz-content-sha256", req.body_sha),
               "%s: signed for another body than the %s it sent",
               req.url, req.body_sha);
      }

      memset(&resp, 0, sizeof(resp));
      resp.status = 200;
      if (!strcmp(req.method, "POST") && strstr(req.url, "?uploads="))
      {
         if (script->initiate_status)
            resp.status = script->initiate_status;
         else
         {
            resp.data = initiated;
            resp.len  = strlen(initiated);
         }
      }
      else if (!strcmp(req.method, "PUT") && strstr(req.url, "partNumber="))
         resp.headers = etag;
      else if (is_copy(&req))
      {
         if (script->copy_status)
            resp.status = script->copy_status;
         if (script->copy_body)
         {
            resp.data = (char*)script->copy_body;
            resp.len  = strlen(script->copy_body);
         }
      }
      else if (!strcmp(req.method, "DELETE"))
         resp.status = 204;
      req.cb(NULL, &resp, req.user_data, NULL);
   }
}

static void run_op(bool is_delete, const char *path, bool destructive,
      const script_t *script, run_t *run, const char *tmp)
{
   RFILE *rfile = NULL;
   bool   started;

   memset(run, 0, sizeof(*run));
   config_get_ptr()->bools.cloud_sync_destructive = destructive;
   stub_has_pending        = false;
   stub_pending.overlapped = false;

   if (is_delete)
      started = s3_free(path, on_done, run);
   else
   {
      rfile   = filestream_open(tmp, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      started = s3_update(path, rfile, on_done, run);
   }

   CHECK(started, "%s %s: request must start", is_delete ? "delete" : "update", path);
   serve(script, run);
   CHECK(!stub_pending.overlapped, "%s: requests overlapped", path);
   if (rfile)
      filestream_close(rfile);
}

static int find(const run_t *run, bool copy, const char *method)
{
   unsigned i;
   for (i = 0; i < run->steps; i++)
      if (   is_copy(&run->req[i]) == copy
          && !strcmp(run->req[i].method, method))
         return (int)i;
   return -1;
}

static unsigned count_copies(const run_t *run)
{
   unsigned i, n = 0;
   for (i = 0; i < run->steps; i++)
      n += is_copy(&run->req[i]);
   return n;
}

static bool header_is(const stub_request_t *req, const char *name,
      const char *value)
{
   char want[1200];
   snprintf(want, sizeof(want), "%s: %s\r\n", name, value);
   return strstr(req->headers, want) != NULL;
}

/* <base>deleted/saves/a.srm-yymmdd-hhmmss */
static bool backup_url(const char *url, const char *base)
{
   char want[512];
   const char *p;
   int i;
   snprintf(want, sizeof(want), "%sdeleted/saves/a.srm-", base);
   if (strncmp(url, want, strlen(want)))
      return false;
   p = url + strlen(want);
   for (i = 0; i < 13; i++)
      if (i == 6 ? p[i] != '-' : !isdigit((unsigned char)p[i]))
         return false;
   return p[13] == '\0';
}

static void check_backup_first(const char *what, const run_t *run,
      const char *base, const char *source, const char *then_method)
{
   int copy = find(run, true, "PUT");
   int then = find(run, false, then_method);

   CHECK(copy >= 0, "%s: must copy the object to deleted/ first", what);
   CHECK(then >= 0, "%s: must still %s", what, then_method);
   CHECK(copy < 0 || then < 0 || copy < then, "%s: copy must come first", what);
   if (copy < 0)
      return;
   CHECK(backup_url(run->req[copy].url, base),
         "%s: copy must target deleted/saves/a.srm-<yymmdd-hhmmss>, got %s",
         what, run->req[copy].url);
   CHECK(header_is(&run->req[copy], "x-amz-copy-source", source),
         "%s: x-amz-copy-source must be %s", what, source);
   CHECK(strstr(run->req[copy].headers,
         "SignedHeaders=host;x-amz-content-sha256;x-amz-copy-source;x-amz-date,") != NULL,
         "%s: x-amz-copy-source must be signed", what);
   CHECK(run->req[copy].body_len == 0, "%s: copy must have an empty body", what);
   CHECK(run->calls == 1 && run->success, "%s: must report success once", what);
}

static void test_update_virtual_hosted(const char *tmp)
{
   static const char base[] = "https://bucket.s3.us-east-1.amazonaws.com/retroarch/";
   script_t script = {0};
   run_t    run;

   use_endpoint(base);
   run_op(false, "saves/a.srm", false, &script, &run, tmp);
   check_backup_first("update, virtual-hosted", &run, base,
         "/bucket/retroarch/saves/a.srm", "PUT");
   CHECK(find(&run, false, "PUT") < 0
         || !strcmp(run.req[find(&run, false, "PUT")].url, "https://bucket.s3.us-east-1.amazonaws.com/retroarch/saves/a.srm"),
         "update, virtual-hosted: upload must go to the object itself");
}

static void test_update_path_style(const char *tmp)
{
   static const char base[] = "http://127.0.0.1:9000/bucket/retroarch/";
   script_t script = {0};
   run_t    run;

   use_endpoint(base);
   run_op(false, "saves/a.srm", false, &script, &run, tmp);
   check_backup_first("update, path-style", &run, base,
         "/bucket/retroarch/saves/a.srm", "PUT");
}

static void test_delete(const char *tmp)
{
   static const char base[] = "https://bucket.s3.us-east-1.amazonaws.com/retroarch/";
   script_t script = {0};
   run_t    run;

   use_endpoint(base);
   run_op(true, "saves/a.srm", false, &script, &run, tmp);
   check_backup_first("delete", &run, base,
         "/bucket/retroarch/saves/a.srm", "DELETE");
}

static void test_nothing_to_keep(const char *tmp)
{
   script_t script = {NULL, 404};
   run_t    run;

   use_endpoint("https://bucket.s3.us-east-1.amazonaws.com/retroarch/");
   run_op(false, "saves/a.srm", false, &script, &run, tmp);
   CHECK(find(&run, false, "PUT") >= 0 && run.calls == 1 && run.success,
         "update: 404 on the copy (no server copy yet) must still upload");
   run_op(true, "saves/a.srm", false, &script, &run, tmp);
   CHECK(find(&run, false, "DELETE") >= 0 && run.calls == 1 && run.success,
         "delete: 404 on the copy must still delete");
}

static void test_copy_fails(const char *tmp)
{
   static const char error_body[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<Error><Code>InternalError</Code><Message>We encountered an internal error.</Message></Error>";
   script_t failed  = {NULL, 500};
   script_t errored = {error_body, 200};
   run_t    run;

   use_endpoint("https://bucket.s3.us-east-1.amazonaws.com/retroarch/");

   run_op(false, "saves/a.srm", false, &failed, &run, tmp);
   CHECK(find(&run, false, "PUT") < 0 && run.calls == 1 && !run.success,
         "update: a failed copy must stop the upload and fail once");

   run_op(true, "saves/a.srm", false, &failed, &run, tmp);
   CHECK(find(&run, false, "DELETE") < 0 && run.calls == 1 && !run.success,
         "delete: a failed copy must stop the delete and fail once");

   run_op(true, "saves/a.srm", false, &errored, &run, tmp);
   CHECK(find(&run, false, "DELETE") < 0 && run.calls == 1 && !run.success,
         "delete: a 200 with an <Error> body is a failed copy");
}

static void test_no_backup_wanted(const char *tmp)
{
   script_t script = {0};
   run_t    run;

   use_endpoint("https://bucket.s3.us-east-1.amazonaws.com/retroarch/");

   run_op(false, "saves/a.srm", true, &script, &run, tmp);
   CHECK(!count_copies(&run) && find(&run, false, "PUT") >= 0
         && run.calls == 1 && run.success,
         "destructive update must upload without a copy");

   run_op(true, "saves/a.srm", true, &script, &run, tmp);
   CHECK(!count_copies(&run) && find(&run, false, "DELETE") >= 0
         && run.calls == 1 && run.success,
         "destructive delete must delete without a copy");

   run_op(false, CLOUD_SYNC_SERVER_MANIFEST, false, &script, &run, tmp);
   CHECK(!count_copies(&run) && find(&run, false, "PUT") >= 0
         && run.calls == 1 && run.success,
         "the server manifest must upload without a copy");
}

#define BIG_PART  (5u * 1024u * 1024u)
#define BIG_SIZE  (2u * BIG_PART + 4099u)

/* The SHA-256 of @len bytes of @path from @start, read here on its own */
static void sha_of(char *out, const char *path, long start, size_t len)
{
   static const char hex[] = "0123456789abcdef";
   static uint8_t     run[65536];
   struct sha256_state st;
   uint8_t digest[32];
   FILE   *f = fopen(path, "rb");
   unsigned i;

   sha256_stream_init(&st, 0);
   if (f && !fseek(f, start, SEEK_SET))
      while (len)
      {
         size_t n = len < sizeof(run) ? len : sizeof(run);
         if (fread(run, 1, n, f) != n)
            break;
         sha256_stream_update(&st, run, n);
         len -= n;
      }
   if (f)
      fclose(f);
   sha256_stream_final(&st, digest);
   for (i = 0; i < 32; i++)
   {
      out[2 * i]     = hex[digest[i] >> 4];
      out[2 * i + 1] = hex[digest[i] & 15];
   }
   out[64] = '\0';
}

/* A file past the multipart threshold goes up in parts, each streamed
 * from its own range of the file and signed for those bytes; when the
 * multipart start fails in transport, the whole file goes up in one
 * PUT, streamed the same way. */
static void test_multipart_streamed(const char *big)
{
   static const char base[] = "https://bucket.s3.us-east-1.amazonaws.com/retroarch/";
   static const size_t want_len[3] = { BIG_PART, BIG_PART, 4099u };
   script_t script = {0};
   run_t    run;
   char     sha[65];
   unsigned i;
   int      part = 0;

   use_endpoint(base);
   run_op(false, "saves/big.srm", true, &script, &run, big);
   CHECK(run.steps == 5, "multipart: wanted start, 3 parts, complete; got %u requests",
         run.steps);
   CHECK(run.calls == 1 && run.success, "multipart: must report success once");
   for (i = 0; i < run.steps; i++)
   {
      const stub_request_t *req = &run.req[i];
      char want[64];
      if (strcmp(req->method, "PUT"))
         continue;
      snprintf(want, sizeof(want), "partNumber=%d&uploadId=up1", part + 1);
      CHECK(strstr(req->url, want) != NULL, "multipart: part %d went to %s",
            part + 1, req->url);
      if (part < 3)
      {
         sha_of(sha, big, (long)part * (long)BIG_PART, want_len[part]);
         CHECK(req->body_len == want_len[part], "multipart: part %d is %u bytes, wanted %u",
               part + 1, (unsigned)req->body_len, (unsigned)want_len[part]);
         CHECK(!strcmp(req->body_sha, sha),
               "multipart: part %d sent other bytes than its range of the file", part + 1);
      }
      part++;
   }
   CHECK(part == 3, "multipart: %d parts went up, wanted 3", part);

   script.initiate_status = -1;
   run_op(false, "saves/big.srm", true, &script, &run, big);
   sha_of(sha, big, 0, BIG_SIZE);
   i = (unsigned)find(&run, false, "PUT");
   CHECK(run.steps == 2 && find(&run, false, "PUT") == 1,
         "multipart fallback: wanted the start, then one PUT; got %u requests", run.steps);
   if (run.steps == 2 && i == 1)
   {
      CHECK(run.req[1].body_len == BIG_SIZE && !strcmp(run.req[1].body_sha, sha),
            "multipart fallback: the single PUT must send the whole file");
      CHECK(!strstr(run.req[1].url, "partNumber="),
            "multipart fallback: the single PUT must go to the object itself");
   }
   CHECK(run.calls == 1 && run.success, "multipart fallback: must report success once");
}

/* An upload of nothing still goes up, with the empty payload hash */
static void test_upload_empty(const char *empty)
{
   script_t script = {0};
   run_t    run;

   use_endpoint("https://bucket.s3.us-east-1.amazonaws.com/retroarch/");
   run_op(false, "saves/empty.srm", true, &script, &run, empty);
   CHECK(run.steps == 1 && run.req[0].body_len == 0
         && header_is(&run.req[0], "x-amz-content-sha256", S3_EMPTY_PAYLOAD_SHA256),
         "empty upload: one PUT, no body, the empty payload hash");
   CHECK(run.calls == 1 && run.success, "empty upload: must report success once");
}

/* The URLs and the backup's names are on the heap now, not 4 to 12 KiB
 * of stack: out of memory for them, nothing starts, nothing is
 * reported, and nothing leaks. */
static void test_names_out_of_memory(const char *tmp)
{
   static const char *const what[5] = {
      "backup then upload", "backup then delete", "upload", "delete", "read"
   };
   run_t    run;
   unsigned i;

   use_endpoint("https://bucket.s3.us-east-1.amazonaws.com/retroarch/");
   for (i = 0; i < 5; i++)
   {
      RFILE *rfile = NULL;
      bool   started;

      memset(&run, 0, sizeof(run));
      config_get_ptr()->bools.cloud_sync_destructive = i >= 2;
      stub_has_pending = false;
      if (i == 0 || i == 2)
         rfile = filestream_open(tmp, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE);
      fail_alloc = 1;
      if (i == 0 || i == 2)
         started = s3_update("saves/a.srm", rfile, on_done, &run);
      else if (i == 4)
         started = s3_read("saves/a.srm", tmp, on_done, &run);
      else
         started = s3_free("saves/a.srm", on_done, &run);
      fail_alloc = 0;
      CHECK(!started && !stub_has_pending && !run.calls,
            "%s, out of memory: it started anyway", what[i]);
      if (rfile)
         filestream_close(rfile);
   }
}

static char *sign_failing_alloc(unsigned nth)
{
   char *sig;

   fail_alloc = nth;
   sig        = s3_calculate_signature_v4_with_time("PUT", "/key", "uploadId=x",
         "host;x-amz-content-sha256;x-amz-date", S3_EMPTY_PAYLOAD_SHA256,
         "us-east-1", S3_SERVICE, "access", "secret", "example.com",
         NULL, 0);
   fail_alloc = 0;
   return sig;
}

/* The signer allocates nothing but its result, and the index of a query
 * with more parameters than it sorts in place.  Losing either must give
 * no signature: one made without the query only shows up later, as a
 * 403 from S3. */
static void test_signer_out_of_memory(void)
{
   static char many[128];
   char       *sig = sign_failing_alloc(0);
   int         n;
   int         i;

   CHECK(sig, "the signer must sign when memory is available");
   free(sig);

   sig = sign_failing_alloc(1);
   CHECK(!sig, "no memory for the signature, but it signed: %s", sig);
   free(sig);

   sig = sign_failing_alloc(2);
   CHECK(sig, "a short query must sign without another allocation");
   free(sig);

   for (n = 0, i = 0; i < 17; i++)
      n += snprintf(many + n, sizeof(many) - n, "%sq%02d=v", i ? "&" : "", i);
   fail_alloc = 2;
   sig        = s3_calculate_signature_v4_with_time("GET", "/", many,
         "host;x-amz-content-sha256;x-amz-date", S3_EMPTY_PAYLOAD_SHA256,
         "us-east-1", S3_SERVICE, "access", "secret", "example.com",
         NULL, 0);
   fail_alloc = 0;
   CHECK(!sig, "no memory for the query index, but it signed: %s", sig);
   free(sig);
}

struct query_sink
{
   size_t len;
   char   buf[512];
};

static void query_sink_emit(void *data, const char *s, size_t len)
{
   struct query_sink *q = (struct query_sink*)data;
   if (q->len + len < sizeof(q->buf))
   {
      memcpy(q->buf + q->len, s, len);
      q->len += len;
   }
   q->buf[q->len] = '\0';
}

/* SigV4 sorts query parameters by name, then by value.  Sorting the
 * whole "name=value" string would put "a1=2" before "a=1", because '1'
 * sorts below '='. */
static void test_query_order(void)
{
   static const char *cases[][2] = {
      { "a1=2&a=1",                  "a=1&a1=2" },
      { "a-b=1&a=2",                 "a=2&a-b=1" },
      { "uploadId=x&partNumber=3",   "partNumber=3&uploadId=x" },
      { "k=2&k=1",                   "k=1&k=2" },
      { "k=10&k=1",                  "k=1&k=10" },
      { "uploads=",                  "uploads=" },
      { "",                          "" },
      { "b=2&c=3&a=1&b=1",           "a=1&b=1&b=2&c=3" }
   };
   size_t i;

   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      struct query_sink q;
      bool              ok;
      q.len    = 0;
      q.buf[0] = '\0';
      ok       = s3_query_emit(cases[i][0], query_sink_emit, &q);
      CHECK(ok && !strcmp(q.buf, cases[i][1]),
            "query \"%s\": sorted to \"%s\", SigV4 wants \"%s\"",
            cases[i][0], q.buf, cases[i][1]);
   }
}

/* Signatures against an independent SigV4 computation (Python's
 * hashlib and hmac over the same canonical request), at time 0: the
 * plain requests, a CopyObject, a query too long for the signer's own
 * index, a URI past 4 KiB and a 150-octet secret, both signed whole. */
static void test_signature_known(void)
{
   static char many[256];
   static char long_uri[5002];
   static char long_secret[151];
   static const char *const E  = S3_EMPTY_PAYLOAD_SHA256;
   static const char *const HD = "host;x-amz-content-sha256;x-amz-date";
   struct
   {
      const char *method, *uri, *query, *region, *ak, *sk, *host, *copy;
      const char *want;
   } c[7];
   size_t i, n = 0;

   for (i = 20; i-- > 0;)
      n += (size_t)snprintf(many + n, sizeof(many) - n, "%sp%02u=%u",
            n ? "&" : "", (unsigned)i, (unsigned)(i % 3));
   long_uri[0] = '/';
   memset(long_uri + 1, 'k', 5000);
   long_uri[5001] = '\0';
   memset(long_secret, 'S', 150);
   long_secret[150] = '\0';

   c[0].method = "PUT";  c[0].uri = "/key"; c[0].query = "uploadId=x";
   c[0].region = "us-east-1"; c[0].ak = "access"; c[0].sk = "secret";
   c[0].host = "example.com"; c[0].copy = NULL;
   c[0].want = "d808fe25cbc9179929e8278876efe7eafa74c6f8e61b5fcd48f336e049b05f09";
   c[1].method = "GET";  c[1].uri = "/retroarch/saves/a.srm"; c[1].query = "";
   c[1].region = "eu-west-2"; c[1].ak = "AKIDEXAMPLE";
   c[1].sk = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
   c[1].host = "bucket.s3.eu-west-2.amazonaws.com"; c[1].copy = NULL;
   c[1].want = "88628b3cd21ef9099ec3e65c4206ffdbfbdcb1c8bcea85788f1369b50c8dce9c";
   c[2] = c[0]; c[2].uri = "/dst"; c[2].query = "partNumber=3&uploadId=abc";
   c[2].copy = "bucket/src%20name";
   c[2].want = "3b1f9dc7df0aa60e39acdbb7e83ffe00eff9d1dc48a31b667155c7728bc090c7";
   c[3] = c[0]; c[3].method = "POST"; c[3].uri = "/k"; c[3].query = "uploads=";
   c[3].want = "ea8e6b2d0d077940803de2f7528709ee5051d071c3dd9abff1cb2f2547b27456";
   c[4] = c[0]; c[4].method = "GET"; c[4].uri = "/"; c[4].query = many;
   c[4].want = "f3afef1486dc1ad64f77dc483359a0e40c69fcb4ffba1efc623f9735dc1a700d";
   c[5] = c[0]; c[5].uri = long_uri; c[5].query = "";
   c[5].want = "d5e860eac53dd460cea810f05454bbd084c3a37166a2a059ad362592f0efc2d3";
   c[6] = c[0]; c[6].query = ""; c[6].sk = long_secret;
   c[6].want = "789d5f99519bb1bc6cd055d8f9d60eaee5204e62d093b432428e7e80b77beb16";

   for (i = 0; i < sizeof(c) / sizeof(c[0]); i++)
   {
      char *sig = s3_calculate_signature_v4_with_time(c[i].method, c[i].uri,
            c[i].query, HD, E, c[i].region, S3_SERVICE, c[i].ak, c[i].sk,
            c[i].host, c[i].copy, 0);
      CHECK(sig && !strcmp(sig, c[i].want),
            "signature %u: %s, SigV4 gives %s", (unsigned)i,
            sig ? sig : "(none)", c[i].want);
      free(sig);
   }
}

int main(void)
{
   char  tmp[64];
   char  big[80];
   FILE *f;

   /* in the current directory: a native Windows build has no /tmp */
   snprintf(tmp, sizeof(tmp), "s3_backup_%ld", (long)getpid());
   if ((f = fopen(tmp, "wb")))
   {
      fputs("SAVEDATA", f);
      fclose(f);
   }

   test_update_virtual_hosted(tmp);
   test_update_path_style(tmp);
   test_delete(tmp);
   test_nothing_to_keep(tmp);
   test_copy_fails(tmp);
   test_no_backup_wanted(tmp);
   test_names_out_of_memory(tmp);

   snprintf(big, sizeof(big), "%s.big", tmp);
   if ((f = fopen(big, "wb")))
   {
      unsigned i;
      for (i = 0; i < BIG_SIZE; i++)
         fputc((int)((i * 31u + (i >> 13)) % 251u), f);
      fclose(f);
   }
   test_multipart_streamed(big);
   remove(big);

   snprintf(big, sizeof(big), "%s.empty", tmp);
   if ((f = fopen(big, "wb")))
      fclose(f);
   test_upload_empty(big);
   remove(big);

   test_signer_out_of_memory();
   test_query_order();
   test_signature_known();

   remove(tmp);

   if (failures)
   {
      printf("\n%u check(s) failed\n", failures);
      return 1;
   }
   printf("\nall S3 backup checks passed\n");
   return 0;
}

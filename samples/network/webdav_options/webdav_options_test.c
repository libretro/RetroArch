/* Harness for the WebDAV OPTIONS capability probe.
 *
 * Cloud sync opens with OPTIONS against the collection the user
 * configured, and what that response has to prove is "this is a WebDAV
 * server", nothing more. The distinction that matters is between a
 * WebDAV server and a plain HTTP server that answers OPTIONS happily
 * and refuses the first PUT: the first announces a DAV header (RFC
 * 4918 10.1) or offers methods only WebDAV defines, the second does
 * neither.
 *
 * PUT is deliberately not part of that test. Allow describes the
 * target resource (RFC 9110 5.6.1.2), the target here is always a
 * collection, and PUT is not defined on a collection (RFC 4918
 * 9.7.1) - so a correct server omits it and its absence says nothing.
 * Apache mod_dav omits it; Koofr's endpoint (libretro/RetroArch#19486)
 * was rejected over it while working fine in other WebDAV clients.
 * Those header sets are cases below.
 *
 * Includes the driver's translation unit so the probe is tested as
 * shipped rather than as a copy that can drift from it.
 */

#include <stdio.h>
#include <string.h>

#include <boolean.h>
#include <lists/string_list.h>

#include "../../../network/cloud_sync/webdav.c"

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

/* What the probe concluded, in the terms webdav_stat_cb() acts on:
 * verified means "treat as WebDAV", fatal means "stop the sync". */
typedef struct
{
   bool dav;
   bool allow_seen;
   bool allow_dav_method;
   bool verified;
   bool fatal;
} probe_result_t;

static void run_probe(const char **hdrs, size_t n, probe_result_t *out)
{
   http_transfer_data_t data;
   struct string_list  *list = string_list_new();
   size_t i;

   for (i = 0; i < n; i++)
      string_list_append(list, hdrs[i], (union string_list_elem_attr){0});

   data.data    = NULL;
   data.headers = list;
   data.len     = 0;
   data.status  = 200;

   out->dav              = false;
   out->allow_seen       = false;
   out->allow_dav_method = false;

   webdav_check_options(&data, &out->dav, &out->allow_seen,
         &out->allow_dav_method);

   /* Mirrors the decision in webdav_stat_cb(). */
   out->verified = out->dav || out->allow_dav_method;
   out->fatal    = (!out->verified && out->allow_seen);

   string_list_free(list);
}

/* --- real-world header sets ------------------------------------------ */

/* Apache mod_dav, OPTIONS on a collection. No PUT: you cannot PUT a
 * collection. It is unambiguously a WebDAV server all the same. */
static const char *hdrs_mod_dav_collection[] = {
   "HTTP/1.1 200 OK",
   "DAV: 1,2,<http://apache.org/dav/propset/fs/1>",
   "Allow: OPTIONS,GET,HEAD,POST,DELETE,TRACE,PROPFIND,PROPPATCH,COPY,MOVE,LOCK,UNLOCK",
   "Content-Length: 0"
};

/* A collection whose Allow carries WebDAV methods but which sends no
 * DAV header. Strictly non-compliant, common in the wild, and still a
 * WebDAV server. */
static const char *hdrs_no_dav_header[] = {
   "HTTP/1.1 200 OK",
   "Allow: OPTIONS, PROPFIND, MKCOL, DELETE, MOVE, COPY",
   "Content-Type: text/plain"
};

/* DAV header, no Allow at all: the header is the compliance
 * announcement and is enough on its own. */
static const char *hdrs_dav_only[] = {
   "HTTP/1.1 200 OK",
   "DAV: 1, 2, 3",
   "Server: nginx"
};

/* Lowercased field names over HTTP/2, as an Android build sees them. */
static const char *hdrs_h2_lowercase[] = {
   "HTTP/2 200",
   "dav: 1, 2",
   "allow: OPTIONS, GET, HEAD, DELETE, PROPFIND, PROPPATCH, COPY, MOVE",
   "content-length: 0"
};

/* A resource rather than a collection: PUT present. Must still pass,
 * and for the same reason as everything else - the DAV methods, not
 * the PUT. */
static const char *hdrs_resource_with_put[] = {
   "HTTP/1.1 200 OK",
   "DAV: 1",
   "Allow: OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND"
};

/* A plain HTTP server. Answers OPTIONS, knows nothing of WebDAV: the
 * one case the probe exists to catch. */
static const char *hdrs_plain_http[] = {
   "HTTP/1.1 200 OK",
   "Allow: OPTIONS, GET, HEAD, POST",
   "Server: nginx/1.24.0"
};

/* A plain HTTP server that offers PUT - a static file host, an object
 * store. The old probe passed this and failed mod_dav above, which is
 * exactly backwards. */
static const char *hdrs_plain_http_with_put[] = {
   "HTTP/1.1 200 OK",
   "Allow: OPTIONS, GET, HEAD, PUT, POST, DELETE",
   "Server: nginx/1.24.0"
};

/* Neither header. Nothing is proven either way, so this warns and
 * continues rather than failing the sync. */
static const char *hdrs_nothing[] = {
   "HTTP/1.1 200 OK",
   "Content-Length: 0"
};

/* --- token matching -------------------------------------------------- */

static void check_token_matching(void)
{
   /* Whole tokens only: a longer method containing a shorter one's
    * name must not match it. */
   CHECK(!webdav_allow_lists_method("OPTIONS, GET, PROPFINDX", "PROPFIND"),
         "PROPFINDX matched PROPFIND");
   CHECK(!webdav_allow_lists_method("OPTIONS, XMKCOL", "MKCOL"),
         "XMKCOL matched MKCOL");
   CHECK(webdav_allow_lists_method("OPTIONS,PROPFIND,GET", "PROPFIND"),
         "unspaced PROPFIND not matched");
   CHECK(webdav_allow_lists_method("OPTIONS,\tPROPFIND ,GET", "PROPFIND"),
         "tab/space padded PROPFIND not matched");
   CHECK(webdav_allow_lists_method("propfind", "PROPFIND"),
         "lowercase propfind not matched");
   CHECK(webdav_allow_lists_method("GET, PROPFIND", "PROPFIND"),
         "trailing PROPFIND not matched");
   CHECK(!webdav_allow_lists_method("", "PROPFIND"),
         "empty Allow matched");
   CHECK(!webdav_allow_lists_method("  ,, , ", "PROPFIND"),
         "separator-only Allow matched");
}

/* --------------------------------------------------------------------- */

int main(void)
{
   probe_result_t r;

   check_token_matching();

   run_probe(hdrs_mod_dav_collection, 4, &r);
   CHECK(r.verified, "mod_dav collection: not verified");
   CHECK(!r.fatal, "mod_dav collection: rejected (the #19486 regression)");
   CHECK(r.dav, "mod_dav collection: DAV header missed");

   run_probe(hdrs_no_dav_header, 3, &r);
   CHECK(r.verified, "Allow-only WebDAV: not verified");
   CHECK(!r.fatal, "Allow-only WebDAV: rejected");
   CHECK(r.allow_dav_method, "Allow-only WebDAV: methods missed");

   run_probe(hdrs_dav_only, 3, &r);
   CHECK(r.verified, "DAV-only: not verified");
   CHECK(!r.fatal, "DAV-only: rejected");

   run_probe(hdrs_h2_lowercase, 4, &r);
   CHECK(r.verified, "lowercase h2 headers: not verified");
   CHECK(!r.fatal, "lowercase h2 headers: rejected");
   CHECK(r.dav, "lowercase h2 headers: DAV header missed");

   run_probe(hdrs_resource_with_put, 3, &r);
   CHECK(r.verified, "resource with PUT: not verified");
   CHECK(!r.fatal, "resource with PUT: rejected");

   run_probe(hdrs_plain_http, 3, &r);
   CHECK(!r.verified, "plain HTTP: wrongly verified");
   CHECK(r.fatal, "plain HTTP: not caught");

   run_probe(hdrs_plain_http_with_put, 3, &r);
   CHECK(!r.verified, "plain HTTP with PUT: wrongly verified");
   CHECK(r.fatal, "plain HTTP with PUT: not caught");

   run_probe(hdrs_nothing, 2, &r);
   CHECK(!r.verified, "no headers: wrongly verified");
   CHECK(!r.fatal, "no headers: must warn and continue, not fail");

   /* No headers at all on the response. */
   {
      http_transfer_data_t data;
      bool dav = false, seen = false, method = false;

      data.data    = NULL;
      data.headers = NULL;
      data.len     = 0;
      data.status  = 200;
      webdav_check_options(&data, &dav, &seen, &method);
      CHECK(!dav && !seen && !method, "NULL header list: probe invented a result");
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("webdav_check_options: all header sets classified correctly\n");
   return 0;
}

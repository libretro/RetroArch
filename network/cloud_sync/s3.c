/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <encodings/base64.h>
#include <lrc_hash.h>
#include <crypto/crypto.h>
#include <crypto/kdf.h>
#include <string/stdstring.h>
#include <time/rtime.h>
#include <formats/rjson.h>
#include <retro_miscellaneous.h>

#include "../cloud_sync_driver.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"
#include <compat/strl.h>

#define S3_PFX "[S3] "

/* S3 multipart constants (AWS): 5 MiB min part size, 5 GiB max part size.
 * The nominal multipart threshold below is a local policy trigger, not an S3 limit. */
#define S3_MULTIPART_MIN_PART_SIZE_BYTES (5ULL * 1024ULL * 1024ULL)
#define S3_MULTIPART_MAX_PART_SIZE_BYTES (5ULL * 1024ULL * 1024ULL * 1024ULL)
#define S3_MAX_PARTS 10000
#define S3_EMPTY_PAYLOAD_SHA256 "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
#define S3_SIGNATURE_VERSION "AWS4-HMAC-SHA256"
#define S3_SERVICE "s3"

/* A test can set its own, down to the 5 MiB a part has at least */
#ifndef S3_MULTIPART_NOMINAL_THRESHOLD_BYTES
#if defined(_3DS) || defined(VITA) || defined(PSP) || defined(PS2) || defined(WIIU) || defined(GEKKO)
#define S3_MULTIPART_NOMINAL_THRESHOLD_BYTES (16ULL * 1024ULL * 1024ULL)
#elif defined(HAVE_LIBNX)
#define S3_MULTIPART_NOMINAL_THRESHOLD_BYTES (32ULL * 1024ULL * 1024ULL)
#else
#define S3_MULTIPART_NOMINAL_THRESHOLD_BYTES (64ULL * 1024ULL * 1024ULL)
#endif
#endif

/* A byte range of the file being uploaded, pulled by net_http on the
 * task thread as the socket takes it */
typedef struct
{
   int64_t start;
   RFILE  *file;
   size_t  len;
   size_t  pos;
} s3_body_t;

typedef struct
{
   s3_body_t body;
   char path[PATH_MAX_LENGTH];
   char file[PATH_MAX_LENGTH];
   cloud_sync_complete_handler_t cb;
   void *user_data;
   RFILE *rfile;
} s3_cb_state_t;

typedef struct
{
   char url[PATH_MAX_LENGTH];
   char bucket[NAME_MAX_LENGTH];
   char region[NAME_MAX_LENGTH];
   char host[NAME_MAX_LENGTH];
   char access_key_id[128];
   char secret_access_key[186];
   bool connected;
} s3_state_t;

typedef struct
{
   s3_cb_state_t *cb_state;
   char url[PATH_MAX_LENGTH];
   char canonical_uri[PATH_MAX_LENGTH];
   s3_body_t part;
   RFILE *file;
   size_t file_size;
   size_t part_size;
   size_t part_count;
   size_t current_part;
   char *upload_id;
   char **part_etags;
   bool aborting;
} s3_multipart_state_t;

static s3_state_t s3_driver_st = {0};

s3_state_t *s3_state_get_ptr(void)
{
   return &s3_driver_st;
}

static size_t s3_get_multipart_nominal_threshold_bytes(void)
{
   size_t threshold = (size_t)S3_MULTIPART_NOMINAL_THRESHOLD_BYTES;
   if (threshold < (size_t)S3_MULTIPART_MIN_PART_SIZE_BYTES)
      threshold = (size_t)S3_MULTIPART_MIN_PART_SIZE_BYTES;
   if (threshold > (size_t)S3_MULTIPART_MAX_PART_SIZE_BYTES)
      threshold = (size_t)S3_MULTIPART_MAX_PART_SIZE_BYTES;
   return threshold;
}

static bool s3_calculate_multipart_plan(size_t file_size, size_t *out_part_size, size_t *out_part_count)
{
   size_t part_size  = s3_get_multipart_nominal_threshold_bytes();
   size_t part_count = 0;

   if (!out_part_size || !out_part_count || file_size == 0)
      return false;

   if (part_size < (size_t)S3_MULTIPART_MIN_PART_SIZE_BYTES)
      part_size = (size_t)S3_MULTIPART_MIN_PART_SIZE_BYTES;
   if (part_size > (size_t)S3_MULTIPART_MAX_PART_SIZE_BYTES)
      part_size = (size_t)S3_MULTIPART_MAX_PART_SIZE_BYTES;

   part_count = (file_size + part_size - 1) / part_size;

   if (part_count > S3_MAX_PARTS)
   {
      part_size = (file_size + S3_MAX_PARTS - 1) / S3_MAX_PARTS;
      if (part_size < (size_t)S3_MULTIPART_MIN_PART_SIZE_BYTES)
         part_size = (size_t)S3_MULTIPART_MIN_PART_SIZE_BYTES;
      if (part_size > (size_t)S3_MULTIPART_MAX_PART_SIZE_BYTES)
         return false;

      part_count = (file_size + part_size - 1) / part_size;
      if (part_count > S3_MAX_PARTS)
         return false;
   }

   *out_part_size  = part_size;
   *out_part_count = part_count;
   return true;
}

/* Parse S3 URL to extract bucket, region, and host. Supports both virtual-hosted
 * and path-style layouts so providers can be normalized from one URL string. */
static bool s3_parse_url(const char *url, char *bucket,  char *region,
                         char *host, char *service)
{
   const char *scheme_sep    = NULL;
   const char *authority     = NULL;
   const char *authority_end = NULL;
   const char *path_start    = NULL;
   const char *first_dot     = NULL;
   const char *second_dot    = NULL;
   char first_label[NAME_MAX_LENGTH];
   char first_path_seg[NAME_MAX_LENGTH];

   (void)service;

   if (!url || !bucket || !region || !host)
      return false;

   RARCH_DBG(S3_PFX "Parsing S3 URL: %s\n", url);

   bucket[0] = '\0';
   region[0] = '\0';
   host[0]   = '\0';
   first_label[0]    = '\0';
   first_path_seg[0] = '\0';

   scheme_sep = strstr(url, "://");
   authority  = scheme_sep ? scheme_sep + 3 : url;
   if (!authority || !*authority)
      return false;

   authority_end = strchr(authority, '/');
   if (!authority_end)
      authority_end = authority + strlen(authority);

   if (authority_end > authority)
   {
      size_t host_len = authority_end - authority;
      if (host_len >= NAME_MAX_LENGTH)
         host_len = NAME_MAX_LENGTH - 1;
      strlcpy(host, authority, host_len + 1);
   }

   if (!host || !*host)
      return false;

   if (*authority_end == '/')
      path_start = authority_end + 1;

   /* Capture first host label and first path segment for provider-specific rules. */
   first_dot = strchr(host, '.');
   if (first_dot)
   {
      size_t first_label_len = first_dot - host;
      if (first_label_len > 0 && first_label_len < NAME_MAX_LENGTH)
         strlcpy(first_label, host, first_label_len + 1);
   }

   if (path_start && *path_start)
   {
      const char *path_seg_end = strchr(path_start, '/');
      size_t path_seg_len      = path_seg_end ? (size_t)(path_seg_end - path_start) : strlen(path_start);
      if (path_seg_len > 0 && path_seg_len < NAME_MAX_LENGTH)
         strlcpy(first_path_seg, path_start, path_seg_len + 1);
   }

   if (strstr(host, ".cloudflarestorage.com"))
   {
      const char *r2_suffix = ".r2.cloudflarestorage.com";
      const char *r2_pos    = strstr(host, r2_suffix);

      RARCH_LOG(S3_PFX "Detected Cloudflare R2 format\n");

      if (r2_pos)
      {
         /* Cloudflare R2 supports both:
          * - <bucket>.<accountid>.r2.cloudflarestorage.com
          * - <accountid>.r2.cloudflarestorage.com/<bucket>/... */
         const char *prefix_start = host;
         size_t prefix_len         = (size_t)(r2_pos - prefix_start);

         if (prefix_len > 0)
         {
            const char *prefix_dot = strchr(prefix_start, '.');

            /* If there are two labels before ".r2..." then first label is bucket. */
            if (prefix_dot && prefix_dot < r2_pos)
            {
               size_t bucket_len = (size_t)(prefix_dot - prefix_start);
               if (bucket_len > 0 && bucket_len < NAME_MAX_LENGTH)
               {
                  RARCH_LOG(S3_PFX "Cloudflare R2 style: virtual-hosted\n");
                  strlcpy(bucket, prefix_start, bucket_len + 1);
               }
            }
         }

         /* Path-style endpoint (accountid.r2...) carries bucket in first path segment. */
         if ((!bucket || !*bucket) && *first_path_seg)
         {
            RARCH_LOG(S3_PFX "Cloudflare R2 style: path-style\n");
            strlcpy(bucket, first_path_seg, NAME_MAX_LENGTH);
         }
      }
      else if (*first_label)
      {
         /* Compatibility fallback for other cloudflarestorage host variants. */
         RARCH_LOG(S3_PFX "Cloudflare R2 style: fallback-host-label\n");
         strlcpy(bucket, first_label, NAME_MAX_LENGTH);
      }

      /* Cloudflare R2 uses region "auto" for AWS SigV4. */
      strlcpy_lit(region, "auto", NAME_MAX_LENGTH);
   }
   else if (strstr(host, ".amazonaws.com"))
   {
      const char *s3pos = strstr(host, ".s3.");
      const char *s3dash = strstr(host, ".s3-");
      const char *s3accelerate = strstr(host, ".s3-accelerate.");
      const char *s3acceleration = strstr(host, ".s3-acceleration.");
      const char *host_start = host;

      RARCH_LOG(S3_PFX "Detected AWS S3 format\n");

      if (s3accelerate || s3acceleration)
      {
         const char *accel_pos = s3accelerate ? s3accelerate : s3acceleration;
         size_t bucket_len = accel_pos - host_start;
         if (bucket_len > 0 && bucket_len < NAME_MAX_LENGTH)
            strlcpy(bucket, host_start, bucket_len + 1);

         /* Transfer acceleration endpoints do not encode bucket region in hostname.
          * Keep fallback-compatible behavior by using us-east-1 here. */
         strlcpy_lit(region, "us-east-1", NAME_MAX_LENGTH);
      }
      else if (s3pos)
      {
         size_t bucket_len = s3pos - host_start;
         if (bucket_len > 0 && bucket_len < NAME_MAX_LENGTH)
            strlcpy(bucket, host_start, bucket_len + 1);
         s3pos += 4; /* skip ".s3." */
         second_dot = strchr(s3pos, '.');
         if (second_dot)
         {
            size_t region_len = second_dot - s3pos;
            if (region_len > 0 && region_len < NAME_MAX_LENGTH)
               strlcpy(region, s3pos, region_len + 1);
         }
      }
      else if (s3dash)
      {
         size_t bucket_len = s3dash - host_start;
         if (bucket_len > 0 && bucket_len < NAME_MAX_LENGTH)
            strlcpy(bucket, host_start, bucket_len + 1);
         s3dash += 4; /* skip ".s3-" */
         second_dot = strchr(s3dash, '.');
         if (second_dot)
         {
            size_t region_len = second_dot - s3dash;
            if (region_len > 0 && region_len < NAME_MAX_LENGTH)
               strlcpy(region, s3dash, region_len + 1);
         }
      }
      else
      {
         /* Path-style fallback: s3.<region>.amazonaws.com/<bucket>/... */
         if (*first_path_seg)
            strlcpy(bucket, first_path_seg, NAME_MAX_LENGTH);
      }
   }
   else if (strstr(host, ".backblazeb2.com"))
   {
      const char *s3pos = strstr(host, ".s3.");
      bool b2_path_style_host = false;

      RARCH_LOG(S3_PFX "Detected Backblaze B2 format\n");

      /* Backblaze B2 path-style host: s3.<region>.backblazeb2.com/<bucket>/... */
      if (string_starts_with_case_insensitive(host, "s3."))
      {
         const char *region_start = host + 3; /* skip "s3." */
         second_dot = strchr(region_start, '.');
         if (second_dot)
         {
            size_t region_len = second_dot - region_start;
            if (region_len > 0 && region_len < NAME_MAX_LENGTH)
            {
               strlcpy(region, region_start, region_len + 1);
               b2_path_style_host = true;
            }
         }
      }

      if (s3pos && !b2_path_style_host)
      {
         size_t bucket_len;
         RARCH_LOG(S3_PFX "Backblaze B2 style: virtual-hosted\n");
         bucket_len = s3pos - host;
         if (bucket_len > 0 && bucket_len < NAME_MAX_LENGTH)
            strlcpy(bucket, host, bucket_len + 1);
         s3pos += 4; /* skip ".s3." */
         second_dot = strchr(s3pos, '.');
         if (second_dot)
         {
            size_t region_len = second_dot - s3pos;
            if (region_len > 0 && region_len < NAME_MAX_LENGTH)
               strlcpy(region, s3pos, region_len + 1);
         }
      }
      else if (*first_path_seg)
      {
         RARCH_LOG(S3_PFX "Backblaze B2 style: path-style\n");
         strlcpy(bucket, first_path_seg, NAME_MAX_LENGTH);
      }
   }
   else if (strstr(host, ".digitaloceanspaces.com") || strstr(host, ".linodeobjects.com"))
   {
      if (*first_label)
         strlcpy(bucket, first_label, NAME_MAX_LENGTH);
      if (first_dot)
      {
         const char *region_start = first_dot + 1;
         second_dot = strchr(region_start, '.');
         if (second_dot)
         {
            size_t region_len = second_dot - region_start;
            if (region_len > 0 && region_len < NAME_MAX_LENGTH)
               strlcpy(region, region_start, region_len + 1);
         }
      }
   }
   else
   {
      /* Generic fallback:
       * - virtual-host: bucket.endpoint
       * - path-style: endpoint/bucket */
      if (*first_path_seg)
         strlcpy(bucket, first_path_seg, NAME_MAX_LENGTH);
      else if (*first_label)
         strlcpy(bucket, first_label, NAME_MAX_LENGTH);
   }

   if (!region || !*region)
      strlcpy_lit(region, "us-east-1", NAME_MAX_LENGTH);

   if (!*bucket)
      RARCH_WARN(S3_PFX "Could not extract bucket from URL: %s\n", url);

   RARCH_LOG(S3_PFX "Extracted bucket: %s, region: %s, host: %s\n",
             (!bucket || !*bucket) ? "(none)" : bucket, region, host);

   return (host && *host) && (bucket && *bucket);
}

/* Calculate SHA256 hash */
static char* s3_sha256_hash(const char *data, size_t len)
{
   char *hash = malloc(65);
   if (!hash)
      return NULL;

   sha256_hash(hash, (const uint8_t*)data, len);
   hash[64] = '\0';
   return hash;
}

static void s3_hex32(char *s, const uint8_t *digest)
{
   static const char hex[] = "0123456789abcdef";
   unsigned i;
   for (i = 0; i < 32; i++)
   {
      s[2 * i]     = hex[digest[i] >> 4];
      s[2 * i + 1] = hex[digest[i] & 15];
   }
   s[64] = '\0';
}

/* Read in runs this long to hash a range of the file */
#define S3_HASH_RUN (64 * 1024)

/* The SHA-256 of @len bytes of @file from @start, in hex, read in runs
 * rather than whole: the body is hashed here, then read again as the
 * socket takes it. NULL if the file reads short or out of memory. */
static char *s3_sha256_file(RFILE *file, int64_t start, size_t len)
{
   struct sha256_state st;
   uint8_t  digest[32];
   uint8_t *run;
   char    *hash = NULL;

   if (!(run = (uint8_t*)malloc(len < S3_HASH_RUN ? (len ? len : 1) : S3_HASH_RUN)))
      return NULL;
   if (filestream_seek(file, start, SEEK_SET) < 0)
      goto done;
   sha256_stream_init(&st, 0);
   while (len)
   {
      size_t n = len < S3_HASH_RUN ? len : S3_HASH_RUN;
      if (filestream_read(file, run, (int64_t)n) != (int64_t)n)
         goto done;
      sha256_stream_update(&st, run, n);
      len -= n;
   }
   sha256_stream_final(&st, digest);
   if ((hash = (char*)malloc(65)))
      s3_hex32(hash, digest);
done:
   free(run);
   return hash;
}

/* net_http's source for an upload body: the next run of the range.
 * The first run seeks to the range, so each part of a multipart
 * upload reads its own bytes whatever was read before it. */
static int64_t s3_body_source(void *data, void *buf, size_t len)
{
   s3_body_t *b = (s3_body_t*)data;
   int64_t    got;

   if (len > b->len - b->pos)
      len = b->len - b->pos;
   if (!len)
      return 0;
   if (!b->pos && filestream_seek(b->file, b->start, SEEK_SET) < 0)
      return -1;
   if ((got = filestream_read(b->file, buf, (int64_t)len)) > 0)
      b->pos += (size_t)got;
   return got;
}

/* A replay on a fresh connection sends the range again from its start */
static bool s3_body_rewind(void *data)
{
   ((s3_body_t*)data)->pos = 0;
   return true;
}

static void s3_body_set(s3_body_t *b, RFILE *file, int64_t start, size_t len)
{
   b->start = start;
   b->file  = file;
   b->len   = len;
   b->pos   = 0;
}

/* URL encode a string according to RFC 3986 */
static char* s3_url_encode(const char *input)
{
   size_t input_len;
   size_t output_pos = 0;
   char *output;
   size_t i;

   /* NULL-check input BEFORE calling strlen.  The previous form
    * ran strlen(input) first (line-order: strlen, malloc, then
    * 'if (!input)'), so the 'if (!input)' check was dead code -
    * a NULL input segfaulted in strlen before we ever reached
    * the guard. */
   if (!input)
      return NULL;

   input_len = strlen(input);

   /* Worst case: every char needs encoding */
   if (!(output = malloc(input_len * 3 + 1)))
      return NULL;

   for (i = 0; i < input_len; i++)
   {
      unsigned char c = (unsigned char)input[i];

      /* RFC 3986 unreserved characters: A-Z, a-z, 0-9, -, ., _, ~ */
      /* Path delimiters that should not be encoded: / */
      /* This encodes the object key only, so '&', '=' and '?' are
       * key characters and must be encoded like any other. */
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
          c == '~' || c == '/')
      {
         output[output_pos++] = c;
      }
      else
      {
         /* Percent encode */
         output[output_pos++] = '%';
         output[output_pos++] = "0123456789ABCDEF"[c >> 4];
         output[output_pos++] = "0123456789ABCDEF"[c & 0x0F];
      }
   }
   output[output_pos] = '\0';
   return output;
}

/* One query parameter: a span of the caller's string, and how much of
 * it is the name. */
typedef struct
{
   const char *p;
   size_t      len;
   size_t      name;
} s3_query_param_t;

/* Parameters sorted without allocating, past which the index is on
 * the heap. Every request this client signs has one or two. */
#define S3_QUERY_INLINE 16

/* qsort comparator: SigV4 sorts query parameters by URL-encoded name,
 * then by URL-encoded value, in byte order.  The names are compared on
 * their own: on the whole "name=value" string, a name that extends
 * another ("a1" after "a") would meet the '=' and sort first, since
 * digits, '-', '.' and '%' all sort below '='. */
static int s3_query_param_cmp(const void *a, const void *b)
{
   const s3_query_param_t *x = (const s3_query_param_t*)a;
   const s3_query_param_t *y = (const s3_query_param_t*)b;
   size_t xr                 = x->len - x->name;
   size_t yr                 = y->len - y->name;
   int    r                  = memcmp(x->p, y->p,
         x->name < y->name ? x->name : y->name);

   if (r)
      return r;
   if (x->name != y->name)
      return x->name < y->name ? -1 : 1;
   if ((r = memcmp(x->p + x->name, y->p + y->name, xr < yr ? xr : yr)))
      return r;
   return xr == yr ? 0 : (xr < yr ? -1 : 1);
}

typedef void (*s3_emit_t)(void *data, const char *s, size_t len);

/* The canonical query string per SigV4, handed to @emit piece by
 * piece: @query's parameters, sorted with s3_query_param_cmp, joined
 * by '&'. Names and values arrive URL-encoded, and every parameter as
 * name=value, even with an empty value ("uploads="). False only out of
 * memory. */
static bool s3_query_emit(const char *query, s3_emit_t emit, void *data)
{
   s3_query_param_t  inline_params[S3_QUERY_INLINE];
   s3_query_param_t *params = inline_params;
   const char       *q;
   size_t            count  = 1;
   size_t            i;

   if (!query || !*query)
      return true;
   for (q = query; *q; q++)
      if (*q == '&')
         count++;
   if (     count > S3_QUERY_INLINE
         && !(params = (s3_query_param_t*)malloc(count * sizeof(*params))))
      return false;

   for (i = 0, q = query; i < count; i++)
   {
      const char *amp = strchr(q, '&');
      size_t      len = amp ? (size_t)(amp - q) : strlen(q);
      const char *eq  = (const char*)memchr(q, '=', len);
      params[i].p     = q;
      params[i].len   = len;
      params[i].name  = eq ? (size_t)(eq - q) : len;
      q              += len + (amp ? 1 : 0);
   }

   qsort(params, count, sizeof(*params), s3_query_param_cmp);

   for (i = 0; i < count; i++)
   {
      if (i)
         emit(data, "&", 1);
      emit(data, params[i].p, params[i].len);
   }

   if (params != inline_params)
      free(params);
   return true;
}

/* @s, NUL-terminated, into the hash or the MAC being made */
#define S3_SHA_STR(st, str) \
   sha256_stream_update((st), (const uint8_t*)(str), strlen(str))
#define S3_MAC_STR(ctx, str) \
   hmac_sha256_update((ctx), (const uint8_t*)(str), strlen(str))

static void s3_sha_emit(void *data, const char *s, size_t len)
{
   sha256_stream_update((struct sha256_state*)data, (const uint8_t*)s, len);
}

/* AWS Signature Version 4 calculation with provided timestamp */
/* @copy_source, when set, is the x-amz-copy-source value of a
 * CopyObject request.  S3 requires every x-amz-* header a request
 * carries to be signed, so it joins the canonical and signed headers
 * in its sorted place, between x-amz-content-sha256 and x-amz-date.
 * The canonical request is hashed as it is made, and the string to
 * sign MACed the same way: nothing is assembled, so nothing is cut
 * short. Returns the signature in hex, NULL out of memory. */
static char* s3_calculate_signature_v4_with_time(const char *method, const char *canonical_uri,
                                      const char *query_string, const char *headers,
                                      const char *payload_hash, const char *region,
                                      const char *service, const char *access_key,
                                      const char *secret_key, const char *host,
                                      const char *copy_source, time_t now)
{
   struct sha256_state    cr;
   struct hmac_sha256_ctx mac;
   uint8_t  digest[32];
   uint8_t  key[32];
   char     cr_hash[65];
   char     date[16];
   char     datetime[32];
   /* "AWS4" and the secret, which the settings keep to 185 octets */
   char     aws4_key[4 + 186];
   size_t   secret_len = strlen(secret_key);
   char    *signature;
   struct tm *tm_info = gmtime(&now);

   (void)access_key;
   if (secret_len > sizeof(aws4_key) - 4)
   {
      RARCH_ERR(S3_PFX "Secret access key too long to sign with\n");
      return NULL;
   }
   if (!(signature = (char*)malloc(65)))
      return NULL;
   strftime(date, sizeof(date), "%Y%m%d", tm_info);
   strftime(datetime, sizeof(datetime), "%Y%m%dT%H%M%SZ", tm_info);

   /* The canonical request, into its hash */
   sha256_stream_init(&cr, 0);
   S3_SHA_STR(&cr, method);
   S3_SHA_STR(&cr, "\n");
   S3_SHA_STR(&cr, canonical_uri);
   S3_SHA_STR(&cr, "\n");
   /* Out of memory, the request is not signed at all: a signature made
    * without the query only shows up later, as a 403. */
   if (!s3_query_emit(query_string, s3_sha_emit, &cr))
   {
      free(signature);
      return NULL;
   }
   S3_SHA_STR(&cr, "\n");
   if (headers)
   {
      /* sorted, lowercase, each ending in a newline */
      S3_SHA_STR(&cr, "host:");
      S3_SHA_STR(&cr, host);
      S3_SHA_STR(&cr, "\nx-amz-content-sha256:");
      S3_SHA_STR(&cr, payload_hash);
      if (copy_source)
      {
         S3_SHA_STR(&cr, "\nx-amz-copy-source:");
         S3_SHA_STR(&cr, copy_source);
      }
      S3_SHA_STR(&cr, "\nx-amz-date:");
      S3_SHA_STR(&cr, datetime);
      S3_SHA_STR(&cr, "\n");
   }
   S3_SHA_STR(&cr, "\n");
   S3_SHA_STR(&cr, copy_source
         ? "host;x-amz-content-sha256;x-amz-copy-source;x-amz-date"
         : "host;x-amz-content-sha256;x-amz-date");
   S3_SHA_STR(&cr, "\n");
   S3_SHA_STR(&cr, payload_hash);
   sha256_stream_final(&cr, digest);
   s3_hex32(cr_hash, digest);

   /* The signing key: HMAC chained from "AWS4" and the secret, through
    * the date, the region, the service and "aws4_request" */
   memcpy(aws4_key, "AWS4", 4);
   memcpy(aws4_key + 4, secret_key, secret_len);
   hmac_sha256((const uint8_t*)aws4_key, 4 + secret_len,
         (const uint8_t*)date, strlen(date), key);
   hmac_sha256(key, 32, (const uint8_t*)region, strlen(region), key);
   hmac_sha256(key, 32, (const uint8_t*)service, strlen(service), key);
   hmac_sha256(key, 32, (const uint8_t*)"aws4_request", 12, key);

   /* The string to sign, into its MAC */
   hmac_sha256_init(&mac, key, 32);
   S3_MAC_STR(&mac, S3_SIGNATURE_VERSION "\n");
   S3_MAC_STR(&mac, datetime);
   S3_MAC_STR(&mac, "\n");
   S3_MAC_STR(&mac, date);
   S3_MAC_STR(&mac, "/");
   S3_MAC_STR(&mac, region);
   S3_MAC_STR(&mac, "/");
   S3_MAC_STR(&mac, service);
   S3_MAC_STR(&mac, "/aws4_request\n");
   S3_MAC_STR(&mac, cr_hash);
   hmac_sha256_final(&mac, digest);
   s3_hex32(signature, digest);
   crypto_memzero(aws4_key, sizeof(aws4_key));
   crypto_memzero(key, sizeof(key));
   return signature;
}



/* Build S3 authorization header */
static char* s3_build_auth_header_ex(const char *method, const char *canonical_uri,
                                 const char *query_string, const char *headers,
                                 const char *payload_hash, const char *copy_source,
                                 s3_state_t *s3_st)
{
   char *auth_header = NULL;
   char *signature = NULL;
   char date[16];
   char datetime[32];
   char credential[256];
   time_t now;
   struct tm *tm_info;

   /* Get current time - use the same timestamp for signature and headers */
   now = time(NULL);
   tm_info = gmtime(&now);
   strftime(date, sizeof(date), "%Y%m%d", tm_info);
   strftime(datetime, sizeof(datetime), "%Y%m%dT%H%M%SZ", tm_info);

   /* Calculate signature with the same timestamp */
   signature = s3_calculate_signature_v4_with_time(method, canonical_uri, query_string, headers,
                                        payload_hash, s3_st->region, S3_SERVICE,
                                        s3_st->access_key_id, s3_st->secret_access_key,
                                        s3_st->host, copy_source, now);

   if (!signature)
      return NULL;

   /* Build credential string */
   snprintf(credential, sizeof(credential), "%s/%s/%s/%s/aws4_request",
            s3_st->access_key_id, date, s3_st->region, S3_SERVICE);

   /* Build authorization header */
   /* NULL-check the malloc: snprintf-into-NULL segfaults.  Also
    * free 'signature' on the OOM path - the success path at line
    * below already free's it, but the pre-patch code would have
    * crashed before reaching that free. */
   {
      size_t header_len = 1024 + (copy_source ? strlen(copy_source) : 0);
      if (!(auth_header = malloc(header_len)))
      {
         free(signature);
         return NULL;
      }
      if (copy_source)
         snprintf(auth_header, header_len,
               "Authorization: %s Credential=%s, SignedHeaders=host;x-amz-content-sha256;x-amz-copy-source;x-amz-date, Signature=%s\r\n"
               "x-amz-date: %s\r\n"
               "x-amz-content-sha256: %s\r\n"
               "x-amz-copy-source: %s\r\n",
               S3_SIGNATURE_VERSION, credential, signature, datetime, payload_hash,
               copy_source);
      else
         snprintf(auth_header, header_len,
               "Authorization: %s Credential=%s, SignedHeaders=host;x-amz-content-sha256;x-amz-date, Signature=%s\r\n"
               "x-amz-date: %s\r\n"
               "x-amz-content-sha256: %s\r\n",
               S3_SIGNATURE_VERSION, credential, signature, datetime, payload_hash);
   }

   free(signature);
   return auth_header;
}

static char* s3_build_auth_header(const char *method, const char *canonical_uri,
                                 const char *query_string, const char *headers,
                                 const char *payload_hash, s3_state_t *s3_st)
{
   return s3_build_auth_header_ex(method, canonical_uri, query_string,
         headers, payload_hash, NULL, s3_st);
}

static void s3_log_http_failure(const char *path,
      http_transfer_data_t *data, const char *err)
{
   const char *h;
   /* No status means the transport failed before the server answered;
    * the task's error names the stage and its code, as in webdav.c. */
   if (data->status < 0 && err && *err)
      RARCH_WARN(S3_PFX "Failed: %s: HTTP %d (%s)\n",
            path ? path : "<unknown>", data->status, err);
   else
      RARCH_WARN(S3_PFX "Failed: %s: HTTP %d\n",
            path ? path : "<unknown>", data->status);
   for (h = net_http_header_next(data->headers, NULL); h;
         h = net_http_header_next(data->headers, h))
      RARCH_WARN(S3_PFX "%s\n", h);
   /* See webdav.c: the buffer is sized exactly to data->len, so
    * writing a terminator at data->data[data->len] overflows the
    * heap chunk by one byte.  Use the length-bounded form. */
   if (data->data)
      RARCH_WARN(S3_PFX "%.*s\n", (int)data->len, (const char*)data->data);
}

static void s3_read_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_cb_state_t *s3_cb_st       = (s3_cb_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && ((data->status >= 200 && data->status < 300) || data->status == 404 || data->status == 400));
   RFILE *file                   = NULL;

   (void)task;

   if (!s3_cb_st)
      return;

   if (!data)
      RARCH_WARN(S3_PFX "Did not get HTTP data for read '%s'%s%s\n",
            s3_cb_st->path, (err && *err) ? ": " : "", (err && *err) ? err : "");
   else if (!success)
      s3_log_http_failure(s3_cb_st->path, data, err);

   if (success && data && data->status >= 200 && data->status < 300)
   {
      file = filestream_open(s3_cb_st->file,
            RETRO_VFS_FILE_ACCESS_READ_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (!file)
      {
         RARCH_WARN(S3_PFX "Failed to open local file for read '%s'\n", s3_cb_st->file);
         success = false;
      }
      else
      {
         if (data->data && data->len > 0)
            filestream_write(file, data->data, data->len);
         filestream_seek(file, 0, SEEK_SET);
      }
   }

   if (s3_cb_st->cb)
      s3_cb_st->cb(s3_cb_st->user_data, s3_cb_st->path, success, file);
   free(s3_cb_st);
}

static void s3_update_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_cb_state_t *s3_cb_st       = (s3_cb_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && data->status >= 200 && data->status < 300);

   (void)task;

   if (!s3_cb_st)
      return;

   if (!data)
      RARCH_WARN(S3_PFX "Did not get HTTP data for update '%s'%s%s\n",
            s3_cb_st->path, (err && *err) ? ": " : "", (err && *err) ? err : "");
   else if (!success)
      s3_log_http_failure(s3_cb_st->path, data, err);

   if (s3_cb_st->cb)
      s3_cb_st->cb(s3_cb_st->user_data, s3_cb_st->path, success, s3_cb_st->rfile);
   free(s3_cb_st);
}

static void s3_delete_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_cb_state_t *s3_cb_st       = (s3_cb_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && data->status >= 200 && data->status < 300);

   (void)task;

   if (!s3_cb_st)
      return;

   if (!data)
      RARCH_WARN(S3_PFX "Did not get HTTP data for delete '%s'%s%s\n",
            s3_cb_st->path, (err && *err) ? ": " : "", (err && *err) ? err : "");
   else if (!success)
      s3_log_http_failure(s3_cb_st->path, data, err);

   if (s3_cb_st->cb)
      s3_cb_st->cb(s3_cb_st->user_data, s3_cb_st->path, success, NULL);
   free(s3_cb_st);
}

static bool s3_http_get_async(const char *url, const char *headers, s3_cb_state_t *cb_state)
{
   return task_push_http_transfer_with_headers(url, true, NULL,
         headers, s3_read_cb, cb_state) != NULL;
}

/* PUT @cb_state's body, streamed from its file */
static bool s3_http_put_async(const char *url, const char *headers,
      s3_cb_state_t *cb_state)
{
   if (!cb_state->body.len)
      return task_push_http_transfer_with_content(url, "PUT",
            NULL, 0, "application/octet-stream", true, false,
            headers, s3_update_cb, cb_state) != NULL;
   return task_push_http_transfer_with_source(url, "PUT",
         s3_body_source, s3_body_rewind, &cb_state->body, cb_state->body.len,
         "application/octet-stream", true, false,
         headers, s3_update_cb, cb_state) != NULL;
}

static bool s3_http_delete_async(const char *url, const char *headers, s3_cb_state_t *cb_state)
{
   return task_push_http_transfer_with_headers(url, true, "DELETE",
         headers, s3_delete_cb, cb_state) != NULL;
}

/* Normalize object key path to avoid malformed request URLs:
 * - strip leading '/'
 * - collapse repeated '/' inside key */
static char* s3_normalize_object_key(const char *path)
{
   const char *src = path;
   size_t src_len = 0;
   char *normalized = NULL;
   size_t out = 0;
   bool prev_slash = false;

   if (!path || !*path)
      return strdup("");

   while (*src == '/')
      src++;

   src_len = strlen(src);
   normalized = (char*)malloc(src_len + 1);
   if (!normalized)
      return NULL;

   while (*src)
   {
      if (*src == '/')
      {
         if (prev_slash)
         {
            src++;
            continue;
         }
         prev_slash = true;
      }
      else
         prev_slash = false;

      normalized[out++] = *src++;
   }

   normalized[out] = '\0';
   return normalized;
}

static bool s3_build_request_url(char *url, size_t url_size, s3_state_t *s3_st, const char *path)
{
   char base_url[PATH_MAX_LENGTH];
   char *normalized_path = NULL;
   char *encoded_path = NULL;
   size_t base_len = 0;

   if (!url || !s3_st || !*s3_st->url)
      return false;

   if (!path || !*path)
   {
      strlcpy(url, s3_st->url, url_size);
      return true;
   }

   normalized_path = s3_normalize_object_key(path);
   if (!normalized_path)
      return false;

   encoded_path = s3_url_encode(normalized_path);
   if (!encoded_path)
   {
      free(normalized_path);
      return false;
   }

   strlcpy(base_url, s3_st->url, sizeof(base_url));
   base_len = strlen(base_url);
   while (base_len > 0 && base_url[base_len - 1] == '/')
      base_url[--base_len] = '\0';

   if (!encoded_path || !*encoded_path)
      strlcpy(url, base_url, url_size);
   else
      snprintf(url, url_size, "%s/%s", base_url, encoded_path);

   free(normalized_path);
   free(encoded_path);
   return true;
}

/* Build canonical URI from an already constructed request URL path component.
 * This keeps signing aligned with the exact URL being requested, including any
 * base path prefix (e.g. path-style bucket endpoints). */
static bool s3_build_canonical_uri_from_url(const char *url, char *canonical_uri, size_t canonical_uri_size)
{
   const char *scheme_sep = NULL;
   const char *authority = NULL;
   const char *path_start = NULL;
   const char *path_end = NULL;
   size_t path_len = 0;

   if ((!url || !*url) || !canonical_uri || canonical_uri_size == 0)
      return false;

   scheme_sep = strstr(url, "://");
   authority  = scheme_sep ? scheme_sep + 3 : url;
   if (!authority || !*authority)
      return false;

   path_start = strchr(authority, '/');
   if (!path_start)
   {
      strlcpy_lit(canonical_uri, "/", canonical_uri_size);
      return true;
   }

   path_end = strpbrk(path_start, "?#");
   path_len = path_end ? (size_t)(path_end - path_start) : strlen(path_start);

   if (path_len == 0)
   {
      strlcpy_lit(canonical_uri, "/", canonical_uri_size);
      return true;
   }

   if (path_len >= canonical_uri_size)
      return false;

   strlcpy(canonical_uri, path_start, path_len + 1);
   return true;
}

static char* s3_url_encode_query_value(const char *input)
{
   size_t input_len = 0;
   size_t output_pos = 0;
   size_t i = 0;
   char *output = NULL;

   if (!input)
      return NULL;

   input_len = strlen(input);
   output = (char*)malloc(input_len * 3 + 1);
   if (!output)
      return NULL;

   for (i = 0; i < input_len; i++)
   {
      unsigned char c = (unsigned char)input[i];
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~')
      {
         output[output_pos++] = (char)c;
      }
      else
      {
         output[output_pos++] = '%';
         output[output_pos++] = "0123456789ABCDEF"[c >> 4];
         output[output_pos++] = "0123456789ABCDEF"[c & 0x0F];
      }
   }

   output[output_pos] = '\0';
   return output;
}

static char* s3_build_url_with_query(const char *url, const char *query)
{
   size_t total = 0;
   char *result = NULL;

   if (!url || !*url)
      return NULL;

   total  = strlen(url) + (query ? strlen(query) : 0) + 2;
   result = (char*)malloc(total);
   if (!result)
      return NULL;

   if (query && *query)
      snprintf(result, total, "%s?%s", url, query);
   else
      strlcpy(result, url, total);

   return result;
}

static char* s3_extract_xml_tag_value(const char *xml, const char *tag_name)
{
   char open_tag[64];
   char close_tag[64];
   const char *start = NULL;
   const char *end = NULL;
   size_t len = 0;
   char *value = NULL;

   if ((!xml || !*xml) || (!tag_name || !*tag_name))
      return NULL;

   snprintf(open_tag, sizeof(open_tag), "<%s>", tag_name);
   snprintf(close_tag, sizeof(close_tag), "</%s>", tag_name);

   start = strstr(xml, open_tag);
   if (!start)
      return NULL;
   start += strlen(open_tag);

   end = strstr(start, close_tag);
   if (!end || end <= start)
      return NULL;

   len = (size_t)(end - start);
   value = (char*)malloc(len + 1);
   if (!value)
      return NULL;

   memcpy(value, start, len);
   value[len] = '\0';
   return value;
}

static char* s3_extract_header_value(http_transfer_data_t *data, const char *header_name)
{
   const char *start;
   const char *end;
   size_t len;
   char *out;

   if (!data || !header_name || !*header_name
         || !(start = net_http_header_value(data->headers, header_name)))
      return NULL;

   end = start + strlen(start);
   while (end > start && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t'))
      end--;

   len = (size_t)(end - start);
   if (!(out = (char*)malloc(len + 1)))
      return NULL;
   memcpy(out, start, len);
   out[len] = '\0';
   return out;
}

static void s3_multipart_free(s3_multipart_state_t *mp_st)
{
   size_t i;
   if (!mp_st)
      return;

   if (mp_st->part_etags)
   {
      for (i = 0; i < mp_st->part_count; i++)
         free(mp_st->part_etags[i]);
      free(mp_st->part_etags);
   }

   free(mp_st->upload_id);
   free(mp_st->cb_state);
   free(mp_st);
}

static void s3_multipart_finish(s3_multipart_state_t *mp_st, bool success)
{
   if (mp_st && mp_st->cb_state && mp_st->cb_state->cb)
      mp_st->cb_state->cb(mp_st->cb_state->user_data, mp_st->cb_state->path, success, mp_st->cb_state->rfile);
   s3_multipart_free(mp_st);
}

static void s3_multipart_upload_next_part(s3_multipart_state_t *mp_st);
static void s3_multipart_abort(s3_multipart_state_t *mp_st);
static bool s3_multipart_start(s3_multipart_state_t *mp_st);

static bool s3_multipart_part_bounds(const s3_multipart_state_t *mp_st,
      size_t part_index, size_t *out_offset, size_t *out_content_len)
{
   size_t offset;
   size_t remaining;
   size_t content_len;

   if (!mp_st || !out_offset || !out_content_len)
      return false;
   if (part_index >= mp_st->part_count)
      return false;
   if (!mp_st->part_size)
      return false;
   if (part_index > (mp_st->file_size / mp_st->part_size))
      return false;

   offset = part_index * mp_st->part_size;
   if (offset >= mp_st->file_size)
      return false;

   remaining   = mp_st->file_size - offset;
   content_len = remaining < mp_st->part_size ? remaining : mp_st->part_size;
   if (!content_len)
      return false;
   if (content_len > (mp_st->file_size - offset))
      return false;

   *out_offset      = offset;
   *out_content_len = content_len;
   return true;
}

static void s3_log_multipart_initiate_failure(
      s3_multipart_state_t *mp_st, http_transfer_data_t *data, const char *err)
{
   const char *h;

   if (!mp_st)
      return;

   RARCH_WARN(S3_PFX "Multipart initiate diagnostic: path='%s' url_query='uploads=' canonical_query='uploads=' data=%s status=%d len=%zu headers=%s err='%s'\n",
         mp_st->cb_state ? mp_st->cb_state->path : "<unknown>",
         data ? "present" : "missing",
         data ? data->status : -1,
         data ? data->len : 0,
         (data && data->headers) ? "present" : "missing",
         (err && *err) ? err : "<none>");

   if (data)
      for (h = net_http_header_next(data->headers, NULL); h;
            h = net_http_header_next(data->headers, h))
         RARCH_WARN(S3_PFX "Multipart initiate header: %s\n", h);

   if (data && data->data && data->len > 0)
      RARCH_WARN(S3_PFX "Multipart initiate body: %.*s\n",
            (int)data->len, (const char*)data->data);
}

static bool s3_multipart_fallback_single_put(s3_multipart_state_t *mp_st)
{
   s3_state_t *s3_st = s3_state_get_ptr();
   char *payload_hash = NULL;
   char *auth_header = NULL;

   if (!mp_st || !mp_st->cb_state)
      return false;

   s3_body_set(&mp_st->cb_state->body, mp_st->file, 0, mp_st->file_size);
   if (!(payload_hash = s3_sha256_file(mp_st->file, 0, mp_st->file_size)))
      return false;

   auth_header = s3_build_auth_header("PUT", mp_st->canonical_uri, "",
         "host;x-amz-content-sha256;x-amz-date", payload_hash, s3_st);
   if (!auth_header)
   {
      free(payload_hash);
      return false;
   }

   if (!s3_http_put_async(mp_st->url, auth_header, mp_st->cb_state))
   {
      free(payload_hash);
      free(auth_header);
      return false;
   }

   /* Callback ownership transferred to the HTTP task. */
   mp_st->cb_state = NULL;

   free(payload_hash);
   free(auth_header);
   return true;
}

static void s3_multipart_fail(s3_multipart_state_t *mp_st,
      http_transfer_data_t *data, const char *err, const char *phase)
{
   if (!mp_st)
      return;

   if (data)
      s3_log_http_failure(mp_st->cb_state ? mp_st->cb_state->path : phase, data, err);
   else
      RARCH_WARN(S3_PFX "Multipart upload failed during %s\n", phase ? phase : "unknown phase");

   if (mp_st->upload_id && *mp_st->upload_id && !mp_st->aborting)
      s3_multipart_abort(mp_st);
   else
      s3_multipart_finish(mp_st, false);
}

static void s3_multipart_abort_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_multipart_state_t *mp_st   = (s3_multipart_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && data->status >= 200 && data->status < 300);

   (void)task;
   (void)err;

   if (!mp_st)
      return;

   if (!success)
      RARCH_WARN(S3_PFX "Multipart abort request was not successful\n");

   s3_multipart_finish(mp_st, false);
}

static void s3_multipart_abort(s3_multipart_state_t *mp_st)
{
   s3_state_t *s3_st = s3_state_get_ptr();
   char *encoded_upload_id = NULL;
   char *query = NULL;
   char *url_with_query = NULL;
   char *auth_header = NULL;

   if (!mp_st || (!mp_st->upload_id || !*mp_st->upload_id))
   {
      s3_multipart_finish(mp_st, false);
      return;
   }

   mp_st->aborting = true;
   encoded_upload_id = s3_url_encode_query_value(mp_st->upload_id);
   if (!encoded_upload_id)
   {
      s3_multipart_finish(mp_st, false);
      return;
   }

   query = (char*)malloc(strlen(encoded_upload_id) + 16);
   if (!query)
   {
      free(encoded_upload_id);
      s3_multipart_finish(mp_st, false);
      return;
   }
   snprintf(query, strlen(encoded_upload_id) + 16, "uploadId=%s", encoded_upload_id);

   url_with_query = s3_build_url_with_query(mp_st->url, query);
   auth_header = s3_build_auth_header("DELETE", mp_st->canonical_uri, query,
         "host;x-amz-content-sha256;x-amz-date", S3_EMPTY_PAYLOAD_SHA256, s3_st);

   if (!url_with_query || !auth_header)
      goto fail;

   if (!task_push_http_transfer_with_headers(url_with_query, true, "DELETE",
            auth_header, s3_multipart_abort_cb, mp_st))
      goto fail;

   free(encoded_upload_id);
   free(query);
   free(url_with_query);
   free(auth_header);
   return;

fail:
   free(encoded_upload_id);
   free(query);
   free(url_with_query);
   free(auth_header);
   s3_multipart_finish(mp_st, false);
}

static void s3_multipart_complete_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_multipart_state_t *mp_st   = (s3_multipart_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && data->status >= 200 && data->status < 300);

   (void)task;

   if (!mp_st)
      return;

   if (!success)
   {
      s3_multipart_fail(mp_st, data, err, "complete");
      return;
   }

   RARCH_LOG(S3_PFX "Multipart upload complete: %s (%zu parts)\n",
         mp_st->cb_state ? mp_st->cb_state->path : "<unknown>",
         mp_st->part_count);
   s3_multipart_finish(mp_st, true);
}

static void s3_multipart_complete(s3_multipart_state_t *mp_st)
{
   s3_state_t *s3_st = s3_state_get_ptr();
   char *encoded_upload_id = NULL;
   char *query = NULL;
   char *url_with_query = NULL;
   char *payload_hash = NULL;
   char *auth_header = NULL;
   char *xml_body = NULL;
   size_t i;
   size_t xml_len = 0;
   size_t xml_cap = 128;

   if (!mp_st || (!mp_st->upload_id || !*mp_st->upload_id))
   {
      s3_multipart_finish(mp_st, false);
      return;
   }

   for (i = 0; i < mp_st->part_count; i++)
   {
      if (!mp_st->part_etags[i] || !*mp_st->part_etags[i])
      {
         s3_multipart_fail(mp_st, NULL, NULL, "complete missing ETag");
         return;
      }
      xml_cap += strlen(mp_st->part_etags[i]) + 96;
   }

   xml_body = (char*)malloc(xml_cap);
   if (!xml_body)
   {
      s3_multipart_fail(mp_st, NULL, NULL, "complete allocation");
      return;
   }

   xml_len += (size_t)snprintf(xml_body + xml_len, xml_cap - xml_len,
         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<CompleteMultipartUpload>\n");
   for (i = 0; i < mp_st->part_count; i++)
      xml_len += (size_t)snprintf(xml_body + xml_len, xml_cap - xml_len,
            "  <Part><PartNumber>%u</PartNumber><ETag>%s</ETag></Part>\n",
            (unsigned)(i + 1), mp_st->part_etags[i]);
   xml_len += (size_t)snprintf(xml_body + xml_len, xml_cap - xml_len, "</CompleteMultipartUpload>");

   encoded_upload_id = s3_url_encode_query_value(mp_st->upload_id);
   if (!encoded_upload_id)
      goto fail;

   query = (char*)malloc(strlen(encoded_upload_id) + 16);
   if (!query)
      goto fail;
   snprintf(query, strlen(encoded_upload_id) + 16, "uploadId=%s", encoded_upload_id);

   payload_hash = s3_sha256_hash(xml_body, xml_len);
   url_with_query = s3_build_url_with_query(mp_st->url, query);
   auth_header = s3_build_auth_header("POST", mp_st->canonical_uri, query,
         "host;x-amz-content-sha256;x-amz-date", payload_hash, s3_st);
   if (!payload_hash || !url_with_query || !auth_header)
      goto fail;

   if (!task_push_http_transfer_with_content(url_with_query, "POST",
            xml_body, xml_len, "application/xml", true, false,
            auth_header, s3_multipart_complete_cb, mp_st))
      goto fail;

   free(encoded_upload_id);
   free(query);
   free(url_with_query);
   free(payload_hash);
   free(auth_header);
   free(xml_body);
   return;

fail:
   free(encoded_upload_id);
   free(query);
   free(url_with_query);
   free(payload_hash);
   free(auth_header);
   free(xml_body);
   s3_multipart_fail(mp_st, NULL, NULL, "complete start");
}

static void s3_multipart_upload_part_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_multipart_state_t *mp_st   = (s3_multipart_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && data->status >= 200 && data->status < 300);
   char *etag = NULL;

   (void)task;

   if (!mp_st)
      return;

   if (!success)
   {
      s3_multipart_fail(mp_st, data, err, "upload part");
      return;
   }

   etag = s3_extract_header_value(data, "ETag");
   if (!etag)
   {
      RARCH_WARN(S3_PFX "Missing ETag for multipart part %u\n", (unsigned)(mp_st->current_part + 1));
      s3_multipart_fail(mp_st, data, err, "upload part missing etag");
      return;
   }

   mp_st->part_etags[mp_st->current_part] = etag;
   mp_st->current_part++;

   if (mp_st->current_part >= mp_st->part_count)
      s3_multipart_complete(mp_st);
   else
      s3_multipart_upload_next_part(mp_st);
}

static void s3_multipart_upload_next_part(s3_multipart_state_t *mp_st)
{
   s3_state_t *s3_st = s3_state_get_ptr();
   size_t offset = 0;
   size_t content_len = 0;
   unsigned part_number;
   char *payload_hash = NULL;
   char *encoded_upload_id = NULL;
   char *query = NULL;
   char *url_with_query = NULL;
   char *auth_header = NULL;

   if (!mp_st || mp_st->current_part >= mp_st->part_count)
      return;

   if (!s3_multipart_part_bounds(mp_st, mp_st->current_part, &offset, &content_len))
   {
      RARCH_WARN(S3_PFX "Multipart upload part bounds check failed for part %u (parts=%zu file_size=%zu part_size=%zu)\n",
            (unsigned)(mp_st->current_part + 1),
            mp_st->part_count,
            mp_st->file_size,
            mp_st->part_size);
      goto fail;
   }
   part_number = (unsigned)(mp_st->current_part + 1);

   s3_body_set(&mp_st->part, mp_st->file, (int64_t)offset, content_len);
   payload_hash = s3_sha256_file(mp_st->file, (int64_t)offset, content_len);
   encoded_upload_id = s3_url_encode_query_value(mp_st->upload_id);
   if (!payload_hash || !encoded_upload_id)
      goto fail;

   query = (char*)malloc(strlen(encoded_upload_id) + 48);
   if (!query)
      goto fail;
   snprintf(query, strlen(encoded_upload_id) + 48,
         "partNumber=%u&uploadId=%s", part_number, encoded_upload_id);

   url_with_query = s3_build_url_with_query(mp_st->url, query);
   auth_header = s3_build_auth_header("PUT", mp_st->canonical_uri, query,
         "host;x-amz-content-sha256;x-amz-date", payload_hash, s3_st);
   if (!url_with_query || !auth_header)
      goto fail;

   if (!task_push_http_transfer_with_source(url_with_query, "PUT",
            s3_body_source, s3_body_rewind, &mp_st->part, content_len,
            "application/octet-stream", true, true,
            auth_header, s3_multipart_upload_part_cb, mp_st))
      goto fail;

   free(payload_hash);
   free(encoded_upload_id);
   free(query);
   free(url_with_query);
   free(auth_header);
   return;

fail:
   free(payload_hash);
   free(encoded_upload_id);
   free(query);
   free(url_with_query);
   free(auth_header);
   s3_multipart_fail(mp_st, NULL, NULL, "upload part start");
}

static void s3_multipart_initiate_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_multipart_state_t *mp_st   = (s3_multipart_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && data->status >= 200 && data->status < 300);
   char *response_xml            = NULL;

   (void)task;

   if (!mp_st)
      return;

   if (!success)
   {
      s3_log_multipart_initiate_failure(mp_st, data, err);

      if (!data || data->status < 0)
      {
         RARCH_WARN(S3_PFX "Multipart initiation transport failed (data=%s, status=%d); falling back to single PUT for '%s'\n",
               data ? "present" : "missing",
               data ? data->status : -1,
               mp_st->cb_state ? mp_st->cb_state->path : "<unknown>");
         if (s3_multipart_fallback_single_put(mp_st))
         {
            s3_multipart_free(mp_st);
            return;
         }
      }

      if (err && *err)
         RARCH_WARN(S3_PFX "Multipart initiate transport error: %s\n", err);
      s3_multipart_fail(mp_st, data, err, "initiate");
      return;
   }

   if (!data || (!data->data || !*data->data))
   {
      s3_multipart_fail(mp_st, data, err, "initiate empty response");
      return;
   }

   response_xml = (char*)malloc(data->len + 1);
   if (!response_xml)
   {
      s3_multipart_fail(mp_st, data, err, "initiate allocation");
      return;
   }
   memcpy(response_xml, data->data, data->len);
   response_xml[data->len] = '\0';

   mp_st->upload_id = s3_extract_xml_tag_value(response_xml, "UploadId");
   free(response_xml);
   if (!mp_st->upload_id || !*mp_st->upload_id)
   {
      RARCH_WARN(S3_PFX "Multipart initiation response missing UploadId\n");
      s3_multipart_fail(mp_st, data, err, "initiate missing upload id");
      return;
   }

   RARCH_LOG(S3_PFX "Started multipart upload for %s (uploadId=%s, parts=%zu)\n",
         mp_st->cb_state ? mp_st->cb_state->path : "<unknown>", mp_st->upload_id, mp_st->part_count);
   s3_multipart_upload_next_part(mp_st);
}

static bool s3_multipart_start(s3_multipart_state_t *mp_st)
{
   s3_state_t *s3_st = s3_state_get_ptr();
   char *url_with_query = NULL;
   char *auth_header = NULL;
   const char *url_query = "uploads=";
   const char *canonical_query = "uploads=";

   if (!mp_st)
      return false;

   url_with_query = s3_build_url_with_query(mp_st->url, url_query);
   auth_header = s3_build_auth_header("POST", mp_st->canonical_uri, canonical_query,
         "host;x-amz-content-sha256;x-amz-date", S3_EMPTY_PAYLOAD_SHA256, s3_st);
   if (!url_with_query || !auth_header)
   {
      free(url_with_query);
      free(auth_header);
      return false;
   }

   RARCH_DBG(S3_PFX "Multipart initiate URL: %s\n", url_with_query);
   RARCH_DBG(S3_PFX "Multipart initiate headers:\n%s\n", auth_header);

   if (!task_push_http_post_transfer_with_headers(url_with_query, "",
            true, "POST", auth_header,
            s3_multipart_initiate_cb, mp_st))
   {
      free(url_with_query);
      free(auth_header);
      return false;
   }

   free(url_with_query);
   free(auth_header);
   return true;
}

/* Initialize S3 connection */
static bool s3_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_state_t *s3_st = s3_state_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!settings)
      return false;

   /* Copy configuration - TODO: Use actual settings when available */
   strlcpy(s3_st->url, settings->arrays.s3_url, sizeof(settings->arrays.s3_url));
   strlcpy(s3_st->access_key_id, settings->arrays.access_key_id, sizeof(settings->arrays.access_key_id));
   strlcpy(s3_st->secret_access_key, settings->arrays.secret_access_key, sizeof(settings->arrays.secret_access_key));

   RARCH_DBG(S3_PFX "S3 configuration - URL: '%s'\n", s3_st->url);

   /* Parse URL to extract bucket, region, and host */
   if (!s3_parse_url(s3_st->url, s3_st->bucket, s3_st->region, s3_st->host, S3_SERVICE))
      RARCH_WARN(S3_PFX "Could not parse bucket/region from URL, using defaults\n");

   /* Validate configuration */
   if (   !*s3_st->url
       || !*s3_st->access_key_id
       || !*s3_st->secret_access_key)
   {
      RARCH_ERR(S3_PFX "Missing S3 configuration\n");
      return false;
   }

   RARCH_LOG(S3_PFX "S3 sync initialized for bucket: %s in region: %s\n",
             s3_st->bucket, s3_st->region);

   s3_st->connected = true;

   /* Call completion handler */
   if (cb)
      cb(user_data, NULL, true, NULL);

   return true;
}

/* Clean up S3 connection */
static bool s3_sync_end(cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_state_t *s3_st = s3_state_get_ptr();

   s3_st->connected = false;
   RARCH_LOG(S3_PFX "S3 sync ended\n");

   /* Call completion handler */
   if (cb)
      cb(user_data, NULL, true, NULL);

   return true;
}

/* Read file from S3 */
/* @path's request URL, and its canonical URI behind it in the same
 * allocation, which the caller frees: two path buffers kept off the
 * stack. NULL if either cannot be built or out of memory. */
static char *s3_object_urls(s3_state_t *s3_st, const char *path,
      char **canonical_uri)
{
   char *url = (char*)malloc(2 * PATH_MAX_LENGTH);

   if (!url)
      return NULL;
   *canonical_uri = url + PATH_MAX_LENGTH;
   if (     !s3_build_request_url(url, PATH_MAX_LENGTH, s3_st, path)
         || !s3_build_canonical_uri_from_url(url, *canonical_uri,
            PATH_MAX_LENGTH))
   {
      free(url);
      return NULL;
   }
   return url;
}

static bool s3_read(const char *path, const char *file, cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_cb_state_t *s3_cb_st = (s3_cb_state_t*)calloc(1, sizeof(s3_cb_state_t));
   s3_state_t *s3_st = s3_state_get_ptr();
   char *auth_header = NULL;
   char *url = NULL;
   char *canonical_uri = NULL;
   bool success = false;

   if (!s3_cb_st)
      return false;

   s3_cb_st->cb = cb;
   s3_cb_st->user_data = user_data;
   strlcpy(s3_cb_st->path, path, sizeof(s3_cb_st->path));
   strlcpy(s3_cb_st->file, file, sizeof(s3_cb_st->file));

   if (!(url = s3_object_urls(s3_st, path, &canonical_uri)))
      goto cleanup;

   /* Build authorization header */
   auth_header = s3_build_auth_header("GET", canonical_uri, "", "host;x-amz-content-sha256;x-amz-date",
                                     S3_EMPTY_PAYLOAD_SHA256, s3_st);

   if (!auth_header)
   {
      RARCH_ERR(S3_PFX "Failed to build authorization header\n");
      goto cleanup;
   }

   RARCH_DBG(S3_PFX "Request URL: %s\n", url);
   RARCH_DBG(S3_PFX "Canonical URI: %s\n", canonical_uri);

   /* Perform HTTP GET request asynchronously */
   if (!s3_http_get_async(url, auth_header, s3_cb_st))
   {
      RARCH_ERR(S3_PFX "Failed to start HTTP GET request\n");
      goto cleanup;
   }

   /* Ownership transferred to async callback */
   s3_cb_st = NULL;
   success = true;

cleanup:
   free(auth_header);
   free(s3_cb_st);
   free(url);

   return success;
}

/* Update file to S3 */
static bool s3_update_upload(const char *path, RFILE *file, cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_cb_state_t *s3_cb_st = (s3_cb_state_t*)calloc(1, sizeof(s3_cb_state_t));
   s3_multipart_state_t *mp_st = NULL;
   s3_state_t *s3_st = s3_state_get_ptr();
   char *auth_header = NULL;
   char *url = NULL;
   char *canonical_uri = NULL;
   size_t file_size = 0;
   char *payload_hash = NULL;
   size_t multipart_threshold = s3_get_multipart_nominal_threshold_bytes();
   bool success = false;

   if (!s3_cb_st)
      return false;

   s3_cb_st->cb = cb;
   s3_cb_st->user_data = user_data;
   s3_cb_st->rfile = file;
   strlcpy(s3_cb_st->path, path, sizeof(s3_cb_st->path));

   /* The body is streamed from the file, never read into memory whole:
    * hashed in runs for the signature, then read again as the socket
    * takes it. */
   if (file)
   {
      int64_t size = filestream_get_size(file);
      if (size < 0 || (uint64_t)size > (uint64_t)((size_t)-1))
      {
         RARCH_ERR(S3_PFX "Could not read %s for upload\n", path);
         goto cleanup;
      }
      file_size = (size_t)size;
   }

   if (!(url = s3_object_urls(s3_st, path, &canonical_uri)))
      goto cleanup;

   RARCH_DBG(S3_PFX "Request URL: %s\n", url);
   RARCH_DBG(S3_PFX "Canonical URI: %s\n", canonical_uri);

   /* For large files, use multipart upload */
   if (file_size > multipart_threshold)
   {
      size_t part_size = 0;
      size_t part_count = 0;

      RARCH_LOG(S3_PFX "Large file detected (%zu bytes, nominal multipart threshold: %zu bytes), using multipart upload\n",
            file_size, multipart_threshold);

      if (!s3_calculate_multipart_plan(file_size, &part_size, &part_count))
      {
         RARCH_ERR(S3_PFX "Could not build multipart plan for %zu-byte upload\n", file_size);
         goto cleanup;
      }

      mp_st = (s3_multipart_state_t*)calloc(1, sizeof(*mp_st));
      if (!mp_st)
         goto cleanup;

      mp_st->cb_state = s3_cb_st;
      s3_cb_st = NULL;
      mp_st->file = file;
      mp_st->file_size = file_size;
      mp_st->part_size = part_size;
      mp_st->part_count = part_count;
      strlcpy(mp_st->url, url, sizeof(mp_st->url));
      strlcpy(mp_st->canonical_uri, canonical_uri, sizeof(mp_st->canonical_uri));
      mp_st->part_etags = (char**)calloc(part_count, sizeof(char*));
      if (!mp_st->part_etags)
         goto cleanup;

      RARCH_LOG(S3_PFX "Multipart plan: %zu bytes in %zu parts (%zu bytes/part)\n",
            file_size, part_count, part_size);

      if (!s3_multipart_start(mp_st))
      {
         RARCH_ERR(S3_PFX "Failed to start multipart upload\n");
         goto cleanup;
      }

      /* Ownership transferred to multipart callbacks */
      s3_cb_st = NULL;
      mp_st = NULL;
      success = true;
   }
   else
   {
      /* Single PUT request */
      s3_body_set(&s3_cb_st->body, file, 0, file_size);
      if (file_size)
         payload_hash = s3_sha256_file(file, 0, file_size);
      else
         payload_hash = strdup(S3_EMPTY_PAYLOAD_SHA256);

      if (!payload_hash)
      {
         RARCH_ERR(S3_PFX "Failed to compute payload hash\n");
         goto cleanup;
      }

      auth_header = s3_build_auth_header("PUT", canonical_uri, "", "host;x-amz-content-sha256;x-amz-date",
                                        payload_hash, s3_st);

      if (!auth_header)
      {
         RARCH_ERR(S3_PFX "Failed to build authorization header\n");
         goto cleanup;
      }

      /* Perform HTTP PUT request */
      if (!s3_http_put_async(url, auth_header, s3_cb_st))
      {
         RARCH_ERR(S3_PFX "Failed to start HTTP PUT request\n");
         goto cleanup;
      }
      /* Ownership transferred to async callback */
      s3_cb_st = NULL;
      success = true;
   }

cleanup:
   s3_multipart_free(mp_st);
   free(auth_header);
   free(payload_hash);
   free(s3_cb_st);
   free(url);

   return success;
}

/* Delete file from S3 */
static bool s3_delete_object(const char *path, cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_cb_state_t *s3_cb_st = (s3_cb_state_t*)calloc(1, sizeof(s3_cb_state_t));
   s3_state_t *s3_st = s3_state_get_ptr();
   char *auth_header = NULL;
   char *url = NULL;
   char *canonical_uri = NULL;
   bool success = false;

   if (!s3_cb_st)
      return false;

   s3_cb_st->cb = cb;
   s3_cb_st->user_data = user_data;
   strlcpy(s3_cb_st->path, path, sizeof(s3_cb_st->path));

   if (!(url = s3_object_urls(s3_st, path, &canonical_uri)))
      goto cleanup;

   /* Build authorization header */
   auth_header = s3_build_auth_header("DELETE", canonical_uri, "", "host;x-amz-content-sha256;x-amz-date",
                                     S3_EMPTY_PAYLOAD_SHA256, s3_st);

   if (!auth_header)
   {
      RARCH_ERR(S3_PFX "Failed to build authorization header\n");
      goto cleanup;
   }

   /* Perform HTTP DELETE request */
   success = s3_http_delete_async(url, auth_header, s3_cb_st);
   if (!success)
   {
      RARCH_ERR(S3_PFX "Failed to start HTTP DELETE request\n");
      goto cleanup;
   }
   /* Ownership transferred to async callback */
   s3_cb_st = NULL;

cleanup:
   free(auth_header);
   free(s3_cb_st);
   free(url);

   return success;
}

/* With destructive sync off, cloud sync promises not to throw away the
 * server's copy of a file: before a delete or an upload replaces it,
 * the object is copied to deleted/<path>-<yymmdd-hhmmss>, the name the
 * WebDAV driver uses.  CopyObject is server-side, so the data does not
 * come down and go back up.  The server manifest is rewritten every
 * sync and is not backed up. */
typedef struct
{
   char path[PATH_MAX_LENGTH];
   RFILE *rfile;
   cloud_sync_complete_handler_t cb;
   void *user_data;
   bool is_delete;
} s3_backup_state_t;

static bool s3_wants_backup(const char *path)
{
   settings_t *settings = config_get_ptr();
   return settings
       && !settings->bools.cloud_sync_destructive
       && !string_is_equal(path, CLOUD_SYNC_SERVER_MANIFEST);
}

static bool s3_backup_key(char *s, size_t len, const char *path)
{
   struct tm tm_;
   time_t    cur_time = time(NULL);
   size_t    _len;

   while (*path == '/')
      path++;
   _len = strlcpy_lit(s, "deleted/", len);
   if (_len >= len)
      return false;
   _len += strlcpy(s + _len, path, len - _len);
   if (_len >= len)
      return false;
   rtime_localtime(&cur_time, &tm_);
   return strftime(s + _len, len - _len, "-%y%m%d-%H%M%S", &tm_) != 0;
}

/* x-amz-copy-source is /<bucket>/<key>, URL-encoded, whichever
 * addressing style the endpoint uses.  The request's canonical URI
 * already is that for a path-style endpoint (/<bucket>/<key>); a
 * virtual-hosted one carries the bucket in the host, so it goes in
 * front. */
static bool s3_copy_source(char *s, size_t len, s3_state_t *s3_st,
      const char *canonical_uri)
{
   size_t bucket_len = strlen(s3_st->bucket);
   bool   virtual_hosted;

   if (!bucket_len)
      return false;

   virtual_hosted = !strncmp(s3_st->host, s3_st->bucket, bucket_len)
                 && s3_st->host[bucket_len] == '.';
   if (virtual_hosted)
      return (size_t)snprintf(s, len, "/%s%s", s3_st->bucket, canonical_uri) < len;
   return strlcpy(s, canonical_uri, len) < len;
}

/* A CopyObject that fails part way can still answer 200, with an
 * <Error> document for a body. */
static bool s3_body_has_error(const http_transfer_data_t *data)
{
   static const char tag[] = "<Error>";
   size_t i;

   if (!data->data || data->len < sizeof(tag) - 1)
      return false;
   for (i = 0; i + sizeof(tag) - 1 <= data->len; i++)
      if (!memcmp(data->data + i, tag, sizeof(tag) - 1))
         return true;
   return false;
}

static void s3_backup_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_backup_state_t    *st   = (s3_backup_state_t*)user_data;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   bool copied, missing;

   (void)task;

   if (!st)
      return;

   copied  = data && data->status >= 200 && data->status < 300
          && !s3_body_has_error(data);
   /* 404: nothing on the server yet, so nothing to keep. */
   missing = data && data->status == 404;

   if (copied || missing)
   {
      bool started = st->is_delete
         ? s3_delete_object(st->path, st->cb, st->user_data)
         : s3_update_upload(st->path, st->rfile, st->cb, st->user_data);
      /* Past this point the request is asynchronous: a start that
       * fails has to be reported here, since the caller was already
       * told it began. */
      if (!started)
         st->cb(st->user_data, st->path, false, st->is_delete ? NULL : st->rfile);
      free(st);
      return;
   }

   if (data)
      s3_log_http_failure(st->path, data, err);
   else
      RARCH_WARN(S3_PFX "Did not get HTTP data for backup of '%s'%s%s\n",
            st->path, (err && *err) ? ": " : "", (err && *err) ? err : "");
   RARCH_ERR(S3_PFX "Not %s '%s': the copy on the server could not be backed up.\n",
         st->is_delete ? "deleting" : "replacing", st->path);
   st->cb(st->user_data, st->path, false, st->is_delete ? NULL : st->rfile);
   free(st);
}

/* What a backup copy is named by: six path buffers, kept off the stack */
typedef struct
{
   char key[PATH_MAX_LENGTH];
   char src_url[PATH_MAX_LENGTH];
   char src_uri[PATH_MAX_LENGTH];
   char dest_url[PATH_MAX_LENGTH];
   char dest_uri[PATH_MAX_LENGTH];
   char copy_source[PATH_MAX_LENGTH + NAME_MAX_LENGTH];
} s3_backup_names_t;

static bool s3_backup_then(const char *path, RFILE *file, bool is_delete,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_state_t        *s3_st       = s3_state_get_ptr();
   s3_backup_state_t *st          = NULL;
   char              *auth_header = NULL;
   s3_backup_names_t *n           = (s3_backup_names_t*)malloc(sizeof(*n));
   bool               ok          = false;

   if (!n)
      return false;
   if (   !s3_backup_key(n->key, sizeof(n->key), path)
       || !s3_build_request_url(n->src_url, sizeof(n->src_url), s3_st, path)
       || !s3_build_canonical_uri_from_url(n->src_url, n->src_uri, sizeof(n->src_uri))
       || !s3_build_request_url(n->dest_url, sizeof(n->dest_url), s3_st, n->key)
       || !s3_build_canonical_uri_from_url(n->dest_url, n->dest_uri, sizeof(n->dest_uri))
       || !s3_copy_source(n->copy_source, sizeof(n->copy_source), s3_st, n->src_uri))
   {
      RARCH_ERR(S3_PFX "Not %s '%s': could not name a backup for it.\n",
            is_delete ? "deleting" : "replacing", path);
      goto done;
   }

   if (!(st = (s3_backup_state_t*)calloc(1, sizeof(*st))))
      goto done;
   strlcpy(st->path, path, sizeof(st->path));
   st->rfile     = file;
   st->cb        = cb;
   st->user_data = user_data;
   st->is_delete = is_delete;

   /* CopyObject: a PUT on the new key, empty body, naming the source. */
   auth_header = s3_build_auth_header_ex("PUT", n->dest_uri, "",
         "host;x-amz-content-sha256;x-amz-copy-source;x-amz-date",
         S3_EMPTY_PAYLOAD_SHA256, n->copy_source, s3_st);
   if (!auth_header)
   {
      RARCH_ERR(S3_PFX "Failed to build authorization header\n");
      goto done;
   }

   RARCH_DBG(S3_PFX "COPY %s -> %s\n", n->copy_source, n->dest_url);
   if (!task_push_http_transfer_with_content(n->dest_url, "PUT", NULL, 0, NULL,
            true, false, auth_header, s3_backup_cb, st))
   {
      RARCH_ERR(S3_PFX "Failed to start backup of '%s'\n", path);
      goto done;
   }
   /* Ownership transferred to the backup callback */
   st = NULL;
   ok = true;

done:
   free(auth_header);
   free(st);
   free(n);
   return ok;
}

static bool s3_update(const char *path, RFILE *file, cloud_sync_complete_handler_t cb, void *user_data)
{
   if (s3_wants_backup(path))
      return s3_backup_then(path, file, false, cb, user_data);
   return s3_update_upload(path, file, cb, user_data);
}

static bool s3_free(const char *path, cloud_sync_complete_handler_t cb, void *user_data)
{
   if (s3_wants_backup(path))
      return s3_backup_then(path, NULL, true, cb, user_data);
   return s3_delete_object(path, cb, user_data);
}

/* S3 cloud sync driver */
cloud_sync_driver_t cloud_sync_s3 = {
   s3_sync_begin,
   s3_sync_end,
   s3_read,
   s3_update,
   s3_free,
   "s3",
   0
};

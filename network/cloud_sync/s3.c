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
#include <string/stdstring.h>
#include <time/rtime.h>
#include <formats/rjson.h>
#include <retro_miscellaneous.h>

#include "../cloud_sync_driver.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

#define S3_PFX "[S3] "

/* S3 multipart constants (AWS): 5 MiB min part size, 5 GiB max part size.
 * The nominal multipart threshold below is a local policy trigger, not an S3 limit. */
#define S3_MULTIPART_MIN_PART_SIZE_BYTES (5ULL * 1024ULL * 1024ULL)
#define S3_MULTIPART_MAX_PART_SIZE_BYTES (5ULL * 1024ULL * 1024ULL * 1024ULL)
#define S3_MAX_PARTS 10000
#define S3_EMPTY_PAYLOAD_SHA256 "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
#define S3_SIGNATURE_VERSION "AWS4-HMAC-SHA256"
#define S3_SERVICE "s3"

#if defined(_3DS) || defined(VITA) || defined(PSP) || defined(PS2) || defined(WIIU) || defined(GEKKO)
#define S3_MULTIPART_NOMINAL_THRESHOLD_BYTES (16ULL * 1024ULL * 1024ULL)
#elif defined(HAVE_LIBNX)
#define S3_MULTIPART_NOMINAL_THRESHOLD_BYTES (32ULL * 1024ULL * 1024ULL)
#else
#define S3_MULTIPART_NOMINAL_THRESHOLD_BYTES (64ULL * 1024ULL * 1024ULL)
#endif

typedef struct
{
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
   char *file_data;
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
      strlcpy(region, "auto", NAME_MAX_LENGTH);
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
         strlcpy(region, "us-east-1", NAME_MAX_LENGTH);
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
         RARCH_LOG(S3_PFX "Backblaze B2 style: virtual-hosted\n");
         size_t bucket_len = s3pos - host;
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
      strlcpy(region, "us-east-1", NAME_MAX_LENGTH);

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

/* URL encode a string according to RFC 3986 */
static char* s3_url_encode(const char *input)
{
   size_t input_len = strlen(input);
   size_t output_pos = 0;

   /* Worst case: every char needs encoding */
   char *output = malloc(input_len * 3 + 1); 
   
   size_t i;
   
   if (!output)
      return NULL;
   
   if (!input)
      return NULL;


   for (i = 0; i < input_len; i++)
   {
      unsigned char c = (unsigned char)input[i];

      /* RFC 3986 unreserved characters: A-Z, a-z, 0-9, -, ., _, ~ */
      /* Path delimiters that should not be encoded: / */
      /* Query parameter delimiters that should not be encoded: &, =, ? */
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || 
          c == '~' || c == '/' || c == '&' || c == '=' || c == '?')
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

/* Trim whitespace from string */
static char* s3_trim_string(const char *input)
{
   size_t len = strlen(input);
   size_t start = 0;
   size_t end = len;
   size_t trimmed_len = 0;
   char *trimmed = NULL;

   if (!input)
      return NULL;

   /* Find start of non-whitespace */
   while (start < len && (input[start] == ' ' || input[start] == '\t'))
      start++;

   /* Find end of non-whitespace */
   while (end > start && (input[end-1] == ' ' || input[end-1] == '\t'))
      end--;

   /* Create trimmed string */
   trimmed_len = end - start;
   trimmed = malloc(trimmed_len + 1);
   if (!trimmed)
      return NULL;

   memcpy(trimmed, input + start, trimmed_len);
   trimmed[trimmed_len] = '\0';

   return trimmed;
}

/* Canonicalize query string parameters */
static char* s3_canonicalize_query_string(const char *query_string)
{
   if (!query_string || !*query_string)
      return strdup("");

   /* For now, return the query string as-is since most S3 operations don't use
    * query parameters
    * A full implementation would:
    * 1. Split parameters by '&'
    * 2. URL encode each parameter name and value
    * 3. Sort parameters alphabetically by name
    * 4. Join with '&'
    */
   return strdup(query_string);
}

/* Calculate HMAC-SHA256 and return binary result */
static uint8_t* s3_hmac_sha256_bin(const uint8_t *key, size_t key_len, const char *data, uint8_t *output)
{
   unsigned i       = 0;
   char *inner_data = NULL;
   size_t data_len  = strlen(data);

   char inner_pad[64];
   char outer_pad[64];
   uint8_t key_hash[32];

   char inner_hash_hex[65];
   uint8_t inner_hash_bin[32];
   char final_hash_hex[65];

   /* Hash the key if it's longer than 64 bytes */
   if (key_len > 64)
   {
      char temp_hash[65];/* Use a temporary buffer for the hash */
      sha256_hash(temp_hash, (const uint8_t*)key, key_len);

      /* Convert hex to binary */
      for (i = 0; i < 32; i++)
      {
         char hex_byte[3] = {temp_hash[i*2], temp_hash[i*2+1], 0};
         key_hash[i] = (uint8_t)strtol(hex_byte, NULL, 16);
      }
      key = key_hash;
      key_len = 32;
   }
   else
   {
   }

   /* Pad the key to 64 bytes */
   memset(outer_pad, 0, 64);
   memset(inner_pad, 0, 64);
   if (key_len <= 64)
   {
      memcpy(outer_pad, key, key_len);
      memcpy(inner_pad, key, key_len);
   }
   else
   {
      /* Key is longer than 64 bytes, this shouldn't happen after hashing */
      memcpy(outer_pad, key, 64);
      memcpy(inner_pad, key, 64);
   }

   /* XOR with constants */
   for (i = 0; i < 64; i++)
   {
      outer_pad[i] ^= 0x5c;
      inner_pad[i] ^= 0x36;
   }

   /* Inner hash: H(K XOR 0x36, text) */
   inner_data = malloc(64 + data_len);
   if (!inner_data)
      return NULL;

   memcpy(inner_data, inner_pad, 64);
   memcpy(inner_data + 64, data, data_len);

   /* Hash and convert to binary */
   sha256_hash(inner_hash_hex, (const uint8_t*)inner_data, 64 + data_len);

   for (i = 0; i < 32; i++)
   {
      char hex_byte[3] = {inner_hash_hex[i*2], inner_hash_hex[i*2+1], 0};
      inner_hash_bin[i] = (uint8_t)strtol(hex_byte, NULL, 16);
   }

   /* Outer hash: H(K XOR 0x5c, inner_hash_bin) */
   /* Ensure we have enough space for outer hash (64 + 32 = 96 bytes) */
   if (64 + data_len < 96)
   {
      /* Reallocate if needed */
      char *outer_data = realloc(inner_data, 96);
      if (!outer_data)
      {
         free(inner_data);
         return NULL;
      }
      inner_data = outer_data;
   }

   memcpy(inner_data, outer_pad, 64);
   memcpy(inner_data + 64, inner_hash_bin, 32);

   sha256_hash(final_hash_hex, (const uint8_t*)inner_data, 64 + 32);

   /* Convert final result to binary */
   for (i = 0; i < 32; i++)
   {
      char hex_byte[3] = {final_hash_hex[i*2], final_hash_hex[i*2+1], 0};
      output[i] = (uint8_t)strtol(hex_byte, NULL, 16);
   }

   /* Cleanup */
   free(inner_data);

   return output;
}

/* Calculate HMAC-SHA256 (legacy function for compatibility) */
static char* s3_hmac_sha256(const char *key, const char *data, char *output)
{
   unsigned i;
   char *hmac_hex = malloc(65);
   uint8_t binary_key[64];
   size_t key_len;
   uint8_t result[32];

   if (!hmac_hex)
      return NULL;

   key_len = strlen(key);

   /* Convert key to binary */
   if (key_len <= 64)
   {
      memset(binary_key, 0, 64);
      memcpy(binary_key, key, key_len);
   }
   else
   {
      /* Hash the key if it's longer than 64 bytes */
      char temp_hash[65];
      sha256_hash(temp_hash, (const uint8_t*)key, key_len);
      for (i = 0; i < 32; i++)
      {
         char hex_byte[3] = {temp_hash[i*2], temp_hash[i*2+1], 0};
         binary_key[i] = (uint8_t)strtol(hex_byte, NULL, 16);
      }
      key_len = 32;
   }

   if (!s3_hmac_sha256_bin(binary_key, key_len, data, result))
   {
      free(hmac_hex);
      return NULL;
   }

   /* Convert binary result to hex */
   for (i = 0; i < 32; i++)
      snprintf(hmac_hex + 2 * i, 3, "%02x", (unsigned)result[i]);

   hmac_hex[64] = '\0';
   return hmac_hex;
}

/* Calculate AWS Signature Version 4 signature with proper key derivation */
static char* s3_calculate_aws4_signature(const char *secret_key, const char *date,
                                        const char *region, const char *service,
                                        const char *string_to_sign)
{
   unsigned i;
   char *signature;
   char aws4_key[128];
   uint8_t k_date[32];
   uint8_t k_region[32];
   uint8_t k_service[32];
   uint8_t k_signing[32];
   uint8_t final_signature[32];
   char k_date_hex[65];
   char k_region_hex[65];
   char k_service_hex[65];

   signature = malloc(65);
   if (!signature)
      return NULL;

   /* Build AWS4 key string */
   snprintf(aws4_key, sizeof(aws4_key), "AWS4%s", secret_key);

   /* Derive keys step by step using binary HMAC throughout */
   /* Step 1: DateKey = HMAC-SHA256("AWS4" + SecretAccessKey, Date) */
   /* Note: aws4_key is a string, but HMAC treats it as byte data */
   if (!s3_hmac_sha256_bin((uint8_t*)aws4_key, strlen(aws4_key), date, k_date))
      goto error;

   /* Debug: Show k_date */
   for (i = 0; i < 32; i++)
      snprintf(k_date_hex + 2 * i, 3, "%02x", (unsigned)k_date[i]);
   k_date_hex[64] = '\0';
   /* Step 2: DateRegionKey = HMAC-SHA256(DateKey, Region) */
   if (!s3_hmac_sha256_bin(k_date, 32, region, k_region))
      goto error;

   /* Debug: Show k_region */
   for (i = 0; i < 32; i++)
      snprintf(k_region_hex + 2 * i, 3, "%02x", (unsigned)k_region[i]);
   k_region_hex[64] = '\0';
   /* Step 3: DateRegionServiceKey = HMAC-SHA256(DateRegionKey, Service) */
   if (!s3_hmac_sha256_bin(k_region, 32, service, k_service))
      goto error;

   /* Debug: Show k_service */
   for (i = 0; i < 32; i++)
      snprintf(k_service_hex + 2 * i, 3, "%02x", (unsigned)k_service[i]);
   k_service_hex[64] = '\0';
   /* Step 4: SigningKey = HMAC-SHA256(DateRegionServiceKey, "aws4_request") */
   if (!s3_hmac_sha256_bin(k_service, 32, "aws4_request", k_signing))
      goto error;

   /* Calculate final signature using the derived signing key in binary
   * format
   */
   if (!s3_hmac_sha256_bin(k_signing, 32, string_to_sign, final_signature))
   {
      RARCH_ERR(S3_PFX "Failed to calculate final signature\n");
      goto error;
   }

   /* Convert final signature to hex string */
   for (i = 0; i < 32; i++)
      snprintf(signature + 2 * i, 3, "%02x", (unsigned)final_signature[i]);

   signature[64] = '\0';
   return signature;

error:
   free(signature);
   return NULL;
}

/* AWS Signature Version 4 calculation with provided timestamp */
static char* s3_calculate_signature_v4_with_time(const char *method, const char *canonical_uri,
                                      const char *query_string, const char *headers,
                                      const char *payload_hash, const char *region,
                                      const char *service, const char *access_key,
                                      const char *secret_key, const char *host, time_t now)
{
   char *signature = NULL;
   char date[16];
   char datetime[32];
   char credential_scope[128];
   char canonical_request[4096];
   char string_to_sign[4096];
   char signed_headers[256];
   char *canonical_headers = NULL;
   char *canonical_query_string = NULL;
   char *canonical_request_hash = NULL;

   /* Use provided timestamp */
   struct tm *tm_info = gmtime(&now);
   strftime(date, sizeof(date), "%Y%m%d", tm_info);
   strftime(datetime, sizeof(datetime), "%Y%m%dT%H%M%SZ", tm_info);

   /* Build credential scope */
   snprintf(credential_scope, sizeof(credential_scope), "%s/%s/%s/aws4_request",
            date, region, service);

   /* Build canonical headers */
   if (headers)
   {
      /* Format headers for canonical request - must be sorted alphabetically
       * and lowercase. Each header must be trimmed and end with a newline.
       * Each header must be in the following order:
       * host
       * x-amz-content-sha256
       * x-amz-date
       */
      canonical_headers = malloc(512);
      if (canonical_headers)
      {
         snprintf(canonical_headers, 512, "host:%s\nx-amz-content-sha256:%s\nx-amz-date:%s\n",
                  host, payload_hash, datetime);
      }

   }

   /* Build signed headers list */
   snprintf(signed_headers, sizeof(signed_headers), "host;x-amz-content-sha256;x-amz-date");

   /* Build canonical query string */
   if (query_string)
      canonical_query_string = s3_canonicalize_query_string(query_string);

   /* Build canonical request */
   snprintf(canonical_request, sizeof(canonical_request),
            "%s\n%s\n%s\n%s\n%s\n%s",
            method,
            canonical_uri,
            canonical_query_string ? canonical_query_string : "",
            canonical_headers ? canonical_headers : "",
            signed_headers,
            payload_hash);

   /* Calculate SHA256 hash of canonical request */
   canonical_request_hash = s3_sha256_hash(canonical_request, strlen(canonical_request));
   if (!canonical_request_hash)
      goto cleanup;

   /* Build string to sign */
   snprintf(string_to_sign, sizeof(string_to_sign),
            "%s\n%s\n%s\n%s",
            S3_SIGNATURE_VERSION,
            datetime,
            credential_scope,
            canonical_request_hash);

   /* Calculate signature using AWS Signature Version 4 key derivation */
   signature = s3_calculate_aws4_signature(secret_key, date, region, service, string_to_sign);
   if (!signature)
      RARCH_ERR(S3_PFX "Failed to calculate signature\n");

   /* Cleanup */
   cleanup:
   if (canonical_headers)
      free(canonical_headers);
   if (canonical_query_string)
      free(canonical_query_string);
   if (canonical_request_hash)
      free(canonical_request_hash);

   return signature;
}



/* Build S3 authorization header */
static char* s3_build_auth_header(const char *method, const char *canonical_uri,
                                 const char *query_string, const char *headers,
                                 const char *payload_hash, s3_state_t *s3_st)
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
                                        s3_st->host, now);

   if (!signature)
      return NULL;

   /* Build credential string */
   snprintf(credential, sizeof(credential), "%s/%s/%s/%s/aws4_request",
            s3_st->access_key_id, date, s3_st->region, S3_SERVICE);

   /* Build authorization header */
   auth_header = malloc(1024);
   snprintf(auth_header, 1024,
            "Authorization: %s Credential=%s, SignedHeaders=host;x-amz-content-sha256;x-amz-date, Signature=%s\r\n"
            "x-amz-date: %s\r\n"
            "x-amz-content-sha256: %s\r\n",
            S3_SIGNATURE_VERSION, credential, signature, datetime, payload_hash);

   free(signature);
   return auth_header;
}

static void s3_log_http_failure(const char *path, http_transfer_data_t *data)
{
   size_t i;
   RARCH_WARN(S3_PFX "Failed: %s: HTTP %d\n", path ? path : "<unknown>", data->status);
   for (i = 0; data->headers && i < data->headers->size; i++)
      RARCH_WARN(S3_PFX "%s\n", data->headers->elems[i].data);
   if (data->data)
   {
      data->data[data->len] = '\0';
      RARCH_WARN(S3_PFX "%s\n", data->data);
   }
}

static void s3_read_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_cb_state_t *s3_cb_st       = (s3_cb_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && ((data->status >= 200 && data->status < 300) || data->status == 404 || data->status == 400));
   RFILE *file                   = NULL;

   (void)task;
   (void)err;

   if (!s3_cb_st)
      return;

   if (!data)
      RARCH_WARN(S3_PFX "Did not get HTTP data for read '%s'\n", s3_cb_st->path);
   else if (!success)
      s3_log_http_failure(s3_cb_st->path, data);

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
   (void)err;

   if (!s3_cb_st)
      return;

   if (!data)
      RARCH_WARN(S3_PFX "Did not get HTTP data for update '%s'\n", s3_cb_st->path);
   else if (!success)
      s3_log_http_failure(s3_cb_st->path, data);

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
   (void)err;

   if (!s3_cb_st)
      return;

   if (!data)
      RARCH_WARN(S3_PFX "Did not get HTTP data for delete '%s'\n", s3_cb_st->path);
   else if (!success)
      s3_log_http_failure(s3_cb_st->path, data);

   if (s3_cb_st->cb)
      s3_cb_st->cb(s3_cb_st->user_data, s3_cb_st->path, success, NULL);
   free(s3_cb_st);
}

static bool s3_http_get_async(const char *url, const char *headers, s3_cb_state_t *cb_state)
{
   return task_push_http_transfer_with_headers(url, true, NULL,
         headers, s3_read_cb, cb_state) != NULL;
}

static bool s3_http_put_async(const char *url, const char *headers,
      const void *data, size_t data_len, s3_cb_state_t *cb_state)
{
   return task_push_http_transfer_with_content(url, "PUT",
         data, data_len, "application/octet-stream", true, false,
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
      strlcpy(canonical_uri, "/", canonical_uri_size);
      return true;
   }

   path_end = strpbrk(path_start, "?#");
   path_len = path_end ? (size_t)(path_end - path_start) : strlen(path_start);

   if (path_len == 0)
   {
      strlcpy(canonical_uri, "/", canonical_uri_size);
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
   size_t i;
   char prefix[64];

   if (!data || !data->headers || (!header_name || !*header_name))
      return NULL;

   snprintf(prefix, sizeof(prefix), "%s:", header_name);

   for (i = 0; i < data->headers->size; i++)
   {
      const char *line = data->headers->elems[i].data;
      const char *start = NULL;
      const char *end = NULL;
      size_t len = 0;
      char *out = NULL;

      if (!line || !*line)
         continue;
      if (!string_starts_with_case_insensitive(line, prefix))
         continue;

      start = line + strlen(prefix);
      while (*start == ' ' || *start == '\t')
         start++;

      end = start + strlen(start);
      while (end > start && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t'))
         end--;

      len = (size_t)(end - start);
      out = (char*)malloc(len + 1);
      if (!out)
         return NULL;
      memcpy(out, start, len);
      out[len] = '\0';
      return out;
   }

   return NULL;
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
   free(mp_st->file_data);
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
   size_t i;

   if (!mp_st)
      return;

   RARCH_WARN(S3_PFX "Multipart initiate diagnostic: path='%s' url_query='uploads=' canonical_query='uploads=' data=%s status=%d len=%zu headers=%zu err='%s'\n",
         mp_st->cb_state ? mp_st->cb_state->path : "<unknown>",
         data ? "present" : "missing",
         data ? data->status : -1,
         data ? data->len : 0,
         (data && data->headers) ? data->headers->size : 0,
         (err && *err) ? err : "<none>");

   if (data && data->headers)
      for (i = 0; i < data->headers->size; i++)
         RARCH_WARN(S3_PFX "Multipart initiate header[%zu]: %s\n", i, data->headers->elems[i].data);

   if (data && data->data && data->len > 0)
   {
      data->data[data->len] = '\0';
      RARCH_WARN(S3_PFX "Multipart initiate body: %s\n", data->data);
   }
}

static bool s3_multipart_fallback_single_put(s3_multipart_state_t *mp_st)
{
   s3_state_t *s3_st = s3_state_get_ptr();
   char *payload_hash = NULL;
   char *auth_header = NULL;

   if (!mp_st || !mp_st->cb_state)
      return false;

   if (mp_st->file_data && mp_st->file_size > 0)
      payload_hash = s3_sha256_hash(mp_st->file_data, mp_st->file_size);
   else
      payload_hash = strdup(S3_EMPTY_PAYLOAD_SHA256);

   if (!payload_hash)
      return false;

   auth_header = s3_build_auth_header("PUT", mp_st->canonical_uri, "",
         "host;x-amz-content-sha256;x-amz-date", payload_hash, s3_st);
   if (!auth_header)
   {
      free(payload_hash);
      return false;
   }

   if (!s3_http_put_async(mp_st->url, auth_header,
            mp_st->file_data, mp_st->file_size, mp_st->cb_state))
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

static void s3_multipart_fail(s3_multipart_state_t *mp_st, http_transfer_data_t *data, const char *phase)
{
   if (!mp_st)
      return;

   if (data)
      s3_log_http_failure(mp_st->cb_state ? mp_st->cb_state->path : phase, data);
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
   (void)err;

   if (!mp_st)
      return;

   if (!success)
   {
      s3_multipart_fail(mp_st, data, "complete");
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
         s3_multipart_fail(mp_st, NULL, "complete missing ETag");
         return;
      }
      xml_cap += strlen(mp_st->part_etags[i]) + 96;
   }

   xml_body = (char*)malloc(xml_cap);
   if (!xml_body)
   {
      s3_multipart_fail(mp_st, NULL, "complete allocation");
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
   s3_multipart_fail(mp_st, NULL, "complete start");
}

static void s3_multipart_upload_part_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_multipart_state_t *mp_st   = (s3_multipart_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && data->status >= 200 && data->status < 300);
   char *etag = NULL;

   (void)task;
   (void)err;

   if (!mp_st)
      return;

   if (!success)
   {
      s3_multipart_fail(mp_st, data, "upload part");
      return;
   }

   etag = s3_extract_header_value(data, "ETag");
   if (!etag)
   {
      RARCH_WARN(S3_PFX "Missing ETag for multipart part %u\n", (unsigned)(mp_st->current_part + 1));
      s3_multipart_fail(mp_st, data, "upload part missing etag");
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

   payload_hash = s3_sha256_hash(mp_st->file_data + offset, content_len);
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

   if (!task_push_http_transfer_with_content(url_with_query, "PUT",
            mp_st->file_data + offset, content_len, "application/octet-stream", true, true,
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
   s3_multipart_fail(mp_st, NULL, "upload part start");
}

static void s3_multipart_initiate_cb(retro_task_t *task, void *task_data, void *user_data, const char *err)
{
   s3_multipart_state_t *mp_st   = (s3_multipart_state_t*)user_data;
   http_transfer_data_t *data    = (http_transfer_data_t*)task_data;
   bool success                  = (data && data->status >= 200 && data->status < 300);
   char *response_xml            = NULL;

   (void)task;
   (void)err;

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
      s3_multipart_fail(mp_st, data, "initiate");
      return;
   }

   if (!data || (!data->data || !*data->data))
   {
      s3_multipart_fail(mp_st, data, "initiate empty response");
      return;
   }

   response_xml = (char*)malloc(data->len + 1);
   if (!response_xml)
   {
      s3_multipart_fail(mp_st, data, "initiate allocation");
      return;
   }
   memcpy(response_xml, data->data, data->len);
   response_xml[data->len] = '\0';

   mp_st->upload_id = s3_extract_xml_tag_value(response_xml, "UploadId");
   free(response_xml);
   if (!mp_st->upload_id || !*mp_st->upload_id)
   {
      RARCH_WARN(S3_PFX "Multipart initiation response missing UploadId\n");
      s3_multipart_fail(mp_st, data, "initiate missing upload id");
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
static bool s3_read(const char *path, const char *file, cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_cb_state_t *s3_cb_st = (s3_cb_state_t*)calloc(1, sizeof(s3_cb_state_t));
   s3_state_t *s3_st = s3_state_get_ptr();
   char *auth_header = NULL;
   char url[PATH_MAX_LENGTH];
   char canonical_uri[PATH_MAX_LENGTH];
   bool success = false;

   if (!s3_cb_st)
      return false;

   s3_cb_st->cb = cb;
   s3_cb_st->user_data = user_data;
   strlcpy(s3_cb_st->path, path, sizeof(s3_cb_st->path));
   strlcpy(s3_cb_st->file, file, sizeof(s3_cb_st->file));

   /* Build S3 URL - all supported formats are virtual-hosted style */
   if (!s3_build_request_url(url, sizeof(url), s3_st, path))
      goto cleanup;

   if (!s3_build_canonical_uri_from_url(url, canonical_uri, sizeof(canonical_uri)))
      goto cleanup;

   /* Build authorization header */
   auth_header = s3_build_auth_header("GET", canonical_uri, "", "host;x-amz-content-sha256;x-amz-date",
                                     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", s3_st);

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

   return success;
}

/* Update file to S3 */
static bool s3_update(const char *path, RFILE *file, cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_cb_state_t *s3_cb_st = (s3_cb_state_t*)calloc(1, sizeof(s3_cb_state_t));
   s3_multipart_state_t *mp_st = NULL;
   s3_state_t *s3_st = s3_state_get_ptr();
   char *auth_header = NULL;
   char url[PATH_MAX_LENGTH];
   char canonical_uri[PATH_MAX_LENGTH];
   char *file_data = NULL;
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

   /* Get file size and data */
   if (file)
   {
      filestream_seek(file, 0, SEEK_END);
      file_size = filestream_tell(file);
      filestream_seek(file, 0, SEEK_SET);

      if (file_size > 0)
      {
         file_data = malloc(file_size);
         if (file_data)
            filestream_read(file, file_data, file_size);
         else
         {
            RARCH_ERR(S3_PFX "Failed to allocate memory for upload (%zu bytes)\n", file_size);
            goto cleanup;
         }
      }
   }

   /* Build S3 URL - all supported formats are virtual-hosted style */
   if (!s3_build_request_url(url, sizeof(url), s3_st, path))
      goto cleanup;

   if (!s3_build_canonical_uri_from_url(url, canonical_uri, sizeof(canonical_uri)))
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
      mp_st->file_data = file_data;
      file_data = NULL;
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
      file_data = NULL;
      mp_st = NULL;
      success = true;
   }
   else
   {
      /* Single PUT request */
      if (file_data && file_size > 0)
         payload_hash = s3_sha256_hash(file_data, file_size);
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
      if (!s3_http_put_async(url, auth_header, file_data, file_size, s3_cb_st))
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
   free(file_data);
   free(payload_hash);
   free(s3_cb_st);

   return success;
}

/* Delete file from S3 */
static bool s3_free(const char *path, cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_cb_state_t *s3_cb_st = (s3_cb_state_t*)calloc(1, sizeof(s3_cb_state_t));
   s3_state_t *s3_st = s3_state_get_ptr();
   char *auth_header = NULL;
   char url[PATH_MAX_LENGTH];
   char canonical_uri[PATH_MAX_LENGTH];
   bool success = false;

   if (!s3_cb_st)
      return false;

   s3_cb_st->cb = cb;
   s3_cb_st->user_data = user_data;
   strlcpy(s3_cb_st->path, path, sizeof(s3_cb_st->path));

   /* Build S3 URL - all supported formats are virtual-hosted style */
   if (!s3_build_request_url(url, sizeof(url), s3_st, path))
      goto cleanup;

   if (!s3_build_canonical_uri_from_url(url, canonical_uri, sizeof(canonical_uri)))
      goto cleanup;

   /* Build authorization header */
   auth_header = s3_build_auth_header("DELETE", canonical_uri, "", "host;x-amz-content-sha256;x-amz-date",
                                     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", s3_st);

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

   return success;
}

/* S3 cloud sync driver */
cloud_sync_driver_t cloud_sync_s3 = {
   s3_sync_begin,
   s3_sync_end,
   s3_read,
   s3_update,
   s3_free,
   "s3"
};

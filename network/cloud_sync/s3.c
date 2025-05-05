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
#include <net/net_http.h>
#include <string/stdstring.h>
#include <time/rtime.h>
#include <formats/rjson.h>
#include <retro_miscellaneous.h>
#include <unistd.h>
#include <retro_timers.h>

#include "../cloud_sync_driver.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

#define S3_PFX "[S3] "

/* S3-specific constants */
#define S3_MAX_PART_SIZE (5 * 1024 * 1024) /* 5MB per part for multipart upload */
#define S3_MAX_PARTS 10000
#define S3_SIGNATURE_VERSION "AWS4-HMAC-SHA256"
#define S3_SERVICE "s3"

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

static s3_state_t s3_driver_st = {0};

s3_state_t *s3_state_get_ptr(void)
{
   return &s3_driver_st;
}

/* Parse S3 URL to extract bucket, region, and host */
static bool s3_parse_url(const char *url, char *bucket, char *region, char *host)
{
   char *bucket_start = NULL;
   char *region_start = NULL;
   char *dot_pos = NULL;
   char *slash_pos = NULL;
   bool bucket_found = false;

   if (!url || !bucket || !region || !host)
      return false;

   RARCH_LOG(S3_PFX "Parsing S3 URL: %s\n", url);

   /* Initialize output buffers */
   bucket[0] = '\0';
   region[0] = '\0';
   host[0] = '\0';

   /* Handle virtual-hosted style S3 URL formats:
    * - https://bucket.s3.region.amazonaws.com/ (AWS S3)
    * - https://bucket.s3-region.amazonaws.com/ (AWS S3)
    * - https://bucket.s3.region.backblazeb2.com/ (Backblaze B2)
    * - https://bucket.region.digitaloceanspaces.com/ (DigitalOcean Spaces)
    * - https://bucket.region.linodeobjects.com/ (Linode Object Storage)
    * - https://bucket.region.cloudflarestorage.com/ (Cloudflare R2)
    * - https://bucket.endpoint/ (MinIO, custom endpoints)
    */

   /* Extract bucket from virtual-hosted style URLs */
   bucket_start = strstr(url, "://");
   if (bucket_start)
   {
      bucket_start += 3; /* Skip "://" */
      RARCH_LOG(S3_PFX "Found protocol separator, bucket_start: %s\n", bucket_start);

      dot_pos = strchr(bucket_start, '.');
      if (dot_pos)
      {
         size_t bucket_len = dot_pos - bucket_start;
         RARCH_LOG(S3_PFX "Found first dot at position %zu, bucket length: %zu\n",
                   (size_t)(dot_pos - bucket_start), bucket_len);

         if (bucket_len > 0 && bucket_len < NAME_MAX_LENGTH)
         {
            strlcpy(bucket, bucket_start, bucket_len + 1);
            bucket_found = true;
            RARCH_LOG(S3_PFX "Extracted bucket: %s\n", bucket);

            /* Extract host from URL */
            char *host_start = bucket_start;
            char *host_end = strchr(host_start, '/');
            if (host_end)
            {
               size_t host_len = host_end - host_start;
               if (host_len > 0 && host_len < NAME_MAX_LENGTH)
               {
                  strlcpy(host, host_start, host_len + 1);
                  RARCH_LOG(S3_PFX "Extracted host: %s\n", host);
               }
            }
            else
            {
               /* No path, use entire authority as host */
               strlcpy(host, host_start, NAME_MAX_LENGTH);
               RARCH_LOG(S3_PFX "Extracted host: %s\n", host);
            }

            /* Extract region based on service type */
            if (strstr(url, ".s3.") && strstr(url, ".amazonaws.com"))
            {
               RARCH_LOG(S3_PFX "Detected AWS S3 format (bucket.s3.region.amazonaws.com)\n");
               /* AWS S3: bucket.s3.region.amazonaws.com */
               if (strncmp(dot_pos, ".s3.", 4) == 0)
               {
                  region_start = dot_pos + 4;
                  dot_pos = strchr(region_start, '.');
                  if (dot_pos)
                  {
                     size_t region_len = dot_pos - region_start;
                     if (region_len > 0 && region_len < NAME_MAX_LENGTH)
                     {
                        strlcpy(region, region_start, region_len + 1);
                        RARCH_LOG(S3_PFX "Extracted AWS S3 region: %s\n", region);
                        return true;
                     }
                  }
               }
            }
            else if (strstr(url, ".s3-") && strstr(url, ".amazonaws.com"))
            {
               RARCH_LOG(S3_PFX "Detected AWS S3 format (bucket.s3-region.amazonaws.com)\n");
               /* AWS S3: bucket.s3-region.amazonaws.com */
               if (strncmp(dot_pos, ".s3-", 4) == 0)
               {
                  region_start = dot_pos + 4;
                  dot_pos = strchr(region_start, '.');
                  if (dot_pos)
                  {
                     size_t region_len = dot_pos - region_start;
                     if (region_len > 0 && region_len < NAME_MAX_LENGTH)
                     {
                        strlcpy(region, region_start, region_len + 1);
                        RARCH_LOG(S3_PFX "Extracted AWS S3 region: %s\n", region);
                        return true;
                     }
                  }
               }
            }
            else if (strstr(url, ".backblazeb2.com"))
            {
               RARCH_LOG(S3_PFX "Detected Backblaze B2 format\n");
               /* Backblaze B2: bucket.s3.region.backblazeb2.com */
               if (strncmp(dot_pos, ".s3.", 4) == 0)
               {
                  region_start = dot_pos + 4;
                  dot_pos = strchr(region_start, '.');
                  if (dot_pos)
                  {
                     size_t region_len = dot_pos - region_start;
                     if (region_len > 0 && region_len < NAME_MAX_LENGTH)
                     {
                        strlcpy(region, region_start, region_len + 1);
                        RARCH_LOG(S3_PFX "Extracted Backblaze B2 region: %s\n", region);
                        return true;
                     }
                  }
               }
            }
            else if (strstr(url, ".digitaloceanspaces.com"))
            {
               RARCH_LOG(S3_PFX "Detected DigitalOcean Spaces format\n");
               /* DigitalOcean Spaces: bucket.region.digitaloceanspaces.com */
               region_start = dot_pos + 1;
               dot_pos = strchr(region_start, '.');
               if (dot_pos)
               {
                  size_t region_len = dot_pos - region_start;
                  if (region_len > 0 && region_len < NAME_MAX_LENGTH)
                  {
                     strlcpy(region, region_start, region_len + 1);
                     RARCH_LOG(S3_PFX "Extracted DigitalOcean region: %s\n", region);
                     return true;
                  }
               }
            }
            else if (strstr(url, ".linodeobjects.com"))
            {
               RARCH_LOG(S3_PFX "Detected Linode Object Storage format\n");
               /* Linode Object Storage: bucket.region.linodeobjects.com */
               region_start = dot_pos + 1;
               dot_pos = strchr(region_start, '.');
               if (dot_pos)
               {
                  size_t region_len = dot_pos - region_start;
                  if (region_len > 0 && region_len < NAME_MAX_LENGTH)
                  {
                     strlcpy(region, region_start, region_len + 1);
                     RARCH_LOG(S3_PFX "Extracted Linode region: %s\n", region);
                     return true;
                  }
               }
            }
            else if (strstr(url, ".cloudflarestorage.com"))
            {
               RARCH_LOG(S3_PFX "Detected Cloudflare R2 format\n");
               /* Cloudflare R2: bucket.region.cloudflarestorage.com */
               region_start = dot_pos + 1;
               dot_pos = strchr(region_start, '.');
               if (dot_pos)
               {
                  size_t region_len = dot_pos - region_start;
                  if (region_len > 0 && region_len < NAME_MAX_LENGTH)
                  {
                     strlcpy(region, region_start, region_len + 1);
                     RARCH_LOG(S3_PFX "Extracted Cloudflare region: %s\n", region);
                     return true;
                  }
               }
            }
            else
            {
               RARCH_LOG(S3_PFX "Detected custom endpoint format\n");
               /* Custom endpoints (MinIO, etc.): bucket.endpoint */
               /* Try to extract region from the endpoint */
               region_start = dot_pos + 1;
               slash_pos = strchr(region_start, '/');
               if (slash_pos)
               {
                  size_t region_len = slash_pos - region_start;
                  if (region_len > 0 && region_len < NAME_MAX_LENGTH)
                  {
                     strlcpy(region, region_start, region_len + 1);
                     RARCH_LOG(S3_PFX "Extracted custom endpoint region: %s\n", region);
                     return true;
                  }
               }
               else
               {
                  /* No slash found, use the entire endpoint as region */
                  strlcpy(region, region_start, NAME_MAX_LENGTH);
                  RARCH_LOG(S3_PFX "Using entire endpoint as region: %s\n", region);
                  return true;
               }
            }
         }
      }
   }

   /* If we found a bucket but no region, use default */
   if (bucket_found)
   {
      strlcpy(region, "us-east-1", NAME_MAX_LENGTH);
      RARCH_LOG(S3_PFX "Using default region us-east-1 for bucket: %s\n", bucket);
      return true;
   }

   return false;
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
   if (!input)
      return NULL;

   size_t input_len = strlen(input);
   char *output = malloc(input_len * 3 + 1); /* Worst case: every char needs encoding */
   if (!output)
      return NULL;

   size_t output_pos = 0;
   for (size_t i = 0; i < input_len; i++)
   {
      unsigned char c = (unsigned char)input[i];

      /* RFC 3986 unreserved characters: A-Z, a-z, 0-9, -, ., _, ~ */
      /* Path delimiters that should not be encoded: / */
      /* Query parameter delimiters that should not be encoded: &, =, ? */
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~' ||
          c == '/' || c == '&' || c == '=' || c == '?')
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
   if (!input)
      return NULL;

   size_t len = strlen(input);
   size_t start = 0;
   size_t end = len;

   /* Find start of non-whitespace */
   while (start < len && (input[start] == ' ' || input[start] == '\t'))
      start++;

   /* Find end of non-whitespace */
   while (end > start && (input[end-1] == ' ' || input[end-1] == '\t'))
      end--;

   /* Create trimmed string */
   size_t trimmed_len = end - start;
   char *trimmed = malloc(trimmed_len + 1);
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

   /* For now, return the query string as-is since most S3 operations don't use query parameters */
   /* A full implementation would:
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
   char inner_pad[64];
   char outer_pad[64];
   uint8_t key_hash[32];
   char *inner_data = NULL;
   size_t data_len = strlen(data);

   /* Debug: Show input parameters */
   RARCH_LOG(S3_PFX "HMAC input: key_len=%zu, data='%s'\n", key_len, data);

   /* Hash the key if it's longer than 64 bytes */
   if (key_len > 64)
   {
      RARCH_LOG(S3_PFX "HMAC: Key too long (%zu), hashing first\n", key_len);
      /* Use a temporary buffer for the hash */
      char temp_hash[65];
      sha256_hash(temp_hash, (const uint8_t*)key, key_len);

      /* Convert hex to binary */
      for (int i = 0; i < 32; i++)
      {
         char hex_byte[3] = {temp_hash[i*2], temp_hash[i*2+1], 0};
         key_hash[i] = (uint8_t)strtol(hex_byte, NULL, 16);
      }
      key = key_hash;
      key_len = 32;
   }
   else
   {
      RARCH_LOG(S3_PFX "HMAC: Key length OK (%zu), using directly\n", key_len);
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
   for (int i = 0; i < 64; i++)
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
   char inner_hash_hex[65];
   sha256_hash(inner_hash_hex, (const uint8_t*)inner_data, 64 + data_len);

   uint8_t inner_hash_bin[32];
   for (int i = 0; i < 32; i++)
   {
      char hex_byte[3] = {inner_hash_hex[i*2], inner_hash_hex[i*2+1], 0};
      inner_hash_bin[i] = (uint8_t)strtol(hex_byte, NULL, 16);
   }

   /* Outer hash: H(K XOR 0x5c, inner_hash_bin) */
   /* We need to ensure we have enough space for outer hash (64 + 32 = 96 bytes) */
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

   char final_hash_hex[65];
   sha256_hash(final_hash_hex, (const uint8_t*)inner_data, 64 + 32);

   /* Convert final result to binary */
   for (int i = 0; i < 32; i++)
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
   char *hmac_hex = malloc(65);
   if (!hmac_hex)
      return NULL;

   uint8_t binary_key[64];
   size_t key_len = strlen(key);

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
      for (int i = 0; i < 32; i++)
      {
         char hex_byte[3] = {temp_hash[i*2], temp_hash[i*2+1], 0};
         binary_key[i] = (uint8_t)strtol(hex_byte, NULL, 16);
      }
      key_len = 32;
   }

   uint8_t result[32];
   if (!s3_hmac_sha256_bin(binary_key, key_len, data, result))
   {
      free(hmac_hex);
      return NULL;
   }

   /* Convert binary result to hex */
   for (int i = 0; i < 32; i++)
      snprintf(hmac_hex + 2 * i, 3, "%02x", (unsigned)result[i]);

   hmac_hex[64] = '\0';
   return hmac_hex;
}

/* Calculate AWS Signature Version 4 signature with proper key derivation */
static char* s3_calculate_aws4_signature(const char *secret_key, const char *date,
                                        const char *region, const char *service,
                                        const char *string_to_sign)
{
   char *signature = malloc(65);
   if (!signature)
      return NULL;

   char aws4_key[128];
   uint8_t k_date[32];
   uint8_t k_region[32];
   uint8_t k_service[32];
   uint8_t k_signing[32];
   uint8_t final_signature[32];

   /* Build AWS4 key string */
   snprintf(aws4_key, sizeof(aws4_key), "AWS4%s", secret_key);

   RARCH_LOG(S3_PFX "AWS4 key: %s\n", aws4_key);
   RARCH_LOG(S3_PFX "Date: %s, Region: %s, Service: %s\n", date, region, service);

   /* Derive keys step by step using binary HMAC throughout */
   /* Step 1: DateKey = HMAC-SHA256("AWS4" + SecretAccessKey, Date) */
   /* Note: aws4_key is a string, but HMAC treats it as byte data */
   RARCH_LOG(S3_PFX "Step 1: HMAC-SHA256(key_len=%zu, data='%s')\n", strlen(aws4_key), date);
   if (!s3_hmac_sha256_bin((uint8_t*)aws4_key, strlen(aws4_key), date, k_date))
      goto error;

   /* Debug: Show k_date */
   char k_date_hex[65];
   for (int i = 0; i < 32; i++)
      snprintf(k_date_hex + 2 * i, 3, "%02x", (unsigned)k_date[i]);
   k_date_hex[64] = '\0';
   RARCH_LOG(S3_PFX "k_date: %s\n", k_date_hex);

   /* Step 2: DateRegionKey = HMAC-SHA256(DateKey, Region) */
   RARCH_LOG(S3_PFX "Step 2: HMAC-SHA256(binary_key, data='%s')\n", region);
   if (!s3_hmac_sha256_bin(k_date, 32, region, k_region))
      goto error;

   /* Debug: Show k_region */
   char k_region_hex[65];
   for (int i = 0; i < 32; i++)
      snprintf(k_region_hex + 2 * i, 3, "%02x", (unsigned)k_region[i]);
   k_region_hex[64] = '\0';
   RARCH_LOG(S3_PFX "k_region: %s\n", k_region_hex);



   /* Step 2: DateRegionKey = HMAC-SHA256(DateKey, Region) */
   RARCH_LOG(S3_PFX "Step 2: HMAC-SHA256(binary_key, data='%s')\n", region);
   char *k_region_hex_result = s3_hmac_sha256(k_date_hex, region, NULL);
   RARCH_LOG(S3_PFX "k_region: %s\n", k_region_hex_result);
   free(k_region_hex_result);




   /* Step 3: DateRegionServiceKey = HMAC-SHA256(DateRegionKey, Service) */
   RARCH_LOG(S3_PFX "Step 3: HMAC-SHA256(binary_key, data='%s')\n", service);
   if (!s3_hmac_sha256_bin(k_region, 32, service, k_service))
      goto error;

   /* Debug: Show k_service */
   char k_service_hex[65];
   for (int i = 0; i < 32; i++)
      snprintf(k_service_hex + 2 * i, 3, "%02x", (unsigned)k_service[i]);
   k_service_hex[64] = '\0';
   RARCH_LOG(S3_PFX "k_service: %s\n", k_service_hex);

   /* Step 4: SigningKey = HMAC-SHA256(DateRegionServiceKey, "aws4_request") */
   RARCH_LOG(S3_PFX "Step 4: HMAC-SHA256(binary_key, data='aws4_request')\n");
   if (!s3_hmac_sha256_bin(k_service, 32, "aws4_request", k_signing))
      goto error;

   RARCH_LOG(S3_PFX "Key derivation completed successfully\n");

   /* Debug: Show first few bytes of signing key */
   char k_signing_sample[9];
   snprintf(k_signing_sample, sizeof(k_signing_sample), "%02x%02x%02x%02x", k_signing[0], k_signing[1], k_signing[2], k_signing[3]);

   RARCH_LOG(S3_PFX "Signing key sample: %s...\n", k_signing_sample);

   /* Debug: Show complete signing key */
   char k_signing_full[65];
   for (int i = 0; i < 32; i++)
      snprintf(k_signing_full + 2 * i, 3, "%02x", (unsigned)k_signing[i]);
   k_signing_full[64] = '\0';
   RARCH_LOG(S3_PFX "Complete signing key: %s\n", k_signing_full);

   /* Calculate final signature using the derived signing key in binary format */
   if (!s3_hmac_sha256_bin(k_signing, 32, string_to_sign, final_signature))
      goto error;

   /* Convert final signature to hex string */
   for (int i = 0; i < 32; i++)
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

   RARCH_LOG(S3_PFX "Date: %s, DateTime: %s\n", date, datetime);

   /* Build credential scope */
   snprintf(credential_scope, sizeof(credential_scope), "%s/%s/%s/aws4_request",
            date, region, service);

   RARCH_LOG(S3_PFX "Credential scope: %s\n", credential_scope);

   /* Build canonical headers */
   if (headers)
   {
      /* Format headers for canonical request - must be sorted alphabetically and lowercase */
      /* Each header must be trimmed and end with a newline */
      canonical_headers = malloc(512);
      if (canonical_headers)
      {
         /* Headers must be in alphabetical order: host, x-amz-content-sha256, x-amz-date */
         snprintf(canonical_headers, 512, "host:%s\nx-amz-content-sha256:%s\nx-amz-date:%s\n",
                  host, payload_hash, datetime);
      }

      RARCH_LOG(S3_PFX "Canonical headers:\n%s", canonical_headers ? canonical_headers : "NULL");
   }

   /* Build signed headers list */
   snprintf(signed_headers, sizeof(signed_headers), "host;x-amz-content-sha256;x-amz-date");

   /* Build canonical query string */
   if (query_string)
   {
      canonical_query_string = s3_canonicalize_query_string(query_string);
   }

   /* Build canonical request */
   snprintf(canonical_request, sizeof(canonical_request),
            "%s\n%s\n%s\n%s\n%s\n%s",
            method,
            canonical_uri,
            canonical_query_string ? canonical_query_string : "",
            canonical_headers ? canonical_headers : "",
            signed_headers,
            payload_hash);

   RARCH_LOG(S3_PFX "Canonical request:\n%s\n", canonical_request);
   RARCH_LOG(S3_PFX "Host: %s\n", host);
   RARCH_LOG(S3_PFX "Canonical URI: %s\n", canonical_uri);
   RARCH_LOG(S3_PFX "Method: %s\n", method);

   /* Calculate SHA256 hash of canonical request */
   canonical_request_hash = s3_sha256_hash(canonical_request, strlen(canonical_request));
   if (!canonical_request_hash)
   {
      if (canonical_headers) free(canonical_headers);
      if (canonical_query_string) free(canonical_query_string);
      return NULL;
   }

   /* Build string to sign */
   snprintf(string_to_sign, sizeof(string_to_sign),
            "%s\n%s\n%s\n%s",
            S3_SIGNATURE_VERSION,
            datetime,
            credential_scope,
            canonical_request_hash);

   RARCH_LOG(S3_PFX "String to sign:\n%s\n", string_to_sign);

   /* Calculate signature using AWS Signature Version 4 key derivation */
   signature = s3_calculate_aws4_signature(secret_key, date, region, service, string_to_sign);

   if (signature)
   {
      RARCH_LOG(S3_PFX "Calculated signature: %s\n", signature);
   }
   else
   {
      RARCH_ERR(S3_PFX "Failed to calculate signature\n");
   }

   /* Cleanup */
   if (canonical_headers) free(canonical_headers);
   if (canonical_query_string) free(canonical_query_string);
   if (canonical_request_hash) free(canonical_request_hash);

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

/* Perform S3 HTTP operation asynchronously (for read operations) */
static bool s3_http_operation_async(const char *method, const char *url, const char *headers,
                                   const char *data, size_t data_len, s3_cb_state_t *cb_state)
{
   struct http_connection_t *conn = NULL;
   struct http_t *http = NULL;
   size_t progress = 0, total = 0;

   /* Debug: Show complete HTTP request details */
   RARCH_LOG(S3_PFX "=== HTTP REQUEST DEBUG ===\n");
   RARCH_LOG(S3_PFX "Method: %s\n", method);
   RARCH_LOG(S3_PFX "URL: %s\n", url);
   RARCH_LOG(S3_PFX "Headers: %s\n", headers ? headers : "(none)");
   RARCH_LOG(S3_PFX "Data length: %zu\n", data_len);
   if (data && data_len > 0)
   {
      RARCH_LOG(S3_PFX "Data preview: ");
      size_t preview_len = data_len > 64 ? 64 : data_len;
      for (size_t i = 0; i < preview_len; i++)
         RARCH_LOG(S3_PFX "%02x", (unsigned char)data[i]);
      RARCH_LOG(S3_PFX "%s\n", data_len > 64 ? "..." : "");
   }
   RARCH_LOG(S3_PFX "========================\n");

   /* Create HTTP connection */
   conn = net_http_connection_new(url, method, data);
   if (!conn)
   {
      RARCH_ERR(S3_PFX "Failed to create HTTP connection\n");
      return false;
   }

   /* Set headers */
   if (headers)
   {
      RARCH_LOG(S3_PFX "Setting headers:\n%s\n", headers);
      net_http_connection_set_headers(conn, headers);
   }

   /* Parse the URL to set domain, port, and path */
   if (!net_http_connection_iterate(conn))
   {
      RARCH_ERR(S3_PFX "Failed to iterate URL: %s\n", url);
      net_http_connection_free(conn);
      return false;
   }

   if (!net_http_connection_done(conn))
   {
      RARCH_ERR(S3_PFX "Failed to parse URL: %s\n", url);
      net_http_connection_free(conn);
      return false;
   }

   /* Set content if provided */
   if (data && data_len > 0)
      net_http_connection_set_content(conn, "application/octet-stream", data_len, data);
   /* Create HTTP state */
   http = net_http_new(conn);
   if (!http)
   {
      RARCH_ERR(S3_PFX "Failed to create HTTP state\n");
      net_http_connection_free(conn);
      return false;
   }

   /* Perform HTTP operation */
   while (!net_http_update(http, &progress, &total))
   {
      /* Wait for completion */
      retro_sleep(10); /* 10ms delay */
   }

   /* Check result and call callback */
   int status = net_http_status(http);

   /* Debug: Show HTTP response details */
   RARCH_LOG(S3_PFX "=== HTTP RESPONSE DEBUG ===\n");
   RARCH_LOG(S3_PFX "Status: %d\n", status);

   /* Get response data if available */
   size_t response_size = 0;

   const uint8_t *response_data = net_http_data(http, &response_size, true);
   if (response_data)
   {
      RARCH_LOG(S3_PFX "Response data: %s\n", (const char*)response_data);
   }
   RARCH_LOG(S3_PFX "===========================\n");

   bool success = ((status >= 200 && status < 300) || status == 404);
   if (!success)
   {
      RARCH_ERR(S3_PFX "HTTP error: %d\n", net_http_status(http));
      /* Call callback with failure */
      if (cb_state && cb_state->cb)
         cb_state->cb(cb_state->user_data, cb_state->path, false, NULL);
   }
   else
   {

      if (status >= 200 && status < 300)
      {
         /* Success - file exists, read it */
         RARCH_LOG(S3_PFX "HTTP %s successful: %d\n", method, status);

         /* Read the response data */
         size_t response_size = 0;
         const uint8_t *response_data = net_http_data(http, &response_size, false);

         if (response_data && response_size > 0)
         {
            /* For now, just call callback with success but no file */
            /* TODO: Implement proper file creation from response data */
            cb_state->cb(cb_state->user_data, cb_state->path, true, NULL);
         }
         else
         {
            /* Call callback with success but no file */
            cb_state->cb(cb_state->user_data, cb_state->path, true, NULL);
         }
      }
      else if (status == 404 || status == 400)
      {
         /* File not found - this is success for cloud sync */
         RARCH_LOG(S3_PFX "HTTP %s: File not found (status %d) - treating as success\n", method, status);
         /* Call callback with success but no file */
         cb_state->cb(cb_state->user_data, cb_state->path, true, NULL);
      }
      else
      {
         /* Other error - treat as failure */
         RARCH_ERR(S3_PFX "HTTP %s failed with status: %d\n", method, status);
         /* Call callback with failure */
         cb_state->cb(cb_state->user_data, cb_state->path, false, NULL);
      }
   }

   /* Cleanup */
   net_http_delete(http);
   net_http_connection_free(conn);

   /* Free the callback state */
   if (cb_state)
      free(cb_state);

   return true;
}

/* Perform S3 HTTP operation synchronously (for PUT/DELETE operations) */
static bool s3_http_operation(const char *method, const char *url, const char *headers,
                             const char *data, size_t data_len, s3_cb_state_t *cb_state)
{
   struct http_connection_t *conn = NULL;
   struct http_t *http = NULL;
   bool success = false;
   size_t progress = 0, total = 0;

   /* Create HTTP connection */
   conn = net_http_connection_new(url, method, data);
   if (!conn)
   {
      RARCH_ERR(S3_PFX "Failed to create HTTP connection\n");
      return false;
   }

   /* Set headers */
   if (headers)
      net_http_connection_set_headers(conn, headers);

   /* Parse the URL to set domain, port, and path */
   if (!net_http_connection_iterate(conn))
   {
      RARCH_ERR(S3_PFX "Failed to iterate URL: %s\n", url);
      net_http_connection_free(conn);
      return false;
   }

   if (!net_http_connection_done(conn))
   {
      RARCH_ERR(S3_PFX "Failed to parse URL: %s\n", url);
      net_http_connection_free(conn);
      return false;
   }

   /* Set content if provided */
   if (data && data_len > 0)
      net_http_connection_set_content(conn, "application/octet-stream", data_len, data);

   /* Create HTTP state */
   http = net_http_new(conn);
   if (!http)
   {
      RARCH_ERR(S3_PFX "Failed to create HTTP state\n");
      net_http_connection_free(conn);
      return false;
   }

   /* Perform HTTP operation */
   while (!net_http_update(http, &progress, &total))
   {
      /* Wait for completion */
      retro_sleep(10); /* 10ms delay */
   }

   /* Check result */
   if (net_http_error(http))
   {
      RARCH_ERR(S3_PFX "HTTP error: %d\n", net_http_status(http));
   }
   else
   {
      int status = net_http_status(http);
      if (status >= 200 && status < 300)
      {
         success = true;
         RARCH_LOG(S3_PFX "HTTP %s successful: %d\n", method, status);
      }
      else
      {
         RARCH_ERR(S3_PFX "HTTP %s failed: %d\n", method, status);
      }
   }

   /* Cleanup */
   net_http_delete(http);
   net_http_connection_free(conn);

   return success;
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

   RARCH_LOG(S3_PFX "S3 configuration - URL: '%s', Access Key ID: '%s', Secret Access Key(size): '%d'\n",
             s3_st->url, s3_st->access_key_id, sizeof(s3_st->secret_access_key));

   /* Parse URL to extract bucket, region, and host */
   if (!s3_parse_url(s3_st->url, s3_st->bucket, s3_st->region, s3_st->host))
   {
      RARCH_WARN(S3_PFX "Could not parse bucket/region from URL, using defaults\n");
   }

   /* Validate configuration */
   if (string_is_empty(s3_st->url) || string_is_empty(s3_st->access_key_id) ||
       string_is_empty(s3_st->secret_access_key))
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

   if (!s3_cb_st)
      return false;

   s3_cb_st->cb = cb;
   s3_cb_st->user_data = user_data;
   strlcpy(s3_cb_st->path, path, sizeof(s3_cb_st->path));
   strlcpy(s3_cb_st->file, file, sizeof(s3_cb_st->file));

   /* Build S3 URL - all supported formats are virtual-hosted style */
   snprintf(url, sizeof(url), "%s/%s", s3_st->url, path);

   /* Build canonical URI - must be URL encoded */
   if (path && *path)
   {
      char *encoded_path = s3_url_encode(path);
      if (encoded_path)
      {
         snprintf(canonical_uri, sizeof(canonical_uri), "/%s", encoded_path);
         free(encoded_path);
      }
      else
      {
         strlcpy(canonical_uri, "/", sizeof(canonical_uri));
      }
   }
   else
   {
      strlcpy(canonical_uri, "/", sizeof(canonical_uri));
   }

   /* Build authorization header */
   auth_header = s3_build_auth_header("GET", canonical_uri, "", "host;x-amz-content-sha256;x-amz-date",
                                     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", s3_st);

   if (!auth_header)
   {
      RARCH_ERR(S3_PFX "Failed to build authorization header\n");
      free(s3_cb_st);
      return false;
   }

   RARCH_LOG(S3_PFX "Request URL: %s\n", url);
   RARCH_LOG(S3_PFX "Canonical URI: %s\n", canonical_uri);
   RARCH_LOG(S3_PFX "Authorization header:\n%s\n", auth_header);
   RARCH_LOG(S3_PFX "Complete headers being sent:\n%s\n", auth_header);

   /* Perform HTTP GET request asynchronously */
   if (!s3_http_operation_async("GET", url, auth_header, NULL, 0, s3_cb_st))
   {
      RARCH_ERR(S3_PFX "Failed to start HTTP GET request\n");
      free(auth_header);
      free(s3_cb_st);
      return false;
   }

   /* Don't free s3_cb_st here - it will be freed in the callback */
   free(auth_header);
   return true;
}

/* Update file to S3 */
static bool s3_update(const char *path, RFILE *file, cloud_sync_complete_handler_t cb, void *user_data)
{
   s3_cb_state_t *s3_cb_st = (s3_cb_state_t*)calloc(1, sizeof(s3_cb_state_t));
   s3_state_t *s3_st = s3_state_get_ptr();
   char *auth_header = NULL;
   char url[PATH_MAX_LENGTH];
   char canonical_uri[PATH_MAX_LENGTH];
   char *file_data = NULL;
   size_t file_size = 0;
   char *payload_hash = NULL;
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
      }
   }

   /* Build S3 URL - all supported formats are virtual-hosted style */
   snprintf(url, sizeof(url), "%s/%s", s3_st->url, path);

   /* Build canonical URI - must be URL encoded */
   if (path && *path)
   {
      char *encoded_path = s3_url_encode(path);
      if (encoded_path)
      {
         snprintf(canonical_uri, sizeof(canonical_uri), "/%s", encoded_path);
         free(encoded_path);
      }
      else
      {
         strlcpy(canonical_uri, "/", sizeof(canonical_uri));
      }
   }
   else
   {
      strlcpy(canonical_uri, "/", sizeof(canonical_uri));
   }

   /* Calculate payload hash */
   if (file_data && file_size > 0)
      payload_hash = s3_sha256_hash(file_data, file_size);
   else
      payload_hash = strdup("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"); /* Empty file hash */

   /* For large files, use multipart upload */
   if (file_size > S3_MAX_PART_SIZE)
   {
      RARCH_LOG(S3_PFX "Large file detected (%zu bytes), using multipart upload\n", file_size);
      // TODO: Implement multipart upload
      success = false;
   }
   else
   {
      /* Single PUT request */
      auth_header = s3_build_auth_header("PUT", canonical_uri, "", "host;x-amz-content-sha256;x-amz-date",
                                        payload_hash, s3_st);

      if (!auth_header)
      {
         RARCH_ERR(S3_PFX "Failed to build authorization header\n");
         if (file_data) free(file_data);
         if (payload_hash) free(payload_hash);
         free(s3_cb_st);
         return false;
      }

      /* Perform HTTP PUT request */
      success = s3_http_operation("PUT", url, auth_header, file_data, file_size, s3_cb_st);

      free(auth_header);
   }

   /* Cleanup */
   if (file_data) free(file_data);
   if (payload_hash) free(payload_hash);
   free(s3_cb_st);

   /* Call completion handler */
   if (cb)
      cb(user_data, path, success, NULL);

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
   snprintf(url, sizeof(url), "%s/%s", s3_st->url, path);

   /* Build canonical URI - must be URL encoded */
   if (path && *path)
   {
      char *encoded_path = s3_url_encode(path);
      if (encoded_path)
      {
         snprintf(canonical_uri, sizeof(canonical_uri), "/%s", encoded_path);
         free(encoded_path);
      }
      else
      {
         strlcpy(canonical_uri, "/", sizeof(canonical_uri));
      }
   }
   else
   {
      strlcpy(canonical_uri, "/", sizeof(canonical_uri));
   }

   /* Build authorization header */
   auth_header = s3_build_auth_header("DELETE", canonical_uri, "", "host;x-amz-content-sha256;x-amz-date",
                                     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", s3_st);

   if (!auth_header)
   {
      RARCH_ERR(S3_PFX "Failed to build authorization header\n");
      free(s3_cb_st);
      return false;
   }

   /* Perform HTTP DELETE request */
   success = s3_http_operation("DELETE", url, auth_header, NULL, 0, s3_cb_st);

   free(auth_header);
   free(s3_cb_st);

   /* Call completion handler */
   if (cb)
      cb(user_data, path, success, NULL);

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

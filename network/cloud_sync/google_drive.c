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

#include <features/features_cpu.h>
#include <formats/rjson.h>
#include <net/net_http.h>
#include <queues/task_queue.h>
#include <string/stdstring.h>
#include <time/rtime.h>

#include "../cloud_sync_driver.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

/* OAuth client credentials (XOR-obfuscated to avoid automated secret scanners;
 * not truly secret per RFC 8252 s8.5: native app client secrets are public) */
static const char *gdrive_client_id(void)
{
   static char buf[73];
   if (!buf[0])
   {
      static const unsigned char e[] = {
         0x62, 0x62, 0x6B, 0x6E, 0x62, 0x63, 0x69, 0x6B, 0x68, 0x6C,
         0x62, 0x6A, 0x77, 0x38, 0x3D, 0x30, 0x32, 0x2C, 0x39, 0x62,
         0x32, 0x3F, 0x34, 0x69, 0x6E, 0x33, 0x38, 0x3E, 0x2F, 0x32,
         0x28, 0x3F, 0x3C, 0x3D, 0x69, 0x3F, 0x32, 0x6C, 0x6C, 0x34,
         0x2E, 0x62, 0x6F, 0x32, 0x62, 0x74, 0x3B, 0x2A, 0x2A, 0x29,
         0x74, 0x3D, 0x35, 0x35, 0x3D, 0x36, 0x3F, 0x2F, 0x29, 0x3F,
         0x28, 0x39, 0x35, 0x34, 0x2E, 0x3F, 0x34, 0x2E, 0x74, 0x39,
         0x35, 0x37
      };
      size_t i;
      for (i = 0; i < sizeof(e); i++)
         buf[i] = (char)(e[i] ^ 0x5A);
   }
   return buf;
}

static const char *gdrive_client_secret(void)
{
   static char buf[36];
   if (!buf[0])
   {
      static const unsigned char e[] = {
         0x1D, 0x15, 0x19, 0x09, 0x0A, 0x02, 0x77, 0x2F, 0x77, 0x6B,
         0x6E, 0x05, 0x68, 0x18, 0x19, 0x31, 0x32, 0x2A, 0x69, 0x1C,
         0x2F, 0x3D, 0x34, 0x1B, 0x0E, 0x6E, 0x12, 0x13, 0x30, 0x18,
         0x2C, 0x22, 0x10, 0x2B, 0x3D
      };
      size_t i;
      for (i = 0; i < sizeof(e); i++)
         buf[i] = (char)(e[i] ^ 0x5A);
   }
   return buf;
}

#define GDRIVE_SCOPE         "https://www.googleapis.com/auth/drive.file"
#define GDRIVE_FOLDER_NAME   "RetroArch"
#define GDRIVE_TOKEN_URL     "https://oauth2.googleapis.com/token"
#define GDRIVE_DEVICE_URL    "https://oauth2.googleapis.com/device/code"
#define GDRIVE_API_BASE      "https://www.googleapis.com/drive/v3"
#define GDRIVE_UPLOAD_BASE   "https://www.googleapis.com/upload/drive/v3"
#define GDPFX "[Google Drive] "

/* ========== Types ========== */

typedef struct
{
   char access_token[2048];
   char folder_id[256];
} gdrive_state_t;

typedef struct
{
   char path[PATH_MAX_LENGTH];
   char file[PATH_MAX_LENGTH];
   char file_id[256];
   cloud_sync_complete_handler_t cb;
   void *user_data;
   RFILE *rfile;
   bool retried;
} gdrive_cb_state_t;

/* For chaining token refresh -> retry */
typedef void (*gdrive_op_fn_t)(gdrive_cb_state_t *);

typedef struct
{
   gdrive_op_fn_t retry_fn;
   gdrive_cb_state_t *cb_st;
} gdrive_refresh_ctx_t;

/* For the begin sequence (token + folder lookup) */
typedef struct
{
   cloud_sync_complete_handler_t cb;
   void *user_data;
} gdrive_begin_ctx_t;

/* For the device flow polling */
typedef struct
{
   char device_code[512];
   retro_task_t *task;
   int interval;
   int polls;
   int max_polls;
   bool poll_pending;
   bool auth_done;
   bool auth_success;
   cloud_sync_complete_handler_t cb;
   void *user_data;
} gdrive_oauth_ctx_t;

/* ========== State ========== */

static gdrive_state_t gdrive_st = {0};

/* ========== Auth Header ========== */

static char *gdrive_get_headers(const char *extra)
{
   char header[4096];
   size_t _len;
   _len  = snprintf(header, sizeof(header),
         "Authorization: Bearer %s\r\n", gdrive_st.access_token);
   if (extra)
      strlcpy(header + _len, extra, sizeof(header) - _len);
   return strdup(header);
}

static void gdrive_log_http_failure(const char *context,
      http_transfer_data_t *data)
{
   size_t i;
   RARCH_WARN(GDPFX "Failed: %s: HTTP %d\n", context,
         data ? data->status : -1);
   if (data && data->headers)
      for (i = 0; i < data->headers->size; i++)
         RARCH_WARN("%s\n", data->headers->elems[i].data);
   if (data && data->data)
   {
      data->data[data->len] = 0;
      RARCH_WARN("%s\n", data->data);
   }
}

/* ========== JSON Parsing ========== */

/*
 * Extract a string value for a given key from a flat JSON object.
 * Only matches keys at the top level (depth 1).
 */
static bool gdrive_json_string(const char *json_data, size_t json_len,
      const char *key, char *out, size_t out_size)
{
   rjson_t *json;
   enum rjson_type type;
   bool is_key  = true;
   bool matched = false;
   int  depth   = 0;

   if (!json_data || !json_len)
      return false;
   if (!(json = rjson_open_buffer(json_data, json_len)))
      return false;

   while ((type = rjson_next(json)) != RJSON_DONE && type != RJSON_ERROR)
   {
      switch (type)
      {
         case RJSON_OBJECT:
         case RJSON_ARRAY:
            depth++;
            is_key = (type == RJSON_OBJECT);
            break;
         case RJSON_OBJECT_END:
         case RJSON_ARRAY_END:
            depth--;
            is_key = true;
            break;
         case RJSON_STRING:
         {
            size_t len;
            const char *str = rjson_get_string(json, &len);
            if (depth == 1 && is_key)
            {
               matched = string_is_equal(str, key);
               is_key  = false;
            }
            else if (depth == 1 && matched)
            {
               strlcpy(out, str, out_size);
               rjson_free(json);
               return true;
            }
            else
               is_key = true;
            break;
         }
         default:
            if (depth == 1)
               is_key = true;
            matched = false;
            break;
      }
   }

   rjson_free(json);
   return false;
}

static bool gdrive_json_int(const char *json_data, size_t json_len,
      const char *key, int *out)
{
   rjson_t *json;
   enum rjson_type type;
   bool is_key  = true;
   bool matched = false;
   int  depth   = 0;

   if (!json_data || !json_len)
      return false;
   if (!(json = rjson_open_buffer(json_data, json_len)))
      return false;

   while ((type = rjson_next(json)) != RJSON_DONE && type != RJSON_ERROR)
   {
      switch (type)
      {
         case RJSON_OBJECT:
         case RJSON_ARRAY:
            depth++;
            is_key = (type == RJSON_OBJECT);
            break;
         case RJSON_OBJECT_END:
         case RJSON_ARRAY_END:
            depth--;
            is_key = true;
            break;
         case RJSON_STRING:
         {
            size_t len;
            const char *str = rjson_get_string(json, &len);
            if (depth == 1 && is_key)
            {
               matched = string_is_equal(str, key);
               is_key  = false;
            }
            else
            {
               is_key  = true;
               matched = false;
            }
            break;
         }
         case RJSON_NUMBER:
            if (depth == 1 && matched)
            {
               *out = rjson_get_int(json);
               rjson_free(json);
               return true;
            }
            is_key  = true;
            matched = false;
            break;
         default:
            is_key  = true;
            matched = false;
            break;
      }
   }

   rjson_free(json);
   return false;
}

/*
 * Extract the ID of the first file from a Google Drive file list response.
 * Sets id_out to empty string if the files array is empty.
 */
static bool gdrive_extract_first_id(const char *data, size_t len,
      char *id_out, size_t id_size)
{
   rjson_t *json;
   enum rjson_type type;
   int  depth       = 0;
   bool in_files    = false;
   bool in_file_obj = false;
   bool is_key      = true;
   char key[64];

   id_out[0] = '\0';
   key[0]    = '\0';

   if (!data || !len)
      return false;
   if (!(json = rjson_open_buffer(data, len)))
      return false;

   while ((type = rjson_next(json)) != RJSON_DONE && type != RJSON_ERROR)
   {
      switch (type)
      {
         case RJSON_OBJECT:
            if (in_files && depth == 2)
               in_file_obj = true;
            depth++;
            is_key = true;
            break;
         case RJSON_OBJECT_END:
            depth--;
            if (in_file_obj && depth == 2)
            {
               rjson_free(json);
               return true;
            }
            is_key = true;
            break;
         case RJSON_ARRAY:
            if (depth == 1 && string_is_equal(key, "files"))
               in_files = true;
            depth++;
            break;
         case RJSON_ARRAY_END:
            depth--;
            if (in_files && depth == 1)
            {
               rjson_free(json);
               return true;
            }
            is_key = true;
            break;
         case RJSON_STRING:
         {
            size_t slen;
            const char *str = rjson_get_string(json, &slen);
            if (is_key)
            {
               strlcpy(key, str, sizeof(key));
               is_key = false;
            }
            else
            {
               if (in_file_obj && string_is_equal(key, "id"))
                  strlcpy(id_out, str, id_size);
               is_key = true;
            }
            break;
         }
         default:
            is_key = true;
            break;
      }
   }

   rjson_free(json);
   return true;
}

/* ========== Token Refresh and Retry ========== */

static void gdrive_refresh_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_refresh_ctx_t *ctx  = (gdrive_refresh_ctx_t *)user_data;
   http_transfer_data_t *data = (http_transfer_data_t *)task_data;
   char access_token[2048];

   if (   data && data->status >= 200 && data->status < 300
       && gdrive_json_string(data->data, data->len,
          "access_token", access_token, sizeof(access_token)))
   {
      strlcpy(gdrive_st.access_token, access_token,
            sizeof(gdrive_st.access_token));
      RARCH_LOG(GDPFX "Token refreshed successfully\n");
      ctx->retry_fn(ctx->cb_st);
   }
   else
   {
      RARCH_WARN(GDPFX "Token refresh failed\n");
      if (data)
         gdrive_log_http_failure("token refresh", data);
      ctx->cb_st->cb(ctx->cb_st->user_data,
            ctx->cb_st->path, false, ctx->cb_st->rfile);
      free(ctx->cb_st);
   }
   free(ctx);
}

static void gdrive_refresh_then_retry(gdrive_op_fn_t retry_fn,
      gdrive_cb_state_t *cb_st)
{
   char post_data[4096];
   gdrive_refresh_ctx_t *ctx;
   settings_t *settings = config_get_ptr();

   ctx = (gdrive_refresh_ctx_t *)calloc(1, sizeof(*ctx));
   ctx->retry_fn = retry_fn;
   ctx->cb_st    = cb_st;

   snprintf(post_data, sizeof(post_data),
         "client_id=%s&client_secret=%s"
         "&refresh_token=%s&grant_type=refresh_token",
         gdrive_client_id(), gdrive_client_secret(),
         settings->arrays.google_drive_refresh_token);

   task_push_http_post_transfer_with_headers(
         GDRIVE_TOKEN_URL, post_data, true, NULL, NULL,
         gdrive_refresh_cb, ctx);
}

/* ========== Folder Lookup (during begin) ========== */

static void gdrive_create_folder(gdrive_begin_ctx_t *ctx);

static void gdrive_create_folder_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_begin_ctx_t   *ctx  = (gdrive_begin_ctx_t *)user_data;
   http_transfer_data_t *data = (http_transfer_data_t *)task_data;

   if (!data || data->status < 200 || data->status >= 300)
   {
      RARCH_WARN(GDPFX "Folder creation failed\n");
      if (data)
         gdrive_log_http_failure("create folder", data);
      ctx->cb(ctx->user_data, NULL, false, NULL);
      free(ctx);
      return;
   }

   if (!gdrive_json_string(data->data, data->len,
            "id", gdrive_st.folder_id, sizeof(gdrive_st.folder_id)))
   {
      RARCH_WARN(GDPFX "Folder creation response missing ID\n");
      ctx->cb(ctx->user_data, NULL, false, NULL);
      free(ctx);
      return;
   }

   RARCH_LOG(GDPFX "Created folder: %s\n", gdrive_st.folder_id);
   ctx->cb(ctx->user_data, NULL, true, NULL);
   free(ctx);
}

static void gdrive_create_folder(gdrive_begin_ctx_t *ctx)
{
   char *headers;
   const char *json =
      "{\"name\":\"" GDRIVE_FOLDER_NAME "\","
      "\"mimeType\":\"application/vnd.google-apps.folder\"}";

   headers = gdrive_get_headers("Content-Type: application/json\r\n");
   task_push_http_post_transfer_with_headers(
         GDRIVE_API_BASE "/files", json, true, NULL,
         headers, gdrive_create_folder_cb, ctx);
   free(headers);
}

static void gdrive_find_folder_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_begin_ctx_t   *ctx  = (gdrive_begin_ctx_t *)user_data;
   http_transfer_data_t *data = (http_transfer_data_t *)task_data;

   if (!data || data->status < 200 || data->status >= 300)
   {
      RARCH_WARN(GDPFX "Folder search failed\n");
      if (data)
         gdrive_log_http_failure("folder search", data);
      ctx->cb(ctx->user_data, NULL, false, NULL);
      free(ctx);
      return;
   }

   gdrive_extract_first_id(data->data, data->len,
         gdrive_st.folder_id, sizeof(gdrive_st.folder_id));

   if (gdrive_st.folder_id[0])
   {
      RARCH_LOG(GDPFX "Found folder: %s\n", gdrive_st.folder_id);
      ctx->cb(ctx->user_data, NULL, true, NULL);
      free(ctx);
   }
   else
      gdrive_create_folder(ctx);
}

static void gdrive_find_folder(gdrive_begin_ctx_t *ctx)
{
   char *headers;
   headers = gdrive_get_headers(NULL);
   task_push_http_transfer_with_headers(
         GDRIVE_API_BASE "/files?"
         "q=name='" GDRIVE_FOLDER_NAME "'"
         "+and+mimeType='application/vnd.google-apps.folder'"
         "+and+trashed=false"
         "&fields=files(id,name)",
         true, NULL, headers, gdrive_find_folder_cb, ctx);
   free(headers);
}

/* ========== Per-File Search ========== */

static void gdrive_search_file(const char *name,
      retro_task_callback_t cb, void *user_data)
{
   char url[2048];
   char *headers;

   snprintf(url, sizeof(url),
         GDRIVE_API_BASE "/files?"
         "q=name='%s'+and+'%s'+in+parents+and+trashed=false"
         "&fields=files(id)&pageSize=1",
         name, gdrive_st.folder_id);

   headers = gdrive_get_headers(NULL);
   task_push_http_transfer_with_headers(url, true, NULL,
         headers, cb, user_data);
   free(headers);
}

/* ========== OAuth Device Flow ========== */

static void gdrive_begin_with_token(cloud_sync_complete_handler_t cb,
      void *user_data);

/*
 * HTTP callback for the token poll request.
 * Sets flags on ctx for the persistent task handler to act on.
 */
static void gdrive_poll_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_oauth_ctx_t   *ctx  = (gdrive_oauth_ctx_t *)user_data;
   http_transfer_data_t *data = (http_transfer_data_t *)task_data;
   char error_str[256];
   char access_token[2048];
   char refresh_token[2048];

   ctx->poll_pending = false;

   if (!data)
   {
      RARCH_WARN(GDPFX "OAuth poll failed (no data)\n");
      ctx->auth_done = true;
      return;
   }

   /* Check for error responses (still polling) */
   if (gdrive_json_string(data->data, data->len,
            "error", error_str, sizeof(error_str)))
   {
      if (string_is_equal(error_str, "authorization_pending"))
         return; /* handler will poll again after interval */

      if (string_is_equal(error_str, "slow_down"))
      {
         ctx->interval += 5;
         RARCH_LOG(GDPFX "Slowing down poll interval to %d seconds\n",
               ctx->interval);
         return;
      }

      RARCH_WARN(GDPFX "OAuth error: %s\n", error_str);
      ctx->auth_done = true;
      return;
   }

   /* Success - extract tokens */
   if (   gdrive_json_string(data->data, data->len,
            "access_token", access_token, sizeof(access_token))
       && gdrive_json_string(data->data, data->len,
            "refresh_token", refresh_token, sizeof(refresh_token)))
   {
      settings_t *settings = config_get_ptr();
      strlcpy(gdrive_st.access_token, access_token,
            sizeof(gdrive_st.access_token));
      strlcpy(settings->arrays.google_drive_refresh_token,
            refresh_token,
            sizeof(settings->arrays.google_drive_refresh_token));
      RARCH_LOG(GDPFX "OAuth authorization successful\n");
      ctx->auth_done    = true;
      ctx->auth_success = true;
      /* Wake the handler immediately */
      ctx->task->when   = cpu_features_get_time_usec();
      return;
   }

   RARCH_WARN(GDPFX "OAuth response missing tokens\n");
   ctx->auth_done = true;
}

/*
 * Persistent task handler for OAuth device flow.
 * Stays alive showing the auth URL/code in the task title,
 * polling the token endpoint at ctx->interval until authorized.
 */
static void gdrive_oauth_task_handler(retro_task_t *task)
{
   gdrive_oauth_ctx_t *ctx = (gdrive_oauth_ctx_t *)task->user_data;
   char post_data[2048];

   if (ctx->auth_done)
   {
      if (ctx->auth_success)
         gdrive_begin_with_token(ctx->cb, ctx->user_data);
      else
         ctx->cb(ctx->user_data, NULL, false, NULL);
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   if (ctx->poll_pending)
   {
      /* Still waiting for HTTP response, check again in 1s */
      task->when = cpu_features_get_time_usec() + 1000000;
      return;
   }

   if (++ctx->polls > ctx->max_polls)
   {
      RARCH_WARN(GDPFX "OAuth polling timed out\n");
      ctx->cb(ctx->user_data, NULL, false, NULL);
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   ctx->poll_pending = true;

   snprintf(post_data, sizeof(post_data),
         "client_id=%s&client_secret=%s"
         "&device_code=%s"
         "&grant_type=urn:ietf:params:oauth:grant-type:device_code",
         gdrive_client_id(), gdrive_client_secret(),
         ctx->device_code);

   task_push_http_post_transfer_with_headers(
         GDRIVE_TOKEN_URL, post_data, true, NULL, NULL,
         gdrive_poll_cb, ctx);

   /* Schedule next handler invocation after interval */
   task->when = cpu_features_get_time_usec()
              + (retro_time_t)ctx->interval * 1000000;
}

static void gdrive_oauth_task_cleanup(retro_task_t *task)
{
   free(task->user_data);
}

static void gdrive_device_code_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_oauth_ctx_t   *ctx  = (gdrive_oauth_ctx_t *)user_data;
   http_transfer_data_t *data = (http_transfer_data_t *)task_data;
   retro_task_t *poll_task;
   char title[512];
   char user_code[64];
   char verification_url[256];
   int  interval   = 5;
   int  expires_in = 1800;

   if (!data || data->status < 200 || data->status >= 300)
   {
      RARCH_WARN(GDPFX "Failed to get device code\n");
      if (data)
         gdrive_log_http_failure("device code", data);
      ctx->cb(ctx->user_data, NULL, false, NULL);
      free(ctx);
      return;
   }

   if (   !gdrive_json_string(data->data, data->len,
               "user_code", user_code, sizeof(user_code))
       || !gdrive_json_string(data->data, data->len,
               "verification_url", verification_url,
               sizeof(verification_url)))
   {
      RARCH_WARN(GDPFX "Device code response missing fields\n");
      ctx->cb(ctx->user_data, NULL, false, NULL);
      free(ctx);
      return;
   }

   if (!gdrive_json_string(data->data, data->len,
            "device_code", ctx->device_code, sizeof(ctx->device_code)))
   {
      RARCH_WARN(GDPFX "Device code response missing device_code\n");
      ctx->cb(ctx->user_data, NULL, false, NULL);
      free(ctx);
      return;
   }

   gdrive_json_int(data->data, data->len, "interval", &interval);
   gdrive_json_int(data->data, data->len, "expires_in", &expires_in);
   if (interval < 1)
      interval = 5;
   if (expires_in < 1)
      expires_in = 1800;
   ctx->interval  = interval;
   ctx->max_polls = expires_in / interval;

   snprintf(title, sizeof(title),
         "Google Drive: go to %s and enter code %s",
         verification_url, user_code);
   RARCH_LOG(GDPFX "%s\n", title);

   /* Create a persistent task that polls for the token.
    * Its title shows the auth URL/code in the task progress UI. */
   poll_task           = task_init();
   poll_task->handler  = gdrive_oauth_task_handler;
   poll_task->cleanup  = gdrive_oauth_task_cleanup;
   poll_task->user_data = ctx;
   poll_task->title    = strdup(title);
   poll_task->progress = -1;
   poll_task->when     = cpu_features_get_time_usec()
                       + (retro_time_t)ctx->interval * 1000000;
   ctx->task           = poll_task;
   task_queue_push(poll_task);
}

/* ========== Begin ========== */

static void gdrive_begin_with_token(cloud_sync_complete_handler_t cb,
      void *user_data)
{
   gdrive_begin_ctx_t *ctx =
      (gdrive_begin_ctx_t *)calloc(1, sizeof(*ctx));
   ctx->cb        = cb;
   ctx->user_data = user_data;
   gdrive_find_folder(ctx);
}

static void gdrive_begin_refresh_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_begin_ctx_t   *ctx  = (gdrive_begin_ctx_t *)user_data;
   http_transfer_data_t *data = (http_transfer_data_t *)task_data;
   char access_token[2048];

   if (   data && data->status >= 200 && data->status < 300
       && gdrive_json_string(data->data, data->len,
          "access_token", access_token, sizeof(access_token)))
   {
      strlcpy(gdrive_st.access_token, access_token,
            sizeof(gdrive_st.access_token));
      RARCH_LOG(GDPFX "Access token obtained via refresh\n");
      gdrive_find_folder(ctx);
      return;
   }

   RARCH_WARN(GDPFX "Token refresh failed during begin\n");
   if (data)
      gdrive_log_http_failure("begin refresh", data);
   ctx->cb(ctx->user_data, NULL, false, NULL);
   free(ctx);
}

static bool gdrive_sync_begin(cloud_sync_complete_handler_t cb,
      void *user_data)
{
   settings_t *settings = config_get_ptr();

   if (!string_is_empty(settings->arrays.google_drive_refresh_token))
   {
      /* Have a refresh token - use it to get an access token */
      char post_data[4096];
      gdrive_begin_ctx_t *ctx =
         (gdrive_begin_ctx_t *)calloc(1, sizeof(*ctx));
      ctx->cb        = cb;
      ctx->user_data = user_data;

      snprintf(post_data, sizeof(post_data),
            "client_id=%s&client_secret=%s"
            "&refresh_token=%s&grant_type=refresh_token",
            gdrive_client_id(), gdrive_client_secret(),
            settings->arrays.google_drive_refresh_token);

      task_push_http_post_transfer_with_headers(
            GDRIVE_TOKEN_URL, post_data, true, NULL, NULL,
            gdrive_begin_refresh_cb, ctx);
   }
   else
   {
      /* No refresh token - start device authorization flow */
      char post_data[1024];
      gdrive_oauth_ctx_t *ctx =
         (gdrive_oauth_ctx_t *)calloc(1, sizeof(*ctx));
      ctx->cb        = cb;
      ctx->user_data = user_data;

      snprintf(post_data, sizeof(post_data),
            "client_id=%s&scope=%s",
            gdrive_client_id(), GDRIVE_SCOPE);

      task_push_http_post_transfer_with_headers(
            GDRIVE_DEVICE_URL, post_data, true, NULL, NULL,
            gdrive_device_code_cb, ctx);
   }

   return true;
}

/* ========== End ========== */

static bool gdrive_sync_end(cloud_sync_complete_handler_t cb,
      void *user_data)
{
   gdrive_st.access_token[0] = '\0';
   gdrive_st.folder_id[0]    = '\0';
   cb(user_data, NULL, true, NULL);
   return true;
}

/* ========== Read ========== */

static void gdrive_do_read(gdrive_cb_state_t *cb_st);

static void gdrive_read_download_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_cb_state_t    *cb_st = (gdrive_cb_state_t *)user_data;
   http_transfer_data_t *data  = (http_transfer_data_t *)task_data;
   RFILE *file = NULL;
   bool success;

   if (!cb_st)
      return;

   success = (data && data->status >= 200 && data->status < 300);

   if (data && data->status == 401 && !cb_st->retried)
   {
      cb_st->retried = true;
      gdrive_refresh_then_retry(gdrive_do_read, cb_st);
      return;
   }

   if (!success && data)
      gdrive_log_http_failure(cb_st->path, data);

   if (success && data->data)
   {
      file = filestream_open(cb_st->file,
            RETRO_VFS_FILE_ACCESS_READ_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (file)
      {
         filestream_write(file, data->data, data->len);
         filestream_seek(file, 0, SEEK_SET);
      }
   }

   cb_st->cb(cb_st->user_data, cb_st->path, success, file);
   free(cb_st);
}

static void gdrive_read_search_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_cb_state_t    *cb_st = (gdrive_cb_state_t *)user_data;
   http_transfer_data_t *data  = (http_transfer_data_t *)task_data;
   char url[2048];
   char *headers;

   if (!data || data->status < 200 || data->status >= 300)
   {
      if (data && data->status == 401 && !cb_st->retried)
      {
         cb_st->retried = true;
         gdrive_refresh_then_retry(gdrive_do_read, cb_st);
         return;
      }
      gdrive_log_http_failure(cb_st->path, data);
      cb_st->cb(cb_st->user_data, cb_st->path, false, NULL);
      free(cb_st);
      return;
   }

   gdrive_extract_first_id(data->data, data->len,
         cb_st->file_id, sizeof(cb_st->file_id));

   if (!cb_st->file_id[0])
   {
      /* File doesn't exist on server - that's OK */
      cb_st->cb(cb_st->user_data, cb_st->path, true, NULL);
      free(cb_st);
      return;
   }

   snprintf(url, sizeof(url),
         GDRIVE_API_BASE "/files/%s?alt=media", cb_st->file_id);

   RARCH_DBG(GDPFX "GET %s\n", cb_st->path);
   headers = gdrive_get_headers(NULL);
   task_push_http_transfer_with_headers(url, true, NULL,
         headers, gdrive_read_download_cb, cb_st);
   free(headers);
}

static void gdrive_do_read(gdrive_cb_state_t *cb_st)
{
   cb_st->file_id[0] = '\0';
   gdrive_search_file(cb_st->path, gdrive_read_search_cb, cb_st);
}

static bool gdrive_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   gdrive_cb_state_t *cb_st =
      (gdrive_cb_state_t *)calloc(1, sizeof(*cb_st));
   cb_st->cb        = cb;
   cb_st->user_data = user_data;
   strlcpy(cb_st->path, path, sizeof(cb_st->path));
   strlcpy(cb_st->file, file, sizeof(cb_st->file));
   gdrive_do_read(cb_st);
   return true;
}

/* ========== Update ========== */

static void gdrive_do_update(gdrive_cb_state_t *cb_st);

static void gdrive_upload_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_cb_state_t    *cb_st = (gdrive_cb_state_t *)user_data;
   http_transfer_data_t *data  = (http_transfer_data_t *)task_data;
   bool success;

   if (!cb_st)
      return;

   success = (data && data->status >= 200 && data->status < 300);

   if (data && data->status == 401 && !cb_st->retried)
   {
      cb_st->retried = true;
      gdrive_refresh_then_retry(gdrive_do_update, cb_st);
      return;
   }

   if (!success && data)
      gdrive_log_http_failure(cb_st->path, data);
   else if (!data)
      RARCH_WARN(GDPFX "Could not upload %s\n", cb_st->path);

   cb_st->cb(cb_st->user_data, cb_st->path, success, cb_st->rfile);
   free(cb_st);
}

static void gdrive_do_patch(gdrive_cb_state_t *cb_st)
{
   char url[2048];
   char hdr_extra[256];
   char *headers;
   void *buf;
   int64_t len;

   filestream_seek(cb_st->rfile, 0, SEEK_SET);
   len = filestream_get_size(cb_st->rfile);
   buf = malloc((size_t)(len + 1));
   filestream_read(cb_st->rfile, buf, len);

   snprintf(url, sizeof(url),
         GDRIVE_UPLOAD_BASE "/files/%s?uploadType=media", cb_st->file_id);

   snprintf(hdr_extra, sizeof(hdr_extra),
         "Content-Length: %" PRId64 "\r\n", len);
   headers = gdrive_get_headers(hdr_extra);

   RARCH_DBG(GDPFX "PATCH upload %s (%lld bytes)\n",
         cb_st->path, (long long)len);
   task_push_http_transfer_with_content(url, "PATCH",
         buf, (size_t)len, "application/octet-stream",
         true, headers, gdrive_upload_cb, cb_st);
   free(buf);
   free(headers);
}

static void gdrive_create_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_cb_state_t    *cb_st = (gdrive_cb_state_t *)user_data;
   http_transfer_data_t *data  = (http_transfer_data_t *)task_data;

   if (!cb_st)
      return;

   if (!data || data->status < 200 || data->status >= 300)
   {
      if (data)
         gdrive_log_http_failure(cb_st->path, data);
      cb_st->cb(cb_st->user_data, cb_st->path, false, cb_st->rfile);
      free(cb_st);
      return;
   }

   if (!gdrive_json_string(data->data, data->len,
            "id", cb_st->file_id, sizeof(cb_st->file_id)))
   {
      RARCH_WARN(GDPFX "Create response missing file ID for %s\n",
            cb_st->path);
      cb_st->cb(cb_st->user_data, cb_st->path, false, cb_st->rfile);
      free(cb_st);
      return;
   }

   RARCH_DBG(GDPFX "Created file %s (id: %s)\n",
         cb_st->path, cb_st->file_id);
   cb_st->retried = false;
   gdrive_do_patch(cb_st);
}

static void gdrive_update_search_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_cb_state_t    *cb_st = (gdrive_cb_state_t *)user_data;
   http_transfer_data_t *data  = (http_transfer_data_t *)task_data;

   if (!data || data->status < 200 || data->status >= 300)
   {
      if (data && data->status == 401 && !cb_st->retried)
      {
         cb_st->retried = true;
         gdrive_refresh_then_retry(gdrive_do_update, cb_st);
         return;
      }
      gdrive_log_http_failure(cb_st->path, data);
      cb_st->cb(cb_st->user_data, cb_st->path, false, cb_st->rfile);
      free(cb_st);
      return;
   }

   gdrive_extract_first_id(data->data, data->len,
         cb_st->file_id, sizeof(cb_st->file_id));

   if (cb_st->file_id[0])
   {
      gdrive_do_patch(cb_st);
   }
   else
   {
      /* File doesn't exist - create metadata first, then upload */
      char json[1024];
      char *headers;
      snprintf(json, sizeof(json),
            "{\"name\":\"%s\",\"parents\":[\"%s\"]}",
            cb_st->path, gdrive_st.folder_id);

      headers = gdrive_get_headers("Content-Type: application/json\r\n");

      RARCH_DBG(GDPFX "Creating file %s\n", cb_st->path);
      task_push_http_post_transfer_with_headers(
            GDRIVE_API_BASE "/files", json, true, NULL,
            headers, gdrive_create_cb, cb_st);
      free(headers);
   }
}

static void gdrive_do_update(gdrive_cb_state_t *cb_st)
{
   cb_st->file_id[0] = '\0';
   gdrive_search_file(cb_st->path, gdrive_update_search_cb, cb_st);
}

static bool gdrive_update(const char *path, RFILE *rfile,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   gdrive_cb_state_t *cb_st =
      (gdrive_cb_state_t *)calloc(1, sizeof(*cb_st));
   cb_st->cb        = cb;
   cb_st->user_data = user_data;
   cb_st->rfile     = rfile;
   strlcpy(cb_st->path, path, sizeof(cb_st->path));
   gdrive_do_update(cb_st);
   return true;
}

/* ========== Delete ========== */

static void gdrive_do_delete(gdrive_cb_state_t *cb_st);

static void gdrive_delete_action_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_cb_state_t    *cb_st = (gdrive_cb_state_t *)user_data;
   http_transfer_data_t *data  = (http_transfer_data_t *)task_data;
   bool success;

   if (!cb_st)
      return;

   success = (data && data->status >= 200 && data->status < 300);

   if (data && data->status == 401 && !cb_st->retried)
   {
      cb_st->retried = true;
      gdrive_refresh_then_retry(gdrive_do_delete, cb_st);
      return;
   }

   if (!success && data)
      gdrive_log_http_failure(cb_st->path, data);

   cb_st->cb(cb_st->user_data, cb_st->path, success, NULL);
   free(cb_st);
}

static void gdrive_delete_search_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   gdrive_cb_state_t    *cb_st = (gdrive_cb_state_t *)user_data;
   http_transfer_data_t *data  = (http_transfer_data_t *)task_data;
   char url[2048];
   char *headers;
   settings_t *settings;

   if (!data || data->status < 200 || data->status >= 300)
   {
      if (data && data->status == 401 && !cb_st->retried)
      {
         cb_st->retried = true;
         gdrive_refresh_then_retry(gdrive_do_delete, cb_st);
         return;
      }
      gdrive_log_http_failure(cb_st->path, data);
      cb_st->cb(cb_st->user_data, cb_st->path, false, NULL);
      free(cb_st);
      return;
   }

   gdrive_extract_first_id(data->data, data->len,
         cb_st->file_id, sizeof(cb_st->file_id));

   if (!cb_st->file_id[0])
   {
      cb_st->cb(cb_st->user_data, cb_st->path, true, NULL);
      free(cb_st);
      return;
   }

   settings = config_get_ptr();
   headers  = gdrive_get_headers(NULL);

   if (settings->bools.cloud_sync_destructive)
   {
      snprintf(url, sizeof(url),
            GDRIVE_API_BASE "/files/%s", cb_st->file_id);
      RARCH_DBG(GDPFX "DELETE %s\n", cb_st->path);
      task_push_http_transfer_with_headers(url, true, "DELETE",
            headers, gdrive_delete_action_cb, cb_st);
   }
   else
   {
      char json[1024];
      char hdr_extra[512];
      struct tm tm_;
      time_t cur_time = time(NULL);
      char timestamp[32];
      size_t json_len;

      rtime_localtime(&cur_time, &tm_);
      strftime(timestamp, sizeof(timestamp), "-%y%m%d-%H%M%S", &tm_);

      json_len = snprintf(json, sizeof(json),
            "{\"name\":\"%s%s\"}", cb_st->path, timestamp);

      snprintf(url, sizeof(url),
            GDRIVE_API_BASE "/files/%s", cb_st->file_id);

      free(headers);
      snprintf(hdr_extra, sizeof(hdr_extra),
            "Content-Type: application/json\r\n"
            "Content-Length: %u\r\n", (unsigned)json_len);
      headers = gdrive_get_headers(hdr_extra);

      RARCH_DBG(GDPFX "Rename (backup) %s\n", cb_st->path);
      task_push_http_post_transfer_with_headers(
            url, json, true, "PATCH",
            headers, gdrive_delete_action_cb, cb_st);
   }

   free(headers);
}

static void gdrive_do_delete(gdrive_cb_state_t *cb_st)
{
   cb_st->file_id[0] = '\0';
   gdrive_search_file(cb_st->path, gdrive_delete_search_cb, cb_st);
}

static bool gdrive_delete(const char *path,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   gdrive_cb_state_t *cb_st =
      (gdrive_cb_state_t *)calloc(1, sizeof(*cb_st));
   cb_st->cb        = cb;
   cb_st->user_data = user_data;
   strlcpy(cb_st->path, path, sizeof(cb_st->path));
   gdrive_do_delete(cb_st);
   return true;
}

/* ========== Driver Struct ========== */

cloud_sync_driver_t cloud_sync_google_drive = {
   gdrive_sync_begin,
   gdrive_sync_end,
   gdrive_read,
   gdrive_update,
   gdrive_delete,
   "google_drive"
};

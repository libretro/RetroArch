/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019-2023 - Brian Weiss
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

#include "cheevos_client.h"

#include "cheevos.h"

#include "../configuration.h"
#include "../file_path_special.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../version.h"

#include <features/features_cpu.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../frontend/frontend_driver.h"
#include "../network/net_http_special.h"
#include "../tasks/tasks_internal.h"

#ifdef HAVE_PRESENCE
#include "../network/presence.h"
#endif

#include "../deps/rcheevos/include/rc_api_runtime.h"
#include "../deps/rcheevos/include/rc_api_user.h"

/* Define this macro to log URLs. */
#undef CHEEVOS_LOG_URLS

/* Define this macro to have the password and token logged.
 * THIS WILL DISCLOSE THE USER'S PASSWORD, TAKE CARE! */
#undef CHEEVOS_LOG_PASSWORD

 /* Define this macro with a string to load a JSON file from disk with
  * that name instead of downloading the game data from retroachievements.org. */
#undef CHEEVOS_JSON_OVERRIDE

/* Define this macro with a string to save the JSON file to disk with
 * that name. */
#undef CHEEVOS_SAVE_JSON

/* Define this macro to log downloaded badge images. */
#undef CHEEVOS_LOG_BADGES

#ifndef HAVE_RC_CLIENT

/* Number of usecs to wait between posting rich presence to the site. */
/* Keep consistent with SERVER_PING_FREQUENCY from RAIntegration. */
#define CHEEVOS_PING_FREQUENCY 2 * 60 * 1000000


/****************************
 * data types               *
 ****************************/

enum rcheevos_async_io_type
{
   CHEEVOS_ASYNC_RICHPRESENCE = 0,
   CHEEVOS_ASYNC_AWARD_ACHIEVEMENT,
   CHEEVOS_ASYNC_SUBMIT_LBOARD,
   CHEEVOS_ASYNC_LOGIN,
   CHEEVOS_ASYNC_RESOLVE_HASH,
   CHEEVOS_ASYNC_FETCH_GAME_DATA,
   CHEEVOS_ASYNC_FETCH_USER_UNLOCKS,
   CHEEVOS_ASYNC_FETCH_HARDCORE_USER_UNLOCKS,
   CHEEVOS_ASYNC_START_SESSION,
   CHEEVOS_ASYNC_FETCH_BADGE
};

struct rcheevos_async_io_request;

typedef void (*rcheevos_async_handler)(struct rcheevos_async_io_request *request,
      http_transfer_data_t *data, char buffer[], size_t buffer_size);

typedef struct rcheevos_async_io_request
{
   rc_api_request_t request;
   rcheevos_async_handler handler;
   int id;
   rcheevos_client_callback callback;
   void* callback_data;
   int attempt_count;
   const char* success_message;
   const char* failure_message;
   const char* user_agent;
   char type;
} rcheevos_async_io_request;

#endif /* HAVE_RC_CLIENT */

#ifdef HAVE_THREADS
#define RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS 2
#else
#define RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS 1
#endif

#ifndef HAVE_RC_CLIENT

typedef struct rcheevos_fetch_badge_state
{
   unsigned                 badge_fetch_index;
   unsigned                 locked_badge_fetch_index;
   const char* badge_directory;
   rcheevos_client_callback callback;
   void* callback_data;
   char                     requested_badges[
      RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS][32];
} rcheevos_fetch_badge_state;

typedef struct rcheevos_fetch_badge_data
{
   rcheevos_fetch_badge_state* state;
   int                         request_index;
   void*                       data;
   size_t                      data_len;
   rcheevos_client_callback    callback;
} rcheevos_fetch_badge_data;


/****************************
 * forward declarations     *
 ****************************/

static retro_time_t rcheevos_client_prepare_ping(rcheevos_async_io_request* request);

static void rcheevos_async_http_task_callback(
      retro_task_t* task, void* task_data, void* user_data, const char* error);

static void rcheevos_async_end_request(rcheevos_async_io_request* request);

static void rcheevos_async_fetch_badge_callback(
   struct rcheevos_async_io_request* request,
   http_transfer_data_t* data, char buffer[], size_t buffer_size);

#endif /* HAVE_RC_CLIENT */

/****************************
 * user agent construction  *
 ****************************/

static int append_no_spaces(char* buffer, char* stop, const char* text)
{
   char* ptr = buffer;

   while (ptr < stop && *text)
   {
      if (*text == ' ')
      {
         *ptr++ = '_';
         ++text;
      }
      else
         *ptr++ = *text++;
   }

   *ptr = '\0';
   return (int)(ptr - buffer);
}

void rcheevos_get_user_agent(rcheevos_locals_t *locals,
      char *buffer, size_t len)
{
   char* ptr;
   struct retro_system_info *sysinfo = &runloop_state_get_ptr()->system.info;

   /* if we haven't calculated the non-changing portion yet, do so now
    * [retroarch version + os version] */
   if (!locals->user_agent_prefix[0])
   {
      const frontend_ctx_driver_t *frontend = frontend_get_ptr();

      if (frontend && frontend->get_os)
      {
         char tmp[64];
         int major, minor;
         frontend->get_os(tmp, sizeof(tmp), &major, &minor);
         snprintf(locals->user_agent_prefix, sizeof(locals->user_agent_prefix),
            "RetroArch/%s (%s %d.%d)", PACKAGE_VERSION, tmp, major, minor);
      }
      else
         snprintf(locals->user_agent_prefix, sizeof(locals->user_agent_prefix),
            "RetroArch/%s", PACKAGE_VERSION);
   }

   /* append the non-changing portion */
   ptr = buffer + strlcpy(buffer, locals->user_agent_prefix, len);

   /* if a core is loaded, append its information */
   if (sysinfo && !string_is_empty(sysinfo->library_name))
   {
      char* stop = buffer + len - 1;
      const char* path = path_get(RARCH_PATH_CORE);
      *ptr++ = ' ';

      if (!string_is_empty(path))
      {
         append_no_spaces(ptr, stop, path_basename(path));
         path_remove_extension(ptr);
         ptr += strlen(ptr);
      }
      else
         ptr += append_no_spaces(ptr, stop, sysinfo->library_name);

      if (sysinfo->library_version)
      {
         *ptr++ = '/';
         ptr += append_no_spaces(ptr, stop, sysinfo->library_version);
      }
   }

   *ptr = '\0';
}

/****************************
 * server interaction       *
 ****************************/

#ifdef CHEEVOS_LOG_URLS
#ifndef CHEEVOS_LOG_PASSWORD
static void rcheevos_filter_url_param(char* url, char* param)
{
   char *next;
   size_t param_len = strlen(param);
   char      *start = strchr(url, '?');
   if (!start)
      start         = url;
   else
      ++start;

   do
   {
      next = strchr(start, '&');

      if (start[param_len] == '=' && memcmp(start, param, param_len) == 0)
      {
         if (next)
            strcpy_literal(start, next + 1);
         else if (start > url)
            start[-1] = '\0';
         else
            *start    = '\0';

         return;
      }

      if (!next)
         return;

      start = next + 1;
   } while (1);
}
#endif
#endif

void rcheevos_log_url(const char* url)
{
#ifdef CHEEVOS_LOG_URLS
 #ifdef CHEEVOS_LOG_PASSWORD
   CHEEVOS_LOG(RCHEEVOS_TAG "GET %s\n", url);
 #else
   char copy[256];
   strlcpy(copy, url, sizeof(copy));
   rcheevos_filter_url_param(copy, "p");
   rcheevos_filter_url_param(copy, "t");
   CHEEVOS_LOG(RCHEEVOS_TAG "GET %s\n", copy);
 #endif
#else
   (void)url;
#endif
}

static void rcheevos_log_post_url(const char* url, const char* post)
{
#ifdef CHEEVOS_LOG_URLS
 #ifdef CHEEVOS_LOG_PASSWORD
   if (post && post[0])
      CHEEVOS_LOG(RCHEEVOS_TAG "POST %s %s\n", url, post);
   else
      CHEEVOS_LOG(RCHEEVOS_TAG "POST %s\n", url);
 #else
   if (post && post[0])
   {
      char post_copy[2048];
      strlcpy(post_copy, post, sizeof(post_copy));
      rcheevos_filter_url_param(post_copy, "p");
      rcheevos_filter_url_param(post_copy, "t");

      if (post_copy[0])
         CHEEVOS_LOG(RCHEEVOS_TAG "POST %s %s\n", url, post_copy);
      else
         CHEEVOS_LOG(RCHEEVOS_TAG "POST %s\n", url);
   }
   else
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "POST %s\n", url);
   }
 #endif
#else
   (void)url;
   (void)post;
#endif
}

/****************************
 * dispatch                 *
 ****************************/

#ifdef HAVE_RC_CLIENT

typedef struct rc_client_http_task_data_t
{
   rc_client_server_callback_t callback;
   void* callback_data;
} rc_client_http_task_data_t;

static void rcheevos_client_http_task_callback(retro_task_t* task,
   void* task_data, void* user_data, const char* error)
{
   rc_client_http_task_data_t* callback_data = (rc_client_http_task_data_t*)user_data;
   http_transfer_data_t* http_data = (http_transfer_data_t*)task_data;
   rc_api_server_response_t server_response;
   memset(&server_response, 0, sizeof(server_response));

   if (!http_data)
   {
      callback_data->callback(&server_response, callback_data->callback_data);
   }
   else
   {
      server_response.body = http_data->data;
      server_response.body_length = http_data->len;
      server_response.http_status_code = http_data->status;

      callback_data->callback(&server_response, callback_data->callback_data);
   }

   free(callback_data);
}

#ifdef CHEEVOS_SAVE_JSON
static void rcheevos_client_http_task_save_callback(retro_task_t* task,
   void* task_data, void* user_data, const char* error)
{
   http_transfer_data_t* http_data = (http_transfer_data_t*)task_data;

   if (http_data)
   {
      filestream_write_file(CHEEVOS_SAVE_JSON, http_data->data, http_data->len);
      CHEEVOS_LOG(RCHEEVOS_TAG "Captured game info. Wrote %u bytes to %s\n", http_data->len, CHEEVOS_SAVE_JSON);
   }

   rcheevos_client_http_task_callback(task, task_data, user_data, error);
}
#endif

#ifdef CHEEVOS_JSON_OVERRIDE
void rcheevos_client_http_load_response(const rc_api_request_t* request,
   rc_client_server_callback_t callback, void* callback_data)
{
   size_t size = 0;
   char* contents;
   FILE* file = fopen(CHEEVOS_JSON_OVERRIDE, "rb");

   fseek(file, 0, SEEK_END);
   size = ftell(file);
   fseek(file, 0, SEEK_SET);

   contents = (char*)malloc(size + 1);
   fread((void*)contents, 1, size, file);
   fclose(file);

   contents[size] = 0;
   CHEEVOS_LOG(RCHEEVOS_TAG "Loaded game info. Read %u bytes to %s\n", size, CHEEVOS_JSON_OVERRIDE);

   callback(contents, 200, callback_data);
}
#endif

void rcheevos_client_server_call(const rc_api_request_t* request,
   rc_client_server_callback_t callback, void* callback_data, rc_client_t* client)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   rc_client_http_task_data_t* taskdata = malloc(sizeof(rc_client_http_task_data_t));
   taskdata->callback = callback;
   taskdata->callback_data = callback_data;

   if (request->post_data)
   {
      rcheevos_log_post_url(request->url, request->post_data);

#ifdef CHEEVOS_JSON_OVERRIDE
      if (strstr(request->post_data, "r=patch"))
      {
         rcheevos_client_http_load_response(request, callback, callback_data);
         return;
      }
#endif

#ifdef CHEEVOS_SAVE_JSON
      if (strstr(request->post_data, "r=patch"))
      {
         task_push_http_post_transfer_with_user_agent(request->url,
            request->post_data, true, "POST", rcheevos_locals->user_agent_core,
            rcheevos_client_http_task_save_callback, taskdata);
         return;
      }
#endif

      task_push_http_post_transfer_with_user_agent(request->url,
         request->post_data, true, "POST", rcheevos_locals->user_agent_core,
         rcheevos_client_http_task_callback, taskdata);

#ifdef HAVE_PRESENCE
      if (strstr(request->post_data, "r=ping"))
         presence_update(PRESENCE_RETROACHIEVEMENTS);
#endif
   }
   else
   {
      rcheevos_log_url(request->url);
      task_push_http_transfer_with_user_agent(request->url,
         true, "GET", rcheevos_locals->user_agent_core,
         rcheevos_client_http_task_callback, taskdata);
   }
}

/****************************
 * downloading badges       *
 ****************************/

typedef struct rc_client_download_queue_t
{
   const rc_client_t* client;
   const rc_client_game_t* game;

#ifdef HAVE_THREADS
   slock_t* lock;
#endif

   rc_client_achievement_list_t* list;
   uint32_t pass;
   uint32_t bucket_index;
   uint32_t achievement_index;
   uint32_t count;
   uint32_t outstanding_requests;
} rc_client_download_queue_t;

static void rcheevos_client_fetch_next_badge(rc_client_download_queue_t* queue);

typedef struct rc_client_download_task_data_t
{
   rc_client_download_queue_t* queue;
   char badge_fullpath[PATH_MAX_LENGTH];
   char badge_name[32];
} rc_client_download_task_data_t;

static void rcheevos_client_download_task_callback(retro_task_t* task,
   void* task_data, void* user_data, const char* error)
{
   rc_client_download_task_data_t* callback_data = (rc_client_download_task_data_t*)user_data;
   http_transfer_data_t* http_data = (http_transfer_data_t*)task_data;

   if (!http_data)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "No data received for badge %s\n", callback_data->badge_name);
   }
   else if (http_data->status != 200)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "HTTP status code %d for badge %s\n", http_data->status, callback_data->badge_name);
   }
   else if (!filestream_write_file(callback_data->badge_fullpath, http_data->data, http_data->len))
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Error writing %s\n", callback_data->badge_fullpath);
   }

   if (callback_data->queue)
   {
#ifdef HAVE_THREADS
      slock_lock(callback_data->queue->lock);
#endif
      callback_data->queue->count++;
#ifdef HAVE_THREADS
      slock_unlock(callback_data->queue->lock);
#endif

      rcheevos_client_fetch_next_badge(callback_data->queue);
   }

   free(callback_data);
}

static bool rcheevos_client_download_badge(rc_client_download_queue_t* queue,
   const char* url, const char* badge_name)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   rc_client_download_task_data_t* taskdata;
   char badge_fullpath[512] = "";
   char* badge_fullname;
   size_t badge_fullname_size;

   /* make sure the directory exists */
   fill_pathname_application_special(badge_fullpath, sizeof(badge_fullpath),
      APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

   if (!path_is_directory(badge_fullpath))
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Creating %s\n", badge_fullpath);
      path_mkdir(badge_fullpath);
   }

   fill_pathname_slash(badge_fullpath, sizeof(badge_fullpath));
   badge_fullname = badge_fullpath + strlen(badge_fullpath);
   badge_fullname_size = sizeof(badge_fullpath) - (badge_fullname - badge_fullpath);
   snprintf(badge_fullname, badge_fullname_size, "%s" FILE_PATH_PNG_EXTENSION, badge_name);

   if (path_is_valid(badge_fullpath))
      return false;

#ifdef CHEEVOS_LOG_BADGES
   CHEEVOS_LOG(RCHEEVOS_TAG "Downloading %s from %s\n", badge_name, url);
#else
   rcheevos_log_url(url);
#endif

   taskdata = (rc_client_download_task_data_t*)malloc(sizeof(*taskdata));
   taskdata->queue = queue;
   strlcpy(taskdata->badge_fullpath, badge_fullpath, sizeof(taskdata->badge_fullpath));
   strlcpy(taskdata->badge_name, badge_name, sizeof(taskdata->badge_name));

   task_push_http_transfer_with_user_agent(url,
      true, "GET", rcheevos_locals->user_agent_core,
      rcheevos_client_download_task_callback, taskdata);

   return true;
}

void rcheevos_client_download_badge_from_url(const char* url, const char* badge_name)
{
   rcheevos_client_download_badge(NULL, url, badge_name);
}

static void rcheevos_client_fetch_next_badge(rc_client_download_queue_t* queue)
{
   rc_client_achievement_bucket_t* bucket;
   rc_client_achievement_t* achievement;
   const char* next_badge;
   char badge_name[32];
   char url[256];
   bool done = false;

   do
   {
      next_badge = NULL;

#ifdef HAVE_THREADS
      slock_lock(queue->lock);
#endif
      /* if the game is no longer loaded, stop processing the queue */
      if (queue->game != rc_client_get_game_info(queue->client))
         queue->pass = 2;

      while (queue->pass < 2)
      {
         if (queue->bucket_index >= queue->list->num_buckets)
         {
            queue->bucket_index = 0;
            queue->pass++;
            continue;
         }

         bucket = &queue->list->buckets[queue->bucket_index];

         if (queue->achievement_index >= bucket->num_achievements)
         {
            queue->achievement_index = 0;
            queue->bucket_index++;
            continue;
         }

         achievement = bucket->achievements[queue->achievement_index++];
         if (!achievement->badge_name[0])
            continue;

         if (queue->pass == 0)
         {
            /* first pass - get all unlocked badges */
            if (rc_client_achievement_get_image_url(achievement, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, url, sizeof(url)) != RC_OK)
               continue;

            next_badge = achievement->badge_name;
         }
         else if (achievement->unlock_time)
         {
            /* second pass - don't need locked badge for achievement player has already unlocked */
            continue;
         }
         else
         {
            /* second pass - get locked badge */
            if (rc_client_achievement_get_image_url(achievement, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE, url, sizeof(url)) != RC_OK)
               continue;

            snprintf(badge_name, sizeof(badge_name), "%s_lock", achievement->badge_name);
            next_badge = badge_name;
         }

         break;
      }

      if (!next_badge)
      {
         if (--queue->outstanding_requests == 0)
            done = true;
      }

#ifdef HAVE_THREADS
      slock_unlock(queue->lock);
#endif

      if (next_badge)
      {
         /* if the badge already exists (download_badge returns false), continue
          * looping to the next item. otherwise, a download was queued, so break
          * out of the loop. */
         if (rcheevos_client_download_badge(queue, url, next_badge))
            break;
      }
   } while (next_badge);

   if (done)
   {
      /* queue complete */
      if (queue->count)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Downloaded %u badges\n", queue->count);
      }
      rc_client_destroy_achievement_list(queue->list);

#ifdef HAVE_THREADS
      slock_free(queue->lock);
#endif

      free(queue);
   }
}

void rcheevos_client_download_placeholder_badge(void)
{
   char url[256] = "";

   if (rc_client_achievement_get_image_url(NULL, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, url, sizeof(url)) == RC_OK)
      rcheevos_client_download_badge(NULL, url, "00000");
}

void rcheevos_client_download_game_badge(const rc_client_game_t* game)
{
   char url[256] = "";
   char badge_name[16];

   if (game && rc_client_game_get_image_url(game, url, sizeof(url)) == RC_OK)
   {
      snprintf(badge_name, sizeof(badge_name), "i%s", game->badge_name);
      rcheevos_client_download_badge(NULL, url, badge_name);
   }
}

void rcheevos_client_download_achievement_badges(rc_client_t* client)
{
   rc_client_download_queue_t* queue;
   uint32_t i;

#if !defined(HAVE_GFX_WIDGETS) /* we always want badges if widgets are enabled */
   settings_t* settings = config_get_ptr();
   /* User has explicitly disabled badges */
   if (!settings->bools.cheevos_badges_enable)
      return;

   /* badges are only needed for xmb and ozone menus */
   if (!string_is_equal(settings->arrays.menu_driver, "xmb") &&
      !string_is_equal(settings->arrays.menu_driver, "ozone"))
      return;
#endif /* !defined(HAVE_GFX_WIDGETS) */

   queue = (rc_client_download_queue_t*)calloc(1, sizeof(*queue));
   queue->client = client;
   queue->game = rc_client_get_game_info(client);
   queue->list = rc_client_create_achievement_list(client,
      RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
      RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
   queue->outstanding_requests = RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS;

#ifdef HAVE_THREADS
   queue->lock = slock_new();
#endif

   for (i = 0; i < RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS; i++)
      rcheevos_client_fetch_next_badge(queue);
}

#undef RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS

void rcheevos_client_download_achievement_badge(const char* badge_name, bool locked)
{
   rc_api_fetch_image_request_t image_request;
   rc_api_request_t request;
   char locked_badge_name[32];

   memset(&image_request, 0, sizeof(image_request));
   image_request.image_type = locked ? RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED : RC_IMAGE_TYPE_ACHIEVEMENT;
   image_request.image_name = badge_name;

   if (locked)
   {
      snprintf(locked_badge_name, sizeof(locked_badge_name), "%s_lock", badge_name);
      badge_name = locked_badge_name;
   }

   if (rc_api_init_fetch_image_request(&request, &image_request) == RC_OK)
      rcheevos_client_download_badge(NULL, request.url, badge_name);

   rc_api_destroy_request(&request);
}

#else /* !HAVE_RC_CLIENT */

static void rcheevos_async_begin_http_request(rcheevos_async_io_request* request)
{
   if (request->request.post_data)
      task_push_http_post_transfer_with_user_agent(request->request.url,
         request->request.post_data, true, "POST", request->user_agent,
         rcheevos_async_http_task_callback, request);
   else
      task_push_http_transfer_with_user_agent(request->request.url,
         true, "GET", request->user_agent,
         rcheevos_async_http_task_callback, request);
}

static void rcheevos_async_retry_request(retro_task_t* task)
{
   rcheevos_async_io_request* request = (rcheevos_async_io_request*)
      task->user_data;

   /* the timer task has done its job. let it dispose itself */
   task_set_finished(task, 1);

   /* start a new task for the HTTP call */
   rcheevos_async_begin_http_request(request);
}

static void rcheevos_async_retry_request_after_delay(rcheevos_async_io_request* request, const char* error)
{
   retro_task_t* task       = task_init();
   /* Double the wait between each attempt until we hit
    * a maximum delay of two minutes.
    * 250ms -> 500ms -> 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 64s -> 120s -> 120s... */
   retro_time_t retry_delay = (request->attempt_count > 8)
      ? (120 * 1000 * 1000)
      : ((250 * 1000) << request->attempt_count);

   CHEEVOS_ERR(RCHEEVOS_TAG "%s %u: %s (automatic retry in %dms)\n", request->failure_message,
      request->id, error, (int)retry_delay / 1000);

   task->when         = cpu_features_get_time_usec() + retry_delay;
   task->handler      = rcheevos_async_retry_request;
   task->user_data    = request;
   task->progress     = -1;

   ++request->attempt_count;
   task_queue_push(task);
}

static bool rcheevos_async_request_failed(rcheevos_async_io_request* request, const char* error)
{
   /* always retry any request once (attempt_count==0) in case of network hiccup */
   if (request->attempt_count > 0)
   {
      /* retry failed, don't retry these requests */
      switch (request->type)
      {
         /* timer will ping again */
         case CHEEVOS_ASYNC_RICHPRESENCE:
         /* fallback to the placeholder image */
         case CHEEVOS_ASYNC_FETCH_BADGE:  
            return false;

         case CHEEVOS_ASYNC_RESOLVE_HASH:
         case CHEEVOS_ASYNC_LOGIN:
            /* make a maximum of four attempts 
               (0ms -> 250ms -> 500ms -> 1s) */
            if (request->attempt_count == 3)
               return false;
            break;

         default:
            break;
      }
   }

   /* automatically retry the request */
   rcheevos_async_retry_request_after_delay(request, error);
   return true;
}

static void rcheevos_async_http_task_callback(
      retro_task_t* task, void* task_data, void* user_data,
      const char* error)
{
   rcheevos_async_io_request *request = (rcheevos_async_io_request*)user_data;
   http_transfer_data_t      *data    = (http_transfer_data_t*)task_data;
   const bool                 aborted = rcheevos_load_aborted();
   char buffer[224];

   if (aborted)
   {
      /* load was aborted. don't process the response */
      strlcpy(buffer, "Load aborted", sizeof(buffer));
   }
   else if (error)
   {
      /* there was a communication error */
      /* if automatically requeued, don't process any further */
      if (rcheevos_async_request_failed(request, error))
         return;

      strlcpy(buffer, error, sizeof(buffer));
   }
   else if (!data)
   {
      /* Server did not return HTTP headers */
      strlcpy(buffer, "Server communication error", sizeof(buffer));
   }
   else if (!data->data || !data->len)
   {
      if (data->status <= 0)
      {
         /* something occurred which prevented the response from being processed.
          * assume the server request hasn't happened and try again. */
         snprintf(buffer, sizeof(buffer), "task status code %d", data->status);
         rcheevos_async_request_failed(request, buffer);
         return;
      }

      if (data->status != 200) /* Server returned error via status code. */
      {
         snprintf(buffer, sizeof(buffer), "HTTP error code %d", data->status);

         if (request->type == CHEEVOS_ASYNC_FETCH_BADGE)
         {
            /* This isn't a JSON request. An empty response with a status code is a valid response. */
            if (request->handler)
               request->handler(request, data, buffer, sizeof(buffer));
         }
      }
      else /* Server sent empty response without error status code */
         strlcpy(buffer, "No response from server", sizeof(buffer));
   }
   else
   {
      /* indicate success unless handler provides error */
      buffer[0] = '\0'; 

      /* Call appropriate handler to process the response */
      /* NOTE: data->data is not null-terminated. Most handlers assume the
       * response is properly formatted or will encounter a parse failure
       * before reading past the end of the data */
      if (request->handler)
         request->handler(request, data, buffer, sizeof(buffer));
   }

   if (!buffer[0])
   {
      /* success */
      if (request->success_message)
      {
         if (request->id)
            CHEEVOS_LOG(RCHEEVOS_TAG "%s %u\n", request->success_message, request->id);
         else
            CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", request->success_message);
      }
   }
   else
   {
      /* encountered an error */
      char errbuf[256];
      if (request->id)
         snprintf(errbuf, sizeof(errbuf), "%s %u: %s",
               request->failure_message, request->id, buffer);
      else
         snprintf(errbuf, sizeof(errbuf), "%s: %s",
               request->failure_message, buffer);

      if (!aborted)
      {
         switch (request->type)
         {
            case CHEEVOS_ASYNC_RICHPRESENCE:
            case CHEEVOS_ASYNC_FETCH_BADGE:
               /* Don't bother informing user when these fail */
               break;

            case CHEEVOS_ASYNC_LOGIN:
            case CHEEVOS_ASYNC_RESOLVE_HASH:
               if (error)
               {
                  rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
                  size_t len = 0;
                  char* ptr;

                  if (rcheevos_locals->load_info.state
                     == RCHEEVOS_LOAD_STATE_NETWORK_ERROR)
                     break;

                  rcheevos_locals->load_info.state =
                     RCHEEVOS_LOAD_STATE_NETWORK_ERROR;

                  while (
                     /* find the first single slash */
                     request->request.url[len] != '/'
                     || request->request.url[len + 1] == '/'
                     || request->request.url[len - 1] == '/')
                     ++len;

                  ptr = errbuf + snprintf(errbuf, sizeof(errbuf),
                     "Could not communicate with ");
                  memcpy(ptr, request->request.url, len);
                  ptr[len] = '\0';
               }
               /* fallthrough to default */

            default:
               runloop_msg_queue_push(errbuf, 0, 5 * 60, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
               break;
         }
      }

      CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", errbuf);
   }

   rcheevos_async_end_request(request);
}

static void rcheevos_async_end_request(rcheevos_async_io_request* request)
{
   rc_api_destroy_request(&request->request);

   if (request->callback)
      request->callback(request->callback_data);

   /* rich presence request will be reused on next ping - reset the attempt
    * counter. for all other request types, free the request object */
   if (request->type == CHEEVOS_ASYNC_RICHPRESENCE)
      request->attempt_count = 0;
   else
      free(request);
}

static void rcheevos_async_begin_request(
      rcheevos_async_io_request* request, int init_result,
      rcheevos_async_handler handler, char type, int id,
      const char* success_message, const char* failure_message)
{
   if (init_result != RC_OK)
   {
      char errbuf[256];
      if (id)
         snprintf(errbuf, sizeof(errbuf), "%s %u: %s",
               failure_message, id, rc_error_str(init_result));
      else
         snprintf(errbuf, sizeof(errbuf), "%s: %s",
               failure_message, rc_error_str(init_result));

      CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", errbuf);
      runloop_msg_queue_push(errbuf, 0, 5 * 60, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);

      rcheevos_async_end_request(request);
      return;
   }

   request->handler         = handler;
   request->type            = type;
   request->id              = id;
   request->success_message = success_message;
   request->failure_message = failure_message;
   request->attempt_count   = 0;

   if (!request->user_agent)
      request->user_agent = get_rcheevos_locals()->user_agent_core;

   rcheevos_log_post_url(request->request.url, request->request.post_data);
   rcheevos_async_begin_http_request(request);
}

static bool rcheevos_async_succeeded(int result, 
     const rc_api_response_t* response, char buffer[],
     size_t buffer_size)
{
   if (result != RC_OK)
   {
      strlcpy(buffer, rc_error_str(result), buffer_size);
      return false;
   }

   if (!response->succeeded)
   {
      strlcpy(buffer, response->error_message, buffer_size);
      return false;
   }

   return true;
}

void rcheevos_client_initialize(void)
{
   const settings_t* settings = config_get_ptr();
   const char* host = settings->arrays.cheevos_custom_host;
   if (!host[0])
   {
#ifdef HAVE_SSL
      host = "https://retroachievements.org";
#else
      host = "http://retroachievements.org";
#endif
   }

   CHEEVOS_LOG(RCHEEVOS_TAG "Using host: %s\n", host);
   if (!string_is_equal(host, "https://retroachievements.org"))
   {
      rc_api_set_host(host);

      if (!string_is_equal(host, "http://retroachievements.org"))
         rc_api_set_image_host(host);
   }
}

/****************************
 * login                    *
 ****************************/

static void rcheevos_async_login_callback(
      struct rcheevos_async_io_request* request,
      http_transfer_data_t* data, char buffer[], size_t buffer_size)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   rc_api_login_response_t api_response;

   int result = rc_api_process_login_response(&api_response, data->data);
   if (rcheevos_async_succeeded(result, &api_response.response,
            buffer, buffer_size))
   {
      /* save the token to the config and clear the password on success */
      settings_t* settings = config_get_ptr();
      strlcpy(settings->arrays.cheevos_token, api_response.api_token,
         sizeof(settings->arrays.cheevos_token));
      settings->arrays.cheevos_password[0] = '\0';

      CHEEVOS_LOG(RCHEEVOS_TAG "%s logged in successfully\n",
            api_response.display_name);
      strlcpy(rcheevos_locals->displayname, api_response.display_name,
            sizeof(rcheevos_locals->displayname));
      strlcpy(rcheevos_locals->username, api_response.username,
            sizeof(rcheevos_locals->username));
      strlcpy(rcheevos_locals->token, api_response.api_token,
            sizeof(rcheevos_locals->token));
   }
   else
      rcheevos_locals->token[0] = '\0';

   rc_api_destroy_login_response(&api_response);
}

static void rcheevos_client_login(const char* username,
      const char* password, const char* token,
   rcheevos_client_callback callback, void* userdata)
{
   rcheevos_async_io_request* request = (rcheevos_async_io_request*)
      calloc(1, sizeof(rcheevos_async_io_request));
   if (!request)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate login request\n");
   }
   else
   {
      rc_api_login_request_t api_params;
      int result;

      memset(&api_params, 0, sizeof(api_params));
      api_params.username = username;
      api_params.password = password;
      api_params.api_token = token;

      result = rc_api_init_login_request(&request->request, &api_params);

      request->callback = callback;
      request->callback_data = userdata;

      rcheevos_async_begin_request(request, result,
         rcheevos_async_login_callback,
         CHEEVOS_ASYNC_LOGIN, 0,
         NULL,
         "Error logging in");
   }
}

void rcheevos_client_login_with_password(const char* username,
      const char* password,
      rcheevos_client_callback callback, void* userdata)
{
   rcheevos_client_login(username, password, NULL, callback, userdata);
}

void rcheevos_client_login_with_token(const char* username,
      const char* token,
      rcheevos_client_callback callback, void* userdata)
{
   rcheevos_client_login(username, NULL, token, callback, userdata);
}

/****************************
 * identify game            *
 ****************************/

static void rcheevos_async_resolve_hash_callback(
      struct rcheevos_async_io_request* request,
      http_transfer_data_t* data, char buffer[], size_t buffer_size)
{
   rc_api_resolve_hash_response_t api_response;
   int result = rc_api_process_resolve_hash_response(&api_response,
         data->data);

   if (rcheevos_async_succeeded(result, &api_response.response,
            buffer, buffer_size))
   {
      rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
      rcheevos_locals->game.id = api_response.game_id;
   }

   rc_api_destroy_resolve_hash_response(&api_response);
}

void rcheevos_client_identify_game(const char* hash,
      rcheevos_client_callback callback, void* userdata)
{
   rcheevos_async_io_request* request = (rcheevos_async_io_request*)
      calloc(1, sizeof(rcheevos_async_io_request));
   if (!request)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate game identification request\n");
   }
   else
   {
      rc_api_resolve_hash_request_t api_params;
      int result;

      memset(&api_params, 0, sizeof(api_params));
      api_params.game_hash = hash;

      result = rc_api_init_resolve_hash_request(&request->request, &api_params);

      request->callback      = callback;
      request->callback_data = userdata;

      rcheevos_async_begin_request(request, result,
         rcheevos_async_resolve_hash_callback,
         CHEEVOS_ASYNC_RESOLVE_HASH, 0,
         NULL,
         "Error resolving hash");
   }
}

/****************************
 * initialize runtime       *
 ****************************/

typedef struct rcheevos_async_initialize_runtime_data_t
{
   rc_api_fetch_game_data_response_t game_data;
   rc_api_fetch_user_unlocks_response_t hardcore_unlocks;
   rc_api_fetch_user_unlocks_response_t non_hardcore_unlocks;

   rcheevos_client_callback callback;
   void* callback_data;
} rcheevos_async_initialize_runtime_data_t;

static void rcheevos_client_copy_achievements(
      rcheevos_async_initialize_runtime_data_t* runtime_data)
{
   unsigned i, j;
   const rc_api_achievement_definition_t* definition;
   rcheevos_racheevo_t *achievement;
   rcheevos_locals_t   *rcheevos_locals = get_rcheevos_locals();
   const settings_t    *settings        = config_get_ptr();
   
   rcheevos_locals->game.achievements   = (rcheevos_racheevo_t*)
      calloc(runtime_data->game_data.num_achievements,
            sizeof(rcheevos_racheevo_t));

   achievement = rcheevos_locals->game.achievements;
   if (!achievement)
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "Could not allocate achievements\n");
      return;
   }

   definition = runtime_data->game_data.achievements;
   for (i = 0; i < runtime_data->game_data.num_achievements;
         ++i, ++definition)
   {
      /* invalid definition, ignore */
      if (
             definition->category == 0
         || !definition->definition
         || !definition->definition[0]
         || !definition->title
         || !definition->title[0]
         || !definition->description
         || !definition->description[0])
         continue;

      if (definition->category != 3)
      {
         if (!settings->bools.cheevos_test_unofficial)
            continue;

         achievement->active =  RCHEEVOS_ACTIVE_UNOFFICIAL
                              | RCHEEVOS_ACTIVE_SOFTCORE
                              | RCHEEVOS_ACTIVE_HARDCORE;
      }
      else
      {
         achievement->active =  RCHEEVOS_ACTIVE_SOFTCORE
                              | RCHEEVOS_ACTIVE_HARDCORE;

         for (j = 0; j < 
               runtime_data->hardcore_unlocks.num_achievement_ids; ++j)
         {
            if (runtime_data->hardcore_unlocks.achievement_ids[j] 
                  == definition->id)
            {
               achievement->active &= ~(RCHEEVOS_ACTIVE_HARDCORE 
                     | RCHEEVOS_ACTIVE_SOFTCORE);
               break;
            }
         }

         if ((achievement->active & RCHEEVOS_ACTIVE_SOFTCORE) != 0)
         {
            for (j = 0; j < 
                  runtime_data->non_hardcore_unlocks.num_achievement_ids;
                  ++j)
            {
               if (runtime_data->non_hardcore_unlocks.achievement_ids[j] == definition->id)
               {
                  achievement->active &= ~RCHEEVOS_ACTIVE_SOFTCORE;
                  break;
               }
            }
         }
      }

      achievement->id          = definition->id;
      achievement->title       = strdup(definition->title);
      achievement->description = strdup(definition->description);
      achievement->badge       = strdup(definition->badge_name);
      achievement->points      = definition->points;

      /* If an achievement has been fully unlocked, 
       * we don't need to keep the definition around
       * as it won't be reactivated. Otherwise, 
       * we do have to keep a copy of it. */
      if ((achievement->active & (
                    RCHEEVOS_ACTIVE_HARDCORE 
                  | RCHEEVOS_ACTIVE_SOFTCORE)) != 0)
         achievement->memaddr = strdup(definition->definition);

      ++achievement;
   }

   rcheevos_locals->game.achievement_count = (unsigned)(achievement 
      - rcheevos_locals->game.achievements);
}

static void rcheevos_client_copy_leaderboards(
      rcheevos_async_initialize_runtime_data_t* runtime_data)
{
   unsigned i;
   rcheevos_ralboard_t *leaderboard;
   const rc_api_leaderboard_definition_t *definition;
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

   rcheevos_locals->game.leaderboards = (rcheevos_ralboard_t*)
      calloc(runtime_data->game_data.num_leaderboards,
            sizeof(rcheevos_ralboard_t));
   rcheevos_locals->game.leaderboard_count = 
      runtime_data->game_data.num_leaderboards;

   leaderboard = rcheevos_locals->game.leaderboards;
   if (!leaderboard)
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "Could not allocate leaderboards\n");
      return;
   }

   definition = runtime_data->game_data.leaderboards;
   for (i = 0; i < runtime_data->game_data.num_leaderboards; ++i, ++definition, ++leaderboard)
   {
      leaderboard->id          = definition->id;
      leaderboard->title       = strdup(definition->title);
      leaderboard->description = strdup(definition->description);
      leaderboard->mem         = strdup(definition->definition);
      leaderboard->format      = definition->format;
   }
}

static void rcheevos_client_initialize_runtime_rich_presence(
      rcheevos_async_initialize_runtime_data_t* runtime_data)
{
   if (runtime_data->game_data.rich_presence_script && *runtime_data->game_data.rich_presence_script)
   {
      rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

      /* Just activate the rich presence script now. 
       * It can't be toggled on or off,
       * so there's no reason to keep the unparsed version 
       * around any longer than necessary, and we can avoid
       * making a copy in the process. */
      int result = rc_runtime_activate_richpresence(
            &rcheevos_locals->runtime,
            runtime_data->game_data.rich_presence_script, NULL, 0);

      if (result != RC_OK)
      {
         const settings_t* settings = config_get_ptr();
         char buffer[256];
         snprintf(buffer, sizeof(buffer),
               "Could not activate rich presence: %s",
               rc_error_str(result));

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         CHEEVOS_ERR(RCHEEVOS_TAG "%s\n", buffer);
      }
   }
}

static void rcheevos_client_initialize_runtime_callback(void* userdata)
{
   rcheevos_async_initialize_runtime_data_t* runtime_data = (rcheevos_async_initialize_runtime_data_t*)userdata;

   if (rcheevos_end_load_state() > 0)
      return;

   if (!rcheevos_load_aborted())
   {
      rcheevos_client_copy_achievements(runtime_data);
      rcheevos_client_copy_leaderboards(runtime_data);
      rcheevos_client_initialize_runtime_rich_presence(runtime_data);
   }

   rc_api_destroy_fetch_user_unlocks_response(&runtime_data->hardcore_unlocks);
   rc_api_destroy_fetch_user_unlocks_response(&runtime_data->non_hardcore_unlocks);
   rc_api_destroy_fetch_game_data_response(&runtime_data->game_data);

   if (runtime_data->callback)
      runtime_data->callback(runtime_data->callback_data);

   free(runtime_data);
}

static void rcheevos_client_fetch_game_badge_callback(void* userdata)
{
   rcheevos_fetch_badge_data* data = (rcheevos_fetch_badge_data*)userdata;
   rcheevos_async_initialize_runtime_data_t* runtime_data =
      (rcheevos_async_initialize_runtime_data_t*)data->state->callback_data;

   free((void*)data->state->badge_directory);
   free(data->state);
   free(data);

   rcheevos_client_initialize_runtime_callback(runtime_data);
}

static void rcheevos_client_fetch_game_badge(const char* badge_name, rcheevos_async_initialize_runtime_data_t* runtime_data)
{
#if defined(HAVE_GFX_WIDGETS) /* don't need game badge unless widgets are enabled */
   char badge_fullpath[PATH_MAX_LENGTH] = "";
   char *badge_fullname                 = NULL;
   size_t badge_fullname_size           = 0;

   /* make sure the directory exists */
   fill_pathname_application_special(badge_fullpath,
      sizeof(badge_fullpath),
      APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

   if (!path_is_directory(badge_fullpath))
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Creating %s\n", badge_fullpath);
      path_mkdir(badge_fullpath);
   }

   fill_pathname_slash(badge_fullpath, sizeof(badge_fullpath));
   badge_fullname      = badge_fullpath + strlen(badge_fullpath);
   badge_fullname_size = sizeof(badge_fullpath) -
      (badge_fullname - badge_fullpath);

   snprintf(badge_fullname,
      badge_fullname_size, "i%s" FILE_PATH_PNG_EXTENSION, badge_name);

   /* check if it's already available */
   if (path_is_valid(badge_fullpath))
      return;

#ifdef CHEEVOS_LOG_BADGES
   CHEEVOS_LOG(RCHEEVOS_TAG "Downloading game badge %s\n", badge_name);
#endif

   {
      rcheevos_async_io_request* request = (rcheevos_async_io_request*)
         calloc(1, sizeof(rcheevos_async_io_request));
      rcheevos_fetch_badge_data* data = (rcheevos_fetch_badge_data*)
         calloc(1, sizeof(rcheevos_fetch_badge_data));
      rcheevos_fetch_badge_state* state = (rcheevos_fetch_badge_state*)
         calloc(1, sizeof(rcheevos_fetch_badge_state));

      if (!request || !data || !state)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG
            "Failed to allocate fetch badge request\n");
      }
      else
      {
         rc_api_fetch_image_request_t api_params;
         int result;

         memset(&api_params, 0, sizeof(api_params));
         api_params.image_name = badge_name;
         api_params.image_type = RC_IMAGE_TYPE_GAME;

         result = rc_api_init_fetch_image_request(&request->request, &api_params);

         strlcpy(state->requested_badges[0], badge_fullname, sizeof(state->requested_badges[0]));
         *badge_fullname = '\0';
         state->badge_directory = strdup(badge_fullpath);
         state->callback_data = runtime_data;

         data->state = state;
         data->request_index = 0;
         data->callback = rcheevos_client_fetch_game_badge_callback;

         request->callback_data = data;

         rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_FETCHING_GAME_DATA);
         rcheevos_async_begin_request(request, result,
            rcheevos_async_fetch_badge_callback,
            CHEEVOS_ASYNC_FETCH_BADGE, atoi(badge_name), NULL,
            "Error fetching game badge");
      }
   }
#endif
}

static void rcheevos_async_fetch_user_unlocks_callback(
      struct rcheevos_async_io_request* request,
      http_transfer_data_t* data, char buffer[], size_t buffer_size)
{
   rcheevos_async_initialize_runtime_data_t* runtime_data = 
      (rcheevos_async_initialize_runtime_data_t*)request->callback_data;
   int result;

   if (request->type == CHEEVOS_ASYNC_FETCH_HARDCORE_USER_UNLOCKS)
   {
      result = rc_api_process_fetch_user_unlocks_response(
            &runtime_data->hardcore_unlocks, data->data);
      rcheevos_async_succeeded(result,
            &runtime_data->hardcore_unlocks.response,
            buffer, buffer_size);
   }
   else
   {
      result = rc_api_process_fetch_user_unlocks_response(
            &runtime_data->non_hardcore_unlocks, data->data);
      rcheevos_async_succeeded(result,
            &runtime_data->non_hardcore_unlocks.response,
            buffer, buffer_size);
   }
}

static void rcheevos_async_fetch_game_data_callback(
      struct rcheevos_async_io_request* request,
   http_transfer_data_t* data, char buffer[], size_t buffer_size)
{
   rcheevos_async_initialize_runtime_data_t* runtime_data = 
      (rcheevos_async_initialize_runtime_data_t*)request->callback_data;

#ifdef CHEEVOS_SAVE_JSON
   filestream_write_file(CHEEVOS_SAVE_JSON, data->data, data->len);
#endif

   int result = rc_api_process_fetch_game_data_response(
         &runtime_data->game_data, data->data);
   if (rcheevos_async_succeeded(result, &runtime_data->game_data.response, buffer, buffer_size))
   {
      rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
      rcheevos_locals->game.title        = strdup(
            runtime_data->game_data.title);
      rcheevos_locals->game.console_id   = 
         runtime_data->game_data.console_id;

      snprintf(rcheevos_locals->game.badge_name, sizeof(rcheevos_locals->game.badge_name),
         "i%s", runtime_data->game_data.image_name);
      rcheevos_client_fetch_game_badge(runtime_data->game_data.image_name, runtime_data);
   }
   else
   {
      rcheevos_unload();
   }
}

void rcheevos_client_initialize_runtime(unsigned game_id,
      rcheevos_client_callback callback, void* userdata)
{
   rcheevos_async_io_request* request;
   const settings_t               *settings       = config_get_ptr();
   const rcheevos_locals_t *rcheevos_locals       = get_rcheevos_locals();
   rcheevos_async_initialize_runtime_data_t *data = 
      (rcheevos_async_initialize_runtime_data_t*)
      malloc(sizeof(rcheevos_async_initialize_runtime_data_t));

   if (!data)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate runtime initalization data\n");
      return;
   }

   data->callback      = callback;
   data->callback_data = userdata;
   request             = (rcheevos_async_io_request*)
      calloc(1, sizeof(rcheevos_async_io_request));

   if (!request)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate game data fetch request\n");
   }
   else
   {
#ifdef CHEEVOS_JSON_OVERRIDE
      char buffer[128];
      size_t size = 0;
      char* contents;
      http_transfer_data_t transfer_data;
      FILE* file = fopen(CHEEVOS_JSON_OVERRIDE, "rb");

      fseek(file, 0, SEEK_END);
      size = ftell(file);
      fseek(file, 0, SEEK_SET);

      contents = (char*)malloc(size + 1);
      fread((void*)contents, 1, size, file);
      fclose(file);

      contents[size] = 0;

      transfer_data.data = contents;
      transfer_data.len = size;
      transfer_data.status = 200;

      request->callback_data = data;
      rcheevos_async_fetch_game_data_callback(request, &transfer_data, buffer, sizeof(buffer));

      free(contents);
#else
      rc_api_fetch_game_data_request_t api_params;
      int result;

      memset(&api_params, 0, sizeof(api_params));
      api_params.username = rcheevos_locals->username;
      api_params.api_token = rcheevos_locals->token;
      api_params.game_id = rcheevos_locals->game.id;

      result = rc_api_init_fetch_game_data_request(&request->request, &api_params);

      request->callback = rcheevos_client_initialize_runtime_callback;
      request->callback_data = data;

      rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_FETCHING_GAME_DATA);
      rcheevos_async_begin_request(request, result,
         rcheevos_async_fetch_game_data_callback,
         CHEEVOS_ASYNC_FETCH_GAME_DATA, rcheevos_locals->game.id,
         "Fetched game data",
         "Error fetching game data");
#endif
   }

   if (settings->bools.cheevos_start_active)
   {
      memset(&data->hardcore_unlocks, 0, sizeof(data->hardcore_unlocks));
      memset(&data->non_hardcore_unlocks, 0, sizeof(data->non_hardcore_unlocks));

      data->hardcore_unlocks.num_achievement_ids = 0;
      data->non_hardcore_unlocks.num_achievement_ids = 0;
   }
   else
   {
      int i;
      for (i = 0; i < 2; ++i)
      {
         request = (rcheevos_async_io_request*)calloc(1, sizeof(rcheevos_async_io_request));
         if (!request)
         {
            CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate user unlock request\n");
         }
         else
         {
            rc_api_fetch_user_unlocks_request_t api_params;
            int result;

            memset(&api_params, 0, sizeof(api_params));
            api_params.username = rcheevos_locals->username;
            api_params.api_token = rcheevos_locals->token;
            api_params.game_id = rcheevos_locals->game.id;
            api_params.hardcore = i;

            result = rc_api_init_fetch_user_unlocks_request(&request->request, &api_params);

            request->callback = rcheevos_client_initialize_runtime_callback;
            request->callback_data = data;

            rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_FETCHING_GAME_DATA);
            if (i == 0)
            {
               rcheevos_async_begin_request(request, result,
                  rcheevos_async_fetch_user_unlocks_callback,
                  CHEEVOS_ASYNC_FETCH_USER_UNLOCKS,
                  rcheevos_locals->game.id,
                  "Fetched user unlocks",
                  "Error fetching user unlocks");
            }
            else
            {
               rcheevos_async_begin_request(request, result,
                  rcheevos_async_fetch_user_unlocks_callback,
                  CHEEVOS_ASYNC_FETCH_HARDCORE_USER_UNLOCKS,
                  rcheevos_locals->game.id,
                  "Fetched hardcore user unlocks",
                  "Error fetching hardcore user unlocks");
            }
         }
      }
   }
}

/****************************
 * ping                     *
 ****************************/

static retro_time_t rcheevos_client_prepare_ping(
      rcheevos_async_io_request* request)
{
   rc_api_ping_request_t api_params;
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   const settings_t *settings               = config_get_ptr();
   const bool cheevos_richpresence_enable   =
         rcheevos_hardcore_active() ||
         settings->bools.cheevos_richpresence_enable;
   char buffer[256]                         = "";

   memset(&api_params, 0, sizeof(api_params));
   api_params.username  = rcheevos_locals->username;
   api_params.api_token = rcheevos_locals->token;
   api_params.game_id   = request->id;

   if (cheevos_richpresence_enable)
   {
      rcheevos_get_richpresence(buffer, sizeof(buffer));
      api_params.rich_presence = buffer;
   }

   rc_api_init_ping_request(&request->request, &api_params);

   rcheevos_log_post_url(request->request.url,
         request->request.post_data);

#ifdef HAVE_PRESENCE
   presence_update(PRESENCE_RETROACHIEVEMENTS);
#endif

   /* Update rich presence every two minutes */
   if (cheevos_richpresence_enable)
      return cpu_features_get_time_usec() + CHEEVOS_PING_FREQUENCY;

   /* Send ping every four minutes */
   return cpu_features_get_time_usec() + CHEEVOS_PING_FREQUENCY * 2;
}

static void rcheevos_async_ping_handler(retro_task_t* task)
{
   rcheevos_async_io_request* request = (rcheevos_async_io_request*)
      task->user_data;

   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (request->id != (int)rcheevos_locals->game.id)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG
         "Stopping periodic rich presence update task for game %u\n", request->id);
      /* game changed; stop the recurring task - a new one will
       * be scheduled if a new game is loaded */
      task_set_finished(task, 1);
      /* request->request was destroyed
       * in rcheevos_async_http_task_callback */
      free(request);
      return;
   }

   /* update the request and set the task to fire again in
    * two minutes */
   task->when = rcheevos_client_prepare_ping(request);

   /* start the HTTP request */
   rcheevos_async_begin_http_request(request);
}

/****************************
 * start session            *
 ****************************/

static void rcheevos_async_start_session_callback(
      struct rcheevos_async_io_request* request,
      http_transfer_data_t* data, char buffer[], size_t buffer_size)
{
   rc_api_start_session_response_t api_response;

   int result = rc_api_process_start_session_response(
         &api_response, data->data);
   rcheevos_async_succeeded(result,
         &api_response.response, buffer, buffer_size);
   rc_api_destroy_start_session_response(&api_response);
}

void rcheevos_client_start_session(unsigned game_id)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

   /* the core won't change while a session is active, so only
    * calculate the user agent once */
   rcheevos_get_user_agent(rcheevos_locals,
         rcheevos_locals->user_agent_core,
         sizeof(rcheevos_locals->user_agent_core));

   /* schedule the first rich presence call in 30 seconds */
   {
      rcheevos_async_io_request *request = (rcheevos_async_io_request*)
            calloc(1, sizeof(rcheevos_async_io_request));
      if (!request)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG
               "Failed to allocate rich presence request\n");
      }
      else
      {
         retro_task_t* task       = task_init();

         request->id              = game_id;
         request->type            = CHEEVOS_ASYNC_RICHPRESENCE;
         request->user_agent      = rcheevos_locals->user_agent_core;
         request->failure_message = "Error sending ping";

         task->handler            = rcheevos_async_ping_handler;
         task->user_data          = request;
         task->progress           = -1;
         task->when               = cpu_features_get_time_usec() +
                                    CHEEVOS_PING_FREQUENCY / 4;

         CHEEVOS_LOG(RCHEEVOS_TAG
            "Starting periodic rich presence update task for game %u\n", game_id);
         task_queue_push(task);
      }
   }

   /* send the new session request */
   {
      rcheevos_async_io_request* request = (rcheevos_async_io_request*)
         calloc(1, sizeof(rcheevos_async_io_request));
      if (!request)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG
               "Failed to allocate new session request\n");
      }
      else
      {
         rc_api_start_session_request_t api_params;
         int result;

         memset(&api_params, 0, sizeof(api_params));
         api_params.username = rcheevos_locals->username;
         api_params.api_token = rcheevos_locals->token;
         api_params.game_id = game_id;

         result = rc_api_init_start_session_request(
               &request->request, &api_params);

         rcheevos_async_begin_request(request, result,
            rcheevos_async_start_session_callback,
            CHEEVOS_ASYNC_START_SESSION, game_id,
            "Started session for game",
            "Error starting session for game");
      }
   }
}


/****************************
 * fetch badge              *
 ****************************/

static bool rcheevos_fetch_next_badge(rcheevos_fetch_badge_state* state);

static void rcheevos_end_fetch_badges(rcheevos_fetch_badge_state* state)
{
   if (state->callback)
      state->callback(state->callback_data);

   free((void*)state->badge_directory);
   free(state);
}

static void rcheevos_async_download_next_badge(void* userdata)
{
   rcheevos_fetch_badge_data* badge_data = 
      (rcheevos_fetch_badge_data*)userdata;
   rcheevos_fetch_next_badge(badge_data->state);

   if (rcheevos_end_load_state() == 0)
      rcheevos_end_fetch_badges(badge_data->state);

   free(badge_data);
}

static void rcheevos_async_fetch_badge_complete(rcheevos_fetch_badge_data* badge_data)
{
#ifdef HAVE_THREADS
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

   slock_lock(rcheevos_locals->load_info.request_lock);
#endif
   badge_data->state->requested_badges[badge_data->request_index][0] = '\0';
#ifdef HAVE_THREADS
   slock_unlock(rcheevos_locals->load_info.request_lock);
#endif

   if (badge_data->callback)
      badge_data->callback(badge_data);
}

static void rcheevos_async_write_badge(retro_task_t* task)
{
   char badge_fullpath[PATH_MAX_LENGTH];
   rcheevos_fetch_badge_data* badge_data =
      (rcheevos_fetch_badge_data*)task->user_data;

   fill_pathname_join_special(badge_fullpath,
         badge_data->state->badge_directory,
         badge_data->state->requested_badges[badge_data->request_index],
         sizeof(badge_fullpath));

   if (!filestream_write_file(badge_fullpath, badge_data->data, badge_data->data_len))
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "Error writing badge %s\n",
         badge_fullpath);
   }

   free(badge_data->data);
   badge_data->data = NULL;
   badge_data->data_len = 0;

   task_set_finished(task, true);

   rcheevos_async_fetch_badge_complete(badge_data);
}

static void rcheevos_async_fetch_badge_callback(
      struct rcheevos_async_io_request* request,
      http_transfer_data_t* data, char buffer[], size_t buffer_size)
{
   rcheevos_fetch_badge_data* badge_data    =
      (rcheevos_fetch_badge_data*)request->callback_data;
   retro_task_t* task;

   if (!data->data)
   {
      /* error retrieving badge, nothing to write. just call the callback */
      rcheevos_async_fetch_badge_complete(badge_data);
      return;
   }

   /* take ownership of the file data */
   badge_data->data = data->data;
   badge_data->data_len = data->len;
   data->data = NULL;
   data->len = 0;

   /* this is called on the primary thread. use a task to write the file from a background thread */
   task = task_init();
   task->handler = rcheevos_async_write_badge;
   task->user_data = badge_data;
   task_queue_push(task);
}

static bool rcheevos_client_fetch_badge(
      const char* badge_name, int locked,
      rcheevos_fetch_badge_state* state)
{
   char badge_fullpath[PATH_MAX_LENGTH];
   char* badge_fullname       = NULL;
   size_t badge_fullname_size = 0;
   int request_index          = -1;

   if (!badge_name || !badge_name[0])
      return false;

   strlcpy(badge_fullpath,
         state->badge_directory, sizeof(badge_fullpath));
   fill_pathname_slash(badge_fullpath, sizeof(badge_fullpath));
   badge_fullname      = badge_fullpath + strlen(badge_fullpath);
   badge_fullname_size = sizeof(badge_fullpath) - 
      (badge_fullname - badge_fullpath);

   snprintf(badge_fullname,
         badge_fullname_size, "%s%s" FILE_PATH_PNG_EXTENSION,
      badge_name, locked ? "_lock" : "");

   /* check if it's already available */
   if (path_is_valid(badge_fullpath))
      return false;

   /* check if it's already requested */
   {
      int i;
      int found_index = -1;
#ifdef HAVE_THREADS
      const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
      slock_lock(rcheevos_locals->load_info.request_lock);
#endif
      for (i = RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS - 1; i >= 0; --i)
      {
         if (!state->requested_badges[i][0])
            request_index = i;
         else if (string_is_equal(badge_fullname,
                  state->requested_badges[i]))
         {
            found_index = i;
            break;
         }
      }

      if (found_index == -1)
      {
         /* unexpected - but if it happens, 
          * the queue is full. Pretend we found 
          * a match to prevent an exception */
         if (request_index == -1)
            found_index = 0;
         else
            strlcpy(state->requested_badges[request_index],
                  badge_fullname,
                  sizeof(state->requested_badges[request_index]));
      }
#ifdef HAVE_THREADS
      slock_unlock(rcheevos_locals->load_info.request_lock);
#endif
      if (found_index != -1)
         return false;
   }

   /* request the new badge */
#ifdef CHEEVOS_LOG_BADGES
   CHEEVOS_LOG(RCHEEVOS_TAG "Downloading badge %s\n", badge_name);
#endif

   {
      rcheevos_async_io_request* request = (rcheevos_async_io_request*)
         calloc(1, sizeof(rcheevos_async_io_request));
      rcheevos_fetch_badge_data* data = (rcheevos_fetch_badge_data*)
         calloc(1, sizeof(rcheevos_fetch_badge_data));

      if (!request || !data)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG
               "Failed to allocate fetch badge request\n");
      }
      else
      {
         rc_api_fetch_image_request_t api_params;
         int result;

         memset(&api_params, 0, sizeof(api_params));
         api_params.image_name = badge_name;
         api_params.image_type = locked 
            ? RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED 
            : RC_IMAGE_TYPE_ACHIEVEMENT;

         result = rc_api_init_fetch_image_request(&request->request, &api_params);

         data->state            = state;
         data->request_index    = request_index;
         data->callback         = rcheevos_async_download_next_badge;

         request->callback_data = data;

         rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_FETCHING_BADGES);
         rcheevos_async_begin_request(request, result,
            rcheevos_async_fetch_badge_callback,
            CHEEVOS_ASYNC_FETCH_BADGE, atoi(badge_name), NULL,
            "Error fetching badge");
      }
   }

   return true;
}

static bool rcheevos_fetch_next_badge(rcheevos_fetch_badge_state* state)
{
   if (rcheevos_load_aborted())
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Load aborted while fetching badges\n");
   }
   else
   {
      int active                               = 0;
      const rcheevos_racheevo_t *cheevo        = NULL;
      const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

      /* fetch badges for current state of achievements first */
      do
      {
#ifdef HAVE_THREADS
         slock_lock(rcheevos_locals->load_info.request_lock);
#endif
         if (    state->locked_badge_fetch_index 
               < rcheevos_locals->game.achievement_count)
            cheevo = &rcheevos_locals->game.achievements[state->locked_badge_fetch_index++];
         else
            cheevo = NULL;
#ifdef HAVE_THREADS
         slock_unlock(rcheevos_locals->load_info.request_lock);
#endif

         if (!cheevo)
            break;

         active = (cheevo->active 
               & (RCHEEVOS_ACTIVE_HARDCORE | RCHEEVOS_ACTIVE_SOFTCORE));
         if (rcheevos_client_fetch_badge(cheevo->badge, active, state))
            return true;

      } while (true);

      /* then fetch badges for unlocked state so they're ready when the user unlocks something */
      do
      {
#ifdef HAVE_THREADS
         slock_lock(rcheevos_locals->load_info.request_lock);
#endif
         if (state->badge_fetch_index < rcheevos_locals->game.achievement_count)
            cheevo = &rcheevos_locals->game.achievements[state->badge_fetch_index++];
         else
            cheevo = NULL;

#ifdef HAVE_THREADS
         slock_unlock(rcheevos_locals->load_info.request_lock);
#endif

         if (!cheevo)
            break;

         if (rcheevos_client_fetch_badge(cheevo->badge, 0, state))
            return true;

      } while (true);
   }

   return false;
}

void rcheevos_client_fetch_badges(rcheevos_client_callback callback, void* userdata)
{
#if defined(HAVE_MENU) || defined(HAVE_GFX_WIDGETS) /* don't need badges unless menu or widgets are enabled */
   rcheevos_fetch_badge_state* state    = NULL;
   char badge_fullpath[PATH_MAX_LENGTH] = "";
#if !defined(HAVE_GFX_WIDGETS) /* we always want badges if widgets are enabled */
   settings_t* settings                 = config_get_ptr();
   /* User has explicitly disabled badges */
   if (!settings->bools.cheevos_badges_enable)
      return;

   /* badges are only needed for xmb and ozone menus */
   if (!string_is_equal(settings->arrays.menu_driver, "xmb") &&
         !string_is_equal(settings->arrays.menu_driver, "ozone"))
      return;
#endif /* !defined(HAVE_GFX_WIDGETS) */

   /* make sure the directory exists */
   fill_pathname_application_special(badge_fullpath,
         sizeof(badge_fullpath),
         APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

   if (!path_is_directory(badge_fullpath))
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Creating %s\n", badge_fullpath);
      path_mkdir(badge_fullpath);
   }

   /* start the download task */
   state = (rcheevos_fetch_badge_state*)
      calloc(1, sizeof(rcheevos_fetch_badge_state));

   if (!state)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate fetch badge state\n");
   }
   else
   {
      int num_concurrent = RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS;

      state->badge_directory = strdup(badge_fullpath);
      state->locked_badge_fetch_index = 0;
      state->badge_fetch_index = 0;
      state->callback = callback;
      state->callback_data = userdata;

      rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_FETCHING_BADGES);

      /* fetch the placeholder image */
      if (rcheevos_client_fetch_badge("00000", 0, state))
         num_concurrent--;

      /* queue up additional requests so up to {num_concurrent} downloads are queued */
      while (num_concurrent--)
      {
         if (!rcheevos_fetch_next_badge(state))
            break;
      }

      if (rcheevos_end_load_state() == 0)
         rcheevos_end_fetch_badges(state);
   }
#endif /* defined(HAVE_MENU) || defined(HAVE_GFX_WIDGETS) */
}

#undef RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS


/****************************
 * award achievement        *
 ****************************/

static void rcheevos_async_award_achievement_callback(
      struct rcheevos_async_io_request* request,
      http_transfer_data_t *data, char buffer[], size_t buffer_size)
{
   rc_api_award_achievement_response_t api_response;

   int result = rc_api_process_award_achievement_response(
         &api_response, data->data);
   if (rcheevos_async_succeeded(result, &api_response.response,
            buffer, buffer_size))
   {
      if ((int)api_response.awarded_achievement_id != request->id)
         snprintf(buffer, buffer_size, "Achievement %u awarded instead",
               (unsigned)api_response.awarded_achievement_id);
      else if (api_response.response.error_message)
      {
         /* previously unlocked achievements are returned as a "successful" error */
         CHEEVOS_LOG(RCHEEVOS_TAG "Achievement %u: %s\n",
               request->id, api_response.response.error_message);
      }

      if (api_response.achievements_remaining == 0)
         rcheevos_show_mastery_placard();
   }

   rc_api_destroy_award_achievement_response(&api_response);
}

void rcheevos_client_award_achievement(unsigned achievement_id)
{
   rcheevos_async_io_request *request = (rcheevos_async_io_request*)
         calloc(1, sizeof(rcheevos_async_io_request));
   if (!request)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate unlock request for achievement %u\n",
            achievement_id);
   }
   else 
   {
      const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
      rc_api_award_achievement_request_t api_params;
      int result;

      memset(&api_params, 0, sizeof(api_params));
      api_params.username       = rcheevos_locals->username;
      api_params.api_token      = rcheevos_locals->token;
      api_params.achievement_id = achievement_id;
      api_params.hardcore       = rcheevos_locals->hardcore_active ? 1 : 0;
      api_params.game_hash      = rcheevos_locals->game.hash;

      result = rc_api_init_award_achievement_request(&request->request,
            &api_params);

      rcheevos_async_begin_request(request, result,
            rcheevos_async_award_achievement_callback, 
            CHEEVOS_ASYNC_AWARD_ACHIEVEMENT, achievement_id,
            "Awarded achievement",
            "Error awarding achievement");
   }
}


/****************************
 * submit leaderboard       *
 ****************************/

static void rcheevos_async_submit_lboard_entry_callback(
      struct rcheevos_async_io_request* request,
      http_transfer_data_t* data, char buffer[], size_t buffer_size)
{
   rc_api_submit_lboard_entry_response_t api_response;
   int result = rc_api_process_submit_lboard_entry_response(
         &api_response, data->data);

   /* not currently doing anything with the response */
   if (rcheevos_async_succeeded(result, &api_response.response, buffer,
            buffer_size)) { }

   rc_api_destroy_submit_lboard_entry_response(&api_response);
}

void rcheevos_client_submit_lboard_entry(unsigned leaderboard_id,
      int value)
{
   rcheevos_async_io_request *request = (rcheevos_async_io_request*)
      calloc(1, sizeof(rcheevos_async_io_request));
   if (!request)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG
            "Failed to allocate request for lboard %u submit\n",
            leaderboard_id);
   }
   else 
   {
      const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
      rc_api_submit_lboard_entry_request_t api_params;
      int result;

      memset(&api_params, 0, sizeof(api_params));
      api_params.username       = rcheevos_locals->username;
      api_params.api_token      = rcheevos_locals->token;
      api_params.leaderboard_id = leaderboard_id;
      api_params.score          = value;
      api_params.game_hash      = rcheevos_locals->game.hash;

      result = rc_api_init_submit_lboard_entry_request(&request->request,
            &api_params);

      rcheevos_async_begin_request(request, result,
            rcheevos_async_submit_lboard_entry_callback, 
            CHEEVOS_ASYNC_SUBMIT_LBOARD, leaderboard_id,
            "Submitted leaderboard",
            "Error submitting leaderboard");
   }
}

#endif /* HAVE_RC_CLIENT */

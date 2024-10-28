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

#ifdef HAVE_THREADS
#define RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS 2
#else
#define RCHEEVOS_CONCURRENT_BADGE_DOWNLOADS 1
#endif

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
      CHEEVOS_LOG(RCHEEVOS_TAG "http_task returned null");
      callback_data->callback(&server_response, callback_data->callback_data);
   }
   else if (http_data->status < 0)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "http_task returned %d", http_data->status);
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



/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2021-2021 - Brian Weiss
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
#include "../paths.h"
#include "../version.h"

#include <features/features_cpu.h>
#include "../frontend/frontend_driver.h"
#include "../network/net_http_special.h"
#include "../tasks/tasks_internal.h"

#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.h"
#endif

#ifdef HAVE_DISCORD
#include "../network/discord.h"
#endif

#include "../deps/rcheevos/include/rc_url.h"



/* Number of usecs to wait between posting rich presence to the site. */
/* Keep consistent with SERVER_PING_FREQUENCY from RAIntegration. */
#define CHEEVOS_PING_FREQUENCY 2 * 60 * 1000000

enum rcheevos_async_io_type
{
   CHEEVOS_ASYNC_RICHPRESENCE = 0,
   CHEEVOS_ASYNC_AWARD_ACHIEVEMENT,
   CHEEVOS_ASYNC_SUBMIT_LBOARD
};

typedef struct rcheevos_async_io_request
{
   const char* success_message;
   const char* failure_message;
   int id;
   int value;
   int attempt_count;
   char user_agent[256];
   char type;
   char hardcore;
} rcheevos_async_io_request;

/* forward declarations */
static void rcheevos_async_schedule(
      rcheevos_async_io_request* request, retro_time_t delay);
static void rcheevos_async_task_callback(
      retro_task_t* task, void* task_data, void* user_data, const char* error);

/* user agent construction */

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
      {
         *ptr++ = *text++;
      }
   }

   *ptr = '\0';
   return (ptr - buffer);
}

void rcheevos_get_user_agent(
      rcheevos_locals_t *locals,
      char *buffer, size_t len)
{
   struct retro_system_info *system = runloop_get_libretro_system_info();
   char* ptr;

   /* if we haven't calculated the non-changing portion yet, do so now [retroarch version + os version] */
   if (!locals->user_agent_prefix[0])
   {
      const frontend_ctx_driver_t *frontend = frontend_get_ptr();
      int major, minor;
      char tmp[64];

      if (frontend && frontend->get_os)
      {
         frontend->get_os(tmp, sizeof(tmp), &major, &minor);
         snprintf(locals->user_agent_prefix, sizeof(locals->user_agent_prefix),
            "RetroArch/%s (%s %d.%d)", PACKAGE_VERSION, tmp, major, minor);
      }
      else
      {
         snprintf(locals->user_agent_prefix, sizeof(locals->user_agent_prefix),
            "RetroArch/%s", PACKAGE_VERSION);
      }
   }

   /* append the non-changing portion */
   ptr = buffer + strlcpy(buffer, locals->user_agent_prefix, len);

   /* if a core is loaded, append its information */
   if (system && !string_is_empty(system->library_name))
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
      {
         ptr += append_no_spaces(ptr, stop, system->library_name);
      }

      if (system->library_version)
      {
         *ptr++ = '/';
         ptr += append_no_spaces(ptr, stop, system->library_version);
      }
   }

   *ptr = '\0';
}

#ifdef CHEEVOS_LOG_URLS
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

void rcheevos_log_url(const char* api, const char* url)
{
#ifdef CHEEVOS_LOG_URLS
#ifdef CHEEVOS_LOG_PASSWORD
   CHEEVOS_LOG(RCHEEVOS_TAG "%s: %s\n", api, url);
#else
   char copy[256];
   strlcpy(copy, url, sizeof(copy));
   rcheevos_filter_url_param(copy, "p");
   rcheevos_filter_url_param(copy, "t");
   CHEEVOS_LOG(RCHEEVOS_TAG "%s: %s\n", api, copy);
#endif
#else
   (void)api;
   (void)url;
#endif
}

static void rcheevos_log_post_url(
      const char* api,
      const char* url,
      const char* post)
{
#ifdef CHEEVOS_LOG_URLS
 #ifdef CHEEVOS_LOG_PASSWORD
   if (post && post[0])
      CHEEVOS_LOG(RCHEEVOS_TAG "%s: %s&%s\n", api, url, post);
   else
      CHEEVOS_LOG(RCHEEVOS_TAG "%s: %s\n", api, url);
 #else
   if (post && post[0])
   {
      char post_copy[2048];
      strlcpy(post_copy, post, sizeof(post_copy));
      rcheevos_filter_url_param(post_copy, "p");
      rcheevos_filter_url_param(post_copy, "t");

      if (post_copy[0])
         CHEEVOS_LOG(RCHEEVOS_TAG "%s: %s&%s\n", api, url, post_copy);
      else
         CHEEVOS_LOG(RCHEEVOS_TAG "%s: %s\n", api, url);
   }
   else
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "%s: %s\n", api, url);
   }
 #endif
#else
   (void)api;
   (void)url;
   (void)post;
#endif
}

/* start session */

void rcheevos_start_session(unsigned game_id)
{
   /* schedule the first rich presence call in 30 seconds */
   {
      rcheevos_async_io_request* request = (rcheevos_async_io_request*)
         calloc(1, sizeof(rcheevos_async_io_request));
      request->id                        = game_id;
      request->type                      = CHEEVOS_ASYNC_RICHPRESENCE;
      rcheevos_async_schedule(request, CHEEVOS_PING_FREQUENCY / 4);
   }
}

/* ping */

static retro_time_t rcheevos_async_send_rich_presence(
      rcheevos_locals_t *locals,
      rcheevos_async_io_request* request)
{
   char url[256], post_data[1024];
   char buffer[256] = "";
   const settings_t *settings             = config_get_ptr();
   const char *cheevos_username           = settings->arrays.cheevos_username;
   const bool cheevos_richpresence_enable = settings->bools.cheevos_richpresence_enable;
   int ret;

   if (cheevos_richpresence_enable)
      rcheevos_get_richpresence(buffer, sizeof(buffer));

   ret = rc_url_ping(url, sizeof(url), post_data, sizeof(post_data),
      cheevos_username, locals->token, locals->patchdata.game_id, buffer);

   if (ret < 0)
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
   }
   else
   {
      rcheevos_log_post_url("rc_url_ping", url, post_data);

      rcheevos_get_user_agent(locals,
            request->user_agent, sizeof(request->user_agent));
      task_push_http_post_transfer_with_user_agent(url, post_data, true, "POST", request->user_agent, NULL, NULL);
   }

#ifdef HAVE_DISCORD
   if (settings->bools.discord_enable && discord_is_ready())
      discord_update(DISCORD_PRESENCE_RETROACHIEVEMENTS);
#endif

   /* Update rich presence every two minutes */
   if (cheevos_richpresence_enable)
      return cpu_features_get_time_usec() + CHEEVOS_PING_FREQUENCY;

   /* Send ping every four minutes */
   return cpu_features_get_time_usec() + CHEEVOS_PING_FREQUENCY * 2;
}

/* award achievement */

static void rcheevos_async_award_achievement(
      rcheevos_locals_t *locals,
      rcheevos_async_io_request* request)
{
   char buffer[256];
   settings_t *settings = config_get_ptr();
   int              ret = rc_url_award_cheevo(buffer, sizeof(buffer),
                          settings->arrays.cheevos_username,
                          locals->token,
                          request->id,
                          request->hardcore,
                          locals->hash);

   if (ret != 0)
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "Buffer too small to create URL\n");
      free(request);
      return;
   }

   rcheevos_log_url("rc_url_award_cheevo", buffer);
   task_push_http_transfer_with_user_agent(buffer, true, NULL,
         request->user_agent, rcheevos_async_task_callback, request);

#ifdef HAVE_AUDIOMIXER
   if (settings->bools.cheevos_unlock_sound_enable)
      audio_driver_mixer_play_menu_sound(
            AUDIO_MIXER_SYSTEM_SLOT_ACHIEVEMENT_UNLOCK);
#endif
}

void rcheevos_award_achievement(rcheevos_racheevo_t* cheevo)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

   if (!cheevo)
      return;

   CHEEVOS_LOG(RCHEEVOS_TAG "Awarding achievement %u: %s (%s)\n",
         cheevo->id, cheevo->title, cheevo->description);

   /* Deactivates the achivement. */
   rc_runtime_deactivate_achievement(&rcheevos_locals->runtime, cheevo->id);

   cheevo->active &= ~RCHEEVOS_ACTIVE_SOFTCORE;
   if (rcheevos_locals->hardcore_active)
      cheevo->active &= ~RCHEEVOS_ACTIVE_HARDCORE;

   cheevo->unlock_time = cpu_features_get_time_usec();

   /* Show the on screen message. */
#if defined(HAVE_GFX_WIDGETS)
   if (gfx_widgets_ready())
   {
      gfx_widgets_push_achievement(cheevo->title, cheevo->badge);
   }
   else
#endif
   {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "%s: %s", 
            msg_hash_to_str(MSG_ACHIEVEMENT_UNLOCKED), cheevo->title);
      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      runloop_msg_queue_push(cheevo->description, 0, 3 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* Start the award task (unofficial achievement unlocks are not submitted). */
   if (!(cheevo->active & RCHEEVOS_ACTIVE_UNOFFICIAL))
   {
      rcheevos_async_io_request *request = (rcheevos_async_io_request*)calloc(1, sizeof(rcheevos_async_io_request));
      request->type            = CHEEVOS_ASYNC_AWARD_ACHIEVEMENT;
      request->id              = cheevo->id;
      request->hardcore        = rcheevos_locals->hardcore_active ? 1 : 0;
      request->success_message = "Awarded achievement";
      request->failure_message = "Error awarding achievement";
      rcheevos_get_user_agent(rcheevos_locals,
            request->user_agent, sizeof(request->user_agent));
      rcheevos_async_award_achievement(rcheevos_locals, request);
   }

#ifdef HAVE_SCREENSHOTS
   {
      settings_t *settings = config_get_ptr();
      /* Take a screenshot of the achievement. */
      if (settings && settings->bools.cheevos_auto_screenshot)
      {
         size_t shotname_len  = sizeof(char) * 8192;
         char *shotname       = (char*)malloc(shotname_len);

         if (!shotname)
            return;

         snprintf(shotname, shotname_len, "%s/%s-cheevo-%u",
               settings->paths.directory_screenshot,
               path_basename(path_get(RARCH_PATH_BASENAME)),
               cheevo->id);
         shotname[shotname_len - 1] = '\0';

         if (take_screenshot(settings->paths.directory_screenshot,
                  shotname, true,
                  video_driver_cached_frame_has_valid_framebuffer(),
                  false, true))
            CHEEVOS_LOG(
                  RCHEEVOS_TAG "Captured screenshot for achievement %u\n",
                  cheevo->id);
         else
            CHEEVOS_LOG(
                  RCHEEVOS_TAG "Failed to capture screenshot for achievement %u\n",
                  cheevo->id);
         free(shotname);
      }
   }
#endif
}

/* submit leaderboard */

static void rcheevos_async_submit_lboard(rcheevos_locals_t *locals,
      rcheevos_async_io_request* request)
{
   char buffer[256];
   settings_t *settings = config_get_ptr();
   int ret              = rc_url_submit_lboard(buffer, sizeof(buffer),
         settings->arrays.cheevos_username,
         locals->token, request->id, request->value);

   if (ret != 0)
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "Buffer too small to create URL\n");
      free(request);
      return;
   }

   rcheevos_log_url("rc_url_submit_lboard", buffer);
   task_push_http_transfer_with_user_agent(buffer, true, NULL,
         request->user_agent, rcheevos_async_task_callback, request);
}

void rcheevos_lboard_submit(rcheevos_ralboard_t* lboard, int value)
{
   char buffer[256];
   char formatted_value[16];

   /* Show the OSD message (regardless of notifications setting). */
   rc_runtime_format_lboard_value(formatted_value, sizeof(formatted_value),
         value, lboard->format);

   CHEEVOS_LOG(RCHEEVOS_TAG "Submitting %s for leaderboard %u\n",
         formatted_value, lboard->id);
   snprintf(buffer, sizeof(buffer), "Submitted %s for %s",
      formatted_value, lboard->title);
   runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#if defined(HAVE_GFX_WIDGETS)
   /* Hide the tracker */
   if (gfx_widgets_ready())
      gfx_widgets_set_leaderboard_display(lboard->id, NULL);
#endif

   /* Start the submit task. */
   {
      rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
      rcheevos_async_io_request
         *request              = (rcheevos_async_io_request*)
         calloc(1, sizeof(rcheevos_async_io_request));
     
      request->type            = CHEEVOS_ASYNC_SUBMIT_LBOARD;
      request->id              = lboard->id;
      request->value           = value;
      request->success_message = "Submitted leaderboard";
      request->failure_message = "Error submitting leaderboard";
      rcheevos_get_user_agent(rcheevos_locals,
            request->user_agent, sizeof(request->user_agent));
      rcheevos_async_submit_lboard(rcheevos_locals, request);
   }
}

/* dispatch */

static void rcheevos_async_task_handler(retro_task_t* task)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   rcheevos_async_io_request* request = (rcheevos_async_io_request*)
      task->user_data;

   switch (request->type)
   {
      case CHEEVOS_ASYNC_RICHPRESENCE:
         /* update the task to fire again in two minutes */
         if (request->id == (int)rcheevos_locals->patchdata.game_id)
            task->when = rcheevos_async_send_rich_presence(rcheevos_locals,
                  request);
         else
         {
            /* game changed; stop the recurring task - a new one will 
             * be scheduled for the next game */
            task_set_finished(task, 1);
            free(request);
         }
         break;

      case CHEEVOS_ASYNC_AWARD_ACHIEVEMENT:
         rcheevos_async_award_achievement(rcheevos_locals, request);
         task_set_finished(task, 1);
         break;

      case CHEEVOS_ASYNC_SUBMIT_LBOARD:
         rcheevos_async_submit_lboard(rcheevos_locals, request);
         task_set_finished(task, 1);
         break;
   }
}

static void rcheevos_async_schedule(
      rcheevos_async_io_request* request, retro_time_t delay)
{
   retro_task_t* task = task_init();
   task->when         = cpu_features_get_time_usec() + delay;
   task->handler      = rcheevos_async_task_handler;
   task->user_data    = request;
   task->progress     = -1;
   task_queue_push(task);
}

static void rcheevos_async_task_callback(
      retro_task_t* task, void* task_data, void* user_data, const char* error)
{
   rcheevos_async_io_request  *request = (rcheevos_async_io_request*)user_data;
   http_transfer_data_t        *data   = (http_transfer_data_t*)task_data;

   if (!error)
   {
      char buffer[224] = "";
      /* Server did not return HTTP headers */
      if (!data)
         snprintf(buffer, sizeof(buffer), "Server communication error");
      else if (data->status != 200)
      {
         /* Server returned an error via status code. 
          * Check to see if it also returned a JSON error */
         if (!data->data || rcheevos_get_json_error(data->data, buffer, sizeof(buffer)) != RC_OK)
            snprintf(buffer, sizeof(buffer), "HTTP error code: %d",
                  data->status);
      }
      else if (!data->data || !data->len)
      {
         /* Server sent an empty response without an error status code */
         snprintf(buffer, sizeof(buffer), "No response from server");
      }
      else
      {
         /* Server sent a message - assume it's JSON 
          * and check for a JSON error */
         rcheevos_get_json_error(data->data, buffer, sizeof(buffer));
      }

      if (buffer[0])
      {
         char errbuf[256];
         snprintf(errbuf, sizeof(errbuf), "%s %u: %s",
               request->failure_message, request->id, buffer);
         CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", errbuf);

         switch (request->type)
         {
            case CHEEVOS_ASYNC_RICHPRESENCE:
               /* Don't bother informing user when 
                * rich presence update fails */
               break;

            case CHEEVOS_ASYNC_AWARD_ACHIEVEMENT:
               /* ignore already unlocked */
               if (string_starts_with_size(buffer, "User already has ",
                        STRLEN_CONST("User already has ")))
                  break;
               /* fallthrough to default */

            default:
               runloop_msg_queue_push(errbuf, 0, 5 * 60, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
               break;
         }
      }
      else
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "%s %u\n", request->success_message, request->id);
      }

      free(request);
   }
   else
   {
      /* Double the wait between each attempt until we hit 
       * a maximum delay of two minutes.
      * 250ms -> 500ms -> 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 64s -> 120s -> 120s... */
      retro_time_t retry_delay = 
           (request->attempt_count > 8) 
         ? (120 * 1000 * 1000) 
         : ((250 * 1000) << request->attempt_count);

      request->attempt_count++;
      rcheevos_async_schedule(request, retry_delay);

      CHEEVOS_ERR(RCHEEVOS_TAG "%s %u: %s\n", request->failure_message,
            request->id, error);
   }
}



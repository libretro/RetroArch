/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019-2021 - Brian Weiss
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

#include <string/stdstring.h>
#include <features/features_cpu.h>

#include "../frontend/frontend_driver.h"
#include "../network/net_http_special.h"
#include "../tasks/tasks_internal.h"

#ifdef HAVE_DISCORD
#include "../network/discord.h"
#endif

#include "../deps/rcheevos/include/rc_api_runtime.h"



/* Define this macro to log URLs. */
#undef CHEEVOS_LOG_URLS

/* Define this macro to have the password and token logged.
 * THIS WILL DISCLOSE THE USER'S PASSWORD, TAKE CARE! */
#undef CHEEVOS_LOG_PASSWORD


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
   CHEEVOS_ASYNC_SUBMIT_LBOARD
};

typedef void (*rcheevos_async_handler)(int id, 
      http_transfer_data_t *data, char buffer[], size_t buffer_size);

typedef struct rcheevos_async_io_request
{
   rc_api_request_t request;
   rcheevos_async_handler handler;
   int id;
   int attempt_count;
   const char* success_message;
   const char* failure_message;
   const char* user_agent;
   char type;
} rcheevos_async_io_request;


/****************************
 * forward declarations     *
 ****************************/

static retro_time_t rcheevos_client_prepare_ping(rcheevos_async_io_request* request);

static void rcheevos_async_http_task_callback(
      retro_task_t* task, void* task_data, void* user_data, const char* error);


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
      {
         *ptr++ = *text++;
      }
   }

   *ptr = '\0';
   return (int)(ptr - buffer);
}

void rcheevos_get_user_agent(rcheevos_locals_t *locals,
      char *buffer, size_t len)
{
   char* ptr;
   struct retro_system_info *system = &runloop_state_get_ptr()->system.info;

   /* if we haven't calculated the non-changing portion yet, do so now
    * [retroarch version + os version] */
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

void rcheevos_log_url(const char* api, const char* url)
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
   (void)api;
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

static void rcheevos_async_retry_request(retro_task_t* task)
{
   rcheevos_async_io_request* request = (rcheevos_async_io_request*)
      task->user_data;

   /* the timer task has done its job. let it dispose itself */
   task_set_finished(task, 1);

   /* start a new task for the HTTP call */
   task_push_http_post_transfer_with_user_agent(request->request.url,
         request->request.post_data, true, "POST", request->user_agent,
         rcheevos_async_http_task_callback, request);
}

static void rcheevos_async_retry_request_after_delay(rcheevos_async_io_request* request, const char* error)
{
   retro_task_t* task = task_init();

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

static void rcheevos_async_request_failed(rcheevos_async_io_request* request, const char* error)
{
   if (request->type == CHEEVOS_ASYNC_RICHPRESENCE && request->attempt_count > 0)
   {
      /* only retry the ping once (in case of network hiccup), otherwise let
       * the timer handle it after the normal ping period has elapsed */
      CHEEVOS_ERR(RCHEEVOS_TAG "%s %u: %s\n", request->failure_message,
         request->id, error);
   }
   else
   {
      /* automatically retry the request */
      rcheevos_async_retry_request_after_delay(request, error);
   }
}

static void rcheevos_async_http_task_callback(
      retro_task_t* task, void* task_data, void* user_data, const char* error)
{
   rcheevos_async_io_request *request = (rcheevos_async_io_request*)user_data;
   http_transfer_data_t      *data    = (http_transfer_data_t*)task_data;
   char buffer[224];

   if (error)
   {
      /* there was a communication error */
      rcheevos_async_request_failed(request, error);
      return;
   }

   if (!data)
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

      if (data->status != 200)
      {
         /* Server returned an error via status code. */
         snprintf(buffer, sizeof(buffer), "HTTP error code %d", data->status);
      }
      else
      {
         /* Server sent an empty response without an error status code */
         strlcpy(buffer, "No response from server", sizeof(buffer));
      }
   }
   else
   {
      buffer[0] = '\0'; /* indicate success unless handler provides error */

      /* Call appropriate handler to process the response */
      if (request->handler)
      {
         /* NOTE: data->data is not null-terminated. Most handlers assume the
          * response is properly formatted or will encounter a parse failure
          * before reading past the end of the data */
         request->handler(request->id, data, buffer, sizeof(buffer));
      }
   }

   if (!buffer[0])
   {
      /* success */
      if (request->success_message)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "%s %u\n", request->success_message, request->id);
      }
   }
   else
   {
      /* encountered an error */
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

         default:
            runloop_msg_queue_push(errbuf, 0, 5 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
            break;
      }
   }

   rc_api_destroy_request(&request->request);

   /* rich presence request will be reused on next ping - reset the attempt
    * counter. for all other request types, free the request object */
   if (request->type == CHEEVOS_ASYNC_RICHPRESENCE)
      request->attempt_count = 0;
   else
      free(request);
}

static void rcheevos_async_begin_request(rcheevos_async_io_request* request,
      rcheevos_async_handler handler, char type, int id,
      const char* success_message, const char* failure_message)
{
   request->handler         = handler;
   request->type            = type;
   request->id              = id;
   request->success_message = success_message;
   request->failure_message = failure_message;
   request->attempt_count   = 0;

   if (!request->user_agent)
      request->user_agent = get_rcheevos_locals()->user_agent_core;

   rcheevos_log_post_url(request->request.url, request->request.post_data);

   task_push_http_post_transfer_with_user_agent(request->request.url,
         request->request.post_data, true, "POST", request->user_agent,
         rcheevos_async_http_task_callback, request);
}

static bool rcheevos_async_succeeded(int result, 
     const rc_api_response_t* response, char buffer[], size_t buffer_size)
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


/****************************
 * ping                     *
 ****************************/

static retro_time_t rcheevos_client_prepare_ping(rcheevos_async_io_request* request)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   const settings_t *settings = config_get_ptr();
   const bool cheevos_richpresence_enable = 
         settings->bools.cheevos_richpresence_enable;
   rc_api_ping_request_t api_params;
   char buffer[256] = "";

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

   rcheevos_log_post_url(request->request.url, request->request.post_data);

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

static void rcheevos_async_ping_handler(retro_task_t* task)
{
   rcheevos_async_io_request* request = (rcheevos_async_io_request*)
      task->user_data;

   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (request->id != (int)rcheevos_locals->patchdata.game_id)
   {
      /* game changed; stop the recurring task - a new one will
       * be scheduled if a new game is loaded */
      task_set_finished(task, 1);
      /* request->request was destroyed in rcheevos_async_http_task_callback */
      free(request);
      return;
   }

   /* update the request and set the task to fire again in
    * two minutes */
   task->when = rcheevos_client_prepare_ping(request);

   /* start the HTTP request */
   task_push_http_post_transfer_with_user_agent(request->request.url,
         request->request.post_data, true, "POST", request->user_agent,
         rcheevos_async_http_task_callback, request);
}


/****************************
 * start session            *
 ****************************/

void rcheevos_client_start_session(unsigned game_id)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

   /* the core won't change while a session is active, so only
    * calculate the user agent once */
   rcheevos_get_user_agent(rcheevos_locals,
         rcheevos_locals->user_agent_core,
         sizeof(rcheevos_locals->user_agent_core));

   /* force non-HTTPS until everything uses RAPI */
   rc_api_set_host("http://retroachievements.org");

   /* schedule the first rich presence call in 30 seconds */
   {
      rcheevos_async_io_request *request = (rcheevos_async_io_request*)
            calloc(1, sizeof(rcheevos_async_io_request));
      if (!request)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate rich presence request\n");
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

         task_queue_push(task);
      }
   }
}


/****************************
 * award achievement        *
 ****************************/

static void rcheevos_async_award_achievement_callback(int id,
      http_transfer_data_t *data, char buffer[], size_t buffer_size)
{
   rc_api_award_achievement_response_t api_response;

   int result = rc_api_process_award_achievement_response(&api_response, data->data);
   if (rcheevos_async_succeeded(result, &api_response.response, buffer, buffer_size))
   {
      if (api_response.awarded_achievement_id != id)
      {
         snprintf(buffer, buffer_size, "Achievement %u awarded instead",
               api_response.awarded_achievement_id);
      }
      else if (api_response.response.error_message)
      {
         /* previously unlocked achievements are returned as a "successful" error */
         CHEEVOS_LOG(RCHEEVOS_TAG "Achievement %u: %s\n",
               id, api_response.response.error_message);
      }
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

      memset(&api_params, 0, sizeof(api_params));
      api_params.username       = rcheevos_locals->username;
      api_params.api_token      = rcheevos_locals->token;
      api_params.achievement_id = achievement_id;
      api_params.hardcore       = rcheevos_locals->hardcore_active ? 1 : 0;
      api_params.game_hash      = rcheevos_locals->hash;

      rc_api_init_award_achievement_request(&request->request, &api_params);

      rcheevos_async_begin_request(request,
            rcheevos_async_award_achievement_callback, 
            CHEEVOS_ASYNC_AWARD_ACHIEVEMENT, achievement_id,
            "Awarded achievement",
            "Error awarding achievement");
   }
}


/****************************
 * submit leaderboard       *
 ****************************/

static void rcheevos_async_submit_lboard_entry_callback(int id,
      http_transfer_data_t* data, char buffer[], size_t buffer_size)
{
   rc_api_submit_lboard_entry_response_t api_response;

   int result = rc_api_process_submit_lboard_entry_response(&api_response, data->data);

   if (rcheevos_async_succeeded(result, &api_response.response, buffer, buffer_size))
   {
      /* not currently doing anything with the response */
   }

   rc_api_destroy_submit_lboard_entry_response(&api_response);
}

void rcheevos_client_submit_lboard_entry(unsigned leaderboard_id, int value)
{
   rcheevos_async_io_request *request = (rcheevos_async_io_request*)
      calloc(1, sizeof(rcheevos_async_io_request));
   if (!request)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate request for lboard %u submit\n",
            leaderboard_id);
   }
   else 
   {
      const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
      rc_api_submit_lboard_entry_request_t api_params;

      memset(&api_params, 0, sizeof(api_params));
      api_params.username       = rcheevos_locals->username;
      api_params.api_token      = rcheevos_locals->token;
      api_params.leaderboard_id = leaderboard_id;
      api_params.score          = value;
      api_params.game_hash      = rcheevos_locals->hash;

      rc_api_init_submit_lboard_entry_request(&request->request, &api_params);

      rcheevos_async_begin_request(request,
            rcheevos_async_submit_lboard_entry_callback, 
            CHEEVOS_ASYNC_SUBMIT_LBOARD, leaderboard_id,
            "Submitted leaderboard",
            "Error submitting leaderboard");
   }
}


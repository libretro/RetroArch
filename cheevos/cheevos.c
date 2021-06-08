/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#include <string.h>
#include <ctype.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>
#include <formats/cdfs.h>
#include <formats/m3u_file.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <net/net_http.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#ifdef HAVE_DISCORD
#include "../network/discord.h"
#endif

#ifdef HAVE_CHEATS
#include "../cheat_manager.h"
#endif

#ifdef HAVE_CHD
#include "streams/chd_stream.h"
#endif

#include "cheevos.h"
#include "cheevos_locals.h"
#include "cheevos_memory.h"
#include "cheevos_parser.h"

#include "../file_path_special.h"
#include "../paths.h"
#include "../command.h"
#include "../dynamic.h"
#include "../configuration.h"
#include "../performance_counters.h"
#include "../msg_hash.h"
#include "../retroarch.h"
#include "../core.h"
#include "../core_option_manager.h"
#include "../version.h"

#include "../frontend/frontend_driver.h"
#include "../network/net_http_special.h"
#include "../tasks/tasks_internal.h"

#include "../deps/rcheevos/include/rc_runtime_types.h"
#include "../deps/rcheevos/include/rc_url.h"
#include "../deps/rcheevos/include/rc_hash.h"

/* Define this macro to prevent cheevos from being deactivated. */
#undef CHEEVOS_DONT_DEACTIVATE

/* Define this macro to load a JSON file from disk instead of downloading
 * from retroachievements.org. */
#undef CHEEVOS_JSON_OVERRIDE

/* Define this macro with a string to save the JSON file to disk with
 * that name. */
#undef CHEEVOS_SAVE_JSON

/* Define this macro to log URLs. */
#undef CHEEVOS_LOG_URLS

/* Define this macro to have the password and token logged. THIS WILL DISCLOSE
 * THE USER'S PASSWORD, TAKE CARE! */
#undef CHEEVOS_LOG_PASSWORD

/* Define this macro to log downloaded badge images. */
#undef CHEEVOS_LOG_BADGES

/* Define this macro to capture how long it takes to generate a hash */
#undef CHEEVOS_TIME_HASH

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

static rcheevos_locals_t rcheevos_locals =
{
   {0},  /* runtime */
   {0},  /* patchdata */
   {{0}},/* memory */
   NULL, /* task */
#ifdef HAVE_THREADS
   NULL, /* task_lock */
   CMD_EVENT_NONE, /* queued_command */
#endif
   {0},  /* token */
   "N/A",/* hash */
   "",   /* user_agent_prefix */
#ifdef HAVE_MENU
   NULL, /* menuitems */
   0,    /* menuitem_capacity */
   0,    /* menuitem_count */
#endif
   false,/* hardcore_active */
   false,/* loaded */
   true, /* core_supports */
   false,/* leaderboards_enabled */
   false,/* leaderboard_notifications */
   false /* leaderboard_trackers */
};

rcheevos_locals_t* get_rcheevos_locals()
{
   return &rcheevos_locals;
}

#ifdef HAVE_THREADS
#define CHEEVOS_LOCK(l)   do { slock_lock(l); } while (0)
#define CHEEVOS_UNLOCK(l) do { slock_unlock(l); } while (0)
#else
#define CHEEVOS_LOCK(l)
#define CHEEVOS_UNLOCK(l)
#endif

#define CHEEVOS_MB(x)   ((x) * 1024 * 1024)

/* Forward declaration */
static void rcheevos_async_task_callback(
      retro_task_t* task, void* task_data, void* user_data, const char* error);
static void rcheevos_async_submit_lboard(rcheevos_locals_t *locals,
      rcheevos_async_io_request* request);

/*****************************************************************************
Supporting functions.
*****************************************************************************/

#ifndef CHEEVOS_VERBOSE
void rcheevos_log(const char *fmt, ...)
{
   (void)fmt;
}
#endif

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

static void rcheevos_get_user_agent(
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

static void rcheevos_log_url(const char* api, const char* url)
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

static bool rcheevos_condset_contains_memref(const rc_condset_t* condset,
      const rc_memref_t* memref)
{
   if (condset)
   {
      rc_condition_t* cond = NULL;
      for (cond = condset->conditions; cond; cond = cond->next)
      {
         if (     cond->operand1.value.memref == memref 
               || cond->operand2.value.memref == memref)
            return true;
      }
   }

   return false;
}

static bool rcheevos_trigger_contains_memref(const rc_trigger_t* trigger,
      const rc_memref_t* memref)
{
   rc_condset_t* condset;
   if (!trigger)
      return false;

   if (rcheevos_condset_contains_memref(trigger->requirement, memref))
      return true;

   for (condset = trigger->alternative; condset; condset = condset->next)
   {
      if (rcheevos_condset_contains_memref(condset, memref))
         return true;
   }

   return false;
}

static void rcheevos_achievement_disabled(rcheevos_racheevo_t* cheevo, unsigned address)
{
   if (!cheevo)
      return;

   CHEEVOS_ERR(RCHEEVOS_TAG "Achievement disabled (invalid address %06X): %s\n", address, cheevo->title);
   CHEEVOS_FREE(cheevo->memaddr);
   cheevo->memaddr = NULL;
}

static void rcheevos_lboard_disabled(rcheevos_ralboard_t* lboard, unsigned address)
{
   if (!lboard)
      return;

   CHEEVOS_ERR(RCHEEVOS_TAG "Leaderboard disabled (invalid address %06X): %s\n", address, lboard->title);
   CHEEVOS_FREE(lboard->mem);
   lboard->mem = NULL;
}

static void rcheevos_invalidate_address(unsigned address)
{
   unsigned i, count;
   rcheevos_racheevo_t* cheevo     = NULL;
   rcheevos_ralboard_t* lboard     = NULL;
   /* Remove the invalid memref from the chain so we don't 
    * try to evaluate it in the future.
    * It's still there, so anything referencing it will 
    * continue to fetch 0. */
   rc_memref_t **last_memref       = &rcheevos_locals.runtime.memrefs;
   rc_memref_t *memref             = *last_memref;

   do
   {
      if (memref->address == address && !memref->value.is_indirect)
      {
         *last_memref = memref->next;
         break;
      }

      last_memref = &memref->next;
      memref      = *last_memref;
   } while(memref);

   /* If the address is only used indirectly, 
    * don't disable anything dependent on it */
   if (!memref)
      return;

   /* Disable any achievements dependent on the address */
   for (i = 0; i < 2; ++i)
   {
      if (i == 0)
      {
         cheevo = rcheevos_locals.patchdata.core;
         count  = rcheevos_locals.patchdata.core_count;
      }
      else
      {
         cheevo = rcheevos_locals.patchdata.unofficial;
         count  = rcheevos_locals.patchdata.unofficial_count;
      }

      while (count--)
      {
         if (cheevo->memaddr)
         {
            const rc_trigger_t* trigger = rc_runtime_get_achievement(
                  &rcheevos_locals.runtime, cheevo->id);

            if (trigger && rcheevos_trigger_contains_memref(trigger, memref))
            {
               rcheevos_achievement_disabled(cheevo, address);
               rc_runtime_deactivate_achievement(&rcheevos_locals.runtime,
                     cheevo->id);
            }
         }

         ++cheevo;
      }
   }

   /* disable any leaderboards dependent on the address */
   lboard = rcheevos_locals.patchdata.lboards;
   for (i = 0; i < rcheevos_locals.patchdata.lboard_count; ++i, ++lboard)
   {
      if (lboard->mem)
      {
         const rc_lboard_t* rc_lboard = rc_runtime_get_lboard(
               &rcheevos_locals.runtime, lboard->id);

         if (  rc_lboard &&
            (  rcheevos_trigger_contains_memref(&rc_lboard->start,  memref) ||
               rcheevos_trigger_contains_memref(&rc_lboard->cancel, memref) ||
               rcheevos_trigger_contains_memref(&rc_lboard->submit, memref) ||
               rcheevos_condset_contains_memref(rc_lboard->value.conditions,
                  memref))
            )
         {
            rcheevos_lboard_disabled(lboard, address);
            rc_runtime_deactivate_lboard(&rcheevos_locals.runtime, lboard->id);
         }
      }
   }
}

uint8_t* rcheevos_patch_address(unsigned address)
{
   if (rcheevos_locals.memory.count == 0)
   {
      /* memory map was not previously initialized (no achievements for this game?) try now */
      rcheevos_memory_init(&rcheevos_locals.memory, rcheevos_locals.patchdata.console_id);
   }

   return rcheevos_memory_find(&rcheevos_locals.memory, address);
}

static unsigned rcheevos_peek(unsigned address, unsigned num_bytes, void* ud)
{
   uint8_t* data = rcheevos_memory_find(&rcheevos_locals.memory, address);
   if (data)
   {
      switch (num_bytes)
      {
         case 4:
            return (data[3] << 24) | (data[2] << 16) | 
                   (data[1] <<  8) | (data[0]);
         case 3:
            return (data[2] << 16) | (data[1] << 8) | (data[0]);
         case 2:
            return (data[1] << 8)  | (data[0]);
         case 1:
            return data[0];
      }
   }

   return 0;
}

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

static void rcheevos_async_task_handler(retro_task_t* task)
{
   rcheevos_async_io_request* request = (rcheevos_async_io_request*)
      task->user_data;

   switch (request->type)
   {
      case CHEEVOS_ASYNC_RICHPRESENCE:
         /* update the task to fire again in two minutes */
         if (request->id == (int)rcheevos_locals.patchdata.game_id)
            task->when = rcheevos_async_send_rich_presence(&rcheevos_locals,
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
         rcheevos_async_award_achievement(&rcheevos_locals, request);
         task_set_finished(task, 1);
         break;

      case CHEEVOS_ASYNC_SUBMIT_LBOARD:
         rcheevos_async_submit_lboard(&rcheevos_locals, request);
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

static void rcheevos_validate_memrefs(rcheevos_locals_t* locals)
{
   rc_memref_t* memref = locals->runtime.memrefs;

   while (memref)
   {
      if (!memref->value.is_indirect)
      {
         uint8_t* data = rcheevos_memory_find(&rcheevos_locals.memory,
               memref->address);
         if (!data)
            rcheevos_invalidate_address(memref->address);
      }

      memref = memref->next;
   }
}

static void rcheevos_activate_achievements(rcheevos_locals_t *locals,
      rcheevos_racheevo_t* cheevo, unsigned count, unsigned flags)
{
   int res;
   unsigned i;
   char buffer[256];
   settings_t *settings = config_get_ptr();

   for (i = 0; i < count; i++, cheevo++)
   {
      res = rc_runtime_activate_achievement(&locals->runtime, cheevo->id,
            cheevo->memaddr, NULL, 0);

      if (res < 0)
      {
         snprintf(buffer, sizeof(buffer),
               "Could not activate achievement %d \"%s\": %s",
               cheevo->id, cheevo->title, rc_error_str(res));

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         CHEEVOS_ERR(RCHEEVOS_TAG "%s: mem %s\n", buffer, cheevo->memaddr);
         CHEEVOS_FREE(cheevo->memaddr);
         cheevo->memaddr = NULL;
         continue;
      }

      cheevo->active = 
           RCHEEVOS_ACTIVE_SOFTCORE 
         | RCHEEVOS_ACTIVE_HARDCORE 
         | flags;
   }
}

static int rcheevos_parse(rcheevos_locals_t *locals, const char* json)
{
   char buffer[256];
   unsigned j                  = 0;
   unsigned count              = 0;
   settings_t *settings        = NULL;
   rcheevos_ralboard_t* lboard = NULL;
   int res                     = rcheevos_get_patchdata(
         json, &locals->patchdata);

   if (res != 0)
   {
      char *ptr = NULL;
      strcpy_literal(buffer, "Error retrieving achievement data: ");
      ptr       = buffer + strlen(buffer);

      /* Extract the Error field from the JSON. 
       * If not found, remove the colon from the message. */
      if (rcheevos_get_json_error(json, ptr,
               sizeof(buffer) - (ptr - buffer)) == -1)
         ptr[-2] = '\0';

      runloop_msg_queue_push(buffer, 0, 5 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);

      RARCH_ERR(RCHEEVOS_TAG "%s", buffer);
      return -1;
   }

   if (   locals->patchdata.core_count       == 0
       && locals->patchdata.unofficial_count == 0
       && locals->patchdata.lboard_count     == 0)
   {
      rcheevos_free_patchdata(&locals->patchdata);
      return 0;
   }

   settings        = config_get_ptr();

   if (!rcheevos_memory_init(&locals->memory, locals->patchdata.console_id))
   {
      /* some cores (like Mupen64-Plus) don't expose the 
       * memory until the first call to retro_run.
       * in that case, there will be a total_size of 
       * memory reported by the core, but init will return
       * false, as all of the pointers were null.
       */

      /* reset the memory count and we'll re-evaluate in rcheevos_test() */
      if (locals->memory.total_size != 0)
         locals->memory.count = 0;
      else
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "No memory exposed by core.\n");
         rcheevos_locals.core_supports = false;

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
               0, 4 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);

         goto error;
      }
   }

   /* Initialize. */
   rcheevos_activate_achievements(locals, locals->patchdata.core,
         locals->patchdata.core_count, 0);

   if (settings->bools.cheevos_test_unofficial)
      rcheevos_activate_achievements(locals, locals->patchdata.unofficial,
            locals->patchdata.unofficial_count, RCHEEVOS_ACTIVE_UNOFFICIAL);

   if (locals->hardcore_active && locals->leaderboards_enabled)
   {
      lboard = locals->patchdata.lboards;
      count  = locals->patchdata.lboard_count;

      for (j = 0; j < count; j++, lboard++)
      {
         res = rc_runtime_activate_lboard(&locals->runtime, lboard->id,
               lboard->mem, NULL, 0);

         if (res < 0)
         {
            snprintf(buffer, sizeof(buffer),
                  "Could not activate leaderboard %d \"%s\": %s",
                  lboard->id, lboard->title, rc_error_str(res));

            if (settings->bools.cheevos_verbose_enable)
               runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

            CHEEVOS_ERR(RCHEEVOS_TAG "%s mem: %s\n", buffer, lboard->mem);
            CHEEVOS_FREE(lboard->mem);
            lboard->mem = NULL;
            continue;
         }
      }
   }

   res = RC_MISSING_DISPLAY_STRING;
   if (      locals->patchdata.richpresence_script 
         && *locals->patchdata.richpresence_script)
   {
      res = rc_runtime_activate_richpresence(&locals->runtime,
            locals->patchdata.richpresence_script, NULL, 0);

      if (res < 0)
      {
         snprintf(buffer, sizeof(buffer),
               "Could not activate rich presence: %s",
               rc_error_str(res));

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         CHEEVOS_ERR(RCHEEVOS_TAG "%s\n", buffer);
      }
   }

   /* schedule the first rich presence call in 30 seconds */
   {
      rcheevos_async_io_request* request = (rcheevos_async_io_request*)
         calloc(1, sizeof(rcheevos_async_io_request));
      request->id                        = locals->patchdata.game_id;
      request->type                      = CHEEVOS_ASYNC_RICHPRESENCE;
      rcheevos_async_schedule(request, CHEEVOS_PING_FREQUENCY / 4);
   }

   /* validate the memrefs */
   if (rcheevos_locals.memory.count != 0)
      rcheevos_validate_memrefs(&rcheevos_locals);

   return 0;

error:
   rcheevos_free_patchdata(&locals->patchdata);
   rcheevos_memory_destroy(&locals->memory);
   return -1;
}

static rcheevos_racheevo_t* rcheevos_find_cheevo(unsigned id)
{
   unsigned i;
   rcheevos_racheevo_t* cheevo;

   cheevo = rcheevos_locals.patchdata.core;
   for (i = 0; i < rcheevos_locals.patchdata.core_count; i++, cheevo++)
   {
      if (cheevo->id == id)
         return cheevo;
   }

   cheevo = rcheevos_locals.patchdata.unofficial;
   for (i = 0; i < rcheevos_locals.patchdata.unofficial_count; i++, cheevo++)
   {
      if (cheevo->id == id)
         return cheevo;
   }

   return NULL;
}

static void rcheevos_award_achievement(rcheevos_locals_t *locals,
      rcheevos_racheevo_t* cheevo, bool widgets_ready)
{
   char buffer[256] = "";

   if (!cheevo)
      return;

   CHEEVOS_LOG(RCHEEVOS_TAG "Awarding achievement %u: %s (%s)\n",
         cheevo->id, cheevo->title, cheevo->description);

   /* Deactivates the cheevo. */
   rc_runtime_deactivate_achievement(&locals->runtime, cheevo->id);

   cheevo->active &= ~RCHEEVOS_ACTIVE_SOFTCORE;
   if (locals->hardcore_active)
      cheevo->active &= ~RCHEEVOS_ACTIVE_HARDCORE;

   cheevo->unlock_time = cpu_features_get_time_usec();

   /* Show the OSD message. */
   {
#if defined(HAVE_GFX_WIDGETS)
      if (widgets_ready)
         gfx_widgets_push_achievement(cheevo->title, cheevo->badge);
      else
#endif
      {
         snprintf(buffer, sizeof(buffer),
               "Achievement Unlocked: %s", cheevo->title);
         runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         runloop_msg_queue_push(cheevo->description, 0, 3 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

   /* Start the award task (unofficial achievement unlocks are not submitted). */
   if (!(cheevo->active & RCHEEVOS_ACTIVE_UNOFFICIAL))
   {
      rcheevos_async_io_request *request = (rcheevos_async_io_request*)calloc(1, sizeof(rcheevos_async_io_request));
      request->type            = CHEEVOS_ASYNC_AWARD_ACHIEVEMENT;
      request->id              = cheevo->id;
      request->hardcore        = locals->hardcore_active ? 1 : 0;
      request->success_message = "Awarded achievement";
      request->failure_message = "Error awarding achievement";
      rcheevos_get_user_agent(locals,
            request->user_agent, sizeof(request->user_agent));
      rcheevos_async_award_achievement(locals, request);
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

static rcheevos_ralboard_t* rcheevos_find_lboard(unsigned id)
{
   rcheevos_ralboard_t* lboard = rcheevos_locals.patchdata.lboards;
   unsigned i;

   for (i = 0; i < rcheevos_locals.patchdata.lboard_count; ++i, ++lboard)
   {
      if (lboard->id == id)
         return lboard;
   }

   return NULL;
}

static void rcheevos_lboard_submit(rcheevos_locals_t *locals,
      rcheevos_ralboard_t* lboard, int value, bool widgets_ready)
{
   char buffer[256];
   char formatted_value[16];

   /* Show the OSD message (regardless of notifications setting). */
   rc_format_value(formatted_value, sizeof(formatted_value),
         value, lboard->format);

   CHEEVOS_LOG(RCHEEVOS_TAG "Submitting %s for leaderboard %u\n",
         formatted_value, lboard->id);
   snprintf(buffer, sizeof(buffer), "Submitted %s for %s",
      formatted_value, lboard->title);
   runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#if defined(HAVE_GFX_WIDGETS)
   /* Hide the tracker */
   if (widgets_ready)
      gfx_widgets_set_leaderboard_display(lboard->id, NULL);
#endif

   /* Start the submit task. */
   {
      rcheevos_async_io_request
         *request              = (rcheevos_async_io_request*)
         calloc(1, sizeof(rcheevos_async_io_request));
     
      request->type            = CHEEVOS_ASYNC_SUBMIT_LBOARD;
      request->id              = lboard->id;
      request->value           = value;
      request->success_message = "Submitted leaderboard";
      request->failure_message = "Error submitting leaderboard";
      rcheevos_get_user_agent(locals,
            request->user_agent, sizeof(request->user_agent));
      rcheevos_async_submit_lboard(locals, request);
   }
}

static void rcheevos_lboard_canceled(rcheevos_ralboard_t * lboard,
      bool widgets_ready)
{
   char buffer[256];
   if (!lboard)
      return;

   CHEEVOS_LOG(RCHEEVOS_TAG "Leaderboard %u canceled: %s\n",
         lboard->id, lboard->title);

#if defined(HAVE_GFX_WIDGETS)
   if (widgets_ready)
      gfx_widgets_set_leaderboard_display(lboard->id, NULL);
#endif

   if (rcheevos_locals.leaderboard_notifications)
   {
      snprintf(buffer, sizeof(buffer),
            "Leaderboard attempt failed: %s", lboard->title);
      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

static void rcheevos_lboard_started(rcheevos_ralboard_t * lboard, int value,
      bool widgets_ready)
{
   char buffer[256];
   if (!lboard)
      return;

   CHEEVOS_LOG(RCHEEVOS_TAG "Leaderboard %u started: %s\n",
         lboard->id, lboard->title);

#if defined(HAVE_GFX_WIDGETS)
   if (widgets_ready && rcheevos_locals.leaderboard_trackers)
   {
      rc_format_value(buffer, sizeof(buffer), value, lboard->format);
      gfx_widgets_set_leaderboard_display(lboard->id, buffer);
   }
#endif

   if (rcheevos_locals.leaderboard_notifications)
   {
      if (lboard->description && *lboard->description)
         snprintf(buffer, sizeof(buffer),
               "Leaderboard attempt started: %s - %s",
               lboard->title, lboard->description);
      else
         snprintf(buffer, sizeof(buffer),
               "Leaderboard attempt started: %s",
               lboard->title);

      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

#if defined(HAVE_GFX_WIDGETS)
static void rcheevos_lboard_updated(rcheevos_ralboard_t* lboard, int value,
      bool widgets_ready)
{
   if (!lboard)
      return;

   if (widgets_ready && rcheevos_locals.leaderboard_trackers)
   {
      char buffer[32];
      rc_format_value(buffer, sizeof(buffer), value, lboard->format);
      gfx_widgets_set_leaderboard_display(lboard->id, buffer);
   }
}

static void rcheevos_challenge_started(rcheevos_racheevo_t* cheevo, int value,
      bool widgets_ready)
{
   settings_t* settings = config_get_ptr();
   if (cheevo && widgets_ready && settings->bools.cheevos_challenge_indicators)
      gfx_widgets_set_challenge_display(cheevo->id, cheevo->badge);
}

static void rcheevos_challenge_ended(rcheevos_racheevo_t* cheevo, int value,
      bool widgets_ready)
{
   if (cheevo && widgets_ready)
      gfx_widgets_set_challenge_display(cheevo->id, NULL);
}

#endif

int rcheevos_get_richpresence(char buffer[], int buffer_size)
{
   int ret = rc_runtime_get_richpresence(&rcheevos_locals.runtime, buffer, buffer_size, &rcheevos_peek, NULL, NULL);

   if (ret <= 0 && rcheevos_locals.patchdata.title)
      ret = snprintf(buffer, buffer_size, "Playing %s", rcheevos_locals.patchdata.title);

   return ret;
}

void rcheevos_reset_game(bool widgets_ready)
{
#if defined(HAVE_GFX_WIDGETS)
   /* Hide any visible trackers */
   if (widgets_ready)
   {
      rcheevos_ralboard_t* lboard = rcheevos_locals.patchdata.lboards;
      unsigned i;

      for (i = 0; i < rcheevos_locals.patchdata.lboard_count; ++i, ++lboard)
         gfx_widgets_set_leaderboard_display(lboard->id, NULL);
   }
#endif

   rc_runtime_reset(&rcheevos_locals.runtime);

   /* Some cores reallocate memory on reset, 
    * make sure we update our pointers */
   if (rcheevos_locals.memory.total_size > 0)
      rcheevos_memory_init(
            &rcheevos_locals.memory,
             rcheevos_locals.patchdata.console_id);
}

bool rcheevos_hardcore_active(void)
{
   return rcheevos_locals.hardcore_active;
}

void rcheevos_pause_hardcore(void)
{
   if (rcheevos_locals.hardcore_active)
      rcheevos_toggle_hardcore_paused();
}

bool rcheevos_unload(void)
{
   bool running          = false;
   settings_t* settings  = config_get_ptr();

   CHEEVOS_LOCK(rcheevos_locals.task_lock);
   running               = rcheevos_locals.task != NULL;
   CHEEVOS_UNLOCK(rcheevos_locals.task_lock);

   if (running)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Asked the load thread to terminate\n");
      task_queue_cancel_task(rcheevos_locals.task);

#ifdef HAVE_THREADS
      do
      {
         CHEEVOS_LOCK(rcheevos_locals.task_lock);
         running = rcheevos_locals.task != NULL;
         CHEEVOS_UNLOCK(rcheevos_locals.task_lock);
      } while(running);
#endif
   }

   if (rcheevos_locals.memory.count > 0)
      rcheevos_memory_destroy(&rcheevos_locals.memory);

   if (rcheevos_locals.loaded)
   {
#ifdef HAVE_MENU
      rcheevos_menu_reset_badges();

      if (rcheevos_locals.menuitems)
      {
         CHEEVOS_FREE(rcheevos_locals.menuitems);
         rcheevos_locals.menuitems = NULL;
         rcheevos_locals.menuitem_capacity = rcheevos_locals.menuitem_count = 0;
      }
#endif
      rcheevos_free_patchdata(&rcheevos_locals.patchdata);

      rcheevos_locals.loaded                    = false;
      rcheevos_locals.hardcore_active           = false;
   }

#ifdef HAVE_THREADS
   rcheevos_locals.queued_command = CMD_EVENT_NONE;
#endif

   rc_runtime_destroy(&rcheevos_locals.runtime);

   /* If the config-level token has been cleared, 
    * we need to re-login on loading the next game */
   if (!settings->arrays.cheevos_token[0])
      rcheevos_locals.token[0]                  = '\0';

   return true;
}

static void rcheevos_toggle_hardcore_achievements(rcheevos_locals_t *locals, 
      rcheevos_racheevo_t* cheevo, unsigned count)
{
   const unsigned active_mask = 
      RCHEEVOS_ACTIVE_SOFTCORE | RCHEEVOS_ACTIVE_HARDCORE;

   while (count--)
   {
      if (cheevo->memaddr && (cheevo->active & active_mask) 
            == RCHEEVOS_ACTIVE_HARDCORE)
      {
         /* player has unlocked achievement in non-hardcore,
          * but has not unlocked in hardcore. Toggle state */
         if (locals->hardcore_active)
         {
            rc_runtime_activate_achievement(&locals->runtime, cheevo->id, cheevo->memaddr, NULL, 0);
            CHEEVOS_LOG(RCHEEVOS_TAG "Achievement %u activated: %s\n", cheevo->id, cheevo->title);
         }
         else
         {
            rc_runtime_deactivate_achievement(&locals->runtime, cheevo->id);
            CHEEVOS_LOG(RCHEEVOS_TAG "Achievement %u deactivated: %s\n", cheevo->id, cheevo->title);
         }
      }

      ++cheevo;
   }
}

static void rcheevos_activate_leaderboards(rcheevos_locals_t* locals)
{
   rcheevos_ralboard_t* lboard = locals->patchdata.lboards;
   unsigned i;

   for (i = 0; i < locals->patchdata.lboard_count; ++i, ++lboard)
   {
      if (lboard->mem)
         rc_runtime_activate_lboard(&locals->runtime, lboard->id,
               lboard->mem, NULL, 0);
   }
}

static void rcheevos_deactivate_leaderboards(rcheevos_locals_t* locals)
{
   rcheevos_ralboard_t* lboard = locals->patchdata.lboards;
   unsigned i;

   for (i = 0; i < locals->patchdata.lboard_count; ++i, ++lboard)
   {
      if (lboard->mem)
      {
         rc_runtime_deactivate_lboard(&locals->runtime, lboard->id);

#if defined(HAVE_GFX_WIDGETS)
         /* Hide any visible trackers */
         gfx_widgets_set_leaderboard_display(lboard->id, NULL);
#endif
      }
   }
}

void rcheevos_leaderboards_enabled_changed(void)
{
   const settings_t* settings           = config_get_ptr();
   const bool leaderboards_enabled      = rcheevos_locals.leaderboards_enabled;
   const bool leaderboard_trackers      = rcheevos_locals.leaderboard_trackers;

   rcheevos_locals.leaderboards_enabled = rcheevos_locals.hardcore_active;

   if (string_is_equal(settings->arrays.cheevos_leaderboards_enable, "true"))
   {
      rcheevos_locals.leaderboard_notifications = true;
      rcheevos_locals.leaderboard_trackers = true;
   }
#if defined(HAVE_GFX_WIDGETS)
   else if (string_is_equal(
            settings->arrays.cheevos_leaderboards_enable, "trackers"))
   {
      rcheevos_locals.leaderboard_notifications = false;
      rcheevos_locals.leaderboard_trackers      = true;
   }
   else if (string_is_equal(
            settings->arrays.cheevos_leaderboards_enable, "notifications"))
   {
      rcheevos_locals.leaderboard_notifications = true;
      rcheevos_locals.leaderboard_trackers      = false;
   }
#endif
   else
   {
      rcheevos_locals.leaderboards_enabled      = false;
      rcheevos_locals.leaderboard_notifications = false;
      rcheevos_locals.leaderboard_trackers      = false;
   }

   if (rcheevos_locals.loaded)
   {
      if (leaderboards_enabled != rcheevos_locals.leaderboards_enabled)
      {
         if (rcheevos_locals.leaderboards_enabled)
            rcheevos_activate_leaderboards(&rcheevos_locals);
         else
            rcheevos_deactivate_leaderboards(&rcheevos_locals);
      }

#if defined(HAVE_GFX_WIDGETS)
      if (!rcheevos_locals.leaderboard_trackers && leaderboard_trackers)
      {
         /* Hide any visible trackers */
         unsigned i;
         rcheevos_ralboard_t* lboard = rcheevos_locals.patchdata.lboards;

         for (i = 0; i < rcheevos_locals.patchdata.lboard_count; ++i, ++lboard)
         {
            if (lboard->mem)
               gfx_widgets_set_leaderboard_display(lboard->id, NULL);
         }
      }
#endif
   }
}

static void rcheevos_toggle_hardcore_active(rcheevos_locals_t* locals)
{
   settings_t* settings = config_get_ptr();
   bool rewind_enable   = settings->bools.rewind_enable;

   if (!locals->hardcore_active)
   {
      /* Activate hardcore */
      locals->hardcore_active = true;

      /* If one or more invalid settings is enabled, abort*/
      rcheevos_validate_config_settings();
      if (!locals->hardcore_active)
         return;

#ifdef HAVE_CHEATS
      /* If one or more emulator managed cheats is active, abort */
      cheat_manager_apply_cheats();
      if (!locals->hardcore_active)
         return;
#endif

      if (locals->loaded)
      {
         const char* msg = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_ENABLE);
         CHEEVOS_LOG("%s\n", msg);
         runloop_msg_queue_push(msg, 0, 3 * 60, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         /* Reactivate leaderboards */
         if (locals->leaderboards_enabled)
            rcheevos_activate_leaderboards(locals);

         /* reset the game */
         command_event(CMD_EVENT_RESET, NULL);
      }

      /* deinit rewind */
      if (rewind_enable)
      {
#ifdef HAVE_THREADS
         /* have to "schedule" this. 
          * CMD_EVENT_REWIND_DEINIT should 
          * only be called on the main thread */
         rcheevos_locals.queued_command = CMD_EVENT_REWIND_DEINIT;
#else
         command_event(CMD_EVENT_REWIND_DEINIT, NULL);
#endif
      }
   }
   else
   {
      /* pause hardcore */
      locals->hardcore_active = false;

      if (locals->loaded)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Hardcore paused\n");

         /* deactivate leaderboards */
         rcheevos_deactivate_leaderboards(locals);
      }

      /* re-init rewind */
      if (rewind_enable)
      {
#ifdef HAVE_THREADS
         /* have to "schedule" this. 
          * CMD_EVENT_REWIND_INIT should 
          * only be called on the main thread */
         rcheevos_locals.queued_command = CMD_EVENT_REWIND_INIT;
#else
         command_event(CMD_EVENT_REWIND_INIT, NULL);
#endif
      }
   }

   if (locals->loaded)
   {
      rcheevos_toggle_hardcore_achievements(locals,
            locals->patchdata.core, locals->patchdata.core_count);
      if (settings->bools.cheevos_test_unofficial)
         rcheevos_toggle_hardcore_achievements(locals,
               locals->patchdata.unofficial,
               locals->patchdata.unofficial_count);
   }
}

void rcheevos_toggle_hardcore_paused(void)
{
   settings_t* settings = config_get_ptr();
   /* if hardcore mode is not enabled, we can't toggle it */
   if (settings->bools.cheevos_hardcore_mode_enable)
      rcheevos_toggle_hardcore_active(&rcheevos_locals);
}

void rcheevos_hardcore_enabled_changed(void)
{
   const settings_t* settings = config_get_ptr();
   const bool enabled         = settings 
      && settings->bools.cheevos_enable 
      && settings->bools.cheevos_hardcore_mode_enable;

   if (enabled != rcheevos_locals.hardcore_active)
   {
      rcheevos_toggle_hardcore_active(&rcheevos_locals);

      /* update leaderboard state flags */
      rcheevos_leaderboards_enabled_changed();
   }
}

typedef struct rc_disallowed_setting_t
{
   const char* setting;
   const char* value;
} rc_disallowed_setting_t;

typedef struct rc_disallowed_core_settings_t
{
   const char* library_name;
   const rc_disallowed_setting_t* disallowed_settings;
} rc_disallowed_core_settings_t;

static const rc_disallowed_setting_t _rc_disallowed_dolphin_settings[] = {
   { "dolphin_cheats_enabled",      "enabled" },
   { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_ecwolf_settings[] = {
   { "ecwolf-invulnerability",      "enabled" },
   { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_fbneo_settings[] = {
   { "fbneo-allow-patched-romsets", "enabled" },
   { "fbneo-cheat-*",               "!,Disabled,0 - Disabled" },
   { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_gpgx_settings[] = {
   { "genesis_plus_gx_lock_on",     ",action replay (pro),game genie" },
   { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_ppsspp_settings[] = {
   { "ppsspp_cheats",               "enabled" },
   { NULL, NULL }
};

static const rc_disallowed_core_settings_t rc_disallowed_core_settings[] = {
   { "dolphin-emu",     _rc_disallowed_dolphin_settings },
   { "ecwolf",          _rc_disallowed_ecwolf_settings },
   { "FinalBurn Neo",   _rc_disallowed_fbneo_settings },
   { "Genesis Plus GX", _rc_disallowed_gpgx_settings },
   { "PPSSPP",          _rc_disallowed_ppsspp_settings },
   { NULL,              NULL }
};

static int rcheevos_match_value(const char* val, const char* match)
{
   /* if value starts with a comma, it's a CSV list of potential matches */
   if (*match == ',')
   {
      do
      {
         int size;
         const char* ptr = ++match;

         while (*match && *match != ',')
            ++match;

         size = match - ptr;
         if (val[size] == '\0')
         {
            char buffer[128];
            if (string_is_equal_fast(ptr, val, size))
               return true;

            memcpy(buffer, ptr, size);
            buffer[size] = '\0';
            if (string_is_equal_case_insensitive(buffer, val))
               return true;
         }
      } while(*match == ',');

      return false;
   }

   /* a leading exclamation point means the provided value(s) 
    * are not forbidden (are allowed) */
   if (*match == '!')
      return !rcheevos_match_value(val, &match[1]);

   /* just a single value, attempt to match it */
   return string_is_equal_case_insensitive(val, match);
}

void rcheevos_validate_config_settings(void)
{
   const rc_disallowed_core_settings_t 
      *core_filter                  = rc_disallowed_core_settings;
   struct retro_system_info* system = runloop_get_libretro_system_info();
   if (!system->library_name || !rcheevos_locals.hardcore_active)
      return;

   while (core_filter->library_name)
   {
      if (string_is_equal(core_filter->library_name, system->library_name))
      {
         core_option_manager_t* coreopts = NULL;

         if (rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
         {
            size_t i;
            const char *val               = NULL;
            const rc_disallowed_setting_t
               *disallowed_setting        = core_filter->disallowed_settings;
            int allowed                   = 1;

            for (; disallowed_setting->setting; ++disallowed_setting)
            {
               const char *key = disallowed_setting->setting;
               size_t key_len  = strlen(key);

               if (key[key_len - 1] == '*')
               {
                  for (i = 0; i < coreopts->size; i++)
                  {
                     if (string_starts_with_size(
                              coreopts->opts[i].key, key, key_len - 1))
                     {
                        const char* val = core_option_manager_get_val(
                              coreopts, i);

                        if (val)
                        {
                           if (rcheevos_match_value(
                                    val, disallowed_setting->value))
                           {
                              key     = coreopts->opts[i].key;
                              allowed = 0;
                              break;
                           }
                        }
                     }
                  }
               }
               else
               {
                  for (i = 0; i < coreopts->size; i++)
                  {
                     if (string_is_equal(coreopts->opts[i].key, key))
                     {
                        val = core_option_manager_get_val(coreopts, i);
                        if (rcheevos_match_value(val, disallowed_setting->value))
                        {
                           allowed = 0;
                           break;
                        }
                     }
                  }
               }

               if (!allowed)
               {
                  char buffer[256];
                  snprintf(buffer, sizeof(buffer), "Hardcore paused. Setting not allowed: %s=%s", key, val);
                  CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", buffer);
                  rcheevos_pause_hardcore();

                  runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);

                  break;
               }
            }
         }

         break;
      }

      ++core_filter;
   }
}

static void rcheevos_runtime_event_handler(const rc_runtime_event_t* runtime_event)
{
#if defined(HAVE_GFX_WIDGETS)
   bool widgets_ready = gfx_widgets_ready();
#else
   bool widgets_ready = false;
#endif

   switch (runtime_event->type)
   {
#if defined(HAVE_GFX_WIDGETS)
      case RC_RUNTIME_EVENT_LBOARD_UPDATED:
         rcheevos_lboard_updated(rcheevos_find_lboard(runtime_event->id), runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED:
         rcheevos_challenge_started(rcheevos_find_cheevo(runtime_event->id), runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED:
         rcheevos_challenge_ended(rcheevos_find_cheevo(runtime_event->id), runtime_event->value, widgets_ready);
         break;
#endif

      case RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED:
         rcheevos_award_achievement(&rcheevos_locals, rcheevos_find_cheevo(runtime_event->id), widgets_ready);
         break;

      case RC_RUNTIME_EVENT_LBOARD_STARTED:
         rcheevos_lboard_started(rcheevos_find_lboard(runtime_event->id), runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_LBOARD_CANCELED:
         rcheevos_lboard_canceled(rcheevos_find_lboard(runtime_event->id),
               widgets_ready);
         break;

      case RC_RUNTIME_EVENT_LBOARD_TRIGGERED:
         rcheevos_lboard_submit(&rcheevos_locals, rcheevos_find_lboard(runtime_event->id), runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED:
         rcheevos_achievement_disabled(rcheevos_find_cheevo(runtime_event->id), runtime_event->value);
         break;

      case RC_RUNTIME_EVENT_LBOARD_DISABLED:
         rcheevos_lboard_disabled(rcheevos_find_lboard(runtime_event->id), runtime_event->value);
         break;

      default:
         break;
   }
}

/*****************************************************************************
Test all the achievements (call once per frame).
*****************************************************************************/
void rcheevos_test(void)
{
#ifdef HAVE_THREADS
   if (rcheevos_locals.queued_command != CMD_EVENT_NONE)
   {
      command_event(rcheevos_locals.queued_command, NULL);
      rcheevos_locals.queued_command = CMD_EVENT_NONE;
   }
#endif

   if (!rcheevos_locals.loaded)
      return;

   if (rcheevos_locals.memory.count == 0)
   {
      /* we were unable to initialize memory earlier, try now */
      if (!rcheevos_memory_init(&rcheevos_locals.memory, rcheevos_locals.patchdata.console_id))
      {
         const settings_t* settings    = config_get_ptr();
         rcheevos_locals.core_supports = false;

         CHEEVOS_ERR(RCHEEVOS_TAG "No memory exposed by core\n");

         if (settings && settings->bools.cheevos_verbose_enable)
         {
            runloop_msg_queue_push(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
               0, 4 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
         }

         rcheevos_unload();
         rcheevos_pause_hardcore();
         return;
      }

      rcheevos_validate_memrefs(&rcheevos_locals);
   }

   rc_runtime_do_frame(&rcheevos_locals.runtime, &rcheevos_runtime_event_handler, rcheevos_peek, NULL, 0);
}

size_t rcheevos_get_serialize_size(void)
{
   if (!rcheevos_locals.loaded)
      return 0;
   return rc_runtime_progress_size(&rcheevos_locals.runtime, NULL);
}

bool rcheevos_get_serialized_data(void* buffer)
{
   if (!rcheevos_locals.loaded)
      return false;
   return (rc_runtime_serialize_progress(buffer, &rcheevos_locals.runtime, NULL) == RC_OK);
}

bool rcheevos_set_serialized_data(void* buffer)
{
   if (rcheevos_locals.loaded)
   {
      if (buffer && rc_runtime_deserialize_progress(&rcheevos_locals.runtime, (const unsigned char*)buffer, NULL) == RC_OK)
         return true;

      rc_runtime_reset(&rcheevos_locals.runtime);
   }

   return false;
}

void rcheevos_set_support_cheevos(bool state)
{
   rcheevos_locals.core_supports = state;
}

bool rcheevos_get_support_cheevos(void)
{
   return rcheevos_locals.core_supports;
}

const char* rcheevos_get_hash(void)
{
   return rcheevos_locals.hash;
}

static void rcheevos_unlock_cb(unsigned id, void* userdata)
{
   rcheevos_racheevo_t* cheevo = rcheevos_find_cheevo(id);
   if (cheevo)
   {
      unsigned mode = *(unsigned*)userdata;
#ifndef CHEEVOS_DONT_DEACTIVATE
      cheevo->active &= ~mode;
#endif

      if ((rcheevos_locals.hardcore_active && mode == RCHEEVOS_ACTIVE_HARDCORE) ||
            (!rcheevos_locals.hardcore_active && mode == RCHEEVOS_ACTIVE_SOFTCORE))
      {
         rc_runtime_deactivate_achievement(&rcheevos_locals.runtime, cheevo->id);
         CHEEVOS_LOG(RCHEEVOS_TAG "Achievement %u deactivated: %s\n", id, cheevo->title);
      }
   }
}

#include "coro.h"

/* Uncomment the following two lines to debug rcheevos_iterate, this will
 * disable the coroutine yielding.
 *
 * The code is very easy to understand. It's meant to be like BASIC:
 * CORO_GOTO will jump execution to another label, CORO_GOSUB will
 * call another label, and CORO_RET will return from a CORO_GOSUB.
 *
 * This coroutine code is inspired in a very old pure C implementation
 * that runs everywhere:
 *
 * https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
 */
/*#undef CORO_YIELD
#define CORO_YIELD()*/

typedef struct
{
   /* variables used in the co-routine */
   char badge_name[16];
   char url[256];
   char badge_basepath[PATH_MAX_LENGTH];
   char badge_fullpath[PATH_MAX_LENGTH];
   char hash[33];
   unsigned gameid;
   unsigned i;
   unsigned j;
   unsigned k;
   size_t len;
   retro_time_t t0;
   void *data;
   char *json;
   const char *path;
   rcheevos_racheevo_t *cheevo;
   const rcheevos_racheevo_t *cheevo_end;
   settings_t *settings;
   struct http_connection_t *conn;
   struct http_t *http;
   struct rc_hash_iterator iterator;

   /* co-routine required fields */
   CORO_FIELDS
} rcheevos_coro_t;

enum
{
   /* Negative values because CORO_SUB generates positive values */
   RCHEEVOS_GET_GAMEID   = -1,
   RCHEEVOS_GET_CHEEVOS  = -2,
   RCHEEVOS_GET_BADGES   = -3,
   RCHEEVOS_LOGIN        = -4,
   RCHEEVOS_HTTP_GET     = -5,
   RCHEEVOS_DEACTIVATE   = -6,
   RCHEEVOS_PLAYING      = -7,
   RCHEEVOS_DELAY        = -8
};

static int rcheevos_iterate(rcheevos_coro_t* coro)
{
   char buffer[2048];
   bool ret;
#ifdef CHEEVOS_TIME_HASH
   retro_time_t start;
#endif

   CORO_ENTER();

      coro->settings = config_get_ptr();

      /* Bail out if cheevos are disabled.
         * But set the above anyways,
         * command_read_ram needs it. */
      if (!coro->settings->bools.cheevos_enable)
         CORO_STOP();

      /* iterate over the possible hashes for the file being loaded */
      rc_hash_initialize_iterator(&coro->iterator, coro->path, (uint8_t*)coro->data, coro->len);
#ifdef CHEEVOS_TIME_HASH
      start = cpu_features_get_time_usec();
#endif
      while (rc_hash_iterate(coro->hash, &coro->iterator))
      {
#ifdef CHEEVOS_TIME_HASH
         CHEEVOS_LOG(RCHEEVOS_TAG "hash generated in %ums\n", (cpu_features_get_time_usec() - start) / 1000);
#endif
         CORO_GOSUB(RCHEEVOS_GET_GAMEID);
         if (coro->gameid != 0)
            break;

#ifdef CHEEVOS_TIME_HASH
         start = cpu_features_get_time_usec();
#endif
      }
      rc_hash_destroy_iterator(&coro->iterator);

      /* if no match was found, bail */
      if (coro->gameid == 0)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "this game doesn't feature achievements\n");
         strlcpy(rcheevos_locals.hash, "N/A", sizeof(rcheevos_locals.hash));
         rcheevos_pause_hardcore();
         CORO_STOP();
      }

#ifdef CHEEVOS_JSON_OVERRIDE
      {
         size_t size = 0;
         FILE *file  = fopen(CHEEVOS_JSON_OVERRIDE, "rb");

         fseek(file, 0, SEEK_END);
         size = ftell(file);
         fseek(file, 0, SEEK_SET);

         coro->json = (char*)malloc(size + 1);
         fread((void*)coro->json, 1, size, file);

         fclose(file);
         coro->json[size] = 0;
      }
#else
      CORO_GOSUB(RCHEEVOS_GET_CHEEVOS);

      if (!coro->json)
      {
         runloop_msg_queue_push("Error loading achievements.", 0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         CHEEVOS_ERR(RCHEEVOS_TAG "error loading achievements\n");
         CORO_STOP();
      }
#endif

#ifdef CHEEVOS_SAVE_JSON
      {
         FILE *file = fopen(CHEEVOS_SAVE_JSON, "w");
         fwrite((void*)coro->json, 1, strlen(coro->json), file);
         fclose(file);
      }
#endif

#if HAVE_REWIND
      if (!rcheevos_locals.hardcore_active)
      {
         /* deactivate rewind while we activate the achievements */
         if (coro->settings->bools.rewind_enable)
         {
#ifdef HAVE_THREADS
            /* have to "schedule" this. CMD_EVENT_REWIND_DEINIT should only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_DEINIT;

            /* wait for rewind to be disabled */
            while (rcheevos_locals.queued_command != CMD_EVENT_NONE)
            {
               CORO_YIELD();
            }
#else
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
#endif
         }
      }
#endif

      ret = rcheevos_parse(&rcheevos_locals, coro->json);
      CHEEVOS_FREE(coro->json);

      if (ret == 0)
      {
         if (     rcheevos_locals.patchdata.core_count       == 0
               && rcheevos_locals.patchdata.unofficial_count == 0
               && rcheevos_locals.patchdata.lboard_count     == 0
            )
         {
            runloop_msg_queue_push(
                  "This game has no achievements.",
                  0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

            rcheevos_pause_hardcore();
         }
         else
            rcheevos_locals.loaded = true;
      }

#if HAVE_REWIND
      if (!rcheevos_locals.hardcore_active)
      {
         /* re-enable rewind. if rcheevos_locals.loaded is true, additional space will be allocated
          * for the achievement state data */
         if (coro->settings->bools.rewind_enable)
         {
#ifdef HAVE_THREADS
            /* have to "schedule" this. CMD_EVENT_REWIND_INIT should only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_INIT;
#else
            command_event(CMD_EVENT_REWIND_INIT, NULL);
#endif
         }
      }
#endif

      if (!rcheevos_locals.loaded)
      {
         /* parse failure or no achievements - nothing more to do */
         CORO_STOP();
      }

      /*
         * Inputs:  CHEEVOS_VAR_GAMEID
         * Outputs:
         */
      if (!coro->settings->bools.cheevos_start_active)
         CORO_GOSUB(RCHEEVOS_DEACTIVATE);

      /*
         * Inputs:  CHEEVOS_VAR_GAMEID
         * Outputs:
         */
      CORO_GOSUB(RCHEEVOS_PLAYING);

      if (coro->settings->bools.cheevos_verbose_enable && rcheevos_locals.patchdata.core_count > 0)
      {
         char msg[256];
         int mode                        = RCHEEVOS_ACTIVE_SOFTCORE;
         const rcheevos_racheevo_t* cheevo = rcheevos_locals.patchdata.core;
         const rcheevos_racheevo_t* end    = cheevo + rcheevos_locals.patchdata.core_count;
         int number_of_unlocked          = rcheevos_locals.patchdata.core_count;
         int number_of_unsupported       = 0;

         if (rcheevos_locals.hardcore_active)
            mode = RCHEEVOS_ACTIVE_HARDCORE;

         for (; cheevo < end; cheevo++)
         {
            if (!cheevo->memaddr)
               number_of_unsupported++;
            else if (cheevo->active & mode)
               number_of_unlocked--;
         }

         if (!number_of_unsupported)
         {
            if (coro->settings->bools.cheevos_start_active)
            {
               snprintf(msg, sizeof(msg),
                  "All %d achievements activated for this session.",
                  rcheevos_locals.patchdata.core_count);
               CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg);
            }
            else
            {
               snprintf(msg, sizeof(msg),
                  "You have %d of %d achievements unlocked.",
                  number_of_unlocked, rcheevos_locals.patchdata.core_count);
               CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", &msg[9]);
            }
         }
         else
         {
            if (coro->settings->bools.cheevos_start_active)
            {
               snprintf(msg, sizeof(msg),
                  "All %d achievements activated for this session (%d unsupported).",
                  rcheevos_locals.patchdata.core_count,
                  number_of_unsupported);
               CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg);
            }
            else
            {
               snprintf(msg, sizeof(msg),
                     "You have %d of %d achievements unlocked (%d unsupported).",
                     number_of_unlocked - number_of_unsupported,
                     rcheevos_locals.patchdata.core_count,
                     number_of_unsupported);
               CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", &msg[9]);
            }
         }

         msg[sizeof(msg) - 1] = 0;
         runloop_msg_queue_push(msg, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      CORO_GOSUB(RCHEEVOS_GET_BADGES);
      CORO_STOP();


   /**************************************************************************
    * Info    Gets the achievements from Retro Achievements
    * Inputs  coro->hash
    * Outputs coro->gameid
    *************************************************************************/
   CORO_SUB(RCHEEVOS_GET_GAMEID)

      {
         int size;

         CHEEVOS_LOG(RCHEEVOS_TAG "checking %s\n", coro->hash);
         memcpy(rcheevos_locals.hash, coro->hash, sizeof(coro->hash));

         size = rc_url_get_gameid(coro->url, sizeof(coro->url), rcheevos_locals.hash);
         if (size < 0)
         {
            CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
            CORO_RET();
         }

         rcheevos_log_url("rc_url_get_gameid", coro->url);
         CORO_GOSUB(RCHEEVOS_HTTP_GET);

         if (!coro->json)
            CORO_RET();

         coro->gameid = chevos_get_gameid(coro->json);

         CHEEVOS_FREE(coro->json);
         CHEEVOS_LOG(RCHEEVOS_TAG "got game id %u\n", coro->gameid);
         CORO_RET();
      }


   /**************************************************************************
    * Info    Gets the achievements from Retro Achievements
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs CHEEVOS_VAR_JSON
    *************************************************************************/
   CORO_SUB(RCHEEVOS_GET_CHEEVOS)
   {
      int ret;

      CORO_GOSUB(RCHEEVOS_LOGIN);

      ret = rc_url_get_patch(coro->url, sizeof(coro->url), coro->settings->arrays.cheevos_username, rcheevos_locals.token, coro->gameid);

      if (ret < 0)
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
         CORO_STOP();
      }

      rcheevos_log_url("rc_url_get_patch", coro->url);
      CORO_GOSUB(RCHEEVOS_HTTP_GET);

      if (!coro->json)
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "error getting achievements for game id %u\n", coro->gameid);
         CORO_STOP();
      }

      CHEEVOS_LOG(RCHEEVOS_TAG "got achievements for game id %u\n", coro->gameid);
      CORO_RET();
   }


   /**************************************************************************
    * Info    Gets the achievements from Retro Achievements
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs CHEEVOS_VAR_JSON
    *************************************************************************/
   CORO_SUB(RCHEEVOS_GET_BADGES)

   /* we always want badges if display widgets are enabled */
#if !defined(HAVE_GFX_WIDGETS)
   {
      settings_t *settings = config_get_ptr();
      if (!(
               string_is_equal(settings->arrays.menu_driver, "xmb") ||
               string_is_equal(settings->arrays.menu_driver, "ozone")
           ) ||
            !settings->bools.cheevos_badges_enable)
         CORO_RET();
   }
#endif

      /* make sure the directory exists */
      coro->badge_fullpath[0] = '\0';
      fill_pathname_application_special(coro->badge_fullpath,
         sizeof(coro->badge_fullpath),
         APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

      if (!path_is_directory(coro->badge_fullpath))
         path_mkdir(coro->badge_fullpath);

      /* fetch the placeholder image */
      strlcpy(coro->badge_name, "00000" FILE_PATH_PNG_EXTENSION,
            sizeof(coro->badge_name));
      fill_pathname_join(coro->badge_fullpath, coro->badge_fullpath,
         coro->badge_name, sizeof(coro->badge_fullpath));

      if (!path_is_valid(coro->badge_fullpath))
      {
#ifdef CHEEVOS_LOG_BADGES
         CHEEVOS_LOG(RCHEEVOS_TAG "downloading badge %s\n",
            coro->badge_fullpath);
#endif
         snprintf(coro->url, sizeof(coro->url),
            FILE_PATH_RETROACHIEVEMENTS_URL "/Badge/%s", coro->badge_name);

         CORO_GOSUB(RCHEEVOS_HTTP_GET);

         if (coro->json)
         {
            if (!filestream_write_file(coro->badge_fullpath, coro->json, coro->k))
               CHEEVOS_ERR(RCHEEVOS_TAG "Error writing badge %s\n", coro->badge_fullpath);

            CHEEVOS_FREE(coro->json);
            coro->json = NULL;
         }
      }

      /* fetch the game images */
      for (coro->i = 0; coro->i < 2; coro->i++)
      {
         if (coro->i == 0)
         {
            coro->cheevo     = rcheevos_locals.patchdata.core;
            coro->cheevo_end = coro->cheevo + rcheevos_locals.patchdata.core_count;
         }
         else
         {
            coro->cheevo     = rcheevos_locals.patchdata.unofficial;
            coro->cheevo_end = coro->cheevo + rcheevos_locals.patchdata.unofficial_count;
         }

         for (; coro->cheevo < coro->cheevo_end; coro->cheevo++)
         {
            if (!coro->cheevo->badge || !coro->cheevo->badge[0])
               continue;

            for (coro->j = 0 ; coro->j < 2; coro->j++)
            {
               CORO_YIELD();

               if (coro->j == 0)
                  snprintf(coro->badge_name,
                        sizeof(coro->badge_name),
                        "%s" FILE_PATH_PNG_EXTENSION,
                        coro->cheevo->badge);
               else
                  snprintf(coro->badge_name,
                        sizeof(coro->badge_name),
                        "%s_lock" FILE_PATH_PNG_EXTENSION,
                        coro->cheevo->badge);

               coro->badge_fullpath[0] = '\0';
               fill_pathname_application_special(coro->badge_fullpath,
                     sizeof(coro->badge_fullpath),
                     APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

               fill_pathname_join(
                     coro->badge_fullpath,
                     coro->badge_fullpath,
                     coro->badge_name,
                     sizeof(coro->badge_fullpath));

               if (!path_is_valid(coro->badge_fullpath))
               {
#ifdef CHEEVOS_LOG_BADGES
                  CHEEVOS_LOG(
                        RCHEEVOS_TAG "downloading badge %s\n",
                        coro->badge_fullpath);
#endif
                  snprintf(coro->url,
                        sizeof(coro->url),
                        FILE_PATH_RETROACHIEVEMENTS_URL "/Badge/%s",
                        coro->badge_name);

                  CORO_GOSUB(RCHEEVOS_HTTP_GET);

                  if (coro->json)
                  {
                     if (!filestream_write_file(coro->badge_fullpath,
                              coro->json, coro->k))
                        CHEEVOS_ERR(RCHEEVOS_TAG "Error writing badge %s\n", coro->badge_fullpath);

                     CHEEVOS_FREE(coro->json);
                     coro->json = NULL;
                  }
               }
            }
         }
      }

      CORO_RET();


   /**************************************************************************
    * Info Logs in the user at Retro Achievements
    *************************************************************************/
   CORO_SUB(RCHEEVOS_LOGIN)
   {
      int ret;
      char tok[256];

      if (rcheevos_locals.token[0])
         CORO_RET();

      if (string_is_empty(coro->settings->arrays.cheevos_username))
      {
         runloop_msg_queue_push(
               "Missing RetroAchievements account information.",
               0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         runloop_msg_queue_push(
               "Please fill in your account information in Settings.",
               0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         CHEEVOS_ERR(RCHEEVOS_TAG "login info not informed\n");
         CORO_STOP();
      }

      if (string_is_empty(coro->settings->arrays.cheevos_token))
      {
         ret = rc_url_login_with_password(coro->url, sizeof(coro->url),
               coro->settings->arrays.cheevos_username,
               coro->settings->arrays.cheevos_password);

         if (ret == RC_OK)
         {
            CHEEVOS_LOG(RCHEEVOS_TAG "attempting to login %s (with password)\n", coro->settings->arrays.cheevos_username);
            rcheevos_log_url("rc_url_login_with_password", coro->url);
         }
      }
      else
      {
         ret = rc_url_login_with_token(coro->url, sizeof(coro->url),
               coro->settings->arrays.cheevos_username,
               coro->settings->arrays.cheevos_token);

         if (ret == RC_OK)
         {
            CHEEVOS_LOG(RCHEEVOS_TAG "attempting to login %s (with token)\n", coro->settings->arrays.cheevos_username);
            rcheevos_log_url("rc_url_login_with_token", coro->url);
         }
      }

      if (ret < 0)
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
         CORO_STOP();
      }

      CORO_GOSUB(RCHEEVOS_HTTP_GET);

      if (!coro->json)
      {
         runloop_msg_queue_push("RetroAchievements: Error contacting server.", 0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         CHEEVOS_ERR(RCHEEVOS_TAG "error getting user token\n");

         CORO_STOP();
      }

      ret = rcheevos_get_token(coro->json, tok, sizeof(tok));

      if (ret != 0)
      {
         char msg[512];
         snprintf(msg, sizeof(msg),
               "RetroAchievements: %s",
               tok);
         runloop_msg_queue_push(msg, 0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         *coro->settings->arrays.cheevos_token = 0;
         CHEEVOS_ERR(RCHEEVOS_TAG "login error: %s\n", tok);

         CHEEVOS_FREE(coro->json);
         CORO_STOP();
      }

      CHEEVOS_FREE(coro->json);

      if (coro->settings->bools.cheevos_verbose_enable)
      {
         char msg[256];
         snprintf(msg, sizeof(msg),
               "RetroAchievements: Logged in as \"%s\".",
               coro->settings->arrays.cheevos_username);
         msg[sizeof(msg) - 1] = 0;
         runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      CHEEVOS_LOG(RCHEEVOS_TAG "logged in successfully\n");
      strlcpy(rcheevos_locals.token, tok,
            sizeof(rcheevos_locals.token));

      /* Save token to config and clear pass on success */
      strlcpy(coro->settings->arrays.cheevos_token, tok,
            sizeof(coro->settings->arrays.cheevos_token));

      *coro->settings->arrays.cheevos_password = 0;
      CORO_RET();
   }


   /**************************************************************************
    * Info    Pauses execution for five seconds
    *************************************************************************/
   CORO_SUB(RCHEEVOS_DELAY)

      {
         retro_time_t t1;
         coro->t0         = cpu_features_get_time_usec();

         do
         {
            CORO_YIELD();
            t1 = cpu_features_get_time_usec();
         } while((t1 - coro->t0) < 3000000);
      }

      CORO_RET();


   /**************************************************************************
    * Info    Makes a HTTP GET request
    * Inputs  CHEEVOS_VAR_URL
    * Outputs CHEEVOS_VAR_JSON
    *************************************************************************/
   CORO_SUB(RCHEEVOS_HTTP_GET)

      for (coro->k = 0; coro->k < 5; coro->k++)
      {
         if (coro->k != 0)
            CHEEVOS_LOG(RCHEEVOS_TAG "Retrying HTTP request: %u of 5\n", coro->k + 1);

         coro->json       = NULL;
         coro->conn       = net_http_connection_new(
               coro->url, "GET", NULL);

         if (!coro->conn)
         {
            CORO_GOSUB(RCHEEVOS_DELAY);
            continue;
         }

         /* Don't bother with timeouts here, it's just a string scan. */
         while (!net_http_connection_iterate(coro->conn)) {}

         /* Error finishing the connection descriptor. */
         if (!net_http_connection_done(coro->conn))
         {
            net_http_connection_free(coro->conn);
            continue;
         }

         rcheevos_get_user_agent(&rcheevos_locals,
               buffer, sizeof(buffer));
         net_http_connection_set_user_agent(coro->conn, buffer);

         coro->http = net_http_new(coro->conn);

         /* Error connecting to the endpoint. */
         if (!coro->http)
         {
            net_http_connection_free(coro->conn);
            CORO_GOSUB(RCHEEVOS_DELAY);
            continue;
         }

         while (!net_http_update(coro->http, NULL, NULL))
            CORO_YIELD();

         {
            size_t length;
            uint8_t *data = net_http_data(coro->http,
                  &length, false);

            if (data)
            {
               coro->json = (char*)malloc(length + 1);

               if (coro->json)
               {
                  memcpy((void*)coro->json, (void*)data, length);
                  CHEEVOS_FREE(data);
                  coro->json[length] = 0;
               }

               coro->k = (unsigned)length;
               net_http_delete(coro->http);
               net_http_connection_free(coro->conn);
               CORO_RET();
            }
         }

         net_http_delete(coro->http);
         net_http_connection_free(coro->conn);
      }

      CHEEVOS_LOG(RCHEEVOS_TAG "Couldn't connect to server after 5 tries\n");
      CORO_RET();


   /**************************************************************************
    * Info    Deactivates the achievements already awarded
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs
    *************************************************************************/
   CORO_SUB(RCHEEVOS_DEACTIVATE)

      CORO_GOSUB(RCHEEVOS_LOGIN);
      {
         int ret;
         unsigned mode;

         /* Two calls - one for softcore and one for hardcore */
         for (coro->i = 0; coro->i < 2; coro->i++)
         {
            ret = rc_url_get_unlock_list(coro->url, sizeof(coro->url),
                  coro->settings->arrays.cheevos_username,
                  rcheevos_locals.token, coro->gameid, coro->i);

            if (ret < 0)
            {
               CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
               CORO_STOP();
            }

            rcheevos_log_url("rc_url_get_unlock_list", coro->url);
            CORO_GOSUB(RCHEEVOS_HTTP_GET);

            if (coro->json)
            {
               mode = coro->i == 0 ? RCHEEVOS_ACTIVE_SOFTCORE : RCHEEVOS_ACTIVE_HARDCORE;
               rcheevos_deactivate_unlocks(coro->json, rcheevos_unlock_cb, &mode);
               CHEEVOS_FREE(coro->json);
            }
            else
               CHEEVOS_ERR(RCHEEVOS_TAG "error retrieving list of unlocked achievements in softcore mode\n");
         }
      }

      CORO_RET();


   /**************************************************************************
    * Info    Posts the "playing" activity to Retro Achievements
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs
    *************************************************************************/
   CORO_SUB(RCHEEVOS_PLAYING)

      {
         int ret = rc_url_post_playing(coro->url, sizeof(coro->url),
            coro->settings->arrays.cheevos_username,
            rcheevos_locals.token, coro->gameid);

         if (ret < 0)
         {
            CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
            CORO_STOP();
         }
      }

      rcheevos_log_url("rc_url_post_playing", coro->url);

      CORO_GOSUB(RCHEEVOS_HTTP_GET);

      if (coro->json)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Posted playing activity\n");
         CHEEVOS_FREE(coro->json);
      }
      else
         CHEEVOS_ERR(RCHEEVOS_TAG "error posting playing activity\n");

      CORO_RET();

   CORO_LEAVE();
}

static void rcheevos_task_handler(retro_task_t *task)
{
   rcheevos_coro_t *coro = (rcheevos_coro_t*)task->state;

   if (!coro)
      return;

   if (!rcheevos_iterate(coro) || task_get_cancelled(task))
   {
      task_set_finished(task, true);

      CHEEVOS_LOCK(rcheevos_locals.task_lock);
      rcheevos_locals.task = NULL;
      CHEEVOS_UNLOCK(rcheevos_locals.task_lock);

      if (task_get_cancelled(task))
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Load task cancelled\n");
      }
      else
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Load task finished\n");
      }

      CHEEVOS_FREE(coro->data);
      CHEEVOS_FREE(coro->path);
      CHEEVOS_FREE(coro);
   }
}

/* hooks for rc_hash library */

static void* rc_hash_handle_file_open(const char* path)
{
   return intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static void rc_hash_handle_file_seek(void* file_handle, int64_t offset, int origin)
{
   intfstream_seek((intfstream_t*)file_handle, offset, origin);
}

static int64_t rc_hash_handle_file_tell(void* file_handle)
{
   return intfstream_tell((intfstream_t*)file_handle);
}

static size_t rc_hash_handle_file_read(void* file_handle, void* buffer, size_t requested_bytes)
{
   return intfstream_read((intfstream_t*)file_handle, buffer, requested_bytes);
}

static void rc_hash_handle_file_close(void* file_handle)
{
   intfstream_close((intfstream_t*)file_handle);
   CHEEVOS_FREE(file_handle);
}

static void* rc_hash_handle_cd_open_track(const char* path, uint32_t track)
{
   cdfs_track_t* cdfs_track;

   switch (track)
   {
      case RC_HASH_CDTRACK_FIRST_DATA:
         cdfs_track = cdfs_open_data_track(path);
         break;

      case RC_HASH_CDTRACK_LAST:
#ifdef HAVE_CHD
         if (string_is_equal_noncase(path_get_extension(path), "chd"))
         {
            cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_LAST);
            break;
         }
#endif
         CHEEVOS_LOG(RCHEEVOS_TAG "Last track only supported for CHD\n");
         cdfs_track = NULL;
         break;

      case RC_HASH_CDTRACK_LARGEST:
#ifdef HAVE_CHD
         if (string_is_equal_noncase(path_get_extension(path), "chd"))
         {
            cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_PRIMARY);
            break;
         }
#endif
         CHEEVOS_LOG(RCHEEVOS_TAG "Largest track only supported for CHD, using first data track\n");
         cdfs_track = cdfs_open_data_track(path);
         break;

      default:
         cdfs_track = cdfs_open_track(path, track);
         break;
   }

   if (cdfs_track)
   {
      cdfs_file_t* file = (cdfs_file_t*)malloc(sizeof(cdfs_file_t));
      if (cdfs_open_file(file, cdfs_track, NULL))
         return file; /* ASSERT: file owns cdfs_track now */

      CHEEVOS_FREE(file);
      cdfs_close_track(cdfs_track); /* ASSERT: this free()s cdfs_track */
   }

   return NULL;
}

static size_t rc_hash_handle_cd_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
   cdfs_file_t* file = (cdfs_file_t*)track_handle;

   cdfs_seek_sector(file, sector);
   return cdfs_read_file(file, buffer, requested_bytes);
}

static void rc_hash_handle_cd_close_track(void* track_handle)
{
   cdfs_file_t* file = (cdfs_file_t*)track_handle;
   if (file)
   {
      cdfs_close_track(file->track);
      cdfs_close_file(file); /* ASSERT: this does not free() file */
      CHEEVOS_FREE(file);
   }
}

static void rc_hash_handle_log_message(const char* message)
{
   CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", message);
}

/* end hooks */

bool rcheevos_load(const void *data)
{
   retro_task_t *task                 = NULL;
   const struct retro_game_info *info = NULL;
   rcheevos_coro_t *coro              = NULL;
   settings_t *settings               = config_get_ptr();
   bool cheevos_enable                = settings && settings->bools.cheevos_enable;
   struct rc_hash_filereader filereader;
   struct rc_hash_cdreader cdreader;

   rcheevos_locals.loaded             = false;
#ifdef HAVE_THREADS
   rcheevos_locals.queued_command     = CMD_EVENT_NONE;
#endif
   rc_runtime_init(&rcheevos_locals.runtime);

   if (!cheevos_enable || !rcheevos_locals.core_supports || !data)
   {
      rcheevos_pause_hardcore();
      return false;
   }

   /* reset hardcore mode and leaderboard settings based on configs */
   rcheevos_hardcore_enabled_changed();
   rcheevos_validate_config_settings();
   rcheevos_leaderboards_enabled_changed();

   coro = (rcheevos_coro_t*)calloc(1, sizeof(*coro));

   if (!coro)
      return false;

   /* provide hooks for reading files */
   memset(&filereader, 0, sizeof(filereader));
   filereader.open = rc_hash_handle_file_open;
   filereader.seek = rc_hash_handle_file_seek;
   filereader.tell = rc_hash_handle_file_tell;
   filereader.read = rc_hash_handle_file_read;
   filereader.close = rc_hash_handle_file_close;
   rc_hash_init_custom_filereader(&filereader);

   memset(&cdreader, 0, sizeof(cdreader));
   cdreader.open_track = rc_hash_handle_cd_open_track;
   cdreader.read_sector = rc_hash_handle_cd_read_sector;
   cdreader.close_track = rc_hash_handle_cd_close_track;
   rc_hash_init_custom_cdreader(&cdreader);

   rc_hash_init_error_message_callback(rc_hash_handle_log_message);

#ifndef DEBUG /* in DEBUG mode, always initialize the verbose message handler */
   if (settings->bools.cheevos_verbose_enable)
#endif
   {
      rc_hash_init_verbose_message_callback(rc_hash_handle_log_message);
   }

   task = task_init();
   if (!task)
   {
      CHEEVOS_FREE(coro);
      return false;
   }

   CORO_SETUP();

   info = (const struct retro_game_info*)data;
   coro->path = strdup(info->path);

   if (info->data)
   {
      coro->len = info->size;

      /* size limit */
      if (coro->len > CHEEVOS_MB(64))
         coro->len = CHEEVOS_MB(64);

      coro->data = malloc(coro->len);

      if (!coro->data)
      {
         CHEEVOS_FREE(task);
         CHEEVOS_FREE(coro);
         return false;
      }

      memcpy(coro->data, info->data, coro->len);
   }
   else
   {
      coro->data       = NULL;
   }

   task->handler   = rcheevos_task_handler;
   task->state     = (void*)coro;
   task->mute      = true;
   task->callback  = NULL;
   task->user_data = NULL;
   task->progress  = 0;
   task->title     = NULL;

#ifdef HAVE_THREADS
   if (!rcheevos_locals.task_lock)
      rcheevos_locals.task_lock = slock_new();
#endif

   CHEEVOS_LOCK(rcheevos_locals.task_lock);
   rcheevos_locals.task = task;
   CHEEVOS_UNLOCK(rcheevos_locals.task_lock);

   task_queue_push(task);
   return true;
}

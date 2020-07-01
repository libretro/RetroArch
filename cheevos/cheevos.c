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

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_entries.h"
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

#include "badges.h"
#include "cheevos.h"
#include "fixup.h"
#include "parser.h"
#include "hash.h"
#include "util.h"

#include "../file_path_special.h"
#include "../paths.h"
#include "../command.h"
#include "../dynamic.h"
#include "../configuration.h"
#include "../performance_counters.h"
#include "../msg_hash.h"
#include "../retroarch.h"
#include "../core.h"
#include "../version.h"

#include "../frontend/frontend_driver.h"
#include "../network/net_http_special.h"
#include "../tasks/tasks_internal.h"

#include "../deps/rcheevos/include/rcheevos.h"
#include "../deps/rcheevos/include/rurl.h"
#include "../deps/rcheevos/include/rhash.h"

/* Define this macro to prevent cheevos from being deactivated. */
#undef CHEEVOS_DONT_DEACTIVATE

/* Define this macro to dump all cheevos' addresses. */
#undef CHEEVOS_DUMP_ADDRS

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
   CHEEVOS_ASYNC_RICHPRESENCE,
   CHEEVOS_ASYNC_AWARD_ACHIEVEMENT,
   CHEEVOS_ASYNC_SUBMIT_LBOARD
};

typedef struct
{
   rc_trigger_t* trigger;
   const rcheevos_racheevo_t* info;
   int active;
   int last;
} rcheevos_cheevo_t;

typedef struct
{
   rc_lboard_t* lboard;
   const rcheevos_ralboard_t* info;
   bool active;
   int last_value;
   int format;
} rcheevos_lboard_t;

typedef struct
{
   rc_richpresence_t* richpresence;
   char evaluation[256];
   retro_time_t last_update;
} rcheevos_richpresence_t;

typedef struct rcheevos_async_io_request
{
   int id;
   int value;
   int attempt_count;
   char type;
   char hardcore;
   const char* success_message;
   const char* failure_message;
   char user_agent[256];
} rcheevos_async_io_request;

typedef struct
{
   retro_task_t* task;
#ifdef HAVE_THREADS
   slock_t* task_lock;
#endif

   bool core_supports;
   bool invalid_peek_address;

   rcheevos_rapatchdata_t patchdata;
   rcheevos_cheevo_t* core;
   rcheevos_cheevo_t* unofficial;
   rcheevos_lboard_t* lboards;
   rcheevos_richpresence_t richpresence;

   rcheevos_fixups_t fixups;

   char token[32];
   char hash[33];
} rcheevos_locals_t;

static rcheevos_locals_t rcheevos_locals =
{
   NULL, /* task */
#ifdef HAVE_THREADS
   NULL, /* task_lock */
#endif
   true, /* core_supports */
   false,/* invalid_peek_address */
   {0},  /* patchdata */
   NULL, /* core */
   NULL, /* unofficial */
   NULL, /* lboards */
   {0},  /* rich presence */
   {0},  /* fixups */
   {0},  /* token */
   "N/A",/* hash */
};

/* TODO/FIXME - public global variables */
bool rcheevos_loaded                 = false;
bool rcheevos_hardcore_active        = false;
bool rcheevos_hardcore_paused        = false;
bool rcheevos_state_loaded_flag      = false;
char rcheevos_user_agent_prefix[128] = "";

#ifdef HAVE_THREADS
#define CHEEVOS_LOCK(l)   do { slock_lock(l); } while (0)
#define CHEEVOS_UNLOCK(l) do { slock_unlock(l); } while (0)
#else
#define CHEEVOS_LOCK(l)
#define CHEEVOS_UNLOCK(l)
#endif

#define CHEEVOS_MB(x)   ((x) * 1024 * 1024)

/*****************************************************************************
Supporting functions.
*****************************************************************************/

#ifndef CHEEVOS_VERBOSE
void rcheevos_log(const char *fmt, ...)
{
   (void)fmt;
}
#endif

static void rcheevos_get_user_agent(char* buffer)
{
   struct retro_system_info *system = runloop_get_libretro_system_info();
   const char* scan;
   char* ptr;

   if (!rcheevos_user_agent_prefix[0])
   {
      const frontend_ctx_driver_t *frontend = frontend_get_ptr();
      int major, minor;
      char tmp[64];

      ptr = rcheevos_user_agent_prefix + sprintf(rcheevos_user_agent_prefix, "RetroArch/%s", PACKAGE_VERSION);

      if (frontend && frontend->get_os)
      {
         frontend->get_os(tmp, sizeof(tmp), &major, &minor);
         ptr += sprintf(ptr, " (%s %d.%d)", tmp, major, minor);
      }
   }

   ptr = buffer + sprintf(buffer, "%s", rcheevos_user_agent_prefix);

   if (system && !string_is_empty(system->library_name))
   {
      const char* path = path_get(RARCH_PATH_CORE);
      if (!string_is_empty(path))
      {
         sprintf(ptr, " %s", path_basename(path));
         path_remove_extension(ptr);
         ptr += strlen(ptr);
      }
      else
      {
         *ptr++ = ' ';

         scan = system->library_name;
         while (*scan)
         {
            if (*scan == ' ')
            {
               *ptr++ = '_';
               ++scan;
            }
            else
               *ptr++ = *scan++;
         }
      }

      if (system->library_version)
      {
         *ptr++ = '/';

         scan = system->library_version;
         while (*scan)
         {
            if (*scan == ' ')
            {
               *ptr++ = '_';
               ++scan;
            }
            else
               *ptr++ = *scan++;
         }
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
      start = url;
   else
      ++start;

   do
   {
      next = strchr(start, '&');

      if (start[param_len] == '=' && memcmp(start, param, param_len) == 0)
      {
         if (next)
            strcpy(start, next + 1);
         else if (start > url)
            start[-1] = '\0';
         else
            *start = '\0';

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

static unsigned rcheevos_peek(unsigned address, unsigned num_bytes, void* ud)
{
   const uint8_t* data = rcheevos_fixup_find(&rcheevos_locals.fixups,
      address, rcheevos_locals.patchdata.console_id);
   unsigned      value = 0;

   if (data)
   {
      switch (num_bytes)
      {
         case 4:
            value |= data[2] << 16 | data[3] << 24;
         case 2:
            value |= data[1] << 8;
         case 1:
            value |= data[0];
      }
   }
   else
      rcheevos_locals.invalid_peek_address = true;

   return value;
}


static void rcheevos_async_award_achievement(rcheevos_async_io_request* request);
static void rcheevos_async_submit_lboard(rcheevos_async_io_request* request);

static retro_time_t rcheevos_async_send_rich_presence(
      rcheevos_async_io_request* request)
{
   settings_t *settings             = config_get_ptr();
   const char *cheevos_username     = settings->arrays.cheevos_username;
   bool cheevos_richpresence_enable = settings->bools.cheevos_richpresence_enable;

   if (cheevos_richpresence_enable && rcheevos_locals.richpresence.richpresence)
   {
      rc_evaluate_richpresence(rcheevos_locals.richpresence.richpresence,
         rcheevos_locals.richpresence.evaluation,
         sizeof(rcheevos_locals.richpresence.evaluation), rcheevos_peek, NULL, NULL);
   }

   {
      char url[256], post_data[1024];
      int ret = rc_url_ping(url, sizeof(url), post_data, sizeof(post_data),
         cheevos_username, rcheevos_locals.token, rcheevos_locals.patchdata.game_id,
         rcheevos_locals.richpresence.evaluation);

      if (ret < 0)
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
      }
      else
      {
         rcheevos_log_post_url("rc_url_ping", url, post_data);

         rcheevos_get_user_agent(request->user_agent);
         task_push_http_post_transfer_with_user_agent(url, post_data, true, "POST", request->user_agent, NULL, NULL);
      }
   }

#ifdef HAVE_DISCORD
   if (rcheevos_locals.richpresence.evaluation[0])
   {
      if (settings->bools.discord_enable
            && discord_is_ready())
         discord_update(DISCORD_PRESENCE_RETROACHIEVEMENTS);
   }
#endif

   /* Update rich presence every two minutes */
   if (settings->bools.cheevos_richpresence_enable)
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
            task->when = rcheevos_async_send_rich_presence(request);
         else
         {
            /* game changed; stop the recurring task - a new one will 
             * be scheduled for the next game */
            task_set_finished(task, 1);
            free(request);
         }
         break;

      case CHEEVOS_ASYNC_AWARD_ACHIEVEMENT:
         rcheevos_async_award_achievement(request);
         task_set_finished(task, 1);
         break;

      case CHEEVOS_ASYNC_SUBMIT_LBOARD:
         rcheevos_async_submit_lboard(request);
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
   rcheevos_async_io_request* request = (rcheevos_async_io_request*)user_data;

   if (!error)
   {
      char buffer[224];
      const http_transfer_data_t* data = (http_transfer_data_t*)task->task_data;
      if (rcheevos_get_json_error(data->data, buffer, sizeof(buffer)) == RC_OK)
      {
         char errbuf[256];
         snprintf(errbuf, sizeof(errbuf), "%s %u: %s", request->failure_message, request->id, buffer);
         CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", errbuf);

         switch (request->type)
         {
            case CHEEVOS_ASYNC_RICHPRESENCE:
               /* don't bother informing user when rich presence update fails */
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
      /* double the wait between each attempt until we hit a maximum delay of two minutes
      * 250ms -> 500ms -> 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 64s -> 120s -> 120s... */
      retro_time_t retry_delay = (request->attempt_count > 8) ? (120 * 1000 * 1000) : ((250 * 1000) << request->attempt_count);

      request->attempt_count++;
      rcheevos_async_schedule(request, retry_delay);

      CHEEVOS_ERR(RCHEEVOS_TAG "%s %u: %s\n", request->failure_message, request->id, error);
   }
}

static int rcheevos_parse(const char* json)
{
   char buffer[256];
   settings_t *settings      = config_get_ptr();
   int res                   = 0;
   int i                     = 0;
   unsigned j                = 0;
   unsigned count            = 0;
   rcheevos_cheevo_t* cheevo = NULL;
   rcheevos_lboard_t* lboard = NULL;
   rcheevos_racheevo_t* rac  = NULL;

   rcheevos_fixup_init(&rcheevos_locals.fixups);

   res = rcheevos_get_patchdata(json, &rcheevos_locals.patchdata);

   if (res != 0)
   {
      char* ptr = buffer + snprintf(buffer, sizeof(buffer), "Error retrieving achievement data: ");

      /* extract the Error field from the JSON. if not found, remove the colon from the message */
      if (rcheevos_get_json_error(json, ptr, sizeof(buffer) - (ptr - buffer)) == -1)
         ptr[-2] = '\0';

      runloop_msg_queue_push(buffer, 0, 5 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);

      RARCH_ERR(RCHEEVOS_TAG "%s", buffer);
      return -1;
   }

   if (   rcheevos_locals.patchdata.core_count == 0
       && rcheevos_locals.patchdata.unofficial_count == 0
       && rcheevos_locals.patchdata.lboard_count == 0)
   {
      rcheevos_locals.core                      = NULL;
      rcheevos_locals.unofficial                = NULL;
      rcheevos_locals.lboards                   = NULL;
      rcheevos_locals.richpresence.richpresence = NULL;
      rcheevos_free_patchdata(&rcheevos_locals.patchdata);
      return 0;
   }

   /* Achievement memory accesses are 0-based, regardless of 
    * where the memory is accessed by the
    * emulated code. As such, address 0 should always be 
    * accessible and serves as an indicator that
    * other addresses will also be accessible. 
    * Individual achievements will be "Unsupported" if
    * they contain addresses that cannot be resolved. 
    * This check gives the user immediate feedback
    * if the core they're trying to use will disable all 
    * achievements as "Unsupported".
    */
   if (!rcheevos_patch_address(0, rcheevos_locals.patchdata.console_id))
   {
      int delay_judgment          = 0;
      rarch_system_info_t* system = runloop_get_system_info();

      if (system->mmaps.num_descriptors == 0)
      {
         /* Special case: the mupen64plus-nx core doesn't 
          * initialize the RAM immediately. To avoid a race
          * condition - if the core says there's SYSTEM_RAM, 
          * but the pointer is NULL, proceed. If the memory
          * isn't exposed when the achievements start processing, 
          * they'll be marked "Unsupported" individually.
          */
         retro_ctx_memory_info_t meminfo;
         meminfo.id = RETRO_MEMORY_SYSTEM_RAM;
         core_get_memory(&meminfo);

         delay_judgment |= (meminfo.size > 0);
      }
      else
      {
         /* Special case: the sameboy core exposes the RAM 
          * at $8000, but not the ROM at $0000. NES and
          * Gameboy achievements do attempt to map the 
          * entire bus, and it's unlikely that an achievement
          * will reference the ROM data, so if the RAM is 
          * still present, allow the core to load. If any
          * achievements do reference the ROM data, they'll 
          * be marked "Unsupported" individually.
          */
         delay_judgment |= (rcheevos_patch_address(0x8000, rcheevos_locals.patchdata.console_id) != NULL);
      }

      if (!delay_judgment)
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "No memory exposed by core\n");

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push("Cannot activate achievements using this core.", 0, 4 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);

         goto error;
      }
   }

   /* Allocate memory. */
   rcheevos_locals.core = (rcheevos_cheevo_t*)
      calloc(rcheevos_locals.patchdata.core_count, sizeof(rcheevos_cheevo_t));

   rcheevos_locals.unofficial = (rcheevos_cheevo_t*)
      calloc(rcheevos_locals.patchdata.unofficial_count, sizeof(rcheevos_cheevo_t));

   rcheevos_locals.lboards = (rcheevos_lboard_t*)
      calloc(rcheevos_locals.patchdata.lboard_count, sizeof(rcheevos_lboard_t));

   if (   !rcheevos_locals.core
       || !rcheevos_locals.unofficial
       || !rcheevos_locals.lboards)
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "Error allocating memory for cheevos");
      goto error;
   }

   /* Initialize. */
   for (i = 0; i < 2; i++)
   {
      if (i == 0)
      {
         cheevo = rcheevos_locals.core;
         rac    = rcheevos_locals.patchdata.core;
         count  = rcheevos_locals.patchdata.core_count;
      }
      else
      {
         cheevo = rcheevos_locals.unofficial;
         rac    = rcheevos_locals.patchdata.unofficial;
         count  = rcheevos_locals.patchdata.unofficial_count;
      }

      for (j = 0; j < count; j++, cheevo++, rac++)
      {
         cheevo->info = rac;
         res          = rc_trigger_size(cheevo->info->memaddr);

         if (res < 0)
         {
            snprintf(buffer, sizeof(buffer),
                  "Error in achievement %d \"%s\": %s",
                  cheevo->info->id, cheevo->info->title, rc_error_str(res));

            if (settings->bools.cheevos_verbose_enable)
               runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

            CHEEVOS_ERR(RCHEEVOS_TAG "%s: mem %s\n", buffer, cheevo->info->memaddr);
            cheevo->trigger = NULL;
            cheevo->active  = 0;
            cheevo->last    = 1;
            continue;
         }

         cheevo->trigger = (rc_trigger_t*)calloc(1, res);

         if (!cheevo->trigger)
         {
            CHEEVOS_ERR(RCHEEVOS_TAG "Error allocating memory for cheevos");
            goto error;
         }

         rc_parse_trigger(cheevo->trigger, cheevo->info->memaddr, NULL, 0);
         cheevo->active = RCHEEVOS_ACTIVE_SOFTCORE | RCHEEVOS_ACTIVE_HARDCORE;
         cheevo->last   = 1;
      }
   }

   lboard = rcheevos_locals.lboards;
   count = rcheevos_locals.patchdata.lboard_count;

   for (j = 0; j < count; j++, lboard++)
   {
      lboard->info = rcheevos_locals.patchdata.lboards + j;
      res          = rc_lboard_size(lboard->info->mem);

      if (res < 0)
      {
         snprintf(buffer, sizeof(buffer),
               "Error in leaderboard %d \"%s\": %s",
               lboard->info->id, lboard->info->title, rc_error_str(res));

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         CHEEVOS_ERR(RCHEEVOS_TAG "%s mem: %s\n", buffer, lboard->info->mem);
         lboard->lboard = NULL;
         continue;
      }

      lboard->lboard = (rc_lboard_t*)calloc(1, res);

      if (!lboard->lboard)
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "Error allocating memory for cheevos");
         goto error;
      }

      rc_parse_lboard(lboard->lboard,
         lboard->info->mem, NULL, 0);
      lboard->active     = false;
      lboard->last_value = 0;
      lboard->format     = rc_parse_format(lboard->info->format);
   }

   if (rcheevos_locals.patchdata.richpresence_script && *rcheevos_locals.patchdata.richpresence_script)
   {
      int buffer_size = rc_richpresence_size(rcheevos_locals.patchdata.richpresence_script);
      if (buffer_size <= 0)
      {
         snprintf(buffer, sizeof(buffer),
               "Error in rich presence: %s",
               rc_error_str(buffer_size));

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         CHEEVOS_ERR(RCHEEVOS_TAG "%s\n", buffer);
         rcheevos_locals.richpresence.richpresence = NULL;
      }
      else
      {
         char *rp_buffer = (char*)malloc(buffer_size);
         rcheevos_locals.richpresence.richpresence = rc_parse_richpresence(rp_buffer, rcheevos_locals.patchdata.richpresence_script, NULL, 0);
      }

      rcheevos_locals.richpresence.evaluation[0] = '\0';
   }

   if (!rcheevos_locals.richpresence.richpresence && rcheevos_locals.patchdata.title)
      snprintf(rcheevos_locals.richpresence.evaluation,
            sizeof(rcheevos_locals.richpresence.evaluation),
         "Playing %s", rcheevos_locals.patchdata.title);

   /* schedule the first rich presence call in 30 seconds */
   {
      rcheevos_async_io_request* request = (rcheevos_async_io_request*)
         calloc(1, sizeof(rcheevos_async_io_request));
      request->id                        = rcheevos_locals.patchdata.game_id;
      request->type                      = CHEEVOS_ASYNC_RICHPRESENCE;
      rcheevos_async_schedule(request, CHEEVOS_PING_FREQUENCY / 4);
   }

   return 0;

error:
   CHEEVOS_FREE(rcheevos_locals.core);
   CHEEVOS_FREE(rcheevos_locals.unofficial);
   CHEEVOS_FREE(rcheevos_locals.lboards);
   rcheevos_free_patchdata(&rcheevos_locals.patchdata);
   rcheevos_fixup_destroy(&rcheevos_locals.fixups);
   return -1;
}

static void rcheevos_async_award_achievement(rcheevos_async_io_request* request)
{
   char buffer[256];
   settings_t *settings = config_get_ptr();
   int ret = rc_url_award_cheevo(buffer, sizeof(buffer), settings->arrays.cheevos_username, rcheevos_locals.token, request->id, request->hardcore, rcheevos_locals.hash);

   if (ret != 0)
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "Buffer too small to create URL\n");
      free(request);
      return;
   }

   rcheevos_log_url("rc_url_award_cheevo", buffer);
   task_push_http_transfer_with_user_agent(buffer, true, NULL, request->user_agent, rcheevos_async_task_callback, request);
}

static void rcheevos_award(rcheevos_cheevo_t* cheevo, int mode)
{
   char buffer[256];
   buffer[0] = 0;

   CHEEVOS_LOG(RCHEEVOS_TAG "awarding cheevo %u: %s (%s)\n",
         cheevo->info->id, cheevo->info->title, cheevo->info->description);

   /* Deactivates the cheevo. */
   cheevo->active &= ~mode;

   if (mode == RCHEEVOS_ACTIVE_HARDCORE)
      cheevo->active &= ~RCHEEVOS_ACTIVE_SOFTCORE;

   /* Show the OSD message. */
   {
#if defined(HAVE_GFX_WIDGETS)
      bool widgets_ready = gfx_widgets_ready();
      if (widgets_ready)
         gfx_widgets_push_achievement(cheevo->info->title, cheevo->info->badge);
      else
#endif
      {
         snprintf(buffer, sizeof(buffer), "Achievement Unlocked: %s", cheevo->info->title);
         runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         runloop_msg_queue_push(cheevo->info->description, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

   /* Start the award task. */
   {
      rcheevos_async_io_request *request = (rcheevos_async_io_request*)calloc(1, sizeof(rcheevos_async_io_request));
      request->type            = CHEEVOS_ASYNC_AWARD_ACHIEVEMENT;
      request->id              = cheevo->info->id;
      request->hardcore        = ((mode & RCHEEVOS_ACTIVE_HARDCORE) != 0) 
         ? 1 : 0;
      request->success_message = "Awarded achievement";
      request->failure_message = "Error awarding achievement";
      rcheevos_get_user_agent(request->user_agent);
      rcheevos_async_award_achievement(request);
   }

#ifdef HAVE_SCREENSHOTS
   {
      settings_t *settings = config_get_ptr();
      /* Take a screenshot of the achievement. */
      if (settings && settings->bools.cheevos_auto_screenshot)
      {
         char shotname[8192];

         snprintf(shotname, sizeof(shotname), "%s/%s-cheevo-%u",
               settings->paths.directory_screenshot,
               path_basename(path_get(RARCH_PATH_BASENAME)),
               cheevo->info->id);
         shotname[sizeof(shotname) - 1] = '\0';

         if (take_screenshot(settings->paths.directory_screenshot,
                  shotname, true,
                  video_driver_cached_frame_has_valid_framebuffer(), false, true))
            CHEEVOS_LOG(RCHEEVOS_TAG "got a screenshot for cheevo %u\n", cheevo->info->id);
         else
            CHEEVOS_LOG(RCHEEVOS_TAG "failed to get screenshot for cheevo %u\n", cheevo->info->id);
      }
   }
#endif
}

static int rcheevos_has_indirect_memref(const rc_memref_value_t* memrefs)
{
   const rc_memref_value_t* memref = memrefs;
   while (memref)
   {
      if (memref->memref.is_indirect)
         return 1;

      memref = memref->next;
   }

   return 0;
}

static void rcheevos_test_cheevo_set(bool official)
{
   settings_t *settings = config_get_ptr();
   int mode = RCHEEVOS_ACTIVE_SOFTCORE;
   rcheevos_cheevo_t* cheevo;
   int i, count;

   if (settings && settings->bools.cheevos_hardcore_mode_enable && !rcheevos_hardcore_paused)
      mode = RCHEEVOS_ACTIVE_HARDCORE;

   if (official)
   {
      cheevo = rcheevos_locals.core;
      count = rcheevos_locals.patchdata.core_count;
   }
   else
   {
      cheevo = rcheevos_locals.unofficial;
      count = rcheevos_locals.patchdata.unofficial_count;
   }

   rcheevos_locals.invalid_peek_address = false;

   for (i = 0; i < count; i++, cheevo++)
   {
      /* Check if the achievement is active for the current mode. */
      if (cheevo->active & mode)
      {
         int valid = rc_test_trigger(cheevo->trigger, rcheevos_peek, NULL, NULL);

         /* trigger must be false for at least one frame before it can trigger. if last is true, the trigger hasn't yet been false. */
         if (cheevo->last)
         {
            /* if the we're still waiting for the trigger to stabilize, check to see if an error occurred */
            if (rcheevos_locals.invalid_peek_address)
            {
               /* reset the flag for the next achievement */
               rcheevos_locals.invalid_peek_address = false;

               if (rcheevos_has_indirect_memref(cheevo->trigger->memrefs))
               {
                  /* ignore bad addresses possibly generated by AddAddress */
                  CHEEVOS_LOG(RCHEEVOS_TAG "Ignoring invalid address in achievement with AddAddress: %s\n", cheevo->info->title);
               }
               else
               {
                  /* could not map one or more addresses - disable the achievement */
                  CHEEVOS_ERR(RCHEEVOS_TAG "Achievement disabled (invalid address): %s\n", cheevo->info->title);
                  cheevo->active = 0;

                  /* clear out the trigger so it shows up as 'Unsupported' in the menu */
                  CHEEVOS_FREE(cheevo->trigger);
                  cheevo->trigger = NULL;

                  continue;
               }
            }

            /* no error, reset any hit counts for the next check */
            rc_reset_trigger(cheevo->trigger);
         }
         else if (valid)
            rcheevos_award(cheevo, mode);

         cheevo->last = valid;
      }
   }
}

static void rcheevos_async_submit_lboard(rcheevos_async_io_request* request)
{
   char buffer[256];
   settings_t *settings = config_get_ptr();
   int ret = rc_url_submit_lboard(buffer, sizeof(buffer), settings->arrays.cheevos_username,
      rcheevos_locals.token, request->id, request->value);

   if (ret != 0)
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "Buffer too small to create URL\n");
      free(request);
      return;
   }

   rcheevos_log_url("rc_url_submit_lboard", buffer);
   task_push_http_transfer_with_user_agent(buffer, true, NULL, request->user_agent, rcheevos_async_task_callback, request);
}

static void rcheevos_lboard_submit(rcheevos_lboard_t* lboard)
{
   char buffer[256];
   char value[16];

   /* Deactivate the leaderboard. */
   lboard->active = 0;

   /* Failsafe for improper leaderboards. */
   if (lboard->last_value == 0)
   {
      CHEEVOS_ERR(RCHEEVOS_TAG "Leaderboard %s tried to submit 0\n", lboard->info->title);
      runloop_msg_queue_push("Leaderboard attempt cancelled!", 0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return;
   }

   /* Show the OSD message. */
   rc_format_value(value, sizeof(value), lboard->last_value, lboard->format);

   snprintf(buffer, sizeof(buffer), "Submitted %s for %s",
         value, lboard->info->title);
   runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   /* Start the submit task. */
   {
      rcheevos_async_io_request* request = (rcheevos_async_io_request*)calloc(1, sizeof(rcheevos_async_io_request));
      request->type = CHEEVOS_ASYNC_SUBMIT_LBOARD;
      request->id = lboard->info->id;
      request->value = lboard->last_value;
      request->success_message = "Submitted leaderboard";
      request->failure_message = "Error submitting leaderboard";
      rcheevos_get_user_agent(request->user_agent);
      rcheevos_async_submit_lboard(request);
   }
}

static void rcheevos_test_leaderboards(void)
{
   rcheevos_lboard_t* lboard = rcheevos_locals.lboards;
   unsigned	 i;

   rcheevos_locals.invalid_peek_address = false;

   for (i = 0; i < rcheevos_locals.patchdata.lboard_count; i++, lboard++)
   {
      if (!lboard->lboard)
         continue;

      switch (rc_evaluate_lboard(lboard->lboard, &lboard->last_value, rcheevos_peek, NULL, NULL))
      {
         default:
            break;

         case RC_LBOARD_STATE_TRIGGERED:
            rcheevos_lboard_submit(lboard);
            break;

         case RC_LBOARD_STATE_CANCELED:
            CHEEVOS_LOG(RCHEEVOS_TAG "Cancel leaderboard %s\n", lboard->info->title);
            lboard->active = 0;
            runloop_msg_queue_push("Leaderboard attempt cancelled!",
                  0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;

         case RC_LBOARD_STATE_STARTED:
            if (!lboard->active)
            {
               char buffer[256];

               CHEEVOS_LOG(RCHEEVOS_TAG "Leaderboard started: %s\n", lboard->info->title);
               lboard->active     = 1;

               snprintf(buffer, sizeof(buffer),
                     "Leaderboard Active: %s", lboard->info->title);
               runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               runloop_msg_queue_push(lboard->info->description, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
            break;
      }

      if (rcheevos_locals.invalid_peek_address)
      {
         /* reset the flag for the next leaderboard */
         rcheevos_locals.invalid_peek_address = false;

         if (!rcheevos_has_indirect_memref(lboard->lboard->memrefs))
         {
            /* disable the leaderboard */
            CHEEVOS_FREE(lboard->lboard);
            lboard->lboard = NULL;

            CHEEVOS_LOG(RCHEEVOS_TAG "Leaderboard disabled (invalid address): %s\n", lboard->info->title);
         }
      }
   }
}

const char* rcheevos_get_richpresence(void)
{
   if (!rcheevos_locals.richpresence.richpresence)
      return NULL;
   else
      return rcheevos_locals.richpresence.evaluation;
}

void rcheevos_reset_game(void)
{
   unsigned i;
   rcheevos_lboard_t* lboard;
   rcheevos_cheevo_t* cheevo = rcheevos_locals.core;

   for (i = 0; i < rcheevos_locals.patchdata.core_count; i++, cheevo++)
   {
      if (cheevo->trigger)
         rc_reset_trigger(cheevo->trigger);
      cheevo->last = 1;
   }

   cheevo = rcheevos_locals.unofficial;

   for (i = 0; i < rcheevos_locals.patchdata.unofficial_count;
         i++, cheevo++)
   {
      if (cheevo->trigger)
         rc_reset_trigger(cheevo->trigger);
      cheevo->last = 1;
   }

   lboard = rcheevos_locals.lboards;
   for (i = 0; i < rcheevos_locals.patchdata.lboard_count;
         i++, lboard++)
   {
      if (lboard->lboard)
         rc_reset_lboard(lboard->lboard);

      if (lboard->active)
         lboard->active = 0;
   }

   rcheevos_locals.richpresence.last_update = cpu_features_get_time_usec();
}

#ifdef HAVE_MENU
void rcheevos_get_achievement_state(unsigned index, char *buffer, size_t buffer_size)
{
   rcheevos_cheevo_t* cheevo;
   enum msg_hash_enums enum_idx;
   bool check_measured  = false;

   if (index < rcheevos_locals.patchdata.core_count)
   {
      enum_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY;
      cheevo = rcheevos_locals.core ? &rcheevos_locals.core[index] : NULL;
   }
   else
   {
      enum_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY;
      cheevo = rcheevos_locals.unofficial ? &rcheevos_locals.unofficial[index - rcheevos_locals.patchdata.core_count] : NULL;
   }

   if (!cheevo || !cheevo->trigger)
   {
      enum_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY;
   }
   else if (!(cheevo->active & RCHEEVOS_ACTIVE_HARDCORE))
   {
      enum_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE;
   }
   else if (!(cheevo->active & RCHEEVOS_ACTIVE_SOFTCORE))
   {
      /* if in hardcore mode, track progress towards hardcore unlock */
      const settings_t* settings = config_get_ptr();
      const bool hardcore = settings->bools.cheevos_hardcore_mode_enable && !rcheevos_hardcore_paused;
      check_measured = hardcore;

      enum_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY;
   }
   else
   {
      /* Use either "Locked" for core or "Unofficial" for unofficial as set above and track progress */
      check_measured = true;
   }

   strlcpy(buffer, msg_hash_to_str(enum_idx), buffer_size);

   if (check_measured)
   {
      const unsigned int target = cheevo->trigger->measured_target;
      if (target > 0 && cheevo->trigger->measured_value > 0)
      {
         char measured_buffer[12];
         const unsigned int value = MIN(cheevo->trigger->measured_value, target);
         const int        percent = (int)(((unsigned long)value) * 100 / target);

         snprintf(measured_buffer, sizeof(measured_buffer), " - %d%%", percent);
         strlcat(buffer, measured_buffer, buffer_size);
      }
   }
}

static void rcheevos_append_menu_achievement(
      menu_displaylist_info_t* info, size_t idx, rcheevos_cheevo_t* cheevo)
{
   bool badge_grayscale;

   menu_entries_append_enum(info->list, cheevo->info->title,
      cheevo->info->description, MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
      MENU_SETTINGS_CHEEVOS_START + idx, 0, 0);

   if (!cheevo->trigger)
   {
      /* unsupported */
      badge_grayscale = true;
   }
   else if (!(cheevo->active & RCHEEVOS_ACTIVE_HARDCORE) || 
            !(cheevo->active & RCHEEVOS_ACTIVE_SOFTCORE))
      badge_grayscale = false; /* unlocked */
   else
      badge_grayscale = true;  /* locked */

   cheevos_set_menu_badge(idx, cheevo->info->badge, badge_grayscale);
}
#endif

void rcheevos_populate_menu(void* data)
{
#ifdef HAVE_MENU
   int i                             = 0;
   int count                         = 0;
   rcheevos_cheevo_t* cheevo         = NULL;
   menu_displaylist_info_t* info     = (menu_displaylist_info_t*)data;
   settings_t* settings              = config_get_ptr();
   bool cheevos_enable               = settings->bools.cheevos_enable;
   bool cheevos_hardcore_mode_enable = settings->bools.cheevos_hardcore_mode_enable;
   bool cheevos_test_unofficial      = settings->bools.cheevos_test_unofficial;

   if (   cheevos_enable
       && cheevos_hardcore_mode_enable
       && rcheevos_loaded)
   {
      if (!rcheevos_hardcore_paused)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE,
               MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS, 0, 0);
      else
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_RESUME),
               MENU_ENUM_LABEL_ACHIEVEMENT_RESUME,
               MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS, 0, 0);
   }

   cheevo = rcheevos_locals.core;
   for (count = rcheevos_locals.patchdata.core_count; count > 0; count--)
      rcheevos_append_menu_achievement(info, i++, cheevo++);

   if (cheevos_test_unofficial)
   {
      cheevo = rcheevos_locals.unofficial;
      for (count = rcheevos_locals.patchdata.unofficial_count; count > 0; count--)
         rcheevos_append_menu_achievement(info, i++, cheevo++);
   }

   if (i == 0)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0);
   }
#endif
}

bool rcheevos_get_description(rcheevos_ctx_desc_t* desc)
{
   unsigned idx;
   const rcheevos_cheevo_t* cheevo;

   if (!desc)
      return false;

   *desc->s = 0;

   if (rcheevos_loaded)
   {
      idx = desc->idx;

      if (idx < rcheevos_locals.patchdata.core_count)
         cheevo = rcheevos_locals.core + idx;
      else
      {
         idx -= rcheevos_locals.patchdata.core_count;

         if (idx < rcheevos_locals.patchdata.unofficial_count)
            cheevo = rcheevos_locals.unofficial + idx;
         else
            return true;
      }

      strlcpy(desc->s, cheevo->info->description, desc->len);
   }

   return true;
}

void rcheevos_pause_hardcore(void)
{
   rcheevos_hardcore_paused = true;
}

bool rcheevos_unload(void)
{
   bool running          = false;
   unsigned i = 0, count = 0;
   settings_t* settings  = config_get_ptr();

   CHEEVOS_LOCK(rcheevos_locals.task_lock);
   running = rcheevos_locals.task != NULL;
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
      }while (running);
#endif
   }

   if (rcheevos_loaded)
   {
      for (i = 0, count = rcheevos_locals.patchdata.core_count; i < count; i++)
      {
         CHEEVOS_FREE(rcheevos_locals.core[i].trigger);
      }

      for (i = 0, count = rcheevos_locals.patchdata.unofficial_count; i < count; i++)
      {
         CHEEVOS_FREE(rcheevos_locals.unofficial[i].trigger);
      }

      for (i = 0, count = rcheevos_locals.patchdata.lboard_count; i < count; i++)
      {
         CHEEVOS_FREE(rcheevos_locals.lboards[i].lboard);
      }

      CHEEVOS_FREE(rcheevos_locals.core);
      CHEEVOS_FREE(rcheevos_locals.unofficial);
      CHEEVOS_FREE(rcheevos_locals.lboards);
      CHEEVOS_FREE(rcheevos_locals.richpresence.richpresence);
      rcheevos_free_patchdata(&rcheevos_locals.patchdata);
      rcheevos_fixup_destroy(&rcheevos_locals.fixups);

      rcheevos_locals.core                      = NULL;
      rcheevos_locals.unofficial                = NULL;
      rcheevos_locals.lboards                   = NULL;
      rcheevos_locals.richpresence.richpresence = NULL;

      rcheevos_loaded                           = false;
      rcheevos_hardcore_active                  = false;
      rcheevos_hardcore_paused                  = false;
      rcheevos_state_loaded_flag                = false;
   }

   /* if the config-level token has been cleared, we need to re-login on loading the next game */
   if (!settings->arrays.cheevos_token[0])
      rcheevos_locals.token[0]                  = '\0';

   return true;
}

bool rcheevos_toggle_hardcore_mode(void)
{
   settings_t *settings              = config_get_ptr();
   bool cheevos_hardcore_mode_enable = settings->bools.cheevos_hardcore_mode_enable;
   bool rewind_enable                = settings->bools.rewind_enable;

   /* reset and deinit rewind to avoid cheat the score */
   if (cheevos_hardcore_mode_enable
       && !rcheevos_hardcore_paused)
   {
      const char *msg            = msg_hash_to_str(
            MSG_CHEEVOS_HARDCORE_MODE_ENABLE);

      /* reset the state loaded flag in case it was set */
      rcheevos_state_loaded_flag = false;

      /* send reset core cmd to avoid any user
       * savestate previusly loaded. */
      command_event(CMD_EVENT_RESET, NULL);

      if (rewind_enable)
         command_event(CMD_EVENT_REWIND_DEINIT, NULL);

      CHEEVOS_LOG("%s\n", msg);
      runloop_msg_queue_push(msg, 0, 3 * 60, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   else
   {
      if (rewind_enable)
         command_event(CMD_EVENT_REWIND_INIT, NULL);
   }

   return true;
}

/*****************************************************************************
Test all the achievements (call once per frame).
*****************************************************************************/
void rcheevos_test(void)
{
   settings_t *settings = config_get_ptr();

   rcheevos_test_cheevo_set(true);

   if (settings)
   {
      if (settings->bools.cheevos_test_unofficial)
         rcheevos_test_cheevo_set(false);

      if (settings->bools.cheevos_hardcore_mode_enable &&
          settings->bools.cheevos_leaderboards_enable  &&
          !rcheevos_hardcore_paused)
         rcheevos_test_leaderboards();
   }
}

void rcheevos_set_support_cheevos(bool state)
{
   rcheevos_locals.core_supports = state;
}

bool rcheevos_get_support_cheevos(void)
{
   return rcheevos_locals.core_supports;
}

int rcheevos_get_console(void)
{
   return rcheevos_locals.patchdata.console_id;
}

const char* rcheevos_get_hash(void)
{
   return rcheevos_locals.hash;
}

static void rcheevos_unlock_cb(unsigned id, void* userdata)
{
   int i;
   unsigned j = 0, count     = 0;
   rcheevos_cheevo_t* cheevo = NULL;

   for (i = 0; i < 2; i++)
   {
      if (i == 0)
      {
         cheevo = rcheevos_locals.core;
         count  = rcheevos_locals.patchdata.core_count;
      }
      else
      {
         cheevo = rcheevos_locals.unofficial;
         count  = rcheevos_locals.patchdata.unofficial_count;
      }

      for (j = 0; j < count; j++, cheevo++)
      {
         if (cheevo->info->id == id)
         {
#ifndef CHEEVOS_DONT_DEACTIVATE
            cheevo->active &= ~*(unsigned*)userdata;
#endif
            CHEEVOS_LOG(RCHEEVOS_TAG "cheevo %u deactivated (%s): %s\n", id,
               (*(unsigned*)userdata) == RCHEEVOS_ACTIVE_HARDCORE 
               ? "hardcore" : "softcore",
               cheevo->info->title);
            return;
         }
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
   rcheevos_cheevo_t *cheevo;
   const rcheevos_cheevo_t *cheevo_end;
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
         strcpy(rcheevos_locals.hash, "N/A");
         rcheevos_hardcore_paused = true;
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
      if (rcheevos_parse(coro->json))
      {
         CHEEVOS_FREE(coro->json);
         CORO_STOP();
      }

      CHEEVOS_FREE(coro->json);

      if (   rcheevos_locals.patchdata.core_count == 0
          && rcheevos_locals.patchdata.unofficial_count == 0
          && rcheevos_locals.patchdata.lboard_count == 0)
      {
         runloop_msg_queue_push(
               "This game has no achievements.",
               0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         rcheevos_hardcore_paused = true;

         CORO_STOP();
      }

      rcheevos_loaded = true;

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
         const rcheevos_cheevo_t* cheevo = rcheevos_locals.core;
         const rcheevos_cheevo_t* end    = cheevo + rcheevos_locals.patchdata.core_count;
         int number_of_unlocked          = rcheevos_locals.patchdata.core_count;
         int number_of_unsupported       = 0;

         if (coro->settings->bools.cheevos_hardcore_mode_enable && !rcheevos_hardcore_paused)
            mode = RCHEEVOS_ACTIVE_HARDCORE;

         for (; cheevo < end; cheevo++)
         {
            if (!cheevo->trigger)
               number_of_unsupported++;
            else if (cheevo->active & mode)
               number_of_unlocked--;
         }

         if (!number_of_unsupported)
         {
            if (coro->settings->bools.cheevos_start_active)
               snprintf(msg, sizeof(msg),
                  "All %d achievements activated for this session.",
                  rcheevos_locals.patchdata.core_count);
            else
            {
               snprintf(msg, sizeof(msg),
                  "You have %d of %d achievements unlocked.",
                  number_of_unlocked, rcheevos_locals.patchdata.core_count);
            }
         }
         else
         {
            if (coro->settings->bools.cheevos_start_active)
               snprintf(msg, sizeof(msg),
                  "All %d achievements activated for this session (%d unsupported).",
                  rcheevos_locals.patchdata.core_count,
                  number_of_unsupported);
            else
            {
               snprintf(msg, sizeof(msg),
                     "You have %d of %d achievements unlocked (%d unsupported).",
                     number_of_unlocked - number_of_unsupported,
                     rcheevos_locals.patchdata.core_count,
                     number_of_unsupported);
            }
         }

         msg[sizeof(msg) - 1] = 0;
         runloop_msg_queue_push(msg, 0, 6 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
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

#ifdef HAVE_MENU
      cheevos_reset_menu_badges();
#endif

      for (coro->i = 0; coro->i < 2; coro->i++)
      {
         if (coro->i == 0)
         {
            coro->cheevo     = rcheevos_locals.core;
            coro->cheevo_end = coro->cheevo + rcheevos_locals.patchdata.core_count;
         }
         else
         {
            coro->cheevo     = rcheevos_locals.unofficial;
            coro->cheevo_end = coro->cheevo + rcheevos_locals.patchdata.unofficial_count;
         }

         for (; coro->cheevo < coro->cheevo_end; coro->cheevo++)
         {
            if (!coro->cheevo->info->badge[0])
               continue;

            for (coro->j = 0 ; coro->j < 2; coro->j++)
            {
               coro->badge_fullpath[0] = '\0';
               fill_pathname_application_special(
                     coro->badge_fullpath,
                     sizeof(coro->badge_fullpath),
                     APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

               if (!path_is_directory(coro->badge_fullpath))
                  path_mkdir(coro->badge_fullpath);
               CORO_YIELD();

               if (!coro->cheevo->info->badge || !coro->cheevo->info->badge[0])
                  continue;

               if (coro->j == 0)
                  snprintf(coro->badge_name,
                        sizeof(coro->badge_name),
                        "%s.png", coro->cheevo->info->badge);
               else
                  snprintf(coro->badge_name,
                        sizeof(coro->badge_name),
                        "%s_lock.png", coro->cheevo->info->badge);

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
                        "http://i.retroachievements.org/Badge/%s",
                        coro->badge_name);

                  CORO_GOSUB(RCHEEVOS_HTTP_GET);

                  if (coro->json)
                  {
                     if (!filestream_write_file(coro->badge_fullpath,
                              coro->json, coro->k))
                        CHEEVOS_ERR(RCHEEVOS_TAG "error writing badge %s\n", coro->badge_fullpath);
                     else
                     {
                        CHEEVOS_FREE(coro->json);
                        coro->json = NULL;
                     }
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
         runloop_msg_queue_push(msg, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
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
         }while ((t1 - coro->t0) < 3000000);
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

         rcheevos_get_user_agent(buffer);
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
         CHEEVOS_LOG(RCHEEVOS_TAG "posted playing activity\n");
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

/* hooks for rhash library */

static void* rc_hash_handle_file_open(const char* path)
{
   return intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static void rc_hash_handle_file_seek(void* file_handle, size_t offset, int origin)
{
   intfstream_seek((intfstream_t*)file_handle, offset, origin);
}

static size_t rc_hash_handle_file_tell(void* file_handle)
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

   if (track == 0)
      cdfs_track = cdfs_open_data_track(path);
   else
      cdfs_track = cdfs_open_track(path, track);

   if (cdfs_track)
   {
      cdfs_file_t* file = (cdfs_file_t*)malloc(sizeof(cdfs_file_t));
      if (cdfs_open_file(file, cdfs_track, NULL))
         return file;

      CHEEVOS_FREE(file);
   }

   cdfs_close_track(cdfs_track); /* ASSERT: this free()s cdfs_track */
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

   rcheevos_loaded                    = false;
   rcheevos_hardcore_paused           = false;

   if (!cheevos_enable || !rcheevos_locals.core_supports || !data)
   {
      rcheevos_hardcore_paused        = true;
      return false;
   }

   coro = (rcheevos_coro_t*)calloc(1, sizeof(*coro));

   if (!coro)
      return false;

   /* provide hooks for reading files */
   filereader.open = rc_hash_handle_file_open;
   filereader.seek = rc_hash_handle_file_seek;
   filereader.tell = rc_hash_handle_file_tell;
   filereader.read = rc_hash_handle_file_read;
   filereader.close = rc_hash_handle_file_close;
   rc_hash_init_custom_filereader(&filereader);

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

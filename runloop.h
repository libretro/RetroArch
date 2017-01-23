/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016 - Brad Parker
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

#ifndef __RETROARCH_RUNLOOP_H
#define __RETROARCH_RUNLOOP_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <boolean.h>
#include <retro_common_api.h>

#define runloop_cmd_press(current_input, id)     (BIT64_GET(current_input, id))

RETRO_BEGIN_DECLS

enum runloop_ctl_state
{
   RUNLOOP_CTL_NONE = 0,

   RUNLOOP_CTL_SET_FRAME_LIMIT,

   RUNLOOP_CTL_TASK_INIT,

   RUNLOOP_CTL_FRAME_TIME_FREE,
   RUNLOOP_CTL_SET_FRAME_TIME_LAST,
   RUNLOOP_CTL_SET_FRAME_TIME,

   RUNLOOP_CTL_IS_IDLE,
   RUNLOOP_CTL_SET_IDLE,

   RUNLOOP_CTL_GET_WINDOWED_SCALE,
   RUNLOOP_CTL_SET_WINDOWED_SCALE,

   RUNLOOP_CTL_IS_OVERRIDES_ACTIVE,
   RUNLOOP_CTL_SET_OVERRIDES_ACTIVE,
   RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE,

   RUNLOOP_CTL_IS_MISSING_BIOS,
   RUNLOOP_CTL_SET_MISSING_BIOS,
   RUNLOOP_CTL_UNSET_MISSING_BIOS,

   RUNLOOP_CTL_IS_GAME_OPTIONS_ACTIVE,

   RUNLOOP_CTL_IS_NONBLOCK_FORCED,
   RUNLOOP_CTL_SET_NONBLOCK_FORCED,
   RUNLOOP_CTL_UNSET_NONBLOCK_FORCED,

   RUNLOOP_CTL_SET_LIBRETRO_PATH,

   RUNLOOP_CTL_SET_SLOWMOTION,
   
   RUNLOOP_CTL_IS_PAUSED,
   RUNLOOP_CTL_SET_PAUSED,
   RUNLOOP_CTL_SET_MAX_FRAMES,
   RUNLOOP_CTL_GLOBAL_FREE,

   RUNLOOP_CTL_SET_CORE_SHUTDOWN,

   RUNLOOP_CTL_SET_SHUTDOWN,
   RUNLOOP_CTL_IS_SHUTDOWN,

   RUNLOOP_CTL_SET_EXEC,

   /* Runloop state */
   RUNLOOP_CTL_CLEAR_STATE,
   RUNLOOP_CTL_STATE_FREE,

   /* Performance counters */
   RUNLOOP_CTL_GET_PERFCNT,
   RUNLOOP_CTL_SET_PERFCNT_ENABLE,
   RUNLOOP_CTL_UNSET_PERFCNT_ENABLE,
   RUNLOOP_CTL_IS_PERFCNT_ENABLE,

   /* Key event */
   RUNLOOP_CTL_FRONTEND_KEY_EVENT_GET,
   RUNLOOP_CTL_KEY_EVENT_GET,
   RUNLOOP_CTL_DATA_DEINIT,

   /* Message queue */
   RUNLOOP_CTL_MSG_QUEUE_INIT,
   RUNLOOP_CTL_MSG_QUEUE_DEINIT,

   /* Core options */
   RUNLOOP_CTL_HAS_CORE_OPTIONS,
   RUNLOOP_CTL_GET_CORE_OPTION_SIZE,
   RUNLOOP_CTL_IS_CORE_OPTION_UPDATED,
   RUNLOOP_CTL_CORE_OPTIONS_LIST_GET,
   RUNLOOP_CTL_CORE_OPTION_PREV,
   RUNLOOP_CTL_CORE_OPTION_NEXT,
   RUNLOOP_CTL_CORE_OPTIONS_GET,
   RUNLOOP_CTL_CORE_OPTIONS_INIT,
   RUNLOOP_CTL_CORE_OPTIONS_DEINIT,
   RUNLOOP_CTL_CORE_OPTIONS_FREE,

   /* System info */
   RUNLOOP_CTL_SYSTEM_INFO_GET,
   RUNLOOP_CTL_SYSTEM_INFO_INIT,
   RUNLOOP_CTL_SYSTEM_INFO_FREE,

   /* HTTP server */
   RUNLOOP_CTL_HTTPSERVER_INIT,
   RUNLOOP_CTL_HTTPSERVER_DESTROY
};

typedef struct rarch_resolution
{
   unsigned idx;
   unsigned id;
} rarch_resolution_t;

/* All run-time- / command line flag-related globals go here. */

typedef struct global
{
   struct
   {
      char savefile[4096];
      char savestate[4096];
      char cheatfile[4096];
      char ups[4096];
      char bps[4096];
      char ips[4096];
      char remapfile[4096];
   } name;

   /* Recording. */
   struct
   {
      char path[4096];
      char config[4096];
      unsigned width;
      unsigned height;

      size_t gpu_width;
      size_t gpu_height;
      char output_dir[4096];
      char config_dir[4096];
      bool use_output_dir;
   } record;

   /* Settings and/or global state that is specific to 
    * a console-style implementation. */
   struct
   {
      struct
      {
         struct
         {
            rarch_resolution_t current;
            rarch_resolution_t initial;
            uint32_t *list;
            unsigned count;
            bool check;
         } resolutions;

         unsigned gamma_correction;
         unsigned int flicker_filter_index;
         unsigned char soft_filter_index;
         bool pal_enable;
         bool pal60_enable;
      } screen;

      struct
      {
         bool system_bgm_enable;
      } sound;

      bool flickerfilter_enable;
      bool softfilter_enable;
   } console;
} global_t;

global_t *global_get_ptr(void);

/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on successful run, 
 * Returns 1 if we have to wait until button input in order 
 * to wake up the loop.
 * Returns -1 if we forcibly quit out of the 
 * RetroArch iteration loop. 
 **/
int runloop_iterate(unsigned *sleep_ms);

void runloop_msg_queue_push(const char *msg, unsigned prio,
      unsigned duration, bool flush);

bool runloop_msg_queue_pull(const char **ret);

void runloop_get_status(bool *is_paused, bool *is_idle, bool *is_slowmotion);

bool runloop_ctl(enum runloop_ctl_state state, void *data);

RETRO_END_DECLS

#endif

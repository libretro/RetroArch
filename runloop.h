/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <retro_miscellaneous.h>

#include "configuration.h"
#include "core_info.h"
#include "dynamic.h"

#ifdef __cplusplus
extern "C" {
#endif

enum runloop_ctl_state
{
   RUNLOOP_CTL_NONE = 0,
   RUNLOOP_CTL_SET_FRAME_LIMIT,
   RUNLOOP_CTL_UNSET_FRAME_LIMIT,
   RUNLOOP_CTL_SHOULD_SET_FRAME_LIMIT,
   RUNLOOP_CTL_SET_FRAME_TIME_LAST,
   RUNLOOP_CTL_UNSET_FRAME_TIME_LAST,
   RUNLOOP_CTL_IS_FRAME_TIME_LAST,
   RUNLOOP_CTL_IS_FRAME_COUNT_END,
   RUNLOOP_CTL_IS_IDLE,
   RUNLOOP_CTL_GET_WINDOWED_SCALE,
   RUNLOOP_CTL_SET_WINDOWED_SCALE,
   RUNLOOP_CTL_SET_IDLE,
   RUNLOOP_CTL_CHECK_IDLE_STATE,
   RUNLOOP_CTL_GET_CONTENT_PATH,
   RUNLOOP_CTL_SET_CONTENT_PATH,
   RUNLOOP_CTL_CLEAR_CONTENT_PATH,
   RUNLOOP_CTL_SET_LIBRETRO_PATH,
   RUNLOOP_CTL_IS_SLOWMOTION,
   RUNLOOP_CTL_SET_SLOWMOTION,
   RUNLOOP_CTL_IS_PAUSED,
   RUNLOOP_CTL_SET_PAUSED,
   RUNLOOP_CTL_SET_MAX_FRAMES,
   RUNLOOP_CTL_CLEAR_STATE,
   RUNLOOP_CTL_STATE_FREE,
   RUNLOOP_CTL_GLOBAL_FREE,
   RUNLOOP_CTL_CHECK_FOCUS,
   RUNLOOP_CTL_SET_CORE_SHUTDOWN,
   RUNLOOP_CTL_UNSET_CORE_SHUTDOWN,
   RUNLOOP_CTL_IS_CORE_SHUTDOWN,
   RUNLOOP_CTL_SET_SHUTDOWN,
   RUNLOOP_CTL_UNSET_SHUTDOWN,
   RUNLOOP_CTL_IS_SHUTDOWN,
   RUNLOOP_CTL_SET_EXEC,
   RUNLOOP_CTL_UNSET_EXEC,
   RUNLOOP_CTL_IS_EXEC,
   RUNLOOP_CTL_GET_PERFCNT,
   RUNLOOP_CTL_SET_PERFCNT_ENABLE,
   RUNLOOP_CTL_UNSET_PERFCNT_ENABLE,
   RUNLOOP_CTL_IS_PERFCNT_ENABLE,
   RUNLOOP_CTL_DATA_DEINIT,
   /* Checks for state changes in this frame. */
   RUNLOOP_CTL_CHECK_STATE,
   RUNLOOP_CTL_CHECK_MOVIE,
   /* Checks if movie is being played. */
   RUNLOOP_CTL_CHECK_MOVIE_PLAYBACK,
   RUNLOOP_CTL_CHECK_MOVIE_INIT,
   /* Checks if movie is being recorded. */
   RUNLOOP_CTL_CHECK_MOVIE_RECORD,
   /* Checks if slowmotion toggle/hold was being pressed and/or held. */
   RUNLOOP_CTL_CHECK_SLOWMOTION,
   RUNLOOP_CTL_CHECK_PAUSE_STATE,
   /* Initializes message queue. */
   RUNLOOP_CTL_MSG_QUEUE_INIT,
   /* Deinitializes message queue. */
   RUNLOOP_CTL_MSG_QUEUE_DEINIT,
   /* Initializes dummy core. */
   RUNLOOP_CTL_MSG_QUEUE_LOCK,
   RUNLOOP_CTL_MSG_QUEUE_UNLOCK,
   RUNLOOP_CTL_MSG_QUEUE_FREE,
   RUNLOOP_CTL_HAS_CORE_OPTIONS,
   RUNLOOP_CTL_GET_CORE_OPTION_SIZE,
   RUNLOOP_CTL_IS_CORE_OPTION_UPDATED,
   RUNLOOP_CTL_CORE_OPTION_PREV,
   RUNLOOP_CTL_CORE_OPTION_NEXT,
   RUNLOOP_CTL_CORE_OPTIONS_GET,
   RUNLOOP_CTL_CORE_OPTIONS_INIT,
   RUNLOOP_CTL_CORE_OPTIONS_DEINIT,
   RUNLOOP_CTL_SHADER_DIR_DEINIT,
   RUNLOOP_CTL_SHADER_DIR_INIT,
   RUNLOOP_CTL_SYSTEM_INFO_INIT,
   RUNLOOP_CTL_SYSTEM_INFO_FREE,
   RUNLOOP_CTL_PREPARE_DUMMY
};

typedef struct rarch_dir_list
{
   struct string_list *list;
   size_t ptr;
} rarch_dir_list_t;

typedef struct rarch_dir
{
   /* Used on reentrancy to use a savestate dir. */
   char savefile[PATH_MAX_LENGTH];
   char savestate[PATH_MAX_LENGTH];
   char systemdir[PATH_MAX_LENGTH];
#ifdef HAVE_OVERLAY
   char osk_overlay[PATH_MAX_LENGTH];
#endif
   rarch_dir_list_t filter_dir;
} rarch_dir_t;

typedef struct rarch_path
{
   char gb_rom[PATH_MAX_LENGTH];
   char bsx_rom[PATH_MAX_LENGTH];
   char sufami_rom[2][PATH_MAX_LENGTH];
   /* Config associated with global "default" config. */
   char config[PATH_MAX_LENGTH];
   char append_config[PATH_MAX_LENGTH];
   char input_config[PATH_MAX_LENGTH];
#ifdef HAVE_FILE_LOGGER
   char default_log[PATH_MAX_LENGTH];
#endif
   /* Config file associated with per-core configs. */
   char core_specific_config[PATH_MAX_LENGTH];
} rarch_path_t;

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
      core_info_list_t *list;
      core_info_t *current;
   } core_info;

   uint32_t content_crc;

   rarch_path_t path;

   struct
   {
      bool input_descriptors;
      bool save_path;
      bool state_path;
      bool libretro_device[MAX_USERS];
      bool libretro;
      bool libretro_directory;
      bool verbosity;

      bool netplay_mode;
      bool username;
      bool netplay_ip_address;
      bool netplay_delay_frames;
      bool netplay_ip_port;

      bool ups_pref;
      bool bps_pref;
      bool ips_pref;
   } has_set;

   
   bool overrides_active;

   struct
   {
      char base[PATH_MAX_LENGTH];
      char savefile[PATH_MAX_LENGTH];
      char savestate[PATH_MAX_LENGTH];
      char cheatfile[PATH_MAX_LENGTH];
      char ups[PATH_MAX_LENGTH];
      char bps[PATH_MAX_LENGTH];
      char ips[PATH_MAX_LENGTH];
   } name;

   /* A list of save types and associated paths for all content. */
   struct string_list *savefiles;

   /* For --subsystem content. */
   char subsystem[PATH_MAX_LENGTH];
   struct string_list *subsystem_fullpaths;

   rarch_dir_t dir;

   struct
   {
      bool block_patch;
      bool ups_pref;
      bool bps_pref;
      bool ips_pref;
   } patch;

   struct
   {
      bool load_disable;
      bool save_disable;
      bool use;
   } sram;

#ifdef HAVE_NETPLAY
   /* Netplay. */
   struct
   {
      char server[PATH_MAX_LENGTH];
      bool enable;
      bool is_client;
      bool is_spectate;
      unsigned sync_frames;
      unsigned port;
   } netplay;
#endif

   /* Recording. */
   struct
   {
      char path[PATH_MAX_LENGTH];
      char config[PATH_MAX_LENGTH];
      unsigned width;
      unsigned height;

      size_t gpu_width;
      size_t gpu_height;
      char output_dir[PATH_MAX_LENGTH];
      char config_dir[PATH_MAX_LENGTH];
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
		    unsigned width;
		    unsigned height;
         } resolutions;

         unsigned gamma_correction;
         unsigned char flicker_filter_index;
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
   
   struct
   {
      bool main;
      bool content;
      struct
      {
         bool no_content;
         enum rarch_core_type type;
      } core;
   } inited;

   retro_keyboard_event_t frontend_key_event;
} global_t;

global_t *global_get_ptr(void);

/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on successful run, 1 if we have to wait until button input in order
 * to wake up the loop, -1 if we forcibly quit out of the RetroArch iteration loop. 
 **/
int runloop_iterate(unsigned *sleep_ms);

void runloop_msg_queue_push(const char *msg, unsigned prio,
      unsigned duration, bool flush);

void runloop_msg_queue_push_new(uint32_t hash, unsigned prio,
      unsigned duration, bool flush);

const char *runloop_msg_queue_pull(void);

bool runloop_ctl(enum runloop_ctl_state state, void *data);

typedef int (*transfer_cb_t)(void *data, size_t len);

void runloop_data_iterate(void);

#ifdef __cplusplus
}
#endif

#endif

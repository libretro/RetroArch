/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __RETROARCH_H
#define __RETROARCH_H

#include <boolean.h>
#include "core_info.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "command_event.h"

enum action_state
{
   RARCH_ACTION_STATE_NONE = 0,
   RARCH_ACTION_STATE_LOAD_CONTENT,
   RARCH_ACTION_STATE_MENU_RUNNING,
   RARCH_ACTION_STATE_MENU_RUNNING_FINISHED,
   RARCH_ACTION_STATE_QUIT,
   RARCH_ACTION_STATE_FORCE_QUIT,
};

struct rarch_main_wrap
{
   const char *content_path;
   const char *sram_path;
   const char *state_path;
   const char *config_path;
   const char *libretro_path;
   bool verbose;
   bool no_content;

   bool touched;
};

typedef struct rarch_cmd_state
{
   bool fullscreen_toggle;
   bool overlay_next_pressed;
   bool grab_mouse_pressed;
   bool menu_pressed;
   bool quit_key_pressed;
   bool screenshot_pressed;
   bool mute_pressed;
   bool osk_pressed;
   bool volume_up_pressed;
   bool volume_down_pressed;
   bool reset_pressed;
   bool disk_prev_pressed;
   bool disk_next_pressed;
   bool disk_eject_pressed;
   bool movie_record;
   bool save_state_pressed;
   bool load_state_pressed;
   bool slowmotion_pressed;
   bool shader_next_pressed;
   bool shader_prev_pressed;
   bool fastforward_pressed;
   bool hold_pressed;
   bool old_hold_pressed;
   bool state_slot_increase;
   bool state_slot_decrease;
   bool pause_pressed;
   bool frameadvance_pressed;
   bool rewind_pressed;
   bool netplay_flip_pressed;
   bool cheat_index_plus_pressed;
   bool cheat_index_minus_pressed;
   bool cheat_toggle_pressed;
} rarch_cmd_state_t;

void rarch_main_alloc(void);

void rarch_main_new(void);

void rarch_main_free(void);

void rarch_main_set_state(unsigned action);

/**
 * rarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments. 
 *
 * Initializes RetroArch.
 *
 * Returns: 0 on success, otherwise 1 if there was an error.
 **/
int rarch_main_init(int argc, char *argv[]);

/**
 * rarch_main_init_wrap:
 * @args                 : Input arguments.
 * @argc                 : Count of arguments.
 * @argv                 : Arguments.
 *
 * Generates an @argc and @argv pair based on @args
 * of type rarch_main_wrap.
 **/
void rarch_main_init_wrap(const struct rarch_main_wrap *args,
      int *argc, char **argv);

/**
 * rarch_main_deinit:
 *
 * Deinitializes RetroArch.
 **/
void rarch_main_deinit(void);

/**
 * rarch_render_cached_frame:
 *
 * Renders the current video frame.
 **/
void rarch_render_cached_frame(void);

/**
 * rarch_replace_config:
 * @path                 : Path to config file to replace
 *                         current config file with.
 *
 * Replaces currently loaded configuration file with
 * another one. Will load a dummy core to flush state
 * properly.
 *
 * Quite intrusive and error prone.
 * Likely to have lots of small bugs.
 * Cleanly exit the main loop to ensure that all the tiny details
 * get set properly.
 *
 * This should mitigate most of the smaller bugs.
 *
 * Returns: true (1) if successful, false (0) if @path was the
 * same as the current config file.
 **/
bool rarch_replace_config(const char *path);

/**
 * rarch_playlist_load_content:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
void rarch_playlist_load_content(content_playlist_t *playlist,
      unsigned index);

/**
 * rarch_defer_core:
 * @core_info            : Core info list handle.
 * @dir                  : Directory. Gets joined with @path.
 * @path                 : Path. Gets joined with @dir.
 * @menu_label           : Label identifier of menu setting.
 * @deferred_path        : Deferred core path. Will be filled in
 *                         by function.
 * @sizeof_deferred_path : Size of @deferred_path.
 *
 * Gets deferred core.
 *
 * Returns: 0 if there are multiple deferred cores and a 
 * selection needs to be made from a list, otherwise
 * returns -1 and fills in @deferred_path with path to core.
 **/
int rarch_defer_core(core_info_list_t *data,
      const char *dir, const char *path, const char *menu_label,
      char *deferred_path, size_t sizeof_deferred_path);

void rarch_fill_pathnames(void);

/* 
 * rarch_verify_api_version:
 *
 * Compare libretro core API version against API version
 * used by RetroArch.
 *
 * TODO - when libretro v2 gets added, allow for switching
 * between libretro version backend dynamically.
 **/
void rarch_verify_api_version(void);

/**
 * rarch_init_system_av_info:
 *
 * Initialize system A/V information by calling the libretro core's
 * get_system_av_info function.
 **/
void rarch_init_system_av_info(void);

void rarch_set_paths(const char *path);

#ifdef __cplusplus
}
#endif

#endif

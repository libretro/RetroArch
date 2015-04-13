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

enum event_command
{
   EVENT_CMD_NONE = 0,
   /* Resets RetroArch. */
   EVENT_CMD_RESET,
   /* Loads content file. */
   EVENT_CMD_LOAD_CONTENT,
   EVENT_CMD_LOAD_CONTENT_PERSIST,
   /* Loads core. */
   EVENT_CMD_LOAD_CORE_DEINIT,
   EVENT_CMD_LOAD_CORE,
   EVENT_CMD_LOAD_CORE_PERSIST,
   EVENT_CMD_UNLOAD_CORE,
   EVENT_CMD_LOAD_STATE,
   EVENT_CMD_SAVE_STATE,
   /* Takes screenshot. */
   EVENT_CMD_TAKE_SCREENSHOT,
   /* Initializes dummy core. */
   EVENT_CMD_PREPARE_DUMMY,
   /* Quits RetroArch. */
   EVENT_CMD_QUIT,
   /* Reinitialize all drivers. */
   EVENT_CMD_REINIT,
   /* Deinitialize rewind. */
   EVENT_CMD_REWIND_DEINIT,
   /* Initializes rewind. */
   EVENT_CMD_REWIND_INIT,
   /* Toggles rewind. */
   EVENT_CMD_REWIND_TOGGLE,
   /* Deinitializes autosave. */
   EVENT_CMD_AUTOSAVE_DEINIT,
   /* Initializes autosave. */
   EVENT_CMD_AUTOSAVE_INIT,
   EVENT_CMD_AUTOSAVE_STATE,
   /* Stops audio. */
   EVENT_CMD_AUDIO_STOP,
   /* Starts audio. */
   EVENT_CMD_AUDIO_START,
   /* Mutes audio. */
   EVENT_CMD_AUDIO_MUTE_TOGGLE,
   /* Initializes overlay. */
   EVENT_CMD_OVERLAY_INIT,
   /* Deinitializes overlay. */
   EVENT_CMD_OVERLAY_DEINIT,
   /* Sets current scale factor for overlay. */
   EVENT_CMD_OVERLAY_SET_SCALE_FACTOR,
   /* Sets current alpha modulation for overlay. */
   EVENT_CMD_OVERLAY_SET_ALPHA_MOD,
   /* Cycle to next overlay. */
   EVENT_CMD_OVERLAY_NEXT,
   /* Deinitializes overlay. */
   EVENT_CMD_DSP_FILTER_INIT,
   /* Deinitializes graphics filter. */
   EVENT_CMD_DSP_FILTER_DEINIT,
   /* Deinitializes GPU recoring. */
   EVENT_CMD_GPU_RECORD_DEINIT,
   /* Initializes recording system. */
   EVENT_CMD_RECORD_INIT,
   /* Deinitializes recording system. */
   EVENT_CMD_RECORD_DEINIT,
   /* Deinitializes history playlist. */
   EVENT_CMD_HISTORY_DEINIT,
   /* Initializes history playlist. */
   EVENT_CMD_HISTORY_INIT,
   /* Deinitializes core information. */
   EVENT_CMD_CORE_INFO_DEINIT,
   /* Initializes core information. */
   EVENT_CMD_CORE_INFO_INIT,
   /* Deinitializes core. */
   EVENT_CMD_CORE_DEINIT,
   /* Initializes core. */
   EVENT_CMD_CORE_INIT,
   /* Set audio blocking state. */
   EVENT_CMD_AUDIO_SET_BLOCKING_STATE,
   /* Set audio nonblocking state. */
   EVENT_CMD_AUDIO_SET_NONBLOCKING_STATE,
   /* Apply video state changes. */
   EVENT_CMD_VIDEO_APPLY_STATE_CHANGES,
   /* Set video blocking state. */
   EVENT_CMD_VIDEO_SET_BLOCKING_STATE,
   /* Set video nonblocking state. */
   EVENT_CMD_VIDEO_SET_NONBLOCKING_STATE,
   /* Sets current aspect ratio index. */
   EVENT_CMD_VIDEO_SET_ASPECT_RATIO,
   EVENT_CMD_RESET_CONTEXT,
   /* Restarts RetroArch. */
   EVENT_CMD_RESTART_RETROARCH,
   /* Force-quit RetroArch. */
   EVENT_CMD_QUIT_RETROARCH,
   /* Resume RetroArch when in menu. */
   EVENT_CMD_RESUME,
   /* Toggles pause. */
   EVENT_CMD_PAUSE_TOGGLE,
   /* Pauses RetroArch. */
   EVENT_CMD_UNPAUSE,
   /* Unpauses retroArch. */
   EVENT_CMD_PAUSE,
   EVENT_CMD_PAUSE_CHECKS,
   EVENT_CMD_MENU_SAVE_CONFIG,
   EVENT_CMD_MENU_PAUSE_LIBRETRO,
   /* Toggles menu on/off. */
   EVENT_CMD_MENU_TOGGLE,
   /* Applies shader changes. */
   EVENT_CMD_SHADERS_APPLY_CHANGES,
   /* Initializes shader directory. */
   EVENT_CMD_SHADER_DIR_INIT,
   /* Deinitializes shader directory. */
   EVENT_CMD_SHADER_DIR_DEINIT,
   /* Initializes controllers. */
   EVENT_CMD_CONTROLLERS_INIT,
   EVENT_CMD_SAVEFILES,
   /* Initializes savefiles. */
   EVENT_CMD_SAVEFILES_INIT,
   /* Deinitializes savefiles. */
   EVENT_CMD_SAVEFILES_DEINIT,
   /* Initializes message queue. */
   EVENT_CMD_MSG_QUEUE_INIT,
   /* Deinitializes message queue. */
   EVENT_CMD_MSG_QUEUE_DEINIT,
   /* Initializes cheats. */
   EVENT_CMD_CHEATS_INIT,
   /* Deinitializes cheats. */
   EVENT_CMD_CHEATS_DEINIT,
   /* Deinitializes network system. */
   EVENT_CMD_NETWORK_DEINIT,
   /* Initializes network system. */
   EVENT_CMD_NETWORK_INIT,
   /* Initializes netplay system. */
   EVENT_CMD_NETPLAY_INIT,
   /* Deinitializes netplay system. */
   EVENT_CMD_NETPLAY_DEINIT,
   /* Flip netplay players. */
   EVENT_CMD_NETPLAY_FLIP_PLAYERS,
   /* Initializes BSV movie. */
   EVENT_CMD_BSV_MOVIE_INIT,
   /* Deinitializes BSV movie. */
   EVENT_CMD_BSV_MOVIE_DEINIT,
   /* Initializes command interface. */
   EVENT_CMD_COMMAND_INIT,
   /* Deinitialize command interface. */
   EVENT_CMD_COMMAND_DEINIT,
   /* Deinitializes drivers. */
   EVENT_CMD_DRIVERS_DEINIT,
   /* Initializes drivers. */
   EVENT_CMD_DRIVERS_INIT,
   /* Reinitializes audio driver. */
   EVENT_CMD_AUDIO_REINIT,
   /* Resizes windowed scale. Will reinitialize video driver. */
   EVENT_CMD_RESIZE_WINDOWED_SCALE,
   /* Deinitializes temporary content. */
   EVENT_CMD_TEMPORARY_CONTENT_DEINIT,
   EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT,
   EVENT_CMD_LOG_FILE_DEINIT,
   /* Toggles disk eject. */
   EVENT_CMD_DISK_EJECT_TOGGLE,
   /* Cycle to next disk. */
   EVENT_CMD_DISK_NEXT,
   /* Cycle to previous disk. */
   EVENT_CMD_DISK_PREV,
   /* Stops rumbling. */
   EVENT_CMD_RUMBLE_STOP,
   /* Toggles mouse grab. */
   EVENT_CMD_GRAB_MOUSE_TOGGLE,
   /* Toggles fullscreen mode. */
   EVENT_CMD_FULLSCREEN_TOGGLE,
   EVENT_CMD_PERFCNT_REPORT_FRONTEND_LOG,
   EVENT_CMD_REMAPPING_INIT,
   EVENT_CMD_REMAPPING_DEINIT,
   EVENT_CMD_VOLUME_UP,
   EVENT_CMD_VOLUME_DOWN,
   EVENT_CMD_DATA_RUNLOOP_FREE,
};

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
 * rarch_main_command:
 * @cmd                  : Command index.
 *
 * Performs RetroArch command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool rarch_main_command(unsigned action);

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
 * rarch_disk_control_set_eject:
 * @new_state            : Eject or close the virtual drive tray.
 *                         false (0) : Close
 *                         true  (1) : Eject
 * @print_log            : Show message onscreen.
 *
 * Ejects/closes of the virtual drive tray.
 **/
void rarch_disk_control_set_eject(bool state, bool log);

/**
 * rarch_disk_control_set_index:
 * @index                : Index of disk to set as current.
 *
 * Sets current disk to @index.
 **/
void rarch_disk_control_set_index(unsigned index);

/**
 * rarch_disk_control_append_image:
 * @path                 : Path to disk image. 
 *
 * Appends disk image to disk image list.
 **/
void rarch_disk_control_append_image(const char *path);

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

#ifdef __cplusplus
}
#endif

#endif

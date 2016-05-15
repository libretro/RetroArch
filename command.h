/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef COMMAND_H__
#define COMMAND_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct command command_t;

typedef struct command_handle
{
   command_t *handle;
   unsigned id;
} command_handle_t;

enum event_command
{
   CMD_EVENT_NONE = 0,
   /* Resets RetroArch. */
   CMD_EVENT_RESET,
   /* Loads content file. */
   CMD_EVENT_LOAD_CONTENT,
   CMD_EVENT_LOAD_CONTENT_PERSIST,
   CMD_EVENT_LOAD_CONTENT_FFMPEG,
   CMD_EVENT_LOAD_CONTENT_IMAGEVIEWER,
   CMD_EVENT_SET_PER_GAME_RESOLUTION,
   CMD_EVENT_SET_FRAME_LIMIT,
   /* Loads core. */
   CMD_EVENT_LOAD_CORE_DEINIT,
   CMD_EVENT_LOAD_CORE,
   CMD_EVENT_LOAD_CORE_PERSIST,
   CMD_EVENT_UNLOAD_CORE,
   CMD_EVENT_LOAD_STATE,
   CMD_EVENT_SAVE_STATE,
   CMD_EVENT_SAVE_STATE_DECREMENT,
   CMD_EVENT_SAVE_STATE_INCREMENT,
   /* Takes screenshot. */
   CMD_EVENT_TAKE_SCREENSHOT,
   /* Quits RetroArch. */
   CMD_EVENT_QUIT,
   /* Reinitialize all drivers. */
   CMD_EVENT_REINIT,
   /* Toggles cheevos hardcore mode. */
   CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE,
   /* Deinitialize rewind. */
   CMD_EVENT_REWIND_DEINIT,
   /* Initializes rewind. */
   CMD_EVENT_REWIND_INIT,
   /* Toggles rewind. */
   CMD_EVENT_REWIND_TOGGLE,
   /* Deinitializes autosave. */
   CMD_EVENT_AUTOSAVE_DEINIT,
   /* Initializes autosave. */
   CMD_EVENT_AUTOSAVE_INIT,
   CMD_EVENT_AUTOSAVE_STATE,
   /* Stops audio. */
   CMD_EVENT_AUDIO_STOP,
   /* Starts audio. */
   CMD_EVENT_AUDIO_START,
   /* Mutes audio. */
   CMD_EVENT_AUDIO_MUTE_TOGGLE,
   /* Initializes overlay. */
   CMD_EVENT_OVERLAY_INIT,
   /* Deinitializes overlay. */
   CMD_EVENT_OVERLAY_DEINIT,
   /* Sets current scale factor for overlay. */
   CMD_EVENT_OVERLAY_SET_SCALE_FACTOR,
   /* Sets current alpha modulation for overlay. */
   CMD_EVENT_OVERLAY_SET_ALPHA_MOD,
   /* Cycle to next overlay. */
   CMD_EVENT_OVERLAY_NEXT,
   /* Deinitializes overlay. */
   CMD_EVENT_DSP_FILTER_INIT,
   /* Deinitializes graphics filter. */
   CMD_EVENT_DSP_FILTER_DEINIT,
   /* Deinitializes GPU recoring. */
   CMD_EVENT_GPU_RECORD_DEINIT,
   /* Initializes recording system. */
   CMD_EVENT_RECORD_INIT,
   /* Deinitializes recording system. */
   CMD_EVENT_RECORD_DEINIT,
   /* Deinitializes history playlist. */
   CMD_EVENT_HISTORY_DEINIT,
   /* Initializes history playlist. */
   CMD_EVENT_HISTORY_INIT,
   /* Deinitializes core information. */
   CMD_EVENT_CORE_INFO_DEINIT,
   /* Initializes core information. */
   CMD_EVENT_CORE_INFO_INIT,
   /* Deinitializes core. */
   CMD_EVENT_CORE_DEINIT,
   /* Initializes core. */
   CMD_EVENT_CORE_INIT,
   /* Set audio blocking state. */
   CMD_EVENT_AUDIO_SET_BLOCKING_STATE,
   /* Set audio nonblocking state. */
   CMD_EVENT_AUDIO_SET_NONBLOCKING_STATE,
   /* Apply video state changes. */
   CMD_EVENT_VIDEO_APPLY_STATE_CHANGES,
   /* Set video blocking state. */
   CMD_EVENT_VIDEO_SET_BLOCKING_STATE,
   /* Set video nonblocking state. */
   CMD_EVENT_VIDEO_SET_NONBLOCKING_STATE,
   /* Sets current aspect ratio index. */
   CMD_EVENT_VIDEO_SET_ASPECT_RATIO,
   CMD_EVENT_RESET_CONTEXT,
   /* Restarts RetroArch. */
   CMD_EVENT_RESTART_RETROARCH,
   /* Shutdown the OS */
   CMD_EVENT_SHUTDOWN,
   /* Reboot the OS */
   CMD_EVENT_REBOOT,
   /* Resume RetroArch when in menu. */
   CMD_EVENT_RESUME,
   /* Toggles pause. */
   CMD_EVENT_PAUSE_TOGGLE,
   /* Pauses RetroArch. */
   CMD_EVENT_UNPAUSE,
   /* Unpauses retroArch. */
   CMD_EVENT_PAUSE,
   CMD_EVENT_PAUSE_CHECKS,
   CMD_EVENT_MENU_SAVE_CURRENT_CONFIG,
   CMD_EVENT_MENU_SAVE_CONFIG,
   CMD_EVENT_MENU_PAUSE_LIBRETRO,
   /* Toggles menu on/off. */
   CMD_EVENT_MENU_TOGGLE,
   CMD_EVENT_MENU_REFRESH,
   /* Applies shader changes. */
   CMD_EVENT_SHADERS_APPLY_CHANGES,
   /* Initializes shader directory. */
   CMD_EVENT_SHADER_DIR_INIT,
   /* Deinitializes shader directory. */
   CMD_EVENT_SHADER_DIR_DEINIT,
   /* Initializes controllers. */
   CMD_EVENT_CONTROLLERS_INIT,
   CMD_EVENT_SAVEFILES,
   /* Initializes savefiles. */
   CMD_EVENT_SAVEFILES_INIT,
   /* Deinitializes savefiles. */
   CMD_EVENT_SAVEFILES_DEINIT,
   /* Initializes cheats. */
   CMD_EVENT_CHEATS_INIT,
   /* Deinitializes cheats. */
   CMD_EVENT_CHEATS_DEINIT,
   /* Apply cheats. */
   CMD_EVENT_CHEATS_APPLY,
   /* Deinitializes network system. */
   CMD_EVENT_NETWORK_DEINIT,
   /* Initializes network system. */
   CMD_EVENT_NETWORK_INIT,
   /* Initializes netplay system. */
   CMD_EVENT_NETPLAY_INIT,
   /* Deinitializes netplay system. */
   CMD_EVENT_NETPLAY_DEINIT,
   /* Flip netplay players. */
   CMD_EVENT_NETPLAY_FLIP_PLAYERS,
   /* Initializes BSV movie. */
   CMD_EVENT_BSV_MOVIE_INIT,
   /* Deinitializes BSV movie. */
   CMD_EVENT_BSV_MOVIE_DEINIT,
   /* Initializes command interface. */
   CMD_EVENT_COMMAND_INIT,
   /* Deinitialize command interface. */
   CMD_EVENT_COMMAND_DEINIT,
   /* Initializes remote gamepad interface. */
   CMD_EVENT_REMOTE_INIT,
   /* Deinitializes remote gamepad interface. */
   CMD_EVENT_REMOTE_DEINIT,
   /* Reinitializes audio driver. */
   CMD_EVENT_AUDIO_REINIT,
   /* Resizes windowed scale. Will reinitialize video driver. */
   CMD_EVENT_RESIZE_WINDOWED_SCALE,
   /* Deinitializes temporary content. */
   CMD_EVENT_TEMPORARY_CONTENT_DEINIT,
   CMD_EVENT_SUBSYSTEM_FULLPATHS_DEINIT,
   CMD_EVENT_LOG_FILE_DEINIT,
   /* Toggles disk eject. */
   CMD_EVENT_DISK_EJECT_TOGGLE,
   /* Cycle to next disk. */
   CMD_EVENT_DISK_NEXT,
   /* Cycle to previous disk. */
   CMD_EVENT_DISK_PREV,
   /* Appends disk image to disk image list. */
   CMD_EVENT_DISK_APPEND_IMAGE,
   /* Stops rumbling. */
   CMD_EVENT_RUMBLE_STOP,
   /* Toggles mouse grab. */
   CMD_EVENT_GRAB_MOUSE_TOGGLE,
   /* Toggles fullscreen mode. */
   CMD_EVENT_FULLSCREEN_TOGGLE,
   CMD_EVENT_PERFCNT_REPORT_FRONTEND_LOG,
   CMD_EVENT_VOLUME_UP,
   CMD_EVENT_VOLUME_DOWN,
   CMD_EVENT_EXEC
};

#ifdef HAVE_COMMAND
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
bool command_network_send(const char *cmd_);
#endif
#endif

bool command_network_new(
      command_t *handle,
      bool stdin_enable,
      bool network_enable,
      uint16_t port);

command_t *command_new(bool local_enable);

bool command_poll(command_t *handle);

bool command_get(command_handle_t *handle);

bool command_set(command_handle_t *handle);

bool command_free(command_t *handle);

/**
 * command_event:
 * @cmd                  : Command index.
 *
 * Performs RetroArch command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool command_event(enum event_command action, void *data);

#ifdef __cplusplus
}
#endif

#endif


/*  RetroArch - A frontend for libretro.
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

#ifndef COMMAND_EVENT_H__
#define COMMAND_EVENT_H__

#include <stdint.h>
#include <boolean.h>

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
#ifdef HAVE_FFMPEG
   EVENT_CMD_LOAD_CONTENT_FFMPEG,
#endif
   EVENT_CMD_LOAD_CONTENT_IMAGEVIEWER,
   EVENT_CMD_SET_PER_GAME_RESOLUTION,
   EVENT_CMD_SET_FRAME_LIMIT,
   /* Loads core. */
   EVENT_CMD_LOAD_CORE_DEINIT,
   EVENT_CMD_LOAD_CORE,
   EVENT_CMD_LOAD_CORE_PERSIST,
   EVENT_CMD_UNLOAD_CORE,
   EVENT_CMD_LOAD_STATE,
   EVENT_CMD_SAVE_STATE,
   EVENT_CMD_SAVE_STATE_DECREMENT,
   EVENT_CMD_SAVE_STATE_INCREMENT,
   /* Takes screenshot. */
   EVENT_CMD_TAKE_SCREENSHOT,
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
   /* Shutdown the OS */
   EVENT_CMD_SHUTDOWN,
   /* Resume RetroArch when in menu. */
   EVENT_CMD_RESUME,
   /* Toggles pause. */
   EVENT_CMD_PAUSE_TOGGLE,
   /* Pauses RetroArch. */
   EVENT_CMD_UNPAUSE,
   /* Unpauses retroArch. */
   EVENT_CMD_PAUSE,
   EVENT_CMD_PAUSE_CHECKS,
   EVENT_CMD_MENU_SAVE_CURRENT_CONFIG,
   EVENT_CMD_MENU_SAVE_CONFIG,
   EVENT_CMD_MENU_PAUSE_LIBRETRO,
   /* Toggles menu on/off. */
   EVENT_CMD_MENU_TOGGLE,
   EVENT_CMD_MENU_REFRESH,
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
   /* Initializes cheats. */
   EVENT_CMD_CHEATS_INIT,
   /* Deinitializes cheats. */
   EVENT_CMD_CHEATS_DEINIT,
   /* Apply cheats. */
   EVENT_CMD_CHEATS_APPLY,
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
   /* Initializes remote gamepad interface. */
   EVENT_CMD_REMOTE_INIT,
   /* Deinitializes remote gamepad interface. */
   EVENT_CMD_REMOTE_DEINIT,
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
   EVENT_CMD_VOLUME_DOWN
};

/**
 * event_disk_control_append_image:
 * @path                 : Path to disk image. 
 *
 * Appends disk image to disk image list.
 **/
void event_disk_control_append_image(const char *path);

/**
 * event_command:
 * @cmd                  : Command index.
 *
 * Performs RetroArch command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool event_command(enum event_command action);

#ifdef __cplusplus
}
#endif

#endif

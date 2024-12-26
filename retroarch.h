/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <lists/string_list.h>
#include <queues/task_queue.h>
#include <queues/message_queue.h>
#include "gfx/video_driver.h"

#include "core.h"

#include "driver.h"
#include "runloop.h"
#include "retroarch_types.h"

#define RETRO_ENVIRONMENT_RETROARCH_START_BLOCK 0x800000

#define RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND (2 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
                                            /* bool * --
                                            * Boolean value that tells the front end to save states in the
                                            * background or not.
                                            */

#define RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB (3 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
                                            /* retro_environment_t * --
                                            * Provides the callback to the frontend method which will cancel
                                            * all currently waiting threads.  Used when coordination is needed
                                            * between the core and the frontend to gracefully stop all threads.
                                            */

#define RETRO_ENVIRONMENT_POLL_TYPE_OVERRIDE (4 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
                                            /* unsigned * --
                                            * Tells the frontend to override the poll type behavior.
                                            * Allows the frontend to influence the polling behavior of the
                                            * frontend.
                                            *
                                            * Will be unset when retro_unload_game is called.
                                            *
                                            * 0 - Don't Care, no changes, frontend still determines polling type behavior.
                                            * 1 - Early
                                            * 2 - Normal
                                            * 3 - Late
                                            */

#define DRIVERS_CMD_ALL \
      ( DRIVER_AUDIO_MASK \
      | DRIVER_MICROPHONE_MASK \
      | DRIVER_VIDEO_MASK \
      | DRIVER_INPUT_MASK \
      | DRIVER_CAMERA_MASK \
      | DRIVER_LOCATION_MASK \
      | DRIVER_MENU_MASK \
      | DRIVERS_VIDEO_INPUT_MASK \
      | DRIVER_BLUETOOTH_MASK \
      | DRIVER_WIFI_MASK \
      | DRIVER_LED_MASK \
      | DRIVER_MIDI_MASK )


RETRO_BEGIN_DECLS

enum rarch_state_flags
{
   RARCH_FLAGS_HAS_SET_USERNAME             = (1 << 0),
   RARCH_FLAGS_HAS_SET_VERBOSITY            = (1 << 1),
   RARCH_FLAGS_HAS_SET_LIBRETRO             = (1 << 2),
   RARCH_FLAGS_HAS_SET_LIBRETRO_DIRECTORY   = (1 << 3),
   RARCH_FLAGS_HAS_SET_SAVE_PATH            = (1 << 4),
   RARCH_FLAGS_HAS_SET_STATE_PATH           = (1 << 5),
   RARCH_FLAGS_HAS_SET_UPS_PREF             = (1 << 6),
   RARCH_FLAGS_HAS_SET_BPS_PREF             = (1 << 7),
   RARCH_FLAGS_HAS_SET_IPS_PREF             = (1 << 8),
   RARCH_FLAGS_HAS_SET_LOG_TO_FILE          = (1 << 9),
   RARCH_FLAGS_UPS_PREF                     = (1 << 10),
   RARCH_FLAGS_BPS_PREF                     = (1 << 11),
   RARCH_FLAGS_IPS_PREF                     = (1 << 12),
   RARCH_FLAGS_BLOCK_CONFIG_READ            = (1 << 13),
   RARCH_FLAGS_CLI_DATABASE_SCAN            = (1 << 14),
   RARCH_FLAGS_HAS_SET_XDELTA_PREF          = (1 << 15),
   RARCH_FLAGS_XDELTA_PREF                  = (1 << 16)
};

bool retroarch_ctl(enum rarch_ctl_state state, void *data);

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len);

void retroarch_override_setting_set(enum rarch_override_setting enum_idx, void *data);

void retroarch_override_setting_unset(enum rarch_override_setting enum_idx, void *data);

bool retroarch_override_setting_is_set(enum rarch_override_setting enum_idx, void *data);

const char* video_shader_get_current_shader_preset(void);

/**
 * retroarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Initializes the program.
 *
 * @return true on success, otherwise false if there was an error.
 **/
bool retroarch_main_init(int argc, char *argv[]);

bool retroarch_main_quit(void);

global_t *global_get_ptr(void);

content_state_t *content_state_get_ptr(void);

unsigned content_get_subsystem_rom_id(void);

int content_get_subsystem(void);

void retroarch_menu_running(void);

void retroarch_menu_running_finished(bool quit);

enum retro_language retroarch_get_language_from_iso(const char *lang);

void retroarch_favorites_init(void);

void retroarch_favorites_deinit(void);

/* Audio */

/**
 * config_get_audio_driver_options:
 *
 * Get an enumerated list of all audio driver names, separated by '|'.
 *
 * Returns: string listing of all audio driver names, separated by '|'.
 **/
const char* config_get_audio_driver_options(void);

#ifdef HAVE_MICROPHONE
/**
 * config_get_microphone_driver_options:
 *
 * Get an enumerated list of all microphone driver names, separated by '|'.
 *
 * Returns: string listing of all microphone driver names, separated by '|'.
 **/
const char* config_get_microphone_driver_options(void);
#endif

/* Camera */

/*
   Returns rotation requested by the core regardless of if it has been
   applied with the final video rotation
*/
unsigned int retroarch_get_core_requested_rotation(void);

/*
   Returns final rotation including both user chosen video rotation
   and core requested rotation if allowed by video_allow_rotate
*/
unsigned int retroarch_get_rotation(void);

void retroarch_init_task_queue(void);

/* Creates folder and core options stub file for subsequent runs */
bool core_options_create_override(bool game_specific);
bool core_options_remove_override(bool game_specific);
void core_options_reset(void);
void core_options_flush(void);

/**
 * retroarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
void retroarch_fail(int error_code, const char *error);

uint16_t retroarch_get_flags(void);

RETRO_END_DECLS

#endif

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

#ifndef __MSG_HASH_H
#define __MSG_HASH_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

enum msg_hash_enums
{
   MSG_UNKNOWN = 0,
   MSG_PROGRAM,
   MSG_FOUND_SHADER,
   MSG_LOADING_HISTORY_FILE,
   MSG_SRAM_WILL_NOT_BE_SAVED,
   MSG_RECEIVED,
   MSG_LOADING_CONTENT_FILE,
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   MSG_FAILED_TO_START_RECORDING,
   MSG_REWIND_INIT,
   MSG_REWIND_INIT_FAILED,
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   MSG_REWIND_INIT_FAILED_NO_SAVESTATES,
   MSG_LIBRETRO_ABI_BREAK,
   MSG_NETPLAY_FAILED,
   MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED,
   MSG_DETECTED_VIEWPORT_OF,
   MSG_RECORDING_TO,
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   MSG_AUTOSAVE_FAILED,
   MSG_MOVIE_RECORD_STOPPED,
   MSG_MOVIE_PLAYBACK_ENDED,
   MSG_TAKING_SCREENSHOT,
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   MSG_CUSTOM_TIMING_GIVEN,
   MSG_SAVING_STATE,
   MSG_LOADING_STATE,
   MSG_FAILED_TO_SAVE_STATE_TO,
   MSG_FAILED_TO_SAVE_SRAM,
   MSG_STATE_SIZE,
   MSG_FAILED_TO_LOAD_CONTENT,
   MSG_COULD_NOT_READ_CONTENT_FILE,
   MSG_SAVED_SUCCESSFULLY_TO,
   MSG_BYTES,
   MSG_BLOCKING_SRAM_OVERWRITE,
   MSG_UNRECOGNIZED_COMMAND,
   MSG_SENDING_COMMAND,
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   MSG_REWINDING,
   MSG_SLOW_MOTION_REWIND,
   MSG_SLOW_MOTION,
   MSG_REWIND_REACHED_END,
   MSG_FAILED_TO_START_MOVIE_RECORD,
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   MSG_STATE_SLOT,
   MSG_STARTING_MOVIE_RECORD_TO,
   MSG_FAILED_TO_APPLY_SHADER,
   MSG_APPLYING_SHADER,
   MSG_SHADER,
   MSG_REDIRECTING_SAVESTATE_TO,
   MSG_REDIRECTING_SAVEFILE_TO,
   MSG_REDIRECTING_CHEATFILE_TO,
   MSG_SCANNING,
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   MSG_COULD_NOT_PROCESS_ZIP_FILE,
   MSG_LOADED_STATE_FROM_SLOT,
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   MSG_STARTING_MOVIE_PLAYBACK,
   MSG_APPENDED_DISK,
   MSG_SKIPPING_SRAM_LOAD,
   MSG_CONFIG_DIRECTORY_NOT_SET,
   MSG_SAVED_STATE_TO_SLOT,
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   MSG_FAILED_TO_LOAD_STATE,
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   MSG_RESET,
   MSG_AUDIO_MUTED,
   MSG_AUDIO_UNMUTED,
   MSG_FAILED_TO_UNMUTE_AUDIO,
   MSG_FAILED_TO_LOAD_OVERLAY,
   MSG_PAUSED,
   MSG_UNPAUSED,
   MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
   MSG_GRAB_MOUSE_STATE,
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   MSG_FAILED_TO,
   MSG_SAVING_RAM_TYPE,
   MSG_TO,
   MSG_VIRTUAL_DISK_TRAY,
   MSG_REMOVED_DISK_FROM_TRAY,
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   MSG_GOT_INVALID_DISK_INDEX,
   MSG_TASK_FAILED,
   MSG_DOWNLOADING,
   MSG_EXTRACTING
};

RETRO_BEGIN_DECLS

const char *msg_hash_to_str(enum msg_hash_enums msg);

const char *msg_hash_to_str_fr(enum msg_hash_enums msg);

#ifdef HAVE_UTF8
const char *msg_hash_to_str_ru(enum msg_hash_enums msg);
#endif

const char *msg_hash_to_str_de(enum msg_hash_enums msg);

const char *msg_hash_to_str_es(enum msg_hash_enums msg);

const char *msg_hash_to_str_eo(enum msg_hash_enums msg);

const char *msg_hash_to_str_it(enum msg_hash_enums msg);

const char *msg_hash_to_str_pt(enum msg_hash_enums msg);

const char *msg_hash_to_str_pl(enum msg_hash_enums msg);

const char *msg_hash_to_str_nl(enum msg_hash_enums msg);

const char *msg_hash_to_str_us(enum msg_hash_enums msg);

uint32_t msg_hash_calculate(const char *s);

RETRO_END_DECLS

#endif


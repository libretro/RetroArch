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

#include "../msg_hash.h"

const char *msg_hash_to_str_us(uint32_t hash)
{
   switch (hash)
   {
      case MSG_PROGRAM:
         return "RetroArch";
      case MSG_MOVIE_RECORD_STOPPED:
         return "Stopping movie record.";
      case MSG_MOVIE_PLAYBACK_ENDED:
         return "Movie playback ended.";
      case MSG_AUTOSAVE_FAILED:
         return "Could not initialize autosave.";
      case MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED:
         return "Movie playback has started. Cannot start netplay.";
      case MSG_NETPLAY_FAILED:
         return "Failed to initialize netplay.";
      case MSG_LIBRETRO_ABI_BREAK:
         return "is compiled against a different version of libretro than this libretro implementation.";
      case MSG_REWIND_INIT_FAILED_NO_SAVESTATES:
         return "Implementation does not support save states. Cannot use rewind.";
      case MSG_REWIND_INIT_FAILED_THREADED_AUDIO:
         return "Implementation uses threaded audio. Cannot use rewind.";
      case MSG_REWIND_INIT_FAILED:
         return "Failed to initialize rewind buffer. Rewinding will be disabled.";
      case MSG_REWIND_INIT:
         return "Initializing rewind buffer with size";
      case MSG_CUSTOM_TIMING_GIVEN:
         return "Custom timing given";
      case MSG_VIEWPORT_SIZE_CALCULATION_FAILED:
         return "Viewport size calculation failed! Will continue using raw data. This will probably not work right ...";
      case MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING:
         return "Libretro core is hardware rendered. Must use post-shaded recording as well.";
      case MSG_RECORDING_TO:
         return "Recording to";
      case MSG_DETECTED_VIEWPORT_OF:
         return "Detected viewport of";
      case MSG_TAKING_SCREENSHOT:
         return "Taking screenshot.";
      case MSG_FAILED_TO_TAKE_SCREENSHOT:
         return "Failed to take screenshot.";
      case MSG_FAILED_TO_START_RECORDING:
         return "Failed to start recording.";
      case MSG_RECORDING_TERMINATED_DUE_TO_RESIZE:
         return "Recording terminated due to resize.";
      case MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED:
         return "Using libretro dummy core. Skipping recording.";
      case MSG_UNKNOWN:
         return "Unknown";
      case MSG_LOADING_CONTENT_FILE:
         return "Loading content file";
      case MSG_RECEIVED:
         return "received";
      case MSG_UNRECOGNIZED_COMMAND:
         return "Unrecognized command";
      case MSG_SENDING_COMMAND:
         return "Sending command";
      case MSG_GOT_INVALID_DISK_INDEX:
         return "Got invalid disk index.";
      case MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY:
         return "Failed to remove disk from tray.";
      case MSG_REMOVED_DISK_FROM_TRAY:
         return "Removed disk from tray.";
      case MSG_VIRTUAL_DISK_TRAY:
         return "virtual disk tray.";
      case MSG_FAILED_TO:
         return "Failed to";
      case MSG_TO:
         return "to";
      case MSG_SAVING_RAM_TYPE:
         return "Saving RAM type";
      case MSG_SAVING_STATE:
         return "Saving state";
      case MSG_LOADING_STATE:
         return "Loading state";
      case MSG_FAILED_TO_LOAD_MOVIE_FILE:
         return "Failed to load movie file";
      case MSG_FAILED_TO_LOAD_CONTENT:
         return "Failed to load content";
      case MSG_COULD_NOT_READ_CONTENT_FILE:
         return "Could not read content file";
      case MSG_GRAB_MOUSE_STATE:
         return "Grab mouse state";
      case MSG_PAUSED:
         return "Paused.";
      case MSG_UNPAUSED:
         return "Unpaused.";
      case MSG_FAILED_TO_LOAD_OVERLAY:
         return "Failed to load overlay.";
      case MSG_FAILED_TO_UNMUTE_AUDIO:
         return "Failed to unmute audio.";
      case MSG_AUDIO_MUTED:
         return "Audio muted.";
      case MSG_AUDIO_UNMUTED:
         return "Audio unmuted.";
      case MSG_RESET:
         return "Reset";
      case MSG_FAILED_TO_LOAD_STATE:
         return "Failed to load state from";
      case MSG_FAILED_TO_SAVE_STATE_TO:
         return "Failed to save state to";
      case MSG_FAILED_TO_SAVE_SRAM:
         return "Failed to save SRAM";
      case MSG_STATE_SIZE:
         return "State size";
      case MSG_FOUND_SHADER:
         return "Found shader";
      case MSG_SRAM_WILL_NOT_BE_SAVED:
         return "SRAM will not be saved.";
      case MSG_BLOCKING_SRAM_OVERWRITE:
         return "Blocking SRAM Overwrite";
      case MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES:
         return "Core does not support save states.";
      case MSG_SAVED_STATE_TO_SLOT:
         return "Saved state to slot";
      case MSG_SAVED_SUCCESSFULLY_TO:
         return "Saved successfully to";
      case MSG_BYTES:
         return "bytes";
      case MSG_CONFIG_DIRECTORY_NOT_SET:
         return "Config directory not set. Cannot save new config.";
      case MSG_SKIPPING_SRAM_LOAD:
         return "Skipping SRAM load.";
      case MSG_APPENDED_DISK:
         return "Appended disk";
      case MSG_STARTING_MOVIE_PLAYBACK:
         return "Starting movie playback.";
      case MSG_FAILED_TO_REMOVE_TEMPORARY_FILE:
         return "Failed to remove temporary file";
      case MSG_REMOVING_TEMPORARY_CONTENT_FILE:
         return "Removing temporary content file";
      case MSG_LOADED_STATE_FROM_SLOT:
         return "Loaded state from slot";
      case MSG_COULD_NOT_PROCESS_ZIP_FILE:
         return "Could not process ZIP file.";
      case MSG_SCANNING_OF_DIRECTORY_FINISHED:
         return "Scanning of directory finished";
      case MSG_SCANNING:
         return "Scanning";
      case MSG_REDIRECTING_CHEATFILE_TO:
         return "Redirecting cheat file to";
      case MSG_REDIRECTING_SAVEFILE_TO:
         return "Redirecting save file to";
      case MSG_REDIRECTING_SAVESTATE_TO:
         return "Redirecting savestate to";
      case MSG_SHADER:
         return "Shader";
      case MSG_APPLYING_SHADER:
         return "Applying shader";
      case MSG_FAILED_TO_APPLY_SHADER:
         return "Failed to apply shader.";
      case MSG_STARTING_MOVIE_RECORD_TO:
         return "Starting movie record to";
      case MSG_FAILED_TO_START_MOVIE_RECORD:
         return "Failed to start movie record.";
      case MSG_STATE_SLOT:
         return "State slot";
      case MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT:
         return "Restarting recording due to driver reinit.";
      case MSG_SLOW_MOTION:
         return "Slow motion.";
      case MSG_SLOW_MOTION_REWIND:
         return "Slow motion rewind.";
      case MSG_REWINDING:
         return "Rewinding.";
      case MSG_REWIND_REACHED_END:
         return "Reached end of rewind buffer.";
      case MSG_CHEEVOS_HARDCORE_MODE_ENABLE:
         return "Hardcore Mode Enabled: savestate & rewind were disabled.";
      case MSG_TASK_FAILED:
         return "Failed";
      case MSG_DOWNLOADING:
         return "Downloading";
      case MSG_EXTRACTING:
         return "Extracting";
      default:
         break;
   }

   return "null";
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
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
      case MSG_DOWNLOAD_PROGRESS:
         return "Download progress";
      case MSG_COULD_NOT_PROCESS_ZIP_FILE:
         return "Could not process ZIP file.";
      case MSG_DOWNLOAD_COMPLETE:
         return "Download complete";
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
      default:
         break;
   }

   return "null";
}

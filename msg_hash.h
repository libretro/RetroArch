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


#ifndef __MSG_HASH_H
#define __MSG_HASH_H

#include <stdint.h>
#include <stddef.h>


#define MSG_RECEIVED                                  0xfe0c06acU

#define MSG_UNRECOGNIZED_COMMAND                      0x946b8a50U

#define MSG_SENDING_COMMAND                           0x562cf28bU

#define MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT 0x5aa753b8U

#define MSG_REWINDING                                 0xccbeec2cU
#define MSG_SLOW_MOTION_REWIND                        0x385adb27U
#define MSG_SLOW_MOTION                               0x744c437fU
#define MSG_REWIND_REACHED_END                        0x4f1aab8fU
#define MSG_FAILED_TO_START_MOVIE_RECORD              0x61221776U

#define MSG_STATE_SLOT                                0x27b67f67U
#define MSG_STARTING_MOVIE_RECORD_TO                  0x6a7e0d50U
#define MSG_FAILED_TO_START_MOVIE_RECORD              0x61221776U

#define MSG_FAILED_TO_APPLY_SHADER                    0x2094eb67U
#define MSG_APPLYING_SHADER                           0x35599b7fU
#define MSG_SHADER                                    0x1bb1211cU

#define MSG_REDIRECTING_SAVESTATE_TO                  0x8d98f7a6U
#define MSG_REDIRECTING_SAVEFILE_TO                   0x868c54a5U
#define MSG_REDIRECTING_CHEATFILE_TO                  0xd5f1b27bU

#define MSG_SCANNING                                  0x4c547516U
#define MSG_SCANNING_OF_DIRECTORY_FINISHED            0x399632a7U

#define MSG_DOWNLOAD_COMPLETE                         0x4b9c4f75U
#define MSG_COULD_NOT_PROCESS_ZIP_FILE                0xc18c89bbU
#define MSG_DOWNLOAD_PROGRESS                         0x35ed9411U

#define MSG_LOADED_STATE_FROM_SLOT                    0xadb48582U

#define MSG_REMOVING_TEMPORARY_CONTENT_FILE           0x7121c9e7U
#define MSG_FAILED_TO_REMOVE_TEMPORARY_FILE           0xb6707b1aU

#define MSG_STARTING_MOVIE_PLAYBACK                   0x96e545b6U

#define MSG_APPENDED_DISK                             0x814ea0f0U

#define MSG_SKIPPING_SRAM_LOAD                        0x88d4c8dbU

#define MSG_CONFIG_DIRECTORY_NOT_SET                  0xcd45252aU

#define MSG_SAVED_STATE_TO_SLOT                       0xe1e3dc3bU

#define MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES          0xd50adf46U
#define MSG_FAILED_TO_LOAD_STATE                      0x91f348ebU

#define MSG_RESET                                     0x10474288U

#define MSG_AUDIO_MUTED                               0xfa0c3bd5U
#define MSG_AUDIO_UNMUTED                             0x0512bab8U
#define MSG_FAILED_TO_UNMUTE_AUDIO                    0xf698763aU

#define MSG_FAILED_TO_LOAD_OVERLAY                    0xacf201ecU

#define MSG_PAUSED                                    0x143e3307U
#define MSG_UNPAUSED                                  0x95aede0aU

#define MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS        0x6ba5abf9U

#define MSG_GRAB_MOUSE_STATE                          0x893a7329U

#define MSG_FAILED_TO_LOAD_MOVIE_FILE                 0x9455a5a9U

#define MSG_FAILED_TO                                 0x768f6dacU

#define MSG_SAVING_RAM_TYPE                           0x9cd21d2dU

#define MSG_TO                                        0x005979a8U

#define MSG_VIRTUAL_DISK_TRAY                         0x4aa37f15U
#define MSG_REMOVED_DISK_FROM_TRAY                    0xf26a9653U

#define MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY           0xc1c9a655U

#define MSG_GOT_INVALID_DISK_INDEX                    0xb138dd76U

const char *msg_hash_to_str(uint32_t hash);

const char *msg_hash_to_str_fr(uint32_t hash);

const char *msg_hash_to_str_de(uint32_t hash);

const char *msg_hash_to_str_es(uint32_t hash);

const char *msg_hash_to_str_eo(uint32_t hash);

const char *msg_hash_to_str_it(uint32_t hash);

const char *msg_hash_to_str_pt(uint32_t hash);

const char *msg_hash_to_str_nl(uint32_t hash);

const char *msg_hash_to_str_us(uint32_t hash);

uint32_t msg_hash_calculate(const char *s);

#endif


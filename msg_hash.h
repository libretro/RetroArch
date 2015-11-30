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

#define MSG_UNKNOWN                                   0x3a834e55U
#define MSG_PROGRAM                                   0xc339565dU
#define MSG_FOUND_SHADER                              0x817f42b7U

#define MSG_LOADING_HISTORY_FILE                      0x865210d3U
#define MSG_SRAM_WILL_NOT_BE_SAVED                    0x16f17d61U

#define MSG_RECEIVED                                  0xfe0c06acU

#define MSG_LOADING_CONTENT_FILE                      0x236398dcU

#define MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED  0x9e8a1febU
#define MSG_RECORDING_TERMINATED_DUE_TO_RESIZE        0x361a07feU
#define MSG_FAILED_TO_START_RECORDING                 0x90c3e2d5U

#define MSG_REWIND_INIT                               0xf7732001U
#define MSG_REWIND_INIT_FAILED                        0x9c1db0a6U
#define MSG_REWIND_INIT_FAILED_THREADED_AUDIO         0x359001b6U
#define MSG_REWIND_INIT_FAILED_NO_SAVESTATES          0x979b9cc3U

#define MSG_LIBRETRO_ABI_BREAK                        0xf02cccd7U

#define MSG_NETPLAY_FAILED                            0x61ee3426U
#define MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED 0xb1e5dbfcU

#define MSG_DETECTED_VIEWPORT_OF                      0xdf7002baU
#define MSG_RECORDING_TO                              0x189fd324U
#define MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING 0x7f9f7659U
#define MSG_VIEWPORT_SIZE_CALCULATION_FAILED          0x9da84911U

#define MSG_AUTOSAVE_FAILED                           0x9a02d8d1U

#define MSG_MOVIE_RECORD_STOPPED                      0xbc7832c1U
#define MSG_MOVIE_PLAYBACK_ENDED                      0xbeadce2aU

#define MSG_TAKING_SCREENSHOT                         0xdcfda0e0U
#define MSG_FAILED_TO_TAKE_SCREENSHOT                 0x7a480a2dU

#define MSG_CUSTOM_TIMING_GIVEN                       0x259c95dfU

#define MSG_SAVING_STATE                              0xe4f3eb4dU
#define MSG_LOADING_STATE                             0x68d8d483U
#define MSG_FAILED_TO_SAVE_STATE_TO                   0xcc005f3cU
#define MSG_FAILED_TO_SAVE_SRAM                       0x0f72de6cU
#define MSG_STATE_SIZE                                0x27b67400U

#define MSG_FAILED_TO_LOAD_CONTENT                    0x0186e5a5U
#define MSG_COULD_NOT_READ_CONTENT_FILE               0x2dc7f4a0U
#define MSG_SAVED_SUCCESSFULLY_TO                     0x9f59a7deU

#define MSG_BYTES                                     0x0f30b64cU

#define MSG_BLOCKING_SRAM_OVERWRITE                   0x1f91d486U

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

#define MSG_COULD_NOT_PROCESS_ZIP_FILE                0xc18c89bbU

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

#define MSG_TASK_FAILED                               0xb23ed64aU
#define MSG_DOWNLOADING                               0x465305dbU
#define MSG_EXTRACTING                                0x25a4c19eU

const char *msg_hash_to_str(uint32_t hash);

const char *msg_hash_to_str_fr(uint32_t hash);

const char *msg_hash_to_str_de(uint32_t hash);

const char *msg_hash_to_str_es(uint32_t hash);

const char *msg_hash_to_str_eo(uint32_t hash);

const char *msg_hash_to_str_it(uint32_t hash);

const char *msg_hash_to_str_pt(uint32_t hash);

const char *msg_hash_to_str_pl(uint32_t hash);

const char *msg_hash_to_str_nl(uint32_t hash);

const char *msg_hash_to_str_us(uint32_t hash);

uint32_t msg_hash_calculate(const char *s);

#endif


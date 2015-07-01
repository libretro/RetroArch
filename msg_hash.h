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


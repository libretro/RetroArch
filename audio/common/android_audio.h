/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __ANDROID_AUDIO_H
#define __ANDROID_AUDIO_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct string_list;

/* The outputs AudioManager lists, as "<id> (<name>, <type>)" entries;
 * NULL when they cannot be read. Caller frees. */
struct string_list *frontend_android_audio_outputs_new(void);

/* What the output with this id says it takes: the most PCM channels
 * (0 when it lists none, which means any count) and whether AC-3 is
 * among its encodings. False when no output has the id. */
bool frontend_android_audio_device_caps(int32_t id,
      unsigned *max_channels, bool *ac3);

RETRO_END_DECLS

#endif

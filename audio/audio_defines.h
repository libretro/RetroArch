/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __AUDIO_DEFINES__H
#define __AUDIO_DEFINES__H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define AUDIO_CHUNK_SIZE_BLOCKING      512

/* So we don't get complete line-noise when fast-forwarding audio. */
#define AUDIO_CHUNK_SIZE_NONBLOCKING   2048

#define AUDIO_MAX_RATIO                16

#define AUDIO_MIXER_MAX_STREAMS        16

#define AUDIO_MIXER_MAX_SYSTEM_STREAMS (AUDIO_MIXER_MAX_STREAMS + 4)

/* do not define more than (MAX_SYSTEM_STREAMS - MAX_STREAMS) */
enum audio_mixer_system_slot
{
   AUDIO_MIXER_SYSTEM_SLOT_OK = AUDIO_MIXER_MAX_STREAMS,
   AUDIO_MIXER_SYSTEM_SLOT_CANCEL,
   AUDIO_MIXER_SYSTEM_SLOT_NOTICE,
   AUDIO_MIXER_SYSTEM_SLOT_BGM
};

enum audio_action
{
   AUDIO_ACTION_NONE = 0,
   AUDIO_ACTION_RATE_CONTROL_DELTA,
   AUDIO_ACTION_MIXER_MUTE_ENABLE,
   AUDIO_ACTION_MUTE_ENABLE,
   AUDIO_ACTION_VOLUME_GAIN,
   AUDIO_ACTION_MIXER_VOLUME_GAIN,
   AUDIO_ACTION_MIXER
};

enum audio_mixer_slot_selection_type
{
   AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC = 0,
   AUDIO_MIXER_SLOT_SELECTION_MANUAL
};

enum audio_mixer_stream_type
{
   AUDIO_STREAM_TYPE_NONE = 0,
   AUDIO_STREAM_TYPE_USER,
   AUDIO_STREAM_TYPE_SYSTEM
};

enum audio_mixer_state
{
   AUDIO_STREAM_STATE_NONE = 0,
   AUDIO_STREAM_STATE_STOPPED,
   AUDIO_STREAM_STATE_PLAYING,
   AUDIO_STREAM_STATE_PLAYING_LOOPED,
   AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL
};

typedef struct audio_statistics
{
   float average_buffer_saturation;
   float std_deviation_percentage;
   float close_to_underrun;
   float close_to_blocking;
   unsigned samples;
} audio_statistics_t;

RETRO_END_DECLS

#endif

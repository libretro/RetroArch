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
#define AUDIO_MIN_RATIO                0.0625

#define AUDIO_MIXER_MAX_STREAMS        16

#define AUDIO_MIXER_MAX_SYSTEM_STREAMS (AUDIO_MIXER_MAX_STREAMS + 8)

/* Fastforward timing calculations running average samples. Helps with a
consistent pitch when fast-forwarding. */
#define AUDIO_FF_EXP_AVG_SAMPLES       16

/* do not define more than (MAX_SYSTEM_STREAMS - MAX_STREAMS) */
enum audio_mixer_system_slot
{
   AUDIO_MIXER_SYSTEM_SLOT_OK = AUDIO_MIXER_MAX_STREAMS,
   AUDIO_MIXER_SYSTEM_SLOT_CANCEL,
   AUDIO_MIXER_SYSTEM_SLOT_NOTICE,
   AUDIO_MIXER_SYSTEM_SLOT_NOTICE_BACK,
   AUDIO_MIXER_SYSTEM_SLOT_BGM,
   AUDIO_MIXER_SYSTEM_SLOT_ACHIEVEMENT_UNLOCK,
   AUDIO_MIXER_SYSTEM_SLOT_UP,
   AUDIO_MIXER_SYSTEM_SLOT_DOWN
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

/**
 * Bit flags that describe the current state of the audio driver.
 */
enum audio_driver_state_flags
{
   /**
    * Indicates that the driver was successfully created
    * and is currently valid.
    * You may submit samples for output at any time.
    *
    * This flag does \em not mean that the player will hear anything;
    * the driver might be suspended.
    *
    * @see AUDIO_FLAG_SUSPENDED
    */
   AUDIO_FLAG_ACTIVE       = (1 << 0),

   /**
    * Indicates that the audio driver outputs floating-point samples,
    * as opposed to integer samples.
    *
    * All audio is sent through the resampler,
    * which operates on floating-point samples.
    *
    * If this flag is set, then the resampled output doesn't need
    * to be converted back to \c int16_t format.
    *
    * This won't affect the audio that the core writes;
    * either way, it's supposed to output \c int16_t samples.
    *
    * This flag won't be set if the selected audio driver
    * doesn't support (or is configured to not use) \c float samples.
    *
    * @see audio_driver_t::use_float
    */
   AUDIO_FLAG_USE_FLOAT    = (1 << 1),

   /**
    * Indicates that the audio driver is not currently rendering samples,
    * although it's valid and can be resumed.
    *
    * Usually set when RetroArch needs to simulate audio output
    * without actually rendering samples (e.g. runahead),
    * or when reinitializing the driver.
    *
    * Samples will still be accepted, but they will be silently dropped.
    */
   AUDIO_FLAG_SUSPENDED    = (1 << 2),

   /**
    * Indicates that the audio mixer is available
    * and can mix one or more audio streams.
    *
    * Will not be set if RetroArch was built without \c HAVE_AUDIOMIXER.
    */
   AUDIO_FLAG_MIXER_ACTIVE = (1 << 3),

   /**
    * Indicates that the frontend will never need audio from the core,
    * usually when runahead is active.
    *
    * When set, any audio received by the core will not be processed.
    *
    * Will not be set if RetroArch was built without \c HAVE_RUNAHEAD.
    *
    * @see RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE
    */
   AUDIO_FLAG_HARD_DISABLE = (1 << 4),

   /**
    * Indicates that audio rate control is enabled.
    * This means that the audio system will adjust the rate at which
    * it sends samples to the driver,
    * minimizing the occurrences of buffer overrun or underrun.
    *
    * @see audio_driver_t::write_avail
    * @see audio_driver_t::buffer_size
    */
   AUDIO_FLAG_CONTROL      = (1 << 5),

   /**
    * Indicates that the audio driver is forcing gain to 0.
    * Used for temporary rewind and fast-forward muting.
    */
   AUDIO_FLAG_MUTED        = (1 << 6)
};

typedef struct audio_statistics
{
   unsigned samples;
   float average_buffer_saturation;
   float std_deviation_percentage;
   float close_to_underrun;
   float close_to_blocking;
} audio_statistics_t;

RETRO_END_DECLS

#endif

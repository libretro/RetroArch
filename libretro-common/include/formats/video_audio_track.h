/* SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_COMMON_VIDEO_AUDIO_TRACK_H
#define LIBRETRO_COMMON_VIDEO_AUDIO_TRACK_H

#include <stddef.h>
#include <stdint.h>

enum video_audio_codec
{
   VIDEO_AUDIO_UNKNOWN = 0,
   VIDEO_AUDIO_AAC,
   VIDEO_AUDIO_OPUS,
   VIDEO_AUDIO_VORBIS,
   VIDEO_AUDIO_FLAC,
   VIDEO_AUDIO_AC3
};

enum video_audio_language
{
   VIDEO_AUDIO_LANGUAGE_NONE = 0,
   VIDEO_AUDIO_LANGUAGE_TEXT,
   VIDEO_AUDIO_LANGUAGE_MP4_PACKED
};

/* Metadata borrows the open container and its input. Query on the decoder
 * owner, before publication or between decoding calls; no cursor advances. */
typedef struct video_audio_track
{
   const char *codec_id;
   const uint8_t *language;
   size_t language_size;
   unsigned sample_rate, channels;
   int index; /* demux index, including non-audio tracks */
   enum video_audio_codec codec;
   enum video_audio_language language_kind;
} video_audio_track_t;

#endif

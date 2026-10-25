/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VIDEO_FIXTURE_STREAM_H
#define VIDEO_FIXTURE_STREAM_H

#include <stdlib.h>
#include <string.h>
#include <retro_inline.h>
#include <formats/rmp4_video.h>
#include <formats/rwebm_video.h>
#include <formats/video_hw_frame.h>

typedef struct video_fixture_stream
{
   void *handle;
   int mp4;
} video_fixture_stream_t;

static INLINE video_fixture_stream_t *video_fixture_stream_open(const uint8_t *data,
      size_t size)
{
   video_fixture_stream_t *s;
   if (!data || size < 4) return NULL;
   s = (video_fixture_stream_t*)calloc(1, sizeof(*s));
   if (!s) return NULL;
   s->mp4 = memcmp(data, "\x1a\x45\xdf\xa3", 4) != 0;
   s->handle = s->mp4 ? (void*)rmp4_video_stream_open(data, size)
      : (void*)rwebm_video_stream_open(data, size);
   if (s->handle) return s;
   free(s);
   return NULL;
}

static INLINE void video_fixture_stream_close(video_fixture_stream_t *s)
{
   if (!s) return;
   if (s->mp4) rmp4_video_stream_close(s->handle);
   else rwebm_video_stream_close(s->handle);
   free(s);
}

static INLINE void *video_fixture_stream_h264(video_fixture_stream_t *s)
{
   return s->mp4 ? rmp4_video_stream_h264(s->handle)
      : rwebm_video_stream_h264(s->handle);
}

static INLINE void *video_fixture_stream_h265(video_fixture_stream_t *s)
{
   return s->mp4 ? rmp4_video_stream_h265(s->handle)
      : rwebm_video_stream_h265(s->handle);
}

static INLINE int video_fixture_stream_is_hdr(video_fixture_stream_t *s)
{
   return s->mp4 ? rmp4_video_stream_is_hdr(s->handle)
      : rwebm_video_stream_is_hdr(s->handle);
}

static INLINE void video_fixture_stream_get_info(video_fixture_stream_t *s,
      unsigned *width, unsigned *height, int *frames, int *loops)
{
   if (s->mp4) rmp4_video_stream_get_info(s->handle, width, height, frames, loops);
   else rwebm_video_stream_get_info(s->handle, width, height, frames, loops);
}

static INLINE int video_fixture_stream_set_hw(video_fixture_stream_t *s, int enable)
{
   return s->mp4 ? rmp4_video_stream_set_hw(s->handle, enable)
      : rwebm_video_stream_set_hw(s->handle, enable);
}

static INLINE const uint32_t *video_fixture_stream_next(video_fixture_stream_t *s, int *duration)
{
   return s->mp4 ? rmp4_video_stream_next(s->handle, duration)
      : rwebm_video_stream_next(s->handle, duration);
}

static INLINE int64_t video_fixture_stream_frame_time_ns(video_fixture_stream_t *s)
{
   return s->mp4 ? rmp4_video_stream_frame_time_ns(s->handle)
      : rwebm_video_stream_frame_time_ns(s->handle);
}

static INLINE int video_fixture_stream_hw_frame(video_fixture_stream_t *s, video_hw_frame_t *frame)
{
   return s->mp4 ? rmp4_video_stream_hw_frame(s->handle, frame)
      : rwebm_video_stream_hw_frame(s->handle, frame);
}

static INLINE int video_fixture_stream_completed(video_fixture_stream_t *s)
{
   return s->mp4 ? rmp4_video_stream_completed(s->handle)
      : rwebm_video_stream_completed(s->handle);
}

static INLINE int video_fixture_stream_waiting(video_fixture_stream_t *s)
{
   return s->mp4 ? rmp4_video_stream_waiting(s->handle)
      : rwebm_video_stream_waiting(s->handle);
}

static INLINE int video_fixture_stream_seek_begin(video_fixture_stream_t *s, int64_t time)
{
   return s->mp4 ? rmp4_video_stream_seek_movie_begin_ns(s->handle, time)
      : rwebm_video_stream_seek_movie_begin_ns(s->handle, time);
}

static INLINE int video_fixture_stream_seek_step(video_fixture_stream_t *s, int64_t *position)
{
   return s->mp4 ? rmp4_video_stream_seek_movie_step(s->handle, position)
      : rwebm_video_stream_seek_movie_step(s->handle, position);
}

#endif

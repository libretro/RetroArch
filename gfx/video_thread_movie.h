/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VIDEO_THREAD_MOVIE_H
#define RARCH_VIDEO_THREAD_MOVIE_H

#include <string.h>
#include <retro_inline.h>
#include <retro_atomic.h>

typedef struct video_thread_movie_state
{
   retro_atomic_size_t epoch;
   retro_atomic_size_t refused;
} video_thread_movie_state_t;

static INLINE void video_thread_movie_state_init(video_thread_movie_state_t *state)
{
   retro_atomic_size_init(&state->epoch, 1);
   retro_atomic_size_init(&state->refused, 0);
}

static INLINE bool video_thread_movie_ready(video_thread_movie_state_t *state)
{
   size_t epoch = retro_atomic_load_acquire_size(&state->epoch);
   return epoch != (size_t)-1
      && epoch != retro_atomic_load_acquire_size(&state->refused);
}

/* Owned by the producer, then by its frame-ring slot. */
typedef struct video_thread_movie
{
   video_frame_import_t frame;
   size_t epoch;
   bool pending;
   bool reset;
} video_thread_movie_t;

static INLINE void video_thread_movie_discard(video_thread_movie_t *movie)
{
   if (movie->pending && movie->frame.abandon)
      movie->frame.abandon(movie->frame.owner, movie->frame.done,
            movie->frame.done_value);
   memset(movie, 0, sizeof(*movie));
}

static INLINE enum video_texture_update video_thread_movie_stage(
      video_thread_movie_state_t *state, video_thread_movie_t *movie,
      const video_frame_import_t *frame)
{
   if (!frame)
   {
      size_t epoch = retro_atomic_load_relaxed_size(&state->epoch);
      /* Exhaustion disables movies until driver replacement, never aliases. */
      if (epoch != (size_t)-1)
         retro_atomic_store_release_size(&state->epoch, epoch + 1);
      video_thread_movie_discard(movie);
      movie->reset = true;
      return VIDEO_TEXTURE_UPDATE_DONE;
   }
   if (movie->pending)
      return VIDEO_TEXTURE_UPDATE_DROPPED;
   if (!video_thread_movie_ready(state))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   movie->frame = *frame;
   movie->epoch = retro_atomic_load_relaxed_size(&state->epoch);
   movie->pending = true;
   return VIDEO_TEXTURE_UPDATE_DONE;
}

static INLINE void video_thread_movie_move(video_thread_movie_t *to,
      video_thread_movie_t *from)
{
   bool reset = to->reset;
   if (!reset && !to->pending && !from->pending && !from->reset)
      return;
   video_thread_movie_discard(to);
   *to = *from;
   to->reset = to->reset || reset;
   memset(from, 0, sizeof(*from));
}

static INLINE void video_thread_movie_apply(video_thread_movie_state_t *state,
      video_thread_movie_t *movie, void *data, const video_poke_interface_t *poke)
{
   enum video_texture_update result = VIDEO_TEXTURE_UPDATE_REFUSED;
   if (!movie->pending && !movie->reset)
      return;
   if (data && poke && poke->import_movie_frame)
   {
      if (movie->reset)
         poke->import_movie_frame(data, NULL);
      if (movie->pending)
         result = poke->import_movie_frame(data, &movie->frame);
   }
   if (movie->pending)
   {
      if (result == VIDEO_TEXTURE_UPDATE_DONE)
         movie->pending = false;
      else if (result == VIDEO_TEXTURE_UPDATE_REFUSED)
         retro_atomic_store_release_size(&state->refused, movie->epoch);
   }
   /* A busy GPU drops this accepted ring frame; it never blocks. */
   video_thread_movie_discard(movie);
}

#endif

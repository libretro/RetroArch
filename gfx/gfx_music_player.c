/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include "gfx_music_player.h"
#include "gfx_music_visualizer.h"
#include "gfx_surface.h"
#include "../audio/audio_driver.h"
#include "../tasks/task_audio_mixer.h"

struct gfx_music_player
{
   uint64_t clock_frames;
   int64_t duration_us; /* Published by the release store to bound. */
   gfx_music_player_t *retired_next;
   gfx_music_visualizer_t *visualizer;
   audio_mixer_voice_t *voice;
   char *path;
   size_t serial;
   size_t mixed_frames;
   size_t last_published;
   retro_atomic_size_t published_frames;
   retro_atomic_int_t references, active, bound;
   unsigned slot, rate;
   bool paused, rgba;
};

/* Main-thread retirement keeps the final free off the audio thread. */
static gfx_music_player_t *gfx_music_retired;

static void gfx_music_player_retain(void *user)
{
   gfx_music_player_t *p = (gfx_music_player_t*)user;
   retro_atomic_fetch_add_int(&p->references, 1);
}

static void gfx_music_player_release(void *user)
{
   gfx_music_player_t *p = (gfx_music_player_t*)user;
   retro_atomic_fetch_add_int(&p->references, -1);
}

static bool gfx_music_player_active(void *user)
{
   return retro_atomic_load_acquire_int(&((gfx_music_player_t*)user)->active) != 0;
}

static void gfx_music_player_samples(void *user, const void *pcm,
      size_t frames, bool floating)
{
   gfx_music_player_t *p = (gfx_music_player_t*)user;
   if (gfx_music_player_active(p))
   {
      gfx_music_visualizer_feed(p->visualizer, pcm, frames, floating, p->rate);
      p->mixed_frames += frames;
      retro_atomic_store_release_size(&p->published_frames, p->mixed_frames);
   }
}

static void gfx_music_player_format(void *user, unsigned rate, uint64_t frames)
{
   gfx_music_player_t *p = (gfx_music_player_t*)user;
   uint64_t maximum = (~(uint64_t)0) >> 1;
   uint64_t seconds, duration;
   if (!rate) return;
   seconds = frames / rate;
   duration = seconds > maximum / 1000000 ? maximum
      : seconds * 1000000 + (frames % rate) * 1000000 / rate;
   p->duration_us = (int64_t)(duration > maximum ? maximum : duration);
}

static void gfx_music_player_started(void *user, unsigned slot,
      audio_mixer_voice_t *voice, size_t serial)
{
   gfx_music_player_t *p = (gfx_music_player_t*)user;
   p->voice = voice;
   p->serial = serial;
   p->slot = slot;
   retro_atomic_store_release_int(&p->bound, 1);
   if (!gfx_music_player_active(p))
      audio_driver_mixer_remove_stream_clocked(slot, voice, serial);
}

void gfx_music_player_reap(void)
{
   gfx_music_player_t **next = &gfx_music_retired;
   while (*next)
   {
      gfx_music_player_t *p = *next;
      if (retro_atomic_load_acquire_int(&p->references) != 1)
      {
         next = &p->retired_next;
         continue;
      }
      *next = p->retired_next;
      gfx_music_visualizer_free(p->visualizer);
      free(p->path);
      free(p);
   }
}

void gfx_music_player_close(gfx_music_player_t *p)
{
   if (!p) return;
   retro_atomic_store_release_int(&p->active, 0);
   if (retro_atomic_load_acquire_int(&p->bound))
      audio_driver_mixer_remove_stream_clocked(p->slot, p->voice, p->serial);
   p->retired_next = gfx_music_retired;
   gfx_music_retired = p;
   gfx_music_player_reap();
}

gfx_music_player_t *gfx_music_player_new(const char *path)
{
   task_audio_mixer_viewer_t viewer;
   gfx_music_player_t *p;
   unsigned rate;
   bool s16;
   if (!path || !*path || !audio_driver_mixer_prepare_format(&rate, &s16)) return NULL;
   p = (gfx_music_player_t*)calloc(1, sizeof(*p));
   if (!p) return NULL;
   retro_atomic_int_init(&p->references, 1);
   retro_atomic_int_init(&p->active, 1);
   retro_atomic_int_init(&p->bound, 0);
   retro_atomic_size_init(&p->published_frames, 0);
   p->rate = rate;
   p->path = (char*)malloc(strlen(path) + 1);
   p->visualizer = gfx_music_visualizer_new();
   if (!p->path || !p->visualizer)
   {
      gfx_music_player_close(p);
      return NULL;
   }
   memcpy(p->path, path, strlen(path) + 1);
   memset(&viewer, 0, sizeof(viewer));
   viewer.observer.user = p;
   viewer.observer.samples = gfx_music_player_samples;
   viewer.observer.format = gfx_music_player_format;
   viewer.observer.retain = gfx_music_player_retain;
   viewer.observer.release = gfx_music_player_release;
   viewer.active = gfx_music_player_active;
   viewer.started = gfx_music_player_started;
   if (!task_push_audio_mixer_viewer(path, &viewer))
   {
      gfx_music_player_close(p);
      return NULL;
   }
   return p;
}

const char *gfx_music_player_path(const gfx_music_player_t *p)
{
   return p ? p->path : NULL;
}

bool gfx_music_player_pause(gfx_music_player_t *p, bool paused)
{
   size_t serial;
   if (!p || !retro_atomic_load_acquire_int(&p->bound)
         || !audio_mixer_voice_clock(p->voice, NULL, NULL, &serial)
         || serial != p->serial) return false;
   audio_mixer_voice_set_paused(p->voice, paused);
   p->paused = paused;
   return true;
}

bool gfx_music_player_state(gfx_music_player_t *p, gfx_media_playback_state_t *state)
{
   size_t frames;
   if (!p || !state) return false;
   memset(state, 0, sizeof(*state));
   state->dims = VIDEO_SCALE_PACK(GFX_MUSIC_VISUALIZER_WIDTH, GFX_MUSIC_VISUALIZER_HEIGHT);
   state->paused = p->paused;
   if (retro_atomic_load_acquire_int(&p->bound))
      state->duration_us = p->duration_us;
   frames = retro_atomic_load_acquire_size(&p->published_frames);
   p->clock_frames += frames - p->last_published;
   p->last_published = frames;
   if (p->rate)
   {
      uint64_t maximum = (~(uint64_t)0) >> 1;
      uint64_t seconds = p->clock_frames / p->rate;
      uint64_t position = seconds > maximum / 1000000 ? maximum
         : seconds * 1000000 + (p->clock_frames % p->rate) * 1000000 / p->rate;
      state->position_us = (int64_t)(position > maximum ? maximum : position);
   }
   return true;
}

static void gfx_music_player_shown(void *user, gfx_surface_t *surface, unsigned slot)
{
   gfx_thumbnail_t *thumbnail = (gfx_thumbnail_t*)user;
   (void)slot;
   if (surface->dropped)
   {
      gfx_music_player_invalidate((gfx_music_player_t*)thumbnail->music_player);
      return;
   }
   if (thumbnail->anim_surface != surface || !surface->handle) return;
   thumbnail->texture = surface->handle;
   thumbnail->dims = surface->dims;
   thumbnail->alpha = 1.0f;
   retro_atomic_store_release_int(&thumbnail->status, GFX_THUMBNAIL_STATUS_AVAILABLE);
}

bool gfx_music_player_render(gfx_music_player_t *p,
      uint32_t *pixels, size_t pitch, bool rgba)
{
   return p && gfx_music_visualizer_render(p->visualizer, pixels, pitch, rgba);
}

void gfx_music_player_invalidate(gfx_music_player_t *p)
{
   if (p) gfx_music_visualizer_invalidate(p->visualizer);
}

void gfx_music_player_poll(gfx_music_player_t *p, gfx_thumbnail_t *thumbnail)
{
   gfx_surface_t *surface;
   uint32_t *pixels;
   bool changed;
   enum gfx_surface_submit_result result;
   if (!p || !thumbnail) return;
   surface = (gfx_surface_t*)thumbnail->anim_surface;
   if (!surface)
   {
      p->rgba = gfx_surface_wants_rgba();
      surface = gfx_surface_new(VIDEO_SCALE_PACK(GFX_MUSIC_VISUALIZER_WIDTH,
               GFX_MUSIC_VISUALIZER_HEIGHT), 1, IMAGE_PIXFMT_8888,
            TEXTURE_FILTER_LINEAR, gfx_music_player_shown, thumbnail);
      if (!surface) return;
      surface->padded = 1;
      thumbnail->anim_surface = surface;
   }
   if (!gfx_surface_slot_writable(surface, 0)) return;
   pixels = gfx_surface_slot_begin(surface, 0);
   if (!pixels) return;
   changed = gfx_music_visualizer_render(p->visualizer, pixels,
         surface->pitch[0], p->rgba);
   gfx_surface_slot_end(surface, 0);
   if (!changed) return;
   result = gfx_surface_submit(surface, 0, p->rgba);
   if (result == GFX_SURFACE_SUBMIT_DONE)
      gfx_music_player_shown(thumbnail, surface, 0);
   else if (result != GFX_SURFACE_SUBMIT_QUEUED)
      gfx_music_visualizer_invalidate(p->visualizer);
}

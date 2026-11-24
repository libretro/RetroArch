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

#include <stdlib.h>
#include <string.h>

#include "gfx_media_cpu.h"
#include "video_defines.h"
#if defined(HAVE_MENU) && defined(HAVE_AUDIOMIXER)
#include "gfx_music_player.h"
#include "gfx_music_visualizer.h"
#endif

struct gfx_media_cpu
{
   gfx_anim_preview_t *session;
   video_player_t *player;
#if defined(HAVE_MENU) && defined(HAVE_AUDIOMIXER)
   gfx_music_player_t *music;
#endif
   gfx_media_cpu_present_t present;
   void *user;
   uint32_t *slots[2];
   unsigned num_slots;
   bool ended, completed_pending;
   struct gfx_media_cpu *retired_next;
};

static gfx_media_cpu_t *gfx_media_cpu_retired;

static int gfx_media_cpu_start(void *user, unsigned width,
      unsigned height, unsigned num_slots)
{
   gfx_media_cpu_t *p = (gfx_media_cpu_t*)user;
   uint64_t pixels = (uint64_t)width * height;
   unsigned i;
   if (!width || !height || !num_slots || num_slots > 2
         || pixels > (size_t)-1 / sizeof(uint32_t)
         || !gfx_anim_preview_mem_ok(p->session, pixels))
      return -1;
   for (i = 0; i < num_slots; i++)
      if (!p->slots[i] && !(p->slots[i] = (uint32_t*)malloc(
                  (size_t)pixels * sizeof(uint32_t))))
         return -1;
   p->num_slots = num_slots;
   return 0;
}

static uint32_t *gfx_media_cpu_slot(void *user, unsigned slot)
{
   gfx_media_cpu_t *p = (gfx_media_cpu_t*)user;
   return slot < p->num_slots ? p->slots[slot] : NULL;
}

static enum video_player_present gfx_media_cpu_present(void *user,
      const video_player_frame_t *frame)
{
   gfx_media_cpu_t *p = (gfx_media_cpu_t*)user;
   video_player_frame_t view = *frame;
   if (view.hw || view.slot >= p->num_slots)
      return VIDEO_PLAYER_PRESENT_FAILED;
   if (!view.pixels) view.pixels = p->slots[view.slot];
   return view.pixels && p->present && p->present(p->user, &view)
      ? VIDEO_PLAYER_PRESENT_DONE : VIDEO_PLAYER_PRESENT_FAILED;
}

static bool gfx_media_cpu_feed(void *user)
{
   return gfx_anim_preview_feed(((gfx_media_cpu_t*)user)->session);
}

gfx_media_cpu_t *gfx_media_cpu_new(gfx_anim_preview_t *session,
      gfx_media_cpu_present_t present, void *user, unsigned flags)
{
   gfx_media_cpu_t *p;
   video_player_sink_t sink;
   if (!session || !session->stream || !present
         || (flags & ~VIDEO_PLAYER_INLINE)) return NULL;
   if (!(p = (gfx_media_cpu_t*)calloc(1, sizeof(*p)))) return NULL;
   p->session = session;
   p->present = present;
   p->user = user;
   memset(&sink, 0, sizeof(sink));
   sink.user = p;
   sink.start = gfx_media_cpu_start;
   sink.slot_begin = gfx_media_cpu_slot;
   sink.present = gfx_media_cpu_present;
   sink.feed = gfx_media_cpu_feed;
   p->player = video_player_new(session->stream, session->type,
         session->loop_count, &sink, flags);
   if (!p->player)
   {
      free(p);
      return NULL;
   }
   video_player_set_seek_handler(p->player, gfx_anim_preview_seek_step, session);
   return p;
}

#if defined(HAVE_MENU) && defined(HAVE_AUDIOMIXER)
gfx_media_cpu_t *gfx_media_cpu_new_music(const char *path,
      gfx_media_cpu_present_t present, void *user)
{
   gfx_media_cpu_t *p;
   if (!present || !(p = (gfx_media_cpu_t*)calloc(1, sizeof(*p)))) return NULL;
   p->present = present;
   p->user = user;
   p->slots[0] = (uint32_t*)malloc(GFX_MUSIC_VISUALIZER_WIDTH
         * GFX_MUSIC_VISUALIZER_HEIGHT * sizeof(uint32_t));
   if (!p->slots[0] || !(p->music = gfx_music_player_new(path)))
   {
      free(p->slots[0]);
      free(p);
      return NULL;
   }
   p->num_slots = 1;
   return p;
}
#endif

enum video_player_state gfx_media_cpu_poll(gfx_media_cpu_t *p, int64_t wall_us)
{
   enum video_player_state result;
   if (!p || p->ended) return VIDEO_PLAYER_ENDED;
#if defined(HAVE_MENU) && defined(HAVE_AUDIOMIXER)
   if (p->music)
   {
      if (gfx_music_player_render(p->music, p->slots[0],
               GFX_MUSIC_VISUALIZER_WIDTH * sizeof(uint32_t), false))
      {
         video_player_frame_t frame;
         memset(&frame, 0, sizeof(frame));
         frame.pixels = p->slots[0];
         frame.width = GFX_MUSIC_VISUALIZER_WIDTH;
         frame.height = GFX_MUSIC_VISUALIZER_HEIGHT;
         frame.native = true;
         frame.how = VIDEO_PLAYER_DIRECT;
         if (!p->present(p->user, &frame)) gfx_music_player_invalidate(p->music);
      }
      return VIDEO_PLAYER_PLAYING;
   }
#endif
   gfx_anim_preview_audio_feed(p->session);
   result = gfx_media_player_poll(p->session, p->player, wall_us);
   if (result == VIDEO_PLAYER_ENDED && video_player_completed(p->player))
   {
      p->ended = p->completed_pending = true;
      gfx_media_player_set_paused(p->session, p->player, true, false, wall_us);
   }
   return result;
}

bool gfx_media_cpu_set_repeat(gfx_media_cpu_t *p, bool repeat)
{
   return p && gfx_media_player_set_repeat(p->session, p->player, repeat);
}

bool gfx_media_cpu_completed(gfx_media_cpu_t *p)
{
   return p && p->ended;
}

bool gfx_media_cpu_take_completed(gfx_media_cpu_t *p)
{
   if (!p || !p->completed_pending) return false;
   p->completed_pending = false;
   return true;
}

bool gfx_media_cpu_pause(gfx_media_cpu_t *p, bool paused, int64_t wall_us)
{
#if defined(HAVE_MENU) && defined(HAVE_AUDIOMIXER)
   if (p && p->music) return gfx_music_player_pause(p->music, paused);
#endif
   return p && gfx_media_player_set_paused(p->session, p->player,
         paused, true, wall_us);
}

bool gfx_media_cpu_seek(gfx_media_cpu_t *p, int64_t target_us,
      unsigned mix_rate, bool s16, int64_t wall_us)
{
   bool result = p && gfx_media_player_seek(p->session, p->player, target_us,
         mix_rate, s16, wall_us);
   if (result) p->ended = p->completed_pending = false;
   return result;
}

const gfx_anim_preview_t *gfx_media_cpu_session(const gfx_media_cpu_t *p)
{
   return p ? p->session : NULL;
}

bool gfx_media_cpu_request_audio(gfx_media_cpu_t *p, int index)
{
   return p && gfx_anim_preview_audio_set_track(p->session, index);
}

bool gfx_media_cpu_select_audio(gfx_media_cpu_t *p, int index,
      unsigned mix_rate, bool s16, int64_t wall_us)
{
   bool result = p && gfx_media_player_select_audio(p->session, p->player,
         index, mix_rate, s16, wall_us);
   if (result) p->ended = p->completed_pending = false;
   return result;
}

bool gfx_media_cpu_state(gfx_media_cpu_t *p, int64_t wall_us,
      gfx_media_playback_state_t *state)
{
#if defined(HAVE_MENU) && defined(HAVE_AUDIOMIXER)
   if (p && p->music) return gfx_music_player_state(p->music, state);
#endif
   return gfx_media_player_state(p ? p->session : NULL,
         p ? p->player : NULL, wall_us, state);
}

bool gfx_media_cpu_free(gfx_media_cpu_t *p)
{
   unsigned i;
   if (!p) return true;
#if defined(HAVE_MENU) && defined(HAVE_AUDIOMIXER)
   if (p->music)
   {
      gfx_music_player_close(p->music);
      free(p->slots[0]);
      free(p);
      return true;
   }
#endif
   gfx_anim_preview_audio_stop(p->session);
   if (!video_player_stop(p->player)) return false;
   video_player_free(p->player);
   gfx_anim_preview_close(p->session);
   for (i = 0; i < 2; i++) free(p->slots[i]);
   free(p);
   return true;
}

void gfx_media_cpu_reap(void)
{
   gfx_media_cpu_t **link = &gfx_media_cpu_retired;
   while (*link)
   {
      gfx_media_cpu_t *p = *link;
      gfx_media_cpu_t *next = p->retired_next;
      if (gfx_media_cpu_free(p)) *link = next;
      else link = &p->retired_next;
   }
}

void gfx_media_cpu_retire(gfx_media_cpu_t *p)
{
   if (!p) return;
   p->present = NULL;
   p->user = NULL;
   if (!gfx_media_cpu_free(p))
   {
      p->retired_next = gfx_media_cpu_retired;
      gfx_media_cpu_retired = p;
   }
}

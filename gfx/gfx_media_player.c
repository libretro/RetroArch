#include <string.h>
#include "gfx_anim_preview.h"
#include "gfx_media_player.h"

bool gfx_media_player_set_repeat(gfx_anim_preview_t *session,
      video_player_t *player, bool repeat)
{
   if (!session || !player) return false;
   if (session->loop_count == (repeat ? 0 : 1)) return true;
   if (!video_player_set_repeat(player, repeat)) return false;
   gfx_anim_preview_set_repeat(session, repeat);
   return true;
}

bool gfx_media_player_set_paused(gfx_anim_preview_t *session,
      video_player_t *player, bool paused, bool audio_ready, int64_t wall_us)
{
   int64_t now = wall_us;
   if (!player || (session && session->seek_failed && !paused)) return false;
   if (session)
   {
      now = gfx_anim_preview_clock(session, now);
      gfx_anim_preview_set_paused(session, paused);
      if (!paused && session->playback_started && audio_ready)
         gfx_anim_preview_audio_begin(session);
   }
   video_player_set_paused(player, paused, now);
   return true;
}

bool gfx_media_player_state(gfx_anim_preview_t *session,
      video_player_t *player, int64_t wall_us, gfx_media_playback_state_t *state)
{
   int64_t now, elapsed = 0, duration;
   uint64_t delta;
   const int64_t maximum = (int64_t)((~(uint64_t)0) >> 1);
   if (!state) return false;
   memset(state, 0, sizeof(*state));
   if (!session || (session->type != IMAGE_TYPE_MP4
            && session->type != IMAGE_TYPE_WEBM))
      return false;
   now = gfx_anim_preview_clock(session, wall_us);
   duration = image_transfer_anim_stream_duration_ns(session->stream, session->type);
   state->duration_us = duration > 0 ? duration / 1000 : 0;
   state->duration_us = state->duration_us > session->timeline_start_us
      ? state->duration_us - session->timeline_start_us : 0;
   if (session->media_clock_valid && now > session->media_clock_us)
   {
      delta = (uint64_t)now - (uint64_t)session->media_clock_us;
      elapsed = delta > (uint64_t)maximum ? maximum : (int64_t)delta;
   }
   state->position_us = session->seeking ? session->media_requested_us
      : session->media_origin_us > maximum - elapsed ? maximum
      : session->media_origin_us + elapsed;
   if (!session->seeking && (session->type == IMAGE_TYPE_MP4 || session->type == IMAGE_TYPE_WEBM)
         && !session->loop_count && state->duration_us
         && state->position_us > state->duration_us)
      state->position_us %= state->duration_us;
   if (state->duration_us && state->position_us > state->duration_us)
      state->position_us = state->duration_us;
   video_player_cadence(player, &state->cadence_num, &state->cadence_den);
   state->cadence_unstable = video_player_cadence_unstable(player);
   state->dims = session->dims;
   state->paused = session->paused;
   state->seeking = session->seeking;
   state->seekable = session->seekable;
   state->seek_failed = session->seek_failed;
   state->seek_target_us = session->media_requested_us;
   return true;
}

bool gfx_media_player_seek(gfx_anim_preview_t *session,
      video_player_t *player, int64_t target_us, unsigned mix_rate,
      bool s16, int64_t wall_us)
{
   int64_t duration;
   if (!session || !player || !session->seekable) return false;
   if (mix_rate > 0x3fffffffU) mix_rate = 0;
   if (target_us < 0) target_us = 0;
   duration = image_transfer_anim_stream_duration_ns(session->stream, session->type);
   duration = duration > 0 ? duration / 1000 : 0;
   duration = duration > session->timeline_start_us ? duration - session->timeline_start_us : 0;
   if (duration && target_us > duration) target_us = duration;
   if (!video_player_seek(player, target_us)) return false;
   session->media_requested_us = target_us;
   gfx_anim_preview_clock(session, wall_us);
   return gfx_anim_preview_seek_request(session, mix_rate, s16);
}

enum video_player_state gfx_media_player_poll(gfx_anim_preview_t *session,
      video_player_t *player, int64_t wall_us)
{
   int64_t now = session ? gfx_anim_preview_clock(session, wall_us) : wall_us;
   enum video_player_state result = video_player_poll(player, now);
   if (result == VIDEO_PLAYER_SEEK_FAILED)
   {
      if (session && session->seeking)
      {
         gfx_anim_preview_seek_cancel(session);
         video_player_set_paused(player, true, now);
      }
      return VIDEO_PLAYER_PLAYING;
   }
   if (result != VIDEO_PLAYER_ENDED && session && video_player_started(player))
   {
      int64_t position_us;
      if (session->seeking && video_player_seek_position(player, &position_us))
         gfx_anim_preview_seek_commit(session, position_us);
      if (!session->playback_started)
      {
         session->media_clock_us = now;
         session->media_clock_valid = true;
         session->playback_started = true;
         gfx_anim_preview_audio_begin(session);
      }
   }
   return result;
}

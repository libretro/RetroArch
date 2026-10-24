#ifndef GFX_MEDIA_PLAYER_H
#define GFX_MEDIA_PLAYER_H

#include <formats/video_player.h>
#include "gfx_media_state.h"

struct gfx_anim_preview;

RETRO_BEGIN_DECLS

/* Main-thread transport shared by CPU and texture sinks. */
bool gfx_media_player_set_repeat(struct gfx_anim_preview *session,
      video_player_t *player, bool repeat);
bool gfx_media_player_set_paused(struct gfx_anim_preview *session,
      video_player_t *player, bool paused, bool audio_ready, int64_t wall_us);
bool gfx_media_player_state(struct gfx_anim_preview *session,
      video_player_t *player, int64_t wall_us, gfx_media_playback_state_t *state);
bool gfx_media_player_seek(struct gfx_anim_preview *session,
      video_player_t *player, int64_t target_us, unsigned mix_rate,
      bool s16, int64_t wall_us);
enum video_player_state gfx_media_player_poll(struct gfx_anim_preview *session,
      video_player_t *player, int64_t wall_us);

RETRO_END_DECLS

#endif

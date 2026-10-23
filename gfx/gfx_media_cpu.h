#ifndef GFX_MEDIA_CPU_H
#define GFX_MEDIA_CPU_H

#include "gfx_anim_preview.h"
#include "gfx_media_player.h"

typedef struct gfx_media_cpu gfx_media_cpu_t;
/* The polling thread consumes these pixels before returning. It must
 * not retain frame/pixels or publish them to a render thread. */
typedef bool (*gfx_media_cpu_present_t)(void *user,
      const video_player_frame_t *frame);

RETRO_BEGIN_DECLS

/* Takes session ownership on success only; use a software-open session. */
gfx_media_cpu_t *gfx_media_cpu_new(gfx_anim_preview_t *session,
      gfx_media_cpu_present_t present, void *user, unsigned flags);
enum video_player_state gfx_media_cpu_poll(gfx_media_cpu_t *player,
      int64_t wall_us);
bool gfx_media_cpu_set_repeat(gfx_media_cpu_t *player, bool repeat);
bool gfx_media_cpu_completed(gfx_media_cpu_t *player);
bool gfx_media_cpu_take_completed(gfx_media_cpu_t *player);
bool gfx_media_cpu_pause(gfx_media_cpu_t *player, bool paused, int64_t wall_us);
bool gfx_media_cpu_seek(gfx_media_cpu_t *player, int64_t target_us,
      unsigned mix_rate, bool s16, int64_t wall_us);
bool gfx_media_cpu_state(gfx_media_cpu_t *player, int64_t wall_us,
      gfx_media_playback_state_t *state);
/* Nonblocking. False retains everything until a later call succeeds.
 * The callback user remains alive until this returns true. */
bool gfx_media_cpu_free(gfx_media_cpu_t *player);

/* Main thread: detach the callback and transfer ownership immediately.
 * Reap from the preview pump, and after the shared worker is stopped. */
void gfx_media_cpu_retire(gfx_media_cpu_t *player);
void gfx_media_cpu_reap(void);

RETRO_END_DECLS

#endif

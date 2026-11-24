#ifndef GFX_MUSIC_PLAYER_H
#define GFX_MUSIC_PLAYER_H
#include "gfx_thumbnail.h"

typedef struct gfx_music_player gfx_music_player_t;
/* Main thread. Close retires asynchronously; reap during ticks and teardown. */
gfx_music_player_t *gfx_music_player_new(const char *path);
void gfx_music_player_close(gfx_music_player_t *player);
void gfx_music_player_reap(void);
void gfx_music_player_poll(gfx_music_player_t *player, gfx_thumbnail_t *thumbnail);
bool gfx_music_player_render(gfx_music_player_t *player,
      uint32_t *pixels, size_t pitch, bool rgba);
void gfx_music_player_invalidate(gfx_music_player_t *player);
bool gfx_music_player_pause(gfx_music_player_t *player, bool paused);
bool gfx_music_player_state(gfx_music_player_t *player,
      gfx_media_playback_state_t *state);
const char *gfx_music_player_path(const gfx_music_player_t *player);
#endif

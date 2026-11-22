#include <stdlib.h>
#include "../../../gfx/gfx_music_player.h"

/* Preview requests must never create a fullscreen music session. */
gfx_music_player_t *gfx_music_player_new(const char *path)
{ (void)path; abort(); return NULL; }
void gfx_music_player_reap(void) { }
void gfx_music_player_close(gfx_music_player_t *p) { if (p) abort(); }
void gfx_music_player_poll(gfx_music_player_t *p, gfx_thumbnail_t *thumbnail)
{ (void)thumbnail; if (p) abort(); }
bool gfx_music_player_pause(gfx_music_player_t *p, bool paused)
{ (void)paused; if (p) abort(); return false; }
bool gfx_music_player_state(gfx_music_player_t *p, gfx_media_playback_state_t *state)
{ (void)state; if (p) abort(); return false; }
const char *gfx_music_player_path(const gfx_music_player_t *p)
{ if (p) abort(); return NULL; }
bool gfx_music_player_render(gfx_music_player_t *p, uint32_t *pixels, size_t pitch, bool rgba)
{ (void)pixels; (void)pitch; (void)rgba; if (p) abort(); return false; }
void gfx_music_player_invalidate(gfx_music_player_t *p) { if (p) abort(); }

#ifndef GFX_MEDIA_DISPLAY_H
#define GFX_MEDIA_DISPLAY_H

#include "video_display_server.h"
#include "gfx_media_state.h"

RETRO_BEGIN_DECLS

typedef struct gfx_media_display_owner
{
   const video_display_server_t *server;
   void *data;
   void *lease;
   unsigned num, den;
} gfx_media_display_owner_t;

void gfx_media_display_owner_reset(gfx_media_display_owner_t *owner);
void gfx_media_display_owner_update(gfx_media_display_owner_t *owner,
      const video_display_server_t *server, void *data,
      unsigned num, unsigned den, bool output_changed);

bool gfx_media_display_source(const gfx_media_playback_state_t *state,
      unsigned *dims, float *hz);

/* Advertised progressive modes at the current geometry; zero means no match.
 * Selection only: the caller owns mode application and restoration. */
float gfx_media_display_rate(const video_display_config_t *modes,
      unsigned count, unsigned dims, unsigned bpp,
      unsigned cadence_num, unsigned cadence_den, float current_hz);

RETRO_END_DECLS

#endif

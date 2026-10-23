#ifndef GFX_MEDIA_DISPLAY_H
#define GFX_MEDIA_DISPLAY_H

#include "video_display_server.h"
#include "gfx_media_state.h"

RETRO_BEGIN_DECLS

bool gfx_media_display_source(const gfx_media_playback_state_t *state,
      unsigned *dims, float *hz);

/* Advertised progressive modes at the current geometry; zero means no match.
 * Selection only: the caller owns mode application and restoration. */
float gfx_media_display_rate(const video_display_config_t *modes,
      unsigned count, unsigned dims, unsigned bpp,
      unsigned cadence_num, unsigned cadence_den, float current_hz);

RETRO_END_DECLS

#endif

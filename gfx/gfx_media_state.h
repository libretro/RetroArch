#ifndef GFX_MEDIA_STATE_H
#define GFX_MEDIA_STATE_H

#include <stdint.h>
#include <boolean.h>

typedef struct gfx_media_playback_state
{
   int64_t position_us, duration_us;
   int64_t seek_target_us;
   bool paused, seeking, seekable, seek_failed;
} gfx_media_playback_state_t;

#endif

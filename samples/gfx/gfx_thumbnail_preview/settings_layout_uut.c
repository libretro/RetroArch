/* Compiled with UUT_CFLAGS so settings_t here has the exact layout
 * gfx/gfx_thumbnail.c sees.  The stub pokes its config blob through
 * these exports, keeping the offset tied to configuration.h by the
 * compiler rather than by hand. */
#include <stddef.h>

#include "configuration.h"

const size_t settings_layout_sizeof = sizeof(settings_t);
const size_t settings_layout_preview_audio_off =
      offsetof(settings_t, bools.menu_thumbnail_preview_audio);

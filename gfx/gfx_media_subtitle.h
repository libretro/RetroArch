#ifndef GFX_MEDIA_SUBTITLE_H
#define GFX_MEDIA_SUBTITLE_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>

#define GFX_MEDIA_SUBTITLE_TEXT_SIZE 1024

typedef struct gfx_media_subtitle gfx_media_subtitle_t;
typedef struct gfx_media_subtitle_view
{
   char text[GFX_MEDIA_SUBTITLE_TEXT_SIZE];
   char track[128];
   unsigned track_count;
   bool truncated, off, discovering;
} gfx_media_subtitle_view_t;

/* One producer and one consumer. Zero-initialize once; never reset live slots. */
typedef struct gfx_media_subtitle_mailbox
{
   gfx_media_subtitle_view_t views[3];
   retro_atomic_int_t states[3];
} gfx_media_subtitle_mailbox_t;

RETRO_BEGIN_DECLS

/* Main-thread owner; loading and parsing run through the task queue. */
gfx_media_subtitle_t *gfx_media_subtitle_new(void);
void gfx_media_subtitle_free(gfx_media_subtitle_t *subtitle);
/* path belongs to the active playback session, not the browser selection.
 * Positive delay displays the cue later. No synchronous file access. */
bool gfx_media_subtitle_update(gfx_media_subtitle_t *subtitle,
      const void *identity, const char *path, int64_t position_us,
      int64_t delay_us, bool enabled);
/* Cycle discovered tracks and Off; discovery starts only on demand. */
void gfx_media_subtitle_next(gfx_media_subtitle_t *subtitle);
/* Retain CPU track choice across a graphics context replacement. */
void gfx_media_subtitle_suspend(gfx_media_subtitle_t *subtitle);
const gfx_media_subtitle_view_t *gfx_media_subtitle_view(
      const gfx_media_subtitle_t *subtitle);
void gfx_media_subtitle_publish(gfx_media_subtitle_mailbox_t *mailbox,
      const gfx_media_subtitle_view_t *view);
bool gfx_media_subtitle_consume(gfx_media_subtitle_mailbox_t *mailbox,
      gfx_media_subtitle_view_t *view);

RETRO_END_DECLS

#endif

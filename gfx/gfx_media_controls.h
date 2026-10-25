/* Fullscreen media controls: main-thread semantic actions and pointer
 * snapshots only. Input mapping, providers and playback context belong
 * to the frontend input layer. */
#ifndef GFX_MEDIA_CONTROLS_H
#define GFX_MEDIA_CONTROLS_H
#include "gfx_thumbnail.h"
#include <retro_atomic.h>

typedef struct gfx_media_rect { int x, y, w, h; } gfx_media_rect_t;
typedef struct gfx_media_layout
{
   gfx_media_rect_t panel, timeline, buttons[8];
   int unit;
   unsigned button_count;
} gfx_media_layout_t;
enum gfx_media_action
{
   GFX_MEDIA_ACTIVATE, GFX_MEDIA_PREVIOUS, GFX_MEDIA_NEXT,
   GFX_MEDIA_FOCUS, GFX_MEDIA_UNFOCUS, GFX_MEDIA_STOP,
   GFX_MEDIA_SEEK_BACK, GFX_MEDIA_SEEK_FORWARD,
   GFX_MEDIA_PLAY, GFX_MEDIA_PAUSE,
   GFX_MEDIA_PREVIOUS_FILE, GFX_MEDIA_NEXT_FILE, GFX_MEDIA_SETTINGS
};
/* A framebuffer player and a texture player provide the same transport
 * interface. identity changes when their underlying session changes. */
typedef struct gfx_media_target
{
   void *user;
   const void *identity;
   bool (*state)(void *user, int64_t now, gfx_media_playback_state_t *out);
   bool (*pause)(void *user, bool paused, int64_t now);
   bool (*seek)(void *user, int64_t target_us, int64_t now);
   void *navigation_user;
   bool (*step)(void *user, int direction);
   void *settings_user;
   bool (*settings)(void *user);
   const char *source_path; /* main thread, immutable session path */
   unsigned directions;
   bool subtitle_tracks;
} gfx_media_target_t;

typedef struct gfx_media_controls
{
   gfx_media_target_t target;
   gfx_media_playback_state_t playback;
   gfx_media_layout_t layout;
   int64_t last_activity, drag_us;
   int last_x, last_y, hover, press_target;
   unsigned focus;
   unsigned seek_seconds, hide_seconds;
   bool configured, subtitle_requested;
   bool settings_preview;
   bool visible, row_focus, held, dragging, release_inhibit;
} gfx_media_controls_t;

/* Numeric-only snapshots cross to a video/render thread. Three bounded
 * slot attempts; a busy producer/consumer keeps its previous frame.
 * Single producer and single consumer. Zero-initialize once. Never reset this mailbox while a reader runs. */
typedef struct gfx_media_view
{
   gfx_media_layout_t layout;
   gfx_media_playback_state_t playback;
   int64_t drag_us;
   unsigned focus;
   unsigned seek_seconds;
   unsigned directions;
   int hover;
   bool visible, row_focus, dragging, subtitle_tracks;
   bool settings_preview;
} gfx_media_view_t;
typedef struct gfx_media_mailbox
{
   gfx_media_view_t views[3];
   retro_atomic_int_t states[3];
} gfx_media_mailbox_t;
RETRO_BEGIN_DECLS

void gfx_media_controls_publish(const gfx_media_controls_t *c, gfx_media_mailbox_t *m);
bool gfx_media_controls_consume(gfx_media_mailbox_t *m, gfx_media_view_t *out);
void gfx_media_controls_thumbnail_target(gfx_thumbnail_t *thumbnail, gfx_media_target_t *out);

void gfx_media_controls_reset(gfx_media_controls_t *c);
void gfx_media_controls_configure(gfx_media_controls_t *c,
      unsigned seek_seconds, unsigned hide_seconds);
/* Cancel held/drag state on focus loss, device loss or context reset. */
void gfx_media_controls_cancel(gfx_media_controls_t *c);
bool gfx_media_controls_update(gfx_media_controls_t *c, const gfx_media_target_t *target,
      unsigned width, unsigned height, int64_t now);
bool gfx_media_controls_action(gfx_media_controls_t *c,
      enum gfx_media_action action, int64_t now);
void gfx_media_controls_pointer(gfx_media_controls_t *c, int x, int y,
      bool pressed, bool valid, int64_t now);
/* A release is consumed before the fullscreen-view/browser fallback.
 * Dragging previews locally and commits one seek on release. */
void gfx_media_controls_release(gfx_media_controls_t *c, int x, int y,
      int64_t now);

RETRO_END_DECLS

#endif

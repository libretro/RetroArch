#ifndef MENU_MEDIA_CONTROLS_H
#define MENU_MEDIA_CONTROLS_H

#include "menu_driver.h"
#include "../gfx/gfx_media_controls.h"
#include "../gfx/gfx_display.h"

typedef struct menu_media_controls
{
   gfx_media_controls_t controls; /* main thread */
   gfx_media_mailbox_t mailbox;
   gfx_media_view_t view; /* render thread */
} menu_media_controls_t;

RETRO_BEGIN_DECLS

void menu_media_controls_update(menu_media_controls_t *media,
      const gfx_media_target_t *target, unsigned width, unsigned height,
      const menu_input_pointer_t *pointer, int64_t now);
void menu_media_controls_thumbnail(menu_media_controls_t *media,
      gfx_thumbnail_t *primary, gfx_thumbnail_t *secondary, bool shown,
      unsigned width, unsigned height, const menu_input_pointer_t *pointer,
      int64_t now);
bool menu_media_controls_action(menu_media_controls_t *media,
      enum menu_action action, int64_t now);
bool menu_media_controls_release(menu_media_controls_t *media,
      unsigned x, unsigned y, int64_t now);
void menu_media_controls_cancel(menu_media_controls_t *media);
void menu_media_controls_time(char *out, size_t size, int64_t us);
/* Render-thread snapshot only: no session queries or commands. */
void menu_media_controls_draw(menu_media_controls_t *media,
      gfx_display_t *display, void *userdata, unsigned dims,
      const font_data_t *font);

RETRO_END_DECLS

#endif

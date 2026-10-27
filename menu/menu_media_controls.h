#ifndef MENU_MEDIA_CONTROLS_H
#define MENU_MEDIA_CONTROLS_H

#include "menu_driver.h"
#include "../gfx/gfx_media_controls.h"
#include "../gfx/gfx_media_subtitle.h"
#include "../gfx/gfx_display.h"

struct menu_media_queue;

typedef struct menu_media_controls
{
   gfx_media_playback_state_t resume;
   char *resume_path;
   struct menu_media_queue *queue;
   void *settings_user;
   void (*settings_hide)(void *user);
   void (*settings_show)(void *user);
   size_t settings_depth, settings_selection;
   uintptr_t settings_parent;
   gfx_media_subtitle_t *subtitle; /* main thread */
   gfx_media_controls_t controls; /* main thread */
   gfx_media_mailbox_t mailbox;
   gfx_media_view_t view; /* render thread */
   gfx_media_subtitle_mailbox_t subtitle_mailbox;
   gfx_media_subtitle_view_t subtitle_view; /* render thread */
   bool subtitle_seeking, context_reset; /* main thread */
   bool settings_open, settings_returning; /* main thread */
} menu_media_controls_t;

RETRO_BEGIN_DECLS

bool menu_media_controls_display(unsigned *dims, float *hz);
void menu_media_controls_settings_iterate(menu_media_controls_t *media);
bool menu_media_controls_settings_active(const menu_media_controls_t *media);

bool menu_media_controls_continuous(menu_media_controls_t *media, bool shown);
bool menu_media_controls_complete(menu_media_controls_t *media);
bool menu_media_controls_update(menu_media_controls_t *media,
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
void menu_media_controls_context_reset(menu_media_controls_t *media, bool preserve);
void menu_media_controls_resume(menu_media_controls_t *media,
      const gfx_media_target_t *target, int64_t now);
void menu_media_controls_subtitle_caption(const gfx_media_subtitle_view_t *view,
      char *text, size_t size);
void menu_media_controls_time(char *out, size_t size, int64_t us);
/* Render-thread snapshot only: no session queries or commands. */
void menu_media_controls_draw(menu_media_controls_t *media,
      gfx_display_t *display, void *userdata, unsigned dims,
      const font_data_t *font);

RETRO_END_DECLS

#endif

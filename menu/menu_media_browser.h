#ifndef MENU_MEDIA_BROWSER_H
#define MENU_MEDIA_BROWSER_H
#include "menu_driver.h"
#include "../gfx/gfx_thumbnail.h"
RETRO_BEGIN_DECLS
typedef struct menu_media_queue menu_media_queue_t;
menu_media_queue_t *menu_media_queue_new(struct menu_state *menu_st);
void menu_media_queue_free(menu_media_queue_t *queue);
unsigned menu_media_queue_directions(const menu_media_queue_t *queue);
bool menu_media_queue_step(menu_media_queue_t *queue,
      struct menu_state *menu_st, int direction);

/* Main-thread movie activation, independent of playlist thumbnail settings.
 * Retains a matching preview and removes a duplicate/obsolete second one. */
bool menu_media_open_selected_movie(struct menu_state *menu_st,
      gfx_thumbnail_t *primary, gfx_thumbnail_t *secondary);
RETRO_END_DECLS
#endif

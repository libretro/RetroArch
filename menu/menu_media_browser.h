#ifndef MENU_MEDIA_BROWSER_H
#define MENU_MEDIA_BROWSER_H
#include "menu_driver.h"
#include "../gfx/gfx_thumbnail.h"
RETRO_BEGIN_DECLS
/* Main-thread movie activation, independent of playlist thumbnail settings.
 * Retains a matching preview and removes a duplicate/obsolete second one. */
bool menu_media_open_selected_movie(struct menu_state *menu_st,
      gfx_thumbnail_t *primary, gfx_thumbnail_t *secondary);
RETRO_END_DECLS
#endif

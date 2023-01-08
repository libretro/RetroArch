/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2019-2021 - James Leaver
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MENU_SCREENSAVER_H
#define _MENU_SCREENSAVER_H

#include <retro_common_api.h>
#include <libretro.h>

#include "menu_defines.h"
#include "../retroarch.h"
#include "../gfx/gfx_display.h"
#include "../gfx/gfx_animation.h"

RETRO_BEGIN_DECLS

/* Prevent direct access to menu_screensaver_t members */
typedef struct menu_ss_handle menu_screensaver_t;

/******************/
/* Initialisation */
/******************/

/* Creates a new, 'blank' screensaver object. Auxiliary
 * internal structures will be initialised on the first
 * call of menu_screensaver_iterate().
 * Returned object must be freed using menu_screensaver_free().
 * Returns NULL in the event of an error. */
menu_screensaver_t *menu_screensaver_init(void);

/* Frees specified screensaver object */
void menu_screensaver_free(menu_screensaver_t *screensaver);

/*********************/
/* Context functions */
/*********************/

/* Called when the graphics context is destroyed
 * or reset (a dedicated 'reset' function is
 * unnecessary) */
void menu_screensaver_context_destroy(menu_screensaver_t *screensaver);

/**********************/
/* Run loop functions */
/**********************/

/* Processes screensaver animation logic
 * Called every frame on the main thread
 * (Note: particle_tint is in RGB24 format) */
void menu_screensaver_iterate(
      menu_screensaver_t *screensaver,
      gfx_display_t *p_disp, gfx_animation_t *p_anim,
      enum menu_screensaver_effect effect, float effect_speed,
      uint32_t particle_tint, unsigned width, unsigned height,
      const char *dir_assets);

/* Draws screensaver
 * Called every frame (on the video thread,
 * if threaded video is on) */
void menu_screensaver_frame(menu_screensaver_t *screensaver,
      video_frame_info_t *video_info, gfx_display_t *p_disp);

RETRO_END_DECLS

#endif

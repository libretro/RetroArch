/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2018 - Daniel De Matteis
 *  Copyright (C) 2018      - natinusala
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

#ifndef _OZONE_THEME_H
#define _OZONE_THEME_H

#include "ozone.h"
#include "ozone_texture.h"

typedef struct ozone_theme
{
   /* Background color */
   float background[16];
   float *background_libretro_running;

   /* Float colors for quads and icons */
   float header_footer_separator[16];
   float text[16];
   float selection[16];
   float selection_border[16];
   float entries_border[16];
   float entries_icon[16];
   float text_selected[16];
   float message_background[16];

   /* RGBA colors for text */
   uint32_t text_rgba;
   uint32_t text_selected_rgba;
   uint32_t text_sublabel_rgba;

   /* Screensaver 'tint' (RGB24) */
   uint32_t screensaver_tint;

   /* Sidebar color */
   float *sidebar_background;
   float *sidebar_top_gradient;
   float *sidebar_bottom_gradient;

   /*
      Fancy cursor colors
   */
   float *cursor_border_0;
   float *cursor_border_1;

   uintptr_t textures[OZONE_THEME_TEXTURE_LAST];

   const char *name;
} ozone_theme_t;

extern ozone_theme_t ozone_theme_light;
extern ozone_theme_t ozone_theme_dark;
extern ozone_theme_t ozone_theme_nord;
extern ozone_theme_t ozone_theme_gruvbox_dark;
extern ozone_theme_t ozone_theme_boysenberry;
extern ozone_theme_t ozone_theme_hacking_the_kernel;
extern ozone_theme_t ozone_theme_twilight_zone;
extern ozone_theme_t ozone_theme_dracula;

extern ozone_theme_t *ozone_themes[];

/* TODO/FIXME - global variables referenced outside */
extern const unsigned ozone_themes_count;
extern unsigned last_color_theme;
extern bool last_use_preferred_system_color_theme;
extern ozone_theme_t *ozone_default_theme;
extern float last_framebuffer_opacity;

void ozone_set_color_theme(ozone_handle_t *ozone, unsigned color_theme);
unsigned ozone_get_system_theme(void);
void ozone_set_background_running_opacity(ozone_handle_t *ozone, float framebuffer_opacity);

#endif

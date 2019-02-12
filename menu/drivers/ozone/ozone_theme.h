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

#include "../../../retroarch.h"

static float ozone_pure_white[16] = {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
};

static float ozone_backdrop[16] = {
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
};

static float ozone_osk_backdrop[16] = {
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
};

static float ozone_sidebar_background_light[16] = {
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
};

static float ozone_sidebar_gradient_top_light[16] = {
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.922, 0.922, 0.922, 1.00,
      0.922, 0.922, 0.922, 1.00,
};

static float ozone_sidebar_gradient_bottom_light[16] = {
      0.922, 0.922, 0.922, 1.00,
      0.922, 0.922, 0.922, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
};

static float ozone_sidebar_background_dark[16] = {
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
};

static float ozone_sidebar_gradient_top_dark[16] = {
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.18, 0.18, 0.18, 1.00,
      0.18, 0.18, 0.18, 1.00,
};

static float ozone_sidebar_gradient_bottom_dark[16] = {
      0.18, 0.18, 0.18, 1.00,
      0.18, 0.18, 0.18, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
};

static float ozone_border_0_light[16] = COLOR_HEX_TO_FLOAT(0x50EFD9, 1.00);
static float ozone_border_1_light[16] = COLOR_HEX_TO_FLOAT(0x0DB6D5, 1.00);

static float ozone_border_0_dark[16] = COLOR_HEX_TO_FLOAT(0x198AC6, 1.00);
static float ozone_border_1_dark[16] = COLOR_HEX_TO_FLOAT(0x89F1F2, 1.00);

static float ozone_background_libretro_running_light[16] = {
   0.690, 0.690, 0.690, 0.75,
   0.690, 0.690, 0.690, 0.75,
   0.922, 0.922, 0.922, 1.0,
   0.922, 0.922, 0.922, 1.0
};

static float ozone_background_libretro_running_dark[16] = {
   0.176, 0.176, 0.176, 0.75,
   0.176, 0.176, 0.176, 0.75,
   0.178, 0.178, 0.178, 1.0,
   0.178, 0.178, 0.178, 1.0,
};

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

   /* Sidebar color */
   float *sidebar_background;
   float *sidebar_top_gradient;
   float *sidebar_bottom_gradient;

   /*
      Fancy cursor colors
   */
   float *cursor_border_0;
   float *cursor_border_1;

   menu_texture_item textures[OZONE_THEME_TEXTURE_LAST];

   const char *name;
} ozone_theme_t;

extern ozone_theme_t ozone_theme_light;
extern ozone_theme_t ozone_theme_dark;

extern ozone_theme_t *ozone_themes[];

extern unsigned ozone_themes_count;
extern unsigned last_color_theme;
extern bool last_use_preferred_system_color_theme;
extern ozone_theme_t *ozone_default_theme;

void ozone_set_color_theme(ozone_handle_t *ozone, unsigned color_theme);
unsigned ozone_get_system_theme(void);

#endif

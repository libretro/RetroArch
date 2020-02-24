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

#include "ozone.h"
#include "ozone_theme.h"
#include "ozone_display.h"

ozone_theme_t ozone_theme_light = {
   COLOR_HEX_TO_FLOAT(0xEBEBEB, 1.00),
   ozone_background_libretro_running_light,

   COLOR_HEX_TO_FLOAT(0x2B2B2B, 1.00),
   COLOR_HEX_TO_FLOAT(0x333333, 1.00),
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),
   COLOR_HEX_TO_FLOAT(0x10BEC5, 1.00),
   COLOR_HEX_TO_FLOAT(0xCDCDCD, 1.00),
   COLOR_HEX_TO_FLOAT(0x333333, 1.00),
   COLOR_HEX_TO_FLOAT(0x374CFF, 1.00),
   COLOR_HEX_TO_FLOAT(0xF0F0F0, 1.00),

   0x333333FF,
   0x374CFFFF,
   0x878787FF,

   ozone_sidebar_background_light,
   ozone_sidebar_gradient_top_light,
   ozone_sidebar_gradient_bottom_light,

   ozone_border_0_light,
   ozone_border_1_light,

   {0},

   "light"
};

ozone_theme_t ozone_theme_dark = {
   COLOR_HEX_TO_FLOAT(0x2D2D2D, 1.00),
   ozone_background_libretro_running_dark,

   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),
   COLOR_HEX_TO_FLOAT(0x212227, 1.00),
   COLOR_HEX_TO_FLOAT(0x2DA3CB, 1.00),
   COLOR_HEX_TO_FLOAT(0x51514F, 1.00),
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),
   COLOR_HEX_TO_FLOAT(0x00D9AE, 1.00),
   COLOR_HEX_TO_FLOAT(0x464646, 1.00),

   0xFFFFFFFF,
   0x00FFC5FF,
   0x9F9FA1FF,

   ozone_sidebar_background_dark,
   ozone_sidebar_gradient_top_dark,
   ozone_sidebar_gradient_bottom_dark,

   ozone_border_0_dark,
   ozone_border_1_dark,

   {0},

   "dark"
};

ozone_theme_t ozone_theme_nord = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x2E3440, 1.0f),     /* background */
   ozone_background_libretro_running_nord, /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0xD8DEE9, 1.0f),     /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xECEFF4, 1.0f),     /* text */
   COLOR_HEX_TO_FLOAT(0x232730, 1.0f),     /* selection */
   COLOR_HEX_TO_FLOAT(0x73A1BE, 1.0f),     /* selection_border */
   COLOR_HEX_TO_FLOAT(0x4C566A, 1.0f),     /* entries_border */
   COLOR_HEX_TO_FLOAT(0xE5E9F0, 1.0f),     /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xA9C791, 1.0f),     /* text_selected */
   COLOR_HEX_TO_FLOAT(0x434C5E, 1.0f),     /* message_background */

   /* RGBA colors for text */
   0xECEFF4FF,                             /* text_rgba */
   0xA9C791FF,                             /* text_selected_rgba */
   0x8FBCBBFF,                             /* text_sublabel_rgba */

   /* Sidebar color */
   ozone_sidebar_background_nord,          /* <TODO> sidebar_background */
   ozone_sidebar_gradient_top_nord,        /* <TODO> sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_nord,     /* <TODO> sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_nord,                    /* cursor_border_0 */
   ozone_border_1_nord,                    /* cursor_border_1 */

   {0},                                    /* textures */

   "nord"                                  /* name */
};

ozone_theme_t ozone_theme_gruvbox_dark = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x282828, 1.0f),             /* background */
   ozone_background_libretro_running_gruvbox_dark, /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0xD5C4A1, 1.0f),             /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xEBDBB2, 1.0f),             /* text */
   COLOR_HEX_TO_FLOAT(0x1D2021, 1.0f),             /* selection */
   COLOR_HEX_TO_FLOAT(0xD75D0E, 1.0f),             /* selection_border */
   COLOR_HEX_TO_FLOAT(0x665C54, 1.0f),             /* entries_border */
   COLOR_HEX_TO_FLOAT(0xEBDBB2, 1.0f),             /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x8EC07C, 1.0f),             /* text_selected */
   COLOR_HEX_TO_FLOAT(0x32302F, 1.0f),             /* message_background */

   /* RGBA colors for text */
   0xEBDBB2FF,                                     /* text_rgba */
   0x8EC07CFF,                                     /* text_selected_rgba */
   0xD79921FF,                                     /* text_sublabel_rgba */

   /* Sidebar color */
   ozone_sidebar_background_gruvbox_dark,          /* sidebar_background */
   ozone_sidebar_gradient_top_gruvbox_dark,        /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_gruvbox_dark,     /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_gruvbox_dark,                    /* cursor_border_0 */
   ozone_border_1_gruvbox_dark,                    /* cursor_border_1 */

   {0},                                            /* textures */

   "gruvbox_dark"                                  /* name */
};


ozone_theme_t *ozone_themes[] = {
   &ozone_theme_light,
   &ozone_theme_dark,
   &ozone_theme_nord,
   &ozone_theme_gruvbox_dark
};

unsigned ozone_themes_count                 = sizeof(ozone_themes) / sizeof(ozone_themes[0]);
unsigned last_color_theme                   = 0;
bool last_use_preferred_system_color_theme  = false;
ozone_theme_t *ozone_default_theme          = &ozone_theme_dark; /* also used as a tag for cursor animation */

/* Enable runtime configuration of framebuffer
 * opacity */
float last_framebuffer_opacity               = -1.0f;
static float background_running_alpha_top    = 1.0f;
static float background_running_alpha_bottom = 0.75f;

void ozone_set_color_theme(ozone_handle_t *ozone, unsigned color_theme)
{
   ozone_theme_t *theme = ozone_default_theme;

   if (!ozone)
      return;

   switch (color_theme)
   {
      case 0:
         theme = &ozone_theme_light;
         break;
      case 1:
         theme = &ozone_theme_dark;
         break;
      case 2:
         theme = &ozone_theme_nord;
         break;
      case 3:
         theme = &ozone_theme_gruvbox_dark;
         break;
      default:
         break;
   }

   ozone->theme = theme;

   memcpy(ozone->theme_dynamic.selection_border, ozone->theme->selection_border, sizeof(ozone->theme_dynamic.selection_border));
   memcpy(ozone->theme_dynamic.selection, ozone->theme->selection, sizeof(ozone->theme_dynamic.selection));
   memcpy(ozone->theme_dynamic.entries_border, ozone->theme->entries_border, sizeof(ozone->theme_dynamic.entries_border));
   memcpy(ozone->theme_dynamic.entries_icon, ozone->theme->entries_icon, sizeof(ozone->theme_dynamic.entries_icon));
   memcpy(ozone->theme_dynamic.entries_checkmark, ozone_pure_white, sizeof(ozone->theme_dynamic.entries_checkmark));
   memcpy(ozone->theme_dynamic.cursor_alpha, ozone_pure_white, sizeof(ozone->theme_dynamic.cursor_alpha));
   memcpy(ozone->theme_dynamic.message_background, ozone->theme->message_background, sizeof(ozone->theme_dynamic.message_background));

   ozone_restart_cursor_animation(ozone);

   last_color_theme = color_theme;
}

unsigned ozone_get_system_theme(void)
{
#ifdef HAVE_LIBNX
   unsigned ret = 0;
   if (R_SUCCEEDED(setsysInitialize()))
   {
      ColorSetId theme;
      setsysGetColorSetId(&theme);
      ret = (theme == ColorSetId_Dark) ? 1 : 0;
      setsysExit();
   }

   return ret;
#endif
   return 0;
}

void ozone_set_background_running_opacity(ozone_handle_t *ozone, float framebuffer_opacity)
{
   float *background = NULL;

   if (!ozone || !ozone->theme->background_libretro_running)
      return;

   background = ozone->theme->background_libretro_running;

   /* When content is running, background is a
    * gradient that from top to bottom transitions
    * from maximum to minimum opacity
    * > RetroArch default 'framebuffer_opacity'
    *   is 0.900. At this setting:
    *   - Background top has an alpha of 1.0
    *   - Background bottom has an alpha of 0.75 */
   background_running_alpha_top = framebuffer_opacity / 0.9f;
   background_running_alpha_top = (background_running_alpha_top > 1.0f) ?
         1.0f : (background_running_alpha_top < 0.0f) ?
               0.0f : background_running_alpha_top;

   background_running_alpha_bottom = (2.5f * framebuffer_opacity) - 1.5f;
   background_running_alpha_bottom = (background_running_alpha_bottom > 1.0f) ?
         1.0f : (background_running_alpha_bottom < 0.0f) ?
               0.0f : background_running_alpha_bottom;

   background[11] = background_running_alpha_top;
   background[15] = background_running_alpha_top;
   background[3]  = background_running_alpha_bottom;
   background[7]  = background_running_alpha_bottom;

   last_framebuffer_opacity = framebuffer_opacity;
}

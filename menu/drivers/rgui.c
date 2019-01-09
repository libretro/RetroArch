/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <string/stdstring.h>
#include <lists/string_list.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <encodings/utf.h>
#include <file/file_path.h>
#include <retro_inline.h>
#include <string/stdstring.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../frontend/frontend_driver.h"

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"

#include "../widgets/menu_input_dialog.h"

#include "../../configuration.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"

#define RGUI_TERM_START_X(width)        (width / 21)
#define RGUI_TERM_START_Y(height)       (height / 9)
#define RGUI_TERM_WIDTH(width)          (((width - RGUI_TERM_START_X(width) - RGUI_TERM_START_X(width)) / (FONT_WIDTH_STRIDE)))
#define RGUI_TERM_HEIGHT(width, height) (((height - RGUI_TERM_START_Y(height) - RGUI_TERM_START_X(width)) / (FONT_HEIGHT_STRIDE)) - 1)

typedef struct
{
   uint32_t hover_color;
   uint32_t normal_color;
   uint32_t title_color;
   uint32_t bg_dark_color;
   uint32_t bg_light_color;
   uint32_t border_dark_color;
   uint32_t border_light_color;
} rgui_theme_t;

static const rgui_theme_t rgui_theme_classic_red = {
   0xFFFF362B, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFFF362B, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC08C0000, /* border_dark_color */
   0xC0CC0E03  /* border_light_color */
};

static const rgui_theme_t rgui_theme_classic_orange = {
   0xFFF87217, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFF87217, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0962800, /* border_dark_color */
   0xC0E46C03  /* border_light_color */
};

static const rgui_theme_t rgui_theme_classic_yellow = {
   0xFFFFD801, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFFFD801, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0AC7800, /* border_dark_color */
   0xC0F3C60D  /* border_light_color */
};

static const rgui_theme_t rgui_theme_classic_green = {
   0xFF64FF64, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFF64FF64, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0204020, /* border_dark_color */
   0xC0408040  /* border_light_color */
};

static const rgui_theme_t rgui_theme_classic_blue = {
   0xFF48BEFF, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFF48BEFF, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0005BA6, /* border_dark_color */
   0xC02E94E2  /* border_light_color */
};

static const rgui_theme_t rgui_theme_classic_violet = {
   0xFFD86EFF, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFD86EFF, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC04C0A60, /* border_dark_color */
   0xC0842DCE  /* border_light_color */
};

static const rgui_theme_t rgui_theme_classic_grey = {
   0xFFB6C1C7, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFB6C1C7, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0505050, /* border_dark_color */
   0xC0798A99  /* border_light_color */
};

static const rgui_theme_t rgui_theme_legacy_red = {
   0xFFFFBDBD, /* hover_color */
   0xFFFAF6D5, /* normal_color */
   0xFFFF948A, /* title_color */
   0xC09E4137, /* bg_dark_color */
   0xC0B34B41, /* bg_light_color */
   0xC0BF5E58, /* border_dark_color */
   0xC0F27A6F  /* border_light_color */
};

static const rgui_theme_t rgui_theme_dark_purple = {
   0xFFF2B5D6, /* hover_color */
   0xFFE8D0CC, /* normal_color */
   0xFFC79FC2, /* title_color */
   0xC0562D56, /* bg_dark_color */
   0xC0663A66, /* bg_light_color */
   0xC0885783, /* border_dark_color */
   0xC0A675A1  /* border_light_color */
};

static const rgui_theme_t rgui_theme_midnight_blue = {
   0xFFB2D3ED, /* hover_color */
   0xFFD3DCDE, /* normal_color */
   0xFF86A1BA, /* title_color */
   0xC024374A, /* bg_dark_color */
   0xC03C4D5E, /* bg_light_color */
   0xC046586A, /* border_dark_color */
   0xC06D7F91  /* border_light_color */
};

static const rgui_theme_t rgui_theme_golden = {
   0xFFFFE666, /* hover_color */
   0xFFFFFFDC, /* normal_color */
   0xFFFFCC00, /* title_color */
   0xC0B88D0B, /* bg_dark_color */
   0xC0BF962B, /* bg_light_color */
   0xC0e1ad21, /* border_dark_color */
   0xC0FCC717  /* border_light_color */
};

static const rgui_theme_t rgui_theme_electric_blue = {
   0xFF7DF9FF, /* hover_color */
   0xFFDBE9F4, /* normal_color */
   0xFF86CDE0, /* title_color */
   0xC02E69C6, /* bg_dark_color */
   0xC0007FFF, /* bg_light_color */
   0xC034A5D8, /* border_dark_color */
   0xC070C9FF  /* border_light_color */
};

static const rgui_theme_t rgui_theme_apple_green = {
   0xFFB0FC64, /* hover_color */
   0xFFD8F2CB, /* normal_color */
   0xFFA6D652, /* title_color */
   0xC04F7942, /* bg_dark_color */
   0xC0688539, /* bg_light_color */
   0xC0608E3A, /* border_dark_color */
   0xC09AB973  /* border_light_color */
};

static const rgui_theme_t rgui_theme_volcanic_red = {
   0xFFFFCC99, /* hover_color */
   0xFFD3D3D3, /* normal_color */
   0xFFDDADAF, /* title_color */
   0xC0922724, /* bg_dark_color */
   0xC0BD0F1E, /* bg_light_color */
   0xC0CE2029, /* border_dark_color */
   0xC0FF0000  /* border_light_color */
};

static const rgui_theme_t rgui_theme_lagoon = {
   0xFFBCE1EB, /* hover_color */
   0xFFCFCFC4, /* normal_color */
   0xFF86C7C7, /* title_color */
   0xC0495C6B, /* bg_dark_color */
   0xC0526778, /* bg_light_color */
   0xC058848F, /* border_dark_color */
   0xC060909C  /* border_light_color */
};

static const rgui_theme_t rgui_theme_brogrammer = {
   0xFF3498DB, /* hover_color */
   0xFFECF0F1, /* normal_color */
   0xFF2ECC71, /* title_color */
   0xC0242424, /* bg_dark_color */
   0xC0242424, /* bg_light_color */
   0xC0E74C3C, /* border_dark_color */
   0xC0E74C3C  /* border_light_color */
};

static const rgui_theme_t rgui_theme_dracula = {
   0xFFBD93F9, /* hover_color */
   0xFFF8F8F2, /* normal_color */
   0xFFFF79C6, /* title_color */
   0xC02F3240, /* bg_dark_color */
   0xC02F3240, /* bg_light_color */
   0xC06272A4, /* border_dark_color */
   0xC06272A4  /* border_light_color */
};

static const rgui_theme_t rgui_theme_fairyfloss = {
   0xFFFFF352, /* hover_color */
   0xFFF8F8F2, /* normal_color */
   0xFFFFB8D1, /* title_color */
   0xC0675F87, /* bg_dark_color */
   0xC0675F87, /* bg_light_color */
   0xC08077A8, /* border_dark_color */
   0xC08077A8  /* border_light_color */
};

static const rgui_theme_t rgui_theme_flatui = {
   0xFF0A74B9, /* hover_color */
   0xFF2C3E50, /* normal_color */
   0xFF8E44AD, /* title_color */
   0xE0ECF0F1, /* bg_dark_color */
   0xE0ECF0F1, /* bg_light_color */
   0xE095A5A6, /* border_dark_color */
   0xE095A5A6  /* border_light_color */
};

static const rgui_theme_t rgui_theme_gruvbox_dark = {
   0xFFFE8019, /* hover_color */
   0xFFEBDBB2, /* normal_color */
   0xFF83A598, /* title_color */
   0xC03D3D3D, /* bg_dark_color */
   0xC03D3D3D, /* bg_light_color */
   0xC099897A, /* border_dark_color */
   0xC099897A  /* border_light_color */
};

static const rgui_theme_t rgui_theme_gruvbox_light = {
   0xFFAF3A03, /* hover_color */
   0xFF3C3836, /* normal_color */
   0xFF076678, /* title_color */
   0xE0FBEBC7, /* bg_dark_color */
   0xE0FBEBC7, /* bg_light_color */
   0xE0928374, /* border_dark_color */
   0xE0928374  /* border_light_color */
};

static const rgui_theme_t rgui_theme_hacking_the_kernel = {
   0xFF83FF83, /* hover_color */
   0xFF00E000, /* normal_color */
   0xFF00FF00, /* title_color */
   0xC0000000, /* bg_dark_color */
   0xC0000000, /* bg_light_color */
   0xC0036303, /* border_dark_color */
   0xC0036303  /* border_light_color */
};

static const rgui_theme_t rgui_theme_nord = {
   0xFF8FBCBB, /* hover_color */
   0xFFD8DEE9, /* normal_color */
   0xFF81A1C1, /* title_color */
   0xC0363C4F, /* bg_dark_color */
   0xC0363C4F, /* bg_light_color */
   0xC04E596E, /* border_dark_color */
   0xC04E596E  /* border_light_color */
};

static const rgui_theme_t rgui_theme_nova = {
   0XFF7FC1CA, /* hover_color */
   0XFFC5D4DD, /* normal_color */
   0XFF9A93E1, /* title_color */
   0xC0485B66, /* bg_dark_color */
   0xC0485B66, /* bg_light_color */
   0xC0627985, /* border_dark_color */
   0xC0627985  /* border_light_color */
};

static const rgui_theme_t rgui_theme_one_dark = {
   0XFF98C379, /* hover_color */
   0XFFBBBBBB, /* normal_color */
   0XFFD19A66, /* title_color */
   0xC02D323B, /* bg_dark_color */
   0xC02D323B, /* bg_light_color */
   0xC0495162, /* border_dark_color */
   0xC0495162  /* border_light_color */
};

static const rgui_theme_t rgui_theme_palenight = {
   0xFFC792EA, /* hover_color */
   0xFFBFC7D5, /* normal_color */
   0xFF82AAFF, /* title_color */
   0xC02F3347, /* bg_dark_color */
   0xC02F3347, /* bg_light_color */
   0xC0697098, /* border_dark_color */
   0xC0697098  /* border_light_color */
};

static const rgui_theme_t rgui_theme_solarized_dark = {
   0xFFB58900, /* hover_color */
   0xFF839496, /* normal_color */
   0xFF268BD2, /* title_color */
   0xC0003542, /* bg_dark_color */
   0xC0003542, /* bg_light_color */
   0xC093A1A1, /* border_dark_color */
   0xC093A1A1  /* border_light_color */
};

static const rgui_theme_t rgui_theme_solarized_light = {
   0xFFB58900, /* hover_color */
   0xFF657B83, /* normal_color */
   0xFF268BD2, /* title_color */
   0xE0FDEDDF, /* bg_dark_color */
   0xE0FDEDDF, /* bg_light_color */
   0xE093A1A1, /* border_dark_color */
   0xE093A1A1  /* border_light_color */
};

static const rgui_theme_t rgui_theme_tango_dark = {
   0xFF8AE234, /* hover_color */
   0xFFEEEEEC, /* normal_color */
   0xFF729FCF, /* title_color */
   0xC0384042, /* bg_dark_color */
   0xC0384042, /* bg_light_color */
   0xC06A767A, /* border_dark_color */
   0xC06A767A  /* border_light_color */
};

static const rgui_theme_t rgui_theme_tango_light = {
   0xFF4E9A06, /* hover_color */
   0xFF2E3436, /* normal_color */
   0xFF204A87, /* title_color */
   0xE0EEEEEC, /* bg_dark_color */
   0xE0EEEEEC, /* bg_light_color */
   0xE0C7C7C7, /* border_dark_color */
   0xE0C7C7C7  /* border_light_color */
};

static const rgui_theme_t rgui_theme_zenburn = {
   0xFFF0DFAF, /* hover_color */
   0xFFDCDCCC, /* normal_color */
   0xFF8FB28F, /* title_color */
   0xC04F4F4F, /* bg_dark_color */
   0xC04F4F4F, /* bg_light_color */
   0xC0636363, /* border_dark_color */
   0xC0636363  /* border_light_color */
};

static const rgui_theme_t rgui_theme_anti_zenburn = {
   0xFF336C6C, /* hover_color */
   0xFF232333, /* normal_color */
   0xFF205070, /* title_color */
   0xE0C0C0C0, /* bg_dark_color */
   0xE0C0C0C0, /* bg_light_color */
   0xE0A0A0A0, /* border_dark_color */
   0xE0A0A0A0  /* border_light_color */
};

typedef struct
{
   uint16_t hover_color;
   uint16_t normal_color;
   uint16_t title_color;
   uint16_t bg_dark_color;
   uint16_t bg_light_color;
   uint16_t border_dark_color;
   uint16_t border_light_color;
} rgui_colors_t;

typedef struct
{
   bool bg_modified;
   bool force_redraw;
   bool mouse_show;
   unsigned last_width;
   unsigned last_height;
   unsigned frame_count;
   bool bg_thickness;
   bool border_thickness;
   float scroll_y;
   char *msgbox;
   unsigned color_theme;
   rgui_colors_t colors;
} rgui_t;

static uint16_t *rgui_framebuf_data      = NULL;

#if defined(PS2)

static uint16_t argb32_to_abgr1555(uint32_t col)
{
   /* Extract colour components */
   unsigned a = (col >> 24) & 0xff;
   unsigned r = (col >> 16) & 0xff;
   unsigned g = (col >> 8)  & 0xff;
   unsigned b = col & 0xff;
   /* Background and border colours are normally semi-transparent
    * (so we can see suspended content when opening the quick menu).
    * When no content is loaded, the 'image' behind the RGUI background
    * and border is black - which has the effect of darkening the
    * perceived background/border colours. All the preset theme (and
    * default 'custom') colour values have been adjusted to account for
    * this, but abgr1555 only has a 1 bit alpha channel. This means all
    * colours become fully opaque, and consequently backgrounds/borders
    * become abnormally bright.
    * We therefore have to darken each RGB value according to the alpha
    * component of the input colour... */
   float a_factor = (float)a * (1.0 / 255.0);
   r = (unsigned)(((float)r * a_factor) + 0.5) & 0xff;
   g = (unsigned)(((float)g * a_factor) + 0.5) & 0xff;
   b = (unsigned)(((float)b * a_factor) + 0.5) & 0xff;
   /* Convert from 8 bit to 5 bit */
   r = r >> 3;
   g = g >> 3;
   b = b >> 3;
   /* Return final value - alpha always set to 1 */
   return (1 << 15) | (b << 10) | (g << 5) | r;
}

#define argb32_to_pixel_platform_format(color) argb32_to_abgr1555(color)

#elif defined(PSP) || defined(GEKKO)

static uint16_t argb32_to_abgr4444(uint32_t col)
{
   unsigned a = ((col >> 24) & 0xff) >> 4;
   unsigned r = ((col >> 16) & 0xff) >> 4;
   unsigned g = ((col >> 8)  & 0xff) >> 4;
   unsigned b = ((col & 0xff)      ) >> 4;
   return (a << 12) | (b << 8) | (g << 4) | r;
}

#define argb32_to_pixel_platform_format(color) argb32_to_abgr4444(color)

#else

static uint16_t argb32_to_rgba4444(uint32_t col)
{
   unsigned a = ((col >> 24) & 0xff) >> 4;
   unsigned r = ((col >> 16) & 0xff) >> 4;
   unsigned g = ((col >> 8)  & 0xff) >> 4;
   unsigned b = ((col & 0xff)      ) >> 4;
   return (r << 12) | (g << 8) | (b << 4) | a;
}

#define argb32_to_pixel_platform_format(color) argb32_to_rgba4444(color)

#endif

static const rgui_theme_t *get_theme(rgui_t *rgui)
{
   switch (rgui->color_theme)
   {
      case RGUI_THEME_CLASSIC_RED:
         return &rgui_theme_classic_red;
      case RGUI_THEME_CLASSIC_ORANGE:
         return &rgui_theme_classic_orange;
      case RGUI_THEME_CLASSIC_YELLOW:
         return &rgui_theme_classic_yellow;
      case RGUI_THEME_CLASSIC_GREEN:
         return &rgui_theme_classic_green;
      case RGUI_THEME_CLASSIC_BLUE:
         return &rgui_theme_classic_blue;
      case RGUI_THEME_CLASSIC_VIOLET:
         return &rgui_theme_classic_violet;
      case RGUI_THEME_CLASSIC_GREY:
         return &rgui_theme_classic_grey;
      case RGUI_THEME_LEGACY_RED:
         return &rgui_theme_legacy_red;
      case RGUI_THEME_DARK_PURPLE:
         return &rgui_theme_dark_purple;
      case RGUI_THEME_MIDNIGHT_BLUE:
         return &rgui_theme_midnight_blue;
      case RGUI_THEME_GOLDEN:
         return &rgui_theme_golden;
      case RGUI_THEME_ELECTRIC_BLUE:
         return &rgui_theme_electric_blue;
      case RGUI_THEME_APPLE_GREEN:
         return &rgui_theme_apple_green;
      case RGUI_THEME_VOLCANIC_RED:
         return &rgui_theme_volcanic_red;
      case RGUI_THEME_LAGOON:
         return &rgui_theme_lagoon;
      case RGUI_THEME_BROGRAMMER:
         return &rgui_theme_brogrammer;
      case RGUI_THEME_DRACULA:
         return &rgui_theme_dracula;
      case RGUI_THEME_FAIRYFLOSS:
         return &rgui_theme_fairyfloss;
      case RGUI_THEME_FLATUI:
         return &rgui_theme_flatui;
      case RGUI_THEME_GRUVBOX_DARK:
         return &rgui_theme_gruvbox_dark;
      case RGUI_THEME_GRUVBOX_LIGHT:
         return &rgui_theme_gruvbox_light;
      case RGUI_THEME_HACKING_THE_KERNEL:
         return &rgui_theme_hacking_the_kernel;
      case RGUI_THEME_NORD:
         return &rgui_theme_nord;
      case RGUI_THEME_NOVA:
         return &rgui_theme_nova;
      case RGUI_THEME_ONE_DARK:
         return &rgui_theme_one_dark;
      case RGUI_THEME_PALENIGHT:
         return &rgui_theme_palenight;
      case RGUI_THEME_SOLARIZED_DARK:
         return &rgui_theme_solarized_dark;
      case RGUI_THEME_SOLARIZED_LIGHT:
         return &rgui_theme_solarized_light;
      case RGUI_THEME_TANGO_DARK:
         return &rgui_theme_tango_dark;
      case RGUI_THEME_TANGO_LIGHT:
         return &rgui_theme_tango_light;
      case RGUI_THEME_ZENBURN:
         return &rgui_theme_zenburn;
      case RGUI_THEME_ANTI_ZENBURN:
         return &rgui_theme_anti_zenburn;
      default:
         break;
   }
   
   return &rgui_theme_classic_green;
}

static void prepare_rgui_colors(rgui_t *rgui, settings_t *settings)
{
   rgui_theme_t theme_colors;
   rgui->color_theme = settings->uints.menu_rgui_color_theme;
   if (rgui->color_theme == RGUI_THEME_CUSTOM)
   {
      theme_colors.hover_color = settings->uints.menu_entry_hover_color;
      theme_colors.normal_color = settings->uints.menu_entry_normal_color;
      theme_colors.title_color = settings->uints.menu_title_color;
      theme_colors.bg_dark_color = settings->uints.menu_bg_dark_color;
      theme_colors.bg_light_color = settings->uints.menu_bg_light_color;
      theme_colors.border_dark_color = settings->uints.menu_border_dark_color;
      theme_colors.border_light_color = settings->uints.menu_border_light_color;
   }
   else
   {
      const rgui_theme_t *current_theme = get_theme(rgui);
      theme_colors.hover_color = current_theme->hover_color;
      theme_colors.normal_color = current_theme->normal_color;
      theme_colors.title_color = current_theme->title_color;
      theme_colors.bg_dark_color = current_theme->bg_dark_color;
      theme_colors.bg_light_color = current_theme->bg_light_color;
      theme_colors.border_dark_color = current_theme->border_dark_color;
      theme_colors.border_light_color = current_theme->border_light_color;
   }
   rgui->colors.hover_color = argb32_to_pixel_platform_format(theme_colors.hover_color);
   rgui->colors.normal_color = argb32_to_pixel_platform_format(theme_colors.normal_color);
   rgui->colors.title_color = argb32_to_pixel_platform_format(theme_colors.title_color);
   rgui->colors.bg_dark_color = argb32_to_pixel_platform_format(theme_colors.bg_dark_color);
   rgui->colors.bg_light_color = argb32_to_pixel_platform_format(theme_colors.bg_light_color);
   rgui->colors.border_dark_color = argb32_to_pixel_platform_format(theme_colors.border_dark_color);
   rgui->colors.border_light_color = argb32_to_pixel_platform_format(theme_colors.border_light_color);
}

static uint16_t rgui_bg_filler(rgui_t *rgui, unsigned x, unsigned y)
{
   unsigned shift  = (rgui->bg_thickness ? 1 : 0);
   unsigned select = ((x >> shift) + (y >> shift)) & 1;
   return (select == 0) ? rgui->colors.bg_dark_color : rgui->colors.bg_light_color;
}

static uint16_t rgui_border_filler(rgui_t *rgui, unsigned x, unsigned y)
{
   unsigned shift  = (rgui->border_thickness ? 1 : 0);
   unsigned select = ((x >> shift) + (y >> shift)) & 1;
   return (select == 0) ? rgui->colors.border_dark_color : rgui->colors.border_light_color;
}

static void rgui_fill_rect(
      rgui_t *rgui,
      uint16_t *data,
      size_t pitch,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t (*col)(rgui_t *rgui, unsigned x, unsigned y))
{
   unsigned i, j;

   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         data[j * (pitch >> 1) + i] = col(rgui, i, j);
}

static void rgui_color_rect(
      uint16_t *data,
      size_t pitch,
      unsigned fb_width, unsigned fb_height,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t color)
{
   unsigned i, j;

   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         if (i < fb_width && j < fb_height)
            data[j * (pitch >> 1) + i] = color;
}

static void blit_line(int x, int y,
      const char *message, uint16_t color)
{
   size_t pitch = menu_display_get_framebuffer_pitch();
   const uint8_t *font_fb = menu_display_get_font_framebuffer();

   if (font_fb)
   {
      while (!string_is_empty(message))
      {
         unsigned i, j;
         char symbol = *message++;

         for (j = 0; j < FONT_HEIGHT; j++)
         {
            for (i = 0; i < FONT_WIDTH; i++)
            {
               uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
               int offset  = (i + j * FONT_WIDTH) >> 3;
               bool col    = (font_fb[FONT_OFFSET(symbol) + offset] & rem);

               if (col)
                  rgui_framebuf_data[(y + j) * (pitch >> 1) + (x + i)] = color;
            }
         }

         x += FONT_WIDTH_STRIDE;
      }
   }
}

#if 0
static void rgui_copy_glyph(uint8_t *glyph, const uint8_t *buf)
{
   int x, y;

   if (!glyph)
      return;

   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         uint32_t col    =
            ((uint32_t)buf[3 * (-y * 256 + x) + 0] << 0) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 1] << 8) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 2] << 16);

         uint8_t rem     = 1 << ((x + y * FONT_WIDTH) & 7);
         unsigned offset = (x + y * FONT_WIDTH) >> 3;

         if (col != 0xff)
            glyph[offset] |= rem;
      }
   }
}

static bool init_font(menu_handle_t *menu, const uint8_t *font_bmp_buf)
{
   unsigned i;
   bool fb_font_inited  = true;
   uint8_t        *font = (uint8_t *)calloc(1, FONT_OFFSET(256));

   if (!font)
      return false;

   menu_display_set_font_data_init(fb_font_inited);

   for (i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   menu_display_set_font_framebuffer(font);

   return true;
}
#endif

static bool rguidisp_init_font(menu_handle_t *menu)
{
#if 0
   const uint8_t *font_bmp_buf = NULL;
#endif
   const uint8_t *font_bin_buf = bitmap_bin;

   if (!menu)
      return false;

#if 0
   if (font_bmp_buf)
      return init_font(menu, font_bmp_buf);
#endif

   menu_display_set_font_framebuffer(font_bin_buf);

   return true;
}

static void rgui_render_background(rgui_t *rgui)
{
   size_t pitch_in_pixels, size;
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   uint16_t             *src  = NULL;
   uint16_t             *dst  = NULL;
   
   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   pitch_in_pixels = fb_pitch >> 1;
   size            = fb_pitch * 4;
   src             = rgui_framebuf_data + pitch_in_pixels * fb_height;
   dst             = rgui_framebuf_data;

   while (dst < src)
   {
      memcpy(dst, src, size);
      dst += pitch_in_pixels * 4;
   }

   if (rgui_framebuf_data)
   {
      settings_t *settings       = config_get_ptr();

      if (settings->bools.menu_rgui_border_filler_enable)
      {
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, 5, 5, fb_width - 10, 5, rgui_border_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, 5, fb_height - 10, fb_width - 10, 5, rgui_border_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, 5, 5, 5, fb_height - 10, rgui_border_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, fb_width - 10, 5, 5, fb_height - 10, rgui_border_filler);
      }
   }
}

static void rgui_set_message(void *data, const char *message)
{
   rgui_t           *rgui = (rgui_t*)data;

   if (!rgui || !message || !*message)
      return;

   if (!string_is_empty(rgui->msgbox))
      free(rgui->msgbox);
   rgui->msgbox       = strdup(message);
   rgui->force_redraw = true;
}

static void rgui_render_messagebox(rgui_t *rgui, const char *message)
{
   int x, y;
   size_t i, fb_pitch;
   unsigned fb_width, fb_height;
   unsigned width, glyphs_width, height;
   struct string_list *list   = NULL;
   settings_t *settings       = config_get_ptr();

   (void)settings;

   if (!message || !*message)
      return;

   list = string_split(message, "\n");
   if (!list)
      return;
   if (list->elems == 0)
      goto end;

   width        = 0;
   glyphs_width = 0;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   for (i = 0; i < list->size; i++)
   {
      unsigned line_width;
      char     *msg   = list->elems[i].data;
      unsigned msglen = (unsigned)utf8len(msg);

      if (msglen > RGUI_TERM_WIDTH(fb_width))
      {
         msg[RGUI_TERM_WIDTH(fb_width) - 2] = '.';
         msg[RGUI_TERM_WIDTH(fb_width) - 1] = '.';
         msg[RGUI_TERM_WIDTH(fb_width) - 0] = '.';
         msg[RGUI_TERM_WIDTH(fb_width) + 1] = '\0';
         msglen = RGUI_TERM_WIDTH(fb_width);
      }

      line_width   = msglen * FONT_WIDTH_STRIDE - 1 + 6 + 10;
      width        = MAX(width, line_width);
      glyphs_width = MAX(glyphs_width, msglen);
   }

   height = (unsigned)(FONT_HEIGHT_STRIDE * list->size + 6 + 10);
   x      = (fb_width  - width) / 2;
   y      = (fb_height - height) / 2;

   if (rgui_framebuf_data)
   {
      rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, x + 5, y + 5, width - 10, height - 10, rgui_bg_filler);

      if (settings->bools.menu_rgui_border_filler_enable)
      {
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, x, y, width - 5, 5, rgui_border_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, x + width - 5, y, 5, height - 5, rgui_border_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, x + 5, y + height - 5, width - 5, 5, rgui_border_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, x, y + 5, 5, height - 5, rgui_border_filler);
      }
   }

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int offset_x    = (int)(FONT_WIDTH_STRIDE * (glyphs_width - utf8len(msg)) / 2);
      int offset_y    = (int)(FONT_HEIGHT_STRIDE * i);

      if (rgui_framebuf_data)
         blit_line(x + 8 + offset_x, y + 8 + offset_y, msg, rgui->colors.normal_color);
   }

end:
   string_list_free(list);
}

static void rgui_blit_cursor(void)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   int16_t        x   = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t        y   = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   if (rgui_framebuf_data)
   {
      rgui_color_rect(rgui_framebuf_data, fb_pitch, fb_width, fb_height, x, y - 5, 1, 11, 0xFFFF);
      rgui_color_rect(rgui_framebuf_data, fb_pitch, fb_width, fb_height, x - 5, y, 11, 1, 0xFFFF);
   }
}

static void rgui_frame(void *data, video_frame_info_t *video_info)
{
   rgui_t *rgui                   = (rgui_t*)data;
   settings_t *settings           = config_get_ptr();

   if ((settings->bools.menu_rgui_background_filler_thickness_enable != rgui->bg_thickness) ||
       (settings->bools.menu_rgui_border_filler_thickness_enable     != rgui->border_thickness)
      )
      rgui->bg_modified           = true;

   rgui->bg_thickness             = settings->bools.menu_rgui_background_filler_thickness_enable;
   rgui->border_thickness         = settings->bools.menu_rgui_border_filler_thickness_enable;

   if (settings->uints.menu_rgui_color_theme != rgui->color_theme)
   {
      prepare_rgui_colors(rgui, settings);
      rgui->bg_modified = true;
   }

   rgui->frame_count++;
}

static void rgui_render(void *data, bool is_idle)
{
   menu_animation_ctx_ticker_t ticker;
   unsigned x, y;
   size_t i, end, fb_pitch, old_start;
   unsigned fb_width, fb_height;
   int bottom;
   char title[255];
   char title_buf[255];
   char title_msg[64];
   char msg[255];
   size_t entries_end             = 0;
   bool msg_force                 = false;
   settings_t *settings           = config_get_ptr();
   rgui_t *rgui                   = (rgui_t*)data;
   uint64_t frame_count           = rgui->frame_count;

   msg[0] = title[0] = title_buf[0] = title_msg[0] = '\0';

   if (!rgui->force_redraw)
   {
      msg_force = menu_display_get_msg_force();

      if (menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL)
            && menu_driver_is_alive() && !msg_force)
         return;

      if (is_idle || !menu_display_get_update_pending())
         return;
   }

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   /* if the framebuffer changed size, recache the background */
   if (rgui->bg_modified || rgui->last_width != fb_width || rgui->last_height != fb_height)
   {
      if (rgui_framebuf_data)
      {
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, 0, fb_height, fb_width, 4, rgui_bg_filler);
      }
      rgui->last_width  = fb_width;
      rgui->last_height = fb_height;
   }
   
   if (rgui->bg_modified)
      rgui->bg_modified = false;

   menu_display_set_framebuffer_dirty_flag();
   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);

   rgui->force_redraw        = false;

   if (settings->bools.menu_pointer_enable)
   {
      unsigned new_val;

      menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

      new_val = (unsigned)(menu_input_pointer_state(MENU_POINTER_Y_AXIS)
         / (11 - 2 + old_start));

      menu_input_ctl(MENU_INPUT_CTL_POINTER_PTR, &new_val);

      if (menu_input_ctl(MENU_INPUT_CTL_IS_POINTER_DRAGGED, NULL))
      {
         size_t start;
         int16_t delta_y = menu_input_pointer_state(MENU_POINTER_DELTA_Y_AXIS);
         rgui->scroll_y += delta_y;

         start = -rgui->scroll_y / 11 + 2;

         menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);

         if (rgui->scroll_y > 0)
            rgui->scroll_y = 0;
      }
   }

   if (settings->bools.menu_mouse_enable)
   {
      unsigned new_mouse_ptr;
      int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

      new_mouse_ptr = (unsigned)(mouse_y / 11 - 2 + old_start);

      menu_input_ctl(MENU_INPUT_CTL_MOUSE_PTR, &new_mouse_ptr);
   }

   /* Do not scroll if all items are visible. */
   if (menu_entries_get_size() <= RGUI_TERM_HEIGHT(fb_width, fb_height))
   {
      size_t start = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
   }

   bottom    = (int)(menu_entries_get_size() - RGUI_TERM_HEIGHT(fb_width, fb_height));
   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

   if (old_start > (unsigned)bottom)
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &bottom);

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

   entries_end = menu_entries_get_size();

   end         = ((old_start + RGUI_TERM_HEIGHT(fb_width, fb_height)) <= (entries_end)) ?
      old_start + RGUI_TERM_HEIGHT(fb_width, fb_height) : entries_end;

   rgui_render_background(rgui);

   menu_entries_get_title(title, sizeof(title));

   ticker.s        = title_buf;
   ticker.len      = RGUI_TERM_WIDTH(fb_width) - 10;
   ticker.idx      = frame_count / RGUI_TERM_START_X(fb_width);
   ticker.str      = title;
   ticker.selected = true;

   menu_animation_ticker(&ticker);

   if (menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL))
   {
      char back_buf[32];
      char back_msg[32];

      back_buf[0] = back_msg[0] = '\0';

      strlcpy(back_buf, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK), sizeof(back_buf));
      string_to_upper(back_buf);
      if (rgui_framebuf_data)
         blit_line(
               RGUI_TERM_START_X(fb_width),
               RGUI_TERM_START_X(fb_width),
               back_msg,
               rgui->colors.title_color);
   }

   string_to_upper(title_buf);

   if (rgui_framebuf_data)
      blit_line(
            (int)(RGUI_TERM_START_X(fb_width) + (RGUI_TERM_WIDTH(fb_width)
                  - utf8len(title_buf)) * FONT_WIDTH_STRIDE / 2),
            RGUI_TERM_START_X(fb_width),
            title_buf, rgui->colors.title_color);

   if (settings->bools.menu_core_enable &&
         menu_entries_get_core_title(title_msg, sizeof(title_msg)) == 0)
   {
      if (rgui_framebuf_data)
         blit_line(
               RGUI_TERM_START_X(fb_width),
               (RGUI_TERM_HEIGHT(fb_width, fb_height) * FONT_HEIGHT_STRIDE) +
               RGUI_TERM_START_Y(fb_height) + 2, title_msg, rgui->colors.hover_color);
   }

   if (settings->bools.menu_timedate_enable)
   {
      menu_display_ctx_datetime_t datetime;
      char timedate[255];

      timedate[0]        = '\0';

      datetime.s         = timedate;
      datetime.len       = sizeof(timedate);
      datetime.time_mode = 4;

      menu_display_timedate(&datetime);

      if (rgui_framebuf_data)
         blit_line(
               RGUI_TERM_WIDTH(fb_width) * FONT_WIDTH_STRIDE - RGUI_TERM_START_X(fb_width),
               (RGUI_TERM_HEIGHT(fb_width, fb_height) * FONT_HEIGHT_STRIDE) +
               RGUI_TERM_START_Y(fb_height) + 2, timedate, rgui->colors.hover_color);
   }

   x = RGUI_TERM_START_X(fb_width);
   y = RGUI_TERM_START_Y(fb_height);

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   for (; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      menu_entry_t entry;
      menu_animation_ctx_ticker_t ticker;
      char entry_value[255];
      char message[255];
      char entry_title_buf[255];
      char type_str_buf[255];
      char *entry_path                      = NULL;
      unsigned entry_spacing                = 0;
      size_t entry_title_buf_utf8len        = 0;
      size_t entry_title_buf_len            = 0;
      bool                entry_selected    = menu_entry_is_currently_selected((unsigned)i);
      size_t selection                      = menu_navigation_get_selection();

      if (i > (selection + 100))
         continue;

      entry_value[0]     = '\0';
      message[0]         = '\0';
      entry_title_buf[0] = '\0';
      type_str_buf[0]    = '\0';

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

      entry_spacing = menu_entry_get_spacing(&entry);
      menu_entry_get_value(&entry, entry_value, sizeof(entry_value));
      entry_path      = menu_entry_get_rich_label(&entry);

      ticker.s        = entry_title_buf;
      ticker.len      = RGUI_TERM_WIDTH(fb_width) - (entry_spacing + 1 + 2);
      ticker.idx      = frame_count / RGUI_TERM_START_X(fb_width);
      ticker.str      = entry_path;
      ticker.selected = entry_selected;

      menu_animation_ticker(&ticker);

      ticker.s        = type_str_buf;
      ticker.len      = entry_spacing;
      ticker.str      = entry_value;

      menu_animation_ticker(&ticker);

      entry_title_buf_utf8len = utf8len(entry_title_buf);
      entry_title_buf_len     = strlen(entry_title_buf);

      snprintf(message, sizeof(message), "%c %-*.*s %-.*s",
            entry_selected ? '>' : ' ',
            (int)(RGUI_TERM_WIDTH(fb_width) - (entry_spacing + 1 + 2) - entry_title_buf_utf8len + entry_title_buf_len),
            (int)(RGUI_TERM_WIDTH(fb_width) - (entry_spacing + 1 + 2) - entry_title_buf_utf8len + entry_title_buf_len),
            entry_title_buf,
            entry_spacing,
            type_str_buf);

      if (rgui_framebuf_data)
         blit_line(x, y, message,
               entry_selected ? rgui->colors.hover_color : rgui->colors.normal_color);

      menu_entry_free(&entry);
      if (!string_is_empty(entry_path))
         free(entry_path);
   }

   if (menu_input_dialog_get_display_kb())
   {
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      rgui_render_messagebox(rgui, msg);
   }

   if (!string_is_empty(rgui->msgbox))
   {
      rgui_render_messagebox(rgui, rgui->msgbox);
      free(rgui->msgbox);
      rgui->msgbox       = NULL;
      rgui->force_redraw = true;
   }

   if (rgui->mouse_show)
   {
      settings_t *settings = config_get_ptr();
      bool cursor_visible  = settings->bools.video_fullscreen ||
         !video_driver_has_windowed();

      if (settings->bools.menu_mouse_enable && cursor_visible)
         rgui_blit_cursor();
   }

}

static void rgui_framebuffer_free(void)
{
   if (rgui_framebuf_data)
      free(rgui_framebuf_data);
   rgui_framebuf_data = NULL;
}

static void *rgui_init(void **userdata, bool video_is_threaded)
{
   size_t fb_pitch, start;
   unsigned fb_width, fb_height, new_font_height;
   rgui_t               *rgui = NULL;
   bool                   ret = false;
   settings_t *settings       = config_get_ptr();
   menu_handle_t        *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   rgui = (rgui_t*)calloc(1, sizeof(rgui_t));

   if (!rgui)
      goto error;

   *userdata              = rgui;

   /* Prepare RGUI colors, to improve performance */
   prepare_rgui_colors(rgui, settings);

   /* 4 extra lines to cache  the checked background */
   rgui_framebuf_data = (uint16_t*)
      calloc(400 * (240 + 4), sizeof(uint16_t));

   if (!rgui_framebuf_data)
      goto error;

   fb_width                   = 320;
   fb_height                  = 240;
   fb_pitch                   = fb_width * sizeof(uint16_t);
   new_font_height            = FONT_HEIGHT_STRIDE * 2;

   menu_display_set_width(fb_width);
   menu_display_set_height(fb_height);
   menu_display_set_header_height(new_font_height);
   menu_display_set_framebuffer_pitch(fb_pitch);

   start = 0;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);

   ret = rguidisp_init_font(menu);

   if (!ret)
      goto error;

   rgui->bg_thickness             = settings->bools.menu_rgui_background_filler_thickness_enable;
   rgui->border_thickness         = settings->bools.menu_rgui_border_filler_thickness_enable;
   rgui->bg_modified              = true;

   rgui->last_width  = fb_width;
   rgui->last_height = fb_height;

   return menu;

error:
   rgui_framebuffer_free();
   if (menu)
      free(menu);
   return NULL;
}

static void rgui_free(void *data)
{
   const uint8_t *font_fb;
   bool fb_font_inited   = false;

   fb_font_inited = menu_display_get_font_data_init();
   font_fb = menu_display_get_font_framebuffer();

   if (fb_font_inited)
      free((void*)font_fb);

   fb_font_inited = false;
   menu_display_set_font_data_init(fb_font_inited);
}

static void rgui_set_texture(void)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;

   if (!menu_display_get_framebuffer_dirty_flag())
      return;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   menu_display_unset_framebuffer_dirty_flag();

   video_driver_set_texture_frame(rgui_framebuf_data,
         false, fb_width, fb_height, 1.0f);
}

static void rgui_navigation_clear(void *data, bool pending_push)
{
   size_t start;
   rgui_t           *rgui = (rgui_t*)data;
   if (!rgui)
      return;

   start = 0;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
   rgui->scroll_y = 0;
}

static void rgui_navigation_set(void *data, bool scroll)
{
   size_t start, fb_pitch;
   unsigned fb_width, fb_height;
   bool do_set_start              = false;
   size_t end                     = menu_entries_get_size();
   size_t selection               = menu_navigation_get_selection();

   if (!scroll)
      return;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   if (selection < RGUI_TERM_HEIGHT(fb_width, fb_height) /2)
   {
      start        = 0;
      do_set_start = true;
   }
   else if (selection >= (RGUI_TERM_HEIGHT(fb_width, fb_height) /2)
         && selection < (end - RGUI_TERM_HEIGHT(fb_width, fb_height) /2))
   {
      start        = selection - RGUI_TERM_HEIGHT(fb_width, fb_height) /2;
      do_set_start = true;
   }
   else if (selection >= (end - RGUI_TERM_HEIGHT(fb_width, fb_height) /2))
   {
      start        = end - RGUI_TERM_HEIGHT(fb_width, fb_height);
      do_set_start = true;
   }

   if (do_set_start)
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
}

static void rgui_navigation_set_last(void *data)
{
   rgui_navigation_set(data, true);
}

static void rgui_navigation_descend_alphabet(void *data, size_t *unused)
{
   rgui_navigation_set(data, true);
}

static void rgui_navigation_ascend_alphabet(void *data, size_t *unused)
{
   rgui_navigation_set(data, true);
}

static void rgui_populate_entries(void *data,
      const char *path,
      const char *label, unsigned k)
{
   rgui_navigation_set(data, true);
}

static int rgui_environ(enum menu_environ_cb type,
      void *data, void *userdata)
{
   rgui_t           *rgui = (rgui_t*)userdata;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         if (!rgui)
            return -1;
         rgui->mouse_show = true;
         menu_display_set_framebuffer_dirty_flag();
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         if (!rgui)
            return -1;
         rgui->mouse_show = false;
         menu_display_unset_framebuffer_dirty_flag();
         break;
      case 0:
      default:
         break;
   }

   return -1;
}

static int rgui_pointer_tap(void *data,
      unsigned x, unsigned y,
      unsigned ptr, menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned header_height = menu_display_get_header_height();

   if (y < header_height)
   {
      size_t selection = menu_navigation_get_selection();
      return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_CANCEL);
   }
   else if (ptr <= (menu_entries_get_size() - 1))
   {
      size_t selection         = menu_navigation_get_selection();

      if (ptr == selection && cbs && cbs->action_select)
         return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_SELECT);

      menu_navigation_set_selection(ptr);
      menu_driver_navigation_set(false);
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_rgui = {
   rgui_set_texture,
   rgui_set_message,
   generic_menu_iterate,
   rgui_render,
   rgui_frame,
   rgui_init,
   rgui_free,
   NULL,
   NULL,
   rgui_populate_entries,
   NULL,
   rgui_navigation_clear,
   NULL,
   NULL,
   rgui_navigation_set,
   rgui_navigation_set_last,
   rgui_navigation_descend_alphabet,
   rgui_navigation_ascend_alphabet,
   generic_menu_init_list,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "rgui",
   rgui_environ,
   rgui_pointer_tap,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL
};

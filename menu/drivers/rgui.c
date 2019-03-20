/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <lists/file_list.h>
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

#include "../../playlist.h"
#include "../../frontend/frontend_driver.h"

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"

#include "../widgets/menu_input_dialog.h"

#include "../../configuration.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"

#include <file/config_file.h>

/* Thumbnail additions */
#include "../menu_thumbnail_path.h"
#include <streams/file_stream.h>
#include "../../tasks/tasks_internal.h"
#include <gfx/scaler/scaler.h>

#if defined(GEKKO)
#define RGUI_TERM_START_X(fb_width)        (fb_width / 21)
#define RGUI_TERM_START_Y(fb_height)       (fb_height / 9)
#define RGUI_TERM_WIDTH(fb_width)          (((fb_width - RGUI_TERM_START_X(fb_width) - RGUI_TERM_START_X(fb_width)) / (FONT_WIDTH_STRIDE)))
#define RGUI_TERM_HEIGHT(fb_height)        (((fb_height - RGUI_TERM_START_Y(fb_height) - RGUI_TERM_START_Y(fb_height)) / (FONT_HEIGHT_STRIDE)))
#else
#define RGUI_TERM_START_X(fb_width)        rgui_term_layout.start_x
#define RGUI_TERM_START_Y(fb_height)       rgui_term_layout.start_y
#define RGUI_TERM_WIDTH(fb_width)          rgui_term_layout.width
#define RGUI_TERM_HEIGHT(fb_height)        rgui_term_layout.height
#endif

#define RGUI_ENTRY_VALUE_MAXLEN 19

#define RGUI_TICKER_SPACER " | "

typedef struct
{
   unsigned start_x;
   unsigned start_y;
   unsigned width;
   unsigned height;
   unsigned value_maxlen;
} rgui_term_layout_t;

static rgui_term_layout_t rgui_term_layout = {0};

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
   unsigned aspect_ratio_idx;
   video_viewport_t viewport;
} rgui_video_settings_t;

typedef struct
{
   bool bg_modified;
   bool force_redraw;
   bool mouse_show;
   unsigned last_width;
   unsigned last_height;
   bool bg_thickness;
   bool border_thickness;
   float scroll_y;
   char *msgbox;
   unsigned color_theme;
   rgui_colors_t colors;
   bool is_playlist;
   bool entry_has_thumbnail;
   bool show_thumbnail;
   menu_thumbnail_path_data_t *thumbnail_path_data;
   uint32_t thumbnail_queue_size;
   bool show_wallpaper;
   char theme_preset_path[PATH_MAX_LENGTH]; /* Must be a fixed length array... */
   char menu_title[255]; /* Must be a fixed length array... */
   char menu_sublabel[255]; /* Must be a fixed length array... */
   unsigned menu_aspect_ratio;
   unsigned menu_aspect_ratio_lock;
   rgui_video_settings_t menu_video_settings;
   rgui_video_settings_t content_video_settings;
   struct scaler_ctx image_scaler;
} rgui_t;

typedef struct
{
   unsigned width;
   unsigned height;
   bool is_valid;
   char *path;
   uint16_t *data;
} thumbnail_t;

static thumbnail_t thumbnail = {
   0,
   0,
   false,
   NULL,
   NULL
};

typedef struct
{
   bool is_valid;
   char *path;
   uint16_t *data;
} wallpaper_t;

static wallpaper_t wallpaper = {
   false,
   NULL,
   NULL
};

typedef struct
{
   unsigned width;
   unsigned height;
   uint16_t *data;
} frame_buf_t;

static frame_buf_t rgui_frame_buf = {
   0,
   0,
   NULL
};

static frame_buf_t rgui_upscale_buf = {
   0,
   0,
   NULL
};

#if defined(PS2)

static uint16_t argb32_to_abgr1555(uint32_t col)
{
   /* Extract colour components */
   unsigned a = (col >> 24) & 0xff;
   unsigned r = (col >> 16) & 0xff;
   unsigned g = (col >> 8)  & 0xff;
   unsigned b = col & 0xff;
   if (a < 0xff)
   {
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
   }
   /* Convert from 8 bit to 5 bit */
   r = r >> 3;
   g = g >> 3;
   b = b >> 3;
   /* Return final value - alpha always set to 1 */
   return (1 << 15) | (b << 10) | (g << 5) | r;
}

#define argb32_to_pixel_platform_format(color) argb32_to_abgr1555(color)

#elif defined(GEKKO)

static uint16_t argb32_to_rgb5a3(uint32_t col)
{
   /* Extract colour components */
   unsigned a = (col >> 24) & 0xff;
   unsigned r = (col >> 16) & 0xff;
   unsigned g = (col >> 8)  & 0xff;
   unsigned b = col & 0xff;
   unsigned a3 = a >> 5;
   if (a < 0xff)
   {
      /* Gekko platforms only have a 3 bit alpha channel, which
       * is one bit less than all 'standard' target platforms.
       * As a result, Gekko colours are effectively ~6-7% less
       * transparent than expected, which causes backgrounds and
       * borders to appear too bright. We therefore have to darken
       * each RGB component according to the difference between Gekko
       * alpha and normal 4 bit alpha values... */
      unsigned a4 = a >> 4;
      float a_factor = 1.0;
      if (a3 > 0)
      {
         /* Avoid divide by zero errors... */
         a_factor = ((float)a4 * (1.0 / 15.0)) / ((float)a3 * (1.0 / 7.0));
      }
      r = (unsigned)(((float)r * a_factor) + 0.5);
      g = (unsigned)(((float)g * a_factor) + 0.5);
      b = (unsigned)(((float)b * a_factor) + 0.5);
      /* a_factor can actually be greater than 1. This will never happen
       * with the current preset theme colour values, but users can set
       * any custom values they like, so we have to play it safe... */
      r = (r <= 0xff) ? r : 0xff;
      g = (g <= 0xff) ? g : 0xff;
      b = (b <= 0xff) ? b : 0xff;
   }
   /* Convert RGB from 8 bit to 4 bit */
   r = r >> 4;
   g = g >> 4;
   b = b >> 4;
   /* Return final value */
   return (a3 << 12) | (r << 8) | (g << 4) | b;
}

#define argb32_to_pixel_platform_format(color) argb32_to_rgb5a3(color)

#elif defined(PSP)

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

static bool request_wallpaper(const char *path)
{
   /* Do nothing if current wallpaper path hasn't changed */
   if (!string_is_empty(path) && !string_is_empty(wallpaper.path))
   {
      if (string_is_equal(wallpaper.path, path))
         return true;
   }

   /* 'Reset' current wallpaper */
   wallpaper.is_valid = false;
   free(wallpaper.path);
   wallpaper.path = NULL;

   /* Ensure that new path is valid... */
   if (!string_is_empty(path))
   {
      wallpaper.path = strdup(path);
      if (filestream_exists(path))
      {
         /* Unlike thumbnails, we don't worry about queued images
          * here - in general, wallpaper is loaded once per session
          * and then forgotten, so performance issues are not a concern */
         task_push_image_load(wallpaper.path, menu_display_handle_wallpaper_upload, NULL);
         return true;
      }
   }

   return false;
}

static void process_wallpaper(rgui_t *rgui, struct texture_image *image)
{
   unsigned x, y;

   /* Sanity check */
   if (!image->pixels || (image->width != rgui_frame_buf.width) || (image->height != rgui_frame_buf.height) || !wallpaper.data)
      return;

   /* Copy image to wallpaper buffer, performing pixel format conversion */
   for (x = 0; x < rgui_frame_buf.width; x++)
   {
      for (y = 0; y < rgui_frame_buf.height; y++)
      {
         wallpaper.data[x + (y * rgui_frame_buf.width)] =
            argb32_to_pixel_platform_format(image->pixels[x + (y * rgui_frame_buf.width)]);
      }
   }

   wallpaper.is_valid = true;

   /* Tell menu that a display update is required */
   rgui->force_redraw = true;
}

static bool request_thumbnail(rgui_t *rgui, const char *path)
{
   /* Do nothing if current thumbnail path hasn't changed */
   if (!string_is_empty(path) && !string_is_empty(thumbnail.path))
   {
      if (string_is_equal(thumbnail.path, path))
         return true;
   }

   /* 'Reset' current thumbnail */
   thumbnail.width = 0;
   thumbnail.height = 0;
   thumbnail.is_valid = false;
   free(thumbnail.path);
   thumbnail.path = NULL;

   /* Ensure that new path is valid... */
   if (!string_is_empty(path))
   {
      thumbnail.path = strdup(path);
      if (filestream_exists(path))
      {
         /* Would like to cancel any existing image load tasks
          * here, but can't see how to do it... */
         if(task_push_image_load(thumbnail.path, menu_display_handle_thumbnail_upload, NULL))
         {
            rgui->thumbnail_queue_size++;
            return true;
         }
      }
   }
   
   return false;
}

static bool downscale_thumbnail(rgui_t *rgui, struct texture_image *image_src, struct texture_image *image_dst)
{
   settings_t *settings = config_get_ptr();

   /* Determine output dimensions */
   float display_aspect_ratio = (float)rgui_frame_buf.width / (float)rgui_frame_buf.height;
   float aspect_ratio = (float)image_src->width / (float)image_src->height;
   if (aspect_ratio > display_aspect_ratio)
   {
      image_dst->width = rgui_frame_buf.width;
      image_dst->height = image_src->height * rgui_frame_buf.width / image_src->width;
      /* Account for any possible rounding errors... */
      image_dst->height = (image_dst->height < 1) ? 1 : image_dst->height;
      image_dst->height = (image_dst->height > rgui_frame_buf.height) ? rgui_frame_buf.height : image_dst->height;
   }
   else
   {
      image_dst->height = rgui_frame_buf.height;
      image_dst->width = image_src->width * rgui_frame_buf.height / image_src->height;
      /* Account for any possible rounding errors... */
      image_dst->width = (image_dst->width < 1) ? 1 : image_dst->width;
      image_dst->width = (image_dst->width > rgui_frame_buf.width) ? rgui_frame_buf.width : image_dst->width;
   }

   /* Allocate pixel buffer */
   image_dst->pixels = (uint32_t*)calloc(image_dst->width * image_dst->height, sizeof(uint32_t));
   if (!image_dst->pixels)
      return false;

   /* Determine scaling method */
   if (settings->uints.menu_rgui_thumbnail_downscaler == RGUI_THUMB_SCALE_POINT)
   {
      uint32_t x_ratio, y_ratio;
      unsigned x_src, y_src;
      unsigned x_dst, y_dst;

      /* Perform nearest neighbour resampling
       * > Fastest method, minimal performance impact */
      x_ratio = ((image_src->width  << 16) / image_dst->width);
      y_ratio = ((image_src->height << 16) / image_dst->height);

      for (y_dst = 0; y_dst < image_dst->height; y_dst++)
      {
         y_src = (y_dst * y_ratio) >> 16;
         for (x_dst = 0; x_dst < image_dst->width; x_dst++)
         {
            x_src = (x_dst * x_ratio) >> 16;
            image_dst->pixels[(y_dst * image_dst->width) + x_dst] = image_src->pixels[(y_src * image_src->width) + x_src];
         }
      }
   }
   else
   {
      /* Perform either bilinear or sinc (Lanczos3) resampling
       * using libretro-common scaler
       * > Better quality, but substantially higher performance
       *   impact - although not an issue on desktop-class
       *   hardware */
      rgui->image_scaler.in_width    = image_src->width;
      rgui->image_scaler.in_height   = image_src->height;
      rgui->image_scaler.in_stride   = image_src->width * sizeof(uint32_t);
      rgui->image_scaler.in_fmt      = SCALER_FMT_ARGB8888;

      rgui->image_scaler.out_width   = image_dst->width;
      rgui->image_scaler.out_height  = image_dst->height;
      rgui->image_scaler.out_stride  = image_dst->width * sizeof(uint32_t);
      rgui->image_scaler.out_fmt     = SCALER_FMT_ARGB8888;

      rgui->image_scaler.scaler_type = (settings->uints.menu_rgui_thumbnail_downscaler == RGUI_THUMB_SCALE_SINC) ?
         SCALER_TYPE_SINC : SCALER_TYPE_BILINEAR;

      /* This reset is redundant, since scaler_ctx_gen_filter()
       * calls it - but do it anyway in case the
       * scaler_ctx_gen_filter() internals ever change... */
      scaler_ctx_gen_reset(&rgui->image_scaler);
      if(!scaler_ctx_gen_filter(&rgui->image_scaler))
      {
         /* Could be leftovers if scaler_ctx_gen_filter()
          * fails, so reset just in case... */
         scaler_ctx_gen_reset(&rgui->image_scaler);
         return false;
      }

      scaler_ctx_scale(&rgui->image_scaler, image_dst->pixels, image_src->pixels);
      /* Reset again - don't want to leave anything hanging around
       * if the user switches back to nearest neighbour scaling */
      scaler_ctx_gen_reset(&rgui->image_scaler);
   }

   return true;
}

static void process_thumbnail(rgui_t *rgui, struct texture_image *image_src)
{
   unsigned x, y;
   struct texture_image *image = NULL;
   struct texture_image image_resampled = {
      0,
      0,
      NULL,
      false
   };

   /* Ensure that we only process the most recently loaded
    * thumbnail image (i.e. don't waste CPU cycles processing
    * old images if we have a backlog)
    * > NB: After some testing, cannot seem to ever trigger a
    *   situation where rgui->thumbnail_queue_size is greater
    *   than 1, so perhaps image loading is synchronous after all.
    *   This probably makes the check redundant, but we'll leave
    *   it here for now... */
   if (rgui->thumbnail_queue_size > 0)
      rgui->thumbnail_queue_size--;
   if (rgui->thumbnail_queue_size > 0)
      return;

   /* Sanity check */
   if (!image_src->pixels || (image_src->width < 1) || (image_src->height < 1) || !thumbnail.data)
      return;

   /* Downscale thumbnail if it exceeds maximum size limits */
   if ((image_src->width > rgui_frame_buf.width) || (image_src->height > rgui_frame_buf.height))
   {
      if (!downscale_thumbnail(rgui, image_src, &image_resampled))
         return;
      image = &image_resampled;
   }
   else
   {
      image = image_src;
   }

   thumbnail.width = image->width;
   thumbnail.height = image->height;

   /* Copy image to thumbnail buffer, performing pixel format conversion */
   for (x = 0; x < thumbnail.width; x++)
   {
      for (y = 0; y < thumbnail.height; y++)
      {
         thumbnail.data[x + (y * thumbnail.width)] =
            argb32_to_pixel_platform_format(image->pixels[x + (y * thumbnail.width)]);
      }
   }

   thumbnail.is_valid = true;

   /* Tell menu that a display update is required */
   rgui->force_redraw = true;

   /* Clean up */
   image = NULL;
   if (image_resampled.pixels)
      free(image_resampled.pixels);
   image_resampled.pixels = NULL;
}

static bool rgui_load_image(void *userdata, void *data, enum menu_image_type type)
{
   rgui_t *rgui = (rgui_t*)userdata;

   if (!rgui || !data)
      return false;

   switch (type)
   {
      case MENU_IMAGE_WALLPAPER:
         {
            struct texture_image *image = (struct texture_image*)data;
            process_wallpaper(rgui, image);
         }
         break;
      case MENU_IMAGE_THUMBNAIL:
         {
            struct texture_image *image = (struct texture_image*)data;
            process_thumbnail(rgui, image);
         }
         break;
      default:
         break;
   }

   return true;
}

static bool rgui_render_wallpaper(void)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;

   if (wallpaper.is_valid && rgui_frame_buf.data && wallpaper.data)
   {
      menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

      /* Sanity check */
      if ((fb_width != rgui_frame_buf.width) || (fb_height != rgui_frame_buf.height) || (fb_pitch != rgui_frame_buf.width << 1))
         return false;

      /* Copy wallpaper to framebuffer */
      memcpy(rgui_frame_buf.data, wallpaper.data, rgui_frame_buf.width * rgui_frame_buf.height * sizeof(uint16_t));

      return true;
   }

   return false;
}

static void rgui_render_thumbnail(void)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   unsigned x, y;
   unsigned fb_x_offset, fb_y_offset;
   unsigned thumb_x_offset, thumb_y_offset;
   unsigned width, height;

   if (thumbnail.is_valid && rgui_frame_buf.data && thumbnail.data)
   {
      menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

      /* Ensure that thumbnail is centred
       * > Have to perform some stupid tests here because we
       *   cannot assume fb_width and fb_height are constant and
       *   >= thumbnail.width and thumbnail.height (even though
       *   they are...) */
      if (thumbnail.width <= fb_width)
      {
         thumb_x_offset = 0;
         fb_x_offset = (fb_width - thumbnail.width) >> 1;
         width = thumbnail.width;
      }
      else
      {
         thumb_x_offset = (thumbnail.width - fb_width) >> 1;
         fb_x_offset = 0;
         width = fb_width;
      }
      if (thumbnail.height <= fb_height)
      {
         thumb_y_offset = 0;
         fb_y_offset = (fb_height - thumbnail.height) >> 1;
         height = thumbnail.height;
      }
      else
      {
         thumb_y_offset = (thumbnail.height - fb_height) >> 1;
         fb_y_offset = 0;
         height = fb_height;
      }

      /* Copy thumbnail to framebuffer */
      for (y = 0; y < height; y++)
      {
         for (x = 0; x < width; x++)
         {
            rgui_frame_buf.data[(y + fb_y_offset) * (fb_pitch >> 1) + (x + fb_x_offset)] =
               thumbnail.data[(x + thumb_x_offset) + ((y + thumb_y_offset) * thumbnail.width)];
         }
      }
   }
}

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

static void load_custom_theme(rgui_t *rgui, rgui_theme_t *theme_colors, const char *theme_path)
{
   settings_t *settings = config_get_ptr();
   config_file_t *conf = NULL;
   char *wallpaper_key = NULL;
   unsigned normal_color, hover_color, title_color,
      bg_dark_color, bg_light_color,
      border_dark_color, border_light_color;
   char wallpaper_file[PATH_MAX_LENGTH];
   bool success = false;

   /* Determine which type of wallpaper to load */
   switch (settings->uints.menu_rgui_aspect_ratio)
   {
      case RGUI_ASPECT_RATIO_16_9:
      case RGUI_ASPECT_RATIO_16_9_CENTRE:
         wallpaper_key = "rgui_wallpaper_16_9";
         break;
      case RGUI_ASPECT_RATIO_16_10:
      case RGUI_ASPECT_RATIO_16_10_CENTRE:
         wallpaper_key = "rgui_wallpaper_16_10";
         break;
      default:
         /* 4:3 */
         wallpaper_key = "rgui_wallpaper";
         break;
   }

   wallpaper_file[0] = '\0';

   /* Sanity check */
   if (string_is_empty(theme_path))
      goto end;
   if (!filestream_exists(theme_path))
      goto end;

   /* Open config file */
   conf = config_file_new(theme_path);
   if (!conf)
      goto end;

   /* Parse config file */
   if(!config_get_hex(conf, "rgui_entry_normal_color", &normal_color))
      goto end;

   if(!config_get_hex(conf, "rgui_entry_hover_color", &hover_color))
      goto end;

   if(!config_get_hex(conf, "rgui_title_color", &title_color))
      goto end;

   if(!config_get_hex(conf, "rgui_bg_dark_color", &bg_dark_color))
      goto end;

   if(!config_get_hex(conf, "rgui_bg_light_color", &bg_light_color))
      goto end;

   if(!config_get_hex(conf, "rgui_border_dark_color", &border_dark_color))
      goto end;

   if(!config_get_hex(conf, "rgui_border_light_color", &border_light_color))
      goto end;

   config_get_array(conf, wallpaper_key, wallpaper_file, sizeof(wallpaper_file));

   success = true;

end:

   if (success)
   {
      theme_colors->normal_color = (uint32_t)normal_color;
      theme_colors->hover_color = (uint32_t)hover_color;
      theme_colors->title_color = (uint32_t)title_color;
      theme_colors->bg_dark_color = (uint32_t)bg_dark_color;
      theme_colors->bg_light_color = (uint32_t)bg_light_color;
      theme_colors->border_dark_color = (uint32_t)border_dark_color;
      theme_colors->border_light_color = (uint32_t)border_light_color;

      /* Load wallpaper, if required */
      if (!string_is_empty(wallpaper_file))
      {
         char wallpaper_path[PATH_MAX_LENGTH];
         wallpaper_path[0] = '\0';

         fill_pathname_resolve_relative(wallpaper_path, theme_path, wallpaper_file, sizeof(wallpaper_path));
         rgui->show_wallpaper = request_wallpaper(wallpaper_path);
      }
   }
   else
   {
      /* Use 'Classic Green' fallback */
      theme_colors->normal_color = rgui_theme_classic_green.normal_color;
      theme_colors->hover_color = rgui_theme_classic_green.hover_color;
      theme_colors->title_color = rgui_theme_classic_green.title_color;
      theme_colors->bg_dark_color = rgui_theme_classic_green.bg_dark_color;
      theme_colors->bg_light_color = rgui_theme_classic_green.bg_light_color;
      theme_colors->border_dark_color = rgui_theme_classic_green.border_dark_color;
      theme_colors->border_light_color = rgui_theme_classic_green.border_light_color;
   }

   if (conf)
      config_file_free(conf);
   conf = NULL;
}

static void prepare_rgui_colors(rgui_t *rgui, settings_t *settings)
{
   rgui_theme_t theme_colors;
   rgui->color_theme = settings->uints.menu_rgui_color_theme;
   rgui->show_wallpaper = false;

   if (rgui->color_theme == RGUI_THEME_CUSTOM)
   {
      memcpy(rgui->theme_preset_path, settings->paths.path_rgui_theme_preset, sizeof(rgui->theme_preset_path));
      load_custom_theme(rgui, &theme_colors, settings->paths.path_rgui_theme_preset);
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

   rgui->bg_modified = true;
   rgui->force_redraw = true;
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
                  rgui_frame_buf.data[(y + j) * (pitch >> 1) + (x + i)] = color;
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
   src             = rgui_frame_buf.data + pitch_in_pixels * fb_height;
   dst             = rgui_frame_buf.data;

   while (dst < src)
   {
      memcpy(dst, src, size);
      dst += pitch_in_pixels * 4;
   }

   /* Skip drawing border if we are currently showing a thumbnail */
   if (!(rgui->show_thumbnail && rgui->entry_has_thumbnail && (thumbnail.is_valid || (rgui->thumbnail_queue_size > 0))))
   {
      if (rgui_frame_buf.data)
      {
         settings_t *settings       = config_get_ptr();

         if (settings->bools.menu_rgui_border_filler_enable)
         {
            rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, 5, 5, fb_width - 10, 5, rgui_border_filler);
            rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, 5, fb_height - 10, fb_width - 10, 5, rgui_border_filler);
            rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, 5, 5, 5, fb_height - 10, rgui_border_filler);
            rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, fb_width - 10, 5, 5, fb_height - 10, rgui_border_filler);
         }
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

   if (rgui_frame_buf.data)
   {
      rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, x + 5, y + 5, width - 10, height - 10, rgui_bg_filler);

      if (settings->bools.menu_rgui_border_filler_enable)
      {
         rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, x, y, width - 5, 5, rgui_border_filler);
         rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, x + width - 5, y, 5, height - 5, rgui_border_filler);
         rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, x + 5, y + height - 5, width - 5, 5, rgui_border_filler);
         rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, x, y + 5, 5, height - 5, rgui_border_filler);
      }
   }

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int offset_x    = (int)(FONT_WIDTH_STRIDE * (glyphs_width - utf8len(msg)) / 2);
      int offset_y    = (int)(FONT_HEIGHT_STRIDE * i);

      if (rgui_frame_buf.data)
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

   if (rgui_frame_buf.data)
   {
      rgui_color_rect(rgui_frame_buf.data, fb_pitch, fb_width, fb_height, x, y - 5, 1, 11, 0xFFFF);
      rgui_color_rect(rgui_frame_buf.data, fb_pitch, fb_width, fb_height, x - 5, y, 11, 1, 0xFFFF);
   }
}

static void rgui_render(void *data, bool is_idle)
{
   menu_animation_ctx_ticker_t ticker;
   static const char* const ticker_spacer = RGUI_TICKER_SPACER;
   unsigned x, y;
   size_t i, end, fb_pitch, old_start;
   unsigned fb_width, fb_height;
   int bottom;
   size_t entries_end             = 0;
   bool msg_force                 = false;
   settings_t *settings           = config_get_ptr();
   rgui_t *rgui                   = (rgui_t*)data;

   static bool display_kb         = false;
   bool current_display_cb        = false;

   current_display_cb = menu_input_dialog_get_display_kb();

   if (!rgui->force_redraw)
   {
      msg_force = menu_display_get_msg_force();

      if (menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL)
            && menu_driver_is_alive() && !msg_force)
         return;

      if (!display_kb && !current_display_cb && (is_idle || !menu_display_get_update_pending()))
         return;
   }

   display_kb = current_display_cb;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   /* if the framebuffer changed size, recache the background */
   if (rgui->bg_modified || rgui->last_width != fb_width || rgui->last_height != fb_height)
   {
      if (rgui_frame_buf.data)
      {
         rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch, 0, fb_height, fb_width, 4, rgui_bg_filler);
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
   if (menu_entries_get_size() <= RGUI_TERM_HEIGHT(fb_height))
   {
      size_t start = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
   }

   bottom    = (int)(menu_entries_get_size() - RGUI_TERM_HEIGHT(fb_height));
   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

   if (old_start > (unsigned)bottom)
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &bottom);

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

   entries_end = menu_entries_get_size();

   end         = ((old_start + RGUI_TERM_HEIGHT(fb_height)) <= (entries_end)) ?
      old_start + RGUI_TERM_HEIGHT(fb_height) : entries_end;

   /* Render wallpaper or 'normal' background */
   if (rgui->show_wallpaper && wallpaper.is_valid)
   {
      /* If rgui_render_wallpaper() fails (can't actually happen...)
       * then use 'normal' background fallback */
      if (!rgui_render_wallpaper())
         rgui_render_background(rgui);
   }
   else
   {
      rgui_render_background(rgui);
   }

   /* We use a single ticker for all text animations,
    * with the following configuration: */
   ticker.idx = menu_animation_get_ticker_idx();
   ticker.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
   ticker.spacer = ticker_spacer;

   /* If thumbnails are enabled and we are viewing a playlist,
    * switch to thumbnail view mode if either current thumbnail
    * is valid or we are waiting for current thumbnail to load
    * (if load is pending we'll get a blank screen + title, but
    * this is better than switching back to the text playlist
    * view, which causes ugly flickering when scrolling quickly
    * through a list...) */
   if (rgui->show_thumbnail && rgui->entry_has_thumbnail && (thumbnail.is_valid || (rgui->thumbnail_queue_size > 0)))
   {
      const char *thumbnail_title = NULL;
      char thumbnail_title_buf[255];
      unsigned title_x, title_width;
      thumbnail_title_buf[0] = '\0';

      /* Draw thumbnail */
      rgui_render_thumbnail();

      /* Get thumbnail title */
      if (menu_thumbnail_get_label(rgui->thumbnail_path_data, &thumbnail_title))
      {
         /* Format thumbnail title */
         ticker.s        = thumbnail_title_buf;
         ticker.len      = RGUI_TERM_WIDTH(fb_width) - 10;
         ticker.str      = thumbnail_title;
         ticker.selected = true;
         menu_animation_ticker(&ticker);

         title_width = utf8len(thumbnail_title_buf) * FONT_WIDTH_STRIDE;
         title_x = RGUI_TERM_START_X(fb_width) + ((RGUI_TERM_WIDTH(fb_width) * FONT_WIDTH_STRIDE) - title_width) / 2;

         if (rgui_frame_buf.data)
         {
            /* Draw thumbnail title background */
            rgui_fill_rect(rgui, rgui_frame_buf.data, fb_pitch,
                           title_x - 5, 0, title_width + 10, FONT_HEIGHT_STRIDE, rgui_bg_filler);

            /* Draw thumbnail title */
            blit_line((int)title_x, 0, thumbnail_title_buf, rgui->colors.hover_color);
         }
      }
   }
   else
   {
      /* No thumbnail - render usual text */
      char title_buf[255];
      unsigned timedate_x = (RGUI_TERM_START_X(fb_width) + (RGUI_TERM_WIDTH(fb_width) * FONT_WIDTH_STRIDE)) -
            (5 * FONT_WIDTH_STRIDE);
      unsigned core_name_len = ((timedate_x - RGUI_TERM_START_X(fb_width)) / FONT_WIDTH_STRIDE) - 3;

      /* Print title */
      title_buf[0] = '\0';

      ticker.s        = title_buf;
      ticker.len      = RGUI_TERM_WIDTH(fb_width) - 10;
      ticker.str      = rgui->menu_title;
      ticker.selected = true;

      menu_animation_ticker(&ticker);

      string_to_upper(title_buf);

      if (rgui_frame_buf.data)
         blit_line(
               (int)(RGUI_TERM_START_X(fb_width) + (RGUI_TERM_WIDTH(fb_width)
                     - utf8len(title_buf)) * FONT_WIDTH_STRIDE / 2),
               RGUI_TERM_START_Y(fb_height) - FONT_HEIGHT_STRIDE,
               title_buf, rgui->colors.title_color);

      /* Print menu entries */
      x = RGUI_TERM_START_X(fb_width);
      y = RGUI_TERM_START_Y(fb_height);

      menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

      for (; i < end; i++, y += FONT_HEIGHT_STRIDE)
      {
         char entry_value[255];
         char message[255];
         char entry_title_buf[255];
         char type_str_buf[255];
         menu_entry_t entry;
         char *entry_path                      = NULL;
         size_t entry_title_max_len            = 0;
         size_t entry_title_buf_utf8len        = 0;
         size_t entry_title_buf_len            = 0;
         unsigned entry_value_len              = 0;
         bool entry_selected                   = menu_entry_is_currently_selected((unsigned)i);
         size_t selection                      = menu_navigation_get_selection();

         if (i > (selection + 100))
            continue;

         entry_value[0]     = '\0';
         message[0]         = '\0';
         entry_title_buf[0] = '\0';
         type_str_buf[0]    = '\0';

         /* Get current entry */
         menu_entry_init(&entry);
         menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

         /* Read entry parameters */
         entry_path = menu_entry_get_rich_label(&entry);
         menu_entry_get_value(&entry, entry_value, sizeof(entry_value));

         /* Get base width of entry title field */
         entry_title_max_len = RGUI_TERM_WIDTH(fb_width) - (1 + 2);

         /* Determine whether entry has a value component */
         if (!string_is_empty(entry_value))
         {
            if (settings->bools.menu_rgui_full_width_layout)
            {
               /* Resize fields according to actual length of value string */
               entry_value_len = strlen(entry_value);
               entry_value_len = entry_value_len > rgui_term_layout.value_maxlen ?
                     rgui_term_layout.value_maxlen : entry_value_len;
            }
            else
            {
               /* Use classic fixed width layout */
               entry_value_len = menu_entry_get_spacing(&entry);
            }

            /* Update width of entry title field */
            entry_title_max_len -= entry_value_len + 2;
         }

         menu_entry_free(&entry);

         /* Format entry title string */
         ticker.s        = entry_title_buf;
         ticker.len      = entry_title_max_len;
         ticker.str      = entry_path;
         ticker.selected = entry_selected;

         menu_animation_ticker(&ticker);

         entry_title_buf_utf8len = utf8len(entry_title_buf);
         entry_title_buf_len     = strlen(entry_title_buf);

         if (entry_value_len > 0)
         {
            /* Format entry value string */
            ticker.s        = type_str_buf;
            ticker.len      = entry_value_len;
            ticker.str      = entry_value;

            menu_animation_ticker(&ticker);

            /* Print entry title + value */
            snprintf(message, sizeof(message), "%c %-*.*s  %-.*s",
                  entry_selected ? '>' : ' ',
                  (int)(entry_title_max_len - entry_title_buf_utf8len + entry_title_buf_len),
                  (int)(entry_title_max_len - entry_title_buf_utf8len + entry_title_buf_len),
                  entry_title_buf,
                  entry_value_len,
                  type_str_buf);
         }
         else
         {
            /* No value - just print entry title */
            snprintf(message, sizeof(message), "%c %-*.*s",
                  entry_selected ? '>' : ' ',
                  (int)(entry_title_max_len - entry_title_buf_utf8len + entry_title_buf_len),
                  (int)(entry_title_max_len - entry_title_buf_utf8len + entry_title_buf_len),
                  entry_title_buf);
         }

         if (rgui_frame_buf.data)
            blit_line(x, y, message,
                  entry_selected ? rgui->colors.hover_color : rgui->colors.normal_color);

         if (!string_is_empty(entry_path))
            free(entry_path);
      }

      /* Print menu sublabel/core name (if required) */
      if (settings->bools.menu_show_sublabels && !string_is_empty(rgui->menu_sublabel))
      {
         char sublabel_buf[255];
         sublabel_buf[0] = '\0';

         ticker.s        = sublabel_buf;
         ticker.len      = core_name_len;
         ticker.str      = rgui->menu_sublabel;
         ticker.selected = true;

         menu_animation_ticker(&ticker);

         if (rgui_frame_buf.data)
            blit_line(
                  RGUI_TERM_START_X(fb_width) + FONT_WIDTH_STRIDE,
                  (RGUI_TERM_HEIGHT(fb_height) * FONT_HEIGHT_STRIDE) +
                  RGUI_TERM_START_Y(fb_height) + 2, sublabel_buf, rgui->colors.hover_color);
      }
      else if (settings->bools.menu_core_enable)
      {
         char core_title[64];
         char core_title_buf[64];
         core_title[0] = core_title_buf[0] = '\0';

         if (menu_entries_get_core_title(core_title, sizeof(core_title)) == 0)
         {
            ticker.s        = core_title_buf;
            ticker.len      = core_name_len;
            ticker.str      = core_title;
            ticker.selected = true;

            menu_animation_ticker(&ticker);

            if (rgui_frame_buf.data)
               blit_line(
                     RGUI_TERM_START_X(fb_width) + FONT_WIDTH_STRIDE,
                     (RGUI_TERM_HEIGHT(fb_height) * FONT_HEIGHT_STRIDE) +
                     RGUI_TERM_START_Y(fb_height) + 2, core_title_buf, rgui->colors.hover_color);
         }
      }

      /* Print clock (if required) */
      if (settings->bools.menu_timedate_enable)
      {
         menu_display_ctx_datetime_t datetime;
         char timedate[255];

         timedate[0]        = '\0';

         datetime.s         = timedate;
         datetime.len       = sizeof(timedate);
         datetime.time_mode = 4;

         menu_display_timedate(&datetime);

         if (rgui_frame_buf.data)
            blit_line(
                  timedate_x,
                  (RGUI_TERM_HEIGHT(fb_height) * FONT_HEIGHT_STRIDE) +
                  RGUI_TERM_START_Y(fb_height) + 2, timedate, rgui->colors.hover_color);
      }
   }

   if (current_display_cb)
   {
      char msg[255];
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();
      msg[0] = '\0';

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
      bool cursor_visible  = settings->bools.video_fullscreen ||
         !video_driver_has_windowed();

      if (settings->bools.menu_mouse_enable && cursor_visible)
         rgui_blit_cursor();
   }
}

static void rgui_framebuffer_free(void)
{
   if (rgui_frame_buf.data)
      free(rgui_frame_buf.data);
   rgui_frame_buf.data = NULL;
}

static void rgui_thumbnail_free(void)
{
   thumbnail.width = 0;
   thumbnail.height = 0;
   thumbnail.is_valid = false;
   
   if (!string_is_empty(thumbnail.path))
      free(thumbnail.path);
   thumbnail.path = NULL;
   
   if (thumbnail.data)
      free(thumbnail.data);
   thumbnail.data = NULL;
}

static void rgui_wallpaper_free(void)
{
   wallpaper.is_valid = false;
   
   if (!string_is_empty(wallpaper.path))
      free(wallpaper.path);
   wallpaper.path = NULL;
   
   if (wallpaper.data)
      free(wallpaper.data);
   wallpaper.data = NULL;
}

bool rgui_is_video_config_equal(rgui_video_settings_t *config_a, rgui_video_settings_t *config_b)
{
   return (config_a->aspect_ratio_idx == config_b->aspect_ratio_idx) &&
          (config_a->viewport.width == config_b->viewport.width) &&
          (config_a->viewport.height == config_b->viewport.height) &&
          (config_a->viewport.x == config_b->viewport.x) &&
          (config_a->viewport.y == config_b->viewport.y);
}

static void rgui_get_video_config(rgui_video_settings_t *video_settings)
{
   settings_t *settings        = config_get_ptr();
   /* Could use settings->video_viewport_custom directly,
    * but this seems to be the standard way of doing it... */
   video_viewport_t *custom_vp = video_viewport_get_custom();
   
   if (!settings)
      return;
   
   video_settings->aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;
   video_settings->viewport.width = custom_vp->width;
   video_settings->viewport.height = custom_vp->height;
   video_settings->viewport.x = custom_vp->x;
   video_settings->viewport.y = custom_vp->y;
}

static void rgui_set_video_config(rgui_video_settings_t *video_settings)
{
   settings_t *settings        = config_get_ptr();
   /* Could use settings->video_viewport_custom directly,
    * but this seems to be the standard way of doing it... */
   video_viewport_t *custom_vp = video_viewport_get_custom();
   
   if (!settings)
      return;
   
   settings->uints.video_aspect_ratio_idx = video_settings->aspect_ratio_idx;
   custom_vp->width = video_settings->viewport.width;
   custom_vp->height = video_settings->viewport.height;
   custom_vp->x = video_settings->viewport.x;
   custom_vp->y = video_settings->viewport.y;
   
   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom_vp->width / custom_vp->height;
   
   video_driver_set_aspect_ratio();
}

/* Note: This function is only called when aspect ratio
 * lock is enabled */
static void rgui_update_menu_viewport(rgui_t *rgui)
{
   settings_t *settings = config_get_ptr();
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   struct video_viewport vp;
   
   if (!settings)
      return;
   
   menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);
   video_driver_get_viewport_info(&vp);
   
   /* Could do this once in rgui_init(), but seems cleaner to
    * handle all video config in one place... */
   rgui->menu_video_settings.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;
   
   /* Determine custom viewport layout */
   if (fb_width > 0 && fb_height > 0 && vp.full_width > 0 && vp.full_height > 0)
   {
      /* Check whether we need to perform integer scaling */
      bool do_integer_scaling = (settings->uints.menu_rgui_aspect_ratio_lock == RGUI_ASPECT_RATIO_LOCK_INTEGER);
      
      if (do_integer_scaling)
      {
         unsigned width_scale = (vp.full_width / fb_width);
         unsigned height_scale = (vp.full_height / fb_height);
         unsigned scale = (width_scale <= height_scale) ? width_scale : height_scale;
         
         if (scale > 0)
         {
            rgui->menu_video_settings.viewport.width = scale * fb_width;
            rgui->menu_video_settings.viewport.height = scale * fb_height;
         }
         else
            do_integer_scaling = false;
      }
      
      if (!do_integer_scaling)
      {
         float display_aspect_ratio = (float)vp.full_width / (float)vp.full_height;
         float aspect_ratio = (float)fb_width / (float)fb_height;
         
         if (aspect_ratio > display_aspect_ratio)
         {
            rgui->menu_video_settings.viewport.width = vp.full_width;
            rgui->menu_video_settings.viewport.height = fb_height * vp.full_width / fb_width;
         }
         else
         {
            rgui->menu_video_settings.viewport.height = vp.full_height;
            rgui->menu_video_settings.viewport.width = fb_width * vp.full_height / fb_height;
         }
      }
      
      /* Sanity check */
      rgui->menu_video_settings.viewport.width = (rgui->menu_video_settings.viewport.width < 1) ?
         1 : rgui->menu_video_settings.viewport.width;
      rgui->menu_video_settings.viewport.height = (rgui->menu_video_settings.viewport.height < 1) ?
         1 : rgui->menu_video_settings.viewport.height;
   }
   else
   {
      rgui->menu_video_settings.viewport.width = 1;
      rgui->menu_video_settings.viewport.height = 1;
   }
   
   rgui->menu_video_settings.viewport.x = (vp.full_width - rgui->menu_video_settings.viewport.width) / 2;
   rgui->menu_video_settings.viewport.y = (vp.full_height - rgui->menu_video_settings.viewport.height) / 2;
}

static bool rgui_set_aspect_ratio(rgui_t *rgui)
{
#if !defined(GEKKO)
   settings_t *settings = config_get_ptr();
#endif
   unsigned base_term_width;
   
   rgui_framebuffer_free();
   rgui_thumbnail_free();
   rgui_wallpaper_free();
   
   /* Cache new aspect ratio */
   rgui->menu_aspect_ratio = settings->uints.menu_rgui_aspect_ratio;
   
#if defined(GEKKO)
   
   /* Set frame buffer dimensions
    * and update menu aspect index */
   rgui_frame_buf.height = 240;
   rgui_frame_buf.width = 320;
   base_term_width = rgui_frame_buf.width;
   
   /* Allocate frame buffer
    * (4 extra lines to cache the chequered background) */
   rgui_frame_buf.data = (uint16_t*)calloc(
         400 * (rgui_frame_buf.height + 4), sizeof(uint16_t));
   
#else
   
   /* Set frame buffer dimensions
    * and update menu aspect index */
   rgui_frame_buf.height = 240;
   switch (settings->uints.menu_rgui_aspect_ratio)
   {
      case RGUI_ASPECT_RATIO_16_9:
         rgui_frame_buf.width = 426;
         base_term_width = rgui_frame_buf.width;
         break;
      case RGUI_ASPECT_RATIO_16_9_CENTRE:
         rgui_frame_buf.width = 426;
         base_term_width = 320;
         break;
      case RGUI_ASPECT_RATIO_16_10:
         rgui_frame_buf.width = 384;
         base_term_width = rgui_frame_buf.width;
         break;
      case RGUI_ASPECT_RATIO_16_10_CENTRE:
         rgui_frame_buf.width = 384;
         base_term_width = 320;
         break;
      default:
         /* 4:3 */
         rgui_frame_buf.width = 320;
         base_term_width = rgui_frame_buf.width;
         break;
   }
   
   /* Allocate frame buffer
    * (4 extra lines to cache the chequered background) */
   rgui_frame_buf.data = (uint16_t*)calloc(
         rgui_frame_buf.width * (rgui_frame_buf.height + 4), sizeof(uint16_t));
   
#endif
   
   if (!rgui_frame_buf.data)
      return false;
   
   /* Configure 'menu display' settings */
   menu_display_set_width(rgui_frame_buf.width);
   menu_display_set_height(rgui_frame_buf.height);
   menu_display_set_framebuffer_pitch(rgui_frame_buf.width * sizeof(uint16_t));
   
   /* Determine terminal layout */
   rgui_term_layout.start_x = (3 * 5) + 1;
   rgui_term_layout.start_y = (3 * 5) + FONT_HEIGHT_STRIDE;
   rgui_term_layout.width = (base_term_width - (2 * rgui_term_layout.start_x)) / FONT_WIDTH_STRIDE;
   rgui_term_layout.height = (rgui_frame_buf.height - (2 * rgui_term_layout.start_y)) / FONT_HEIGHT_STRIDE;
   rgui_term_layout.value_maxlen = (unsigned)(((float)RGUI_ENTRY_VALUE_MAXLEN * (float)base_term_width / 320.0f) + 0.5);
   
   /* > 'Start X/Y' adjustments */
   rgui_term_layout.start_x = (rgui_frame_buf.width - (rgui_term_layout.width * FONT_WIDTH_STRIDE)) / 2;
   rgui_term_layout.start_y = (rgui_frame_buf.height - (rgui_term_layout.height * FONT_HEIGHT_STRIDE)) / 2;
   
   /* Allocate thumbnail buffer */
   thumbnail.data = (uint16_t*)calloc(
         rgui_frame_buf.width * rgui_frame_buf.height, sizeof(uint16_t));
   if (!thumbnail.data)
      return false;
   
   /* Allocate wallpaper buffer */
   wallpaper.data = (uint16_t*)calloc(
         rgui_frame_buf.width * rgui_frame_buf.height, sizeof(uint16_t));
   if (!wallpaper.data)
      return false;
   
   /* Trigger background/display update */
   rgui->theme_preset_path[0] = '\0';
   rgui->bg_modified = true;
   rgui->force_redraw = true;
   
   /* If aspect ratio lock is enabled, notify
    * video driver of change */
   if (settings->uints.menu_rgui_aspect_ratio_lock != RGUI_ASPECT_RATIO_LOCK_NONE)
   {
      rgui_update_menu_viewport(rgui);
      rgui_set_video_config(&rgui->menu_video_settings);
   }
   
   return true;
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

   rgui->menu_title[0] = '\0';
   rgui->menu_sublabel[0] = '\0';

   /* Cache initial video settings */
   rgui_get_video_config(&rgui->content_video_settings);

   /* Set aspect ratio
    * - Allocates frame buffer
    * - Configures variable 'menu display' settings */
   rgui->menu_aspect_ratio_lock = settings->uints.menu_rgui_aspect_ratio_lock;
   if (!rgui_set_aspect_ratio(rgui))
      goto error;

   /* Fixed 'menu display' settings */
   new_font_height = FONT_HEIGHT_STRIDE * 2;
   menu_display_set_header_height(new_font_height);

   /* Prepare RGUI colors, to improve performance */
   rgui->theme_preset_path[0] = '\0';
   prepare_rgui_colors(rgui, settings);

   start = 0;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);

   ret = rguidisp_init_font(menu);

   if (!ret)
      goto error;

   rgui->bg_thickness             = settings->bools.menu_rgui_background_filler_thickness_enable;
   rgui->border_thickness         = settings->bools.menu_rgui_border_filler_thickness_enable;
   rgui->bg_modified              = true;

   rgui->last_width  = rgui_frame_buf.width;
   rgui->last_height = rgui_frame_buf.height;

   rgui->thumbnail_path_data = menu_thumbnail_path_init();
   if (!rgui->thumbnail_path_data)
      goto error;

   rgui->thumbnail_queue_size = 0;
   /* Ensure that we start with thumbnails disabled */
   rgui->show_thumbnail = false;

   return menu;

error:
   rgui_framebuffer_free();
   rgui_thumbnail_free();
   rgui_wallpaper_free();
   if (menu)
      free(menu);
   return NULL;
}

static void rgui_free(void *data)
{
   const uint8_t *font_fb;
   bool fb_font_inited   = false;
   rgui_t *rgui = (rgui_t*)data;

   if (rgui)
   {
      if (rgui->thumbnail_path_data)
         free(rgui->thumbnail_path_data);
   }

   fb_font_inited = menu_display_get_font_data_init();
   font_fb = menu_display_get_font_framebuffer();

   if (fb_font_inited)
      free((void*)font_fb);

   fb_font_inited = false;
   menu_display_set_font_data_init(fb_font_inited);

   rgui_framebuffer_free();
   rgui_thumbnail_free();
   rgui_wallpaper_free();

   if (rgui_upscale_buf.data)
   {
      free(rgui_upscale_buf.data);
      rgui_upscale_buf.data = NULL;
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
   }
   else if (settings->uints.menu_rgui_color_theme == RGUI_THEME_CUSTOM)
   {
      if (string_is_not_equal_fast(settings->paths.path_rgui_theme_preset, rgui->theme_preset_path, sizeof(rgui->theme_preset_path)))
      {
         prepare_rgui_colors(rgui, settings);
      }
   }

   if (settings->uints.menu_rgui_aspect_ratio != rgui->menu_aspect_ratio)
      rgui_set_aspect_ratio(rgui);

   if (settings->uints.menu_rgui_aspect_ratio_lock != rgui->menu_aspect_ratio_lock)
   {
      rgui->menu_aspect_ratio_lock = settings->uints.menu_rgui_aspect_ratio_lock;

      if (settings->uints.menu_rgui_aspect_ratio_lock == RGUI_ASPECT_RATIO_LOCK_NONE)
      {
         rgui_set_video_config(&rgui->content_video_settings);
      }
      else
      {
         rgui_update_menu_viewport(rgui);
         rgui_set_video_config(&rgui->menu_video_settings);
      }
   }
}

static void rgui_set_texture(void)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   settings_t *settings = config_get_ptr();

   if (!menu_display_get_framebuffer_dirty_flag())
      return;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   menu_display_unset_framebuffer_dirty_flag();

   if (settings->uints.menu_rgui_internal_upscale_level == RGUI_UPSCALE_NONE)
   {
      video_driver_set_texture_frame(rgui_frame_buf.data,
         false, fb_width, fb_height, 1.0f);
   }
   else
   {
      /* Get viewport dimensions */
      struct video_viewport vp;
      video_driver_get_viewport_info(&vp);
      
      /* If viewport is currently the same size (or smaller)
       * than the menu framebuffer, no scaling is required */
      if ((vp.width <= fb_width) && (vp.height <= fb_height))
      {
         video_driver_set_texture_frame(rgui_frame_buf.data,
            false, fb_width, fb_height, 1.0f);
      }
      else
      {
         unsigned out_width;
         unsigned out_height;
         uint32_t x_ratio, y_ratio;
         unsigned x_src, y_src;
         unsigned x_dst, y_dst;
         
         /* Determine output size */
         if (settings->uints.menu_rgui_internal_upscale_level == RGUI_UPSCALE_AUTO)
         {
            out_width = ((vp.width / fb_width) + 1) * fb_width;
            out_height = ((vp.height / fb_height) + 1) * fb_height;
         }
         else
         {
            out_width = settings->uints.menu_rgui_internal_upscale_level * fb_width;
            out_height = settings->uints.menu_rgui_internal_upscale_level * fb_height;
         }
         
         /* Allocate upscaling buffer, if required */
         if ((rgui_upscale_buf.width != out_width) || (rgui_upscale_buf.height != out_height) || !rgui_upscale_buf.data)
         {
            rgui_upscale_buf.width = out_width;
            rgui_upscale_buf.height = out_height;
            
            if (rgui_upscale_buf.data)
            {
               free(rgui_upscale_buf.data);
               rgui_upscale_buf.data = NULL;
            }
            
            rgui_upscale_buf.data = (uint16_t*)calloc(out_width * out_height, sizeof(uint16_t));
            if (!rgui_upscale_buf.data)
            {
               /* Uh oh... This could mean we don't have enough
                * memory, so disable upscaling and draw the usual
                * framebuffer... */
               settings->uints.menu_rgui_internal_upscale_level = RGUI_UPSCALE_NONE;
               video_driver_set_texture_frame(rgui_frame_buf.data,
                  false, fb_width, fb_height, 1.0f);
               return;
            }
         }
         
         /* Perform nearest neighbour upscaling
          * NB: We're duplicating code here, but trying to handle
          * this with a polymorphic function is too much of a drag... */
         x_ratio = ((fb_width  << 16) / out_width);
         y_ratio = ((fb_height << 16) / out_height);

         for (y_dst = 0; y_dst < out_height; y_dst++)
         {
            y_src = (y_dst * y_ratio) >> 16;
            for (x_dst = 0; x_dst < out_width; x_dst++)
            {
               x_src = (x_dst * x_ratio) >> 16;
               rgui_upscale_buf.data[(y_dst * out_width) + x_dst] = rgui_frame_buf.data[(y_src * fb_width) + x_src];
            }
         }
         
         /* Draw upscaled texture */
         video_driver_set_texture_frame(rgui_upscale_buf.data,
            false, out_width, out_height, 1.0f);
      }
   }
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

static void rgui_set_thumbnail_system(void *userdata, char *s, size_t len)
{
   rgui_t *rgui = (rgui_t*)userdata;
   if (!rgui)
      return;
   menu_thumbnail_set_system(rgui->thumbnail_path_data, s);
}

static void rgui_scan_selected_entry_thumbnail(rgui_t *rgui)
{
   rgui->entry_has_thumbnail = false;
   
   if (rgui->show_thumbnail && rgui->is_playlist)
   {
      if (menu_thumbnail_set_content_playlist(rgui->thumbnail_path_data,
            playlist_get_cached(), menu_navigation_get_selection()))
      {
         if (menu_thumbnail_update_path(rgui->thumbnail_path_data, MENU_THUMBNAIL_RIGHT))
         {
            const char *thumbnail_path = NULL;
            
            if (menu_thumbnail_get_path(rgui->thumbnail_path_data,
                  MENU_THUMBNAIL_RIGHT, &thumbnail_path))
            {
               rgui->entry_has_thumbnail = request_thumbnail(rgui, thumbnail_path);
            }
         }
      }
   }
}

static void rgui_update_thumbnail_image(void *userdata)
{
   rgui_t *rgui = (rgui_t*)userdata;
   if (!rgui)
      return;

   rgui->show_thumbnail = !rgui->show_thumbnail;

   rgui_scan_selected_entry_thumbnail(rgui);
}

static void rgui_update_menu_sublabel(rgui_t *rgui)
{
   size_t selection = menu_navigation_get_selection();
   settings_t *settings = config_get_ptr();
   
   rgui->menu_sublabel[0] = '\0';
   
   if (settings->bools.menu_show_sublabels && selection < menu_entries_get_size())
   {
      menu_entry_t entry;
      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, (unsigned)selection, NULL, true);
      
      if (!string_is_empty(entry.sublabel))
      {
         static const char* const sublabel_spacer = RGUI_TICKER_SPACER;
         struct string_list *list = NULL;
         size_t line_index;
         bool prev_line_empty = true;

         /* Sanitise sublabel
          * > Replace newline characters with standard delimiter
          * > Remove whitespace surrounding each sublabel line */
         list = string_split(entry.sublabel, "\n");
         if (list)
         {
            for (line_index = 0; line_index < list->size; line_index++)
            {
               const char *line = string_trim_whitespace(list->elems[line_index].data);
               if (!string_is_empty(line))
               {
                  if (!prev_line_empty)
                     strlcat(rgui->menu_sublabel, sublabel_spacer, sizeof(rgui->menu_sublabel));
                  strlcat(rgui->menu_sublabel, line, sizeof(rgui->menu_sublabel));
                  prev_line_empty = false;
               }
            }
            
            string_list_free(list);
         }
      }
      
      menu_entry_free(&entry);
   }
}

static void rgui_navigation_set(void *data, bool scroll)
{
   size_t start, fb_pitch;
   unsigned fb_width, fb_height;
   bool do_set_start              = false;
   size_t end                     = menu_entries_get_size();
   size_t selection               = menu_navigation_get_selection();
   rgui_t *rgui = (rgui_t*)data;

   if (!rgui)
      return;

   rgui_scan_selected_entry_thumbnail(rgui);
   rgui_update_menu_sublabel(rgui);

   if (!scroll)
      return;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   if (selection < RGUI_TERM_HEIGHT(fb_height) /2)
   {
      start        = 0;
      do_set_start = true;
   }
   else if (selection >= (RGUI_TERM_HEIGHT(fb_height) /2)
         && selection < (end - RGUI_TERM_HEIGHT(fb_height) /2))
   {
      start        = selection - RGUI_TERM_HEIGHT(fb_height) /2;
      do_set_start = true;
   }
   else if (selection >= (end - RGUI_TERM_HEIGHT(fb_height) /2))
   {
      start        = end - RGUI_TERM_HEIGHT(fb_height);
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
   rgui_t *rgui         = (rgui_t*)data;
   settings_t *settings = config_get_ptr();
   
   if (!rgui || !settings)
      return;
   
   /* Check whether we are currently viewing a playlist */
   rgui->is_playlist = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST)) ||
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY)) ||
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST)) ||
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST));
   
   /* Set menu title */
   menu_entries_get_title(rgui->menu_title, sizeof(rgui->menu_title));
   
   rgui_navigation_set(data, true);
   
   /* If aspect ratio lock is enabled, must restore
    * content video settings when accessing the video
    * settings menu... */
   if (settings->uints.menu_rgui_aspect_ratio_lock != RGUI_ASPECT_RATIO_LOCK_NONE)
   {
      if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST)))
      {
         /* Make sure that any changes made while accessing
          * the video settings menu are preserved */
         rgui_video_settings_t current_video_settings = {0};
         rgui_get_video_config(&current_video_settings);
         if (rgui_is_video_config_equal(&current_video_settings, &rgui->menu_video_settings))
            rgui_set_video_config(&rgui->content_video_settings);
      }
   }
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

static void rgui_toggle(void *userdata, bool menu_on)
{
   rgui_t *rgui = (rgui_t*)userdata;
   settings_t *settings = config_get_ptr();
   
   /* TODO/FIXME - when we close RetroArch, this function
    * gets called and settings is NULL at this point. 
    * Maybe fundamentally change control flow so that on RetroArch
    * exit, this doesn't get called. */
   if (!rgui || !settings)
      return;
   
   if (settings->uints.menu_rgui_aspect_ratio_lock != RGUI_ASPECT_RATIO_LOCK_NONE)
   {
      if (menu_on)
      {
         /* Cache content video settings */
         rgui_get_video_config(&rgui->content_video_settings);
         
         /* Update menu viewport */
         rgui_update_menu_viewport(rgui);
         
         /* Apply menu video settings */
         rgui_set_video_config(&rgui->menu_video_settings);
      }
      else
      {
         /* Restore content video settings *if* user
          * has not changed video settings since menu was
          * last toggled on */
         rgui_video_settings_t current_video_settings = {0};
         rgui_get_video_config(&current_video_settings);
         
         if (rgui_is_video_config_equal(&current_video_settings, &rgui->menu_video_settings))
            rgui_set_video_config(&rgui->content_video_settings);
      }
   }
   
   /* Upscaling buffer is only required while menu is on. Save
    * memory by freeing it whenever we switch back to the current
    * content */
   if (!menu_on && rgui_upscale_buf.data)
   {
      free(rgui_upscale_buf.data);
      rgui_upscale_buf.data = NULL;
   }
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
   rgui_toggle,
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
   rgui_load_image,
   "rgui",
   rgui_environ,
   rgui_pointer_tap,
   NULL,                               /* update_thumbnail_path */
   rgui_update_thumbnail_image,
   rgui_set_thumbnail_system,
   NULL,                               /* set_thumbnail_content */
   NULL,                               /* osk_ptr_at_pos */
   NULL,                               /* update_savestate_thumbnail_path */
   NULL,                               /* update_savestate_thumbnail_image */
   NULL,                               /* pointer_down */
   NULL,                               /* pointer_up */
   NULL,                               /* get_load_content_animation_data */
};

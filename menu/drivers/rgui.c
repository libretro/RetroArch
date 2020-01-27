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
#include <formats/image.h>

#include <retro_inline.h>
#include <string/stdstring.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#if defined(HAVE_MENU_WIDGETS)
#include "../widgets/menu_widgets.h"
#endif

#include "../../playlist.h"
#include "../../frontend/frontend_driver.h"

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"

#include "../widgets/menu_osk.h"

#include "../../configuration.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"

#include <file/config_file.h>

/* Thumbnail additions */
#include "../menu_thumbnail_path.h"
#include "../../tasks/tasks_internal.h"
#include <gfx/scaler/scaler.h>
#include <features/features_cpu.h>

#if defined(GEKKO)
/* Required for the Wii build, since we have
 * to query the hardware for the actual display
 * aspect ratio... */
#include "../../wii/libogc/include/ogc/conf.h"
#endif

#define MAX_FB_WIDTH 426

#define RGUI_ENTRY_VALUE_MAXLEN 19

#define RGUI_TICKER_SPACER " | "

#define NUM_FONT_GLYPHS_REGULAR 128
#define NUM_FONT_GLYPHS_EXTENDED 256

#define NUM_PARTICLES 256

#ifndef PI
#define PI 3.14159265359f
#endif

#define BATTERY_WARN_THRESHOLD 20

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
   uint32_t shadow_color;
   uint32_t particle_color;
} rgui_theme_t;

static const rgui_theme_t rgui_theme_classic_red = {
   0xFFFF362B, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFFF362B, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC08C0000, /* border_dark_color */
   0xC0CC0E03, /* border_light_color */
   0xC0000000, /* shadow_color */
   0xC09E8686  /* particle_color */
};

static const rgui_theme_t rgui_theme_classic_orange = {
   0xFFF87217, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFF87217, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0962800, /* border_dark_color */
   0xC0E46C03, /* border_light_color */
   0xC0000000, /* shadow_color */
   0xC09E9286  /* particle_color */
};

static const rgui_theme_t rgui_theme_classic_yellow = {
   0xFFFFD801, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFFFD801, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0AC7800, /* border_dark_color */
   0xC0F3C60D, /* border_light_color */
   0xC0000000, /* shadow_color */
   0xC0999581  /* particle_color */
};

static const rgui_theme_t rgui_theme_classic_green = {
   0xFF64FF64, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFF64FF64, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0204020, /* border_dark_color */
   0xC0408040, /* border_light_color */
   0xC0000000, /* shadow_color */
   0xC0879E87  /* particle_color */
};

static const rgui_theme_t rgui_theme_classic_blue = {
   0xFF48BEFF, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFF48BEFF, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0005BA6, /* border_dark_color */
   0xC02E94E2, /* border_light_color */
   0xC0000000, /* shadow_color */
   0xC086949E  /* particle_color */
};

static const rgui_theme_t rgui_theme_classic_violet = {
   0xFFD86EFF, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFD86EFF, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC04C0A60, /* border_dark_color */
   0xC0842DCE, /* border_light_color */
   0xC0000000, /* shadow_color */
   0xC08E8299  /* particle_color */
};

static const rgui_theme_t rgui_theme_classic_grey = {
   0xFFB6C1C7, /* hover_color */
   0xFFFFFFFF, /* normal_color */
   0xFFB6C1C7, /* title_color */
   0xC0202020, /* bg_dark_color */
   0xC0404040, /* bg_light_color */
   0xC0505050, /* border_dark_color */
   0xC0798A99, /* border_light_color */
   0xC0000000, /* shadow_color */
   0xC078828A  /* particle_color */
};

static const rgui_theme_t rgui_theme_legacy_red = {
   0xFFFFBDBD, /* hover_color */
   0xFFFAF6D5, /* normal_color */
   0xFFFF948A, /* title_color */
   0xC09E4137, /* bg_dark_color */
   0xC0B34B41, /* bg_light_color */
   0xC0BF5E58, /* border_dark_color */
   0xC0F27A6F, /* border_light_color */
   0xC01F0C0A, /* shadow_color */
   0xC0F75431  /* particle_color */
};

static const rgui_theme_t rgui_theme_dark_purple = {
   0xFFF2B5D6, /* hover_color */
   0xFFE8D0CC, /* normal_color */
   0xFFC79FC2, /* title_color */
   0xC0562D56, /* bg_dark_color */
   0xC0663A66, /* bg_light_color */
   0xC0885783, /* border_dark_color */
   0xC0A675A1, /* border_light_color */
   0xC0140A14, /* shadow_color */
   0xC09786A0  /* particle_color */
};

static const rgui_theme_t rgui_theme_midnight_blue = {
   0xFFB2D3ED, /* hover_color */
   0xFFD3DCDE, /* normal_color */
   0xFF86A1BA, /* title_color */
   0xC024374A, /* bg_dark_color */
   0xC03C4D5E, /* bg_light_color */
   0xC046586A, /* border_dark_color */
   0xC06D7F91, /* border_light_color */
   0xC00A0F14, /* shadow_color */
   0xC084849E  /* particle_color */
};

static const rgui_theme_t rgui_theme_golden = {
   0xFFFFE666, /* hover_color */
   0xFFFFFFDC, /* normal_color */
   0xFFFFCC00, /* title_color */
   0xC0B88D0B, /* bg_dark_color */
   0xC0BF962B, /* bg_light_color */
   0xC0E1AD21, /* border_dark_color */
   0xC0FCC717, /* border_light_color */
   0xC0382B03, /* shadow_color */
   0xC0F7D15E  /* particle_color */
};

static const rgui_theme_t rgui_theme_electric_blue = {
   0xFF7DF9FF, /* hover_color */
   0xFFDBE9F4, /* normal_color */
   0xFF86CDE0, /* title_color */
   0xC02E69C6, /* bg_dark_color */
   0xC0007FFF, /* bg_light_color */
   0xC034A5D8, /* border_dark_color */
   0xC070C9FF, /* border_light_color */
   0xC012294D, /* shadow_color */
   0xC080C7E6  /* particle_color */
};

static const rgui_theme_t rgui_theme_apple_green = {
   0xFFB0FC64, /* hover_color */
   0xFFD8F2CB, /* normal_color */
   0xFFA6D652, /* title_color */
   0xC04F7942, /* bg_dark_color */
   0xC0688539, /* bg_light_color */
   0xC0608E3A, /* border_dark_color */
   0xC09AB973, /* border_light_color */
   0xC01F2E19, /* shadow_color */
   0xC0A3C44E  /* particle_color */
};

static const rgui_theme_t rgui_theme_volcanic_red = {
   0xFFFFCC99, /* hover_color */
   0xFFD3D3D3, /* normal_color */
   0xFFDDADAF, /* title_color */
   0xC0922724, /* bg_dark_color */
   0xC0BD0F1E, /* bg_light_color */
   0xC0CE2029, /* border_dark_color */
   0xC0FF0000, /* border_light_color */
   0xC0330D0D, /* shadow_color */
   0xC0E67D45  /* particle_color */
};

static const rgui_theme_t rgui_theme_lagoon = {
   0xFFBCE1EB, /* hover_color */
   0xFFCFCFC4, /* normal_color */
   0xFF86C7C7, /* title_color */
   0xC0495C6B, /* bg_dark_color */
   0xC0526778, /* bg_light_color */
   0xC058848F, /* border_dark_color */
   0xC060909C, /* border_light_color */
   0xC01C2329, /* shadow_color */
   0xC09FB1C7  /* particle_color */
};

static const rgui_theme_t rgui_theme_brogrammer = {
   0xFF3498DB, /* hover_color */
   0xFFECF0F1, /* normal_color */
   0xFF2ECC71, /* title_color */
   0xC0242424, /* bg_dark_color */
   0xC0242424, /* bg_light_color */
   0xC0E74C3C, /* border_dark_color */
   0xC0E74C3C, /* border_light_color */
   0xC0000000, /* shadow_color */
   0xC0606060  /* particle_color */
};

static const rgui_theme_t rgui_theme_dracula = {
   0xFFBD93F9, /* hover_color */
   0xFFF8F8F2, /* normal_color */
   0xFFFF79C6, /* title_color */
   0xC02F3240, /* bg_dark_color */
   0xC02F3240, /* bg_light_color */
   0xC06272A4, /* border_dark_color */
   0xC06272A4, /* border_light_color */
   0xC00F0F0F, /* shadow_color */
   0xC06272A4  /* particle_color */
};

static const rgui_theme_t rgui_theme_fairyfloss = {
   0xFFFFF352, /* hover_color */
   0xFFF8F8F2, /* normal_color */
   0xFFFFB8D1, /* title_color */
   0xC0675F87, /* bg_dark_color */
   0xC0675F87, /* bg_light_color */
   0xC08077A8, /* border_dark_color */
   0xC08077A8, /* border_light_color */
   0xC0262433, /* shadow_color */
   0xC0C5A3FF  /* particle_color */
};

static const rgui_theme_t rgui_theme_flatui = {
   0xFF0A74B9, /* hover_color */
   0xFF2C3E50, /* normal_color */
   0xFF8E44AD, /* title_color */
   0xE0ECF0F1, /* bg_dark_color */
   0xE0ECF0F1, /* bg_light_color */
   0xE095A5A6, /* border_dark_color */
   0xE095A5A6, /* border_light_color */
   0xE0C3DBDE, /* shadow_color */
   0xE0B3DFFF  /* particle_color */
};

static const rgui_theme_t rgui_theme_gruvbox_dark = {
   0xFFFE8019, /* hover_color */
   0xFFEBDBB2, /* normal_color */
   0xFF83A598, /* title_color */
   0xC03D3D3D, /* bg_dark_color */
   0xC03D3D3D, /* bg_light_color */
   0xC099897A, /* border_dark_color */
   0xC099897A, /* border_light_color */
   0xC0000000, /* shadow_color */
   0xC098971A  /* particle_color */
};

static const rgui_theme_t rgui_theme_gruvbox_light = {
   0xFFAF3A03, /* hover_color */
   0xFF3C3836, /* normal_color */
   0xFF076678, /* title_color */
   0xE0FBEBC7, /* bg_dark_color */
   0xE0FBEBC7, /* bg_light_color */
   0xE0928374, /* border_dark_color */
   0xE0928374, /* border_light_color */
   0xE0D5C4A1, /* shadow_color */
   0xE0D5C4A1  /* particle_color */
};

static const rgui_theme_t rgui_theme_hacking_the_kernel = {
   0xFF83FF83, /* hover_color */
   0xFF00E000, /* normal_color */
   0xFF00FF00, /* title_color */
   0xC0000000, /* bg_dark_color */
   0xC0000000, /* bg_light_color */
   0xC0036303, /* border_dark_color */
   0xC0036303, /* border_light_color */
   0xC0154D2B, /* shadow_color */
   0xC0008C00  /* particle_color */
};

static const rgui_theme_t rgui_theme_nord = {
   0xFF8FBCBB, /* hover_color */
   0xFFD8DEE9, /* normal_color */
   0xFF81A1C1, /* title_color */
   0xC0363C4F, /* bg_dark_color */
   0xC0363C4F, /* bg_light_color */
   0xC04E596E, /* border_dark_color */
   0xC04E596E, /* border_light_color */
   0xC0040505, /* shadow_color */
   0xC05E81AC  /* particle_color */
};

static const rgui_theme_t rgui_theme_nova = {
   0XFF7FC1CA, /* hover_color */
   0XFFC5D4DD, /* normal_color */
   0XFF9A93E1, /* title_color */
   0xC0485B66, /* bg_dark_color */
   0xC0485B66, /* bg_light_color */
   0xC0627985, /* border_dark_color */
   0xC0627985, /* border_light_color */
   0xC01E272C, /* shadow_color */
   0xC0889BA7  /* particle_color */
};

static const rgui_theme_t rgui_theme_one_dark = {
   0XFF98C379, /* hover_color */
   0XFFBBBBBB, /* normal_color */
   0XFFD19A66, /* title_color */
   0xC02D323B, /* bg_dark_color */
   0xC02D323B, /* bg_light_color */
   0xC0495162, /* border_dark_color */
   0xC0495162, /* border_light_color */
   0xC007080A, /* shadow_color */
   0xC05F697A  /* particle_color */
};

static const rgui_theme_t rgui_theme_palenight = {
   0xFFC792EA, /* hover_color */
   0xFFBFC7D5, /* normal_color */
   0xFF82AAFF, /* title_color */
   0xC02F3347, /* bg_dark_color */
   0xC02F3347, /* bg_light_color */
   0xC0697098, /* border_dark_color */
   0xC0697098, /* border_light_color */
   0xC00D0E14, /* shadow_color */
   0xC0697098  /* particle_color */
};

static const rgui_theme_t rgui_theme_solarized_dark = {
   0xFFB58900, /* hover_color */
   0xFF839496, /* normal_color */
   0xFF268BD2, /* title_color */
   0xC0003542, /* bg_dark_color */
   0xC0003542, /* bg_light_color */
   0xC093A1A1, /* border_dark_color */
   0xC093A1A1, /* border_light_color */
   0xC000141A, /* shadow_color */
   0xC0586E75  /* particle_color */
};

static const rgui_theme_t rgui_theme_solarized_light = {
   0xFFB58900, /* hover_color */
   0xFF657B83, /* normal_color */
   0xFF268BD2, /* title_color */
   0xE0FDEDDF, /* bg_dark_color */
   0xE0FDEDDF, /* bg_light_color */
   0xE093A1A1, /* border_dark_color */
   0xE093A1A1, /* border_light_color */
   0xE0E0DBC9, /* shadow_color */
   0xE0FFC5AD  /* particle_color */
};

static const rgui_theme_t rgui_theme_tango_dark = {
   0xFF8AE234, /* hover_color */
   0xFFEEEEEC, /* normal_color */
   0xFF729FCF, /* title_color */
   0xC0384042, /* bg_dark_color */
   0xC0384042, /* bg_light_color */
   0xC06A767A, /* border_dark_color */
   0xC06A767A, /* border_light_color */
   0xC01A1A1A, /* shadow_color */
   0xC0C4A000  /* particle_color */
};

static const rgui_theme_t rgui_theme_tango_light = {
   0xFF4E9A06, /* hover_color */
   0xFF2E3436, /* normal_color */
   0xFF204A87, /* title_color */
   0xE0EEEEEC, /* bg_dark_color */
   0xE0EEEEEC, /* bg_light_color */
   0xE0C7C7C7, /* border_dark_color */
   0xE0C7C7C7, /* border_light_color */
   0xE0D3D7CF, /* shadow_color */
   0xE0FFCA78  /* particle_color */
};

static const rgui_theme_t rgui_theme_zenburn = {
   0xFFF0DFAF, /* hover_color */
   0xFFDCDCCC, /* normal_color */
   0xFF8FB28F, /* title_color */
   0xC04F4F4F, /* bg_dark_color */
   0xC04F4F4F, /* bg_light_color */
   0xC0636363, /* border_dark_color */
   0xC0636363, /* border_light_color */
   0xC01F1F1F, /* shadow_color */
   0xC0AC7373  /* particle_color */
};

static const rgui_theme_t rgui_theme_anti_zenburn = {
   0xFF336C6C, /* hover_color */
   0xFF232333, /* normal_color */
   0xFF205070, /* title_color */
   0xE0C0C0C0, /* bg_dark_color */
   0xE0C0C0C0, /* bg_light_color */
   0xE0A0A0A0, /* border_dark_color */
   0xE0A0A0A0, /* border_light_color */
   0xE0B0B0B0, /* shadow_color */
   0xE0B090B0  /* particle_color */
};

#if 0
static const rgui_theme_t rgui_theme_flux = {
   0xFF6FCB9F, /* hover_color */
   0xFF666547, /* normal_color */
   0xFFFB2E01, /* title_color */
   0xE0FFFEB3, /* bg_dark_color */
   0xE0FFFEB3, /* bg_light_color */
   0xE0FFE28A, /* border_dark_color */
   0xE0FFE28A, /* border_light_color */
   0xE0FFE28A, /* shadow_color */
   0xE0FB2E01  /* particle_color */
};
#endif

typedef struct
{
   uint16_t hover_color;
   uint16_t normal_color;
   uint16_t title_color;
   uint16_t bg_dark_color;
   uint16_t bg_light_color;
   uint16_t border_dark_color;
   uint16_t border_light_color;
   uint16_t shadow_color;
   uint16_t particle_color;
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
   unsigned window_width;
   unsigned window_height;
   bool ignore_resize_events;
   bool bg_thickness;
   bool border_thickness;
   bool border_enable;
   bool shadow_enable;
   unsigned particle_effect;
   bool extended_ascii_enable;
   int16_t scroll_y;
   char msgbox[1024];
   unsigned color_theme;
   rgui_colors_t colors;
   bool is_playlist;
   bool entry_has_thumbnail;
   bool entry_has_left_thumbnail;
   bool show_fs_thumbnail;
   menu_thumbnail_path_data_t *thumbnail_path_data;
   uint32_t thumbnail_queue_size;
   uint32_t left_thumbnail_queue_size;
   bool thumbnail_load_pending;
   retro_time_t thumbnail_load_trigger_time;
   bool show_wallpaper;
   char theme_preset_path[PATH_MAX_LENGTH]; /* Must be a fixed length array... */
   char menu_title[255]; /* Must be a fixed length array... */
   char menu_sublabel[MENU_SUBLABEL_MAX_LENGTH]; /* Must be a fixed length array... */
   unsigned menu_aspect_ratio;
   unsigned menu_aspect_ratio_lock;
   bool aspect_update_pending;
   rgui_video_settings_t menu_video_settings;
   rgui_video_settings_t content_video_settings;
#if defined(HAVE_MENU_WIDGETS)
   bool widgets_supported;
#endif
   struct scaler_ctx image_scaler;
   menu_input_pointer_t pointer;
} rgui_t;

static unsigned mini_thumbnail_max_width = 0;
static unsigned mini_thumbnail_max_height = 0;

static bool font_lut[NUM_FONT_GLYPHS_EXTENDED][FONT_WIDTH * FONT_HEIGHT];

/* A 'particle' is just 4 float variables that can
 * be used for any purpose - e.g.:
 * > a = x pos
 * > b = y pos
 * > c = x velocity
 * or:
 * > a = radius
 * > b = theta
 * etc. */
typedef struct
{
   float a;
   float b;
   float c;
   float d;
} rgui_particle_t;

static rgui_particle_t particles[NUM_PARTICLES] = {{ 0.0f }};

/* Particle effect animations update at a base rate
 * of 60Hz (-> 16.666 ms update period) */
static const float particle_effect_period = (1.0f / 60.0f) * 1000.0f;

/* ==============================
 * Custom Symbols (glyphs) START
 * ============================== */

enum rgui_symbol_type
{
   RGUI_SYMBOL_BACKSPACE = 0,
   RGUI_SYMBOL_ENTER,
   RGUI_SYMBOL_SHIFT_UP,
   RGUI_SYMBOL_SHIFT_DOWN,
   RGUI_SYMBOL_NEXT,
   RGUI_SYMBOL_TEXT_CURSOR,
   RGUI_SYMBOL_CHARGING,
   RGUI_SYMBOL_BATTERY_100,
   RGUI_SYMBOL_BATTERY_80,
   RGUI_SYMBOL_BATTERY_60,
   RGUI_SYMBOL_BATTERY_40,
   RGUI_SYMBOL_BATTERY_20,
   RGUI_SYMBOL_CHECKMARK
};

/* All custom symbols must have dimensions
 * of exactly FONT_WIDTH * FONT_HEIGHT */
static const uint8_t rgui_symbol_data_backspace[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0,
      0, 0, 1, 0, 0,
      0, 1, 0, 0, 0,
      1, 1, 1, 1, 1,
      0, 1, 0, 0, 0,
      0, 0, 1, 0, 0,
      0, 0, 0, 0, 0, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_enter[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 1,
      0, 0, 0, 0, 1,
      0, 0, 0, 0, 1,
      0, 0, 1, 0, 1,
      0, 1, 0, 0, 1,
      1, 1, 1, 1, 1,
      0, 1, 0, 0, 0, /* Baseline */
      0, 0, 1, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_shift_up[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 0, 1, 0, 0,
      0, 1, 1, 1, 0,
      1, 1, 0, 1, 1,
      0, 1, 0, 1, 0,
      0, 1, 0, 1, 0,
      0, 1, 0, 1, 0,
      0, 1, 1, 1, 0, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_shift_down[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 1, 1, 1, 0,
      0, 1, 0, 1, 0,
      0, 1, 0, 1, 0,
      0, 1, 0, 1, 0,
      1, 1, 0, 1, 1,
      0, 1, 1, 1, 0,
      0, 0, 1, 0, 0, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_next[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0,
      0, 1, 1, 1, 0,
      1, 0, 1, 0, 1,
      1, 1, 1, 1, 1,
      1, 0, 1, 0, 1,
      0, 1, 1, 1, 0,
      0, 0, 0, 0, 0, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_text_cursor[FONT_WIDTH * FONT_HEIGHT] = {
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, /* Baseline */
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1};

static const uint8_t rgui_symbol_data_charging[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 1, 0, 1, 0,
      0, 1, 0, 1, 0,
      1, 1, 1, 1, 1,
      1, 1, 1, 1, 1,
      0, 1, 1, 1, 0,
      0, 0, 1, 0, 0,
      0, 0, 1, 0, 0, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_battery_100[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 0, 1, 1, 0,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_battery_80[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 0, 1, 1, 0,
      0, 1, 1, 1, 1,
      0, 1, 0, 0, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_battery_60[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 0, 1, 1, 0,
      0, 1, 1, 1, 1,
      0, 1, 0, 0, 1,
      0, 1, 0, 0, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_battery_40[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 0, 1, 1, 0,
      0, 1, 1, 1, 1,
      0, 1, 0, 0, 1,
      0, 1, 0, 0, 1,
      0, 1, 0, 0, 1,
      0, 1, 1, 1, 1,
      0, 1, 1, 1, 1, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

static const uint8_t rgui_symbol_data_battery_20[FONT_WIDTH * FONT_HEIGHT] = {
      0, 0, 0, 0, 0,
      0, 0, 1, 1, 0,
      0, 1, 1, 1, 1,
      0, 1, 0, 0, 1,
      0, 1, 0, 0, 1,
      0, 1, 0, 0, 1,
      0, 1, 0, 0, 1,
      0, 1, 1, 1, 1, /* Baseline */
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0};

/* Note: This is not actually a 'checkmark' - we don't
 * have enough pixels to draw one effectively. The 'icon'
 * is merely named according to its function... */
static const uint8_t rgui_symbol_data_checkmark[FONT_WIDTH * FONT_HEIGHT] = {
      0, 1, 1, 0, 0,
      0, 1, 1, 0, 0,
      0, 1, 1, 0, 0,
      0, 1, 1, 0, 0,
      0, 1, 1, 0, 0,
      0, 1, 1, 0, 0,
      0, 1, 1, 0, 0,
      0, 1, 1, 0, 0, /* Baseline */
      0, 1, 1, 0, 0,
      0, 0, 0, 0, 0};

/* ==============================
 * Custom Symbols (glyphs) END
 * ============================== */

typedef struct
{
   unsigned max_width;
   unsigned max_height;
   unsigned width;
   unsigned height;
   bool is_valid;
   char path[PATH_MAX_LENGTH];
   uint16_t *data;
} thumbnail_t;

static thumbnail_t fs_thumbnail = {
   0,
   0,
   0,
   0,
   false,
   {0},
   NULL
};

static thumbnail_t mini_thumbnail = {
   0,
   0,
   0,
   0,
   false,
   {0},
   NULL
};

static thumbnail_t mini_left_thumbnail = {
   0,
   0,
   0,
   0,
   false,
   {0},
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

static frame_buf_t rgui_background_buf = {
   0,
   0,
   NULL
};

static frame_buf_t rgui_upscale_buf = {
   0,
   0,
   NULL
};

/* ==============================
 * pixel format conversion START
 * ============================== */

/* PS2 */
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

/* GEKKO */
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

/* PSP */
static uint16_t argb32_to_abgr4444(uint32_t col)
{
   unsigned a = ((col >> 24) & 0xff) >> 4;
   unsigned r = ((col >> 16) & 0xff) >> 4;
   unsigned g = ((col >> 8)  & 0xff) >> 4;
   unsigned b = ((col & 0xff)      ) >> 4;
   return (a << 12) | (b << 8) | (g << 4) | r;
}

/* D3D10/11/12 */
static uint16_t argb32_to_bgra4444(uint32_t col)
{
   unsigned a = ((col >> 24) & 0xff) >> 4;
   unsigned r = ((col >> 16) & 0xff) >> 4;
   unsigned g = ((col >> 8)  & 0xff) >> 4;
   unsigned b = ((col & 0xff)      ) >> 4;
   return (b << 12) | (g << 8) | (r << 4) | a;
}

/* All other platforms */
static uint16_t argb32_to_rgba4444(uint32_t col)
{
   unsigned a = ((col >> 24) & 0xff) >> 4;
   unsigned r = ((col >> 16) & 0xff) >> 4;
   unsigned g = ((col >> 8)  & 0xff) >> 4;
   unsigned b = ((col & 0xff)      ) >> 4;
   return (r << 12) | (g << 8) | (b << 4) | a;
}

static uint16_t (*argb32_to_pixel_platform_format)(uint32_t col) = argb32_to_rgba4444;

static void rgui_set_pixel_format_function(void)
{
   const char *driver_ident = video_driver_get_ident();
   
   /* Default fallback... */
   if (string_is_empty(driver_ident))
   {
      argb32_to_pixel_platform_format = argb32_to_rgba4444;
      return;
   }
   
   if (     string_is_equal(driver_ident, "ps2"))     /* PS2 */
      argb32_to_pixel_platform_format = argb32_to_abgr1555;
   else if (string_is_equal(driver_ident, "gx"))      /* GEKKO */
      argb32_to_pixel_platform_format = argb32_to_rgb5a3;
   else if (string_is_equal(driver_ident, "psp1"))    /* PSP */
      argb32_to_pixel_platform_format = argb32_to_abgr4444;
   else if (string_is_equal(driver_ident, "d3d10") || /* D3D10/11/12 */
            string_is_equal(driver_ident, "d3d11") ||
            string_is_equal(driver_ident, "d3d12"))
      argb32_to_pixel_platform_format = argb32_to_bgra4444;
   else
      argb32_to_pixel_platform_format = argb32_to_rgba4444;
}

/* ==============================
 * pixel format conversion END
 * ============================== */

static void rgui_fill_rect(
      uint16_t *data,
      unsigned fb_width, unsigned fb_height,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t dark_color, uint16_t light_color,
      bool thickness)
{
   unsigned x_index, y_index;
   unsigned x_start = x <= fb_width  ? x : fb_width;
   unsigned y_start = y <= fb_height ? y : fb_height;
   unsigned x_end   = x + width;
   unsigned y_end   = y + height;
   size_t x_size;
   uint16_t scanline_even[MAX_FB_WIDTH]; /* Initial values don't matter here */
   uint16_t scanline_odd[MAX_FB_WIDTH];

   /* Note: unlike rgui_color_rect() and rgui_draw_particle(),
    * this function is frequently used to fill large areas.
    * We therefore gain significant performance benefits
    * from using memcpy() tricks... */

   x_end  = x_end <= fb_width  ? x_end : fb_width;
   y_end  = y_end <= fb_height ? y_end : fb_height;
   x_size = (x_end - x_start) * sizeof(uint16_t);

   /* Sanity check */
   if (x_size == 0)
      return;

   /* If dark_color and light_color are the same,
    * perform a solid fill */
   if (dark_color == light_color)
   {
      uint16_t *src = scanline_even + x_start;
      uint16_t *dst = data + x_start;

      /* Populate source array */
      for (x_index = x_start; x_index < x_end; x_index++)
         *(scanline_even + x_index) = dark_color;

      /* Fill destination array */
      for (y_index = y_start; y_index < y_end; y_index++)
         memcpy(dst + (y_index * fb_width), src, x_size);
   }
   else if (thickness)
   {
      uint16_t *src_a      = NULL;
      uint16_t *src_b      = NULL;
      uint16_t *src_c      = NULL;
      uint16_t *src_d      = NULL;
      uint16_t *dst        = data + x_start;

      /* Determine in which order the source arrays
       * should be copied */
      switch (y_start & 0x3)
      {
         case 0x1:
            src_a = scanline_even + x_start;
            src_b = scanline_odd  + x_start;
            src_c = src_b;
            src_d = src_a;
            break;
         case 0x2:
            src_a = scanline_odd  + x_start;
            src_b = src_a;
            src_c = scanline_even + x_start;
            src_d = src_c;
            break;
         case 0x3:
            src_a = scanline_odd  + x_start;
            src_b = scanline_even + x_start;
            src_c = src_b;
            src_d = src_a;
            break;
         case 0x0:
         default:
            src_a = scanline_even + x_start;
            src_b = src_a;
            src_c = scanline_odd  + x_start;
            src_d = src_c;
            break;
      }

      /* Populate source arrays */
      for (x_index = x_start; x_index < x_end; x_index++)
      {
         bool x_is_even = (((x_index >> 1) & 1) == 0);
         *(scanline_even + x_index) = x_is_even ? dark_color  : light_color;
         *(scanline_odd  + x_index) = x_is_even ? light_color : dark_color;
      }

      /* Fill destination array */
      for (y_index = y_start    ; y_index < y_end; y_index += 4)
         memcpy(dst + (y_index * fb_width), src_a, x_size);

      for (y_index = y_start + 1; y_index < y_end; y_index += 4)
         memcpy(dst + (y_index * fb_width), src_b, x_size);

      for (y_index = y_start + 2; y_index < y_end; y_index += 4)
         memcpy(dst + (y_index * fb_width), src_c, x_size);

      for (y_index = y_start + 3; y_index < y_end; y_index += 4)
         memcpy(dst + (y_index * fb_width), src_d, x_size);
   }
   else
   {
      uint16_t *src_a      = NULL;
      uint16_t *src_b      = NULL;
      uint16_t *dst        = data + x_start;

      /* Determine in which order the source arrays
       * should be copied */
      if ((y_start & 1) == 0)
      {
         src_a = scanline_even + x_start;
         src_b = scanline_odd  + x_start;
      }
      else
      {
         src_a = scanline_odd  + x_start;
         src_b = scanline_even + x_start;
      }

      /* Populate source arrays */
      for (x_index = x_start; x_index < x_end; x_index++)
      {
         bool x_is_even = ((x_index & 1) == 0);
         *(scanline_even + x_index) = x_is_even ? dark_color  : light_color;
         *(scanline_odd  + x_index) = x_is_even ? light_color : dark_color;
      }

      /* Fill destination array */
      for (y_index = y_start    ; y_index < y_end; y_index += 2)
         memcpy(dst + (y_index * fb_width), src_a, x_size);

      for (y_index = y_start + 1; y_index < y_end; y_index += 2)
         memcpy(dst + (y_index * fb_width), src_b, x_size);
   }
}

static void rgui_color_rect(
      uint16_t *data,
      unsigned fb_width, unsigned fb_height,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t color)
{
   unsigned x_index, y_index;
   unsigned x_start = x <= fb_width  ? x : fb_width;
   unsigned y_start = y <= fb_height ? y : fb_height;
   unsigned x_end   = x + width;
   unsigned y_end   = y + height;

   x_end = x_end <= fb_width  ? x_end : fb_width;
   y_end = y_end <= fb_height ? y_end : fb_height;

   for (y_index = y_start; y_index < y_end; y_index++)
   {
      uint16_t *data_ptr = data + (y_index * fb_width);
      for (x_index = x_start; x_index < x_end; x_index++)
         *(data_ptr + x_index) = color;
   }
}

static void rgui_render_border(rgui_t *rgui, uint16_t *data,
      unsigned fb_width, unsigned fb_height)
{
   uint16_t dark_color;
   uint16_t light_color;
   bool thickness;
   
   /* Sanity check */
   if (!rgui || !data)
      return;
   
   dark_color   = rgui->colors.border_dark_color;
   light_color  = rgui->colors.border_light_color;
   thickness    = rgui->border_thickness;
   
   /* Draw border */
   rgui_fill_rect(data, fb_width, fb_height,
         5, 5, fb_width - 10, 5,
         dark_color, light_color, thickness);
   rgui_fill_rect(data, fb_width, fb_height,
         5, fb_height - 10, fb_width - 10, 5,
         dark_color, light_color, thickness);
   rgui_fill_rect(data, fb_width, fb_height,
         5, 5, 5, fb_height - 10,
         dark_color, light_color, thickness);
   rgui_fill_rect(data, fb_width, fb_height,
         fb_width - 10, 5, 5, fb_height - 10,
         dark_color, light_color, thickness);
   
   /* Draw drop shadow, if required */
   if (rgui->shadow_enable)
   {
      uint16_t shadow_color = rgui->colors.shadow_color;
      
      rgui_color_rect(data, fb_width, fb_height,
            10, 10, 1, fb_height - 20, shadow_color);
      rgui_color_rect(data, fb_width, fb_height,
            10, 10, fb_width - 20, 1, shadow_color);
      rgui_color_rect(data, fb_width, fb_height,
            fb_width - 5, 6, 1, fb_height - 10, shadow_color);
      rgui_color_rect(data, fb_width, fb_height,
            6, fb_height - 5, fb_width - 10, 1, shadow_color);
   }
}

/* Returns true if particle is on screen */
static bool INLINE rgui_draw_particle(
      uint16_t *data,
      unsigned fb_width, unsigned fb_height,
      int x, int y,
      unsigned width, unsigned height,
      uint16_t color)
{
   unsigned x_index, y_index;
   
   /* This great convoluted mess just saves us
    * having to perform comparisons on every
    * iteration of the for loops... */
   int x_start = x > 0 ? x : 0;
   int y_start = y > 0 ? y : 0;
   int x_end = x + width;
   int y_end = y + height;
   
   x_start = x_start <= (int)fb_width  ? x_start : fb_width;
   y_start = y_start <= (int)fb_height ? y_start : fb_height;
   
   x_end = x_end >  0        ? x_end : 0;
   x_end = x_end <= (int)fb_width ? x_end : fb_width;
   
   y_end = y_end >  0         ? y_end : 0;
   y_end = y_end <= (int)fb_height ? y_end : fb_height;
   
   for (y_index = (unsigned)y_start; y_index < (unsigned)y_end; y_index++)
   {
      uint16_t *data_ptr = data + (y_index * fb_width);
      for (x_index = (unsigned)x_start; x_index < (unsigned)x_end; x_index++)
         *(data_ptr + x_index) = color;
   }
   
   return (x_end > x_start) && (y_end > y_start);
}

static void rgui_init_particle_effect(rgui_t *rgui)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   size_t i;
   
   /* Sanity check */
   if (!rgui)
      return;
   
   menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);
   
   switch (rgui->particle_effect)
   {
      case RGUI_PARTICLE_EFFECT_SNOW:
      case RGUI_PARTICLE_EFFECT_SNOW_ALT:
         {
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               particle->a = (float)(rand() % fb_width);
               particle->b = (float)(rand() % fb_height);
               particle->c = (float)(rand() % 64 - 16) * 0.1f;
               particle->d = (float)(rand() % 64 - 48) * 0.1f;
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_RAIN:
         {
            uint8_t weights[] = { /* 60 entries */
               2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
               3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
               4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
               5, 5, 5, 5, 5, 5, 5, 5,
               6, 6, 6, 6, 6, 6,
               7, 7, 7, 7,
               8, 8, 8,
               9, 9,
               10};
            unsigned num_drops = (unsigned)(0.85f * ((float)fb_width / (float)MAX_FB_WIDTH) * (float)NUM_PARTICLES);
            
            num_drops = num_drops < NUM_PARTICLES ? num_drops : NUM_PARTICLES;
            
            for (i = 0; i < num_drops; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* x pos */
               particle->a = (float)(rand() % (fb_width / 3)) * 3.0f;
               /* y pos */
               particle->b = (float)(rand() % fb_height);
               /* drop length */
               particle->c = (float)weights[(unsigned)(rand() % 60)];
               /* drop speed (larger drops fall faster) */
               particle->d = (particle->c / 12.0f) * (0.5f + ((float)(rand() % 150) / 200.0f));
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_VORTEX:
         {
            float max_radius         = (float)sqrt((double)((fb_width * fb_width) + (fb_height * fb_height))) / 2.0f;
            float one_degree_radians = PI / 360.0f;
            
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* radius */
               particle->a = 1.0f + (((float)rand() / (float)RAND_MAX) * max_radius);
               /* theta */
               particle->b = ((float)rand() / (float)RAND_MAX) * 2.0f * PI;
               /* radial speed */
               particle->c = (float)((rand() % 100) + 1) * 0.001f;
               /* rotational speed */
               particle->d = (((float)((rand() % 50) + 1) / 200.0f) + 0.1f) * one_degree_radians;
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_STARFIELD:
         {
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* x pos */
               particle->a = (float)(rand() % fb_width);
               /* y pos */
               particle->b = (float)(rand() % fb_height);
               /* depth */
               particle->c = (float)fb_width;
               /* speed */
               particle->d = 1.0f + ((float)(rand() % 20) * 0.01f);
            }
         }
         break;
      default:
         /* Do nothing... */
         break;
   }
}

static void rgui_render_particle_effect(rgui_t *rgui)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   size_t i;
   /* Give speed factor a long, awkward name to minimise
    * risk of clashing with specific particle effect
    * implementation variables... */
   float global_speed_factor;
   settings_t *settings = config_get_ptr();
   
   /* Sanity check */
   if (!rgui || !rgui_frame_buf.data || !settings)
      return;
   
   menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);
   
   /* Adjust global animation speed */
   /* > Apply user configured speed multiplier */
   global_speed_factor =
         (settings->floats.menu_rgui_particle_effect_speed > 0.0001f) ?
               settings->floats.menu_rgui_particle_effect_speed : 1.0f;
   /* > Account for non-standard frame times
    *   (high/low refresh rates, or frame drops) */
   global_speed_factor *= menu_animation_get_delta_time() / particle_effect_period;
   
   /* Note: It would be more elegant to have 'update' and 'draw'
    * as separate functions, since 'update' is the part that
    * varies with particle effect whereas 'draw' is always
    * pretty much the same. However, this has the following
    * disadvantages:
    * - It means we have to loop through all particles twice,
    *   and given that we're already using a heap of CPU cycles
    *   to draw these effects any further performance overheads
    *   are to be avoided
    * - It locks us into a particular draw style. e.g. What if
    *   an effect calls for round particles, instead of square
    *   ones? This would make a mess of any 'standardised'
    *   drawing
    * So we go with the simple option of having the entire
    * update/draw sequence here. This results in some code
    * repetition, but it has better performance and allows for
    * complete flexibility */
   
   switch (rgui->particle_effect)
   {
      case RGUI_PARTICLE_EFFECT_SNOW:
      case RGUI_PARTICLE_EFFECT_SNOW_ALT:
         {
            unsigned particle_size;
            bool on_screen;
            
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* Update particle 'speed' */
               particle->c = particle->c + (float)(rand() % 16 - 9) * 0.01f;
               particle->d = particle->d + (float)(rand() % 16 - 7) * 0.01f;
               
               particle->c = (particle->c < -0.4f) ? -0.4f : particle->c;
               particle->c = (particle->c >  0.1f) ?  0.1f : particle->c;
               
               particle->d = (particle->d < -0.1f) ? -0.1f : particle->d;
               particle->d = (particle->d >  0.4f) ?  0.4f : particle->d;
               
               /* Update particle location */
               particle->a = fmod(particle->a + (global_speed_factor * particle->c), fb_width);
               particle->b = fmod(particle->b + (global_speed_factor * particle->d), fb_height);
               
               /* Get particle size */
               particle_size = 1;
               if (rgui->particle_effect == RGUI_PARTICLE_EFFECT_SNOW_ALT)
               {
                  /* Gives the following distribution:
                   * 1x1: 96
                   * 2x2: 128
                   * 3x3: 32 */
                  if (!(i & 0x2))
                     particle_size = 2;
                  else if ((i & 0x7) == 0x7)
                     particle_size = 3;
               }
               
               /* Draw particle */
               on_screen = rgui_draw_particle(rgui_frame_buf.data, fb_width, fb_height,
                                 (int)particle->a, (int)particle->b,
                                 particle_size, particle_size, rgui->colors.particle_color);
               
               /* Reset particle if it has fallen off screen */
               if (!on_screen)
               {
                  particle->a = (particle->a < 0.0f) ? (particle->a + (float)fb_width)  : particle->a;
                  particle->b = (particle->b < 0.0f) ? (particle->b + (float)fb_height) : particle->b;
               }
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_RAIN:
         {
            uint8_t weights[] = { /* 60 entries */
               2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
               3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
               4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
               5, 5, 5, 5, 5, 5, 5, 5,
               6, 6, 6, 6, 6, 6,
               7, 7, 7, 7,
               8, 8, 8,
               9, 9,
               10};
            unsigned num_drops = (unsigned)(0.85f * ((float)fb_width / (float)MAX_FB_WIDTH) * (float)NUM_PARTICLES);
            bool on_screen;
            
            num_drops = num_drops < NUM_PARTICLES ? num_drops : NUM_PARTICLES;
            
            for (i = 0; i < num_drops; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* Draw particle */
               on_screen = rgui_draw_particle(rgui_frame_buf.data, fb_width, fb_height,
                                 (int)particle->a, (int)particle->b,
                                 2, (unsigned)particle->c, rgui->colors.particle_color);
               
               /* Update y pos */
               particle->b += particle->d * global_speed_factor;
               
               /* Reset particle if it has fallen off the bottom of the screen */
               if (!on_screen)
               {
                  /* x pos */
                  particle->a = (float)(rand() % (fb_width / 3)) * 3.0f;
                  /* y pos */
                  particle->b = 0.0f;
                  /* drop length */
                  particle->c = (float)weights[(unsigned)(rand() % 60)];
                  /* drop speed (larger drops fall faster) */
                  particle->d = (particle->c / 12.0f) * (0.5f + ((float)(rand() % 150) / 200.0f));
               }
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_VORTEX:
         {
            float max_radius         = (float)sqrt((double)((fb_width * fb_width) + (fb_height * fb_height))) / 2.0f;
            float one_degree_radians = PI / 360.0f;
            int x_centre             = (int)(fb_width >> 1);
            int y_centre             = (int)(fb_height >> 1);
            unsigned particle_size;
            float r_speed, theta_speed;
            int x, y;
            
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* Get particle location */
               x = (int)(particle->a * cos(particle->b)) + x_centre;
               y = (int)(particle->a * sin(particle->b)) + y_centre;
               
               /* Get particle size */
               particle_size = 1 + (unsigned)(((1.0f - ((max_radius - particle->a) / max_radius)) * 3.5f) + 0.5f);
               
               /* Draw particle */
               rgui_draw_particle(rgui_frame_buf.data, fb_width, fb_height,
                     x, y, particle_size, particle_size, rgui->colors.particle_color);
               
               /* Update particle speed */
               r_speed     = particle->c * global_speed_factor;
               theta_speed = particle->d * global_speed_factor;
               if ((particle->a > 0.0f) && (particle->a < (float)fb_height))
               {
                  float base_scale_factor = ((float)fb_height - particle->a) / (float)fb_height;
                  r_speed     *= 1.0f + (base_scale_factor * 8.0f);
                  theta_speed *= 1.0f + (base_scale_factor * base_scale_factor * 6.0f);
               }
               particle->a -= r_speed;
               particle->b += theta_speed;
               
               /* Reset particle if it has reached the centre of the screen */
               if (particle->a < 0.0f)
               {
                  /* radius
                   * Note: In theory, this should be:
                   * > particle->a = max_radius;
                   * ...but it turns out that spawning new particles at random
                   * locations produces a more visually appealing result... */
                  particle->a = 1.0f + (((float)rand() / (float)RAND_MAX) * max_radius);
                  /* theta */
                  particle->b = ((float)rand() / (float)RAND_MAX) * 2.0f * PI;
                  /* radial speed */
                  particle->c = (float)((rand() % 100) + 1) * 0.001f;
                  /* rotational speed */
                  particle->d = (((float)((rand() % 50) + 1) / 200.0f) + 0.1f) * one_degree_radians;
               }
            }
         }
         break;
      case RGUI_PARTICLE_EFFECT_STARFIELD:
         {
            float focal_length = (float)fb_width * 2.0f;
            int x_centre       = (int)(fb_width >> 1);
            int y_centre       = (int)(fb_height >> 1);
            unsigned particle_size;
            int x, y;
            bool on_screen;
            
            /* Based on an example found here:
             * https://codepen.io/nodws/pen/pejBNb */
            for (i = 0; i < NUM_PARTICLES; i++)
            {
               rgui_particle_t *particle = &particles[i];
               
               /* Get particle location */
               x = (int)((particle->a - (float)x_centre) * (focal_length / particle->c));
               x += x_centre;
               
               y = (int)((particle->b - (float)y_centre) * (focal_length / particle->c));
               y += y_centre;
               
               /* Get particle size */
               particle_size = (unsigned)(focal_length / (2.0f * particle->c));
               
               /* Draw particle */
               on_screen = rgui_draw_particle(rgui_frame_buf.data, fb_width, fb_height,
                                 x, y, particle_size, particle_size, rgui->colors.particle_color);
               
               /* Update depth */
               particle->c -= particle->d * global_speed_factor;
               
               /* Reset particle if it has:
                * - Dropped off the edge of the screen
                * - Reached the screen depth
                * - Grown larger than 16 pixels across
                *   (this is an arbitrary limit, set to reduce overall
                *   performance impact - i.e. larger particles are slower
                *   to draw, and without setting a limit they can fill the screen...) */
               if (!on_screen || (particle->c <= 0.0f) || particle_size > 16)
               {
                  /* x pos */
                  particle->a = (float)(rand() % fb_width);
                  /* y pos */
                  particle->b = (float)(rand() % fb_height);
                  /* depth */
                  particle->c = (float)fb_width;
                  /* speed */
                  particle->d = 1.0f + ((float)(rand() % 20) * 0.01f);
               }
            }
         }
         break;
      default:
         /* Do nothing... */
         break;
   }
   
   /* If border is enabled, it must be drawn *above*
    * particle effect
    * (Wastes CPU cycles, but nothing we can do about it...) */
   if (rgui->border_enable && !rgui->show_wallpaper)
      rgui_render_border(rgui, rgui_frame_buf.data, fb_width, fb_height);
}

static void process_wallpaper(rgui_t *rgui, struct texture_image *image)
{
   unsigned x, y;

   /* Note: Ugly hacks required for the Wii, since GEKKO
    * platforms only support a 16:9 framebuffer width of
    * 424 instead of the usual 426... */

   /* Sanity check */
   if (!image->pixels ||
#if defined(GEKKO)
       (image->width != ((rgui_background_buf.width == 424) ?
            (rgui_background_buf.width + 2) : rgui_background_buf.width)) ||
#else
       (image->width != rgui_background_buf.width) ||
#endif
       (image->height != rgui_background_buf.height) ||
       !rgui_background_buf.data)
      return;

   /* Copy image to wallpaper buffer, performing pixel format conversion */
   for (x = 0; x < rgui_background_buf.width; x++)
   {
      for (y = 0; y < rgui_background_buf.height; y++)
      {
         rgui_background_buf.data[x + (y * rgui_background_buf.width)] =
#if defined(GEKKO)
            argb32_to_pixel_platform_format(image->pixels[
                  x + ((rgui_background_buf.width == 424) ? 1 : 0) +
                  (y * image->width)]);
#else
            argb32_to_pixel_platform_format(image->pixels[x + (y * rgui_background_buf.width)]);
#endif
      }
   }

   rgui->show_wallpaper = true;

   /* Tell menu that a display update is required */
   rgui->force_redraw = true;
}

static bool request_thumbnail(
      thumbnail_t *thumbnail,
      enum menu_thumbnail_id thumbnail_id,
      uint32_t *queue_size,
      const char *path,
      bool *file_missing)
{
   /* Do nothing if current thumbnail path hasn't changed */
   if (!string_is_empty(path) && !string_is_empty(thumbnail->path))
      if (string_is_equal(thumbnail->path, path))
         return true;

   /* 'Reset' current thumbnail */
   thumbnail->width = 0;
   thumbnail->height = 0;
   thumbnail->is_valid = false;
   thumbnail->path[0] = '\0';

   /* Ensure that new path is valid... */
   if (!string_is_empty(path))
   {
      strlcpy(thumbnail->path, path, sizeof(thumbnail->path));
      if (path_is_valid(path))
      {
         /* Would like to cancel any existing image load tasks
          * here, but can't see how to do it... */
         if(task_push_image_load(thumbnail->path,
                  video_driver_supports_rgba(), 0,
                  (thumbnail_id == MENU_THUMBNAIL_LEFT) ?
            menu_display_handle_left_thumbnail_upload : menu_display_handle_thumbnail_upload, NULL))
         {
            *queue_size = *queue_size + 1;
            return true;
         }
      }
      else
         *file_missing = true;
   }

   return false;
}

static bool downscale_thumbnail(rgui_t *rgui, unsigned max_width, unsigned max_height,
      struct texture_image *image_src, struct texture_image *image_dst)
{
   settings_t *settings = config_get_ptr();

   /* Determine output dimensions */
   float display_aspect_ratio = (float)max_width / (float)max_height;
   float aspect_ratio = (float)image_src->width / (float)image_src->height;
   if (aspect_ratio > display_aspect_ratio)
   {
      image_dst->width = max_width;
      image_dst->height = image_src->height * max_width / image_src->width;
      /* Account for any possible rounding errors... */
      image_dst->height = (image_dst->height < 1) ? 1 : image_dst->height;
      image_dst->height = (image_dst->height > max_height) ? max_height : image_dst->height;
   }
   else
   {
      image_dst->height = max_height;
      image_dst->width = image_src->width * max_height / image_src->height;
      /* Account for any possible rounding errors... */
      image_dst->width = (image_dst->width < 1) ? 1 : image_dst->width;
      image_dst->width = (image_dst->width > max_width) ? max_width : image_dst->width;
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

static void process_thumbnail(rgui_t *rgui, thumbnail_t *thumbnail, uint32_t *queue_size, struct texture_image *image_src)
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
    * old images if we have a backlog) */
   if (*queue_size > 0)
      *queue_size = *queue_size - 1;
   if (*queue_size > 0)
      return;

   /* Sanity check */
   if (!image_src->pixels || (image_src->width < 1) || (image_src->height < 1) || !thumbnail->data)
      return;

   /* Downscale thumbnail if it exceeds maximum size limits */
   if ((image_src->width > thumbnail->max_width) || (image_src->height > thumbnail->max_height))
   {
      if (!downscale_thumbnail(rgui, thumbnail->max_width, thumbnail->max_height, image_src, &image_resampled))
         return;
      image = &image_resampled;
   }
   else
   {
      image = image_src;
   }

   thumbnail->width = image->width;
   thumbnail->height = image->height;

   /* Copy image to thumbnail buffer, performing pixel format conversion */
   for (x = 0; x < thumbnail->width; x++)
   {
      for (y = 0; y < thumbnail->height; y++)
      {
         thumbnail->data[x + (y * thumbnail->width)] =
            argb32_to_pixel_platform_format(image->pixels[x + (y * thumbnail->width)]);
      }
   }

   thumbnail->is_valid = true;

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
   rgui_t *rgui         = (rgui_t*)userdata;
   settings_t *settings = config_get_ptr();

   if (!rgui || !settings)
   {
      return false;
   }

   if (!data)
   {
      /* This means we have a 'broken' image. There is no
       * data, but we still have to decrement any thumbnail
       * queues (otherwise further thumbnail processing will
       * be blocked) */
      if (type == MENU_IMAGE_THUMBNAIL)
      {
         if (rgui->thumbnail_queue_size > 0)
            rgui->thumbnail_queue_size--;
      }
      else if (type == MENU_IMAGE_LEFT_THUMBNAIL)
      {
         if (rgui->left_thumbnail_queue_size > 0)
            rgui->left_thumbnail_queue_size--;
      }

      return false;
   }

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
            
            if (rgui->show_fs_thumbnail)
               process_thumbnail(rgui, &fs_thumbnail, &rgui->thumbnail_queue_size, image);
            else if (settings->bools.menu_rgui_inline_thumbnails)
               process_thumbnail(rgui, &mini_thumbnail, &rgui->thumbnail_queue_size, image);
            else
            {
               /* If user toggles settings rapidly on very slow systems,
                * it is possible for a thumbnail to be requested without
                * it ever being processed. In this case, we still have to
                * decrement the thumbnail queue (otherwise image updates
                * will get 'stuck') */
               if (rgui->thumbnail_queue_size > 0)
                  rgui->thumbnail_queue_size--;
            }
         }
         break;
      case MENU_IMAGE_LEFT_THUMBNAIL:
         {
            struct texture_image *image = (struct texture_image*)data;
            process_thumbnail(rgui, &mini_left_thumbnail, &rgui->left_thumbnail_queue_size, image);
         }
         break;
      default:
         break;
   }

   return true;
}

static void rgui_render_background(void)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;

   if (rgui_frame_buf.data && rgui_background_buf.data)
   {
      menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

      /* Sanity check */
      if ((fb_width != rgui_frame_buf.width) || (fb_height != rgui_frame_buf.height) || (fb_pitch != rgui_frame_buf.width << 1))
         return;

      /* Copy background to framebuffer */
      memcpy(rgui_frame_buf.data, rgui_background_buf.data, rgui_frame_buf.width * rgui_frame_buf.height * sizeof(uint16_t));
   }
}

static void rgui_render_fs_thumbnail(rgui_t *rgui)
{
   if (fs_thumbnail.is_valid && rgui_frame_buf.data && fs_thumbnail.data)
   {
      size_t fb_pitch;
      unsigned fb_width, fb_height;
      unsigned y;
      unsigned fb_x_offset, fb_y_offset;
      unsigned thumb_x_offset, thumb_y_offset;
      unsigned width, height;
      uint16_t *src  = NULL;
      uint16_t *dst  = NULL;

      menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

      /* Ensure that thumbnail is centred
       * > Have to perform some stupid tests here because we
       *   cannot assume fb_width and fb_height are constant and
       *   >= thumbnail.width and thumbnail.height (even though
       *   they are...) */
      if (fs_thumbnail.width <= fb_width)
      {
         thumb_x_offset = 0;
         fb_x_offset = (fb_width - fs_thumbnail.width) >> 1;
         width = fs_thumbnail.width;
      }
      else
      {
         thumb_x_offset = (fs_thumbnail.width - fb_width) >> 1;
         fb_x_offset = 0;
         width = fb_width;
      }
      if (fs_thumbnail.height <= fb_height)
      {
         thumb_y_offset = 0;
         fb_y_offset = (fb_height - fs_thumbnail.height) >> 1;
         height = fs_thumbnail.height;
      }
      else
      {
         thumb_y_offset = (fs_thumbnail.height - fb_height) >> 1;
         fb_y_offset = 0;
         height = fb_height;
      }

      /* Copy thumbnail to framebuffer */
      for (y = 0; y < height; y++)
      {
         src = fs_thumbnail.data + thumb_x_offset + ((y + thumb_y_offset) * fs_thumbnail.width);
         dst = rgui_frame_buf.data + (y + fb_y_offset) * (fb_pitch >> 1) + fb_x_offset;
         
         memcpy(dst, src, width * sizeof(uint16_t));
      }

      /* Draw drop shadow, if required */
      if (rgui->shadow_enable)
      {
         unsigned shadow_x;
         unsigned shadow_y;
         unsigned shadow_width;
         unsigned shadow_height;

         /* Vertical component */
         if (fs_thumbnail.width < fb_width)
         {
            shadow_width = fb_width - fs_thumbnail.width;
            shadow_width = shadow_width > 2 ? 2 : shadow_width;
            shadow_height = fs_thumbnail.height + 2 < fb_height ? fs_thumbnail.height : fb_height - 2;

            shadow_x = fb_x_offset + fs_thumbnail.width;
            shadow_y = fb_y_offset + 2;

            rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
                  shadow_x, shadow_y, shadow_width, shadow_height, rgui->colors.shadow_color);
         }

         /* Horizontal component */
         if (fs_thumbnail.height < fb_height)
         {
            shadow_height = fb_height - fs_thumbnail.height;
            shadow_height = shadow_height > 2 ? 2 : shadow_height;
            shadow_width = fs_thumbnail.width + 2 < fb_width ? fs_thumbnail.width : fb_width - 2;

            shadow_x = fb_x_offset + 2;
            shadow_y = fb_y_offset + fs_thumbnail.height;

            rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
                  shadow_x, shadow_y, shadow_width, shadow_height, rgui->colors.shadow_color);
         }
      }
   }
}

static unsigned INLINE rgui_get_mini_thumbnail_fullwidth(void)
{
   unsigned width = mini_thumbnail.is_valid ? mini_thumbnail.width : 0;
   unsigned left_width = mini_left_thumbnail.is_valid ? mini_left_thumbnail.width : 0;
   return width >= left_width ? width : left_width;
}

static void rgui_render_mini_thumbnail(rgui_t *rgui, thumbnail_t *thumbnail, enum menu_thumbnail_id thumbnail_id)
{
   settings_t *settings = config_get_ptr();

   if (!thumbnail || !settings)
      return;

   if (thumbnail->is_valid && rgui_frame_buf.data && thumbnail->data)
   {
      size_t fb_pitch;
      unsigned fb_width, fb_height;
      unsigned term_width, term_height;
      unsigned y;
      unsigned fb_x_offset, fb_y_offset;
      unsigned thumbnail_fullwidth = rgui_get_mini_thumbnail_fullwidth();
      uint16_t *src  = NULL;
      uint16_t *dst  = NULL;

      menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

      term_width = rgui_term_layout.width * FONT_WIDTH_STRIDE;
      term_height = rgui_term_layout.height * FONT_HEIGHT_STRIDE;

      /* Sanity check (this can never, ever happen, so just return
       * instead of trying to crop the thumbnail image...) */
      if ((thumbnail_fullwidth > term_width) || (thumbnail->height > term_height))
         return;

      fb_x_offset = (rgui_term_layout.start_x + term_width) -
            (thumbnail->width + ((thumbnail_fullwidth - thumbnail->width) >> 1));
      
      if (((thumbnail_id == MENU_THUMBNAIL_RIGHT) && !settings->bools.menu_rgui_swap_thumbnails) ||
          ((thumbnail_id == MENU_THUMBNAIL_LEFT)  && settings->bools.menu_rgui_swap_thumbnails))
      {
         fb_y_offset = rgui_term_layout.start_y + ((thumbnail->max_height - thumbnail->height) >> 1);
      }
      else
      {
         fb_y_offset = (rgui_term_layout.start_y + term_height) -
               (thumbnail->height + ((thumbnail->max_height - thumbnail->height) >> 1));
      }

      /* Copy thumbnail to framebuffer */
      for (y = 0; y < thumbnail->height; y++)
      {
         src = thumbnail->data + (y * thumbnail->width);
         dst = rgui_frame_buf.data + (y + fb_y_offset) * (fb_pitch >> 1) + fb_x_offset;
         
         memcpy(dst, src, thumbnail->width * sizeof(uint16_t));
      }

      /* Draw drop shadow, if required */
      if (rgui->shadow_enable)
      {
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               fb_x_offset + thumbnail->width, fb_y_offset + 1, 1, thumbnail->height, rgui->colors.shadow_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               fb_x_offset + 1, fb_y_offset + thumbnail->height, thumbnail->width, 1, rgui->colors.shadow_color);
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
   char wallpaper_file[PATH_MAX_LENGTH];
   unsigned normal_color       = 0;
   unsigned hover_color        = 0;
   unsigned title_color        = 0;
   unsigned bg_dark_color      = 0;
   unsigned bg_light_color     = 0;
   unsigned border_dark_color  = 0;
   unsigned border_light_color = 0;
   unsigned shadow_color       = 0;
   unsigned particle_color     = 0;
   config_file_t *conf         = NULL;
   char *wallpaper_key         = NULL;
   settings_t *settings        = config_get_ptr();
   bool success                = false;

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
   if (!path_is_valid(theme_path))
      goto end;

   /* Open config file */
   if (!(conf = config_file_new_from_path_to_string(theme_path)))
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

   /* Make shadow colour optional (fallback to fully opaque black)
    * - i.e. if user has no intention of enabling shadows, they
    * should not have to include this entry */
   if(!config_get_hex(conf, "rgui_shadow_color", &shadow_color))
      shadow_color = 0xFF000000;

   /* Make particle colour optional too (fallback to normal
    * rgb with bg_light alpha) */
   if(!config_get_hex(conf, "rgui_particle_color", &particle_color))
      particle_color = (normal_color & 0x00FFFFFF) |
                       (bg_light_color & 0xFF000000);

   config_get_array(conf, wallpaper_key,
         wallpaper_file, sizeof(wallpaper_file));

   success = true;

end:

   if (success)
   {
      theme_colors->normal_color       = (uint32_t)normal_color;
      theme_colors->hover_color        = (uint32_t)hover_color;
      theme_colors->title_color        = (uint32_t)title_color;
      theme_colors->bg_dark_color      = (uint32_t)bg_dark_color;
      theme_colors->bg_light_color     = (uint32_t)bg_light_color;
      theme_colors->border_dark_color  = (uint32_t)border_dark_color;
      theme_colors->border_light_color = (uint32_t)border_light_color;
      theme_colors->shadow_color       = (uint32_t)shadow_color;
      theme_colors->particle_color     = (uint32_t)particle_color;

      /* Load wallpaper, if required */
      if (!string_is_empty(wallpaper_file))
      {
         char wallpaper_path[PATH_MAX_LENGTH];
         wallpaper_path[0] = '\0';

         fill_pathname_resolve_relative(wallpaper_path, theme_path, wallpaper_file, sizeof(wallpaper_path));
         /* Ensure that path is valid... */
         if (path_is_valid(wallpaper_path))
            /* Unlike thumbnails, we don't worry about queued images
             * here - in general, wallpaper is loaded once per session
             * and then forgotten, so performance issues are not a concern */
            task_push_image_load(wallpaper_path,
                  video_driver_supports_rgba(), 0,
                  menu_display_handle_wallpaper_upload, NULL);
      }
   }
   else
   {
      /* Use 'Classic Green' fallback */
      theme_colors->normal_color       = rgui_theme_classic_green.normal_color;
      theme_colors->hover_color        = rgui_theme_classic_green.hover_color;
      theme_colors->title_color        = rgui_theme_classic_green.title_color;
      theme_colors->bg_dark_color      = rgui_theme_classic_green.bg_dark_color;
      theme_colors->bg_light_color     = rgui_theme_classic_green.bg_light_color;
      theme_colors->border_dark_color  = rgui_theme_classic_green.border_dark_color;
      theme_colors->border_light_color = rgui_theme_classic_green.border_light_color;
      theme_colors->shadow_color       = rgui_theme_classic_green.shadow_color;
      theme_colors->particle_color     = rgui_theme_classic_green.particle_color;
   }

   if (conf)
      config_file_free(conf);
   conf = NULL;
}

static void rgui_cache_background(rgui_t *rgui)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;

   /* Only regenerate the background if we are *not*
    * currently showing a wallpaper image */
   if (rgui->show_wallpaper)
      return;

   menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

   /* Sanity check */
   if ((fb_width  != rgui_background_buf.width)      ||
       (fb_height != rgui_background_buf.height)     ||
       (fb_pitch  != rgui_background_buf.width << 1) ||
       !rgui_background_buf.data)
      return;

   /* Fill background buffer with standard chequer pattern */
   rgui_fill_rect(rgui_background_buf.data, fb_width, fb_height,
         0, 0, fb_width, fb_height,
         rgui->colors.bg_dark_color, rgui->colors.bg_light_color, rgui->bg_thickness);

   /* Draw border, if required */
   if (rgui->border_enable)
      rgui_render_border(rgui, rgui_background_buf.data, fb_width, fb_height);
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

      theme_colors.hover_color          = current_theme->hover_color;
      theme_colors.normal_color         = current_theme->normal_color;
      theme_colors.title_color          = current_theme->title_color;
      theme_colors.bg_dark_color        = current_theme->bg_dark_color;
      theme_colors.bg_light_color       = current_theme->bg_light_color;
      theme_colors.border_dark_color    = current_theme->border_dark_color;
      theme_colors.border_light_color   = current_theme->border_light_color;
      theme_colors.shadow_color         = current_theme->shadow_color;
      theme_colors.particle_color       = current_theme->particle_color;
   }
   rgui->colors.hover_color             = argb32_to_pixel_platform_format(theme_colors.hover_color);
   rgui->colors.normal_color            = argb32_to_pixel_platform_format(theme_colors.normal_color);
   rgui->colors.title_color             = argb32_to_pixel_platform_format(theme_colors.title_color);
   rgui->colors.bg_dark_color           = argb32_to_pixel_platform_format(theme_colors.bg_dark_color);
   rgui->colors.bg_light_color          = argb32_to_pixel_platform_format(theme_colors.bg_light_color);
   rgui->colors.border_dark_color       = argb32_to_pixel_platform_format(theme_colors.border_dark_color);
   rgui->colors.border_light_color      = argb32_to_pixel_platform_format(theme_colors.border_light_color);
   rgui->colors.shadow_color            = argb32_to_pixel_platform_format(theme_colors.shadow_color);
   rgui->colors.particle_color          = argb32_to_pixel_platform_format(theme_colors.particle_color);

   rgui->bg_modified                    = true;
   rgui->force_redraw                   = true;
}

/* ==============================
 * blit_line/symbol() START
 * ============================== */

/* NOTE: These functions are WET (Write Everything Twice).
 * This is bad design and difficult to maintain, but we have
 * no other choice here. blit_line() is so performance
 * critical that we simply cannot afford to check user
 * settings internally. */

/* blit_line() */

static void blit_line_regular(unsigned fb_width, int x, int y,
      const char *message, uint16_t color, uint16_t shadow_color)
{
   uint16_t *frame_buf_data = rgui_frame_buf.data;

   while (!string_is_empty(message))
   {
      unsigned i, j;
      uint8_t symbol = (uint8_t)*message++;

      if (symbol >= NUM_FONT_GLYPHS_REGULAR)
         continue;

      if (symbol != ' ')
      {
         for (j = 0; j < FONT_HEIGHT; j++)
         {
            unsigned buff_offset = ((y + j) * fb_width) + x;

            for (i = 0; i < FONT_WIDTH; i++)
            {
               if (font_lut[symbol][i + (j * FONT_WIDTH)])
                  *(frame_buf_data + buff_offset + i) = color;
            }
         }
      }

      x += FONT_WIDTH_STRIDE;
   }
}

static void blit_line_regular_shadow(unsigned fb_width, int x, int y,
      const char *message, uint16_t color, uint16_t shadow_color)
{
   uint16_t *frame_buf_data     = rgui_frame_buf.data;
   uint16_t color_buf[2];
   uint16_t shadow_color_buf[2];

   color_buf[0] = color;
   color_buf[1] = shadow_color;

   shadow_color_buf[0] = shadow_color;
   shadow_color_buf[1] = shadow_color;

   while (!string_is_empty(message))
   {
      unsigned i, j;
      uint8_t symbol = (uint8_t)*message++;

      if (symbol >= NUM_FONT_GLYPHS_REGULAR)
         continue;

      if (symbol != ' ')
      {
         for (j = 0; j < FONT_HEIGHT; j++)
         {
            unsigned buff_offset = ((y + j) * fb_width) + x;

            for (i = 0; i < FONT_WIDTH; i++)
            {
               if (font_lut[symbol][i + (j * FONT_WIDTH)])
               {
                  uint16_t *frame_buf_ptr = frame_buf_data + buff_offset + i;

                  /* Text pixel + right shadow */
                  memcpy(frame_buf_ptr, color_buf, sizeof(color_buf));

                  /* Bottom shadow */
                  frame_buf_ptr += fb_width;
                  memcpy(frame_buf_ptr, shadow_color_buf, sizeof(shadow_color_buf));
               }
            }
         }
      }

      x += FONT_WIDTH_STRIDE;
   }
}

static void blit_line_extended(unsigned fb_width, int x, int y,
      const char *message, uint16_t color, uint16_t shadow_color)
{
   uint16_t *frame_buf_data = rgui_frame_buf.data;

   while (!string_is_empty(message))
   {
      /* Deal with spaces first, for efficiency */
      if (*message == ' ')
         message++;
      else
      {
         unsigned i, j;
         uint32_t symbol = utf8_walk(&message);

         /* Stupid cretinous hack: 'oe' ligatures are not
          * really standard extended ASCII, so we have to
          * waste CPU cycles performing a conversion from
          * the unicode values...
          * (Note: This is only really required for msg_hash_fr.h) */
         if (symbol == 339) /* Latin small ligature oe */
            symbol = 156;
         if (symbol == 338) /* Latin capital ligature oe */
            symbol = 140;

         if (symbol >= NUM_FONT_GLYPHS_EXTENDED)
            continue;

         for (j = 0; j < FONT_HEIGHT; j++)
         {
            unsigned buff_offset = ((y + j) * fb_width) + x;

            for (i = 0; i < FONT_WIDTH; i++)
            {
               if (font_lut[symbol][i + (j * FONT_WIDTH)])
                  *(frame_buf_data + buff_offset + i) = color;
            }
         }
      }

      x += FONT_WIDTH_STRIDE;
   }
}

static void blit_line_extended_shadow(unsigned fb_width, int x, int y,
      const char *message, uint16_t color, uint16_t shadow_color)
{
   uint16_t *frame_buf_data     = rgui_frame_buf.data;
   uint16_t color_buf[2];
   uint16_t shadow_color_buf[2];

   color_buf[0] = color;
   color_buf[1] = shadow_color;

   shadow_color_buf[0] = shadow_color;
   shadow_color_buf[1] = shadow_color;

   while (!string_is_empty(message))
   {
      /* Deal with spaces first, for efficiency */
      if (*message == ' ')
         message++;
      else
      {
         unsigned i, j;
         uint32_t symbol = utf8_walk(&message);

         /* Stupid cretinous hack: 'oe' ligatures are not
          * really standard extended ASCII, so we have to
          * waste CPU cycles performing a conversion from
          * the unicode values...
          * (Note: This is only really required for msg_hash_fr.h) */
         if (symbol == 339) /* Latin small ligature oe */
            symbol = 156;
         if (symbol == 338) /* Latin capital ligature oe */
            symbol = 140;

         if (symbol >= NUM_FONT_GLYPHS_EXTENDED)
            continue;

         for (j = 0; j < FONT_HEIGHT; j++)
         {
            unsigned buff_offset = ((y + j) * fb_width) + x;

            for (i = 0; i < FONT_WIDTH; i++)
            {
               if (font_lut[symbol][i + (j * FONT_WIDTH)])
               {
                  uint16_t *frame_buf_ptr = frame_buf_data + buff_offset + i;

                  /* Text pixel + right shadow */
                  memcpy(frame_buf_ptr, color_buf, sizeof(color_buf));

                  /* Bottom shadow */
                  frame_buf_ptr += fb_width;
                  memcpy(frame_buf_ptr, shadow_color_buf, sizeof(shadow_color_buf));
               }
            }
         }
      }

      x += FONT_WIDTH_STRIDE;
   }
}

static void (*blit_line)(unsigned fb_width, int x, int y,
      const char *message, uint16_t color, uint16_t shadow_color) = blit_line_regular;

/* blit_symbol() */

static const uint8_t *rgui_get_symbol_data(enum rgui_symbol_type symbol)
{
   switch (symbol)
   {
      case RGUI_SYMBOL_BACKSPACE:
         return rgui_symbol_data_backspace;
      case RGUI_SYMBOL_ENTER:
         return rgui_symbol_data_enter;
      case RGUI_SYMBOL_SHIFT_UP:
         return rgui_symbol_data_shift_up;
      case RGUI_SYMBOL_SHIFT_DOWN:
         return rgui_symbol_data_shift_down;
      case RGUI_SYMBOL_NEXT:
         return rgui_symbol_data_next;
      case RGUI_SYMBOL_TEXT_CURSOR:
         return rgui_symbol_data_text_cursor;
      case RGUI_SYMBOL_CHARGING:
         return rgui_symbol_data_charging;
      case RGUI_SYMBOL_BATTERY_100:
         return rgui_symbol_data_battery_100;
      case RGUI_SYMBOL_BATTERY_80:
         return rgui_symbol_data_battery_80;
      case RGUI_SYMBOL_BATTERY_60:
         return rgui_symbol_data_battery_60;
      case RGUI_SYMBOL_BATTERY_40:
         return rgui_symbol_data_battery_40;
      case RGUI_SYMBOL_BATTERY_20:
         return rgui_symbol_data_battery_20;
      case RGUI_SYMBOL_CHECKMARK:
         return rgui_symbol_data_checkmark;
      default:
         break;
   }

   return NULL;
}

static void blit_symbol_regular(unsigned fb_width, int x, int y,
      enum rgui_symbol_type symbol, uint16_t color, uint16_t shadow_color)
{
   unsigned i, j;
   uint16_t *frame_buf_data   = rgui_frame_buf.data;
   const uint8_t *symbol_data = rgui_get_symbol_data(symbol);

   if (!symbol_data)
      return;

   for (j = 0; j < FONT_HEIGHT; j++)
   {
      unsigned buff_offset = ((y + j) * fb_width) + x;

      for (i = 0; i < FONT_WIDTH; i++)
      {
         if (*symbol_data++ == 1)
            *(frame_buf_data + buff_offset + i) = color;
      }
   }
}

static void blit_symbol_shadow(unsigned fb_width, int x, int y,
      enum rgui_symbol_type symbol, uint16_t color, uint16_t shadow_color)
{
   unsigned i, j;
   uint16_t *frame_buf_data     = rgui_frame_buf.data;
   const uint8_t *symbol_data   = rgui_get_symbol_data(symbol);
   uint16_t color_buf[2];
   uint16_t shadow_color_buf[2];

   color_buf[0] = color;
   color_buf[1] = shadow_color;

   shadow_color_buf[0] = shadow_color;
   shadow_color_buf[1] = shadow_color;

   if (!symbol_data)
      return;

   for (j = 0; j < FONT_HEIGHT; j++)
   {
      unsigned buff_offset = ((y + j) * fb_width) + x;

      for (i = 0; i < FONT_WIDTH; i++)
      {
         if (*symbol_data++ == 1)
         {
            uint16_t *frame_buf_ptr = frame_buf_data + buff_offset + i;

            /* Symbol pixel + right shadow */
            memcpy(frame_buf_ptr, color_buf, sizeof(color_buf));

            /* Bottom shadow */
            frame_buf_ptr += fb_width;
            memcpy(frame_buf_ptr, shadow_color_buf, sizeof(shadow_color_buf));
         }
      }
   }
}

static void (*blit_symbol)(unsigned fb_width, int x, int y,
      enum rgui_symbol_type symbol, uint16_t color, uint16_t shadow_color) = blit_symbol_regular;

static void rgui_set_blit_functions(bool draw_shadow, bool extended_ascii)
{
   if (draw_shadow)
   {
      if (extended_ascii)
         blit_line = blit_line_extended_shadow;
      else
         blit_line = blit_line_regular_shadow;
      
      blit_symbol = blit_symbol_shadow;
   }
   else
   {
      if (extended_ascii)
         blit_line = blit_line_extended;
      else
         blit_line = blit_line_regular;
      
      blit_symbol = blit_symbol_regular;
   }
}

/* ==============================
 * blit_line/symbol() END
 * ============================== */

static void rgui_init_font_lut(void)
{
   unsigned symbol_index, i, j;
   
   /* Loop over all possible characters */
   for (symbol_index = 0; symbol_index < NUM_FONT_GLYPHS_EXTENDED; symbol_index++)
   {
      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            unsigned offset  = (i + j * FONT_WIDTH) >> 3;
            
            /* LUT value is 'true' if specified glyph position contains a pixel */
            font_lut[symbol_index][i + (j * FONT_WIDTH)] = (bitmap_bin[FONT_OFFSET(symbol_index) + offset] & rem) > 0;
         }
      }
   }
}

static void rgui_set_message(void *data, const char *message)
{
   rgui_t           *rgui = (rgui_t*)data;

   if (!rgui || !message)
      return;

   rgui->msgbox[0] = '\0';

   if (!string_is_empty(message))
      strlcpy(rgui->msgbox, message, sizeof(rgui->msgbox));

   rgui->force_redraw = true;
}

static void rgui_render_messagebox(rgui_t *rgui, const char *message)
{
   int x, y;
   size_t i, fb_pitch;
   unsigned fb_width, fb_height;
   unsigned width, glyphs_width, height;
   struct string_list *list   = NULL;

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

      if (msglen > rgui_term_layout.width)
      {
         msg[rgui_term_layout.width - 2] = '.';
         msg[rgui_term_layout.width - 1] = '.';
         msg[rgui_term_layout.width - 0] = '.';
         msg[rgui_term_layout.width + 1] = '\0';
         msglen = rgui_term_layout.width;
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
      uint16_t border_dark_color  = rgui->colors.border_dark_color;
      uint16_t border_light_color = rgui->colors.border_light_color;
      bool border_thickness       = rgui->border_thickness;

      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            x + 5, y + 5, width - 10, height - 10,
            rgui->colors.bg_dark_color, rgui->colors.bg_light_color, rgui->bg_thickness);

      /* Note: We draw borders around message boxes regardless
       * of the rgui->border_enable setting, because they look
       * ridiculous without... */

      /* Draw drop shadow, if required */
      if (rgui->shadow_enable)
      {
         uint16_t shadow_color = rgui->colors.shadow_color;

         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               x + 5, y + 5, 1, height - 5, shadow_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               x + 5, y + 5, width - 5, 1, shadow_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               x + width, y + 1, 1, height, shadow_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               x + 1, y + height, width, 1, shadow_color);
      }

      /* Draw border */
      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            x, y, width - 5, 5,
            border_dark_color, border_light_color, border_thickness);
      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            x + width - 5, y, 5, height - 5,
            border_dark_color, border_light_color, border_thickness);
      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            x + 5, y + height - 5, width - 5, 5,
            border_dark_color, border_light_color, border_thickness);
      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            x, y + 5, 5, height - 5,
            border_dark_color, border_light_color, border_thickness);
   }

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int offset_x    = (int)(FONT_WIDTH_STRIDE * (glyphs_width - utf8len(msg)) / 2);
      int offset_y    = (int)(FONT_HEIGHT_STRIDE * i);

      if (rgui_frame_buf.data)
         blit_line(fb_width, x + 8 + offset_x, y + 8 + offset_y, msg,
               rgui->colors.normal_color, rgui->colors.shadow_color);
   }

end:
   string_list_free(list);
}

static void rgui_blit_cursor(rgui_t *rgui)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   if (rgui_frame_buf.data)
   {
      rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height, rgui->pointer.x, rgui->pointer.y - 5, 1, 11, rgui->colors.normal_color);
      rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height, rgui->pointer.x - 5, rgui->pointer.y, 11, 1, rgui->colors.normal_color);
   }
}

int rgui_osk_ptr_at_pos(void *data, int x, int y,
      unsigned width, unsigned height)
{
   /* This is a lazy copy/paste from rgui_render_osk(),
    * but it will do for now... */
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   size_t key_index;
   
   unsigned key_width, key_height;
   unsigned key_text_offset_x, key_text_offset_y;
   unsigned ptr_width, ptr_height;
   unsigned ptr_offset_x, ptr_offset_y;
   
   unsigned keyboard_width, keyboard_height;
   unsigned keyboard_offset_x, keyboard_offset_y;
   
   unsigned osk_width, osk_height;
   unsigned osk_x, osk_y;

   /* Get dimensions/layout */
   menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

   key_text_offset_x      = 8;
   key_text_offset_y      = 6;
   key_width              = FONT_WIDTH  + (key_text_offset_x * 2);
   key_height             = FONT_HEIGHT + (key_text_offset_y * 2);
   ptr_offset_x           = 2;
   ptr_offset_y           = 2;
   ptr_width              = key_width  - (ptr_offset_x * 2);
   ptr_height             = key_height - (ptr_offset_y * 2);
   keyboard_width         = key_width  * OSK_CHARS_PER_LINE;
   keyboard_height        = key_height * 4;
   keyboard_offset_x      = 10;
   keyboard_offset_y      = 10 + 15 + (2 * FONT_HEIGHT_STRIDE);
   osk_width              = keyboard_width + 20;
   osk_height             = keyboard_offset_y + keyboard_height + 10;
   osk_x                  = (fb_width - osk_width) / 2;
   osk_y                  = (fb_height - osk_height) / 2;

   for (key_index = 0; key_index < 44; key_index++)
   {
      unsigned key_row     = (unsigned)(key_index / OSK_CHARS_PER_LINE);
      unsigned key_column  = (unsigned)(key_index - (key_row * OSK_CHARS_PER_LINE));

      unsigned osk_ptr_x = osk_x + keyboard_offset_x + ptr_offset_x + (key_column * key_width);
      unsigned osk_ptr_y = osk_y + keyboard_offset_y + ptr_offset_y + (key_row    * key_height);

      if (x > osk_ptr_x && x < osk_ptr_x + ptr_width &&
          y > osk_ptr_y && y < osk_ptr_y + ptr_height)
         return (int)key_index;
   }

   return -1;
}

static void rgui_render_osk(
      rgui_t *rgui,
      menu_animation_ctx_ticker_t *ticker, menu_animation_ctx_ticker_smooth_t *ticker_smooth,
      bool use_smooth_ticker)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   size_t key_index;
   
   unsigned input_label_max_length;
   unsigned input_str_max_length;
   unsigned input_offset_x, input_offset_y;
   
   unsigned key_width, key_height;
   unsigned key_text_offset_x, key_text_offset_y;
   unsigned ptr_width, ptr_height;
   unsigned ptr_offset_x, ptr_offset_y;
   
   unsigned keyboard_width, keyboard_height;
   unsigned keyboard_offset_x, keyboard_offset_y;
   
   unsigned osk_width, osk_height;
   unsigned osk_x, osk_y;
   
   int osk_ptr             = menu_event_get_osk_ptr();
   char **osk_grid         = menu_event_get_osk_grid();
   const char *input_str   = menu_input_dialog_get_buffer();
   const char *input_label = menu_input_dialog_get_label_buffer();
   
   /* Sanity check 1 */
   if (!rgui_frame_buf.data || osk_ptr < 0 || osk_ptr >= 44 || !osk_grid[0])
      return;
   
   /* Get dimensions/layout */
   menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);
   
   key_text_offset_x      = 8;
   key_text_offset_y      = 6;
   key_width              = FONT_WIDTH  + (key_text_offset_x * 2);
   key_height             = FONT_HEIGHT + (key_text_offset_y * 2);
   ptr_offset_x           = 2;
   ptr_offset_y           = 2;
   ptr_width              = key_width  - (ptr_offset_x * 2);
   ptr_height             = key_height - (ptr_offset_y * 2);
   keyboard_width         = key_width  * OSK_CHARS_PER_LINE;
   keyboard_height        = key_height * 4;
   keyboard_offset_x      = 10;
   keyboard_offset_y      = 10 + 15 + (2 * FONT_HEIGHT_STRIDE);
   input_label_max_length = (keyboard_width / FONT_WIDTH_STRIDE);
   input_str_max_length   = input_label_max_length - 1;
   input_offset_x         = 10 + (keyboard_width - (input_label_max_length * FONT_WIDTH_STRIDE)) / 2;
   input_offset_y         = 10;
   osk_width              = keyboard_width + 20;
   osk_height             = keyboard_offset_y + keyboard_height + 10;
   osk_x                  = (fb_width - osk_width) / 2;
   osk_y                  = (fb_height - osk_height) / 2;
   
   /* Sanity check 2 */
   if ((osk_width + 2 > fb_width) || (osk_height + 2 > fb_height))
   {
      /* This can never happen, but have to make sure...
       * If OSK cannot physically fit on the screen,
       * fallback to old style 'message box' implementation */
      char msg[255];
      msg[0] = '\0';
      
      snprintf(msg, sizeof(msg), "%s\n%s", input_label, input_str);
      rgui_render_messagebox(rgui, msg);
      
      return;
   }
   
   /* Draw background */
   rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
         osk_x + 5, osk_y + 5, osk_width - 10, osk_height - 10,
         rgui->colors.bg_dark_color, rgui->colors.bg_light_color, rgui->bg_thickness);
   
   /* Draw border */
   if (rgui->border_enable)
   {
      uint16_t border_dark_color  = rgui->colors.border_dark_color;
      uint16_t border_light_color = rgui->colors.border_light_color;
      bool border_thickness       = rgui->border_thickness;
      
      /* Draw drop shadow, if required */
      if (rgui->shadow_enable)
      {
         uint16_t shadow_color = rgui->colors.shadow_color;
         
         /* Frame */
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               osk_x + 5, osk_y + 5, osk_width - 10, 1, shadow_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               osk_x + osk_width, osk_y + 1, 1, osk_height, shadow_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               osk_x + 1, osk_y + osk_height, osk_width, 1, shadow_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               osk_x + 5, osk_y + 5, 1, osk_height - 10, shadow_color);
         /* Divider */
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               osk_x + 5, osk_y + keyboard_offset_y - 5, osk_width - 10, 1, shadow_color);
      }
      
      /* Frame */
      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            osk_x, osk_y, osk_width - 5, 5,
            border_dark_color, border_light_color, border_thickness);
      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            osk_x + osk_width - 5, osk_y, 5, osk_height - 5,
            border_dark_color, border_light_color, border_thickness);
      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            osk_x + 5, osk_y + osk_height - 5, osk_width - 5, 5,
            border_dark_color, border_light_color, border_thickness);
      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            osk_x, osk_y + 5, 5, osk_height - 5,
            border_dark_color, border_light_color, border_thickness);
      /* Divider */
      rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
            osk_x + 5, osk_y + keyboard_offset_y - 10, osk_width - 10, 5,
            border_dark_color, border_light_color, border_thickness);
   }
   
   /* Draw input label text */
   if (!string_is_empty(input_label))
   {
      char input_label_buf[255];
      unsigned input_label_length;
      int input_label_x, input_label_y;
      unsigned ticker_x_offset = 0;
      
      input_label_buf[0] = '\0';
      
      if (use_smooth_ticker)
      {
         ticker_smooth->selected    = true;
         ticker_smooth->field_width = input_label_max_length * FONT_WIDTH_STRIDE;
         ticker_smooth->src_str     = input_label;
         ticker_smooth->dst_str     = input_label_buf;
         ticker_smooth->dst_str_len = sizeof(input_label_buf);
         ticker_smooth->x_offset    = &ticker_x_offset;
         
         menu_animation_ticker_smooth(ticker_smooth);
      }
      else
      {
         ticker->s        = input_label_buf;
         ticker->len      = input_label_max_length;
         ticker->str      = input_label;
         ticker->selected = true;
         
         menu_animation_ticker(ticker);
      }

      input_label_length = (unsigned)(utf8len(input_label_buf) * FONT_WIDTH_STRIDE);
      input_label_x      = ticker_x_offset + osk_x + input_offset_x + ((input_label_max_length * FONT_WIDTH_STRIDE) - input_label_length) / 2;
      input_label_y      = osk_y + input_offset_y;
      
      blit_line(fb_width, input_label_x, input_label_y, input_label_buf,
            rgui->colors.normal_color, rgui->colors.shadow_color);
   }
   
   /* Draw input buffer text */
   {
      unsigned input_str_char_offset;
      int input_str_x, input_str_y;
      int text_cursor_x;
      unsigned input_str_length = (unsigned)strlen(input_str);
      
      if (input_str_length > input_str_max_length)
      {
         input_str_char_offset = input_str_length - input_str_max_length;
         input_str_length      = input_str_max_length;
      }
      else
         input_str_char_offset = 0;
      
      input_str_x = osk_x + input_offset_x;
      input_str_y = osk_y + input_offset_y + FONT_HEIGHT_STRIDE;
      
      if (!string_is_empty(input_str + input_str_char_offset))
         blit_line(fb_width, input_str_x, input_str_y, input_str + input_str_char_offset,
               rgui->colors.hover_color, rgui->colors.shadow_color);
      
      /* Draw text cursor */
      text_cursor_x = osk_x + input_offset_x + (input_str_length * FONT_WIDTH_STRIDE);
      
      blit_symbol(fb_width, text_cursor_x, input_str_y, RGUI_SYMBOL_TEXT_CURSOR,
            rgui->colors.normal_color, rgui->colors.shadow_color);
   }
   
   /* Draw keyboard 'keys' */
   for (key_index = 0; key_index < 44; key_index++)
   {
      unsigned key_row     = (unsigned)(key_index / OSK_CHARS_PER_LINE);
      unsigned key_column  = (unsigned)(key_index - (key_row * OSK_CHARS_PER_LINE));
      
      int key_text_x       = osk_x + keyboard_offset_x + key_text_offset_x + (key_column * key_width);
      int key_text_y       = osk_y + keyboard_offset_y + key_text_offset_y + (key_row    * key_height);
      
      const char *key_text = osk_grid[key_index];
      
      /* 'Command' keys use custom symbols - have to
       * detect them and use blit_symbol(). Everything
       * else is plain text, and can be drawn directly
       * using blit_line(). */
#ifdef HAVE_LANGEXTRA
      if (     string_is_equal(key_text, "\xe2\x87\xa6")) /* backspace character */
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_BACKSPACE,
               rgui->colors.normal_color, rgui->colors.shadow_color);
      else if (string_is_equal(key_text, "\xe2\x8f\x8e")) /* return character */
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_ENTER,
               rgui->colors.normal_color, rgui->colors.shadow_color);
      else if (string_is_equal(key_text, "\xe2\x87\xa7")) /* up arrow */
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_SHIFT_UP,
               rgui->colors.normal_color, rgui->colors.shadow_color);
      else if (string_is_equal(key_text, "\xe2\x87\xa9")) /* down arrow */
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_SHIFT_DOWN,
               rgui->colors.normal_color, rgui->colors.shadow_color);
      else if (string_is_equal(key_text, "\xe2\x8a\x95")) /* plus sign (next button) */
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_NEXT,
               rgui->colors.normal_color, rgui->colors.shadow_color);
#else
      if (     string_is_equal(key_text, "Bksp"))
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_BACKSPACE,
               rgui->colors.normal_color, rgui->colors.shadow_color);
      else if (string_is_equal(key_text, "Enter"))
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_ENTER,
               rgui->colors.normal_color, rgui->colors.shadow_color);
      else if (string_is_equal(key_text, "Upper"))
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_SHIFT_UP,
               rgui->colors.normal_color, rgui->colors.shadow_color);
      else if (string_is_equal(key_text, "Lower"))
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_SHIFT_DOWN,
               rgui->colors.normal_color, rgui->colors.shadow_color);
      else if (string_is_equal(key_text, "Next"))
         blit_symbol(fb_width, key_text_x, key_text_y, RGUI_SYMBOL_NEXT,
               rgui->colors.normal_color, rgui->colors.shadow_color);
#endif
      else
         blit_line(fb_width, key_text_x, key_text_y, key_text,
               rgui->colors.normal_color, rgui->colors.shadow_color);
      
      /* Draw selection pointer */
      if (key_index == osk_ptr)
      {
         unsigned osk_ptr_x = osk_x + keyboard_offset_x + ptr_offset_x + (key_column * key_width);
         unsigned osk_ptr_y = osk_y + keyboard_offset_y + ptr_offset_y + (key_row    * key_height);
         
         /* Draw drop shadow, if required */
         if (rgui->shadow_enable)
         {
            rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
                  osk_ptr_x + 1, osk_ptr_y + 1, 1, ptr_height, rgui->colors.shadow_color);
            rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
                  osk_ptr_x + 1, osk_ptr_y + 1, ptr_width, 1, rgui->colors.shadow_color);
            rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
                  osk_ptr_x + ptr_width, osk_ptr_y + 1, 1, ptr_height, rgui->colors.shadow_color);
            rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
                  osk_ptr_x + 1, osk_ptr_y + ptr_height, ptr_width, 1, rgui->colors.shadow_color);
         }
         
         /* Draw selection rectangle */
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               osk_ptr_x, osk_ptr_y, 1, ptr_height, rgui->colors.hover_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               osk_ptr_x, osk_ptr_y, ptr_width, 1, rgui->colors.hover_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               osk_ptr_x + ptr_width - 1, osk_ptr_y, 1, ptr_height, rgui->colors.hover_color);
         rgui_color_rect(rgui_frame_buf.data, fb_width, fb_height,
               osk_ptr_x, osk_ptr_y + ptr_height - 1, ptr_width, 1, rgui->colors.hover_color);
      }
   }
}

#if defined(GEKKO)
/* Need to forward declare this for the Wii build
 * (I'm not going to reorder the functions and mess
 * up the git diff for a single platform...) */
static bool rgui_set_aspect_ratio(rgui_t *rgui, bool delay_update);
#endif

static void rgui_render(void *data,
      unsigned width, unsigned height,
      bool is_idle)
{
   menu_animation_ctx_ticker_t ticker;
   menu_animation_ctx_ticker_smooth_t ticker_smooth;
   static const char* const ticker_spacer = RGUI_TICKER_SPACER;
   bool use_smooth_ticker;
   unsigned x, y;
   size_t i, end, fb_pitch, old_start, new_start;
   unsigned fb_width, fb_height;
   int bottom;
   unsigned ticker_x_offset       = 0;
   size_t entries_end             = menu_entries_get_size();
   bool msg_force                 = false;
   bool fb_size_changed           = false;
   settings_t *settings           = config_get_ptr();
   rgui_t *rgui                   = (rgui_t*)data;

   static bool display_kb         = false;
   bool current_display_cb        = false;

   bool show_fs_thumbnail         =
         rgui->show_fs_thumbnail &&
         rgui->entry_has_thumbnail &&
         (fs_thumbnail.is_valid || (rgui->thumbnail_queue_size > 0));

   /* Sanity check */
   if (!rgui || !rgui_frame_buf.data || !settings)
      return;

   /* Apply pending aspect ratio update */
   if (rgui->aspect_update_pending)
   {
      command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);
      rgui->aspect_update_pending = false;
   }

   current_display_cb = menu_input_dialog_get_display_kb();

   if (!rgui->force_redraw)
   {
      msg_force = menu_display_get_msg_force();

      if (menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL)
            && !msg_force)
         return;

      if (!display_kb && !current_display_cb && (is_idle || !menu_display_get_update_pending()))
         return;
   }

   display_kb = current_display_cb;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   /* If the framebuffer changed size, or the background config has
    * changed, recache the background buffer */
   fb_size_changed = (rgui->last_width != fb_width) || (rgui->last_height != fb_height);

#if defined(GEKKO)
   /* Wii gfx driver changes menu framebuffer size at
    * will... If a change is detected, all texture buffers
    * must be regenerated - easiest way is to just call
    * rgui_set_aspect_ratio() */
   if (fb_size_changed)
      rgui_set_aspect_ratio(rgui, false);
#endif

   if (rgui->bg_modified || fb_size_changed)
   {
      rgui_cache_background(rgui);

      /* Reinitialise particle effect, if required */
      if (fb_size_changed && (rgui->particle_effect != RGUI_PARTICLE_EFFECT_NONE))
         rgui_init_particle_effect(rgui);

      rgui->last_width  = fb_width;
      rgui->last_height = fb_height;
   }

   if (rgui->bg_modified)
      rgui->bg_modified = false;

   menu_display_set_framebuffer_dirty_flag();
   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);

   rgui->force_redraw        = false;

   /* Get offset of bottommost entry */
   bottom = (int)(entries_end - rgui_term_layout.height);
   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);
   if (old_start > (unsigned)bottom)
   {
      /* MENU_ENTRIES_CTL_SET_START requires a pointer of
       * type size_t, so have to create a copy of 'bottom'
       * here to avoid memory errors... */
      size_t bottom_cpy = (size_t)bottom;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &bottom_cpy);
   }

   /* Handle pointer input
    * Note: This is ignored when showing a fullscreen thumbnail */
   if ((rgui->pointer.type != MENU_POINTER_DISABLED) &&
       rgui->pointer.active && !show_fs_thumbnail)
   {
      /* Update currently 'highlighted' item */
      if (rgui->pointer.y > rgui_term_layout.start_y)
      {
         unsigned new_ptr;
         menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

         /* Note: It's okay for this to go out of range
          * (limits are checked in rgui_pointer_up()) */
         new_ptr = (unsigned)((rgui->pointer.y - rgui_term_layout.start_y) / FONT_HEIGHT_STRIDE) + old_start;

         menu_input_set_pointer_selection(new_ptr);
      }

      /* Allow drag-scrolling if items are currently off-screen */
      if (rgui->pointer.dragged && (bottom > 0))
      {
         size_t start;
         int16_t scroll_y_max = bottom * FONT_HEIGHT_STRIDE;

         rgui->scroll_y += -1 * rgui->pointer.dy;
         rgui->scroll_y = (rgui->scroll_y < 0)            ? 0            : rgui->scroll_y;
         rgui->scroll_y = (rgui->scroll_y > scroll_y_max) ? scroll_y_max : rgui->scroll_y;

         start = rgui->scroll_y / FONT_HEIGHT_STRIDE;
         menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
      }
   }

   /* Start position may have changed - get current
    * value and determine index of last displayed entry */
   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);
   end = ((old_start + rgui_term_layout.height) <= entries_end) ?
         old_start + rgui_term_layout.height : entries_end;

   /* Do not scroll if all items are visible. */
   if (entries_end <= rgui_term_layout.height)
   {
      size_t start = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
   }

   /* Render background */
   rgui_render_background();

   /* Render particle effect, if required */
   if (rgui->particle_effect != RGUI_PARTICLE_EFFECT_NONE)
      rgui_render_particle_effect(rgui);

   /* We use a single ticker for all text animations,
    * with the following configuration: */
   use_smooth_ticker = settings->bools.menu_ticker_smooth;
   if (use_smooth_ticker)
   {
      ticker_smooth.idx           = menu_animation_get_ticker_pixel_idx();
      ticker_smooth.font          = NULL;
      ticker_smooth.glyph_width   = FONT_WIDTH_STRIDE;
      ticker_smooth.type_enum     = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
      ticker_smooth.spacer        = ticker_spacer;
      ticker_smooth.dst_str_width = NULL;
   }
   else
   {
      ticker.idx       = menu_animation_get_ticker_idx();
      ticker.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
      ticker.spacer    = ticker_spacer;
   }

   /* Note: On-screen keyboard takes precedence over
    * normal menu thumbnail/text list display modes */
   if (current_display_cb)
      rgui_render_osk(rgui, &ticker, &ticker_smooth, use_smooth_ticker);
   else if (show_fs_thumbnail)
   {
      /* If fullscreen thumbnails are enabled and we are viewing a playlist,
       * switch to fullscreen thumbnail view mode if either current thumbnail
       * is valid or we are waiting for current thumbnail to load
       * (if load is pending we'll get a blank screen + title, but
       * this is better than switching back to the text playlist
       * view, which causes ugly flickering when scrolling quickly
       * through a list...) */
      const char *thumbnail_title = NULL;
      char thumbnail_title_buf[255];
      unsigned title_x, title_width;
      thumbnail_title_buf[0] = '\0';

      /* Draw thumbnail */
      rgui_render_fs_thumbnail(rgui);

      /* Get thumbnail title */
      if (menu_thumbnail_get_label(rgui->thumbnail_path_data, &thumbnail_title))
      {
         /* Format thumbnail title */
         if (use_smooth_ticker)
         {
            ticker_smooth.selected    = true;
            ticker_smooth.field_width = (rgui_term_layout.width - 10) * FONT_WIDTH_STRIDE;
            ticker_smooth.src_str     = thumbnail_title;
            ticker_smooth.dst_str     = thumbnail_title_buf;
            ticker_smooth.dst_str_len = sizeof(thumbnail_title_buf);
            ticker_smooth.x_offset    = &ticker_x_offset;

            /* If title is scrolling, then width == field_width */
            if (menu_animation_ticker_smooth(&ticker_smooth))
               title_width            = ticker_smooth.field_width;
            else
               title_width            = (unsigned)(utf8len(thumbnail_title_buf) * FONT_WIDTH_STRIDE);
         }
         else
         {
            ticker.s        = thumbnail_title_buf;
            ticker.len      = rgui_term_layout.width - 10;
            ticker.str      = thumbnail_title;
            ticker.selected = true;

            menu_animation_ticker(&ticker);

            title_width     = (unsigned)(utf8len(thumbnail_title_buf) * FONT_WIDTH_STRIDE);
         }

         title_x = rgui_term_layout.start_x + ((rgui_term_layout.width * FONT_WIDTH_STRIDE) - title_width) / 2;

         /* Draw thumbnail title background */
         rgui_fill_rect(rgui_frame_buf.data, fb_width, fb_height,
               title_x - 5, 0, title_width + 10, FONT_HEIGHT_STRIDE,
               rgui->colors.bg_dark_color, rgui->colors.bg_light_color, rgui->bg_thickness);

         /* Draw thumbnail title */
         blit_line(fb_width, ticker_x_offset + title_x, 0, thumbnail_title_buf,
               rgui->colors.hover_color, rgui->colors.shadow_color);
      }
   }
   else
   {
      /* Render usual text */
      size_t selection               = menu_navigation_get_selection();
      char title_buf[255];
      size_t title_max_len;
      size_t title_len;
      unsigned title_x;
      unsigned title_y               = rgui_term_layout.start_y - FONT_HEIGHT_STRIDE;
      unsigned term_end_x            = rgui_term_layout.start_x + (rgui_term_layout.width * FONT_WIDTH_STRIDE);
      unsigned timedate_x            = term_end_x - (5 * FONT_WIDTH_STRIDE);
      unsigned core_name_len         = ((timedate_x - rgui_term_layout.start_x) / FONT_WIDTH_STRIDE) - 3;
      bool show_mini_thumbnails      = rgui->is_playlist && settings->bools.menu_rgui_inline_thumbnails;
      bool show_thumbnail            = false;
      bool show_left_thumbnail       = false;
      unsigned thumbnail_panel_width = 0;
      unsigned term_mid_point        = 0;
      size_t powerstate_len          = 0;

      /* Cache mini thumbnail related parameters, if required */
      if (show_mini_thumbnails)
      {
         /* Get whether each thumbnail type is enabled */
         show_thumbnail = rgui->entry_has_thumbnail &&
               (mini_thumbnail.is_valid || (rgui->thumbnail_queue_size > 0));
         show_left_thumbnail = rgui->entry_has_left_thumbnail &&
               (mini_left_thumbnail.is_valid || (rgui->left_thumbnail_queue_size > 0));

         /* Get maximum width of thumbnail 'panel' on right side
          * of screen */
         thumbnail_panel_width = rgui_get_mini_thumbnail_fullwidth();

         if ((rgui->entry_has_thumbnail && rgui->thumbnail_queue_size > 0) ||
             (rgui->entry_has_left_thumbnail && rgui->left_thumbnail_queue_size > 0))
            thumbnail_panel_width = mini_thumbnail_max_width;

         /* Index (relative to first displayed menu entry) of
          * the vertical centre of RGUI's 'terminal'
          * (required to determine whether a particular entry
          * is adjacent to the 'right' or 'left' thumbnail) */
         term_mid_point = (unsigned)((rgui_term_layout.height * 0.5f) + 0.5f) - 1;
      }

      /* Show battery indicator, if required */
      if (settings->bools.menu_battery_level_enable)
      {
         menu_display_ctx_powerstate_t powerstate;
         char percent_str[12];

         percent_str[0] = '\0';

         powerstate.s   = percent_str;
         powerstate.len = sizeof(percent_str);

         menu_display_powerstate(&powerstate);

         if (powerstate.battery_enabled)
         {
            powerstate_len = strlen(percent_str);

            if (powerstate_len > 0)
            {
               unsigned powerstate_x;
               enum rgui_symbol_type powerstate_symbol;
               uint16_t powerstate_color = (powerstate.percent > BATTERY_WARN_THRESHOLD || powerstate.charging) ?
                     rgui->colors.title_color : rgui->colors.hover_color;

               if (powerstate.charging)
                  powerstate_symbol = RGUI_SYMBOL_CHARGING;
               else
               {
                  if (powerstate.percent > 80)
                     powerstate_symbol = RGUI_SYMBOL_BATTERY_100;
                  else if (powerstate.percent > 60)
                     powerstate_symbol = RGUI_SYMBOL_BATTERY_80;
                  else if (powerstate.percent > 40)
                     powerstate_symbol = RGUI_SYMBOL_BATTERY_60;
                  else if (powerstate.percent > 20)
                     powerstate_symbol = RGUI_SYMBOL_BATTERY_40;
                  else
                     powerstate_symbol = RGUI_SYMBOL_BATTERY_20;
               }

               /* Note: percent symbol is particularly hideous when
                * drawn using RGUI's bitmap font, so strip it off the
                * end of the output string... */
               powerstate_len--;
               percent_str[powerstate_len] = '\0';

               powerstate_len += 2;
               powerstate_x    = (unsigned)(term_end_x - (powerstate_len * FONT_WIDTH_STRIDE));

               /* Draw symbol */
               blit_symbol(fb_width, powerstate_x, title_y, powerstate_symbol,
                           powerstate_color, rgui->colors.shadow_color);

               /* Print text */
               blit_line(fb_width, powerstate_x + (2 * FONT_WIDTH_STRIDE), title_y,
                         percent_str, powerstate_color, rgui->colors.shadow_color);

               /* Final length of battery indicator is 'powerstate_len' + a
                * spacer of 3 characters */
               powerstate_len += 3;
            }
         }
      }

      /* Print title */
      title_max_len = rgui_term_layout.width - 5 - (powerstate_len > 5 ? powerstate_len : 5);
      title_buf[0] = '\0';

      if (use_smooth_ticker)
      {
         ticker_smooth.selected    = true;
         ticker_smooth.field_width = title_max_len * FONT_WIDTH_STRIDE;
         ticker_smooth.src_str     = rgui->menu_title;
         ticker_smooth.dst_str     = title_buf;
         ticker_smooth.dst_str_len = sizeof(title_buf);
         ticker_smooth.x_offset    = &ticker_x_offset;

         /* If title is scrolling, then title_len == title_max_len */
         if (menu_animation_ticker_smooth(&ticker_smooth))
            title_len = title_max_len;
         else
            title_len = utf8len(title_buf);
      }
      else
      {
         ticker.s        = title_buf;
         ticker.len      = title_max_len;
         ticker.str      = rgui->menu_title;
         ticker.selected = true;

         menu_animation_ticker(&ticker);

         title_len = utf8len(title_buf);
      }

      string_to_upper(title_buf);

      title_x = ticker_x_offset + rgui_term_layout.start_x +
                (rgui_term_layout.width - title_len) * FONT_WIDTH_STRIDE / 2;

      /* Title is always centred, unless it is long enough
       * to infringe upon the battery indicator, in which case
       * we shift it to the left */
      if (powerstate_len > 5)
         if (title_len > title_max_len - (powerstate_len - 5))
            title_x -= (powerstate_len - 5) * FONT_WIDTH_STRIDE / 2;

      blit_line(fb_width, title_x, title_y,
            title_buf, rgui->colors.title_color, rgui->colors.shadow_color);

      /* Print menu entries */
      x = rgui_term_layout.start_x;
      y = rgui_term_layout.start_y;

      menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &new_start);

      for (i = new_start; i < end; i++, y += FONT_HEIGHT_STRIDE)
      {
         char entry_title_buf[255];
         char type_str_buf[255];
         menu_entry_t entry;
         const char *entry_label               = NULL;
         const char *entry_value               = NULL;
         size_t entry_title_max_len            = 0;
         unsigned entry_value_len              = 0;
         bool entry_selected                   = (i == selection);
         uint16_t entry_color                  = entry_selected ?
               rgui->colors.hover_color : rgui->colors.normal_color;

         if (i > (selection + 100))
            continue;

         entry_title_buf[0] = '\0';
         type_str_buf[0]    = '\0';

         /* Get current entry */
         menu_entry_init(&entry);
         entry.path_enabled     = false;
         entry.label_enabled    = false;
         entry.sublabel_enabled = false;
         menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

         /* Read entry parameters */
         menu_entry_get_rich_label(&entry, &entry_label);
         menu_entry_get_value(&entry, &entry_value);

         /* Get base length of entry title field */
         entry_title_max_len = rgui_term_layout.width - (1 + 2);

         /* If showing mini thumbnails, reduce title field length accordingly */
         if (show_mini_thumbnails)
         {
            unsigned term_offset = settings->bools.menu_rgui_swap_thumbnails ?
                  (unsigned)(rgui_term_layout.height - (i - new_start) - 1) : (i - new_start);
            unsigned thumbnail_width = 0;

            /* Note:
             * - 'Right' thumbnail is drawn at the top
             * - 'Left' thumbnail is drawn at the bottom
             * ...unless thumbnail postions are swapped.
             * (legacy naming, unfortunately...) */

            /* An annoyance - cannot assume terminal will have a
             * standard layout (even though it always will...),
             * so have to check whether there are an odd or even
             * number of entries... */
            if((rgui_term_layout.height & 1) == 0)
            {
               /* Even number of entries */
               if ((show_thumbnail      && (term_offset <= term_mid_point)) ||
                   (show_left_thumbnail && (term_offset >  term_mid_point)))
                  thumbnail_width = thumbnail_panel_width;
            }
            else
            {
               /* Odd number of entries (will always be the case) */
               if ((show_thumbnail      && (term_offset < term_mid_point)) ||
                   (show_left_thumbnail && (term_offset > term_mid_point)) ||
                   ((show_thumbnail || show_left_thumbnail) && (term_offset == term_mid_point)))
                  thumbnail_width = thumbnail_panel_width;
            }

            entry_title_max_len -= (thumbnail_width / FONT_WIDTH_STRIDE) + 1;
         }

         /* Determine whether entry has a value component */
         if (!string_is_empty(entry_value))
         {
            if (settings->bools.menu_rgui_full_width_layout)
            {
               /* Resize fields according to actual length of value string */
               entry_value_len = (unsigned)strlen(entry_value);
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

         /* Format entry title string */
         if (use_smooth_ticker)
         {
            ticker_smooth.selected    = entry_selected;
            ticker_smooth.field_width = entry_title_max_len * FONT_WIDTH_STRIDE;
            ticker_smooth.src_str     = entry_label;
            ticker_smooth.dst_str     = entry_title_buf;
            ticker_smooth.dst_str_len = sizeof(entry_title_buf);
            ticker_smooth.x_offset    = &ticker_x_offset;

            menu_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.s        = entry_title_buf;
            ticker.len      = entry_title_max_len;
            ticker.str      = entry_label;
            ticker.selected = entry_selected;

            menu_animation_ticker(&ticker);
         }

         /* Print entry title */
         blit_line(fb_width, ticker_x_offset + x + (2 * FONT_WIDTH_STRIDE), y,
               entry_title_buf,
               entry_color, rgui->colors.shadow_color);

         /* Print entry value, if required */
         if (entry_value_len > 0)
         {
            /* Format entry value string */
            if (use_smooth_ticker)
            {
               ticker_smooth.field_width = entry_value_len * FONT_WIDTH_STRIDE;
               ticker_smooth.src_str     = entry_value;
               ticker_smooth.dst_str     = type_str_buf;
               ticker_smooth.dst_str_len = sizeof(type_str_buf);
               ticker_smooth.x_offset    = &ticker_x_offset;

               menu_animation_ticker_smooth(&ticker_smooth);
            }
            else
            {
               ticker.s        = type_str_buf;
               ticker.len      = entry_value_len;
               ticker.str      = entry_value;

               menu_animation_ticker(&ticker);
            }

            /* Print entry value */
            blit_line(fb_width, ticker_x_offset + term_end_x - ((entry_value_len + 1) * FONT_WIDTH_STRIDE), y,
                  type_str_buf,
                  entry_color, rgui->colors.shadow_color);
         }
         /* Print marker for currently selected item in
          * drop down lists, if required */
         else if (entry.checked)
            blit_symbol(fb_width, x + FONT_WIDTH_STRIDE, y, RGUI_SYMBOL_CHECKMARK,
                  entry_color, rgui->colors.shadow_color);

         /* Print selection marker, if required */
         if (entry_selected)
            blit_line(fb_width, x, y, ">",
                  entry_color, rgui->colors.shadow_color);
      }

      /* Draw mini thumbnails, if required */
      if (show_mini_thumbnails)
      {
         if (show_thumbnail)
            rgui_render_mini_thumbnail(rgui, &mini_thumbnail, MENU_THUMBNAIL_RIGHT);
         
         if (show_left_thumbnail)
            rgui_render_mini_thumbnail(rgui, &mini_left_thumbnail, MENU_THUMBNAIL_LEFT);
      }

      /* Print menu sublabel/core name (if required) */
      if (settings->bools.menu_show_sublabels && !string_is_empty(rgui->menu_sublabel))
      {
         char sublabel_buf[MENU_SUBLABEL_MAX_LENGTH];
         sublabel_buf[0] = '\0';

         if (use_smooth_ticker)
         {
            ticker_smooth.selected    = true;
            ticker_smooth.field_width = core_name_len * FONT_WIDTH_STRIDE;
            ticker_smooth.src_str     = rgui->menu_sublabel;
            ticker_smooth.dst_str     = sublabel_buf;
            ticker_smooth.dst_str_len = sizeof(sublabel_buf);
            ticker_smooth.x_offset    = &ticker_x_offset;

            menu_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.s        = sublabel_buf;
            ticker.len      = core_name_len;
            ticker.str      = rgui->menu_sublabel;
            ticker.selected = true;

            menu_animation_ticker(&ticker);
         }

         blit_line(
               fb_width,
               ticker_x_offset + rgui_term_layout.start_x + FONT_WIDTH_STRIDE,
               (rgui_term_layout.height * FONT_HEIGHT_STRIDE) +
               rgui_term_layout.start_y + 2, sublabel_buf,
               rgui->colors.hover_color, rgui->colors.shadow_color);
      }
      else if (settings->bools.menu_core_enable)
      {
         char core_title[64];
         char core_title_buf[64];
         core_title[0] = core_title_buf[0] = '\0';

         menu_entries_get_core_title(core_title, sizeof(core_title));

         if (use_smooth_ticker)
         {
            ticker_smooth.selected    = true;
            ticker_smooth.field_width = core_name_len * FONT_WIDTH_STRIDE;
            ticker_smooth.src_str     = core_title;
            ticker_smooth.dst_str     = core_title_buf;
            ticker_smooth.dst_str_len = sizeof(core_title_buf);
            ticker_smooth.x_offset    = &ticker_x_offset;

            menu_animation_ticker_smooth(&ticker_smooth);
         }
         else
         {
            ticker.s        = core_title_buf;
            ticker.len      = core_name_len;
            ticker.str      = core_title;
            ticker.selected = true;

            menu_animation_ticker(&ticker);
         }

         blit_line(
               fb_width,
               ticker_x_offset + rgui_term_layout.start_x + FONT_WIDTH_STRIDE,
               (rgui_term_layout.height * FONT_HEIGHT_STRIDE) +
               rgui_term_layout.start_y + 2, core_title_buf,
               rgui->colors.hover_color, rgui->colors.shadow_color);
      }

      /* Print clock (if required) */
      if (settings->bools.menu_timedate_enable)
      {
         menu_display_ctx_datetime_t datetime;
         char timedate[16];

         timedate[0] = '\0';

         datetime.s = timedate;
         datetime.len = sizeof(timedate);
         datetime.time_mode = MENU_TIMEDATE_STYLE_HM;

         menu_display_timedate(&datetime);

         blit_line(
               fb_width,
               timedate_x,
               (rgui_term_layout.height * FONT_HEIGHT_STRIDE) +
               rgui_term_layout.start_y + 2, timedate,
               rgui->colors.hover_color, rgui->colors.shadow_color);
      }
   }

   if (!string_is_empty(rgui->msgbox))
   {
      rgui_render_messagebox(rgui, rgui->msgbox);
      rgui->msgbox[0]    = '\0';
      rgui->force_redraw = true;
   }

   if (rgui->mouse_show)
   {
      bool cursor_visible  = settings->bools.video_fullscreen ||
         !video_driver_has_windowed();

      if (settings->bools.menu_mouse_enable && cursor_visible)
         rgui_blit_cursor(rgui);
   }
}

static void rgui_framebuffer_free(void)
{
   rgui_frame_buf.width = 0;
   rgui_frame_buf.height = 0;
   
   if (rgui_frame_buf.data)
      free(rgui_frame_buf.data);
   rgui_frame_buf.data = NULL;
}

static void rgui_background_free(void)
{
   rgui_background_buf.width = 0;
   rgui_background_buf.height = 0;
   
   if (rgui_background_buf.data)
      free(rgui_background_buf.data);
   rgui_background_buf.data = NULL;
}

static void rgui_thumbnail_free(thumbnail_t *thumbnail)
{
   if (!thumbnail)
      return;
   
   thumbnail->max_width = 0;
   thumbnail->max_height = 0;
   thumbnail->width = 0;
   thumbnail->height = 0;
   thumbnail->is_valid = false;
   thumbnail->path[0] = '\0';
   
   if (thumbnail->data)
      free(thumbnail->data);
   thumbnail->data = NULL;
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

static void rgui_set_video_config(rgui_t *rgui, rgui_video_settings_t *video_settings, bool delay_update)
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
   
   if (delay_update)
      rgui->aspect_update_pending = true;
   else
   {
      command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);
      rgui->aspect_update_pending = false;
   }
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
#if defined(GEKKO)
      /* The Wii is a special case, since it uses anamorphic
       * widescreen. The display aspect ratio cannot therefore
       * be determined simply by dividing viewport width by height */
#ifdef HW_RVL
      float device_aspect  = (CONF_GetAspectRatio() == CONF_ASPECT_4_3) ?
            (4.0f / 3.0f) : (16.0f / 9.0f);
#else
      float device_aspect  = (4.0f / 3.0f);
#endif
      float desired_aspect = (float)fb_width / (float)fb_height;
      float delta;
      
      if (device_aspect > desired_aspect)
      {
         delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
         rgui->menu_video_settings.viewport.width  = (unsigned)(2.0f * (float)vp.full_width * delta);
         rgui->menu_video_settings.viewport.height = vp.full_height;
      }
      else
      {
         delta = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
         rgui->menu_video_settings.viewport.height = (unsigned)(2.0 * vp.full_height * delta);
         rgui->menu_video_settings.viewport.width  = vp.full_width;
      }
#else
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
#endif
      
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

static bool rgui_set_aspect_ratio(rgui_t *rgui, bool delay_update)
{
   unsigned base_term_width;
   settings_t *settings = config_get_ptr();
   
   rgui_framebuffer_free();
   rgui_background_free();
   rgui_thumbnail_free(&fs_thumbnail);
   rgui_thumbnail_free(&mini_thumbnail);
   rgui_thumbnail_free(&mini_left_thumbnail);
   
   /* Cache new aspect ratio */
   rgui->menu_aspect_ratio = settings->uints.menu_rgui_aspect_ratio;
   
#if defined(GEKKO)
   {
      size_t fb_pitch;
      unsigned fb_width, fb_height;
      
      menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);
      
      /* Set frame buffer dimensions */
      rgui_frame_buf.height = fb_height;
      switch (rgui->menu_aspect_ratio)
      {
         /* Note: Maximum Wii framebuffer width is 424, not
          * the usual 426, since the last two bits of the
          * width value must be zero... */
         case RGUI_ASPECT_RATIO_16_9:
            if (rgui_frame_buf.height == 240)
               rgui_frame_buf.width = 424;
            else
               rgui_frame_buf.width = (unsigned)((16.0f / 9.0f) * (float)rgui_frame_buf.height) & ~3;
            base_term_width = rgui_frame_buf.width;
            break;
         case RGUI_ASPECT_RATIO_16_9_CENTRE:
            if (rgui_frame_buf.height == 240)
            {
               rgui_frame_buf.width = 424;
               base_term_width      = 320;
            }
            else
            {
               rgui_frame_buf.width = (unsigned)((16.0f / 9.0f) * (float)rgui_frame_buf.height) & ~3;
               base_term_width      = (unsigned)(( 4.0f / 3.0f) * (float)rgui_frame_buf.height) & ~3;
            }
            break;
         case RGUI_ASPECT_RATIO_16_10:
            if (rgui_frame_buf.height == 240)
               rgui_frame_buf.width = 384;
            else
               rgui_frame_buf.width = (unsigned)((16.0f / 10.0f) * (float)rgui_frame_buf.height) & ~3;
            base_term_width = rgui_frame_buf.width;
            break;
         case RGUI_ASPECT_RATIO_16_10_CENTRE:
            if (rgui_frame_buf.height == 240)
            {
               rgui_frame_buf.width = 384;
               base_term_width      = 320;
            }
            else
            {
               rgui_frame_buf.width = (unsigned)((16.0f / 10.0f) * (float)rgui_frame_buf.height) & ~3;
               base_term_width      = (unsigned)(( 4.0f / 3.0f)  * (float)rgui_frame_buf.height) & ~3;
            }
            break;
         default:
            /* 4:3 */
            if (rgui_frame_buf.height == 240)
               rgui_frame_buf.width = 320;
            else
               rgui_frame_buf.width = (unsigned)(( 4.0f / 3.0f)  * (float)rgui_frame_buf.height) & ~3;
            base_term_width = rgui_frame_buf.width;
            break;
      }
   }
#else
   /* Set frame buffer dimensions */
   rgui_frame_buf.height = 240;
   switch (rgui->menu_aspect_ratio)
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
#endif
   
   /* Allocate frame buffer */
   rgui_frame_buf.data = (uint16_t*)calloc(
         rgui_frame_buf.width * rgui_frame_buf.height, sizeof(uint16_t));
   
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
   
   /* Allocate background buffer */
   rgui_background_buf.width = rgui_frame_buf.width;
   rgui_background_buf.height = rgui_frame_buf.height;
   rgui_background_buf.data = (uint16_t*)calloc(
         rgui_background_buf.width * rgui_background_buf.height, sizeof(uint16_t));
   
   if (!rgui_background_buf.data)
      return false;
   
   /* Allocate thumbnail buffer */
   fs_thumbnail.max_width = rgui_frame_buf.width;
   fs_thumbnail.max_height = rgui_frame_buf.height;
   fs_thumbnail.data = (uint16_t*)calloc(
         fs_thumbnail.max_width * fs_thumbnail.max_height, sizeof(uint16_t));
   if (!fs_thumbnail.data)
      return false;
   
   /* Allocate mini thumbnail buffers */
   mini_thumbnail_max_width = ((rgui_term_layout.width - 4) > 19 ? 19 : (rgui_term_layout.width - 4)) * FONT_WIDTH_STRIDE;
   mini_thumbnail_max_height = (unsigned)((rgui_term_layout.height * FONT_HEIGHT_STRIDE) * 0.5f) - 2;
   
   mini_thumbnail.max_width = mini_thumbnail_max_width;
   mini_thumbnail.max_height = mini_thumbnail_max_height;
   mini_thumbnail.data = (uint16_t*)calloc(
         mini_thumbnail.max_width * mini_thumbnail.max_height, sizeof(uint16_t));
   if (!mini_thumbnail.data)
      return false;
   
   mini_left_thumbnail.max_width = mini_thumbnail_max_width;
   mini_left_thumbnail.max_height = mini_thumbnail_max_height;
   mini_left_thumbnail.data = (uint16_t*)calloc(
         mini_left_thumbnail.max_width * mini_left_thumbnail.max_height, sizeof(uint16_t));
   if (!mini_left_thumbnail.data)
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
      rgui_set_video_config(rgui, &rgui->menu_video_settings, delay_update);
   }
   
   return true;
}

static void *rgui_init(void **userdata, bool video_is_threaded)
{
   unsigned new_font_height;
   size_t start;
   struct video_viewport vp;
   rgui_t               *rgui = NULL;
   settings_t *settings       = config_get_ptr();
   menu_handle_t        *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   rgui = (rgui_t*)calloc(1, sizeof(rgui_t));

   if (!rgui)
      goto error;

   *userdata              = rgui;

#if defined(HAVE_MENU_WIDGETS)
   /* We have to be somewhat careful here, since some
    * platforms do not like video_driver_texture-related
    * operations (e.g. 3DS). We would hope that these
    * platforms will always have HAVE_MENU_WIDGETS disabled,
    * but for extra safety we will only permit menu widget
    * additions when the current gfx driver reports that it
    * has widget support */
   rgui->widgets_supported = menu_widgets_ready();

   if (rgui->widgets_supported)
   {
      if (!menu_display_init_first_driver(video_is_threaded))
         goto error;

      menu_display_allocate_white_texture();
   }
#endif

   rgui->menu_title[0] = '\0';
   rgui->menu_sublabel[0] = '\0';

   /* Set pixel format conversion function */
   rgui_set_pixel_format_function();

   /* Cache initial video settings */
   rgui_get_video_config(&rgui->content_video_settings);

   /* Set aspect ratio
    * - Allocates frame buffer
    * - Configures variable 'menu display' settings */
   rgui->menu_aspect_ratio_lock = settings->uints.menu_rgui_aspect_ratio_lock;
   rgui->aspect_update_pending = false;
   if (!rgui_set_aspect_ratio(rgui, false))
      goto error;

   /* Fixed 'menu display' settings */
   new_font_height = FONT_HEIGHT_STRIDE * 2;
   menu_display_set_header_height(new_font_height);

   /* Prepare RGUI colors, to improve performance */
   rgui->theme_preset_path[0] = '\0';
   prepare_rgui_colors(rgui, settings);

   start = 0;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
   rgui->scroll_y = 0;

   rgui_init_font_lut();

   rgui->bg_thickness          = settings->bools.menu_rgui_background_filler_thickness_enable;
   rgui->border_thickness      = settings->bools.menu_rgui_border_filler_thickness_enable;
   rgui->border_enable         = settings->bools.menu_rgui_border_filler_enable;
   rgui->shadow_enable         = settings->bools.menu_rgui_shadows;
   rgui->particle_effect       = settings->uints.menu_rgui_particle_effect;
   rgui->extended_ascii_enable = settings->bools.menu_rgui_extended_ascii;

   rgui->last_width  = rgui_frame_buf.width;
   rgui->last_height = rgui_frame_buf.height;

   /* Get initial 'window' dimensions */
   video_driver_get_viewport_info(&vp);
   rgui->window_width         = vp.full_width;
   rgui->window_height        = vp.full_height;
   rgui->ignore_resize_events = false;

   /* Initialise particle effect, if required */
   if (rgui->particle_effect != RGUI_PARTICLE_EFFECT_NONE)
      rgui_init_particle_effect(rgui);

   /* Set initial 'blit_line/symbol' functions */
   rgui_set_blit_functions(
         settings->bools.menu_rgui_shadows, settings->bools.menu_rgui_extended_ascii);

   rgui->thumbnail_path_data = menu_thumbnail_path_init();
   if (!rgui->thumbnail_path_data)
      goto error;

   rgui->thumbnail_queue_size = 0;
   rgui->left_thumbnail_queue_size = 0;
   rgui->thumbnail_load_pending = false;
   rgui->thumbnail_load_trigger_time = 0;
   /* Ensure that we start with fullscreen thumbnails disabled */
   rgui->show_fs_thumbnail = false;

   /* Ensure that pointer device starts with well defined
    * values (shoult not be necessary, but some platforms may
    * not handle struct initialisation correctly...) */
   memset(&rgui->pointer, 0, sizeof(menu_input_pointer_t));

   return menu;

error:
   rgui_framebuffer_free();
   rgui_background_free();
   rgui_thumbnail_free(&fs_thumbnail);
   rgui_thumbnail_free(&mini_thumbnail);
   rgui_thumbnail_free(&mini_left_thumbnail);
   if (menu)
      free(menu);
   return NULL;
}

static void rgui_free(void *data)
{
   rgui_t *rgui = (rgui_t*)data;

   if (rgui)
   {
      if (rgui->thumbnail_path_data)
         free(rgui->thumbnail_path_data);
   }

   rgui_framebuffer_free();
   rgui_background_free();
   rgui_thumbnail_free(&fs_thumbnail);
   rgui_thumbnail_free(&mini_thumbnail);
   rgui_thumbnail_free(&mini_left_thumbnail);

   if (rgui_upscale_buf.data)
   {
      free(rgui_upscale_buf.data);
      rgui_upscale_buf.data = NULL;
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
   menu_thumbnail_set_system(
         rgui->thumbnail_path_data, s, playlist_get_cached());
}

static void rgui_get_thumbnail_system(void *userdata, char *s, size_t len)
{
   rgui_t *rgui       = (rgui_t*)userdata;
   const char *system = NULL;
   if (!rgui)
      return;
   if (menu_thumbnail_get_system(rgui->thumbnail_path_data, &system))
      strlcpy(s, system, len);
}

static void rgui_load_current_thumbnails(rgui_t *rgui, bool download_missing)
{
   const char *thumbnail_path      = NULL;
   const char *left_thumbnail_path = NULL;
   bool thumbnails_missing         = false;
   
   /* Right (or fullscreen) thumbnail */
   if (menu_thumbnail_get_path(rgui->thumbnail_path_data,
         MENU_THUMBNAIL_RIGHT, &thumbnail_path))
   {
      rgui->entry_has_thumbnail = request_thumbnail(
            rgui->show_fs_thumbnail ? &fs_thumbnail : &mini_thumbnail,
            MENU_THUMBNAIL_RIGHT,
            &rgui->thumbnail_queue_size,
            thumbnail_path,
            &thumbnails_missing);
   }
   
   /* Left thumbnail
    * (Note: there is no need to load this when viewing
    * fullscreen thumbnails) */
   if (!rgui->show_fs_thumbnail)
   {
      if (menu_thumbnail_get_path(rgui->thumbnail_path_data,
            MENU_THUMBNAIL_LEFT, &left_thumbnail_path))
      {
         rgui->entry_has_left_thumbnail = request_thumbnail(
               &mini_left_thumbnail,
               MENU_THUMBNAIL_LEFT,
               &rgui->left_thumbnail_queue_size,
               left_thumbnail_path,
               &thumbnails_missing);
      }
   }
   
   /* Reset 'load pending' state */
   rgui->thumbnail_load_pending = false;
   
   /* Force a redraw (so 'entry_has_thumbnail' values are
    * applied immediately) */
   rgui->force_redraw = true;
   
#ifdef HAVE_NETWORKING
   /* On demand thumbnail downloads */
   if (thumbnails_missing && download_missing)
   {
      const char *system = NULL;

      if (menu_thumbnail_get_system(rgui->thumbnail_path_data, &system))
         task_push_pl_entry_thumbnail_download(system,
               playlist_get_cached(), (unsigned)menu_navigation_get_selection(),
               false, true);
   }
#endif
}

static void rgui_scan_selected_entry_thumbnail(rgui_t *rgui, bool force_load)
{
   bool has_thumbnail   = false;
   settings_t *settings = config_get_ptr();
   
   if (!settings)
      return;
   
   rgui->entry_has_thumbnail      = false;
   rgui->entry_has_left_thumbnail = false;
   rgui->thumbnail_load_pending   = false;
   
   /* Update thumbnail content/path */
   if ((rgui->show_fs_thumbnail || settings->bools.menu_rgui_inline_thumbnails) && rgui->is_playlist)
   {
      if (menu_thumbnail_set_content_playlist(rgui->thumbnail_path_data,
            playlist_get_cached(), menu_navigation_get_selection()))
      {
         if (menu_thumbnail_is_enabled(rgui->thumbnail_path_data, MENU_THUMBNAIL_RIGHT))
            has_thumbnail = menu_thumbnail_update_path(rgui->thumbnail_path_data, MENU_THUMBNAIL_RIGHT);
         
         if (settings->bools.menu_rgui_inline_thumbnails &&
             menu_thumbnail_is_enabled(rgui->thumbnail_path_data, MENU_THUMBNAIL_LEFT))
            has_thumbnail = menu_thumbnail_update_path(rgui->thumbnail_path_data, MENU_THUMBNAIL_LEFT) ||
                            has_thumbnail;
      }
   }
   
   /* Check whether thumbnails should be loaded */
   if (has_thumbnail)
   {
      /* Check whether thumbnails should be loaded immediately */
      if ((settings->uints.menu_rgui_thumbnail_delay == 0) || force_load)
         rgui_load_current_thumbnails(rgui, settings->bools.network_on_demand_thumbnails);
      else
      {
         /* Schedule a delayed load */
         rgui->thumbnail_load_pending = true;
         rgui->thumbnail_load_trigger_time = cpu_features_get_time_usec();
      }
   }
}

static void rgui_update_thumbnail_image(void *userdata)
{
   rgui_t *rgui         = (rgui_t*)userdata;
   settings_t *settings = config_get_ptr();
   if (!rgui || !settings)
      return;

   rgui->show_fs_thumbnail = !rgui->show_fs_thumbnail;

   /* It is possible that we are waiting for a 'right' thumbnail
    * image to load at this point. If so, and we are displaying
    * inline thumbnails, then 'fs_thumbnail' and 'mini_thumbnail'
    * can get mixed up. To avoid this, we simply 'reset' the
    * currently inactive right thumbnail. */
   if (settings->bools.menu_rgui_inline_thumbnails)
   {
      if (rgui->show_fs_thumbnail)
      {
         mini_thumbnail.width = 0;
         mini_thumbnail.height = 0;
         mini_thumbnail.is_valid = false;
         mini_thumbnail.path[0] = '\0';
      }
      else
      {
         fs_thumbnail.width = 0;
         fs_thumbnail.height = 0;
         fs_thumbnail.is_valid = false;
         fs_thumbnail.path[0] = '\0';
      }
   }

   /* Note that we always load thumbnails immediately
    * when toggling via the 'scan' button (scheduling a
    * delayed load here would make for a poor user
    * experience...) */
   rgui_scan_selected_entry_thumbnail(rgui, true);
}

static void rgui_refresh_thumbnail_image(void *userdata, unsigned i)
{
   rgui_t *rgui         = (rgui_t*)userdata;
   settings_t *settings = config_get_ptr();
   if (!rgui || !settings)
      return;

   /* Only refresh thumbnails if thumbnails are enabled */
   if ((rgui->show_fs_thumbnail || settings->bools.menu_rgui_inline_thumbnails) &&
       (menu_thumbnail_is_enabled(rgui->thumbnail_path_data, MENU_THUMBNAIL_RIGHT) ||
        menu_thumbnail_is_enabled(rgui->thumbnail_path_data, MENU_THUMBNAIL_LEFT)))
   {
      /* In all cases, reset current thumbnails */
      fs_thumbnail.width = 0;
      fs_thumbnail.height = 0;
      fs_thumbnail.is_valid = false;
      fs_thumbnail.path[0] = '\0';

      mini_thumbnail.width = 0;
      mini_thumbnail.height = 0;
      mini_thumbnail.is_valid = false;
      mini_thumbnail.path[0] = '\0';

      mini_left_thumbnail.width = 0;
      mini_left_thumbnail.height = 0;
      mini_left_thumbnail.is_valid = false;
      mini_left_thumbnail.path[0] = '\0';

      /* Only load thumbnails if currently viewing a
       * playlist (note that thumbnails are loaded
       * immediately, for an optimal user experience) */
      if (rgui->is_playlist)
         rgui_scan_selected_entry_thumbnail(rgui, true);
   }
}

static void rgui_update_menu_sublabel(rgui_t *rgui)
{
   size_t selection = menu_navigation_get_selection();
   settings_t *settings = config_get_ptr();
   
   rgui->menu_sublabel[0] = '\0';
   
   if (settings->bools.menu_show_sublabels && selection < menu_entries_get_size())
   {
      menu_entry_t entry;
      const char *sublabel = NULL;
      
      menu_entry_init(&entry);
      entry.path_enabled       = false;
      entry.label_enabled      = false;
      entry.rich_label_enabled = false;
      entry.value_enabled      = false;
      menu_entry_get(&entry, 0, (unsigned)selection, NULL, true);
      
      menu_entry_get_sublabel(&entry, &sublabel);
      
      if (!string_is_empty(sublabel))
      {
         static const char* const sublabel_spacer = RGUI_TICKER_SPACER;
         struct string_list *list = NULL;
         size_t line_index;
         bool prev_line_empty = true;

         /* Sanitise sublabel
          * > Replace newline characters with standard delimiter
          * > Remove whitespace surrounding each sublabel line */
         list = string_split(sublabel, "\n");
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
   }
}

static void rgui_navigation_set(void *data, bool scroll)
{
   size_t start;
   bool do_set_start              = false;
   size_t end                     = menu_entries_get_size();
   size_t selection               = menu_navigation_get_selection();
   rgui_t *rgui = (rgui_t*)data;

   if (!rgui)
      return;

   rgui_scan_selected_entry_thumbnail(rgui, false);
   rgui_update_menu_sublabel(rgui);

   if (!scroll)
      return;

   if (selection < rgui_term_layout.height /2)
   {
      start        = 0;
      do_set_start = true;
   }
   else if (selection >= (rgui_term_layout.height /2)
         && selection < (end - rgui_term_layout.height /2))
   {
      start        = selection - rgui_term_layout.height /2;
      do_set_start = true;
   }
   else if (selection >= (end - rgui_term_layout.height /2))
   {
      start        = end - rgui_term_layout.height;
      do_set_start = true;
   }

   if (do_set_start)
   {
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
      rgui->scroll_y = start * FONT_HEIGHT_STRIDE;
   }
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
                       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST));
   
   /* Set menu title */
   menu_entries_get_title(rgui->menu_title, sizeof(rgui->menu_title));
   
   /* Cancel any pending thumbnail load operations */
   rgui->thumbnail_load_pending = false;
   
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
         {
            rgui_set_video_config(rgui, &rgui->content_video_settings, false);
            /* Menu viewport has been overridden - must ignore
             * resize events until the menu is next toggled off */
            rgui->ignore_resize_events = true;
         }
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

static int rgui_pointer_up(void *data,
      unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   rgui_t *rgui           = (rgui_t*)data;
   unsigned header_height = menu_display_get_header_height();
   size_t selection       = menu_navigation_get_selection();
   bool show_fs_thumbnail = false;

   if (!rgui)
      return -1;

   show_fs_thumbnail =
         rgui->show_fs_thumbnail &&
         rgui->entry_has_thumbnail &&
         (fs_thumbnail.is_valid || (rgui->thumbnail_queue_size > 0));

   switch (gesture)
   {
      case MENU_INPUT_GESTURE_TAP:
      case MENU_INPUT_GESTURE_SHORT_PRESS:
         {
            /* Normal pointer input */
            if (show_fs_thumbnail)
            {
               /* If we are currently showing a fullscreen thumbnail:
                * - Must provide a mechanism for toggling it off
                * - A normal mouse press should just select the current
                *   entry (for which the thumbnail is being shown) */
               if (y < header_height)
                  rgui_update_thumbnail_image(rgui);
               else
                  return menu_entry_action(entry, selection, MENU_ACTION_SELECT);
            }
            else
            {
               if (y < header_height)
                  return menu_entry_action(entry, selection, MENU_ACTION_CANCEL);
               else if (ptr <= (menu_entries_get_size() - 1))
               {
                  /* If currently selected item matches 'pointer' value,
                   * perform a MENU_ACTION_SELECT on it */
                  if (ptr == selection)
                     return menu_entry_action(entry, selection, MENU_ACTION_SELECT);

                  /* Otherwise, just move the current selection to the
                   * 'pointer' value */
                  menu_navigation_set_selection(ptr);
                  menu_driver_navigation_set(false);
               }
            }
         }
         break;
      case MENU_INPUT_GESTURE_LONG_PRESS:
         /* 'Reset to default' action */
         if ((ptr <= (menu_entries_get_size() - 1)) &&
             (ptr == selection))
            return menu_entry_action(entry, selection, MENU_ACTION_START);
         break;
      default:
         /* Ignore input */
         break;
   }

   return 0;
}

static void rgui_frame(void *data, video_frame_info_t *video_info)
{
   rgui_t *rgui                   = (rgui_t*)data;
   settings_t *settings           = config_get_ptr();

   if (settings->bools.menu_rgui_background_filler_thickness_enable != rgui->bg_thickness)
   {
      rgui->bg_thickness = settings->bools.menu_rgui_background_filler_thickness_enable;
      rgui->bg_modified  = true;
      rgui->force_redraw = true;
   }

   if (settings->bools.menu_rgui_border_filler_thickness_enable != rgui->border_thickness)
   {
      rgui->border_thickness = settings->bools.menu_rgui_border_filler_thickness_enable;
      rgui->bg_modified      = true;
      rgui->force_redraw     = true;
   }

   if (settings->bools.menu_rgui_border_filler_enable != rgui->border_enable)
   {
      rgui->border_enable = settings->bools.menu_rgui_border_filler_enable;
      rgui->bg_modified   = true;
      rgui->force_redraw  = true;
   }

   if (settings->bools.menu_rgui_shadows != rgui->shadow_enable)
   {
      rgui_set_blit_functions(
            settings->bools.menu_rgui_shadows, settings->bools.menu_rgui_extended_ascii);

      rgui->shadow_enable = settings->bools.menu_rgui_shadows;
      rgui->bg_modified   = true;
      rgui->force_redraw  = true;
   }

   if (settings->uints.menu_rgui_particle_effect != rgui->particle_effect)
   {
      rgui->particle_effect = settings->uints.menu_rgui_particle_effect;

      if (rgui->particle_effect != RGUI_PARTICLE_EFFECT_NONE)
         rgui_init_particle_effect(rgui);

      rgui->force_redraw = true;
   }

   if (rgui->particle_effect != RGUI_PARTICLE_EFFECT_NONE)
      rgui->force_redraw = true;

   if (settings->bools.menu_rgui_extended_ascii != rgui->extended_ascii_enable)
   {
      rgui_set_blit_functions(
            settings->bools.menu_rgui_shadows, settings->bools.menu_rgui_extended_ascii);

      rgui->extended_ascii_enable = settings->bools.menu_rgui_extended_ascii;
      rgui->force_redraw          = true;
   }

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

   /* Note: both rgui_set_aspect_ratio() and rgui_set_video_config()
    * normally call command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL)
    * ## THIS CANNOT BE DONE INSIDE rgui_frame() IF THREADED VIDEO IS ENABLED ##
    * Attempting to do so creates a deadlock, and causes RetroArch to hang.
    * We therefore have to set the 'delay_update' argument, which causes
    * command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL) to be called at
    * the next instance of rgui_render() */

   /* > Check for changes in aspect ratio */
   if (settings->uints.menu_rgui_aspect_ratio != rgui->menu_aspect_ratio)
   {
      rgui_set_aspect_ratio(rgui, true);

      /* If user changes aspect ratio directly after opening
       * the video settings menu, then all bets are off - we
       * can no longer guarantee that changes to aspect ratio
       * and custom viewport settings will be preserved. So it
       * no longer makes sense to ignore resize events */
      rgui->ignore_resize_events = false;
   }

   /* > Check for changes in aspect ratio lock setting */
   if (settings->uints.menu_rgui_aspect_ratio_lock != rgui->menu_aspect_ratio_lock)
   {
      rgui->menu_aspect_ratio_lock = settings->uints.menu_rgui_aspect_ratio_lock;

      if (settings->uints.menu_rgui_aspect_ratio_lock == RGUI_ASPECT_RATIO_LOCK_NONE)
      {
         rgui_set_video_config(rgui, &rgui->content_video_settings, true);
      }
      else
      {
         rgui_update_menu_viewport(rgui);
         rgui_set_video_config(rgui, &rgui->menu_video_settings, true);

         /* As with changes in aspect ratio, if we reach this point
          * after visiting the video settings menu, resize events
          * should be monitored again */
         rgui->ignore_resize_events = false;
      }
   }

   /* > If aspect ratio is locked, have to rescale if window
    *   dimensions change */
   if ((rgui->window_width  != video_info->width) ||
       (rgui->window_height != video_info->height))
   {
      if ((settings->uints.menu_rgui_aspect_ratio_lock != RGUI_ASPECT_RATIO_LOCK_NONE) &&
          !rgui->ignore_resize_events)
      {
         rgui_update_menu_viewport(rgui);
         rgui_set_video_config(rgui, &rgui->menu_video_settings, true);
      }

      rgui->window_width  = video_info->width;
      rgui->window_height = video_info->height;
   }

   /* Handle pending thumbnail load operations */
   if (rgui->thumbnail_load_pending)
   {
      /* Check whether current 'load delay' duration has elapsed
       * Note: Delay is increased when viewing fullscreen thumbnails,
       * since the flicker when switching between playlist view and
       * fullscreen thumbnail view is incredibly jarring...) */
      if ((cpu_features_get_time_usec() - rgui->thumbnail_load_trigger_time) >=
          (settings->uints.menu_rgui_thumbnail_delay * 1000 * (rgui->show_fs_thumbnail ? 1.5f : 1.0f)))
         rgui_load_current_thumbnails(rgui, settings->bools.network_on_demand_thumbnails);
   }

   /* Read pointer input */
   if (settings->bools.menu_mouse_enable || settings->bools.menu_pointer_enable)
   {
      menu_input_get_pointer_state(&rgui->pointer);

      /* Screen must be redrawn whenever pointer is active */
      if ((rgui->pointer.type != MENU_POINTER_DISABLED) && rgui->pointer.active)
         rgui->force_redraw = true;
   }
   else
      rgui->pointer.type = MENU_POINTER_DISABLED;
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
         rgui_set_video_config(rgui, &rgui->menu_video_settings, false);
      }
      else
      {
         /* Restore content video settings *if* user
          * has not changed video settings since menu was
          * last toggled on */
         rgui_video_settings_t current_video_settings = {0};
         rgui_get_video_config(&current_video_settings);
         
         if (rgui_is_video_config_equal(&current_video_settings, &rgui->menu_video_settings))
            rgui_set_video_config(rgui, &rgui->content_video_settings, false);
         
         /* Any modified video settings have now been registered,
          * so it is again 'safe' to respond to window resize events */
         rgui->ignore_resize_events = false;
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

#if defined(HAVE_MENU_WIDGETS)
static void rgui_context_reset(void *data, bool is_threaded)
{
   rgui_t *rgui = (rgui_t*)data;

   if (!rgui)
      return;

   if (rgui->widgets_supported)
      menu_display_allocate_white_texture();
   video_driver_monitor_reset();
}

static void rgui_context_destroy(void *data)
{
   rgui_t *rgui = (rgui_t*)data;

   if (!rgui)
      return;

   if (rgui->widgets_supported)
      video_driver_texture_unload(&menu_display_white_texture);
}
#endif

menu_ctx_driver_t menu_ctx_rgui = {
   rgui_set_texture,
   rgui_set_message,
   generic_menu_iterate,
   rgui_render,
   rgui_frame,
   rgui_init,
   rgui_free,
#if defined(HAVE_MENU_WIDGETS)
   rgui_context_reset,
   rgui_context_destroy,
#else
   NULL,
   NULL,
#endif
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
   NULL,                               /* update_thumbnail_path */
   rgui_update_thumbnail_image,
   rgui_refresh_thumbnail_image,
   rgui_set_thumbnail_system,
   rgui_get_thumbnail_system,
   NULL,                               /* set_thumbnail_content */
   rgui_osk_ptr_at_pos,
   NULL,                               /* update_savestate_thumbnail_path */
   NULL,                               /* update_savestate_thumbnail_image */
   NULL,                               /* pointer_down */
   rgui_pointer_up,
   NULL,                               /* get_load_content_animation_data */
   generic_menu_entry_action
};

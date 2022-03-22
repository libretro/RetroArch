/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
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

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#include <compat/posix_string.h>
#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <formats/image.h>
#include <gfx/math/matrix_4x4.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <encodings/utf.h>
#include <retro_inline.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../frontend/frontend_driver.h"

#include "../menu_driver.h"
#include "../menu_screensaver.h"

#include "../../gfx/gfx_animation.h"
#include "../../gfx/gfx_thumbnail_path.h"
#include "../../gfx/gfx_thumbnail.h"

#include "../../input/input_osk.h"

#include "../../core_info.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"
#include "../../runtime_file.h"
#include "../../file_path_special.h"
#include "../../list_special.h"

/* Defines the 'device independent pixel' base
 * unit reference size for all UI elements.
 * 212 px corresponds to the the baseline standard
 * 22 inch, 96 DPI display */
#define MUI_DIP_BASE_UNIT_SIZE 212.0f

/* Spacer for left scrolling ticker text */
#if defined(__APPLE__)
/* UTF-8 support is currently broken on Apple devices... */
#define MUI_TICKER_SPACER "   |   "
#else
/* <EM SPACE><BULLET><EM SPACE>
 * UCN equivalent: "\u2003\u2022\u2003" */
#define MUI_TICKER_SPACER "\xE2\x80\x83\xE2\x80\xA2\xE2\x80\x83"
#endif

/* ==============================
 * Colour Themes START
 * ============================== */

/* Theme colours */
typedef struct
{
   /* Text (& small inline icon) colours */
   uint32_t on_sys_bar;
   uint32_t on_header;
   uint32_t list_text;
   uint32_t list_text_highlighted;
   uint32_t list_hint_text;
   uint32_t list_hint_text_highlighted;
   uint32_t status_bar_text;
   /* Background colours */
   uint32_t sys_bar_background;
   uint32_t title_bar_background;
   uint32_t list_background;
   uint32_t list_highlighted_background;
   uint32_t nav_bar_background;
   uint32_t surface_background;
   uint32_t thumbnail_background;
   uint32_t side_bar_background;
   uint32_t status_bar_background;
   /* List icon colours */
   uint32_t list_icon;
   uint32_t list_switch_on;
   uint32_t list_switch_on_background;
   uint32_t list_switch_off;
   uint32_t list_switch_off_background;
   /* Navigation bar icon colours */
   uint32_t nav_bar_icon_active;
   uint32_t nav_bar_icon_passive;
   uint32_t nav_bar_icon_disabled;
   /* Screensaver */
   uint32_t screensaver_tint;
   /* Misc. colours */
   uint32_t header_shadow;
   uint32_t landscape_border_shadow;
   uint32_t status_bar_shadow;
   uint32_t selection_marker_shadow;
   uint32_t scrollbar;
   uint32_t divider;
   uint32_t screen_fade;
   uint32_t missing_thumbnail_icon;
   float header_shadow_opacity;
   float landscape_border_shadow_opacity;
   float status_bar_shadow_opacity;
   float selection_marker_shadow_opacity;
   float screen_fade_opacity;
} materialui_theme_t;

static const materialui_theme_t materialui_theme_blue = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   0x000000, /* status_bar_text */
   /* Background colours */
   0x0069c0, /* sys_bar_background */
   0x2196f3, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xc1d5e0, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
   0xc1d5e0, /* side_bar_background */
   0x9F9FA0, /* status_bar_background */
   /* List icon colours */
   0x0069c0, /* list_icon */
   0x2196f3, /* list_switch_on */
   0x6ec6ff, /* list_switch_on_background */
   0x808e95, /* list_switch_off */
   0xbabdbe, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x0069c0, /* nav_bar_icon_active */
   0x9ea7aa, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xF5F5F6, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x0069c0, /* scrollbar */
   0x9ea7aa, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
   0.45f,    /* status_bar_shadow_opacity */
   0.1f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_blue_grey = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   0x000000, /* status_bar_text */
   /* Background colours */
   0x34515e, /* sys_bar_background */
   0x607d8b, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xe0e0e0, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
   0xe0e0e0, /* side_bar_background */
   0x9F9FA0, /* status_bar_background */
   /* List icon colours */
   0x34515e, /* list_icon */
   0x607d8b, /* list_switch_on */
   0x8eacbb, /* list_switch_on_background */
   0xbcbcbc, /* list_switch_off */
   0xc7c7c7, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x34515e, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xF5F5F6, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x34515e, /* scrollbar */
   0xc2c2c2, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
   0.45f,    /* status_bar_shadow_opacity */
   0.2f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_dark_blue = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xDEDEDE, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0x999999, /* list_hint_text */
   0xDEDEDE, /* list_hint_text_highlighted */
   0x999999, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x1F1F1F, /* title_bar_background */
   0x121212, /* list_background */
   0x34515e, /* list_highlighted_background */
   0x242424, /* nav_bar_background */
   0x1D1D1D, /* surface_background */
   0x000000, /* thumbnail_background */
   0x1D1D1D, /* side_bar_background */
   0x242424, /* status_bar_background */
   /* List icon colours */
   0x90caf9, /* list_icon */
   0x64b5f6, /* list_switch_on */
   0x5d99c6, /* list_switch_on_background */
   0x4b636e, /* list_switch_off */
   0x607d8b, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x6ec6ff, /* nav_bar_icon_active */
   0xA5B4BB, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xDEDEDE, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x3B3B3B, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x3B3B3B, /* selection_marker_shadow */
   0x90caf9, /* scrollbar */
   0x607d8b, /* divider */
   0x000000, /* screen_fade */
   0xDEDEDE, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.8f,     /* status_bar_shadow_opacity */
   0.2f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_green = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   0x000000, /* status_bar_text */
   /* Background colours */
   0x087f23, /* sys_bar_background */
   0x4caf50, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xdcedc8, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
   0xdcedc8, /* side_bar_background */
   0x9F9FA0, /* status_bar_background */
   /* List icon colours */
   0x087f23, /* list_icon */
   0x4caf50, /* list_switch_on */
   0x80e27e, /* list_switch_on_background */
   0xaabb97, /* list_switch_off */
   0xbec5b7, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x087f23, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xF5F5F6, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x087f23, /* scrollbar */
   0xaabb97, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
   0.45f,    /* status_bar_shadow_opacity */
   0.15f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_red = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   0x000000, /* status_bar_text */
   /* Background colours */
   0xba000d, /* sys_bar_background */
   0xf44336, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xf8bbd0, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
   0xf8bbd0, /* side_bar_background */
   0x9F9FA0, /* status_bar_background */
   /* List icon colours */
   0xba000d, /* list_icon */
   0xf44336, /* list_switch_on */
   0xff7961, /* list_switch_on_background */
   0xbf5f82, /* list_switch_off */
   0xc48b9f, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xba000d, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xF5F5F6, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0xba000d, /* scrollbar */
   0xbf5f82, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
   0.45f,    /* status_bar_shadow_opacity */
   0.15f,    /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_yellow = {
   /* Text (& small inline icon) colours */
   0x212121, /* on_sys_bar */
   0x000000, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   0x000000, /* status_bar_text */
   /* Background colours */
   0xc8b900, /* sys_bar_background */
   0xffeb3b, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xffecb3, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
   0xffecb3, /* side_bar_background */
   0x9F9FA0, /* status_bar_background */
   /* List icon colours */
   0xc6a700, /* list_icon */
   0xffeb3b, /* list_switch_on */
   0xccc5af, /* list_switch_on_background */
   0xcaae53, /* list_switch_off */
   0xccc5af, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xc6a700, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xFFFFFF, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xF5F5F6, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x33311A, /* selection_marker_shadow */
   0xc6a700, /* scrollbar */
   0xcbba83, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
   0.45f,    /* status_bar_shadow_opacity */
   0.15f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_nvidia_shield = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xDEDEDE, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0x999999, /* list_hint_text */
   0xDEDEDE, /* list_hint_text_highlighted */
   0x999999, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x1F1F1F, /* title_bar_background */
   0x121212, /* list_background */
   0x255d00, /* list_highlighted_background */
   0x242424, /* nav_bar_background */
   0x1D1D1D, /* surface_background */
   0x000000, /* thumbnail_background */
   0x1D1D1D, /* side_bar_background */
   0x242424, /* status_bar_background */
   /* List icon colours */
   0x7ab547, /* list_icon */
   0x85bb5c, /* list_switch_on */
   0x498515, /* list_switch_on_background */
   0x33691e, /* list_switch_off */
   0x003d00, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x7ab547, /* nav_bar_icon_active */
   0x558b2f, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xDEDEDE, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x3B3B3B, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x3B3B3B, /* selection_marker_shadow */
   0x7ab547, /* scrollbar */
   0x498515, /* divider */
   0x000000, /* screen_fade */
   0xDEDEDE, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.8f,     /* status_bar_shadow_opacity */
   0.2f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_materialui = {
   /* Text (& small inline icon) colours */
   0xDEDEDE, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0x212121, /* list_text */
   0x000000, /* list_text_highlighted */
   0x666666, /* list_hint_text */
   0x212121, /* list_hint_text_highlighted */
   0x000000, /* status_bar_text */
   /* Background colours */
   0x3700B3, /* sys_bar_background */
   0x6200ee, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xe7b9ff, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
   0xe7b9ff, /* side_bar_background */
   0x9F9FA0, /* status_bar_background */
   /* List icon colours */
   0x3700B3, /* list_icon */
   0x03DAC6, /* list_switch_on */
   0x018786, /* list_switch_on_background */
   0x9e47ff, /* list_switch_off */
   0x0400ba, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x018786, /* nav_bar_icon_active */
   0xaeaeae, /* nav_bar_icon_passive */
   0xffffff, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xF5F5F6, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x018786, /* scrollbar */
   0x018786, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
   0.45f,    /* status_bar_shadow_opacity */
   0.1f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_materialui_dark = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xDEDEDE, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0x999999, /* list_hint_text */
   0xDEDEDE, /* list_hint_text_highlighted */
   0x999999, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x1F1F1F, /* title_bar_background */
   0x121212, /* list_background */
   0x51455E, /* list_highlighted_background */
   0x242424, /* nav_bar_background */
   0x1D1D1D, /* surface_background */
   0x000000, /* thumbnail_background */
   0x1D1D1D, /* side_bar_background */
   0x242424, /* status_bar_background */
   /* List icon colours */
   0xbb86fc, /* list_icon */
   0x03DAC5, /* list_switch_on */
   0x00a895, /* list_switch_on_background */
   0xbb86fc, /* list_switch_off */
   0x8858c8, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x03DAC6, /* nav_bar_icon_active */
   0x00a895, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xDEDEDE, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x3B3B3B, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x3B3B3B, /* selection_marker_shadow */
   0xC89EFC, /* scrollbar */
   0x03DAC6, /* divider */
   0x000000, /* screen_fade */
   0xDEDEDE, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.8f,     /* status_bar_shadow_opacity */
   0.2f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_ozone_dark = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xFFFFFF, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0xDADADA, /* list_hint_text */
   0xEEEEEE, /* list_hint_text_highlighted */
   0xDADADA, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x373737, /* title_bar_background */
   0x2D2D2D, /* list_background */
   0x268C75, /* list_highlighted_background */
   0x373737, /* nav_bar_background */
   0x333333, /* surface_background */
   0x0B0B0B, /* thumbnail_background */
   0x333333, /* side_bar_background */
   0x191919, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0x00FFC5, /* list_switch_on */
   0x00D8AE, /* list_switch_on_background */
   0x9F9FA1, /* list_switch_off */
   0x7D7D7D, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x00FFC5, /* nav_bar_icon_active */
   0xDADADA, /* nav_bar_icon_passive */
   0x242424, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xDADADA, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x9F9F9F, /* scrollbar */
   0xFFFFFF, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.8f,     /* status_bar_shadow_opacity */
   0.05f,    /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_nord = {
   /* Text (& small inline icon) colours */
   0xD8DEE9, /* on_sys_bar */
   0xECEFF4, /* on_header */
   0xD8DEE9, /* list_text */
   0xECEFF4, /* list_text_highlighted */
   0x93E5CC, /* list_hint_text */
   0x93E5CC, /* list_hint_text_highlighted */
   0x93E5CC, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x4C566A, /* title_bar_background */
   0x2E3440, /* list_background */
   0x3f444f, /* list_highlighted_background */
   0x3B4252, /* nav_bar_background */
   0x3B4252, /* surface_background */
   0x0B0B0B, /* thumbnail_background */
   0x3f444f, /* side_bar_background */
   0x191D23, /* status_bar_background */
   /* List icon colours */
   0xD8DEE9, /* list_icon */
   0xA3BE8C, /* list_switch_on */
   0x7E946D, /* list_switch_on_background */
   0xB48EAD, /* list_switch_off */
   0x8A6D84, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xD8DEE9, /* nav_bar_icon_active */
   0x81A1C1, /* nav_bar_icon_passive */
   0x242A33, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xD8DEE9, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0xA0A5AD, /* scrollbar */
   0x81A1C1, /* divider */
   0x000000, /* screen_fade */
   0xD8DEE9, /* missing_thumbnail_icon */
   0.4f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.8f,     /* status_bar_shadow_opacity */
   0.35f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_gruvbox_dark = {
   /* Text (& small inline icon) colours */
   0xA89984, /* on_sys_bar */
   0xFBF1C7, /* on_header */
   0xEBDBB2, /* list_text */
   0xFBF1C7, /* list_text_highlighted */
   0xD79921, /* list_hint_text */
   0xFABD2F, /* list_hint_text_highlighted */
   0xD79921, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x504945, /* title_bar_background */
   0x282828, /* list_background */
   0x3C3836, /* list_highlighted_background */
   0x1D2021, /* nav_bar_background */
   0x32302F, /* surface_background */
   0x0B0B0B, /* thumbnail_background */
   0x3C3836, /* side_bar_background */
   0x161616, /* status_bar_background */
   /* List icon colours */
   0xA89984, /* list_icon */
   0xB8BB26, /* list_switch_on */
   0x98971A, /* list_switch_on_background */
   0xFB4934, /* list_switch_off */
   0xCC241D, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xBF9137, /* nav_bar_icon_active */
   0xA89984, /* nav_bar_icon_passive */
   0x3C3836, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xEBDBB2, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x7C6F64, /* scrollbar */
   0xD5C4A1, /* divider */
   0x000000, /* screen_fade */
   0xA89984, /* missing_thumbnail_icon */
   0.4f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.8f,     /* status_bar_shadow_opacity */
   0.35f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_solarized_dark = {
   /* Text (& small inline icon) colours */
   0x657B83, /* on_sys_bar */
   0x93A1A1, /* on_header */
   0x839496, /* list_text */
   0x93A1A1, /* list_text_highlighted */
   0x2AA198, /* list_hint_text */
   0x2AA198, /* list_hint_text_highlighted */
   0x2AA198, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x053542, /* title_bar_background */
   0x002B36, /* list_background */
   0x073642, /* list_highlighted_background */
   0x003541, /* nav_bar_background */
   0x073642, /* surface_background */
   0x0B0B0B, /* thumbnail_background */
   0x073642, /* side_bar_background */
   0x00181E, /* status_bar_background */
   /* List icon colours */
   0x657B83, /* list_icon */
   0x859900, /* list_switch_on */
   0x667500, /* list_switch_on_background */
   0x6C71C4, /* list_switch_off */
   0x565A9C, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x2AA198, /* nav_bar_icon_active */
   0x839496, /* nav_bar_icon_passive */
   0x00222B, /* nav_bar_icon_disabled */
   /* Screensaver */
   0x839496, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x586E75, /* scrollbar */
   0x2AA198, /* divider */
   0x000000, /* screen_fade */
   0x657B83, /* missing_thumbnail_icon */
   0.4f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.8f,     /* status_bar_shadow_opacity */
   0.35f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_cutie_blue = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xFFFFFF, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0xDADADA, /* list_hint_text */
   0xEEEEEE, /* list_hint_text_highlighted */
   0xDADADA, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0x3399FF, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
   0x333333, /* side_bar_background */
   0x0E0E0E, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0x3399FF, /* list_switch_on */
   0x454545, /* list_switch_on_background */
   0x454545, /* list_switch_off */
   0x414141, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x3399FF, /* nav_bar_icon_active */
   0xDADADA, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xFFFFFF, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.9f,     /* status_bar_shadow_opacity */
   0.1f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_cutie_cyan = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xFFFFFF, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0xDADADA, /* list_hint_text */
   0xEEEEEE, /* list_hint_text_highlighted */
   0xDADADA, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0x39859A, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
   0x333333, /* side_bar_background */
   0x0E0E0E, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0x39859A, /* list_switch_on */
   0x454545, /* list_switch_on_background */
   0x454545, /* list_switch_off */
   0x414141, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x39859A, /* nav_bar_icon_active */
   0xDADADA, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xFFFFFF, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.9f,     /* status_bar_shadow_opacity */
   0.1f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_cutie_green = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xFFFFFF, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0xDADADA, /* list_hint_text */
   0xEEEEEE, /* list_hint_text_highlighted */
   0xDADADA, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0x23A367, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
   0x333333, /* side_bar_background */
   0x0E0E0E, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0x23A367, /* list_switch_on */
   0x454545, /* list_switch_on_background */
   0x454545, /* list_switch_off */
   0x414141, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x23A367, /* nav_bar_icon_active */
   0xDADADA, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xFFFFFF, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.9f,     /* status_bar_shadow_opacity */
   0.1f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_cutie_orange = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xFFFFFF, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0xDADADA, /* list_hint_text */
   0xEEEEEE, /* list_hint_text_highlighted */
   0xDADADA, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0xCE6E1F, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
   0x333333, /* side_bar_background */
   0x0E0E0E, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0xCE6E1F, /* list_switch_on */
   0x454545, /* list_switch_on_background */
   0x454545, /* list_switch_off */
   0x414141, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xCE6E1F, /* nav_bar_icon_active */
   0xDADADA, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xFFFFFF, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.9f,     /* status_bar_shadow_opacity */
   0.1f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_cutie_pink = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xFFFFFF, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0xDADADA, /* list_hint_text */
   0xEEEEEE, /* list_hint_text_highlighted */
   0xDADADA, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0xD16FD8, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
   0x333333, /* side_bar_background */
   0x0E0E0E, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0xD16FD8, /* list_switch_on */
   0x454545, /* list_switch_on_background */
   0x454545, /* list_switch_off */
   0x414141, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xD16FD8, /* nav_bar_icon_active */
   0xDADADA, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xFFFFFF, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.9f,     /* status_bar_shadow_opacity */
   0.1f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_cutie_purple = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xFFFFFF, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0xDADADA, /* list_hint_text */
   0xEEEEEE, /* list_hint_text_highlighted */
   0xDADADA, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0x814FFF, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
   0x333333, /* side_bar_background */
   0x0E0E0E, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0x814FFF, /* list_switch_on */
   0x454545, /* list_switch_on_background */
   0x454545, /* list_switch_off */
   0x414141, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x814FFF, /* nav_bar_icon_active */
   0xDADADA, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xFFFFFF, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.9f,     /* status_bar_shadow_opacity */
   0.1f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_cutie_red = {
   /* Text (& small inline icon) colours */
   0xC4C4C4, /* on_sys_bar */
   0xFFFFFF, /* on_header */
   0xFFFFFF, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0xDADADA, /* list_hint_text */
   0xEEEEEE, /* list_hint_text_highlighted */
   0xDADADA, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0xCB1619, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
   0x333333, /* side_bar_background */
   0x0E0E0E, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0xCB1619, /* list_switch_on */
   0x454545, /* list_switch_on_background */
   0x454545, /* list_switch_off */
   0x414141, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xCB1619, /* nav_bar_icon_active */
   0xDADADA, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xFFFFFF, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x000000, /* selection_marker_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.9f,     /* status_bar_shadow_opacity */
   0.1f,     /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_virtual_boy = {
   /* Text (& small inline icon) colours */
   0xE60000, /* on_sys_bar */
   0xF00000, /* on_header */
   0xE60000, /* list_text */
   0xF00000, /* list_text_highlighted */
   0xE60000, /* list_hint_text */
   0xF00000, /* list_hint_text_highlighted */
   0xE60000, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x350000, /* title_bar_background */
   0x000000, /* list_background */
   0x400000, /* list_highlighted_background */
   0x350000, /* nav_bar_background */
   0x400000, /* surface_background */
   0x250000, /* thumbnail_background */
   0x400000, /* side_bar_background */
   0x000000, /* status_bar_background */
   /* List icon colours */
   0xE60000, /* list_icon */
   0xE60000, /* list_switch_on */
   0x6B0000, /* list_switch_on_background */
   0x6B0000, /* list_switch_off */
   0x6B0000, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xF00000, /* nav_bar_icon_active */
   0xA10000, /* nav_bar_icon_passive */
   0x300000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0xE60000, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0xE60000, /* selection_marker_shadow */
   0xA10000, /* scrollbar */
   0xE60000, /* divider */
   0x000000, /* screen_fade */
   0xE60000, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.7f,     /* status_bar_shadow_opacity */
   0.35f,    /* selection_marker_shadow_opacity */
   0.75f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_hacking_the_kernel = {
   /* Text (& small inline icon) colours */
   0x00E000, /* on_sys_bar */
   0x00E02D, /* on_header */
   0x00E000, /* list_text */
   0x00E02D, /* list_text_highlighted */
   0x83FF83, /* list_hint_text */
   0x83FF83, /* list_hint_text_highlighted */
   0x83FF83, /* status_bar_text */
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x003400, /* title_bar_background */
   0x000000, /* list_background */
   0x022F1C, /* list_highlighted_background */
   0x002200, /* nav_bar_background */
   0x022F1C, /* surface_background */
   0x001100, /* thumbnail_background */
   0x022F1C, /* side_bar_background */
   0x002200, /* status_bar_background */
   /* List icon colours */
   0x008C00, /* list_icon */
   0x89DE00, /* list_switch_on */
   0x4A8500, /* list_switch_on_background */
   0x04804C, /* list_switch_off */
   0x02663C, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0x00E02D, /* nav_bar_icon_active */
   0x008C00, /* nav_bar_icon_passive */
   0x000000, /* nav_bar_icon_disabled */
   /* Screensaver */
   0x00E000, /* screensaver_tint */
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x08ED8D, /* landscape_border_shadow */
   0x000000, /* status_bar_shadow */
   0x00FF00, /* selection_marker_shadow */
   0x008C00, /* scrollbar */
   0x006F00, /* divider */
   0x000000, /* screen_fade */
   0x008C00, /* missing_thumbnail_icon */
   0.8f,     /* header_shadow_opacity */
   0.2f,     /* landscape_border_shadow_opacity */
   1.0f,     /* status_bar_shadow_opacity */
   0.12f,    /* selection_marker_shadow_opacity */
   0.85f     /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_gray_dark = {
   /* Text (& small inline icon) colours */
   0x808080, /* on_sys_bar */
   0xC0C0C0, /* on_header */
   0xC0C0C0, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0x707070, /* list_hint_text */
   0x808080, /* list_hint_text_highlighted */
   0x808080, /* status_bar_text */
   /* Background colours */
   0x101010, /* sys_bar_background */
   0x101010, /* title_bar_background */
   0x101010, /* list_background */
   0x303030, /* list_highlighted_background */
   0x101010, /* nav_bar_background */
   0x202020, /* surface_background */
   0x0C0C0C, /* thumbnail_background */
   0x101010, /* side_bar_background */
   0x101010, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0xFFFFFF, /* list_switch_on */
   0x202020, /* list_switch_on_background */
   0x707070, /* list_switch_off */
   0x202020, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xFFFFFF, /* nav_bar_icon_active */
   0x707070, /* nav_bar_icon_passive */
   0x202020, /* nav_bar_icon_disabled */
   /* Screensaver */
   0x101010, /* screensaver_tint */
   /* Misc. colours */
   0x202020, /* header_shadow */
   0x202020, /* landscape_border_shadow */
   0x202020, /* status_bar_shadow */
   0x0C0C0C, /* selection_marker_shadow */
   0x202020, /* scrollbar */
   0x101010, /* divider */
   0x0C0C0C, /* screen_fade */
   0x202020, /* missing_thumbnail_icon */
   0.0f,     /* header_shadow_opacity */
   0.5f,     /* landscape_border_shadow_opacity */
   0.0f,     /* status_bar_shadow_opacity */
   0.0f,     /* selection_marker_shadow_opacity */
   0.5f      /* screen_fade_opacity */
};

static const materialui_theme_t materialui_theme_gray_light = {
   /* Text (& small inline icon) colours */
   0x808080, /* on_sys_bar */
   0xC0C0C0, /* on_header */
   0xC0C0C0, /* list_text */
   0xFFFFFF, /* list_text_highlighted */
   0x707070, /* list_hint_text */
   0x808080, /* list_hint_text_highlighted */
   0x808080, /* status_bar_text */
   /* Background colours */
   0x303030, /* sys_bar_background */
   0x303030, /* title_bar_background */
   0x303030, /* list_background */
   0x101010, /* list_highlighted_background */
   0x303030, /* nav_bar_background */
   0x202020, /* surface_background */
   0x0C0C0C, /* thumbnail_background */
   0x303030, /* side_bar_background */
   0x303030, /* status_bar_background */
   /* List icon colours */
   0xFFFFFF, /* list_icon */
   0xFFFFFF, /* list_switch_on */
   0x202020, /* list_switch_on_background */
   0x707070, /* list_switch_off */
   0x202020, /* list_switch_off_background */
   /* Navigation bar icon colours */
   0xFFFFFF, /* nav_bar_icon_active */
   0x707070, /* nav_bar_icon_passive */
   0x202020, /* nav_bar_icon_disabled */
   /* Screensaver */
   0x101010, /* screensaver_tint */
   /* Misc. colours */
   0x202020, /* header_shadow */
   0x202020, /* landscape_border_shadow */
   0x202020, /* status_bar_shadow */
   0x0C0C0C, /* selection_marker_shadow */
   0x202020, /* scrollbar */
   0x303030, /* divider */
   0x0C0C0C, /* screen_fade */
   0x202020, /* missing_thumbnail_icon */
   0.0f,     /* header_shadow_opacity */
   0.5f,     /* landscape_border_shadow_opacity */
   0.0f,     /* status_bar_shadow_opacity */
   0.0f,     /* selection_marker_shadow_opacity */
   0.5f      /* screen_fade_opacity */
};

typedef struct
{
   /* Text */
   uint32_t sys_bar_text;
   uint32_t header_text;
   uint32_t list_text;
   uint32_t list_text_highlighted;
   uint32_t list_hint_text;
   uint32_t list_hint_text_highlighted;
   uint32_t status_bar_text;
   /* Screensaver */
   uint32_t screensaver_tint;
   /* Background colours */
   float sys_bar_background[16];
   float title_bar_background[16];
   float list_background[16];
   float list_highlighted_background[16];
   float nav_bar_background[16];
   float surface_background[16];
   float thumbnail_background[16];
   float side_bar_background[16];
   float status_bar_background[16];
   /* System bar + header icon colours */
   float sys_bar_icon[16];
   float header_icon[16];
   /* List icon colours */
   float list_icon[16];
   float list_switch_on[16];
   float list_switch_on_background[16];
   float list_switch_off[16];
   float list_switch_off_background[16];
   /* Navigation bar icon colours */
   float nav_bar_icon_active[16];
   float nav_bar_icon_passive[16];
   float nav_bar_icon_disabled[16];
   /* Misc. colours */
   float header_shadow[16];
   float landscape_border_shadow_left[16];
   float landscape_border_shadow_right[16];
   float status_bar_shadow[16];
   float selection_marker_shadow_top[16];
   float selection_marker_shadow_bottom[16];
   float scrollbar[16];
   float divider[16];
   float entry_divider[16];
   float screen_fade[16];
   float missing_thumbnail_icon[16];
   float landscape_border_shadow_opacity;
   float status_bar_shadow_opacity;
   float selection_marker_shadow_opacity;
   float screen_fade_opacity;
   /* Flags */
   bool divider_is_list_background;
} materialui_colors_t;

/* ==============================
 * Colour Themes END
 * ============================== */

/* Specifies minimum period (in usec) between
 * tab switch events when input repeat is
 * active (i.e. when navigating between top level
 * menu categories by *holding* left/right on
 * RetroPad or keyboard)
 * > Note: We want to set a value of 300 ms
 *   here, but doing so leads to bad pacing when
 *   running at 60 Hz (due to random frame time
 *   deviations - input repeat cycles always take
 *   slightly more or less than 300 ms, so tab
 *   switches occur every n or (n + 1) frames,
 *   which gives the appearance of stuttering).
 *   Reducing the delay by 1 ms accommodates
 *   any timing fluctuations, resulting in
 *   smooth motion */
#define MUI_TAB_SWITCH_REPEAT_DELAY 299000

/* Animation defines */
#define MUI_ANIM_DURATION_SCROLL 166.66667f
#define MUI_ANIM_DURATION_SCROLL_RESET 83.333333f
/* According to Material UI specifications, animations
 * that affect a large portion of the screen should
 * have a duration of between 250ms and 300ms. This
 * should therefore be the value used for menu
 * transitions - but even 250ms feels too slow...
 * We compromise by setting a time of 200ms, which
 * is the same as the 'short press' duration.
 * This is reasonably fast, without making slide
 * animations too 'jarring'... */
#define MUI_ANIM_DURATION_MENU_TRANSITION 200.0f

/* Set a baseline aspect ratio of 4:3 for thumbnail
 * images */
#define MUI_THUMBNAIL_DEFAULT_ASPECT_RATIO 1.3333333f

/* Default thumbnail type to select when force-enabling
 * secondary thumbnails
 * > 1 == Named_Snaps */
#define MUI_DEFAULT_SECONDARY_THUMBNAIL_TYPE 1
/* Default thumbnail type to select when force-enabling
 * secondary thumbnails *if* primary thumbnail is
 * already set to MUI_DEFAULT_SECONDARY_THUMBNAIL_TYPE
 * > 3 == Named_Boxarts */
#define MUI_DEFAULT_SECONDARY_THUMBNAIL_FALLBACK_TYPE 3

/* Thumbnail stream delay when performing standard
 * menu navigation */
#define MUI_THUMBNAIL_STREAM_DELAY_DEFAULT 83.333333f
/* Thumbnail stream delay when performing 'fast'
 * navigation by dragging the scrollbar
 * > Must increase stream delay, otherwise it's
 *   too easy to enqueue vast numbers of image
 *   requests... */
#define MUI_THUMBNAIL_STREAM_DELAY_SCROLLBAR_DRAG 166.66667f
/* Thumbnail stream delay when viewing
 * 'desktop'-layout playlists
 * > In this case, thumbnails are loaded
 *   as the entry selection is changed. We
 *   therefore want the stream delay to match
 *   the scroll animation duration */
#define MUI_THUMBNAIL_STREAM_DELAY_PLAYLIST_DESKTOP MUI_ANIM_DURATION_SCROLL

/* Defines the various types of supported menu
 * list views
 * - MUI_LIST_VIEW_DEFAULT is the standard for
 *   all non-playlist views
 * - MUI_LIST_VIEW_PLAYLIST is for playlists
 *   without thumbnails
 * - Everything else is for playlists with fancy
 *   thumbnail-based layouts */
enum materialui_list_view_type
{
   MUI_LIST_VIEW_DEFAULT = 0,
   MUI_LIST_VIEW_PLAYLIST,
   MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL,
   MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM,
   MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE,
   MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON,
   MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP
};

/* Defines the various types of icon that
 * can be associated with menu entries */
enum materialui_node_icon_type
{
   MUI_ICON_TYPE_NONE = 0,
   MUI_ICON_TYPE_INTERNAL,
   MUI_ICON_TYPE_MENU_EXPLORE,
   MUI_ICON_TYPE_PLAYLIST,
   MUI_ICON_TYPE_MENU_CONTENTLESS_CORE
};

/* This structure holds auxiliary information for
 * each menu entry (physical on-screen size/position,
 * icon data, thumbnail data, etc.) */
typedef struct
{
   /* Thumbnail containers */
   struct
   {
      gfx_thumbnail_t primary;   /* uintptr_t alignment */
      gfx_thumbnail_t secondary; /* uintptr_t alignment */
   } thumbnails;
   unsigned icon_texture_index;
   float entry_width;
   float entry_height;
   float text_height;
   float x;
   float y;
   enum materialui_node_icon_type icon_type;
} materialui_node_t;

/* Defines all standard menu textures */
enum
{
   MUI_TEXTURE_POINTER = 0,
   MUI_TEXTURE_BACK,
   MUI_TEXTURE_SWITCH_ON,
   MUI_TEXTURE_SWITCH_OFF,
   MUI_TEXTURE_SWITCH_BG,
   MUI_TEXTURE_TAB_MAIN,
   MUI_TEXTURE_TAB_PLAYLISTS,
   MUI_TEXTURE_TAB_SETTINGS,
   MUI_TEXTURE_TAB_BACK,
   MUI_TEXTURE_TAB_RESUME,
   MUI_TEXTURE_KEY,
   MUI_TEXTURE_KEY_HOVER,
   MUI_TEXTURE_FOLDER,
   MUI_TEXTURE_PARENT_DIRECTORY,
   MUI_TEXTURE_IMAGE,
   MUI_TEXTURE_ARCHIVE,
   MUI_TEXTURE_VIDEO,
   MUI_TEXTURE_MUSIC,
   MUI_TEXTURE_QUIT,
   MUI_TEXTURE_HELP,
   MUI_TEXTURE_HISTORY,
   MUI_TEXTURE_INFO,
   MUI_TEXTURE_ADD,
   MUI_TEXTURE_SETTINGS,
   MUI_TEXTURE_FILE,
   MUI_TEXTURE_PLAYLIST,
   MUI_TEXTURE_UPDATER,
   MUI_TEXTURE_QUICKMENU,
   MUI_TEXTURE_NETPLAY,
   MUI_TEXTURE_CORES,
   MUI_TEXTURE_SHADERS,
   MUI_TEXTURE_CONTROLS,
   MUI_TEXTURE_CLOSE,
   MUI_TEXTURE_CORE_OPTIONS,
   MUI_TEXTURE_CORE_CHEAT_OPTIONS,
   MUI_TEXTURE_RESUME,
   MUI_TEXTURE_RESTART,
   MUI_TEXTURE_ADD_TO_FAVORITES,
   MUI_TEXTURE_RUN,
   MUI_TEXTURE_RENAME,
   MUI_TEXTURE_DATABASE,
   MUI_TEXTURE_ADD_TO_MIXER,
   MUI_TEXTURE_SCAN,
   MUI_TEXTURE_REMOVE,
   MUI_TEXTURE_START_CORE,
   MUI_TEXTURE_LOAD_STATE,
   MUI_TEXTURE_SAVE_STATE,
   MUI_TEXTURE_UNDO_LOAD_STATE,
   MUI_TEXTURE_UNDO_SAVE_STATE,
   MUI_TEXTURE_STATE_SLOT,
   MUI_TEXTURE_TAKE_SCREENSHOT,
   MUI_TEXTURE_CONFIGURATIONS,
   MUI_TEXTURE_LOAD_CONTENT,
   MUI_TEXTURE_DISK,
   MUI_TEXTURE_EJECT,
   MUI_TEXTURE_CHECKMARK,
   MUI_TEXTURE_SEARCH,
   MUI_TEXTURE_BATTERY_CRITICAL,
   MUI_TEXTURE_BATTERY_20,
   MUI_TEXTURE_BATTERY_30,
   MUI_TEXTURE_BATTERY_50,
   MUI_TEXTURE_BATTERY_60,
   MUI_TEXTURE_BATTERY_80,
   MUI_TEXTURE_BATTERY_90,
   MUI_TEXTURE_BATTERY_100,
   MUI_TEXTURE_BATTERY_CHARGING,
   MUI_TEXTURE_SWITCH_VIEW,
   MUI_TEXTURE_LAST
};

/* This structure holds all runtime parameters
 * associated with landscape optimisation
 * (enable state, border width, nominal
 * additional horizontal margin/padding for
 * menu entries) */
typedef struct
{
   unsigned border_width;
   unsigned entry_margin;
   bool enabled;
} materialui_landscape_optimization_t;

/* Maximum number of menu tabs that can be shown on
 * the navigation bar */
#define MUI_NAV_BAR_NUM_MENU_TABS_MAX 3

/* Number of action tabs shown on the navigation bar */
#define MUI_NAV_BAR_NUM_ACTION_TABS 2

/* Defines the various types of menu tab that can
 * be shown on the navigation bar */
enum materialui_nav_bar_menu_tab_type
{
   MUI_NAV_BAR_MENU_TAB_NONE = 0,
   MUI_NAV_BAR_MENU_TAB_MAIN,
   MUI_NAV_BAR_MENU_TAB_PLAYLISTS,
   MUI_NAV_BAR_MENU_TAB_SETTINGS
};

/* Defines the various types of action tab that can
 * be shown on the navigation bar */
enum materialui_nav_bar_action_tab_type
{
   MUI_NAV_BAR_ACTION_TAB_NONE = 0,
   MUI_NAV_BAR_ACTION_TAB_BACK,
   MUI_NAV_BAR_ACTION_TAB_RESUME
};

/* Defines navigation bar draw locations
 * Note: Only bottom, right and 'hidden'
 * are supported at present... */
enum materialui_nav_bar_location_type
{
   MUI_NAV_BAR_LOCATION_BOTTOM = 0,
   MUI_NAV_BAR_LOCATION_RIGHT,
   MUI_NAV_BAR_LOCATION_HIDDEN
};

/* This structure holds all runtime parameters
 * associated with a navigation bar menu tab */
typedef struct
{
   unsigned texture_index;
   enum materialui_nav_bar_menu_tab_type type;
   bool active;
} materialui_nav_bar_menu_tab_t;

/* This structure holds all runtime parameters
 * associated with a navigation bar action tab */
typedef struct
{
   unsigned texture_index;
   enum materialui_nav_bar_action_tab_type type;
   bool enabled;
} materialui_nav_bar_action_tab_t;

/* This structure holds all runtime parameters for
 * the navigation bar */
typedef struct
{
   unsigned width;
   unsigned divider_width;
   unsigned selection_marker_width;
   unsigned num_menu_tabs;
   unsigned active_menu_tab_index;
   unsigned last_active_menu_tab_index;
   materialui_nav_bar_action_tab_t back_tab;    /* unsigned alignment */
   materialui_nav_bar_action_tab_t resume_tab;  /* unsigned alignment */
   materialui_nav_bar_menu_tab_t menu_tabs[MUI_NAV_BAR_NUM_MENU_TABS_MAX]; /* unsigned alignment */
   enum materialui_nav_bar_location_type location;
   bool menu_navigation_wrapped;
} materialui_nav_bar_t;

/* This structure holds all runtime parameters for
 * the scrollbar */
typedef struct
{
   int x;
   int y;
   unsigned width;
   unsigned height;
   bool active;
   bool dragged;
} materialui_scrollbar_t;

/* Defines all possible entry value types
 * > Note: These are not necessarily 'values',
 *   but they correspond to the object drawn in
 *   the 'value' location when rendering
 *   menu lists */
enum materialui_entry_value_type
{
   MUI_ENTRY_VALUE_NONE = 0,
   MUI_ENTRY_VALUE_TEXT,
   MUI_ENTRY_VALUE_SWITCH_ON,
   MUI_ENTRY_VALUE_SWITCH_OFF,
   MUI_ENTRY_VALUE_CHECKMARK
};

/* This structure holds all objects + metadata
 * corresponding to a particular font */
typedef struct
{
   font_data_t *font;
   video_font_raster_block_t raster_block;   /* ptr alignment */
   unsigned glyph_width;
   unsigned wideglyph_width;
   int line_height;
   int line_ascender;
   int line_centre_offset;
} materialui_font_data_t;

#define MUI_BATTERY_PERCENT_MAX_LENGTH 12
#define MUI_TIMEDATE_MAX_LENGTH        255

/* This structure is used to cache system bar
 * string data (+ metadata) to improve rendering
 * performance */
typedef struct
{
   int battery_percent_width;
   int timedate_width;
   char battery_percent_str[MUI_BATTERY_PERCENT_MAX_LENGTH];
   char timedate_str[MUI_TIMEDATE_MAX_LENGTH];
} materialui_sys_bar_cache_t;

/* This structure holds all runtime parameters
 * for the status bar (used to display auxiliary
 * information at the bottom of the current list
 * view). At present, this enables metadata for
 * the currently selected playlist entry to be
 * shown when using the 'desktop'-layout */
typedef struct
{
   size_t last_selected;
   unsigned height;
   float delay_timer;
   float alpha;
   char str[MENU_SUBLABEL_MAX_LENGTH];
   char runtime_fallback_str[255];
   char last_played_fallback_str[255];
   bool enabled;
   bool cached;
} materialui_status_bar_t;

/* Defines common positions when referencing
 * the list of currently on screen menu entries
 * > Used to specify a target when the current
 *   selection is off screen, and we wish to
 *   automatically move the selection marker
 *   to a specific on screen location */
enum materialui_onscreen_entry_position_type
{
   MUI_ONSCREEN_ENTRY_FIRST = 0,
   MUI_ONSCREEN_ENTRY_LAST,
   MUI_ONSCREEN_ENTRY_CENTRE
};

/* Contains the file path(s) and texture pointer
 * of a single playlist icon */
typedef struct
{
   char *playlist_file;
   char *image_file;
   uintptr_t image;
} materialui_playlist_icon_t;

/* Contains icon data for all installed
 * playlists */
typedef struct
{
   materialui_playlist_icon_t *icons;
   size_t size;
} materialui_playlist_icons_t;

typedef struct materialui_handle
{
   /* Pointer info */
   menu_input_pointer_t pointer;                      /* int64_t alignment */
   /* Use common tickers for all text
    * > Simplifies configuration and
    *   improves performance */
   gfx_animation_ctx_ticker_t ticker;                 /* uint64_t alignment */
   gfx_animation_ctx_ticker_smooth_t ticker_smooth;   /* uint64_t alignment */
   /* Keeps track of the last time tabs were switched
    * via a MENU_ACTION_LEFT/MENU_ACTION_RIGHT event */
   retro_time_t last_tab_switch_time;  /* uint64_t alignment */

   playlist_t *playlist;            /* ptr alignment */

   /* Font data */
   struct
   {
      materialui_font_data_t title; /* ptr alignment */
      materialui_font_data_t list;  /* ptr alignment */
      materialui_font_data_t hint;  /* ptr alignment */
   } font_data;

   void (*word_wrap)(char *dst, size_t dst_size, const char *src,
      int line_width, int wideglyph_width, unsigned max_lines);

   /* Thumbnail helpers */
   gfx_thumbnail_path_data_t *thumbnail_path_data;

   struct
   {
      materialui_playlist_icons_t playlist;  /* ptr alignment */
      uintptr_t bg;
      uintptr_t list[MUI_TEXTURE_LAST];
   } textures;

   menu_screensaver_t *screensaver;

   /* Status bar */
   materialui_status_bar_t status_bar; /* size_t alignment */
   size_t last_stack_size;
   size_t first_onscreen_entry;
   size_t last_onscreen_entry;
   /* Used to track scroll animations */
   size_t scroll_animation_selection;
   size_t fullscreen_thumbnail_selection;
   /* > When viewing 'desktop'-layout playlists,
    *   need to cache the index of the last
    *   selected entry so we can keep displaying
    *   its thumbnails while waiting for next
    *   to load after the selection has changed */
   size_t desktop_thumbnail_last_selection;
   unsigned last_width;
   unsigned last_height;
   unsigned sys_bar_height;
   unsigned title_bar_height;
   unsigned header_shadow_height;
   unsigned selection_marker_shadow_height;
   unsigned icon_size;
   unsigned sys_bar_icon_size;
   unsigned margin;
   unsigned sys_bar_margin;
   unsigned entry_divider_width;
   unsigned sublabel_gap;
   unsigned sublabel_padding;
   /* Navigation bar parameters
    * Note: layout width and height are convenience
    * variables used when determining usable width/
    * height for all other menu elements - e.g. when
    * navigation bar is at the bottom of the screen
    * nav_bar_screen_width is zero */
   unsigned nav_bar_layout_width;
   unsigned nav_bar_layout_height;

   unsigned ticker_x_offset;
   unsigned ticker_str_width;

   /* Touch feedback animation parameters */
   unsigned touch_feedback_selection;

   unsigned thumbnail_width_max;
   unsigned thumbnail_height_max;
   materialui_landscape_optimization_t
         landscape_optimization; /* unsigned alignment */
   materialui_nav_bar_t nav_bar; /* unsigned alignment */
   /* Colour theme parameters */
   materialui_colors_t colors;   /* uint32_t alignment */

   /* Scrollbar parameters */
   materialui_scrollbar_t scrollbar;   /* int alignment */
   int cursor_size;
   /* Cached system bar data */
   materialui_sys_bar_cache_t sys_bar_cache; /* int alignment */
   float last_scale_factor;
   float dip_base_unit_size;
   /* Y position of the vertical scroll */
   float scroll_y;
   float content_height;
   float pointer_start_scroll_y;
   float transition_alpha;
   float transition_x_offset;
   float thumbnail_stream_delay;
   float fullscreen_thumbnail_alpha;
   float touch_feedback_alpha;
   int16_t pointer_start_x;
   int16_t pointer_start_y;

   /* Colour theme parameters */
   enum materialui_color_theme color_theme;

   enum materialui_landscape_layout_optimization_type
         last_landscape_layout_optimization;
   enum materialui_list_view_type list_view_type;
   char msgbox[1024];
   char menu_title[255];
   char fullscreen_thumbnail_label[255];
   bool is_portrait;
   bool need_compute;
   bool show_mouse;
   bool show_screensaver;
   bool is_playlist_tab;
   bool is_playlist;
   bool is_file_list;
   bool is_dropdown_list;
   bool is_core_updater_list;
   bool last_show_nav_bar;
   bool last_auto_rotate_nav_bar;
   bool menu_stack_flushed;
   /* Used to track scroll animations */
   bool scroll_animation_active;
   bool use_smooth_ticker;
   bool touch_feedback_update_selection;
   bool primary_thumbnail_available;
   bool secondary_thumbnail_enabled;
   bool show_fullscreen_thumbnails;
   bool show_selection_marker_shadow;
} materialui_handle_t;

static void hex32_to_rgba_normalized(uint32_t hex, float* rgba, float alpha)
{
   rgba[0] = rgba[4] = rgba[8]  = rgba[12] = ((hex >> 16) & 0xFF) * (1.0f / 255.0f); /* r */
   rgba[1] = rgba[5] = rgba[9]  = rgba[13] = ((hex >> 8 ) & 0xFF) * (1.0f / 255.0f); /* g */
   rgba[2] = rgba[6] = rgba[10] = rgba[14] = ((hex >> 0 ) & 0xFF) * (1.0f / 255.0f); /* b */
   rgba[3] = rgba[7] = rgba[11] = rgba[15] = alpha;
}

static const materialui_theme_t *materialui_get_theme(enum materialui_color_theme color_theme)
{
   switch (color_theme)
   {
      case MATERIALUI_THEME_BLUE:
         return &materialui_theme_blue;
      case MATERIALUI_THEME_BLUE_GREY:
         return &materialui_theme_blue_grey;
      case MATERIALUI_THEME_DARK_BLUE:
         return &materialui_theme_dark_blue;
      case MATERIALUI_THEME_GREEN:
         return &materialui_theme_green;
      case MATERIALUI_THEME_RED:
         return &materialui_theme_red;
      case MATERIALUI_THEME_YELLOW:
         return &materialui_theme_yellow;
      case MATERIALUI_THEME_NVIDIA_SHIELD:
         return &materialui_theme_nvidia_shield;
      case MATERIALUI_THEME_MATERIALUI:
         return &materialui_theme_materialui;
      case MATERIALUI_THEME_MATERIALUI_DARK:
         return &materialui_theme_materialui_dark;
      case MATERIALUI_THEME_OZONE_DARK:
         return &materialui_theme_ozone_dark;
      case MATERIALUI_THEME_NORD:
         return &materialui_theme_nord;
      case MATERIALUI_THEME_GRUVBOX_DARK:
         return &materialui_theme_gruvbox_dark;
      case MATERIALUI_THEME_SOLARIZED_DARK:
         return &materialui_theme_solarized_dark;
      case MATERIALUI_THEME_CUTIE_BLUE:
         return &materialui_theme_cutie_blue;
      case MATERIALUI_THEME_CUTIE_CYAN:
         return &materialui_theme_cutie_cyan;
      case MATERIALUI_THEME_CUTIE_GREEN:
         return &materialui_theme_cutie_green;
      case MATERIALUI_THEME_CUTIE_ORANGE:
         return &materialui_theme_cutie_orange;
      case MATERIALUI_THEME_CUTIE_PINK:
         return &materialui_theme_cutie_pink;
      case MATERIALUI_THEME_CUTIE_PURPLE:
         return &materialui_theme_cutie_purple;
      case MATERIALUI_THEME_CUTIE_RED:
         return &materialui_theme_cutie_red;
      case MATERIALUI_THEME_VIRTUAL_BOY:
         return &materialui_theme_virtual_boy;
      case MATERIALUI_THEME_HACKING_THE_KERNEL:
         return &materialui_theme_hacking_the_kernel;
      case MATERIALUI_THEME_GRAY_DARK:
         return &materialui_theme_gray_dark;
      case MATERIALUI_THEME_GRAY_LIGHT:
         return &materialui_theme_gray_light;
      default:
         break;
   }

   return &materialui_theme_blue;
}

static void materialui_prepare_colors(
      materialui_handle_t *mui, enum materialui_color_theme color_theme)
{
   const materialui_theme_t *current_theme = materialui_get_theme(color_theme);

   /* Parse theme colours */

   /* > Text (& small inline icon) colours */
   mui->colors.sys_bar_text               = (current_theme->on_sys_bar                 << 8) | 0xFF;
   mui->colors.header_text                = (current_theme->on_header                  << 8) | 0xFF;
   mui->colors.list_text                  = (current_theme->list_text                  << 8) | 0xFF;
   mui->colors.list_text_highlighted      = (current_theme->list_text_highlighted      << 8) | 0xFF;
   mui->colors.list_hint_text             = (current_theme->list_hint_text             << 8) | 0xFF;
   mui->colors.list_hint_text_highlighted = (current_theme->list_hint_text_highlighted << 8) | 0xFF;
   mui->colors.status_bar_text            = (current_theme->status_bar_text            << 8) | 0xFF;

   /* > Background colours */
   hex32_to_rgba_normalized(
            current_theme->sys_bar_background,
            mui->colors.sys_bar_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->title_bar_background,
            mui->colors.title_bar_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_background,
            mui->colors.list_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_highlighted_background,
            mui->colors.list_highlighted_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->nav_bar_background,
            mui->colors.nav_bar_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->surface_background,
            mui->colors.surface_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->thumbnail_background,
            mui->colors.thumbnail_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->side_bar_background,
            mui->colors.side_bar_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->status_bar_background,
            mui->colors.status_bar_background, 1.0f);

   /* > System bar + header icon colours */
   hex32_to_rgba_normalized(
            current_theme->on_sys_bar,
            mui->colors.sys_bar_icon, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->on_header,
            mui->colors.header_icon, 1.0f);

   /* > List icon colours */
   hex32_to_rgba_normalized(
            current_theme->list_icon,
            mui->colors.list_icon, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_switch_on,
            mui->colors.list_switch_on, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_switch_on_background,
            mui->colors.list_switch_on_background, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_switch_off,
            mui->colors.list_switch_off, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->list_switch_off_background,
            mui->colors.list_switch_off_background, 1.0f);

   /* > Navigation bar icon colours */
   hex32_to_rgba_normalized(
            current_theme->nav_bar_icon_active,
            mui->colors.nav_bar_icon_active, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->nav_bar_icon_passive,
            mui->colors.nav_bar_icon_passive, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->nav_bar_icon_disabled,
            mui->colors.nav_bar_icon_disabled, 1.0f);

   /* > Misc. colours */
   hex32_to_rgba_normalized(
            current_theme->header_shadow,
            mui->colors.header_shadow, 0.0f);
   hex32_to_rgba_normalized(
            current_theme->landscape_border_shadow,
            mui->colors.landscape_border_shadow_left, 0.0f);
   hex32_to_rgba_normalized(
            current_theme->landscape_border_shadow,
            mui->colors.landscape_border_shadow_right, 0.0f);
   hex32_to_rgba_normalized(
            current_theme->status_bar_shadow,
            mui->colors.status_bar_shadow, 0.0f);
   hex32_to_rgba_normalized(
            current_theme->selection_marker_shadow,
            mui->colors.selection_marker_shadow_top, 0.0f);
   hex32_to_rgba_normalized(
            current_theme->selection_marker_shadow,
            mui->colors.selection_marker_shadow_bottom, 0.0f);
   hex32_to_rgba_normalized(
            current_theme->scrollbar,
            mui->colors.scrollbar, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->divider,
            mui->colors.divider, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->divider,
            mui->colors.entry_divider, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->missing_thumbnail_icon,
            mui->colors.missing_thumbnail_icon, 1.0f);

   /* Have to record nominal screen fade opacity,
    * since the actual value is set dynamically
    * (based on animation transitions) */
   mui->colors.screen_fade_opacity = current_theme->screen_fade_opacity;
   hex32_to_rgba_normalized(
            current_theme->screen_fade,
            mui->colors.screen_fade, mui->colors.screen_fade_opacity);

   /* Shadow colours require special handling
    * (since they are gradients) */
   mui->colors.header_shadow[11]                 = current_theme->header_shadow_opacity;
   mui->colors.header_shadow[15]                 = current_theme->header_shadow_opacity;
   mui->colors.landscape_border_shadow_left[7]   = current_theme->landscape_border_shadow_opacity;
   mui->colors.landscape_border_shadow_left[15]  = current_theme->landscape_border_shadow_opacity;
   mui->colors.landscape_border_shadow_right[3]  = current_theme->landscape_border_shadow_opacity;
   mui->colors.landscape_border_shadow_right[11] = current_theme->landscape_border_shadow_opacity;
   mui->colors.landscape_border_shadow_opacity   = current_theme->landscape_border_shadow_opacity;
   mui->colors.status_bar_shadow[11]             = current_theme->status_bar_shadow_opacity;
   mui->colors.status_bar_shadow[15]             = current_theme->status_bar_shadow_opacity;
   mui->colors.status_bar_shadow_opacity         = current_theme->status_bar_shadow_opacity;
   mui->colors.selection_marker_shadow_top[11]   = current_theme->selection_marker_shadow_opacity;
   mui->colors.selection_marker_shadow_top[15]   = current_theme->selection_marker_shadow_opacity;
   mui->colors.selection_marker_shadow_bottom[3] = current_theme->selection_marker_shadow_opacity;
   mui->colors.selection_marker_shadow_bottom[7] = current_theme->selection_marker_shadow_opacity;
   mui->colors.selection_marker_shadow_opacity   = current_theme->selection_marker_shadow_opacity;

   /* Screensaver 'tint' */
   mui->colors.screensaver_tint = current_theme->screensaver_tint;

   /* Flags */
   mui->colors.divider_is_list_background = (current_theme->divider == current_theme->list_background);
}

static const char *materialui_texture_path(unsigned id)
{
   switch (id)
   {
      case MUI_TEXTURE_POINTER:
         return "pointer.png";
      case MUI_TEXTURE_BACK:
         return "back.png";
      case MUI_TEXTURE_SWITCH_ON:
         return "switch_on.png";
      case MUI_TEXTURE_SWITCH_OFF:
         return "switch_off.png";
      case MUI_TEXTURE_SWITCH_BG:
         return "switch_bg.png";
      case MUI_TEXTURE_TAB_MAIN:
         return "main_tab_passive.png";
      case MUI_TEXTURE_TAB_PLAYLISTS:
         return "playlists_tab_passive.png";
      case MUI_TEXTURE_TAB_SETTINGS:
         return "settings_tab_passive.png";
      case MUI_TEXTURE_TAB_BACK:
         return "back_tab.png";
      case MUI_TEXTURE_TAB_RESUME:
         return "resume_tab.png";
      case MUI_TEXTURE_KEY:
         return "key.png";
      case MUI_TEXTURE_KEY_HOVER:
         return "key-hover.png";
      case MUI_TEXTURE_FOLDER:
         return "folder.png";
      case MUI_TEXTURE_PARENT_DIRECTORY:
         return "parent_directory.png";
      case MUI_TEXTURE_IMAGE:
         return "image.png";
      case MUI_TEXTURE_VIDEO:
         return "video.png";
      case MUI_TEXTURE_MUSIC:
         return "music.png";
      case MUI_TEXTURE_ARCHIVE:
         return "archive.png";
      case MUI_TEXTURE_QUIT:
         return "quit.png";
      case MUI_TEXTURE_HELP:
         return "help.png";
      case MUI_TEXTURE_NETPLAY:
         return "netplay.png";
      case MUI_TEXTURE_CORES:
         return "cores.png";
      case MUI_TEXTURE_CONTROLS:
         return "controls.png";
      case MUI_TEXTURE_RESUME:
         return "resume.png";
      case MUI_TEXTURE_RESTART:
         return "restart.png";
      case MUI_TEXTURE_CLOSE:
         return "close.png";
      case MUI_TEXTURE_CORE_OPTIONS:
         return "core_options.png";
      case MUI_TEXTURE_CORE_CHEAT_OPTIONS:
         return "core_cheat_options.png";
      case MUI_TEXTURE_SHADERS:
         return "shaders.png";
      case MUI_TEXTURE_ADD_TO_FAVORITES:
         return "add_to_favorites.png";
      case MUI_TEXTURE_RUN:
         return "run.png";
      case MUI_TEXTURE_RENAME:
         return "rename.png";
      case MUI_TEXTURE_DATABASE:
         return "database.png";
      case MUI_TEXTURE_ADD_TO_MIXER:
         return "add_to_mixer.png";
      case MUI_TEXTURE_SCAN:
         return "scan.png";
      case MUI_TEXTURE_REMOVE:
         return "remove.png";
      case MUI_TEXTURE_START_CORE:
         return "start_core.png";
      case MUI_TEXTURE_LOAD_STATE:
         return "load_state.png";
      case MUI_TEXTURE_SAVE_STATE:
         return "save_state.png";
      case MUI_TEXTURE_DISK:
         return "disk.png";
      case MUI_TEXTURE_EJECT:
         return "eject.png";
      case MUI_TEXTURE_CHECKMARK:
         return "menu_check.png";
      case MUI_TEXTURE_UNDO_LOAD_STATE:
         return "undo_load_state.png";
      case MUI_TEXTURE_UNDO_SAVE_STATE:
         return "undo_save_state.png";
      case MUI_TEXTURE_STATE_SLOT:
         return "state_slot.png";
      case MUI_TEXTURE_TAKE_SCREENSHOT:
         return "take_screenshot.png";
      case MUI_TEXTURE_CONFIGURATIONS:
         return "configurations.png";
      case MUI_TEXTURE_LOAD_CONTENT:
         return "load_content.png";
      case MUI_TEXTURE_UPDATER:
         return "update.png";
      case MUI_TEXTURE_QUICKMENU:
         return "quickmenu.png";
      case MUI_TEXTURE_HISTORY:
         return "history.png";
      case MUI_TEXTURE_INFO:
         return "information.png";
      case MUI_TEXTURE_ADD:
         return "add.png";
      case MUI_TEXTURE_SETTINGS:
         return "settings.png";
      case MUI_TEXTURE_FILE:
         return "file.png";
      case MUI_TEXTURE_PLAYLIST:
         return "playlist.png";
      case MUI_TEXTURE_SEARCH:
         return "search.png";
      case MUI_TEXTURE_BATTERY_CRITICAL:
         return "battery_critical.png";
      case MUI_TEXTURE_BATTERY_20:
         return "battery_20.png";
      case MUI_TEXTURE_BATTERY_30:
         return "battery_30.png";
      case MUI_TEXTURE_BATTERY_50:
         return "battery_50.png";
      case MUI_TEXTURE_BATTERY_60:
         return "battery_60.png";
      case MUI_TEXTURE_BATTERY_80:
         return "battery_80.png";
      case MUI_TEXTURE_BATTERY_90:
         return "battery_90.png";
      case MUI_TEXTURE_BATTERY_100:
         return "battery_100.png";
      case MUI_TEXTURE_BATTERY_CHARGING:
         return "battery_charging.png";
      case MUI_TEXTURE_SWITCH_VIEW:
         return "switch_view.png";
   }

   return NULL;
}

static void INLINE materialui_font_bind(materialui_font_data_t *font_data)
{
   font_driver_bind_block(font_data->font, &font_data->raster_block);
   font_data->raster_block.carr.coords.vertices = 0;
}

static void INLINE materialui_font_unbind(materialui_font_data_t *font_data)
{
   font_driver_bind_block(font_data->font, NULL);
}

void materialui_font_flush(
      unsigned video_width, unsigned video_height,
      materialui_font_data_t *font_data)
{
   /* Flushing is slow - only do it if font
    * has actually been used */
   if (!font_data ||
       (font_data->raster_block.carr.coords.vertices == 0))
      return;

   font_driver_flush(video_width, video_height, font_data->font);
   font_data->raster_block.carr.coords.vertices = 0;
}

/* ==============================
 * Playlist icons START
 * ============================== */

static void materialui_context_destroy_playlist_icons(materialui_handle_t *mui)
{
   size_t i;

   for (i = 0; i < mui->textures.playlist.size; i++)
      video_driver_texture_unload(&mui->textures.playlist.icons[i].image);
}

static void materialui_context_reset_playlist_icons(materialui_handle_t *mui)
{
   size_t i;
   char icon_path[PATH_MAX_LENGTH];

   icon_path[0] = '\0';

   if (mui->textures.playlist.size < 1)
      return;

   /* Get icon directory */
   fill_pathname_application_special(
         icon_path, sizeof(icon_path),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_SYSICONS);

   if (string_is_empty(icon_path))
      return;

   /* Load icons
    * > Note that missing icons are ignored */
   for (i = 0; i < mui->textures.playlist.size; i++)
   {
      const char *image_file =
            mui->textures.playlist.icons[i].image_file;

      if (string_is_empty(image_file))
         continue;

      gfx_display_reset_textures_list(
            image_file, icon_path,
            &mui->textures.playlist.icons[i].image,
            TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL);
   }
}

static void materialui_free_playlist_icon_list(materialui_handle_t *mui)
{
   size_t i;

   for (i = 0; i < mui->textures.playlist.size; i++)
   {
      /* Ensure that any textures are unloaded
       * > Note: This should never be required.
       *   Loaded icons will always be 'freed' by
       *   materialui_context_destroy_playlist_icons() */
      if (mui->textures.playlist.icons[i].image)
         video_driver_texture_unload(&mui->textures.playlist.icons[i].image);

      /* Free file names */
      if (mui->textures.playlist.icons[i].playlist_file)
         free(mui->textures.playlist.icons[i].playlist_file);
      if (mui->textures.playlist.icons[i].image_file)
         free(mui->textures.playlist.icons[i].image_file);
      mui->textures.playlist.icons[i].playlist_file = NULL;
      mui->textures.playlist.icons[i].image_file    = NULL;
   }

   /* Free icons array and set list size to zero */
   if (mui->textures.playlist.icons)
      free(mui->textures.playlist.icons);

   mui->textures.playlist.icons = NULL;
   mui->textures.playlist.size  = 0;
}

static void materialui_refresh_playlist_icon_list(materialui_handle_t *mui,
      settings_t *settings)
{
   const char *dir_playlist      = settings ?
         settings->paths.directory_playlist : NULL;
   bool icons_enabled            = settings ?
         settings->bools.menu_materialui_icons_enable : false;
   bool playlist_icons_enabled   = settings ?
         settings->bools.menu_materialui_playlist_icons_enable : false;
   struct string_list *file_list = NULL;
   size_t i;

   /* Free existing icon list */
   materialui_free_playlist_icon_list(mui);

   /* If playlist icons are disabled, no further
    * action is required */
   if (!icons_enabled || !playlist_icons_enabled)
      goto end;

   /* Get list of .lpl files in playlists directory */
   if (string_is_empty(dir_playlist))
      goto end;

   file_list = dir_list_new_special(dir_playlist,
         DIR_LIST_COLLECTIONS, NULL, false);

   if (!file_list || (file_list->size < 1))
      goto end;

   /* Allocate icons array
    * > We may end up making this larger than
    *   necessary (if 'invalid' playlist files
    *   are included in the list), but this
    *   reduces code complexity */
   mui->textures.playlist.icons = (materialui_playlist_icon_t*)
         malloc(file_list->size * sizeof(materialui_playlist_icon_t));

   if (!mui->textures.playlist.icons)
      goto end;

   mui->textures.playlist.size  = file_list->size;

   for (i = 0; i < file_list->size; i++)
   {
      const char *path          = file_list->elems[i].data;
      const char *playlist_file = NULL;
      char image_file[PATH_MAX_LENGTH];

      image_file[0] = '\0';

      /* We used malloc() to create the icons
       * array - ensure struct members are
       * correctly initialised */
      mui->textures.playlist.icons[i].playlist_file = NULL;
      mui->textures.playlist.icons[i].image_file    = NULL;
      mui->textures.playlist.icons[i].image         = 0;

      /* dir_list_new_special() is 'well behaved'.
       * - It will only return file paths, not
       *   directories
       * - It will only return .lpl files
       * Only basic sanity checks are therefore
       * required */
      if (string_is_empty(path))
         continue;

      playlist_file = path_basename_nocompression(path);

      if (string_is_empty(playlist_file))
         continue;

      /* > Ignore history/favourites playlists */
      if (string_ends_with_size(playlist_file, "_history.lpl",
            strlen(playlist_file), STRLEN_CONST("_history.lpl")) ||
          string_is_equal(playlist_file,
               FILE_PATH_CONTENT_FAVORITES))
         continue;

      /* Playlist is valid - generate image file name */
      strlcpy(image_file, playlist_file, sizeof(image_file));
      path_remove_extension(image_file);
      strlcat(image_file, FILE_PATH_PNG_EXTENSION, sizeof(image_file));

      if (string_is_empty(image_file))
         continue;

      /* All good - cache paths */
      mui->textures.playlist.icons[i].playlist_file = strdup(playlist_file);
      mui->textures.playlist.icons[i].image_file    = strdup(image_file);
   }

end:
   if (file_list)
      string_list_free(file_list);
}

static void materialui_set_node_playlist_icon(
      materialui_handle_t *mui, materialui_node_t* node,
      const char *playlist_path)
{
   const char *playlist_file = NULL;
   size_t i;

   /* Set defaults */
   node->icon_texture_index = MUI_TEXTURE_PLAYLIST;
   node->icon_type          = MUI_ICON_TYPE_INTERNAL;

   if (mui->textures.playlist.size < 1)
      return;

   /* Get playlist file name */
   if (string_is_empty(playlist_path))
      return;

   playlist_file = path_basename_nocompression(playlist_path);

   if (string_is_empty(playlist_path))
      return;

   /* Search icon list for specified file */
   for (i = 0; i < mui->textures.playlist.size; i++)
   {
      const char *icon_playlist_file =
            mui->textures.playlist.icons[i].playlist_file;

      if (string_is_empty(icon_playlist_file))
         continue;

      if (string_is_equal(playlist_file, icon_playlist_file))
      {
         node->icon_texture_index = (unsigned)i;
         node->icon_type          = MUI_ICON_TYPE_PLAYLIST;
         break;
      }
   }
}

static uintptr_t materialui_get_playlist_icon(
      materialui_handle_t *mui, unsigned texture_index)
{
   uintptr_t playlist_icon;

   /* Always use MUI_TEXTURE_PLAYLIST as
    * a fallback */
   if (texture_index >= mui->textures.playlist.size)
      return mui->textures.list[MUI_TEXTURE_PLAYLIST];

   playlist_icon = mui->textures.playlist.icons[texture_index].image;

   return playlist_icon ? playlist_icon : mui->textures.list[MUI_TEXTURE_PLAYLIST];
}

/* ==============================
 * Playlist icons END
 * ============================== */

static void materialui_context_reset_textures(materialui_handle_t *mui)
{
   bool has_all_assets = true;
   char icon_path[PATH_MAX_LENGTH];
   unsigned i;

   icon_path[0] = '\0';

   fill_pathname_application_special(
         icon_path, sizeof(icon_path),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_ICONS);

   /* Loop through all textures */
   for (i = 0; i < MUI_TEXTURE_LAST; i++)
   {
      if (!gfx_display_reset_textures_list(
            materialui_texture_path(i), icon_path, &mui->textures.list[i],
            TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
      {
         RARCH_WARN("[GLUI]: Asset missing: \"%s%s%s\".\n", icon_path,
               PATH_DEFAULT_SLASH(), materialui_texture_path(i));
         has_all_assets = false;
      }
   }

   /* Warn user if assets are missing */
   if (!has_all_assets)
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_MISSING_ASSETS), 1, 256, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

static void materialui_draw_icon(
      void *userdata,
      gfx_display_t *p_disp,
      unsigned video_width,
      unsigned video_height,
      unsigned icon_size,
      uintptr_t texture,
      float x, float y,
      float rotation, float scale_factor,
      float *color,
      math_matrix_4x4 *mymat)
{
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   draw.x               = x;
   draw.y               = video_height - y - icon_size;
   draw.width           = icon_size;
   draw.height          = icon_size;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = mymat;
   draw.texture         = texture;
   draw.prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline_id     = 0;

   if (dispctx)
   {
      if (dispctx->draw)
         if (draw.height > 0 && draw.width > 0)
            dispctx->draw(&draw, userdata, video_width, video_height);
      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }
}

static void materialui_draw_thumbnail(
      materialui_handle_t *mui,
      gfx_thumbnail_t *thumbnail,
      settings_t *settings,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      float x, float y,
      float scale_factor,
      math_matrix_4x4 *mymat)
{
   float bg_x;
   float bg_y;
   float bg_width;
   float bg_height;

   /* Sanity check */
   if (scale_factor <= 0)
      return;

   /* Get background draw position + dimensions,
    * accounting for scale factor */
   bg_width  = (float)mui->thumbnail_width_max * scale_factor;
   bg_height = (float)mui->thumbnail_height_max * scale_factor;
   bg_x      = x - (bg_width - (float)mui->thumbnail_width_max) / 2.0f;
   bg_y      = y - (bg_height - (float)mui->thumbnail_height_max) / 2.0f;

   /* If thumbnail is missing, draw fallback image... */
   switch (thumbnail->status)
   {
      case GFX_THUMBNAIL_STATUS_MISSING:
         {
            float icon_size = (float)mui->icon_size;
            float alpha     = mui->transition_alpha * thumbnail->alpha;

            if (alpha <= 0.0f)
               return;

            /* Adjust icon size based on scale factor */
            if (scale_factor < 1.0f)
               icon_size *= scale_factor;

            /* Background */
            gfx_display_set_alpha(
                  mui->colors.thumbnail_background, alpha);

            gfx_display_draw_quad(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                  bg_x,
                  bg_y,
                  (unsigned)bg_width,
                  (unsigned)bg_height,
                  video_width,
                  video_height,
                  mui->colors.thumbnail_background,
                  NULL);

            /* Icon */
            gfx_display_set_alpha(
                  mui->colors.missing_thumbnail_icon, alpha);

            materialui_draw_icon(
                  userdata, p_disp,
                  video_width,
                  video_height,
                  (unsigned)icon_size,
                  mui->textures.list[MUI_TEXTURE_IMAGE],
                  bg_x + (bg_width - icon_size) / 2.0f,
                  bg_y + (bg_height - icon_size) / 2.0f,
                  0.0f,
                  1.0f,
                  mui->colors.missing_thumbnail_icon,
                  mymat);
         }
         break;
      case GFX_THUMBNAIL_STATUS_AVAILABLE:
         /* If thumbnail is available, draw it
          * > Note that other conditions are ignored - i.e. we
          *   we draw nothing if thumbnail status is unknown,
          *   or we are waiting for a thumbnail to load) */
         {
            /* Background */
            if (settings &&
                  settings->bools.menu_materialui_thumbnail_background_enable)
            {
               /* > If enabled by the user, we draw a background here
                *   to ensure a uniform visual appearance regardless
                *   of thumbnail size
                * NOTE: Have to round up and add 1 to height,
                * otherwise background and thumbnail have obvious
                * misalignment (due to various rounding errors...) */

               /* > Set background alpha
                *   - Can't do this in materialui_colors_set_transition_alpha()
                *     because it's dependent upon thumbnail opacity
                *   - No need to restore the original alpha value, since it is
                *     always set 'manually' before use */
               gfx_display_set_alpha(
                     mui->colors.thumbnail_background,
                     mui->transition_alpha * thumbnail->alpha);

               /* > Draw background quad */
               gfx_display_draw_quad(
                     p_disp,
                     userdata,
                     video_width,
                     video_height,
                     (int)bg_x,
                     (int)bg_y,
                     (unsigned)(bg_width + 0.5f),
                     (unsigned)(bg_height + 1.5f),
                     video_width,
                     video_height,
                     mui->colors.thumbnail_background,
                     NULL);
            }

            /* Thumbnail */
            gfx_thumbnail_draw(
                  userdata,
                  video_width,
                  video_height,
                  thumbnail,
                  x, y, mui->thumbnail_width_max, mui->thumbnail_height_max,
                  GFX_THUMBNAIL_ALIGN_CENTRE,
                  mui->transition_alpha, scale_factor, NULL);
         }
         break;
      case GFX_THUMBNAIL_STATUS_UNKNOWN:
      default:
         break;
   }
}

static void materialui_get_message(void *data, const char *message)
{
   materialui_handle_t *mui   = (materialui_handle_t*)data;

   if (!mui || !message || !*message)
      return;

   mui->msgbox[0] = '\0';

   if (!string_is_empty(message))
      strlcpy(mui->msgbox, message, sizeof(mui->msgbox));
}

static void materialui_render_messagebox(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int y_centre, const char *message)
{
   unsigned i;
   int x                    = 0;
   int y                    = 0;
   int usable_width         = 0;
   int longest_width        = 0;
   struct string_list list  = {0};
   char wrapped_message[MENU_SUBLABEL_MAX_LENGTH];

   wrapped_message[0] = '\0';

   /* Sanity check */
   if (string_is_empty(message) ||
       !mui ||
       !mui->font_data.list.font)
      return;

   usable_width = (int)video_width - (mui->margin * 4.0);

   if (usable_width < 1)
      return;

   /* Split message into lines */
   (mui->word_wrap)(
         wrapped_message, sizeof(wrapped_message), message,
         usable_width / (int)mui->font_data.list.glyph_width,
         mui->font_data.list.wideglyph_width, 0);

   string_list_initialize(&list);
   if (
            !string_split_noalloc(&list, wrapped_message, "\n")
         || list.elems == 0)
   {
      string_list_deinitialize(&list);
      return;
   }

   /* Get coordinates of message box centre */
   x = video_width / 2;
   y = (int)(y_centre - (list.size * mui->font_data.list.line_height) / 2);

   /* TODO/FIXME: Reduce text scale if width or height
    * are too large to fit on screen */

   /* Find the longest line width */
   for (i = 0; i < list.size; i++)
   {
      const char *line = list.elems[i].data;

      if (!string_is_empty(line))
      {
         int width = font_driver_get_message_width(
               mui->font_data.list.font, line, (unsigned)strlen(line), 1);

         longest_width = (width > longest_width) ?
               width : longest_width;
      }
   }

   /* Draw message box background */
   gfx_display_set_alpha(
         mui->colors.surface_background, mui->transition_alpha);

   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x - longest_width / 2.0 - mui->margin * 2.0,
         y - mui->margin * 2.0,
         longest_width + mui->margin * 4.0,
         mui->font_data.list.line_height * list.size + mui->margin * 4.0,
         video_width,
         video_height,
         mui->colors.surface_background,
         NULL);

   /* Print each line of the message */
   for (i = 0; i < list.size; i++)
   {
      const char *line = list.elems[i].data;

      if (!string_is_empty(line))
         gfx_display_draw_text(
               mui->font_data.list.font, line,
               x - longest_width / 2.0f,
               y + (i * mui->font_data.list.line_height) + mui->font_data.list.line_ascender,
               video_width, video_height, mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0.0f, true);
   }

   string_list_deinitialize(&list);
}

/* Initialises scrollbar parameters (width/height) */
static void materialui_scrollbar_init(
      materialui_handle_t* mui,
      unsigned width, unsigned height, unsigned header_height)
{
   int view_height = (int)height - (int)header_height -
         (int)mui->nav_bar_layout_height - (int)mui->status_bar.height;
   int scrollbar_height;

   /* Set initial defaults */
   mui->scrollbar.width   = mui->dip_base_unit_size / 36;
   mui->scrollbar.height  = 0;
   mui->scrollbar.x       = 0;
   mui->scrollbar.y       = 0;
   mui->scrollbar.active  = false;
   mui->scrollbar.dragged = false;

   /* If current window is too small to show any content
    * (unlikely) or all entries currently fit on a single
    * screen, scrollbar is disabled - can return immediately */
   if ((view_height <= 0) ||
       (mui->content_height <= (float)view_height))
      return;

   /* If we pass the above check, scrollbar is enabled */
   mui->scrollbar.active = true;

   /* Get scrollbar height */
   scrollbar_height = (int)((float)(view_height * view_height) / mui->content_height);

   /* > Apply vertical padding to improve visual appearance */
   scrollbar_height -= (int)mui->scrollbar.width * 2;

   /* > If the scrollbar is extremely short, display
    *   it as a square */
   if (scrollbar_height < (int)mui->scrollbar.width)
      scrollbar_height = (int)mui->scrollbar.width;

   mui->scrollbar.height = (unsigned)scrollbar_height;

   /* X and Y position are dynamic, and must be
    * set elsewhere */
}

/* ==============================
 * materialui_compute_entries_box() START
 * ============================== */

/* Calculate physical size of each menu entry, plus
 * any derived screen dimensions of menu list elements */

/* Utility functions */

/* > Returns number of lines in a string */
static unsigned materialui_count_lines(const char *str)
{
   unsigned c     = 0;
   unsigned lines = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

/* > Returns number of lines required to display
 *   the sublabel of entry 'entry_idx' */
static unsigned materialui_count_sublabel_lines(
      materialui_handle_t* mui, int usable_width,
      size_t entry_idx, bool has_icon)
{
   menu_entry_t entry;
   char wrapped_sublabel_str[MENU_SUBLABEL_MAX_LENGTH];
   int sublabel_width_max   = 0;

   wrapped_sublabel_str[0] = '\0';

   /* Get entry sublabel */
   MENU_ENTRY_INIT(entry);
   entry.path_enabled       = false;
   entry.label_enabled      = false;
   entry.rich_label_enabled = false;
   entry.value_enabled      = false;
   menu_entry_get(&entry, 0, entry_idx, NULL, true);

   /* If sublabel is empty, return immediately */
   if (string_is_empty(entry.sublabel))
      return 0;

   /* Wrap sublabel string to fit available width */
   sublabel_width_max = usable_width - (int)mui->sublabel_padding -
         (has_icon ? (int)mui->icon_size : 0);

   (mui->word_wrap)(
         wrapped_sublabel_str, sizeof(wrapped_sublabel_str), entry.sublabel,
         sublabel_width_max / (int)mui->font_data.hint.glyph_width,
         mui->font_data.hint.wideglyph_width, 0);

   /* Return number of lines in wrapped string */
   return materialui_count_lines(wrapped_sublabel_str);
}

/* Used for standard, non-playlist entries
 * > MUI_LIST_VIEW_DEFAULT */
static void materialui_compute_entries_box_default(
      materialui_handle_t* mui,
      unsigned width, unsigned height, unsigned header_height)
{
   size_t i;
   file_list_t *list      = menu_entries_get_selection_buf_ptr(0);
   float node_entry_width = (float)width -
         (float)(mui->landscape_optimization.border_width * 2) -
         (float)mui->nav_bar_layout_width;
   float node_x           = (float)mui->landscape_optimization.border_width;
   int usable_width       = node_entry_width -
         (int)(mui->margin * 2) -
         (int)(mui->landscape_optimization.entry_margin * 2);
   float sum              = 0;
   size_t entries_end     = menu_entries_get_size();

   if (!list)
      return;

   for (i = 0; i < entries_end; i++)
   {
      unsigned num_sublabel_lines = 0;
      materialui_node_t *node     = (materialui_node_t*)list->list[i].userdata;
      bool has_icon               = false;

      if (!node)
         continue;

      switch (node->icon_type)
      {
         case MUI_ICON_TYPE_INTERNAL:
            has_icon = mui->textures.list[node->icon_texture_index] != 0;
            break;
         case MUI_ICON_TYPE_MENU_EXPLORE:
         case MUI_ICON_TYPE_MENU_CONTENTLESS_CORE:
            has_icon = true;
            break;
         case MUI_ICON_TYPE_PLAYLIST:
            has_icon = materialui_get_playlist_icon(
                  mui, node->icon_texture_index) != 0;
            break;
         default:
            break;
      }

      num_sublabel_lines = materialui_count_sublabel_lines(
            mui, usable_width, i, has_icon);

      node->text_height  = mui->font_data.list.line_height +
            (num_sublabel_lines * mui->font_data.hint.line_height);

      node->entry_height = node->text_height +
            mui->dip_base_unit_size / 10;

      node->entry_height      += mui->dip_base_unit_size / 10;
      node->y                  = sum;

      node->entry_width        = node_entry_width;
      node->x                  = node_x;

      sum                     += node->entry_height;
   }

   mui->content_height = sum;

   /* Total height is now known - can initialise scrollbar */
   materialui_scrollbar_init(mui, width, height, header_height);
}

/* Used for playlist 'list view' (with and without
 * thumbnails) entries
 * > MUI_LIST_VIEW_PLAYLIST
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE */
static void materialui_compute_entries_box_playlist_list(
      materialui_handle_t* mui,
      unsigned width, unsigned height, unsigned header_height)
{
   size_t i;
   file_list_t *list      = menu_entries_get_selection_buf_ptr(0);
   float node_entry_width = (float)width -
         (float)(mui->landscape_optimization.border_width * 2) -
         (float)mui->nav_bar_layout_width;
   float node_x           = (float)mui->landscape_optimization.border_width;
   int usable_width       = node_entry_width - (int)(mui->margin * 2);
   float sum              = 0;
   size_t entries_end     = menu_entries_get_size();

   if (!list)
      return;

   /* If thumbnails are *not* enabled, decrease usable
    * width by landscape optimisation entry margin */
   if (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST)
      usable_width -= (int)(mui->landscape_optimization.entry_margin * 2);
   /* If a thumbnail view mode is enabled, must reduce
    * usable width by thumbnail width */
   else
   {
      int thumbnail_margin = 0;

      /* Account for additional padding in portrait mode */
      if (mui->is_portrait)
      {
         if (mui->secondary_thumbnail_enabled)
            thumbnail_margin = (int)mui->scrollbar.width;
      }
      /* Account for additional padding in landscape mode */
      else
         thumbnail_margin = (int)mui->margin;

      usable_width -= mui->thumbnail_width_max + thumbnail_margin;

      /* Account for second thumbnail, if enabled */
      if (mui->secondary_thumbnail_enabled)
         usable_width -= mui->thumbnail_width_max + thumbnail_margin;
   }

   for (i = 0; i < entries_end; i++)
   {
      unsigned num_sublabel_lines = 0;
      materialui_node_t *node     = (materialui_node_t*)list->list[i].userdata;

      if (!node)
         continue;

      num_sublabel_lines = materialui_count_sublabel_lines(
            mui, usable_width, i, false);

      node->text_height  = mui->font_data.list.line_height +
            (num_sublabel_lines * mui->font_data.hint.line_height);

      node->entry_height = node->text_height +
            mui->dip_base_unit_size / 10;

      /* If thumbnails are enabled, must ensure
       * that line_height is greater than maximum
       * thumbnail height */
      if (mui->list_view_type != MUI_LIST_VIEW_PLAYLIST)
         node->entry_height = (node->entry_height < mui->thumbnail_height_max) ?
               mui->thumbnail_height_max : node->entry_height;

      node->entry_height      += mui->dip_base_unit_size / 10;
      node->y                  = sum;

      node->entry_width        = node_entry_width;
      node->x                  = node_x;

      sum                     += node->entry_height;
   }

   mui->content_height = sum;

   /* Total height is now known - can initialise scrollbar */
   materialui_scrollbar_init(mui, width, height, header_height);
}

/* Used for playlist 'dual icon' entries
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON */
static void materialui_compute_entries_box_playlist_dual_icon(
      materialui_handle_t* mui,
      unsigned width, unsigned height, unsigned header_height)
{
   size_t i;
   file_list_t *list       = menu_entries_get_selection_buf_ptr(0);
   float node_entry_width  = (float)width -
         (float)(mui->landscape_optimization.border_width * 2) -
         (float)mui->nav_bar_layout_width;
   float node_x            = (float)mui->landscape_optimization.border_width;
   /* Entry height is constant:
    * > One line of list text */
   float node_text_height  = (float)mui->font_data.list.line_height;
   /* > List text + thumbnail height + padding */
   float node_entry_height = node_text_height +
         (float)mui->thumbnail_height_max +
         ((float)mui->dip_base_unit_size / 5.0f);
   float sum               = 0;
   size_t entries_end      = menu_entries_get_size();

   if (!list)
      return;

   for (i = 0; i < entries_end; i++)
   {
      materialui_node_t *node = (materialui_node_t*)list->list[i].userdata;

      if (!node)
         continue;

      node->text_height  = node_text_height;
      node->entry_width  = node_entry_width;
      node->entry_height = node_entry_height;
      node->x            = node_x;
      node->y            = sum;
      sum               += node_entry_height;
   }

   mui->content_height = sum;

   /* Total height is now known - can initialise scrollbar */
   materialui_scrollbar_init(mui, width, height, header_height);
}

/* Used for playlist 'desktop'-layout entries
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP */
static void materialui_compute_entries_box_playlist_desktop(
      materialui_handle_t* mui,
      unsigned width, unsigned height, unsigned header_height)
{
   size_t i;
   file_list_t *list       = menu_entries_get_selection_buf_ptr(0);
   /* Entry width is available screen width minus
    * thumbnail sidebar
    * > Note: If landscape optimisations are enabled,
    *   need to allow space for a second divider at
    *   the left hand edge of the sidebar */
   float node_entry_width  = (float)width -
         (float)(mui->landscape_optimization.border_width * 2) -
         (float)mui->nav_bar_layout_width -
         (float)mui->thumbnail_width_max -
         (float)(mui->margin * 2) -
         (float)(mui->entry_divider_width *
               (mui->landscape_optimization.enabled ?
                     2 : 1));
   /* Entry x position is the right hand edge of
    * the thumbnail sidebar */
   float node_x            = (float)mui->landscape_optimization.border_width +
         (float)mui->thumbnail_width_max + (float)(mui->margin * 2) +
         (float)(mui->entry_divider_width *
               (mui->landscape_optimization.enabled ?
                     2 : 1));
   /* Entry height:
    * > One line of list text */
   float node_text_height  = (float)mui->font_data.list.line_height;
   /* > List text + padding
    *   Note: Since this is intended for the desktop,
    *   use less padding than normal to increase list
    *   density (each entry will still be large enough
    *   to select with a finger via touchscreen, but
    *   this is optimised for gamepad/keyboard) */
   float node_entry_height = node_text_height +
         ((float)mui->dip_base_unit_size / 7.0f);
   float sum               = 0;
   size_t entries_end      = menu_entries_get_size();

   if (!list)
      return;

   for (i = 0; i < entries_end; i++)
   {
      materialui_node_t *node = (materialui_node_t*)list->list[i].userdata;

      if (!node)
         continue;

      node->text_height  = node_text_height;
      node->entry_width  = node_entry_width;
      node->entry_height = node_entry_height;
      node->x            = node_x;
      node->y            = sum;
      sum               += node_entry_height;
   }

   mui->content_height = sum;

   /* Total height is now known - can initialise scrollbar */
   materialui_scrollbar_init(mui, width, height, header_height);
}

static void (*materialui_compute_entries_box)(
      materialui_handle_t* mui,
      unsigned width, unsigned height, unsigned header_height) = materialui_compute_entries_box_default;

/* ==============================
 * materialui_compute_entries_box() END
 * ============================== */

/* Compute the scroll value depending on the highlighted entry */
static float materialui_get_scroll(materialui_handle_t *mui,
      gfx_display_t *p_disp)
{
   file_list_t *list       = menu_entries_get_selection_buf_ptr(0);
   materialui_node_t *node = NULL;
   size_t selection        = menu_navigation_get_selection();
   unsigned header_height  = p_disp->header_height;
   unsigned width          = 0;
   unsigned height         = 0;
   float view_centre       = 0.0f;
   float selection_centre  = 0.0f;
   size_t i;

   if (!mui || !list)
      return 0;

   /* Get current window size */
   video_driver_get_size(&width, &height);

   /* Get the vertical midpoint of the actual
    * list view - i.e. account for header +
    * navigation bar */
   view_centre = (float)(height - header_height - mui->nav_bar_layout_height -
         mui->status_bar.height) / 2.0f;

   /* Get the vertical midpoint of the currently
    * selected entry */

   /* > Account for entries *before* current selection */
   for (i = 0; i < selection; i++)
   {
      node = (materialui_node_t*)list->list[i].userdata;

      /* If this ever happens, the scroll position
       * will be entirely wrong... */
      if (!node)
         continue;

      selection_centre += node->entry_height;
   }

   /* > Account for current selection */
   node = (materialui_node_t*)list->list[selection].userdata;
   if (node)
      selection_centre += node->entry_height / 2.0f;

   /* If selected entry is near the beginning of the list
    * (such that it fits within the first half of the
    * list view when the list is rendered from the start),
    * scroll position can be reset to zero */
   if (selection_centre < view_centre)
      return 0.0f;

   /* ...Otherwise, set the scroll position such that the
    * centre of the selected item is at the centre of the
    * list view */
   return selection_centre - view_centre;
}

/* Returns true if specified entry is currently
 * displayed on screen */
static bool INLINE materialui_entry_onscreen(
      materialui_handle_t *mui, size_t idx)
{
   return (idx >= mui->first_onscreen_entry) &&
         (idx <= mui->last_onscreen_entry);
}

/* If currently selected entry is off screen,
 * moves selection to specified on screen target
 * > Does nothing if currently selected item is
 *   already on screen
 * > Does nothing if we are already scrolling
 *   towards currently selected item
 * > Returns index of selected item */
static size_t materialui_auto_select_onscreen_entry(
      materialui_handle_t *mui,
      enum materialui_onscreen_entry_position_type target_entry)
{
   size_t selection = menu_navigation_get_selection();

   /* Check whether selected item is already on screen */
   if (materialui_entry_onscreen(mui, selection))
      return selection;

   /* If selected item is off screen but we are
    * currently scrolling towards it (via an animation),
    * no action is required */
   if (mui->scroll_animation_active &&
       (mui->scroll_animation_selection == selection))
      return selection;

   /* Update selection index */
   switch (target_entry)
   {
      case MUI_ONSCREEN_ENTRY_FIRST:
         selection = mui->first_onscreen_entry;
         break;
      case MUI_ONSCREEN_ENTRY_LAST:
         selection = mui->last_onscreen_entry;
         break;
      case MUI_ONSCREEN_ENTRY_CENTRE:
      default:
         selection = (mui->first_onscreen_entry >> 1) +
               (mui->last_onscreen_entry >> 1);
         break;
   }

   /* Apply new selection */
   menu_navigation_set_selection(selection);

   return selection;
}

/* Kills any existing scroll animation and
 * resets scroll acceleration */
static INLINE void materialui_kill_scroll_animation(
      materialui_handle_t *mui)
{
   uintptr_t scroll_tag = (uintptr_t)&mui->scroll_y;

   gfx_animation_kill_by_tag(&scroll_tag);
   menu_input_set_pointer_y_accel(0.0f);

   mui->scroll_animation_active    = false;
   mui->scroll_animation_selection = 0;
}

/* ==============================
 * materialui_render_process_entry() START
 * ============================== */

/* Handles any auxiliary entry-specific processing
 * required during the per-frame 'materialui_render()'
 * task. This typically involves the loading/unloading
 * of playlist thumbnails.
 * > Should be called within a loop over all entries
 *   in the current menu list
 * > If return value is false, loop should break
 *   (i.e. indicates that last entry to be processed
 *   has been found) */

/* Used for non-playlist menus, and playlists
 * without thumbnails (i.e. any list without
 * thumbnails)
 * > MUI_LIST_VIEW_DEFAULT
 * > MUI_LIST_VIEW_PLAYLIST
 * Returns false when the last on-screen entry
 * is detected */
static bool materialui_render_process_entry_default(
      materialui_handle_t* mui,
      materialui_node_t *node,
      size_t entry_idx, size_t selection, size_t playlist_idx,
      bool first_entry_found, bool last_entry_found,
      unsigned thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   /* 'Normal' menu lists require no entry-specific
    * processing */
   return !last_entry_found;
}

/* Used for 'list view' playlists *with*
 * thumbnails
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE
 * Always returns true */
static bool materialui_render_process_entry_playlist_thumb_list(
      materialui_handle_t* mui,
      materialui_node_t *node,
      size_t entry_idx, size_t selection, size_t playlist_idx,
      bool first_entry_found, bool last_entry_found,
      unsigned thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   bool on_screen = first_entry_found && !last_entry_found;
   gfx_animation_t     *p_anim = anim_get_ptr();

   /* Load thumbnails for all on-screen entries
    * and free thumbnails for all off-screen entries */
   if (mui->secondary_thumbnail_enabled)
      gfx_thumbnail_process_streams(
            mui->thumbnail_path_data,
            p_anim,
            mui->playlist, playlist_idx,
            &node->thumbnails.primary,
            &node->thumbnails.secondary,
            on_screen,
            thumbnail_upscale_threshold,
            network_on_demand_thumbnails);
   else
      gfx_thumbnail_process_stream(
            mui->thumbnail_path_data,
            p_anim,
            GFX_THUMBNAIL_RIGHT,
            mui->playlist, playlist_idx,
            &node->thumbnails.primary,
            on_screen,
            thumbnail_upscale_threshold,
            network_on_demand_thumbnails);

   /* Always return true - every entry must
    * be processed */
   return true;
}

/* Used for 'dual icon' playlists
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON
 * Always returns true */
static bool materialui_render_process_entry_playlist_dual_icon(
      materialui_handle_t* mui,
      materialui_node_t *node,
      size_t entry_idx, size_t selection, size_t playlist_idx,
      bool first_entry_found, bool last_entry_found,
      unsigned thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   gfx_animation_t *p_anim        = anim_get_ptr();
   bool on_screen                 = first_entry_found && !last_entry_found;

   /* Load thumbnails for all on-screen entries
    * and free thumbnails for all off-screen entries
    * > Note that secondary thumbnail is force
    *   enabled in dual icon mode */
   gfx_thumbnail_process_streams(
         mui->thumbnail_path_data,
         p_anim,
         mui->playlist, playlist_idx,
         &node->thumbnails.primary,
         &node->thumbnails.secondary,
         on_screen,
         thumbnail_upscale_threshold,
         network_on_demand_thumbnails);

   /* Always return true - every entry must
    * be processed */
   return true;
}

/* Used for 'desktop'-layout playlists
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP
 * Always returns true */
static bool materialui_render_process_entry_playlist_desktop(
      materialui_handle_t* mui,
      materialui_node_t *node,
      size_t entry_idx, size_t selection, size_t playlist_idx,
      bool first_entry_found, bool last_entry_found,
      unsigned thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails)
{
   gfx_animation_t *p_anim            = anim_get_ptr();
   bool is_selected                   = (entry_idx == selection);
   /* We want to load (and keep in memory)
    * thumbnails for the currently selected
    * entry *and* the last entry for which
    * thumbnail data was available. This allows
    * us to keep showing 'old' thumbnails in the
    * sidebar while waiting for new ones to load
    * (otherwise the sidebar is left blank,
    * which looks ugly...) */
   bool is_on_screen = is_selected ||
         (entry_idx == mui->desktop_thumbnail_last_selection);

   /* Load thumbnails for selected (and last
    * selected) entry and free thumbnails for
    * all other entries
    * > Note that secondary thumbnail is force
    *   enabled */
   gfx_thumbnail_process_streams(
         mui->thumbnail_path_data,
         p_anim,
         mui->playlist, playlist_idx,
         &node->thumbnails.primary,
         &node->thumbnails.secondary,
         is_on_screen,
         thumbnail_upscale_threshold,
         network_on_demand_thumbnails);

   /* If this is *not* the currently selected
    * entry, then our work is done */
   if (!is_selected)
      return true;

   /* If thumbnails have been requested for the
    * selected entry, then it has valid content
    * to display in the sidebar -> cache this as
    * the 'last selected' entry */
   if ((node->thumbnails.primary.status   != GFX_THUMBNAIL_STATUS_UNKNOWN) &&
       (node->thumbnails.secondary.status != GFX_THUMBNAIL_STATUS_UNKNOWN))
      mui->desktop_thumbnail_last_selection = selection;

   /* Fetch metadata for selected entry */
   if (mui->status_bar.enabled)
   {
      uintptr_t alpha_tag = (uintptr_t)&mui->status_bar.alpha;

      /* Reset metadata if current selection
       * has changed */
      if (selection != mui->status_bar.last_selected)
      {
         gfx_animation_kill_by_tag(&alpha_tag);

         mui->status_bar.cached        = false;
         mui->status_bar.last_selected = selection;
         mui->status_bar.delay_timer   = 0.0f;
         mui->status_bar.alpha         = 0.0f;
         mui->status_bar.str[0]        = '\0';
      }

      /* Check whether metadata needs to be cached */
      if (!mui->status_bar.cached)
      {
         gfx_animation_t *p_anim        = anim_get_ptr();
         /* Check if delay timer has elapsed */
         mui->status_bar.delay_timer   += p_anim->delta_time;

         if (mui->status_bar.delay_timer > mui->thumbnail_stream_delay)
         {
            settings_t *settings               = config_get_ptr();
            bool content_runtime_log           = settings->bools.content_runtime_log;
            bool content_runtime_log_aggregate = settings->bools.content_runtime_log_aggregate;
            const char *directory_runtime_log  = settings->paths.directory_runtime_log;
            const char *directory_playlist     = settings->paths.directory_playlist;
            unsigned runtime_type              = settings->uints.playlist_sublabel_runtime_type;
            enum playlist_sublabel_last_played_style_type
                  runtime_last_played_style    =
                        (enum playlist_sublabel_last_played_style_type)
                              settings->uints.playlist_sublabel_last_played_style;
            enum playlist_sublabel_last_played_date_separator_type
                  runtime_date_separator       =
                        (enum playlist_sublabel_last_played_date_separator_type)
                              settings->uints.menu_timedate_date_separator;
            float fade_duration                = gfx_thumb_get_ptr()->fade_duration;
            const struct playlist_entry *entry = NULL;
            const char *core_name              = NULL;
            const char *runtime_str            = NULL;
            const char *last_played_str        = NULL;

            /* Read playlist entry */
            playlist_get_index(mui->playlist, playlist_idx, &entry);

            /* Sanity check */
            if (!entry)
            {
               /* If this happens, then everything is
                * broken - just ensure that metadata
                * string is NUL and set cached status
                * to 'true' to avoid reading this
                * broken playlist again... */
               mui->status_bar.str[0] = '\0';
               mui->status_bar.cached = true;
               return true;
            }

            /* Get core name */
            if (string_is_empty(entry->core_name) ||
                string_is_equal(entry->core_name, FILE_PATH_DETECT))
               core_name = msg_hash_to_str(MSG_AUTODETECT);
            else
               core_name = entry->core_name;

            /* Get runtime info, if available */
            if (content_runtime_log || content_runtime_log_aggregate)
            {
               if (entry->runtime_status == PLAYLIST_RUNTIME_UNKNOWN)
                  runtime_update_playlist(
                        mui->playlist, playlist_idx,
                        directory_runtime_log,
                        directory_playlist,
                        (runtime_type == PLAYLIST_RUNTIME_PER_CORE),
                        runtime_last_played_style,
                        runtime_date_separator);

               if (!string_is_empty(entry->runtime_str))
                  runtime_str = entry->runtime_str;

               if (!string_is_empty(entry->last_played_str))
                  last_played_str = entry->last_played_str;
            }

            /* Set fallback strings, if required */
            if (string_is_empty(runtime_str))
               runtime_str = mui->status_bar.runtime_fallback_str;

            if (string_is_empty(last_played_str))
               last_played_str = mui->status_bar.last_played_fallback_str;

            /* Generate metadata string */
            snprintf(mui->status_bar.str, sizeof(mui->status_bar.str),
                  "%s %s%s%s%s%s",
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE),
                  core_name,
                  MUI_TICKER_SPACER,
                  runtime_str,
                  MUI_TICKER_SPACER,
                  last_played_str);

            /* All metadata is cached */
            mui->status_bar.cached = true;

            /* Trigger fade in animation */
            if (fade_duration > 0.0f)
            {
               gfx_animation_ctx_entry_t animation_entry;

               mui->status_bar.alpha = 0.0f;

               animation_entry.easing_enum      = EASING_OUT_QUAD;
               animation_entry.tag              = alpha_tag;
               animation_entry.duration         = fade_duration;
               animation_entry.target_value     = 1.0f;
               animation_entry.subject          = &mui->status_bar.alpha;
               animation_entry.cb               = NULL;
               animation_entry.userdata         = NULL;

               gfx_animation_push(&animation_entry);
            }
            else
               mui->status_bar.alpha = 1.0f;
         }
      }
   }

   /* Always return true - every entry must
    * be processed */
   return true;
}

static bool (*materialui_render_process_entry)(
      materialui_handle_t* mui,
      materialui_node_t *node,
      size_t entry_idx, size_t selection, size_t playlist_idx,
      bool first_entry_found, bool last_entry_found,
      unsigned thumbnail_upscale_threshold,
      bool network_on_demand_thumbnails) = materialui_render_process_entry_default;

/* ==============================
 * materialui_render_process_entry() END
 * ============================== */

static void materialui_init_font(
   gfx_display_t *p_disp,
   materialui_font_data_t *font_data,
   int font_size,
   bool video_is_threaded,
   const char *str_latin);

static void materialui_layout(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      settings_t *settings,
      bool video_is_threaded);

/* Called on each frame. We use this callback to:
 * - Determine current scroll position
 * - Determine index of first/last on-screen entries
 * - Handle dynamic pointer input
 * - Handle streaming thumbnails */
static void materialui_render(void *data,
      unsigned width, unsigned height,
      bool is_idle)
{
   size_t i;
   float bottom;
   /* c.f. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323
    * On some platforms (e.g. 32-bit x86 without SSE),
    * gcc can produce inconsistent floating point results
    * depending upon optimisation level. This can break
    * floating point variable comparisons. A workaround is
    * to declare the affected variable as 'volatile', which
    * disables optimisations and removes excess precision
    * (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323#c87) */
   volatile float scale_factor;
   settings_t *settings     = config_get_ptr();
   materialui_handle_t *mui = (materialui_handle_t*)data;
   gfx_display_t *p_disp    = disp_get_ptr();
   size_t entries_end       = menu_entries_get_size();
   size_t selection         = menu_navigation_get_selection();
   file_list_t *list        = menu_entries_get_selection_buf_ptr(0);
   unsigned header_height   = p_disp->header_height;
   bool first_entry_found   = false;
   bool last_entry_found    = false;
   unsigned landscape_layout_optimization
                            = settings->uints.menu_materialui_landscape_layout_optimization;
   bool show_nav_bar        = settings->bools.menu_materialui_show_nav_bar;
   bool auto_rotate_nav_bar = settings->bools.menu_materialui_auto_rotate_nav_bar;
   unsigned thumbnail_upscale_threshold = 
      settings->uints.gfx_thumbnail_upscale_threshold;
   bool network_on_demand_thumbnails    = 
      settings->bools.network_on_demand_thumbnails;

   if (!mui || !list)
      return;

   /* Check whether screen dimensions, menu scale
    * factor or layout optimisation settings have changed */
   scale_factor = gfx_display_get_dpi_scale(p_disp, settings,
         width, height, false, false);

   if ((scale_factor != mui->last_scale_factor) ||
       (width != mui->last_width) ||
       (height != mui->last_height) ||
       ((enum materialui_landscape_layout_optimization_type)
            landscape_layout_optimization !=
                  mui->last_landscape_layout_optimization) ||
       (show_nav_bar != mui->last_show_nav_bar) ||
       (auto_rotate_nav_bar != mui->last_auto_rotate_nav_bar))
   {
      mui->dip_base_unit_size                 = scale_factor * MUI_DIP_BASE_UNIT_SIZE;
      mui->last_scale_factor                  = scale_factor;
      mui->last_width                         = width;
      mui->last_height                        = height;
      mui->last_landscape_layout_optimization =
            (enum materialui_landscape_layout_optimization_type)
                  landscape_layout_optimization;
      mui->last_show_nav_bar                  = show_nav_bar;
      mui->last_auto_rotate_nav_bar           = auto_rotate_nav_bar;

      /* Screen dimensions/layout are going to change
       * > Once this happens, menu will scroll to the
       *   currently selected entry
       * > If selected entry is off screen, this will
       *   throw the user to an unexpected location
       * > To avoid this, we auto select the 'middle'
       *   on screen entry before readjusting the layout */
      materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_CENTRE);

      /* Note: We don't need a full context reset here
       * > Just rescale layout, and reset frame time counter */
      materialui_layout(mui, p_disp,
            settings, video_driver_is_threaded());
      video_driver_monitor_reset();
   }

   if (mui->need_compute)
   {
      if (mui->font_data.list.font && mui->font_data.hint.font)
         materialui_compute_entries_box(mui, width, height, header_height);

      /* After calling populate_entries(), we need to call
       * materialui_get_scroll() so the last selected item
       * is correctly displayed on screen.
       * But we can't do this until materialui_compute_entries_box()
       * has been called, so we delay it until here, when
       * mui->need_compute is acted upon. */

      /* Kill any existing scroll animation
       * and reset scroll acceleration */
      materialui_kill_scroll_animation(mui);

      /* Get new scroll position */
      mui->scroll_y     = materialui_get_scroll(mui, p_disp);
      mui->need_compute = false;
   }

   /* Need to update this each frame, otherwise touchscreen
    * input breaks when changing orientation */
   p_disp->framebuf_width  = width;
   p_disp->framebuf_height = height;

   /* Read pointer state */
   menu_input_get_pointer_state(&mui->pointer);

   /* If menu screensaver is active, update
    * screensaver and return */
   if (mui->show_screensaver)
   {
      menu_screensaver_iterate(
            mui->screensaver,
            p_disp, anim_get_ptr(),
            (enum menu_screensaver_effect)settings->uints.menu_screensaver_animation,
            settings->floats.menu_screensaver_animation_speed,
            mui->colors.screensaver_tint,
            width, height,
            settings->paths.directory_assets);
      return;
   }

   /* Need to adjust/range-check scroll position first,
    * otherwise cannot determine correct entry index for
    * MENU_ENTRIES_CTL_SET_START */
   if (mui->pointer.type != MENU_POINTER_DISABLED)
   {
      /* If user is dragging the scrollbar, scroll
       * location is determined by current pointer
       * y position */
      if (mui->scrollbar.dragged)
      {
         float view_height  = (float)height - (float)header_height -
               (float)mui->nav_bar_layout_height - (float)mui->status_bar.height;
         float view_y       = (float)mui->pointer.y - (float)header_height;
         float y_scroll_max = mui->content_height - view_height;

         /* Scroll position is just fraction of view height
          * multiplied by y_scroll_max...
          * ...but to achieve proper synchronisation with the
          * scrollbar, have to offset y position and limit
          * view height range... */
         view_y      -= (float)mui->scrollbar.width + ((float)mui->scrollbar.height / 2.0f);
         view_height -= (float)((2 * mui->scrollbar.width) + mui->scrollbar.height);

         if (view_height > 0.0f)
            mui->scroll_y = y_scroll_max * (view_y / view_height);
         else
            mui->scroll_y = 0.0f;
      }
      /* If fullscreen thumbnail view is enabled,
       * scrolling is disabled - otherwise, just apply
       * normal pointer acceleration */
      else if (!mui->show_fullscreen_thumbnails)
         mui->scroll_y -= mui->pointer.y_accel;
   }

   if (mui->scroll_y < 0.0f)
      mui->scroll_y = 0.0f;

   bottom = mui->content_height - (float)height + (float)header_height +
         (float)mui->nav_bar_layout_height + (float)mui->status_bar.height;
   if (mui->scroll_y > bottom)
      mui->scroll_y = bottom;

   if (mui->content_height < (height - header_height - mui->nav_bar_layout_height - mui->status_bar.height))
      mui->scroll_y = 0.0f;

   /* Loop over all entries */
   mui->first_onscreen_entry = 0;
   mui->last_onscreen_entry  = (entries_end > 0) ? entries_end - 1 : 0;

   for (i = 0; i < entries_end; i++)
   {
      int entry_x;
      int entry_y;
      materialui_node_t *node = (materialui_node_t*)list->list[i].userdata;

      /* Sanity check */
      if (!node)
         break;

      /* Get current entry x/y position */
      entry_x = (int)node->x;
      entry_y = (int)((float)header_height - mui->scroll_y + node->y);

      /* Check whether this is the first on screen entry */
      if (!first_entry_found)
      {
         if ((entry_y + (int)node->entry_height) > (int)header_height)
         {
            mui->first_onscreen_entry = i;
            first_entry_found = true;
         }
      }
      /* Check whether this is the last on screen entry */
      else if (!last_entry_found)
      {
         if (entry_y > ((int)height - (int)mui->nav_bar_layout_height - (int)mui->status_bar.height))
         {
            /* Current entry is off screen - get index
             * of previous entry */
            if (i > 0)
            {
               mui->last_onscreen_entry = i - 1;
               last_entry_found = true;
            }
         }
      }

      /* Track pointer input, if required */
      if (first_entry_found &&
          !last_entry_found &&
          (mui->pointer.type != MENU_POINTER_DISABLED) &&
          !mui->scrollbar.dragged &&
          !mui->show_fullscreen_thumbnails)
      {
         int16_t pointer_x = mui->pointer.x;
         int16_t pointer_y = mui->pointer.y;

         /* Check if pointer is within the 'list' region of
          * the window (i.e. exclude header, navigation bar,
          * landscape borders) */
         if ((pointer_x >  mui->landscape_optimization.border_width) &&
             (pointer_x <  width - mui->landscape_optimization.border_width - mui->nav_bar_layout_width) &&
             (pointer_y >= header_height) &&
             (pointer_y <= height - mui->nav_bar_layout_height - mui->status_bar.height))
         {
            /* Check if pointer is within the bounds of the
             * current entry */
            if ((pointer_x > entry_x) &&
                (pointer_x < (entry_x + node->entry_width)) &&
                (pointer_y > entry_y) &&
                (pointer_y < (entry_y + node->entry_height)))
            {
               /* Pointer selection is always updated */
               menu_input_set_pointer_selection((unsigned)i);

               /* If pointer is pressed and stationary... */
               if (mui->pointer.pressed && !mui->pointer.dragged)
               {
                  /* ...check whether feedback selection updates
                   * are enabled... */
                  if (mui->touch_feedback_update_selection)
                  {
                     /* ...apply touch feedback to current entry */
                     mui->touch_feedback_selection = (unsigned)i;

                     /* ...and if pointer has been held for at least
                      * MENU_INPUT_PRESS_TIME_SHORT ms, automatically
                      * select current entry */
                     if (mui->pointer.press_duration >= MENU_INPUT_PRESS_TIME_SHORT)
                     {
                        menu_navigation_set_selection(i);
                        selection = i;

                        /* Once an entry has been auto selected, disable
                         * touch feedback selection updates until the next
                         * pointer down event */
                        mui->touch_feedback_update_selection = false;
                     }
                  }
               }
            }
         }
      }

      /* Perform any additional processing required
       * for the current entry */
      if (!materialui_render_process_entry(
            mui, node, i, selection,
            list->list[i].entry_idx,
            first_entry_found, last_entry_found,
            thumbnail_upscale_threshold,
            network_on_demand_thumbnails))
         break;
   }

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &mui->first_onscreen_entry);
}

/* ==============================
 * materialui_render_menu_entry() START
 * ============================== */

/* Draws specified menu entry */

/* Utility functions */

enum materialui_entry_value_type materialui_get_entry_value_type(
      materialui_handle_t *mui,
      const char *entry_value, bool entry_checked,
      unsigned entry_type, enum msg_file_type entry_file_type)
{
   enum materialui_entry_value_type value_type = MUI_ENTRY_VALUE_NONE;

   /* Check entry value string */
   if (!string_is_empty(entry_value))
   {
      /* Toggle switch off */
      if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
          string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
      {
         if (mui->textures.list[MUI_TEXTURE_SWITCH_OFF])
            value_type = MUI_ENTRY_VALUE_SWITCH_OFF;
         else
            value_type = MUI_ENTRY_VALUE_TEXT;
      }
      /* Toggle switch on */
      else if (string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
               string_is_equal(entry_value, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)))
      {
         if (mui->textures.list[MUI_TEXTURE_SWITCH_ON])
            value_type = MUI_ENTRY_VALUE_SWITCH_ON;
         else
            value_type = MUI_ENTRY_VALUE_TEXT;
      }
      /* Normal value text */
      else
      {
         switch (entry_file_type)
         {
            case FILE_TYPE_IN_CARCHIVE:
            case FILE_TYPE_MORE:
            case FILE_TYPE_CORE:
            case FILE_TYPE_DIRECT_LOAD:
            case FILE_TYPE_RDB:
            case FILE_TYPE_CURSOR:
            case FILE_TYPE_PLAIN:
            case FILE_TYPE_DIRECTORY:
            case FILE_TYPE_MUSIC:
            case FILE_TYPE_IMAGE:
            case FILE_TYPE_MOVIE:
               break;
            case FILE_TYPE_COMPRESSED:
               /* Note that we have to perform a backup check here,
                * since the 'manual content scan - file extensions'
                * setting may have a value of 'zip' or '7z' etc, which
                * means it would otherwise get incorreclty identified as
                * an archive file... */
               if (entry_type != FILE_TYPE_CARCHIVE)
                  value_type = MUI_ENTRY_VALUE_TEXT;
               break;
            default:
               value_type = MUI_ENTRY_VALUE_TEXT;
               break;
         }
      }
   }

   /* Check whether this is the currently selected item
    * of a drop down list */
   if (entry_checked &&
       ((entry_type >= MENU_SETTING_DROPDOWN_ITEM) &&
        (entry_type <= MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL)))
      value_type = MUI_ENTRY_VALUE_CHECKMARK;

   return value_type;
}

static void materialui_render_switch_icon(
      materialui_handle_t *mui,
      materialui_node_t *node,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      float y,
      int x_offset,
      bool on,
      math_matrix_4x4 *mymat)
{
   unsigned switch_texture_index = on ?
         MUI_TEXTURE_SWITCH_ON : MUI_TEXTURE_SWITCH_OFF;
   float *bg_color               = on ?
         mui->colors.list_switch_on_background : mui->colors.list_switch_off_background;
   float *switch_color           = on ?
         mui->colors.list_switch_on : mui->colors.list_switch_off;
   int x                         = x_offset + node->x + node->entry_width -
         (int)mui->landscape_optimization.entry_margin -
         (int)mui->margin - (int)mui->icon_size;

   /* Draw background */
   if (mui->textures.list[MUI_TEXTURE_SWITCH_BG])
      materialui_draw_icon(
            userdata, p_disp,
            video_width,
            video_height,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_SWITCH_BG],
            x,
            y,
            0,
            1,
            bg_color,
            mymat);

   /* Draw switch */
   if (mui->textures.list[switch_texture_index])
      materialui_draw_icon(
            userdata, p_disp,
            video_width,
            video_height,
            mui->icon_size,
            mui->textures.list[switch_texture_index],
            x,
            y,
            0,
            1,
            switch_color,
            mymat);
}

/* Used for standard, non-playlist entries
 * > MUI_LIST_VIEW_DEFAULT */
static void materialui_render_menu_entry_default(
      materialui_handle_t *mui,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      materialui_node_t *node,
      menu_entry_t *entry,
      bool entry_selected,
      bool touch_feedback_active,
      unsigned header_height,
      int x_offset)
{
   math_matrix_4x4 mymat;
   gfx_display_ctx_rotate_draw_t rotate_draw;
   const char *entry_value                           = NULL;
   const char *entry_label                           = NULL;
   unsigned entry_type                               = 0;
   enum materialui_entry_value_type entry_value_type = MUI_ENTRY_VALUE_NONE;
   unsigned entry_value_width                        = 0;
   enum msg_file_type entry_file_type                = FILE_TYPE_NONE;
   int entry_x                                       = x_offset + node->x;
   int entry_y                                       = header_height - mui->scroll_y + node->y;
   int entry_margin                                  = (int)mui->margin + (int)mui->landscape_optimization.entry_margin;
   int usable_width                                  = (int)node->entry_width -
         (int)(mui->margin * 2) - (int)(mui->landscape_optimization.entry_margin * 2);
   int label_y                                       = 0;
   int value_icon_y                                  = 0;
   uintptr_t icon_texture                            = 0;
   bool draw_text_outside                            = (x_offset != 0);
   gfx_display_t *p_disp                             = disp_get_ptr();

   {
      rotate_draw.matrix       = &mymat;
      rotate_draw.rotation     = 0.0f;
      rotate_draw.scale_x      = 1.0f;
      rotate_draw.scale_y      = 1.0f;
      rotate_draw.scale_z      = 1;
      rotate_draw.scale_enable = true;

      gfx_display_rotate_z(p_disp, &rotate_draw, userdata);
   }

   /* Initial ticker configuration
    * > Note: ticker is only used for labels/values,
    *   not sublabel text */
   if (mui->use_smooth_ticker)
   {
      mui->ticker_smooth.font     = mui->font_data.list.font;
      mui->ticker_smooth.selected = entry_selected;
   }
   else
      mui->ticker.selected = entry_selected;

   /* Read entry parameters */
   if (!string_is_empty(entry->rich_label))
      entry_label          = entry->rich_label;
   else
      entry_label          = entry->path;

   if (entry->enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
      entry_value          = entry->password_value;
   else
      entry_value          = entry->value;
   entry_type              = entry->type;

   entry_file_type         = msg_hash_to_file_type(
         msg_hash_calculate(entry_value));
   entry_value_type        = materialui_get_entry_value_type(
         mui, entry_value, entry->checked, entry_type, entry_file_type);

   /* Draw entry icon
    * > Has to be done first, since it affects the left
    *   hand margin size for label + sublabel text */
   switch (node->icon_type)
   {
      case MUI_ICON_TYPE_INTERNAL:
         /* Note: Checked entries never have icons */
         if (!entry->checked)
            icon_texture = mui->textures.list[node->icon_texture_index];
         break;
      case MUI_ICON_TYPE_MENU_EXPLORE:
         icon_texture = menu_explore_get_entry_icon(entry_type);
         break;
      case MUI_ICON_TYPE_MENU_CONTENTLESS_CORE:
         icon_texture = menu_contentless_cores_get_entry_icon(entry->label);
         break;
      case MUI_ICON_TYPE_PLAYLIST:
         icon_texture = materialui_get_playlist_icon(
               mui, node->icon_texture_index);
         break;
      default:
         switch (entry_file_type)
         {
            case FILE_TYPE_COMPRESSED:
               /* Note that we have to perform a backup check here,
                * since the 'manual content scan - file extensions'
                * setting may have a value of 'zip' or '7z' etc, which
                * means it would otherwise get incorrectly identified as
                * an archive file... */
               if (entry_type == FILE_TYPE_CARCHIVE)
                  icon_texture = mui->textures.list[MUI_TEXTURE_ARCHIVE];
               break;
            case FILE_TYPE_IMAGE:
               icon_texture = mui->textures.list[MUI_TEXTURE_IMAGE];
               break;
            default:
               break;
         }
         break;
   }

   if (icon_texture)
   {
      materialui_draw_icon(
            userdata, p_disp,
            video_width,
            video_height,
            mui->icon_size,
            (uintptr_t)icon_texture,
            entry_x + (int)mui->landscape_optimization.entry_margin,
            entry_y + (node->entry_height / 2.0f) - (mui->icon_size / 2.0f),
            0,
            1,
            mui->colors.list_icon,
            &mymat);

      entry_margin += mui->icon_size;
      usable_width -= mui->icon_size;
   }

   /* Draw entry sublabel
    * > Must be done before label + value, since it
    *   affects y offset positions */
   if (!string_is_empty(entry->sublabel))
   {
      /* Note: Due to the way the selection highlight
       * marker is drawn (height is effectively 1px larger
       * than the entry height, to avoid visible seams),
       * drawing the label+sublabel text at the exact centre
       * of the entry gives the illusion of misalignment
       * > Have to offset the label downwards by half a pixel
       *   (rounded up) */
      int vertical_margin = ((node->entry_height - node->text_height) / 2.0f) - (float)mui->sublabel_gap + 1.0f;
      int sublabel_y;
      char wrapped_sublabel[MENU_SUBLABEL_MAX_LENGTH];

      wrapped_sublabel[0] = '\0';

      /* Label + sublabel 'block' is drawn at the
       * vertical centre of the current node.
       * > Value icon is drawn in line with the centre
       *   of the part of the label above the baseline
       *   (needs a little extra padding at the top, since
       *   the line ascender is usually somewhat taller
       *   than the visible text) */
      label_y      = entry_y + vertical_margin + mui->font_data.list.line_ascender;
      value_icon_y = label_y + (mui->dip_base_unit_size / 60.0f) - (mui->font_data.list.line_ascender / 2.0f) - (mui->icon_size / 2.0f);
      sublabel_y   = entry_y + vertical_margin + mui->font_data.list.line_height + (int)mui->sublabel_gap + mui->font_data.hint.line_ascender;

      /* Wrap sublabel string */
      (mui->word_wrap)(wrapped_sublabel, sizeof(wrapped_sublabel), entry->sublabel,
            (int)((usable_width - (int)mui->sublabel_padding) / mui->font_data.hint.glyph_width),
            mui->font_data.hint.wideglyph_width, 0);

      /* Draw sublabel string
       * > Note: We must allow text to be drawn off-screen
       *   if the current y position is negative, otherwise topmost
       *   entries with very long sublabels may get 'clipped' too
       *   early as they are scrolled upwards beyond the top edge
       *   of the screen */
      gfx_display_draw_text(mui->font_data.hint.font, wrapped_sublabel,
            entry_x + entry_margin,
            sublabel_y,
            video_width, video_height,
            (entry_selected || touch_feedback_active) ?
                  mui->colors.list_hint_text_highlighted : mui->colors.list_hint_text,
            TEXT_ALIGN_LEFT, 1.0f, false, 0.0f,
            draw_text_outside || (sublabel_y < 0));
   }
   else
   {
      /* If we don't have a sublabel, entry label is drawn
       * at the vertical centre of the current node */
      label_y      = entry_y + (node->entry_height / 2.0f) + mui->font_data.list.line_centre_offset;
      value_icon_y = entry_y + (node->entry_height / 2.0f) - (mui->icon_size / 2.0f);
   }

   /* Draw entry value */
   switch (entry_value_type)
   {
      case MUI_ENTRY_VALUE_TEXT:
         {
            int value_x_offset             = 0;
            unsigned entry_value_width_max = (usable_width / 2) - mui->margin;
            char value_buf[255];

            value_buf[0] = '\0';

            /* Apply ticker */
            if (mui->use_smooth_ticker)
            {
               mui->ticker_smooth.field_width = entry_value_width_max;
               mui->ticker_smooth.src_str     = entry_value;
               mui->ticker_smooth.dst_str     = value_buf;
               mui->ticker_smooth.dst_str_len = sizeof(value_buf);

               if (gfx_animation_ticker_smooth(&mui->ticker_smooth))
               {
                  /* If ticker is active, then value text is effectively
                   * entry_value_width_max pixels wide... */
                  entry_value_width = entry_value_width_max;
                  /* ...and since value text is right aligned, have to
                   * offset x position by the 'padding' width at the
                   * end of the ticker string */
                  value_x_offset =
                        (int)(mui->ticker_x_offset + mui->ticker_str_width) -
                              (int)entry_value_width_max;
               }
               /* If ticker is inactive, width of value string is
                * exactly mui->ticker_str_width pixels, and no x offset
                * is required */
               else
                  entry_value_width = mui->ticker_str_width;
            }
            else
            {
               size_t entry_value_len     = utf8len(entry_value);
               size_t entry_value_len_max =
                     (size_t)(entry_value_width_max / mui->font_data.list.glyph_width);

               /* Limit length of value string */
               entry_value_len_max = (entry_value_len_max > 0) ?
                     entry_value_len_max - 1 : entry_value_len_max;
               entry_value_len = (entry_value_len > entry_value_len_max) ?
                     entry_value_len_max : entry_value_len;

               mui->ticker.s        = value_buf;
               mui->ticker.len      = entry_value_len;
               mui->ticker.str      = entry_value;

               gfx_animation_ticker(&mui->ticker);

               /* Get effective width of value string
                * > Approximate value - only the smooth ticker
                *   returns the actual width in pixels, and any
                *   platform too slow to run the smooth ticker
                *   won't appreciate the overheads of using
                *   font_driver_get_message_width() here... */
               entry_value_width = (entry_value_len + 1) * mui->font_data.list.glyph_width;
            }

            /* Draw value string */
            gfx_display_draw_text(mui->font_data.list.font, value_buf,
                  entry_x + value_x_offset + node->entry_width - (int)mui->margin - (int)mui->landscape_optimization.entry_margin,
                  label_y,
                  video_width, video_height,
                  (entry_selected || touch_feedback_active) ?
                        mui->colors.list_text_highlighted : mui->colors.list_text,
                  TEXT_ALIGN_RIGHT, 1.0f, false, 0.0f, draw_text_outside);
         }
         break;
      case MUI_ENTRY_VALUE_SWITCH_ON:
         {
            materialui_render_switch_icon(mui, node, p_disp, userdata,
                  video_width, video_height, value_icon_y, x_offset,
                  true,
                  &mymat);
            entry_value_width = mui->icon_size;
         }
         break;
      case MUI_ENTRY_VALUE_SWITCH_OFF:
         {
            materialui_render_switch_icon(mui, node, p_disp, userdata,
                  video_width, video_height, value_icon_y, x_offset,
                  false,
                  &mymat);
            entry_value_width = mui->icon_size;
         }
         break;
      case MUI_ENTRY_VALUE_CHECKMARK:
         {
            /* Draw checkmark */
            if (mui->textures.list[MUI_TEXTURE_CHECKMARK])
               materialui_draw_icon(
                     userdata, p_disp,
                     video_width,
                     video_height,
                     mui->icon_size,
                     mui->textures.list[MUI_TEXTURE_CHECKMARK],
                     entry_x + node->entry_width - (int)mui->margin - (int)mui->landscape_optimization.entry_margin - (int)mui->icon_size,
                     value_icon_y,
                     0,
                     1,
                     mui->colors.list_switch_on,
                     &mymat);

            entry_value_width = mui->icon_size;
         }
         break;
      default:
         entry_value_width = 0;
         break;
   }

   /* Draw entry label */
   if (!string_is_empty(entry_label))
   {
      int label_width = usable_width;
      char label_buf[255];

      label_buf[0] = '\0';

      /* Get maximum width of label string
       * > If a value is present, need additional padding
       *   between label and value */
      label_width = (entry_value_width > 0) ?
            label_width - (int)(entry_value_width + mui->margin) : label_width;

      if (label_width > 0)
      {
         /* Apply ticker */
         if (mui->use_smooth_ticker)
         {
            /* Label */
            mui->ticker_smooth.field_width = (unsigned)label_width;
            mui->ticker_smooth.src_str     = entry_label;
            mui->ticker_smooth.dst_str     = label_buf;
            mui->ticker_smooth.dst_str_len = sizeof(label_buf);

            gfx_animation_ticker_smooth(&mui->ticker_smooth);
         }
         else
         {
            /* Label */
            mui->ticker.s        = label_buf;
            mui->ticker.len      = (size_t)(label_width / mui->font_data.list.glyph_width);
            mui->ticker.str      = entry_label;

            gfx_animation_ticker(&mui->ticker);
         }

         /* Draw label string */
         gfx_display_draw_text(mui->font_data.list.font, label_buf,
               (int)mui->ticker_x_offset + entry_x + entry_margin,
               label_y,
               video_width, video_height,
               (entry_selected || touch_feedback_active) ?
                     mui->colors.list_text_highlighted : mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0.0f, draw_text_outside);
      }
   }
}

/* Used for playlist 'list view' (with and without
 *   thumbnails) entries
 * > MUI_LIST_VIEW_PLAYLIST
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE */
static void materialui_render_menu_entry_playlist_list(
      materialui_handle_t *mui,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      materialui_node_t *node,
      menu_entry_t *entry,
      bool entry_selected,
      bool touch_feedback_active,
      unsigned header_height,
      int x_offset)
{
   bool draw_divider;
   math_matrix_4x4 mymat;
   gfx_display_ctx_rotate_draw_t rotate_draw;
   const char *entry_label    = NULL;
   int entry_x                = x_offset + node->x;
   int entry_y                = header_height - mui->scroll_y + node->y;
   int divider_y              = entry_y + (int)node->entry_height;
   int entry_margin           = (int)mui->margin;
   int usable_width           = (int)node->entry_width - (int)(mui->margin * 2);
   int label_y                = 0;
   bool draw_text_outside     = (x_offset != 0);
   settings_t *settings       = config_get_ptr();
   gfx_display_t *p_disp      = disp_get_ptr();

   {
      rotate_draw.matrix       = &mymat;
      rotate_draw.rotation     = 0.0f;
      rotate_draw.scale_x      = 1.0f;
      rotate_draw.scale_y      = 1.0f;
      rotate_draw.scale_z      = 1;
      rotate_draw.scale_enable = true;

      gfx_display_rotate_z(p_disp, &rotate_draw, userdata);
   }

   /* Initial ticker configuration
    * > Note: ticker is only used for labels,
    *   not sublabel text */
   if (mui->use_smooth_ticker)
   {
      mui->ticker_smooth.font     = mui->font_data.list.font;
      mui->ticker_smooth.selected = entry_selected;
   }
   else
      mui->ticker.selected = entry_selected;

   /* Read entry parameters */
   if (!string_is_empty(entry->rich_label))
      entry_label          = entry->rich_label;
   else
      entry_label          = entry->path;

   /* If thumbnails are *not* enabled, increase entry
    * margin and decrease usable width by landscape
    * optimisation margin */
   if (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST)
   {
      entry_margin += (int)mui->landscape_optimization.entry_margin;
      usable_width -= (int)(mui->landscape_optimization.entry_margin * 2);
   }
   /* Draw entry thumbnail(s)
    * > Has to be done first, since it affects the left
    *   hand margin size and total width for label +
    *   sublabel text */
   else
   {
      int thumbnail_margin = 0;
      float thumbnail_y    = (float)entry_y +
            ((float)node->entry_height / 2.0f) -
            ((float)mui->thumbnail_height_max / 2.0f);

      /* When using portrait display orientations with
       * secondary thumbnails enabled, have to add a
       * left/right margin equal to the scroll bar
       * width (to prevent the scroll bar from being
       * drawn on top of the secondary thumbnail) */
      if (mui->is_portrait)
      {
         if (mui->secondary_thumbnail_enabled)
            thumbnail_margin = (int)mui->scrollbar.width;
      }
      /* When using landscape display orientations, we
       * have enough screen space to improve thumbnail
       * appearance by adding left/right margins */
      else
         thumbnail_margin = (int)mui->margin;

      /* Draw primary thumbnail */
      materialui_draw_thumbnail(
            mui,
            &node->thumbnails.primary,
            settings,
            p_disp,
            userdata,
            video_width,
            video_height,
            (float)(entry_x + thumbnail_margin),
            thumbnail_y,
            1.0f,
            &mymat);

      entry_margin += mui->thumbnail_width_max + thumbnail_margin;
      usable_width -= mui->thumbnail_width_max + thumbnail_margin;

      /* Draw secondary thumbnail, if required */
      if (mui->secondary_thumbnail_enabled)
      {
         materialui_draw_thumbnail(
               mui,
               &node->thumbnails.secondary,
               settings,
               p_disp,
               userdata,
               video_width,
               video_height,
               (float)(entry_x + node->entry_width - thumbnail_margin - (int)mui->thumbnail_width_max),
               thumbnail_y,
               1.0f,
               &mymat);

         usable_width -= mui->thumbnail_width_max + thumbnail_margin;
      }
   }

   /* Draw entry sublabel
    * > Must be done before label, since it
    *   affects y offset positions */
   if (!string_is_empty(entry->sublabel))
   {
      /* Note: Due to the way the selection highlight
       * marker is drawn (height is effectively 1px larger
       * than the entry height, to avoid visible seams),
       * drawing the label+sublabel text at the exact centre
       * of the entry gives the illusion of misalignment
       * > Have to offset the label downwards by half a pixel
       *   (rounded up) */
      int vertical_margin = ((node->entry_height - node->text_height) / 2.0f) - (float)mui->sublabel_gap + 1.0f;
      int sublabel_y;
      char wrapped_sublabel[MENU_SUBLABEL_MAX_LENGTH];

      wrapped_sublabel[0] = '\0';

      /* Label + sublabel 'block' is drawn at the
       * vertical centre of the current node */
      label_y      = entry_y + vertical_margin + mui->font_data.list.line_ascender;
      sublabel_y   = entry_y + vertical_margin + mui->font_data.list.line_height + (int)mui->sublabel_gap + mui->font_data.hint.line_ascender;

      /* Wrap sublabel string */
      (mui->word_wrap)(wrapped_sublabel, sizeof(wrapped_sublabel), entry->sublabel,
            (int)((usable_width - (int)mui->sublabel_padding) / mui->font_data.hint.glyph_width),
            mui->font_data.hint.wideglyph_width, 0);

      /* Draw sublabel string
       * > Note: We must allow text to be drawn off-screen
       *   if the current y position is negative, otherwise topmost
       *   entries with very long sublabels may get 'clipped' too
       *   early as they are scrolled upwards beyond the top edge
       *   of the screen */
      gfx_display_draw_text(mui->font_data.hint.font, wrapped_sublabel,
            entry_x + entry_margin,
            sublabel_y,
            video_width, video_height,
            (entry_selected || touch_feedback_active) ?
                  mui->colors.list_hint_text_highlighted : mui->colors.list_hint_text,
            TEXT_ALIGN_LEFT, 1.0f, false, 0.0f,
            draw_text_outside || (sublabel_y < 0));
   }
   /* If we don't have a sublabel, entry label is drawn
    * at the vertical centre of the current node */
   else
      label_y = entry_y + (node->entry_height / 2.0f) + mui->font_data.list.line_centre_offset;

   /* Draw entry label */
   if (!string_is_empty(entry_label))
   {
      char label_buf[255];

      label_buf[0] = '\0';

      if (usable_width > 0)
      {
         /* Apply ticker */
         if (mui->use_smooth_ticker)
         {
            /* Label */
            mui->ticker_smooth.field_width = (unsigned)usable_width;
            mui->ticker_smooth.src_str     = entry_label;
            mui->ticker_smooth.dst_str     = label_buf;
            mui->ticker_smooth.dst_str_len = sizeof(label_buf);

            gfx_animation_ticker_smooth(&mui->ticker_smooth);
         }
         else
         {
            /* Label */
            mui->ticker.s        = label_buf;
            mui->ticker.len      = (size_t)(usable_width / mui->font_data.list.glyph_width);
            mui->ticker.str      = entry_label;

            gfx_animation_ticker(&mui->ticker);
         }

         /* Draw label string */
         gfx_display_draw_text(mui->font_data.list.font, label_buf,
               (int)mui->ticker_x_offset + entry_x + entry_margin,
               label_y,
               video_width, video_height,
               (entry_selected || touch_feedback_active) ?
                     mui->colors.list_text_highlighted : mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0.0f,
               draw_text_outside);
      }
   }

   /* When using thumbnail views, the horizontal
    * text area is unpleasantly vacuous, such that the
    * label + sublabel strings float in a sea of nothingness.
    * We can partially mitigate the visual 'emptiness' of this
    * by drawing a divider between entries. This is particularly
    * beneficial when dual thumbnails are enabled, since it
    * 'ties' the left/right thumbnails together
    * > To prevent any ugly alignment issues, we
    *   only draw a divider if its bottom edge is
    *   more than two times the divider thickness from
    *   the bottom edge of the list region,
    *   and when the divider color is different from
    *   the list background color */
   draw_divider = (mui->list_view_type != MUI_LIST_VIEW_PLAYLIST) &&
         !mui->colors.divider_is_list_background &&
         (usable_width > 0) &&
               ((divider_y + (mui->entry_divider_width * 2)) <
                     (video_height - mui->nav_bar_layout_height - mui->status_bar.height));

   if (draw_divider)
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            (float)(entry_x + entry_margin),
            (float)divider_y,
            (unsigned)usable_width,
            mui->entry_divider_width,
            video_width,
            video_height,
            mui->colors.entry_divider,
            NULL);
}

/* Used for playlist 'dual icon' entries
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON */
static void materialui_render_menu_entry_playlist_dual_icon(
      materialui_handle_t *mui,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      materialui_node_t *node,
      menu_entry_t *entry,
      bool entry_selected,
      bool touch_feedback_active,
      unsigned header_height,
      int x_offset)
{
   math_matrix_4x4 mymat;
   gfx_display_ctx_rotate_draw_t rotate_draw;
   const char *entry_label = NULL;
   float entry_x           = (float)x_offset + node->x;
   float entry_y           = (float)header_height - mui->scroll_y + node->y;
   int usable_width        = (int)node->entry_width - (int)(mui->margin * 2);
   float thumbnail_y       = entry_y + ((float)mui->dip_base_unit_size / 10.0f);
   float divider_y         = thumbnail_y + (float)mui->thumbnail_height_max +
         ((float)mui->dip_base_unit_size / 10.0f) +
               (float)mui->font_data.list.line_height;
   /* To prevent any ugly alignment issues, we
    * only draw a divider if its bottom edge is
    * more than two times the divider thickness from
    * the bottom edge of the list region */
   bool draw_divider       = (usable_width > 0) &&
         !mui->colors.divider_is_list_background &&
         ((divider_y + (mui->entry_divider_width * 2)) <
               (video_height - mui->nav_bar_layout_height - mui->status_bar.height));
   gfx_display_t *p_disp   = disp_get_ptr();
   settings_t *settings    = config_get_ptr();

   {
      rotate_draw.matrix       = &mymat;
      rotate_draw.rotation     = 0.0f;
      rotate_draw.scale_x      = 1.0f;
      rotate_draw.scale_y      = 1.0f;
      rotate_draw.scale_z      = 1;
      rotate_draw.scale_enable = true;

      gfx_display_rotate_z(p_disp, &rotate_draw, userdata);
   }

   /* Initial ticker configuration
    * > Note: ticker is only used for labels */
   if (mui->use_smooth_ticker)
   {
      mui->ticker_smooth.font     = mui->font_data.list.font;
      mui->ticker_smooth.selected = entry_selected;
   }
   else
      mui->ticker.selected = entry_selected;

   /* Read entry parameters */
   if (!string_is_empty(entry->rich_label))
      entry_label          = entry->rich_label;
   else
      entry_label          = entry->path;

   /* Draw thumbnails
    * > These go at the top of the entry, with a
    *   small vertical margin */

   /* > Primary thumbnail */
   materialui_draw_thumbnail(
         mui,
         &node->thumbnails.primary,
         settings,
         p_disp,
         userdata,
         video_width,
         video_height,
         entry_x + (float)mui->margin,
         thumbnail_y,
         1.0f,
         &mymat);

   /* > Secondary thumbnail */
   materialui_draw_thumbnail(
         mui,
         &node->thumbnails.secondary,
         settings,
         p_disp,
         userdata,
         video_width,
         video_height,
         entry_x + node->entry_width 
         - (float)mui->margin - (float)mui->thumbnail_width_max,
         thumbnail_y,
         1.0f,
         &mymat);

   /* Draw entry label */
   if (!string_is_empty(entry_label))
   {
      float label_x          = 0.0f;
      /* Label is drawn beneath thumbnails,
       * with a small vertical margin */
      float label_y          =
            thumbnail_y + (float)mui->thumbnail_height_max +
            ((float)mui->dip_base_unit_size / 20.0f) +
            ((float)mui->font_data.list.line_height / 2.0f) +
            (float)mui->font_data.list.line_centre_offset;

      bool draw_text_outside = (x_offset != 0);
      char label_buf[255];

      label_buf[0] = '\0';

      if (usable_width > 0)
      {
         /* Apply ticker */
         if (mui->use_smooth_ticker)
         {
            /* Label */
            mui->ticker_smooth.field_width = (unsigned)usable_width;
            mui->ticker_smooth.src_str     = entry_label;
            mui->ticker_smooth.dst_str     = label_buf;
            mui->ticker_smooth.dst_str_len = sizeof(label_buf);

            /* If ticker is inactive, centre the text */
            if (!gfx_animation_ticker_smooth(&mui->ticker_smooth))
               label_x = (float)(usable_width - mui->ticker_str_width) / 2.0f;
         }
         else
         {
            /* Label */
            mui->ticker.s   = label_buf;
            mui->ticker.len = (size_t)(usable_width / mui->font_data.list.glyph_width);
            mui->ticker.str = entry_label;

            /* If ticker is inactive, centre the text */
            if (!gfx_animation_ticker(&mui->ticker))
            {
               int str_width = (int)(utf8len(label_buf) *
                  mui->font_data.list.glyph_width);

               label_x = (float)(usable_width - str_width) / 2.0f;
            }
         }

         label_x += (float)mui->ticker_x_offset + entry_x + (float)mui->margin;

         /* Draw label string */
         gfx_display_draw_text(mui->font_data.list.font, label_buf,
               label_x,
               label_y,
               video_width, video_height,
               (entry_selected || touch_feedback_active) ?
                     mui->colors.list_text_highlighted : mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0.0f,
               draw_text_outside);
      }
   }

   /* Draw divider */
   if (draw_divider)
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            entry_x + (float)mui->margin,
            divider_y,
            (unsigned)usable_width,
            mui->entry_divider_width,
            video_width,
            video_height,
            mui->colors.entry_divider,
            NULL);
}

/* Used for playlist 'desktop'-layout entries
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP */
static void materialui_render_menu_entry_playlist_desktop(
      materialui_handle_t *mui,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      materialui_node_t *node,
      menu_entry_t *entry,
      bool entry_selected,
      bool touch_feedback_active,
      unsigned header_height,
      int x_offset)
{
   const char *entry_label = NULL;
   gfx_display_t *p_disp   = disp_get_ptr();
   int entry_x             = x_offset + node->x;
   int entry_y             = header_height - mui->scroll_y + node->y;
   int divider_y           = entry_y + (int)node->entry_height;
   int entry_margin        = (int)mui->margin;
   int usable_width        = node->entry_width - (int)(mui->margin * 2);
   /* Entry label is drawn at the vertical centre
    * of the current node */
   int label_y             = entry_y + (node->entry_height / 2.0f) +
         mui->font_data.list.line_centre_offset;
   bool draw_text_outside  = (x_offset != 0);
   /* To prevent any ugly alignment issues, we
    * only draw a divider if its bottom edge is
    * more than two times the divider thickness from
    * the bottom edge of the list region */
   bool draw_divider = (usable_width > 0) &&
         !mui->colors.divider_is_list_background &&
         ((divider_y + (mui->entry_divider_width * 2)) <
               (video_height - mui->nav_bar_layout_height - mui->status_bar.height));

   /* Read entry parameters */
   if (!string_is_empty(entry->rich_label))
      entry_label          = entry->rich_label;
   else
      entry_label          = entry->path;

   /* Draw entry label */
   if (!string_is_empty(entry_label))
   {
      char label_buf[255];

      label_buf[0] = '\0';

      if (usable_width > 0)
      {
         /* Apply ticker */
         if (mui->use_smooth_ticker)
         {
            mui->ticker_smooth.font        = mui->font_data.list.font;
            mui->ticker_smooth.selected    = entry_selected;
            mui->ticker_smooth.field_width = (unsigned)usable_width;
            mui->ticker_smooth.src_str     = entry_label;
            mui->ticker_smooth.dst_str     = label_buf;
            mui->ticker_smooth.dst_str_len = sizeof(label_buf);

            gfx_animation_ticker_smooth(&mui->ticker_smooth);
         }
         else
         {
            mui->ticker.selected = entry_selected;
            mui->ticker.s        = label_buf;
            mui->ticker.len      = (size_t)(usable_width / mui->font_data.list.glyph_width);
            mui->ticker.str      = entry_label;

            gfx_animation_ticker(&mui->ticker);
         }

         /* Draw text */
         gfx_display_draw_text(mui->font_data.list.font, label_buf,
               (int)mui->ticker_x_offset + entry_x + entry_margin,
               label_y,
               video_width, video_height,
               (entry_selected || touch_feedback_active) ?
                     mui->colors.list_text_highlighted : mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0.0f,
               draw_text_outside);
      }
   }

   /* Draw divider */
   if (draw_divider)
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            (float)entry_x,
            (float)divider_y,
            (unsigned)node->entry_width,
            mui->entry_divider_width,
            video_width,
            video_height,
            mui->colors.entry_divider,
            NULL);
}

static void (*materialui_render_menu_entry)(
      materialui_handle_t *mui,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      materialui_node_t *node,
      menu_entry_t *entry,
      bool entry_selected,
      bool touch_feedback_active,
      unsigned header_height,
      int x_offset) = materialui_render_menu_entry_default;

/* ==============================
 * materialui_render_menu_entry() END
 * ============================== */

/* ==============================
 * materialui_render_selected_entry_aux() START
 * ============================== */

/* Draws any auxiliary items required for the
 * currently selected menu entry */

/* Used for 'desktop'-layout playlist displays.
 * Draws thumbnails + metadata for currently
 * selected item.
 * > MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP */
static void materialui_render_selected_entry_aux_playlist_desktop(
      materialui_handle_t *mui, void *userdata,
      unsigned video_width, unsigned video_height,
      unsigned header_height, int x_offset,
      file_list_t *list, size_t selection)
{
   math_matrix_4x4 mymat;
   gfx_display_ctx_rotate_draw_t rotate_draw;
   materialui_node_t *node    = (materialui_node_t*)list->list[selection].userdata;
   float background_x         = (float)(x_offset + (int)mui->landscape_optimization.border_width);
   float background_y         = (float)header_height;
   /* Note: If landscape optimisations are enabled,
    * need to allow space for a second divider at
    * the left hand edge of the sidebar */
   int background_width       = mui->thumbnail_width_max + (mui->margin * 2) +
         (mui->entry_divider_width * (mui->landscape_optimization.enabled ?
               2 : 1));
   int background_height      = (int)video_height - (int)header_height -
         (int)mui->nav_bar_layout_height - (int)mui->status_bar.height;
   float thumbnail_x          = background_x + (float)mui->margin +
         (mui->landscape_optimization.enabled ? mui->entry_divider_width : 0);
   float thumbnail_y          = background_y + (float)mui->margin;
   gfx_display_t *p_disp      = disp_get_ptr();
   settings_t *settings       = config_get_ptr();

   /* Sanity check */
   if ((background_width <= 0) ||
       (background_height <= 0))
      return;

   {
      rotate_draw.matrix       = &mymat;
      rotate_draw.rotation     = 0.0f;
      rotate_draw.scale_x      = 1.0f;
      rotate_draw.scale_y      = 1.0f;
      rotate_draw.scale_z      = 1;
      rotate_draw.scale_enable = true;

      gfx_display_rotate_z(p_disp, &rotate_draw, userdata);
   }

   /* Draw sidebar background
    * > Surface */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         background_x,
         background_y,
         (unsigned)background_width,
         (unsigned)background_height,
         video_width,
         video_height,
         mui->colors.side_bar_background,
         NULL);

   /* > Divider */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         background_x + (float)background_width - (float)mui->entry_divider_width,
         background_y,
         mui->entry_divider_width,
         (unsigned)background_height,
         video_width,
         video_height,
         mui->colors.entry_divider,
         NULL);

   /* > Additional divider */
   if (mui->landscape_optimization.enabled)
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            background_x,
            background_y,
            mui->entry_divider_width,
            (unsigned)background_height,
            video_width,
            video_height,
            mui->colors.entry_divider,
            NULL);

   /* Draw thumbnails */
   if (node)
   {
      gfx_thumbnail_t *primary_thumbnail   = &node->thumbnails.primary;
      gfx_thumbnail_t *secondary_thumbnail = &node->thumbnails.secondary;

      /* If we have not yet requested thumbnails
       * for the currently selected entry, keep
       * drawing thumbnails for the last 'valid'
       * entry instead (this ensures we always
       * display *something* in the sidebar
       * - leaving it blank is ugly...) */
      if ((primary_thumbnail->status   == GFX_THUMBNAIL_STATUS_UNKNOWN) &&
          (secondary_thumbnail->status == GFX_THUMBNAIL_STATUS_UNKNOWN))
      {
         materialui_node_t *last_node = (materialui_node_t*)list->list[mui->desktop_thumbnail_last_selection].userdata;

         if (last_node)
         {
            primary_thumbnail   = &last_node->thumbnails.primary;
            secondary_thumbnail = &last_node->thumbnails.secondary;
         }
      }

      /* Draw primary */
      materialui_draw_thumbnail(
            mui,
            primary_thumbnail,
            settings,
            p_disp,
            userdata,
            video_width,
            video_height,
            thumbnail_x,
            thumbnail_y,
            1.0f,
            &mymat);

      /* Draw secondary */
      materialui_draw_thumbnail(
            mui,
            secondary_thumbnail,
            settings,
            p_disp,
            userdata,
            video_width,
            video_height,
            thumbnail_x,
            thumbnail_y 
            + (float)mui->thumbnail_height_max + (float)mui->margin,
            1.0f,
            &mymat);
   }

   /* Draw status bar */
   if (mui->status_bar.enabled)
   {
      float status_bar_x   = background_x;
      float status_bar_y   = (float)(video_height - mui->nav_bar_layout_height - mui->status_bar.height);
      int status_bar_width = (int)video_width - (int)(mui->landscape_optimization.border_width * 2) -
            (int)mui->nav_bar_layout_width;
      int text_width       = status_bar_width - (int)(mui->margin * 2);

      /* Sanity check */
      if (status_bar_width <= 0)
         return;

      /* Status bar overlaps list entries
       * > Must flush list font before attempting
       *   to draw it */
      materialui_font_flush(video_width, video_height, &mui->font_data.list);

      /* Background
       * > Surface */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            status_bar_x,
            status_bar_y,
            (unsigned)status_bar_width,
            mui->status_bar.height,
            video_width,
            video_height,
            mui->colors.status_bar_background,
            NULL);

      /* > Shadow
       *   (For symmetry, header and status bar
       *    shadows have the same height) */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            status_bar_x,
            status_bar_y,
            (unsigned)status_bar_width,
            mui->header_shadow_height,
            video_width,
            video_height,
            mui->colors.status_bar_shadow,
            NULL);

      /* Text */
      if ((text_width > 0) && !string_is_empty(mui->status_bar.str))
      {
         bool draw_text_outside = (x_offset != 0);
         uint32_t text_color    = mui->colors.status_bar_text;
         float text_x           = 0.0f;
         char metadata_buf[MENU_SUBLABEL_MAX_LENGTH];

         metadata_buf[0] = '\0';

         /* Set text opacity */
         text_color = (text_color & 0xFFFFFF00) |
               (unsigned)((255.0f * mui->transition_alpha * mui->status_bar.alpha) + 0.5f);

         /* Apply ticker */
         if (mui->use_smooth_ticker)
         {
            mui->ticker_smooth.font        = mui->font_data.hint.font;
            mui->ticker_smooth.selected    = true;
            mui->ticker_smooth.field_width = (unsigned)text_width;
            mui->ticker_smooth.src_str     = mui->status_bar.str;
            mui->ticker_smooth.dst_str     = metadata_buf;
            mui->ticker_smooth.dst_str_len = sizeof(metadata_buf);

            gfx_animation_ticker_smooth(&mui->ticker_smooth);

            /* If ticker is inactive, centre the text */
            if (!gfx_animation_ticker_smooth(&mui->ticker_smooth))
               text_x = (float)(text_width - mui->ticker_str_width) / 2.0f;
         }
         else
         {
            mui->ticker.selected = true;
            mui->ticker.s        = metadata_buf;
            mui->ticker.len      = (size_t)(text_width / mui->font_data.hint.glyph_width);
            mui->ticker.str      = mui->status_bar.str;

            /* If ticker is inactive, centre the text */
            if (!gfx_animation_ticker(&mui->ticker))
            {
               int str_width = (int)(utf8len(metadata_buf) *
                     mui->font_data.hint.glyph_width);

               text_x = (float)(text_width - str_width) / 2.0f;
            }
         }

         text_x += (float)mui->ticker_x_offset + status_bar_x + (float)mui->margin;

         /* Draw metadata string */
         gfx_display_draw_text(mui->font_data.hint.font, metadata_buf,
               text_x,
               status_bar_y + ((float)mui->status_bar.height * 0.5f) +
                     (float)mui->font_data.hint.line_centre_offset,
               video_width, video_height,
               text_color,
               TEXT_ALIGN_LEFT, 1.0f, false, 0.0f,
               draw_text_outside);
      }
   }
}

static void (*materialui_render_selected_entry_aux)(
      materialui_handle_t *mui, void *userdata,
      unsigned video_width, unsigned video_height,
      unsigned header_height, int x_offset,
      file_list_t *list, size_t selection) = NULL;

/* ==============================
 * materialui_render_selected_entry_aux() END
 * ============================== */

static void materialui_render_scrollbar(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width, unsigned video_height)
{
   /* Do nothing if scrollbar is disabled */
   if (!mui->scrollbar.active)
      return;

   /* Draw scrollbar */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         mui->scrollbar.x,
         mui->scrollbar.y,
         mui->scrollbar.width,
         mui->scrollbar.height,
         video_width,
         video_height,
         mui->colors.scrollbar,
         NULL);
}

/* Draws current menu list */
static void materialui_render_menu_list(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x_offset)
{
   size_t i;
   size_t first_entry;
   size_t last_entry;
   file_list_t *list           = NULL;
   size_t entries_end          = menu_entries_get_size();
   size_t selection            = menu_navigation_get_selection();
   unsigned header_height      = p_disp->header_height; 
   bool touch_feedback_enabled =
         !mui->scrollbar.dragged &&
         !mui->show_fullscreen_thumbnails &&
         (mui->touch_feedback_alpha >= 0.5f) &&
         (mui->touch_feedback_selection == menu_input_get_pointer_selection());
   bool entry_value_enabled    = (mui->list_view_type == MUI_LIST_VIEW_DEFAULT);
   bool entry_sublabel_enabled =
         (mui->list_view_type != MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON) &&
         (mui->list_view_type != MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP);

   list = menu_entries_get_selection_buf_ptr(0);
   if (!list)
      return;

   /* Unnecessary sanity check... */
   first_entry = (mui->first_onscreen_entry < entries_end) ? mui->first_onscreen_entry : entries_end;
   last_entry  = (mui->last_onscreen_entry  < entries_end) ? mui->last_onscreen_entry  : entries_end;

   for (i = first_entry; i <= last_entry; i++)
   {
      bool entry_selected        = (selection == i);
      bool touch_feedback_active = touch_feedback_enabled && (mui->touch_feedback_selection == i);
      materialui_node_t *node    = (materialui_node_t*)list->list[i].userdata;
      menu_entry_t entry;

      /* Sanity check */
      if (!node)
         break;

      /* Get current entry */
      MENU_ENTRY_INIT(entry);
      entry.path_enabled     = false;
      entry.value_enabled    = entry_value_enabled;
      entry.sublabel_enabled = entry_sublabel_enabled;
      menu_entry_get(&entry, 0, i, NULL, true);

      /* Render entry: label, value + associated icons */
      materialui_render_menu_entry(
            mui,
            userdata,
            video_width,
            video_height,
            node,
            &entry,
            entry_selected,
            touch_feedback_active,
            header_height,
            x_offset);
   }

   /* Draw any auxiliary items required for the
    * currently selected entry */
   if (materialui_render_selected_entry_aux)
      materialui_render_selected_entry_aux(
            mui, userdata,
            video_width, video_height,
            header_height, x_offset,
            list, selection);

   /* Draw scrollbar */
   materialui_render_scrollbar(
         mui, p_disp, userdata,
         video_width, video_height);
}

static size_t materialui_list_get_size(void *data, enum menu_list_type type)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_TABS:
         if (!mui)
            return 0;
         return (size_t)mui->nav_bar.num_menu_tabs;
      default:
         break;
   }

   return 0;
}

static void materialui_render_background(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width, unsigned video_height,
      bool libretro_running,
      float menu_wallpaper_opacity,
      float menu_framebuffer_opacity)
{
   gfx_display_ctx_draw_t draw;
   bool add_opacity       = false;
   float opacity_override = 1.0f;
   float draw_color[16]   = {
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f
   };
   gfx_display_ctx_driver_t *dispctx = p_disp->dispctx;

   /* Configure draw object */
   draw.x                     = 0;
   draw.y                     = 0;
   draw.width                 = video_width;
   draw.height                = video_height;
   draw.coords                = NULL;
   draw.matrix_data           = NULL;
   draw.prim_type             = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.vertex                = NULL;
   draw.tex_coord             = NULL;
   draw.vertex_count          = 4;
   draw.pipeline_id           = 0;
   draw.pipeline_active       = false;
   draw.backend_data          = NULL;
   draw.color                 = draw_color;
   draw.texture               = 0;

   if (mui->textures.bg && !libretro_running)
   {
      draw.texture = mui->textures.bg;

      /* We are showing a wallpaper - set opacity
       * override to menu_wallpaper_opacity */
      add_opacity      = true;
      opacity_override = menu_wallpaper_opacity;
   }
   else
   {
      /* Copy 'list_background' colour to draw colour */
      memcpy(draw_color, mui->colors.list_background, sizeof(draw_color));

      /* We are not showing a wallpaper - if content
       * is running, set opacity override to
       * menu_framebuffer_opacity */
      if (libretro_running)
      {
         add_opacity      = true;
         opacity_override = menu_framebuffer_opacity;
      }
   }

   /* Draw background */
   if (dispctx)
   {
      if (dispctx->blend_begin)
         dispctx->blend_begin(userdata);
      gfx_display_draw_bg(p_disp, &draw, userdata,
            add_opacity, opacity_override);
      if (dispctx->draw)
         if (draw.height > 0 && draw.width > 0)
            dispctx->draw(&draw, userdata, video_width, video_height);
      if (dispctx->blend_end)
         dispctx->blend_end(userdata);
   }
}

static void materialui_render_landscape_border(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      unsigned header_height, int x_offset)
{
   if (mui->landscape_optimization.enabled)
   {
      unsigned border_height = video_height - header_height - mui->nav_bar_layout_height;
      int left_x             = x_offset;
      int right_x            = x_offset + (int)video_width -
            (int)mui->nav_bar_layout_width -
            (int)mui->landscape_optimization.border_width;
      int y                  = (int)header_height;

      /* Draw left border */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            left_x,
            y,
            mui->landscape_optimization.border_width,
            border_height,
            video_width,
            video_height,
            mui->colors.landscape_border_shadow_left,
            NULL);

      /* Draw right border */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            right_x,
            y,
            mui->landscape_optimization.border_width,
            border_height,
            video_width,
            video_height,
            mui->colors.landscape_border_shadow_right,
            NULL);
   }
}

static void materialui_render_selection_highlight(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width, unsigned video_height,
      unsigned header_height, int x_offset,
      size_t selection, float *highlight_color,
      float *shadow_top_colour, float *shadow_bottom_colour)
{
   /* Only draw highlight if selection is onscreen */
   if (materialui_entry_onscreen(mui, selection))
   {
      int highlight_x;
      int highlight_y;
      int highlight_width;
      int highlight_height;
      materialui_node_t *node  = NULL;
      file_list_t *list        = menu_entries_get_selection_buf_ptr(0);
      if (!list)
         return;

      node = (materialui_node_t*)list->list[selection].userdata;
      if (!node)
         return;

      /* Now we have a valid node, can determine
       * highlight position and size...
       * > Note: We round x/y position down and add 1 to
       *   the height in order to avoid obvious 'seams'
       *   when entries have dividers (rounding errors
       *   would otherwise cause 1px vertical gaps) */
      highlight_x      = (int)(x_offset + node->x);
      highlight_width  = (int)(node->entry_width + 0.5f);
      highlight_y      = (int)((float)header_height - mui->scroll_y + node->y);
      highlight_height = (int)(node->entry_height + 1.5f);

      /* Draw highlight quad */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            highlight_x,
            highlight_y,
            (unsigned)highlight_width,
            (unsigned)highlight_height,
            video_width,
            video_height,
            highlight_color,
            NULL);

      /* Draw shadow, if required */
      if (mui->show_selection_marker_shadow)
      {
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               highlight_x,
               highlight_y,
               (unsigned)highlight_width,
               mui->selection_marker_shadow_height,
               video_width,
               video_height,
               shadow_top_colour,
               NULL);

         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               highlight_x,
               highlight_y + highlight_height -
                     (int)mui->selection_marker_shadow_height,
               (unsigned)highlight_width,
               mui->selection_marker_shadow_height,
               video_width,
               video_height,
               shadow_bottom_colour,
               NULL);
      }
   }
}

static void materialui_render_entry_touch_feedback(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width, unsigned video_height,
      unsigned header_height, int x_offset,
      size_t current_selection)
{
   /* Check whether pointer is currently
    * held and stationary */
   bool pointer_active =
         (!mui->scrollbar.dragged && !mui->show_fullscreen_thumbnails &&
          mui->pointer.pressed && !mui->pointer.dragged);

   /* If pointer is held and stationary, need to check
    * that current pointer selection is valid
    * i.e. user may be touching the header/navigation bar,
    * or pointer may no longer be held above the entry
    * currently selected for feedback animations */
   if (pointer_active)
      pointer_active = (mui->touch_feedback_selection == menu_input_get_pointer_selection()) &&
                       (mui->pointer.x >  mui->landscape_optimization.border_width) &&
                       (mui->pointer.x <  video_width - mui->landscape_optimization.border_width - mui->nav_bar_layout_width) &&
                       (mui->pointer.y >= header_height) &&
                       (mui->pointer.y <= video_height - mui->nav_bar_layout_height - mui->status_bar.height);

   /* Touch feedback highlight fades in when pointer
    * is held stationary on a menu entry */
   if (pointer_active)
   {
      /* If pointer is held on currently selected item,
       * background highlight is already drawn
       * > Feedback animation is over, so reset
       *   alpha value and draw nothing */
      if (mui->touch_feedback_selection == current_selection)
      {
         mui->touch_feedback_alpha = 0.0f;
         return;
      }

      /* Update highlight opacity */
      mui->touch_feedback_alpha = (float)mui->pointer.press_duration / (float)MENU_INPUT_PRESS_TIME_SHORT;
      mui->touch_feedback_alpha = (mui->touch_feedback_alpha > 1.0f) ? 1.0f : mui->touch_feedback_alpha;
   }
   /* If pointer has moved, or has been released, any
    * unfinished feedback highlight animation must
    * fade out */
   else if (mui->touch_feedback_alpha > 0.0f)
   {
      gfx_animation_t *p_anim    = anim_get_ptr();
      mui->touch_feedback_alpha -= (p_anim->delta_time * 1000.0f) 
         / (float)MENU_INPUT_PRESS_TIME_SHORT;
      mui->touch_feedback_alpha = (mui->touch_feedback_alpha < 0.0f) ? 0.0f : mui->touch_feedback_alpha;
   }

   /* If alpha value is greater than zero, draw
    * touch feedback highlight */
   if (mui->touch_feedback_alpha > 0.0f)
   {
      float higlight_color[16];
      float shadow_top_color[16];
      float shadow_bottom_color[16];

      /* Set highlight colour */
      memcpy(higlight_color, mui->colors.list_highlighted_background,
            sizeof(higlight_color));
      gfx_display_set_alpha(higlight_color,
            mui->transition_alpha * mui->touch_feedback_alpha);

      /* Set shadow colour (if required) */
      if (mui->show_selection_marker_shadow)
      {
         float selection_marker_shadow_alpha =
               mui->colors.selection_marker_shadow_opacity *
                     mui->transition_alpha * mui->touch_feedback_alpha;

         memcpy(shadow_top_color, mui->colors.selection_marker_shadow_top,
               sizeof(shadow_top_color));
         shadow_top_color[11]   = selection_marker_shadow_alpha;
         shadow_top_color[15]   = selection_marker_shadow_alpha;

         memcpy(shadow_bottom_color, mui->colors.selection_marker_shadow_bottom,
               sizeof(shadow_bottom_color));
         shadow_bottom_color[3] = selection_marker_shadow_alpha;
         shadow_bottom_color[7] = selection_marker_shadow_alpha;
      }

      /* Draw highlight */
      materialui_render_selection_highlight(
            mui, p_disp, userdata, video_width, video_height,
            header_height, x_offset,
            mui->touch_feedback_selection,
            higlight_color,
            shadow_top_color, shadow_bottom_color);
   }
}

static void materialui_render_header(
      materialui_handle_t *mui,
      settings_t *settings,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width, unsigned video_height,
      math_matrix_4x4 *mymat)
{
   char menu_title_buf[255];
   size_t menu_title_margin              = 0;
   int usable_sys_bar_width              = (int)video_width - (int)mui->nav_bar_layout_width;
   int usable_title_bar_width            = usable_sys_bar_width;
   size_t sys_bar_battery_width          = 0;
   size_t sys_bar_clock_width            = 0;
   int sys_bar_text_y                    = (int)(((float)mui->sys_bar_height / 2.0f) + (float)mui->font_data.hint.line_centre_offset);
   int title_x                           = 0;
   bool show_back_icon                   = menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL);
   bool show_search_icon                 = mui->is_playlist || mui->is_file_list || mui->is_core_updater_list;
   bool show_switch_view_icon            = mui->is_playlist && mui->primary_thumbnail_available;
   bool use_landscape_layout             = !mui->is_portrait &&
         (mui->last_landscape_layout_optimization != MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED);
   const char *menu_title                = mui->menu_title;
   bool battery_level_enable             = settings->bools.menu_battery_level_enable;
   bool menu_timedate_enable             = settings->bools.menu_timedate_enable;
   unsigned menu_timedate_style          = settings->uints.menu_timedate_style;
   unsigned menu_timedate_date_separator = settings->uints.menu_timedate_date_separator;
   bool menu_core_enable                 = settings->bools.menu_core_enable;

   menu_title_buf[0]  = '\0';

   /* Draw background quads
    * > Title bar is underneath system bar
    * > Shadow is underneath title bar */

   /* > Shadow */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         0,
         mui->sys_bar_height + mui->title_bar_height,
         video_width,
         mui->header_shadow_height,
         video_width,
         video_height,
         mui->colors.header_shadow,
         NULL);

   /* > Title bar background */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         0,
         0,
         video_width,
         mui->sys_bar_height + mui->title_bar_height,
         video_width,
         video_height,
         mui->colors.title_bar_background,
         NULL);

   /* > System bar background */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         0,
         0,
         video_width,
         mui->sys_bar_height,
         video_width,
         video_height,
         mui->colors.sys_bar_background,
         NULL);

   /* System bar items */

   /* > Draw battery indicator (if required) */
   if (battery_level_enable)
   {
      gfx_display_ctx_powerstate_t powerstate;
      char percent_str[MUI_BATTERY_PERCENT_MAX_LENGTH];

      percent_str[0] = '\0';

      powerstate.s   = percent_str;
      powerstate.len = sizeof(percent_str);

      menu_display_powerstate(&powerstate);

      if (powerstate.battery_enabled)
      {
         /* Need to determine pixel width of percent string
          * > This is somewhat expensive, so utilise a cache
          *   and only update when the string actually changes */
         if (!string_is_equal(percent_str,
                  mui->sys_bar_cache.battery_percent_str))
         {
            /* Cache new string */
            strlcpy(mui->sys_bar_cache.battery_percent_str, percent_str,
                  MUI_BATTERY_PERCENT_MAX_LENGTH * sizeof(char));

            /* Cache width */
            mui->sys_bar_cache.battery_percent_width = 
               font_driver_get_message_width(
                  mui->font_data.hint.font,
                  mui->sys_bar_cache.battery_percent_str,
                  (unsigned)strlen(mui->sys_bar_cache.battery_percent_str),
                  1.0f);
         }

         if (mui->sys_bar_cache.battery_percent_width > 0)
         {
            /* Set critical by default, to ensure texture_battery
             * is always valid */
            uintptr_t texture_battery = 
               mui->textures.list[MUI_TEXTURE_BATTERY_CRITICAL];

            /* Draw battery icon */
            if (powerstate.charging)
               texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_CHARGING];
            else
            {
               if (powerstate.percent >= 100)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_100];
               else if (powerstate.percent >= 90)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_90];
               else if (powerstate.percent >= 80)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_80];
               else if (powerstate.percent >= 60)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_60];
               else if (powerstate.percent >= 50)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_50];
               else if (powerstate.percent >= 30)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_30];
               else if (powerstate.percent >= 20)
                  texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_20];
            }

            materialui_draw_icon(
                  userdata, p_disp,
                  video_width,
                  video_height,
                  mui->sys_bar_icon_size,
                  (uintptr_t)texture_battery,
                  (int)video_width - (
                     (int)mui->sys_bar_cache.battery_percent_width +
                     (int)mui->sys_bar_margin                      + 
                     (int)mui->sys_bar_icon_size                   + 
                     (int)mui->nav_bar_layout_width),
                  0,
                  0,
                  1,
                  mui->colors.sys_bar_icon,
                  mymat);

            /* Draw percent text */
            gfx_display_draw_text(mui->font_data.hint.font,
                  mui->sys_bar_cache.battery_percent_str,
                  (int)video_width - ((int)mui->sys_bar_cache.battery_percent_width + (int)mui->sys_bar_margin + (int)mui->nav_bar_layout_width),
                  sys_bar_text_y,
                  video_width, video_height, mui->colors.sys_bar_text,
                  TEXT_ALIGN_LEFT, 1.0f, false, 0.0f, false);

            sys_bar_battery_width = mui->sys_bar_cache.battery_percent_width +
                  mui->sys_bar_margin + mui->sys_bar_icon_size;
            usable_sys_bar_width -= sys_bar_battery_width;
         }
      }
   }

   /* > Draw clock (if required) */
   if (menu_timedate_enable)
   {
      gfx_display_ctx_datetime_t datetime;
      char timedate_str[MUI_TIMEDATE_MAX_LENGTH];

      timedate_str[0] = '\0';

      datetime.s              = timedate_str;
      datetime.len            = sizeof(timedate_str);
      datetime.time_mode      = menu_timedate_style;
      datetime.date_separator = menu_timedate_date_separator;

      menu_display_timedate(&datetime);

      /* Need to determine pixel width of time string
       * > This is somewhat expensive, so utilise a cache
       *   and only update when the string actually changes */
      if (!string_is_equal(timedate_str, mui->sys_bar_cache.timedate_str))
      {
         /* Cache new string */
         strlcpy(mui->sys_bar_cache.timedate_str, timedate_str,
               MUI_TIMEDATE_MAX_LENGTH * sizeof(char));

         /* Cache width */
         mui->sys_bar_cache.timedate_width 
            = font_driver_get_message_width(
               mui->font_data.hint.font,
               mui->sys_bar_cache.timedate_str,
               (unsigned)strlen(mui->sys_bar_cache.timedate_str),
               1.0f);
      }

      /* Draw time string */
      if (mui->sys_bar_cache.timedate_width > 0)
      {
         sys_bar_clock_width = mui->sys_bar_cache.timedate_width;

         /* If there is no battery indicator, must add padding */
         if (sys_bar_battery_width == 0)
            sys_bar_clock_width += mui->sys_bar_margin;

         gfx_display_draw_text(mui->font_data.hint.font,
               mui->sys_bar_cache.timedate_str,
               (int)video_width - (
                    (int)sys_bar_clock_width 
                  + (int)sys_bar_battery_width 
                  + (int)mui->nav_bar_layout_width),
               sys_bar_text_y,
               video_width, video_height, mui->colors.sys_bar_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0.0f, false);

         usable_sys_bar_width -= sys_bar_clock_width;
      }
   }

   usable_sys_bar_width -= (2 * mui->sys_bar_margin);
   usable_sys_bar_width  = (usable_sys_bar_width > 0) 
      ? usable_sys_bar_width 
      : 0;

   /* > Draw core name, if required */
   if (menu_core_enable)
   {
      char core_title[255];
      char core_title_buf[255];

      core_title[0]     = '\0';
      core_title_buf[0] = '\0';

      menu_entries_get_core_title(core_title, sizeof(core_title));

      if (mui->use_smooth_ticker)
      {
         mui->ticker_smooth.font        = mui->font_data.hint.font;
         mui->ticker_smooth.selected    = true;
         mui->ticker_smooth.field_width = (unsigned)usable_sys_bar_width;
         mui->ticker_smooth.src_str     = core_title;
         mui->ticker_smooth.dst_str     = core_title_buf;
         mui->ticker_smooth.dst_str_len = sizeof(core_title_buf);

         gfx_animation_ticker_smooth(&mui->ticker_smooth);
      }
      else
      {
         mui->ticker.s        = core_title_buf;
         mui->ticker.len      = (unsigned)(usable_sys_bar_width / mui->font_data.hint.glyph_width);
         mui->ticker.str      = core_title;
         mui->ticker.selected = true;

         gfx_animation_ticker(&mui->ticker);
      }

      gfx_display_draw_text(mui->font_data.hint.font, core_title_buf,
            (int)mui->ticker_x_offset + (int)mui->sys_bar_margin,
            sys_bar_text_y,
            video_width, video_height, mui->colors.sys_bar_text,
            TEXT_ALIGN_LEFT, 1.0f, false, 0.0f, false);
   }

   /* Title bar items */

   /* > Draw 'back' icon, if required */
   menu_title_margin = mui->margin;

   if (show_back_icon)
   {
      menu_title_margin = mui->icon_size;

      materialui_draw_icon(
            userdata, p_disp,
            video_width,
            video_height,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_BACK],
            0,
            (int)mui->sys_bar_height,
            0,
            1,
            mui->colors.header_icon,
            mymat);
   }

   usable_title_bar_width -= menu_title_margin;

   /* > Draw 'search' icon, if required */
   if (show_search_icon)
   {
      materialui_draw_icon(
            userdata, p_disp,
            video_width,
            video_height,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_SEARCH],
            (int)video_width - (int)mui->icon_size - (int)mui->nav_bar_layout_width,
            (int)mui->sys_bar_height,
            0,
            1,
            mui->colors.header_icon,
            mymat);

      usable_title_bar_width -= mui->icon_size;

      /* > Draw 'switch view' icon, if required
       *   Note: We can take a shortcut here because
       *   'switch view' can only be shown if
       *   'search' is also shown... */
      if (show_switch_view_icon)
      {
         materialui_draw_icon(
               userdata, p_disp,
               video_width,
               video_height,
               mui->icon_size,
               mui->textures.list[MUI_TEXTURE_SWITCH_VIEW],
               (int)video_width - (2 * (int)mui->icon_size) 
               - (int)mui->nav_bar_layout_width,
               (int)mui->sys_bar_height,
               0,
               1,
               mui->colors.header_icon,
               mymat);

         usable_title_bar_width -= mui->icon_size;
      }
   }
   else
      usable_title_bar_width -= mui->margin;

   /* If landscape optimisation is enabled and we are
    * drawing a back icon but no search icon (and by
    * extension, no switch view icon), title maximum
    * width must be reduced (otherwise cannot centre
    * properly...) */
   if (use_landscape_layout)
      if (show_back_icon && !show_search_icon)
         usable_title_bar_width -= (mui->icon_size - mui->margin);

   usable_title_bar_width = (usable_title_bar_width > 0) ? usable_title_bar_width : 0;

   /* > Draw title string */

   /* >> If fullscreen thumbnail view is enabled, title
    *    is the label of the currently selected entry */
   if (mui->show_fullscreen_thumbnails)
      menu_title = mui->fullscreen_thumbnail_label;

   if (mui->use_smooth_ticker)
   {
      mui->ticker_smooth.font        = mui->font_data.title.font;
      mui->ticker_smooth.selected    = true;
      mui->ticker_smooth.field_width = (unsigned)usable_title_bar_width;
      mui->ticker_smooth.src_str     = menu_title;
      mui->ticker_smooth.dst_str     = menu_title_buf;
      mui->ticker_smooth.dst_str_len = sizeof(menu_title_buf);

      /* If ticker is not active and landscape
       * optimisation is enabled, centre the title text */
      if (!gfx_animation_ticker_smooth(&mui->ticker_smooth))
      {
         if (use_landscape_layout)
         {
            title_x = (int)(usable_title_bar_width - mui->ticker_str_width) >> 1;

            /* Even more trickery required for proper centring
             * if both search and switch view icons are shown... */
            if (show_search_icon && show_switch_view_icon)
               if (mui->ticker_str_width + mui->ticker_x_offset <
                     usable_title_bar_width - mui->icon_size)
                  title_x += (int)(mui->icon_size >> 1);
         }
      }
   }
   else
   {
      mui->ticker.s        = menu_title_buf;
      mui->ticker.len      = (unsigned)(usable_title_bar_width / mui->font_data.title.glyph_width) - 1;
      mui->ticker.str      = menu_title;
      mui->ticker.selected = true;

      /* If ticker is not active and landscape
       * optimisation is enabled, centre the title text */
      if (!gfx_animation_ticker(&mui->ticker))
      {
         if (use_landscape_layout)
         {
            int str_width = (int)(utf8len(menu_title_buf) *
                  mui->font_data.title.glyph_width);

            title_x = (int)(usable_title_bar_width - str_width) >> 1;

            /* Even more trickery required for proper centring
             * if both search and switch view icons are shown... */
            if (show_search_icon && show_switch_view_icon)
               if (str_width < usable_title_bar_width - mui->icon_size)
                  title_x += (int)(mui->icon_size >> 1);
         }
      }
   }

   title_x += (int)(mui->ticker_x_offset + menu_title_margin);

   gfx_display_draw_text(mui->font_data.title.font, menu_title_buf,
         title_x,
         (int)(mui->sys_bar_height + (mui->title_bar_height / 2.0f) + mui->font_data.title.line_centre_offset),
         video_width, video_height, mui->colors.header_text,
         TEXT_ALIGN_LEFT, 1.0f, false, 0.0f, false);
}

/* Use separate functions for bottom/right navigation
 * bars. This involves substantial code duplication, but if
 * we try to handle this with a single function then
 * things get incredibly messy and inefficient... */
static void materialui_render_nav_bar_bottom(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width, unsigned video_height,
      math_matrix_4x4 *mymat)
{
   unsigned i;
   unsigned nav_bar_width           = video_width;
   unsigned nav_bar_height          = mui->nav_bar.width;
   int nav_bar_x                    = 0;
   int nav_bar_y                    = (int)video_height - (int)mui->nav_bar.width;
   unsigned num_tabs                = mui->nav_bar.num_menu_tabs + MUI_NAV_BAR_NUM_ACTION_TABS;
   float tab_width                  = (float)video_width / (float)num_tabs;
   unsigned tab_width_int           = (unsigned)(tab_width + 0.5f);
   unsigned selection_marker_width  = tab_width_int;
   unsigned selection_marker_height = mui->nav_bar.selection_marker_width;
   int selection_marker_y           = (int)video_height - (int)mui->nav_bar.selection_marker_width;

   /* Draw navigation bar background */

   /* > Background */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         nav_bar_x,
         nav_bar_y,
         nav_bar_width,
         nav_bar_height,
         video_width,
         video_height,
         mui->colors.nav_bar_background,
         NULL);

   /* > Divider */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         nav_bar_x,
         nav_bar_y,
         nav_bar_width,
         mui->nav_bar.divider_width,
         video_width,
         video_height,
         mui->colors.divider,
         NULL);

   /* Draw tabs */

   /* > Back - left hand side */
   materialui_draw_icon(
         userdata, p_disp,
         video_width,
         video_height,
         mui->icon_size,
         mui->textures.list[mui->nav_bar.back_tab.texture_index],
         (int)((0.5f * tab_width) - ((float)mui->icon_size / 2.0f)),
         nav_bar_y,
         0,
         1,
         mui->nav_bar.back_tab.enabled 
         ? mui->colors.nav_bar_icon_passive
         : mui->colors.nav_bar_icon_disabled,
         mymat);

   /* > Resume - right hand side */
   materialui_draw_icon(
         userdata, p_disp,
         video_width,
         video_height,
         mui->icon_size,
         mui->textures.list[mui->nav_bar.resume_tab.texture_index],
         (int)((((float)num_tabs - 0.5f) * tab_width) - ((float)mui->icon_size / 2.0f)),
         nav_bar_y,
         0,
         1,
         mui->nav_bar.resume_tab.enabled 
         ? mui->colors.nav_bar_icon_passive
         : mui->colors.nav_bar_icon_disabled,
         mymat);

   /* Menu tabs - in the centre, left to right */
   for (i = 0; i < mui->nav_bar.num_menu_tabs; i++)
   {
      materialui_nav_bar_menu_tab_t *tab = &mui->nav_bar.menu_tabs[i];
      float *draw_color                  = tab->active ?
            mui->colors.nav_bar_icon_active : mui->colors.nav_bar_icon_passive;

      /* Draw icon */
      materialui_draw_icon(
            userdata, p_disp,
            video_width,
            video_height,
            mui->icon_size,
            mui->textures.list[tab->texture_index],
            (((float)i + 1.5f) * tab_width) 
            - ((float)mui->icon_size / 2.0f),
            nav_bar_y,
            0,
            1,
            draw_color,
            mymat);

      /* Draw selection marker */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            (int)((i + 1) * tab_width_int),
            selection_marker_y,
            selection_marker_width,
            selection_marker_height,
            video_width,
            video_height,
            draw_color,
            NULL);
   }
}

static void materialui_render_nav_bar_right(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      math_matrix_4x4 *mymat)
{
   unsigned i;
   unsigned nav_bar_width           = mui->nav_bar.width;
   unsigned nav_bar_height          = video_height;
   int nav_bar_x                    = (int)video_width - (int)mui->nav_bar.width;
   int nav_bar_y                    = 0;
   unsigned num_tabs                = mui->nav_bar.num_menu_tabs + MUI_NAV_BAR_NUM_ACTION_TABS;
   float tab_height                 = (float)video_height / (float)num_tabs;
   unsigned tab_height_int          = (unsigned)(tab_height + 0.5f);
   unsigned selection_marker_width  = mui->nav_bar.selection_marker_width;
   unsigned selection_marker_height = tab_height_int;
   int selection_marker_x           = (int)video_width - (int)mui->nav_bar.selection_marker_width;

   /* Draw navigation bar background */

   /* > Background */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         nav_bar_x,
         nav_bar_y,
         nav_bar_width,
         nav_bar_height,
         video_width,
         video_height,
         mui->colors.nav_bar_background,
         NULL);

   /* > Divider */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         nav_bar_x,
         nav_bar_y,
         mui->nav_bar.divider_width,
         nav_bar_height,
         video_width,
         video_height,
         mui->colors.divider,
         NULL);

   /* Draw tabs */

   /* > Back - bottom */
   materialui_draw_icon(
         userdata, p_disp,
         video_width,
         video_height,
         mui->icon_size,
         mui->textures.list[mui->nav_bar.back_tab.texture_index],
         nav_bar_x,
         (int)((((float)num_tabs - 0.5f) * tab_height) - ((float)mui->icon_size / 2.0f)),
         0,
         1,
         mui->nav_bar.back_tab.enabled 
         ? mui->colors.nav_bar_icon_passive
         : mui->colors.nav_bar_icon_disabled,
         mymat);

   /* > Resume - top */
   materialui_draw_icon(
         userdata, p_disp,
         video_width,
         video_height,
         mui->icon_size,
         mui->textures.list[mui->nav_bar.resume_tab.texture_index],
         nav_bar_x,
         (int)((0.5f * tab_height) 
            - ((float)mui->icon_size / 2.0f)),
         0,
         1,
         mui->nav_bar.resume_tab.enabled
         ? mui->colors.nav_bar_icon_passive 
         : mui->colors.nav_bar_icon_disabled,
         mymat);

   /* Menu tabs - in the centre, top to bottom */
   for (i = 0; i < mui->nav_bar.num_menu_tabs; i++)
   {
      materialui_nav_bar_menu_tab_t *tab = &mui->nav_bar.menu_tabs[i];
      float *draw_color                  = tab->active ?
            mui->colors.nav_bar_icon_active : mui->colors.nav_bar_icon_passive;

      /* Draw icon */
      materialui_draw_icon(
            userdata, p_disp,
            video_width,
            video_height,
            mui->icon_size,
            mui->textures.list[tab->texture_index],
            nav_bar_x,
            (((float)i + 1.5f) * tab_height) 
            - ((float)mui->icon_size / 2.0f),
            0,
            1,
            draw_color,
            mymat);

      /* Draw selection marker */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            selection_marker_x,
            (int)((i + 1) * tab_height_int),
            selection_marker_width,
            selection_marker_height,
            video_width,
            video_height,
            draw_color,
            NULL);
   }
}

static void materialui_render_nav_bar(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      math_matrix_4x4 *mymat)
{
   switch (mui->nav_bar.location)
   {
      case MUI_NAV_BAR_LOCATION_RIGHT:
         materialui_render_nav_bar_right(
            mui, p_disp, userdata, video_width, video_height,
            mymat);
         break;
      case MUI_NAV_BAR_LOCATION_HIDDEN:
         /* Draw nothing */
         break;
      /* 'Bottom' is the default case */
      case MUI_NAV_BAR_LOCATION_BOTTOM:
      default:
         materialui_render_nav_bar_bottom(
            mui, p_disp, userdata, video_width, video_height,
            mymat);
         break;
   }
}

/* Convenience function for accessing the thumbnails
 * associated with the selected node.
 * > Thumbnails are only valid if function returns true
 * > Returns false if current selection is off screen,
 *   or node is unallocated */
static bool materialui_get_selected_thumbnails(
      materialui_handle_t *mui, size_t selection,
      gfx_thumbnail_t **primary_thumbnail,
      gfx_thumbnail_t **secondary_thumbnail)
{
   file_list_t *list       = NULL;
   materialui_node_t *node = NULL;

   /* Ensure selection is on screen
    * > Special case: When viewing 'desktop'-layout
    *   playlists skip this check, since thumbnails
    *   for the selected item are always shown via
    *   the sidebar regardless of whether the current
    *   selection is on screen */
   if ((mui->list_view_type != MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP) &&
       !materialui_entry_onscreen(mui, selection))
      return false;

   /* Get currently selected node */
   list = menu_entries_get_selection_buf_ptr(0);
   if (!list)
      return false;

   node = (materialui_node_t*)list->list[selection].userdata;
   if (!node)
      return false;

   /* Assign thumbnails */
   *primary_thumbnail   = &node->thumbnails.primary;
   *secondary_thumbnail = &node->thumbnails.secondary;

   return true;
}

/* Disables the fullscreen thumbnail view, with
 * an optional fade out animation */
static void materialui_hide_fullscreen_thumbnails(
      materialui_handle_t *mui, bool animate)
{
   uintptr_t alpha_tag = (uintptr_t)&mui->fullscreen_thumbnail_alpha;

   /* Kill any existing fade in/out animations */
   gfx_animation_kill_by_tag(&alpha_tag);

   /* Check whether animations are enabled */
   if (animate && (mui->fullscreen_thumbnail_alpha > 0.0f))
   {
      gfx_animation_ctx_entry_t animation_entry;

      /* Configure fade out animation */
      animation_entry.easing_enum  = EASING_OUT_QUAD;
      animation_entry.tag          = alpha_tag;
      animation_entry.duration     = gfx_thumb_get_ptr()->fade_duration;
      animation_entry.target_value = 0.0f;
      animation_entry.subject      = &mui->fullscreen_thumbnail_alpha;
      animation_entry.cb           = NULL;
      animation_entry.userdata     = NULL;

      /* Push animation */
      gfx_animation_push(&animation_entry);
   }
   /* No animation - just set thumbnail alpha to zero */
   else
      mui->fullscreen_thumbnail_alpha = 0.0f;

   /* Disable fullscreen thumbnails */
   mui->show_fullscreen_thumbnails = false;
}

/* Enables (and triggers a fade in of) the fullscreen
 * thumbnail view */
static void materialui_show_fullscreen_thumbnails(
      materialui_handle_t *mui, size_t selection)
{
   menu_entry_t selected_entry;
   gfx_animation_ctx_entry_t animation_entry;
   gfx_thumbnail_t *primary_thumbnail   = NULL;
   gfx_thumbnail_t *secondary_thumbnail = NULL;
   uintptr_t                  alpha_tag = (uintptr_t)
      &mui->fullscreen_thumbnail_alpha;
   const char *thumbnail_label          = NULL;

   /* Before showing fullscreen thumbnails, must
    * ensure that any existing fullscreen thumbnail
    * view is disabled... */
   materialui_hide_fullscreen_thumbnails(mui, false);

   /* Sanity check: Return immediately if this is a view
    * mode without thumbnails */
   if ((mui->list_view_type == MUI_LIST_VIEW_DEFAULT) ||
       (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST))
      return;

   /* Get thumbnails */
   if (!materialui_get_selected_thumbnails(
            mui, selection, &primary_thumbnail, &secondary_thumbnail))
      return;

   /* We can only enable fullscreen thumbnails if
    * current selection has at least one valid thumbnail
    * and all thumbnails for current selection are already
    * loaded/available */
   if ((primary_thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE) &&
       (mui->secondary_thumbnail_enabled &&
            ((secondary_thumbnail->status != GFX_THUMBNAIL_STATUS_MISSING) &&
             (secondary_thumbnail->status != GFX_THUMBNAIL_STATUS_AVAILABLE))))
      return;

   if ((primary_thumbnail->status == GFX_THUMBNAIL_STATUS_MISSING) &&
       (!mui->secondary_thumbnail_enabled ||
            (secondary_thumbnail->status != GFX_THUMBNAIL_STATUS_AVAILABLE)))
      return;

   /* Menu list must be stationary while fullscreen
    * thumbnails are shown
    * > Kill any existing scroll animation
    *   and reset scroll acceleration */
   materialui_kill_scroll_animation(mui);

   /* Cache selected entry label
    * (used as menu title when fullscreen thumbnails
    * are shown) */
   mui->fullscreen_thumbnail_label[0] = '\0';

   /* > Get menu entry */
   MENU_ENTRY_INIT(selected_entry);
   selected_entry.path_enabled     = false;
   selected_entry.value_enabled    = false;
   selected_entry.sublabel_enabled = false;
   menu_entry_get(&selected_entry, 0, selection, NULL, true);

   /* > Get entry label */
   if (!string_is_empty(selected_entry.rich_label))
      thumbnail_label          = selected_entry.rich_label;
   else
      thumbnail_label          = selected_entry.path;

   /* > Sanity check */
   if (!string_is_empty(thumbnail_label))
      strlcpy(
            mui->fullscreen_thumbnail_label,
            thumbnail_label,
            sizeof(mui->fullscreen_thumbnail_label));

   /* Configure fade in animation */
   animation_entry.easing_enum  = EASING_OUT_QUAD;
   animation_entry.tag          = alpha_tag;
   animation_entry.duration     = gfx_thumb_get_ptr()->fade_duration;
   animation_entry.target_value = 1.0f;
   animation_entry.subject      = &mui->fullscreen_thumbnail_alpha;
   animation_entry.cb           = NULL;
   animation_entry.userdata     = NULL;

   /* Push animation */
   gfx_animation_push(&animation_entry);

   /* Enable fullscreen thumbnails */
   mui->fullscreen_thumbnail_selection = selection;
   mui->show_fullscreen_thumbnails     = true;
}

static void materialui_render_fullscreen_thumbnails(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width, unsigned video_height,
      unsigned header_height,
      size_t selection)
{
   /* Check whether fullscreen thumbnails are visible */
   if (mui->fullscreen_thumbnail_alpha > 0.0f)
   {
      gfx_thumbnail_t *primary_thumbnail   = NULL;
      gfx_thumbnail_t *secondary_thumbnail = NULL;
      bool show_primary_thumbnail           = false;
      bool show_secondary_thumbnail         = false;
      unsigned num_thumbnails               = 0;
      float primary_thumbnail_draw_width    = 0.0f;
      float primary_thumbnail_draw_height   = 0.0f;
      float secondary_thumbnail_draw_width  = 0.0f;
      float secondary_thumbnail_draw_height = 0.0f;
      int view_width;
      int view_height;
      int thumbnail_box_width;
      int thumbnail_box_height;
      int primary_thumbnail_x;
      int primary_thumbnail_y;
      int secondary_thumbnail_x;
      int secondary_thumbnail_y;

      /* Sanity check: Return immediately if this is a view
       * mode without thumbnails
       * > Note: Baring inexplicable internal errors, this
       *   can never happen... */
      if ((mui->list_view_type == MUI_LIST_VIEW_DEFAULT) ||
          (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST))
         goto error;

      /* Paranoid safety check: ensure that current
       * selection matches the entry selected when
       * fullscreen thumbnails were enabled.
       * This can only fail in extreme cases, if
       * the user manages to change the selection
       * while fullscreen thumbnails are fading out */
      if (selection != mui->fullscreen_thumbnail_selection)
         goto error;

      /* Get thumbnails */
      if (!materialui_get_selected_thumbnails(
               mui, selection, &primary_thumbnail, &secondary_thumbnail))
         goto error;

      /* Get number of 'active' thumbnails */
      show_primary_thumbnail =
            (primary_thumbnail->status != GFX_THUMBNAIL_STATUS_MISSING);
      show_secondary_thumbnail =
            mui->secondary_thumbnail_enabled &&
            (secondary_thumbnail->status != GFX_THUMBNAIL_STATUS_MISSING);

      if (show_primary_thumbnail)
         num_thumbnails++;

      if (show_secondary_thumbnail)
         num_thumbnails++;

      /* Do nothing if both thumbnails are missing
       * > Note: Baring inexplicable internal errors, this
       *   can never happen... */
      if (num_thumbnails < 1)
         goto error;

      /* Get dimensions of list view */
      view_width  = (int)video_width  - (int)mui->nav_bar_layout_width;
      view_height = (int)video_height - (int)mui->nav_bar_layout_height - (int)header_height;

      /* Check screen orientation
       * > When using portrait layouts, primary is shown
       *   at the top, secondary at the bottom
       * > When using landscape layouts, primary is shown
       *   on the left, secondary on the right */
      if (mui->is_portrait)
      {
         /* Thumbnail bounding box width is fixed */
         thumbnail_box_width = view_width - (int)(mui->margin * 4);

         /* Thumbnail x position is fixed */
         primary_thumbnail_x   = (int)(mui->margin * 2);
         secondary_thumbnail_x = primary_thumbnail_x;

         /* Thumbnail bounding box height and y position
          * depend upon number of active thumbnails */
         if (num_thumbnails == 2)
         {
            thumbnail_box_height  = (view_height - (int)(mui->margin * 6)) >> 1;
            primary_thumbnail_y   = (int)header_height + (int)(mui->margin * 2);
            secondary_thumbnail_y = primary_thumbnail_y + thumbnail_box_height + (int)(mui->margin * 2);
         }
         else
         {
            thumbnail_box_height  = view_height - (int)(mui->margin * 4);
            primary_thumbnail_y   = (int)header_height + (int)(mui->margin * 2);
            secondary_thumbnail_y = primary_thumbnail_y;
         }
      }
      else
      {
         /* Thumbnail bounding box height is fixed */
         thumbnail_box_height = view_height - (int)(mui->margin * 4);

         /* Thumbnail y position is fixed */
         primary_thumbnail_y   = (int)header_height + (int)(mui->margin * 2);
         secondary_thumbnail_y = primary_thumbnail_y;

         /* Thumbnail bounding box width and x position
          * depend upon number of active thumbnails */
         if (num_thumbnails == 2)
         {
            thumbnail_box_width   = (view_width - (int)(mui->margin * 6)) >> 1;
            primary_thumbnail_x   = (int)(mui->margin * 2);
            secondary_thumbnail_x = primary_thumbnail_x + thumbnail_box_width + (int)(mui->margin * 2);
         }
         else
         {
            thumbnail_box_width   = view_width - (int)(mui->margin * 4);
            primary_thumbnail_x   = (int)(mui->margin * 2);
            secondary_thumbnail_x = primary_thumbnail_x;
         }
      }

      /* Sanity check */
      if ((view_width < 1) ||
          (view_height < 1) ||
          (thumbnail_box_width < 1) ||
          (thumbnail_box_height < 1))
         goto error;

      /* Get thumbnail draw dimensions
       * > Note: The following code is a bit awkward, since
       *   we have to do things in a very specific order
       *   - i.e. we cannot determine proper thumbnail
       *     layout until we have thumbnail draw dimensions.
       *     and we cannot get draw dimensions until we have
       *     the bounding box dimensions...  */
      if (show_primary_thumbnail)
      {
         gfx_thumbnail_get_draw_dimensions(
               primary_thumbnail,
               thumbnail_box_width, thumbnail_box_height, 1.0f,
               &primary_thumbnail_draw_width, &primary_thumbnail_draw_height);

         /* Sanity check */
         if ((primary_thumbnail_draw_width <= 0.0f) ||
             (primary_thumbnail_draw_height <= 0.0f))
            goto error;
      }

      if (show_secondary_thumbnail)
      {
         gfx_thumbnail_get_draw_dimensions(
               secondary_thumbnail,
               thumbnail_box_width, thumbnail_box_height, 1.0f,
               &secondary_thumbnail_draw_width, &secondary_thumbnail_draw_height);

         /* Sanity check */
         if ((secondary_thumbnail_draw_width <= 0.0f) ||
             (secondary_thumbnail_draw_height <= 0.0f))
            goto error;
      }

      /* Adjust thumbnail draw positions to achieve
       * uniform appearance (accounting for actual
       * draw dimensions...) */
      if (num_thumbnails == 2)
      {
         if (mui->is_portrait)
         {
            int primary_padding   = (thumbnail_box_height - (int)primary_thumbnail_draw_height)   >> 1;
            int secondary_padding = (thumbnail_box_height - (int)secondary_thumbnail_draw_height) >> 1;

            /* Move thumbnails as close together as possible,
             * and vertically centre the resultant 'block'
             * of images */
            primary_thumbnail_y   += secondary_padding;
            secondary_thumbnail_y -= primary_padding;
         }
         else
         {
            int primary_padding   = (thumbnail_box_width - (int)primary_thumbnail_draw_width)   >> 1;
            int secondary_padding = (thumbnail_box_width - (int)secondary_thumbnail_draw_width) >> 1;

            /* Move thumbnails as close together as possible,
             * and horizontally centre the resultant 'block'
             * of images */
            primary_thumbnail_x   += secondary_padding;
            secondary_thumbnail_x -= primary_padding;
         }
      }

      /* Set colour alpha values */
      gfx_display_set_alpha(
            mui->colors.screen_fade,
            mui->colors.screen_fade_opacity * mui->fullscreen_thumbnail_alpha);

      gfx_display_set_alpha(
            mui->colors.surface_background, mui->fullscreen_thumbnail_alpha);

      /* Darken background */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0,
            header_height,
            (unsigned)view_width,
            (unsigned)view_height,
            video_width,
            video_height,
            mui->colors.screen_fade,
            NULL);

      /* Draw thumbnails
       * > Primary */
      if (show_primary_thumbnail)
      {
         /* Background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               primary_thumbnail_x - (int)(mui->margin >> 1) +
                     ((thumbnail_box_width - (int)primary_thumbnail_draw_width) >> 1),
               primary_thumbnail_y - (int)(mui->margin >> 1) +
                     ((thumbnail_box_height - (int)primary_thumbnail_draw_height) >> 1),
               (unsigned)primary_thumbnail_draw_width + mui->margin,
               (unsigned)primary_thumbnail_draw_height + mui->margin,
               video_width,
               video_height,
               mui->colors.surface_background,
               NULL);

         /* Thumbnail */
         gfx_thumbnail_draw(
               userdata,
               video_width,
               video_height,
               primary_thumbnail,
               primary_thumbnail_x,
               primary_thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               GFX_THUMBNAIL_ALIGN_CENTRE,
               mui->fullscreen_thumbnail_alpha,
               1.0f,
               NULL);
      }

      /* > Secondary */
      if (show_secondary_thumbnail)
      {
         /* Background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               secondary_thumbnail_x - (int)(mui->margin >> 1) +
                     ((thumbnail_box_width - (int)secondary_thumbnail_draw_width) >> 1),
               secondary_thumbnail_y - (int)(mui->margin >> 1) +
                     ((thumbnail_box_height - (int)secondary_thumbnail_draw_height) >> 1),
               (unsigned)secondary_thumbnail_draw_width + mui->margin,
               (unsigned)secondary_thumbnail_draw_height + mui->margin,
               video_width,
               video_height,
               mui->colors.surface_background,
               NULL);

         /* Thumbnail */
         gfx_thumbnail_draw(
               userdata,
               video_width,
               video_height,
               secondary_thumbnail,
               secondary_thumbnail_x,
               secondary_thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               GFX_THUMBNAIL_ALIGN_CENTRE,
               mui->fullscreen_thumbnail_alpha,
               1.0f,
               NULL);
      }
   }

   return;

error:
   /* If fullscreen thumbnails are enabled at
    * this point, must disable them immediately... */
   if (mui->show_fullscreen_thumbnails)
      materialui_hide_fullscreen_thumbnails(mui, false);
}

/* Sets transparency of all menu list colours if
 * a transition animation is in process */
static void materialui_colors_set_transition_alpha(materialui_handle_t *mui)
{
   if (mui->transition_alpha < 1.0f)
   {
      float alpha        = mui->transition_alpha;
      unsigned alpha_255 = (unsigned)((255.0f * alpha) + 0.5f);

      /* Text colours */
      mui->colors.list_text                  = (mui->colors.list_text                  & 0xFFFFFF00) | alpha_255;
      mui->colors.list_text_highlighted      = (mui->colors.list_text_highlighted      & 0xFFFFFF00) | alpha_255;
      mui->colors.list_hint_text             = (mui->colors.list_hint_text             & 0xFFFFFF00) | alpha_255;
      mui->colors.list_hint_text_highlighted = (mui->colors.list_hint_text_highlighted & 0xFFFFFF00) | alpha_255;

      /* Background/object colours */
      gfx_display_set_alpha(mui->colors.list_highlighted_background, alpha);
      gfx_display_set_alpha(mui->colors.list_icon,                   alpha);
      gfx_display_set_alpha(mui->colors.list_switch_on,              alpha);
      gfx_display_set_alpha(mui->colors.list_switch_on_background,   alpha);
      gfx_display_set_alpha(mui->colors.list_switch_off,             alpha);
      gfx_display_set_alpha(mui->colors.list_switch_off_background,  alpha);
      gfx_display_set_alpha(mui->colors.scrollbar,                   alpha);
      gfx_display_set_alpha(mui->colors.entry_divider,               alpha);

      /* Landscape border shadow only fades if:
       * - Landscape border is shown
       * - We are currently performing a slide animation */
      if (mui->landscape_optimization.enabled &&
          (mui->transition_x_offset != 0.0f))
      {
         float border_shadow_alpha =
               mui->colors.landscape_border_shadow_opacity * alpha;

         mui->colors.landscape_border_shadow_left[7]   = border_shadow_alpha;
         mui->colors.landscape_border_shadow_left[15]  = border_shadow_alpha;
         mui->colors.landscape_border_shadow_right[3]  = border_shadow_alpha;
         mui->colors.landscape_border_shadow_right[11] = border_shadow_alpha;
      }

      /* Sidebar and status bar only fade if we are
       * currently viewing a playlist 'desktop'-layout */
      if (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP)
      {
         float status_bar_shadow_alpha =
               mui->colors.status_bar_shadow_opacity * alpha;

         gfx_display_set_alpha(mui->colors.side_bar_background,   alpha);
         gfx_display_set_alpha(mui->colors.status_bar_background, alpha);

         mui->colors.status_bar_shadow[11] = status_bar_shadow_alpha;
         mui->colors.status_bar_shadow[15] = status_bar_shadow_alpha;
      }

      /* Selection marker shadow only fades if
       * it is enabled (i.e. content running +
       * semi-transparent background) */
      if (mui->show_selection_marker_shadow)
      {
         float selection_marker_shadow_alpha =
               mui->colors.selection_marker_shadow_opacity * alpha;

         mui->colors.selection_marker_shadow_top[11]   = selection_marker_shadow_alpha;
         mui->colors.selection_marker_shadow_top[15]   = selection_marker_shadow_alpha;
         mui->colors.selection_marker_shadow_bottom[3] = selection_marker_shadow_alpha;
         mui->colors.selection_marker_shadow_bottom[7] = selection_marker_shadow_alpha;
      }
   }
}

/* Resets transparency of all menu list colours if
 * previously altered by a menu transition animation */
static void materialui_colors_reset_transition_alpha(materialui_handle_t *mui)
{
   if (mui->transition_alpha < 1.0f)
   {
      /* Text colours */
      mui->colors.list_text                  = (mui->colors.list_text                  | 0xFF);
      mui->colors.list_text_highlighted      = (mui->colors.list_text_highlighted      | 0xFF);
      mui->colors.list_hint_text             = (mui->colors.list_hint_text             | 0xFF);
      mui->colors.list_hint_text_highlighted = (mui->colors.list_hint_text_highlighted | 0xFF);

      /* Background/object colours */
      gfx_display_set_alpha(mui->colors.list_highlighted_background, 1.0f);
      gfx_display_set_alpha(mui->colors.list_icon,                   1.0f);
      gfx_display_set_alpha(mui->colors.list_switch_on,              1.0f);
      gfx_display_set_alpha(mui->colors.list_switch_on_background,   1.0f);
      gfx_display_set_alpha(mui->colors.list_switch_off,             1.0f);
      gfx_display_set_alpha(mui->colors.list_switch_off_background,  1.0f);
      gfx_display_set_alpha(mui->colors.scrollbar,                   1.0f);
      gfx_display_set_alpha(mui->colors.entry_divider,               1.0f);

      /* Landscape border shadow only fades if:
       * - Landscape border is shown
       * - We are currently performing a slide animation */
      if (mui->landscape_optimization.enabled &&
          (mui->transition_x_offset != 0.0f))
      {
         float border_shadow_alpha =
               mui->colors.landscape_border_shadow_opacity;

         mui->colors.landscape_border_shadow_left[7]   = border_shadow_alpha;
         mui->colors.landscape_border_shadow_left[15]  = border_shadow_alpha;
         mui->colors.landscape_border_shadow_right[3]  = border_shadow_alpha;
         mui->colors.landscape_border_shadow_right[11] = border_shadow_alpha;
      }

      /* Sidebar and status bar only fade if we are
       * currently viewing a playlist 'desktop'-layout */
      if (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP)
      {
         float status_bar_shadow_alpha =
               mui->colors.status_bar_shadow_opacity;

         gfx_display_set_alpha(mui->colors.side_bar_background,   1.0f);
         gfx_display_set_alpha(mui->colors.status_bar_background, 1.0f);

         mui->colors.status_bar_shadow[11] = status_bar_shadow_alpha;
         mui->colors.status_bar_shadow[15] = status_bar_shadow_alpha;
      }

      /* Selection marker shadow only fades if
       * it is enabled (i.e. content running +
       * semi-transparent background) */
      if (mui->show_selection_marker_shadow)
      {
         float selection_marker_shadow_alpha =
               mui->colors.selection_marker_shadow_opacity;

         mui->colors.selection_marker_shadow_top[11]   = selection_marker_shadow_alpha;
         mui->colors.selection_marker_shadow_top[15]   = selection_marker_shadow_alpha;
         mui->colors.selection_marker_shadow_bottom[3] = selection_marker_shadow_alpha;
         mui->colors.selection_marker_shadow_bottom[7] = selection_marker_shadow_alpha;
      }
   }
}

/* Updates scrollbar draw position */
static void materialui_update_scrollbar(
      materialui_handle_t *mui,
      unsigned width, unsigned height,
      unsigned header_height, int x_offset)
{
   /* Do nothing if scrollbar is disabled */
   if (mui->scrollbar.active)
   {
      int view_height = (int)height - (int)header_height -
            (int)mui->nav_bar_layout_height - (int)mui->status_bar.height;
      int y_max       = view_height + (int)header_height -
            (int)(mui->scrollbar.width + mui->scrollbar.height);

      /* Get X position */
      mui->scrollbar.x = x_offset + (int)width - (int)mui->scrollbar.width -
            (int)mui->landscape_optimization.border_width -
            (int)mui->nav_bar_layout_width;

      /* Get Y position */
      mui->scrollbar.y = (int)header_height + (int)(mui->scroll_y * (float)view_height / mui->content_height);

      /* > Apply vertical padding to improve visual appearance */
      mui->scrollbar.y += (int)mui->scrollbar.width;

      /* > Ensure we don't fall off the bottom of the screen... */
      mui->scrollbar.y = (mui->scrollbar.y > y_max) ? y_max : mui->scrollbar.y;
   }
}

/* Main function of the menu driver
 * Draws all menu elements */
static void materialui_frame(void *data, video_frame_info_t *video_info)
{
   int list_x_offset;
   math_matrix_4x4 mymat;
   gfx_display_ctx_rotate_draw_t rotate_draw;
   materialui_handle_t *mui       = (materialui_handle_t*)data;
   settings_t *settings           = config_get_ptr();
   gfx_display_t *p_disp          = disp_get_ptr();
   size_t selection               = menu_navigation_get_selection();
   unsigned header_height         = p_disp->header_height;
   enum gfx_animation_ticker_type
      menu_ticker_type            = (enum gfx_animation_ticker_type)settings->uints.menu_ticker_type;
   bool menu_ticker_smooth        = settings->bools.menu_ticker_smooth;
   bool libretro_running          = video_info->libretro_running;
   float menu_wallpaper_opacity   = video_info->menu_wallpaper_opacity;
   float menu_framebuffer_opacity = video_info->menu_framebuffer_opacity;
   void *userdata                 = video_info->userdata;
   unsigned video_width           = video_info->width;
   unsigned video_height          = video_info->height;
   unsigned 
      materialui_color_theme      = video_info->materialui_color_theme;
   bool video_fullscreen          = video_info->fullscreen;
   bool mouse_grabbed             = video_info->input_driver_grab_mouse_state;
   bool menu_mouse_enable         = video_info->menu_mouse_enable;
   gfx_animation_t *p_anim        = anim_get_ptr();

   if (!mui)
      return;

   /* If menu screensaver is active, draw
    * screensaver and return */
   if (mui->show_screensaver)
   {
      menu_screensaver_frame(mui->screensaver,
            video_info, p_disp);
      return;
   }

   {
      rotate_draw.matrix       = &mymat;
      rotate_draw.rotation     = 0.0f;
      rotate_draw.scale_x      = 1.0f;
      rotate_draw.scale_y      = 1.0f;
      rotate_draw.scale_z      = 1;
      rotate_draw.scale_enable = true;

      gfx_display_rotate_z(p_disp, &rotate_draw, userdata);
   }

   video_driver_set_viewport(video_width, video_height, true, false);

   /* Clear text */
   materialui_font_bind(&mui->font_data.title);
   materialui_font_bind(&mui->font_data.list);
   materialui_font_bind(&mui->font_data.hint);

   /* Update theme colours, if required */
   if (mui->color_theme != materialui_color_theme)
   {
      materialui_prepare_colors(mui,
            (enum materialui_color_theme)
            materialui_color_theme);
      mui->color_theme = (enum materialui_color_theme)
         materialui_color_theme;
   }

   /* Update line ticker(s) */
   mui->use_smooth_ticker          = menu_ticker_smooth;

   if (mui->use_smooth_ticker)
   {
      mui->ticker_smooth.idx       = p_anim->ticker_pixel_idx;
      mui->ticker_smooth.type_enum = menu_ticker_type;
   }
   else
   {
      mui->ticker.idx              = p_anim->ticker_idx;
      mui->ticker.type_enum        = menu_ticker_type;
   }

   /* Determine whether a selection marker 'shadow'
    * should be drawn
    * > Improves selection marker visibility when
    *   running with a transparent background */
   mui->show_selection_marker_shadow = libretro_running &&
         (menu_framebuffer_opacity < 1.0f);

   /* Handle any transparency adjustments required
    * by menu transition animations */
   materialui_colors_set_transition_alpha(mui);

   /* Get x offset for list items, required by
    * menu transition 'slide' animations */
   list_x_offset = (int)(mui->transition_x_offset * (float)((int)video_width - (int)mui->nav_bar_layout_width));

   /* Draw background */
   materialui_render_background(mui, p_disp,
         userdata,
         video_width,
         video_height,
         libretro_running,
         menu_wallpaper_opacity,
         menu_framebuffer_opacity);

   /* Draw landscape border
    * (does nothing in portrait mode, or if landscape
    * optimisations are disabled) */
   materialui_render_landscape_border(mui,
         p_disp,
         userdata,
         video_width, video_height,
         header_height, list_x_offset);

   /* Draw 'short press' touch feedback highlight */
   materialui_render_entry_touch_feedback(
         mui, p_disp, userdata, video_width, video_height,
         header_height, list_x_offset, selection);

   /* Draw 'highlighted entry' selection box */
   materialui_render_selection_highlight(
         mui,
         p_disp,
         userdata, video_width, video_height,
         header_height, list_x_offset, selection,
         mui->colors.list_highlighted_background,
         mui->colors.selection_marker_shadow_top,
         mui->colors.selection_marker_shadow_bottom);

   /* Draw menu list
    * > Must update scrollbar draw position before
    *   list is rendered
    * > We handle the scrollbar in a separate step
    *   like this because we need to track its
    *   position in order to enable fast navigation
    *   via scrollbar 'dragging' */
   materialui_update_scrollbar(mui, video_width, video_height,
         header_height, list_x_offset);
   materialui_render_menu_list(mui, p_disp,
         userdata,
         video_width, video_height, list_x_offset);

   /* Flush first layer of text
    * > Menu list only uses list and hint fonts */
   materialui_font_flush(video_width, video_height, &mui->font_data.list);
   materialui_font_flush(video_width, video_height, &mui->font_data.hint);

   /* Draw fullscreen thumbnails, if currently active
    * > Must be done *after* we flush the first layer
    *   of text */
   materialui_render_fullscreen_thumbnails(mui, p_disp, userdata,
         video_width, video_height, header_height, selection);

   /* Draw title + system bar */
   materialui_render_header(mui, settings, p_disp, userdata,
         video_width, video_height, &mymat);

   /* Draw navigation bar */
   materialui_render_nav_bar(mui, p_disp, userdata, 
         video_width, video_height, &mymat);

   /* Flush second layer of text
    * > Title + system bar only use title and hint fonts */
   materialui_font_flush(video_width,
         video_height, &mui->font_data.title);
   materialui_font_flush(video_width,
         video_height, &mui->font_data.hint);

   /* Handle onscreen keyboard */
   if (menu_input_dialog_get_display_kb())
   {
      char msg[255];
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      msg[0] = '\0';

      /* Darken screen */
      gfx_display_set_alpha(
            mui->colors.screen_fade, mui->colors.screen_fade_opacity);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0, 0,
            video_width, video_height,
            video_width, video_height,
            mui->colors.screen_fade,
            NULL);

      /* Draw message box */
      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      materialui_render_messagebox(mui,
            p_disp,
            userdata, video_width, video_height,
            video_height / 4, msg);

      /* Draw onscreen keyboard */
      {
         input_driver_state_t *input_st = input_state_get_ptr();
         gfx_display_draw_keyboard(
               p_disp,
               userdata,
               video_width,
               video_height,
               mui->textures.list[MUI_TEXTURE_KEY_HOVER],
               mui->font_data.list.font,
               input_st->osk_grid,
               input_st->osk_ptr,
               0xFFFFFFFF);
      }

      /* Flush message box & osk text
       * > Message box & osk only use list font */
      materialui_font_flush(video_width, video_height, &mui->font_data.list);
   }

   /* Draw message box */
   if (!string_is_empty(mui->msgbox))
   {
      /* Darken screen */
      gfx_display_set_alpha(
            mui->colors.screen_fade, mui->colors.screen_fade_opacity);
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0, 0,
            video_width, video_height,
            video_width, video_height,
            mui->colors.screen_fade,
            NULL);

      /* Draw message box */
      materialui_render_messagebox(mui, 
            p_disp,
            userdata, video_width, video_height,
            video_height / 2, mui->msgbox);
      mui->msgbox[0] = '\0';

      /* Flush message box text
       * > Message box only uses list font */
      materialui_font_flush(video_width, video_height, &mui->font_data.list);
   }

   /* Draw mouse cursor */
   if (mui->show_mouse && (mui->pointer.type != MENU_POINTER_DISABLED))
   {
      float color_white[16] = {
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f
      };
      bool cursor_visible   = (video_fullscreen || mouse_grabbed) &&
            menu_mouse_enable;

      if (cursor_visible)
         gfx_display_draw_cursor(
               p_disp,
               userdata,
               video_width,
               video_height,
               cursor_visible,
               color_white,
               mui->cursor_size,
               mui->textures.list[MUI_TEXTURE_POINTER],
               mui->pointer.x,
               mui->pointer.y,
               video_width,
               video_height);
   }

   /* Undo any transparency adjustments caused
    * by menu transition animations */
   materialui_colors_reset_transition_alpha(mui);

   /* Unbind fonts */
   materialui_font_unbind(&mui->font_data.title);
   materialui_font_unbind(&mui->font_data.list);
   materialui_font_unbind(&mui->font_data.hint);

   video_driver_set_viewport(video_width, video_height, false, true);
}

/* Determines current list view type, based on
 * whether current menu is a playlist, and whether
 * user has enabled playlist thumbnails */
static void materialui_set_list_view_type(
      materialui_handle_t *mui, 
      unsigned thumbnail_view_portrait,
      unsigned thumbnail_view_landscape)
{
   if (!mui->is_playlist)
   {
      /* This is not a playlist - set default list
       * view and register that primary thumbnail
       * is disabled */
      mui->list_view_type              = MUI_LIST_VIEW_DEFAULT;
      mui->primary_thumbnail_available = false;
   }
   else
   {
      /* This is a playlist - set non-thumbnail view
       * by default (saves checks later) */
      mui->list_view_type = MUI_LIST_VIEW_PLAYLIST;

      /* Check whether primary thumbnail is enabled */
      mui->primary_thumbnail_available =
            gfx_thumbnail_is_enabled(mui->thumbnail_path_data, GFX_THUMBNAIL_RIGHT);

      if (mui->primary_thumbnail_available)
      {
         /* Get thumbnail view mode based on current
          * display orientation */
         if (mui->is_portrait)
         {
            switch (thumbnail_view_portrait)
            {
               case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL;
                  break;
               case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM;
                  break;
               case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON;
                  break;
               case MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED:
               default:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST;
                  break;
            }
         }
         else
         {
            switch (thumbnail_view_landscape)
            {
               case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL;
                  break;
               case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM;
                  break;
               case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE;
                  break;
               case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP;
                  break;
               case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED:
               default:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST;
                  break;
            }
         }
      }
   }

   /* List view type has changed -> assign
    * relevant function pointers */
   switch (mui->list_view_type)
   {
      case MUI_LIST_VIEW_PLAYLIST:
         materialui_compute_entries_box       = materialui_compute_entries_box_playlist_list;
         materialui_render_process_entry      = materialui_render_process_entry_default;
         materialui_render_menu_entry         = materialui_render_menu_entry_playlist_list;
         materialui_render_selected_entry_aux = NULL;
         break;
      case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL:
      case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM:
      case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE:
         materialui_compute_entries_box       = materialui_compute_entries_box_playlist_list;
         materialui_render_process_entry      = materialui_render_process_entry_playlist_thumb_list;
         materialui_render_menu_entry         = materialui_render_menu_entry_playlist_list;
         materialui_render_selected_entry_aux = NULL;
         break;
      case MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON:
         materialui_compute_entries_box       = materialui_compute_entries_box_playlist_dual_icon;
         materialui_render_process_entry      = materialui_render_process_entry_playlist_dual_icon;
         materialui_render_menu_entry         = materialui_render_menu_entry_playlist_dual_icon;
         materialui_render_selected_entry_aux = NULL;
         break;
      case MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP:
         materialui_compute_entries_box       = materialui_compute_entries_box_playlist_desktop;
         materialui_render_process_entry      = materialui_render_process_entry_playlist_desktop;
         materialui_render_menu_entry         = materialui_render_menu_entry_playlist_desktop;
         materialui_render_selected_entry_aux = materialui_render_selected_entry_aux_playlist_desktop;
         break;
      case MUI_LIST_VIEW_DEFAULT:
      default:
         materialui_compute_entries_box       = materialui_compute_entries_box_default;
         materialui_render_process_entry      = materialui_render_process_entry_default;
         materialui_render_menu_entry         = materialui_render_menu_entry_default;
         materialui_render_selected_entry_aux = NULL;
         break;
   }
}

/* Determines whether landscape optimisations should
 * be applied, and calculates appropriate landscape
 * entry margin size */
static void materialui_set_landscape_optimisations_enable(
      materialui_handle_t *mui)
{
   /* In landscape orientations, menu lists are too wide
    * (to the extent that they are rather uncomfortable
    * to look at...)
    * > Depending upon user configuration, we therefore
    *   use additional padding at the left/right sides of
    *   the screen */

   /* Disable optimisations by default */
   mui->landscape_optimization.enabled      = false;
   mui->landscape_optimization.border_width = 0;
   mui->landscape_optimization.entry_margin = 0;

   /* Early out if current orientation is portrait */
   if (mui->is_portrait)
      return;

   /* Check whether optimisations are enabled, globally
    * or for current list view type */
   switch (mui->last_landscape_layout_optimization)
   {
      case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS:
         mui->landscape_optimization.enabled = true;
         break;
      case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS:

         switch (mui->list_view_type)
         {
            case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL:
            case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM:
            case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE:
            case MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON:
            case MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP:
               mui->landscape_optimization.enabled = false;
               break;
            case MUI_LIST_VIEW_PLAYLIST:
            case MUI_LIST_VIEW_DEFAULT:
            default:
               mui->landscape_optimization.enabled = true;
               break;
         }

         break;

      case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED:
      default:
         mui->landscape_optimization.enabled = false;
         break;
   }

   /* Calculate landscape border size, if required */
   if (mui->landscape_optimization.enabled)
   {
      /* After testing various approaches, it seems that
       * simply enforcing a 4:3 aspect ratio produces the
       * best results */
      const float base_aspect = 4.0f / 3.0f;
      float landscape_margin  =
            ((float)(mui->last_width - mui->nav_bar_layout_width) -
                  (base_aspect * (float)mui->last_height)) / 2.0f;

      if (landscape_margin > 1.0f)
      {
         float entry_margin = 0.0f;
         float border_width = 0.0f;

         /* When landscape optimisations are active,
          * we increase the effective width of the list
          * view by up to 'mui->margin', and any remaining
          * 'landscape_margin' space is filled with a
          * (shadow gradient) border */
         entry_margin = (landscape_margin >= (float)mui->margin) ?
                     (float)mui->margin : landscape_margin;
         border_width = landscape_margin - entry_margin;

         /* Note: In all cases, we want to round down
          * when converting these to integers */
         mui->landscape_optimization.entry_margin = (unsigned)entry_margin;
         mui->landscape_optimization.border_width = (unsigned)border_width;
      }
      /* If margin is less than 1px, disable optimisations */
      else
         mui->landscape_optimization.enabled = false;
   }
}

/* Initialises status bar, determining current
 * enable state based on view mode and user
 * configuration */
static void materialui_status_bar_init(
      materialui_handle_t *mui, settings_t *settings)
{
   bool playlist_show_sublabels = settings->bools.playlist_show_sublabels;
   uintptr_t          alpha_tag = (uintptr_t)&mui->status_bar.alpha;

   /* Kill any existing fade in animation */
   if (mui->status_bar.enabled ||
       (mui->status_bar.alpha > 0.0f))
      gfx_animation_kill_by_tag(&alpha_tag);

   /* Reset base parameters */
   mui->status_bar.cached                      = false;
   mui->status_bar.last_selected               = 0;
   mui->status_bar.delay_timer                 = 0.0f;
   mui->status_bar.alpha                       = 0.0f;
   mui->status_bar.height                      = 0;
   mui->status_bar.str[0]                      = '\0';
   mui->status_bar.runtime_fallback_str[0]     = '\0';
   mui->status_bar.last_played_fallback_str[0] = '\0';

   /* Determine enable state */
   mui->status_bar.enabled =
         (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP) &&
         playlist_show_sublabels;

   if (mui->status_bar.enabled)
   {
      /* Determine status bar height */
      mui->status_bar.height = (unsigned)(((float)mui->font_data.hint.line_height * 1.6f) + 0.5f);

      /* Cache fallback runtime/last played strings
       * (Why do we do this here instead of once in
       *  materialui_init()? Because re-caching the
       *  values each time allows us to handle changes
       *  in user interface language settings) */
      snprintf(mui->status_bar.runtime_fallback_str,
            sizeof(mui->status_bar.runtime_fallback_str), "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED));

      snprintf(mui->status_bar.last_played_fallback_str,
            sizeof(mui->status_bar.last_played_fallback_str), "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED));
   }
}

/* Determines appropriate thumbnail dimensions based
 * on current list view type */
static void materialui_set_thumbnail_dimensions(materialui_handle_t *mui)
{
   switch (mui->list_view_type)
   {
      case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL:

         /* Maximum height is just standard icon size */
         mui->thumbnail_height_max = mui->icon_size;

         /* Set thumbnail width based on max height */
         mui->thumbnail_width_max =
               (unsigned)(((float)mui->thumbnail_height_max *
                     MUI_THUMBNAIL_DEFAULT_ASPECT_RATIO) + 0.5f);

         break;

      case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM:

         /* Maximum height corresponds to text height when
          * showing full playlist sublabel metadata
          * (core association + runtime info)
          * > One line of list text + three lines of
          *   hint text + padding */
         mui->thumbnail_height_max =
               mui->font_data.list.line_height +
               (3 * mui->font_data.hint.line_height) +
               (mui->dip_base_unit_size / 10);

         /* Set thumbnail width based on max height
          * Note: We're duplicating this calculation each time
          * for consistency - some view modes will require
          * something different, and we want each case to
          * be self-contained */
         mui->thumbnail_width_max =
               (unsigned)(((float)mui->thumbnail_height_max *
                     MUI_THUMBNAIL_DEFAULT_ASPECT_RATIO) + 0.5f);

         break;

      case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE:

         /* Maximum height corresponds to twice the
          * text height when showing full playlist sublabel
          * metadata (core association + runtime info)
          * > Two lines of list text + three lines of
          *   hint text (no padding) */
         mui->thumbnail_height_max =
               (mui->font_data.list.line_height +
                (3 * mui->font_data.hint.line_height)) * 2;

         /* Set thumbnail width based on max height */
         mui->thumbnail_width_max =
               (unsigned)(((float)mui->thumbnail_height_max *
                     MUI_THUMBNAIL_DEFAULT_ASPECT_RATIO) + 0.5f);

         break;

      case MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON:
         {
            /* This view shows two thumbnail icons
             * side-by-side across the full width of
             * the list area */

            /* > Get total usable width
             *   (list view width minus padding between
             *   and either side of thumbnails) */
            int usable_width =
                  (int)mui->last_width - (int)(mui->margin * 3) -
                  (int)(mui->landscape_optimization.border_width * 2) -
                  (int)mui->nav_bar_layout_width;

            /* Sanity check */
            if (usable_width < 2)
            {
               mui->thumbnail_width_max  = 0;
               mui->thumbnail_height_max = 0;
            }
            else
            {
               /* Get maximum thumbnail width */
               mui->thumbnail_width_max = (usable_width >> 1);

               /* Set thumbnail height based on max width */
               mui->thumbnail_height_max =
                     (unsigned)(((float)mui->thumbnail_width_max *
                           (1.0f / MUI_THUMBNAIL_DEFAULT_ASPECT_RATIO)) + 0.5f);
            }
         }
         break;

      case MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP:
         {
            /* This view shows two thumbnail icons
             * for the selected entry, one below the
             * other across the full height of the
             * list area */

            /* > Get total usable height
             *   (list view height minus vertical padding
             *    between thumbnails minus status bar height) */
            gfx_display_t *p_disp  = disp_get_ptr();
            unsigned header_height = p_disp->header_height;
            int usable_height      = (int)mui->last_height - (int)header_height -
                  (int)(mui->margin * 3) - (int)mui->nav_bar_layout_height -
                  (int)mui->status_bar.height;

            /* Sanity check */
            if (usable_height < 2)
            {
               mui->thumbnail_width_max  = 0;
               mui->thumbnail_height_max = 0;
            }
            else
            {
               /* Get maximum thumbnail height */
               mui->thumbnail_height_max = (usable_height >> 1);

               /* Set thumbnail width based on max height */
               mui->thumbnail_width_max =
                     (unsigned)(((float)mui->thumbnail_height_max *
                           MUI_THUMBNAIL_DEFAULT_ASPECT_RATIO) + 0.5f);
            }
         }
         break;

      case MUI_LIST_VIEW_PLAYLIST:
      case MUI_LIST_VIEW_DEFAULT:
      default:
         /* Not required, but might as well zero
          * out thumbnail dimensions... */
         mui->thumbnail_height_max = 0;
         mui->thumbnail_width_max  = 0;
         break;
   }
}

/* Checks global 'Secondary Thumbnail' option - if
 * currently set to 'OFF', changes value to
 * MUI_DEFAULT_SECONDARY_THUMBNAIL_TYPE
 * - Does not affect per-playlist thumbnail settings,
 *   i.e. a user with custom config may selectively
 *   force-disable secondary thumbnails regardless of
 *   list view mode
 * - Follows the existing precedent of automatically
 *   changing global settings->uints.menu_left_thumbnails
 *   value (i.e. XMB/Ozone already allow this parameter
 *   to be cycled via the 'scan' function)
 * - Returns false if secondary thumbnails cannot be
 *   enabled (due to per-playlist override) */
static bool materialui_force_enable_secondary_thumbnail(
      materialui_handle_t *mui, settings_t *settings)
{
   /* If secondary thumbnail is already enabled,
    * do nothing */
   if (gfx_thumbnail_is_enabled(
         mui->thumbnail_path_data, GFX_THUMBNAIL_LEFT))
      return true;

   /* Secondary thumbnail is disabled
    * > Check if this is a global setting... */
   if (settings->uints.menu_left_thumbnails == 0)
   {
      /* > If possible, set secondary thumbnail
       *   type to MUI_DEFAULT_SECONDARY_THUMBNAIL_TYPE
       * > If primary thumbnail is already set to
       *   MUI_DEFAULT_SECONDARY_THUMBNAIL_TYPE, use
       *   MUI_DEFAULT_SECONDARY_THUMBNAIL_FALLBACK_TYPE
       *   instead */
      if (settings->uints.gfx_thumbnails ==
            MUI_DEFAULT_SECONDARY_THUMBNAIL_TYPE)
      {
         configuration_set_uint(settings,
               settings->uints.menu_left_thumbnails,
               MUI_DEFAULT_SECONDARY_THUMBNAIL_FALLBACK_TYPE);
      }
      else
      {
         configuration_set_uint(settings,
               settings->uints.menu_left_thumbnails,
               MUI_DEFAULT_SECONDARY_THUMBNAIL_TYPE);
      }
   }

   /* Final check - this will return true unless a
    * per-playlist override is in place */
   return gfx_thumbnail_is_enabled(
         mui->thumbnail_path_data, GFX_THUMBNAIL_LEFT);
}

/* Determines whether dual thumbnails should be enabled
 * based on current list view mode, thumbnail dimensions
 * and screen size */
static void materialui_set_secondary_thumbnail_enable(
      materialui_handle_t *mui, settings_t *settings)
{
   switch (mui->list_view_type)
   {
      case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL:
      case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM:
      case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE:
         /* List view has optional secondary thumbnails */
         {
            int usable_width     = 0;
            int thumbnail_margin = 0;
            bool menu_materialui_dual_thumbnail_list_view_enable =
                  settings->bools.menu_materialui_dual_thumbnail_list_view_enable;

            /* Disable by default */
            mui->secondary_thumbnail_enabled = false;

            /* Check whether user has manually disabled
             * secondary thumbnails */
            if (!menu_materialui_dual_thumbnail_list_view_enable)
               return;

            /* Attempt to force enable secondary thumbnails if
             * global 'Secondary Thumbnail' type is set to OFF */
            if (!materialui_force_enable_secondary_thumbnail(mui, settings))
               return;

            /* Secondary thumbnails are supported/enabled
             * Check whether screen has sufficient
             * width to display them */

            /* > Get total usable width */
            usable_width = (int)mui->last_width - (int)(mui->margin * 2) -
                  (int)(mui->landscape_optimization.border_width * 2) -
                  (int)mui->nav_bar_layout_width;

            /* > Account for additional padding (margins) when
             *   using portrait orientations */
            if (mui->is_portrait)
               thumbnail_margin = (int)mui->scrollbar.width;
            /* > Account for additional padding (margins) when
             *   using landscape orientations */
            else
               thumbnail_margin = (int)mui->margin;

            /* > Get remaining (text) width after drawing
             *   primary + secondary thumbnails */
            usable_width -= 2 * ((int)mui->thumbnail_width_max + thumbnail_margin);

            /* > A secondary thumbnail may only be drawn
             *   if the remaining (text) width is greater
             *   than twice the thumbnail width */
            mui->secondary_thumbnail_enabled =
                  usable_width > (int)(mui->thumbnail_width_max * 2);
         }
         break;
      case MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON:
      case MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP:
         /* List view requires secondary thumbnails
          * > Attempt to force enable, but set
          *   mui->secondary_thumbnail_enabled to 'true'
          *   regardless of the result since we still
          *   want 'missing thumbnail' images if
          *   thumbnails are actively disabled via
          *   a per-playlist override */
         materialui_force_enable_secondary_thumbnail(mui, settings);
         mui->secondary_thumbnail_enabled = true;
         break;
      case MUI_LIST_VIEW_PLAYLIST:
      case MUI_LIST_VIEW_DEFAULT:
      default:
         /* List view has no thumbnails */
         mui->secondary_thumbnail_enabled = false;
         break;
   }
}

/* Determines current list view mode based upon
 * display orientation and user config, then
 * applies view-dependent landscape display optimisations
 * and calculates appropriate thumbnail dimensions/settings.
 * Must be called when updating menu layout and
 * populating menu lists. */
static void materialui_update_list_view(materialui_handle_t *mui, settings_t *settings)
{
   materialui_set_list_view_type(mui, 
         settings->uints.menu_materialui_thumbnail_view_portrait,
         settings->uints.menu_materialui_thumbnail_view_landscape);
   materialui_set_landscape_optimisations_enable(mui);
   materialui_status_bar_init(mui, settings);
   materialui_set_thumbnail_dimensions(mui);
   materialui_set_secondary_thumbnail_enable(mui, settings);

   /* Miscellaneous post-list-switch configuration:
    * > Set appropriate thumbnail stream delay */
   mui->thumbnail_stream_delay =
         (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP) ?
               MUI_THUMBNAIL_STREAM_DELAY_PLAYLIST_DESKTOP :
               MUI_THUMBNAIL_STREAM_DELAY_DEFAULT;
   gfx_thumbnail_set_stream_delay(mui->thumbnail_stream_delay);
   /* > Reset 'desktop'-layout last selected
    *   entry index (we only need to do this if
    *   list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP,
    *   but it's faster to always set the variable
    *   than it is to perform a check...) */
   mui->desktop_thumbnail_last_selection = 0;

   /* List view configuration complete - signal
    * that entry dimensions must be recalculated */
   mui->need_compute = true;
}

static void materialui_init_font(
   gfx_display_t *p_disp,
   materialui_font_data_t *font_data,
   int font_size,
   bool video_is_threaded,
   const char *str_latin
   )
{
   const char *wideglyph_str = msg_hash_get_wideglyph_str();

   /* We assume the average glyph aspect ratio is close to 3:4 */
   font_data->glyph_width = (int)((font_size * (3.0f / 4.0f)) + 0.5f);

   if (font_data->font)
   {
      gfx_display_font_free(font_data->font);
      font_data->font = NULL;
   }

   font_data->font = gfx_display_font(
         p_disp,
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         font_size, video_is_threaded);

  if (font_data->font)
   {
      /* Calculate a more realistic ticker_limit */
      int char_width =
         font_driver_get_message_width(font_data->font, str_latin, 1, 1);

      if (char_width > 0)
         font_data->glyph_width = (unsigned)char_width;

      font_data->wideglyph_width = 100;

      if (wideglyph_str)
      {
         int wideglyph_width =
            font_driver_get_message_width(font_data->font, wideglyph_str, (unsigned)strlen(wideglyph_str), 1);

         if (wideglyph_width > 0 && char_width > 0) 
            font_data->wideglyph_width = wideglyph_width * 100 / char_width;
      }

      /* Get line metrics */
      font_data->line_height        = font_driver_get_line_height(font_data->font, 1.0f);
      font_data->line_ascender      = font_driver_get_line_ascender(font_data->font, 1.0f);
      font_data->line_centre_offset = font_driver_get_line_centre_offset(font_data->font, 1.0f);
   }
}

/* Compute the positions of the widgets */
static void materialui_layout(
      materialui_handle_t *mui,
      gfx_display_t *p_disp,
      settings_t *settings,
      bool video_is_threaded)
{
   int title_font_size;
   int list_font_size;
   int hint_font_size;
   unsigned new_header_height;

   mui->is_portrait          = mui->last_height >= mui->last_width;

   mui->cursor_size          = mui->dip_base_unit_size / 3;

   mui->sys_bar_height       = mui->dip_base_unit_size / 7;
   mui->title_bar_height     = mui->dip_base_unit_size / 3;
   new_header_height         = mui->sys_bar_height + mui->title_bar_height;

   title_font_size           = mui->dip_base_unit_size / 7;
   list_font_size            = mui->dip_base_unit_size / 9;
   hint_font_size            = mui->dip_base_unit_size / 11;

   mui->header_shadow_height = mui->dip_base_unit_size / 36;
   mui->selection_marker_shadow_height = mui->dip_base_unit_size / 30;

   mui->margin               = mui->dip_base_unit_size / 9;
   mui->icon_size            = mui->dip_base_unit_size / 3;

   mui->sys_bar_margin       = mui->dip_base_unit_size / 12;
   mui->sys_bar_icon_size    = mui->dip_base_unit_size / 7;

   mui->entry_divider_width  = (mui->last_scale_factor > 1.0f) ?
         (unsigned)(mui->last_scale_factor + 0.5f) : 1;

   /* Additional vertical spacing between label and
    * sublabel text */
   mui->sublabel_gap         = mui->dip_base_unit_size / 42;
   /* Additional horizontal padding inserted at the
    * end of sublabel text to prevent line overflow */
   mui->sublabel_padding     = mui->dip_base_unit_size / 20;

   /* Note: We used to set scrollbar width here, but
    * since we now have several scrollbar parameters
    * that cannot be determined until materialui_compute_entries_box()
    * has been called, we delegate this to materialui_scrollbar_init() */

   /* Get navigation bar layout
    * > Normally drawn at the bottom of the screen,
    *   but in landscape orientations should be placed
    *   on the right hand side
    * > When navigation bar is hidden, just set layout
    *   width and height to zero */
   mui->nav_bar.width                  = mui->dip_base_unit_size / 3;
   mui->nav_bar.divider_width          = mui->entry_divider_width;
   mui->nav_bar.selection_marker_width = mui->nav_bar.width / 16;

   if (!mui->last_show_nav_bar)
   {
      mui->nav_bar.location            = MUI_NAV_BAR_LOCATION_HIDDEN;
      mui->nav_bar_layout_width        = 0;
      mui->nav_bar_layout_height       = 0;
   }
   else if (!mui->is_portrait && mui->last_auto_rotate_nav_bar)
   {
      mui->nav_bar.location            = MUI_NAV_BAR_LOCATION_RIGHT;
      mui->nav_bar_layout_width        = mui->nav_bar.width;
      mui->nav_bar_layout_height       = 0;
   }
   else
   {
      mui->nav_bar.location            = MUI_NAV_BAR_LOCATION_BOTTOM;
      mui->nav_bar_layout_width        = 0;
      mui->nav_bar_layout_height       = mui->nav_bar.width;
   }

   p_disp->header_height = new_header_height;

   materialui_init_font(p_disp, &mui->font_data.title, title_font_size, video_is_threaded, "a");
   materialui_init_font(p_disp, &mui->font_data.list, list_font_size, video_is_threaded, "a");
   materialui_init_font(p_disp, &mui->font_data.hint, hint_font_size, video_is_threaded, "t");

   /* When updating the layout, the system bar
    * cache must be invalidated (since all text
    * size will change) */
   mui->sys_bar_cache.battery_percent_str[0] = '\0';
   mui->sys_bar_cache.battery_percent_width  = 0;
   mui->sys_bar_cache.timedate_str[0]        = '\0';
   mui->sys_bar_cache.timedate_width         = 0;

   materialui_update_list_view(mui, settings);

   mui->need_compute = true;
}

static void materialui_init_nav_bar(materialui_handle_t *mui)
{
   /* Assign action tab textures and types, and ensure sane
    * menu tab starting values */
   unsigned i;

   /* Back tab */
   mui->nav_bar.back_tab.type          = MUI_NAV_BAR_ACTION_TAB_BACK;
   mui->nav_bar.back_tab.texture_index = MUI_TEXTURE_TAB_BACK;
   mui->nav_bar.back_tab.enabled       = false;

   /* Resume tab */
   mui->nav_bar.resume_tab.type          = MUI_NAV_BAR_ACTION_TAB_RESUME;
   mui->nav_bar.resume_tab.texture_index = MUI_TEXTURE_TAB_RESUME;
   mui->nav_bar.resume_tab.enabled       = false;

   /* Menu tabs */
   for (i = 0; i < MUI_NAV_BAR_NUM_MENU_TABS_MAX; i++)
   {
      mui->nav_bar.menu_tabs[i].type          = MUI_NAV_BAR_MENU_TAB_NONE;
      mui->nav_bar.menu_tabs[i].texture_index = 0;
      mui->nav_bar.menu_tabs[i].active        = false;
   }

   /* 'Metadata' */
   mui->nav_bar.num_menu_tabs              = 0;
   mui->nav_bar.active_menu_tab_index      = 0;
   mui->nav_bar.last_active_menu_tab_index = 0;
   mui->nav_bar.menu_navigation_wrapped    = false;
   mui->nav_bar.location                   = MUI_NAV_BAR_LOCATION_BOTTOM;
}

static void materialui_menu_animation_update_time(
      float *ticker_pixel_increment,
      unsigned video_width, unsigned video_height)
{
   gfx_display_t *p_disp      = disp_get_ptr();
   settings_t *settings       = config_get_ptr();
   /* MaterialUI uses DPI scaling
    * > Smooth ticker scaling multiplier is
    *   gfx_display_get_dpi_scale() multiplied by
    *   a small correction factor to achieve a
    *   default scroll speed equal to that of the
    *   non-smooth ticker */
   *(ticker_pixel_increment) *= gfx_display_get_dpi_scale(
         p_disp, settings,
         video_width, video_height, false, false) * 0.8f;
}

static void *materialui_init(void **userdata, bool video_is_threaded)
{
   unsigned width, height;
   settings_t *settings                   = config_get_ptr();
   gfx_animation_t     *p_anim            = anim_get_ptr();
   materialui_handle_t *mui               = NULL;
   static const char* const ticker_spacer = MUI_TICKER_SPACER;
   gfx_display_t *p_disp                  = disp_get_ptr();
   menu_handle_t *menu                    = (menu_handle_t*)
      calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   if (!settings)
   {
      free(menu);
      return NULL;
   }

   mui                                    = (materialui_handle_t*)
      calloc(1, sizeof(materialui_handle_t));

   if (!mui)
      goto error;

   *userdata = mui;

   /* Initialise thumbnail path data */
   mui->thumbnail_path_data               = gfx_thumbnail_path_init();
   if (!mui->thumbnail_path_data)
      goto error;

   /* Get DPI/screen-size-aware base unit size for
    * UI elements */
   video_driver_get_size(&width, &height);

   mui->last_width                        = width;
   mui->last_height                       = height;
   mui->last_scale_factor                 = gfx_display_get_dpi_scale(
         p_disp, settings, width, height,
         false, false);
   mui->dip_base_unit_size                = mui->last_scale_factor 
      * MUI_DIP_BASE_UNIT_SIZE;

   mui->last_show_nav_bar                 = settings->bools.menu_materialui_show_nav_bar;
   mui->last_auto_rotate_nav_bar          = settings->bools.menu_materialui_auto_rotate_nav_bar;

   mui->show_mouse                        = false;
   mui->show_screensaver                  = false;

   mui->need_compute                      = false;
   mui->is_playlist_tab                   = false;
   mui->is_playlist                       = false;
   mui->is_file_list                      = false;
   mui->is_dropdown_list                  = false;
   mui->is_core_updater_list              = false;
   mui->menu_stack_flushed                = false;

   mui->first_onscreen_entry              = 0;
   mui->last_onscreen_entry               = 0;

   mui->menu_title[0]                     = '\0';

   /* Set initial theme colours */
   mui->color_theme                       = (enum materialui_color_theme)
      settings->uints.menu_materialui_color_theme;
   materialui_prepare_colors(mui, (enum materialui_color_theme)mui->color_theme);

   /* Initialise screensaver */
   mui->screensaver                       = menu_screensaver_init();
   if (!mui->screensaver)
      goto error;

   /* Initial ticker configuration */
   mui->use_smooth_ticker                 = settings->bools.menu_ticker_smooth;
   mui->ticker_smooth.font_scale          = 1.0f;
   mui->ticker_smooth.spacer              = ticker_spacer;
   mui->ticker_smooth.x_offset            = &mui->ticker_x_offset;
   mui->ticker_smooth.dst_str_width       = &mui->ticker_str_width;
   mui->ticker.spacer                     = ticker_spacer;

   /* Ensure menu animation parameters are properly
    * reset */
   mui->touch_feedback_selection          = 0;
   mui->touch_feedback_alpha              = 0.0f;
   mui->touch_feedback_update_selection   = false;
   mui->transition_alpha                  = 1.0f;
   mui->transition_x_offset               = 0.0f;
   mui->last_stack_size                   = 1;

   mui->scroll_animation_active           = false;
   mui->scroll_animation_selection        = 0;

   /* Ensure message box string is empty */
   mui->msgbox[0]                         = '\0';

   /* Initialise navigation bar */
   materialui_init_nav_bar(mui);

   /* Set initial thumbnail stream delay */
   mui->thumbnail_stream_delay = MUI_THUMBNAIL_STREAM_DELAY_DEFAULT;
   gfx_thumbnail_set_stream_delay(mui->thumbnail_stream_delay);

   /* Set thumbnail fade duration to default */
   gfx_thumbnail_set_fade_duration(-1.0f);

   /* Enable fade in animation for missing thumbnails */
   gfx_thumbnail_set_fade_missing(true);

   /* Ensure that fullscreen thumbnails are inactive */
   mui->show_fullscreen_thumbnails             = false;
   mui->fullscreen_thumbnail_selection         = 0;
   mui->fullscreen_thumbnail_alpha             = 0.0f;
   mui->fullscreen_thumbnail_label[0]          = '\0';

   /* Ensure status bar has sane initial values */
   mui->status_bar.enabled                     = false;
   mui->status_bar.height                      = 0;
   mui->status_bar.str[0]                      = '\0';
   mui->status_bar.runtime_fallback_str[0]     = '\0';
   mui->status_bar.last_played_fallback_str[0] = '\0';

   /* Initialise playlist icon list */
   mui->textures.playlist.size  = 0;
   mui->textures.playlist.icons = NULL;
   materialui_refresh_playlist_icon_list(mui, settings);

   p_anim->updatetime_cb = materialui_menu_animation_update_time;

   /* set word_wrap function pointer */
   mui->word_wrap = msg_hash_get_wideglyph_str() ? word_wrap_wideglyph : word_wrap;

   return menu;
error:
   if (menu)
      free(menu);
   p_anim->updatetime_cb = NULL;
   return NULL;
}

static void materialui_free(void *data)
{
   materialui_handle_t *mui    = (materialui_handle_t*)data;
   gfx_animation_t     *p_anim = anim_get_ptr();

   if (!mui)
      return;

   video_coord_array_free(&mui->font_data.title.raster_block.carr);
   video_coord_array_free(&mui->font_data.list.raster_block.carr);
   video_coord_array_free(&mui->font_data.hint.raster_block.carr);

   gfx_display_deinit_white_texture();

   font_driver_bind_block(NULL, NULL);

   if (mui->thumbnail_path_data)
      free(mui->thumbnail_path_data);

   materialui_free_playlist_icon_list(mui);

   p_anim->updatetime_cb = NULL;

   menu_screensaver_free(mui->screensaver);
}

static void materialui_context_bg_destroy(materialui_handle_t *mui)
{
   if (!mui)
      return;

   video_driver_texture_unload(&mui->textures.bg);
   gfx_display_deinit_white_texture();
}

static void materialui_reset_thumbnails(void)
{
   file_list_t *list = menu_entries_get_selection_buf_ptr(0);
   unsigned i;

   if (!list)
      return;

   /* Free node thumbnails */
   for (i = 0; i < list->size; i++)
   {
      materialui_node_t *node = (materialui_node_t*)list->list[i].userdata;

      if (!node)
         continue;

      gfx_thumbnail_reset(&node->thumbnails.primary);
      gfx_thumbnail_reset(&node->thumbnails.secondary);
   }
}

static void materialui_context_destroy(void *data)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;
   size_t i;

   if (!mui)
      return;

   /* Free standard menu textures */
   for (i = 0; i < MUI_TEXTURE_LAST; i++)
      video_driver_texture_unload(&mui->textures.list[i]);

   /* Free playlist icons */
   materialui_context_destroy_playlist_icons(mui);

   /* Free fonts */
   if (mui->font_data.title.font)
      gfx_display_font_free(mui->font_data.title.font);
   mui->font_data.title.font = NULL;

   if (mui->font_data.list.font)
      gfx_display_font_free(mui->font_data.list.font);
   mui->font_data.list.font = NULL;

   if (mui->font_data.hint.font)
      gfx_display_font_free(mui->font_data.hint.font);
   mui->font_data.hint.font = NULL;

   /* Free node thumbnails */
   materialui_reset_thumbnails();

   /* Free background/wallpaper textures */
   materialui_context_bg_destroy(mui);

   /* Destroy screensaver context */
   menu_screensaver_context_destroy(mui->screensaver);
}

/* Note: This is only used for loading wallpaper
 * images. Thumbnails are 'streamed' and must be
 * handled differently */
static bool materialui_load_image(void *userdata, void *data, enum menu_image_type type)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;

   if (type == MENU_IMAGE_WALLPAPER)
   {
      materialui_context_bg_destroy(mui);
      video_driver_texture_load(data,
            TEXTURE_FILTER_MIPMAP_LINEAR, &mui->textures.bg);
      gfx_display_deinit_white_texture();
      gfx_display_init_white_texture();
   }

   return true;
}

static void materialui_scroll_animation_end(void *userdata)
{
   materialui_handle_t *mui        = (materialui_handle_t*)userdata;
   mui->scroll_animation_active    = false;
   mui->scroll_animation_selection = 0;
}

static void materialui_animate_scroll(
      materialui_handle_t *mui, float scroll_pos, float duration)
{
   gfx_animation_ctx_entry_t animation_entry;
   uintptr_t animation_tag = (uintptr_t)&mui->scroll_y;

   /* Kill any existing scroll animation */
   gfx_animation_kill_by_tag(&animation_tag);

   /* mui->scroll_y will be modified by the animation
    * > Set scroll acceleration to zero to minimise
    *   potential conflicts */
   menu_input_set_pointer_y_accel(0.0f);

   /* Set 'animation active' flag */
   mui->scroll_animation_active    = true;
   mui->scroll_animation_selection = menu_navigation_get_selection();

   /* Configure animation */
   animation_entry.easing_enum     = EASING_IN_OUT_QUAD;
   animation_entry.tag             = animation_tag;
   animation_entry.duration        = duration;
   animation_entry.target_value    = scroll_pos;
   animation_entry.subject         = &mui->scroll_y;
   animation_entry.cb              = materialui_scroll_animation_end;
   animation_entry.userdata        = mui;

   /* Push animation */
   gfx_animation_push(&animation_entry);
}

/* The navigation pointer has been updated (for example by pressing up or down
   on the keyboard) */
static void materialui_navigation_set(void *data, bool scroll)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;
   gfx_display_t *p_disp    = disp_get_ptr();
    
   if (!mui || !scroll)
      return;

   materialui_animate_scroll(
         mui,
         materialui_get_scroll(mui, p_disp),
         MUI_ANIM_DURATION_SCROLL);
}

static void materialui_list_set_selection(void *data, file_list_t *list)
{
   /* This is called upon MENU_ACTION_CANCEL
    * Have to set 'scroll' to false, otherwise
    * navigating backwards in the menu is absolutely
    * horrendous... */
   materialui_navigation_set(data, false);
}

/* The navigation pointer is set back to zero */
static void materialui_navigation_clear(void *data, bool pending_push)
{
   size_t i                 = 0;
   materialui_handle_t *mui = (materialui_handle_t*)data;
   if (!mui)
      return;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);

   materialui_animate_scroll(
         mui,
         0.0f,
         MUI_ANIM_DURATION_SCROLL_RESET);
}

static void materialui_navigation_set_last(void *data)
{
   materialui_navigation_set(data, true);
}

static void materialui_navigation_alphabet(void *data, size_t *unused)
{
   materialui_navigation_set(data, true);
}

static void materialui_populate_nav_bar(
      materialui_handle_t *mui, const char *label, settings_t *settings)
{
   unsigned menu_tab_index          = 0;
   bool menu_content_show_playlists = 
      settings->bools.menu_content_show_playlists;

   /* Cache last active menu tab index */
   mui->nav_bar.last_active_menu_tab_index = mui->nav_bar.active_menu_tab_index;

   /* Back tab */
   mui->nav_bar.back_tab.enabled = menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL);

   /* Resume tab
    * > Menu driver must be alive at this point, and retroarch
    *   must be initialised, so all we have to do (or can do)
    *   is check whether a non-dummy core is loaded) */
   mui->nav_bar.resume_tab.enabled = !retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL);

   /* Menu tabs */

   /* > Main menu */
   mui->nav_bar.menu_tabs[menu_tab_index].type          =
         MUI_NAV_BAR_MENU_TAB_MAIN;
   mui->nav_bar.menu_tabs[menu_tab_index].texture_index =
         MUI_TEXTURE_TAB_MAIN;
   mui->nav_bar.menu_tabs[menu_tab_index].active        =
         string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));

   if (mui->nav_bar.menu_tabs[menu_tab_index].active)
      mui->nav_bar.active_menu_tab_index = menu_tab_index;

   menu_tab_index++;

   /* > Playlists */
   if (menu_content_show_playlists)
   {
      mui->nav_bar.menu_tabs[menu_tab_index].type          =
            MUI_NAV_BAR_MENU_TAB_PLAYLISTS;
      mui->nav_bar.menu_tabs[menu_tab_index].texture_index =
            MUI_TEXTURE_TAB_PLAYLISTS;
      mui->nav_bar.menu_tabs[menu_tab_index].active        =
            string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      if (mui->nav_bar.menu_tabs[menu_tab_index].active)
         mui->nav_bar.active_menu_tab_index = menu_tab_index;

      menu_tab_index++;
   }

   /* > Settings */
   mui->nav_bar.menu_tabs[menu_tab_index].type          =
         MUI_NAV_BAR_MENU_TAB_SETTINGS;
   mui->nav_bar.menu_tabs[menu_tab_index].texture_index =
         MUI_TEXTURE_TAB_SETTINGS;
   mui->nav_bar.menu_tabs[menu_tab_index].active        =
         string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB));

   if (mui->nav_bar.menu_tabs[menu_tab_index].active)
      mui->nav_bar.active_menu_tab_index = menu_tab_index;

   menu_tab_index++;

   /* Cache current number of menu tabs */
   mui->nav_bar.num_menu_tabs = menu_tab_index;
}

static void materialui_init_transition_animation(
      materialui_handle_t *mui, settings_t *settings)
{
   gfx_animation_ctx_entry_t alpha_entry;
   gfx_animation_ctx_entry_t x_offset_entry;
   size_t stack_size                   = 
      materialui_list_get_size(mui, MENU_LIST_PLAIN);
   uintptr_t             alpha_tag     = (uintptr_t)&mui->transition_alpha;
   uintptr_t             x_offset_tag  = (uintptr_t)&mui->transition_x_offset;
   unsigned transition_animation       = settings->uints.menu_materialui_transition_animation;

   /* If animations are disabled, reset alpha/x offset
    * values and return immediately */
   if (transition_animation == MATERIALUI_TRANSITION_ANIM_NONE)
   {
      mui->transition_alpha    = 1.0f;
      mui->transition_x_offset = 0.0f;
      mui->last_stack_size     = stack_size;
      return;
   }

   /* Fade in animation (alpha)
    * This is *always* used, regardless of the set animation
    * type */

   /* > Kill any existing animations and set
    *   initial alpha value */
   gfx_animation_kill_by_tag(&alpha_tag);
   mui->transition_alpha = 0.0f;

   /* > Configure animation */
   alpha_entry.easing_enum  = EASING_OUT_QUAD;
   alpha_entry.tag          = alpha_tag;
   alpha_entry.duration     = MUI_ANIM_DURATION_MENU_TRANSITION;
   alpha_entry.target_value = 1.0f;
   alpha_entry.subject      = &mui->transition_alpha;
   alpha_entry.cb           = NULL;
   alpha_entry.userdata     = NULL;

   /* > Push animation */
   gfx_animation_push(&alpha_entry);

   /* Slide animation (x offset) */

   /* > Kill any existing animations and set
    *   initial x offset value */
   gfx_animation_kill_by_tag(&x_offset_tag);
   mui->transition_x_offset = 0.0f;

   /* >> Menu tab 'reset' action - using navigation
    *    bar to switch directly from low level menu
    *    to a top level menu
    *    - We apply a standard 'back' animation here */
   if (mui->menu_stack_flushed)
   {
      if (transition_animation != MATERIALUI_TRANSITION_ANIM_FADE)
         mui->transition_x_offset = -1.0f;
   }
   /* >> Menu 'forward' action */
   else if (stack_size > mui->last_stack_size)
   {
      if (transition_animation == MATERIALUI_TRANSITION_ANIM_SLIDE)
         mui->transition_x_offset = 1.0f;
   }
   /* >> Menu 'back' action */
   else if (stack_size < mui->last_stack_size)
   {
      if (transition_animation == MATERIALUI_TRANSITION_ANIM_SLIDE)
         mui->transition_x_offset = -1.0f;
   }
   /* >> Menu tab 'switch' action - using navigation
    *    bar to switch between top level menus */
   else if ((stack_size == 1) &&
            (transition_animation != MATERIALUI_TRANSITION_ANIM_FADE))
   {
      /* We're not changing menu levels here, so set
       * slide to match horizontal list 'movement'
       * direction */
      if (mui->nav_bar.active_menu_tab_index < mui->nav_bar.last_active_menu_tab_index)
      {
         if (mui->nav_bar.menu_navigation_wrapped)
            mui->transition_x_offset = 1.0f;
         else
            mui->transition_x_offset = -1.0f;
      }
      else if (mui->nav_bar.active_menu_tab_index > mui->nav_bar.last_active_menu_tab_index)
      {
         if (mui->nav_bar.menu_navigation_wrapped)
            mui->transition_x_offset = -1.0f;
         else
            mui->transition_x_offset = 1.0f;
      }
   }

   mui->last_stack_size = stack_size;

   if (mui->transition_x_offset != 0.0f)
   {
      /* > Configure animation */
      x_offset_entry.easing_enum  = EASING_OUT_QUAD;
      x_offset_entry.tag          = x_offset_tag;
      x_offset_entry.duration     = MUI_ANIM_DURATION_MENU_TRANSITION;
      x_offset_entry.target_value = 0.0f;
      x_offset_entry.subject      = &mui->transition_x_offset;
      x_offset_entry.cb           = NULL;
      x_offset_entry.userdata     = NULL;

      /* > Push animation */
      gfx_animation_push(&x_offset_entry);
   }
}

/* A new list has been pushed */
static void materialui_populate_entries(
      void *data, const char *path,
      const char *label, unsigned i)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;
   settings_t *settings     = config_get_ptr();

   if (!mui || !settings)
      return;

   /* Set menu title */
   menu_entries_get_title(mui->menu_title, sizeof(mui->menu_title));

   /* Check whether this is the playlists tab
    * (this requires special handling when
    * scrolling via an alphabet search) */
   mui->is_playlist_tab = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

   /* Check whether this is the core updater menu
    * (this requires special handling when long
    * pressing an entry) */
   mui->is_core_updater_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST));

   /* Check whether we are currently viewing a playlist,
    * file-browser-type list or dropdown list
    * (each of these is regarded as a 'plain' list,
    * and potentially a 'long' list, with special
    * gesture-based navigation shortcuts)  */
   mui->is_playlist      = false;
   mui->is_file_list     = false;
   mui->is_dropdown_list = false;

   mui->is_playlist = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST)) ||
                      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY)) ||
                      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST));

   if (!mui->is_playlist)
   {
      /* > All of the following count as a 'file list'
       *   Note: MENU_ENUM_LABEL_FAVORITES is always set
       *   as the 'label' when navigating directories after
       *   selecting load content */
      mui->is_file_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PASS)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_OVERLAY)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_LIST));

      if (!mui->is_file_list)
         mui->is_dropdown_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_SPECIAL)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_RESOLUTION)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PARAMETER)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PRESET_PARAMETER)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_DISK_INDEX)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD));
   }

   /* If this is a *valid* playlist, then cache it
    * Note: Empty playlists have a 'no entries available'
    * message as the first item - thus a playlist is
    * valid if the first item is of the correct
    * FILE_TYPE_RPL_ENTRY type */
   mui->playlist = NULL;
   if (mui->is_playlist)
   {
      file_list_t *list = menu_entries_get_selection_buf_ptr(0);
      size_t list_size  = menu_entries_get_size();

      if (list &&
          (list_size > 0) &&
          (list->list[0].type == FILE_TYPE_RPL_ENTRY))
         mui->playlist = playlist_get_cached();
   }

   /* Update navigation bar tabs
    * > Note: We do this regardless of whether
    *   the navigation bar is currently shown.
    *   Since the visibility may change at any
    *   point, we must always keep track of the
    *   current navigation bar status */
   materialui_populate_nav_bar(mui, label, settings);

   /* Update list view/thumbnail parameters */
   materialui_update_list_view(mui, settings);

   /* Reset touch feedback parameters
    * (i.e. there should be no leftover highlight
    * animations when switching to a new list) */
   mui->touch_feedback_selection        = 0;
   mui->touch_feedback_alpha            = 0.0f;
   mui->touch_feedback_update_selection = false;

   /* Initialise menu transition animation */
   materialui_init_transition_animation(mui, settings);

   /* Ensure that fullscreen thumbnail view
    * is disabled */
   materialui_hide_fullscreen_thumbnails(mui, false);

   /* Reset 'menu stack flushed' state */
   mui->menu_stack_flushed = false;

   /* At this point, the first and last on screen
    * entry indices are set based on the *previous*
    * menu list. The new updated (correct) values
    * cannot be determined until the next call of
    * materialui_render(). Normally this isn't an
    * issue, but we have an annoying edge case:
    * if the menu scale (or anything else that
    * affects layout) changes in-between a call
    * of materialui_populate_entries() and a call
    * of materialui_render(), we can end up auto
    * selecting an on screen entry using the old
    * (wrong) first and last entry indices. A
    * simple fix (workaround) for this is to just
    * reset the first and last entry indices to zero
    * whenever materialui_populate_entries() is called
    * > ADDENDUM: If 'prevent populate' is currently
    *   set, then we are to assume that this is the
    *   same menu list as the previous populate_entries()
    *   invocation. In this very specific case we must
    *   not reset the first and last entry indices,
    *   since this may in fact correspond to an option
    *   value toggle that simultaneously refreshes the
    *   existing menu list *and* causes a layout change
    *   (i.e. if we *did* reset the entry indices, the
    *   selection pointer would incorrectly 'jump' from
    *   the current selection to the top of the list) */
   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
   else
   {
      mui->first_onscreen_entry = 0;
      mui->last_onscreen_entry  = 0;
   }

   /* Note: mui->scroll_y position needs to be set here,
    * but we can't do this until materialui_compute_entries_box()
    * has been called. We therefore delegate it until mui->need_compute
    * is acted upon */
   mui->need_compute = true;
}

/* Context reset is called on launch or when a core is launched */
static void materialui_context_reset(void *data, bool is_threaded)
{
   materialui_handle_t        *mui = (materialui_handle_t*)data;
   settings_t        *settings     = config_get_ptr();
   const char *path_menu_wallpaper = settings ? settings->paths.path_menu_wallpaper : NULL;
   gfx_display_t *p_disp           = disp_get_ptr();

   if (!mui)
      return;

   materialui_layout(mui, p_disp, settings, is_threaded);
   materialui_context_bg_destroy(mui);
   gfx_display_deinit_white_texture();
   gfx_display_init_white_texture();
   materialui_context_reset_textures(mui);
   materialui_context_reset_playlist_icons(mui);
   menu_screensaver_context_destroy(mui->screensaver);

   if (path_is_valid(path_menu_wallpaper))
      task_push_image_load(path_menu_wallpaper,
            video_driver_supports_rgba(), 0,
            menu_display_handle_wallpaper_upload, NULL);

   video_driver_monitor_reset();
}

static int materialui_environ(enum menu_environ_cb type,
      void *data, void *userdata)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;

   if (!mui)
      return -1;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         mui->show_mouse = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         mui->show_mouse = false;
         break;
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         {
            settings_t *settings          = config_get_ptr();
            /* Reset playlist icon list */
            materialui_context_destroy_playlist_icons(mui);
            materialui_refresh_playlist_icon_list(mui, settings);
            materialui_context_reset_playlist_icons(mui);

            /* If we are currently viewing the playlists tab,
             * the menu must be refreshed (since icon indices
             * may have changed) */
            if (mui->is_playlist_tab)
            {
               bool refresh = false;
               menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
               menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
            }
         }
         break;
      case MENU_ENVIRON_ENABLE_SCREENSAVER:
         mui->show_screensaver = true;
         break;
      case MENU_ENVIRON_DISABLE_SCREENSAVER:
         mui->show_screensaver = false;
         break;
      default:
         return -1;
   }

   return 0;
}

/* Called before we push the new list after:
 * - Clicking a menu-type tab on the navigation bar
 * - Using left/right to navigate between top level menus */
static bool materialui_preswitch_tabs(
      materialui_handle_t *mui, materialui_nav_bar_menu_tab_t *target_tab)
{
   size_t stack_size       = 0;
   file_list_t *menu_stack = NULL;
   bool stack_flushed      = false;

   /* Pressing a navigation menu tab always returns us to
    * one of the top level menus. There are two ways to
    * implement this:
    * 1) Push a new menu list
    * 2) Reset the current menu stack, then switch
    *    to new menu
    * Option 1 seems like a good idea, since it means the
    * user's last menu position is remembered (so a back
    * action still works as expected after switching to the
    * new top level menu) - but the issue here is that the
    * menu stack can easily balloon to 'infinite' size,
    * which we simply cannot allow.
    * So we choose option 2 instead.
    * Thus, if the current menu stack size is greater than
    * 1, flush it all away...
    * Note: As far as I can tell, this if functionally
    * identical to just triggering multiple 'back' actions,
    * and so should be 'safe' */
   if (materialui_list_get_size(mui, MENU_LIST_PLAIN) > 1)
   {
      stack_flushed = true;
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU), 0);
      /* Clear this, just in case... */
      filebrowser_clear_type();
   }

   /* Get current stack
    * (stack size should be zero here, but account
    * for unknown errors)  */
   menu_stack = menu_entries_get_menu_stack_ptr(0);
   stack_size = menu_stack->size;

   /* Sanity check
    * Note: if this fails, then 'stack flushed'
    * status is irrelevant... */
   if (stack_size < 1)
      return false;

   /* Delete existing label */
   if (menu_stack->list[stack_size - 1].label)
      free(menu_stack->list[stack_size - 1].label);
   menu_stack->list[stack_size - 1].label = NULL;

   /* Assign new label/type */
   switch (target_tab->type)
   {
      case MUI_NAV_BAR_MENU_TAB_PLAYLISTS:
         menu_stack->list[stack_size - 1].label =
            strdup(msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));
         menu_stack->list[stack_size - 1].type =
            MENU_PLAYLISTS_TAB;
         break;
      case MUI_NAV_BAR_MENU_TAB_SETTINGS:
         menu_stack->list[stack_size - 1].label =
            strdup(msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB));
         menu_stack->list[stack_size - 1].type =
            MENU_SETTINGS;
         break;
      case MUI_NAV_BAR_MENU_TAB_MAIN:
      default:
         menu_stack->list[stack_size - 1].label =
            strdup(msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
         menu_stack->list[stack_size - 1].type =
            MENU_SETTINGS;
         break;
   }

   return stack_flushed;
}

/* Navigates to a top level menu tab
 * > If tab != NULL, switches directly to tab
 * > If tab == NULL, this is a left/right navigation
 *   event - in this case, 'action' is used to determine
 *   target tab */
static int materialui_switch_tabs(
      materialui_handle_t *mui, materialui_nav_bar_menu_tab_t *tab,
      enum menu_action action)
{
   materialui_nav_bar_menu_tab_t *target_tab = tab;

   /* Reset status parameters to default values
    * > Saves checks later */
   mui->nav_bar.menu_navigation_wrapped = false;
   mui->menu_stack_flushed              = false;

   /* If target tab is NULL, interpret menu action */
   if (!target_tab)
   {
      int target_tab_index = 0;

      switch (action)
      {
         case MENU_ACTION_LEFT:
            {
               target_tab_index = (int)mui->nav_bar.active_menu_tab_index - 1;

               if (target_tab_index < 0)
               {
                  target_tab_index = (int)mui->nav_bar.num_menu_tabs - 1;
                  mui->nav_bar.menu_navigation_wrapped = true;
               }
            }
            break;
         case MENU_ACTION_RIGHT:
            {
               target_tab_index = (int)mui->nav_bar.active_menu_tab_index + 1;

               if (target_tab_index >= mui->nav_bar.num_menu_tabs)
               {
                  target_tab_index = 0;
                  mui->nav_bar.menu_navigation_wrapped = true;
               }
            }
            break;
         default:
            /* Error condition */
            return -1;
      }

      target_tab = &mui->nav_bar.menu_tabs[target_tab_index];
   }

   /* Cannot switch to a tab that is already active */
   if (!target_tab->active)
   {
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
      bool stack_flushed         = false;
      int ret                    = 0;

      /* Sanity check */
      if (!selection_buf)
         return -1;

      /* Perform pre-switch operations */
      stack_flushed = materialui_preswitch_tabs(mui, target_tab);

      /* Perform switch */
      ret           = menu_driver_deferred_push_content_list(
            selection_buf);

      /* Note: If materialui_preswitch_tabs() flushes
       * the stack, then both materialui_preswitch_tabs()
       * AND action_content_list_switch() will cause the
       * menu to refresh
       * > For animation purposes, we therefore cannot
       *   register 'menu_stack_flushed' status until
       *   AFTER action_content_list_switch() has been
       *   called */
      mui->menu_stack_flushed = stack_flushed;

      return ret;
   }

   return 0;
}

static void materialui_switch_list_view(materialui_handle_t *mui, settings_t *settings);

/* Material UI requires special handling of certain
 * menu input functions, due to the fact that navigation
 * controls are relative to the currently selected item,
 * but with Material UI it is common for the currently
 * selected item to be off screen (so normal up/down/left/
 * right input can send the user to unexpected menu
 * locations).
 * This function pre-processes a menu action, performing
 * internal navigation adjustments.
 * The returned menu action will in most cases be the same
 * as the provided function argument, but may be
 * MENU_ACTION_NOOP if the current selection position
 * requires input to be inhibited */
static enum menu_action materialui_parse_menu_entry_action(
      materialui_handle_t *mui, enum menu_action action)
{
   enum menu_action new_action = action;

   /* If fullscreen thumbnail view is active, any
    * valid menu action will disable it... */
   if (mui->show_fullscreen_thumbnails)
   {
      if (action != MENU_ACTION_NOOP)
      {
         materialui_hide_fullscreen_thumbnails(mui, true);

         /* ...and any action other than Select/OK
          * is ignored
          * > We allow pass-through of Select/OK since
          *   users may want to run content directly
          *   after viewing fullscreen thumbnails (i.e.
          *   the large images may pique their interest),
          *   and having to press RetroPad A or the Return
          *   key twice is navigationally confusing
          * > Note that we can only do this for non-pointer
          *   input, though (when using a mouse/touchscreen,
          *   there just aren't enough distinct inputs types
          *   to single out a rational Select/OK action
          *   when fullscreen thumbnails are shown) */
         if ((action != MENU_ACTION_SELECT) &&
             (action != MENU_ACTION_OK))
            return MENU_ACTION_NOOP;
      }
   }

   /* Scan user inputs */
   switch (action)
   {
      case MENU_ACTION_UP:
      case MENU_ACTION_DOWN:
         /* Navigate up/down
          * > If current selection is off screen,
          *   auto select 'middle' item */
         materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_CENTRE);
         break;
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         /* Navigate left/right
          * > If this is a top level menu *and* the navigation
          *   bar is shown, left/right is used to switch tabs */
         if ((mui->nav_bar.location != MUI_NAV_BAR_LOCATION_HIDDEN) &&
             (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1))
         {
            retro_time_t current_time = menu_driver_get_current_time();
            size_t scroll_accel       = 0;

            /* Determine whether input repeat is
             * currently active
             * > This is always true when scroll
             *   acceleration is greater than zero */
            menu_driver_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL,
                  &scroll_accel);

            if (scroll_accel > 0)
            {
               /* Ignore input action if tab switch period
                * is less than defined limit */
               if ((current_time - mui->last_tab_switch_time) <
                     MUI_TAB_SWITCH_REPEAT_DELAY)
               {
                  new_action = MENU_ACTION_NOOP;
                  break;
               }
            }
            mui->last_tab_switch_time = current_time;

            /* Perform tab switch */
            materialui_switch_tabs(mui, NULL, action);
            new_action = MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE_LABEL;
         }
         else
         {
            /* If this is a playlist, file list or drop down
             * list, left/right are used for fast navigation
             * > If current selection is off screen, auto select
             *  'middle' item */
            if (mui->is_playlist || mui->is_file_list || mui->is_dropdown_list)
               materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_CENTRE);
            else
            {
               size_t selection = menu_navigation_get_selection();

               /* In all other cases, if current selection is off
                * screen, have to disable input - otherwise user can
                * inadvertently change the value of settings they
                * cannot see... */
               if (!materialui_entry_onscreen(mui, selection))
                  new_action = MENU_ACTION_NOOP;
            }
         }
         break;
      case MENU_ACTION_SCROLL_UP:
         /* Descend alphabet (Z towards A)
          * > If this is the playlists tab, an alphabet
          *   search is highly ineffective - instead,
          *   interpret this as a 'left' scroll action */
         if (mui->is_playlist_tab)
         {
            materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_CENTRE);
            new_action = MENU_ACTION_LEFT;
         }
         /* > ...otherwise, if current selection is off
          *   screen, auto select *last* item */
         else
            materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_LAST);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         /* Ascend alphabet (A towards Z)
          * > If this is the playlists tab, an alphabet
          *   search is highly ineffective - instead,
          *   interpret this as a 'right' scroll action */
         if (mui->is_playlist_tab)
         {
            materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_CENTRE);
            new_action = MENU_ACTION_RIGHT;
         }
         /* > ...otherwise, if current selection is off
          *   screen, auto select *first* item */
         else
            materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_FIRST);
         break;
      case MENU_ACTION_SCAN:
         /* - If this is a playlist, 'scan' command is used
          *   to cycle current thumbnail view
          * - If this is not a playlist, perform default
          *   'scan' action *if* current selection is
          *   on screen */
         {
            size_t selection = menu_navigation_get_selection();

            if (mui->is_playlist)
            {
               settings_t *settings = config_get_ptr();
               if (settings)
                  materialui_switch_list_view(mui, settings);
               new_action = MENU_ACTION_NOOP;
            }
            else if (!materialui_entry_onscreen(mui, selection))
               new_action = MENU_ACTION_NOOP;
         }
         break;
      case MENU_ACTION_START:
         /* - If this is a playlist, attempt to show
          *   fullscreen thumbnail view
          * - If this is not a playlist, perform default
          *   'start' action *if* current selection is
          *   on screen */
         {
            size_t selection = menu_navigation_get_selection();

            if (mui->is_playlist)
            {
               materialui_show_fullscreen_thumbnails(mui, selection);
               new_action = MENU_ACTION_NOOP;
            }
            else if (!materialui_entry_onscreen(mui, selection))
               new_action = MENU_ACTION_NOOP;
         }
         break;
      case MENU_ACTION_INFO:
         /* Well here's a fine piece of nonsense...
          * > Whenever an 'info' action is performed,
          *   materialui_list_insert() gets called, which
          *   in turn causes materialui_compute_entries_box()
          *   to be called
          * > For node sizes to be determined correctly by
          *   materialui_compute_entries_box(), we need
          *   complete menu entry sublabel text
          * > When viewing a playlist, sublabel text is
          *   determined by action_bind_sublabel_playlist_entry()
          * > action_bind_sublabel_playlist_entry() skips content
          *   runtime info if the input 'label' argument does not
          *   correspond to a valid playlist type
          * > ...and guess what: when showing an info box, the
          *   'label' argument passed to action_bind_sublabel_playlist_entry()
          *   gets changed to 'info_screen' - which isn't a valid
          *   playlist type (obviously...)
          * The net result is that an info action on a playlist
          * entry completely screws up the node dimensions,
          * because we end up measuring the height of the wrong
          * sublabel text...
          * Since playlist entries have no info data anyway, we
          * avoid this foolishness by simply disabling the 'info'
          * action when viewing playlists...
          * In addition, an 'info' action is only valid in general
          * if the currently selected entry is on screen */
         {
            size_t selection = menu_navigation_get_selection();

            if (mui->is_playlist ||
                !materialui_entry_onscreen(mui, selection))
               new_action = MENU_ACTION_NOOP;
         }
         break;
      case MENU_ACTION_SELECT:
      case MENU_ACTION_OK:
         /* Select/OK: Perform action on currently
          * selected item
          * > This only makes sense if currently
          *   selected item is on screen. If it
          *   is off screen, must disable input */
         {
            size_t selection = menu_navigation_get_selection();

            if (!materialui_entry_onscreen(mui, selection))
               new_action = MENU_ACTION_NOOP;
         }
         break;
      case MENU_ACTION_CANCEL:
         /* If user hides navigation bar via the settings
          * tab, pressing cancel (several times) will return
          * them to the top level settings menu - but since
          * left/right does not switch tabs when the navigation
          * bar is hidden, they will get 'stuck' (i.e. cannot
          * return to the main menu)
          * > We therefore have to handle this special case
          *   by switching to the main menu tab whenever the
          *   user instigates a cancel action from any top
          *   level menu other than main *if* the navigation
          *   bar is hidden */
         if ((mui->nav_bar.location == MUI_NAV_BAR_LOCATION_HIDDEN) &&
             (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1))
         {
            unsigned main_menu_tab_index                 = 0;
            materialui_nav_bar_menu_tab_t *main_menu_tab = NULL;
            unsigned i;

            /* Find index of main menu tab */
            for (i = 0; i < mui->nav_bar.num_menu_tabs; i++)
            {
               if (mui->nav_bar.menu_tabs[i].type == MUI_NAV_BAR_MENU_TAB_MAIN)
               {
                  main_menu_tab       = &mui->nav_bar.menu_tabs[i];
                  main_menu_tab_index = i;
               }
            }

            /* If current tab is not main, switch to main */
            if (main_menu_tab &&
                (main_menu_tab_index != mui->nav_bar.active_menu_tab_index))
            {
               materialui_switch_tabs(mui, main_menu_tab, MENU_ACTION_NOOP);
               new_action = MENU_ACTION_NOOP;
            }
         }
         break;
      default:
         /* In all other cases, pass through input
          * menu action without intervention */
         break;
   }

   return new_action;
}

/* Menu entry action callback
 * > Performs required Material UI menu
 *   input handling/pre-processing */
static int materialui_menu_entry_action(
      void *userdata, menu_entry_t *entry,
      size_t i, enum menu_action action)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;
   menu_entry_t *entry_ptr  = entry;
   size_t selection         = i;
   size_t new_selection;
   /* Process input action */
   enum menu_action new_action = materialui_parse_menu_entry_action(
         mui, action);

   /* NOTE: It would make sense to stop here if the
    * resultant action is a NOOP (MENU_ACTION_NOOP),
    * but the underlying menu code requires us to call
    * generic_menu_entry_action() even in this case.
    * If we don't, internal breakage will occur - so
    * ignore new_action type, and just continue
    * regardless... */

   /* Check whether current selection has changed
    * (due to automatic on screen entry selection...) */
   new_selection = menu_navigation_get_selection();

   if (new_selection != selection)
   {
      static menu_entry_t new_entry;

      /* Selection has changed - must update entry
       * pointer (we could probably get away without
       * doing this, but it would break the API...) */
      MENU_ENTRY_INIT(new_entry);
      new_entry.path_enabled       = false;
      new_entry.label_enabled      = false;
      new_entry.rich_label_enabled = false;
      new_entry.value_enabled      = false;
      new_entry.sublabel_enabled   = false;
      menu_entry_get(&new_entry, 0, new_selection, NULL, true);
      entry_ptr                    = &new_entry;
   }

   /* Call standard generic_menu_entry_action() function */
   return generic_menu_entry_action(userdata, entry_ptr, new_selection, new_action);
}

/* A new list has been pushed. We use this callback to customize a few lists for
   this menu driver */
static int materialui_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   int ret                  = -1;
   core_info_list_t *list   = NULL;
   materialui_handle_t *mui = (materialui_handle_t*)userdata;

   if (!mui)
      return ret;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         {
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION_FAVORITES_DIR, 0, 0);

            core_info_get_list(&list);
            if (list->info_count > 0)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                     MENU_SETTING_ACTION, 0, 0);
            }

            if (frontend_driver_parse_drive_list(info->list, true) != 0)
               menu_entries_append_enum(info->list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                  MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                  MENU_SETTING_ACTION, 0, 0);

            info->need_push    = true;
            info->need_refresh = true;
            ret = 0;
         }
         break;
      case DISPLAYLIST_MAIN_MENU:
         {
            settings_t   *settings      = config_get_ptr();
            rarch_system_info_t *system = &runloop_state_get_ptr()->system;

            /* If navigation bar is hidden, use default
             * main menu */
            if (mui->nav_bar.location == MUI_NAV_BAR_LOCATION_HIDDEN)
               return ret;

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            if (retroarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL))
            {
               if (!retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                        info->list,
                        MENU_ENUM_LABEL_CONTENT_SETTINGS,
                        PARSE_ACTION,
                        false);
               }
            }
            else
            {
               if (system && system->load_no_content)
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                        info->list,
                        MENU_ENUM_LABEL_START_CORE,
                        PARSE_ACTION,
                        false);
               }

#ifndef HAVE_DYNAMIC
               if (frontend_driver_has_fork())
#endif
               {
                  if (settings->bools.menu_show_load_core)
                  {
                     MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                           info->list,
                           MENU_ENUM_LABEL_CORE_LIST,
                           PARSE_ACTION,
                           false);
                  }
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
                     PARSE_ACTION,
                     false);

               if (menu_displaylist_has_subsystems())
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                        info->list,
                        MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS,
                        PARSE_ACTION,
                        false);
               }
            }

            if (settings->bools.menu_content_show_history)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_load_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_LOAD_DISC,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_dump_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_DUMP_DISC,
                     PARSE_ACTION,
                     false);
            }

#ifdef HAVE_LAKKA
            if (settings->bools.menu_show_eject_disc)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_EJECT_DISC,
                     PARSE_ACTION,
                     false);
            }
#endif

#if defined(HAVE_NETWORKING)
#ifdef HAVE_LAKKA
            MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_UPDATE_LAKKA,
                  PARSE_ACTION,
                  false);
#else
#ifdef HAVE_ONLINE_UPDATER
            if (settings->bools.menu_show_online_updater)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_ONLINE_UPDATER,
                     PARSE_ACTION,
                     false);
            }
#endif
#endif
#ifdef HAVE_MIST
            if (settings->bools.menu_show_core_manager_steam)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_CORE_MANAGER_STEAM_LIST,
                  PARSE_ACTION,
                  false);
            }
#endif
            if (settings->uints.menu_content_show_add_entry ==
                  MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_ADD_CONTENT_LIST,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_content_show_netplay)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_NETPLAY,
                     PARSE_ACTION,
                     false);
            }
#endif
            if (settings->bools.menu_show_information)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_INFORMATION_LIST,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_configurations)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_help)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_HELP_LIST,
                     PARSE_ACTION,
                     false);
            }
#if !defined(IOS)

            if (settings->bools.menu_show_restart_retroarch)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_RESTART_RETROARCH,
                     PARSE_ACTION,
                     false);
            }

            MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                  info->list,
                  MENU_ENUM_LABEL_QUIT_RETROARCH,
                  PARSE_ACTION,
                  false);
#endif
#if defined(HAVE_LAKKA)
            if (settings->bools.menu_show_reboot)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_REBOOT,
                     PARSE_ACTION,
                     false);
            }

            if (settings->bools.menu_show_shutdown)
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(
                     info->list,
                     MENU_ENUM_LABEL_SHUTDOWN,
                     PARSE_ACTION,
                     false);
            }
#endif
            info->need_push    = true;
            ret = 0;
         }
         break;
   }
   return ret;
}

/* Returns the active tab id */
static size_t materialui_list_get_selection(void *data)
{
   materialui_handle_t *mui   = (materialui_handle_t*)data;

   if (!mui)
      return 0;

   return (size_t)mui->nav_bar.active_menu_tab_index;
}

/* Pointer down event - used to:
 * > Cache the initial pointer x/y position
 * > Initialise touch feedback animations
 * > Activate scrollbar 'dragging' */
static int materialui_pointer_down(void *userdata,
      unsigned x, unsigned y,
      unsigned ptr, menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;

   if (!mui)
      return -1;

   /* Get initial pointer location and scroll position */
   mui->pointer_start_x        = x;
   mui->pointer_start_y        = y;
   mui->pointer_start_scroll_y = mui->scroll_y;

   /* Initialise touch feedback animation
    * > Note: ptr argument is useless here, since it
    *   has no meaning when handling touch screen input... */
   mui->touch_feedback_selection        = 0;
   mui->touch_feedback_alpha            = 0.0f;
   mui->touch_feedback_update_selection = true;

   /* Enable scrollbar dragging, if required */

   /* > Disable by default (saves checks later) */
   mui->scrollbar.dragged = false;

   /* > Check if scrollbar is enabled
    *   (note: dragging is disabled when showing
    *   fullscreen thumbnails) */
   if (mui->scrollbar.active && !mui->show_fullscreen_thumbnails)
   {
      unsigned width;
      unsigned height;
      int drag_margin_horz;
      int drag_margin_vert;
      gfx_display_t *p_disp  = disp_get_ptr();
      unsigned header_height = p_disp->header_height;

      video_driver_get_size(&width, &height);

      /* Check whether pointer down event is within
       * vertical list region */
      if ((y < header_height) ||
          (y > height - mui->nav_bar_layout_height - mui->status_bar.height))
         return 0;

      /* Determine horizontal width of scrollbar
       * 'grab box' */
      drag_margin_horz = 2 * (int)mui->margin;
      /* > If this is a landscape layout with navigation
       *   bar on the right, and landscape optimisations
       *   are disabled (or inhibited due to insufficient
       *   screen width), need to increase 'grab box' size
       *   (otherwise the active region is too close to the
       *   navigation bar) */
      if (!mui->is_portrait && mui->last_auto_rotate_nav_bar)
      {
         if (mui->landscape_optimization.border_width <= mui->margin)
            drag_margin_horz += (int)mui->margin;
         else if (mui->landscape_optimization.border_width <= 2 * mui->margin)
            drag_margin_horz += (int)((2 * mui->margin) - mui->landscape_optimization.entry_margin);
      }

      /* Check whether pointer X position is within
       * scrollbar 'grab box' */
      if (((int)x < mui->scrollbar.x - drag_margin_horz) ||
          ((int)x > mui->scrollbar.x + (int)mui->scrollbar.width))
         return 0;

      /* Determine vertical height of scrollbar
       * 'grab box' */
      drag_margin_vert = 2 * (int)mui->margin;
      /* > If scrollbar is very short, increase 'grab
       *   box' size */
      if (mui->scrollbar.height < mui->margin)
         drag_margin_vert += (int)(mui->margin - mui->scrollbar.height);

      /* Check whether pointer Y position is within
       * scrollbar 'grab box' */
      if (((int)y < mui->scrollbar.y - drag_margin_vert) ||
          ((int)y > mui->scrollbar.y + (int)mui->scrollbar.height + drag_margin_vert))
         return 0;

      /* User has 'selected' scrollbar */

      /* > Kill any existing scroll animation
       *   and reset scroll acceleration */
      materialui_kill_scroll_animation(mui);

      /* > Enable dragging */
      mui->scrollbar.dragged = true;

      /* Increase thumbnail stream delay
       * (prevents overloading the system with
       * hundreds of image requests...) */
      gfx_thumbnail_set_stream_delay(MUI_THUMBNAIL_STREAM_DELAY_SCROLLBAR_DRAG);
   }

   return 0;
}

static int materialui_pointer_up_swipe_horz_plain_list(
      materialui_handle_t *mui, menu_entry_t *entry,
      unsigned height, unsigned header_height, unsigned y,
      size_t selection, bool scroll_up)
{
   /* A swipe in the top half of the screen ascends/
    * descends the alphabet */
   if (y < (height >> 1))
      return materialui_menu_entry_action(
            mui, entry, selection,
            scroll_up ? MENU_ACTION_SCROLL_UP : MENU_ACTION_SCROLL_DOWN);
   /* A swipe in the bottom half of the screen scrolls
    * by 10% of the list size or one 'page', whichever
    * is largest */
   else
   {
      float content_height_fraction = mui->content_height * 0.1f;
      float display_height          = (int)height - (int)header_height -
            (int)mui->nav_bar_layout_height - (int)mui->status_bar.height;
      float scroll_offset           = (display_height > content_height_fraction) ?
            display_height : content_height_fraction;

      materialui_animate_scroll(
            mui,
            mui->scroll_y + (scroll_up ? (scroll_offset * -1.0f) : scroll_offset),
            MUI_ANIM_DURATION_SCROLL);
   }

   return 0;
}

static int materialui_pointer_up_swipe_horz_default(
      materialui_handle_t *mui, menu_entry_t *entry,
      unsigned ptr, size_t selection, size_t entries_end, enum menu_action action)
{
   int ret = 0;

   if ((ptr < entries_end) && (ptr == selection))
   {
      size_t new_selection = menu_navigation_get_selection();
      ret                  = materialui_menu_entry_action(
            mui, entry, selection, action);

      /* If we are changing a settings value, want to scroll
       * back to the 'pointer down' position. In all other cases
       * we do not. An entry is of the 'settings' type if:
       * - Selection pointer remains the same after MENU_ACTION event
       * - Entry value type is:
       *   > MUI_ENTRY_VALUE_TEXT
       *   > MUI_ENTRY_VALUE_SWITCH_ON
       *   > MUI_ENTRY_VALUE_SWITCH_OFF
       * Note: cannot use input (argument) entry, since this
       * will always have a blank value component */
      if (selection == new_selection)
      {
         const char *entry_value                           = NULL;
         unsigned entry_type                               = 0;
         enum msg_file_type entry_file_type                = FILE_TYPE_NONE;
         enum materialui_entry_value_type entry_value_type = MUI_ENTRY_VALUE_NONE;
         menu_entry_t last_entry;

         /* Get entry */
         MENU_ENTRY_INIT(last_entry);
         last_entry.path_enabled       = false;
         last_entry.label_enabled      = false;
         last_entry.rich_label_enabled = false;
         last_entry.sublabel_enabled   = false;

         menu_entry_get(&last_entry, 0, selection, NULL, true);

         /* Parse entry */
         if (last_entry.enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD)
            entry_value          = last_entry.password_value;
         else
            entry_value          = last_entry.value;
         entry_type                     = last_entry.type;
         entry_file_type                = msg_hash_to_file_type(
               msg_hash_calculate(entry_value));
         entry_value_type               = materialui_get_entry_value_type(
               mui, entry_value, last_entry.checked, entry_type, entry_file_type);

         /* If entry has a 'settings' type, reset scroll position */
         if ((entry_value_type == MUI_ENTRY_VALUE_TEXT) ||
             (entry_value_type == MUI_ENTRY_VALUE_SWITCH_ON) ||
             (entry_value_type == MUI_ENTRY_VALUE_SWITCH_OFF))
            materialui_animate_scroll(
                  mui,
                  mui->pointer_start_scroll_y,
                  MUI_ANIM_DURATION_SCROLL_RESET);
      }
   }

   return ret;
}

static int materialui_pointer_up_nav_bar(
      materialui_handle_t *mui,
      unsigned x, unsigned y, unsigned width, unsigned height, size_t selection,
      menu_file_list_cbs_t *cbs, menu_entry_t *entry, unsigned action)
{
   unsigned num_tabs = mui->nav_bar.num_menu_tabs + MUI_NAV_BAR_NUM_ACTION_TABS;
   unsigned tab_index;

   /* If navigation bar is hidden, do nothing */
   if (mui->nav_bar.location == MUI_NAV_BAR_LOCATION_HIDDEN)
      return 0;

   /* Determine tab 'index' - integer corresponding
    * to physical location on screen */
   if (mui->nav_bar.location == MUI_NAV_BAR_LOCATION_RIGHT)
      tab_index = y / (height / num_tabs);
   else
      tab_index = x / (width / num_tabs);

   /* Check if this is an action tab */
   if ((tab_index == 0) || (tab_index >= num_tabs - 1))
   {
      materialui_nav_bar_action_tab_t *target_tab = NULL;

      if (mui->nav_bar.location == MUI_NAV_BAR_LOCATION_RIGHT)
         target_tab = (tab_index == 0) ?
               &mui->nav_bar.resume_tab : &mui->nav_bar.back_tab;
      else
         target_tab = (tab_index == 0) ?
               &mui->nav_bar.back_tab : &mui->nav_bar.resume_tab;

      switch (target_tab->type)
      {
         case MUI_NAV_BAR_ACTION_TAB_BACK:
            if (target_tab->enabled)
               return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_CANCEL);
            break;
         case MUI_NAV_BAR_ACTION_TAB_RESUME:
            if (target_tab->enabled)
               return command_event(CMD_EVENT_MENU_TOGGLE, NULL) ? 0 : -1;
            break;
         default:
            break;
      }
   }
   /* Tab is a menu tab */
   else
      return materialui_switch_tabs(
            mui, &mui->nav_bar.menu_tabs[tab_index - 1], MENU_ACTION_NOOP);

   return 0;
}

/* If viewing a playlist with thumbnails enabled,
 * cycles current thumbnail view mode */
static void materialui_switch_list_view(materialui_handle_t *mui, settings_t *settings)
{
   bool secondary_thumbnail_enabled_prev = mui->secondary_thumbnail_enabled;

   /* Only enable view switching if we are currently viewing
    * a playlist with thumbnails enabled */
   if ((mui->list_view_type == MUI_LIST_VIEW_DEFAULT) ||
       !mui->primary_thumbnail_available)
      return;

   /* If currently selected item is off screen, then
    * changing the view mode will throw the user to
    * an unexpected off screen location...
    * To prevent this, must immediately select the
    * 'middle' on screen entry */
   materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_CENTRE);

   /* Update setting based upon current display orientation */
   if (mui->is_portrait)
   {
      configuration_set_uint(
            settings,
            settings->uints.menu_materialui_thumbnail_view_portrait,
            settings->uints.menu_materialui_thumbnail_view_portrait + 1);

      if (settings->uints.menu_materialui_thumbnail_view_portrait >=
            MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LAST)
         configuration_set_uint(settings,
               settings->uints.menu_materialui_thumbnail_view_portrait, 0);
   }
   else
   {
      configuration_set_uint(settings,
            settings->uints.menu_materialui_thumbnail_view_landscape,
            settings->uints.menu_materialui_thumbnail_view_landscape + 1);

      if (settings->uints.menu_materialui_thumbnail_view_landscape >=
            MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LAST)
         configuration_set_uint(settings,
               settings->uints.menu_materialui_thumbnail_view_landscape, 0);
   }

   /* Update list view parameters */
   materialui_update_list_view(mui, settings);

   /* If the new list view does not have thumbnails
    * enabled, or last view had dual thumbnails and
    * current does not, reset all existing thumbnails
    * (this would happen automatically at the next
    * menu level change - or destroy context, etc.
    * - but it's cleanest to do it here) */
   if ((mui->list_view_type == MUI_LIST_VIEW_DEFAULT) ||
       (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST) ||
       (secondary_thumbnail_enabled_prev && !mui->secondary_thumbnail_enabled))
      materialui_reset_thumbnails();

   /* We want to 'fade in' when switching views, so
    * trigger normal transition animation */
   materialui_init_transition_animation(mui, settings);

   mui->need_compute = true;
}

/* Pointer up event */
static int materialui_pointer_up(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      enum menu_input_pointer_gesture gesture,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned width;
   unsigned height;
   gfx_display_t *p_disp    = disp_get_ptr();
   unsigned header_height   = p_disp->header_height;
   size_t selection         = menu_navigation_get_selection();
   size_t entries_end       = menu_entries_get_size();
   materialui_handle_t *mui = (materialui_handle_t*)userdata;

   if (!mui)
      return -1;

   /* All input is ignored if user was previously
    * dragging the scrollbar */
   if (mui->scrollbar.dragged)
   {
      /* Must reset scroll acceleration, otherwise
       * list will continue to 'drift' in drag direction */
      menu_input_set_pointer_y_accel(0.0f);

      /* Reset thumbnail stream delay */
      gfx_thumbnail_set_stream_delay(mui->thumbnail_stream_delay);

      mui->scrollbar.dragged = false;
      return 0;
   }

   /* If fullscreen thumbnail view is enabled,
    * all input will disable it and otherwise
    * be ignored */
   if (mui->show_fullscreen_thumbnails)
   {
      /* Must reset scroll acceleration, in case
       * user performed a swipe (don't want menu
       * list to 'drift' after hiding fullscreen
       * thumbnails...) */
      menu_input_set_pointer_y_accel(0.0f);

      materialui_hide_fullscreen_thumbnails(mui, true);
      return 0;
   }

   video_driver_get_size(&width, &height);

   switch (gesture)
   {
      case MENU_INPUT_GESTURE_TAP:
      case MENU_INPUT_GESTURE_SHORT_PRESS:
         {
            /* Tap/press navigation bar: perform tab-specific action */
            if ((y > height - mui->nav_bar_layout_height) ||
                (x > width  - mui->nav_bar_layout_width))
               return materialui_pointer_up_nav_bar(
                     mui, x, y, width, height, selection, cbs, entry, action);
            /* Tap/press header: Menu back/cancel, or search/switch view */
            else if (y < header_height)
            {
               /* If this is a playlist, file list or core
                * updater list, enable search functionality */
               if (mui->is_playlist || mui->is_file_list || mui->is_core_updater_list)
               {
                  bool switch_view_enabled  =
                        mui->is_playlist && mui->primary_thumbnail_available;
                  /* Note: We add a little extra padding to minimise
                   * the risk of accidentally triggering a cancel */
                  unsigned back_x_threshold =
                        width -
                        ((switch_view_enabled ? 3 : 2) * mui->icon_size) -
                         mui->nav_bar_layout_width;

                  /* Check if user has touched search icon */
                  if (x > width - mui->icon_size - mui->nav_bar_layout_width)
                     return menu_input_dialog_start_search() ? 0 : -1;
                  /* Check if user has touched switch view icon */
                  else if (switch_view_enabled &&
                           x > width - (2 * mui->icon_size) - mui->nav_bar_layout_width)
                  {
                     settings_t *settings = config_get_ptr();
                     if (settings)
                        materialui_switch_list_view(mui, settings);
                     return 0;
                  }
                  /* Fall back to normal cancel action */
                  else if (x <= back_x_threshold)
                     return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_CANCEL);
               }
               /* If this is not a playlist or file list, a tap/press
                * anywhere on the header triggers a MENU_ACTION_CANCEL
                * action */
               else
                  return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_CANCEL);
            }
            /* Tap/press menu item: Activate and/or select item */
            else if ((y   < height - mui->nav_bar_layout_height - mui->status_bar.height) &&
                     (ptr < entries_end) &&
                     (x   > mui->landscape_optimization.border_width) &&
                     (x   < width - mui->landscape_optimization.border_width - mui->nav_bar_layout_width))
            {
               file_list_t *list       = NULL;
               materialui_node_t *node = NULL;
               int entry_x;
               int entry_y;

               /* Special case: If we are currently viewing
                * a 'desktop'-layout playlist, pressing the
                * sidebar toggles fullscreen thumbnails */
               if ((mui->list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP) &&
                   (x < mui->landscape_optimization.border_width + mui->thumbnail_width_max + (mui->margin * 2)))
               {
                  materialui_show_fullscreen_thumbnails(mui, selection);
                  break;
               }

               /* Get node (entry) associated with current
                * pointer item */
               list = menu_entries_get_selection_buf_ptr(0);
               if (!list)
                  break;

               node = (materialui_node_t*)list->list[ptr].userdata;
               if (!node)
                  break;

               /* Get pointer item x/y position */
               entry_x = (int)node->x;
               entry_y = (int)((float)header_height - mui->scroll_y + node->y);

               /* Check if pointer location is within the
                * bounds of the pointer item */
               if ((x < entry_x) ||
                   (x > (entry_x + node->entry_width)) ||
                   (y < entry_y) ||
                   (y > (entry_y + node->entry_height)))
                  break;

               /* Pointer input is valid - perform action */
               if (gesture == MENU_INPUT_GESTURE_TAP)
               {
                  /* A 'tap' always produces a menu action */

                  /* If current 'pointer' item is not active,
                   * activate it immediately */
                  if (ptr != selection)
                     menu_navigation_set_selection(ptr);

                  /* Perform a MENU_ACTION_SELECT on currently
                   * active item
                   * > Note that we still use 'selection'
                   *   (i.e. old selection value) here. This
                   *   ensures that materialui_menu_entry_action()
                   *   registers any change due to the above automatic
                   *   'pointer item' activation, and thus operates
                   *   on the correct target entry */
                  return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_SELECT);
               }
               else
               {
                  /* A 'short' press is used only to activate (highlight)
                   * an item - it does not invoke a MENU_ACTION_SELECT
                   * action (this is intended for use in activating a
                   * settings-type entry, prior to swiping)
                   * Note: If everything is working correctly, the
                   * ptr item should already by selected at this stage
                   * - but menu_navigation_set_selection() just sets a
                   * variable, so there's no real point in performing
                   * a (selection != ptr) check here */
                  menu_navigation_set_selection(ptr);
                  menu_input_set_pointer_y_accel(0.0f);
               }
            }
         }
         break;
      case MENU_INPUT_GESTURE_LONG_PRESS:
         if ((ptr < entries_end) && (ptr == selection))
         {
            /* If this is the core updater list, show info
             * message box for current entry.
             * In all other cases, perform 'reset to default'
             * action */
            if (mui->is_core_updater_list)
               return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_INFO);
            else
               return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_START);
         }
         break;
      case MENU_INPUT_GESTURE_SWIPE_LEFT:
         {
            /* If we are at the top level and the navigation bar is
             * enabled, a swipe should just switch between the three
             * main menu screens
             * (i.e. we don't care which item is currently selected)
             * Note: For intuitive behaviour, a *left* swipe should
             * trigger a *right* navigation event */
            if ((mui->nav_bar.location != MUI_NAV_BAR_LOCATION_HIDDEN) &&
                (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1))
               return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_RIGHT);
            /* If we are displaying a playlist/file list/dropdown list,
             * swipes are used for fast navigation */
            else if (mui->is_playlist || mui->is_file_list || mui->is_dropdown_list)
               return materialui_pointer_up_swipe_horz_plain_list(
                     mui, entry, height, header_height, y,
                     selection, true);
            /* If this is the core updater list, swipes are used
             * to open the core information menu */
            else if (mui->is_core_updater_list)
               return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_START);
         }
         /* In all other cases, just perform a normal 'left'
          * navigation event */
         return materialui_pointer_up_swipe_horz_default(
               mui, entry, ptr, selection, entries_end, MENU_ACTION_LEFT);
      case MENU_INPUT_GESTURE_SWIPE_RIGHT:
         {
            /* If we are at the top level and the navigation bar is
             * enabled, a swipe should just switch between the three
             * main menu screens
             * (i.e. we don't care which item is currently selected)
             * Note: For intuitive behaviour, a *left* swipe should
             * trigger a *left* navigation event */
            if ((mui->nav_bar.location != MUI_NAV_BAR_LOCATION_HIDDEN) &&
                (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1))
               return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_LEFT);
            /* If we are displaying a playlist/file list/dropdown list,
             * swipes are used for fast navigation */
            else if (mui->is_playlist || mui->is_file_list || mui->is_dropdown_list)
               return materialui_pointer_up_swipe_horz_plain_list(
                     mui, entry, height, header_height, y,
                     selection, false);
            /* If this is the core updater list, swipes are used
             * to open the core information menu */
            else if (mui->is_core_updater_list)
               return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_START);
         }
         /* In all other cases, just perform a normal 'right'
          * navigation event */
         return materialui_pointer_up_swipe_horz_default(
               mui, entry, ptr, selection, entries_end, MENU_ACTION_RIGHT);
      default:
         /* Ignore input */
         break;
   }

   return 0;
}

/* The menu system can insert menu entries on the fly.
 * It is used in the shaders UI, the wifi UI,
 * the netplay lobby, etc.
 *
 * This function allocates the materialui_node_t
 * for the new entry. */
static void materialui_list_insert(
      void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *label,
      size_t list_size,
      unsigned type)
{
   int i                    = (int)list_size;
   materialui_node_t *node  = NULL;
   settings_t *settings     = config_get_ptr();
   materialui_handle_t *mui = (materialui_handle_t*)userdata;
   bool menu_materialui_icons_enable = settings->bools.menu_materialui_icons_enable;
   bool thumbnail_reset     = false;

   if (!mui || !list)
      return;

   mui->need_compute        = true;
   node                     = (materialui_node_t*)list->list[i].userdata;

   if (!node)
   {
      node = (materialui_node_t*)malloc(sizeof(materialui_node_t));

      node->icon_type                        = MUI_ICON_TYPE_NONE;
      node->icon_texture_index               = 0;
      node->entry_width                      = 0.0f;
      node->entry_height                     = 0.0f;
      node->text_height                      = 0.0f;
      node->x                                = 0.0f;
      node->y                                = 0.0f;

      node->thumbnails.primary.status        = GFX_THUMBNAIL_STATUS_UNKNOWN;
      node->thumbnails.primary.texture       = 0;
      node->thumbnails.primary.width         = 0;
      node->thumbnails.primary.height        = 0;
      node->thumbnails.primary.alpha         = 0.0f;
      node->thumbnails.primary.delay_timer   = 0.0f;
      node->thumbnails.primary.fade_active   = false;

      node->thumbnails.secondary.status      = GFX_THUMBNAIL_STATUS_UNKNOWN;
      node->thumbnails.secondary.texture     = 0;
      node->thumbnails.secondary.width       = 0;
      node->thumbnails.secondary.height      = 0;
      node->thumbnails.secondary.alpha       = 0.0f;
      node->thumbnails.secondary.delay_timer = 0.0f;
      node->thumbnails.secondary.fade_active = false;
   }
   else
   {
      /* If node already exists, must free any
       * existing thumbnail */
      gfx_thumbnail_reset(&node->thumbnails.primary);
      gfx_thumbnail_reset(&node->thumbnails.secondary);
      thumbnail_reset = true;
   }

   if (!node)
   {
      RARCH_ERR("GLUI node could not be allocated.\n");
      return;
   }

   node->icon_type          = MUI_ICON_TYPE_NONE;
   node->icon_texture_index = 0;
   node->entry_width        = 0.0f;
   node->entry_height       = 0.0f;
   node->text_height        = 0.0f;
   node->x                  = 0.0f;
   node->y                  = 0.0f;

   if (!thumbnail_reset)
   {
      gfx_thumbnail_reset(&node->thumbnails.primary);
      gfx_thumbnail_reset(&node->thumbnails.secondary);
   }

   if (menu_materialui_icons_enable)
   {
      switch (type)
      {
         case MENU_SET_CDROM_INFO:
         case MENU_SET_CDROM_LIST:
#ifdef HAVE_LAKKA
         case MENU_SET_EJECT_DISC:
#endif
         case MENU_SET_LOAD_CDROM_LIST:
            node->icon_texture_index = MUI_TEXTURE_DISK;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case FILE_TYPE_DOWNLOAD_CORE:
         case FILE_TYPE_CORE:
         case MENU_SETTING_ACTION_CORE_MANAGER_OPTIONS:
         case MENU_SETTING_ACTION_CORE_LOCK:
         case MENU_SETTING_ACTION_CORE_SET_STANDALONE_EXEMPT:
         case MENU_EXPLORE_TAB:
         case MENU_CONTENTLESS_CORES_TAB:
            node->icon_texture_index = MUI_TEXTURE_CORES;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case MENU_SETTING_ACTION_CORE_OPTIONS:
            node->icon_texture_index = MUI_TEXTURE_CORE_OPTIONS;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case MENU_SETTING_ACTION_CORE_OPTION_OVERRIDE_LIST:
         case MENU_SETTING_ACTION_REMAP_FILE_MANAGER_LIST:
         case MENU_SETTING_ACTION_REMAP_FILE_LOAD:
            node->icon_texture_index = MUI_TEXTURE_SETTINGS;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
         case FILE_TYPE_DOWNLOAD_PL_THUMBNAIL_CONTENT:
            node->icon_texture_index = MUI_TEXTURE_IMAGE;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case FILE_TYPE_PARENT_DIRECTORY:
            node->icon_texture_index = MUI_TEXTURE_PARENT_DIRECTORY;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case FILE_TYPE_PLAYLIST_COLLECTION:
            materialui_set_node_playlist_icon(mui, node, path);
            break;
         case FILE_TYPE_RDB:
            node->icon_texture_index = MUI_TEXTURE_DATABASE;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case FILE_TYPE_RDB_ENTRY:
            node->icon_texture_index = MUI_TEXTURE_SETTINGS;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case FILE_TYPE_IN_CARCHIVE:
         case FILE_TYPE_PLAIN:
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
         case FILE_TYPE_DOWNLOAD_CORE_SYSTEM_FILES:
            node->icon_texture_index = MUI_TEXTURE_FILE;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case FILE_TYPE_MUSIC:
            node->icon_texture_index = MUI_TEXTURE_MUSIC;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case FILE_TYPE_MOVIE:
            node->icon_texture_index = MUI_TEXTURE_VIDEO;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case FILE_TYPE_DIRECTORY:
         case FILE_TYPE_DOWNLOAD_URL:
            node->icon_texture_index = MUI_TEXTURE_FOLDER;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case MENU_ROOM_LAN:
         case MENU_ROOM_RELAY:
         case MENU_ROOM:
            node->icon_texture_index = MUI_TEXTURE_SETTINGS;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case MENU_SETTING_ACTION_CORE_DELETE:
         case MENU_SETTING_ACTION_CORE_DELETE_BACKUP:
         case MENU_SETTING_ACTION_VIDEO_FILTER_REMOVE:
         case MENU_SETTING_ACTION_AUDIO_DSP_PLUGIN_REMOVE:
         case MENU_SETTING_ACTION_GAME_SPECIFIC_CORE_OPTIONS_REMOVE:
         case MENU_SETTING_ACTION_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE:
         case MENU_SETTING_ACTION_REMAP_FILE_REMOVE_CORE:
         case MENU_SETTING_ACTION_REMAP_FILE_REMOVE_CONTENT_DIR:
         case MENU_SETTING_ACTION_REMAP_FILE_REMOVE_GAME:
            node->icon_texture_index = MUI_TEXTURE_REMOVE;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case MENU_SETTING_ACTION_CORE_CREATE_BACKUP:
         case MENU_SETTING_ACTION_GAME_SPECIFIC_CORE_OPTIONS_CREATE:
         case MENU_SETTING_ACTION_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE:
         case MENU_SETTING_ACTION_REMAP_FILE_SAVE_CORE:
         case MENU_SETTING_ACTION_REMAP_FILE_SAVE_CONTENT_DIR:
         case MENU_SETTING_ACTION_REMAP_FILE_SAVE_GAME:
            node->icon_texture_index = MUI_TEXTURE_SAVE_STATE;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case MENU_SETTING_ACTION_CORE_RESTORE_BACKUP:
            node->icon_texture_index = MUI_TEXTURE_LOAD_STATE;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case MENU_SETTING_ACTION_CORE_OPTIONS_RESET:
         case MENU_SETTING_ACTION_REMAP_FILE_RESET:
            node->icon_texture_index = MUI_TEXTURE_UNDO_SAVE_STATE;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case MENU_SETTING_ACTION_CORE_OPTIONS_FLUSH:
            node->icon_texture_index = MUI_TEXTURE_FILE;
            node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            break;
         case MENU_SETTING_ACTION_CONTENTLESS_CORE_RUN:
            node->icon_type          = MUI_ICON_TYPE_MENU_CONTENTLESS_CORE;
            break;
         case FILE_TYPE_RPL_ENTRY:
         case MENU_SETTING_DROPDOWN_ITEM:
         case MENU_SETTING_DROPDOWN_ITEM_RESOLUTION:
         case MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_PARAM:
         case MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_PRESET_PARAM:
         case MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_NUM_PASS:
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_DEFAULT_CORE:
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_LABEL_DISPLAY_MODE:
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_RIGHT_THUMBNAIL_MODE:
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_LEFT_THUMBNAIL_MODE:
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_SORT_MODE:
         case MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
         case MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_CORE_NAME:
         case MENU_SETTING_DROPDOWN_ITEM_DISK_INDEX:
         case MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION:
         case MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION_KBD:
         case MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM:
         case MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM:
         case MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM:
         case MENU_SETTING_DROPDOWN_SETTING_INT_ITEM:
         case MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM:
         case MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM_SPECIAL:
         case MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM_SPECIAL:
         case MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM_SPECIAL:
         case MENU_SETTING_DROPDOWN_SETTING_INT_ITEM_SPECIAL:
         case MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL:
         case MENU_SETTINGS_CORE_INFO_NONE:
         case MENU_SETTING_ITEM_CORE_RESTORE_BACKUP:
         case MENU_SETTING_ITEM_CORE_DELETE_BACKUP:
            /* None of these entries have icons - catch them
             * here (and leave icon_texture_index/icon_type
             * set to the default 'disabled' state) to avoid
             * having to process the 'default' case of this
             * switch */
            break;
         default:
            if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION_LIST))              ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SYSTEM_INFORMATION))            ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_INFORMATION_AVAILABLE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS))                      ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE))     ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION))                   ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND))             ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_PRESETS_FOUND))
               )
            {
               node->icon_texture_index = MUI_TEXTURE_INFO;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DATABASE_MANAGER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CURSOR_MANAGER_LIST))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_DATABASE;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES)))
            {
               node->icon_texture_index = MUI_TEXTURE_IMAGE;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC)))
            {
               node->icon_texture_index = MUI_TEXTURE_MUSIC;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_VIDEO;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY)))
            {
               node->icon_texture_index = MUI_TEXTURE_SCAN;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY)))
            {
               node->icon_texture_index = MUI_TEXTURE_HISTORY;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_HELP;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESTART_CONTENT)))
            {
               node->icon_texture_index = MUI_TEXTURE_RESTART;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESUME_CONTENT)))
            {
               node->icon_texture_index = MUI_TEXTURE_RESUME;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CLOSE_CONTENT)))
            {
               node->icon_texture_index = MUI_TEXTURE_CLOSE;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_CORE_CHEAT_OPTIONS;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_CONTROLS;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SHADER_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_SHADERS;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_LIST)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_INFORMATION)))
            {
               node->icon_texture_index = MUI_TEXTURE_CORES;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RUN)))
            {
               node->icon_texture_index = MUI_TEXTURE_RUN;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_ADD_TO_FAVORITES;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RENAME_ENTRY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_RENAME;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_ADD_TO_MIXER;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_START_CORE))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RUN_MUSIC))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_LOAD))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_START_CORE;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_STATE)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SWITCH_INSTALLED_CORES_PFD)))
            {
               node->icon_texture_index = MUI_TEXTURE_LOAD_STATE;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_TRAY_EJECT)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_TRAY_INSERT))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_EJECT;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_IMAGE_APPEND)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_DISC)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DUMP_DISC)) ||
#ifdef HAVE_LAKKA
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_EJECT_DISC)) ||
#endif
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISC_INFORMATION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_OPTIONS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_INDEX))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_DISK;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_STATE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_SAVE_STATE;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UNDO_LOAD_STATE)))
            {
               node->icon_texture_index = MUI_TEXTURE_UNDO_LOAD_STATE;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UNDO_SAVE_STATE)))
            {
               node->icon_texture_index = MUI_TEXTURE_UNDO_SAVE_STATE;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_STATE_SLOT)))
            {
               node->icon_texture_index = MUI_TEXTURE_STATE_SLOT;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT)))
            {
               node->icon_texture_index = MUI_TEXTURE_TAKE_SCREENSHOT;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATIONS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_NEW_CONFIG)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATIONS_LIST))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_CONFIGURATIONS;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_LIST))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_LOAD_CONTENT;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DELETE_ENTRY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DELETE_PLAYLIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_REMOVE;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_HOSTING_SETTINGS)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_INFORMATION))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_NETPLAY;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS)))
            {
               node->icon_texture_index = MUI_TEXTURE_QUICKMENU;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_INSTALLED_CORES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_ASSETS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CHEATS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_DATABASES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_OVERLAYS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CG_SHADERS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST))
                  )
                  {
                     node->icon_texture_index = MUI_TEXTURE_UPDATER;
                     node->icon_type          = MUI_ICON_TYPE_INTERNAL;
                  }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_CONTENT_LIST))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_ADD;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUIT_RETROARCH)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESTART_RETROARCH))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_QUIT;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            /* TODO/FIXME - all this should go away and be refactored so that we don't have to do
             * all this manually inside this menu driver */
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DRIVER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_OUTPUT_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SYNCHRONIZATION_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FULLSCREEN_MODE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_WINDOWED_MODE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SCALING_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_HDR_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_OUTPUT_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_SYNCHRONIZATION_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MENU_SOUNDS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_MENU_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LATENCY_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATION_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SIDELOAD_CORE_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVING_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOGGING_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RECORDING_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AI_SERVICE_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCESSIBILITY_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_YOUTUBE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_TWITCH)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_FACEBOOK)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_BLUETOOTH_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_WIFI_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LAKKA_SERVICES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_USER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DIRECTORY_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PRIVACY_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MIDI_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_VIEWS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MENU_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS)) ||
#ifdef HAVE_VIDEO_LAYOUT
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS)) ||
#endif
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_REWIND_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATER_SETTINGS))        ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_SYSTEM_FILES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SET_CORE_ASSOCIATION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SHADER_APPLY_CHANGES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_REFRESH_LAN)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_LOAD)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_META_CHEAT_SEARCH)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_START_OR_CONT)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_META_CHEAT_SEARCH)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_MATERIALUI)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LEFT_THUMBNAILS_MATERIALUI)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DELETE_ALL)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_COPY_AFTER)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_COPY_BEFORE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DELETE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_MANAGER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS))
                  )
                  {
                     node->icon_texture_index = MUI_TEXTURE_SETTINGS;
                     node->icon_type          = MUI_ICON_TYPE_INTERNAL;
                  }
            else if (type >= MENU_SETTINGS_REMAPPING_PORT_BEGIN && 
                  type <= MENU_SETTINGS_REMAPPING_PORT_END)
            {
               node->icon_texture_index = MUI_TEXTURE_SETTINGS;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_FOLDER;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)))
            {
               node->icon_texture_index = MUI_TEXTURE_PLAYLIST;
               node->icon_type          = MUI_ICON_TYPE_INTERNAL;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_EXPLORE_ITEM)))
               node->icon_type          = MUI_ICON_TYPE_MENU_EXPLORE;
            else if (string_ends_with_size(label, "_input_binds_list",
                     strlen(label), STRLEN_CONST("_input_binds_list")))
            {
               unsigned i;

               for (i = 0; i < MAX_USERS; i++)
               {
                  char val[255];
                  unsigned user_value = i + 1;

                  snprintf(val, sizeof(val), "%d_input_binds_list", user_value);

                  if (string_is_equal(label, val))
                  {
                     node->icon_texture_index = MUI_TEXTURE_SETTINGS;
                     node->icon_type          = MUI_ICON_TYPE_INTERNAL;
                  }
               }
            }
            break;
      }
   }

   list->list[i].userdata = node;
}

/* Clears the current menu list */
static void materialui_list_clear(file_list_t *list)
{
   size_t i;
   size_t size = list ? list->size : 0;

   /* Must cancel pending thumbnail requests before
    * freeing node->thumbnails objects */
   gfx_thumbnail_cancel_pending_requests();

   for (i = 0; i < size; i++)
   {
      materialui_node_t *node = (materialui_node_t*)list->list[i].userdata;

      if (!node)
         continue;

      gfx_thumbnail_reset(&node->thumbnails.primary);
      gfx_thumbnail_reset(&node->thumbnails.secondary);
      file_list_free_userdata(list, i);
   }
}

static void materialui_set_thumbnail_system(void *userdata, char *s, size_t len)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;
   if (!mui)
      return;
   gfx_thumbnail_set_system(
         mui->thumbnail_path_data, s, playlist_get_cached());
}

static void materialui_get_thumbnail_system(void *userdata, char *s, size_t len)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;
   const char *system       = NULL;
   if (!mui)
      return;
   if (gfx_thumbnail_get_system(mui->thumbnail_path_data, &system))
      strlcpy(s, system, len);
}

static void materialui_refresh_thumbnail_image(void *userdata, unsigned i)
{
   materialui_handle_t *mui           = (materialui_handle_t*)userdata;
   size_t selection                   = menu_navigation_get_selection();
   bool refresh_enabled               = false;

   if (!mui)
      return;

   /* Only refresh thumbnails if we are currently viewing
    * a playlist with thumbnails enabled */
   if ((mui->list_view_type == MUI_LIST_VIEW_DEFAULT) ||
       (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST))
      return;

   /* Only refresh thumbnails if:
    * - This is *not* a 'desktop'-layout playlist and
    *   the current entry is on-screen
    * - This *is* a 'desktop'-layout playlist and current
    *   entry is selected */
   refresh_enabled = (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_DESKTOP) ?
         (i == selection) :
         materialui_entry_onscreen(mui, (size_t)i);

   if (refresh_enabled)
   {
      file_list_t *list       = menu_entries_get_selection_buf_ptr(0);
      materialui_node_t *node = NULL;
      float stream_delay      = gfx_thumb_get_ptr()->stream_delay;

      if (!list)
         return;

      node                    = (materialui_node_t*)
         list->list[(size_t)i].userdata;

      if (!node)
         return;

      /* Reset existing thumbnails */
      gfx_thumbnail_reset(&node->thumbnails.primary);
      gfx_thumbnail_reset(&node->thumbnails.secondary);

      /* No need to actually request thumbnails here
       * > Just set delay timer to the current maximum
       *   value, and thumbnails will be processed via
       *   regular means on the next frame */
      node->thumbnails.primary.delay_timer   = stream_delay;
      node->thumbnails.secondary.delay_timer = stream_delay;
   }
}

menu_ctx_driver_t menu_ctx_mui = {
   NULL,
   materialui_get_message,
   materialui_render,
   materialui_frame,
   materialui_init,
   materialui_free,
   materialui_context_reset,
   materialui_context_destroy,
   materialui_populate_entries,
   NULL,
   materialui_navigation_clear,
   NULL,
   NULL,
   materialui_navigation_set,
   materialui_navigation_set_last,
   materialui_navigation_alphabet,
   materialui_navigation_alphabet,
   NULL,
   materialui_list_insert,
   NULL,
   NULL,
   materialui_list_clear,
   NULL,
   materialui_list_push,
   materialui_list_get_selection,
   materialui_list_get_size,
   NULL,
   materialui_list_set_selection,
   NULL,
   materialui_load_image,
   "glui",
   materialui_environ,
   NULL,
   NULL,
   materialui_refresh_thumbnail_image,
   materialui_set_thumbnail_system,
   materialui_get_thumbnail_system,
   NULL,
   gfx_display_osk_ptr_at_pos,
   NULL, /* update_savestate_thumbnail_path */
   NULL, /* update_savestate_thumbnail_image */
   materialui_pointer_down,
   materialui_pointer_up,
   materialui_menu_entry_action
};

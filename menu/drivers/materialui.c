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
#include <stdio.h>
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

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_input.h"
#include "../menu_thumbnail_path.h"
#include "../menu_thumbnail.h"

#include "../widgets/menu_osk.h"
#include "../widgets/menu_filebrowser.h"

#include "../../core_info.h"
#include "../../core.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

#include "../../paths.h"
#include "../../file_path_special.h"

#include "../../dynamic.h"

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
   /* Background colours */
   uint32_t sys_bar_background;
   uint32_t title_bar_background;
   uint32_t list_background;
   uint32_t list_highlighted_background;
   uint32_t nav_bar_background;
   uint32_t surface_background;
   uint32_t thumbnail_background;
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
   /* Misc. colours */
   uint32_t header_shadow;
   uint32_t landscape_border_shadow;
   uint32_t scrollbar;
   uint32_t divider;
   uint32_t screen_fade;
   uint32_t missing_thumbnail_icon;
   float header_shadow_opacity;
   float landscape_border_shadow_opacity;
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
   /* Background colours */
   0x0069c0, /* sys_bar_background */
   0x2196f3, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xc1d5e0, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x0069c0, /* scrollbar */
   0x9ea7aa, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x34515e, /* sys_bar_background */
   0x607d8b, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xe0e0e0, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x34515e, /* scrollbar */
   0xc2c2c2, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x1F1F1F, /* title_bar_background */
   0x121212, /* list_background */
   0x34515e, /* list_highlighted_background */
   0x242424, /* nav_bar_background */
   0x1D1D1D, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x3B3B3B, /* landscape_border_shadow */
   0x90caf9, /* scrollbar */
   0x607d8b, /* divider */
   0x000000, /* screen_fade */
   0xDEDEDE, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x087f23, /* sys_bar_background */
   0x4caf50, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xdcedc8, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x087f23, /* scrollbar */
   0xaabb97, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0xba000d, /* sys_bar_background */
   0xf44336, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xf8bbd0, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0xba000d, /* scrollbar */
   0xbf5f82, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0xc8b900, /* sys_bar_background */
   0xffeb3b, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xffecb3, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0xc6a700, /* scrollbar */
   0xcbba83, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x1F1F1F, /* title_bar_background */
   0x121212, /* list_background */
   0x255d00, /* list_highlighted_background */
   0x242424, /* nav_bar_background */
   0x1D1D1D, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x3B3B3B, /* landscape_border_shadow */
   0x7ab547, /* scrollbar */
   0x498515, /* divider */
   0x000000, /* screen_fade */
   0xDEDEDE, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x3700B3, /* sys_bar_background */
   0x6200ee, /* title_bar_background */
   0xF5F5F6, /* list_background */
   0xe7b9ff, /* list_highlighted_background */
   0xE1E2E1, /* nav_bar_background */
   0xFFFFFF, /* surface_background */
   0x242424, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x018786, /* scrollbar */
   0x018786, /* divider */
   0x000000, /* screen_fade */
   0xF5F5F6, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.35f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x1F1F1F, /* title_bar_background */
   0x121212, /* list_background */
   0x51455E, /* list_highlighted_background */
   0x242424, /* nav_bar_background */
   0x1D1D1D, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x3B3B3B, /* landscape_border_shadow */
   0xC89EFC, /* scrollbar */
   0x03DAC6, /* divider */
   0x000000, /* screen_fade */
   0xDEDEDE, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x373737, /* title_bar_background */
   0x2D2D2D, /* list_background */
   0x268C75, /* list_highlighted_background */
   0x373737, /* nav_bar_background */
   0x333333, /* surface_background */
   0x0B0B0B, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x9F9F9F, /* scrollbar */
   0xFFFFFF, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x4C566A, /* title_bar_background */
   0x2E3440, /* list_background */
   0x3f444f, /* list_highlighted_background */
   0x3B4252, /* nav_bar_background */
   0x3B4252, /* surface_background */
   0x0B0B0B, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0xA0A5AD, /* scrollbar */
   0x81A1C1, /* divider */
   0x000000, /* screen_fade */
   0xD8DEE9, /* missing_thumbnail_icon */
   0.4f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x504945, /* title_bar_background */
   0x282828, /* list_background */
   0x3C3836, /* list_highlighted_background */
   0x1D2021, /* nav_bar_background */
   0x32302F, /* surface_background */
   0x0B0B0B, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x7C6F64, /* scrollbar */
   0xD5C4A1, /* divider */
   0x000000, /* screen_fade */
   0xA89984, /* missing_thumbnail_icon */
   0.4f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x053542, /* title_bar_background */
   0x002B36, /* list_background */
   0x073642, /* list_highlighted_background */
   0x003541, /* nav_bar_background */
   0x073642, /* surface_background */
   0x0B0B0B, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x586E75, /* scrollbar */
   0x2AA198, /* divider */
   0x000000, /* screen_fade */
   0x657B83, /* missing_thumbnail_icon */
   0.4f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0x3399FF, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0x39859A, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0x23A367, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0xCE6E1F, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0xD16FD8, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0x814FFF, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x353535, /* title_bar_background */
   0x191919, /* list_background */
   0xCB1619, /* list_highlighted_background */
   0x282828, /* nav_bar_background */
   0x333333, /* surface_background */
   0x000000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0x727272, /* scrollbar */
   0x727272, /* divider */
   0x000000, /* screen_fade */
   0xDADADA, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
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
   /* Background colours */
   0x000000, /* sys_bar_background */
   0x350000, /* title_bar_background */
   0x000000, /* list_background */
   0x400000, /* list_highlighted_background */
   0x350000, /* nav_bar_background */
   0x400000, /* surface_background */
   0x250000, /* thumbnail_background */
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
   /* Misc. colours */
   0x000000, /* header_shadow */
   0x000000, /* landscape_border_shadow */
   0xA10000, /* scrollbar */
   0xE60000, /* divider */
   0x000000, /* screen_fade */
   0xE60000, /* missing_thumbnail_icon */
   0.3f,     /* header_shadow_opacity */
   0.45f,    /* landscape_border_shadow_opacity */
   0.75f     /* screen_fade_opacity */
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
   /* Background colours */
   float sys_bar_background[16];
   float title_bar_background[16];
   float list_background[16];
   float list_highlighted_background[16];
   float nav_bar_background[16];
   float surface_background[16];
   float thumbnail_background[16];
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
   float scrollbar[16];
   float divider[16];
   float screen_fade[16];
   float missing_thumbnail_icon[16];
   float landscape_border_shadow_opacity;
   float screen_fade_opacity;
} materialui_colors_t;

/* ==============================
 * Colour Themes END
 * ============================== */

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
   MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON
};

/* This structure holds auxiliary information for
 * each menu entry (physical on-screen size/position,
 * icon data, thumbnail data, etc.) */
typedef struct
{
   bool has_icon;
   unsigned icon_texture_index;
   float entry_height;
   float text_height;
   float y;

   /* Thumbnail containers */
   struct
   {
      menu_thumbnail_t primary;
      menu_thumbnail_t secondary;
   } thumbnails;

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
 * Note: Only bottom and right are supported
 * at present... */
enum materialui_nav_bar_location_type
{
   MUI_NAV_BAR_LOCATION_BOTTOM = 0,
   MUI_NAV_BAR_LOCATION_RIGHT
};

/* This structure holds all runtime parameters
 * associated with a navigation bar menu tab */
typedef struct
{
   enum materialui_nav_bar_menu_tab_type type;
   unsigned texture_index;
   bool active;
} materialui_nav_bar_menu_tab_t;

/* This structure holds all runtime parameters
 * associated with a navigation bar action tab */
typedef struct
{
   enum materialui_nav_bar_action_tab_type type;
   unsigned texture_index;
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
   bool menu_navigation_wrapped;
   enum materialui_nav_bar_location_type location;
   materialui_nav_bar_action_tab_t back_tab;
   materialui_nav_bar_action_tab_t resume_tab;
   materialui_nav_bar_menu_tab_t menu_tabs[MUI_NAV_BAR_NUM_MENU_TABS_MAX];
} materialui_nav_bar_t;

/* This structure holds all runtime parameters for
 * the scrollbar */
typedef struct
{
   unsigned width;
   unsigned height;
   int x;
   int y;
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
   video_font_raster_block_t raster_block;
   int font_height;
   unsigned glyph_width;
} materialui_font_data_t;

#define MUI_BATTERY_PERCENT_MAX_LENGTH 12
#define MUI_TIMEDATE_MAX_LENGTH        255

/* This structure is used to cache system bar
 * string data (+ metadata) to improve rendering
 * performance */
typedef struct
{
   char battery_percent_str[MUI_BATTERY_PERCENT_MAX_LENGTH];
   char timedate_str[MUI_TIMEDATE_MAX_LENGTH];
   int battery_percent_width;
   int timedate_width;
} materialui_sys_bar_cache_t;

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

typedef struct materialui_handle
{
   bool is_portrait;
   bool need_compute;
   bool mouse_show;
   bool is_playlist;
   bool is_file_list;
   bool is_dropdown_list;
   bool last_auto_rotate_nav_bar;
   bool menu_stack_flushed;

   enum materialui_landscape_layout_optimization_type
         last_landscape_layout_optimization;

   playlist_t *playlist;

   unsigned last_width;
   unsigned last_height;
   float last_scale_factor;
   float dip_base_unit_size;

   int cursor_size;

   unsigned sys_bar_height;
   unsigned title_bar_height;
   unsigned header_shadow_height;
   unsigned icon_size;
   unsigned sys_bar_icon_size;
   unsigned margin;
   unsigned sys_bar_margin;
   unsigned landscape_entry_margin;
   unsigned entry_divider_width;

   /* Navigation bar parameters
    * Note: layout width and height are convenience
    * variables used when determining usable width/
    * height for all other menu elements - e.g. when
    * navigation bar is at the bottom of the screen
    * nav_bar_screen_width is zero */
   unsigned nav_bar_layout_width;
   unsigned nav_bar_layout_height;
   materialui_nav_bar_t nav_bar;

   /* Scrollbar parameters */
   materialui_scrollbar_t scrollbar;

   size_t first_onscreen_entry;
   size_t last_onscreen_entry;

   /* Y position of the vertical scroll */
   float scroll_y;
   float content_height;

   char msgbox[1024];

   char menu_title[255];

   struct
   {
      menu_texture_item bg;
      menu_texture_item list[MUI_TEXTURE_LAST];
   } textures;

   /* Font data */
   struct
   {
      materialui_font_data_t title;
      materialui_font_data_t list;
      materialui_font_data_t hint;
   } font_data;

   /* Pointer info */
   menu_input_pointer_t pointer;
   int16_t pointer_start_x;
   int16_t pointer_start_y;
   float pointer_start_scroll_y;

   /* Colour theme parameters */
   enum materialui_color_theme color_theme;
   materialui_colors_t colors;

   /* Cached system bar data */
   materialui_sys_bar_cache_t sys_bar_cache;

   /* Use common tickers for all text
    * > Simplifies configuration and
    *   improves performance */
   bool use_smooth_ticker;
   menu_animation_ctx_ticker_t ticker;
   menu_animation_ctx_ticker_smooth_t ticker_smooth;
   unsigned ticker_x_offset;
   unsigned ticker_str_width;

   /* Touch feedback animation parameters */
   unsigned touch_feedback_selection;
   float touch_feedback_alpha;
   bool touch_feedback_update_selection;

   /* Menu transition animation parameters */
   float transition_alpha;
   float transition_x_offset;
   size_t last_stack_size;

   /* Thumbnail helpers */
   menu_thumbnail_path_data_t *thumbnail_path_data;
   unsigned thumbnail_width_max;
   unsigned thumbnail_height_max;
   bool primary_thumbnail_available;
   bool secondary_thumbnail_enabled;
   bool show_fullscreen_thumbnails;
   size_t fullscreen_thumbnail_selection;
   float fullscreen_thumbnail_alpha;
   char fullscreen_thumbnail_label[255];

   enum materialui_list_view_type list_view_type;

} materialui_handle_t;

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
            current_theme->scrollbar,
            mui->colors.scrollbar, 1.0f);
   hex32_to_rgba_normalized(
            current_theme->divider,
            mui->colors.divider, 1.0f);
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
      if (!menu_display_reset_textures_list(
            materialui_texture_path(i), icon_path, &mui->textures.list[i],
            TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
      {
         RARCH_WARN("[GLUI] Asset missing: %s%s%s\n", icon_path, path_default_slash(), materialui_texture_path(i));
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
      video_frame_info_t *video_info,
      unsigned icon_size,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color)
{
   menu_display_ctx_rotate_draw_t rotate_draw;
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;

   menu_display_blend_begin(video_info);

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   draw.x               = x;
   draw.y               = height - y - icon_size;
   draw.width           = icon_size;
   draw.height          = icon_size;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   menu_display_draw(&draw, video_info);
   menu_display_blend_end(video_info);
}

static void materialui_draw_thumbnail(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      menu_thumbnail_t *thumbnail,
      float x, float y,
      unsigned width, unsigned height,
      float scale_factor)
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
   if (thumbnail->status == MENU_THUMBNAIL_STATUS_MISSING)
   {
      float icon_size;

      /* Adjust icon size based on scale factor */
      if (scale_factor >= 1.0f)
         icon_size = (float)mui->icon_size;
      else
         icon_size = (float)mui->icon_size * scale_factor;

      /* Background */
      menu_display_set_alpha(
            mui->colors.thumbnail_background,
            mui->transition_alpha);

      menu_display_draw_quad(
            video_info,
            bg_x,
            bg_y,
            (unsigned)bg_width,
            (unsigned)bg_height,
            width,
            height,
            mui->colors.thumbnail_background);

      /* Icon */
      materialui_draw_icon(video_info,
            (unsigned)icon_size,
            mui->textures.list[MUI_TEXTURE_IMAGE],
            bg_x + (bg_width - icon_size) / 2.0f,
            bg_y + (bg_height - icon_size) / 2.0f,
            width,
            height,
            0.0f,
            1.0f,
            mui->colors.missing_thumbnail_icon);
   }
   /* If thumbnail is available, draw it
    * > Note that other conditions are ignored - i.e. we
    *   we draw nothing if thumbnail status is unknown,
    *   or we are waiting for a thumbnail to load) */
   else if (thumbnail->status == MENU_THUMBNAIL_STATUS_AVAILABLE)
   {
      settings_t *settings = config_get_ptr();

      if (!settings)
         return;

      /* Background */
      if (settings->bools.menu_materialui_thumbnail_background_enable)
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
         menu_display_set_alpha(
               mui->colors.thumbnail_background,
               mui->transition_alpha * thumbnail->alpha);

         /* > Draw background quad */
         menu_display_draw_quad(
               video_info,
               (int)bg_x,
               (int)bg_y,
               (unsigned)(bg_width + 0.5f),
               (unsigned)(bg_height + 1.5f),
               width,
               height,
               mui->colors.thumbnail_background);
      }

      /* Thumbnail */
      menu_thumbnail_draw(
            video_info, thumbnail,
            x, y, mui->thumbnail_width_max, mui->thumbnail_height_max,
            MENU_THUMBNAIL_ALIGN_CENTRE,
            mui->transition_alpha, scale_factor, NULL);
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

static void materialui_render_messagebox(materialui_handle_t *mui,
      video_frame_info_t *video_info, int y_centre, const char *message)
{
   unsigned i;
   int x                    = 0;
   int y                    = 0;
   int longest_width        = 0;
   size_t longest_len       = 0;
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   struct string_list *list = NULL;

   /* Sanity check */
   if (!mui || !mui->font_data.list.font)
      goto end;

   /* Split message into lines */
   list = string_split(message, "\n");

   if (!list || list->elems == 0)
      goto end;

   /* Get coordinates of message box centre */
   x = width / 2;
   y = (int)(y_centre - (list->size - 1) * (mui->font_data.list.font_height / 2));

   /* TODO/FIXME: Reduce text scale if width or height
    * are too large to fit on screen */

   /* Find the longest line width */
   for (i = 0; i < list->size; i++)
   {
      const char *line = list->elems[i].data;

      if (!string_is_empty(line))
      {
         size_t len = utf8len(line);

         if (len > longest_len)
         {
            longest_len   = len;
            longest_width = font_driver_get_message_width(
                  mui->font_data.list.font, line, (unsigned)strlen(line), 1);
         }
      }
   }

   /* Draw message box background */
   menu_display_set_alpha(
         mui->colors.surface_background, mui->transition_alpha);

   menu_display_draw_quad(
         video_info,
         x - longest_width / 2.0 - mui->margin * 2.0,
         y -  mui->font_data.list.font_height / 2.0 -  mui->margin * 2.0,
         longest_width + mui->margin * 4.0,
         mui->font_data.list.font_height * list->size + mui->margin * 4.0,
         width,
         height,
         mui->colors.surface_background);

   /* Print each line of the message */
   for (i = 0; i < list->size; i++)
   {
      const char *line = list->elems[i].data;

      if (!string_is_empty(line))
         menu_display_draw_text(
               mui->font_data.list.font, line,
               x - longest_width/2.0,
               y + i *  mui->font_data.list.font_height + mui->font_data.list.font_height / 3,
               width, height, mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0, true);
   }

end:
   if (list)
      string_list_free(list);
}

/* Used for the sublabels */
static unsigned materialui_count_lines(const char *str)
{
   unsigned c     = 0;
   unsigned lines = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

/* Initialises scrollbar parameters (width/height) */
static void materialui_scrollbar_init(
      materialui_handle_t* mui,
      unsigned width, unsigned height, unsigned header_height)
{
   int view_height = (int)height - (int)header_height - (int)mui->nav_bar_layout_height;
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

/* Calculate physical size of each menu entry, plus
 * any derived screen dimensions of menu list elements */
static void materialui_compute_entries_box(
      materialui_handle_t* mui,
      unsigned width, unsigned height, unsigned header_height)
{
   unsigned i;
   int usable_width     =
            (int)width - (int)(mui->margin * 2) - (int)(mui->landscape_entry_margin * 2) - (int)mui->nav_bar_layout_width;
   file_list_t *list    = menu_entries_get_selection_buf_ptr(0);
   float sum            = 0;
   size_t entries_end   = menu_entries_get_size();

   /* If this is a playlist and the 'dual icon'
    * thumbnail view mode is enabled, each node has
    * a fixed height - and we can skip all the
    * usual calculations */
   if (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON)
   {
      /* One line of list text */
      float node_text_height  = (float)mui->font_data.list.font_height;
      /* List text + thumbnail height + padding */
      float node_entry_height =
            node_text_height + (float)mui->thumbnail_height_max +
            ((float)mui->dip_base_unit_size / 5.0f);

      for (i = 0; i < entries_end; i++)
      {
         materialui_node_t *node = (materialui_node_t*)
                  file_list_get_userdata_at_offset(list, i);

         node->text_height  = node_text_height;
         node->entry_height = node_entry_height;
         node->y            = sum;
         sum               += node_entry_height;
      }

      mui->content_height = sum;

      /* Total height is now known - can initialise scrollbar */
      materialui_scrollbar_init(mui, width, height, header_height);

      return;
   }

   /* If this is a playlist and any other thumbnail view
    * mode is enabled, must reduce usable width by
    * thumbnail width */
   if ((mui->list_view_type != MUI_LIST_VIEW_DEFAULT) &&
       (mui->list_view_type != MUI_LIST_VIEW_PLAYLIST))
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
         if (mui->landscape_entry_margin < mui->margin)
            thumbnail_margin = (int)(mui->margin - mui->landscape_entry_margin);

      usable_width -= mui->thumbnail_width_max + thumbnail_margin;

      /* Account for second thumbnail, if enabled */
      if (mui->secondary_thumbnail_enabled)
         usable_width -= mui->thumbnail_width_max + thumbnail_margin;
      /* If there is no second thumbnail and landscape
       * optimisations are active, increase width to
       * balance x offset of primary thumbnail */
      else if (mui->landscape_entry_margin > 0)
         usable_width += (mui->margin > mui->landscape_entry_margin) ?
               (int)mui->landscape_entry_margin : (int)mui->margin;
   }

   for (i = 0; i < entries_end; i++)
   {
      menu_entry_t entry;
      char wrapped_sublabel_str[MENU_SUBLABEL_MAX_LENGTH];
      const char *sublabel_str      = NULL;
      unsigned num_sublabel_lines   = 0;
      materialui_node_t *node       = (materialui_node_t*)
         file_list_get_userdata_at_offset(list, i);

      wrapped_sublabel_str[0] = '\0';

      menu_entry_init(&entry);
      entry.path_enabled       = false;
      entry.label_enabled      = false;
      entry.rich_label_enabled = false;
      entry.value_enabled      = false;
      menu_entry_get(&entry, 0, i, NULL, true);

      menu_entry_get_sublabel(&entry, &sublabel_str);

      if (!string_is_empty(sublabel_str))
      {
         int sublabel_width_max = usable_width;

         /* If this is a default menu list with an icon,
          * must subtract icon size from sublabel width */
         if (mui->list_view_type == MUI_LIST_VIEW_DEFAULT)
            if (node->has_icon)
               if (mui->textures.list[node->icon_texture_index])
                  sublabel_width_max -= (int)mui->icon_size;

         word_wrap(
               wrapped_sublabel_str, sublabel_str,
               sublabel_width_max / (int)mui->font_data.hint.glyph_width,
               false, 0);

         num_sublabel_lines = materialui_count_lines(wrapped_sublabel_str);
      }

      node->text_height  = mui->font_data.list.font_height +
            (num_sublabel_lines * mui->font_data.hint.font_height);

      node->entry_height = node->text_height +
            mui->dip_base_unit_size / 10;

      /* If this is a playlist and thumbnails are enabled,
       * must ensure that line_height is greater than
       * maximum thumbnail height */
      if ((mui->list_view_type != MUI_LIST_VIEW_DEFAULT) &&
          (mui->list_view_type != MUI_LIST_VIEW_PLAYLIST))
         node->entry_height = (node->entry_height < mui->thumbnail_height_max) ?
               mui->thumbnail_height_max : node->entry_height;

      node->entry_height      += mui->dip_base_unit_size / 10;
      node->y                  = sum;
      sum                     += node->entry_height;
   }

   mui->content_height = sum;

   /* Total height is now known - can initialise scrollbar */
   materialui_scrollbar_init(mui, width, height, header_height);
}

/* Compute the scroll value depending on the highlighted entry */
static float materialui_get_scroll(materialui_handle_t *mui)
{
   file_list_t *list       = menu_entries_get_selection_buf_ptr(0);
   materialui_node_t *node = NULL;
   size_t selection        = menu_navigation_get_selection();
   unsigned header_height  = menu_display_get_header_height();
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
   view_centre =
         (float)(height - header_height - mui->nav_bar_layout_height) / 2.0f;

   /* Get the vertical midpoint of the currently
    * selected entry */

   /* > Account for entries *before* current selection */
   for (i = 0; i < selection; i++)
   {
      node = (materialui_node_t*)file_list_get_userdata_at_offset(list, i);

      /* If this ever happens, the scroll position
       * will be entirely wrong... */
      if (!node)
         continue;

      selection_centre += node->entry_height;
   }

   /* > Account for current selection */
   node = (materialui_node_t*)file_list_get_userdata_at_offset(list, selection);
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
 * > Returns index of selected item */
static size_t materialui_auto_select_onscreen_entry(
      materialui_handle_t *mui,
      enum materialui_onscreen_entry_position_type target_entry)
{
   size_t selection = menu_navigation_get_selection();

   /* Check whether selected item is already on screen */
   if (materialui_entry_onscreen(mui, selection))
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

static void materialui_layout(
      materialui_handle_t *mui, bool video_is_threaded);

/* Called on each frame. We use this callback to:
 * - Determine current scroll position
 * - Determine index of first/last onscreen entries
 * - Handle dynamic pointer input
 * - Handle streaming thumbnails */
static void materialui_render(void *data,
      unsigned width, unsigned height,
      bool is_idle)
{
   settings_t *settings     = config_get_ptr();
   materialui_handle_t *mui = (materialui_handle_t*)data;
   unsigned header_height   = menu_display_get_header_height();
   size_t entries_end       = menu_entries_get_size();
   file_list_t *list        = menu_entries_get_selection_buf_ptr(0);
   bool first_entry_found   = false;
   bool last_entry_found    = false;
   size_t i;
   float bottom;
   float scale_factor;

   if (!settings || !mui || !list)
      return;

   /* Check whether screen dimensions, menu scale
    * factor or layout optimisation settings have changed */
   scale_factor = menu_display_get_dpi_scale(width, height);

   if ((scale_factor != mui->last_scale_factor) ||
       (width != mui->last_width) ||
       (height != mui->last_height) ||
       ((enum materialui_landscape_layout_optimization_type)
            settings->uints.menu_materialui_landscape_layout_optimization !=
                  mui->last_landscape_layout_optimization) ||
       (settings->bools.menu_materialui_auto_rotate_nav_bar != mui->last_auto_rotate_nav_bar))
   {
      mui->dip_base_unit_size                 = scale_factor * MUI_DIP_BASE_UNIT_SIZE;
      mui->last_scale_factor                  = scale_factor;
      mui->last_width                         = width;
      mui->last_height                        = height;
      mui->last_landscape_layout_optimization =
            (enum materialui_landscape_layout_optimization_type)
                  settings->uints.menu_materialui_landscape_layout_optimization;
      mui->last_auto_rotate_nav_bar           = settings->bools.menu_materialui_auto_rotate_nav_bar;

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
      materialui_layout(mui, video_driver_is_threaded());
      video_driver_monitor_reset();
   }

   if (mui->need_compute)
   {
      menu_animation_ctx_tag tag = (uintptr_t)&mui->scroll_y;

      if (mui->font_data.list.font && mui->font_data.hint.font)
         materialui_compute_entries_box(mui, width, height, header_height);

      /* After calling populate_entries(), we need to call
       * materialui_get_scroll() so the last selected item
       * is correctly displayed on screen.
       * But we can't do this until materialui_compute_entries_box()
       * has been called, so we delay it until here, when
       * mui->need_compute is acted upon. */

      /* Kill any existing scroll animation */
      menu_animation_kill_by_tag(&tag);

      /* Reset scroll acceleration */
      menu_input_set_pointer_y_accel(0.0f);

      /* Get new scroll position */
      mui->scroll_y     = materialui_get_scroll(mui);
      mui->need_compute = false;
   }

   /* Need to update this each frame, otherwise touchscreen
    * input breaks when changing orientation */
   menu_display_set_width(width);
   menu_display_set_height(height);

   /* Read pointer state */
   menu_input_get_pointer_state(&mui->pointer);

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
         float view_height  = (float)height - (float)header_height - (float)mui->nav_bar_layout_height;
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

   bottom = mui->content_height - (float)height + (float)header_height + (float)mui->nav_bar_layout_height;
   if (mui->scroll_y > bottom)
      mui->scroll_y = bottom;

   if (mui->content_height < (height - header_height - mui->nav_bar_layout_height))
      mui->scroll_y = 0.0f;

   /* Loop over all entries */
   mui->first_onscreen_entry = 0;
   mui->last_onscreen_entry  = (entries_end > 0) ? entries_end - 1 : 0;

   for (i = 0; i < entries_end; i++)
   {
      materialui_node_t *node = (materialui_node_t*)
            file_list_get_userdata_at_offset(list, i);
      int entry_y;

      /* Sanity check */
      if (!node)
         break;

      /* Get current entry y position */
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
         if (entry_y > ((int)height - (int)mui->nav_bar_layout_height))
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
         if ((pointer_x >  mui->landscape_entry_margin) &&
             (pointer_x <  width - mui->landscape_entry_margin - mui->nav_bar_layout_width) &&
             (pointer_y >= header_height) &&
             (pointer_y <= height - mui->nav_bar_layout_height))
         {
            /* Check if pointer is within the bounds of the
             * current entry */
            if ((pointer_y > entry_y) &&
                (pointer_y < (entry_y + node->entry_height)))
            {
               /* Pointer selection is always updated */
               menu_input_set_pointer_selection(i);

               /* If pointer is pressed and stationary... */
               if (mui->pointer.pressed && !mui->pointer.dragged)
               {
                  /* ...check whether feedback selection updates
                   * are enabled... */
                  if (mui->touch_feedback_update_selection)
                  {
                     /* ...apply touch feedback to current entry */
                     mui->touch_feedback_selection = i;

                     /* ...and if pointer has been held for at least
                      * MENU_INPUT_PRESS_TIME_SHORT ms, automatically
                      * select current entry */
                     if (mui->pointer.press_duration >= MENU_INPUT_PRESS_TIME_SHORT)
                     {
                        menu_navigation_set_selection(i);

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

      /* If this is a playlist and thumbnails are enabled,
       * have to load thumbnails for all on-screen entries
       * and free thumbnails for all off-screen entries */
      if ((mui->list_view_type != MUI_LIST_VIEW_DEFAULT) &&
          (mui->list_view_type != MUI_LIST_VIEW_PLAYLIST))
      {
         bool on_screen = first_entry_found && !last_entry_found;

         if (mui->secondary_thumbnail_enabled)
            menu_thumbnail_process_streams(
               mui->thumbnail_path_data, mui->playlist, i,
               &node->thumbnails.primary, &node->thumbnails.secondary,
               on_screen,
               settings->uints.menu_thumbnail_upscale_threshold,
               settings->bools.network_on_demand_thumbnails);
         else
            menu_thumbnail_process_stream(
                  mui->thumbnail_path_data, MENU_THUMBNAIL_RIGHT,
                  mui->playlist, i, &node->thumbnails.primary, on_screen,
                  settings->uints.menu_thumbnail_upscale_threshold,
                  settings->bools.network_on_demand_thumbnails);
      }
      else if (last_entry_found)
         break;
   }

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &mui->first_onscreen_entry);
}

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
                * an achive file... */
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
      video_frame_info_t *video_info,
      float y,
      unsigned width, unsigned height, int x_offset,
      bool on)
{
   unsigned switch_texture_index = on ?
         MUI_TEXTURE_SWITCH_ON : MUI_TEXTURE_SWITCH_OFF;
   float *bg_color               = on ?
         mui->colors.list_switch_on_background : mui->colors.list_switch_off_background;
   float *switch_color           = on ?
         mui->colors.list_switch_on : mui->colors.list_switch_off;
   int x                         =
         x_offset + (int)width - (int)mui->margin - (int)mui->landscape_entry_margin -
               (int)mui->nav_bar_layout_width - (int)mui->icon_size;

   /* Draw background */
   if (mui->textures.list[MUI_TEXTURE_SWITCH_BG])
      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_SWITCH_BG],
            x,
            y,
            width,
            height,
            0,
            1,
            bg_color);

   /* Draw switch */
   if (mui->textures.list[switch_texture_index])
      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[switch_texture_index],
            x,
            y,
            width,
            height,
            0,
            1,
            switch_color);
}

/* Draws specified menu entry
 * > Used for standard, non-playlist entries
 * >> MUI_LIST_VIEW_DEFAULT */
static void materialui_render_menu_entry_default(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      materialui_node_t *node,
      menu_entry_t *entry,
      bool entry_selected,
      bool touch_feedback_active,
      unsigned header_height,
      unsigned width, unsigned height,
      int x_offset)
{
   const char *entry_value                           = NULL;
   const char *entry_label                           = NULL;
   const char *entry_sublabel                        = NULL;
   unsigned entry_type                               = 0;
   enum materialui_entry_value_type entry_value_type = MUI_ENTRY_VALUE_NONE;
   unsigned entry_value_width                        = 0;
   enum msg_file_type entry_file_type                = FILE_TYPE_NONE;
   int entry_y                                       = header_height - mui->scroll_y + node->y;
   int entry_margin                                  = (int)mui->margin + (int)mui->landscape_entry_margin;
   int usable_width                                  =
         (int)width - (int)(mui->margin * 2) - (int)(mui->landscape_entry_margin * 2) - (int)mui->nav_bar_layout_width;
   int label_y                                       = 0;
   int value_icon_y                                  = 0;
   uintptr_t icon_texture                            = 0;
   bool draw_text_outside                            = (x_offset != 0);

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
   menu_entry_get_rich_label(entry, &entry_label);
   menu_entry_get_value(entry, &entry_value);
   menu_entry_get_sublabel(entry, &entry_sublabel);
   entry_type = menu_entry_get_type_new(entry);

   entry_file_type = msg_hash_to_file_type(msg_hash_calculate(entry_value));
   entry_value_type = materialui_get_entry_value_type(
         mui, entry_value, entry->checked, entry_type, entry_file_type);

   /* Draw entry icon
    * > Has to be done first, since it affects the left
    *   hand margin size for label + sublabel text */
   if (node->has_icon)
   {
      if (entry->checked &&
          ((entry_type >= MENU_SETTING_DROPDOWN_ITEM) &&
           (entry_type <= MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL)))
         node->has_icon = false;
      else
         icon_texture = mui->textures.list[node->icon_texture_index];
   }
   else
   {
      switch (entry_file_type)
      {
         case FILE_TYPE_COMPRESSED:
            /* Note that we have to perform a backup check here,
             * since the 'manual content scan - file extensions'
             * setting may have a value of 'zip' or '7z' etc, which
             * means it would otherwise get incorreclty identified as
             * an achive file... */
            if (entry_type == FILE_TYPE_CARCHIVE)
               icon_texture = mui->textures.list[MUI_TEXTURE_ARCHIVE];
            break;
         case FILE_TYPE_IMAGE:
            icon_texture = mui->textures.list[MUI_TEXTURE_IMAGE];
            break;
         default:
            break;
      }
   }

   if (icon_texture)
   {
      materialui_draw_icon(video_info,
            mui->icon_size,
            (uintptr_t)icon_texture,
            x_offset + (int)mui->landscape_entry_margin,
            entry_y + (node->entry_height / 2.0f) - (mui->icon_size / 2.0f),
            width,
            height,
            0,
            1,
            mui->colors.list_icon);

      entry_margin += mui->icon_size;
      usable_width -= mui->icon_size;
   }

   /* Draw entry sublabel
    * > Must be done before label + value, since it
    *   affects y offset positions */
   if (!string_is_empty(entry_sublabel))
   {
      int sublabel_y =
            entry_y + (int)(mui->dip_base_unit_size / 5.0f) +
                  (int)mui->font_data.list.font_height;
      char wrapped_sublabel[MENU_SUBLABEL_MAX_LENGTH];

      wrapped_sublabel[0] = '\0';

      /* Wrap sublabel string */
      word_wrap(wrapped_sublabel, entry_sublabel,
            (int)(usable_width / mui->font_data.hint.glyph_width),
            true, 0);

      /* Draw sublabel string
       * > Note: We must allow text to be drawn off-screen
       *   if the current y position is negative, otherwise topmost
       *   entries with very long sublabels may get 'clipped' too
       *   early as they are scrolled upwards beyond the top edge
       *   of the screen */
      menu_display_draw_text(mui->font_data.hint.font, wrapped_sublabel,
            x_offset + entry_margin,
            sublabel_y,
            width, height,
            (entry_selected || touch_feedback_active) ?
                  mui->colors.list_hint_text_highlighted : mui->colors.list_hint_text,
            TEXT_ALIGN_LEFT, 1.0f, false, 0, draw_text_outside || (sublabel_y < 0));

      /* If we have a sublabel, entry label y position has a
       * fixed vertical offset */
      label_y      = entry_y + (int)(mui->dip_base_unit_size / 5.0f);
      value_icon_y = entry_y + (int)((mui->dip_base_unit_size / 6.0f) - (mui->icon_size / 2.0f));
   }
   else
   {
      /* If we don't have a sublabel, entry label is drawn
       * at the vertical centre of the current node
       * Note: Text is drawn relative to the baseline,
       * so we can't do this accurately - but as a general
       * rule of thumb, the descender of a font is at least
       * 20% of it's height - so we just add (font_height / 5) */
      label_y      = entry_y + (int)((node->entry_height / 2.0f) + (mui->font_data.list.font_height / 5.0f));
      value_icon_y = entry_y + (int)((node->entry_height / 2.0f) - (mui->icon_size / 2.0f));
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

               if (menu_animation_ticker_smooth(&mui->ticker_smooth))
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

               menu_animation_ticker(&mui->ticker);

               /* Get effective width of value string
                * > Approximate value - only the smooth ticker
                *   returns the actual width in pixels, and any
                *   platform too slow to run the smooth ticker
                *   won't appreciate the overheads of using
                *   font_driver_get_message_width() here... */
               entry_value_width = (entry_value_len + 1) * mui->font_data.list.glyph_width;
            }

            /* Draw value string */
            menu_display_draw_text(mui->font_data.list.font, value_buf,
                  x_offset + value_x_offset + (int)width - (int)mui->margin - (int)mui->landscape_entry_margin - (int)mui->nav_bar_layout_width,
                  label_y,
                  width, height,
                  (entry_selected || touch_feedback_active) ?
                        mui->colors.list_text_highlighted : mui->colors.list_text,
                  TEXT_ALIGN_RIGHT, 1.0f, false, 0, draw_text_outside);
         }
         break;
      case MUI_ENTRY_VALUE_SWITCH_ON:
         {
            materialui_render_switch_icon(
                  mui, video_info, value_icon_y, width, height, x_offset, true);
            entry_value_width = mui->icon_size;
         }
         break;
      case MUI_ENTRY_VALUE_SWITCH_OFF:
         {
            materialui_render_switch_icon(
                  mui, video_info, value_icon_y, width, height, x_offset, false);
            entry_value_width = mui->icon_size;
         }
         break;
      case MUI_ENTRY_VALUE_CHECKMARK:
         {
            /* Draw checkmark */
            if (mui->textures.list[MUI_TEXTURE_CHECKMARK])
               materialui_draw_icon(video_info,
                     mui->icon_size,
                     mui->textures.list[MUI_TEXTURE_CHECKMARK],
                     x_offset + (int)width - (int)mui->margin - (int)mui->landscape_entry_margin - (int)mui->nav_bar_layout_width - (int)mui->icon_size,
                     value_icon_y,
                     width,
                     height,
                     0,
                     1,
                     mui->colors.list_switch_on);

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

            menu_animation_ticker_smooth(&mui->ticker_smooth);
         }
         else
         {
            /* Label */
            mui->ticker.s        = label_buf;
            mui->ticker.len      = (size_t)(label_width / mui->font_data.list.glyph_width);
            mui->ticker.str      = entry_label;

            menu_animation_ticker(&mui->ticker);
         }

         /* Draw label string */
         menu_display_draw_text(mui->font_data.list.font, label_buf,
               x_offset + (int)mui->ticker_x_offset + entry_margin,
               label_y,
               width, height,
               (entry_selected || touch_feedback_active) ?
                     mui->colors.list_text_highlighted : mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0, draw_text_outside);
      }
   }
}

/* Draws specified menu entry
 * > Used for playlist 'list view' (with and without
 *   thumbnails) entries
 * >> MUI_LIST_VIEW_PLAYLIST
 * >> MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL
 * >> MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM
 * >> MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE */
static void materialui_render_menu_entry_playlist_list(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      materialui_node_t *node,
      menu_entry_t *entry,
      bool entry_selected,
      bool touch_feedback_active,
      unsigned header_height,
      unsigned width, unsigned height,
      int x_offset)
{
   const char *entry_label    = NULL;
   const char *entry_sublabel = NULL;
   int entry_y                = header_height - mui->scroll_y + node->y;
   int entry_margin           = (int)mui->margin + (int)mui->landscape_entry_margin;
   int usable_width           =
         (int)width - (int)(mui->margin * 2) - (int)(mui->landscape_entry_margin * 2) - (int)mui->nav_bar_layout_width;
   /* The label + sublabel text block is always drawn at
    * the vertical centre of the current node
    * Note: Text is drawn relative to the baseline,
    * so we can't do this accurately - but tests
    * indicate that the ascender of Material UI's
    * font accounts for 7/20 of its height, so we
    * just add (13/20) * font_height */
   int label_y                =
         entry_y + ((float)(node->entry_height - node->text_height) / 2.0f) +
         (13.0f * (float)mui->font_data.list.font_height / 20.0f);
   bool draw_text_outside     = (x_offset != 0);

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
   menu_entry_get_rich_label(entry, &entry_label);
   menu_entry_get_sublabel(entry, &entry_sublabel);

   /* Draw entry thumbnail(s)
    * > Has to be done first, since it affects the left
    *   hand margin size and total width for label +
    *   sublabel text */
   if (mui->list_view_type != MUI_LIST_VIEW_PLAYLIST)
   {
      int thumbnail_margin = 0;
      float thumbnail_y    =
            (float)entry_y + ((float)node->entry_height / 2.0f) - ((float)mui->thumbnail_height_max / 2.0f);

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
       * appearance by adding left/right margins - but
       * we only need to do this if landscape optimisations
       * are disabled (or landscape_entry_margin is less
       * than mui->margin) */
      else
         if (mui->landscape_entry_margin < mui->margin)
            thumbnail_margin = (int)(mui->margin - mui->landscape_entry_margin);

      /* Draw primary thumbnail */
      materialui_draw_thumbnail(
            mui,
            video_info,
            &node->thumbnails.primary,
            (float)(x_offset + thumbnail_margin + (int)mui->landscape_entry_margin),
            thumbnail_y,
            width,
            height,
            1.0f);

      entry_margin += mui->thumbnail_width_max + thumbnail_margin;
      usable_width -= mui->thumbnail_width_max + thumbnail_margin;

      /* Draw secondary thumbnail, if required */
      if (mui->secondary_thumbnail_enabled)
      {
         materialui_draw_thumbnail(
               mui,
               video_info,
               &node->thumbnails.secondary,
               (float)(x_offset + (int)width - thumbnail_margin - (int)mui->landscape_entry_margin -
                     (int)mui->nav_bar_layout_width - (int)mui->thumbnail_width_max),
               thumbnail_y,
               width,
               height,
               1.0f);

         usable_width -= mui->thumbnail_width_max + thumbnail_margin;
      }
      /* If there is no secondary thumbnail and landscape
       * optimisations are active, must increase usable width
       * to balance out the margin of the primary thumbnail
       * (since this is effectively left shifted when landscape
       * optimisations are active...) */
      else if (mui->landscape_entry_margin > 0)
         usable_width += (mui->margin > mui->landscape_entry_margin) ?
               (int)mui->landscape_entry_margin : (int)mui->margin;
   }

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

            menu_animation_ticker_smooth(&mui->ticker_smooth);
         }
         else
         {
            /* Label */
            mui->ticker.s        = label_buf;
            mui->ticker.len      = (size_t)(usable_width / mui->font_data.list.glyph_width);
            mui->ticker.str      = entry_label;

            menu_animation_ticker(&mui->ticker);
         }

         /* Draw label string */
         menu_display_draw_text(mui->font_data.list.font, label_buf,
               x_offset + (int)mui->ticker_x_offset + entry_margin,
               label_y,
               width, height,
               (entry_selected || touch_feedback_active) ?
                     mui->colors.list_text_highlighted : mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0, draw_text_outside);
      }
   }

   /* Draw entry sublabel */
   if (!string_is_empty(entry_sublabel))
   {
      int sublabel_y = label_y + (int)mui->font_data.list.font_height;
      char wrapped_sublabel[MENU_SUBLABEL_MAX_LENGTH];

      wrapped_sublabel[0] = '\0';

      /* Wrap sublabel string */
      word_wrap(wrapped_sublabel, entry_sublabel,
            (int)(usable_width / mui->font_data.hint.glyph_width),
            true, 0);

      /* Draw sublabel string
       * > Note: We must allow text to be drawn off-screen
       *   if the current y position is negative, otherwise topmost
       *   entries with very long sublabels may get 'clipped' too
       *   early as they are scrolled upwards beyond the top edge
       *   of the screen */
      menu_display_draw_text(mui->font_data.hint.font, wrapped_sublabel,
            x_offset + entry_margin,
            sublabel_y,
            width, height,
            (entry_selected || touch_feedback_active) ?
                  mui->colors.list_hint_text_highlighted : mui->colors.list_hint_text,
            TEXT_ALIGN_LEFT, 1.0f, false, 0, draw_text_outside || (sublabel_y < 0));
   }

   /* When using the larger thumbnail views, the horizontal
    * text area is unpleasantly vacuous, such that the
    * label + sublabel strings float in a sea of nothingness.
    * We can partially mitigate the visual 'emptiness' of this
    * by drawing a divider between entries. This is particularly
    * beneficial when dual thumbnails are enabled, since it
    * 'ties' the left/right thumbnails together
    * NOTE: This does not work at all for the smallest thumbnail
    * list view, or when thumbnails are disabled - the result is
    * just too 'busy'/visually cluttered */
   if ((mui->list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM) ||
       (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE))
   {
      if (usable_width > 0)
         menu_display_draw_quad(
               video_info,
               (float)(x_offset + entry_margin),
               entry_y + (float)node->entry_height,
               (unsigned)usable_width,
               mui->entry_divider_width,
               width,
               height,
               mui->colors.divider);
   }
}

/* Draws specified menu entry
 * > Used for playlist 'dual icon' entries
 * >> MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON */
static void materialui_render_menu_entry_playlist_dual_icon(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      materialui_node_t *node,
      menu_entry_t *entry,
      bool entry_selected,
      bool touch_feedback_active,
      unsigned header_height,
      unsigned width, unsigned height,
      int x_offset)
{
   const char *entry_label = NULL;
   int usable_width        =
         (int)width - (int)(mui->margin * 2) -
         (int)(mui->landscape_entry_margin * 2) - (int)mui->nav_bar_layout_width;
   float entry_y           = (float)header_height - mui->scroll_y + node->y;
   float thumbnail_y;

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
   menu_entry_get_rich_label(entry, &entry_label);

   /* Draw thumbnails
    * > These go at the top of the entry, with a
    *   small vertical margin */
   thumbnail_y           =
         entry_y + ((float)mui->dip_base_unit_size / 10.0f);

   /* > Primary thumbnail */
   materialui_draw_thumbnail(
         mui,
         video_info,
         &node->thumbnails.primary,
         (float)(x_offset + (int)mui->margin + (int)mui->landscape_entry_margin),
         thumbnail_y,
         width,
         height,
         1.0f);

   /* > Secondary thumbnail */
   materialui_draw_thumbnail(
         mui,
         video_info,
         &node->thumbnails.secondary,
         (float)(x_offset + (int)width - (int)mui->margin - (int)mui->landscape_entry_margin -
               (int)mui->nav_bar_layout_width - (int)mui->thumbnail_width_max),
         thumbnail_y,
         width,
         height,
         1.0f);

   /* Draw entry label */
   if (!string_is_empty(entry_label))
   {
      float label_x          = 0.0f;
      /* Label is drawn beneath thumbnails,
       * with a small vertical margin */
      float label_y          =
            thumbnail_y + (float)mui->thumbnail_height_max +
            ((float)mui->dip_base_unit_size / 10.0f) +
            (9.0f * (float)mui->font_data.list.font_height / 20.0f);

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
            if (!menu_animation_ticker_smooth(&mui->ticker_smooth))
               label_x = (float)(usable_width - mui->ticker_str_width) / 2.0f;
         }
         else
         {
            /* Label */
            mui->ticker.s   = label_buf;
            mui->ticker.len = (size_t)(usable_width / mui->font_data.list.glyph_width);
            mui->ticker.str = entry_label;

            /* If ticker is inactive, centre the text */
            if (!menu_animation_ticker(&mui->ticker))
            {
               int str_width = (int)(utf8len(label_buf) *
                  mui->font_data.list.glyph_width);

               label_x = (float)(usable_width - str_width) / 2.0f;
            }
         }

         label_x += (float)(x_offset + (int)mui->ticker_x_offset +
               (int)mui->margin + (int)mui->landscape_entry_margin);

         /* Draw label string */
         menu_display_draw_text(mui->font_data.list.font, label_buf,
               label_x,
               label_y,
               width, height,
               (entry_selected || touch_feedback_active) ?
                     mui->colors.list_text_highlighted : mui->colors.list_text,
               TEXT_ALIGN_LEFT, 1.0f, false, 0, draw_text_outside);
      }
   }

   /* Draw divider */
   if (usable_width > 0)
      menu_display_draw_quad(
            video_info,
            (float)(x_offset + (int)mui->margin + (int)mui->landscape_entry_margin),
            thumbnail_y + (float)mui->thumbnail_height_max +
                  ((float)mui->dip_base_unit_size / 10.0f) +
                  (float)mui->font_data.list.font_height,
            (unsigned)usable_width,
            mui->entry_divider_width,
            width,
            height,
            mui->colors.divider);
}

static void materialui_render_scrollbar(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned width, unsigned height)
{
   /* Do nothing if scrollbar is disabled */
   if (!mui->scrollbar.active)
      return;

   /* Draw scrollbar */
   menu_display_draw_quad(
         video_info,
         mui->scrollbar.x,
         mui->scrollbar.y,
         mui->scrollbar.width,
         mui->scrollbar.height,
         width, height,
         mui->colors.scrollbar);
}

/* Draws current menu list */
static void materialui_render_menu_list(
      materialui_handle_t *mui,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      int x_offset)
{
   size_t i;
   size_t first_entry;
   size_t last_entry;
   file_list_t *list           = NULL;
   size_t entries_end          = menu_entries_get_size();
   unsigned header_height      = menu_display_get_header_height();
   size_t selection            = menu_navigation_get_selection();
   bool touch_feedback_enabled =
         !mui->scrollbar.dragged &&
         !mui->show_fullscreen_thumbnails &&
         (mui->touch_feedback_alpha >= 0.5f) &&
         (mui->touch_feedback_selection == menu_input_get_pointer_selection());

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
      materialui_node_t *node    = (materialui_node_t*)file_list_get_userdata_at_offset(list, i);
      menu_entry_t entry;

      /* Sanity check */
      if (!node)
         break;

      /* Get current entry */
      menu_entry_init(&entry);
      entry.path_enabled     = false;
      entry.value_enabled    = (mui->list_view_type == MUI_LIST_VIEW_DEFAULT);
      entry.sublabel_enabled = (mui->list_view_type != MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON);
      menu_entry_get(&entry, 0, i, NULL, true);

      /* Render label, value, and associated icons
       * TODO/FIXME: Yes, I know this is ugly...
       * Once we refactor the code to enable alternative
       * non-list-type view modes (e.g. grid, coverflow),
       * this sort of thing will be handled via function
       * pointers (we'll need these in several places:
       * handling pointer input, loading thumbnails,
       * menu drawing, selection highlight drawing,
       * etc.). Until then, a simple switch (and a bunch
       * of duplicated code in the two render_menu_entry
       * functions) will suffice... */
      switch (mui->list_view_type)
      {
         case MUI_LIST_VIEW_PLAYLIST:
         case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL:
         case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM:
         case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE:
            materialui_render_menu_entry_playlist_list(
                  mui,
                  video_info,
                  node,
                  &entry,
                  entry_selected,
                  touch_feedback_active,
                  header_height,
                  width,
                  height,
                  x_offset);
            break;
         case MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON:
            materialui_render_menu_entry_playlist_dual_icon(
                  mui,
                  video_info,
                  node,
                  &entry,
                  entry_selected,
                  touch_feedback_active,
                  header_height,
                  width,
                  height,
                  x_offset);
            break;
         case MUI_LIST_VIEW_DEFAULT:
         default:
            materialui_render_menu_entry_default(
                  mui,
                  video_info,
                  node,
                  &entry,
                  entry_selected,
                  touch_feedback_active,
                  header_height,
                  width,
                  height,
                  x_offset);
            break;
      }
   }

   /* Draw scrollbar */
   materialui_render_scrollbar(
         mui, video_info, width, height);
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

static void materialui_render_background(materialui_handle_t *mui, video_frame_info_t *video_info)
{
   menu_display_ctx_draw_t draw;
   bool add_opacity       = false;
   float opacity_override = 1.0f;
   float draw_color[16]   = {
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f
   };

   /* Configure draw object */
   draw.x                     = 0;
   draw.y                     = 0;
   draw.width                 = video_info->width;
   draw.height                = video_info->height;
   draw.coords                = NULL;
   draw.matrix_data           = NULL;
   draw.prim_type             = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.vertex                = NULL;
   draw.tex_coord             = NULL;
   draw.vertex_count          = 4;
   draw.pipeline.id           = 0;
   draw.pipeline.active       = false;
   draw.pipeline.backend_data = NULL;
   draw.color                 = draw_color;

   if (mui->textures.bg && !video_info->libretro_running)
   {
      draw.texture = mui->textures.bg;

      /* We are showing a wallpaper - set opacity
       * override to menu_wallpaper_opacity */
      add_opacity      = true;
      opacity_override = video_info->menu_wallpaper_opacity;
   }
   else
   {
      draw.texture = menu_display_white_texture;

      /* Copy 'list_background' colour to draw colour */
      memcpy(draw_color, mui->colors.list_background, sizeof(draw_color));

      /* We are not showing a wallpaper - if content
       * is running, set opacity override to
       * menu_framebuffer_opacity */
      if (video_info->libretro_running)
      {
         add_opacity      = true;
         opacity_override = video_info->menu_framebuffer_opacity;
      }
   }

   /* Draw background */
   menu_display_blend_begin(video_info);
   menu_display_draw_bg(&draw, video_info, add_opacity, opacity_override);
   menu_display_draw(&draw, video_info);
   menu_display_blend_end(video_info);
}

static void materialui_render_landscape_border(
      materialui_handle_t *mui, video_frame_info_t *video_info,
      unsigned width, unsigned height, unsigned header_height, int x_offset)
{
   if (mui->landscape_entry_margin > mui->margin)
   {
      unsigned border_width  = mui->landscape_entry_margin - mui->margin;
      unsigned border_height = height - header_height - mui->nav_bar_layout_height;
      int left_x             = x_offset;
      int right_x            =
            x_offset + (int)width - (int)mui->landscape_entry_margin +
                  (int)mui->margin - (int)mui->nav_bar_layout_width;
      int y                  = (int)header_height;

      /* Draw left border */
      menu_display_draw_quad(
            video_info,
            left_x,
            y,
            border_width,
            border_height,
            width,
            height,
            mui->colors.landscape_border_shadow_left);

      /* Draw right border */
      menu_display_draw_quad(
            video_info,
            right_x,
            y,
            border_width,
            border_height,
            width,
            height,
            mui->colors.landscape_border_shadow_right);
   }
}

static void materialui_render_selection_highlight(
      materialui_handle_t *mui, video_frame_info_t *video_info,
      unsigned width, unsigned height, unsigned header_height, int x_offset,
      size_t selection, float *color)
{
   /* Only draw highlight if selection is onscreen */
   if (materialui_entry_onscreen(mui, selection))
   {
      file_list_t *list        = NULL;
      materialui_node_t *node  = NULL;
      int highlight_x          = x_offset;
      int highlight_y          = 0;
      int highlight_width      = (int)width - (int)mui->nav_bar_layout_width;
      int highlight_height     = 0;

      /* If landscape optimisations are enabled/active,
       * adjust highlight layout */
      if (mui->landscape_entry_margin > 0)
      {
         highlight_x     += (int)mui->landscape_entry_margin - (int)mui->margin;
         highlight_width -= (int)(2 * mui->landscape_entry_margin) - (int)(2 * mui->margin);
         highlight_width  = (highlight_width < 0) ? 0 : highlight_width;
      }

      list = menu_entries_get_selection_buf_ptr(0);
      if (!list)
         return;

      node = (materialui_node_t*)file_list_get_userdata_at_offset(list, selection);
      if (!node)
         return;

      /* Now we have a valid node, can determine
       * highlight y position and height...
       * > Note: We round y position down and add 1 to
       *   the height in order to avoid obvious 'seams'
       *   when entries have dividers (rounding errors
       *   would otherwise cause 1px vertical gaps) */
      highlight_y      = (int)((float)header_height - mui->scroll_y + node->y);
      highlight_height = (int)(node->entry_height + 1.5f);

      /* Draw highlight quad */
      menu_display_draw_quad(
            video_info,
            highlight_x,
            highlight_y,
            (unsigned)highlight_width,
            (unsigned)highlight_height,
            width,
            height,
            color);
   }
}

static void materialui_render_entry_touch_feedback(
      materialui_handle_t *mui, video_frame_info_t *video_info,
      unsigned width, unsigned height, unsigned header_height, int x_offset,
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
                       (mui->pointer.x >  mui->landscape_entry_margin) &&
                       (mui->pointer.x <  width - mui->landscape_entry_margin - mui->nav_bar_layout_width) &&
                       (mui->pointer.y >= header_height) &&
                       (mui->pointer.y <= height - mui->nav_bar_layout_height);

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
      mui->touch_feedback_alpha -= (menu_animation_get_delta_time() * 1000.0f) / (float)MENU_INPUT_PRESS_TIME_SHORT;
      mui->touch_feedback_alpha = (mui->touch_feedback_alpha < 0.0f) ? 0.0f : mui->touch_feedback_alpha;
   }

   /* If alpha value is greater than zero, draw
    * touch feedback highlight */
   if (mui->touch_feedback_alpha > 0.0f)
   {
      float higlight_color[16];

      /* Set highlight colour */
      memcpy(higlight_color, mui->colors.list_highlighted_background, sizeof(higlight_color));
      menu_display_set_alpha(higlight_color, mui->transition_alpha * mui->touch_feedback_alpha);

      /* Draw highlight */
      materialui_render_selection_highlight(
            mui, video_info, width, height, header_height, x_offset,
            mui->touch_feedback_selection,
            higlight_color);
   }
}

static void materialui_render_header(
      materialui_handle_t *mui, video_frame_info_t *video_info, unsigned width, unsigned height)
{
   settings_t *settings          = config_get_ptr();
   size_t menu_title_margin      = 0;
   int usable_sys_bar_width      = (int)width - (int)mui->nav_bar_layout_width;
   int usable_title_bar_width    = usable_sys_bar_width;
   size_t sys_bar_battery_width  = 0;
   size_t sys_bar_clock_width    = 0;
   int sys_bar_text_y            = (int)(((float)mui->sys_bar_height / 2.0f) + ((float)mui->font_data.hint.font_height / 4.0f));
   int title_x                   = 0;
   bool show_back_icon           = menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL);
   bool show_search_icon         = mui->is_playlist || mui->is_file_list;
   bool show_switch_view_icon    = mui->is_playlist && mui->primary_thumbnail_available;
   bool use_landscape_layout     = !mui->is_portrait &&
         (mui->last_landscape_layout_optimization != MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED);
   const char *menu_title        = mui->menu_title;
   char menu_title_buf[255];

   menu_title_buf[0]  = '\0';

   if (!settings)
      return;

   /* Draw background quads
    * > Title bar is underneath system bar
    * > Shadow is underneath title bar */

   /* > Shadow */
   menu_display_draw_quad(
         video_info,
         0,
         mui->sys_bar_height + mui->title_bar_height,
         width,
         mui->header_shadow_height,
         width,
         height,
         mui->colors.header_shadow);

   /* > Title bar background */
   menu_display_draw_quad(
         video_info,
         0,
         0,
         width,
         mui->sys_bar_height + mui->title_bar_height,
         width,
         height,
         mui->colors.title_bar_background);

   /* > System bar background */
   menu_display_draw_quad(
         video_info,
         0,
         0,
         width,
         mui->sys_bar_height,
         width,
         height,
         mui->colors.sys_bar_background);

   /* System bar items */

   /* > Draw battery indicator (if required) */
   if (settings->bools.menu_battery_level_enable)
   {
      menu_display_ctx_powerstate_t powerstate;
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
         if (!string_is_equal(percent_str, mui->sys_bar_cache.battery_percent_str))
         {
            /* Cache new string */
            strlcpy(mui->sys_bar_cache.battery_percent_str, percent_str,
                  MUI_BATTERY_PERCENT_MAX_LENGTH * sizeof(char));

            /* Cache width */
            mui->sys_bar_cache.battery_percent_width = font_driver_get_message_width(
                  mui->font_data.hint.font,
                  mui->sys_bar_cache.battery_percent_str,
                  (unsigned)strlen(mui->sys_bar_cache.battery_percent_str),
                  1.0f);
         }

         if (mui->sys_bar_cache.battery_percent_width > 0)
         {
            /* Set critical by default, to ensure texture_battery
             * is always valid */
            uintptr_t texture_battery = mui->textures.list[MUI_TEXTURE_BATTERY_CRITICAL];

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

            materialui_draw_icon(video_info,
                  mui->sys_bar_icon_size,
                  (uintptr_t)texture_battery,
                  (int)width - ((int)mui->sys_bar_cache.battery_percent_width +
                        (int)mui->sys_bar_margin + (int)mui->sys_bar_icon_size + (int)mui->nav_bar_layout_width),
                  0,
                  width,
                  height,
                  0,
                  1,
                  mui->colors.sys_bar_icon);

            /* Draw percent text */
            menu_display_draw_text(mui->font_data.hint.font,
                  mui->sys_bar_cache.battery_percent_str,
                  (int)width - ((int)mui->sys_bar_cache.battery_percent_width + (int)mui->sys_bar_margin + (int)mui->nav_bar_layout_width),
                  sys_bar_text_y,
                  width, height, mui->colors.sys_bar_text, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);

            sys_bar_battery_width = mui->sys_bar_cache.battery_percent_width +
                  mui->sys_bar_margin + mui->sys_bar_icon_size;
            usable_sys_bar_width -= sys_bar_battery_width;
         }
      }
   }

   /* > Draw clock (if required) */
   if (settings->bools.menu_timedate_enable)
   {
      menu_display_ctx_datetime_t datetime;
      char timedate_str[MUI_TIMEDATE_MAX_LENGTH];

      timedate_str[0] = '\0';

      datetime.s         = timedate_str;
      datetime.len       = sizeof(timedate_str);
      datetime.time_mode = settings->uints.menu_timedate_style;

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
         mui->sys_bar_cache.timedate_width = font_driver_get_message_width(
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

         menu_display_draw_text(mui->font_data.hint.font,
               mui->sys_bar_cache.timedate_str,
               (int)width - ((int)sys_bar_clock_width + (int)sys_bar_battery_width + (int)mui->nav_bar_layout_width),
               sys_bar_text_y,
               width, height, mui->colors.sys_bar_text, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);

         usable_sys_bar_width -= sys_bar_clock_width;
      }
   }

   usable_sys_bar_width -= (2 * mui->sys_bar_margin);
   usable_sys_bar_width = (usable_sys_bar_width > 0) ? usable_sys_bar_width : 0;

   /* > Draw core name, if required */
   if (settings->bools.menu_core_enable)
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

         menu_animation_ticker_smooth(&mui->ticker_smooth);
      }
      else
      {
         mui->ticker.s        = core_title_buf;
         mui->ticker.len      = (unsigned)(usable_sys_bar_width / mui->font_data.hint.glyph_width);
         mui->ticker.str      = core_title;
         mui->ticker.selected = true;

         menu_animation_ticker(&mui->ticker);
      }

      menu_display_draw_text(mui->font_data.hint.font, core_title_buf,
            (int)mui->ticker_x_offset + (int)mui->sys_bar_margin,
            sys_bar_text_y,
            width, height, mui->colors.sys_bar_text, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);
   }

   /* Title bar items */

   /* > Draw 'back' icon, if required */
   menu_title_margin = mui->margin;

   if (show_back_icon)
   {
      menu_title_margin = mui->icon_size;

      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_BACK],
            0,
            (int)mui->sys_bar_height,
            width,
            height,
            0,
            1,
            mui->colors.header_icon);
   }

   usable_title_bar_width -= menu_title_margin;

   /* > Draw 'search' icon, if required */
   if (show_search_icon)
   {
      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[MUI_TEXTURE_SEARCH],
            (int)width - (int)mui->icon_size - (int)mui->nav_bar_layout_width,
            (int)mui->sys_bar_height,
            width,
            height,
            0,
            1,
            mui->colors.header_icon);

      usable_title_bar_width -= mui->icon_size;

      /* > Draw 'switch view' icon, if required
       *   Note: We can take a shortcut here because
       *   'switch view' can only be shown if
       *   'search' is also shown... */
      if (show_switch_view_icon)
      {
         materialui_draw_icon(video_info,
               mui->icon_size,
               mui->textures.list[MUI_TEXTURE_SWITCH_VIEW],
               (int)width - (2 * (int)mui->icon_size) - (int)mui->nav_bar_layout_width,
               (int)mui->sys_bar_height,
               width,
               height,
               0,
               1,
               mui->colors.header_icon);

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
      if (!menu_animation_ticker_smooth(&mui->ticker_smooth))
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
      if (!menu_animation_ticker(&mui->ticker))
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

   menu_display_draw_text(mui->font_data.title.font, menu_title_buf,
         title_x,
         (int)(mui->sys_bar_height + (mui->title_bar_height / 2.0f) + (mui->font_data.title.font_height / 4.0f)),
         width, height, mui->colors.header_text, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);
}

/* Use seperate functions for bottom/right navigation
 * bars. This involves substantial code duplication, but if
 * we try to handle this with a single function then
 * things get incredibly messy and inefficient... */
static void materialui_render_nav_bar_bottom(
      materialui_handle_t *mui, video_frame_info_t *video_info,
      unsigned width, unsigned height)
{
   unsigned nav_bar_width           = width;
   unsigned nav_bar_height          = mui->nav_bar.width;
   int nav_bar_x                    = 0;
   int nav_bar_y                    = (int)height - (int)mui->nav_bar.width;
   unsigned num_tabs                = mui->nav_bar.num_menu_tabs + MUI_NAV_BAR_NUM_ACTION_TABS;
   float tab_width                  = (float)width / (float)num_tabs;
   unsigned tab_width_int           = (unsigned)(tab_width + 0.5f);
   unsigned selection_marker_width  = tab_width_int;
   unsigned selection_marker_height = mui->nav_bar.selection_marker_width;
   int selection_marker_y           = (int)height - (int)mui->nav_bar.selection_marker_width;
   unsigned i;

   /* Draw navigation bar background */

   /* > Background */
   menu_display_draw_quad(
         video_info,
         nav_bar_x,
         nav_bar_y,
         nav_bar_width,
         nav_bar_height,
         width,
         height,
         mui->colors.nav_bar_background);

   /* > Divider */
   menu_display_draw_quad(
         video_info,
         nav_bar_x,
         nav_bar_y,
         nav_bar_width,
         mui->nav_bar.divider_width,
         width,
         height,
         mui->colors.divider);

   /* Draw tabs */

   /* > Back - left hand side */
   materialui_draw_icon(video_info,
         mui->icon_size,
         mui->textures.list[mui->nav_bar.back_tab.texture_index],
         (int)((0.5f * tab_width) - ((float)mui->icon_size / 2.0f)),
         nav_bar_y,
         width,
         height,
         0,
         1,
         mui->nav_bar.back_tab.enabled ?
               mui->colors.nav_bar_icon_passive : mui->colors.nav_bar_icon_disabled);

   /* > Resume - right hand side */
   materialui_draw_icon(video_info,
         mui->icon_size,
         mui->textures.list[mui->nav_bar.resume_tab.texture_index],
         (int)((((float)num_tabs - 0.5f) * tab_width) - ((float)mui->icon_size / 2.0f)),
         nav_bar_y,
         width,
         height,
         0,
         1,
         mui->nav_bar.resume_tab.enabled ?
               mui->colors.nav_bar_icon_passive : mui->colors.nav_bar_icon_disabled);

   /* Menu tabs - in the centre, left to right */
   for (i = 0; i < mui->nav_bar.num_menu_tabs; i++)
   {
      materialui_nav_bar_menu_tab_t *tab = &mui->nav_bar.menu_tabs[i];
      float *draw_color                  = tab->active ?
            mui->colors.nav_bar_icon_active : mui->colors.nav_bar_icon_passive;

      /* Draw icon */
      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[tab->texture_index],
            (((float)i + 1.5f) * tab_width) - ((float)mui->icon_size / 2.0f),
            nav_bar_y,
            width,
            height,
            0,
            1,
            draw_color);

      /* Draw selection marker */
      menu_display_draw_quad(
            video_info,
            (int)((i + 1) * tab_width_int),
            selection_marker_y,
            selection_marker_width,
            selection_marker_height,
            width,
            height,
            draw_color);
   }
}

static void materialui_render_nav_bar_right(
      materialui_handle_t *mui, video_frame_info_t *video_info,
      unsigned width, unsigned height)
{
   unsigned nav_bar_width           = mui->nav_bar.width;
   unsigned nav_bar_height          = height;
   int nav_bar_x                    = (int)width - (int)mui->nav_bar.width;
   int nav_bar_y                    = 0;
   unsigned num_tabs                = mui->nav_bar.num_menu_tabs + MUI_NAV_BAR_NUM_ACTION_TABS;
   float tab_height                 = (float)height / (float)num_tabs;
   unsigned tab_height_int          = (unsigned)(tab_height + 0.5f);
   unsigned selection_marker_width  = mui->nav_bar.selection_marker_width;
   unsigned selection_marker_height = tab_height_int;
   int selection_marker_x           = (int)width - (int)mui->nav_bar.selection_marker_width;
   unsigned i;

   /* Draw navigation bar background */

   /* > Background */
   menu_display_draw_quad(
         video_info,
         nav_bar_x,
         nav_bar_y,
         nav_bar_width,
         nav_bar_height,
         width,
         height,
         mui->colors.nav_bar_background);

   /* > Divider */
   menu_display_draw_quad(
         video_info,
         nav_bar_x,
         nav_bar_y,
         mui->nav_bar.divider_width,
         nav_bar_height,
         width,
         height,
         mui->colors.divider);

   /* Draw tabs */

   /* > Back - bottom */
   materialui_draw_icon(video_info,
         mui->icon_size,
         mui->textures.list[mui->nav_bar.back_tab.texture_index],
         nav_bar_x,
         (int)((((float)num_tabs - 0.5f) * tab_height) - ((float)mui->icon_size / 2.0f)),
         width,
         height,
         0,
         1,
         mui->nav_bar.back_tab.enabled ?
               mui->colors.nav_bar_icon_passive : mui->colors.nav_bar_icon_disabled);

   /* > Resume - top */
   materialui_draw_icon(video_info,
         mui->icon_size,
         mui->textures.list[mui->nav_bar.resume_tab.texture_index],
         nav_bar_x,
         (int)((0.5f * tab_height) - ((float)mui->icon_size / 2.0f)),
         width,
         height,
         0,
         1,
         mui->nav_bar.resume_tab.enabled ?
               mui->colors.nav_bar_icon_passive : mui->colors.nav_bar_icon_disabled);

   /* Menu tabs - in the centre, top to bottom */
   for (i = 0; i < mui->nav_bar.num_menu_tabs; i++)
   {
      materialui_nav_bar_menu_tab_t *tab = &mui->nav_bar.menu_tabs[i];
      float *draw_color                  = tab->active ?
            mui->colors.nav_bar_icon_active : mui->colors.nav_bar_icon_passive;

      /* Draw icon */
      materialui_draw_icon(video_info,
            mui->icon_size,
            mui->textures.list[tab->texture_index],
            nav_bar_x,
            (((float)i + 1.5f) * tab_height) - ((float)mui->icon_size / 2.0f),
            width,
            height,
            0,
            1,
            draw_color);

      /* Draw selection marker */
      menu_display_draw_quad(
            video_info,
            selection_marker_x,
            (int)((i + 1) * tab_height_int),
            selection_marker_width,
            selection_marker_height,
            width,
            height,
            draw_color);
   }
}

static void materialui_render_nav_bar(
      materialui_handle_t *mui, video_frame_info_t *video_info,
      unsigned width, unsigned height)
{
   if (mui->nav_bar.location == MUI_NAV_BAR_LOCATION_RIGHT)
      materialui_render_nav_bar_right(
            mui, video_info, width, height);
   else
      materialui_render_nav_bar_bottom(
            mui, video_info, width, height);
}

/* Convenience function for accessing the thumbnails
 * associated with the selected node.
 * > Thumbnails are only valid if function returns true
 * > Returns false if current selection is off screen,
 *   or node is unallocated */
static bool materialui_get_selected_thumbnails(
      materialui_handle_t *mui, size_t selection,
      menu_thumbnail_t **primary_thumbnail,
      menu_thumbnail_t **secondary_thumbnail)
{
   file_list_t *list       = NULL;
   materialui_node_t *node = NULL;

   /* Ensure selection is on screen */
   if (!materialui_entry_onscreen(mui, selection))
      return false;

   /* Get currently selected node */
   list = menu_entries_get_selection_buf_ptr(0);
   if (!list)
      return false;

   node = (materialui_node_t*)file_list_get_userdata_at_offset(list, selection);
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
   menu_animation_ctx_tag alpha_tag = (uintptr_t)&mui->fullscreen_thumbnail_alpha;

   /* Kill any existing fade in/out animations */
   menu_animation_kill_by_tag(&alpha_tag);

   /* Check whether animations are enabled */
   if (animate && (mui->fullscreen_thumbnail_alpha > 0.0f))
   {
      menu_animation_ctx_entry_t animation_entry;

      /* Configure fade out animation */
      animation_entry.easing_enum  = EASING_OUT_QUAD;
      animation_entry.tag          = alpha_tag;
      animation_entry.duration     = menu_thumbnail_get_fade_duration();
      animation_entry.target_value = 0.0f;
      animation_entry.subject      = &mui->fullscreen_thumbnail_alpha;
      animation_entry.cb           = NULL;
      animation_entry.userdata     = NULL;

      /* Push animation */
      menu_animation_push(&animation_entry);
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
   menu_thumbnail_t *primary_thumbnail   = NULL;
   menu_thumbnail_t *secondary_thumbnail = NULL;
   menu_animation_ctx_tag scroll_tag     = (uintptr_t)&mui->scroll_y;
   menu_animation_ctx_tag alpha_tag      = (uintptr_t)&mui->fullscreen_thumbnail_alpha;
   const char *thumbnail_label           = NULL;
   menu_animation_ctx_entry_t animation_entry;
   menu_entry_t selected_entry;

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
   if ((primary_thumbnail->status == MENU_THUMBNAIL_STATUS_AVAILABLE) &&
       (mui->secondary_thumbnail_enabled &&
            ((secondary_thumbnail->status != MENU_THUMBNAIL_STATUS_MISSING) &&
             (secondary_thumbnail->status != MENU_THUMBNAIL_STATUS_AVAILABLE))))
      return;

   if ((primary_thumbnail->status == MENU_THUMBNAIL_STATUS_MISSING) &&
       (!mui->secondary_thumbnail_enabled ||
            (secondary_thumbnail->status != MENU_THUMBNAIL_STATUS_AVAILABLE)))
      return;

   /* Menu list must be stationary while fullscreen
    * thumbnails are shown
    * > Kill any existing scroll animations and
    *   reset scroll acceleration */
   menu_animation_kill_by_tag(&scroll_tag);
   menu_input_set_pointer_y_accel(0.0f);

   /* Cache selected entry label
    * (used as menu title when fullscreen thumbnails
    * are shown) */
   mui->fullscreen_thumbnail_label[0] = '\0';

   /* > Get menu entry */
   menu_entry_init(&selected_entry);
   selected_entry.path_enabled     = false;
   selected_entry.value_enabled    = false;
   selected_entry.sublabel_enabled = false;
   menu_entry_get(&selected_entry, 0, selection, NULL, true);

   /* > Get entry label */
   menu_entry_get_rich_label(&selected_entry, &thumbnail_label);

   /* > Sanity check */
   if (!string_is_empty(thumbnail_label))
      strlcpy(
            mui->fullscreen_thumbnail_label,
            thumbnail_label,
            sizeof(mui->fullscreen_thumbnail_label));

   /* Configure fade in animation */
   animation_entry.easing_enum  = EASING_OUT_QUAD;
   animation_entry.tag          = alpha_tag;
   animation_entry.duration     = menu_thumbnail_get_fade_duration();
   animation_entry.target_value = 1.0f;
   animation_entry.subject      = &mui->fullscreen_thumbnail_alpha;
   animation_entry.cb           = NULL;
   animation_entry.userdata     = NULL;

   /* Push animation */
   menu_animation_push(&animation_entry);

   /* Enable fullscreen thumbnails */
   mui->fullscreen_thumbnail_selection = selection;
   mui->show_fullscreen_thumbnails     = true;
}

static void materialui_render_fullscreen_thumbnails(
      materialui_handle_t *mui, video_frame_info_t *video_info,
      unsigned width, unsigned height, unsigned header_height,
      size_t selection)
{
   /* Check whether fullscreen thumbnails are visible */
   if (mui->fullscreen_thumbnail_alpha > 0.0f)
   {
      menu_thumbnail_t *primary_thumbnail   = NULL;
      menu_thumbnail_t *secondary_thumbnail = NULL;
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
            (primary_thumbnail->status != MENU_THUMBNAIL_STATUS_MISSING);
      show_secondary_thumbnail =
            mui->secondary_thumbnail_enabled &&
            (secondary_thumbnail->status != MENU_THUMBNAIL_STATUS_MISSING);

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
      view_width  = (int)width  - (int)mui->nav_bar_layout_width;
      view_height = (int)height - (int)mui->nav_bar_layout_height - (int)header_height;

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
         menu_thumbnail_get_draw_dimensions(
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
         menu_thumbnail_get_draw_dimensions(
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
      menu_display_set_alpha(
            mui->colors.screen_fade,
            mui->colors.screen_fade_opacity * mui->fullscreen_thumbnail_alpha);

      menu_display_set_alpha(
            mui->colors.surface_background, mui->fullscreen_thumbnail_alpha);

      /* Darken background */
      menu_display_draw_quad(
            video_info,
            0,
            header_height,
            (unsigned)view_width,
            (unsigned)view_height,
            width,
            height,
            mui->colors.screen_fade);

      /* Draw thumbnails
       * > Primary */
      if (show_primary_thumbnail)
      {
         /* Background */
         menu_display_draw_quad(
               video_info,
               primary_thumbnail_x - (int)(mui->margin >> 1) +
                     ((thumbnail_box_width - (int)primary_thumbnail_draw_width) >> 1),
               primary_thumbnail_y - (int)(mui->margin >> 1) +
                     ((thumbnail_box_height - (int)primary_thumbnail_draw_height) >> 1),
               (unsigned)primary_thumbnail_draw_width + mui->margin,
               (unsigned)primary_thumbnail_draw_height + mui->margin,
               width,
               height,
               mui->colors.surface_background);

         /* Thumbnail */
         menu_thumbnail_draw(
               video_info,
               primary_thumbnail,
               primary_thumbnail_x,
               primary_thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               MENU_THUMBNAIL_ALIGN_CENTRE,
               mui->fullscreen_thumbnail_alpha,
               1.0f,
               NULL);
      }

      /* > Secondary */
      if (show_secondary_thumbnail)
      {
         /* Background */
         menu_display_draw_quad(
               video_info,
               secondary_thumbnail_x - (int)(mui->margin >> 1) +
                     ((thumbnail_box_width - (int)secondary_thumbnail_draw_width) >> 1),
               secondary_thumbnail_y - (int)(mui->margin >> 1) +
                     ((thumbnail_box_height - (int)secondary_thumbnail_draw_height) >> 1),
               (unsigned)secondary_thumbnail_draw_width + mui->margin,
               (unsigned)secondary_thumbnail_draw_height + mui->margin,
               width,
               height,
               mui->colors.surface_background);

         /* Thumbnail */
         menu_thumbnail_draw(
               video_info,
               secondary_thumbnail,
               secondary_thumbnail_x,
               secondary_thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               MENU_THUMBNAIL_ALIGN_CENTRE,
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
      menu_display_set_alpha(mui->colors.list_highlighted_background, alpha);
      menu_display_set_alpha(mui->colors.list_icon,                   alpha);
      menu_display_set_alpha(mui->colors.list_switch_on,              alpha);
      menu_display_set_alpha(mui->colors.list_switch_on_background,   alpha);
      menu_display_set_alpha(mui->colors.list_switch_off,             alpha);
      menu_display_set_alpha(mui->colors.list_switch_off_background,  alpha);
      menu_display_set_alpha(mui->colors.scrollbar,                   alpha);
      menu_display_set_alpha(mui->colors.missing_thumbnail_icon,      alpha);

      /* Landscape border shadow only fades if:
       * - Landscape border is shown
       * - We are currently performing a slide animation */
      if ((mui->landscape_entry_margin != 0) &&
          (mui->transition_x_offset != 0.0f))
      {
         float border_shadow_alpha =
               mui->colors.landscape_border_shadow_opacity * alpha;

         mui->colors.landscape_border_shadow_left[7]   = border_shadow_alpha;
         mui->colors.landscape_border_shadow_left[15]  = border_shadow_alpha;
         mui->colors.landscape_border_shadow_right[3]  = border_shadow_alpha;
         mui->colors.landscape_border_shadow_right[11] = border_shadow_alpha;
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
      menu_display_set_alpha(mui->colors.list_highlighted_background, 1.0f);
      menu_display_set_alpha(mui->colors.list_icon,                   1.0f);
      menu_display_set_alpha(mui->colors.list_switch_on,              1.0f);
      menu_display_set_alpha(mui->colors.list_switch_on_background,   1.0f);
      menu_display_set_alpha(mui->colors.list_switch_off,             1.0f);
      menu_display_set_alpha(mui->colors.list_switch_off_background,  1.0f);
      menu_display_set_alpha(mui->colors.scrollbar,                   1.0f);
      menu_display_set_alpha(mui->colors.missing_thumbnail_icon,      1.0f);

      /* Landscape border shadow only fades if:
       * - Landscape border is shown
       * - We are currently performing a slide animation */
      if ((mui->landscape_entry_margin != 0) &&
          (mui->transition_x_offset != 0.0f))
      {
         float border_shadow_alpha =
               mui->colors.landscape_border_shadow_opacity;

         mui->colors.landscape_border_shadow_left[7]   = border_shadow_alpha;
         mui->colors.landscape_border_shadow_left[15]  = border_shadow_alpha;
         mui->colors.landscape_border_shadow_right[3]  = border_shadow_alpha;
         mui->colors.landscape_border_shadow_right[11] = border_shadow_alpha;
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
      int view_height = (int)height - (int)header_height - (int)mui->nav_bar_layout_height;
      int y_max       = view_height + (int)header_height - (int)(mui->scrollbar.width + mui->scrollbar.height);

      /* Get X position */
      mui->scrollbar.x = x_offset + (int)width - (int)mui->scrollbar.width - (int)mui->nav_bar_layout_width;

      /* > Scrollbar must be offset by the current
       *   landscape border width when landscape optimisations
       *   are enabled */
      if (mui->landscape_entry_margin > mui->margin)
         mui->scrollbar.x -= (int)mui->landscape_entry_margin - (int)mui->margin;

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
   materialui_handle_t *mui = (materialui_handle_t*)data;
   settings_t *settings     = config_get_ptr();
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   unsigned header_height   = menu_display_get_header_height();
   size_t selection         = menu_navigation_get_selection();
   int list_x_offset;

   if (!mui || !settings)
      return;

   menu_display_set_viewport(width, height);

   /* Clear text */
   font_driver_bind_block(mui->font_data.title.font, &mui->font_data.title.raster_block);
   font_driver_bind_block(mui->font_data.list.font,  &mui->font_data.list.raster_block);
   font_driver_bind_block(mui->font_data.hint.font,  &mui->font_data.hint.raster_block);

   mui->font_data.title.raster_block.carr.coords.vertices = 0;
   mui->font_data.list.raster_block.carr.coords.vertices  = 0;
   mui->font_data.hint.raster_block.carr.coords.vertices  = 0;

   /* Update theme colours, if required */
   if (mui->color_theme != video_info->materialui_color_theme)
   {
      materialui_prepare_colors(mui, (enum materialui_color_theme)video_info->materialui_color_theme);
      mui->color_theme = (enum materialui_color_theme)video_info->materialui_color_theme;
   }

   /* Update line ticker(s) */
   mui->use_smooth_ticker = settings->bools.menu_ticker_smooth;

   if (mui->use_smooth_ticker)
   {
      mui->ticker_smooth.idx       = menu_animation_get_ticker_pixel_idx();
      mui->ticker_smooth.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
   }
   else
   {
      mui->ticker.idx       = menu_animation_get_ticker_idx();
      mui->ticker.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
   }

   /* Handle any transparency adjustments required
    * by menu transition animations */
   materialui_colors_set_transition_alpha(mui);

   /* Get x offset for list items, required by
    * menu transition 'slide' animations */
   list_x_offset = (int)(mui->transition_x_offset * (float)((int)width - (int)mui->nav_bar_layout_width));

   /* Draw background */
   materialui_render_background(mui, video_info);

   /* Draw landscape border
    * (does nothing in portrait mode, or if landscape
    * optimisations are disabled) */
   materialui_render_landscape_border(
         mui, video_info, width, height, header_height, list_x_offset);

   /* Draw 'highlighted entry' selection box */
   materialui_render_selection_highlight(
         mui, video_info, width, height, header_height, list_x_offset, selection,
         mui->colors.list_highlighted_background);

   /* Draw 'short press' touch feedback highlight */
   materialui_render_entry_touch_feedback(
         mui, video_info, width, height, header_height, list_x_offset, selection);

   /* Draw menu list
    * > Must update scrollbar draw position before
    *   list is rendered
    * > We handle the scrollbar in a separate step
    *   like this because we need to track its
    *   position in order to enable fast navigation
    *   via scrollbar 'dragging' */
   materialui_update_scrollbar(mui, width, height, header_height, list_x_offset);
   materialui_render_menu_list(mui, video_info, width, height, list_x_offset);

   /* Flush first layer of text
    * > Menu list only uses list and hint fonts */
   font_driver_flush(width, height, mui->font_data.list.font, video_info);
   font_driver_flush(width, height, mui->font_data.hint.font, video_info);

   mui->font_data.list.raster_block.carr.coords.vertices = 0;
   mui->font_data.hint.raster_block.carr.coords.vertices = 0;

   /* Draw fullscreen thumbnails, if currently active
    * > Must be done *after* we flush the first layer
    *   of text */
   materialui_render_fullscreen_thumbnails(
         mui, video_info, width, height, header_height, selection);

   /* Draw title + system bar */
   materialui_render_header(mui, video_info, width, height);

   /* Draw navigation bar */
   materialui_render_nav_bar(mui, video_info, width, height);

   /* Flush second layer of text
    * > Title + system bar only use title and hint fonts */
   font_driver_flush(width, height, mui->font_data.title.font, video_info);
   font_driver_flush(width, height, mui->font_data.hint.font,  video_info);

   mui->font_data.title.raster_block.carr.coords.vertices = 0;
   mui->font_data.hint.raster_block.carr.coords.vertices  = 0;

   /* Handle onscreen keyboard */
   if (menu_input_dialog_get_display_kb())
   {
      char msg[255];
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      msg[0] = '\0';

      /* Darken screen */
      menu_display_set_alpha(
            mui->colors.screen_fade, mui->colors.screen_fade_opacity);
      menu_display_draw_quad(video_info,
            0, 0, width, height, width, height, mui->colors.screen_fade);

      /* Draw message box */
      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      materialui_render_messagebox(mui, video_info, height / 4, msg);

      /* Draw onscreen keyboard */
      menu_display_draw_keyboard(
            mui->textures.list[MUI_TEXTURE_KEY_HOVER],
            mui->font_data.list.font,
            video_info,
            menu_event_get_osk_grid(), menu_event_get_osk_ptr(),
            0xFFFFFFFF);

      /* Flush message box & osk text
       * > Message box & osk only use list font */
      font_driver_flush(width, height, mui->font_data.list.font, video_info);
      mui->font_data.list.raster_block.carr.coords.vertices = 0;
   }

   /* Draw message box */
   if (!string_is_empty(mui->msgbox))
   {
      /* Darken screen */
      menu_display_set_alpha(
            mui->colors.screen_fade, mui->colors.screen_fade_opacity);
      menu_display_draw_quad(video_info,
            0, 0, width, height, width, height, mui->colors.screen_fade);

      /* Draw message box */
      materialui_render_messagebox(mui, video_info, height / 2, mui->msgbox);
      mui->msgbox[0] = '\0';

      /* Flush message box text
       * > Message box only uses list font */
      font_driver_flush(width, height, mui->font_data.list.font, video_info);
      mui->font_data.list.raster_block.carr.coords.vertices = 0;
   }

   /* Draw mouse cursor */
   if (mui->mouse_show && (mui->pointer.type != MENU_POINTER_DISABLED))
   {
      float color_white[16]  = {
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 1.0f
      };

      menu_display_draw_cursor(
            video_info,
            color_white,
            mui->cursor_size,
            mui->textures.list[MUI_TEXTURE_POINTER],
            mui->pointer.x,
            mui->pointer.y,
            width,
            height);
   }

   /* Undo any transparency adjustments caused
    * by menu transition animations */
   materialui_colors_reset_transition_alpha(mui);

   /* Unbind fonts */
   font_driver_bind_block(mui->font_data.title.font, NULL);
   font_driver_bind_block(mui->font_data.list.font,  NULL);
   font_driver_bind_block(mui->font_data.hint.font,  NULL);

   menu_display_unset_viewport(width, height);
}

/* Determines current list view type, based on
 * whether current menu is a playlist, and whether
 * user has enabled playlist thumbnails */
static void materialui_set_list_view_type(
      materialui_handle_t *mui, settings_t *settings)
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
            menu_thumbnail_is_enabled(mui->thumbnail_path_data, MENU_THUMBNAIL_RIGHT);

      if (mui->primary_thumbnail_available)
      {
         /* Get thumbnail view mode based on current
          * display orientation */
         if (mui->is_portrait)
         {
            switch (settings->uints.menu_materialui_thumbnail_view_portrait)
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
            switch (settings->uints.menu_materialui_thumbnail_view_landscape)
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
               case MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED:
               default:
                  mui->list_view_type = MUI_LIST_VIEW_PLAYLIST;
                  break;
            }
         }
      }
   }
}

/* Determines whether landscape optimisations should
 * be applied, and calculates appropriate landscape
 * entry margin size */
static void materialui_set_landscape_optimisations_enable(
      materialui_handle_t *mui, settings_t *settings)
{
   bool optimize_landscape_layout = false;

   /* In landscape orientations, menu lists are too wide
    * (to the extent that they are rather uncomfortable
    * to look at...)
    * > Depending upon user configuration, we therefore
    *   use additional padding at the left/right sides of
    *   the screen */

   /* Disable optimisations by default */
   mui->landscape_entry_margin = 0;

   /* Early out if current orientation is portrait */
   if (mui->is_portrait)
      return;

   /* Check whether optimisations are enabled, globally
    * or for current list view type */
   switch (mui->last_landscape_layout_optimization)
   {
      case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS:
         optimize_landscape_layout = true;
         break;
      case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS:

         switch (mui->list_view_type)
         {
            case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_SMALL:
            case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_MEDIUM:
            case MUI_LIST_VIEW_PLAYLIST_THUMB_LIST_LARGE:
            case MUI_LIST_VIEW_PLAYLIST_THUMB_DUAL_ICON:
               optimize_landscape_layout = false;
               break;
            case MUI_LIST_VIEW_PLAYLIST:
            case MUI_LIST_VIEW_DEFAULT:
            default:
               optimize_landscape_layout = true;
               break;
         }

         break;

      case MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED:
      default:
         optimize_landscape_layout = false;
         break;
   }

   /* Calculate landscape entry margin size, if required */
   if (optimize_landscape_layout)
   {
      /* After testing various approaches, it seems that
       * simply enforcing a 4:3 aspect ratio produces the
       * best results */
      const float base_aspect = 4.0f / 3.0f;
      float landscape_margin  =
            ((float)(mui->last_width - mui->nav_bar_layout_width) -
                  (base_aspect * (float)mui->last_height)) / 2.0f;

      /* Note: Want to round down here */
      if (landscape_margin > 1.0f)
         mui->landscape_entry_margin = (unsigned)landscape_margin;
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
               mui->font_data.list.font_height +
               (3 * mui->font_data.hint.font_height) +
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
               (mui->font_data.list.font_height +
                (3 * mui->font_data.hint.font_height)) * 2;

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
                  (int)(mui->landscape_entry_margin * 2) - (int)mui->nav_bar_layout_width;

            /* > Sanity check */
            if (usable_width < 2)
            {
               mui->thumbnail_width_max  = 0;
               mui->thumbnail_height_max = 0;
            }

            /* > Get maximum thumbnail width */
            mui->thumbnail_width_max = (usable_width >> 1);

            /* > Set thumbnail height based on max width */
            mui->thumbnail_height_max =
                  (unsigned)(((float)mui->thumbnail_width_max *
                        (1.0f / MUI_THUMBNAIL_DEFAULT_ASPECT_RATIO)) + 0.5f);                  
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
   if (menu_thumbnail_is_enabled(
         mui->thumbnail_path_data, MENU_THUMBNAIL_LEFT))
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
      if (settings->uints.menu_thumbnails ==
            MUI_DEFAULT_SECONDARY_THUMBNAIL_TYPE)
         settings->uints.menu_left_thumbnails =
               MUI_DEFAULT_SECONDARY_THUMBNAIL_FALLBACK_TYPE;
      else
         settings->uints.menu_left_thumbnails =
               MUI_DEFAULT_SECONDARY_THUMBNAIL_TYPE;
   }

   /* Final check - this will return true unless a
    * per-playlist override is in place */
   return menu_thumbnail_is_enabled(
         mui->thumbnail_path_data, MENU_THUMBNAIL_LEFT);
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

            /* Disable by default */
            mui->secondary_thumbnail_enabled = false;

            /* Check whether user has manually disabled
             * secondary thumbnails */
            if (!settings->bools.menu_materialui_dual_thumbnail_list_view_enable)
               return;

            /* Attempt to force enable secondary thumbnails if
             * global 'Secondary Thumbnail' type is set to OFF */
            if (!materialui_force_enable_secondary_thumbnail(mui, settings))
               return;

            /* Secondary thumbnails are supported/enabled
             * Check whether screen has sufficient
             * width to display them */

            /* > Get total usable width */
            usable_width =
                  (int)mui->last_width - (int)(mui->margin * 2) -
                  (int)(mui->landscape_entry_margin * 2) - (int)mui->nav_bar_layout_width;

            /* > Account for additional padding (margins) when
             *   using portrait orientations */
            if (mui->is_portrait)
               thumbnail_margin = (int)mui->scrollbar.width;
            /* > Account for additional padding (margins) when
             *   using landscape orientations */
            else
               if (mui->landscape_entry_margin < mui->margin)
                  thumbnail_margin = (int)(mui->margin - mui->landscape_entry_margin);

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
static void materialui_update_list_view(materialui_handle_t *mui)
{
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   materialui_set_list_view_type(mui, settings);
   materialui_set_landscape_optimisations_enable(mui, settings);
   materialui_set_thumbnail_dimensions(mui);
   materialui_set_secondary_thumbnail_enable(mui, settings);

   mui->need_compute = true;
}

/* Compute the positions of the widgets */
static void materialui_layout(materialui_handle_t *mui, bool video_is_threaded)
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

   mui->margin               = mui->dip_base_unit_size / 9;
   mui->icon_size            = mui->dip_base_unit_size / 3;

   mui->sys_bar_margin       = mui->dip_base_unit_size / 12;
   mui->sys_bar_icon_size    = mui->dip_base_unit_size / 7;

   mui->entry_divider_width  = (mui->last_scale_factor > 1.0f) ?
         (unsigned)(mui->last_scale_factor + 0.5f) : 1;

   /* Note: We used to set scrollbar width here, but
    * since we now have several scrollbar parameters
    * that cannot be determined until materialui_compute_entries_box()
    * has been called, we delegate this to materialui_scrollbar_init() */

   /* Get navigation bar layout
    * > Normally drawn at the bottom of the screen,
    *   but in landscape orientations should be placed
    *   on the right hand side */
   mui->nav_bar.width                  = mui->dip_base_unit_size / 3;
   mui->nav_bar.divider_width          = mui->entry_divider_width;
   mui->nav_bar.selection_marker_width = mui->nav_bar.width / 16;

   if (!mui->is_portrait && mui->last_auto_rotate_nav_bar)
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

   /* We assume the average glyph aspect ratio is close to 3:4 */
   mui->font_data.title.glyph_width = (int)((title_font_size * (3.0f / 4.0f)) + 0.5f);
   mui->font_data.list.glyph_width  = (int)((list_font_size  * (3.0f / 4.0f)) + 0.5f);
   mui->font_data.hint.glyph_width  = (int)((hint_font_size  * (3.0f / 4.0f)) + 0.5f);

   menu_display_set_header_height(new_header_height);

   if (mui->font_data.title.font)
   {
      menu_display_font_free(mui->font_data.title.font);
      mui->font_data.title.font = NULL;
   }
   if (mui->font_data.list.font)
   {
      menu_display_font_free(mui->font_data.list.font);
      mui->font_data.list.font = NULL;
   }
   if (mui->font_data.hint.font)
   {
      menu_display_font_free(mui->font_data.hint.font);
      mui->font_data.hint.font = NULL;
   }

   mui->font_data.title.font = menu_display_font(
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         title_font_size, video_is_threaded);

   mui->font_data.list.font  = menu_display_font(
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         list_font_size, video_is_threaded);

   mui->font_data.hint.font  = menu_display_font(
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
         hint_font_size, video_is_threaded);

   if (mui->font_data.title.font)
   {
      /* Calculate a more realistic ticker_limit */
      unsigned title_char_width =
         font_driver_get_message_width(mui->font_data.title.font, "a", 1, 1);

      if (title_char_width)
         mui->font_data.title.glyph_width = title_char_width;

      /* Get font height */
      mui->font_data.title.font_height = font_driver_get_line_height(mui->font_data.title.font, 1.0f);
   }

   if (mui->font_data.list.font)
   {
      /* Calculate a more realistic ticker_limit */
      unsigned list_char_width =
         font_driver_get_message_width(mui->font_data.list.font, "a", 1, 1);

      if (list_char_width)
         mui->font_data.list.glyph_width = list_char_width;

      /* Get font height */
      mui->font_data.list.font_height = font_driver_get_line_height(mui->font_data.list.font, 1.0f);
   }

   if (mui->font_data.hint.font)
   {
      /* Calculate a more realistic ticker_limit */
      unsigned hint_char_width =
         font_driver_get_message_width(mui->font_data.hint.font, "t", 1, 1);

      if (hint_char_width)
         mui->font_data.hint.glyph_width = hint_char_width;

      /* Get font height */
      mui->font_data.hint.font_height = font_driver_get_line_height(mui->font_data.hint.font, 1.0f);
   }

   /* When updating the layout, the system bar
    * cache must be invalidated (since all text
    * size will change) */
   mui->sys_bar_cache.battery_percent_str[0] = '\0';
   mui->sys_bar_cache.battery_percent_width  = 0;
   mui->sys_bar_cache.timedate_str[0]        = '\0';
   mui->sys_bar_cache.timedate_width         = 0;

   materialui_update_list_view(mui);

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

static void *materialui_init(void **userdata, bool video_is_threaded)
{
   unsigned width, height;
   settings_t *settings                   = config_get_ptr();
   materialui_handle_t *mui               = NULL;
   menu_handle_t *menu                    = (menu_handle_t*)calloc(1, sizeof(*menu));
   static const char* const ticker_spacer = MUI_TICKER_SPACER;

   if (!menu || !settings)
      return NULL;

   if (!menu_display_init_first_driver(video_is_threaded))
      goto error;

   mui = (materialui_handle_t*)calloc(1, sizeof(materialui_handle_t));

   if (!mui)
      goto error;

   *userdata = mui;

   /* Initialise thumbnail path data */
   mui->thumbnail_path_data = menu_thumbnail_path_init();
   if (!mui->thumbnail_path_data)
      goto error;

   /* Get DPI/screen-size-aware base unit size for
    * UI elements */
   video_driver_get_size(&width, &height);

   mui->last_width           = width;
   mui->last_height          = height;
   mui->last_scale_factor    = menu_display_get_dpi_scale(width, height);
   mui->dip_base_unit_size   = mui->last_scale_factor * MUI_DIP_BASE_UNIT_SIZE;

   mui->need_compute         = false;
   mui->is_playlist          = false;
   mui->is_file_list         = false;
   mui->is_dropdown_list     = false;
   mui->menu_stack_flushed   = false;

   mui->first_onscreen_entry = 0;
   mui->last_onscreen_entry  = 0;

   mui->menu_title[0]        = '\0';

   /* Set initial theme colours */
   mui->color_theme = (enum materialui_color_theme)settings->uints.menu_materialui_color_theme;
   materialui_prepare_colors(mui, (enum materialui_color_theme)mui->color_theme);

   /* Initial ticker configuration */
   mui->use_smooth_ticker           = settings->bools.menu_ticker_smooth;
   mui->ticker_smooth.font_scale    = 1.0f;
   mui->ticker_smooth.spacer        = ticker_spacer;
   mui->ticker_smooth.x_offset      = &mui->ticker_x_offset;
   mui->ticker_smooth.dst_str_width = &mui->ticker_str_width;
   mui->ticker.spacer               = ticker_spacer;

   /* Ensure menu animation parameters are properly
    * reset */
   mui->touch_feedback_selection        = 0;
   mui->touch_feedback_alpha            = 0.0f;
   mui->touch_feedback_update_selection = false;
   mui->transition_alpha                = 1.0f;
   mui->transition_x_offset             = 0.0f;
   mui->last_stack_size                 = 1;

   /* Ensure message box string is empty */
   mui->msgbox[0] = '\0';

   /* Initialise navigation bar */
   materialui_init_nav_bar(mui);

   /* Set initial thumbnail stream delay */
   menu_thumbnail_set_stream_delay(MUI_THUMBNAIL_STREAM_DELAY_DEFAULT);

   /* Ensure that fullscreen thumbnails are inactive */
   mui->show_fullscreen_thumbnails     = false;
   mui->fullscreen_thumbnail_selection = 0;
   mui->fullscreen_thumbnail_alpha     = 0.0f;
   mui->fullscreen_thumbnail_label[0]  = '\0';

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void materialui_free(void *data)
{
   materialui_handle_t *mui   = (materialui_handle_t*)data;

   if (!mui)
      return;

   video_coord_array_free(&mui->font_data.title.raster_block.carr);
   video_coord_array_free(&mui->font_data.list.raster_block.carr);
   video_coord_array_free(&mui->font_data.hint.raster_block.carr);

   font_driver_bind_block(NULL, NULL);

   if (mui->thumbnail_path_data)
      free(mui->thumbnail_path_data);
}

static void materialui_context_bg_destroy(materialui_handle_t *mui)
{
   if (!mui)
      return;

   video_driver_texture_unload(&mui->textures.bg);
   video_driver_texture_unload(&menu_display_white_texture);
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
      materialui_node_t *node = (materialui_node_t*)
            file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      menu_thumbnail_reset(&node->thumbnails.primary);
      menu_thumbnail_reset(&node->thumbnails.secondary);
   }
}

static void materialui_context_destroy(void *data)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;
   unsigned i;

   if (!mui)
      return;

   /* Free standard menu textures */
   for (i = 0; i < MUI_TEXTURE_LAST; i++)
      video_driver_texture_unload(&mui->textures.list[i]);

   /* Free fonts */
   if (mui->font_data.title.font)
      menu_display_font_free(mui->font_data.title.font);
   mui->font_data.title.font = NULL;

   if (mui->font_data.list.font)
      menu_display_font_free(mui->font_data.list.font);
   mui->font_data.list.font = NULL;

   if (mui->font_data.hint.font)
      menu_display_font_free(mui->font_data.hint.font);
   mui->font_data.hint.font = NULL;

   /* Free node thumbnails */
   materialui_reset_thumbnails();

   /* Free background/wallpaper textures */
   materialui_context_bg_destroy(mui);
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
      menu_display_allocate_white_texture();
   }

   return true;
}

static void materialui_animate_scroll(
      materialui_handle_t *mui, float scroll_pos, float duration)
{
   menu_animation_ctx_tag animation_tag = (uintptr_t)&mui->scroll_y;
   menu_animation_ctx_entry_t animation_entry;

   /* Kill any existing scroll animation */
   menu_animation_kill_by_tag(&animation_tag);

   /* mui->scroll_y will be modified by the animation
    * > Set scroll acceleration to zero to minimise
    *   potential conflicts */
   menu_input_set_pointer_y_accel(0.0f);

   /* Configure animation */
   animation_entry.easing_enum  = EASING_IN_OUT_QUAD;
   animation_entry.tag          = animation_tag;
   animation_entry.duration     = duration;
   animation_entry.target_value = scroll_pos;
   animation_entry.subject      = &mui->scroll_y;
   animation_entry.cb           = NULL;
   animation_entry.userdata     = NULL;

   /* Push animation */
   menu_animation_push(&animation_entry);
}

/* The navigation pointer has been updated (for example by pressing up or down
   on the keyboard) */
static void materialui_navigation_set(void *data, bool scroll)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;
    
   if (!mui || !scroll)
      return;

   materialui_animate_scroll(
         mui,
         materialui_get_scroll(mui),
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
   unsigned menu_tab_index = 0;

   /* Cache last active menu tab index */
   mui->nav_bar.last_active_menu_tab_index = mui->nav_bar.active_menu_tab_index;

   /* Back tab */
   mui->nav_bar.back_tab.enabled = menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL);

   /* Resume tab
    * > Menu driver must be alive at this point, and retroarch
    *   must be initialised, so all we have to do (or can do)
    *   is check whether a non-dummy core is loaded) */
   mui->nav_bar.resume_tab.enabled = !rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL);

   /* Menu tabs */

   /* > Main menu */
   mui->nav_bar.menu_tabs[menu_tab_index].type          =
         MUI_NAV_BAR_MENU_TAB_MAIN;
   mui->nav_bar.menu_tabs[menu_tab_index].texture_index =
         MUI_TEXTURE_TAB_MAIN;
   mui->nav_bar.menu_tabs[menu_tab_index].active        =
         string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));

   if(mui->nav_bar.menu_tabs[menu_tab_index].active)
      mui->nav_bar.active_menu_tab_index = menu_tab_index;

   menu_tab_index++;

   /* > Playlists */
   if (settings->bools.menu_content_show_playlists)
   {
      mui->nav_bar.menu_tabs[menu_tab_index].type          =
            MUI_NAV_BAR_MENU_TAB_PLAYLISTS;
      mui->nav_bar.menu_tabs[menu_tab_index].texture_index =
            MUI_TEXTURE_TAB_PLAYLISTS;
      mui->nav_bar.menu_tabs[menu_tab_index].active        =
            string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

      if(mui->nav_bar.menu_tabs[menu_tab_index].active)
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

   if(mui->nav_bar.menu_tabs[menu_tab_index].active)
      mui->nav_bar.active_menu_tab_index = menu_tab_index;

   menu_tab_index++;

   /* Cache current number of menu tabs */
   mui->nav_bar.num_menu_tabs = menu_tab_index;
}

static void materialui_init_transition_animation(
      materialui_handle_t *mui, settings_t *settings)
{
   size_t stack_size                   = materialui_list_get_size(mui, MENU_LIST_PLAIN);
   menu_animation_ctx_tag alpha_tag    = (uintptr_t)&mui->transition_alpha;
   menu_animation_ctx_tag x_offset_tag = (uintptr_t)&mui->transition_x_offset;
   menu_animation_ctx_entry_t alpha_entry;
   menu_animation_ctx_entry_t x_offset_entry;

   /* If animations are disabled, reset alpha/x offset
    * values and return immediately */
   if (settings->uints.menu_materialui_transition_animation ==
         MATERIALUI_TRANSITION_ANIM_NONE)
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
   menu_animation_kill_by_tag(&alpha_tag);
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
   menu_animation_push(&alpha_entry);

   /* Slide animation (x offset) */

   /* > Kill any existing animations and set
    *   initial x offset value */
   menu_animation_kill_by_tag(&x_offset_tag);
   mui->transition_x_offset = 0.0f;

   /* >> Menu tab 'reset' action - using navigation
    *    bar to switch directly from low level menu
    *    to a top level menu
    *    - We apply a standard 'back' animation here */
   if (mui->menu_stack_flushed)
   {
      if (settings->uints.menu_materialui_transition_animation !=
               MATERIALUI_TRANSITION_ANIM_FADE)
         mui->transition_x_offset = -1.0f;
   }
   /* >> Menu 'forward' action */
   else if (stack_size > mui->last_stack_size)
   {
      if (settings->uints.menu_materialui_transition_animation ==
               MATERIALUI_TRANSITION_ANIM_SLIDE)
         mui->transition_x_offset = 1.0f;
   }
   /* >> Menu 'back' action */
   else if (stack_size < mui->last_stack_size)
   {
      if (settings->uints.menu_materialui_transition_animation ==
               MATERIALUI_TRANSITION_ANIM_SLIDE)
         mui->transition_x_offset = -1.0f;
   }
   /* >> Menu tab 'switch' action - using navigation
    *    bar to switch between top level menus */
   else if ((stack_size == 1) &&
            (settings->uints.menu_materialui_transition_animation !=
                  MATERIALUI_TRANSITION_ANIM_FADE))
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
      menu_animation_push(&x_offset_entry);
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
      mui->is_file_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_LIST)) ||
                          string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES));

      if (!mui->is_file_list)
         mui->is_dropdown_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_SPECIAL)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_RESOLUTION)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE)) ||
                                 string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE));
   }

   /* If this is a playlist, then cache it */
   if (mui->is_playlist)
      mui->playlist = playlist_get_cached();
   else
      mui->playlist = NULL;

   /* Update navigation bar tabs */
   materialui_populate_nav_bar(mui, label, settings);

   /* Update list view/thumbnail parameters */
   materialui_update_list_view(mui);

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
    * whenever materialui_populate_entries() is called */
   mui->first_onscreen_entry = 0;
   mui->last_onscreen_entry  = 0;

   /* Note: mui->scroll_y position needs to be set here,
    * but we can't do this until materialui_compute_entries_box()
    * has been called. We therefore delegate it until mui->need_compute
    * is acted upon */
   mui->need_compute = true;
}

/* Context reset is called on launch or when a core is launched */
static void materialui_context_reset(void *data, bool is_threaded)
{
   materialui_handle_t *mui = (materialui_handle_t*)data;
   settings_t *settings     = config_get_ptr();

   if (!mui || !settings)
      return;

   materialui_layout(mui, is_threaded);
   materialui_context_bg_destroy(mui);
   menu_display_allocate_white_texture();
   materialui_context_reset_textures(mui);

   if (path_is_valid(settings->paths.path_menu_wallpaper))
      task_push_image_load(settings->paths.path_menu_wallpaper,
            video_driver_supports_rgba(), 0,
            menu_display_handle_wallpaper_upload, NULL);

   video_driver_monitor_reset();
}

static int materialui_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   materialui_handle_t *mui              = (materialui_handle_t*)userdata;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         if (!mui)
            return -1;
         mui->mouse_show = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         if (!mui)
            return -1;
         mui->mouse_show = false;
         break;
      case 0:
      default:
         break;
   }

   return -1;
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
      file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
      size_t selection           = menu_navigation_get_selection();
      menu_file_list_cbs_t *cbs  = selection_buf ?
            (menu_file_list_cbs_t*)file_list_get_actiondata_at_offset(
                  selection_buf, selection) : NULL;
      bool stack_flushed         = false;
      int ret                    = 0;

      /* Sanity check */
      if (!selection_buf || !menu_stack || !cbs)
         return -1;

      if (!cbs->action_content_list_switch)
         return -1;

      /* Perform pre-switch operations */
      stack_flushed = materialui_preswitch_tabs(mui, target_tab);

      /* Perform switch */
      ret = cbs->action_content_list_switch(
            selection_buf, menu_stack, "", "", 0);

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

static void materialui_switch_list_view(materialui_handle_t *mui);

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
          * > If this is a top level menu, left/right is
          *   used to switch tabs */
         if (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1)
         {
            materialui_switch_tabs(mui, NULL, action);
            new_action = MENU_ACTION_NOOP;
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
          * > If current selection is off screen,
          *   auto select *last* item */
         materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_LAST);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         /* Ascend alphabet (A towards Z)
          * > If current selection is off screen,
          *   auto select *first* item */
         materialui_auto_select_onscreen_entry(mui, MUI_ONSCREEN_ENTRY_FIRST);
         break;
      case MENU_ACTION_SCAN:
         /* 'Scan' command is used to cycle current
          * thumbnail view mode */
         materialui_switch_list_view(mui);
         new_action = MENU_ACTION_NOOP;
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
   enum menu_action new_action;

   if (!mui)
      generic_menu_entry_action(userdata, entry, i, action);

   /* Process input action */
   new_action = materialui_parse_menu_entry_action(mui, action);

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
      menu_entry_init(&new_entry);
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
   menu_displaylist_ctx_parse_entry_t entry;
   int ret                = -1;
   core_info_list_t *list = NULL;
   menu_handle_t *menu    = (menu_handle_t*)data;

   (void)userdata;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         {
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION, 0, 0);

            core_info_get_list(&list);
            if (core_info_list_num_info_files(list))
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
            rarch_system_info_t *system = runloop_get_system_info();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            entry.data            = menu;
            entry.info            = info;
            entry.parse_type      = PARSE_ACTION;
            entry.add_empty_entry = false;

            if (rarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL))
            {
               if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
               {
                  entry.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
                  menu_displaylist_setting(&entry);
               }
            }
            else
            {
               if (system->load_no_content)
               {
                  entry.enum_idx      = MENU_ENUM_LABEL_START_CORE;
                  menu_displaylist_setting(&entry);
               }

#ifndef HAVE_DYNAMIC
               if (frontend_driver_has_fork())
#endif
               {
                  if (settings->bools.menu_show_load_core)
                  {
                     entry.enum_idx      = MENU_ENUM_LABEL_CORE_LIST;
                     menu_displaylist_setting(&entry);
                  }
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_LIST;
               menu_displaylist_setting(&entry);

               if (menu_displaylist_has_subsystems())
               {
                  entry.enum_idx      = MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS;
                  menu_displaylist_setting(&entry);
               }
            }

            if (settings->bools.menu_content_show_history)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_load_disc)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_DISC;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_dump_disc)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_DUMP_DISC;
               menu_displaylist_setting(&entry);
            }

#if defined(HAVE_NETWORKING)
#ifdef HAVE_LAKKA
            entry.enum_idx      = MENU_ENUM_LABEL_UPDATE_LAKKA;
            menu_displaylist_setting(&entry);
#else
#ifdef HAVE_ONLINE_UPDATER
            if (settings->bools.menu_show_online_updater)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_ONLINE_UPDATER;
               menu_displaylist_setting(&entry);
            }
#endif
#endif

            if (settings->bools.menu_content_show_netplay)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_NETPLAY;
               menu_displaylist_setting(&entry);
            }
#endif
            if (settings->bools.menu_show_information)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_INFORMATION_LIST;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_configurations)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_CONFIGURATIONS_LIST;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_help)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_HELP_LIST;
               menu_displaylist_setting(&entry);
            }
#if !defined(IOS)

            if (settings->bools.menu_show_restart_retroarch)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_RESTART_RETROARCH;
               menu_displaylist_setting(&entry);
            }

            entry.enum_idx      = MENU_ENUM_LABEL_QUIT_RETROARCH;
            menu_displaylist_setting(&entry);
#endif
#if defined(HAVE_LAKKA)
            if (settings->bools.menu_show_reboot)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_REBOOT;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_shutdown)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_SHUTDOWN;
               menu_displaylist_setting(&entry);
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
      unsigned header_height     = menu_display_get_header_height();
      menu_animation_ctx_tag tag = (uintptr_t)&mui->scroll_y;
      unsigned width;
      unsigned height;
      int drag_margin_horz;
      int drag_margin_vert;

      video_driver_get_size(&width, &height);

      /* Check whether pointer down event is within
       * vertical list region */
      if ((y < header_height) ||
          (y > height - mui->nav_bar_layout_height))
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
      if (!mui->is_portrait)
      {
         if (mui->landscape_entry_margin <= mui->margin)
            drag_margin_horz += (int)mui->margin;
         else if (mui->landscape_entry_margin <= 2 * mui->margin)
            drag_margin_horz += (int)((2 * mui->margin) - mui->landscape_entry_margin);
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

      /* > Kill any existing scroll animations
       *   and reset scroll acceleration */
      menu_animation_kill_by_tag(&tag);
      menu_input_set_pointer_y_accel(0.0f);

      /* > Enable dragging */
      mui->scrollbar.dragged = true;

      /* Increase thumbnail stream delay
       * (prevents overloading the system with
       * hundreds of image requests...) */
      menu_thumbnail_set_stream_delay(MUI_THUMBNAIL_STREAM_DELAY_SCROLLBAR_DRAG);
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
      float display_height          = (int)height - (int)header_height - (int)mui->nav_bar_layout_height;
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
         menu_entry_init(&last_entry);
         last_entry.path_enabled       = false;
         last_entry.label_enabled      = false;
         last_entry.rich_label_enabled = false;
         last_entry.sublabel_enabled   = false;

         menu_entry_get(&last_entry, 0, selection, NULL, true);

         /* Parse entry */
         menu_entry_get_value(&last_entry, &entry_value);
         entry_type      = menu_entry_get_type_new(&last_entry);
         entry_file_type = msg_hash_to_file_type(msg_hash_calculate(entry_value));
         entry_value_type = materialui_get_entry_value_type(
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
static void materialui_switch_list_view(materialui_handle_t *mui)
{
   settings_t *settings                  = config_get_ptr();
   bool secondary_thumbnail_enabled_prev = mui->secondary_thumbnail_enabled;

   if (!settings)
      return;

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
      settings->uints.menu_materialui_thumbnail_view_portrait++;

      if (settings->uints.menu_materialui_thumbnail_view_portrait >=
            MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LAST)
         settings->uints.menu_materialui_thumbnail_view_portrait = 0;
   }
   else
   {
      settings->uints.menu_materialui_thumbnail_view_landscape++;

      if (settings->uints.menu_materialui_thumbnail_view_landscape >=
            MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LAST)
         settings->uints.menu_materialui_thumbnail_view_landscape = 0;
   }

   /* Update list view parameters */
   materialui_update_list_view(mui);

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
   unsigned header_height   = menu_display_get_header_height();
   size_t selection         = menu_navigation_get_selection();
   size_t entries_end       = menu_entries_get_size();
   materialui_handle_t *mui = (materialui_handle_t*)userdata;
   unsigned width;
   unsigned height;

   if (!mui)
      return -1;

   /* All input is ignored if user was previously
    * dragging the scrollbar */
   if (mui->scrollbar.dragged)
   {
      /* Must reset scroll acceleration, otherwise
       * list will continue to 'drift' in drag direction */
      menu_input_set_pointer_y_accel(0.0f);

      /* Reset thumbnail stream delay to default */
      menu_thumbnail_set_stream_delay(MUI_THUMBNAIL_STREAM_DELAY_DEFAULT);

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
               /* If this is a playlist or file list, enable
                * search functionality */
               if (mui->is_playlist || mui->is_file_list)
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
                     materialui_switch_list_view(mui);
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
            else if ((ptr < entries_end) &&
                     (x   > mui->landscape_entry_margin) &&
                     (x   < width - mui->landscape_entry_margin - mui->nav_bar_layout_width))
            {
               if (gesture == MENU_INPUT_GESTURE_TAP)
               {
                  /* A 'tap' always produces a menu action */

                  /* If current 'pointer' item is not active,
                   * activate it immediately */
                  if (ptr != selection)
                     menu_navigation_set_selection(ptr);

                  /* Perform a MENU_ACTION_SELECT on currently
                   * active item */
                  return materialui_menu_entry_action(mui, entry, (size_t)ptr, MENU_ACTION_SELECT);
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
         /* 'Reset to default' action */
         if ((ptr < entries_end) && (ptr == selection))
            return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_START);
         break;
      case MENU_INPUT_GESTURE_SWIPE_LEFT:
         {
            /* If we are at the top level, a swipe should
             * just switch between the three main menu screens
             * (i.e. we don't care which item is currently selected)
             * Note: For intuitive behaviour, a *left* swipe should
             * trigger a *right* navigation event */
            if (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1)
               return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_RIGHT);
            /* If we are displaying a playlist/file list/dropdown list,
             * swipes are used for fast navigation */
            else if (mui->is_playlist || mui->is_file_list || mui->is_dropdown_list)
               return materialui_pointer_up_swipe_horz_plain_list(
                     mui, entry, height, header_height, y,
                     selection, true);
            /* In all other cases, just perform a normal 'left'
             * navigation event */
            else
               return materialui_pointer_up_swipe_horz_default(
                     mui, entry, ptr, selection, entries_end, MENU_ACTION_LEFT);
         }
         break;
      case MENU_INPUT_GESTURE_SWIPE_RIGHT:
         {
            /* If we are at the top level, a swipe should
             * just switch between the three main menu screens
             * (i.e. we don't care which item is currently selected)
             * Note: For intuitive behaviour, a *right* swipe should
             * trigger a *left* navigation event */
            if (materialui_list_get_size(mui, MENU_LIST_PLAIN) == 1)
               return materialui_menu_entry_action(mui, entry, selection, MENU_ACTION_LEFT);
            /* If we are displaying a playlist/file list/dropdown list,
             * swipes are used for fast navigation */
            else if (mui->is_playlist || mui->is_file_list || mui->is_dropdown_list)
               return materialui_pointer_up_swipe_horz_plain_list(
                     mui, entry, height, header_height, y,
                     selection, false);
            /* In all other cases, just perform a normal 'right'
             * navigation event */
            else
               return materialui_pointer_up_swipe_horz_default(
                     mui, entry, ptr, selection, entries_end, MENU_ACTION_RIGHT);
         }
         break;
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
   bool thumbnail_reset     = false;

   if (!mui || !list)
      return;

   mui->need_compute = true;
   node = (materialui_node_t*)file_list_get_userdata_at_offset(list, i);

   if (!node)
      node = (materialui_node_t*)calloc(1, sizeof(materialui_node_t));
   else
   {
      /* If node already exists, must free any
       * existing thumbnail */
      menu_thumbnail_reset(&node->thumbnails.primary);
      menu_thumbnail_reset(&node->thumbnails.secondary);
      thumbnail_reset = true;
   }

   if (!node)
   {
      RARCH_ERR("GLUI node could not be allocated.\n");
      return;
   }

   node->has_icon           = false;
   node->icon_texture_index = 0;
   node->entry_height       = 0.0f;
   node->text_height        = 0.0f;
   node->y                  = 0.0f;

   if (!thumbnail_reset)
   {
      menu_thumbnail_reset(&node->thumbnails.primary);
      menu_thumbnail_reset(&node->thumbnails.secondary);
   }

   if (settings->bools.menu_materialui_icons_enable)
   {
      switch (type)
      {
         case MENU_SET_CDROM_INFO:
         case MENU_SET_CDROM_LIST:
         case MENU_SET_LOAD_CDROM_LIST:
            node->icon_texture_index = MUI_TEXTURE_DISK;
            node->has_icon           = true;
            break;
         case FILE_TYPE_DOWNLOAD_CORE:
         case FILE_TYPE_CORE:
            node->icon_texture_index = MUI_TEXTURE_CORES;
            node->has_icon           = true;
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
         case FILE_TYPE_DOWNLOAD_PL_THUMBNAIL_CONTENT:
            node->icon_texture_index = MUI_TEXTURE_IMAGE;
            node->has_icon           = true;
            break;
         case FILE_TYPE_PARENT_DIRECTORY:
            node->icon_texture_index = MUI_TEXTURE_PARENT_DIRECTORY;
            node->has_icon           = true;
            break;
         case FILE_TYPE_PLAYLIST_COLLECTION:
            node->icon_texture_index = MUI_TEXTURE_PLAYLIST;
            node->has_icon           = true;
            break;
         case FILE_TYPE_RDB:
            node->icon_texture_index = MUI_TEXTURE_DATABASE;
            node->has_icon           = true;
            break;
         case FILE_TYPE_RDB_ENTRY:
            node->icon_texture_index = MUI_TEXTURE_SETTINGS;
            node->has_icon           = true;
            break;
         case FILE_TYPE_IN_CARCHIVE:
         case FILE_TYPE_PLAIN:
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
            node->icon_texture_index = MUI_TEXTURE_FILE;
            node->has_icon           = true;
            break;
         case FILE_TYPE_MUSIC:
            node->icon_texture_index = MUI_TEXTURE_MUSIC;
            node->has_icon           = true;
            break;
         case FILE_TYPE_MOVIE:
            node->icon_texture_index = MUI_TEXTURE_VIDEO;
            node->has_icon           = true;
            break;
         case FILE_TYPE_DIRECTORY:
         case FILE_TYPE_DOWNLOAD_URL:
            node->icon_texture_index = MUI_TEXTURE_FOLDER;
            node->has_icon           = true;
            break;
         case MENU_ROOM_LAN:
         case MENU_ROOM_RELAY:
         case MENU_ROOM:
            node->icon_texture_index = MUI_TEXTURE_SETTINGS;
            node->has_icon           = true;
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
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DATABASE_MANAGER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CURSOR_MANAGER_LIST))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_DATABASE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES)))
            {
               node->icon_texture_index = MUI_TEXTURE_IMAGE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC)))
            {
               node->icon_texture_index = MUI_TEXTURE_MUSIC;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_VIDEO;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY)))
            {
               node->icon_texture_index = MUI_TEXTURE_SCAN;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY)))
            {
               node->icon_texture_index = MUI_TEXTURE_HISTORY;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_HELP;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESTART_CONTENT)))
            {
               node->icon_texture_index = MUI_TEXTURE_RESTART;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESUME_CONTENT)))
            {
               node->icon_texture_index = MUI_TEXTURE_RESUME;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CLOSE_CONTENT)))
            {
               node->icon_texture_index = MUI_TEXTURE_CLOSE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_CORE_OPTIONS;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_CORE_CHEAT_OPTIONS;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_CONTROLS;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SHADER_OPTIONS)))
            {
               node->icon_texture_index = MUI_TEXTURE_SHADERS;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_LIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_CORES;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RUN)))
            {
               node->icon_texture_index = MUI_TEXTURE_RUN;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_ADD_TO_FAVORITES;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RENAME_ENTRY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_RENAME;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_ADD_TO_MIXER;
               node->has_icon           = true;
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
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_STATE))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_LOAD_STATE;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_TRAY_EJECT)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_TRAY_INSERT))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_EJECT;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_IMAGE_APPEND)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_DISC)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DUMP_DISC)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISC_INFORMATION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_OPTIONS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DISK_INDEX))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_DISK;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_STATE))
                  ||
                  (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE)))
                  ||
                  (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR)))
                  ||
                  (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS)))
                  ||
                  (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME)))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_SAVE_STATE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UNDO_LOAD_STATE)))
            {
               node->icon_texture_index = MUI_TEXTURE_UNDO_LOAD_STATE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UNDO_SAVE_STATE)))
            {
               node->icon_texture_index = MUI_TEXTURE_UNDO_SAVE_STATE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_STATE_SLOT)))
            {
               node->icon_texture_index = MUI_TEXTURE_STATE_SLOT;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT)))
            {
               node->icon_texture_index = MUI_TEXTURE_TAKE_SCREENSHOT;
               node->has_icon           = true;
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
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_LIST))
                  ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_LOAD_CONTENT;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DELETE_ENTRY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DELETE_PLAYLIST)))
            {
               node->icon_texture_index = MUI_TEXTURE_REMOVE;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_HOSTING_SETTINGS)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_INFORMATION))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_NETPLAY;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS)))
            {
               node->icon_texture_index = MUI_TEXTURE_QUICKMENU;
               node->has_icon           = true;
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
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS))
                  )
                  {
                     node->icon_texture_index = MUI_TEXTURE_UPDATER;
                     node->has_icon           = true;
                  }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_ADD;
               node->has_icon           = true;
            }
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_QUIT_RETROARCH)) ||
                     string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RESTART_RETROARCH))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_QUIT;
               node->has_icon           = true;
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
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_OUTPUT_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_SYNCHRONIZATION_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MENU_SOUNDS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_MENU_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS)) ||
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
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_YOUTUBE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_TWITCH)) ||
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
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_REWIND_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_UPDATER_SETTINGS))        ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SET_CORE_ASSOCIATION)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SHADER_APPLY_CHANGES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS)) ||
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
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST))
                  )
                  {
                     node->icon_texture_index = MUI_TEXTURE_SETTINGS;
                     node->has_icon           = true;
                  }
            else if (type >= MENU_SETTINGS_REMAPPING_PORT_BEGIN && 
                  type <= MENU_SETTINGS_REMAPPING_PORT_END)
            {
               node->icon_texture_index = MUI_TEXTURE_SETTINGS;
               node->has_icon           = true;
            }
            else if (
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES)) ||
                  string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST))
                  )
            {
               node->icon_texture_index = MUI_TEXTURE_FOLDER;
               node->has_icon           = true;
            }
            else if (strcasestr(label, "_input_binds_list"))
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
                     node->has_icon           = true;
                  }
               }
            }
            break;
      }
   }

   file_list_set_userdata(list, i, node);
}

/* Clears the current menu list */
static void materialui_list_clear(file_list_t *list)
{
   size_t i;
   size_t size = list ? list->size : 0;

   /* Must cancel pending thumbnail requests before
    * freeing node->thumbnails objects */
   menu_thumbnail_cancel_pending_requests();

   for (i = 0; i < size; i++)
   {
      materialui_node_t *node = (materialui_node_t*)
            file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      menu_thumbnail_reset(&node->thumbnails.primary);
      menu_thumbnail_reset(&node->thumbnails.secondary);
      file_list_free_userdata(list, i);
   }
}

static void materialui_set_thumbnail_system(void *userdata, char *s, size_t len)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;
   if (!mui)
      return;
   menu_thumbnail_set_system(
         mui->thumbnail_path_data, s, playlist_get_cached());
}

static void materialui_get_thumbnail_system(void *userdata, char *s, size_t len)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;
   const char *system       = NULL;
   if (!mui)
      return;
   if (menu_thumbnail_get_system(mui->thumbnail_path_data, &system))
      strlcpy(s, system, len);
}

static void materialui_refresh_thumbnail_image(void *userdata, unsigned i)
{
   materialui_handle_t *mui = (materialui_handle_t*)userdata;

   if (!mui)
      return;

   /* Only refresh thumbnails if we are currently viewing
    * a playlist with thumbnails enabled */
   if ((mui->list_view_type == MUI_LIST_VIEW_DEFAULT) ||
       (mui->list_view_type == MUI_LIST_VIEW_PLAYLIST))
      return;

   /* Only refresh thumbnails if the current entry is
    * on-screen */
   if (materialui_entry_onscreen(mui, (size_t)i))
   {
      file_list_t *list       = menu_entries_get_selection_buf_ptr(0);
      materialui_node_t *node = NULL;
      float stream_delay      = menu_thumbnail_get_stream_delay();

      if (!list)
         return;

      node = (materialui_node_t*)file_list_get_userdata_at_offset(list, (size_t)i);

      if (!node)
         return;

      /* Reset existing thumbnails */
      menu_thumbnail_reset(&node->thumbnails.primary);
      menu_thumbnail_reset(&node->thumbnails.secondary);

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
   generic_menu_iterate,
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
   generic_menu_init_list,
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
   menu_display_osk_ptr_at_pos,
   NULL, /* update_savestate_thumbnail_path */
   NULL, /* update_savestate_thumbnail_image */
   materialui_pointer_down,
   materialui_pointer_up,
   NULL, /* get_load_content_animation_data */
   materialui_menu_entry_action
};

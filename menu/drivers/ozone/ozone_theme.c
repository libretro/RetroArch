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

/* Required for default theme setting */
#include "../../../config.def.h"

#include "ozone.h"
#include "ozone_theme.h"
#include "ozone_display.h"

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

static float ozone_sidebar_gradient_top_nord[16] = {
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
      0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
};

static float ozone_sidebar_gradient_bottom_nord[16] = {
      0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
      0.1921569f, 0.2196078f, 0.2705882f, 0.9f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
};

static float ozone_sidebar_gradient_top_gruvbox_dark[16] = {
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
      0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
};

static float ozone_sidebar_gradient_bottom_gruvbox_dark[16] = {
      0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
      0.1686275f, 0.1686275f, 0.1686275f, 0.9f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
};

static float ozone_sidebar_background_light[16] = {
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
      0.94, 0.94, 0.94, 1.00,
};

static float ozone_sidebar_background_dark[16] = {
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
      0.2, 0.2, 0.2, 1.00,
};

static float ozone_sidebar_background_nord[16] = {
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
      0.2078431f, 0.2352941f, 0.2901961f, 1.0f,
};

static float ozone_sidebar_background_gruvbox_dark[16] = {
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
      0.1960784f, 0.1882353f, 0.1843137f, 1.0f,
};

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

static float ozone_background_libretro_running_nord[16] = {
   0.1803922f, 0.2039216f, 0.2509804f, 0.75f,
   0.1803922f, 0.2039216f, 0.2509804f, 0.75f,
   0.1803922f, 0.2039216f, 0.2509804f, 1.0f,
   0.1803922f, 0.2039216f, 0.2509804f, 1.0f,
};

static float ozone_background_libretro_running_gruvbox_dark[16] = {
   0.1568627f, 0.1568627f, 0.1568627f, 0.75f,
   0.1568627f, 0.1568627f, 0.1568627f, 0.75f,
   0.1568627f, 0.1568627f, 0.1568627f, 1.0f,
   0.1568627f, 0.1568627f, 0.1568627f, 1.0f,
};

static float ozone_background_libretro_running_boysenberry[16] = {
      0.27058823529, 0.09803921568, 0.14117647058, 0.75f,
      0.27058823529, 0.09803921568, 0.14117647058, 0.75f,
      0.27058823529, 0.09803921568, 0.14117647058, 0.75f,
      0.27058823529, 0.09803921568, 0.14117647058, 0.75f,
};

static float ozone_sidebar_background_boysenberry[16] = {
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
};

static float ozone_sidebar_gradient_top_boysenberry[16] = {
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.19215686274, 0.0, 0.04705882352, 1.00,
      0.19215686274, 0.0, 0.04705882352, 1.00,
};

static float ozone_sidebar_gradient_bottom_boysenberry[16] = {
      0.19215686274, 0.0, 0.04705882352, 1.00,
      0.19215686274, 0.0, 0.04705882352, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,
      0.27058823529, 0.09803921568, 0.14117647058, 1.00,      
};

static float ozone_background_libretro_running_hacking_the_kernel[16] = {
      0.0, 0.0666666f, 0.0, 0.75f,
      0.0, 0.0666666f, 0.0, 0.75f,
      0.0, 0.0666666f, 0.0, 1.0f,
      0.0, 0.0666666f, 0.0, 1.0f,
};

static float ozone_sidebar_background_hacking_the_kernel[16] = {
      0.0, 0.1333333f, 0.0, 1.0f,
      0.0, 0.1333333f, 0.0, 1.0f,
      0.0, 0.1333333f, 0.0, 1.0f,
      0.0, 0.1333333f, 0.0, 1.0f,
};

static float ozone_sidebar_gradient_top_hacking_the_kernel[16] = {
      0.0, 0.13333333, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
};

static float ozone_sidebar_gradient_bottom_hacking_the_kernel[16] = {
      0.0, 0.0666666f, 0.0, 1.0f,
      0.0, 0.0666666f, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
      0.0, 0.13333333, 0.0, 1.0f,
};

static float ozone_background_libretro_running_twilight_zone[16] = {
      0.0078431, 0.0, 0.0156862, 0.75f,
      0.0078431, 0.0, 0.0156862, 0.75f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
};

static float ozone_sidebar_background_twilight_zone[16] = {
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
};

static float ozone_sidebar_gradient_top_twilight_zone[16] = {
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
};

static float ozone_sidebar_gradient_bottom_twilight_zone[16] = {
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
      0.0078431, 0.0, 0.0156862, 1.0f,
};

static float ozone_background_libretro_running_dracula[16] = {
      0.1568627, 0.1647058, 0.2117647, 0.75f,
      0.1568627, 0.1647058, 0.2117647, 0.75f,
      0.1568627, 0.1647058, 0.2117647, 1.0f,
      0.1568627, 0.1647058, 0.2117647, 1.0f,
};

static float ozone_sidebar_background_dracula[16] = {
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
};

static float ozone_sidebar_gradient_top_dracula[16] = {
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
};

static float ozone_sidebar_gradient_bottom_dracula[16] = {
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
      0.2666666, 0.2784314, 0.3529412, 1.0f,
};


static float ozone_border_0_light[16] = COLOR_HEX_TO_FLOAT(0x50EFD9, 1.00);
static float ozone_border_1_light[16] = COLOR_HEX_TO_FLOAT(0x0DB6D5, 1.00);

static float ozone_border_0_dark[16] = COLOR_HEX_TO_FLOAT(0x198AC6, 1.00);
static float ozone_border_1_dark[16] = COLOR_HEX_TO_FLOAT(0x89F1F2, 1.00);

static float ozone_border_0_nord[16] = COLOR_HEX_TO_FLOAT(0x5E81AC, 1.0f);
static float ozone_border_1_nord[16] = COLOR_HEX_TO_FLOAT(0x88C0D0, 1.0f);

static float ozone_border_0_gruvbox_dark[16] = COLOR_HEX_TO_FLOAT(0xAF3A03, 1.0f);
static float ozone_border_1_gruvbox_dark[16] = COLOR_HEX_TO_FLOAT(0xFE8019, 1.0f);

static float ozone_border_0_boysenberry[16] = COLOR_HEX_TO_FLOAT(0x50EFD9, 1.00);
static float ozone_border_1_boysenberry[16] = COLOR_HEX_TO_FLOAT(0x0DB6D5, 1.00);

static float ozone_border_0_hacking_the_kernel[16] = COLOR_HEX_TO_FLOAT(0x008C00, 1.0f);
static float ozone_border_1_hacking_the_kernel[16] = COLOR_HEX_TO_FLOAT(0x00E000, 1.0f);

static float ozone_border_0_twilight_zone[16] = COLOR_HEX_TO_FLOAT(0xC3A0E0, 1.0f);
static float ozone_border_1_twilight_zone[16] = COLOR_HEX_TO_FLOAT(0x9B61CC, 1.0f);

static float ozone_border_0_dracula[16] = COLOR_HEX_TO_FLOAT(0xC3A0E0, 1.0f);
static float ozone_border_1_dracula[16] = COLOR_HEX_TO_FLOAT(0x9B61CC, 1.0f);


ozone_theme_t ozone_theme_light = {
   COLOR_HEX_TO_FLOAT(0xEBEBEB, 1.00),                   /* background */
   ozone_background_libretro_running_light,              /* background_libretro_running */

   COLOR_HEX_TO_FLOAT(0x2B2B2B, 1.00),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0x333333, 1.00),                   /* text */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x10BEC5, 1.00),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0xCDCDCD, 1.00),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0x333333, 1.00),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x374CFF, 1.00),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0xF0F0F0, 1.00),                   /* message_background */

   0x333333FF,                                           /* text_rgba */
   0x374CFFFF,                                           /* text_selected_rgba */
   0x878787FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xEBEBEB,                                             /* screensaver_tint */

   ozone_sidebar_background_light,                       /* sidebar_background */
   ozone_sidebar_gradient_top_light,                     /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_light,                  /* sidebar_bottom_gradient */

   ozone_border_0_light,                                 /* cursor_border_0 */
   ozone_border_1_light,                                 /* cursor_border_1 */

   {0},                                                  /* textures */

   "light"                                               /* name */
};

ozone_theme_t ozone_theme_dark = {
   COLOR_HEX_TO_FLOAT(0x2D2D2D, 1.00),                   /* background */
   ozone_background_libretro_running_dark,               /* background_libretro_running */

   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),                   /* text */
   COLOR_HEX_TO_FLOAT(0x212227, 1.00),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x2DA3CB, 1.00),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x51514F, 1.00),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.00),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x00D9AE, 1.00),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x464646, 1.00),                   /* message_background */

   0xFFFFFFFF,                                           /* text_rgba */
   0x00FFC5FF,                                           /* text_selected_rgba */
   0x9F9FA1FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xFFFFFF,                                             /* screensaver_tint */

   ozone_sidebar_background_dark,                        /* sidebar_background */
   ozone_sidebar_gradient_top_dark,                      /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_dark,                   /* sidebar_bottom_gradient */

   ozone_border_0_dark,                                  /* cursor_border_0 */
   ozone_border_1_dark,                                  /* cursor_border_1 */

   {0},                                                  /* textures */

   "dark"                                                /* name */
};

ozone_theme_t ozone_theme_nord = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x2E3440, 1.0f),                   /* background */
   ozone_background_libretro_running_nord,               /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0xD8DEE9, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xECEFF4, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x232730, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x73A1BE, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x4C566A, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xE5E9F0, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xA9C791, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x434C5E, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xECEFF4FF,                                           /* text_rgba */
   0xA9C791FF,                                           /* text_selected_rgba */
   0x8FBCBBFF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xECEFF4,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_nord,                        /* sidebar_background */
   ozone_sidebar_gradient_top_nord,                      /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_nord,                   /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_nord,                                  /* cursor_border_0 */
   ozone_border_1_nord,                                  /* cursor_border_1 */

   {0},                                                  /* textures */

   "nord"                                                /* name */
};

ozone_theme_t ozone_theme_gruvbox_dark = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x282828, 1.0f),                   /* background */
   ozone_background_libretro_running_gruvbox_dark,       /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0xD5C4A1, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xEBDBB2, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x1D2021, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0xD75D0E, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x665C54, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xEBDBB2, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x8EC07C, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x32302F, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xEBDBB2FF,                                           /* text_rgba */
   0x8EC07CFF,                                           /* text_selected_rgba */
   0xD79921FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xEBDBB2,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_gruvbox_dark,                /* sidebar_background */
   ozone_sidebar_gradient_top_gruvbox_dark,              /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_gruvbox_dark,           /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_gruvbox_dark,                          /* cursor_border_0 */
   ozone_border_1_gruvbox_dark,                          /* cursor_border_1 */

   {0},                                                  /* textures */

   "gruvbox_dark"                                        /* name */
};

ozone_theme_t ozone_theme_boysenberry = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x31000C, 1.0f),                   /* background */
   ozone_background_libretro_running_boysenberry,        /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x85535F, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xEBDBB2, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x4E2A35, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0xD599FF, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x73434C, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFEBCFF, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xD599FF, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x32302F, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xFEBCFFFF,                                           /* text_rgba */
   0xFEBCFFFF,                                           /* text_selected_rgba */
   0xD599FFFF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xFEBCFF,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_boysenberry,                 /* sidebar_background */
   ozone_sidebar_gradient_top_boysenberry,               /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_boysenberry,            /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_boysenberry,                           /* cursor_border_0 */
   ozone_border_1_boysenberry,                           /* cursor_border_1 */

   {0},                                                  /* textures */

   "boysenberry"                                         /* name */
};

ozone_theme_t ozone_theme_hacking_the_kernel = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x001100, 1.0f),                   /* background */
   ozone_background_libretro_running_hacking_the_kernel, /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x17C936, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0x00FF29, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x003400, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x1BDA3C, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x008C00, 0.1f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0x00FF00, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0x8EC07C, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x0D0E0F, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0x00E528FF,                                           /* text_rgba */
   0x83FF83FF,                                           /* text_selected_rgba */
   0x53E63DFF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0x00E528,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_hacking_the_kernel,          /* sidebar_background */
   ozone_sidebar_gradient_top_hacking_the_kernel,        /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_hacking_the_kernel,     /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_hacking_the_kernel,                    /* cursor_border_0 */
   ozone_border_1_hacking_the_kernel,                    /* cursor_border_1 */

   {0},                                                  /* textures */

   "hacking_the_kernel"                                  /* name */
};

ozone_theme_t ozone_theme_twilight_zone = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x020004, 1.0f),                   /* background */
   ozone_background_libretro_running_twilight_zone,      /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x5B5069, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xF7F0FA, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x232038, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0x9B61CC, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0xC27AFF, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xFFFFFF, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xB78CC8, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0xB78CC8, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xFDFCFEFF,                                           /* text_rgba */
   0xB78CC8FF,                                           /* text_selected_rgba */
   0x9A6C99FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xFDFCFE,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_twilight_zone,               /* sidebar_background */
   ozone_sidebar_gradient_top_twilight_zone,             /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_twilight_zone,          /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_twilight_zone,                         /* cursor_border_0 */
   ozone_border_1_twilight_zone,                         /* cursor_border_1 */

   {0},                                                  /* textures */

   "twilight_zone"                                       /* name */
};

ozone_theme_t ozone_theme_dracula = {
   /* Background color */
   COLOR_HEX_TO_FLOAT(0x282A36, 1.0f),                   /* background */
   ozone_background_libretro_running_dracula,            /* background_libretro_running */

   /* Float colors for quads and icons */
   COLOR_HEX_TO_FLOAT(0x44475A, 1.0f),                   /* header_footer_separator */
   COLOR_HEX_TO_FLOAT(0xF8F8F2, 1.0f),                   /* text */
   COLOR_HEX_TO_FLOAT(0x44475A, 1.0f),                   /* selection */
   COLOR_HEX_TO_FLOAT(0xBD93F9, 1.0f),                   /* selection_border */
   COLOR_HEX_TO_FLOAT(0x44475A, 1.0f),                   /* entries_border */
   COLOR_HEX_TO_FLOAT(0xF8F8F2, 1.0f),                   /* entries_icon */
   COLOR_HEX_TO_FLOAT(0xF8F8F2, 1.0f),                   /* text_selected */
   COLOR_HEX_TO_FLOAT(0x6272A4, 1.0f),                   /* message_background */

   /* RGBA colors for text */
   0xF8F8F2FF,                                           /* text_rgba */
   0xFF79C6FF,                                           /* text_selected_rgba */
   0xBD93F9FF,                                           /* text_sublabel_rgba */

   /* Screensaver 'tint' (RGB24) */
   0xF8F8F2,                                             /* screensaver_tint */

   /* Sidebar color */
   ozone_sidebar_background_dracula,                     /* sidebar_background */
   ozone_sidebar_gradient_top_dracula,                   /* sidebar_top_gradient */
   ozone_sidebar_gradient_bottom_dracula,                /* sidebar_bottom_gradient */

   /* Fancy cursor colors */
   ozone_border_0_dracula,                               /* cursor_border_0 */
   ozone_border_1_dracula,                               /* cursor_border_1 */

   {0},                                                  /* textures */

   "dracula"                                             /* name */
};


ozone_theme_t *ozone_themes[] = {
   &ozone_theme_light,
   &ozone_theme_dark,
   &ozone_theme_nord,
   &ozone_theme_gruvbox_dark,
   &ozone_theme_boysenberry,
   &ozone_theme_hacking_the_kernel,
   &ozone_theme_twilight_zone,
   &ozone_theme_dracula

};

const unsigned ozone_themes_count           = ARRAY_SIZE(ozone_themes);
/* TODO/FIXME - global variables referenced outside */
unsigned last_color_theme                   = 0;
bool last_use_preferred_system_color_theme  = false;
ozone_theme_t *ozone_default_theme          = &ozone_theme_dark; /* also used as a tag for cursor animation */
/* Enable runtime configuration of framebuffer
 * opacity */
float last_framebuffer_opacity               = -1.0f;

void ozone_set_color_theme(ozone_handle_t *ozone, unsigned color_theme)
{
   ozone_theme_t *theme = ozone_default_theme;

   if (!ozone)
      return;

   switch (color_theme)
   {
      case OZONE_COLOR_THEME_BASIC_WHITE:
         theme = &ozone_theme_light;
         break;
      case OZONE_COLOR_THEME_BASIC_BLACK:
         theme = &ozone_theme_dark;
         break;
      case OZONE_COLOR_THEME_NORD:
         theme = &ozone_theme_nord;
         break;
      case OZONE_COLOR_THEME_GRUVBOX_DARK:
         theme = &ozone_theme_gruvbox_dark;
         break;
      case OZONE_COLOR_THEME_BOYSENBERRY:
         theme = &ozone_theme_boysenberry;
         break;
      case OZONE_COLOR_THEME_HACKING_THE_KERNEL:
         theme = &ozone_theme_hacking_the_kernel;
         break;
      case OZONE_COLOR_THEME_TWILIGHT_ZONE:
         theme = &ozone_theme_twilight_zone;
         break;
      case OZONE_COLOR_THEME_DRACULA:
         theme = &ozone_theme_dracula;
         break;
      default:
         break;
   }

   ozone->theme = theme;

   memcpy(ozone->theme_dynamic.selection_border, ozone->theme->selection_border, sizeof(ozone->theme_dynamic.selection_border));
   memcpy(ozone->theme_dynamic.selection, ozone->theme->selection, sizeof(ozone->theme_dynamic.selection));
   memcpy(ozone->theme_dynamic.entries_border, ozone->theme->entries_border, sizeof(ozone->theme_dynamic.entries_border));
   memcpy(ozone->theme_dynamic.entries_icon, ozone->theme->entries_icon, sizeof(ozone->theme_dynamic.entries_icon));
   memcpy(ozone->theme_dynamic.entries_checkmark, ozone->pure_white, sizeof(ozone->theme_dynamic.entries_checkmark));
   memcpy(ozone->theme_dynamic.cursor_alpha, ozone->pure_white, sizeof(ozone->theme_dynamic.cursor_alpha));
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
#else
   return DEFAULT_OZONE_COLOR_THEME;
#endif
}

void ozone_set_background_running_opacity(
      ozone_handle_t *ozone, float framebuffer_opacity)
{
   static float background_running_alpha_top    = 1.0f;
   static float background_running_alpha_bottom = 0.75f;
   float *background                            = NULL;

   if (!ozone || !ozone->theme->background_libretro_running)
      return;

   background                      = 
      ozone->theme->background_libretro_running;

   /* When content is running, background is a
    * gradient that from top to bottom transitions
    * from maximum to minimum opacity
    * > RetroArch default 'framebuffer_opacity'
    *   is 0.900. At this setting:
    *   - Background top has an alpha of 1.0
    *   - Background bottom has an alpha of 0.75 */
   background_running_alpha_top    = framebuffer_opacity / 0.9f;
   background_running_alpha_top    = (background_running_alpha_top > 1.0f) ?
         1.0f : (background_running_alpha_top < 0.0f) ?
               0.0f : background_running_alpha_top;

   background_running_alpha_bottom = (2.5f * framebuffer_opacity) - 1.5f;
   background_running_alpha_bottom = (background_running_alpha_bottom > 1.0f) ?
         1.0f : (background_running_alpha_bottom < 0.0f) ?
               0.0f : background_running_alpha_bottom;

   background[11]                  = background_running_alpha_top;
   background[15]                  = background_running_alpha_top;
   background[3]                   = background_running_alpha_bottom;
   background[7]                   = background_running_alpha_bottom;

   last_framebuffer_opacity        = framebuffer_opacity;
}

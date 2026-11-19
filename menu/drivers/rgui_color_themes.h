/*  RetroArch - A frontend for libretro.
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

#ifndef __RGUI_COLOR_THEMES_H
#define __RGUI_COLOR_THEMES_H

/* X(identifier, theme, label) in menu order. Old numeric configs
 * map through the frozen tables in configuration.c instead. */
#define RGUI_COLOR_THEME_LIST(X) \
   X("custom",             RGUI_THEME_CUSTOM,             MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM) \
   X("classic_red",        RGUI_THEME_CLASSIC_RED,        MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED) \
   X("classic_orange",     RGUI_THEME_CLASSIC_ORANGE,     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE) \
   X("classic_yellow",     RGUI_THEME_CLASSIC_YELLOW,     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW) \
   X("classic_green",      RGUI_THEME_CLASSIC_GREEN,      MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN) \
   X("classic_blue",       RGUI_THEME_CLASSIC_BLUE,       MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE) \
   X("classic_violet",     RGUI_THEME_CLASSIC_VIOLET,     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET) \
   X("classic_grey",       RGUI_THEME_CLASSIC_GREY,       MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY) \
   X("legacy_red",         RGUI_THEME_LEGACY_RED,         MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED) \
   X("dark_purple",        RGUI_THEME_DARK_PURPLE,        MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE) \
   X("midnight_blue",      RGUI_THEME_MIDNIGHT_BLUE,      MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE) \
   X("golden",             RGUI_THEME_GOLDEN,             MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN) \
   X("electric_blue",      RGUI_THEME_ELECTRIC_BLUE,      MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE) \
   X("apple_green",        RGUI_THEME_APPLE_GREEN,        MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN) \
   X("volcanic_red",       RGUI_THEME_VOLCANIC_RED,       MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED) \
   X("lagoon",             RGUI_THEME_LAGOON,             MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON) \
   X("brogrammer",         RGUI_THEME_BROGRAMMER,         MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER) \
   X("dracula",            RGUI_THEME_DRACULA,            MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA) \
   X("fairyfloss",         RGUI_THEME_FAIRYFLOSS,         MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS) \
   X("flatui",             RGUI_THEME_FLATUI,             MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI) \
   X("gruvbox_dark",       RGUI_THEME_GRUVBOX_DARK,       MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK) \
   X("gruvbox_light",      RGUI_THEME_GRUVBOX_LIGHT,      MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT) \
   X("hacking_the_kernel", RGUI_THEME_HACKING_THE_KERNEL, MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL) \
   X("nord",               RGUI_THEME_NORD,               MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD) \
   X("nova",               RGUI_THEME_NOVA,               MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NOVA) \
   X("one_dark",           RGUI_THEME_ONE_DARK,           MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK) \
   X("palenight",          RGUI_THEME_PALENIGHT,          MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT) \
   X("solarized_dark",     RGUI_THEME_SOLARIZED_DARK,     MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK) \
   X("solarized_light",    RGUI_THEME_SOLARIZED_LIGHT,    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT) \
   X("tango_dark",         RGUI_THEME_TANGO_DARK,         MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK) \
   X("tango_light",        RGUI_THEME_TANGO_LIGHT,        MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT) \
   X("zenburn",            RGUI_THEME_ZENBURN,            MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ZENBURN) \
   X("anti_zenburn",       RGUI_THEME_ANTI_ZENBURN,       MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ANTI_ZENBURN) \
   X("flux",               RGUI_THEME_FLUX,               MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLUX) \
   X("dynamic",            RGUI_THEME_DYNAMIC,            MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC) \
   X("gray_dark",          RGUI_THEME_GRAY_DARK,          MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK) \
   X("gray_light",         RGUI_THEME_GRAY_LIGHT,         MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT) \
   X("evergarden",         RGUI_THEME_EVERGARDEN,         MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_EVERGARDEN)

#endif

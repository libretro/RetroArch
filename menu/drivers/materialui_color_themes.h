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

#ifndef __MATERIALUI_COLOR_THEMES_H
#define __MATERIALUI_COLOR_THEMES_H

/* X(identifier, theme, label) in menu order. Old numeric configs
 * map through the frozen tables in configuration.c instead. */
#define MATERIALUI_COLOR_THEME_LIST(X) \
   X("blue",               MATERIALUI_THEME_BLUE,               MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE) \
   X("blue_grey",          MATERIALUI_THEME_BLUE_GREY,          MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY) \
   X("dark_blue",          MATERIALUI_THEME_DARK_BLUE,          MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE) \
   X("green",              MATERIALUI_THEME_GREEN,              MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN) \
   X("red",                MATERIALUI_THEME_RED,                MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED) \
   X("yellow",             MATERIALUI_THEME_YELLOW,             MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW) \
   X("nvidia_shield",      MATERIALUI_THEME_NVIDIA_SHIELD,      MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD) \
   X("materialui",         MATERIALUI_THEME_MATERIALUI,         MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI) \
   X("materialui_dark",    MATERIALUI_THEME_MATERIALUI_DARK,    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK) \
   X("ozone_dark",         MATERIALUI_THEME_OZONE_DARK,         MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK) \
   X("nord",               MATERIALUI_THEME_NORD,               MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NORD) \
   X("gruvbox_dark",       MATERIALUI_THEME_GRUVBOX_DARK,       MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK) \
   X("solarized_dark",     MATERIALUI_THEME_SOLARIZED_DARK,     MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK) \
   X("cutie_blue",         MATERIALUI_THEME_CUTIE_BLUE,         MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE) \
   X("cutie_cyan",         MATERIALUI_THEME_CUTIE_CYAN,         MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN) \
   X("cutie_green",        MATERIALUI_THEME_CUTIE_GREEN,        MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN) \
   X("cutie_orange",       MATERIALUI_THEME_CUTIE_ORANGE,       MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE) \
   X("cutie_pink",         MATERIALUI_THEME_CUTIE_PINK,         MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK) \
   X("cutie_purple",       MATERIALUI_THEME_CUTIE_PURPLE,       MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE) \
   X("cutie_red",          MATERIALUI_THEME_CUTIE_RED,          MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED) \
   X("virtual_boy",        MATERIALUI_THEME_VIRTUAL_BOY,        MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_VIRTUAL_BOY) \
   X("hacking_the_kernel", MATERIALUI_THEME_HACKING_THE_KERNEL, MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL) \
   X("gray_dark",          MATERIALUI_THEME_GRAY_DARK,          MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK) \
   X("gray_light",         MATERIALUI_THEME_GRAY_LIGHT,         MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT) \
   X("dracula",            MATERIALUI_THEME_DRACULA,            MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DRACULA)

#endif

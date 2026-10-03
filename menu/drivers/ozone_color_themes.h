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

#ifndef __OZONE_COLOR_THEMES_H
#define __OZONE_COLOR_THEMES_H

/* The one list of Ozone color themes, in menu order.
 *
 * Each row is X(identifier, theme, label):
 *   identifier - the string stored in ozone_menu_color_theme
 *   theme      - the ozone_theme_t in menu/drivers/ozone.c
 *   label      - the msg_hash enum shown in the menu
 *
 * Users expand the list with their own X and ignore the columns they
 * do not need, so ozone.c never touches the labels and menu_setting.c
 * never touches the theme structs.  Order is free to change: the
 * setting stores identifiers, not positions.  The numeric values old
 * configs carry are mapped separately, by the frozen table in
 * configuration.c - do NOT reorder that one. */
#define OZONE_COLOR_THEME_LIST(X) \
   X("basic_white",        ozone_theme_light,              MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE) \
   X("basic_black",        ozone_theme_dark,               MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK) \
   X("nord",               ozone_theme_nord,               MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_NORD) \
   X("gruvbox_dark",       ozone_theme_gruvbox_dark,       MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK) \
   X("boysenberry",        ozone_theme_boysenberry,        MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY) \
   X("hacking_the_kernel", ozone_theme_hacking_the_kernel, MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL) \
   X("twilight_zone",      ozone_theme_twilight_zone,      MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE) \
   X("dracula",            ozone_theme_dracula,            MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_DRACULA) \
   X("solarized_dark",     ozone_theme_solarized_dark,     MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK) \
   X("solarized_light",    ozone_theme_solarized_light,    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT) \
   X("gray_dark",          ozone_theme_gray_dark,          MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK) \
   X("gray_light",         ozone_theme_gray_light,         MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT) \
   X("purple_rain",        ozone_theme_purple_rain,        MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN) \
   X("selenium",           ozone_theme_selenium,           MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SELENIUM) \
   X("evergarden",         ozone_theme_evergarden,         MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_EVERGARDEN)

#endif

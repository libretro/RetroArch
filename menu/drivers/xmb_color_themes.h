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

#ifndef __XMB_COLOR_THEMES_H
#define __XMB_COLOR_THEMES_H

/* X(identifier, theme, label) in menu order. Old numeric configs
 * map through the frozen tables in configuration.c instead. */
#define XMB_COLOR_THEME_LIST(X) \
   X("legacy_red",      XMB_THEME_LEGACY_RED,      MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED) \
   X("dark_purple",     XMB_THEME_DARK_PURPLE,     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE) \
   X("midnight_blue",   XMB_THEME_MIDNIGHT_BLUE,   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE) \
   X("golden",          XMB_THEME_GOLDEN,          MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN) \
   X("electric_blue",   XMB_THEME_ELECTRIC_BLUE,   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE) \
   X("apple_green",     XMB_THEME_APPLE_GREEN,     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN) \
   X("undersea",        XMB_THEME_UNDERSEA,        MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA) \
   X("volcanic_red",    XMB_THEME_VOLCANIC_RED,    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED) \
   X("dark",            XMB_THEME_DARK,            MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK) \
   X("light",           XMB_THEME_LIGHT,           MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT) \
   X("plain",           XMB_THEME_WALLPAPER,       MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN) \
   X("morning_blue",    XMB_THEME_MORNING_BLUE,    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE) \
   X("sunbeam",         XMB_THEME_SUNBEAM,         MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM) \
   X("lime",            XMB_THEME_LIME,            MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME) \
   X("pikachu_yellow",  XMB_THEME_PIKACHU_YELLOW,  MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW) \
   X("gamecube_purple", XMB_THEME_GAMECUBE_PURPLE, MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE) \
   X("famicom_red",     XMB_THEME_FAMICOM_RED,     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED) \
   X("flaming_hot",     XMB_THEME_FLAMING_HOT,     MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT) \
   X("ice_cold",        XMB_THEME_ICE_COLD,        MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD) \
   X("midgar",          XMB_THEME_MIDGAR,          MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDGAR) \
   X("gray_dark",       XMB_THEME_GRAY_DARK,       MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK) \
   X("gray_light",      XMB_THEME_GRAY_LIGHT,      MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT)

#endif

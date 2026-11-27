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

#ifndef __XMB_ICON_THEMES_H
#define __XMB_ICON_THEMES_H

/* X(identifier, theme, label) in menu order. Old numeric configs
 * map through the frozen tables in configuration.c instead. */
#define XMB_ICON_THEME_LIST(X) \
   X("monochrome",          XMB_ICON_THEME_MONOCHROME,          MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME) \
   X("flatui",              XMB_ICON_THEME_FLATUI,              MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI) \
   X("flatux",              XMB_ICON_THEME_FLATUX,              MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUX) \
   X("pixel",               XMB_ICON_THEME_PIXEL,               MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL) \
   X("systematic",          XMB_ICON_THEME_SYSTEMATIC,          MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC) \
   X("dotart",              XMB_ICON_THEME_DOTART,              MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART) \
   X("custom",              XMB_ICON_THEME_CUSTOM,              MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM) \
   X("retrosystem",         XMB_ICON_THEME_RETROSYSTEM,         MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM) \
   X("monochrome_inverted", XMB_ICON_THEME_MONOCHROME_INVERTED, MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED) \
   X("automatic",           XMB_ICON_THEME_AUTOMATIC,           MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC) \
   X("automatic_inverted",  XMB_ICON_THEME_AUTOMATIC_INVERTED,  MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED) \
   X("daite",               XMB_ICON_THEME_DAITE,               MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DAITE)

#endif

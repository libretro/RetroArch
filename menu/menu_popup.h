/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef _MENU_POPUP_H
#define _MENU_POPUP_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>

#include <retro_common_api.h>

enum menu_help_type
{
   MENU_HELP_NONE       = 0,
   MENU_HELP_WELCOME,
   MENU_HELP_EXTRACT,
   MENU_HELP_CONTROLS,
   MENU_HELP_CHEEVOS_DESCRIPTION,
   MENU_HELP_LOADING_CONTENT,
   MENU_HELP_WHAT_IS_A_CORE,
   MENU_HELP_CHANGE_VIRTUAL_GAMEPAD,
   MENU_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   MENU_HELP_SCANNING_CONTENT,
   MENU_HELP_LAST
};

RETRO_BEGIN_DECLS

void menu_popup_push_pending(
      bool push, enum menu_help_type type);

int menu_popup_iterate_help( 
      char *s, size_t len, const char *label);

void menu_popup_unset_pending_push(void);

bool menu_popup_is_push_pending(void);

void menu_popup_push(void);

void menu_popup_reset(void);

RETRO_END_DECLS

#endif

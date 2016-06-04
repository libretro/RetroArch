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

#ifndef _COCOA_WINDOW_UI
#define _COCOA_WINDOW_UI

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "../../ui_companion_driver.h"

RETRO_BEGIN_DECLS

typedef struct ui_window_cocoa
{
    void *empty;
} ui_window_cocoa_t;

void ui_window_cocoa_set_visible(void *data,
        bool set_visible);

void ui_window_cocoa_set_focused(void *data);

void ui_window_cocoa_destroy(void *data);

void ui_window_cocoa_set_title(void *data, char *buf);

RETRO_END_DECLS

#endif

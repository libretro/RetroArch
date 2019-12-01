/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _MENU_FILEBROWSER_H
#define _MENU_FILEBROWSER_H

#include <stdint.h>
#include <stdlib.h>

#include <retro_common_api.h>

#include "../menu_displaylist.h"

RETRO_BEGIN_DECLS

enum filebrowser_enums
{
   FILEBROWSER_NONE              = 0,
   FILEBROWSER_SELECT_DIR,
   FILEBROWSER_SCAN_DIR,
   FILEBROWSER_SCAN_FILE,
   FILEBROWSER_MANUAL_SCAN_DIR,
   FILEBROWSER_SELECT_FILE,
   FILEBROWSER_SELECT_FILE_SUBSYSTEM,
   FILEBROWSER_SELECT_IMAGE,
   FILEBROWSER_SELECT_FONT,
   FILEBROWSER_SELECT_COLLECTION
};

enum filebrowser_enums filebrowser_get_type(void);

void filebrowser_clear_type(void);

void filebrowser_set_type(enum filebrowser_enums type);

void filebrowser_parse(menu_displaylist_info_t *data, unsigned type);

RETRO_END_DECLS

#endif

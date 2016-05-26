/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
 *  Copyright (C) 2016      - Andrés Suárez
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


 /*  This file is intended for helper functions, custom controls, etc. */

#include "nk_menu.h"

/* sets window position and size */
void nk_menu_wnd_set_state(nk_menu_handle_t *zr, const int id,
   struct nk_vec2 pos, struct nk_vec2 size)
{
   zr->window[id].position = pos;
   zr->window[id].size = size;
}

/* gets window position and size */
void nk_menu_wnd_get_state(nk_menu_handle_t *zr, const int id,
   struct nk_vec2 *pos, struct nk_vec2 *size)
{
   *pos = zr->window[id].position;
   *size = zr->window[id].size;
}

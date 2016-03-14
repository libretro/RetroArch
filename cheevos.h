/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#ifndef __RARCH_CHEEVOS_H
#define __RARCH_CHEEVOS_H

#include <stdint.h>
#include <stdlib.h>

enum cheevos_ctl_state
{
   CHEEVOS_CTL_NONE = 0,
   CHEEVOS_CTL_TEST,
   CHEEVOS_CTL_LOAD,

   CHEEVOS_CTL_APPLY_CHEATS,

   /* Unload the achievements from memory. */
   CHEEVOS_CTL_UNLOAD,

   /* Toggle Hardcore Mode */
   CHEEVOS_CTL_TOGGLE_HARDCORE_MODE,

   /* Load the achievements into memory if 
    * the game has content. */
   CHEEVOS_CTL_SET_CHEATS,

   CHEEVOS_CTL_GET_DESCRIPTION,

   CHEEVOS_CTL_POPULATE_MENU
};

typedef struct cheevos_ctx_desc
{
   unsigned idx;
   char *s;
   size_t len;
} cheevos_ctx_desc_t;

bool cheevos_ctl(enum cheevos_ctl_state state, void *data);

#endif /* __RARCH_CHEEVOS_H */

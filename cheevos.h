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

typedef struct cheevos_ctx_desc
{
   unsigned idx;
   char *s;
   size_t len;
} cheevos_ctx_desc_t;

bool cheevos_load(const void *data);

void cheevos_populate_menu(void *data);

bool cheevos_get_description(cheevos_ctx_desc_t *desc);

bool cheevos_apply_cheats(bool *data_bool);

bool cheevos_unload(void);

bool cheevos_toggle_hardcore_mode(void);

bool cheevos_test(void);

bool cheevos_set_cheats(void);

void cheevos_set_support_cheevos(bool state);

#endif /* __RARCH_CHEEVOS_H */

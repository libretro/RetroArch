/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2018 - Andre Leiradella
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

#ifndef __RARCH_CHEEVOS_PARSER_H
#define __RARCH_CHEEVOS_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct {
  const char* title;
  const char* description;
  const char* badge;
  const char* memaddr;
  unsigned points;
  unsigned id;
} rcheevos_racheevo_t;

typedef struct {
  const char* title;
  const char* description;
  const char* format;
  const char* mem;
  unsigned id;
} rcheevos_ralboard_t;

typedef struct {
   unsigned console_id;

   rcheevos_racheevo_t* core;
   rcheevos_racheevo_t* unofficial;
   rcheevos_ralboard_t* lboards;

   unsigned core_count;
   unsigned unofficial_count;
   unsigned lboard_count;
} rcheevos_rapatchdata_t;

typedef void (*rcheevos_unlock_cb_t)(unsigned id, void* userdata);

int rcheevos_get_token(const char* json, char* token, size_t length);

int  rcheevos_get_patchdata(const char* json, rcheevos_rapatchdata_t* patchdata);
void rcheevos_free_patchdata(rcheevos_rapatchdata_t* patchdata);

void rcheevos_deactivate_unlocks(const char* json, rcheevos_unlock_cb_t unlock_cb, void* userdata);

unsigned chevos_get_gameid(const char* json);

RETRO_END_DECLS

#endif

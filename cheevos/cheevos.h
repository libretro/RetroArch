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

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum
{
   /* Don't change those, the values match the console IDs
    * at retroachievements.org. */
   CHEEVOS_CONSOLE_NONE             = 0,
   CHEEVOS_CONSOLE_MEGA_DRIVE       = 1,
   CHEEVOS_CONSOLE_NINTENDO_64      = 2,
   CHEEVOS_CONSOLE_SUPER_NINTENDO   = 3,
   CHEEVOS_CONSOLE_GAMEBOY          = 4,
   CHEEVOS_CONSOLE_GAMEBOY_ADVANCE  = 5,
   CHEEVOS_CONSOLE_GAMEBOY_COLOR    = 6,
   CHEEVOS_CONSOLE_NINTENDO         = 7,
   CHEEVOS_CONSOLE_PC_ENGINE        = 8,
   CHEEVOS_CONSOLE_SEGA_CD          = 9,
   CHEEVOS_CONSOLE_SEGA_32X         = 10,
   CHEEVOS_CONSOLE_MASTER_SYSTEM    = 11
};

typedef struct cheevos_ctx_desc
{
   unsigned idx;
   char *s;
   size_t len;
} cheevos_ctx_desc_t;

typedef struct
{
   unsigned size;
   unsigned type;
   int      bank_id;
   unsigned value;
   unsigned previous;
} cheevos_var_t;

bool cheevos_load(const void *data);

void cheevos_reset_game(void);

void cheevos_populate_menu(void *data, bool hardcore);

bool cheevos_get_description(cheevos_ctx_desc_t *desc);

bool cheevos_apply_cheats(bool *data_bool);

bool cheevos_unload(void);

bool cheevos_toggle_hardcore_mode(void);

void cheevos_test(void);

bool cheevos_set_cheats(void);

void cheevos_set_support_cheevos(bool state);

bool cheevos_get_support_cheevos(void);

void cheevos_parse_guest_addr(cheevos_var_t *var, unsigned value);

uint8_t *cheevos_get_memory(const cheevos_var_t *var);

unsigned cheevos_get_console_id(void);

extern bool cheevos_loaded;
extern int cheats_are_enabled;
extern int cheats_were_enabled;

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_H */

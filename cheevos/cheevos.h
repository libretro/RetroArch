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

#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/*****************************************************************************
Setup - mainly for debugging
*****************************************************************************/

/* Define this macro to get extra-verbose log for cheevos. */
#undef CHEEVOS_VERBOSE

/*****************************************************************************
End of setup
*****************************************************************************/

#define CHEEVOS_TAG "[CHEEVOS]: "

typedef struct cheevos_ctx_desc
{
   unsigned idx;
   char *s;
   size_t len;
} cheevos_ctx_desc_t;

typedef enum
{
   CHEEVOS_CONSOLE_NONE = 0,
   /* Don't change those, the values match the console IDs
    * at retroachievements.org. */
   CHEEVOS_CONSOLE_MEGA_DRIVE      = 1,
   CHEEVOS_CONSOLE_NINTENDO_64     = 2,
   CHEEVOS_CONSOLE_SUPER_NINTENDO  = 3,
   CHEEVOS_CONSOLE_GAMEBOY         = 4,
   CHEEVOS_CONSOLE_GAMEBOY_ADVANCE = 5,
   CHEEVOS_CONSOLE_GAMEBOY_COLOR   = 6,
   CHEEVOS_CONSOLE_NINTENDO        = 7,
   CHEEVOS_CONSOLE_PC_ENGINE       = 8,
   CHEEVOS_CONSOLE_SEGA_CD         = 9,
   CHEEVOS_CONSOLE_SEGA_32X        = 10,
   CHEEVOS_CONSOLE_MASTER_SYSTEM   = 11,
   CHEEVOS_CONSOLE_PLAYSTATION     = 12,
   CHEEVOS_CONSOLE_ATARI_LYNX      = 13,
   CHEEVOS_CONSOLE_NEOGEO_POCKET   = 14,
   CHEEVOS_CONSOLE_XBOX_360        = 15,
   CHEEVOS_CONSOLE_GAMECUBE        = 16,
   CHEEVOS_CONSOLE_ATARI_JAGUAR    = 17,
   CHEEVOS_CONSOLE_NINTENDO_DS     = 18,
   CHEEVOS_CONSOLE_WII             = 19,
   CHEEVOS_CONSOLE_WII_U           = 20,
   CHEEVOS_CONSOLE_PLAYSTATION_2   = 21,
   CHEEVOS_CONSOLE_XBOX            = 22,
   CHEEVOS_CONSOLE_SKYNET          = 23,
   CHEEVOS_CONSOLE_XBOX_ONE        = 24,
   CHEEVOS_CONSOLE_ATARI_2600      = 25,
   CHEEVOS_CONSOLE_MS_DOS          = 26,
   CHEEVOS_CONSOLE_ARCADE          = 27,
   CHEEVOS_CONSOLE_VIRTUAL_BOY     = 28,
   CHEEVOS_CONSOLE_MSX             = 29,
   CHEEVOS_CONSOLE_COMMODORE_64    = 30,
   CHEEVOS_CONSOLE_ZX81            = 31
} cheevos_console_t;

enum
{
   CHEEVOS_DIRTY_TITLE       = 1 << 0,
   CHEEVOS_DIRTY_DESC        = 1 << 1,
   CHEEVOS_DIRTY_POINTS      = 1 << 2,
   CHEEVOS_DIRTY_AUTHOR      = 1 << 3,
   CHEEVOS_DIRTY_ID          = 1 << 4,
   CHEEVOS_DIRTY_BADGE       = 1 << 5,
   CHEEVOS_DIRTY_CONDITIONS  = 1 << 6,
   CHEEVOS_DIRTY_VOTES       = 1 << 7,
   CHEEVOS_DIRTY_DESCRIPTION = 1 << 8,

   CHEEVOS_DIRTY_ALL         = (1 << 9) - 1
};

enum
{
   CHEEVOS_ACTIVE_SOFTCORE = 1 << 0,
   CHEEVOS_ACTIVE_HARDCORE = 1 << 1
};

enum
{
   CHEEVOS_FORMAT_FRAMES = 0,
   CHEEVOS_FORMAT_SECS,
   CHEEVOS_FORMAT_MILLIS,
   CHEEVOS_FORMAT_SCORE,
   CHEEVOS_FORMAT_VALUE,
   CHEEVOS_FORMAT_OTHER
};

bool cheevos_load(const void *data);

void cheevos_reset_game(void);

void cheevos_populate_menu(void *data);

bool cheevos_get_description(cheevos_ctx_desc_t *desc);

bool cheevos_apply_cheats(bool *data_bool);

bool cheevos_unload(void);

bool cheevos_toggle_hardcore_mode(void);

void cheevos_test(void);

bool cheevos_set_cheats(void);

void cheevos_set_support_cheevos(bool state);

bool cheevos_get_support_cheevos(void);

cheevos_console_t cheevos_get_console(void);

extern bool cheevos_loaded;
extern int cheats_are_enabled;
extern int cheats_were_enabled;

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_H */

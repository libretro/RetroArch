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

#ifndef __WEBHOOKS_H
#define __WEBHOOKS_H

#include <stdint.h>
#include <stdlib.h>
#include <libretro.h>
#include <lrc_hash.h>

#include "rc_api_request.h"

/* Define this macro to get extra-verbose log for cheevos. */
#define WEBHOOKS_VERBOSE

#define WEBHOOKS_TAG "[WEBHOOKS]: "
#define WEBHOOKS_FREE(p) do { void* q = (void*)p; if (q) free(q); } while (0)

#ifdef WEBHOOKS_VERBOSE
#define WEBHOOKS_LOG RARCH_LOG
#define WEBHOOKS_ERR RARCH_ERR
#else
void webhooks_log(const char *fmt, ...);
 #define WEBHOOKS_LOG webhooks_log
 #define WEBHOOKS_ERR RARCH_ERR
#endif

const int HASH_LENGTH = 33;
const int GAME_PROGRESS_LENGTH = 4096;

typedef struct wb_locals_t
{
  bool initialized;
  char hash[HASH_LENGTH];
  char game_progress[GAME_PROGRESS_LENGTH];
  struct rc_runtime_t runtime;
  rc_libretro_memory_regions_t memory;
  uint console_id;
} wb_locals_t;

unsigned wb_peek
(
  unsigned address,
  unsigned num_bytes,
  void* ud
);

void webhooks_initialize();

void webhooks_game_loaded(const struct retro_game_info* info);
void webhooks_game_unloaded();
void webhooks_game_reset();

void webhooks_process_frame();
void webhooks_send_presence();

#endif /* __WEBHOOKS_H */

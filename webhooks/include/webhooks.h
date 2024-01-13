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

#include <libretro.h>

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos_locals.h"
#endif

#include "../tasks/tasks_internal.h"

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

#define HASH_LENGTH 33
#define GAME_PROGRESS_LENGTH 4096

RETRO_BEGIN_DECLS

typedef struct async_http_request_t async_http_request_t;

typedef void (*async_http_handler)
(
  async_http_request_t *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
);

typedef void (*async_client_callback)(void* userdata);

struct async_http_request_t
{
    rc_api_request_t request;
    async_http_handler handler;
    async_client_callback callback;
    void* callback_data;

    //  Not used yet.
    int id;
    int attempt_count;
    const char* success_message;
    const char* failure_message;
    const char* headers;
    char type;
};

typedef struct wb_locals_t
{
  bool initialized;
  char hash[HASH_LENGTH];
  char game_progress[GAME_PROGRESS_LENGTH];
  struct rc_runtime_t runtime;
  rc_libretro_memory_regions_t memory;
  unsigned int console_id;
  const rcheevos_racheevo_t* current_achievement;
  const rcheevos_racheevo_t* last_achievement;
} wb_locals_t;

unsigned wb_peek
(
  unsigned address,
  unsigned num_bytes,
  void* ud
);

void webhooks_initialize();

void webhooks_load_game
(
    const struct retro_game_info* info
);

void webhooks_unload_game();
void webhooks_reset_game();

void webhooks_process_frame();
void webhooks_send_presence();

void webhooks_on_achievements_loaded
(
  const rcheevos_racheevo_t* achievements,
  const unsigned int achievements_count
);

void webhooks_on_achievement_awarded
(
    rcheevos_racheevo_t* cheevo
);

RETRO_END_DECLS

#endif /* __WEBHOOKS_H */

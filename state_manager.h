/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Alfred Agrell
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

#ifndef __STATE_MANAGER_H
#define __STATE_MANAGER_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "dynamic.h"

RETRO_BEGIN_DECLS

struct state_manager
{
   uint8_t *data;
   /* Reading and writing is done here here. */
   uint8_t *head;
   /* If head comes close to this, discard a frame. */
   uint8_t *tail;

   uint8_t *thisblock;
   uint8_t *nextblock;
#if STRICT_BUF_SIZE
   uint8_t *debugblock;
   size_t debugsize;
#endif

   size_t capacity;
   /* This one is rounded up from reset::blocksize. */
   size_t blocksize;
   /* size_t + (blocksize + 131071) / 131072 *
    * (blocksize + u16 + u16) + u16 + u32 + size_t
    * (yes, the math is a bit ugly). */
   size_t maxcompsize;

   unsigned entries;
   bool thisblock_valid;
};

typedef struct state_manager state_manager_t;

struct state_manager_rewind_state
{
   /* Rewind support. */
   state_manager_t *state;
   size_t size;
   bool frame_is_reversed;
   bool init_attempted;
   bool hotkey_was_checked;
   bool hotkey_was_pressed;
};

bool state_manager_frame_is_reversed(void);

void state_manager_event_deinit(
      struct state_manager_rewind_state *rewind_st,
      struct retro_core_t *current_core);

void state_manager_event_init(struct state_manager_rewind_state *rewind_st,
      unsigned rewind_buffer_size);

/**
 * check_rewind:
 * @pressed              : was rewind key pressed or held?
 *
 * Checks if rewind toggle/hold was being pressed and/or held.
 **/
bool state_manager_check_rewind(
      struct state_manager_rewind_state *rewind_st,
      struct retro_core_t *current_core,
      bool pressed,
      unsigned rewind_granularity, bool is_paused,
      char *s, size_t len, unsigned *time);

RETRO_END_DECLS

#endif

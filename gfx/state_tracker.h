/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __RARCH_SNES_TRACKER_H
#define __RARCH_SNES_TRACKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../boolean.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

enum state_tracker_type
{
   RARCH_STATE_CAPTURE = 0,
   RARCH_STATE_CAPTURE_PREV,
   RARCH_STATE_TRANSITION,
   RARCH_STATE_TRANSITION_COUNT,
   RARCH_STATE_TRANSITION_PREV,
   RARCH_STATE_PYTHON
};

enum state_ram_type
{
   RARCH_STATE_NONE,
   RARCH_STATE_WRAM,
   RARCH_STATE_INPUT_SLOT1,
   RARCH_STATE_INPUT_SLOT2
};

struct state_tracker_uniform_info
{
   char id[64];
   uint32_t addr;
   enum state_tracker_type type;
   enum state_ram_type ram_type;
   uint16_t mask;
   uint16_t equal;
};

struct state_tracker_info
{
   const uint8_t *wram;

   const struct state_tracker_uniform_info *info;
   unsigned info_elem;

#ifdef HAVE_PYTHON
   const char *script;
   const char *script_class;
   bool script_is_file;
#endif
};

struct state_tracker_uniform
{
   const char *id;
   float value;
};

typedef struct state_tracker state_tracker_t;

state_tracker_t* state_tracker_init(const struct state_tracker_info *info);
void state_tracker_free(state_tracker_t *tracker);

unsigned state_get_uniform(state_tracker_t *tracker, struct state_tracker_uniform *uniforms, unsigned elem, unsigned frame_count);

#ifdef __cplusplus
}
#endif

#endif

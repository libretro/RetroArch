/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SSNES_SNES_STATE_H
#define __SSNES_SNES_STATE_H

#include <stdint.h>
#include "../boolean.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

enum snes_tracker_type
{
   SSNES_STATE_CAPTURE,
   SSNES_STATE_CAPTURE_PREV,
   SSNES_STATE_TRANSITION,
   SSNES_STATE_TRANSITION_COUNT,
   SSNES_STATE_TRANSITION_PREV,
#ifdef HAVE_PYTHON
   SSNES_STATE_PYTHON
#endif
};

enum snes_ram_type
{
   SSNES_STATE_NONE,
   SSNES_STATE_WRAM,
   SSNES_STATE_APURAM,
   SSNES_STATE_OAM,
   SSNES_STATE_CGRAM,
   SSNES_STATE_VRAM,
   SSNES_STATE_INPUT_SLOT1,
   SSNES_STATE_INPUT_SLOT2
};

struct snes_tracker_uniform_info
{
   char id[64];
   uint32_t addr;
   enum snes_tracker_type type;
   enum snes_ram_type ram_type;
   unsigned mask;
};

struct snes_tracker_info
{
   const uint8_t *wram;
   const uint8_t *vram;
   const uint8_t *cgram;
   const uint8_t *oam;
   const uint8_t *apuram;

   const struct snes_tracker_uniform_info *info;
   unsigned info_elem;

#ifdef HAVE_PYTHON
   const char *script;
   const char *script_class;
   bool script_is_file;
#endif
};

struct snes_tracker_uniform
{
   const char *id;
   float value;
};

typedef struct snes_tracker snes_tracker_t;

snes_tracker_t* snes_tracker_init(const struct snes_tracker_info *info);
void snes_tracker_free(snes_tracker_t *tracker);

unsigned snes_get_uniform(snes_tracker_t *tracker, struct snes_tracker_uniform *uniforms, unsigned elem, unsigned frame_count);

#endif

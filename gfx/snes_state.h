/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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

enum snes_tracker_type
{
   SSNES_STATE_CAPTURE,
   SSNES_STATE_TRANSITION,
   SSNES_STATE_CAPTURE_PREV
};

enum snes_ram_type
{
   SSNES_STATE_WRAM,
   SSNES_STATE_APURAM,
   SSNES_STATE_OAM,
   SSNES_STATE_CGRAM,
   SSNES_STATE_VRAM
};

struct snes_tracker_uniform_info
{
   const char *id;
   uint32_t addr;
   enum snes_tracker_type type;
   enum snes_ram_type ram_type;
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
};

struct snes_tracker_uniform
{
   const char *id;
   int value;
};

typedef struct snes_tracker snes_tracker_t;

snes_tracker_t* snes_tracker_init(const struct snes_tracker_info *info);
void snes_tracker_free(snes_tracker_t *tracker);

unsigned snes_get_uniform(snes_tracker_t *tracker, struct snes_tracker_uniform *uniforms, unsigned elem, unsigned frame_count);

#endif

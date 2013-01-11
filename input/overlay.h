/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#ifndef INPUT_OVERLAY_H__
#define INPUT_OVERLAY_H__

#include "../boolean.h"
#include <stdint.h>

// Overlay driver acts as a medium between input drivers and video driver.
// Coordinates are fetched from input driver, and an overlay with pressable actions are
// displayed on-screen.
//
// This interface requires that the video driver has support for the overlay interface.
typedef struct input_overlay input_overlay_t;

input_overlay_t *input_overlay_new(const char *overlay);
void input_overlay_free(input_overlay_t *ol);

void input_overlay_enable(input_overlay_t *ol, bool enable);

bool input_overlay_full_screen(input_overlay_t *ol);

// norm_x and norm_y are the result of input_translate_coord_viewport().
// Resulting state is a bitmask of (1 << key_bind_id).
uint64_t input_overlay_poll(input_overlay_t *ol, int16_t norm_x, int16_t norm_y);

void input_overlay_next(input_overlay_t *ol);

#endif


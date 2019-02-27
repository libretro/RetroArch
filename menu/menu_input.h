/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef _MENU_INPUT_H
#define _MENU_INPUT_H

#include <stdint.h>
#include <compat/strl.h>

#include <retro_common_api.h>
#include <libretro.h>

#include "menu_defines.h"
#include "../input/input_types.h"

RETRO_BEGIN_DECLS

typedef struct menu_input
{
   struct
   {
      unsigned ptr;
   } mouse;

   struct
   {
      bool back;
      bool pressed[2];
      int16_t x;
      int16_t y;
      int16_t dx;
      int16_t dy;
      unsigned ptr;
      unsigned counter;
      float accel;
   } pointer;
} menu_input_t;

typedef struct menu_input_ctx_hitbox
{
   int32_t x1;
   int32_t x2;
   int32_t y1;
   int32_t y2;
} menu_input_ctx_hitbox_t;

/*
 * This function gets called in order to process all input events
 * for the current frame.
 *
 * Sends input code to menu for one frame.
 *
 * It uses as input the local variables' input' and 'trigger_input'.
 *
 * Mouse and touch input events get processed inside this function.
 *
 * NOTE: 'input' and 'trigger_input' is sourced from the keyboard and/or
 * the gamepad. It does not contain input state derived from the mouse
 * and/or touch - this gets dealt with separately within this function.
 *
 * TODO/FIXME - maybe needs to be overhauled so we can send multiple
 * events per frame if we want to, and we shouldn't send the
 * entire button state either but do a separate event per button
 * state.
 */
unsigned menu_event(input_bits_t *p_input, input_bits_t *p_trigger_state);

/* Set a specific keyboard key.
 *
 * 'down' sets the latch (true would
 * mean the key is being pressed down, while 'false' would mean that
 * the key has been released).
 **/
void menu_event_kb_set(bool down, enum retro_key key);

/* Check if a specific keyboard key has been pressed. */
unsigned char menu_event_kb_is_set(enum retro_key key);

void menu_input_post_iterate(int *ret, unsigned action);

int16_t menu_input_pointer_state(enum menu_input_pointer_state state);

int16_t menu_input_mouse_state(enum menu_input_mouse_state state);

bool menu_input_mouse_check_vector_inside_hitbox(menu_input_ctx_hitbox_t *hitbox);

bool menu_input_ctl(enum menu_input_ctl_state state, void *data);

RETRO_END_DECLS

#endif

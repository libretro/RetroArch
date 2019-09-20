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

/* Mouse wheel tilt actions repeat at a very high
 * frequency - we ignore any input that occurs
 * with a period less than MENU_INPUT_HORIZ_WHEEL_DELAY */
#define MENU_INPUT_HORIZ_WHEEL_DELAY 250000  /* 250 ms */

#define MENU_INPUT_HIDE_CURSOR_DELAY 4000000 /* 4 seconds */

#define MENU_INPUT_PRESS_TIME_SHORT 250000   /* 250 ms */
#define MENU_INPUT_PRESS_TIME_LONG 1500000   /* 1.5 second */
/* (Anthing less than 'short' is considered a tap) */

#define MENU_INPUT_Y_ACCEL_DECAY_FACTOR 0.96f

enum menu_pointer_type
{
   MENU_POINTER_DISABLED = 0,
   MENU_POINTER_MOUSE,
   MENU_POINTER_TOUCHSCREEN
};

enum menu_input_mouse_hw_id
{
   MENU_MOUSE_X_AXIS = 0,
   MENU_MOUSE_Y_AXIS,
   MENU_MOUSE_LEFT_BUTTON,
   MENU_MOUSE_RIGHT_BUTTON,
   MENU_MOUSE_WHEEL_UP,
   MENU_MOUSE_WHEEL_DOWN,
   MENU_MOUSE_HORIZ_WHEEL_UP,
   MENU_MOUSE_HORIZ_WHEEL_DOWN
};

/* Defines set of (abstracted) inputs/states
 * common to mouse + touchscreen hardware */
typedef struct menu_input_pointer_hw_state
{
   bool active;
   int16_t x;
   int16_t y;
   bool select_pressed;
   bool cancel_pressed;
   bool up_pressed;
   bool down_pressed;
   bool left_pressed;
   bool right_pressed;
} menu_input_pointer_hw_state_t;

typedef struct menu_input_pointer
{
   enum menu_pointer_type type;
   bool active;
   bool pressed;
   bool dragged;
   retro_time_t press_duration;
   int16_t x;
   int16_t y;
   int16_t dx;
   int16_t dy;
   float y_accel;
} menu_input_pointer_t;

typedef struct menu_input
{
   menu_input_pointer_t pointer;
   unsigned ptr;
   bool select_inhibit;
} menu_input_t;

typedef struct menu_input_ctx_hitbox
{
   int32_t x1;
   int32_t x2;
   int32_t y1;
   int32_t y2;
} menu_input_ctx_hitbox_t;

/* Must be called inside menu_driver_toggle()
 * Prevents phantom input when using an overlay to
 * toggle menu ON if overlays are disabled in-menu */
void menu_input_driver_toggle(bool on);

/* Provides access to all pointer device parameters */
void menu_input_get_pointer_state(menu_input_pointer_t *pointer);

/* Getters/setters for menu item (index) currently
 * selected/highlighted (hovered over) by the pointer
 * device
 * Note: Each menu driver is responsible for setting this */
unsigned menu_input_get_pointer_selection(void);
void menu_input_set_pointer_selection(unsigned selection);

/* Allows pointer y acceleration to be overridden
 * (typically want to set acceleration to zero when
 * calling populate entries) */
void menu_input_set_pointer_y_accel(float y_accel);

void menu_input_reset(void);

bool menu_input_pointer_check_vector_inside_hitbox(menu_input_ctx_hitbox_t *hitbox);

void menu_input_post_iterate(int *ret, unsigned action);

RETRO_END_DECLS

#endif

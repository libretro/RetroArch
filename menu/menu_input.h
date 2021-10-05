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
#include "../input/input_driver.h"
#include "../performance_counters.h"

RETRO_BEGIN_DECLS

/* Mouse wheel tilt actions repeat at a very high
 * frequency - we ignore any input that occurs
 * with a period less than MENU_INPUT_HORIZ_WHEEL_DELAY */
#define MENU_INPUT_HORIZ_WHEEL_DELAY 250000          /* 250 ms */

/* Press directions are triggered as a pulse train.
 * Pulse period starts at MENU_INPUT_PRESS_DIRECTION_DELAY_MAX,
 * and decreases to MENU_INPUT_PRESS_DIRECTION_DELAY_MIN as
 * the start/current delta offset increases from
 * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_MIN to
 * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_MAX */
#define MENU_INPUT_PRESS_DIRECTION_DELAY_MIN 100000  /* 100 ms */
#define MENU_INPUT_PRESS_DIRECTION_DELAY_MAX 500000  /* 500 ms */

#define MENU_INPUT_HIDE_CURSOR_DELAY 4000000         /* 4 seconds */

#define MENU_INPUT_PRESS_TIME_SHORT 200000           /* 200 ms */
#define MENU_INPUT_PRESS_TIME_LONG 1000000           /* 1 second */
/* (Anything less than 'short' is considered a tap) */

/* Swipe gestures must be completed within a duration
 * of MENU_INPUT_SWIPE_TIMEOUT ms (helps to minimise
 * unwanted input if user 'zones out' and meanders on
 * a touchscreen) */
#define MENU_INPUT_SWIPE_TIMEOUT 500000              /* 500 ms */

/* Standard behaviour (on Android, at least) is to stop
 * scrolling when the user touches the screen. To prevent
 * jerky stop/start scrolling, we wait MENU_INPUT_Y_ACCEL_RESET_DELAY
 * ms before resetting y acceleration after a stationary
 * pointer down event is detected */
#define MENU_INPUT_Y_ACCEL_RESET_DELAY 50000         /* 50 ms */

#define MENU_INPUT_Y_ACCEL_DECAY_FACTOR 0.96f

/* Pointer is considered stationary if dx/dy remain
 * below (display DPI) * MENU_INPUT_DPI_THRESHOLD_DRAG */
#define MENU_INPUT_DPI_THRESHOLD_DRAG 0.1f

/* Press direction detection:
 * While holding the pointer down, a press in a
 * specific direction (up, down, left, right) will
 * be detected if:
 * - Current delta (i.e. from start to current) in
 *   press direction is greater than
 *   (display DPI) * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_MIN
 * - Current delta in perpendicular axis is less than
 *   (display DPI) * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_TANGENT
 * Press direction repeat rate is proportional to the current
 * delta in press direction.
 * Note: 'Tangent' is technically not the correct word here,
 * but the alternatives look silly, and the actual meaning
 * is transparent... */
#define MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_MIN 0.5f
#define MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_MAX 1.4f
#define MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_TANGENT 0.35f

/* Swipe detection:
 * A gesture will register as a swipe if:
 * - Final start/current delta in swipe direction is
 *   greater than (display DPI) * MENU_INPUT_DPI_THRESHOLD_SWIPE
 * - Maximum start/current delta in all other directions is
 *   less than (display DPI) * MENU_INPUT_DPI_THRESHOLD_SWIPE_TANGENT
 * - Pointer was held for less than MENU_INPUT_SWIPE_TIMEOUT ms */
#define MENU_INPUT_DPI_THRESHOLD_SWIPE 0.55f
#define MENU_INPUT_DPI_THRESHOLD_SWIPE_TANGENT 0.45f

#define MENU_MAX_BUTTONS           219
#define MENU_MAX_AXES              32
#define MENU_MAX_HATS              4
#define MENU_MAX_MBUTTONS          32 /* Enough to cover largest libretro constant*/

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

enum menu_input_pointer_press_direction
{
   MENU_INPUT_PRESS_DIRECTION_NONE = 0,
   MENU_INPUT_PRESS_DIRECTION_UP,
   MENU_INPUT_PRESS_DIRECTION_DOWN,
   MENU_INPUT_PRESS_DIRECTION_LEFT,
   MENU_INPUT_PRESS_DIRECTION_RIGHT
};

enum menu_input_pointer_gesture
{
   MENU_INPUT_GESTURE_NONE = 0,
   MENU_INPUT_GESTURE_TAP,
   MENU_INPUT_GESTURE_SHORT_PRESS,
   MENU_INPUT_GESTURE_LONG_PRESS,
   MENU_INPUT_GESTURE_SWIPE_UP,
   MENU_INPUT_GESTURE_SWIPE_DOWN,
   MENU_INPUT_GESTURE_SWIPE_LEFT,
   MENU_INPUT_GESTURE_SWIPE_RIGHT
};

struct menu_bind_state_port
{
   int16_t axes[MENU_MAX_AXES];
   uint16_t hats[MENU_MAX_HATS];
   bool mouse_buttons[MENU_MAX_MBUTTONS];
   bool buttons[MENU_MAX_BUTTONS];
};

struct menu_bind_axis_state
{
   /* Default axis state. */
   int16_t rested_axes[MENU_MAX_AXES];
   /* Locked axis state. If we configured an axis,
    * avoid having the same axis state trigger something again right away. */
   int16_t locked_axes[MENU_MAX_AXES];
};

struct menu_bind_state
{
   rarch_timer_t timer_timeout;
   rarch_timer_t timer_hold;

   struct retro_keybind *output;
   struct retro_keybind buffer;

   struct menu_bind_state_port state[MAX_USERS];
   struct menu_bind_axis_state axis_state[MAX_USERS];

   unsigned begin;
   unsigned last;
   unsigned user;
   unsigned port;

   bool skip;
};

/* Defines set of (abstracted) inputs/states
 * common to mouse + touchscreen hardware */
typedef struct menu_input_pointer_hw_state
{
   int16_t x;
   int16_t y;
   bool active;
   bool select_pressed;
   bool cancel_pressed;
   bool up_pressed;
   bool down_pressed;
   bool left_pressed;
   bool right_pressed;
} menu_input_pointer_hw_state_t;

typedef struct menu_input_pointer
{
   retro_time_t press_duration;  /* int64_t alignment */
   float y_accel;
   enum menu_pointer_type type;
   enum menu_input_pointer_press_direction press_direction;
   int16_t x;
   int16_t y;
   int16_t dx;
   int16_t dy;
   bool active;
   bool pressed;
   bool dragged;
} menu_input_pointer_t;

typedef struct menu_input
{
   menu_input_pointer_t pointer; /* retro_time_t alignment */
   unsigned ptr;
   bool select_inhibit;
   bool cancel_inhibit;
} menu_input_t;

typedef struct menu_input_ctx_hitbox
{
   int32_t x1;
   int32_t x2;
   int32_t y1;
   int32_t y2;
} menu_input_ctx_hitbox_t;

typedef struct key_desc
{
   /* libretro key id */
   unsigned key;

   /* description */
   char desc[32];
} key_desc_t;

/* TODO/FIXME - public global variables */
extern struct key_desc key_descriptors[RARCH_MAX_KEYS];

/**
 * Copy parameters from the global menu_input_state to a menu_input_pointer_t
 * in order to provide access to all pointer device parameters.
 * 
 * @param copy_target  menu_input_pointer_t struct where values will be copied
 **/
void menu_input_get_pointer_state(menu_input_pointer_t *copy_target);

/**
 * Get the menu item index currently selected or hovered over by the pointer.
 * 
 * @return the selected menu index
 **/
unsigned menu_input_get_pointer_selection(void);

/**
 * Set the menu item index that is currently selected or hovered over by the
 * pointer. Note: Each menu driver is responsible for setting this.
 *
 * @param selection  the selected menu index
 **/
void menu_input_set_pointer_selection(unsigned selection);

/**
 * Allows the pointer's y acceleration to be overridden. For example, menu
 * drivers typically set acceleration to zero when populating entries.
 * 
 * @param y_accel
 **/
void menu_input_set_pointer_y_accel(float y_accel);

typedef struct menu_input_ctx_line
{
   const char *label;
   const char *label_setting;
   unsigned type;
   unsigned idx;
   input_keyboard_line_complete_t cb;
} menu_input_ctx_line_t;

bool menu_input_dialog_start(menu_input_ctx_line_t *line);

const char *menu_input_dialog_get_label_setting_buffer(void);

const char *menu_input_dialog_get_label_buffer(void);

const char *menu_input_dialog_get_buffer(void);

unsigned menu_input_dialog_get_kb_idx(void);

bool menu_input_dialog_start_search(void);

bool menu_input_dialog_get_display_kb(void);

void menu_input_dialog_end(void);

bool menu_input_key_bind_poll_find_hold(
      unsigned max_users,
      struct menu_bind_state *new_state,
      struct retro_keybind * output);

void menu_input_set_pointer_visibility(
      menu_input_pointer_hw_state_t *pointer_hw_state,
      menu_input_t *menu_input,
      retro_time_t current_time);

RETRO_END_DECLS

#endif

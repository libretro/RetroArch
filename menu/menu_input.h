/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "../input/input_driver.h"
#include "../input/keyboard_line.h"
#include "../libretro.h"

#ifdef __cplusplus
extern "C" {
#endif
 
#ifndef MENU_MAX_BUTTONS
#define MENU_MAX_BUTTONS 219
#endif

#ifndef MENU_MAX_AXES
#define MENU_MAX_AXES 32
#endif

#ifndef MENU_MAX_HATS
#define MENU_MAX_HATS 4
#endif

#ifndef MAX_USERS
#define MAX_USERS 16
#endif

typedef enum menu_action
{
   MENU_ACTION_UP,
   MENU_ACTION_DOWN,
   MENU_ACTION_LEFT,
   MENU_ACTION_RIGHT,
   MENU_ACTION_OK,
   MENU_ACTION_SEARCH,
   MENU_ACTION_SCAN,
   MENU_ACTION_CANCEL,
   MENU_ACTION_REFRESH,
   MENU_ACTION_INFO,
   MENU_ACTION_SELECT,
   MENU_ACTION_START,
   MENU_ACTION_MESSAGE,
   MENU_ACTION_SCROLL_DOWN,
   MENU_ACTION_SCROLL_UP,
   MENU_ACTION_TOGGLE,
   MENU_ACTION_NOOP
} menu_action_t;

enum mouse_action
{
   MOUSE_ACTION_NONE = 0,
   MOUSE_ACTION_BUTTON_L,
   MOUSE_ACTION_BUTTON_L_OK,
   MOUSE_ACTION_BUTTON_L_TOGGLE,
   MOUSE_ACTION_BUTTON_L_SET_NAVIGATION,
   MOUSE_ACTION_BUTTON_R,
   MOUSE_ACTION_WHEEL_UP,
   MOUSE_ACTION_WHEEL_DOWN
};

enum menu_input_bind_mode
{
   MENU_INPUT_BIND_NONE,
   MENU_INPUT_BIND_SINGLE,
   MENU_INPUT_BIND_ALL
};

struct menu_bind_state_port
{
   bool buttons[MENU_MAX_BUTTONS];
   int16_t axes[MENU_MAX_AXES];
   uint16_t hats[MENU_MAX_HATS];
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
   struct retro_keybind *target;
   /* For keyboard binding. */
   int64_t timeout_end;
   unsigned begin;
   unsigned last;
   unsigned user;
   struct menu_bind_state_port state[MAX_USERS];
   struct menu_bind_axis_state axis_state[MAX_USERS];
   bool skip;
};

typedef struct menu_input
{
   struct menu_bind_state binds;

   struct
   {
      unsigned state;
   } joypad;

   struct
   {
      int16_t dx;
      int16_t dy;
      int16_t x;
      int16_t y;
      int16_t screen_x;
      int16_t screen_y;
      bool    left;
      bool    right;
      bool    oldleft;
      bool    oldright;
      bool    wheelup;
      bool    wheeldown;
      bool    hwheelup;
      bool    hwheeldown;
      bool    scrollup;
      bool    scrolldown;
      unsigned ptr;
      uint64_t state;
   } mouse;

   struct
   {
      int16_t x;
      int16_t y;
      int16_t dx;
      int16_t dy;
      int16_t old_x;
      int16_t old_y;
      int16_t start_x;
      int16_t start_y;
      float accel;
      float accel0;
      float accel1;
      bool pressed[2];
      bool oldpressed[2];
      bool dragging;
      bool back;
      bool oldback;
      unsigned ptr;
   } pointer;

   struct
   {
      const char **buffer;
      const char *label;
      const char *label_setting;
      bool display;
      unsigned type;
      unsigned idx;
   } keyboard;

   /* Used for key repeat */
   struct
   {
      float timer;
      float count;
   } delay;
} menu_input_t;

void menu_input_key_event(bool down, unsigned keycode, uint32_t character,
      uint16_t key_modifiers);

void menu_input_key_start_line(const char *label,
      const char *label_setting, unsigned type, unsigned idx,
      input_keyboard_line_complete_t cb);

void menu_input_st_uint_callback(void *userdata, const char *str);
void menu_input_st_hex_callback(void *userdata, const char *str);

void menu_input_st_string_callback(void *userdata, const char *str);

void menu_input_st_cheat_callback(void *userdata, const char *str);

int menu_input_bind_iterate(void);

unsigned menu_input_frame(retro_input_t input, retro_input_t trigger_state);

void menu_input_post_iterate(int *ret, unsigned action);

void menu_input_search_start(void);

int menu_input_set_keyboard_bind_mode(void *data, enum menu_input_bind_mode type);

int menu_input_set_input_device_bind_mode(void *data, enum menu_input_bind_mode type);

menu_input_t *menu_input_get_ptr(void);

#ifdef __cplusplus
}
#endif

#endif

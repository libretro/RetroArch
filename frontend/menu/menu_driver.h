/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifndef DRIVER_MENU_H__
#define DRIVER_MENU_H__

#include <stddef.h>
#include <stdint.h>
#include "../../boolean.h"
#include "../../file_list.h"


#ifdef __cplusplus
extern "C" {
#endif

#define MENU_MAX_BUTTONS 219

#define MENU_MAX_AXES 32
#define MENU_MAX_HATS 4

#ifndef MAX_PLAYERS
#define MAX_PLAYERS 16
#endif

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
   unsigned player;
   struct menu_bind_state_port state[MAX_PLAYERS];
   struct menu_bind_axis_state axis_state[MAX_PLAYERS];
   bool skip;
};

typedef struct
{
   uint64_t old_input_state;
   uint64_t trigger_state;
   bool do_held;

   unsigned delay_timer;
   unsigned delay_count;

   unsigned width;
   unsigned height;

   uint16_t *frame_buf;
   size_t frame_buf_pitch;

   file_list_t *menu_stack;
   file_list_t *selection_buf;
   size_t selection_ptr;
   bool need_refresh;
   bool msg_force;
   bool push_start_screen;

   bool defer_core;
   char deferred_path[PATH_MAX];

   /* This buffer can be used to display generic OK messages to the user.
    * Fill it and call
    * file_list_push(driver.menu->menu_stack, "", "message", 0, 0);
    */
   char message_contents[PATH_MAX];

   /* Quick jumping indices with L/R.
    * Rebuilt when parsing directory. */
   size_t scroll_indices[2 * (26 + 2) + 1];
   unsigned scroll_indices_size;
   unsigned scroll_accel;

   char default_glslp[PATH_MAX];
   char default_cgp[PATH_MAX];

   const uint8_t *font;
   bool alloc_font;

   bool load_no_content;

   struct gfx_shader *shader;
   /* Points to either shader or 
    * graphics driver's current shader. */
   struct gfx_shader *parameter_shader;
   unsigned current_pad;

   /* Used to throttle menu in case
    * VSync is broken. */
   int64_t last_time;

   struct menu_bind_state binds;
   struct
   {
      const char **buffer;
      const char *label;
      const char *label_setting;
      bool display;
   } keyboard;

   bool bind_mode_keyboard;
} menu_handle_t;

#ifdef __cplusplus
}
#endif

#endif


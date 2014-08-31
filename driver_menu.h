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
#include "boolean.h"
#include "file_list.h"
#include "core_info.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_MAX_BUTTONS 219

#define MENU_MAX_AXES 32
#define MENU_MAX_HATS 4

#ifndef MAX_PLAYERS
#define MAX_PLAYERS 8
#endif

struct menu_bind_state_port
{
   bool buttons[MENU_MAX_BUTTONS];
   int16_t axes[MENU_MAX_AXES];
   uint16_t hats[MENU_MAX_HATS];
};

struct menu_bind_axis_state
{
   // Default axis state.
   int16_t rested_axes[MENU_MAX_AXES];
   // Locked axis state. If we configured an axis, avoid having the same axis state trigger something again right away.
   int16_t locked_axes[MENU_MAX_AXES];
};

struct menu_bind_state
{
   struct retro_keybind *target;
   int64_t timeout_end; // For keyboard binding.
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
   bool frame_buf_show;

   file_list_t *menu_stack;
   file_list_t *selection_buf;
   size_t selection_ptr;
   unsigned info_selection;
   bool need_refresh;
   bool msg_force;
   bool push_start_screen;

   core_info_list_t *core_info;
   core_info_t *core_info_current;
   bool defer_core;
   char deferred_path[PATH_MAX];

   // Quick jumping indices with L/R.
   // Rebuilt when parsing directory.
   size_t scroll_indices[2 * (26 + 2) + 1];
   unsigned scroll_indices_size;
   unsigned scroll_accel;

   char default_glslp[PATH_MAX];
   char default_cgp[PATH_MAX];

   const uint8_t *font;
   bool alloc_font;

   struct retro_system_info info;
   bool load_no_content;

   struct gfx_shader *shader;
   struct gfx_shader *parameter_shader; // Points to either shader or graphics driver current shader.
   unsigned current_pad;

   retro_time_t last_time; // Used to throttle menu in case VSync is broken.

   struct menu_bind_state binds;
   struct
   {
      const char **buffer;
      const char *label;
      const char *label_setting;
      bool display;
   } keyboard;

   bool bind_mode_keyboard;
   retro_time_t time;
   retro_time_t delta;
   retro_time_t target_msec;
   retro_time_t sleep_msec;
} menu_handle_t;

typedef struct menu_ctx_driver_backend
{
   void     (*entries_init)(menu_handle_t *, unsigned);
   int      (*iterate)(unsigned);
   void     (*shader_manager_init)(menu_handle_t *);
   void     (*shader_manager_get_str)(struct gfx_shader *, char *, size_t, unsigned);
   void     (*shader_manager_set_preset)(struct gfx_shader *, unsigned, const char*);
   void     (*shader_manager_save_preset)(const char *, bool);
   unsigned (*shader_manager_get_type)(const struct gfx_shader *);
   int      (*shader_manager_setting_toggle)(unsigned, unsigned);
   unsigned (*type_is)(unsigned);
   void     (*setting_set_label)(char *, size_t, unsigned *, unsigned,unsigned);
   void     (*defer_decision_automatic)(void);
   void     (*defer_decision_manual)(void);
   const char *ident;
} menu_ctx_driver_backend_t;

typedef struct menu_ctx_driver
{
   void  (*set_texture)(void*);
   void  (*render_messagebox)(const char*);
   void  (*render)(void);
   void  (*frame)(void);
   void* (*init)(void);
   void  (*free)(void*);
   void  (*context_reset)(void*);
   void  (*context_destroy)(void*);
   void  (*populate_entries)(void*, unsigned);
   void  (*iterate)(void*, unsigned);
   int   (*input_postprocess)(uint64_t);
   void  (*navigation_clear)(void *);
   void  (*navigation_decrement)(void *);
   void  (*navigation_increment)(void *);
   void  (*navigation_set)(void *);
   void  (*navigation_set_last)(void *);
   void  (*navigation_descend_alphabet)(void *, size_t *);
   void  (*navigation_ascend_alphabet)(void *, size_t *);
   void  (*list_insert)(void *, const char *, const char *, size_t);
   void  (*list_delete)(void *, size_t);
   void  (*list_clear)(void *);
   void  (*list_set_selection)(void *);
   void  (*init_core_info)(void *);

   const menu_ctx_driver_backend_t *backend;
   // Human readable string.
   const char *ident;
} menu_ctx_driver_t;


#ifdef __cplusplus
}
#endif

#endif


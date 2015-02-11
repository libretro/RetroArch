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

#ifndef __MENU_DRIVER_H__
#define __MENU_DRIVER_H__

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_miscellaneous.h>
#include "menu_animation.h"
#include "menu_list.h"
#include "../settings_list.h"
#include "../playlist.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_MAX_BUTTONS 219

#define MENU_MAX_AXES 32
#define MENU_MAX_HATS 4

#ifndef MAX_USERS
#define MAX_USERS 16
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
   unsigned user;
   struct menu_bind_state_port state[MAX_USERS];
   struct menu_bind_axis_state axis_state[MAX_USERS];
   bool skip;
};

typedef struct
{
   void *userdata;

   /* Used for key repeat */
   unsigned delay_timer;
   unsigned delay_count;

   unsigned width;
   unsigned height;
   size_t begin;

   menu_list_t *menu_list;
   size_t cat_selection_ptr;
   size_t num_categories;
   size_t selection_ptr;
   bool need_refresh;
   bool msg_force;
   bool push_start_screen;

   bool defer_core;
   char deferred_path[PATH_MAX_LENGTH];

   /* This buffer can be used to display generic OK messages to the user.
    * Fill it and call
    * menu_list_push(driver.menu->menu_stack, "", "message", 0, 0);
    */
   char message_contents[PATH_MAX_LENGTH];

   /* Quick jumping indices with L/R.
    * Rebuilt when parsing directory. */
   size_t scroll_indices[2 * (26 + 2) + 1];
   unsigned scroll_indices_size;
   unsigned scroll_accel;

   char default_glslp[PATH_MAX_LENGTH];
   char default_cgp[PATH_MAX_LENGTH];

   struct
   {
      uint16_t *data;
      size_t pitch;
   } frame_buf;

   const uint8_t *font;
   bool alloc_font;

   bool load_no_content;

   struct video_shader *shader;

   struct menu_bind_state binds;

   struct
   {
      int16_t dx;
      int16_t dy;
      int16_t x;
      int16_t y;
      bool    enable;
      bool    left;
      bool    right;
      bool    oldleft;
      bool    oldright;
      bool    wheelup;
      bool    wheeldown;
      unsigned ptr;
   } mouse;

   struct
   {
      const char **buffer;
      const char *label;
      const char *label_setting;
      bool display;
      unsigned type;
      unsigned idx;
   } keyboard;

   rarch_setting_t *list_settings;
   animation_t *animation;

   content_playlist_t *db_playlist;
   char db_playlist_file[PATH_MAX_LENGTH];
} menu_handle_t;

typedef struct menu_file_list_cbs
{
   int (*action_iterate)(const char *label, unsigned action);
   int (*action_deferred_push)(void *data, void *userdata, const char
         *path, const char *label, unsigned type);
   int (*action_ok)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_cancel)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_start)(unsigned type,  const char *label, unsigned action);
   int (*action_select)(unsigned type,  const char *label, unsigned action);
   int (*action_content_list_switch)(void *data, void *userdata, const char
         *path, const char *label, unsigned type);
   int (*action_toggle)(unsigned type, const char *label, unsigned action);
   int (*action_refresh)(file_list_t *list, file_list_t *menu_list);
   int (*action_up_or_down)(unsigned type, const char *label, unsigned action);
   void (*action_get_representation)(file_list_t* list,
         unsigned *w, unsigned type, unsigned i,
         const char *label,
         char *type_str, size_t type_str_size,
         const char *entry_label,
         const char *path,
         char *path_buf, size_t path_buf_size);
} menu_file_list_cbs_t;

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
   void  (*populate_entries)(void*, const char *, const char *,
         unsigned);
   void  (*toggle)(bool);
   void  (*navigation_clear)(void *, bool);
   void  (*navigation_decrement)(void *);
   void  (*navigation_increment)(void *);
   void  (*navigation_set)(void *, bool);
   void  (*navigation_set_last)(void *);
   void  (*navigation_descend_alphabet)(void *, size_t *);
   void  (*navigation_ascend_alphabet)(void *, size_t *);
   void  (*list_insert)(void *, const char *, const char *, size_t);
   void  (*list_delete)(void *, size_t, size_t);
   void  (*list_clear)(void *);
   void  (*list_cache)(bool, unsigned);
   void  (*list_set_selection)(void *);
   int   (*entry_iterate)(menu_handle_t *menu, unsigned);
   const char *ident;
} menu_ctx_driver_t;

extern menu_ctx_driver_t menu_ctx_rmenu;
extern menu_ctx_driver_t menu_ctx_rmenu_xui;
extern menu_ctx_driver_t menu_ctx_rgui;
extern menu_ctx_driver_t menu_ctx_glui;
extern menu_ctx_driver_t menu_ctx_xmb;
extern menu_ctx_driver_t menu_ctx_ios;

/**
 * menu_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to menu driver at index. Can be NULL
 * if nothing found.
 **/
const void *menu_driver_find_handle(int index);

/**
 * menu_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of menu driver at index. Can be NULL
 * if nothing found.
 **/
const char *menu_driver_find_ident(int index);

/**
 * config_get_menu_driver_options:
 *
 * Get an enumerated list of all menu driver names,
 * separated by '|'.
 *
 * Returns: string listing of all menu driver names,
 * separated by '|'.
 **/
const char* config_get_menu_driver_options(void);

void find_menu_driver(void);

void init_menu(void);

#ifdef __cplusplus
}
#endif

#endif


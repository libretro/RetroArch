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
#include "menu_display.h"
#include "menu_displaylist.h"
#include "menu_entries.h"
#include "menu_list.h"
#include "menu_input.h"
#include "menu_navigation.h"
#include "menu_setting.h"
#include "../libretro.h"
#include "../playlist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   MENU_IMAGE_NONE = 0,
   MENU_IMAGE_WALLPAPER,
   MENU_IMAGE_BOXART
} menu_image_type_t;

typedef enum
{
   MENU_ENVIRON_NONE = 0,
   MENU_ENVIRON_RESET_HORIZONTAL_LIST,
   MENU_ENVIRON_LAST
} menu_environ_cb_t;

typedef enum
{
   MENU_HELP_NONE       = 0,
   MENU_HELP_WELCOME,
   MENU_HELP_EXTRACT,
   MENU_HELP_CONTROLS,
   MENU_HELP_LOADING_CONTENT,
   MENU_HELP_WHAT_IS_A_CORE,
   MENU_HELP_CHANGE_VIRTUAL_GAMEPAD,
   MENU_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   MENU_HELP_SCANNING_CONTENT,
   MENU_HELP_LAST
} menu_help_type_t;

typedef struct
{
   void *userdata;

   float scroll_y;

   bool push_help_screen;
   menu_help_type_t help_screen_type;

   bool defer_core;
   char deferred_path[PATH_MAX_LENGTH];

   char scratch_buf[PATH_MAX_LENGTH];
   char scratch2_buf[PATH_MAX_LENGTH];

   /* Menu display */
   menu_display_t display;

   /* Menu entries */
   menu_entries_t entries;

   bool load_no_content;

   /* Menu shader */
   char default_glslp[PATH_MAX_LENGTH];
   char default_cgp[PATH_MAX_LENGTH];
   struct video_shader *shader;

   menu_input_t input;

   content_playlist_t *playlist;
   char db_playlist_file[PATH_MAX_LENGTH];
} menu_handle_t;

typedef struct menu_ctx_driver
{
   void  (*set_texture)(void);
   void  (*render_messagebox)(const char *msg);
   void  (*render)(void);
   void  (*frame)(void);
   void* (*init)(void);
   void  (*free)(void*);
   void  (*context_reset)(void);
   void  (*context_destroy)(void);
   void  (*populate_entries)(const char *path, const char *label,
         unsigned k);
   void  (*toggle)(bool);
   void  (*navigation_clear)(bool);
   void  (*navigation_decrement)(void);
   void  (*navigation_increment)(void);
   void  (*navigation_set)(bool);
   void  (*navigation_set_last)(void);
   void  (*navigation_descend_alphabet)(size_t *);
   void  (*navigation_ascend_alphabet)(size_t *);
   void  (*list_insert)(file_list_t *list, const char *, const char *, size_t);
   void  (*list_free)(file_list_t *list, size_t, size_t);
   void  (*list_clear)(file_list_t *list);
   void  (*list_cache)(menu_list_type_t, unsigned);
   size_t(*list_get_selection)(void *data);
   size_t(*list_get_size)(void *data, menu_list_type_t type);
   void *(*list_get_entry)(void *data, menu_list_type_t type, unsigned i);
   void  (*list_set_selection)(file_list_t *list);
   int   (*bind_init)(menu_file_list_cbs_t *cbs,
         const char *path, const char *label, unsigned type, size_t idx,
         const char *elem0, const char *elem1,
         uint32_t label_hash, uint32_t menu_label_hash);
   bool  (*load_image)(void *data, menu_image_type_t type);
   const char *ident;
   int (*environ_cb)(menu_environ_cb_t type, void *data);
   bool  (*perform_action)(void* data, unsigned action);
} menu_ctx_driver_t;

extern menu_ctx_driver_t menu_ctx_rmenu;
extern menu_ctx_driver_t menu_ctx_rmenu_xui;
extern menu_ctx_driver_t menu_ctx_rgui;
extern menu_ctx_driver_t menu_ctx_glui;
extern menu_ctx_driver_t menu_ctx_xmb;
extern menu_ctx_driver_t menu_ctx_null;

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

menu_handle_t *menu_driver_get_ptr(void);

void menu_driver_navigation_increment(void);

void menu_driver_navigation_decrement(void);

void menu_driver_navigation_clear(bool pending_push);

void menu_driver_navigation_set(bool scroll);

void menu_driver_navigation_set_last(void);

void menu_driver_set_texture(void);

void menu_driver_frame(void);

void menu_driver_context_reset(void);

void menu_driver_free(menu_handle_t *menu);

void menu_driver_render(void);

void menu_driver_toggle(bool latch);

void menu_driver_render_messagebox(const char *msg);

void menu_driver_populate_entries(const char *path, const char *label,
         unsigned k);

bool menu_driver_load_image(void *data, menu_image_type_t type);

void  menu_driver_navigation_descend_alphabet(size_t *);

void  menu_driver_navigation_ascend_alphabet(size_t *);

void menu_driver_list_cache(menu_list_type_t type, unsigned action);

void  menu_driver_list_free(file_list_t *list, size_t i, size_t list_size);

void  menu_driver_list_insert(file_list_t *list, const char *path,
      const char *label, unsigned type, size_t list_size);

void  menu_driver_list_clear(file_list_t *list);

size_t menu_driver_list_get_size(menu_list_type_t type);

void  menu_driver_list_set_selection(file_list_t *list);

void *menu_driver_list_get_entry(menu_list_type_t type, unsigned i);

const menu_ctx_driver_t *menu_ctx_driver_get_ptr(void);

void  menu_driver_context_destroy(void);

bool menu_driver_alive(void);

void menu_driver_set_alive(void);

void menu_driver_unset_alive(void);

size_t  menu_driver_list_get_selection(void);

bool menu_environment_cb(menu_environ_cb_t type, void *data);

int menu_driver_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash);

/* HACK */
extern unsigned int rdb_entry_start_game_selection_ptr;

#ifdef __cplusplus
}
#endif

#endif


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

#include <string.h>
#include <time.h>
#include <locale.h>

#include <compat/strl.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>

#ifdef HAVE_DISCORD
#include "discord/discord.h"
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif

#include "menu_animation.h"
#include "menu_driver.h"
#include "menu_cbs.h"
#include "menu_input.h"
#include "menu_entries.h"
#include "widgets/menu_dialog.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "menu_shader.h"
#endif

#include "../config.def.h"
#include "../content.h"
#include "../core.h"
#include "../configuration.h"
#include "../dynamic.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../version.h"
#include "../defaults.h"
#include "../frontend/frontend.h"
#include "../list_special.h"
#include "../tasks/tasks_internal.h"
#include "../ui/ui_companion_driver.h"
#include "../verbosity.h"
#include "../tasks/task_powerstate.h"

#define SCROLL_INDEX_SIZE          (2 * (26 + 2) + 1)

#define PARTICLES_COUNT            100

#define POWERSTATE_CHECK_INTERVAL  (30 * 1000000)
#define DATETIME_CHECK_INTERVAL    1000000

typedef struct menu_ctx_load_image
{
   void *data;
   enum menu_image_type type;
} menu_ctx_load_image_t;

float osk_dark[16] =  {
   0.00, 0.00, 0.00, 0.85,
   0.00, 0.00, 0.00, 0.85,
   0.00, 0.00, 0.00, 0.85,
   0.00, 0.00, 0.00, 0.85,
};

/* Menu drivers */
static const menu_ctx_driver_t *menu_ctx_drivers[] = {
#if defined(HAVE_MATERIALUI)
   &menu_ctx_mui,
#endif
#if defined(HAVE_OZONE)
   &menu_ctx_ozone,
#endif
#if defined(HAVE_RGUI)
   &menu_ctx_rgui,
#endif
#if defined(HAVE_STRIPES)
   &menu_ctx_stripes,
#endif
#if defined(HAVE_XMB)
   &menu_ctx_xmb,
#endif
#if defined(HAVE_XUI)
   &menu_ctx_xui,
#endif
   &menu_ctx_null,
   NULL
};

/* Menu display drivers */
static menu_display_ctx_driver_t *menu_display_ctx_drivers[] = {
#ifdef HAVE_D3D8
   &menu_display_ctx_d3d8,
#endif
#ifdef HAVE_D3D9
   &menu_display_ctx_d3d9,
#endif
#ifdef HAVE_D3D10
   &menu_display_ctx_d3d10,
#endif
#ifdef HAVE_D3D11
   &menu_display_ctx_d3d11,
#endif
#ifdef HAVE_D3D12
   &menu_display_ctx_d3d12,
#endif
#ifdef HAVE_OPENGL
   &menu_display_ctx_gl,
#endif
#ifdef HAVE_OPENGL1
   &menu_display_ctx_gl1,
#endif
#ifdef HAVE_OPENGL_CORE
   &menu_display_ctx_gl_core,
#endif
#ifdef HAVE_VULKAN
   &menu_display_ctx_vulkan,
#endif
#ifdef HAVE_METAL
   &menu_display_ctx_metal,
#endif
#ifdef HAVE_VITA2D
   &menu_display_ctx_vita2d,
#endif
#ifdef _3DS
   &menu_display_ctx_ctr,
#endif
#ifdef WIIU
   &menu_display_ctx_wiiu,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
   &menu_display_ctx_gdi,
#endif
#endif
#ifdef DJGPP
   &menu_display_ctx_vga,
#endif
#ifdef HAVE_SIXEL
   &menu_display_ctx_sixel,
#endif
#ifdef HAVE_CACA
   &menu_display_ctx_caca,
#endif
#ifdef HAVE_FPGA
   &menu_display_ctx_fpga,
#endif
   &menu_display_ctx_null,
   NULL,
};

uintptr_t menu_display_white_texture;

static video_coord_array_t menu_disp_ca;

/* Width, height and pitch of the menu framebuffer */
static unsigned menu_display_framebuf_width      = 0;
static unsigned menu_display_framebuf_height     = 0;
static size_t menu_display_framebuf_pitch        = 0;

/* Height of the menu display header */
static unsigned menu_display_header_height       = 0;

static bool menu_display_has_windowed            = false;
static bool menu_display_msg_force               = false;
static bool menu_display_framebuf_dirty          = false;
static menu_display_ctx_driver_t *menu_disp      = NULL;

/* when enabled, on next iteration the 'Quick Menu' list will
 * be pushed onto the stack */
static bool menu_driver_pending_quick_menu      = false;

static bool menu_driver_prevent_populate        = false;

/* The menu driver owns the userdata */
static bool menu_driver_data_own                = false;

static menu_handle_t *menu_driver_data          = NULL;
static const menu_ctx_driver_t *menu_driver_ctx = NULL;
static void *menu_userdata                      = NULL;

/* Quick jumping indices with L/R.
 * Rebuilt when parsing directory. */
static size_t   scroll_index_list[SCROLL_INDEX_SIZE];
static unsigned scroll_index_size               = 0;
static unsigned scroll_acceleration             = 0;
static size_t menu_driver_selection_ptr         = 0;

/* Timers */
static retro_time_t menu_driver_current_time_us         = 0;
static retro_time_t menu_driver_powerstate_last_time_us = 0;
static retro_time_t menu_driver_datetime_last_time_us   = 0;

/* Storage container for current menu datetime
 * representation string */
static char menu_datetime_cache[255]                    = {0};

/* Flagged when menu entries need to be refreshed */
static bool menu_entries_need_refresh              = false;
static bool menu_entries_nonblocking_refresh       = false;
static size_t menu_entries_begin                   = 0;
static rarch_setting_t *menu_entries_list_settings = NULL;
static menu_list_t *menu_entries_list              = NULL;

struct menu_list
{
   size_t menu_stack_size;
   size_t selection_buf_size;
   file_list_t **menu_stack;
   file_list_t **selection_buf;
};

#define menu_entries_need_refresh() ((!menu_entries_nonblocking_refresh) && menu_entries_need_refresh)

menu_handle_t *menu_driver_get_ptr(void)
{
   return menu_driver_data;
}

size_t menu_navigation_get_selection(void)
{
   return menu_driver_selection_ptr;
}

void menu_navigation_set_selection(size_t val)
{
   menu_driver_selection_ptr = val;
}


#define menu_list_get(list, idx) ((list) ? ((list)->menu_stack[(idx)]) : NULL)

#define menu_list_get_selection(list, idx) ((list) ? ((list)->selection_buf[(idx)]) : NULL)

#define menu_list_get_stack_size(list, idx) ((list)->menu_stack[(idx)]->size)

#define menu_entries_get_selection_buf_ptr_internal(idx) ((menu_entries_list) ? menu_list_get_selection(menu_entries_list, (unsigned)idx) : NULL)

/* Menu entry interface -
 *
 * This provides an abstraction of the currently displayed
 * menu.
 *
 * It is organized into an event-based system where the UI companion
 * calls this functions and RetroArch responds by changing the global
 * state (including arranging for these functions to return different
 * values).
 *
 * Its only interaction back to the UI is to arrange for
 * notify_list_loaded on the UI companion.
 */

enum menu_entry_type menu_entry_get_type(uint32_t i)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs  = NULL;
   rarch_setting_t *setting   = NULL;
   
   /* FIXME/TODO - XXX Really a special kind of ST_ACTION, 
    * but this should be changed */
   if (menu_setting_ctl(MENU_SETTING_CTL_IS_OF_PATH_TYPE, (void*)setting))
      return MENU_ENTRY_PATH;

   cbs                        = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   setting                    = cbs ? cbs->setting : NULL;

   if (setting)
   {
      switch (setting_get_type(setting))
      {
         case ST_BOOL:
            return MENU_ENTRY_BOOL;
         case ST_BIND:
            return MENU_ENTRY_BIND;
         case ST_INT:
            return MENU_ENTRY_INT;
         case ST_UINT:
            return MENU_ENTRY_UINT;
         case ST_SIZE:
            return MENU_ENTRY_SIZE;
         case ST_FLOAT:
            return MENU_ENTRY_FLOAT;
         case ST_PATH:
            return MENU_ENTRY_PATH;
         case ST_DIR:
            return MENU_ENTRY_DIR;
         case ST_STRING_OPTIONS:
            return MENU_ENTRY_ENUM;
         case ST_STRING:
            return MENU_ENTRY_STRING;
         case ST_HEX:
            return MENU_ENTRY_HEX;

         default:
            break;
      }
   }

   return MENU_ENTRY_ACTION;
}

void menu_entry_init(menu_entry_t *entry)
{
   entry->path[0]            = '\0';
   entry->label[0]           = '\0';
   entry->sublabel[0]        = '\0';
   entry->rich_label[0]      = '\0';
   entry->value[0]           = '\0';
   entry->password_value[0]  = '\0';
   entry->enum_idx           = MSG_UNKNOWN;
   entry->entry_idx          = 0;
   entry->idx                = 0;
   entry->type               = 0;
   entry->spacing            = 0;
   entry->path_enabled       = true;
   entry->label_enabled      = true;
   entry->rich_label_enabled = true;
   entry->value_enabled      = true;
   entry->sublabel_enabled   = true;
}

void menu_entry_get_path(menu_entry_t *entry, const char **path)
{
   if (!entry || !path)
      return;

   *path = entry->path;
}

void menu_entry_get_rich_label(menu_entry_t *entry, const char **rich_label)
{
   if (!entry || !rich_label)
      return;

   if (!string_is_empty(entry->rich_label))
      *rich_label = entry->rich_label;
   else
      *rich_label = entry->path;
}

void menu_entry_get_sublabel(menu_entry_t *entry, const char **sublabel)
{
   if (!entry || !sublabel)
      return;

   *sublabel = entry->sublabel;
}

void menu_entry_get_label(menu_entry_t *entry, const char **label)
{
   if (!entry || !label)
      return;

   *label = entry->label;
}

unsigned menu_entry_get_spacing(menu_entry_t *entry)
{
   if (entry)
      return entry->spacing;
   return 0;
}

unsigned menu_entry_get_type_new(menu_entry_t *entry)
{
   if (!entry)
      return 0;
   return entry->type;
}

uint32_t menu_entry_get_bool_value(uint32_t i)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting   = cbs ? cbs->setting : NULL;
   bool *ptr                  = setting ? (bool*)setting->value.target.boolean : NULL;
   if (!ptr)
      return 0;
   return *ptr;
}

struct string_list *menu_entry_enum_values(uint32_t i)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting   = cbs ? cbs->setting : NULL;
   const char      *values    = setting->values;

   if (!values)
      return NULL;
   return string_split(values, "|");
}

void menu_entry_enum_set_value_with_string(uint32_t i, const char *s)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting   = cbs ? cbs->setting : NULL;
   setting_set_with_string_representation(setting, s);
}

int32_t menu_entry_bind_index(uint32_t i)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting   = cbs ? cbs->setting : NULL;

   if (setting)
      return setting->index - 1;
   return 0;
}

void menu_entry_bind_key_set(uint32_t i, int32_t value)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   struct retro_keybind *keybind = setting ? (struct retro_keybind*)setting->value.target.keybind : NULL;
   if (keybind)
      keybind->key = (enum retro_key)value;
}

void menu_entry_bind_joykey_set(uint32_t i, int32_t value)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   struct retro_keybind *keybind = setting ? (struct retro_keybind*)setting->value.target.keybind : NULL;
   if (keybind)
      keybind->joykey = value;
}

void menu_entry_bind_joyaxis_set(uint32_t i, int32_t value)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   struct retro_keybind *keybind = setting ? (struct retro_keybind*)setting->value.target.keybind : NULL;
   if (keybind)
      keybind->joyaxis = value;
}

void menu_entry_pathdir_selected(uint32_t i)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;

   if (menu_setting_ctl(MENU_SETTING_CTL_IS_OF_PATH_TYPE, (void*)setting))
      menu_setting_ctl(MENU_SETTING_CTL_ACTION_RIGHT, setting);
}

bool menu_entry_pathdir_allow_empty(uint32_t i)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   uint64_t           flags      = setting->flags;

   return flags & SD_FLAG_ALLOW_EMPTY;
}

uint32_t menu_entry_pathdir_for_directory(uint32_t i)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   uint64_t           flags      = setting->flags;

   return flags & SD_FLAG_PATH_DIR;
}

void menu_entry_pathdir_extensions(uint32_t i, char *s, size_t len)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   const char      *values       = setting->values;

   if (!values)
      return;

   strlcpy(s, values, len);
}

void menu_entry_reset(uint32_t i)
{
   menu_entry_t entry;

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, i, NULL, true);

   menu_entry_action(&entry, i, MENU_ACTION_START);
}

void menu_entry_get_value(menu_entry_t *entry, const char **value)
{
   if (!entry || !value)
      return;

   if (menu_entry_is_password(entry))
      *value = entry->password_value;
   else
      *value = entry->value;
}

void menu_entry_set_value(uint32_t i, const char *s)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   setting_set_with_string_representation(setting, s);
}

bool menu_entry_is_password(menu_entry_t *entry)
{
   return entry->enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD;
}

uint32_t menu_entry_num_has_range(uint32_t i)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   uint64_t           flags      = setting->flags;

   return (flags & SD_FLAG_HAS_RANGE);
}

float menu_entry_num_min(uint32_t i)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   double               min      = setting->min;
   return (float)min;
}

float menu_entry_num_max(uint32_t i)
{
   file_list_t *selection_buf    = menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs     = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
   rarch_setting_t *setting      = cbs ? cbs->setting : NULL;
   double               max      = setting->max;
   return (float)max;
}

void menu_entry_get(menu_entry_t *entry, size_t stack_idx,
      size_t i, void *userdata, bool use_representation)
{
   char newpath[255];
   const char *path           = NULL;
   const char *entry_label    = NULL;
   menu_file_list_cbs_t *cbs  = NULL;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr_internal(stack_idx);
   file_list_t *list          = (userdata) ? (file_list_t*)userdata : selection_buf;
   bool path_enabled          = entry->path_enabled;

   newpath[0]                 = '\0';

   if (!list)
      return;

   path                       = list->list[i].path;
   entry_label                = list->list[i].label;
   entry->type                = list->list[i].type;
   entry->entry_idx           = list->list[i].entry_idx;

   cbs                        = (menu_file_list_cbs_t*)list->list[i].actiondata;
   entry->idx                 = (unsigned)i;

   if (entry->label_enabled && !string_is_empty(entry_label))
      strlcpy(entry->label, entry_label, sizeof(entry->label));

   if (cbs)
   {
      const char *label             = NULL;

      entry->enum_idx               = cbs->enum_idx;
      entry->checked                = cbs->checked;

      menu_entries_get_last_stack(NULL, &label, NULL, NULL, NULL);

      if (entry->rich_label_enabled && cbs->action_label)
      {
         cbs->action_label(list,
               entry->type, (unsigned)i,
               label, path,
               entry->rich_label,
               sizeof(entry->rich_label));

         if (string_is_empty(entry->rich_label))
            path_enabled = true;
      }

      if ((path_enabled || entry->value_enabled) &&
          cbs->action_get_value &&
          use_representation)
      {
         cbs->action_get_value(list,
               &entry->spacing, entry->type,
               (unsigned)i, label,
               entry->value,
               entry->value_enabled ? sizeof(entry->value) : 0,
               path,
               newpath,
               path_enabled ? sizeof(newpath) : 0);

         if (!string_is_empty(entry->value))
         {
            if (menu_entry_is_password(entry))
            {
               size_t size, i;
               size = strlcpy(entry->password_value, entry->value,
                     sizeof(entry->password_value));
               for (i = 0; i < size; i++)
                  entry->password_value[i] = '*';
            }
         }
      }

      if (entry->sublabel_enabled)
      {
         if (!string_is_empty(cbs->action_sublabel_cache))
            strlcpy(entry->sublabel,
                     cbs->action_sublabel_cache, sizeof(entry->sublabel));
         else if (cbs->action_sublabel)
         {
            char tmp[MENU_SUBLABEL_MAX_LENGTH];
            tmp[0] = '\0';

            if (cbs->action_sublabel(list,
                     entry->type, (unsigned)i,
                     label, path,
                     tmp,
                     sizeof(tmp)) > 0)
            {
               /* If this function callback returns true,
                * we know that the value won't change - so we
                * can cache it instead. */
               strlcpy(cbs->action_sublabel_cache,
                     tmp, sizeof(cbs->action_sublabel_cache));
            }

            strlcpy(entry->sublabel, tmp, sizeof(entry->sublabel));
         }
      }
   }

   if (path_enabled)
   {
      if (!string_is_empty(path) && !use_representation)
         strlcpy(newpath, path, sizeof(newpath));
      else if (cbs && cbs->setting && cbs->setting->enum_value_idx != MSG_UNKNOWN
            && !cbs->setting->dont_use_enum_idx_representation)
         strlcpy(newpath,
               msg_hash_to_str(cbs->setting->enum_value_idx),
               sizeof(newpath));

      if (!string_is_empty(newpath))
         strlcpy(entry->path, newpath, sizeof(entry->path));
   }
}

bool menu_entry_is_currently_selected(unsigned id)
{
   return id == menu_driver_selection_ptr;
}

/* Performs whatever actions are associated with menu entry 'i'.
 *
 * This is the most important function because it does all the work
 * associated with clicking on things in the UI.
 *
 * This includes loading cores and updating the
 * currently displayed menu. */
int menu_entry_select(uint32_t i)
{
   menu_entry_t     entry;

   menu_driver_selection_ptr = i;

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, i, NULL, false);

   return menu_entry_action(&entry, i, MENU_ACTION_SELECT);
}

int menu_entry_action(menu_entry_t *entry,
      unsigned i, enum menu_action action)
{
   int ret                    = 0;
   file_list_t *selection_buf =
      menu_entries_get_selection_buf_ptr_internal(0);
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (cbs && cbs->action_up)
            ret = cbs->action_up(entry->type, entry->label);
         break;
      case MENU_ACTION_DOWN:
         if (cbs && cbs->action_down)
            ret = cbs->action_down(entry->type, entry->label);
         break;
      case MENU_ACTION_SCROLL_UP:
         menu_driver_ctl(MENU_NAVIGATION_CTL_DESCEND_ALPHABET, NULL);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_driver_ctl(MENU_NAVIGATION_CTL_ASCEND_ALPHABET, NULL);
         break;
      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            ret = cbs->action_cancel(entry->path,
                  entry->label, entry->type, i);
         break;

      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            ret = cbs->action_ok(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_START:
         if (cbs && cbs->action_start)
            ret = cbs->action_start(entry->type, entry->label);
         break;
      case MENU_ACTION_LEFT:
         if (cbs && cbs->action_left)
            ret = cbs->action_left(entry->type, entry->label, false);
         break;
      case MENU_ACTION_RIGHT:
         if (cbs && cbs->action_right)
            ret = cbs->action_right(entry->type, entry->label, false);
         break;
      case MENU_ACTION_INFO:
         if (cbs && cbs->action_info)
            ret = cbs->action_info(entry->type, entry->label);
         break;
      case MENU_ACTION_SELECT:
         if (cbs && cbs->action_select)
            ret = cbs->action_select(entry->path,
                  entry->label, entry->type, i);
         break;
      case MENU_ACTION_SEARCH:
         menu_input_dialog_start_search();
         break;

      case MENU_ACTION_SCAN:
         if (cbs && cbs->action_scan)
            ret = cbs->action_scan(entry->path,
                  entry->label, entry->type, i);
         break;

      default:
         break;
   }

   cbs = selection_buf ? (menu_file_list_cbs_t*)
      selection_buf->list[i].actiondata : NULL;

   if (cbs && cbs->action_refresh)
   {
      if (menu_entries_need_refresh())
      {
         bool refresh               = false;
         file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);

         cbs->action_refresh(selection_buf, menu_stack);
         menu_entries_ctl(MENU_ENTRIES_CTL_UNSET_REFRESH, &refresh);
      }
   }

   return ret;
}

static void menu_list_free_list(file_list_t *list)
{
   unsigned i;

   for (i = 0; i < list->size; i++)
   {
      menu_ctx_list_t list_info;

      list_info.list      = list;
      list_info.idx       = i;
      list_info.list_size = list->size;

      menu_driver_ctl(RARCH_MENU_CTL_LIST_FREE, &list_info);
   }

   file_list_free(list);
}

static void menu_list_free(menu_list_t *menu_list)
{
   if (!menu_list)
      return;

   if (menu_list->menu_stack)
   {
      unsigned i;

      for (i = 0; i < menu_list->menu_stack_size; i++)
      {
         if (!menu_list->menu_stack[i])
            continue;

         menu_list_free_list(menu_list->menu_stack[i]);
         menu_list->menu_stack[i]    = NULL;
      }

      free(menu_list->menu_stack);
   }

   if (menu_list->selection_buf)
   {
      unsigned i;

      for (i = 0; i < menu_list->selection_buf_size; i++)
      {
         if (!menu_list->selection_buf[i])
            continue;

         menu_list_free_list(menu_list->selection_buf[i]);
         menu_list->selection_buf[i] = NULL;
      }

      free(menu_list->selection_buf);
   }

   free(menu_list);
}

static menu_list_t *menu_list_new(void)
{
   unsigned i;
   menu_list_t           *list = (menu_list_t*)malloc(sizeof(*list));

   if (!list)
      return NULL;

   list->menu_stack_size       = 1;
   list->selection_buf_size    = 1;
   list->selection_buf         = NULL;
   list->menu_stack            = (file_list_t**)
      calloc(list->menu_stack_size, sizeof(*list->menu_stack));

   if (!list->menu_stack)
      goto error;

   list->selection_buf         = (file_list_t**)
      calloc(list->selection_buf_size, sizeof(*list->selection_buf));

   if (!list->selection_buf)
      goto error;

   for (i = 0; i < list->menu_stack_size; i++)
      list->menu_stack[i]      = (file_list_t*)
         calloc(1, sizeof(*list->menu_stack[i]));

   for (i = 0; i < list->selection_buf_size; i++)
      list->selection_buf[i]   = (file_list_t*)
         calloc(1, sizeof(*list->selection_buf[i]));

   return list;

error:
   menu_list_free(list);
   return NULL;
}

static int menu_list_flush_stack_type(const char *needle, const char *label,
      unsigned type, unsigned final_type)
{
   return needle ? !string_is_equal(needle, label) : (type != final_type);
}

static bool menu_list_pop_stack(menu_list_t *list,
      size_t idx, size_t *directory_ptr, bool animate)
{
   menu_ctx_list_t list_info;
   bool refresh           = false;
   file_list_t *menu_list = menu_list_get(list, (unsigned)idx);

   if (menu_list_get_stack_size(list, idx) <= 1)
      return false;

   list_info.type   = MENU_LIST_PLAIN;
   list_info.action = 0;

   if (animate)
      menu_driver_list_cache(&list_info);

   if (menu_list->size != 0)
   {
      menu_ctx_list_t list_info;

      list_info.list      = menu_list;
      list_info.idx       = menu_list->size - 1;
      list_info.list_size = menu_list->size - 1;

      menu_driver_ctl(RARCH_MENU_CTL_LIST_FREE, &list_info);
   }

   file_list_pop(menu_list, directory_ptr);
   menu_driver_list_set_selection(menu_list);
   if (animate)
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

   return true;
}

static void menu_list_flush_stack(menu_list_t *list,
      size_t idx, const char *needle, unsigned final_type)
{
   bool refresh           = false;
   const char *path       = NULL;
   const char *label      = NULL;
   unsigned type          = 0;
   size_t entry_idx       = 0;
   file_list_t *menu_list = menu_list_get(list, (unsigned)idx);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   file_list_get_last(menu_list,
         &path, &label, &type, &entry_idx);

   while (menu_list_flush_stack_type(
            needle, label, type, final_type) != 0)
   {
      size_t new_selection_ptr = menu_driver_selection_ptr;

      if (!menu_list_pop_stack(list, idx, &new_selection_ptr, 1))
         break;

      menu_driver_selection_ptr = new_selection_ptr;

      menu_list = menu_list_get(list, (unsigned)idx);

      file_list_get_last(menu_list,
            &path, &label, &type, &entry_idx);
   }
}

void menu_entries_get_at_offset(const file_list_t *list, size_t idx,
      const char **path, const char **label, unsigned *file_type,
      size_t *entry_idx, const char **alt)
{
   file_list_get_at_offset(list, idx, path, label, file_type, entry_idx);
   if (list && alt)
      *alt = list->list[idx].alt 
         ? list->list[idx].alt 
         : list->list[idx].path;
}

/**
 * menu_entries_elem_get_first_char:
 * @list                     : File list handle.
 * @offset                   : Offset index of element.
 *
 * Gets the first character of an element in the
 * file list.
 *
 * Returns: first character of element in file list.
 **/
static int menu_entries_elem_get_first_char(
      file_list_t *list, unsigned offset)
{
   int ret          = 0;
   const char *path = NULL;

   if (list)
      if ((path = list->list[offset].alt
         ? list->list[offset].alt 
         : list->list[offset].path))
         ret = tolower((int)*path);

   /* "Normalize" non-alphabetical entries so they
    * are lumped together for purposes of jumping. */
   if (ret < 'a')
      return ('a' - 1);
   else if (ret > 'z')
      return ('z' + 1);
   return ret;
}

static void menu_navigation_add_scroll_index(size_t sel)
{
   scroll_index_list[scroll_index_size]   = sel;

   if (!((scroll_index_size + 1) >= SCROLL_INDEX_SIZE))
      scroll_index_size++;
}

static void menu_entries_build_scroll_indices(file_list_t *list)
{
   int current;
   bool current_is_dir      = false;
   unsigned type            = 0;
   size_t i                 = 0;

   scroll_index_size        = 0;

   menu_navigation_add_scroll_index(0);

   current                  = menu_entries_elem_get_first_char(list, 0);
   type                     = list->list[0].type;

   if (type == FILE_TYPE_DIRECTORY)
      current_is_dir = true;

   for (i = 1; i < list->size; i++)
   {
      int first    = menu_entries_elem_get_first_char(list, (unsigned)i);
      bool is_dir  = false;
      unsigned idx = (unsigned)i;

      type         = list->list[idx].type;

      if (type == FILE_TYPE_DIRECTORY)
         is_dir = true;

      if ((current_is_dir && !is_dir) || (first > current))
         menu_navigation_add_scroll_index(i);

      current        = first;
      current_is_dir = is_dir;
   }

   menu_navigation_add_scroll_index(list->size - 1);
}

/**
 * Before a refresh, we could have deleted a
 * file on disk, causing selection_ptr to
 * suddendly be out of range.
 *
 * Ensure it doesn't overflow.
 **/
static bool menu_entries_refresh(file_list_t *list)
{
   size_t list_size;
   size_t selection  = menu_driver_selection_ptr;

   if (list->size)
      menu_entries_build_scroll_indices(list);

   list_size         = menu_entries_get_size();

   if ((selection >= list_size) && list_size)
   {
      size_t idx                = list_size - 1;
      menu_driver_selection_ptr = idx;
      menu_driver_navigation_set(true);
   }
   else if (!list_size)
   {
      bool pending_push = true;
      menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
   }

   return true;
}

menu_file_list_cbs_t *menu_entries_get_last_stack_actiondata(void)
{
   if (menu_entries_list)
   {
      const file_list_t *list = menu_list_get(menu_entries_list, 0);
      return (menu_file_list_cbs_t*)list->list[list->size - 1].actiondata;
   }
   return NULL;
}

/* Sets title to what the name of the current menu should be. */
int menu_entries_get_title(char *s, size_t len)
{
   unsigned menu_type            = 0;
   const char *path              = NULL;
   const char *label             = NULL;
   const file_list_t *list       = menu_entries_list ? 
      menu_list_get(menu_entries_list, 0) : NULL;
   menu_file_list_cbs_t *cbs     = list 
      ? (menu_file_list_cbs_t*)list->list[list->size - 1].actiondata
      : NULL;

   if (!cbs)
      return -1;

   if (cbs && cbs->action_get_title)
   {
      int ret;
      if (!string_is_empty(cbs->action_title_cache))
      {
         strlcpy(s, cbs->action_title_cache, len);
         return 0;
      }
      menu_entries_get_last_stack(&path, &label, &menu_type, NULL, NULL);
      ret = cbs->action_get_title(path, label, menu_type, s, len);
      if (ret == 1)
         strlcpy(cbs->action_title_cache, s, sizeof(cbs->action_title_cache));
      return ret;
   }
   return 0;
}

/* Sets 's' to the name of the current core
 * (shown at the top of the UI). */
int menu_entries_get_core_title(char *s, size_t len)
{
   struct retro_system_info    *system = runloop_get_libretro_system_info();
   const char *core_name               = (system && !string_is_empty(system->library_name)) ? system->library_name    : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);
   const char *core_version            = (system && system->library_version) ? system->library_version : "";
#if _MSC_VER == 1200
   strlcpy(s, PACKAGE_VERSION " msvc6" " - ", len);
#elif _MSC_VER == 1300
   strlcpy(s, PACKAGE_VERSION " msvc2002" " - ", len);
#elif _MSC_VER == 1310
   strlcpy(s, PACKAGE_VERSION " msvc2003" " - ", len);
#elif _MSC_VER == 1400
   strlcpy(s, PACKAGE_VERSION " msvc2005" " - ", len);
#elif _MSC_VER == 1500
   strlcpy(s, PACKAGE_VERSION " msvc2008" " - ", len);
#elif _MSC_VER == 1600
   strlcpy(s, PACKAGE_VERSION " msvc2010" " - ", len);
#elif _MSC_VER == 1700
   strlcpy(s, PACKAGE_VERSION " msvc2012" " - ", len);
#elif _MSC_VER == 1800
   strlcpy(s, PACKAGE_VERSION " msvc2013" " - ", len);
#elif _MSC_VER == 1900
   strlcpy(s, PACKAGE_VERSION " msvc2015" " - ", len);
#elif _MSC_VER >= 1910 && _MSC_VER < 2000
   strlcpy(s, PACKAGE_VERSION " msvc2017" " - ", len);
#else
   strlcpy(s, PACKAGE_VERSION " - ", len);
#endif
   strlcat(s, core_name, len);
   if (!string_is_empty(core_version))
   {
      strlcat(s, " (", len);
      strlcat(s, core_version, len);
      strlcat(s, ")", len);
   }

   return 0;
}

file_list_t *menu_entries_get_menu_stack_ptr(size_t idx)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return NULL;
   return menu_list_get(menu_list, (unsigned)idx);
}

file_list_t *menu_entries_get_selection_buf_ptr(size_t idx)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return NULL;
   return menu_list_get_selection(menu_list, (unsigned)idx);
}

static void menu_entries_list_deinit(void)
{
   if (menu_entries_list)
      menu_list_free(menu_entries_list);
   menu_entries_list     = NULL;
}

static void menu_entries_settings_deinit(void)
{
   menu_setting_free(menu_entries_list_settings);
   if (menu_entries_list_settings)
      free(menu_entries_list_settings);
   menu_entries_list_settings = NULL;
}


static bool menu_entries_init(void)
{
   if (!(menu_entries_list = (menu_list_t*)menu_list_new()))
      goto error;

   menu_setting_ctl(MENU_SETTING_CTL_NEW, &menu_entries_list_settings);

   if (!menu_entries_list_settings)
      goto error;

   return true;

error:
   menu_entries_settings_deinit();
   menu_entries_list_deinit();

   return false;
}

void menu_entries_set_checked(file_list_t *list, size_t entry_idx,
      bool checked)
{
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[entry_idx].actiondata;

   if (cbs)
      cbs->checked = checked;
}

void menu_entries_append(file_list_t *list, const char *path, const char *label,
      unsigned type, size_t directory_ptr, size_t entry_idx)
{
   menu_ctx_list_t list_info;
   size_t idx;
   const char *menu_path           = NULL;
   menu_file_list_cbs_t *cbs       = NULL;
   if (!list || !label)
      return;

   file_list_append(list, path, label, type, directory_ptr, entry_idx);

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);

   idx                = list->size - 1;

   list_info.list     = list;
   list_info.path     = path;
   list_info.fullpath = NULL;

   if (!string_is_empty(menu_path))
      list_info.fullpath = strdup(menu_path);

   list_info.label       = label;
   list_info.idx         = idx;
   list_info.entry_type  = type;

   menu_driver_list_insert(&list_info);

   if (list_info.fullpath)
      free(list_info.fullpath);

   file_list_free_actiondata(list, idx);
   cbs = (menu_file_list_cbs_t*)
      calloc(1, sizeof(menu_file_list_cbs_t));

   if (!cbs)
      return;

   file_list_set_actiondata(list, idx, cbs);

   cbs->enum_idx = MSG_UNKNOWN;
   cbs->setting  = menu_setting_find(label);

   menu_cbs_init(list, cbs, path, label, type, idx);
}

bool menu_entries_append_enum(file_list_t *list, const char *path,
      const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx)
{
   menu_ctx_list_t list_info;
   size_t idx;
   const char *menu_path           = NULL;
   menu_file_list_cbs_t *cbs       = NULL;
   if (!list || !label)
      return false;

   file_list_append(list, path, label, type, directory_ptr, entry_idx);

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);

   idx                   = list->size - 1;

   list_info.fullpath    = NULL;

   if (!string_is_empty(menu_path))
      list_info.fullpath = strdup(menu_path);
   list_info.list        = list;
   list_info.path        = path;
   list_info.label       = label;
   list_info.idx         = idx;
   list_info.entry_type  = type;

   menu_driver_list_insert(&list_info);

   if (list_info.fullpath)
      free(list_info.fullpath);

   file_list_free_actiondata(list, idx);
   cbs = (menu_file_list_cbs_t*)
      calloc(1, sizeof(menu_file_list_cbs_t));

   file_list_set_actiondata(list, idx, cbs);

   cbs->enum_idx = enum_idx;

   if (   enum_idx != MENU_ENUM_LABEL_PLAYLIST_ENTRY
       && enum_idx != MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY
       && enum_idx != MENU_ENUM_LABEL_RDB_ENTRY)
      cbs->setting  = menu_setting_find_enum(enum_idx);

   menu_cbs_init(list, cbs, path, label, type, idx);

   return true;
}

void menu_entries_prepend(file_list_t *list,
      const char *path, const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx)
{
   menu_ctx_list_t list_info;
   size_t idx;
   const char *menu_path           = NULL;
   menu_file_list_cbs_t *cbs       = NULL;
   if (!list || !label)
      return;

   file_list_prepend(list, path, label, type, directory_ptr, entry_idx);

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);

   idx              = 0;

   list_info.fullpath    = NULL;

   if (!string_is_empty(menu_path))
      list_info.fullpath = strdup(menu_path);
   list_info.list        = list;
   list_info.path        = path;
   list_info.label       = label;
   list_info.idx         = idx;
   list_info.entry_type  = type;

   menu_driver_list_insert(&list_info);

   if (list_info.fullpath)
      free(list_info.fullpath);

   file_list_free_actiondata(list, idx);
   cbs = (menu_file_list_cbs_t*)
      calloc(1, sizeof(menu_file_list_cbs_t));

   if (!cbs)
      return;

   file_list_set_actiondata(list, idx, cbs);

   cbs->enum_idx = enum_idx;
   cbs->setting  = menu_setting_find_enum(cbs->enum_idx);

   menu_cbs_init(list, cbs, path, label, type, idx);
}

void menu_entries_get_last_stack(const char **path, const char **label,
      unsigned *file_type, enum msg_hash_enums *enum_idx, size_t *entry_idx)
{
   file_list_t *list              = NULL;
   if (!menu_entries_list)
      return;

   list = menu_list_get(menu_entries_list, 0);

   file_list_get_last(list,
         path, label, file_type, entry_idx);

   if (enum_idx)
   {
      menu_file_list_cbs_t *cbs  = (menu_file_list_cbs_t*)
         list->list[list->size - 1].actiondata;

      if (cbs)
         *enum_idx = cbs->enum_idx;
   }
}

void menu_entries_flush_stack(const char *needle, unsigned final_type)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (menu_list)
      menu_list_flush_stack(menu_list, 0, needle, final_type);
}

void menu_entries_pop_stack(size_t *ptr, size_t idx, bool animate)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (menu_list)
      menu_list_pop_stack(menu_list, idx, ptr, animate);
}

size_t menu_entries_get_stack_size(size_t idx)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return 0;
   return menu_list_get_stack_size(menu_list, idx);
}

size_t menu_entries_get_size(void)
{
   const file_list_t *list        = NULL;
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return 0;
   list                           = menu_list_get_selection(menu_list, 0);
   return list->size;
}

bool menu_entries_ctl(enum menu_entries_ctl_state state, void *data)
{
   switch (state)
   {
      case MENU_ENTRIES_CTL_NEEDS_REFRESH:
         if (menu_entries_nonblocking_refresh)
            return false;
         if (!menu_entries_need_refresh)
            return false;
         break;
      case MENU_ENTRIES_CTL_LIST_GET:
         {
            menu_list_t **list = (menu_list_t**)data;
            if (!list)
               return false;
            *list = menu_entries_list;
         }
         return true;
      case MENU_ENTRIES_CTL_SETTINGS_GET:
         {
            rarch_setting_t **settings = (rarch_setting_t**)data;
            if (!settings)
               return false;
            *settings = menu_entries_list_settings;
         }
         break;
      case MENU_ENTRIES_CTL_SET_REFRESH:
         {
            bool *nonblocking = (bool*)data;

            if (*nonblocking)
               menu_entries_nonblocking_refresh = true;
            else
               menu_entries_need_refresh        = true;
         }
         break;
      case MENU_ENTRIES_CTL_UNSET_REFRESH:
         {
            bool *nonblocking = (bool*)data;

            if (*nonblocking)
               menu_entries_nonblocking_refresh = false;
            else
               menu_entries_need_refresh        = false;
         }
         break;
      case MENU_ENTRIES_CTL_SET_START:
         {
            size_t *idx = (size_t*)data;
            if (idx)
               menu_entries_begin = *idx;
         }
      case MENU_ENTRIES_CTL_START_GET:
         {
            size_t *idx = (size_t*)data;
            if (!idx)
               return 0;

            *idx = menu_entries_begin;
         }
         break;
      case MENU_ENTRIES_CTL_REFRESH:
         if (!data)
            return false;
         return menu_entries_refresh((file_list_t*)data);
      case MENU_ENTRIES_CTL_CLEAR:
         {
            unsigned i;
            file_list_t *list = (file_list_t*)data;

            if (!list)
               return false;

            menu_driver_list_clear(list);

            for (i = 0; i < list->size; i++)
               file_list_free_actiondata(list, i);

            file_list_clear(list);
         }
         break;
      case MENU_ENTRIES_CTL_SHOW_BACK:
         /* Returns true if a Back button should be shown
          * (i.e. we are at least
          * one level deep in the menu hierarchy). */
         if (!menu_entries_list)
            return false;
         return (menu_list_get_stack_size(menu_entries_list, 0) > 1);
      case MENU_ENTRIES_CTL_NONE:
      default:
         break;
   }

   return true;
}

/* Returns the OSK key at a given position */
int menu_display_osk_ptr_at_pos(void *data, int x, int y,
      unsigned width, unsigned height)
{
   unsigned i;
   int ptr_width  = width / 11;
   int ptr_height = height / 10;

   if (ptr_width >= ptr_height)
      ptr_width = ptr_height;

   for (i = 0; i < 44; i++)
   {
      int line_y    = (i / 11)*height/10.0;
      int ptr_x     = width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width;
      int ptr_y     = height/2.0 + ptr_height*1.5 + line_y - ptr_height;

      if (x > ptr_x && x < ptr_x + ptr_width
       && y > ptr_y && y < ptr_y + ptr_height)
         return i;
   }

   return -1;
}

/* Check if the current menu driver is compatible
 * with your video driver. */
static bool menu_display_check_compatibility(
      enum menu_display_driver_type type,
      bool video_is_threaded)
{
   const char *video_driver = video_driver_get_ident();

   switch (type)
   {
      case MENU_VIDEO_DRIVER_GENERIC:
         return true;
      case MENU_VIDEO_DRIVER_OPENGL:
         if (string_is_equal(video_driver, "gl"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_OPENGL1:
         if (string_is_equal(video_driver, "gl1"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_OPENGL_CORE:
         if (string_is_equal(video_driver, "glcore"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_VULKAN:
         if (string_is_equal(video_driver, "vulkan"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_METAL:
         if (string_is_equal(video_driver, "metal"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D8:
         if (string_is_equal(video_driver, "d3d8"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D9:
         if (string_is_equal(video_driver, "d3d9"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D10:
         if (string_is_equal(video_driver, "d3d10"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D11:
         if (string_is_equal(video_driver, "d3d11"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D12:
         if (string_is_equal(video_driver, "d3d12"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_VITA2D:
         if (string_is_equal(video_driver, "vita2d"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_CTR:
         if (string_is_equal(video_driver, "ctr"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_WIIU:
         if (string_is_equal(video_driver, "gx2"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_SIXEL:
         if (string_is_equal(video_driver, "sixel"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_CACA:
         if (string_is_equal(video_driver, "caca"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_GDI:
         if (string_is_equal(video_driver, "gdi"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_VGA:
         if (string_is_equal(video_driver, "vga"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_FPGA:
         if (string_is_equal(video_driver, "fpga"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_SWITCH:
         if (string_is_equal(video_driver, "switch"))
            return true;
         break;
   }

   return false;
}

/* Time format strings with AM-PM designation require special
 * handling due to platform dependence */
static void strftime_am_pm(char* ptr, size_t maxsize, const char* format,
      const struct tm* timeptr)
{
   char *local = NULL;

#if defined(__linux__) && !defined(ANDROID)
   strftime(ptr, maxsize, format, timeptr);
#else
   strftime(ptr, maxsize, format, timeptr);
   local = local_to_utf8_string_alloc(ptr);

   if (!string_is_empty(local))
      strlcpy(ptr, local, maxsize);

   if (local)
   {
      free(local);
      local = NULL;
   }
#endif
}

/* Display the date and time - time_mode will influence how
 * the time representation will look like.
 * */
void menu_display_timedate(menu_display_ctx_datetime_t *datetime)
{
   if (!datetime)
      return;

   /* Trigger an update, if required */
   if (menu_driver_current_time_us - menu_driver_datetime_last_time_us >=
         DATETIME_CHECK_INTERVAL)
   {
      time_t time_;
      const struct tm *tm_;

      menu_driver_datetime_last_time_us = menu_driver_current_time_us;

      /* Get current time */
      time(&time_);

      setlocale(LC_TIME, "");

      tm_ = localtime(&time_);

      /* Format string representation */
      switch (datetime->time_mode)
      {
         case MENU_TIMEDATE_STYLE_YMD_HMS: /* YYYY-MM-DD HH:MM:SS */
            strftime(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%Y-%m-%d %H:%M:%S", tm_);
            break;
         case MENU_TIMEDATE_STYLE_YMD_HM: /* YYYY-MM-DD HH:MM */
            strftime(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%Y-%m-%d %H:%M", tm_);
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY: /* MM-DD-YYYY HH:MM */
            strftime(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%m-%d-%Y %H:%M", tm_);
            break;
         case MENU_TIMEDATE_STYLE_HMS: /* HH:MM:SS */
            strftime(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%H:%M:%S", tm_);
            break;
         case MENU_TIMEDATE_STYLE_HM: /* HH:MM */
            strftime(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%H:%M", tm_);
            break;
         case MENU_TIMEDATE_STYLE_DM_HM: /* DD/MM HH:MM */
            strftime(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%d/%m %H:%M", tm_);
            break;
         case MENU_TIMEDATE_STYLE_MD_HM: /* MM/DD HH:MM */
            strftime(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%m/%d %H:%M", tm_);
            break;
         case MENU_TIMEDATE_STYLE_YMD_HMS_AM_PM: /* YYYY-MM-DD HH:MM:SS (am/pm) */
            strftime_am_pm(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%Y-%m-%d %I:%M:%S %p", tm_);
            break;
         case MENU_TIMEDATE_STYLE_YMD_HM_AM_PM: /* YYYY-MM-DD HH:MM (am/pm) */
            strftime_am_pm(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%Y-%m-%d %I:%M %p", tm_);
            break;
         case MENU_TIMEDATE_STYLE_MDYYYY_AM_PM: /* MM-DD-YYYY HH:MM (am/pm) */
            strftime_am_pm(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%m-%d-%Y %I:%M %p", tm_);
            break;
         case MENU_TIMEDATE_STYLE_HMS_AM_PM: /* HH:MM:SS (am/pm) */
            strftime_am_pm(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%I:%M:%S %p", tm_);
            break;
         case MENU_TIMEDATE_STYLE_HM_AM_PM: /* HH:MM (am/pm) */
            strftime_am_pm(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%I:%M %p", tm_);
            break;
         case MENU_TIMEDATE_STYLE_DM_HM_AM_PM: /* DD/MM HH:MM (am/pm) */
            strftime_am_pm(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%d/%m %I:%M %p", tm_);
            break;
         case MENU_TIMEDATE_STYLE_MD_HM_AM_PM: /* MM/DD HH:MM (am/pm) */
            strftime_am_pm(menu_datetime_cache, sizeof(menu_datetime_cache),
                  "%m/%d %I:%M %p", tm_);
            break;
      }
   }

   /* Copy cached datetime string to input
    * menu_display_ctx_datetime_t struct */
   strlcpy(datetime->s, menu_datetime_cache, datetime->len);
}

/* Display current (battery) power state */
void menu_display_powerstate(menu_display_ctx_powerstate_t *powerstate)
{
   int percent                    = 0;
   enum frontend_powerstate state = FRONTEND_POWERSTATE_NONE;

   if (!powerstate)
      return;

   /* Trigger an update, if required */
   if (menu_driver_current_time_us - menu_driver_powerstate_last_time_us >=
         POWERSTATE_CHECK_INTERVAL)
   {
      menu_driver_powerstate_last_time_us = menu_driver_current_time_us;
      task_push_get_powerstate();
   }

   /* Get last recorded state */
   state = get_last_powerstate(&percent);

   /* Populate menu_display_ctx_powerstate_t */
   powerstate->battery_enabled = (state != FRONTEND_POWERSTATE_NONE) &&
                                 (state != FRONTEND_POWERSTATE_NO_SOURCE);

   if (powerstate->battery_enabled)
   {
      powerstate->charging = (state == FRONTEND_POWERSTATE_CHARGING);
      powerstate->percent  = percent > 0 ? (unsigned)percent : 0;
      snprintf(powerstate->s, powerstate->len, "%u%%", powerstate->percent);
   }
   else
   {
      powerstate->charging = false;
      powerstate->percent  = 0;
   }
}

/* Begin blending operation */
void menu_display_blend_begin(video_frame_info_t *video_info)
{
   if (menu_disp && menu_disp->blend_begin)
      menu_disp->blend_begin(video_info);
}

/* End blending operation */
void menu_display_blend_end(video_frame_info_t *video_info)
{
   if (menu_disp && menu_disp->blend_end)
      menu_disp->blend_end(video_info);
}

/* Begin scissoring operation */
void menu_display_scissor_begin(video_frame_info_t *video_info, int x, int y, unsigned width, unsigned height)
{
   if (menu_disp && menu_disp->scissor_begin)
   {
      if (y < 0)
      {
         if (height < (unsigned)(-y))
            height = 0;
         else
            height += y;
         y = 0;
      }
      if (x < 0)
      {
         if (width < (unsigned)(-x))
            width = 0;
         else
            width += x;
         x = 0;
      }
      if (y >= (int)video_info->height)
      {
         height = 0;
         y = 0;
      }
      if (x >= (int)video_info->width)
      {
         width = 0;
         x = 0;
      }
      if ((y + height) > video_info->height)
         height = video_info->height - y;
      if ((x + width) > video_info->width)
         width = video_info->width - x;

      menu_disp->scissor_begin(video_info, x, y, width, height);
   }
}

/* End scissoring operation */
void menu_display_scissor_end(video_frame_info_t *video_info)
{
   if (menu_disp && menu_disp->scissor_end)
      menu_disp->scissor_end(video_info);
}

/* Teardown; deinitializes and frees all
 * fonts associated to the menu driver */
void menu_display_font_free(font_data_t *font)
{
   font_driver_free(font);
}

/* Setup: Initializes the font associated
 * to the menu driver */
font_data_t *menu_display_font(
      enum application_special_type type,
      float menu_font_size,
      bool is_threaded)
{
   char fontpath[PATH_MAX_LENGTH];

   if (!menu_disp)
      return NULL;

   fontpath[0] = '\0';

   fill_pathname_application_special(
         fontpath, sizeof(fontpath), type);

   return menu_display_font_file(fontpath, menu_font_size, is_threaded);
}

font_data_t *menu_display_font_file(char* fontpath, float menu_font_size, bool is_threaded)
{
   font_data_t *font_data = NULL;

   if (!menu_disp)
      return NULL;

   if (!menu_disp->font_init_first((void**)&font_data,
            video_driver_get_ptr(false),
            fontpath, menu_font_size, is_threaded))
      return NULL;

   return font_data;
}

/* Reset the menu's coordinate array vertices.
 * NOTE: Not every menu driver uses this. */
void menu_display_coords_array_reset(void)
{
   menu_disp_ca.coords.vertices = 0;
}

video_coord_array_t *menu_display_get_coords_array(void)
{
   return &menu_disp_ca;
}

/* Get the menu framebuffer's size dimensions. */
void menu_display_get_fb_size(unsigned *fb_width,
      unsigned *fb_height, size_t *fb_pitch)
{
   *fb_width  = menu_display_framebuf_width;
   *fb_height = menu_display_framebuf_height;
   *fb_pitch  = menu_display_framebuf_pitch;
}

/* Set the menu framebuffer's width. */
void menu_display_set_width(unsigned width)
{
   menu_display_framebuf_width = width;
}

/* Set the menu framebuffer's height. */
void menu_display_set_height(unsigned height)
{
   menu_display_framebuf_height = height;
}

void menu_display_set_header_height(unsigned height)
{
   menu_display_header_height = height;
}

unsigned menu_display_get_header_height(void)
{
   return menu_display_header_height;
}

size_t menu_display_get_framebuffer_pitch(void)
{
   return menu_display_framebuf_pitch;
}

void menu_display_set_framebuffer_pitch(size_t pitch)
{
   menu_display_framebuf_pitch = pitch;
}

bool menu_display_get_msg_force(void)
{
   return menu_display_msg_force;
}

void menu_display_set_msg_force(bool state)
{
   menu_display_msg_force = state;
}

/* Returns true if an animation is still active or
 * when the menu framebuffer still is dirty and
 * therefore it still needs to be rendered onscreen.
 *
 * This function can be used for optimization purposes
 * so that we don't have to render the menu graphics per-frame
 * unless a change has happened.
 * */
bool menu_display_get_update_pending(void)
{
   if (menu_animation_is_active() || menu_display_framebuf_dirty)
      return true;
   return false;
}

void menu_display_set_viewport(unsigned width, unsigned height)
{
   video_driver_set_viewport(width, height, true, false);
}

void menu_display_unset_viewport(unsigned width, unsigned height)
{
   video_driver_set_viewport(width, height, false, true);
}

/* Checks if the menu framebuffer has its 'dirty flag' set. This
 * means that the current contents of the framebuffer has changed
 * and that it has to be rendered to the screen. */
bool menu_display_get_framebuffer_dirty_flag(void)
{
   return menu_display_framebuf_dirty;
}

/* Set the menu framebuffer's 'dirty flag'. */
void menu_display_set_framebuffer_dirty_flag(void)
{
   menu_display_framebuf_dirty = true;
}

/* Unset the menu framebufer's 'dirty flag'. */
void menu_display_unset_framebuffer_dirty_flag(void)
{
   menu_display_framebuf_dirty = false;
}

/* Get the preferred DPI at which to render the menu.
 * NOTE: Only MaterialUI menu driver so far uses this, neither
 * RGUI or XMB use this. */
float menu_display_get_dpi(unsigned width, unsigned height)
{
#ifdef RARCH_MOBILE
   float diagonal         = 5.0f;
#else
   float diagonal         = 6.5f;
#endif
   /* Generic dpi calculation formula,
    * the divider is the screen diagonal in inches */
   float dpi              = sqrt(
         (width * width) + (height * height)) / diagonal;

   {
      settings_t *settings = config_get_ptr();
      if (settings && settings->bools.menu_dpi_override_enable)
         return settings->uints.menu_dpi_override_value;
   }

   return dpi;
}

bool menu_display_driver_exists(const char *s)
{
   unsigned i;
   for (i = 0; i < ARRAY_SIZE(menu_display_ctx_drivers); i++)
   {
      if (string_is_equal(s, menu_display_ctx_drivers[i]->ident))
         return true;
   }

   return false;
}

bool menu_display_init_first_driver(bool video_is_threaded)
{
   unsigned i;

   for (i = 0; menu_display_ctx_drivers[i]; i++)
   {
      if (!menu_display_check_compatibility(
               menu_display_ctx_drivers[i]->type,
               video_is_threaded))
         continue;

      RARCH_LOG("[Menu]: Found menu display driver: \"%s\".\n",
            menu_display_ctx_drivers[i]->ident);
      menu_disp = menu_display_ctx_drivers[i];
      return true;
   }
   return false;
}

bool menu_display_restore_clear_color(void)
{
   if (!menu_disp || !menu_disp->restore_clear_color)
      return false;
   menu_disp->restore_clear_color();
   return true;
}

void menu_display_clear_color(menu_display_ctx_clearcolor_t *color,
      video_frame_info_t *video_info)
{
   if (menu_disp && menu_disp->clear_color)
      menu_disp->clear_color(color, video_info);
}

void menu_display_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   if (!menu_disp || !draw || !menu_disp->draw)
      return;

   if (draw->height <= 0)
      return;
   if (draw->width <= 0)
      return;

   menu_disp->draw(draw, video_info);
}

void menu_display_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   if (menu_disp && draw && menu_disp->draw_pipeline)
      menu_disp->draw_pipeline(draw, video_info);
}

void menu_display_draw_bg(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info, bool add_opacity_to_wallpaper,
      float override_opacity)
{
   static struct video_coords coords;
   const float *new_vertex       = NULL;
   const float *new_tex_coord    = NULL;
   if (!menu_disp || !draw)
      return;

   new_vertex           = draw->vertex;
   new_tex_coord        = draw->tex_coord;

   if (!new_vertex)
      new_vertex        = menu_disp->get_default_vertices();
   if (!new_tex_coord)
      new_tex_coord     = menu_disp->get_default_tex_coords();

   coords.vertices      = (unsigned)draw->vertex_count;
   coords.vertex        = new_vertex;
   coords.tex_coord     = new_tex_coord;
   coords.lut_tex_coord = new_tex_coord;
   coords.color         = (const float*)draw->color;

   draw->coords         = &coords;
   draw->scale_factor   = 1.0f;
   draw->rotation       = 0.0f;

   if (draw->texture)
      add_opacity_to_wallpaper = true;

   if (add_opacity_to_wallpaper)
      menu_display_set_alpha(draw->color, override_opacity);

   if (!draw->texture)
      draw->texture     = menu_display_white_texture;

   if (menu_disp && menu_disp->get_default_mvp)
      draw->matrix_data = (math_matrix_4x4*)menu_disp->get_default_mvp(video_info);
}

void menu_display_draw_gradient(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   draw->texture       = 0;
   draw->x             = 0;
   draw->y             = 0;

   menu_display_draw_bg(draw, video_info, false,
         video_info->menu_wallpaper_opacity);
   menu_display_draw(draw, video_info);
}

void menu_display_draw_quad(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   if (menu_disp && menu_disp->blend_begin)
      menu_disp->blend_begin(video_info);

   draw.x            = x;
   draw.y            = (int)height - y - (int)h;
   draw.width        = w;
   draw.height       = h;
   draw.coords       = &coords;
   draw.matrix_data  = NULL;
   draw.texture      = menu_display_white_texture;
   draw.prim_type    = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id  = 0;
   draw.scale_factor = 1.0f;
   draw.rotation     = 0.0f;

   menu_display_draw(&draw, video_info);

   if (menu_disp && menu_disp->blend_end)
      menu_disp->blend_end(video_info);
}

void menu_display_draw_polygon(
      video_frame_info_t *video_info,
      int x1, int y1,
      int x2, int y2,
      int x3, int y3,
      int x4, int y4,
      unsigned width, unsigned height,
      float *color)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;

   float vertex[8];

   vertex[0]             = x1 / (float)width;
   vertex[1]             = y1 / (float)height;
   vertex[2]             = x2 / (float)width;
   vertex[3]             = y2 / (float)height;
   vertex[4]             = x3 / (float)width;
   vertex[5]             = y3 / (float)height;
   vertex[6]             = x4 / (float)width;
   vertex[7]             = y4 / (float)height;

   coords.vertices      = 4;
   coords.vertex        = &vertex[0];
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   if (menu_disp && menu_disp->blend_begin)
      menu_disp->blend_begin(video_info);

   draw.x            = 0;
   draw.y            = 0;
   draw.width        = width;
   draw.height       = height;
   draw.coords       = &coords;
   draw.matrix_data  = NULL;
   draw.texture      = menu_display_white_texture;
   draw.prim_type    = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id  = 0;
   draw.scale_factor = 1.0f;
   draw.rotation     = 0.0f;

   menu_display_draw(&draw, video_info);

   if (menu_disp && menu_disp->blend_end)
      menu_disp->blend_end(video_info);
}

void menu_display_draw_texture(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color, uintptr_t texture)
{
   menu_display_ctx_draw_t draw;
   menu_display_ctx_rotate_draw_t rotate_draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;
   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0.0;
   rotate_draw.scale_x      = 1.0;
   rotate_draw.scale_y      = 1.0;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;
   coords.vertices          = 4;
   coords.vertex            = NULL;
   coords.tex_coord         = NULL;
   coords.lut_tex_coord     = NULL;
   draw.width               = w;
   draw.height              = h;
   draw.coords              = &coords;
   draw.matrix_data         = &mymat;
   draw.prim_type           = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id         = 0;
   coords.color             = (const float*)color;

   menu_display_rotate_z(&rotate_draw, video_info);

   draw.texture             = texture;
   draw.x                   = x;
   draw.y                   = height - y;
   menu_display_draw(&draw, video_info);
}

/* Draw the texture split into 9 sections, without scaling the corners.
 * The middle sections will only scale in the X axis, and the side
 * sections will only scale in the Y axis. */
void menu_display_draw_texture_slice(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned new_w, unsigned new_h,
      unsigned width, unsigned height,
      float *color, unsigned offset, float scale_factor, uintptr_t texture)
{
   menu_display_ctx_draw_t draw;
   menu_display_ctx_rotate_draw_t rotate_draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;
   unsigned i;
   float V_BL[2], V_BR[2], V_TL[2], V_TR[2], T_BL[2], T_BR[2], T_TL[2], T_TR[2];

   /* need space for the coordinates of two triangles in a strip, so 8 vertices */
   float *tex_coord  = (float*)malloc(8 * sizeof(float));
   float *vert_coord = (float*)malloc(8 * sizeof(float));
   float *colors     = (float*)malloc(16 * sizeof(float));

   /* normalized width/height of the amount to offset from the corners,
    * for both the vertex and texture coordinates */
   float vert_woff   = (offset * scale_factor) / (float)width;
   float vert_hoff   = (offset * scale_factor) / (float)height;
   float tex_woff    = offset / (float)w;
   float tex_hoff    = offset / (float)h;

   /* the width/height of the middle sections of both the scaled and original image */
   float vert_scaled_mid_width  = (new_w - (offset * scale_factor * 2)) / (float)width;
   float vert_scaled_mid_height = (new_h - (offset * scale_factor * 2)) / (float)height;
   float tex_mid_width          = (w - (offset * 2)) / (float)w;
   float tex_mid_height         = (h - (offset * 2)) / (float)h;

   /* normalized coordinates for the start position of the image */
   float norm_x                 = x / (float)width;
   float norm_y                 = (height - y) / (float)height;

   /* the four vertices of the top-left corner of the image,
    * used as a starting point for all the other sections */
   V_BL[0] = norm_x;
   V_BL[1] = norm_y;
   V_BR[0] = norm_x + vert_woff;
   V_BR[1] = norm_y;
   V_TL[0] = norm_x;
   V_TL[1] = norm_y + vert_hoff;
   V_TR[0] = norm_x + vert_woff;
   V_TR[1] = norm_y + vert_hoff;
   T_BL[0] = 0.0f;
   T_BL[1] = tex_hoff;
   T_BR[0] = tex_woff;
   T_BR[1] = tex_hoff;
   T_TL[0] = 0.0f;
   T_TL[1] = 0.0f;
   T_TR[0] = tex_woff;
   T_TR[1] = 0.0f;

   for (i = 0; i < (16 * sizeof(float)) / sizeof(colors[0]); i++)
      colors[i] = 1.0f;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0.0;
   rotate_draw.scale_x      = 1.0;
   rotate_draw.scale_y      = 1.0;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;
   coords.vertices          = 4;
   coords.vertex            = vert_coord;
   coords.tex_coord         = tex_coord;
   coords.lut_tex_coord     = NULL;
   draw.width               = width;
   draw.height              = height;
   draw.coords              = &coords;
   draw.matrix_data         = &mymat;
   draw.prim_type           = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id         = 0;
   coords.color             = (const float*)(color == NULL ? colors : color);

   menu_display_rotate_z(&rotate_draw, video_info);

   draw.texture             = texture;
   draw.x                   = 0;
   draw.y                   = 0;

   /* vertex coords are specfied bottom-up in this order: BL BR TL TR */
   /* texture coords are specfied top-down in this order: BL BR TL TR */

   /* If someone wants to change this to not draw several times, the
    * coordinates will need to be modified because of the triangle strip usage. */

   /* top-left corner */
   vert_coord[0] = V_BL[0];
   vert_coord[1] = V_BL[1];
   vert_coord[2] = V_BR[0];
   vert_coord[3] = V_BR[1];
   vert_coord[4] = V_TL[0];
   vert_coord[5] = V_TL[1];
   vert_coord[6] = V_TR[0];
   vert_coord[7] = V_TR[1];

   tex_coord[0] = T_BL[0];
   tex_coord[1] = T_BL[1];
   tex_coord[2] = T_BR[0];
   tex_coord[3] = T_BR[1];
   tex_coord[4] = T_TL[0];
   tex_coord[5] = T_TL[1];
   tex_coord[6] = T_TR[0];
   tex_coord[7] = T_TR[1];

   menu_display_draw(&draw, video_info);

   /* top-middle section */
   vert_coord[0] = V_BL[0] + vert_woff;
   vert_coord[1] = V_BL[1];
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width;
   vert_coord[3] = V_BR[1];
   vert_coord[4] = V_TL[0] + vert_woff;
   vert_coord[5] = V_TL[1];
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width;
   vert_coord[7] = V_TR[1];

   tex_coord[0] = T_BL[0] + tex_woff;
   tex_coord[1] = T_BL[1];
   tex_coord[2] = T_BR[0] + tex_mid_width;
   tex_coord[3] = T_BR[1];
   tex_coord[4] = T_TL[0] + tex_woff;
   tex_coord[5] = T_TL[1];
   tex_coord[6] = T_TR[0] + tex_mid_width;
   tex_coord[7] = T_TR[1];

   menu_display_draw(&draw, video_info);

   /* top-right corner */
   vert_coord[0] = V_BL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[1] = V_BL[1];
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width + vert_woff;
   vert_coord[3] = V_BR[1];
   vert_coord[4] = V_TL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[5] = V_TL[1];
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width + vert_woff;
   vert_coord[7] = V_TR[1];

   tex_coord[0] = T_BL[0] + tex_woff + tex_mid_width;
   tex_coord[1] = T_BL[1];
   tex_coord[2] = T_BR[0] + tex_mid_width + tex_woff;
   tex_coord[3] = T_BR[1];
   tex_coord[4] = T_TL[0] + tex_woff + tex_mid_width;
   tex_coord[5] = T_TL[1];
   tex_coord[6] = T_TR[0] + tex_mid_width + tex_woff;
   tex_coord[7] = T_TR[1];

   menu_display_draw(&draw, video_info);

   /* middle-left section */
   vert_coord[0] = V_BL[0];
   vert_coord[1] = V_BL[1] - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0];
   vert_coord[3] = V_BR[1] - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0];
   vert_coord[5] = V_TL[1] - vert_hoff;
   vert_coord[6] = V_TR[0];
   vert_coord[7] = V_TR[1] - vert_hoff;

   tex_coord[0] = T_BL[0];
   tex_coord[1] = T_BL[1] + tex_mid_height;
   tex_coord[2] = T_BR[0];
   tex_coord[3] = T_BR[1] + tex_mid_height;
   tex_coord[4] = T_TL[0];
   tex_coord[5] = T_TL[1] + tex_hoff;
   tex_coord[6] = T_TR[0];
   tex_coord[7] = T_TR[1] + tex_hoff;

   menu_display_draw(&draw, video_info);

   /* center section */
   vert_coord[0] = V_BL[0] + vert_woff;
   vert_coord[1] = V_BL[1] - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width;
   vert_coord[3] = V_BR[1] - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0] + vert_woff;
   vert_coord[5] = V_TL[1] - vert_hoff;
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width;
   vert_coord[7] = V_TR[1] - vert_hoff;

   tex_coord[0] = T_BL[0] + tex_woff;
   tex_coord[1] = T_BL[1] + tex_mid_height;
   tex_coord[2] = T_BR[0] + tex_mid_width;
   tex_coord[3] = T_BR[1] + tex_mid_height;
   tex_coord[4] = T_TL[0] + tex_woff;
   tex_coord[5] = T_TL[1] + tex_hoff;
   tex_coord[6] = T_TR[0] + tex_mid_width;
   tex_coord[7] = T_TR[1] + tex_hoff;

   menu_display_draw(&draw, video_info);

   /* middle-right section */
   vert_coord[0] = V_BL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[1] = V_BL[1] - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[3] = V_BR[1] - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[5] = V_TL[1] - vert_hoff;
   vert_coord[6] = V_TR[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[7] = V_TR[1] - vert_hoff;

   tex_coord[0] = T_BL[0] + tex_woff + tex_mid_width;
   tex_coord[1] = T_BL[1] + tex_mid_height;
   tex_coord[2] = T_BR[0] + tex_woff + tex_mid_width;
   tex_coord[3] = T_BR[1] + tex_mid_height;
   tex_coord[4] = T_TL[0] + tex_woff + tex_mid_width;
   tex_coord[5] = T_TL[1] + tex_hoff;
   tex_coord[6] = T_TR[0] + tex_woff + tex_mid_width;
   tex_coord[7] = T_TR[1] + tex_hoff;

   menu_display_draw(&draw, video_info);

   /* bottom-left corner */
   vert_coord[0] = V_BL[0];
   vert_coord[1] = V_BL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0];
   vert_coord[3] = V_BR[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0];
   vert_coord[5] = V_TL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[6] = V_TR[0];
   vert_coord[7] = V_TR[1] - vert_hoff - vert_scaled_mid_height;

   tex_coord[0] = T_BL[0];
   tex_coord[1] = T_BL[1] + tex_hoff + tex_mid_height;
   tex_coord[2] = T_BR[0];
   tex_coord[3] = T_BR[1] + tex_hoff + tex_mid_height;
   tex_coord[4] = T_TL[0];
   tex_coord[5] = T_TL[1] + tex_hoff + tex_mid_height;
   tex_coord[6] = T_TR[0];
   tex_coord[7] = T_TR[1] + tex_hoff + tex_mid_height;

   menu_display_draw(&draw, video_info);

   /* bottom-middle section */
   vert_coord[0] = V_BL[0] + vert_woff;
   vert_coord[1] = V_BL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width;
   vert_coord[3] = V_BR[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0] + vert_woff;
   vert_coord[5] = V_TL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width;
   vert_coord[7] = V_TR[1] - vert_hoff - vert_scaled_mid_height;

   tex_coord[0] = T_BL[0] + tex_woff;
   tex_coord[1] = T_BL[1] + tex_hoff + tex_mid_height;
   tex_coord[2] = T_BR[0] + tex_mid_width;
   tex_coord[3] = T_BR[1] + tex_hoff + tex_mid_height;
   tex_coord[4] = T_TL[0] + tex_woff;
   tex_coord[5] = T_TL[1] + tex_hoff + tex_mid_height;
   tex_coord[6] = T_TR[0] + tex_mid_width;
   tex_coord[7] = T_TR[1] + tex_hoff + tex_mid_height;

   menu_display_draw(&draw, video_info);

   /* bottom-right corner */
   vert_coord[0] = V_BL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[1] = V_BL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width + vert_woff;
   vert_coord[3] = V_BR[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[5] = V_TL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width + vert_woff;
   vert_coord[7] = V_TR[1] - vert_hoff - vert_scaled_mid_height;

   tex_coord[0] = T_BL[0] + tex_woff + tex_mid_width;
   tex_coord[1] = T_BL[1] + tex_hoff + tex_mid_height;
   tex_coord[2] = T_BR[0] + tex_woff + tex_mid_width;
   tex_coord[3] = T_BR[1] + tex_hoff + tex_mid_height;
   tex_coord[4] = T_TL[0] + tex_woff + tex_mid_width;
   tex_coord[5] = T_TL[1] + tex_hoff + tex_mid_height;
   tex_coord[6] = T_TR[0] + tex_woff + tex_mid_width;
   tex_coord[7] = T_TR[1] + tex_hoff + tex_mid_height;

   menu_display_draw(&draw, video_info);

   free(colors);
   free(vert_coord);
   free(tex_coord);
}

void menu_display_rotate_z(menu_display_ctx_rotate_draw_t *draw,
      video_frame_info_t *video_info)
{
   math_matrix_4x4 matrix_rotated, matrix_scaled;
   math_matrix_4x4 *b = NULL;

   if (
         !draw                       ||
         !menu_disp                  ||
         !menu_disp->get_default_mvp ||
         menu_disp->handles_transform
      )
      return;

   b = (math_matrix_4x4*)menu_disp->get_default_mvp(video_info);

   if (!b)
      return;

   matrix_4x4_rotate_z(matrix_rotated, draw->rotation);
   matrix_4x4_multiply(*draw->matrix, matrix_rotated, *b);

   if (!draw->scale_enable)
      return;

   matrix_4x4_scale(matrix_scaled,
         draw->scale_x, draw->scale_y, draw->scale_z);
   matrix_4x4_multiply(*draw->matrix, matrix_scaled, *draw->matrix);
}

static bool menu_driver_load_image(menu_ctx_load_image_t *load_image_info)
{
   if (menu_driver_ctx && menu_driver_ctx->load_image)
      return menu_driver_ctx->load_image(menu_userdata,
            load_image_info->data, load_image_info->type);
   return false;
}

void menu_display_handle_thumbnail_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   menu_ctx_load_image_t load_image_info;
   struct texture_image *img = (struct texture_image*)task_data;

   load_image_info.data = img;
   load_image_info.type = MENU_IMAGE_THUMBNAIL;

   menu_driver_load_image(&load_image_info);

   image_texture_free(img);
   free(img);
   free(user_data);
}

void menu_display_handle_left_thumbnail_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   menu_ctx_load_image_t load_image_info;
   struct texture_image *img = (struct texture_image*)task_data;

   load_image_info.data = img;
   load_image_info.type = MENU_IMAGE_LEFT_THUMBNAIL;

   menu_driver_load_image(&load_image_info);

   image_texture_free(img);
   free(img);
   free(user_data);
}

void menu_display_handle_savestate_thumbnail_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   menu_ctx_load_image_t load_image_info;
   struct texture_image *img = (struct texture_image*)task_data;

   load_image_info.data = img;
   load_image_info.type = MENU_IMAGE_SAVESTATE_THUMBNAIL;

   menu_driver_load_image(&load_image_info);

   image_texture_free(img);
   free(img);
   free(user_data);
}

/* Function that gets called when we want to load in a
 * new menu wallpaper.
 */
void menu_display_handle_wallpaper_upload(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   menu_ctx_load_image_t load_image_info;
   struct texture_image *img = (struct texture_image*)task_data;

   load_image_info.data = img;
   load_image_info.type = MENU_IMAGE_WALLPAPER;

   menu_driver_load_image(&load_image_info);
   image_texture_free(img);
   free(img);
   free(user_data);
}

void menu_display_allocate_white_texture(void)
{
   struct texture_image ti;
   static const uint8_t white_data[] = { 0xff, 0xff, 0xff, 0xff };

   ti.width  = 1;
   ti.height = 1;
   ti.pixels = (uint32_t*)&white_data;

   if (menu_display_white_texture)
      video_driver_texture_unload(&menu_display_white_texture);

   video_driver_texture_load(&ti,
         TEXTURE_FILTER_NEAREST, &menu_display_white_texture);
}

/*
 * Draw a hardware cursor on top of the screen for the mouse.
 */
void menu_display_draw_cursor(
      video_frame_info_t *video_info,
      float *color, float cursor_size, uintptr_t texture,
      float x, float y, unsigned width, unsigned height)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   settings_t *settings = config_get_ptr();
   bool cursor_visible  = settings->bools.video_fullscreen ||
       !menu_display_has_windowed;

   if (!settings->bools.menu_mouse_enable || !cursor_visible)
      return;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   if (menu_disp && menu_disp->blend_begin)
      menu_disp->blend_begin(video_info);

   draw.x               = x - (cursor_size / 2);
   draw.y               = (int)height - y - (cursor_size / 2);
   draw.width           = cursor_size;
   draw.height          = cursor_size;
   draw.coords          = &coords;
   draw.matrix_data     = NULL;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   menu_display_draw(&draw, video_info);

   if (menu_disp && menu_disp->blend_end)
      menu_disp->blend_end(video_info);
}

static INLINE float menu_display_scalef(float val,
      float oldmin, float oldmax, float newmin, float newmax)
{
   return (((val - oldmin) * (newmax - newmin)) / (oldmax - oldmin)) + newmin;
}

static INLINE float menu_display_randf(float min, float max)
{
   return (rand() * ((max - min) / RAND_MAX)) + min;
}

void menu_display_push_quad(
      unsigned width, unsigned height,
      const float *colors, int x1, int y1,
      int x2, int y2)
{
   float vertex[8];
   video_coords_t coords;
   const float *coord_draw_ptr   = NULL;
   video_coord_array_t       *ca = &menu_disp_ca;

   vertex[0]             = x1 / (float)width;
   vertex[1]             = y1 / (float)height;
   vertex[2]             = x2 / (float)width;
   vertex[3]             = y1 / (float)height;
   vertex[4]             = x1 / (float)width;
   vertex[5]             = y2 / (float)height;
   vertex[6]             = x2 / (float)width;
   vertex[7]             = y2 / (float)height;

   if (menu_disp && menu_disp->get_default_tex_coords)
      coord_draw_ptr     = menu_disp->get_default_tex_coords();

   coords.color          = colors;
   coords.vertex         = vertex;
   coords.tex_coord      = coord_draw_ptr;
   coords.lut_tex_coord  = coord_draw_ptr;
   coords.vertices       = 3;

   video_coord_array_append(ca, &coords, 3);

   coords.color         += 4;
   coords.vertex        += 2;
   coords.tex_coord     += 2;
   coords.lut_tex_coord += 2;

   video_coord_array_append(ca, &coords, 3);
}

void menu_display_snow(int width, int height)
{
   struct display_particle
   {
      float x, y;
      float xspeed, yspeed;
      float alpha;
      bool alive;
   };
   static struct display_particle particles[PARTICLES_COUNT] = {{0}};
   static int timeout      = 0;
   unsigned i, max_gen     = 2;

   for (i = 0; i < PARTICLES_COUNT; ++i)
   {
      struct display_particle *p = (struct display_particle*)&particles[i];

      if (p->alive)
      {
         menu_input_pointer_t pointer;
         menu_input_get_pointer_state(&pointer);

         p->y            += p->yspeed;
         p->x            += menu_display_scalef(
               pointer.x, 0, width, -0.3, 0.3);
         p->x            += p->xspeed;

         p->alive         = p->y >= 0 && p->y < height
            && p->x >= 0 && p->x < width;
      }
      else if (max_gen > 0 && timeout <= 0)
      {
         p->xspeed = menu_display_randf(-0.2, 0.2);
         p->yspeed = menu_display_randf(1, 2);
         p->y      = 0;
         p->x      = rand() % width;
         p->alpha  = (float)rand() / (float)RAND_MAX;
         p->alive  = true;

         max_gen--;
      }
   }

   if (max_gen == 0)
      timeout = 3;
   else
      timeout--;

   for (i = 0; i < PARTICLES_COUNT; ++i)
   {
      unsigned j;
      float alpha, colors[16];
      struct display_particle *p = &particles[i];

      if (!p->alive)
         continue;

      alpha = menu_display_randf(0, 100) > 90 ?
         p->alpha/2 : p->alpha;

      for (j = 0; j < 16; j++)
      {
         colors[j] = 1;
         if (j == 3 || j == 7 || j == 11 || j == 15)
            colors[j] = alpha;
      }

      menu_display_push_quad(width, height,
            colors, p->x-2, p->y-2, p->x+2, p->y+2);

      j++;
   }
}

void menu_display_draw_keyboard(
      uintptr_t hover_texture,
      const font_data_t *font,
      video_frame_info_t *video_info,
      char *grid[], unsigned id,
      unsigned text_color)
{
   unsigned i;
   int ptr_width, ptr_height;
   unsigned width    = video_info->width;
   unsigned height   = video_info->height;

   float white[16]=  {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
   };

   menu_display_draw_quad(
         video_info,
         0, height/2.0, width, height/2.0,
         width, height,
         &osk_dark[0]);

   ptr_width  = width  / 11;
   ptr_height = height / 10;

   if (ptr_width >= ptr_height)
      ptr_width = ptr_height;

   for (i = 0; i < 44; i++)
   {
      int line_y     = (i / 11) * height / 10.0;
      unsigned color = 0xffffffff;

      if (i == id)
      {
         menu_display_blend_begin(video_info);

         menu_display_draw_texture(
               video_info,
               width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width,
               height/2.0 + ptr_height*1.5 + line_y,
               ptr_width, ptr_height,
               width, height,
               &white[0],
               hover_texture);

         menu_display_blend_end(video_info);

         color = text_color;
      }

      menu_display_draw_text(font, grid[i],
            width/2.0 - (11*ptr_width)/2.0 + (i % 11)
            * ptr_width + ptr_width/2.0,
            height/2.0 + ptr_height + line_y + font->size / 3,
            width, height, color, TEXT_ALIGN_CENTER, 1.0f,
            false, 0, false);
   }
}

/* Draw text on top of the screen.
 */
void menu_display_draw_text(
      const font_data_t *font, const char *text,
      float x, float y, int width, int height,
      uint32_t color, enum text_alignment text_align,
      float scale, bool shadows_enable, float shadow_offset,
      bool draw_outside)
{
   struct font_params params;

   if ((color & 0x000000FF) == 0)
      return;

   /* Don't draw outside of the screen */
   if (!draw_outside &&
           ((x < -64 || x > width  + 64)
         || (y < -64 || y > height + 64))
      )
      return;

   params.x           = x / width;
   params.y           = 1.0f - y / height;
   params.scale       = scale;
   params.drop_mod    = 0.0f;
   params.drop_x      = 0.0f;
   params.drop_y      = 0.0f;
   params.color       = color;
   params.full_screen = true;
   params.text_align  = text_align;

   if (shadows_enable)
   {
      params.drop_x      = shadow_offset;
      params.drop_y      = -shadow_offset;
      params.drop_alpha  = 0.35f;
   }

   video_driver_set_osd_msg(text, &params, (void*)font);
}

bool menu_display_reset_textures_list(
      const char *texture_path, const char *iconpath,
      uintptr_t *item, enum texture_filter_type filter_type,
      unsigned *width, unsigned *height)
{
   struct texture_image ti;
   char texpath[PATH_MAX_LENGTH] = {0};

   ti.width                      = 0;
   ti.height                     = 0;
   ti.pixels                     = NULL;
   ti.supports_rgba              = video_driver_supports_rgba();

   if (string_is_empty(texture_path))
      return false;

   fill_pathname_join(texpath, iconpath, texture_path, sizeof(texpath));

   if (!path_is_valid(texpath))
      return false;

   if (!image_texture_load(&ti, texpath))
      return false;

   if (width)
      *width = ti.width;

   if (height)
      *height = ti.height;

   video_driver_texture_load(&ti,
         filter_type, item);
   image_texture_free(&ti);

   return true;
}


/**
 * menu_driver_find_handle:
 * @idx              : index of driver to get handle to.
 *
 * Returns: handle to menu driver at index. Can be NULL
 * if nothing found.
 **/
const void *menu_driver_find_handle(int idx)
{
   const void *drv = menu_ctx_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * menu_driver_find_ident:
 * @idx              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of menu driver at index.
 * Can be NULL if nothing found.
 **/
const char *menu_driver_find_ident(int idx)
{
   const menu_ctx_driver_t *drv = menu_ctx_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_menu_driver_options:
 *
 * Get an enumerated list of all menu driver names,
 * separated by '|'.
 *
 * Returns: string listing of all menu driver names,
 * separated by '|'.
 **/
const char *config_get_menu_driver_options(void)
{
   return char_list_new_special(STRING_LIST_MENU_DRIVERS, NULL);
}

#ifdef HAVE_COMPRESSION
/* This function gets called at first startup on Android/iOS
 * when we need to extract the APK contents/zip file. This
 * file contains assets which then get extracted to the
 * user's asset directories. */
static void bundle_decompressed(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   settings_t      *settings   = config_get_ptr();
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   if (dec && !err)
      command_event(CMD_EVENT_REINIT, NULL);

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      /* delete bundle? */
      free(dec->source_file);
      free(dec);
   }

   settings->uints.bundle_assets_extract_last_version =
      settings->uints.bundle_assets_extract_version_current;

   configuration_set_bool(settings, settings->bools.bundle_finished, true);

   command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
}
#endif

/**
 * menu_init:
 * @data                     : Menu context handle.
 *
 * Create and initialize menu handle.
 *
 * Returns: menu handle on success, otherwise NULL.
 **/
static bool menu_init(menu_handle_t *menu_data)
{
   settings_t *settings        = config_get_ptr();

   /* Ensure that menu pointer input is correctly
    * initialised */
   menu_input_reset();

   if (!menu_entries_init())
      return false;

   if (settings->bools.menu_show_start_screen)
   {
      /* We don't want the welcome dialog screen to show up
       * again after the first startup, so we save to config
       * file immediately. */

      menu_dialog_push_pending(true, MENU_DIALOG_WELCOME);

      configuration_set_bool(settings,
            settings->bools.menu_show_start_screen, false);
#if !(defined(PS2) && defined(DEBUG)) /* TODO: PS2 IMPROVEMENT */
      if (settings->bools.config_save_on_exit)
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
#endif
   }

   if (      settings->bools.bundle_assets_extract_enable
         && !string_is_empty(settings->arrays.bundle_assets_src)
         && !string_is_empty(settings->arrays.bundle_assets_dst)
#ifdef IOS
         && menu_dialog_is_push_pending()
#else
         && (settings->uints.bundle_assets_extract_version_current
            != settings->uints.bundle_assets_extract_last_version)
#endif
      )
   {
      menu_dialog_push_pending(true, MENU_DIALOG_HELP_EXTRACT);
#ifdef HAVE_COMPRESSION
      task_push_decompress(settings->arrays.bundle_assets_src,
            settings->arrays.bundle_assets_dst,
            NULL, settings->arrays.bundle_assets_dst_subdir,
            NULL, bundle_decompressed, NULL, NULL);
#endif
   }

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   menu_shader_manager_init();
#endif

   menu_disp_ca.allocated    =  0;

   menu_display_has_windowed = video_driver_has_windowed();

   return true;
}

const char *menu_driver_ident(void)
{
   if (!menu_driver_is_alive())
      return NULL;
   if (!menu_driver_ctx || !menu_driver_ctx->ident)
      return NULL;
  return menu_driver_ctx->ident;
}

void menu_driver_frame(video_frame_info_t *video_info)
{
   if (video_info->menu_is_alive && menu_driver_ctx->frame)
      menu_driver_ctx->frame(menu_userdata, video_info);
}

bool menu_driver_get_load_content_animation_data(menu_texture_item *icon, char **playlist_name)
{
   return menu_driver_ctx && menu_driver_ctx->get_load_content_animation_data
      && menu_driver_ctx->get_load_content_animation_data(menu_userdata, icon, playlist_name);
}

/* Iterate the menu driver for one frame. */
bool menu_driver_iterate(menu_ctx_iterate_t *iterate)
{
   /* Get current time */
   menu_driver_current_time_us = cpu_features_get_time_usec();

   if (menu_driver_pending_quick_menu)
   {
      /* If the user had requested that the Quick Menu
       * be spawned during the previous frame, do this now
       * and exit the function to go to the next frame.
       */

      menu_driver_pending_quick_menu = false;
      menu_entries_flush_stack(NULL, MENU_SETTINGS);
      menu_display_set_msg_force(true);

      generic_action_ok_displaylist_push("", NULL,
            "", 0, 0, 0, ACTION_OK_DL_CONTENT_SETTINGS);

      menu_driver_selection_ptr = 0;

      return true;
   }

   if (
         menu_driver_ctx          &&
         menu_driver_ctx->iterate &&
         menu_driver_ctx->iterate(menu_driver_data,
            menu_userdata, iterate->action) != -1)
      return true;

   return false;
}

bool menu_driver_list_cache(menu_ctx_list_t *list)
{
   if (!list || !menu_driver_ctx || !menu_driver_ctx->list_cache)
      return false;

   menu_driver_ctx->list_cache(menu_userdata,
         list->type, list->action);
   return true;
}

/* Clear all the menu lists. */
bool menu_driver_list_clear(file_list_t *list)
{
   if (!list)
      return false;
   if (menu_driver_ctx->list_clear)
      menu_driver_ctx->list_clear(list);
   return true;
}

bool menu_driver_list_insert(menu_ctx_list_t *list)
{
   if (!list || !menu_driver_ctx || !menu_driver_ctx->list_insert)
      return false;
   menu_driver_ctx->list_insert(menu_userdata,
         list->list, list->path, list->fullpath,
         list->label, list->idx, list->entry_type);

   return true;
}

bool menu_driver_list_set_selection(file_list_t *list)
{
   if (!list)
      return false;

   if (!menu_driver_ctx || !menu_driver_ctx->list_set_selection)
      return false;

   menu_driver_ctx->list_set_selection(menu_userdata, list);
   return true;
}

static bool menu_driver_init_internal(bool video_is_threaded)
{
   if (menu_driver_ctx->init)
   {
      menu_driver_data               = (menu_handle_t*)
         menu_driver_ctx->init(&menu_userdata, video_is_threaded);
      menu_driver_data->userdata     = menu_userdata;
      menu_driver_data->driver_ctx   = menu_driver_ctx;
   }

   if (!menu_driver_data || !menu_init(menu_driver_data))
      return false;

   {
      settings_t *settings           = config_get_ptr();
      strlcpy(settings->arrays.menu_driver, menu_driver_ctx->ident,
            sizeof(settings->arrays.menu_driver));
   }

   if (menu_driver_ctx->lists_init)
      if (!menu_driver_ctx->lists_init(menu_driver_data))
         return false;

   return true;
}

bool menu_driver_init(bool video_is_threaded)
{
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
   command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);

   if (  menu_driver_data || 
         menu_driver_init_internal(video_is_threaded))
   {
      if (menu_driver_ctx && menu_driver_ctx->context_reset)
      {
         menu_driver_ctx->context_reset(menu_userdata, video_is_threaded);
         return true;
      }
   }

   return false;
}

void menu_driver_navigation_set(bool scroll)
{
   if (menu_driver_ctx->navigation_set)
      menu_driver_ctx->navigation_set(menu_userdata, scroll);
}

void menu_driver_populate_entries(menu_displaylist_info_t *info)
{
   if (menu_driver_ctx && menu_driver_ctx->populate_entries)
      menu_driver_ctx->populate_entries(
            menu_userdata, info->path,
            info->label, info->type);
}

bool menu_driver_push_list(menu_ctx_displaylist_t *disp_list)
{
   if (menu_driver_ctx->list_push)
      if (menu_driver_ctx->list_push(menu_driver_data,
               menu_userdata, disp_list->info, disp_list->type) == 0)
         return true;
   return false;
}

void menu_driver_set_thumbnail_system(char *s, size_t len)
{
   if (menu_driver_ctx && menu_driver_ctx->set_thumbnail_system)
      menu_driver_ctx->set_thumbnail_system(menu_userdata, s, len);
}

void menu_driver_get_thumbnail_system(char *s, size_t len)
{
   if (menu_driver_ctx && menu_driver_ctx->get_thumbnail_system)
      menu_driver_ctx->get_thumbnail_system(menu_userdata, s, len);
}

void menu_driver_set_thumbnail_content(char *s, size_t len)
{
   if (menu_driver_ctx && menu_driver_ctx->set_thumbnail_content)
      menu_driver_ctx->set_thumbnail_content(menu_userdata, s);
}

/* Teardown function for the menu driver. */
void menu_driver_destroy(void)
{
   menu_driver_pending_quick_menu = false;
   menu_driver_prevent_populate   = false;
   menu_driver_data_own           = false;
   menu_driver_ctx                = NULL;
   menu_userdata                  = NULL;
}

bool menu_driver_list_get_entry(menu_ctx_list_t *list)
{
   if (!menu_driver_ctx || !menu_driver_ctx->list_get_entry)
   {
      list->entry = NULL;
      return false;
   }
   list->entry = menu_driver_ctx->list_get_entry(menu_userdata,
         list->type, (unsigned int)list->idx);
   return true;
}

bool menu_driver_list_get_selection(menu_ctx_list_t *list)
{
   if (!menu_driver_ctx || !menu_driver_ctx->list_get_selection)
   {
      list->selection = 0;
      return false;
   }
   list->selection = menu_driver_ctx->list_get_selection(menu_userdata);

   return true;
}

bool menu_driver_list_get_size(menu_ctx_list_t *list)
{
   if (!menu_driver_ctx || !menu_driver_ctx->list_get_size)
   {
      list->size = 0;
      return false;
   }
   list->size = menu_driver_ctx->list_get_size(menu_userdata, list->type);
   return true;
}

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data)
{
   switch (state)
   {
      case RARCH_MENU_CTL_SET_PENDING_QUICK_MENU:
         menu_entries_flush_stack(NULL, MENU_SETTINGS);
         menu_driver_pending_quick_menu = true;
         break;
      case RARCH_MENU_CTL_FIND_DRIVER:
         {
            int i;
            driver_ctx_info_t drv;
            settings_t *settings = config_get_ptr();

            drv.label = "menu_driver";
            drv.s     = settings->arrays.menu_driver;

            driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

            i = (int)drv.len;

            if (i >= 0)
               menu_driver_ctx = (const menu_ctx_driver_t*)
                  menu_driver_find_handle(i);
            else
            {
               if (verbosity_is_enabled())
               {
                  unsigned d;
                  RARCH_WARN("Couldn't find any menu driver named \"%s\"\n",
                        settings->arrays.menu_driver);
                  RARCH_LOG_OUTPUT("Available menu drivers are:\n");
                  for (d = 0; menu_driver_find_handle(d); d++)
                     RARCH_LOG_OUTPUT("\t%s\n", menu_driver_find_ident(d));
                  RARCH_WARN("Going to default to first menu driver...\n");
               }

               menu_driver_ctx = (const menu_ctx_driver_t*)
                  menu_driver_find_handle(0);

               if (!menu_driver_ctx)
               {
                  retroarch_fail(1, "find_menu_driver()");
                  return false;
               }
            }
         }
         break;
      case RARCH_MENU_CTL_SET_PREVENT_POPULATE:
         menu_driver_prevent_populate = true;
         break;
      case RARCH_MENU_CTL_UNSET_PREVENT_POPULATE:
         menu_driver_prevent_populate = false;
         break;
      case RARCH_MENU_CTL_IS_PREVENT_POPULATE:
         return menu_driver_prevent_populate;
      case RARCH_MENU_CTL_SET_OWN_DRIVER:
         menu_driver_data_own = true;
         break;
      case RARCH_MENU_CTL_UNSET_OWN_DRIVER:
         menu_driver_data_own = false;
         break;
      case RARCH_MENU_CTL_OWNS_DRIVER:
         return menu_driver_data_own;
      case RARCH_MENU_CTL_DEINIT:
         if (menu_driver_ctx && menu_driver_ctx->context_destroy)
            menu_driver_ctx->context_destroy(menu_userdata);

         if (menu_driver_data_own)
            return true;

         playlist_free_cached();
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         menu_shader_manager_free();
#endif

         if (menu_driver_data)
         {
            unsigned i;

            scroll_acceleration       = 0;
            menu_driver_selection_ptr = 0;
            scroll_index_size         = 0;

            for (i = 0; i < SCROLL_INDEX_SIZE; i++)
               scroll_index_list[i] = 0;

            menu_input_reset();

            if (menu_driver_ctx && menu_driver_ctx->free)
               menu_driver_ctx->free(menu_userdata);

            if (menu_userdata)
               free(menu_userdata);
            menu_userdata = NULL;

#ifndef HAVE_DYNAMIC
            if (frontend_driver_has_fork())
#endif
            {
               rarch_system_info_t *system = runloop_get_system_info();
               libretro_free_system_info(&system->info);
               memset(&system->info, 0, sizeof(struct retro_system_info));
            }

            video_coord_array_free(&menu_disp_ca);
            menu_display_msg_force       = false;
            menu_display_header_height   = 0;
            menu_disp                    = NULL;
            menu_display_has_windowed    = false;

            menu_animation_ctl(MENU_ANIMATION_CTL_DEINIT, NULL);

            menu_display_framebuf_width  = 0;
            menu_display_framebuf_height = 0;
            menu_display_framebuf_pitch  = 0;
            menu_entries_settings_deinit();
            menu_entries_list_deinit();

            if (menu_driver_data->core_buf)
               free(menu_driver_data->core_buf);
            menu_driver_data->core_buf       = NULL;

            menu_entries_need_refresh        = false;
            menu_entries_nonblocking_refresh = false;
            menu_entries_begin               = 0;

            command_event(CMD_EVENT_HISTORY_DEINIT, NULL);
            rarch_favorites_deinit();

            menu_dialog_reset();

            free(menu_driver_data);
         }
         menu_driver_data = NULL;
         break;
      case RARCH_MENU_CTL_LIST_FREE:
         {
            menu_ctx_list_t *list = (menu_ctx_list_t*)data;

            if (menu_driver_ctx)
            {
               if (menu_driver_ctx->list_free)
                  menu_driver_ctx->list_free(list->list, list->idx, list->list_size);
            }

            if (list->list)
            {
               file_list_free_userdata  (list->list, list->idx);
               file_list_free_actiondata(list->list, list->idx);
            }
         }
         break;
      case RARCH_MENU_CTL_ENVIRONMENT:
         {
            menu_ctx_environment_t *menu_environ =
               (menu_ctx_environment_t*)data;

            if (menu_driver_ctx->environ_cb)
            {
               if (menu_driver_ctx->environ_cb(menu_environ->type,
                        menu_environ->data, menu_userdata) == 0)
                  return true;
            }
         }
         return false;
      case RARCH_MENU_CTL_POINTER_DOWN:
         {
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_driver_ctx || !menu_driver_ctx->pointer_down)
            {
               point->retcode = 0;
               return false;
            }
            point->retcode = menu_driver_ctx->pointer_down(menu_userdata,
                  point->x, point->y, point->ptr,
                  point->cbs, point->entry, point->action);
         }
         break;
      case RARCH_MENU_CTL_POINTER_UP:
         {
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_driver_ctx || !menu_driver_ctx->pointer_up)
            {
               point->retcode = 0;
               return false;
            }
            point->retcode = menu_driver_ctx->pointer_up(menu_userdata,
                  point->x, point->y, point->ptr,
                  point->cbs, point->entry, point->action);
         }
         break;
      case RARCH_MENU_CTL_OSK_PTR_AT_POS:
         {
            unsigned width            = 0;
            unsigned height           = 0;
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_driver_ctx || !menu_driver_ctx->osk_ptr_at_pos)
            {
               point->retcode = 0;
               return false;
            }
            video_driver_get_size(&width, &height);
            point->retcode = menu_driver_ctx->osk_ptr_at_pos(menu_userdata,
                  point->x, point->y, width, height);
         }
         break;
      case RARCH_MENU_CTL_BIND_INIT:
         {
            menu_ctx_bind_t *bind = (menu_ctx_bind_t*)data;

            if (!menu_driver_ctx || !menu_driver_ctx->bind_init)
            {
               bind->retcode = 0;
               return false;
            }
            bind->retcode = menu_driver_ctx->bind_init(
                  bind->cbs,
                  bind->path,
                  bind->label,
                  bind->type,
                  bind->idx);
         }
         break;
      case RARCH_MENU_CTL_UPDATE_THUMBNAIL_PATH:
         {
            size_t selection = menu_driver_selection_ptr;

            if (!menu_driver_ctx || !menu_driver_ctx->update_thumbnail_path)
               return false;
            menu_driver_ctx->update_thumbnail_path(menu_userdata, (unsigned)selection, 'L');
            menu_driver_ctx->update_thumbnail_path(menu_userdata, (unsigned)selection, 'R');
         }
         break;
      case RARCH_MENU_CTL_UPDATE_THUMBNAIL_IMAGE:
         {
            if (!menu_driver_ctx || !menu_driver_ctx->update_thumbnail_image)
               return false;
            menu_driver_ctx->update_thumbnail_image(menu_userdata);
         }
         break;
      case RARCH_MENU_CTL_REFRESH_THUMBNAIL_IMAGE:
         {
            if (!menu_driver_ctx || !menu_driver_ctx->refresh_thumbnail_image)
               return false;
            menu_driver_ctx->refresh_thumbnail_image(menu_userdata);
         }
         break;
      case RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH:
         {
            size_t selection = menu_driver_selection_ptr;

            if (!menu_driver_ctx || !menu_driver_ctx->update_savestate_thumbnail_path)
               return false;
            menu_driver_ctx->update_savestate_thumbnail_path(menu_userdata, (unsigned)selection);
         }
         break;
      case RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE:
         {
            if (!menu_driver_ctx || !menu_driver_ctx->update_savestate_thumbnail_image)
               return false;
            menu_driver_ctx->update_savestate_thumbnail_image(menu_userdata);
         }
         break;
      case MENU_NAVIGATION_CTL_CLEAR:
         {
            bool *pending_push = (bool*)data;

            /* Always set current selection to first entry */
            menu_driver_selection_ptr = 0;

            /* menu_driver_navigation_set() will be called
             * at the next 'push'.
             * If a push is *not* pending, have to do it here
             * instead */
            if (!(*pending_push))
            {
               menu_driver_navigation_set(true);

               if (menu_driver_ctx->navigation_clear)
                  menu_driver_ctx->navigation_clear(
                        menu_userdata, *pending_push);
            }
         }
         break;
      case MENU_NAVIGATION_CTL_INCREMENT:
         {
            settings_t *settings   = config_get_ptr();
            unsigned scroll_speed  = *((unsigned*)data);
            size_t  menu_list_size = menu_entries_get_size();
            bool wraparound_enable = settings->bools.menu_navigation_wraparound_enable;

            if (menu_driver_selection_ptr >= menu_list_size - 1
                  && !wraparound_enable)
               return false;

            if ((menu_driver_selection_ptr + scroll_speed) < menu_list_size)
            {
               size_t idx  = menu_driver_selection_ptr + scroll_speed;

               menu_driver_selection_ptr = idx;
               menu_driver_navigation_set(true);
            }
            else
            {
               if (wraparound_enable)
               {
                  bool pending_push = false;
                  menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
               }
               else if (menu_list_size > 0)
                  menu_driver_ctl(MENU_NAVIGATION_CTL_SET_LAST,  NULL);
            }

            if (menu_driver_ctx->navigation_increment)
               menu_driver_ctx->navigation_increment(menu_userdata);
         }
         break;
      case MENU_NAVIGATION_CTL_DECREMENT:
         {
            size_t idx             = 0;
            settings_t *settings   = config_get_ptr();
            unsigned scroll_speed  = *((unsigned*)data);
            size_t  menu_list_size = menu_entries_get_size();
            bool wraparound_enable = settings->bools.menu_navigation_wraparound_enable;

            if (menu_driver_selection_ptr == 0 && !wraparound_enable)
               return false;

            if (menu_driver_selection_ptr >= scroll_speed)
               idx = menu_driver_selection_ptr - scroll_speed;
            else
            {
               idx  = menu_list_size - 1;
               if (!wraparound_enable)
                  idx = 0;
            }

            menu_driver_selection_ptr = idx;
            menu_driver_navigation_set(true);

            if (menu_driver_ctx->navigation_decrement)
               menu_driver_ctx->navigation_decrement(menu_userdata);
         }
         break;
      case MENU_NAVIGATION_CTL_SET_LAST:
         {
            size_t menu_list_size     = menu_entries_get_size();
            size_t new_selection      = menu_list_size - 1;
            menu_driver_selection_ptr = new_selection;

            if (menu_driver_ctx->navigation_set_last)
               menu_driver_ctx->navigation_set_last(menu_userdata);
         }
         break;
      case MENU_NAVIGATION_CTL_ASCEND_ALPHABET:
         {
            size_t i               = 0;
            size_t  menu_list_size = menu_entries_get_size();

            if (!scroll_index_size)
               return false;

            if (menu_driver_selection_ptr == scroll_index_list[scroll_index_size - 1])
               menu_driver_selection_ptr = menu_list_size - 1;
            else
            {
               while (i < scroll_index_size - 1
                     && scroll_index_list[i + 1] <= menu_driver_selection_ptr)
                  i++;
               menu_driver_selection_ptr = scroll_index_list[i + 1];

               if (menu_driver_selection_ptr >= menu_list_size)
                  menu_driver_selection_ptr = menu_list_size - 1;
            }

            if (menu_driver_ctx->navigation_ascend_alphabet)
               menu_driver_ctx->navigation_ascend_alphabet(
                     menu_userdata, &menu_driver_selection_ptr);
         }
         break;
      case MENU_NAVIGATION_CTL_DESCEND_ALPHABET:
         {
            size_t i        = 0;

            if (!scroll_index_size)
               return false;

            if (menu_driver_selection_ptr == 0)
               return false;

            i   = scroll_index_size - 1;

            while (i && scroll_index_list[i - 1] >= menu_driver_selection_ptr)
               i--;

            if (i > 0)
               menu_driver_selection_ptr = scroll_index_list[i - 1];

            if (menu_driver_ctx->navigation_descend_alphabet)
               menu_driver_ctx->navigation_descend_alphabet(
                     menu_userdata, &menu_driver_selection_ptr);
         }
         break;
      case MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL:
         {
            size_t *sel = (size_t*)data;
            if (!sel)
               return false;
            *sel = scroll_acceleration;
         }
         break;
      case MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL:
         {
            size_t *sel = (size_t*)data;
            if (!sel)
               return false;
            scroll_acceleration = (unsigned)(*sel);
         }
         break;
      default:
      case RARCH_MENU_CTL_NONE:
         break;
   }

   return true;
}

void hex32_to_rgba_normalized(uint32_t hex, float* rgba, float alpha)
{
   rgba[0] = rgba[4] = rgba[8]  = rgba[12] = ((hex >> 16) & 0xFF) * (1.0f / 255.0f); /* r */
   rgba[1] = rgba[5] = rgba[9]  = rgba[13] = ((hex >> 8 ) & 0xFF) * (1.0f / 255.0f); /* g */
   rgba[2] = rgba[6] = rgba[10] = rgba[14] = ((hex >> 0 ) & 0xFF) * (1.0f / 255.0f); /* b */
   rgba[3] = rgba[7] = rgba[11] = rgba[15] = alpha;
}

void menu_subsystem_populate(const struct retro_subsystem_info* subsystem, menu_displaylist_info_t *info)
{
   settings_t *settings = config_get_ptr();
   /* Note: Create this string here explicitly (rather than
    * using a #define elsewhere) since we need to be aware of
    * its length... */
#if defined(__APPLE__)
   /* UTF-8 support is currently broken on Apple devices... */
   static const char utf8_star_char[] = "*";
#else
   /* <BLACK STAR>
    * UCN equivalent: "\u2605" */
   static const char utf8_star_char[] = "\xE2\x98\x85";
#endif
   char star_char[16];
   unsigned i = 0;
   int n = 0;
   bool is_rgui = string_is_equal(settings->arrays.menu_driver, "rgui");
   
   /* Select appropriate 'star' marker for subsystem menu entries
    * (i.e. RGUI does not support unicode, so use a 'standard'
    * character fallback) */
   snprintf(star_char, sizeof(star_char), "%s", is_rgui ? "*" : utf8_star_char);
   
   if (subsystem && subsystem_current_count > 0)
   {
      for (i = 0; i < subsystem_current_count; i++, subsystem++)
      {
         char s[PATH_MAX_LENGTH];
         if (content_get_subsystem() == i)
         {
            if (content_get_subsystem_rom_id() < subsystem->num_roms)
            {
               snprintf(s, sizeof(s),
                  "Load %s %s",
                  subsystem->desc,
                  star_char);
               
               /* If using RGUI with sublabels disabled, add the
                * appropriate text to the menu entry itself... */
               if (is_rgui && !settings->bools.menu_show_sublabels)
               {
                  char tmp[PATH_MAX_LENGTH];
                  
                  n = snprintf(tmp, sizeof(tmp),
                     "%s [%s %s]", s, "Current Content:",
                     subsystem->roms[content_get_subsystem_rom_id()].desc);

                  /* Stupid GCC will warn about snprintf() truncation even though
                   * we couldn't care less about it (if the menu entry label gets
                   * truncated then the string will already be too long to view in
                   * any usable manner on screen, so the fact that the end is
                   * missing is irrelevant). There are two ways to silence this noise:
                   * 1) Make the destination buffers large enough that text cannot be
                   *    truncated. This is a waste of memory.
                   * 2) Check the snprintf() return value (and take action). This is
                   *    the most harmless option, so we just print a warning if anything
                   *    is truncated.
                   * To reiterate: The actual warning generated here is pointless, and
                   * should be ignored. */
                  if ((n < 0) || (n >= PATH_MAX_LENGTH))
                  {
                     if (verbosity_is_enabled())
                     {
                        RARCH_WARN("Menu subsystem entry: Description label truncated.\n");
                     }
                  }

                  strlcpy(s, tmp, sizeof(s));
               }
               
               menu_entries_append_enum(info->list,
                  s,
                  msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD),
                  MENU_ENUM_LABEL_SUBSYSTEM_ADD,
                  MENU_SETTINGS_SUBSYSTEM_ADD + i, 0, 0);
            }
            else
            {
               snprintf(s, sizeof(s),
                  "Start %s %s",
                  subsystem->desc,
                  star_char);
               
               /* If using RGUI with sublabels disabled, add the
                * appropriate text to the menu entry itself... */
               if (is_rgui && !settings->bools.menu_show_sublabels)
               {
                  unsigned j = 0;
                  char rom_buff[PATH_MAX_LENGTH];
                  char tmp[PATH_MAX_LENGTH];
                  rom_buff[0] = '\0';

                  for (j = 0; j < content_get_subsystem_rom_id(); j++)
                  {
                     strlcat(rom_buff,
                           path_basename(content_get_subsystem_rom(j)), sizeof(rom_buff));
                     if (j != content_get_subsystem_rom_id() - 1)
                        strlcat(rom_buff, "|", sizeof(rom_buff));
                  }

                  if (!string_is_empty(rom_buff))
                  {
                     n = snprintf(tmp, sizeof(tmp), "%s [%s]", s, rom_buff);
                     
                     /* More snprintf() gcc warning suppression... */
                     if ((n < 0) || (n >= PATH_MAX_LENGTH))
                     {
                        if (verbosity_is_enabled())
                        {
                           RARCH_WARN("Menu subsystem entry: Description label truncated.\n");
                        }
                     }
                     
                     strlcpy(s, tmp, sizeof(s));
                  }
               }
               
               menu_entries_append_enum(info->list,
                  s,
                  msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_LOAD),
                  MENU_ENUM_LABEL_SUBSYSTEM_LOAD,
                  MENU_SETTINGS_SUBSYSTEM_LOAD, 0, 0);
            }
         }
         else
         {
            snprintf(s, sizeof(s),
               "Load %s",
               subsystem->desc);
            
            /* If using RGUI with sublabels disabled, add the
             * appropriate text to the menu entry itself... */
            if (is_rgui && !settings->bools.menu_show_sublabels)
            {
               /* This check is probably not required (it's not done
                * in menu_cbs_sublabel.c action_bind_sublabel_subsystem_add(),
                * anyway), but no harm in being safe... */
               if (subsystem->num_roms > 0)
               {
                  char tmp[PATH_MAX_LENGTH];
                  
                  n = snprintf(tmp, sizeof(tmp),
                     "%s [%s %s]", s, "Current Content:",
                     subsystem->roms[0].desc);
                  
                  /* More snprintf() gcc warning suppression... */
                  if ((n < 0) || (n >= PATH_MAX_LENGTH))
                  {
                     if (verbosity_is_enabled())
                     {
                        RARCH_WARN("Menu subsystem entry: Description label truncated.\n");
                     }
                  }
                  
                  strlcpy(s, tmp, sizeof(s));
               }
            }
            
            menu_entries_append_enum(info->list,
               s,
               msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD),
               MENU_ENUM_LABEL_SUBSYSTEM_ADD,
               MENU_SETTINGS_SUBSYSTEM_ADD + i, 0, 0);
         }
      }
   }
}

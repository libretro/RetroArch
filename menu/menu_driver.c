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

#include <string.h>

#include "menu.h"
#include "menu_display.h"
#include "../general.h"
#include "../string_list_special.h"

static const menu_ctx_driver_t *menu_ctx_drivers[] = {
#if defined(HAVE_RMENU)
   &menu_ctx_rmenu,
#endif
#if defined(HAVE_RMENU_XUI)
   &menu_ctx_rmenu_xui,
#endif
#if defined(HAVE_MATERIALUI)
   &menu_ctx_mui,
#endif
#if defined(HAVE_XMB)
   &menu_ctx_xmb,
#endif
#if defined(HAVE_RGUI)
   &menu_ctx_rgui,
#endif
#if defined(HAVE_ZARCH)
   &menu_ctx_zarch,
#endif
   &menu_ctx_null,
   NULL
};

static bool menu_alive = false;
static menu_handle_t *menu_driver_data;
static const menu_ctx_driver_t *menu_driver_ctx;

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
 * Returns: Human-readable identifier of menu driver at index. Can be NULL
 * if nothing found.
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

void find_menu_driver(void)
{
   settings_t *settings = config_get_ptr();

   int i = find_driver_index("menu_driver", settings->menu.driver);
   if (i >= 0)
      menu_driver_ctx = (const menu_ctx_driver_t*)menu_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_WARN("Couldn't find any menu driver named \"%s\"\n",
            settings->menu.driver);
      RARCH_LOG_OUTPUT("Available menu drivers are:\n");
      for (d = 0; menu_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", menu_driver_find_ident(d));
      RARCH_WARN("Going to default to first menu driver...\n");

      menu_driver_ctx = (const menu_ctx_driver_t*)menu_driver_find_handle(0);

      if (!menu_driver_ctx)
         retro_fail(1, "find_menu_driver()");
   }
}

menu_handle_t *menu_driver_get_ptr(void)
{
   if (!menu_driver_data)
      return NULL;
   return menu_driver_data;
}

const menu_ctx_driver_t *menu_ctx_driver_get_ptr(void)
{
   if (!menu_driver_ctx)
      return NULL;
   return menu_driver_ctx;
}

void init_menu(void)
{
   if (menu_driver_data)
      return;

   find_menu_driver();

   if (!(menu_driver_data = (menu_handle_t*)menu_init(menu_driver_ctx)))
      retro_fail(1, "init_menu()");

   if (menu_driver_ctx->lists_init)
      if (!menu_driver_ctx->lists_init(menu_driver_data))
         retro_fail(1, "init_menu()");
}


void  menu_driver_list_free(file_list_t *list, size_t idx, size_t list_size)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->list_free)
      driver->list_free(list, idx, list_size);

   file_list_free_userdata  (list, idx);
   file_list_free_actiondata(list, idx);
}

void  menu_driver_context_destroy(void)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver && driver->context_destroy)
      driver->context_destroy();
}

void  menu_driver_list_set_selection(file_list_t *list)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver && driver->list_set_selection)
      driver->list_set_selection(list);
}

size_t  menu_driver_list_get_selection(void)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();
   menu_handle_t *menu             = menu_driver_get_ptr();

   if (driver && driver->list_get_selection)
      return driver->list_get_selection(menu);
   return 0;
}

bool menu_driver_list_push(menu_displaylist_info_t *info, unsigned type)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->list_push)
      if (driver->list_push(info, type) == 0)
         return true;
   return false;
}

void menu_driver_list_cache(menu_list_type_t type, unsigned action)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->list_cache)
      driver->list_cache(type, action);
}

size_t menu_driver_list_get_size(menu_list_type_t type)
{
   menu_handle_t *menu             = menu_driver_get_ptr();
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver && driver->list_get_size)
      return driver->list_get_size(menu, type);
   return 0;
}

void *menu_driver_list_get_entry(menu_list_type_t type, unsigned i)
{
   menu_handle_t *menu             = menu_driver_get_ptr();
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver && driver->list_get_entry)
      return driver->list_get_entry(menu, type, i);
   return NULL;
}

void menu_driver_set_texture(void)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->set_texture)
      driver->set_texture();
}

void menu_driver_context_reset(void)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->context_reset)
      driver->context_reset();
}

void menu_driver_frame(void)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->frame)
      driver->frame();
}

int menu_driver_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   int ret = 0;
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver && driver->bind_init)
      ret = driver->bind_init(cbs, path, label, type, idx, elem0, elem1,
            label_hash, menu_label_hash);

   return ret;
}

void menu_driver_free(menu_handle_t *menu)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->free)
      driver->free(menu);
}

bool menu_driver_alive(void)
{
   return menu_alive;
}

int menu_driver_iterate(enum menu_action action)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->iterate)
      return driver->iterate(action);
   return -1;
}

void menu_driver_toggle(bool latch)
{
   const menu_ctx_driver_t *menu_driver = menu_ctx_driver_get_ptr();
   settings_t                 *settings = config_get_ptr();
   global_t                   *global   = global_get_ptr();
   rarch_system_info_t          *system = rarch_system_info_get_ptr();

   if (menu_driver->toggle)
      menu_driver->toggle(latch);

   menu_alive = latch;

   if (menu_alive == true)
   {
      menu_entries_set_refresh(false);

      /* Menu should always run with vsync on. */
      event_command(EVENT_CMD_VIDEO_SET_BLOCKING_STATE);
      /* Stop all rumbling before entering the menu. */
      event_command(EVENT_CMD_RUMBLE_STOP);

      if (settings->menu.pause_libretro)
         event_command(EVENT_CMD_AUDIO_STOP);

      /* Override keyboard callback to redirect to menu instead.
       * We'll use this later for something ...
       * FIXME: This should probably be moved to menu_common somehow. */
      if (global)
      {
         global->frontend_key_event = system->key_event;
         system->key_event          = menu_input_key_event;

         runloop_ctl(RUNLOOP_CTL_SET_FRAME_TIME_LAST, NULL);
      }
   }
   else
   {
      if (!runloop_ctl(RUNLOOP_CTL_IS_SHUTDOWN, NULL))
         driver_set_nonblock_state();

      if (settings && settings->menu.pause_libretro)
         event_command(EVENT_CMD_AUDIO_START);

      /* Prevent stray input from going to libretro core */
      input_driver_ctl(RARCH_INPUT_CTL_SET_FLUSHING_INPUT, NULL);

      /* Restore libretro keyboard callback. */
      if (global)
         system->key_event = global->frontend_key_event;
   }
}

bool menu_driver_load_image(void *data, menu_image_type_t type)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->load_image)
      return driver->load_image(data, type);

   return false;
}

bool menu_environment_cb(menu_environ_cb_t type, void *data)
{
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->environ_cb)
   {
      int ret = driver->environ_cb(type, data);
      if (ret == 0)
         return true;
   }

   return false;
}

int menu_driver_pointer_tap(unsigned x, unsigned y, unsigned ptr,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   int ret = 0;
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->pointer_tap)
      ret = driver->pointer_tap(x, y, ptr, cbs, entry, action);

   return ret;
}

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data)
{
   static bool menu_driver_data_own               = false;

   switch (state)
   {
      case RARCH_MENU_CTL_SET_OWN_DRIVER:
         menu_driver_data_own = true;
         break;
      case RARCH_MENU_CTL_UNSET_OWN_DRIVER:
         menu_driver_data_own = false;
         break;
      case RARCH_MENU_CTL_IS_SET_TEXTURE:
         if (!menu_driver_ctx)
            return false;
         return menu_driver_ctx->set_texture;
      case RARCH_MENU_CTL_OWNS_DRIVER:
         return menu_driver_data_own;
      case RARCH_MENU_CTL_DEINIT:
         menu_free(menu_driver_data);
         menu_driver_data = NULL;
         menu_driver_ctx  = NULL;
         break;
      default:
      case RARCH_MENU_CTL_NONE:
         break;
   }

   return false;
}

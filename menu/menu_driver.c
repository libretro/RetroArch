/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <file/file_path.h>
#include <string/stdstring.h>

#include "menu_driver.h"
#include "menu_cbs.h"
#include "menu_display.h"
#include "menu_navigation.h"
#include "menu_hash.h"
#include "menu_shader.h"

#include "../content.h"
#include "../core_info.h"
#include "../general.h"
#include "../system.h"
#include "../defaults.h"
#include "../frontend/frontend.h"
#include "../string_list_special.h"
#include "../tasks/tasks_internal.h"
#include "../verbosity.h"

static const menu_ctx_driver_t *menu_ctx_drivers[] = {
#if defined(HAVE_XUI)
   &menu_ctx_xui,
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

static menu_handle_t *menu_driver_data = NULL;
static const menu_ctx_driver_t *menu_driver_ctx = NULL;
static void *menu_userdata = NULL;

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

static bool find_menu_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = config_get_ptr();

   drv.label = "menu_driver";
   drv.s     = settings->menu.driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i = drv.len;

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
      {
         retro_fail(1, "find_menu_driver()");
         return false;
      }
   }

   return true;
}

menu_handle_t *menu_driver_get_ptr(void)
{
   if (!menu_driver_data)
      return NULL;
   return menu_driver_data;
}

#ifdef HAVE_ZLIB
static void bundle_decompressed(void *task_data,
      void *user_data, const char *err)
{
   settings_t      *settings   = config_get_ptr();
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   if (dec && !err)
      event_cmd_ctl(EVENT_CMD_REINIT, NULL);

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      /* delete bundle? */
      free(dec->source_file);
      free(dec);
   }

   settings->bundle_assets_extract_last_version = 
      settings->bundle_assets_extract_version_current;
   settings->bundle_finished = true;
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
static bool menu_init(void)
{
   settings_t *settings        = config_get_ptr();

   if (!menu_entries_ctl(MENU_ENTRIES_CTL_INIT, NULL))
      return false;

   if (!core_info_ctl(CORE_INFO_CTL_CURRENT_CORE_INIT, NULL))
      return false;

   if (!menu_driver_ctl(RARCH_MENU_CTL_SHADER_INIT, NULL))
      return false;

   if (settings->menu_show_start_screen)
   {
      menu_driver_data->push_help_screen = true;
      menu_driver_data->help_screen_type = MENU_HELP_WELCOME;
      settings->menu_show_start_screen   = false;
      event_cmd_ctl(EVENT_CMD_MENU_SAVE_CURRENT_CONFIG, NULL);
   }

   if (      settings->bundle_assets_extract_enable
         && !string_is_empty(settings->bundle_assets_src_path) 
         && !string_is_empty(settings->bundle_assets_dst_path)
#ifdef IOS
         && menu_driver_data->push_help_screen
#else
         && (settings->bundle_assets_extract_version_current 
            != settings->bundle_assets_extract_last_version)
#endif
      )
   {
      menu_driver_data->help_screen_type           = MENU_HELP_EXTRACT;
      menu_driver_data->push_help_screen           = true;
#ifdef HAVE_ZLIB
      rarch_task_push_decompress(settings->bundle_assets_src_path, 
            settings->bundle_assets_dst_path,
            NULL, settings->bundle_assets_dst_path_subdir,
            NULL, bundle_decompressed, NULL);
#endif
   }

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_MANAGER_INIT, NULL);

   if (!menu_display_ctl(MENU_DISPLAY_CTL_INIT, NULL))
      return false;

   return true;
}

void  menu_driver_list_free(file_list_t *list, size_t idx, size_t list_size)
{
   if (menu_driver_ctx)
   {
      if (menu_driver_ctx->list_free)
         menu_driver_ctx->list_free(list, idx, list_size);
   }

   if (list)
   {
      file_list_free_userdata  (list, idx);
      file_list_free_actiondata(list, idx);
   }
}

size_t  menu_driver_list_get_selection(void)
{
   if (!menu_driver_ctx || !menu_driver_ctx->list_get_selection)
      return 0;
   return menu_driver_ctx->list_get_selection(menu_userdata);
}

bool menu_driver_list_push(menu_displaylist_info_t *info, unsigned type)
{
   if (menu_driver_ctx->list_push)
      if (menu_driver_ctx->list_push(menu_driver_data,
               menu_userdata, info, type) == 0)
         return true;
   return false;
}

size_t menu_driver_list_get_size(menu_list_type_t type)
{
   if (!menu_driver_ctx || !menu_driver_ctx->list_get_size)
      return 0;
   return menu_driver_ctx->list_get_size(menu_userdata, type);
}

void *menu_driver_list_get_entry(menu_list_type_t type, unsigned i)
{
   if (!menu_driver_ctx || !menu_driver_ctx->list_get_entry)
      return NULL;
   return menu_driver_ctx->list_get_entry(menu_userdata, type, i);
}

int menu_driver_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!menu_driver_ctx || !menu_driver_ctx->bind_init)
      return 0;
   return menu_driver_ctx->bind_init(cbs, path, label,
         type, idx, elem0, elem1,
         label_hash, menu_label_hash);
}

static void menu_input_key_event(bool down, unsigned keycode,
      uint32_t character, uint16_t mod)
{
   (void)down;
   (void)keycode;
   (void)mod;

   if (character == '/')
      menu_entry_action(NULL, 0, MENU_ACTION_SEARCH);
}

static void menu_driver_toggle(bool latch)
{
   retro_keyboard_event_t *key_event          = NULL;
   retro_keyboard_event_t *frontend_key_event = NULL;
   settings_t                 *settings       = config_get_ptr();
   rarch_system_info_t          *system       = NULL;
   
   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);
   menu_driver_ctl(RARCH_MENU_CTL_TOGGLE, &latch);

   if (latch)
      menu_driver_ctl(RARCH_MENU_CTL_SET_ALIVE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_ALIVE, NULL);

   runloop_ctl(RUNLOOP_CTL_FRONTEND_KEY_EVENT_GET, &frontend_key_event);
   runloop_ctl(RUNLOOP_CTL_KEY_EVENT_GET, &key_event);

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
   {
      bool refresh = false;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

      /* Menu should always run with vsync on. */
      event_cmd_ctl(EVENT_CMD_VIDEO_SET_BLOCKING_STATE, NULL);
      /* Stop all rumbling before entering the menu. */
      event_cmd_ctl(EVENT_CMD_RUMBLE_STOP, NULL);

      if (settings->menu.pause_libretro)
         event_cmd_ctl(EVENT_CMD_AUDIO_STOP, NULL);

      /* Override keyboard callback to redirect to menu instead.
       * We'll use this later for something ... */

      if (key_event && frontend_key_event)
      {
         *frontend_key_event        = *key_event;
         *key_event                 = menu_input_key_event;

         runloop_ctl(RUNLOOP_CTL_SET_FRAME_TIME_LAST, NULL);
      }
   }
   else
   {
      if (!runloop_ctl(RUNLOOP_CTL_IS_SHUTDOWN, NULL))
         driver_ctl(RARCH_DRIVER_CTL_SET_NONBLOCK_STATE, NULL);

      if (settings && settings->menu.pause_libretro)
         event_cmd_ctl(EVENT_CMD_AUDIO_START, NULL);

      /* Prevent stray input from going to libretro core */
      input_driver_ctl(RARCH_INPUT_CTL_SET_FLUSHING_INPUT, NULL);

      /* Restore libretro keyboard callback. */
      if (key_event && frontend_key_event)
         *key_event = *frontend_key_event;
   }
}

bool menu_environment_cb(menu_environ_cb_t type, void *data)
{
   if (menu_driver_ctx->environ_cb)
   {
      int ret = menu_driver_ctx->environ_cb(type, data, menu_userdata);

      if (ret == 0)
         return true;
   }

   return false;
}

int menu_driver_pointer_tap(unsigned x, unsigned y, unsigned ptr,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   if (!menu_driver_ctx || !menu_driver_ctx->pointer_tap)
      return 0;
   return menu_driver_ctx->pointer_tap(menu_userdata,
         x, y, ptr, cbs, entry, action);
}

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data)
{
   static struct retro_system_info menu_driver_system;
   static bool menu_driver_prevent_populate        = false;
   static bool menu_driver_load_no_content         = false;
   static bool menu_driver_alive                   = false;
   static bool menu_driver_data_own                = false;
   static bool menu_driver_pending_quit            = false;
   static bool menu_driver_pending_shutdown        = false;
   static content_playlist_t *menu_driver_playlist = NULL;
   static struct video_shader *menu_driver_shader  = NULL;
   settings_t *settings                            = config_get_ptr();

   switch (state)
   {
      case RARCH_MENU_CTL_IS_PENDING_QUIT:
         return menu_driver_pending_quit;
      case RARCH_MENU_CTL_SET_PENDING_QUIT:
         menu_driver_pending_quit     = true;
         break;
      case RARCH_MENU_CTL_UNSET_PENDING_QUIT:
         menu_driver_pending_quit     = false;
         break;
      case RARCH_MENU_CTL_IS_PENDING_SHUTDOWN:
         return menu_driver_pending_shutdown;
      case RARCH_MENU_CTL_SET_PENDING_SHUTDOWN:
         menu_driver_pending_shutdown = true;
         break;
      case RARCH_MENU_CTL_UNSET_PENDING_SHUTDOWN:
         menu_driver_pending_shutdown = false;
         break;
      case RARCH_MENU_CTL_DESTROY:
         menu_driver_pending_quit     = false;
         menu_driver_pending_shutdown = false;
         menu_driver_prevent_populate = false;
         menu_driver_load_no_content  = false;
         menu_driver_alive            = false;
         menu_driver_data_own         = false;
         menu_driver_ctx              = NULL;
         break;
      case RARCH_MENU_CTL_PLAYLIST_FREE:
         if (menu_driver_playlist)
            content_playlist_free(menu_driver_playlist);
         menu_driver_playlist = NULL;
         break;
      case RARCH_MENU_CTL_FIND_DRIVER:
         return find_menu_driver();
      case RARCH_MENU_CTL_PLAYLIST_INIT:
         {
            const char *path = (const char*)data;
            if (string_is_empty(path))
               return false;
            menu_driver_playlist  = content_playlist_init(path,
                  COLLECTION_SIZE);
         }
         break;
      case RARCH_MENU_CTL_PLAYLIST_GET:
         {
            content_playlist_t **playlist = (content_playlist_t**)data;
            if (!playlist)
               return false;
            *playlist = menu_driver_playlist;
         }
         break;
      case RARCH_MENU_CTL_SYSTEM_INFO_GET:
         {
            struct retro_system_info **system = 
               (struct retro_system_info**)data;
            if (!system)
               return false;
            *system = &menu_driver_system;
         }
         break;
      case RARCH_MENU_CTL_SYSTEM_INFO_DEINIT:
#ifdef HAVE_DYNAMIC
         libretro_free_system_info(&menu_driver_system);
         memset(&menu_driver_system, 0, sizeof(struct retro_system_info));
#endif
         break;
      case RARCH_MENU_CTL_RENDER_MESSAGEBOX:
         if (menu_driver_ctx->render_messagebox)
            menu_driver_ctx->render_messagebox(menu_userdata,
                  menu_driver_data->menu_state.msg);
         break;
      case RARCH_MENU_CTL_BLIT_RENDER:
         if (menu_driver_ctx->render)
            menu_driver_ctx->render(menu_userdata);
         break;
      case RARCH_MENU_CTL_RENDER:
         menu_iterate_render(menu_driver_data, menu_userdata);
         break;
      case RARCH_MENU_CTL_SHADER_DEINIT:
#ifdef HAVE_SHADER_MANAGER
         if (menu_driver_shader)
            free(menu_driver_shader);
         menu_driver_shader = NULL;
#endif
         break;
      case RARCH_MENU_CTL_SHADER_INIT:
#ifdef HAVE_SHADER_MANAGER
         menu_driver_shader = (struct video_shader*)
            calloc(1, sizeof(struct video_shader));
         if (!menu_driver_shader)
            return false;
#endif
         break;
      case RARCH_MENU_CTL_SHADER_GET:
         {
            struct video_shader **shader = (struct video_shader**)data;
            if (!shader)
               return false;
            *shader = menu_driver_shader;
         }
         break;
      case RARCH_MENU_CTL_FRAME:
         if (!menu_driver_alive)
            return false;
         if (menu_driver_ctx->frame)
            menu_driver_ctx->frame(menu_userdata);
         break;
      case RARCH_MENU_CTL_SET_PREVENT_POPULATE:
         menu_driver_prevent_populate = true;
         break;
      case RARCH_MENU_CTL_UNSET_PREVENT_POPULATE:
         menu_driver_prevent_populate = false;
         break;
      case RARCH_MENU_CTL_IS_PREVENT_POPULATE:
         return menu_driver_prevent_populate;
      case RARCH_MENU_CTL_SET_TOGGLE:
         menu_driver_toggle(true);
         break;
      case RARCH_MENU_CTL_UNSET_TOGGLE:
         menu_driver_toggle(false);
         break;
      case RARCH_MENU_CTL_SET_ALIVE:
         menu_driver_alive = true;
         break;
      case RARCH_MENU_CTL_UNSET_ALIVE:
         menu_driver_alive = false;
         break;
      case RARCH_MENU_CTL_IS_ALIVE:
         return menu_driver_alive;
      case RARCH_MENU_CTL_SET_OWN_DRIVER:
         menu_driver_data_own = true;
         break;
      case RARCH_MENU_CTL_UNSET_OWN_DRIVER:
         if (!content_ctl(CONTENT_CTL_IS_INITED, NULL))
            return false;
         menu_driver_data_own = false;
         break;
      case RARCH_MENU_CTL_SET_TEXTURE:
         if (menu_driver_ctx->set_texture)
            menu_driver_ctx->set_texture();
         break;
      case RARCH_MENU_CTL_IS_SET_TEXTURE:
         if (!menu_driver_ctx)
            return false;
         return menu_driver_ctx->set_texture;
      case RARCH_MENU_CTL_OWNS_DRIVER:
         return menu_driver_data_own;
      case RARCH_MENU_CTL_DEINIT:
         menu_driver_ctl(RARCH_MENU_CTL_CONTEXT_DESTROY, NULL);
         if (menu_driver_ctl(RARCH_MENU_CTL_OWNS_DRIVER, NULL))
            return true;
         if (menu_driver_data)
         {
            menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_FREE, NULL);
            menu_shader_free(menu_driver_data);
            menu_input_ctl(MENU_INPUT_CTL_DEINIT, NULL);
            menu_navigation_ctl(MENU_NAVIGATION_CTL_DEINIT, NULL);

            if (menu_driver_ctx && menu_driver_ctx->free)
               menu_driver_ctx->free(menu_userdata);

            if (menu_userdata)
               free(menu_userdata);
            menu_userdata = NULL;

            menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_DEINIT, NULL);
            menu_display_ctl(MENU_DISPLAY_CTL_DEINIT, NULL);
            menu_entries_ctl(MENU_ENTRIES_CTL_DEINIT, NULL);

            event_cmd_ctl(EVENT_CMD_HISTORY_DEINIT, NULL);

            core_info_ctl(CORE_INFO_CTL_LIST_DEINIT, NULL);
            core_info_ctl(CORE_INFO_CTL_CURRENT_CORE_FREE, NULL);

            free(menu_driver_data);
         }
         menu_driver_data = NULL;
         break;
      case RARCH_MENU_CTL_INIT:
         if (menu_driver_data)
            return true;

         menu_driver_data = (menu_handle_t*)
            menu_driver_ctx->init(&menu_userdata);

         if (!menu_driver_data || !menu_init())
         {
            retro_fail(1, "init_menu()");
            return false;
         }

         strlcpy(settings->menu.driver, menu_driver_ctx->ident,
               sizeof(settings->menu.driver));

         if (menu_driver_ctx->lists_init)
         {
            if (!menu_driver_ctx->lists_init(menu_driver_data))
            {
               retro_fail(1, "init_menu()");
               return false;
            }
         }
         break;
      case RARCH_MENU_CTL_LOAD_NO_CONTENT_GET:
         {
            bool **ptr = (bool**)data;
            if (!ptr)
               return false;
            *ptr = (bool*)&menu_driver_load_no_content;
         }
         break;
      case RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT:
         return menu_driver_load_no_content;
      case RARCH_MENU_CTL_SET_LOAD_NO_CONTENT:
         menu_driver_load_no_content = true;
         break;
      case RARCH_MENU_CTL_UNSET_LOAD_NO_CONTENT:
         menu_driver_load_no_content = false;
         break;
      case RARCH_MENU_CTL_NAVIGATION_INCREMENT:
         if (menu_driver_ctx->navigation_increment)
            menu_driver_ctx->navigation_increment(menu_userdata);
         break;
      case RARCH_MENU_CTL_NAVIGATION_DECREMENT:
         if (menu_driver_ctx->navigation_decrement)
            menu_driver_ctx->navigation_decrement(menu_userdata);
         break;
      case RARCH_MENU_CTL_NAVIGATION_SET:
         {
            bool *scroll = (bool*)data;

            if (!scroll)
               return false;
            if (menu_driver_ctx->navigation_set)
               menu_driver_ctx->navigation_set(menu_userdata, *scroll);
         }
         break;
      case RARCH_MENU_CTL_NAVIGATION_SET_LAST:
         if (menu_driver_ctx->navigation_set_last)
            menu_driver_ctx->navigation_set_last(menu_userdata);
         break;
      case RARCH_MENU_CTL_NAVIGATION_ASCEND_ALPHABET:
         {
            size_t *ptr_out = (size_t*)data;

            if (!ptr_out)
               return false;

            if (menu_driver_ctx->navigation_ascend_alphabet)
               menu_driver_ctx->navigation_ascend_alphabet(
                     menu_userdata, ptr_out);
         }
       case RARCH_MENU_CTL_NAVIGATION_DESCEND_ALPHABET:
         {
            size_t *ptr_out = (size_t*)data;

            if (!ptr_out)
               return false;

            if (menu_driver_ctx->navigation_descend_alphabet)
               menu_driver_ctx->navigation_descend_alphabet(
                     menu_userdata, ptr_out);
         }
         break;
       case RARCH_MENU_CTL_NAVIGATION_CLEAR:
         {
            bool *pending_push = (bool*)data;

            if (!pending_push)
               return false;
            if (menu_driver_ctx->navigation_clear)
               menu_driver_ctx->navigation_clear(
                     menu_userdata, pending_push);
         }
         break;
       case RARCH_MENU_CTL_POPULATE_ENTRIES:
         {
            menu_displaylist_info_t *info = (menu_displaylist_info_t*)data;

            if (!info)
               return false;
            if (menu_driver_ctx->populate_entries)
               menu_driver_ctx->populate_entries(
                     menu_userdata, info->path,
                     info->label, info->type);
         }
         break;
       case RARCH_MENU_CTL_LIST_CLEAR:
         {
            file_list_t *list = (file_list_t*)data;
            if (!list)
               return false;
            if (menu_driver_ctx->list_clear)
               menu_driver_ctx->list_clear(list);
         }
         break;
       case RARCH_MENU_CTL_TOGGLE:
         {
            bool *latch = (bool*)data;
            if (!latch)
               return false;

            if (menu_driver_ctx->toggle)
               menu_driver_ctx->toggle(menu_userdata, *latch);
         }
         break;
       case RARCH_MENU_CTL_REFRESH:
         {
#if 0
            bool refresh = false;
            menu_entries_ctl(MENU_ENTRIES_CTL_LIST_DEINIT, NULL);
            menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_DEINIT, NULL);
            menu_entries_ctl(MENU_ENTRIES_CTL_INIT, NULL);
            menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
#endif
         }
         break;
       case RARCH_MENU_CTL_CONTEXT_RESET:
         if (!menu_driver_ctx || !menu_driver_ctx->context_reset)
            return false;
         menu_driver_ctx->context_reset(menu_userdata);
         break;
       case RARCH_MENU_CTL_CONTEXT_DESTROY:
         if (!menu_driver_ctx || !menu_driver_ctx->context_destroy)
            return false;
         menu_driver_ctx->context_destroy(menu_userdata);
         break;
       case RARCH_MENU_CTL_SHADER_MANAGER_INIT:
         menu_shader_manager_init(menu_driver_data);
         break;
       case RARCH_MENU_CTL_LIST_SET_SELECTION:
         {
            file_list_t *list = (file_list_t*)data;

            if (!list)
               return false;

            if (!menu_driver_ctx || !menu_driver_ctx->list_set_selection)
               return false;

            menu_driver_ctx->list_set_selection(menu_userdata, list);
         }
         break;
       case RARCH_MENU_CTL_LIST_CACHE:
         {
            menu_ctx_list_t *list = (menu_ctx_list_t*)data;
            if (!list || !menu_driver_ctx || !menu_driver_ctx->list_cache)
               return false;
            menu_driver_ctx->list_cache(menu_userdata, list->type, list->action);
         }
         break;
       case RARCH_MENU_CTL_LIST_INSERT:
         {
            menu_ctx_list_t *list = (menu_ctx_list_t*)data;
            if (!list || !menu_driver_ctx || !menu_driver_ctx->list_insert)
               return false;
            menu_driver_ctx->list_insert(menu_userdata, list->list, list->path, list->label, list->idx);
         }
         break;
       case RARCH_MENU_CTL_LOAD_IMAGE:
         {
            menu_ctx_load_image_t *load_image_info = (menu_ctx_load_image_t*)data;
            if (!menu_driver_ctx || !menu_driver_ctx->load_image)
               return false;
            return menu_driver_ctx->load_image(menu_userdata,
                  load_image_info->data, load_image_info->type);
         }
       case RARCH_MENU_CTL_ITERATE:
         {
            menu_ctx_iterate_t *iterate = (menu_ctx_iterate_t*)data;

            if (menu_driver_ctl(RARCH_MENU_CTL_IS_PENDING_QUIT, NULL))
            {
               menu_driver_ctl(RARCH_MENU_CTL_UNSET_PENDING_QUIT, NULL);
               return false;
            }
            if (menu_driver_ctl(RARCH_MENU_CTL_IS_PENDING_SHUTDOWN, NULL))
            {
               menu_driver_ctl(RARCH_MENU_CTL_UNSET_PENDING_SHUTDOWN, NULL);
               if (!event_cmd_ctl(EVENT_CMD_QUIT, NULL))
                  return false;
               return true;
            }
            if (!menu_driver_ctx || !menu_driver_ctx->iterate)
               return false;
            if (menu_driver_ctx->iterate(menu_driver_data, menu_userdata, iterate->action) == -1)
               return false;
         }
         break;
      default:
      case RARCH_MENU_CTL_NONE:
         break;
   }

   return true;
}

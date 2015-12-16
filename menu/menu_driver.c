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

#include <file/file_path.h>

#include "menu_driver.h"
#include "menu_cbs.h"
#include "menu_display.h"
#include "menu_navigation.h"
#include "menu_hash.h"
#include "menu_shader.h"

#include "../system.h"
#include "../defaults.h"
#include "../frontend/frontend.h"
#include "../string_list_special.h"
#include "../tasks/tasks.h"

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

static menu_handle_t *menu_driver_data;
static const menu_ctx_driver_t *menu_driver_ctx;
static struct video_shader *menu_driver_shader;
static void *menu_userdata;

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

void menu_driver_list_insert(file_list_t *list, const char *path,
      const char *label, size_t idx)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver->list_insert)
      driver->list_insert(menu_userdata ? menu_userdata : NULL, list, path, label, idx);
}

void  menu_driver_list_free(file_list_t *list, size_t idx, size_t list_size)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver->list_free)
      driver->list_free(list, idx, list_size);

   file_list_free_userdata  (list, idx);
   file_list_free_actiondata(list, idx);
}

void  menu_driver_list_set_selection(file_list_t *list)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver && driver->list_set_selection)
      driver->list_set_selection(menu_userdata ? menu_userdata : NULL, list);
}

size_t  menu_driver_list_get_selection(void)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver && driver->list_get_selection)
      return driver->list_get_selection(menu_userdata ? menu_userdata : NULL);
   return 0;
}

bool menu_driver_list_push(menu_displaylist_info_t *info, unsigned type)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver->list_push)
      if (driver->list_push(menu_driver_data, menu_userdata ? menu_userdata : NULL,
               info, type) == 0)
         return true;
   return false;
}

void menu_driver_list_cache(menu_list_type_t type, unsigned action)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver->list_cache)
      driver->list_cache(menu_userdata ? menu_userdata : NULL, type, action);
}

size_t menu_driver_list_get_size(menu_list_type_t type)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver && driver->list_get_size)
      return driver->list_get_size(menu_userdata ? menu_userdata : NULL, type);
   return 0;
}

void *menu_driver_list_get_entry(menu_list_type_t type, unsigned i)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver && driver->list_get_entry)
      return driver->list_get_entry(menu_userdata ? menu_userdata : NULL, type, i);
   return NULL;
}

int menu_driver_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   int ret = 0;
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver && driver->bind_init)
      ret = driver->bind_init(cbs, path, label, type, idx, elem0, elem1,
            label_hash, menu_label_hash);

   return ret;
}

void menu_driver_free(menu_handle_t *menu)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver->free)
      driver->free(menu_userdata);

   if (!menu_driver_data)
      return;

   if (menu_userdata)
      free(menu_userdata);
   menu_userdata = NULL;
}

int menu_driver_iterate(enum menu_action action)
{
   const menu_ctx_driver_t *driver = menu_driver_ctx;

   if (driver->iterate)
      return driver->iterate(menu_driver_data, 
            menu_userdata ? menu_userdata : NULL, action);
   return -1;
}

static void menu_driver_toggle(bool latch)
{
   settings_t                 *settings = config_get_ptr();
   global_t                   *global   = global_get_ptr();
   rarch_system_info_t          *system = NULL;
   
   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);
   menu_driver_ctl(RARCH_MENU_CTL_TOGGLE, &latch);

   if (latch)
      menu_driver_ctl(RARCH_MENU_CTL_SET_ALIVE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_ALIVE, NULL);

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
   {
      bool refresh = false;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

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
         retro_keyboard_event_t *key_event = NULL;
         runloop_ctl(RUNLOOP_CTL_KEY_EVENT_GET, &key_event);

         if (key_event)
         {
            global->frontend_key_event = *key_event;
            *key_event                 = menu_input_key_event;

            runloop_ctl(RUNLOOP_CTL_SET_FRAME_TIME_LAST, NULL);
         }
      }
   }
   else
   {
      if (!runloop_ctl(RUNLOOP_CTL_IS_SHUTDOWN, NULL))
         driver_ctl(RARCH_DRIVER_CTL_SET_NONBLOCK_STATE, NULL);

      if (settings && settings->menu.pause_libretro)
         event_command(EVENT_CMD_AUDIO_START);

      /* Prevent stray input from going to libretro core */
      input_driver_ctl(RARCH_INPUT_CTL_SET_FLUSHING_INPUT, NULL);

      /* Restore libretro keyboard callback. */
      if (global)
      {
         retro_keyboard_event_t *key_event = NULL;

         runloop_ctl(RUNLOOP_CTL_KEY_EVENT_GET, &key_event);

         if (key_event)
            *key_event = global->frontend_key_event;
      }
   }
}

bool menu_driver_load_image(void *data, menu_image_type_t type)
{
   const menu_ctx_driver_t      *driver = menu_driver_ctx;

   if (driver->load_image)
      return driver->load_image(menu_userdata ? menu_userdata : NULL, data, type);

   return false;
}

bool menu_environment_cb(menu_environ_cb_t type, void *data)
{
   const menu_ctx_driver_t      *driver = menu_driver_ctx;

   if (driver->environ_cb)
   {
      int ret = driver->environ_cb(type, data, menu_userdata ? menu_userdata : NULL);
      if (ret == 0)
         return true;
   }

   return false;
}

int menu_driver_pointer_tap(unsigned x, unsigned y, unsigned ptr,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   const menu_ctx_driver_t      *driver = menu_driver_ctx;

   if (driver->pointer_tap)
      return driver->pointer_tap(menu_userdata ? menu_userdata : NULL, x, y, ptr, cbs, entry, action);

   return 0;
}

static void menu_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
   char *fullpath       = NULL;
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();
    
   if (!wrap_args)
      return;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   wrap_args->no_content       = menu_driver_ctl(RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL);
   if (!global->has_set.verbosity)
      wrap_args->verbose       = *retro_main_verbosity();

   wrap_args->config_path      = *global->path.config   ? global->path.config   : NULL;
   wrap_args->sram_path        = *global->dir.savefile  ? global->dir.savefile  : NULL;
   wrap_args->state_path       = *global->dir.savestate ? global->dir.savestate : NULL;
   wrap_args->content_path     = *fullpath              ? fullpath              : NULL;

   if (!global->has_set.libretro)
      wrap_args->libretro_path = *settings->libretro ? settings->libretro : NULL;
   wrap_args->touched       = true;
}

static void menu_push_to_history_playlist(void)
{
   struct retro_system_info *system = NULL;
   settings_t *settings        = config_get_ptr();
   char *fullpath              = NULL;

   if (!settings->history_list_enable)
      return;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (*fullpath)
   {
      char tmp[PATH_MAX_LENGTH];
      char str[PATH_MAX_LENGTH];

      fill_pathname_base(tmp, fullpath, sizeof(tmp));
      snprintf(str, sizeof(str), "INFO - Loading %s ...", tmp);
      menu_display_msg_queue_push(str, 1, 1, false);
   }

   menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET,
         &system);

   content_playlist_push(g_defaults.history,
         fullpath,
         NULL,
         settings->libretro,
         system->library_name,
         NULL,
         NULL);

   content_playlist_write_file(g_defaults.history);
}

/**
 * menu_load_content:
 *
 * Loads content into currently selected core.
 * Will also optionally push the content entry to the history playlist.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_load_content(enum rarch_core_type type)
{
   bool msg_force       = true;
   char *fullpath       = NULL;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);
   /* redraw menu frame */
   menu_display_ctl(MENU_DISPLAY_CTL_SET_MSG_FORCE, &msg_force);
   menu_driver_ctl(RARCH_MENU_CTL_RENDER, NULL);

   if (!(main_load_content(0, NULL, NULL, menu_environment_get)))
   {
      char name[PATH_MAX_LENGTH] = {0};
      char msg[PATH_MAX_LENGTH]  = {0};

      fill_pathname_base(name, fullpath, sizeof(name));
      snprintf(msg, sizeof(msg), "Failed to load %s.\n", name);
      menu_display_msg_queue_push(msg, 1, 90, false);

      return false;
   }

   menu_shader_manager_init(menu_driver_data);

   event_command(EVENT_CMD_HISTORY_INIT);

   if (*fullpath || menu_driver_ctl(RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL))
      menu_push_to_history_playlist();

   event_command(EVENT_CMD_VIDEO_SET_ASPECT_RATIO);
   event_command(EVENT_CMD_RESUME);

   return true;
}

int menu_common_load_content(
      const char *core_path, const char *fullpath,
      bool persist, enum rarch_core_type type)
{
   enum event_command cmd       = EVENT_CMD_NONE;

   if (core_path)
   {
      runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
      event_command(EVENT_CMD_LOAD_CORE);
   }

   if (fullpath)
      runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)fullpath);

   switch (type)
   {
      case CORE_TYPE_PLAIN:
      case CORE_TYPE_DUMMY:
         cmd = persist ? EVENT_CMD_LOAD_CONTENT_PERSIST : EVENT_CMD_LOAD_CONTENT;
         break;
#ifdef HAVE_FFMPEG
      case CORE_TYPE_FFMPEG:
         cmd = EVENT_CMD_LOAD_CONTENT_FFMPEG;
         break;
#endif
      case CORE_TYPE_IMAGEVIEWER:
#ifdef HAVE_IMAGEVIEWER
         cmd = EVENT_CMD_LOAD_CONTENT_IMAGEVIEWER;
#endif
         break;
   }

   if (cmd != EVENT_CMD_NONE)
      event_command(cmd);

   return -1;
}

/**
 * menu_free:
 * @menu                     : Menu handle.
 *
 * Frees a menu handle
 **/
static void menu_free(menu_handle_t *menu)
{
   if (!menu)
      return;

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_FREE, NULL);
   menu_shader_free(menu);
   menu_input_ctl(MENU_INPUT_CTL_DEINIT, NULL);
   menu_navigation_ctl(MENU_NAVIGATION_CTL_DEINIT, NULL);
   menu_driver_free(menu);
   menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_DEINIT, NULL);
   menu_display_free();
   menu_entries_ctl(MENU_ENTRIES_CTL_DEINIT, NULL);

   event_command(EVENT_CMD_HISTORY_DEINIT);

   runloop_ctl(RUNLOOP_CTL_CURRENT_CORE_LIST_FREE, NULL);
   runloop_ctl(RUNLOOP_CTL_CURRENT_CORE_FREE, NULL);

   free(menu);
}

static void bundle_decompressed(void *task_data, void *user_data, const char *err)
{
   settings_t      *settings   = config_get_ptr();
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   if (dec && !err)
      event_command(EVENT_CMD_REINIT);

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      /* delete bundle? */
      free(dec->source_file);
      free(dec);
   }

   settings->bundle_assets_extract_last_version = settings->bundle_assets_extract_version_current;
   settings->bundle_finished = true;
}

/**
 * menu_init:
 * @data                     : Menu context handle.
 *
 * Create and initialize menu handle.
 *
 * Returns: menu handle on success, otherwise NULL.
 **/
void *menu_init(const void *data)
{
   menu_handle_t *menu         = NULL;
   menu_ctx_driver_t *menu_ctx = (menu_ctx_driver_t*)data;
   settings_t *settings        = config_get_ptr();
   
   if (!menu_ctx)
      return NULL;

   if (!(menu = (menu_handle_t*)menu_ctx->init(&menu_userdata)))
      return NULL;

   strlcpy(settings->menu.driver, menu_ctx->ident,
         sizeof(settings->menu.driver));

   if (!menu_entries_ctl(MENU_ENTRIES_CTL_INIT, NULL))
      goto error;

   if (!runloop_ctl(RUNLOOP_CTL_CURRENT_CORE_INIT, NULL))
      goto error;

#ifdef HAVE_SHADER_MANAGER
   menu_driver_shader = (struct video_shader*)calloc(1, sizeof(struct video_shader));
   if (!menu_driver_shader)
      goto error;
#endif

   menu->push_help_screen           = settings->menu_show_start_screen;
   menu->help_screen_type           = MENU_HELP_WELCOME;
   settings->menu_show_start_screen = false;

   if (settings->bundle_assets_extract_enable &&
         settings->bundle_assets_src_path[0] != '\0' && settings->bundle_assets_dst_path[0] != '\0' &&
         settings->bundle_assets_extract_version_current != settings->bundle_assets_extract_last_version
      )
   {
      menu->help_screen_type           = MENU_HELP_EXTRACT;
      menu->push_help_screen           = true;
      rarch_task_push_decompress(settings->bundle_assets_src_path, settings->bundle_assets_dst_path,
            settings->bundle_assets_dst_path_subdir, NULL, bundle_decompressed, NULL);
   }

   menu_shader_manager_init(menu);

   if (!menu_display_init())
      goto error;

   return menu;
   
error:
   menu_free(menu);

   return NULL;
}

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data)
{
   static struct retro_system_info menu_driver_system;
   static content_playlist_t *menu_driver_playlist = NULL;
   static bool menu_driver_prevent_populate        = false;
   static bool menu_driver_load_no_content         = false;
   static bool menu_driver_alive                   = false;
   static bool menu_driver_data_own                = false;
   const menu_ctx_driver_t                 *driver = menu_driver_ctx;

   switch (state)
   {
      case RARCH_MENU_CTL_DESTROY:
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
      case RARCH_MENU_CTL_PLAYLIST_INIT:
         {
            const char *path = (const char*)data;
            if (!path || path[0] == '\0')
               return false;
            menu_driver_playlist  = content_playlist_init(path,
                  COLLECTION_SIZE);
         }
         return true;
      case RARCH_MENU_CTL_PLAYLIST_GET:
         {
            content_playlist_t **playlist = (content_playlist_t**)data;
            if (!playlist)
               return false;
            *playlist = menu_driver_playlist;
         }
         return true;
      case RARCH_MENU_CTL_SYSTEM_INFO_GET:
         {
            struct retro_system_info **system = (struct retro_system_info**)data;
            if (!system)
               return false;
            *system = &menu_driver_system;
         }
         return true;
      case RARCH_MENU_CTL_SYSTEM_INFO_DEINIT:
#ifdef HAVE_DYNAMIC
         libretro_free_system_info(&menu_driver_system);
#endif
         memset(&menu_driver_system, 0, sizeof(struct retro_system_info));
         break;
      case RARCH_MENU_CTL_RENDER_MESSAGEBOX:
         if (driver->render_messagebox)
            driver->render_messagebox(menu_userdata, menu_driver_data->menu_state.msg);
         break;
      case RARCH_MENU_CTL_BLIT_RENDER:
         if (driver->render)
            driver->render(menu_userdata);
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
         return true;
      case RARCH_MENU_CTL_SHADER_GET:
         {
            struct video_shader **shader = (struct video_shader**)data;
            if (!shader)
               return false;
            *shader = menu_driver_shader;
         }
         return true;
      case RARCH_MENU_CTL_FRAME:
         if (!menu_driver_alive)
            return false;
         if (driver->frame)
            driver->frame(menu_userdata);
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
         menu_driver_data_own = false;
         break;
      case RARCH_MENU_CTL_SET_TEXTURE:
         if (driver->set_texture)
            driver->set_texture();
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
         break;
      case RARCH_MENU_CTL_LOAD_NO_CONTENT_GET:
         {
            bool **ptr = (bool**)data;
            if (!ptr)
               return false;
            *ptr = (bool*)&menu_driver_load_no_content;
         }
         return true;
      case RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT:
         return menu_driver_load_no_content;
      case RARCH_MENU_CTL_SET_LOAD_NO_CONTENT:
         menu_driver_load_no_content = true;
         break;
      case RARCH_MENU_CTL_UNSET_LOAD_NO_CONTENT:
         menu_driver_load_no_content = false;
         break;
      case RARCH_MENU_CTL_NAVIGATION_INCREMENT:
         if (driver->navigation_increment)
            driver->navigation_increment(menu_userdata);
         break;
      case RARCH_MENU_CTL_NAVIGATION_DECREMENT:
         if (driver->navigation_decrement)
            driver->navigation_decrement(menu_userdata);
         break;
      case RARCH_MENU_CTL_NAVIGATION_SET:
         {
            bool *scroll = (bool*)data;

            if (!scroll)
               return false;
            if (driver->navigation_set)
               driver->navigation_set(menu_userdata, *scroll);
         }
         return true;
      case RARCH_MENU_CTL_NAVIGATION_SET_LAST:
         if (driver->navigation_set_last)
            driver->navigation_set_last(menu_userdata);
         break;
      case RARCH_MENU_CTL_NAVIGATION_ASCEND_ALPHABET:
         {
            size_t *ptr_out = (size_t*)data;

            if (!ptr_out)
               return false;

            if (driver->navigation_ascend_alphabet)
               driver->navigation_ascend_alphabet(menu_userdata, ptr_out);
         }
       case RARCH_MENU_CTL_NAVIGATION_DESCEND_ALPHABET:
         {
            size_t *ptr_out = (size_t*)data;

            if (!ptr_out)
               return false;

            if (driver->navigation_descend_alphabet)
               driver->navigation_descend_alphabet(menu_userdata, ptr_out);
         }
         break;
       case RARCH_MENU_CTL_NAVIGATION_CLEAR:
         {
            bool *pending_push = (bool*)data;

            if (!pending_push)
               return false;
            if (driver->navigation_clear)
               driver->navigation_clear(menu_userdata ? menu_userdata : NULL, pending_push);
         }
         break;
       case RARCH_MENU_CTL_POPULATE_ENTRIES:
         {
            menu_displaylist_info_t *info = (menu_displaylist_info_t*)data;

            if (!info)
               return false;
            if (driver->populate_entries)
               driver->populate_entries(
                     menu_userdata, info->path, info->label, info->type);
         }
         return true;
       case RARCH_MENU_CTL_LIST_CLEAR:
         {
            file_list_t *list = (file_list_t*)data;
            if (!list)
               return false;
            if (driver->list_clear)
               driver->list_clear(list);
         }
         return true;
       case RARCH_MENU_CTL_TOGGLE:
         {
            bool *latch = (bool*)data;
            if (!latch)
               return false;

            if (driver->toggle)
               driver->toggle(menu_userdata, *latch);
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
         if (driver->context_reset)
            driver->context_reset(menu_userdata ? menu_userdata : NULL);
         break;
       case RARCH_MENU_CTL_CONTEXT_DESTROY:
         if (driver && driver->context_destroy)
            driver->context_destroy(menu_userdata ? menu_userdata : NULL);
         break;
      default:
      case RARCH_MENU_CTL_NONE:
         break;
   }

   return false;
}

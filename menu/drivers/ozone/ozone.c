/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018      - natinusala
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

#include "ozone.h"
#include "ozone_display.h"
#include "ozone_theme.h"
#include "ozone_texture.h"
#include "ozone_sidebar.h"

#if 0
#include "discord/discord.h"
#endif

#include <file/file_path.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>
#include <formats/image.h>

#include "../menu_generic.h"

#include "../../menu_driver.h"
#include "../../menu_animation.h"
#include "../../menu_input.h"

#include "../../widgets/menu_input_dialog.h"
#include "../../widgets/menu_osk.h"

#include "../../../configuration.h"
#include "../../../content.h"
#include "../../../core_info.h"
#include "../../../core.h"
#include "../../../verbosity.h"
#include "../../../tasks/task_powerstate.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../dynamic.h"

ozone_node_t *ozone_alloc_node()
{
   ozone_node_t *node = (ozone_node_t*)malloc(sizeof(*node));

   node->height         = 0;
   node->position_y     = 0;
   node->console_name   = NULL;
   node->icon           = 0;
   node->content_icon   = 0;

   return node;
}

size_t ozone_list_get_size(void *data, enum menu_list_type type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone)
      return 0;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_HORIZONTAL:
         if (ozone && ozone->horizontal_list)
            return file_list_get_size(ozone->horizontal_list);
         break;
      case MENU_LIST_TABS:
         return ozone->system_tab_end;
   }

   return 0;
}

static void ozone_free_node(ozone_node_t *node)
{
   if (!node)
      return;

   if (node->console_name)
      free(node->console_name);

   free(node);
}

void ozone_free_list_nodes(file_list_t *list, bool actiondata)
{
   unsigned i, size = (unsigned)file_list_get_size(list);

   for (i = 0; i < size; ++i)
   {
      ozone_free_node((ozone_node_t*)file_list_get_userdata_at_offset(list, i));

      /* file_list_set_userdata() doesn't accept NULL */
      list->list[i].userdata = NULL;

      if (actiondata)
         file_list_free_actiondata(list, i);
   }
}

static void *ozone_init(void **userdata, bool video_is_threaded)
{
   bool fallback_color_theme           = false;
   unsigned width, height, color_theme = 0;
   ozone_handle_t *ozone               = NULL;
   settings_t *settings                = config_get_ptr();
   menu_handle_t *menu                 = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   if (!menu_display_init_first_driver(video_is_threaded))
      goto error;

   video_driver_get_size(&width, &height);

   ozone = (ozone_handle_t*)calloc(1, sizeof(ozone_handle_t));

   if (!ozone)
      goto error;

   *userdata = ozone;

   ozone->selection_buf_old = (file_list_t*)calloc(1, sizeof(file_list_t));

   ozone->draw_sidebar              = true;
   ozone->sidebar_offset            = 0;
   ozone->pending_message           = NULL;
   ozone->is_playlist               = false;
   ozone->categories_selection_ptr  = 0;
   ozone->pending_message           = NULL;

   ozone->system_tab_end                = 0;
   ozone->tabs[ozone->system_tab_end]     = OZONE_SYSTEM_TAB_MAIN;
   if (settings->bools.menu_content_show_settings && !settings->bools.kiosk_mode_enable)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_SETTINGS;
   if (settings->bools.menu_content_show_favorites)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_FAVORITES;
   if (settings->bools.menu_content_show_history)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_HISTORY;
#ifdef HAVE_IMAGEVIEWER
   if (settings->bools.menu_content_show_images)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_IMAGES;
#endif
   if (settings->bools.menu_content_show_music)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_MUSIC;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   if (settings->bools.menu_content_show_video)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_VIDEO;
#endif
#ifdef HAVE_NETWORKING
   if (settings->bools.menu_content_show_netplay)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_NETPLAY;
#endif
#ifdef HAVE_LIBRETRODB
   if (settings->bools.menu_content_show_add && !settings->bools.kiosk_mode_enable)
      ozone->tabs[++ozone->system_tab_end] = OZONE_SYSTEM_TAB_ADD;
#endif

   menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   menu_display_set_width(width);
   menu_display_set_height(height);

   menu_display_allocate_white_texture();

   ozone->horizontal_list = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (ozone->horizontal_list)
      ozone_init_horizontal_list(ozone);

   /* Theme */
   if (settings->bools.menu_use_preferred_system_color_theme)
   {
#ifdef HAVE_LIBNX
      if (R_SUCCEEDED(setsysInitialize()))
      {
         ColorSetId theme;
         setsysGetColorSetId(&theme);
         color_theme = (theme == ColorSetId_Dark) ? 1 : 0;
         ozone_set_color_theme(ozone, color_theme);
         settings->uints.menu_ozone_color_theme = color_theme;
         settings->bools.menu_preferred_system_color_theme_set = true;
         setsysExit();
      }
      else
#endif
         fallback_color_theme = true;
   }
   else
      fallback_color_theme = true;

   if (fallback_color_theme)
   {
      color_theme = settings->uints.menu_ozone_color_theme;
      ozone_set_color_theme(ozone, color_theme);
   }

   ozone->need_compute                 = false;
   ozone->animations.scroll_y          = 0.0f;
   ozone->animations.scroll_y_sidebar  = 0.0f;

   /* Assets path */
   fill_pathname_join(
      ozone->assets_path,
      settings->paths.directory_assets,
      "ozone",
      sizeof(ozone->assets_path)
   );

   /* PNG path */
   fill_pathname_join(
      ozone->png_path,
      ozone->assets_path,
      "png",
      sizeof(ozone->png_path)
   );

   /* Icons path */
   fill_pathname_join(
      ozone->icons_path,
      ozone->png_path,
      "icons",
      sizeof(ozone->icons_path)
   );

   /* Sidebar path */
   fill_pathname_join(
      ozone->tab_path,
      ozone->png_path,
      "sidebar",
      sizeof(ozone->tab_path)
   );

   last_use_preferred_system_color_theme = settings->bools.menu_use_preferred_system_color_theme;

   return menu;

error:
   if (ozone->horizontal_list)
   {
      ozone_free_list_nodes(ozone->horizontal_list, false);
      file_list_free(ozone->horizontal_list);
   }
   ozone->horizontal_list = NULL;

   if (menu)
      free(menu);

   return NULL;
}

static void ozone_free(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (ozone)
   {
      video_coord_array_free(&ozone->raster_blocks.footer.carr);
      video_coord_array_free(&ozone->raster_blocks.title.carr);
      video_coord_array_free(&ozone->raster_blocks.time.carr);
      video_coord_array_free(&ozone->raster_blocks.entries_label.carr);
      video_coord_array_free(&ozone->raster_blocks.entries_sublabel.carr);
      video_coord_array_free(&ozone->raster_blocks.sidebar.carr);

      font_driver_bind_block(NULL, NULL);

      if (ozone->selection_buf_old)
      {
         ozone_free_list_nodes(ozone->selection_buf_old, false);
         file_list_free(ozone->selection_buf_old);
      }

      if (ozone->horizontal_list)
      {
         ozone_free_list_nodes(ozone->horizontal_list, false);
         file_list_free(ozone->horizontal_list);
      }

      if (!string_is_empty(ozone->pending_message))
         free(ozone->pending_message);
   }
}

static void ozone_context_reset(void *data, bool is_threaded)
{
   /* Fonts init */
   unsigned i;
   unsigned size;
   char font_path[PATH_MAX_LENGTH];

   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (ozone)
   {
      ozone->has_all_assets = true;

      fill_pathname_join(font_path, ozone->assets_path, "regular.ttf", sizeof(font_path));
      ozone->fonts.footer = menu_display_font_file(font_path, FONT_SIZE_FOOTER, is_threaded);
      ozone->fonts.entries_label = menu_display_font_file(font_path, FONT_SIZE_ENTRIES_LABEL, is_threaded);
      ozone->fonts.entries_sublabel = menu_display_font_file(font_path, FONT_SIZE_ENTRIES_SUBLABEL, is_threaded);
      ozone->fonts.time = menu_display_font_file(font_path, FONT_SIZE_TIME, is_threaded);
      ozone->fonts.sidebar = menu_display_font_file(font_path, FONT_SIZE_SIDEBAR, is_threaded);

      fill_pathname_join(font_path, ozone->assets_path, "bold.ttf", sizeof(font_path));
      ozone->fonts.title = menu_display_font_file(font_path, FONT_SIZE_TITLE, is_threaded);

      if (
         !ozone->fonts.footer           ||
         !ozone->fonts.entries_label    ||
         !ozone->fonts.entries_sublabel ||
         !ozone->fonts.time             ||
         !ozone->fonts.sidebar          ||
         !ozone->fonts.title
      )
      {
         ozone->has_all_assets = false;
      }

      /* Naive font size */
      ozone->title_font_glyph_width = FONT_SIZE_TITLE * 3/4;
      ozone->entry_font_glyph_width = FONT_SIZE_ENTRIES_LABEL * 3/4;
      ozone->sublabel_font_glyph_width = FONT_SIZE_ENTRIES_SUBLABEL * 3/4;

      /* More realistic font size */
      size = font_driver_get_message_width(ozone->fonts.title, "a", 1, 1);
      if (size)
         ozone->title_font_glyph_width = size;
      size = font_driver_get_message_width(ozone->fonts.entries_label, "a", 1, 1);
      if (size)
         ozone->entry_font_glyph_width = size;
      size = font_driver_get_message_width(ozone->fonts.entries_sublabel, "a", 1, 1);
      if (size)
         ozone->sublabel_font_glyph_width = size;

      /* Textures init */
      for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
#if 0
         if (i == OZONE_TEXTURE_DISCORD_OWN_AVATAR && discord_avatar_is_ready())
            strlcpy(filename, discord_get_own_avatar(), sizeof(filename));
         else
#endif
            strlcpy(filename, OZONE_TEXTURES_FILES[i], sizeof(filename));

         strlcat(filename, ".png", sizeof(filename));

#if 0
         if (i == OZONE_TEXTURE_DISCORD_OWN_AVATAR && discord_avatar_is_ready())
         {
            char buf[PATH_MAX_LENGTH];
            fill_pathname_application_special(buf,
               sizeof(buf),
               APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS);
            if (!menu_display_reset_textures_list(filename, buf, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR))
               RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", ozone->png_path, path_default_slash(), filename);
         }
         else
         {
#endif
            if (!menu_display_reset_textures_list(filename, ozone->png_path, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR))
            {
               ozone->has_all_assets = false;
               RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", ozone->png_path, path_default_slash(), filename);
            }
#if 0
         }
#endif
      }

      /* Sidebar textures */
      for (i = 0; i < OZONE_TAB_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
         strlcpy(filename, OZONE_TAB_TEXTURES_FILES[i], sizeof(filename));
         strlcat(filename, ".png", sizeof(filename));

         if (!menu_display_reset_textures_list(filename, ozone->tab_path, &ozone->tab_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR))
         {
            ozone->has_all_assets = false;
            RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", ozone->tab_path, path_default_slash(), filename);
         }
      }

      /* Theme textures */
      if (!ozone_reset_theme_textures(ozone))
         ozone->has_all_assets = false;

      /* Icons textures init */
      for (i = 0; i < OZONE_ENTRIES_ICONS_TEXTURE_LAST; i++)
         if (!menu_display_reset_textures_list(ozone_entries_icon_texture_path(i), ozone->icons_path, &ozone->icons_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR))
         {
            ozone->has_all_assets = false;
            RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", ozone->icons_path, path_default_slash(), ozone_entries_icon_texture_path(i));
         }

      menu_display_allocate_white_texture();

      /* Horizontal list */
      ozone_context_reset_horizontal_list(ozone);

      /* State reset */
      ozone->frame_count                  = 0;
      ozone->fade_direction               = false;
      ozone->cursor_in_sidebar            = false;
      ozone->cursor_in_sidebar_old        = false;
      ozone->draw_old_list                = false;
      ozone->messagebox_state             = false;
      ozone->messagebox_state_old         = false;

      /* Animations */
      ozone->animations.cursor_alpha   = 1.0f;
      ozone->animations.scroll_y       = 0.0f;
      ozone->animations.list_alpha     = 1.0f;

      /* Missing assets message */
      if (!ozone->has_all_assets)
      {
         RARCH_WARN("[OZONE] Assets missing\n");
         runloop_msg_queue_push(msg_hash_to_str(MSG_MISSING_ASSETS), 1, 256, false);
      }
      ozone_restart_cursor_animation(ozone);
   }
}

static void ozone_collapse_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->draw_sidebar = false;
}

static void ozone_context_destroy(void *data)
{
   unsigned i;
   ozone_handle_t *ozone = (ozone_handle_t*) data;
   menu_animation_ctx_tag tag;

   if (!ozone)
      return;

   /* Theme */
   ozone_unload_theme_textures(ozone);

   /* Icons */
   for (i = 0; i < OZONE_ENTRIES_ICONS_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->icons_textures[i]);

   /* Textures */
   for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->textures[i]);

   /* Icons */
   for (i = 0; i < OZONE_TAB_TEXTURE_LAST; i++)
      video_driver_texture_unload(&ozone->tab_textures[i]);

   video_driver_texture_unload(&menu_display_white_texture);

   menu_display_font_free(ozone->fonts.footer);
   menu_display_font_free(ozone->fonts.title);
   menu_display_font_free(ozone->fonts.time);
   menu_display_font_free(ozone->fonts.entries_label);
   menu_display_font_free(ozone->fonts.entries_sublabel);
   menu_display_font_free(ozone->fonts.sidebar);

   ozone->fonts.footer = NULL;
   ozone->fonts.title = NULL;
   ozone->fonts.time = NULL;
   ozone->fonts.entries_label = NULL;
   ozone->fonts.entries_sublabel = NULL;
   ozone->fonts.sidebar = NULL;

   tag = (uintptr_t) &ozone_default_theme;
   menu_animation_kill_by_tag(&tag);

   /* Horizontal list */
   ozone_context_destroy_horizontal_list(ozone);
}

static void *ozone_list_get_entry(void *data,
      enum menu_list_type type, unsigned i)
{
   size_t list_size        = 0;
   ozone_handle_t* ozone   = (ozone_handle_t*) data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         {
            file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
            list_size  = menu_entries_get_stack_size(0);
            if (i < list_size)
               return (void*)&menu_stack->list[i];
         }
         break;
      case MENU_LIST_HORIZONTAL:
         if (ozone && ozone->horizontal_list)
            list_size = file_list_get_size(ozone->horizontal_list);
         if (i < list_size)
            return (void*)&ozone->horizontal_list->list[i];
         break;
      default:
         break;
   }

   return NULL;
}

static int ozone_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   menu_displaylist_ctx_parse_entry_t entry;
   int ret                = -1;
   core_info_list_t *list = NULL;
   menu_handle_t *menu    = (menu_handle_t*)data;
   const struct retro_subsystem_info* subsystem;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         {
            settings_t *settings = config_get_ptr();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION, 0, 0);

            core_info_get_list(&list);
            if (core_info_list_num_info_files(list))
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                     MENU_SETTING_ACTION, 0, 0);
            }

#ifdef HAVE_LIBRETRODB
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST),
                  MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST,
                  MENU_SETTING_ACTION, 0, 0);
#endif

            if (frontend_driver_parse_drive_list(info->list, true) != 0)
               menu_entries_append_enum(info->list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0);

            if (!settings->bools.kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                     MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                     MENU_SETTING_ACTION, 0, 0);
            }

            info->need_push    = true;
            info->need_refresh = true;
            ret = 0;
         }
         break;
      case DISPLAYLIST_MAIN_MENU:
         {
            settings_t   *settings      = config_get_ptr();
            rarch_system_info_t *system = runloop_get_system_info();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            entry.data            = menu;
            entry.info            = info;
            entry.parse_type      = PARSE_ACTION;
            entry.add_empty_entry = false;

            if (!string_is_empty(system->info.library_name) &&
                  !string_is_equal(system->info.library_name,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
               menu_displaylist_setting(&entry);
            }

            if (system->load_no_content)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_START_CORE;
               menu_displaylist_setting(&entry);
            }

#ifndef HAVE_DYNAMIC
            if (frontend_driver_has_fork())
#endif
            {
               if (settings->bools.menu_show_load_core)
               {
                  entry.enum_idx   = MENU_ENUM_LABEL_CORE_LIST;
                  menu_displaylist_setting(&entry);
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_LIST;
               menu_displaylist_setting(&entry);

               /* Core fully loaded, use the subsystem data */
               if (system->subsystem.data)
                     subsystem = system->subsystem.data;
               /* Core not loaded completely, use the data we peeked on load core */
               else
                  subsystem = subsystem_data;

               menu_subsystem_populate(subsystem, info);
            }

            entry.enum_idx      = MENU_ENUM_LABEL_ADD_CONTENT_LIST;
            menu_displaylist_setting(&entry);
#ifdef HAVE_QT
            if (settings->bools.desktop_menu_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_SHOW_WIMP;
               menu_displaylist_setting(&entry);
            }
#endif
#if defined(HAVE_NETWORKING)
            if (settings->bools.menu_show_online_updater && !settings->bools.kiosk_mode_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_ONLINE_UPDATER;
               menu_displaylist_setting(&entry);
            }
#endif
            if (!settings->bools.menu_content_show_settings && !string_is_empty(settings->paths.menu_content_show_settings_password))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.kiosk_mode_enable && !string_is_empty(settings->paths.kiosk_mode_password))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_information)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_INFORMATION_LIST;
               menu_displaylist_setting(&entry);
            }

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX) 
            entry.enum_idx      = MENU_ENUM_LABEL_SWITCH_CPU_PROFILE;
            menu_displaylist_setting(&entry);
#endif

#ifdef HAVE_LAKKA_SWITCH
            entry.enum_idx      = MENU_ENUM_LABEL_SWITCH_GPU_PROFILE;
            menu_displaylist_setting(&entry);

            entry.enum_idx      = MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL;
            menu_displaylist_setting(&entry);
#endif

#ifndef HAVE_DYNAMIC
            entry.enum_idx      = MENU_ENUM_LABEL_RESTART_RETROARCH;
            menu_displaylist_setting(&entry);
#endif

            if (settings->bools.menu_show_configurations && !settings->bools.kiosk_mode_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_CONFIGURATIONS_LIST;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_help)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_HELP_LIST;
               menu_displaylist_setting(&entry);
            }

#if !defined(IOS)
            if (settings->bools.menu_show_quit_retroarch)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_QUIT_RETROARCH;
               menu_displaylist_setting(&entry);
            }
#endif

            if (settings->bools.menu_show_reboot)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_REBOOT;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_shutdown)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_SHUTDOWN;
               menu_displaylist_setting(&entry);
            }

            info->need_push    = true;
            ret = 0;
         }
         break;
   }
   return ret;
}

static size_t ozone_list_get_selection(void *data)
{
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return 0;

   return ozone->categories_selection_ptr;
}

static void ozone_list_clear(file_list_t *list)
{
   menu_animation_ctx_tag tag = (uintptr_t)list;
   menu_animation_kill_by_tag(&tag);

   ozone_free_list_nodes(list, false);
}

static void ozone_list_free(file_list_t *list, size_t a, size_t b)
{
   ozone_list_clear(list);
}

/* Compute new scroll position
 * If the center of the currently selected entry is not in the middle
 * And if we can scroll so that it's in the middle
 * Then scroll
 */
static void ozone_update_scroll(ozone_handle_t *ozone, bool allow_animation, ozone_node_t *node)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   menu_animation_ctx_tag tag = (uintptr_t) selection_buf;
   menu_animation_ctx_entry_t entry;
   float new_scroll = 0, entries_middle;
   float bottom_boundary, current_selection_middle_onscreen;
   unsigned video_info_height;

   video_driver_get_size(NULL, &video_info_height);

   current_selection_middle_onscreen    = ENTRIES_START_Y + ozone->animations.scroll_y + node->position_y + node->height / 2;
   bottom_boundary                      = video_info_height - 87 - 78;
   entries_middle                       = video_info_height/2;

   new_scroll = ozone->animations.scroll_y - (current_selection_middle_onscreen - entries_middle);

   if (new_scroll + ozone->entries_height < bottom_boundary)
      new_scroll = -(78 + ozone->entries_height - bottom_boundary);

   if (new_scroll > 0)
      new_scroll = 0;

   if (allow_animation)
   {
      /* Cursor animation */
      ozone->animations.cursor_alpha = 0.0f;

      entry.cb = NULL;
      entry.duration = ANIMATION_CURSOR_DURATION;
      entry.easing_enum = EASING_OUT_QUAD;
      entry.subject = &ozone->animations.cursor_alpha;
      entry.tag = tag;
      entry.target_value = 1.0f;
      entry.userdata = NULL;

      menu_animation_push(&entry);

      /* Scroll animation */
      entry.cb = NULL;
      entry.duration = ANIMATION_CURSOR_DURATION;
      entry.easing_enum = EASING_OUT_QUAD;
      entry.subject = &ozone->animations.scroll_y;
      entry.tag = tag;
      entry.target_value = new_scroll;
      entry.userdata = NULL;

      menu_animation_push(&entry);
   }
   else
   {
      ozone->selection_old = ozone->selection;
      ozone->animations.scroll_y = new_scroll;
   }
}

static unsigned ozone_count_lines(const char *str)
{
   unsigned c     = 0;
   unsigned lines = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

static void ozone_compute_entries_position(ozone_handle_t *ozone)
{
   /* Compute entries height and adjust scrolling if needed */
   unsigned video_info_height;
   unsigned video_info_width;
   unsigned lines;
   size_t i, entries_end;
   file_list_t *selection_buf = NULL;

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   entries_end   = menu_entries_get_size();
   selection_buf = menu_entries_get_selection_buf_ptr(0);

   video_driver_get_size(&video_info_width, &video_info_height);

   ozone->entries_height = 0;

   for (i = 0; i < entries_end; i++)
   {
      /* Entry */
      menu_entry_t entry;
      ozone_node_t *node     = NULL;

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

      /* Empty playlist detection:
         only one item which icon is
         OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO */
      if (ozone->is_playlist && entries_end == 1)
      {
         menu_texture_item tex = ozone_entries_icon_get_texture(ozone, entry.enum_idx, entry.type, false);
         ozone->empty_playlist = tex == ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO];
      }
      else
      {
         ozone->empty_playlist = false;
      }

      /* Cache node */
      node = (ozone_node_t*)file_list_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      node->height = (entry.sublabel ? 100 : 60-8);
      node->wrap   = false;

      if (entry.sublabel)
      {
         char *sublabel_str = menu_entry_get_sublabel(&entry);

         word_wrap(sublabel_str, sublabel_str, (video_info_width - 548) / ozone->sublabel_font_glyph_width, false);

         lines = ozone_count_lines(sublabel_str);

         if (lines > 1)
         {
            node->height += lines * 15;
            node->wrap = true;
         }

         free(sublabel_str);
      }

      node->position_y = ozone->entries_height;

      ozone->entries_height += node->height;

      menu_entry_free(&entry);
   }

   /* Update scrolling */
   ozone->selection = menu_navigation_get_selection();
   ozone_update_scroll(ozone, false, (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, ozone->selection));
}

static void ozone_render(void *data, bool is_idle)
{
   size_t i;
   menu_animation_ctx_delta_t delta;
   unsigned end                     = (unsigned)menu_entries_get_size();
   ozone_handle_t *ozone            = (ozone_handle_t*)data;
   if (!data)
      return;

   if (ozone->need_compute)
   {
      ozone_compute_entries_position(ozone);
      ozone->need_compute = false;
   }

   ozone->selection = menu_navigation_get_selection();

   delta.current = menu_animation_get_delta_time();

   if (menu_animation_get_ideal_delta_time(&delta))
      menu_animation_update(delta.ideal);

   /* TODO Handle pointer & mouse */

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (i >= end)
   {
      i = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   }

   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);
}

static void ozone_draw_header(ozone_handle_t *ozone, video_frame_info_t *video_info)
{
   char title[255];
   menu_animation_ctx_ticker_t ticker;
   settings_t *settings     = config_get_ptr();
   unsigned timedate_offset = 0;

   /* Separator */
   menu_display_draw_quad(video_info, 30, 87, video_info->width - 60, 1, video_info->width, video_info->height, ozone->theme->header_footer_separator);

   /* Title */
   ticker.s = title;
   ticker.len = (video_info->width - 128 - 47 - 130) / ozone->title_font_glyph_width;
   ticker.idx = ozone->frame_count / 20;
   ticker.str = ozone->title;
   ticker.selected = true;

   menu_animation_ticker(&ticker);

   ozone_draw_text(video_info, ozone, title, 128, 20 + FONT_SIZE_TITLE, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.title, ozone->theme->text_rgba, false);

   /* Icon */
   menu_display_blend_begin(video_info);
#if 0
   if (discord_avatar_is_ready())
      ozone_draw_icon(video_info, 60, 60, ozone->textures[OZONE_TEXTURE_DISCORD_OWN_AVATAR], 47, 14, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
   else
#endif
      ozone_draw_icon(video_info, 60, 60, ozone->textures[OZONE_TEXTURE_RETROARCH], 47, 14, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
   menu_display_blend_end(video_info);

   /* Battery */
   if (video_info->battery_level_enable)
   {
      char msg[12];
      static retro_time_t last_time  = 0;
      bool charging                  = false;
      retro_time_t current_time      = cpu_features_get_time_usec();
      int percent                    = 0;
      enum frontend_powerstate state = get_last_powerstate(&percent);

      if (state == FRONTEND_POWERSTATE_CHARGING)
         charging = true;

      if (current_time - last_time >= INTERVAL_BATTERY_LEVEL_CHECK)
      {
         last_time = current_time;
         task_push_get_powerstate();
      }

      *msg = '\0';

      if (percent > 0)
      {
         timedate_offset = 95;

         snprintf(msg, sizeof(msg), "%d%%", percent);

         ozone_draw_text(video_info, ozone, msg, video_info->width - 85, 30 + FONT_SIZE_TIME, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.time, ozone->theme->text_rgba, false);

         menu_display_blend_begin(video_info);
         ozone_draw_icon(video_info, 92, 92, ozone->icons_textures[charging ? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING : OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL], video_info->width - 60 - 56, 30 - 28, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
         menu_display_blend_end(video_info);
      }
   }

   /* Timedate */
   if (video_info->timedate_enable)
   {
      menu_display_ctx_datetime_t datetime;
      char timedate[255];

      timedate[0] = '\0';

      datetime.s = timedate;
      datetime.time_mode = settings->uints.menu_timedate_style;
      datetime.len = sizeof(timedate);

      menu_display_timedate(&datetime);

      ozone_draw_text(video_info, ozone, timedate, video_info->width - 87 - timedate_offset, 30 + FONT_SIZE_TIME, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.time, ozone->theme->text_rgba, false);

      menu_display_blend_begin(video_info);
      ozone_draw_icon(video_info, 92, 92, ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOCK], video_info->width - 60 - 56 - timedate_offset, 30 - 28, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
      menu_display_blend_end(video_info);
   }
}

static void ozone_draw_footer(ozone_handle_t *ozone, video_frame_info_t *video_info, settings_t *settings)
{
   char core_title[255];
   /* Separator */
   menu_display_draw_quad(video_info, 23, video_info->height - 78, video_info->width - 60, 1, video_info->width, video_info->height, ozone->theme->header_footer_separator);

   /* Core title or Switch icon */
   if (settings->bools.menu_core_enable && menu_entries_get_core_title(core_title, sizeof(core_title)) == 0)
      ozone_draw_text(video_info, ozone, core_title, 59, video_info->height - 49 + FONT_SIZE_FOOTER, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);
   else
      ozone_draw_icon(video_info, 69, 30, ozone->theme->textures[OZONE_THEME_TEXTURE_SWITCH], 59, video_info->height - 52, video_info->width,video_info->height, 0, 1, NULL);

   /* Buttons */

   {
      unsigned back_width  = 215;
      unsigned back_height = 49;
      unsigned ok_width    = 96;
      unsigned ok_height   = 49;
      bool do_swap         = video_info->input_menu_swap_ok_cancel_buttons;

      if (do_swap)
      {
         back_width  = 96;
         back_height = 49;
         ok_width    = 215;
         ok_height   = 49;
      }

      menu_display_blend_begin(video_info);

      if (do_swap)
      {
         ozone_draw_icon(video_info, 25, 25, ozone->theme->textures[OZONE_THEME_TEXTURE_BUTTON_B], video_info->width - 133, video_info->height - 49, video_info->width,video_info->height, 0, 1, NULL);
         ozone_draw_icon(video_info, 25, 25, ozone->theme->textures[OZONE_THEME_TEXTURE_BUTTON_A], video_info->width - 251, video_info->height - 49, video_info->width,video_info->height, 0, 1, NULL);
      }
      else
      {
         ozone_draw_icon(video_info, 25, 25, ozone->theme->textures[OZONE_THEME_TEXTURE_BUTTON_B], video_info->width - 251, video_info->height - 49, video_info->width,video_info->height, 0, 1, NULL);
         ozone_draw_icon(video_info, 25, 25, ozone->theme->textures[OZONE_THEME_TEXTURE_BUTTON_A], video_info->width - 133, video_info->height - 49, video_info->width,video_info->height, 0, 1, NULL);
      }

      menu_display_blend_end(video_info);

      ozone_draw_text(video_info, ozone,
            do_swap ?
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK) :
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK),
            video_info->width - back_width, video_info->height - back_height + FONT_SIZE_FOOTER, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);
      ozone_draw_text(video_info, ozone,
            do_swap ?
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK) :
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK),
            video_info->width - ok_width, video_info->height - ok_height + FONT_SIZE_FOOTER, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);
   }

   menu_display_blend_end(video_info);
}

static void ozone_selection_changed(ozone_handle_t *ozone, bool allow_animation)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   menu_animation_ctx_tag tag = (uintptr_t) selection_buf;

   size_t new_selection = menu_navigation_get_selection();
   ozone_node_t *node = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, new_selection);

   if (!node)
      return;

   if (ozone->selection != new_selection)
   {
      ozone->selection_old = ozone->selection;
      ozone->selection = new_selection;

      ozone->cursor_in_sidebar_old = ozone->cursor_in_sidebar;

      menu_animation_kill_by_tag(&tag);

      ozone_update_scroll(ozone, allow_animation, node);
   }
}

static void ozone_navigation_clear(void *data, bool pending_push)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!pending_push)
      ozone_selection_changed(ozone, true);
}

static void ozone_navigation_pointer_changed(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_navigation_set(void *data, bool scroll)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_navigation_alphabet(void *data, size_t *unused)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   ozone_selection_changed(ozone, true);
}

static void ozone_messagebox_fadeout_cb(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   free(ozone->pending_message);
   ozone->pending_message = NULL;

   ozone->should_draw_messagebox = false;
}

static void ozone_frame(void *data, video_frame_info_t *video_info)
{
   ozone_handle_t* ozone                  = (ozone_handle_t*) data;
   settings_t  *settings                  = config_get_ptr();
   unsigned color_theme                   = video_info->ozone_color_theme;
   menu_animation_ctx_tag messagebox_tag  = (uintptr_t)ozone->pending_message;
   bool draw_osk                          = menu_input_dialog_get_display_kb();
   static bool draw_osk_old               = false;

#if 0
   static bool reset                      = false;

   if (discord_avatar_is_ready() && !reset)
   {
      ozone_context_reset(data, false);
      reset = true;
   }
#endif

   menu_animation_ctx_entry_t entry;

   if (!ozone)
      return;

   /* OSK Fade detection */
   if (draw_osk != draw_osk_old)
   {
      draw_osk_old = draw_osk;
      if (!draw_osk)
      {
         ozone->should_draw_messagebox       = false;
         ozone->messagebox_state             = false;
         ozone->messagebox_state_old         = false;
         ozone->animations.messagebox_alpha  = 0.0f;
      }
   }

   /* Change theme on the fly */
   if (color_theme != last_color_theme || last_use_preferred_system_color_theme != settings->bools.menu_use_preferred_system_color_theme)
   {
      if (!settings->bools.menu_use_preferred_system_color_theme)
         ozone_set_color_theme(ozone, color_theme);
      else
      {
         video_info->ozone_color_theme = ozone_get_system_theme();
         ozone_set_color_theme(ozone, video_info->ozone_color_theme);
      }

      last_use_preferred_system_color_theme = settings->bools.menu_use_preferred_system_color_theme;
   }

   ozone->frame_count++;

   menu_display_set_viewport(video_info->width, video_info->height);

   /* Clear text */
   font_driver_bind_block(ozone->fonts.footer,  &ozone->raster_blocks.footer);
   font_driver_bind_block(ozone->fonts.title,  &ozone->raster_blocks.title);
   font_driver_bind_block(ozone->fonts.time,  &ozone->raster_blocks.time);
   font_driver_bind_block(ozone->fonts.entries_label,  &ozone->raster_blocks.entries_label);
   font_driver_bind_block(ozone->fonts.entries_sublabel,  &ozone->raster_blocks.entries_sublabel);
   font_driver_bind_block(ozone->fonts.sidebar,  &ozone->raster_blocks.sidebar);

   ozone->raster_blocks.footer.carr.coords.vertices = 0;
   ozone->raster_blocks.title.carr.coords.vertices = 0;
   ozone->raster_blocks.time.carr.coords.vertices = 0;
   ozone->raster_blocks.entries_label.carr.coords.vertices = 0;
   ozone->raster_blocks.entries_sublabel.carr.coords.vertices = 0;
   ozone->raster_blocks.sidebar.carr.coords.vertices = 0;

   /* Background */
   menu_display_draw_quad(video_info,
      0, 0, video_info->width, video_info->height,
      video_info->width, video_info->height,
      !video_info->libretro_running ? ozone->theme->background : ozone->theme->background_libretro_running
   );

   /* Header, footer */
   ozone_draw_header(ozone, video_info);
   ozone_draw_footer(ozone, video_info, settings);

   /* Sidebar */
   ozone_draw_sidebar(ozone, video_info);

   /* Menu entries */
   menu_display_scissor_begin(video_info, ozone->sidebar_offset + 408, 87, video_info->width - 408 + (-ozone->sidebar_offset), video_info->height - 87 - 78);

   /* Current list */
   ozone_draw_entries(ozone,
      video_info,
      ozone->selection,
      ozone->selection_old,
      menu_entries_get_selection_buf_ptr(0),
      ozone->animations.list_alpha,
      ozone->animations.scroll_y,
      ozone->is_playlist
   );

   /* Old list */
   if (ozone->draw_old_list)
      ozone_draw_entries(ozone,
         video_info,
         ozone->selection_old_list,
         ozone->selection_old_list,
         ozone->selection_buf_old,
         ozone->animations.list_alpha,
         ozone->scroll_old,
         ozone->is_playlist_old
      );

   menu_display_scissor_end(video_info);

   /* Flush first layer of text */
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.footer, video_info);
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.title, video_info);
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.time, video_info);

   font_driver_bind_block(ozone->fonts.footer, NULL);
   font_driver_bind_block(ozone->fonts.title, NULL);
   font_driver_bind_block(ozone->fonts.time, NULL);
   font_driver_bind_block(ozone->fonts.entries_label, NULL);

   /* Message box & OSK - second layer of text */
   ozone->raster_blocks.footer.carr.coords.vertices = 0;
   ozone->raster_blocks.entries_label.carr.coords.vertices = 0;

   if (ozone->should_draw_messagebox || draw_osk)
   {
      /* Fade in animation */
      if (ozone->messagebox_state_old != ozone->messagebox_state && ozone->messagebox_state)
      {
         ozone->messagebox_state_old = ozone->messagebox_state;

         menu_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 0.0f;

         entry.cb = NULL;
         entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum = EASING_OUT_QUAD;
         entry.subject = &ozone->animations.messagebox_alpha;
         entry.tag = messagebox_tag;
         entry.target_value = 1.0f;
         entry.userdata = NULL;

         menu_animation_push(&entry);
      }
      /* Fade out animation */
      else if (ozone->messagebox_state_old != ozone->messagebox_state && !ozone->messagebox_state)
      {
         ozone->messagebox_state_old = ozone->messagebox_state;
         ozone->messagebox_state = false;

         menu_animation_kill_by_tag(&messagebox_tag);
         ozone->animations.messagebox_alpha = 1.0f;

         entry.cb = ozone_messagebox_fadeout_cb;
         entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
         entry.easing_enum = EASING_OUT_QUAD;
         entry.subject = &ozone->animations.messagebox_alpha;
         entry.tag = messagebox_tag;
         entry.target_value = 0.0f;
         entry.userdata = ozone;

         menu_animation_push(&entry);
      }

      ozone_draw_backdrop(video_info, fmin(ozone->animations.messagebox_alpha, 0.75f));

      if (draw_osk)
      {
         const char *label = menu_input_dialog_get_label_buffer();
         const char *str   = menu_input_dialog_get_buffer();

         ozone_draw_osk(ozone, video_info, label, str);
      }
      else
      {
         ozone_draw_messagebox(ozone, video_info, ozone->pending_message);
      }
   }

   font_driver_flush(video_info->width, video_info->height, ozone->fonts.footer, video_info);
   font_driver_flush(video_info->width, video_info->height, ozone->fonts.entries_label, video_info);

   menu_display_unset_viewport(video_info->width, video_info->height);
}

static void ozone_set_header(ozone_handle_t *ozone)
{
   if (ozone->categories_selection_ptr <= ozone->system_tab_end)
   {
      menu_entries_get_title(ozone->title, sizeof(ozone->title));
   }
   else if (ozone->horizontal_list)
   {
      ozone_node_t *node = (ozone_node_t*) file_list_get_userdata_at_offset(ozone->horizontal_list, ozone->categories_selection_ptr - ozone->system_tab_end-1);

      if (node && node->console_name)
         strlcpy(ozone->title, node->console_name, sizeof(ozone->title));
   }
}

static void ozone_animation_end(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone->draw_old_list = false;
}

static void ozone_list_open(ozone_handle_t *ozone)
{
   struct menu_animation_ctx_entry entry;

   ozone->draw_old_list = true;

   /* Left/right animation */
   ozone->animations.list_alpha = 0.0f;

   entry.cb = ozone_animation_end;
   entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
   entry.easing_enum = EASING_OUT_QUAD;
   entry.subject = &ozone->animations.list_alpha;
   entry.tag = (uintptr_t) NULL;
   entry.target_value = 1.0f;
   entry.userdata = ozone;

   menu_animation_push(&entry);

   /* Sidebar animation */
   if (ozone->depth == 1)
   {
      ozone->draw_sidebar = true;

      entry.cb = NULL;
      entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
      entry.easing_enum = EASING_OUT_QUAD;
      entry.subject = &ozone->sidebar_offset;
      entry.tag = (uintptr_t) NULL;
      entry.target_value = 0.0f;
      entry.userdata = NULL;

      menu_animation_push(&entry);
   }
   else if (ozone->depth > 1)
   {
      struct menu_animation_ctx_entry entry;

      entry.cb = ozone_collapse_end;
      entry.duration = ANIMATION_PUSH_ENTRY_DURATION;
      entry.easing_enum = EASING_OUT_QUAD;
      entry.subject = &ozone->sidebar_offset;
      entry.tag = (uintptr_t) NULL;
      entry.target_value = -408.0f;
      entry.userdata = (void*) ozone;

      menu_animation_push(&entry);
   }
}

static void ozone_populate_entries(void *data, const char *path, const char *label, unsigned k)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   int new_depth;

   if (!ozone)
      return;

   ozone_set_header(ozone);

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

      /* TODO Update thumbnails */
      ozone_selection_changed(ozone, false);
      return;
   }

   ozone->need_compute = true;

   new_depth = (int)ozone_list_get_size(ozone, MENU_LIST_PLAIN);

   ozone->fade_direction   = new_depth <= ozone->depth;
   ozone->depth            = new_depth;
   ozone->is_playlist      = ozone_is_playlist(ozone);

   if (ozone->categories_selection_ptr == ozone->categories_active_idx_old)
   {
      ozone_list_open(ozone);
   }
}

static int ozone_menu_iterate(menu_handle_t *menu, void *userdata, enum menu_action action)
{
   int new_selection;
   enum menu_action new_action;
   menu_animation_ctx_tag tag;

   file_list_t *selection_buf    = NULL;
   ozone_handle_t *ozone         = (ozone_handle_t*) userdata;
   unsigned horizontal_list_size = 0;

   if (ozone->horizontal_list)
      horizontal_list_size = ozone->horizontal_list->size;

   ozone->messagebox_state = false || menu_input_dialog_get_display_kb();

   if (!ozone)
      return generic_menu_iterate(menu, userdata, action);

   selection_buf              = menu_entries_get_selection_buf_ptr(0);
   tag                        = (uintptr_t)selection_buf;
   new_action                 = action;

   /* Inputs override */
   switch (action)
   {
      case MENU_ACTION_DOWN:
         if (!ozone->cursor_in_sidebar)
            break;

         tag = (uintptr_t)ozone;

         new_selection = (ozone->categories_selection_ptr + 1);

         if (new_selection >= ozone->system_tab_end + horizontal_list_size + 1)
            new_selection = 0;

         ozone_sidebar_goto(ozone, new_selection);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_UP:
         if (!ozone->cursor_in_sidebar)
            break;

         tag = (uintptr_t)ozone;

         new_selection = ozone->categories_selection_ptr - 1;

         if (new_selection < 0)
            new_selection = horizontal_list_size + ozone->system_tab_end;

         ozone_sidebar_goto(ozone, new_selection);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_LEFT:
         if (ozone->cursor_in_sidebar)
         {
            new_action = MENU_ACTION_NOOP;
            break;
         }
         else if (ozone->depth > 1)
            break;

         ozone_go_to_sidebar(ozone, tag);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_RIGHT:
         if (!ozone->cursor_in_sidebar)
         {
            if (ozone->depth == 1)
               new_action = MENU_ACTION_NOOP;
            break;
         }

         ozone_leave_sidebar(ozone, tag);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_OK:
         if (ozone->cursor_in_sidebar)
         {
            ozone_leave_sidebar(ozone, tag);
            new_action = MENU_ACTION_NOOP;
            break;
         }

         break;
      case MENU_ACTION_CANCEL:
         if (ozone->cursor_in_sidebar)
         {
            /* Go back to main menu tab */
            if (ozone->categories_selection_ptr != 0)
               ozone_sidebar_goto(ozone, 0);

            new_action = MENU_ACTION_NOOP;
            break;
         }

         if (menu_entries_get_stack_size(0) == 1)
         {
            ozone_go_to_sidebar(ozone, tag);
            new_action = MENU_ACTION_NOOP;
         }
         break;
      default:
         break;
   }

   return generic_menu_iterate(menu, userdata, new_action);
}

/* TODO Fancy toggle animation */

static void ozone_toggle(void *userdata, bool menu_on)
{
   bool tmp              = false;
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   if (!ozone)
      return;

   tmp = !menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL);

   if (tmp)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   if (ozone->depth == 1)
   {
      ozone->draw_sidebar = true;
      ozone->sidebar_offset = 0.0f;
   }
}

static bool ozone_menu_init_list(void *data)
{
   menu_displaylist_info_t info;

   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf   = menu_entries_get_selection_buf_ptr(0);

   menu_displaylist_info_init(&info);

   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.exts                    =
      strdup(file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT));
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append_enum(menu_stack, info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0);

   info.list  = selection_buf;

   if (!menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info))
      goto error;

   info.need_push = true;

   if (!menu_displaylist_process(&info))
      goto error;

   menu_displaylist_info_free(&info);
   return true;

error:
   menu_displaylist_info_free(&info);
   return false;
}

static ozone_node_t *ozone_copy_node(const ozone_node_t *old_node)
{
   ozone_node_t *new_node = (ozone_node_t*)malloc(sizeof(*new_node));

   *new_node            = *old_node;

   return new_node;
}

static void ozone_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *label,
      size_t list_size,
      unsigned type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   ozone_node_t *node = NULL;
   int i = (int)list_size;

   if (!ozone || !list)
      return;

   ozone->need_compute = true;

   node = (ozone_node_t*)file_list_get_userdata_at_offset(list, i);

   if (!node)
      node = ozone_alloc_node();

   if (!node)
   {
      RARCH_ERR("ozone node could not be allocated.\n");
      return;
   }

   file_list_set_userdata(list, i, node);
}

static void ozone_list_deep_copy(const file_list_t *src, file_list_t *dst,
      size_t first, size_t last)
{
   size_t i, j = 0;
   menu_animation_ctx_tag tag = (uintptr_t)dst;

   menu_animation_kill_by_tag(&tag);

   /* use true here because file_list_copy() doesn't free actiondata */
   ozone_free_list_nodes(dst, true);

   file_list_clear(dst);
   file_list_reserve(dst, (last + 1) - first);

   for (i = first; i <= last; ++i)
   {
      struct item_file *d = &dst->list[j];
      struct item_file *s = &src->list[i];
      void     *src_udata = s->userdata;
      void     *src_adata = s->actiondata;

      *d       = *s;
      d->alt   = string_is_empty(d->alt)   ? NULL : strdup(d->alt);
      d->path  = string_is_empty(d->path)  ? NULL : strdup(d->path);
      d->label = string_is_empty(d->label) ? NULL : strdup(d->label);

      if (src_udata)
         file_list_set_userdata(dst, j, (void*)ozone_copy_node((const ozone_node_t*)src_udata));

      if (src_adata)
      {
         void *data = malloc(sizeof(menu_file_list_cbs_t));
         memcpy(data, src_adata, sizeof(menu_file_list_cbs_t));
         file_list_set_actiondata(dst, j, data);
      }

      ++j;
   }

   dst->size = j;
}

static void ozone_list_cache(void *data,
      enum menu_list_type type, unsigned action)
{
   size_t y, entries_end;
   unsigned i;
   unsigned video_info_height;
   float bottom_boundary;
   ozone_node_t *first_node;
   unsigned first             = 0;
   unsigned last              = 0;
   file_list_t *selection_buf = NULL;
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return;

   ozone->need_compute        = true;
   ozone->selection_old_list  = ozone->selection;
   ozone->scroll_old          = ozone->animations.scroll_y;
   ozone->is_playlist_old     = ozone->is_playlist;

   /* Deep copy visible elements */
   video_driver_get_size(NULL, &video_info_height);
   y                          = ENTRIES_START_Y;
   entries_end                = menu_entries_get_size();
   selection_buf              = menu_entries_get_selection_buf_ptr(0);
   bottom_boundary            = video_info_height - 87 - 78;

   for (i = 0; i < entries_end; i++)
   {
      ozone_node_t *node = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      if (y + ozone->animations.scroll_y + node->height + 20 < ENTRIES_START_Y)
      {
         first++;
         goto text_iterate;
      }
      else if (y + ozone->animations.scroll_y - node->height - 20 > bottom_boundary)
         goto text_iterate;

      last++;
text_iterate:
      y += node->height;
   }

   last -= 1;
   last += first;

   first_node = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, first);
   ozone->old_list_offset_y = first_node->position_y;

   ozone_list_deep_copy(selection_buf, ozone->selection_buf_old, first, last);
}

static int ozone_environ_cb(enum menu_environ_cb type, void *data, void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   if (!ozone)
      return -1;

   switch (type)
   {
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         if (!ozone)
            return -1;

         ozone_refresh_horizontal_list(ozone);
         break;
      default:
         return -1;
   }

   return 0;
}

static void ozone_messagebox(void *data, const char *message)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (!ozone || string_is_empty(message))
      return;

   if (ozone->pending_message)
   {
      free(ozone->pending_message);
      ozone->pending_message = NULL;
   }

   ozone->pending_message = strdup(message);
   ozone->messagebox_state = true || menu_input_dialog_get_display_kb();
   ozone->should_draw_messagebox = true;
}

static int ozone_deferred_push_content_actions(menu_displaylist_info_t *info)
{
   if (!menu_displaylist_ctl(
            DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, info))
      return -1;
   menu_displaylist_process(info);
   menu_displaylist_info_free(info);
   return 0;
}

static int ozone_list_bind_init_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CONTENT_ACTIONS:
            cbs->action_deferred_push = ozone_deferred_push_content_actions;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int ozone_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (ozone_list_bind_init_compare_label(cbs) == 0)
      return 0;

   return -1;
}

menu_ctx_driver_t menu_ctx_ozone = {
   NULL,                         /* set_texture */
   ozone_messagebox,
   ozone_menu_iterate,
   ozone_render,
   ozone_frame,
   ozone_init,
   ozone_free,
   ozone_context_reset,
   ozone_context_destroy,
   ozone_populate_entries,
   ozone_toggle,
   ozone_navigation_clear,
   ozone_navigation_pointer_changed,
   ozone_navigation_pointer_changed,
   ozone_navigation_set,
   ozone_navigation_pointer_changed,
   ozone_navigation_alphabet,
   ozone_navigation_alphabet,
   ozone_menu_init_list,
   ozone_list_insert,
   NULL,                         /* list_prepend */
   ozone_list_free,
   ozone_list_clear,
   ozone_list_cache,
   ozone_list_push,
   ozone_list_get_selection,
   ozone_list_get_size,
   ozone_list_get_entry,
   NULL,                         /* list_set_selection */
   ozone_list_bind_init,         /* bind_init */
   NULL,                         /* load_image */
   "ozone",
   ozone_environ_cb,
   NULL,                         /* pointer_tap */
   NULL,                         /* update_thumbnail_path */
   NULL,                         /* update_thumbnail_image */
   NULL,                         /* set_thumbnail_system */
   NULL,                         /* set_thumbnail_content */
   menu_display_osk_ptr_at_pos,
   NULL,                         /* update_savestate_thumbnail_path */
   NULL                          /* update_savestate_thumbnail_image */
};

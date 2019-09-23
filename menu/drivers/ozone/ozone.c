/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018      - natinusala
 *  Copyright (C) 2019      - Patrick Scheurenbrand
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
#include <features/features_cpu.h>
#include <formats/image.h>
#include <math/float_minmax.h>

#include "../menu_generic.h"

#include "../../menu_driver.h"
#include "../../menu_animation.h"
#include "../../menu_input.h"
#include "../../playlist.h"
#include "../../runtime_file.h"

#include "../../widgets/menu_osk.h"
#include "../../widgets/menu_filebrowser.h"

#include "../../../configuration.h"
#include "../../../content.h"
#include "../../../core_info.h"
#include "../../../core.h"
#include "../../../verbosity.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../dynamic.h"

ozone_node_t *ozone_alloc_node(void)
{
   ozone_node_t *node = (ozone_node_t*)malloc(sizeof(*node));

   node->height         = 0;
   node->position_y     = 0;
   node->console_name   = NULL;
   node->icon           = 0;
   node->content_icon   = 0;
   node->fullpath       = NULL;

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

   node->console_name = NULL;

   if (node->fullpath)
      free(node->fullpath);

   node->fullpath = NULL;

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

   char xmb_path[PATH_MAX_LENGTH];
   char monochrome_path[PATH_MAX_LENGTH];

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
   ozone->show_cursor               = false;

   ozone->first_frame                     = true;
   ozone->cursor_mode                     = false;

   ozone->sidebar_collapsed               = false;
   ozone->animations.sidebar_text_alpha   = 1.0f;

   ozone->animations.thumbnail_bar_position  = 0.0f;
   ozone->show_thumbnail_bar                 = false;
   ozone->dimensions.sidebar_width           = 0.0f;

   ozone->thumbnail_path_data = menu_thumbnail_path_init();
   if (!ozone->thumbnail_path_data)
      goto error;

   ozone_sidebar_update_collapse(ozone, false);

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

   /* Sidebar path */
   fill_pathname_join(
      ozone->tab_path,
      ozone->png_path,
      "sidebar",
      sizeof(ozone->tab_path)
   );

   /* XMB monochrome */
   fill_pathname_join(
      xmb_path,
      settings->paths.directory_assets,
      "xmb",
      sizeof(xmb_path)
   );

   fill_pathname_join(
      monochrome_path,
      xmb_path,
      "monochrome",
      sizeof(monochrome_path)
   );

   /* Icons path */
   fill_pathname_join(
      ozone->icons_path,
      monochrome_path,
      "png",
      sizeof(ozone->icons_path)
   );

   last_use_preferred_system_color_theme = settings->bools.menu_use_preferred_system_color_theme;

   return menu;

error:
   if (ozone)
   {
      if (ozone->horizontal_list)
      {
         ozone_free_list_nodes(ozone->horizontal_list, false);
         file_list_free(ozone->horizontal_list);
      }

      if (ozone->selection_buf_old)
      {
         ozone_free_list_nodes(ozone->selection_buf_old, false);
         file_list_free(ozone->selection_buf_old);
      }

      ozone->selection_buf_old = NULL;
      ozone->horizontal_list   = NULL;
   }

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

      ozone->horizontal_list     = NULL;
      ozone->selection_buf_old   = NULL;

      if (!string_is_empty(ozone->pending_message))
         free(ozone->pending_message);

      if (ozone->thumbnail_path_data)
         free(ozone->thumbnail_path_data);
   }
}

unsigned ozone_count_lines(const char *str)
{
   unsigned c     = 0;
   unsigned lines = 1;

   for (c = 0; str[c]; c++)
      lines += (str[c] == '\n');
   return lines;
}

static void ozone_update_thumbnail_path(void *data, unsigned i, char pos)
{
   ozone_handle_t    *ozone       = (ozone_handle_t*)data;
   const char    *core_name       = NULL;

   if (!ozone)
      return;

   /* imageviewer content requires special treatment... */
   menu_thumbnail_get_core_name(ozone->thumbnail_path_data, &core_name);
   if (string_is_equal(core_name, "imageviewer"))
   {
      if ((pos == 'R') || (pos == 'L' && !menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_RIGHT)))
         menu_thumbnail_update_path(ozone->thumbnail_path_data, pos == 'R' ? MENU_THUMBNAIL_RIGHT : MENU_THUMBNAIL_LEFT);
   }
   else
      menu_thumbnail_update_path(ozone->thumbnail_path_data, pos == 'R' ? MENU_THUMBNAIL_RIGHT : MENU_THUMBNAIL_LEFT);
}

static void ozone_update_thumbnail_image(void *data)
{
   settings_t *settings             = config_get_ptr();
   ozone_handle_t *ozone            = (ozone_handle_t*)data;
   const char *right_thumbnail_path = NULL;
   const char *left_thumbnail_path  = NULL;
   bool supports_rgba               = video_driver_supports_rgba();

   /* Have to wrap `thumbnails_missing` like this to silence
    * brain dead `set but not used` warnings when networking
    * is disabled... */
#ifdef HAVE_NETWORKING
   bool thumbnails_missing          = false;
#endif

   if (!ozone || !settings)
      return;

   if (menu_thumbnail_get_path(ozone->thumbnail_path_data, MENU_THUMBNAIL_RIGHT, &right_thumbnail_path))
   {
      if (path_is_valid(right_thumbnail_path))
         task_push_image_load(right_thumbnail_path,
               supports_rgba, settings->uints.menu_thumbnail_upscale_threshold,
               menu_display_handle_thumbnail_upload, NULL);
      else
      {
         video_driver_texture_unload(&ozone->thumbnail);
#ifdef HAVE_NETWORKING
         thumbnails_missing = true;
#endif
      }
   }
   else
      video_driver_texture_unload(&ozone->thumbnail);

   if (menu_thumbnail_get_path(ozone->thumbnail_path_data, MENU_THUMBNAIL_LEFT, &left_thumbnail_path))
   {
      if (path_is_valid(left_thumbnail_path))
         task_push_image_load(left_thumbnail_path,
               supports_rgba, settings->uints.menu_thumbnail_upscale_threshold,
               menu_display_handle_left_thumbnail_upload, NULL);
      else
      {
         video_driver_texture_unload(&ozone->left_thumbnail);
#ifdef HAVE_NETWORKING
         thumbnails_missing = true;
#endif
      }
   }
   else
      video_driver_texture_unload(&ozone->left_thumbnail);

#ifdef HAVE_NETWORKING
   /* On demand thumbnail downloads */
   if (thumbnails_missing)
   {
      if (settings->bools.network_on_demand_thumbnails)
      {
         const char *system = NULL;

         if (menu_thumbnail_get_system(ozone->thumbnail_path_data, &system))
            task_push_pl_entry_thumbnail_download(system,
                  playlist_get_cached(), (unsigned)menu_navigation_get_selection(),
                  false, true);
      }
   }
#endif
}

static void ozone_refresh_thumbnail_image(void *data)
{
   ozone_handle_t *ozone            = (ozone_handle_t*)data;

   if (!ozone)
      return;

   /* Only refresh thumbnails if thumbnails are enabled
    * and we are currently viewing a playlist */
   if ((menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_RIGHT) ||
        menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_LEFT)) &&
       (ozone->is_playlist && ozone->depth == 1))
      ozone_update_thumbnail_image(ozone);
}

/* TODO: Scale text */
static void ozone_context_reset(void *data, bool is_threaded)
{
   /* Fonts init */
   unsigned i;
   unsigned size;
   char font_path[PATH_MAX_LENGTH];

   float scale = 1; /*TODO: compute that from screen resolution and dpi */

   ozone_handle_t *ozone = (ozone_handle_t*) data;

   if (ozone)
   {
      ozone->has_all_assets = true;

      fill_pathname_join(font_path, ozone->assets_path, "regular.ttf", sizeof(font_path));
      ozone->fonts.footer           = menu_display_font_file(font_path, FONT_SIZE_FOOTER, is_threaded);
      ozone->fonts.entries_label    = menu_display_font_file(font_path, FONT_SIZE_ENTRIES_LABEL, is_threaded);
      ozone->fonts.entries_sublabel = menu_display_font_file(font_path, FONT_SIZE_ENTRIES_SUBLABEL, is_threaded);
      ozone->fonts.time             = menu_display_font_file(font_path, FONT_SIZE_TIME, is_threaded);
      ozone->fonts.sidebar          = menu_display_font_file(font_path, FONT_SIZE_SIDEBAR, is_threaded);

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

      /* Dimensions */
      ozone->dimensions.header_height = HEADER_HEIGHT * scale;
      ozone->dimensions.footer_height = FOOTER_HEIGHT * scale;

      ozone->dimensions.entry_padding_horizontal_half = ENTRY_PADDING_HORIZONTAL_HALF * scale;
      ozone->dimensions.entry_padding_horizontal_full = ENTRY_PADDING_HORIZONTAL_FULL * scale;
      ozone->dimensions.entry_padding_vertical        = ENTRY_PADDING_VERTICAL * scale;
      ozone->dimensions.entry_height                  = ENTRY_HEIGHT * scale;
      ozone->dimensions.entry_spacing                 = ENTRY_SPACING * scale;
      ozone->dimensions.entry_icon_size               = ENTRY_ICON_SIZE * scale;
      ozone->dimensions.entry_icon_padding            = ENTRY_ICON_PADDING * scale;

      ozone->dimensions.sidebar_entry_height             = SIDEBAR_ENTRY_HEIGHT * scale;
      ozone->dimensions.sidebar_padding_horizontal       = SIDEBAR_X_PADDING * scale;
      ozone->dimensions.sidebar_padding_vertical         = SIDEBAR_Y_PADDING * scale;
      ozone->dimensions.sidebar_entry_padding_vertical   = SIDEBAR_ENTRY_Y_PADDING * scale;
      ozone->dimensions.sidebar_entry_icon_size          = SIDEBAR_ENTRY_ICON_SIZE * scale;
      ozone->dimensions.sidebar_entry_icon_padding       = SIDEBAR_ENTRY_ICON_PADDING * scale;

      ozone->dimensions.sidebar_width_normal             = SIDEBAR_WIDTH * scale;
      ozone->dimensions.sidebar_width_collapsed          = ozone->dimensions.sidebar_entry_icon_size 
         + ozone->dimensions.sidebar_entry_icon_padding * 2
         + ozone->dimensions.sidebar_padding_horizontal * 2;

      if (ozone->dimensions.sidebar_width == 0)
         ozone->dimensions.sidebar_width                    = (float) ozone->dimensions.sidebar_width_normal;

      ozone->dimensions.thumbnail_bar_width              = ozone->dimensions.sidebar_width_normal 
         - ozone->dimensions.sidebar_entry_icon_size
         - ozone->dimensions.sidebar_entry_icon_padding;

      ozone->dimensions.thumbnail_width      = ozone->dimensions.thumbnail_bar_width - ozone->dimensions.sidebar_entry_icon_padding * 2;
      ozone->dimensions.left_thumbnail_width = ozone->dimensions.thumbnail_width;

      ozone->dimensions.cursor_size = CURSOR_SIZE * scale;

      /* Naive font size */
      ozone->title_font_glyph_width    = FONT_SIZE_TITLE * 3/4;
      ozone->entry_font_glyph_width    = FONT_SIZE_ENTRIES_LABEL * 3/4;
      ozone->sublabel_font_glyph_width = FONT_SIZE_ENTRIES_SUBLABEL * 3/4;
      ozone->sidebar_font_glyph_width  = FONT_SIZE_SIDEBAR * 3/4;
      ozone->footer_font_glyph_width   = FONT_SIZE_FOOTER * 3/4;

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
      size = font_driver_get_message_width(ozone->fonts.sidebar, "a", 1, 1);
      if (size)
         ozone->sidebar_font_glyph_width = size;
      size = font_driver_get_message_width(ozone->fonts.footer, "a", 1, 1);
      if (size)
         ozone->footer_font_glyph_width = size;

      /* Textures init */
      for (i = 0; i < OZONE_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
         filename[0] = '\0';
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
            if (!menu_display_reset_textures_list(filename, buf, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
               RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", ozone->png_path, path_default_slash(), filename);
         }
         else
         {
#endif
            if (!menu_display_reset_textures_list(filename, ozone->png_path, &ozone->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
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

         filename[0]        = '\0';
         strlcpy(filename,
               OZONE_TAB_TEXTURES_FILES[i], sizeof(filename));
         strlcat(filename, ".png", sizeof(filename));

         if (!menu_display_reset_textures_list(filename, ozone->tab_path, &ozone->tab_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
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
      {
         if (!menu_display_reset_textures_list(ozone_entries_icon_texture_path(i), ozone->icons_path, &ozone->icons_textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
         {
            ozone->has_all_assets = false;
            RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", ozone->icons_path, path_default_slash(), ozone_entries_icon_texture_path(i));
         }
      }

      menu_display_allocate_white_texture();

      /* Horizontal list */
      ozone_context_reset_horizontal_list(ozone);

      /* State reset */
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
         runloop_msg_queue_push(msg_hash_to_str(MSG_MISSING_ASSETS), 1, 256, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      /* Thumbnails */
      if (menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_RIGHT))
         ozone_update_thumbnail_path(ozone, 0, 'R');

      if (menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_LEFT))
         ozone_update_thumbnail_path(ozone, 0, 'L');

      ozone_update_thumbnail_image(ozone);

      /* TODO: update savestate thumbnail image */

      ozone_restart_cursor_animation(ozone);
   }
   video_driver_monitor_reset();
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

   /* Thumbnails */
   video_driver_texture_unload(&ozone->thumbnail);
   video_driver_texture_unload(&ozone->left_thumbnail);

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
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                  msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                  MENU_ENUM_LABEL_PLAYLISTS_TAB,
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

            if (rarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL))
            {
               if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
               {
                  entry.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
                  menu_displaylist_setting(&entry);
               }
            }
            else
            {
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

            if (settings->bools.menu_show_load_disc)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_DISC;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_dump_disc)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_DUMP_DISC;
               menu_displaylist_setting(&entry);
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
            if (settings->bools.menu_show_restart_retroarch)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_RESTART_RETROARCH;
               menu_displaylist_setting(&entry);
            }

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

static void ozone_render(void *data,
      unsigned width, unsigned height,
      bool is_idle)
{
   size_t i;
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

   /* TODO: Handle pointer & mouse */

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
   menu_animation_ctx_ticker_smooth_t ticker_smooth;
   static const char* const ticker_spacer = OZONE_TICKER_SPACER;
   unsigned ticker_x_offset = 0;
   settings_t *settings     = config_get_ptr();
   unsigned timedate_offset = 0;
   bool use_smooth_ticker   = settings->bools.menu_ticker_smooth;

   /* Initial ticker configuration */
   if (use_smooth_ticker)
   {
      ticker_smooth.idx           = menu_animation_get_ticker_pixel_idx();
      ticker_smooth.font_scale    = 1.0f;
      ticker_smooth.type_enum     = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
      ticker_smooth.spacer        = ticker_spacer;
      ticker_smooth.x_offset      = &ticker_x_offset;
      ticker_smooth.dst_str_width = NULL;
   }
   else
   {
      ticker.idx       = menu_animation_get_ticker_idx();
      ticker.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
      ticker.spacer    = ticker_spacer;
   }

   /* Separator */
   menu_display_draw_quad(video_info, 30, ozone->dimensions.header_height, video_info->width - 60, 1, video_info->width, video_info->height, ozone->theme->header_footer_separator);

   /* Title */
   if (use_smooth_ticker)
   {
      ticker_smooth.font        = ozone->fonts.title;
      ticker_smooth.selected    = true;
      ticker_smooth.field_width = (video_info->width - 128 - 47 - 180);
      ticker_smooth.src_str     = ozone->title;
      ticker_smooth.dst_str     = title;
      ticker_smooth.dst_str_len = sizeof(title);

      menu_animation_ticker_smooth(&ticker_smooth);
   }
   else
   {
      ticker.s        = title;
      ticker.len      = (video_info->width - 128 - 47 - 180) / ozone->title_font_glyph_width;
      ticker.str      = ozone->title;
      ticker.selected = true;

      menu_animation_ticker(&ticker);
   }

   ozone_draw_text(video_info, ozone, title, ticker_x_offset + 128, ozone->dimensions.header_height / 2 + FONT_SIZE_TITLE * 3/8, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.title, ozone->theme->text_rgba, false);

   /* Icon */
   menu_display_blend_begin(video_info);
#if 0
   if (discord_avatar_is_ready())
      ozone_draw_icon(video_info, 60, 60, ozone->textures[OZONE_TEXTURE_DISCORD_OWN_AVATAR], 47, 14, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
   else
#endif
      ozone_draw_icon(video_info, 60, 60, ozone->textures[OZONE_TEXTURE_RETROARCH], 47, (ozone->dimensions.header_height - 60) / 2, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
   menu_display_blend_end(video_info);

   /* Battery */
   if (video_info->battery_level_enable)
   {
      menu_display_ctx_powerstate_t powerstate;
      char msg[12];

      msg[0] = '\0';

      powerstate.s   = msg;
      powerstate.len = sizeof(msg);

      menu_display_powerstate(&powerstate);

      if (powerstate.battery_enabled)
      {
         timedate_offset = 95;

         ozone_draw_text(video_info, ozone, msg, video_info->width - 85, ozone->dimensions.header_height / 2 + FONT_SIZE_TIME * 3/8, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.time, ozone->theme->text_rgba, false);

         menu_display_blend_begin(video_info);
         ozone_draw_icon(video_info, 92, 92, ozone->icons_textures[powerstate.charging? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING : (powerstate.percent > 80)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL : (powerstate.percent > 60)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_80 : (powerstate.percent > 40)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_60 : (powerstate.percent > 20)? OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_40 : OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_20], video_info->width - 60 - 56, ozone->dimensions.header_height / 2 - 42, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
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

      ozone_draw_text(video_info, ozone, timedate, video_info->width - 85 - timedate_offset, ozone->dimensions.header_height / 2 + FONT_SIZE_TIME * 3/8, TEXT_ALIGN_RIGHT, video_info->width, video_info->height, ozone->fonts.time, ozone->theme->text_rgba, false);

      menu_display_blend_begin(video_info);
      ozone_draw_icon(video_info, 92, 92, ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOCK], video_info->width - 60 - 56 - timedate_offset, ozone->dimensions.header_height / 2 - 42, video_info->width, video_info->height, 0, 1, ozone->theme->entries_icon);
      menu_display_blend_end(video_info);
   }
}

static void ozone_draw_footer(ozone_handle_t *ozone, video_frame_info_t *video_info, settings_t *settings)
{
   /* Separator */
   menu_display_draw_quad(video_info, 23, video_info->height - ozone->dimensions.footer_height, video_info->width - 60, 1, video_info->width, video_info->height, ozone->theme->header_footer_separator);

   /* Core title or Switch icon */
   if (settings->bools.menu_core_enable)
   {
      char core_title[255];
      menu_entries_get_core_title(core_title, sizeof(core_title));
      ozone_draw_text(video_info, ozone, core_title, 59, video_info->height - ozone->dimensions.footer_height / 2 + FONT_SIZE_FOOTER * 3/8, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);
   }
   else
      ozone_draw_icon(video_info, 69, 30, ozone->theme->textures[OZONE_THEME_TEXTURE_SWITCH], 59, video_info->height - ozone->dimensions.footer_height / 2 - 15, video_info->width,video_info->height, 0, 1, NULL);

   /* Buttons */
   {
      unsigned back_width     = 215;
      unsigned ok_width       = 96;
      unsigned search_width   = 343;
      unsigned thumb_width    = 343 + 188 + 80;
      unsigned icon_size      = 35;
      unsigned icon_offset    = icon_size / 2;
      bool do_swap            = video_info->input_menu_swap_ok_cancel_buttons;

      if (do_swap)
      {
         back_width  = 96;
         ok_width    = 215;
      }

      menu_display_blend_begin(video_info);

      menu_display_set_alpha(ozone->theme_dynamic.entries_icon, 1.0f);

      if (do_swap)
      {
         ozone_draw_icon(video_info, icon_size, icon_size, ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D], video_info->width - 138, video_info->height - ozone->dimensions.footer_height / 2 - icon_offset, video_info->width,video_info->height, 0, 1, ozone->theme_dynamic.entries_icon);
         ozone_draw_icon(video_info, icon_size, icon_size, ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R], video_info->width - 256, video_info->height - ozone->dimensions.footer_height / 2 - icon_offset, video_info->width,video_info->height, 0, 1, ozone->theme_dynamic.entries_icon);
      }
      else
      {
         ozone_draw_icon(video_info, icon_size, icon_size, ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D], video_info->width - 256, video_info->height - ozone->dimensions.footer_height / 2 - icon_offset, video_info->width,video_info->height, 0, 1, ozone->theme_dynamic.entries_icon);
         ozone_draw_icon(video_info, icon_size, icon_size, ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R], video_info->width - 138, video_info->height - ozone->dimensions.footer_height / 2 - icon_offset, video_info->width,video_info->height, 0, 1, ozone->theme_dynamic.entries_icon);
      }

      ozone_draw_icon(video_info, icon_size, icon_size, ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U], video_info->width - 384, video_info->height - ozone->dimensions.footer_height / 2 - icon_offset, video_info->width,video_info->height, 0, 1, ozone->theme_dynamic.entries_icon);

      if (ozone->is_playlist && !ozone->cursor_in_sidebar)
         ozone_draw_icon(video_info, icon_size, icon_size, ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L], video_info->width - 384 - 118 - 100 - 50, video_info->height - ozone->dimensions.footer_height / 2 - icon_offset, video_info->width,video_info->height, 0, 1, ozone->theme_dynamic.entries_icon);

      menu_display_blend_end(video_info);

      ozone_draw_text(video_info, ozone,
            do_swap ?
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK) :
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK),
            video_info->width - back_width, video_info->height - ozone->dimensions.footer_height / 2 + FONT_SIZE_FOOTER * 3/8, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);
      ozone_draw_text(video_info, ozone,
            do_swap ?
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK) :
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK),
            video_info->width - ok_width, video_info->height - ozone->dimensions.footer_height / 2 + FONT_SIZE_FOOTER * 3/8, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);

      ozone_draw_text(video_info, ozone,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH),
            video_info->width - search_width, video_info->height - ozone->dimensions.footer_height / 2 + FONT_SIZE_FOOTER * 3/8, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);

      if (ozone->is_playlist && !ozone->cursor_in_sidebar)
         ozone_draw_text(video_info, ozone,
               msg_hash_to_str(MSG_CHANGE_THUMBNAIL_TYPE),
               video_info->width - thumb_width, video_info->height - ozone->dimensions.footer_height / 2 + FONT_SIZE_FOOTER * 3/8, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.footer, ozone->theme->text_rgba, false);

   }

   menu_display_blend_end(video_info);
}

void ozone_update_content_metadata(ozone_handle_t *ozone)
{
   size_t selection           = menu_navigation_get_selection();
   playlist_t *playlist       = playlist_get_cached();
   settings_t *settings       = config_get_ptr();

   if (ozone->is_playlist && playlist)
   {
      const char *core_name        = NULL;
      const char *core_label       = NULL;
      bool scroll_content_metadata = settings->bools.ozone_scroll_content_metadata;

      menu_thumbnail_get_core_name(ozone->thumbnail_path_data, &core_name);

      /* Fill core name */
      if (!core_name || string_is_equal(core_name, "DETECT"))
         core_label = msg_hash_to_str(MSG_AUTODETECT);
      else
         core_label = core_name;

      snprintf(ozone->selection_core_name, sizeof(ozone->selection_core_name),
         "%s %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE), core_label);

      /* Word wrap core name string, if required */
      if (!scroll_content_metadata)
      {
         unsigned metadata_len =
               (ozone->dimensions.thumbnail_bar_width - ((ozone->dimensions.sidebar_entry_icon_padding * 2) * 2)) /
                     ozone->footer_font_glyph_width;
         word_wrap(ozone->selection_core_name, ozone->selection_core_name, metadata_len, true, 0);
         ozone->selection_core_name_lines = ozone_count_lines(ozone->selection_core_name);
      }
      else
         ozone->selection_core_name_lines = 1;

      ozone->selection_core_is_viewer = string_is_equal(core_label, "imageviewer")
            || string_is_equal(core_label, "musicplayer")
            || string_is_equal(core_label, "movieplayer");

      /* Fill play time if applicable */
      if (settings->bools.content_runtime_log || settings->bools.content_runtime_log_aggregate)
      {
         const struct playlist_entry *entry = NULL;

         playlist_get_index(playlist, selection, &entry);

         if (entry->runtime_status == PLAYLIST_RUNTIME_UNKNOWN)
            runtime_update_playlist(playlist, selection);

         if (!string_is_empty(entry->runtime_str))
            strlcpy(ozone->selection_playtime, entry->runtime_str, sizeof(ozone->selection_playtime));
         if (!string_is_empty(entry->last_played_str))
            strlcpy(ozone->selection_lastplayed, entry->last_played_str, sizeof(ozone->selection_lastplayed));
      }
      else
      {
         snprintf(ozone->selection_playtime, sizeof(ozone->selection_playtime), "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED));

         snprintf(ozone->selection_lastplayed, sizeof(ozone->selection_lastplayed), "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED));
      }

      /* Word wrap last played string, if required */
      if (!scroll_content_metadata)
      {
         /* Note: Have to use a fixed length of '30' here, to
          * avoid awkward wrapping for certain last played time
          * formats. Last played strings are well defined, however
          * (unlike core names), so this should never overflow the
          * side bar */
         word_wrap(ozone->selection_lastplayed, ozone->selection_lastplayed, 30, true, 0);
         ozone->selection_lastplayed_lines = ozone_count_lines(ozone->selection_lastplayed);
      }
      else
         ozone->selection_lastplayed_lines = 1;
   }
}

static void ozone_set_thumbnail_content(void *data, const char *s)
{
   size_t selection           = menu_navigation_get_selection();
   ozone_handle_t *ozone      = (ozone_handle_t*)data;

   if (!ozone)
      return;

   if (ozone->is_playlist)
   {
      /* Playlist content */
      if (string_is_empty(s))
         menu_thumbnail_set_content_playlist(ozone->thumbnail_path_data,
               playlist_get_cached(), selection);
   }
   else if (ozone->is_db_manager_list)
   {
      /* Database list content */
      if (string_is_empty(s))
      {
         menu_entry_t entry;

         menu_entry_init(&entry);
         entry.label_enabled      = false;
         entry.rich_label_enabled = false;
         entry.value_enabled      = false;
         entry.sublabel_enabled   = false;
         menu_entry_get(&entry, 0, selection, NULL, true);

         if (!string_is_empty(entry.path))
            menu_thumbnail_set_content(ozone->thumbnail_path_data, entry.path);
      }
   }
   else if (string_is_equal(s, "imageviewer"))
   {
      /* Filebrowser image updates */
      menu_entry_t entry;
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
      ozone_node_t *node = (ozone_node_t*)file_list_get_userdata_at_offset(selection_buf, selection);

      menu_entry_init(&entry);
      entry.label_enabled      = false;
      entry.rich_label_enabled = false;
      entry.value_enabled      = false;
      entry.sublabel_enabled   = false;
      menu_entry_get(&entry, 0, selection, NULL, true);

      if (node)
         if (!string_is_empty(entry.path) && !string_is_empty(node->fullpath))
            menu_thumbnail_set_content_image(ozone->thumbnail_path_data, node->fullpath, entry.path);
   }
   else if (!string_is_empty(s))
   {
      /* Annoying leftovers...
       * This is required to ensure that thumbnails are
       * updated correctly when navigating deeply through
       * the sublevels of database manager lists.
       * Showing thumbnails on database entries is a
       * pointless nuisance and a waste of CPU cycles, IMHO... */
      menu_thumbnail_set_content(ozone->thumbnail_path_data, s);
   }

   ozone_update_content_metadata(ozone);
}

static void ozone_unload_thumbnail_textures(void *data)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!ozone)
      return;

   if (ozone->thumbnail)
      video_driver_texture_unload(&ozone->thumbnail);
   if (ozone->left_thumbnail)
      video_driver_texture_unload(&ozone->left_thumbnail);
}

static void ozone_set_thumbnail_system(void *data, char*s, size_t len)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   if (!ozone)
      return;

   menu_thumbnail_set_system(
         ozone->thumbnail_path_data, s, playlist_get_cached());
}

static void ozone_get_thumbnail_system(void *data, char*s, size_t len)
{
   ozone_handle_t *ozone = (ozone_handle_t*)data;
   const char *system    = NULL;
   if (!ozone)
      return;

   if (menu_thumbnail_get_system(ozone->thumbnail_path_data, &system))
      strlcpy(s, system, len);
}

static void ozone_selection_changed(ozone_handle_t *ozone, bool allow_animation)
{
   menu_entry_t entry;

   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   menu_animation_ctx_tag tag = (uintptr_t) selection_buf;
   size_t selection           = menu_navigation_get_selection();

   size_t new_selection = menu_navigation_get_selection();
   ozone_node_t *node   = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, new_selection);

   if (!node)
      return;

   menu_entry_init(&entry);
   entry.path_enabled       = false;
   entry.label_enabled      = false;
   entry.rich_label_enabled = false;
   entry.value_enabled      = false;
   entry.sublabel_enabled   = false;
   menu_entry_get(&entry, 0, selection, NULL, true);

   if (ozone->selection != new_selection)
   {
      unsigned entry_type     = menu_entry_get_type_new(&entry);

      ozone->selection_old         = ozone->selection;
      ozone->selection             = new_selection;

      ozone->cursor_in_sidebar_old = ozone->cursor_in_sidebar;

      menu_animation_kill_by_tag(&tag);
      ozone_update_scroll(ozone, allow_animation, node);

      /* Update thumbnail */
      if (menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_RIGHT) ||
          menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_LEFT))
      {
         bool update_thumbnails = false;

         /* Playlist updates */
         if (ozone->is_playlist && ozone->depth == 1)
         {
            ozone_set_thumbnail_content(ozone, "");
            update_thumbnails = true;
         }
         /* Database list updates
            * (pointless nuisance...) */
         else if (ozone->depth == 4 && ozone->is_db_manager_list)
         {
            ozone_set_thumbnail_content(ozone, "");
            update_thumbnails = true;
         }
         /* Filebrowser image updates */
         else if (entry_type == FILE_TYPE_IMAGEVIEWER || entry_type == FILE_TYPE_IMAGE)
         {
            ozone_set_thumbnail_content(ozone, "imageviewer");
            update_thumbnails = true;
         }

         if (update_thumbnails)
         {
            if (menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_RIGHT))
                ozone_update_thumbnail_path(ozone, 0 /* will be ignored */, 'R');

            if (menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_LEFT))
                ozone_update_thumbnail_path(ozone, 0 /* will be ignored */, 'L');

            ozone_update_thumbnail_image(ozone);
         }
      }

      /* TODO: update savestate thumbnail and path */
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

   if (ozone->first_frame)
   {
      menu_input_pointer_t pointer;
      menu_input_get_pointer_state(&pointer);

      ozone->cursor_x_old = pointer.x;
      ozone->cursor_y_old = pointer.y;
      ozone->first_frame  = false;
   }

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
   menu_display_scissor_begin(video_info, ozone->sidebar_offset + (unsigned) ozone->dimensions.sidebar_width, ozone->dimensions.header_height + 1, video_info->width - (unsigned) ozone->dimensions.sidebar_width + (-ozone->sidebar_offset), video_info->height - ozone->dimensions.header_height - ozone->dimensions.footer_height - 1);

   /* Current list */
   ozone_draw_entries(ozone,
      video_info,
      (unsigned)ozone->selection,
      (unsigned)ozone->selection_old,
      menu_entries_get_selection_buf_ptr(0),
      ozone->animations.list_alpha,
      ozone->animations.scroll_y,
      ozone->is_playlist
   );

   /* Old list */
   if (ozone->draw_old_list)
      ozone_draw_entries(ozone,
         video_info,
         (unsigned)ozone->selection_old_list,
         (unsigned)ozone->selection_old_list,
         ozone->selection_buf_old,
         ozone->animations.list_alpha,
         ozone->scroll_old,
         ozone->is_playlist_old
      );

   /* Thumbnail bar */
   if (ozone->show_thumbnail_bar)
      ozone_draw_thumbnail_bar(ozone, video_info);

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

      ozone_draw_backdrop(video_info, float_min(ozone->animations.messagebox_alpha, 0.75f));

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

   /* Cursor */
   if (ozone->show_cursor)
   {
      menu_input_pointer_t pointer;
      menu_input_get_pointer_state(&pointer);

      menu_display_set_alpha(ozone_pure_white, 1.0f);
      menu_display_draw_cursor(
         video_info,
         ozone_pure_white,
         ozone->dimensions.cursor_size,
         ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POINTER],
         pointer.x,
         pointer.y,
         video_info->width,
         video_info->height
      );
   }

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
   ozone->draw_old_list             = false;
   ozone->animations.cursor_alpha   = 1.0f;
}

static void ozone_list_open(ozone_handle_t *ozone)
{
   struct menu_animation_ctx_entry entry;

   ozone->draw_old_list = true;

   /* Left/right animation */
   ozone->animations.list_alpha = 0.0f;

   entry.cb             = ozone_animation_end;
   entry.duration       = ANIMATION_PUSH_ENTRY_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &ozone->animations.list_alpha;
   entry.tag            = (uintptr_t) NULL;
   entry.target_value   = 1.0f;
   entry.userdata       = ozone;

   menu_animation_push(&entry);

   /* Sidebar animation */
   ozone_sidebar_update_collapse(ozone, true);

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
      entry.target_value = -ozone->dimensions.sidebar_width;
      entry.userdata = (void*) ozone;

      menu_animation_push(&entry);
   }
}

static void ozone_populate_entries(void *data, const char *path, const char *label, unsigned k)
{
   ozone_handle_t *ozone = (ozone_handle_t*) data;

   int new_depth;
   bool animate;

   if (!ozone)
      return;

   ozone_set_header(ozone);

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
      ozone_selection_changed(ozone, false);
      return;
   }

   ozone->need_compute = true;

   new_depth = (int)ozone_list_get_size(ozone, MENU_LIST_PLAIN);

   animate                    = new_depth != ozone->depth;
   ozone->fade_direction      = new_depth <= ozone->depth;
   ozone->depth               = new_depth;
   ozone->is_playlist         = ozone_is_playlist(ozone, true);
   ozone->is_db_manager_list  = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST));

   if (ozone->categories_selection_ptr == ozone->categories_active_idx_old)
   {
      if (animate)
         ozone_list_open(ozone);
   }

   /* Thumbnails */
   if (menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_RIGHT) ||
       menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_LEFT))
   {
      ozone_unload_thumbnail_textures(ozone);

      if (ozone->is_playlist)
      {
         ozone_set_thumbnail_content(ozone, "");

         if (menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_RIGHT))
            ozone_update_thumbnail_path(ozone, 0 /* will be ignored */, 'R');

         if (menu_thumbnail_is_enabled(ozone->thumbnail_path_data, MENU_THUMBNAIL_LEFT))
            ozone_update_thumbnail_path(ozone, 0 /* will be ignored */, 'L');

         ozone_update_thumbnail_image(ozone);
      }
   }
}

static int ozone_menu_iterate(void *data,
      void *userdata, enum menu_action action)
{
   int new_selection;
   enum menu_action new_action;
   menu_animation_ctx_tag tag;
   file_list_t *selection_buf    = NULL;
   ozone_handle_t *ozone         = (ozone_handle_t*) userdata;
   unsigned horizontal_list_size = 0;
   menu_handle_t *menu           = (menu_handle_t*)data;

   if (!ozone)
      return generic_menu_iterate(menu, userdata, action);

   if (ozone->horizontal_list)
      horizontal_list_size    = (unsigned)ozone->horizontal_list->size;

   ozone->messagebox_state = false || menu_input_dialog_get_display_kb();

   selection_buf              = menu_entries_get_selection_buf_ptr(0);
   tag                        = (uintptr_t)selection_buf;
   new_action                 = action;

   /* Inputs override */
   switch (action)
   {
      case MENU_ACTION_DOWN:
         ozone->cursor_mode = false;
         if (!ozone->cursor_in_sidebar)
            break;

         tag           = (uintptr_t)ozone;
         new_selection = (int)(ozone->categories_selection_ptr + 1);

         if (new_selection >= (int)(ozone->system_tab_end + horizontal_list_size + 1))
            new_selection = 0;

         ozone_sidebar_goto(ozone, new_selection);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_UP:
         ozone->cursor_mode = false;
         if (!ozone->cursor_in_sidebar)
            break;

         tag           = (uintptr_t)ozone;
         new_selection = (int)ozone->categories_selection_ptr - 1;

         if (new_selection < 0)
            new_selection = horizontal_list_size + ozone->system_tab_end;

         ozone_sidebar_goto(ozone, new_selection);

         new_action = MENU_ACTION_NOOP;
         break;
      case MENU_ACTION_LEFT:
         ozone->cursor_mode = false;
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
         ozone->cursor_mode = false;
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
         ozone->cursor_mode = false;
         if (ozone->cursor_in_sidebar)
         {
            ozone_leave_sidebar(ozone, tag);
            new_action = MENU_ACTION_NOOP;
            break;
         }
         break;
      case MENU_ACTION_CANCEL:
         ozone->cursor_mode = false;
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

/* TODO: Fancy toggle animation */

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

   ozone_sidebar_update_collapse(ozone, false);
}

static bool ozone_menu_init_list(void *data)
{
   menu_displaylist_info_t info;

   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf   = menu_entries_get_selection_buf_ptr(0);

   menu_displaylist_info_init(&info);

   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.exts                    = strdup("lpl");
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
   new_node->fullpath   = old_node->fullpath ? strdup(old_node->fullpath) : NULL;

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
   y                          = ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical;
   entries_end                = menu_entries_get_size();
   selection_buf              = menu_entries_get_selection_buf_ptr(0);
   bottom_boundary            = video_info_height - ozone->dimensions.header_height - ozone->dimensions.footer_height;

   for (i = 0; i < entries_end; i++)
   {
      ozone_node_t *node = (ozone_node_t*) file_list_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      if (y + ozone->animations.scroll_y + node->height + 20 < ozone->dimensions.header_height + ozone->dimensions.entry_padding_vertical)
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
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         ozone->show_cursor = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         ozone->show_cursor = false;
         break;
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

#ifdef HAVE_MENU_WIDGETS
static bool ozone_get_load_content_animation_data(void *userdata, menu_texture_item *icon, char **playlist_name)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   if (ozone->categories_selection_ptr > ozone->system_tab_end)
   {
      ozone_node_t *node = (ozone_node_t*) file_list_get_userdata_at_offset(ozone->horizontal_list, ozone->categories_selection_ptr - ozone->system_tab_end-1);

      *icon          = node->icon;
      *playlist_name = node->console_name;
   }
   else
   {
      *icon          = ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU];
      *playlist_name = "RetroArch";
   }

   return true;
}
#endif

static int ozone_pointer_up(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   size_t selection         = menu_navigation_get_selection();
   if (ptr == selection)
      return (unsigned)menu_entry_action(entry, (unsigned)selection, MENU_ACTION_SELECT);

   menu_navigation_set_selection(ptr);
   menu_driver_navigation_set(false);

   return 0;
}

static bool ozone_load_image(void *userdata, void *data, enum menu_image_type type)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;
   unsigned sidebar_height;
   unsigned height;
   unsigned maximum_height, maximum_width;
   float display_aspect_ratio;

   if (!ozone)
      return false;

   if (!data)
   {
      /* If this happens, the image we attempted to load
       * was corrupt/incorrectly formatted. If this was a
       * thumbnail image, have unload any existing thumbnails
       * (otherwise entry with 'corrupt' thumbnail will show
       * thumbnail from last selected 'good' entry) */
      if (type == MENU_IMAGE_THUMBNAIL)
      {
         ozone->dimensions.thumbnail_width = 0.0f;
         ozone->dimensions.thumbnail_height = 0.0f;
         video_driver_texture_unload(&ozone->thumbnail);
      }
      else if (type == MENU_IMAGE_LEFT_THUMBNAIL)
      {
         ozone->dimensions.left_thumbnail_width = 0.0f;
         ozone->dimensions.left_thumbnail_height = 0.0f;
         video_driver_texture_unload(&ozone->left_thumbnail);
      }

      return false;
   }

   video_driver_get_size(NULL, &height);

   sidebar_height = height - ozone->dimensions.header_height - 55 - ozone->dimensions.footer_height;
   maximum_height = sidebar_height / 2;
   maximum_width  = ozone->dimensions.thumbnail_bar_width - ozone->dimensions.sidebar_entry_icon_padding * 2;
   if (maximum_height > 0)
      display_aspect_ratio = (float)maximum_width / (float)maximum_height;
   else
      display_aspect_ratio = 0.0f;

   switch (type)
   {
      case MENU_IMAGE_THUMBNAIL:
      {
         struct texture_image *img = (struct texture_image*)data;

         if (img->width > 0 && img->height > 0 && display_aspect_ratio > 0.0001f)
         {
            float thumb_aspect_ratio = (float)img->width / (float)img->height;

            if (thumb_aspect_ratio > display_aspect_ratio)
            {
               ozone->dimensions.thumbnail_width = (float)maximum_width;
               ozone->dimensions.thumbnail_height = (float)img->height * (float)maximum_width / (float)img->width;
            }
            else
            {
               ozone->dimensions.thumbnail_height = (float)maximum_height;
               ozone->dimensions.thumbnail_width = (float)img->width * (float)maximum_height / (float)img->height;
            }

            video_driver_texture_unload(&ozone->thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &ozone->thumbnail);
         }
         else
         {
            ozone->dimensions.thumbnail_width = 0.0f;
            ozone->dimensions.thumbnail_height = 0.0f;
            video_driver_texture_unload(&ozone->thumbnail);
         }

         break;
      }
      case MENU_IMAGE_LEFT_THUMBNAIL:
      {
         struct texture_image *img = (struct texture_image*)data;

         if (img->width > 0 && img->height > 0 && display_aspect_ratio > 0.0001f)
         {
            float thumb_aspect_ratio = (float)img->width / (float)img->height;

            if (thumb_aspect_ratio > display_aspect_ratio)
            {
               ozone->dimensions.left_thumbnail_width = (float)maximum_width;
               ozone->dimensions.left_thumbnail_height = (float)img->height * (float)maximum_width / (float)img->width;
            }
            else
            {
               ozone->dimensions.left_thumbnail_height = (float)maximum_height;
               ozone->dimensions.left_thumbnail_width = (float)img->width * (float)maximum_height / (float)img->height;
            }

            video_driver_texture_unload(&ozone->left_thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &ozone->left_thumbnail);
         }
         else
         {
            ozone->dimensions.left_thumbnail_width = 0.0f;
            ozone->dimensions.left_thumbnail_height = 0.0f;
            video_driver_texture_unload(&ozone->left_thumbnail);
         }

         break;
      }
      default:
         break;
   }
   return true;
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
   NULL,
   NULL,
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
   ozone_list_bind_init,
   ozone_load_image,
   "ozone",
   ozone_environ_cb,
   ozone_update_thumbnail_path,
   ozone_update_thumbnail_image,
   ozone_refresh_thumbnail_image,
   ozone_set_thumbnail_system,
   ozone_get_thumbnail_system,
   ozone_set_thumbnail_content,
   menu_display_osk_ptr_at_pos,
   NULL,                         /* update_savestate_thumbnail_path */
   NULL,                         /* update_savestate_thumbnail_image */
   NULL,                         /* pointer_down */
   ozone_pointer_up,
#ifdef HAVE_MENU_WIDGETS
   ozone_get_load_content_animation_data
#else
   NULL
#endif
};

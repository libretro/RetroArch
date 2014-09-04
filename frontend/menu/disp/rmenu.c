/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../backend/menu_common_backend.h"
#include "../menu_common.h"
#include "../../../general.h"
#include "../../../gfx/gfx_common.h"
#include "../../../config.def.h"
#include "../../../file.h"
#include "../../../dynamic.h"
#include "../../../compat/posix_string.h"
#include "../../../gfx/shader_parse.h"
#include "../../../performance.h"
#include "../../../input/input_common.h"

#include "../../../settings_data.h"
#include "../../../screenshot.h"
#include "../../../gfx/fonts/bitmap.h"

#include "shared.h"

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
#define HAVE_SHADER_MANAGER
#endif

#if defined(_XBOX1)
#define ENTRIES_HEIGHT 9
#define POSITION_EDGE_MAX (480)
#define POSITION_EDGE_MIN 6
#define POSITION_EDGE_CENTER (425)
#define POSITION_OFFSET 30
#define POSITION_RENDER_OFFSET 128
#define RMENU_TERM_WIDTH 45
#define FONT_SIZE_NORMAL 21
#elif defined(__CELLOS_LV2__)
#define ENTRIES_HEIGHT 20
#define POSITION_MIDDLE 0.50f
#define POSITION_EDGE_MAX 1.00f
#define POSITION_EDGE_MIN 0.00f
#define POSITION_EDGE_CENTER 0.70f
#define POSITION_RENDER_OFFSET 0.20f
#define POSITION_OFFSET 0.03f
#define FONT_SIZE_NORMAL 0.95f
#define RMENU_TERM_WIDTH 60
#endif

struct texture_image *menu_texture;
static bool render_normal = true;
static bool menu_texture_inited =false;

static void rmenu_render_background(void)
{
}

static void rmenu_render_messagebox(const char *message)
{
   struct font_params font_parms;

   size_t i, j;

   if (!message || !*message)
      return;

   struct string_list *list = string_split(message, "\n");
   if (!list)
      return;
   if (list->elems == 0)
   {
      string_list_free(list);
      return;
   }

   j = 0;
   for (i = 0; i < list->size; i++, j++)
   {
      char *msg = list->elems[i].data;
      unsigned msglen = strlen(msg);
      if (msglen > RMENU_TERM_WIDTH)
      {
         msg[RMENU_TERM_WIDTH - 2] = '.';
         msg[RMENU_TERM_WIDTH - 1] = '.';
         msg[RMENU_TERM_WIDTH - 0] = '.';
         msg[RMENU_TERM_WIDTH + 1] = '\0';
         msglen = RMENU_TERM_WIDTH;
      }

      font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
      font_parms.y = POSITION_EDGE_MIN + POSITION_RENDER_OFFSET + (POSITION_OFFSET * j);
      font_parms.scale = FONT_SIZE_NORMAL;
      font_parms.color = WHITE;

      if (driver.video_data && driver.video_poke
            && driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data,
               msg, &font_parms);
   }

   render_normal = false;
}

static void rmenu_render(void)
{
   size_t begin, end;
   struct font_params font_parms;
   menu_handle_t *menu = (menu_handle_t*)driver.menu;

   if (!menu)
      return;

   if (!render_normal)
   {
      render_normal = true;
      return;
   }

   if (menu->need_refresh && 
         (g_extern.lifecycle_state & (1ULL << MODE_MENU))
         && !menu->msg_force)
      return;

   if (!menu->selection_buf)
      return;

   begin = (menu->selection_ptr >= (ENTRIES_HEIGHT / 2)) ? 
      (menu->selection_ptr - (ENTRIES_HEIGHT / 2)) : 0;
   end = ((menu->selection_ptr + ENTRIES_HEIGHT) <= 
         file_list_get_size(menu->selection_buf)) ?
      menu->selection_ptr + ENTRIES_HEIGHT :
      file_list_get_size(menu->selection_buf);

   if (file_list_get_size(menu->selection_buf) <= ENTRIES_HEIGHT)
      begin = 0;

   if (end - begin > ENTRIES_HEIGHT)
      end = begin + ENTRIES_HEIGHT;
   
   rmenu_render_background();

   char title[256];
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   unsigned menu_type_is = 0;
   file_list_get_last(menu->menu_stack, &dir, &label, &menu_type);

   if (driver.menu_ctx && driver.menu_ctx->backend
         && driver.menu_ctx->backend->type_is)
      menu_type_is = driver.menu_ctx->backend->type_is(label, menu_type);

   get_title(label, dir, menu_type, menu_type_is,
         title, sizeof(title));

   char title_buf[256];
   menu_ticker_line(title_buf, RMENU_TERM_WIDTH,
         g_extern.frame_count / 15, title, true);

   font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
   font_parms.y = POSITION_EDGE_MIN + POSITION_RENDER_OFFSET
      - (POSITION_OFFSET*2);
   font_parms.scale = FONT_SIZE_NORMAL;
   font_parms.color = WHITE;

   if (driver.video_data && driver.video_poke
         && driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data,
            title_buf, &font_parms);

   char title_msg[64];
   const char *core_name = menu->info.library_name;
   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   const char *core_version = menu->info.library_version;
   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
   font_parms.y = POSITION_EDGE_MAX - (POSITION_OFFSET*2);
   font_parms.scale = FONT_SIZE_NORMAL;
   font_parms.color = WHITE;

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s",
         PACKAGE_VERSION, core_name, core_version);

   if (driver.video_data && driver.video_poke
         && driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data,
            title_msg, &font_parms);

   size_t i, j;

   j = 0;

   for (i = begin; i < end; i++, j++)
   {
      const char *path = NULL;
      const char *entry_label = NULL;
      unsigned type = 0;
      file_list_get_at_offset(menu->selection_buf, i,
            &path, &entry_label, &type);
      char message[256];
      char type_str[256];

      unsigned w = 19;
      if (menu_type == MENU_SETTINGS_INPUT_OPTIONS ||
            menu_type == MENU_SETTINGS_CUSTOM_BIND)
         w = 21;
      else if (menu_type == MENU_SETTINGS_PATH_OPTIONS)
         w = 24;

#ifdef HAVE_SHADER_MANAGER
      if (type >= MENU_SETTINGS_SHADER_FILTER &&
            type <= MENU_SETTINGS_SHADER_LAST)
      {
         /* HACK. Work around that we're using the menu_type as 
          * dir type to propagate state correctly. */
         if ((menu_type_is == MENU_SETTINGS_SHADER_OPTIONS)
               && (menu_type_is == MENU_SETTINGS_SHADER_OPTIONS))
         {
            type = MENU_FILE_DIRECTORY;
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            w = 5;
         }
         else if (
               type == MENU_SETTINGS_SHADER_OPTIONS ||
               type == MENU_SETTINGS_SHADER_PRESET ||
               type == MENU_SETTINGS_SHADER_PARAMETERS ||
               type == MENU_SETTINGS_SHADER_PRESET_PARAMETERS)
            strlcpy(type_str, "...", sizeof(type_str));
         else if (type == MENU_SETTINGS_SHADER_FILTER)
            snprintf(type_str, sizeof(type_str), "%s",
                  g_settings.video.smooth ? "Linear" : "Nearest");
         else if (driver.menu_ctx && driver.menu_ctx->backend
               && driver.menu_ctx->backend->shader_manager_get_str)
         {
            if (type >= MENU_SETTINGS_SHADER_PARAMETER_0 &&
                  type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
               driver.menu_ctx->backend->shader_manager_get_str(
                     menu->parameter_shader, type_str, sizeof(type_str), type);
            else
               driver.menu_ctx->backend->shader_manager_get_str(
                     menu->shader, type_str, sizeof(type_str), type);
         }
      }
      else
#endif
      if (type == MENU_FILE_CORE)
      {
         strlcpy(type_str, "(CORE)", sizeof(type_str));
         file_list_get_alt_at_offset(driver.menu->selection_buf, i, &path);
         w = 6;
      }
      else if (type == MENU_FILE_PLAIN)
      {
         strlcpy(type_str, "(FILE)", sizeof(type_str));
         w = 6;
      }
      else if (type == MENU_FILE_USE_DIRECTORY)
      {
         *type_str = '\0';
         w = 0;
      }
      else if (type == MENU_FILE_DIRECTORY)
      {
         strlcpy(type_str, "(DIR)", sizeof(type_str));
         type = MENU_FILE_DIRECTORY;
         w = 5;
      }
      else if (type >= MENU_SETTINGS_CORE_OPTION_START)
         strlcpy(type_str,
               core_option_get_val(g_extern.system.core_options,
                  type - MENU_SETTINGS_CORE_OPTION_START),
               sizeof(type_str));
      else if (type == MENU_FILE_SWITCH)
         strlcpy(type_str, "...", sizeof(type_str));
      else if (driver.menu_ctx && driver.menu_ctx->backend
            && driver.menu_ctx->backend->setting_set_label)
         driver.menu_ctx->backend->setting_set_label(type_str,
               sizeof(type_str), &w, type, i);

      char entry_title_buf[256];
      char type_str_buf[64];
      bool selected = i == menu->selection_ptr;

      menu_ticker_line(entry_title_buf, RMENU_TERM_WIDTH - (w + 1 + 2),
            g_extern.frame_count / 15, path, selected);
      menu_ticker_line(type_str_buf, w, g_extern.frame_count / 15,
            type_str, selected);

      snprintf(message, sizeof(message), "%c %s",
            selected ? '>' : ' ', entry_title_buf);

      //blit_line(menu, x, y, message, selected);
      font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
      font_parms.y = POSITION_EDGE_MIN + POSITION_RENDER_OFFSET
         + (POSITION_OFFSET * j);
      font_parms.scale = FONT_SIZE_NORMAL;
      font_parms.color = WHITE;

      if (driver.video_data && driver.video_poke
            && driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data,
               message, &font_parms);

      font_parms.x = POSITION_EDGE_CENTER + POSITION_OFFSET;

      if (driver.video_data && driver.video_poke
            && driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data,
               type_str_buf, &font_parms);
   }
}

void rmenu_set_texture(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu_texture_inited)
      return;

   if (driver.video_data && driver.video_poke
         && driver.video_poke->set_texture_enable
         && menu_texture && menu_texture->pixels)
   {
      driver.video_poke->set_texture_frame(driver.video_data,
            menu_texture->pixels,
            true, menu->width, menu->height, 1.0f);
      menu_texture_inited = true;
   }
}

static void rmenu_context_reset(void *data)
{
   char menu_bg[PATH_MAX];
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;

   fill_pathname_join(menu_bg, g_settings.assets_directory,
         "rmenu", sizeof(menu_bg));
#ifdef _XBOX1
   fill_pathname_join(menu_bg, menu_bg, "sd", sizeof(menu_bg));
#else
   fill_pathname_join(menu_bg, menu_bg, "hd", sizeof(menu_bg));
#endif
   fill_pathname_join(menu_bg, menu_bg, "main_menu.png", sizeof(menu_bg));

   if (path_file_exists(menu_bg))
      texture_image_load(menu_texture, menu_bg);
   menu->width = menu_texture->width;
   menu->height = menu_texture->height;

   menu_texture_inited = false;
}

static void *rmenu_init(void)
{
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   menu_texture = (struct texture_image*)calloc(1, sizeof(*menu_texture));
   return menu;
}

static void rmenu_context_destroy(void *data)
{
   texture_image_free(menu_texture);
}

static void rmenu_free(void *data)
{
}

static int rmenu_input_postprocess(uint64_t old_state)
{
   menu_handle_t *menu = (menu_handle_t*)driver.menu;

   if ((menu->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      rarch_main_command(RARCH_CMD_RESUME);
      return -1;
   }

   return 0;
}

const menu_ctx_driver_t menu_ctx_rmenu = {
   rmenu_set_texture,
   rmenu_render_messagebox,
   rmenu_render,
   NULL,
   rmenu_init,
   rmenu_free,
   rmenu_context_reset,
   rmenu_context_destroy,
   NULL,
   NULL,
   rmenu_input_postprocess,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   &menu_ctx_backend_common,
   "rmenu",
};

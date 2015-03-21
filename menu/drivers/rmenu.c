/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "../menu_driver.h"
#include "../menu_input.h"
#include "../menu.h"
#include "../../general.h"
#include "../../config.def.h"
#include <compat/posix_string.h>
#include "../../performance.h"

#include "../../settings.h"
#include "../../screenshot.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"

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

static int rmenu_entry_iterate(unsigned action)
{
   const char *label = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;
   
   cbs = (menu_file_list_cbs_t*)menu_list_get_actiondata_at_offset(
         menu->menu_list->selection_buf, menu->navigation.selection_ptr);

   menu_list_get_last_stack(menu->menu_list, NULL, &label, NULL);

   if (cbs && cbs->action_iterate)
      return cbs->action_iterate(label, action);
   
   return -1;
}

static void rmenu_render_background(void)
{
}

static void rmenu_render_messagebox(const char *message)
{
   struct font_params font_parms;
   size_t i, j;
   struct string_list *list = NULL;
   menu_handle_t *menu   = menu_driver_get_ptr();
   driver_t      *driver = driver_get_ptr();

   if (!menu)
      return;

   if (!message || !*message)
      return;

   list = string_split(message, "\n");

   if (!list)
      return;

   if (list->elems == 0)
      goto end;

   j = 0;

   for (i = 0; i < list->size; i++, j++)
   {
      char *msg       = list->elems[i].data;
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

      if (driver->video_data && driver->video_poke
            && driver->video_poke->set_osd_msg)
         driver->video_poke->set_osd_msg(driver->video_data,
               msg, &font_parms, NULL);
   }

   render_normal = false;
end:
   string_list_free(list);
}

static void rmenu_render(void)
{
   size_t begin, end, i, j;
   struct font_params font_parms;
   char title[256], title_buf[256];
   char title_msg[64];
   const char *dir          = NULL;
   const char *label        = NULL;
   const char *core_name    = NULL;
   const char *core_version = NULL;
   unsigned menu_type       = 0;
   menu_handle_t *menu      = menu_driver_get_ptr();
   global_t    *global      = global_get_ptr();
   driver_t    *driver      = driver_get_ptr();
   runloop_t *runloop       = rarch_main_get_ptr();

   if (!menu)
      return;

   if (!render_normal)
   {
      render_normal = true;
      return;
   }

   if (menu->need_refresh && runloop->is_menu
         && !menu->msg_force)
      return;

   runloop->frames.video.current.menu.animation.is_active = false;
   runloop->frames.video.current.menu.label.is_updated    = false;
   runloop->frames.video.current.menu.framebuf.dirty      = false;

   if (!menu->menu_list->selection_buf)
      return;

   begin = (menu->navigation.selection_ptr >= (ENTRIES_HEIGHT / 2)) ? 
      (menu->navigation.selection_ptr - (ENTRIES_HEIGHT / 2)) : 0;
   end   = ((menu->navigation.selection_ptr + ENTRIES_HEIGHT) <= 
         menu_list_get_size(menu->menu_list)) ?
      menu->navigation.selection_ptr + ENTRIES_HEIGHT :
      menu_list_get_size(menu->menu_list);

   if (menu_list_get_size(menu->menu_list) <= ENTRIES_HEIGHT)
      begin = 0;

   if (end - begin > ENTRIES_HEIGHT)
      end = begin + ENTRIES_HEIGHT;
   
   rmenu_render_background();

   menu_list_get_last_stack(menu->menu_list, &dir, &label, &menu_type);

   get_title(label, dir, menu_type, title, sizeof(title));

   menu_animation_ticker_line(title_buf, RMENU_TERM_WIDTH,
         runloop->frames.video.count / 15, title, true);

   font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
   font_parms.y = POSITION_EDGE_MIN + POSITION_RENDER_OFFSET
      - (POSITION_OFFSET*2);
   font_parms.scale = FONT_SIZE_NORMAL;
   font_parms.color = WHITE;

   if (driver->video_data && driver->video_poke
         && driver->video_poke->set_osd_msg)
      driver->video_poke->set_osd_msg(driver->video_data,
            title_buf, &font_parms, NULL);

   core_name = global->menu.info.library_name;
   if (!core_name)
      core_name = global->system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   core_version = global->menu.info.library_version;
   if (!core_version)
      core_version = global->system.info.library_version;
   if (!core_version)
      core_version = "";

   font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
   font_parms.y = POSITION_EDGE_MAX - (POSITION_OFFSET*2);
   font_parms.scale = FONT_SIZE_NORMAL;
   font_parms.color = WHITE;

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s",
         PACKAGE_VERSION, core_name, core_version);

   if (driver->video_data && driver->video_poke
         && driver->video_poke->set_osd_msg)
      driver->video_poke->set_osd_msg(driver->video_data,
            title_msg, &font_parms, NULL);

   j = 0;

   for (i = begin; i < end; i++, j++)
   {
      char message[PATH_MAX_LENGTH], type_str[PATH_MAX_LENGTH],
           entry_title_buf[PATH_MAX_LENGTH], type_str_buf[PATH_MAX_LENGTH],
           path_buf[PATH_MAX_LENGTH];
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;
      bool selected = false;
      menu_file_list_cbs_t *cbs = NULL;

      menu_list_get_at_offset(menu->menu_list->selection_buf, i,
            &path, &entry_label, &type);

      cbs = (menu_file_list_cbs_t*)
         menu_list_get_actiondata_at_offset(menu->menu_list->selection_buf,
               i);

      if (cbs && cbs->action_get_representation)
         cbs->action_get_representation(menu->menu_list->selection_buf,
               &w, type, i, label,
               type_str, sizeof(type_str), 
               entry_label, path,
               path_buf, sizeof(path_buf));

      selected = (i == menu->navigation.selection_ptr);

      menu_animation_ticker_line(entry_title_buf, RMENU_TERM_WIDTH - (w + 1 + 2),
            runloop->frames.video.count / 15, path, selected);
      menu_animation_ticker_line(type_str_buf, w, runloop->frames.video.count / 15,
            type_str, selected);

      snprintf(message, sizeof(message), "%c %s",
            selected ? '>' : ' ', entry_title_buf);

#if 0
      blit_line(x, y, message, selected);
#endif
      font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
      font_parms.y = POSITION_EDGE_MIN + POSITION_RENDER_OFFSET
         + (POSITION_OFFSET * j);
      font_parms.scale = FONT_SIZE_NORMAL;
      font_parms.color = WHITE;

      if (driver->video_data && driver->video_poke
            && driver->video_poke->set_osd_msg)
         driver->video_poke->set_osd_msg(driver->video_data,
               message, &font_parms, NULL);

      font_parms.x = POSITION_EDGE_CENTER + POSITION_OFFSET;

      if (driver->video_data && driver->video_poke
            && driver->video_poke->set_osd_msg)
         driver->video_poke->set_osd_msg(driver->video_data,
               type_str_buf, &font_parms, NULL);
   }
}

static void rmenu_set_texture(void)
{
   menu_handle_t *menu   = menu_driver_get_ptr();
   driver_t      *driver = driver_get_ptr();

   if (!menu)
      return;
   if (menu_texture_inited)
      return;
   if (!driver->video_data)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->set_texture_enable)
      return;
   if (!menu_texture)
      return;
   if (!menu_texture->pixels)
      return;

   driver->video_poke->set_texture_frame(
         driver->video_data,
         menu_texture->pixels,
         true, menu->frame_buf.width, menu->frame_buf.height, 1.0f);
   menu_texture_inited = true;
}

static void rmenu_wallpaper_set_defaults(char *menu_bg, size_t sizeof_menu_bg)
{
   settings_t *settings = config_get_ptr();

   fill_pathname_join(menu_bg, settings->assets_directory,
         "rmenu", sizeof_menu_bg);
#ifdef _XBOX1
   fill_pathname_join(menu_bg, menu_bg, "sd", sizeof_menu_bg);
#else
   fill_pathname_join(menu_bg, menu_bg, "hd", sizeof_menu_bg);
#endif
   fill_pathname_join(menu_bg, menu_bg, "main_menu.png", sizeof_menu_bg);
}

static void rmenu_context_reset(void)
{
   char menu_bg[PATH_MAX_LENGTH];
   menu_handle_t *menu  = menu_driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!menu)
      return;

   if (*settings->menu.wallpaper)
      strlcpy(menu_bg, settings->menu.wallpaper, sizeof(menu_bg));
   else
      rmenu_wallpaper_set_defaults(menu_bg, sizeof(menu_bg));

   if (path_file_exists(menu_bg))
      texture_image_load(menu_texture, menu_bg);
   menu->frame_buf.width = menu_texture->width;
   menu->frame_buf.height = menu_texture->height;

   menu_texture_inited = false;
}

static void *rmenu_init(void)
{
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   menu_texture = (struct texture_image*)calloc(1, sizeof(*menu_texture));

   if (!menu_texture)
   {
      free(menu);
      return NULL;
   }

   return menu;
}

static void rmenu_context_destroy(void)
{
   texture_image_free(menu_texture);
}

static void rmenu_free(void *data)
{
}

menu_ctx_driver_t menu_ctx_rmenu = {
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
   rmenu_entry_iterate,
   NULL,
   "rmenu",
};

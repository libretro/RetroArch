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

#include <compat/posix_string.h>
#include <string/string_list.h>

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_entry.h"
#include "../menu_input.h"
#include "../menu_setting.h"
#include "../menu_display.h"
#include "../menu_navigation.h"
#include "../../general.h"
#include "../../config.def.h"
#include "../../performance.h"

#include "../../screenshot.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"


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

static void rmenu_render_messagebox(void *data, const char *message)
{
   struct font_params font_parms;
   size_t i, j;
   struct string_list *list = NULL;

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

      video_driver_set_osd_msg(msg, &font_parms, NULL);
   }

   render_normal = false;
end:
   string_list_free(list);
}

static void rmenu_render(void *data)
{
   bool msg_force;
   uint64_t *frame_count;
   size_t begin, end, i, j, selection;
   struct font_params font_parms;
   char title[256]               = {0};
   char title_buf[256]           = {0};
   char title_msg[64]            = {0};
   size_t  entries_end           = menu_entries_get_end();

   video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   if (!render_normal)
   {
      render_normal = true;
      return;
   }

   menu_display_ctl(MENU_DISPLAY_CTL_MSG_FORCE, &msg_force);

   if (menu_entries_needs_refresh()
         && menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL)
         && !msg_force)
      return;

   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_FRAMEBUFFER_DIRTY_FLAG, NULL);
   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);

   begin = (selection >= (ENTRIES_HEIGHT / 2)) ? 
      (selection - (ENTRIES_HEIGHT / 2)) : 0;
   end   = ((selection + ENTRIES_HEIGHT) <= entries_end)
      ? selection + ENTRIES_HEIGHT : entries_end;

   if (entries_end <= ENTRIES_HEIGHT)
      begin = 0;

   if (end - begin > ENTRIES_HEIGHT)
      end = begin + ENTRIES_HEIGHT;
   
   menu_entries_get_title(title, sizeof(title));

   menu_animation_ticker_str(title_buf, RMENU_TERM_WIDTH,
         *frame_count / 15, title, true);

   font_parms.x        = POSITION_EDGE_MIN + POSITION_OFFSET;
   font_parms.y        = POSITION_EDGE_MIN + POSITION_RENDER_OFFSET
      - (POSITION_OFFSET*2);
   font_parms.scale    = FONT_SIZE_NORMAL;
   font_parms.color    = WHITE;
   font_parms.drop_mod = 0.0f;
   font_parms.drop_x   = 0.0f;
   font_parms.drop_y   = 0.0f;

   video_driver_set_osd_msg(title_buf, &font_parms, NULL);

   font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
   font_parms.y = POSITION_EDGE_MAX - (POSITION_OFFSET*2);
   font_parms.scale = FONT_SIZE_NORMAL;
   font_parms.color = WHITE;

   menu_entries_get_core_title(title_msg, sizeof(title_msg));

   video_driver_set_osd_msg(title_msg, &font_parms, NULL);

   j = 0;

   for (i = begin; i < end; i++, j++)
   {
      char entry_path[PATH_MAX_LENGTH]      = {0};
      char entry_value[PATH_MAX_LENGTH]     = {0};
      char message[PATH_MAX_LENGTH]         = {0};
      char entry_title_buf[PATH_MAX_LENGTH] = {0};
      char type_str_buf[PATH_MAX_LENGTH]    = {0};
      unsigned entry_spacing                = menu_entry_get_spacing(i);
      bool entry_selected                   = menu_entry_is_currently_selected(i);

      menu_entry_get_value(i, entry_value, sizeof(entry_value));
      menu_entry_get_path(i, entry_path, sizeof(entry_path));

      menu_animation_ticker_str(entry_title_buf, RMENU_TERM_WIDTH - (entry_spacing + 1 + 2),
            *frame_count / 15, entry_path, entry_selected);
      menu_animation_ticker_str(type_str_buf, entry_spacing,
            *frame_count / 15, entry_value, entry_selected);

      snprintf(message, sizeof(message), "%c %s",
            entry_selected ? '>' : ' ', entry_title_buf);

      font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
      font_parms.y = POSITION_EDGE_MIN + POSITION_RENDER_OFFSET
         + (POSITION_OFFSET * j);
      font_parms.scale = FONT_SIZE_NORMAL;
      font_parms.color = WHITE;

      video_driver_set_osd_msg(message, &font_parms, NULL);

      font_parms.x = POSITION_EDGE_CENTER + POSITION_OFFSET;

      video_driver_set_osd_msg(type_str_buf, &font_parms, NULL);
   }
}

static void rmenu_set_texture(void)
{
   unsigned fb_width, fb_height;
   menu_handle_t      *menu   = menu_driver_get_ptr();

   if (!menu)
      return;
   if (menu_texture_inited)
      return;
   if (!menu_texture)
      return;
   if (!menu_texture->pixels)
      return;

   menu_display_ctl(MENU_DISPLAY_CTL_WIDTH,  &fb_width);
   menu_display_ctl(MENU_DISPLAY_CTL_HEIGHT, &fb_height);

   video_driver_set_texture_frame(menu_texture->pixels, true,
         fb_width, fb_height, 1.0f);
   menu_texture_inited = true;
}

static void rmenu_wallpaper_set_defaults(char *s, size_t len)
{
   settings_t *settings = config_get_ptr();

   fill_pathname_join(s, settings->assets_directory,
         "rmenu", len);
#ifdef _XBOX1
   fill_pathname_join(s, s, "sd", len);
#else
   fill_pathname_join(s, s, "hd", len);
#endif
   fill_pathname_join(s, s, "main_menu.png", len);
}

static void rmenu_context_reset(void)
{
   char menu_bg[PATH_MAX_LENGTH] = {0};
   menu_handle_t *menu           = menu_driver_get_ptr();
   settings_t *settings          = config_get_ptr();

   if (!menu)
      return;

   if (*settings->menu.wallpaper)
      strlcpy(menu_bg, settings->menu.wallpaper, sizeof(menu_bg));
   else
      rmenu_wallpaper_set_defaults(menu_bg, sizeof(menu_bg));

   if (path_file_exists(menu_bg))
      texture_image_load(menu_texture, menu_bg);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_WIDTH,  &menu_texture->width);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEIGHT, &menu_texture->height);

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

static int rmenu_environ(menu_environ_cb_t type, void *data)
{
   switch (type)
   {
      case 0:
         break;
      default:
         return -1;
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_rmenu = {
   rmenu_set_texture,
   rmenu_render_messagebox,
   generic_menu_iterate,
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
   generic_menu_init_list,
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
   "rmenu",
   rmenu_environ,
   NULL,
};

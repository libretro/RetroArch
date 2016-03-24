/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
 *  Copyright (C) 2016      - Andrés Suárez
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

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include <retro_assert.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <formats/image.h>
#include <gfx/math/matrix_4x4.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#include "menu_generic.h"
#include "zr_menu.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_navigation.h"
#include "../menu_hash.h"
#include "../menu_display.h"

#include "../../core_info.h"
#include "../../configuration.h"
#include "../../frontend/frontend_driver.h"
#include "../../system.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

static void zrmenu_main(zrmenu_handle_t *zr)
{
   struct zr_context *ctx = &zr->ctx;

   if (zr->window[ZRMENU_WND_MAIN].open)
      zrmenu_wnd_main(zr);
   if (zr->window[ZRMENU_WND_CONTROL].open)
      zrmenu_wnd_control(zr);
   if (zr->window[ZRMENU_WND_SHADER_PARAMETERS].open)
      zrmenu_wnd_shader_parameters(zr);
   if (zr->window[ZRMENU_WND_TEST].open)
      zrmenu_wnd_test(zr);
   if (zr->window[ZRMENU_WND_WIZARD].open)
      zrmenu_wnd_wizard(zr);

   zr->window[ZRMENU_WND_CONTROL].open = !zr_window_is_closed(ctx, "Control");
   zr->window[ZRMENU_WND_SHADER_PARAMETERS].open = !zr_window_is_closed(ctx, "Shader Parameters");
   zr->window[ZRMENU_WND_TEST].open = !zr_window_is_closed(ctx, "Test");
   zr->window[ZRMENU_WND_WIZARD].open = !zr_window_is_closed(ctx, "Setup Wizard");

   if(zr_window_is_closed(ctx, "Setup Wizard"))
      zr->window[ZRMENU_WND_MAIN].open = true;

   zr_buffer_info(&zr->status, &zr->ctx.memory);
}

static void zrmenu_input_gamepad(zrmenu_handle_t *zr)
{
   switch (zr->action)
   {
      case MENU_ACTION_LEFT:
         zr_input_key(&zr->ctx, ZR_KEY_LEFT, 1);
         break;
      case MENU_ACTION_RIGHT:
         zr_input_key(&zr->ctx, ZR_KEY_RIGHT, 1);
         break;
      case MENU_ACTION_DOWN:
         zr_input_key(&zr->ctx, ZR_KEY_DOWN, 1);
         break;
      case MENU_ACTION_UP:
         zr_input_key(&zr->ctx, ZR_KEY_UP, 1);
         break;
      default:
         zr_input_key(&zr->ctx, ZR_KEY_UP, 0);
         zr_input_key(&zr->ctx, ZR_KEY_DOWN, 0);
         zr_input_key(&zr->ctx, ZR_KEY_LEFT, 0);
         zr_input_key(&zr->ctx, ZR_KEY_RIGHT, 0);
         break;
   }

}

static void zrmenu_input_mouse_movement(struct zr_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   zr_input_motion(ctx, mouse_x, mouse_y);
   zr_input_scroll(ctx, menu_input_mouse_state(MENU_MOUSE_WHEEL_UP) -
      menu_input_mouse_state(MENU_MOUSE_WHEEL_DOWN));

}

static void zrmenu_input_mouse_button(struct zr_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   zr_input_button(ctx, ZR_BUTTON_LEFT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON));
   zr_input_button(ctx, ZR_BUTTON_RIGHT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON));
}

static void zrmenu_input_keyboard(struct zr_context *ctx)
{
   /* placeholder, it just presses 1 on right click
      needs to be hooked up correctly
   */
   if(menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON))
      zr_input_char(ctx, '1');
}

static void zrmenu_context_reset_textures(zrmenu_handle_t *zr,
      const char *iconpath)
{
   unsigned i;

   for (i = 0; i < ZR_TEXTURE_LAST; i++)
   {
      struct texture_image ti     = {0};
      char path[PATH_MAX_LENGTH]  = {0};

      switch(i)
      {
         case ZR_TEXTURE_POINTER:
            fill_pathname_join(path, iconpath,
                  "pointer.png", sizeof(path));
            break;
      }

      if (string_is_empty(path) || !path_file_exists(path))
         continue;

      video_texture_image_load(&ti, path);
      video_driver_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_LINEAR, &zr->textures.list[i]);

      video_texture_image_free(&ti);
   }
}

static void zrmenu_get_message(void *data, const char *message)
{
   zrmenu_handle_t *zr   = (zrmenu_handle_t*)data;

   if (!zr || !message || !*message)
      return;

   strlcpy(zr->box_message, message, sizeof(zr->box_message));
}

static void zrmenu_draw_cursor(zrmenu_handle_t *zr,
      float *color,
      float x, float y, unsigned width, unsigned height)
{
   menu_display_ctx_draw_t draw;
   struct gfx_coords coords;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_BEGIN, NULL);

   draw.x           = x - 32;
   draw.y           = (int)height - y - 32;
   draw.width       = 64;
   draw.height      = 64;
   draw.coords      = &coords;
   draw.matrix_data = NULL;
   draw.texture     = zr->textures.list[ZR_TEXTURE_POINTER];
   draw.prim_type   = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

   menu_display_ctl(MENU_DISPLAY_CTL_DRAW, &draw);

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_END, NULL);
}

static void zrmenu_frame(void *data)
{
   float white_bg[16]=  {
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
   };

   unsigned width, height, ticker_limit, i;
   zrmenu_handle_t *zr = (zrmenu_handle_t*)data;
   settings_t *settings  = config_get_ptr();

   bool libretro_running = menu_display_ctl(
         MENU_DISPLAY_CTL_LIBRETRO_RUNNING, NULL);

   if (!zr)
      return;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   zr_input_begin(&zr->ctx);
   zrmenu_input_gamepad(zr);
   zrmenu_input_mouse_movement(&zr->ctx);
   zrmenu_input_mouse_button(&zr->ctx);
   zrmenu_input_keyboard(&zr->ctx);

   if (width != zr->size.x || height != zr->size.y)
   {
      zr->size.x = width;
      zr->size.y = height;
      zr->size_changed = true;
   }

   zr_input_end(&zr->ctx);
   zrmenu_main(zr);
   zr_common_device_draw(&device, &zr->ctx, width, height, ZR_ANTI_ALIASING_ON);

   if (settings->menu.mouse.enable && (settings->video.fullscreen
            || !video_driver_ctl(RARCH_DISPLAY_CTL_HAS_WINDOWED, NULL)))
   {
      int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
      int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      zrmenu_draw_cursor(zr, &white_bg[0], mouse_x, mouse_y, width, height);
   }

   menu_display_ctl(MENU_DISPLAY_CTL_RESTORE_CLEAR_COLOR, NULL);
   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}

static void zrmenu_layout(zrmenu_handle_t *zr)
{
   void *fb_buf;
   float scale_factor;
   unsigned width, height, new_header_height;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_GET_DPI, &scale_factor);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEADER_HEIGHT,
         &new_header_height);

}

static void zrmenu_init_device(zrmenu_handle_t *zr)
{
   char buf[PATH_MAX_LENGTH];
   fill_pathname_join(buf, zr->assets_directory,
         "DroidSans.ttf", sizeof(buf));

   zr_alloc.userdata.ptr = NULL;
   zr_alloc.alloc = zr_common_mem_alloc;
   zr_alloc.free = zr_common_mem_free;
   zr_buffer_init(&device.cmds, &zr_alloc, 1024);
   usrfnt = zr_common_font(&device, &font, buf, 16,
      zr_font_default_glyph_ranges());
   zr_init(&zr->ctx, &zr_alloc, &usrfnt);
   zr_common_device_init(&device);

   fill_pathname_join(buf, zr->assets_directory, "folder.png", sizeof(buf));
   zr->icons.folder = zr_common_image_load(buf);
   fill_pathname_join(buf, zr->assets_directory, "speaker.png", sizeof(buf));
   zr->icons.speaker = zr_common_image_load(buf);
   fill_pathname_join(buf, zr->assets_directory, "gamepad.png", sizeof(buf));
   zr->icons.gamepad = zr_common_image_load(buf);
   fill_pathname_join(buf, zr->assets_directory, "monitor.png", sizeof(buf));
   zr->icons.monitor = zr_common_image_load(buf);
   fill_pathname_join(buf, zr->assets_directory, "settings.png", sizeof(buf));
   zr->icons.settings = zr_common_image_load(buf);
   fill_pathname_join(buf, zr->assets_directory, "invader.png", sizeof(buf));
   zr->icons.invader = zr_common_image_load(buf);
   fill_pathname_join(buf, zr->assets_directory, "page_on.png", sizeof(buf));
   zr->icons.page_on = zr_common_image_load(buf);
   fill_pathname_join(buf, zr->assets_directory, "page_off.png", sizeof(buf));
   zr->icons.page_off = zr_common_image_load(buf);

   zrmenu_set_style(&zr->ctx, THEME_DARK);
   zr->size_changed = true;
}

static void *zrmenu_init(void **userdata)
{
   settings_t *settings = config_get_ptr();
   zrmenu_handle_t   *zr = NULL;
   menu_handle_t *menu = (menu_handle_t*)
      calloc(1, sizeof(*menu));
   unsigned width, height = 0;

   video_driver_get_size(&width, &height);

   if (!menu)
      goto error;

   if (!menu_display_ctl(MENU_DISPLAY_CTL_INIT_FIRST_DRIVER, NULL))
      goto error;

   zr = (zrmenu_handle_t*)calloc(1, sizeof(zrmenu_handle_t));

   if (!zr)
      goto error;

   *userdata = zr;

   fill_pathname_join(zr->assets_directory, settings->assets_directory,
         "zahnrad", sizeof(zr->assets_directory));
   zrmenu_init_device(zr);

   zr->window[ZRMENU_WND_WIZARD].open = true;

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void zrmenu_free(void *data)
{
   zrmenu_handle_t *zr   = (zrmenu_handle_t*)data;

   if (!zr)
      return;

   free(font.glyphs);
   zr_free(&zr->ctx);
   zr_buffer_free(&device.cmds);
   zr_common_device_shutdown(&device);

   gfx_coord_array_free(&zr->list_block.carr);
   font_driver_bind_block(NULL, NULL);
}

static void wimp_context_bg_destroy(zrmenu_handle_t *zr)
{
   if (!zr)
      return;

}

static void zrmenu_context_destroy(void *data)
{
   unsigned i;
   zrmenu_handle_t *zr   = (zrmenu_handle_t*)data;

   if (!zr)
      return;

   for (i = 0; i < ZR_TEXTURE_LAST; i++)
      video_driver_texture_unload((uintptr_t*)&zr->textures.list[i]);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_MAIN_DEINIT, NULL);

   wimp_context_bg_destroy(zr);
}

static void zrmenu_context_reset(void *data)
{
   char iconpath[PATH_MAX_LENGTH] = {0};
   zrmenu_handle_t *zr              = (zrmenu_handle_t*)data;
   settings_t *settings           = config_get_ptr();
   unsigned width, height = 0;

   video_driver_get_size(&width, &height);

   if (!zr || !settings)
      return;

   fill_pathname_join(iconpath, settings->assets_directory,
         "zahnrad", sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   zrmenu_layout(zr);
   zrmenu_init_device(zr);

   wimp_context_bg_destroy(zr);
   zrmenu_context_reset_textures(zr, iconpath);

   rarch_task_push_image_load(settings->menu.wallpaper, "cb_menu_wallpaper",
         menu_display_handle_wallpaper_upload, NULL);
}

static int zrmenu_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   switch (type)
   {
      case 0:
      default:
         break;
   }

   return -1;
}

static bool zrmenu_init_list(void *data)
{
   menu_displaylist_info_t info = {0};
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   strlcpy(info.label,
         menu_hash_to_str(MENU_VALUE_HISTORY_TAB), sizeof(info.label));

   menu_entries_push(menu_stack,
         info.path, info.label, info.type, info.flags, 0);

   event_cmd_ctl(EVENT_CMD_HISTORY_INIT, NULL);

   info.list  = selection_buf;

   if (menu_displaylist_ctl(DISPLAYLIST_HISTORY, &info))
   {
      info.need_push = true;
      return menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info);
   }

   return false;
}

static int zrmenu_iterate(void *data, void *userdata, enum menu_action action)
{
   int ret;
   size_t selection;
   menu_entry_t entry;
   zrmenu_handle_t *zr   = (zrmenu_handle_t*)userdata;

   if (!zr)
      return -1;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return 0;

   menu_entry_get(&entry, 0, selection, NULL, false);

   zr->action       = action;

   ret = menu_entry_action(&entry, selection, action);
   if (ret)
      return -1;
   return 0;
}

menu_ctx_driver_t menu_ctx_zr = {
   NULL,
   zrmenu_get_message,
   zrmenu_iterate,
   NULL,
   zrmenu_frame,
   zrmenu_init,
   zrmenu_free,
   zrmenu_context_reset,
   zrmenu_context_destroy,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   zrmenu_init_list,
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
   "zahnrad",
   zrmenu_environ,
   NULL,
};

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
#include "nk_menu.h"

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

static void nk_menu_main(nk_menu_handle_t *nk)
{
   struct nk_context *ctx = &nk->ctx;

   if (nk->window[ZRMENU_WND_MAIN].open)
      nk_menu_wnd_main(nk);
   if (nk->window[ZRMENU_WND_SHADER_PARAMETERS].open)
      nk_menu_wnd_shader_parameters(nk);
   if (nk->window[ZRMENU_WND_TEST].open)
      nk_menu_wnd_test(nk);

   nk->window[ZRMENU_WND_SHADER_PARAMETERS].open = !nk_window_is_closed(ctx, "Shader Parameters");
   nk->window[ZRMENU_WND_TEST].open = !nk_window_is_closed(ctx, "Test");

   nk_buffer_info(&nk->status, &nk->ctx.memory);
}

static void nk_menu_input_gamepad(nk_menu_handle_t *nk)
{
   switch (nk->action)
   {
      case MENU_ACTION_LEFT:
         nk_input_key(&nk->ctx, NK_KEY_LEFT, 1);
         break;
      case MENU_ACTION_RIGHT:
         nk_input_key(&nk->ctx, NK_KEY_RIGHT, 1);
         break;
      case MENU_ACTION_DOWN:
         nk_input_key(&nk->ctx, NK_KEY_DOWN, 1);
         break;
      case MENU_ACTION_UP:
         nk_input_key(&nk->ctx, NK_KEY_UP, 1);
         break;
      default:
         nk_input_key(&nk->ctx, NK_KEY_UP, 0);
         nk_input_key(&nk->ctx, NK_KEY_DOWN, 0);
         nk_input_key(&nk->ctx, NK_KEY_LEFT, 0);
         nk_input_key(&nk->ctx, NK_KEY_RIGHT, 0);
         break;
   }

}

static void nk_menu_input_mouse_movement(struct nk_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   nk_input_motion(ctx, mouse_x, mouse_y);
   nk_input_scroll(ctx, menu_input_mouse_state(MENU_MOUSE_WHEEL_UP) -
      menu_input_mouse_state(MENU_MOUSE_WHEEL_DOWN));

}

static void nk_menu_input_mouse_button(struct nk_context *ctx)
{
   int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   nk_input_button(ctx, NK_BUTTON_LEFT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON));
   nk_input_button(ctx, NK_BUTTON_RIGHT,
         mouse_x, mouse_y, menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON));
}

static void nk_menu_input_keyboard(struct nk_context *ctx)
{
   /* placeholder, it just presses 1 on right click
      needs to be hooked up correctly
   */
   if(menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON))
      nk_input_char(ctx, '1');
}

static void nk_menu_context_reset_textures(nk_menu_handle_t *nk,
      const char *iconpath)
{
   unsigned i;

   for (i = 0; i < NK_TEXTURE_LAST; i++)
   {
      struct texture_image ti     = {0};
      char path[PATH_MAX_LENGTH]  = {0};

      switch(i)
      {
         case NK_TEXTURE_POINTER:
            fill_pathname_join(path, iconpath,
                  "pointer.png", sizeof(path));
            break;
      }

      if (string_is_empty(path) || !path_file_exists(path))
         continue;

      video_texture_image_load(&ti, path);
      video_driver_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_LINEAR, &nk->textures.list[i]);

      video_texture_image_free(&ti);
   }
}

static void nk_menu_get_message(void *data, const char *message)
{
   nk_menu_handle_t *nk   = (nk_menu_handle_t*)data;

   if (!nk || !message || !*message)
      return;

   strlcpy(nk->box_message, message, sizeof(nk->box_message));
}

static void nk_menu_draw_cursor(nk_menu_handle_t *nk,
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
   draw.texture     = nk->textures.list[NK_TEXTURE_POINTER];
   draw.prim_type   = MENU_DISPLAY_PRIM_TRIANGLESTRIP;

   menu_display_ctl(MENU_DISPLAY_CTL_DRAW, &draw);

   menu_display_ctl(MENU_DISPLAY_CTL_BLEND_END, NULL);
}

static void nk_menu_frame(void *data)
{
   float white_bg[16]=  {
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
   };

   unsigned width, height, ticker_limit, i;
   nk_menu_handle_t *nk = (nk_menu_handle_t*)data;
   settings_t *settings  = config_get_ptr();

   bool libretro_running = menu_display_ctl(
         MENU_DISPLAY_CTL_LIBRETRO_RUNNING, NULL);

   if (!nk)
      return;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   nk_input_begin(&nk->ctx);
   nk_menu_input_gamepad(nk);
   nk_menu_input_mouse_movement(&nk->ctx);
   nk_menu_input_mouse_button(&nk->ctx);
   nk_menu_input_keyboard(&nk->ctx);

   if (width != nk->size.x || height != nk->size.y)
   {
      nk->size.x = width;
      nk->size.y = height;
      nk->size_changed = true;
   }

   nk_input_end(&nk->ctx);
   nk_menu_main(nk);
   if(nk_window_is_closed(&nk->ctx, "Shader Parameters"))
      nk_menu_wnd_shader_parameters(nk);
   nk_common_device_draw(&device, &nk->ctx, width, height, NK_ANTI_ALIASING_ON);

   if (settings->menu.mouse.enable && (settings->video.fullscreen
            || !video_driver_ctl(RARCH_DISPLAY_CTL_HAS_WINDOWED, NULL)))
   {
      int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
      int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      nk_menu_draw_cursor(nk, &white_bg[0], mouse_x, mouse_y, width, height);
   }

   menu_display_ctl(MENU_DISPLAY_CTL_RESTORE_CLEAR_COLOR, NULL);
   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}

static void nk_menu_layout(nk_menu_handle_t *nk)
{
   void *fb_buf;
   float scale_factor;
   unsigned width, height, new_header_height;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_GET_DPI, &scale_factor);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEADER_HEIGHT,
         &new_header_height);

}

static void nk_menu_init_device(nk_menu_handle_t *nk)
{
   char buf[PATH_MAX_LENGTH];
   fill_pathname_join(buf, nk->assets_directory,
         "DroidSans.ttf", sizeof(buf));

   nk_alloc.userdata.ptr = NULL;
   nk_alloc.alloc = nk_common_mem_alloc;
   nk_alloc.free = nk_common_mem_free;
   nk_buffer_init(&device.cmds, &nk_alloc, 1024);
   const void *image; int w, h;
   nk_font_atlas_init_default(&atlas);
   nk_font_atlas_begin(&atlas);
   font = nk_font_atlas_add_default(&atlas, 13.0f, NULL);
   image = nk_font_atlas_bake(&atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
   device_upload_atlas(&device, image, w, h);
   nk_font_atlas_end(&atlas, nk_handle_id((int)device.font_tex), &device.null);
   nk_init_default(&nk->ctx, &font->handle);

   //nk_init(&nk->ctx, &nk_alloc, &usrfnt);
   nk_common_device_init(&device);

   fill_pathname_join(buf, nk->assets_directory, "folder.png", sizeof(buf));
   nk->icons.folder = nk_common_image_load(buf);
   fill_pathname_join(buf, nk->assets_directory, "speaker.png", sizeof(buf));
   nk->icons.speaker = nk_common_image_load(buf);
   fill_pathname_join(buf, nk->assets_directory, "gamepad.png", sizeof(buf));
   nk->icons.gamepad = nk_common_image_load(buf);
   fill_pathname_join(buf, nk->assets_directory, "monitor.png", sizeof(buf));
   nk->icons.monitor = nk_common_image_load(buf);
   fill_pathname_join(buf, nk->assets_directory, "settings.png", sizeof(buf));
   nk->icons.settings = nk_common_image_load(buf);
   fill_pathname_join(buf, nk->assets_directory, "invader.png", sizeof(buf));
   nk->icons.invader = nk_common_image_load(buf);
   fill_pathname_join(buf, nk->assets_directory, "page_on.png", sizeof(buf));
   nk->icons.page_on = nk_common_image_load(buf);
   fill_pathname_join(buf, nk->assets_directory, "page_off.png", sizeof(buf));
   nk->icons.page_off = nk_common_image_load(buf);

   nk->size_changed = true;
}

static void *nk_menu_init(void **userdata)
{
   settings_t *settings = config_get_ptr();
   nk_menu_handle_t   *nk = NULL;
   menu_handle_t *menu = (menu_handle_t*)
      calloc(1, sizeof(*menu));
   unsigned width, height = 0;

   video_driver_get_size(&width, &height);

   if (!menu)
      goto error;

   if (!menu_display_ctl(MENU_DISPLAY_CTL_INIT_FIRST_DRIVER, NULL))
      goto error;

   nk = (nk_menu_handle_t*)calloc(1, sizeof(nk_menu_handle_t));

   if (!nk)
      goto error;

   *userdata = nk;

   fill_pathname_join(nk->assets_directory, settings->directory.assets,
         "zahnrad", sizeof(nk->assets_directory));
   nk_menu_init_device(nk);

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void nk_menu_free(void *data)
{
   nk_menu_handle_t *nk   = (nk_menu_handle_t*)data;

   if (!nk)
      return;
   free(font);
   nk_free(&nk->ctx);
   nk_buffer_free(&device.cmds);
   nk_common_device_shutdown(&device);

   gfx_coord_array_free(&nk->list_block.carr);
   font_driver_bind_block(NULL, NULL);
}

static void wimp_context_bg_destroy(nk_menu_handle_t *nk)
{
   if (!nk)
      return;

}

static void nk_menu_context_destroy(void *data)
{
   unsigned i;
   nk_menu_handle_t *nk   = (nk_menu_handle_t*)data;

   if (!nk)
      return;

   for (i = 0; i < NK_TEXTURE_LAST; i++)
      video_driver_texture_unload((uintptr_t*)&nk->textures.list[i]);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_MAIN_DEINIT, NULL);

   wimp_context_bg_destroy(nk);
}

static void nk_menu_context_reset(void *data)
{
   char iconpath[PATH_MAX_LENGTH] = {0};
   nk_menu_handle_t *nk              = (nk_menu_handle_t*)data;
   settings_t *settings           = config_get_ptr();
   unsigned width, height = 0;

   video_driver_get_size(&width, &height);

   if (!nk || !settings)
      return;

   fill_pathname_join(iconpath, settings->directory.assets,
         "zahnrad", sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   nk_menu_layout(nk);
   nk_menu_init_device(nk);

   wimp_context_bg_destroy(nk);
   nk_menu_context_reset_textures(nk, iconpath);

   rarch_task_push_image_load(settings->path.menu_wallpaper, "cb_menu_wallpaper",
         menu_display_handle_wallpaper_upload, NULL);
}

static int nk_menu_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   switch (type)
   {
      case 0:
      default:
         break;
   }

   return -1;
}

static bool nk_menu_init_list(void *data)
{
   menu_displaylist_info_t info = {0};
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   strlcpy(info.label,
         menu_hash_to_str(MENU_VALUE_HISTORY_TAB), sizeof(info.label));

   menu_entries_add(menu_stack,
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

static int nk_menu_iterate(void *data, void *userdata, enum menu_action action)
{
   int ret;
   size_t selection;
   menu_entry_t entry;
   nk_menu_handle_t *nk   = (nk_menu_handle_t*)userdata;

   if (!nk)
      return -1;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return 0;

   menu_entry_get(&entry, 0, selection, NULL, false);

   nk->action       = action;

   ret = menu_entry_action(&entry, selection, action);
   if (ret)
      return -1;
   return 0;
}

menu_ctx_driver_t menu_ctx_nuklear = {
   NULL,
   nk_menu_get_message,
   nk_menu_iterate,
   NULL,
   nk_menu_frame,
   nk_menu_init,
   nk_menu_free,
   nk_menu_context_reset,
   nk_menu_context_destroy,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   nk_menu_init_list,
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
   "nuklear",
   nk_menu_environ,
   NULL,
   NULL,
   NULL
};

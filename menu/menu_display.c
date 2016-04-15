/*  RetroArch - A frontend for libretro.
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

#include <time.h>

#include <retro_assert.h>
#include <queues/message_queue.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <string/stdstring.h>

#include "../config.def.h"
#include "../retroarch.h"
#include "../configuration.h"
#include "../runloop.h"
#include "../libretro_version_1.h"
#include "../gfx/video_thread_wrapper.h"
#include "../verbosity.h"

#include "menu_driver.h"
#include "menu_animation.h"
#include "menu_display.h"

#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif

static menu_display_ctx_driver_t *menu_display_ctx_drivers[] = {
#ifdef HAVE_D3D
   &menu_display_ctx_d3d,
#endif
#ifdef HAVE_OPENGL
   &menu_display_ctx_gl,
#endif
#ifdef HAVE_VULKAN
   &menu_display_ctx_vulkan,
#endif
   &menu_display_ctx_null,
   NULL,
};

static const char *menu_video_get_ident(void)
{
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();

   if (settings->video.threaded)
      return rarch_threaded_video_get_ident();
#endif

   return video_driver_get_ident();
}

static bool menu_display_check_compatibility(
      enum menu_display_driver_type type)
{
   const char *video_driver = menu_video_get_ident();

   switch (type)
   {
      case MENU_VIDEO_DRIVER_GENERIC:
         return true;
      case MENU_VIDEO_DRIVER_OPENGL:
         if (string_is_equal(video_driver, "gl"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_VULKAN:
         if (string_is_equal(video_driver, "vulkan"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D:
         if (string_is_equal(video_driver, "d3d"))
            return true;
         break;
   }

   return false;
}

static void menu_display_timedate(void *data)
{
   time_t time_;
   menu_display_ctx_datetime_t *datetime =
      (menu_display_ctx_datetime_t *)data;

   if (!datetime)
      return;

   time(&time_);

   switch (datetime->time_mode)
   {
      case 0: /* Date and time */
         strftime(datetime->s, datetime->len,
               "%Y-%m-%d %H:%M:%S", localtime(&time_));
         break;
      case 1: /* Date */
         strftime(datetime->s, datetime->len,
               "%Y-%m-%d", localtime(&time_));
         break;
      case 2: /* Time */
         strftime(datetime->s, datetime->len,
               "%H:%M:%S", localtime(&time_));
         break;
      case 3: /* Time (hours-minutes) */
         strftime(datetime->s, datetime->len,
               "%H:%M", localtime(&time_));
         break;
      case 4: /* Date and time, without year and seconds */
         strftime(datetime->s, datetime->len,
               "%d/%m %H:%M", localtime(&time_));
         break;
   }
}


bool menu_display_ctl(enum menu_display_ctl_state state, void *data)
{
   unsigned width, height;
   static unsigned menu_display_framebuf_width      = 0;
   static unsigned menu_display_framebuf_height     = 0;
   static size_t menu_display_framebuf_pitch        = 0;
   static int menu_display_font_size                = 0;
   static unsigned menu_display_header_height       = 0;
   static bool menu_display_msg_force               = false;
   static bool menu_display_font_alloc_framebuf     = false;
   static bool menu_display_framebuf_dirty          = false;
   static const uint8_t *menu_display_font_framebuf = NULL;
   static void *menu_display_font_buf               = NULL;
   static msg_queue_t *menu_display_msg_queue       = NULL;
   static menu_display_ctx_driver_t *menu_disp      = NULL;
   settings_t *settings                             = config_get_ptr();

   switch (state)
   {
      case MENU_DISPLAY_CTL_BLEND_BEGIN:
         if (!menu_disp || !menu_disp->blend_begin)
            return false;
         menu_disp->blend_begin();
         break;
      case MENU_DISPLAY_CTL_BLEND_END:
         if (!menu_disp || !menu_disp->blend_end)
            return false;
         menu_disp->blend_end();
         break;
      case MENU_DISPLAY_CTL_FONT_MAIN_DEINIT:
         if (menu_display_font_buf)
            font_driver_free(menu_display_font_buf);
         menu_display_font_buf  = NULL;
         menu_display_font_size = 0;
         break;
      case MENU_DISPLAY_CTL_FONT_MAIN_INIT:
         {
            menu_display_ctx_font_t *font = (menu_display_ctx_font_t*)data;

            menu_display_ctl(MENU_DISPLAY_CTL_FONT_MAIN_DEINIT, NULL);

            if (!font || !menu_disp)
               return false;

            if (!menu_disp->font_init_first)
               return false;

            if (!menu_disp->font_init_first(&menu_display_font_buf,
                     video_driver_get_ptr(false),
                     font->path, font->size))
               return false;

            menu_display_font_size = font->size;
         }
         break;
      case MENU_DISPLAY_CTL_FONT_BIND_BLOCK:
         font_driver_bind_block(menu_display_font_buf, data);
         break;
      case MENU_DISPLAY_CTL_FONT_FLUSH_BLOCK:
         if (!menu_display_font_buf)
            return false;

         font_driver_flush(menu_display_font_buf);
         font_driver_bind_block(menu_display_font_buf, NULL);
         break;
      case MENU_DISPLAY_CTL_FRAMEBUF_DEINIT:
         menu_display_framebuf_width  = 0;
         menu_display_framebuf_height = 0;
         menu_display_framebuf_pitch  = 0;
         break;
      case MENU_DISPLAY_CTL_DEINIT:
         if (menu_display_msg_queue)
            msg_queue_free(menu_display_msg_queue);
         menu_display_msg_queue       = NULL;
         menu_display_msg_force       = false;
         menu_display_header_height   = 0;
         menu_disp                    = NULL;

         menu_animation_ctl(MENU_ANIMATION_CTL_DEINIT, NULL);
         menu_display_ctl(MENU_DISPLAY_CTL_FRAMEBUF_DEINIT, NULL);
         break;
      case MENU_DISPLAY_CTL_INIT:
         retro_assert(menu_display_msg_queue = msg_queue_new(8));
         break;
      case MENU_DISPLAY_CTL_SET_STUB_DRAW_FRAME:
         break;
      case MENU_DISPLAY_CTL_UNSET_STUB_DRAW_FRAME:
         break;
      case MENU_DISPLAY_CTL_FONT_BUF:
         {
            void **ptr = (void**)data;
            if (!ptr)
               return false;
            *ptr = menu_display_font_buf;
         }
         break;
      case MENU_DISPLAY_CTL_SET_FONT_BUF:
         {
            void **ptr = (void**)data;
            if (!ptr)
               return false;
            menu_display_font_buf = *ptr;
         }
         break;
      case MENU_DISPLAY_CTL_FONT_FB:
         {
            uint8_t **ptr = (uint8_t**)data;
            if (!ptr)
               return false;
            *ptr = (uint8_t*)menu_display_font_framebuf;
         }
         break;
      case MENU_DISPLAY_CTL_SET_FONT_FB:
         {
            uint8_t **ptr = (uint8_t**)data;
            if (!ptr)
               return false;
            menu_display_font_framebuf = *ptr;
         }
         break;
      case MENU_DISPLAY_CTL_LIBRETRO_RUNNING:
         if (!settings->menu.pause_libretro)
            if (rarch_ctl(RARCH_CTL_IS_INITED, NULL) 
                  && !rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
               return true;
         return false;
      case MENU_DISPLAY_CTL_LIBRETRO:
         video_driver_set_texture_enable(true, false);

         if (menu_display_ctl(MENU_DISPLAY_CTL_LIBRETRO_RUNNING, NULL))
         {
            bool libretro_input_is_blocked = 
               input_driver_ctl(
                     RARCH_INPUT_CTL_IS_LIBRETRO_INPUT_BLOCKED, NULL);

            if (!libretro_input_is_blocked)
               input_driver_ctl(
                     RARCH_INPUT_CTL_SET_LIBRETRO_INPUT_BLOCKED, NULL);

            core_ctl(CORE_CTL_RETRO_RUN, NULL);

            input_driver_ctl(
                  RARCH_INPUT_CTL_UNSET_LIBRETRO_INPUT_BLOCKED, NULL);
            return true;
         }

         return video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);
      case MENU_DISPLAY_CTL_SET_WIDTH:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            menu_display_framebuf_width = *ptr;
         }
         break;
      case MENU_DISPLAY_CTL_WIDTH:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            *ptr = menu_display_framebuf_width;
         }
         break;
      case MENU_DISPLAY_CTL_HEIGHT:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            *ptr = menu_display_framebuf_height;
         }
         break;
      case MENU_DISPLAY_CTL_HEADER_HEIGHT:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            *ptr = menu_display_header_height;
         }
         break;
      case MENU_DISPLAY_CTL_SET_HEADER_HEIGHT:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            menu_display_header_height = *ptr;
         }
         break;
      case MENU_DISPLAY_CTL_FONT_SIZE:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            *ptr = menu_display_font_size;
         }
         break;
      case MENU_DISPLAY_CTL_SET_FONT_SIZE:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            menu_display_font_size = *ptr;
         }
         break;
      case MENU_DISPLAY_CTL_SET_HEIGHT:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            menu_display_framebuf_height = *ptr;
         }
         break;
      case MENU_DISPLAY_CTL_FB_PITCH:
         {
            size_t *ptr = (size_t*)data;
            if (!ptr)
               return false;
            *ptr = menu_display_framebuf_pitch;
         }
         break;
      case MENU_DISPLAY_CTL_SET_FB_PITCH:
         {
            size_t *ptr = (size_t*)data;
            if (!ptr)
               return false;
            menu_display_framebuf_pitch = *ptr;
         }
         break;
      case MENU_DISPLAY_CTL_MSG_FORCE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            *ptr = menu_display_msg_force;
         }
         break;
      case MENU_DISPLAY_CTL_SET_MSG_FORCE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            menu_display_msg_force = *ptr;
         }
         break;
      case MENU_DISPLAY_CTL_FONT_DATA_INIT:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            *ptr = menu_display_font_alloc_framebuf;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_FONT_DATA_INIT:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            menu_display_font_alloc_framebuf = *ptr;
         }
         break;
      case MENU_DISPLAY_CTL_UPDATE_PENDING:
         {
            if (menu_animation_ctl(MENU_ANIMATION_CTL_IS_ACTIVE, NULL))
               return true;
            if (menu_display_ctl(
                  MENU_DISPLAY_CTL_GET_FRAMEBUFFER_DIRTY_FLAG, NULL))
               return true;
         }
         return false;
      case MENU_DISPLAY_CTL_SET_VIEWPORT:
         video_driver_get_size(&width, &height);
         video_driver_set_viewport(width,
               height, true, false);
         break;
      case MENU_DISPLAY_CTL_UNSET_VIEWPORT:
         video_driver_get_size(&width, &height);
         video_driver_set_viewport(width,
               height, false, true);
         break;
      case MENU_DISPLAY_CTL_GET_FRAMEBUFFER_DIRTY_FLAG:
         return menu_display_framebuf_dirty;
      case MENU_DISPLAY_CTL_SET_FRAMEBUFFER_DIRTY_FLAG:
         menu_display_framebuf_dirty = true;
         break;
      case MENU_DISPLAY_CTL_UNSET_FRAMEBUFFER_DIRTY_FLAG:
         menu_display_framebuf_dirty = false;
         break;
      case MENU_DISPLAY_CTL_GET_DPI:
         {
            gfx_ctx_metrics_t metrics;
            float           *dpi = (float*)data;
            *dpi                 = menu_dpi_override_value;

            if (!settings)
               return true;

            metrics.type  = DISPLAY_METRIC_DPI;
            metrics.value = dpi; 

            if (settings->menu.dpi.override_enable)
               *dpi = settings->menu.dpi.override_value;
            else if (!gfx_ctx_ctl(GFX_CTL_GET_METRICS, &metrics) || !*dpi)
               *dpi = menu_dpi_override_value;
         }
         break;
      case MENU_DISPLAY_CTL_INIT_FIRST_DRIVER:
         {
            unsigned i;

            for (i = 0; menu_display_ctx_drivers[i]; i++)
            {
               if (!menu_display_check_compatibility(
                        menu_display_ctx_drivers[i]->type))
                  continue;

               RARCH_LOG("Found menu display driver: \"%s\".\n",
                     menu_display_ctx_drivers[i]->ident);
               menu_disp = menu_display_ctx_drivers[i];
               return true;
            }
         }
         return false;
      case MENU_DISPLAY_CTL_RESTORE_CLEAR_COLOR:
         if (!menu_disp || !menu_disp->restore_clear_color)
            return false;
         menu_disp->restore_clear_color();
         break;
      case MENU_DISPLAY_CTL_CLEAR_COLOR:
         if (!menu_disp || !menu_disp->clear_color)
            return false;
         menu_disp->clear_color(data);
         break;
      case MENU_DISPLAY_CTL_DRAW:
         if (!menu_disp || !menu_disp->draw)
            return false;
         menu_disp->draw(data);
         break;
      case MENU_DISPLAY_CTL_DRAW_BG:
         {
            struct gfx_coords coords;
            const float *new_vertex       = NULL;
            const float *new_tex_coord    = NULL;
            menu_display_ctx_draw_t *draw = (menu_display_ctx_draw_t*)data;
            if (!menu_disp || !draw)
               return false;

            new_vertex           = draw->vertex;
            new_tex_coord        = draw->tex_coord;

            if (!new_vertex)
               new_vertex        = menu_disp->get_default_vertices();
            if (!new_tex_coord)
               new_tex_coord     = menu_disp->get_default_tex_coords();

            coords.vertices      = draw->vertex_count;
            coords.vertex        = new_vertex;
            coords.tex_coord     = new_tex_coord;
            coords.lut_tex_coord = new_tex_coord;
            coords.color         = (const float*)draw->color;

            draw->x              = 0;
            draw->y              = 0;
            draw->coords         = &coords;

            if (!draw->texture)
               draw->texture     = menu_display_white_texture;

            draw->matrix_data = (math_matrix_4x4*)menu_disp->get_default_mvp();

            menu_disp->draw(draw);
         }
         break;
      case MENU_DISPLAY_CTL_ROTATE_Z:
         {
            math_matrix_4x4 matrix_rotated, matrix_scaled;
            math_matrix_4x4 *b                   = NULL;
            menu_display_ctx_rotate_draw_t *draw = 
               (menu_display_ctx_rotate_draw_t*)data;

            if (!draw || !menu_disp || !menu_disp->get_default_mvp)
               return false;

            b = (math_matrix_4x4*)menu_disp->get_default_mvp();

            matrix_4x4_rotate_z(&matrix_rotated, draw->rotation);
            matrix_4x4_multiply(draw->matrix, &matrix_rotated, b);

            if (!draw->scale_enable)
               return false;

            matrix_4x4_scale(&matrix_scaled,
                  draw->scale_x, draw->scale_y, draw->scale_z);
            matrix_4x4_multiply(draw->matrix, &matrix_scaled, draw->matrix);
         }
         break;
      case MENU_DISPLAY_CTL_TEX_COORDS_GET:
         {
            menu_display_ctx_coord_draw_t *draw = 
               (menu_display_ctx_coord_draw_t*)data;
            if (!draw)
               return false;

            if (!menu_disp || !menu_disp->get_default_tex_coords)
               return false;

            draw->ptr = menu_disp->get_default_tex_coords();
         }
         break;
      case MENU_DISPLAY_CTL_TIMEDATE:
         menu_display_timedate(data);
         break;
      case MENU_DISPLAY_CTL_NONE:
      default:
         break;
   }

   return true;
}

void menu_display_handle_wallpaper_upload(void *task_data,
      void *user_data, const char *err)
{
   menu_ctx_load_image_t load_image_info;
   struct texture_image *img = (struct texture_image*)task_data;

   load_image_info.data = img;
   load_image_info.type = MENU_IMAGE_WALLPAPER;

   menu_driver_ctl(RARCH_MENU_CTL_LOAD_IMAGE, &load_image_info);
   video_texture_image_free(img);
   free(img);
}

void menu_display_allocate_white_texture()
{
   struct texture_image ti;
   static const uint8_t white_data[] = { 0xff, 0xff, 0xff, 0xff };

   ti.width  = 1;
   ti.height = 1;
   ti.pixels = (uint32_t*)&white_data;

   video_driver_texture_load(&ti,
         TEXTURE_FILTER_NEAREST, &menu_display_white_texture);
}

/*  RetroArch - A frontend for libretro.
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

#include <time.h>

#include <queues/message_queue.h>
#include <retro_miscellaneous.h>
#include <gfx/math/matrix_4x4.h>
#include <formats/image.h>

#include "../config.def.h"
#include "../gfx/video_thread_wrapper.h"

#include "menu.h"
#include "menu_animation.h"
#include "menu_display.h"

#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif

typedef struct menu_framebuf
{
   uint16_t *data;
   unsigned width;
   unsigned height;
   size_t pitch;
   bool dirty;
} menu_framebuf_t;

typedef struct menu_display
{
   bool msg_force;

   struct
   {
      void *buf;
      int size;

      const uint8_t *framebuf;
      bool alloc_framebuf;
   } font;

   unsigned header_height;

   msg_queue_t *msg_queue;
   menu_display_ctx_driver_t *display_ctx;
} menu_display_t;


static menu_display_ctx_driver_t *menu_display_ctx_drivers[] = {
#ifdef HAVE_DIRECT3D
   &menu_display_ctx_d3d,
#endif
#ifdef HAVE_OPENGL
   &menu_display_ctx_gl,
#endif
   &menu_display_ctx_null,
   NULL,
};

static menu_display_t  menu_display_state;

static menu_framebuf_t frame_buf_state;

static menu_display_t *menu_display_get_ptr(void)
{
   return &menu_display_state;
}

static menu_display_ctx_driver_t *menu_display_context_get_ptr(void)
{
   menu_display_t *disp = menu_display_get_ptr();
   if (!disp)
      return NULL;
   return disp->display_ctx;
}

static menu_framebuf_t *menu_display_fb_get_ptr(void)
{
   return &frame_buf_state;
}

static void menu_display_fb_free(menu_framebuf_t *frame_buf)
{
   if (!frame_buf)
      return;

   if (frame_buf->data)
      free(frame_buf->data);
   frame_buf->data = NULL;
}

void menu_display_free(void)
{
   menu_display_t *disp = menu_display_get_ptr();
   if (!disp)
      return;

   if (disp->msg_queue)
      msg_queue_free(disp->msg_queue);
   disp->msg_queue = NULL;

   menu_animation_free();

   menu_display_fb_free(&frame_buf_state);
   memset(&frame_buf_state,    0, sizeof(menu_framebuf_t));
   memset(&menu_display_state, 0, sizeof(menu_display_t));
}

bool menu_display_init(void)
{
   menu_display_t *disp = menu_display_get_ptr();
   if (!disp)
      return false;

   retro_assert(disp->msg_queue = msg_queue_new(8));

   return true;
}

bool menu_display_font_init_first(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size)
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->font_init_first)
      return false;

   return menu_disp->font_init_first(font_driver, font_handle, video_data,
         font_path, font_size);
}

bool menu_display_font_bind_block(void *data, const void *font_data, void *userdata)
{
   const struct font_renderer *font_driver = 
      (const struct font_renderer*)font_data;
   menu_display_t *disp = menu_display_get_ptr();
   if (!disp || !font_driver || !font_driver->bind_block)
      return false;

   font_driver->bind_block(disp->font.buf, userdata);

   return true;
}

bool menu_display_font_flush_block(void *data, const void *font_data)
{
   const struct font_renderer *font_driver = 
      (const struct font_renderer*)font_data;
   menu_handle_t *menu = (menu_handle_t*)data;
   menu_display_t *disp = menu_display_get_ptr();
   if (!font_driver || !font_driver->flush || !disp || !disp->font.buf)
      return false;

   font_driver->flush(disp->font.buf);

   return menu_display_font_bind_block(menu,
         font_driver, NULL);
}

void menu_display_free_main_font(void)
{
   driver_t     *driver = driver_get_ptr();
   menu_display_t *disp = menu_display_get_ptr();
    
   if (disp && disp->font.buf)
   {
      font_driver_free(disp->font.buf);
      disp->font.buf = NULL;
   }
}

static const char *menu_video_get_ident(void)
{
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();

   if (settings->video.threaded)
      return rarch_threaded_video_get_ident();
#endif

   return video_driver_get_ident();
}

static bool menu_display_check_compatibility(enum menu_display_driver_type type)
{
   const char *video_driver = menu_video_get_ident();

   switch (type)
   {
      case MENU_VIDEO_DRIVER_GENERIC:
         return true;
      case MENU_VIDEO_DRIVER_OPENGL:
         if (!strcmp(video_driver, "gl"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D:
         if (!strcmp(video_driver, "d3d"))
            return true;
         break;
   }

   return false;
}

bool menu_display_driver_init_first(void)
{
   unsigned i;
   menu_display_t *disp = menu_display_get_ptr();

   for (i = 0; menu_display_ctx_drivers[i]; i++)
   {
      if (!menu_display_check_compatibility(menu_display_ctx_drivers[i]->type))
         continue;

      RARCH_LOG("Found menu display driver: \"%s\".\n",
            menu_display_ctx_drivers[i]->ident);
      disp->display_ctx = menu_display_ctx_drivers[i];
      return true;
   }

   return false;
}

bool menu_display_init_main_font(void *data,
      const char *font_path, float font_size)
{
   bool      ret;
   driver_t    *driver  = driver_get_ptr();
   void        *video   = video_driver_get_ptr(false);
   menu_display_t *disp = menu_display_get_ptr();

   if (!disp)
      return false;

   if (disp->font.buf)
      menu_display_free_main_font();

   ret = menu_display_font_init_first(
         (const void**)&driver->font_osd_driver,
         &disp->font.buf, video,
         font_path, font_size);

   if (ret)
      disp->font.size = font_size;
   else
      disp->font.buf = NULL;

   return ret;
}

bool menu_display_ctl(enum menu_display_ctl_state state, void *data)
{
   unsigned width, height;
   menu_framebuf_t *frame_buf = menu_display_fb_get_ptr();
   menu_display_t  *disp      = menu_display_get_ptr();
   settings_t *settings       = config_get_ptr();

   switch (state)
   {
      case MENU_DISPLAY_CTL_FONT_BUF:
         {
            void **ptr = (void**)data;
            if (!ptr)
               return false;
            *ptr = disp->font.buf;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_FONT_BUF:
         {
            void **ptr = (void**)data;
            if (!ptr)
               return false;
            disp->font.buf = *ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_FONT_FB:
         {
            uint8_t **ptr = (uint8_t**)data;
            if (!ptr)
               return false;
            *ptr = (uint8_t*)disp->font.framebuf;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_FONT_FB:
         {
            uint8_t **ptr = (uint8_t**)data;
            if (!ptr)
               return false;
            disp->font.framebuf = *ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_LIBRETRO_RUNNING:
         {
            global_t *global     = global_get_ptr();
            if (!settings->menu.pause_libretro)
               if (global->inited.main && (global->inited.core.type != CORE_TYPE_DUMMY))
                  return true;
         }
         break;
      case MENU_DISPLAY_CTL_LIBRETRO:
         video_driver_set_texture_enable(true, false);

         if (menu_display_ctl(MENU_DISPLAY_CTL_LIBRETRO_RUNNING, NULL))
         {
            bool libretro_input_is_blocked = input_driver_ctl(RARCH_INPUT_CTL_IS_LIBRETRO_INPUT_BLOCKED, NULL);

            if (!libretro_input_is_blocked)
               input_driver_ctl(RARCH_INPUT_CTL_SET_LIBRETRO_INPUT_BLOCKED, NULL);

            core.retro_run();

            input_driver_ctl(RARCH_INPUT_CTL_UNSET_LIBRETRO_INPUT_BLOCKED, NULL);
            return true;
         }

         return video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);
      case MENU_DISPLAY_CTL_SET_WIDTH:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            frame_buf->width = *ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_WIDTH:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            *ptr = frame_buf->width;
         }
         return true;
      case MENU_DISPLAY_CTL_HEIGHT:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            *ptr = frame_buf->height;
         }
         return true;
      case MENU_DISPLAY_CTL_HEADER_HEIGHT:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            *ptr = disp->header_height;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_HEADER_HEIGHT:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            disp->header_height = *ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_FONT_SIZE:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            *ptr = disp->font.size;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_FONT_SIZE:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            disp->font.size = *ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_HEIGHT:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            frame_buf->height = *ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_FB_DATA:
         {
            uint16_t **ptr = (uint16_t**)data;
            if (!ptr)
               return false;
            *ptr = frame_buf->data;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_FB_DATA:
         {
            uint16_t *ptr = (uint16_t*)data;
            if (!ptr)
               return false;
            frame_buf->data = ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_FB_PITCH:
         {
            size_t *ptr = (size_t*)data;
            if (!ptr)
               return false;
            *ptr = frame_buf->pitch;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_FB_PITCH:
         {
            size_t *ptr = (size_t*)data;
            if (!ptr)
               return false;
            frame_buf->pitch = *ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_MSG_FORCE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            *ptr = disp->msg_force;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_MSG_FORCE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            disp->msg_force = *ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_FONT_DATA_INIT:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            *ptr = disp->font.alloc_framebuf;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_FONT_DATA_INIT:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            disp->font.alloc_framebuf = *ptr;
         }
         return true;
      case MENU_DISPLAY_CTL_UPDATE_PENDING:
         {
            bool ptr;
            menu_display_ctl(MENU_DISPLAY_CTL_GET_FRAMEBUFFER_DIRTY_FLAG, &ptr);
            if (menu_animation_ctl(MENU_ANIMATION_CTL_IS_ACTIVE, NULL) || ptr)
               return true;
         }
         return false;
      case MENU_DISPLAY_CTL_SET_VIEWPORT:
         video_driver_get_size(&width, &height);
         video_driver_set_viewport(width,
               height, true, false);
         return true;
      case MENU_DISPLAY_CTL_UNSET_VIEWPORT:
         video_driver_get_size(&width, &height);
         video_driver_set_viewport(width,
               height, false, true);
         return true;
      case MENU_DISPLAY_CTL_GET_FRAMEBUFFER_DIRTY_FLAG:
         {
            bool *ptr = (bool*)data;
            if (!ptr || !frame_buf)
               return false;
            *ptr = frame_buf->dirty;
         }
         return true;
      case MENU_DISPLAY_CTL_SET_FRAMEBUFFER_DIRTY_FLAG:
         if (frame_buf && frame_buf->data)
            frame_buf->dirty = true;
         return true;
      case MENU_DISPLAY_CTL_UNSET_FRAMEBUFFER_DIRTY_FLAG:
         if (frame_buf && frame_buf->data)
            frame_buf->dirty = false;
         return true;
      case MENU_DISPLAY_CTL_GET_DPI:
         {
            float           *dpi = (float*)data;
            *dpi                 = menu_dpi_override_value;

            if (!settings)
               return true;

            if (settings->menu.dpi.override_enable)
               *dpi = settings->menu.dpi.override_value;
            else if (!gfx_ctx_get_metrics(DISPLAY_METRIC_DPI, dpi))
               *dpi = menu_dpi_override_value;
         }
         return true;
   }

   return false;
}

void menu_display_timedate(char *s, size_t len, unsigned time_mode)
{
   time_t time_;
   time(&time_);

   switch (time_mode)
   {
      case 0: /* Date and time */
         strftime(s, len, "%Y-%m-%d %H:%M:%S", localtime(&time_));
         break;
      case 1: /* Date */
         strftime(s, len, "%Y-%m-%d", localtime(&time_));
         break;
      case 2: /* Time */
         strftime(s, len, "%H:%M:%S", localtime(&time_));
         break;
      case 3: /* Time (hours-minutes) */
         strftime(s, len, "%H:%M", localtime(&time_));
         break;
      case 4: /* Date and time, without year and seconds */
         strftime(s, len, "%d/%m %H:%M", localtime(&time_));
         break;
   }
}

void menu_display_msg_queue_push(const char *msg, unsigned prio, unsigned duration,
      bool flush)
{
   rarch_main_msg_queue_push(msg, prio, duration, flush);
}


void menu_display_blend_begin(void)
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->blend_begin)
      return;

   menu_disp->blend_begin();
}

void menu_display_blend_end(void)
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->blend_end)
      return;

   menu_disp->blend_end();
}

void menu_display_matrix_4x4_rotate_z(void *data, float rotation,
      float scale_x, float scale_y, float scale_z, bool scale_enable)
{
   math_matrix_4x4 *matrix, *b;
   math_matrix_4x4 matrix_rotated;
   math_matrix_4x4 matrix_scaled;
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->get_default_mvp)
      return;

   matrix = (math_matrix_4x4*)data;
   b      = (math_matrix_4x4*)menu_disp->get_default_mvp();
   if (!matrix)
      return;

   matrix_4x4_rotate_z(&matrix_rotated, rotation);
   matrix_4x4_multiply(matrix, &matrix_rotated, b);

   if (!scale_enable)
      return;

   matrix_4x4_scale(&matrix_scaled, scale_x, scale_y, scale_z);
   matrix_4x4_multiply(matrix, &matrix_scaled, matrix);
}

unsigned menu_display_texture_load(void *data,
      enum texture_filter_type  filter_type)
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->texture_load)
      return 0;

   return menu_disp->texture_load(data, filter_type);
}

void menu_display_texture_unload(uintptr_t *id)
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->texture_unload)
      return;

   menu_disp->texture_unload(id);
}

void menu_display_draw(float x, float y,
      unsigned width, unsigned height,
      struct gfx_coords *coords,
      void *matrix_data, 
      uintptr_t texture,
      enum menu_display_prim_type prim_type
      )
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->draw)
      return;

   menu_disp->draw(x, y, width, height, coords, matrix_data, texture, prim_type);
}

void menu_display_draw_bg(
      unsigned width, unsigned height,
      uintptr_t texture,
      float handle_alpha,
      bool force_transparency,
      float *color,
      float *color2,
      const float *vertex,
      const float *tex_coord,
      size_t vertex_count,
      enum menu_display_prim_type prim_type
      )
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->draw_bg)
      return;

   menu_disp->draw_bg(width, height, texture, handle_alpha, force_transparency, color,
         color2, vertex, tex_coord, vertex_count, prim_type);
}

void menu_display_restore_clear_color(void)
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->restore_clear_color)
      return;

   menu_disp->restore_clear_color();
}

void menu_display_clear_color(float r, float g, float b, float a)
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->clear_color)
      return;

   menu_disp->clear_color(r, g, b, a);
}

const float *menu_display_get_tex_coords(void)
{
   menu_display_ctx_driver_t *menu_disp = menu_display_context_get_ptr();
   if (!menu_disp || !menu_disp->get_tex_coords)
      return NULL;

   return menu_disp->get_tex_coords();
}

void menu_display_handle_wallpaper_upload(void *task_data, void *user_data, const char *err)
{
   struct texture_image *img = (struct texture_image*)task_data;
   menu_driver_load_image(img, MENU_IMAGE_WALLPAPER);
   texture_image_free(img);
   free(img);
}

void menu_display_handle_boxart_upload(void *task_data, void *user_data, const char *err)
{
   struct texture_image *img = (struct texture_image*)task_data;
   menu_driver_load_image(img, MENU_IMAGE_BOXART);
   texture_image_free(img);
   free(img);
}

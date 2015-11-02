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

#include "../config.def.h"
#include "../gfx/font_renderer_driver.h"
#include "../gfx/video_context_driver.h"
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
} menu_display_t;

static menu_display_t  menu_display_state;

static menu_framebuf_t frame_buf_state;

static menu_display_t *menu_display_get_ptr(void)
{
   return &menu_display_state;
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
   settings_t *settings = config_get_ptr();
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();

   if (settings->video.threaded && !hw_render->context_type)
   {
      thread_packet_t pkt;
      driver_t *driver    = driver_get_ptr();
      thread_video_t *thr = (thread_video_t*)driver->video_data;

      if (!thr)
         return false;

      pkt.type                       = CMD_FONT_INIT;
      pkt.data.font_init.method      = font_init_first;
      pkt.data.font_init.font_driver = (const void**)font_driver;
      pkt.data.font_init.font_handle = font_handle;
      pkt.data.font_init.video_data  = video_data;
      pkt.data.font_init.font_path   = font_path;
      pkt.data.font_init.font_size   = font_size;
      pkt.data.font_init.api         = FONT_DRIVER_RENDER_OPENGL_API;

      thr->send_and_wait(thr, &pkt);

      return pkt.data.font_init.return_value;
   }

   return font_init_first(font_driver, font_handle, video_data,
         font_path, font_size, FONT_DRIVER_RENDER_OPENGL_API);
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
   if (!font_driver || !font_driver->flush)
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
      driver->font_osd_driver->free(disp->font.buf);
      disp->font.buf = NULL;
   }
}

bool menu_display_init_main_font(void *data,
      const char *font_path, float font_size)
{
   bool      ret;
   driver_t    *driver  = driver_get_ptr();
   void        *video   = video_driver_get_ptr(NULL);
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
            driver_t      *driver     = driver_get_ptr();
            bool block_libretro_input = driver->block_libretro_input;

            driver->block_libretro_input = true;
            core.retro_run();
            driver->block_libretro_input = block_libretro_input;
            return true;
         }

         video_driver_cached_frame();
         return true;
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
   }
}

void menu_display_msg_queue_push(const char *msg, unsigned prio, unsigned duration,
      bool flush)
{
   rarch_main_msg_queue_push(msg, prio, duration, flush);
}

#ifdef HAVE_OPENGL
static GLenum menu_display_prim_to_gl_enum(enum menu_display_prim_type prim_type)
{
   switch (prim_type)
   {
      case MENU_DISPLAY_PRIM_TRIANGLESTRIP:
         return GL_TRIANGLE_STRIP;
      case MENU_DISPLAY_PRIM_TRIANGLES:
         return GL_TRIANGLES;
      case MENU_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   return 0;
}

void menu_display_matrix_4x4_rotate_z(void *data, float rotation)
{
   math_matrix_4x4 matrix_rotated;
   math_matrix_4x4 *b = NULL;
   math_matrix_4x4 *matrix = (math_matrix_4x4*)data;
#ifdef HAVE_OPENGL
   gl_t           *gl = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl)
      return;

   b = (math_matrix_4x4*)&gl->mvp_no_rot;
#endif
   if (!matrix)
      return;

   matrix_4x4_rotate_z(&matrix_rotated, rotation);
   matrix_4x4_multiply(matrix, &matrix_rotated, b);
}

void menu_display_blend_begin(void)
{
   gl_t         *gl = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl)
      return;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
}

void menu_display_blend_end(void)
{
   glDisable(GL_BLEND);
}

void menu_display_draw_frame(
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      struct gfx_coords *coords,
      math_matrix_4x4 *mat, 
      GLuint texture,
      enum menu_display_prim_type prim_type
      )
{
   driver_t *driver = driver_get_ptr();
   gl_t         *gl = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl)
      return;

   /* TODO - edge case */
   if (height <= 0)
      height = 1;

   if (!mat)
      mat = &gl->mvp_no_rot;

   glViewport(x, y, width, height);
   glBindTexture(GL_TEXTURE_2D, texture);

   gl->shader->set_coords(coords);
   gl->shader->set_mvp(driver->video_data, mat);

   glDrawArrays(menu_display_prim_to_gl_enum(prim_type), 0, coords->vertices);
}

void menu_display_frame_background(
      menu_handle_t *menu,
      settings_t *settings,
      unsigned width,
      unsigned height,
      GLuint texture,
      float handle_alpha,
      bool force_transparency,
      GRfloat *coord_color,
      GRfloat *coord_color2,
      const GRfloat *vertex,
      const GRfloat *tex_coord,
      size_t vertex_count,
      enum menu_display_prim_type prim_type)
{
   struct gfx_coords coords;
   global_t *global = global_get_ptr();
   gl_t         *gl = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl)
      return;

   coords.vertices      = vertex_count;
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;
   coords.color         = (const float*)coord_color;

   menu_display_blend_begin();

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   if ((settings->menu.pause_libretro
      || !global->inited.main || (global->inited.core.type == CORE_TYPE_DUMMY))
      && !force_transparency
      && texture)
      coords.color = (const float*)coord_color2;

   menu_display_draw_frame(0, 0, width, height,
         &coords, &gl->mvp_no_rot,
         texture, prim_type);

   menu_display_blend_end();

   gl->coords.color = gl->white_color_ptr;
}

void menu_display_restore_clear_color(void *data)
{
   (void)data;

   glClearColor(0.0f, 0.0f, 0.0f, 0.00f);
}

void menu_display_clear_color(void *data, float r, float g, float b, float a)
{
   glClearColor(r, g, b, a);
   glClear(GL_COLOR_BUFFER_BIT);
}
#endif

const char *menu_video_get_ident(void)
{
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();

   if (settings->video.threaded)
      return rarch_threaded_video_get_ident();
#endif

   return video_driver_get_ident();
}

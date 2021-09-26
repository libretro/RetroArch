/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../retroarch.h"
#include "../font_driver.h"
#include "../common/gl2_common.h"

#include "../gfx_display.h"

#if defined(__arm__) || defined(__aarch64__)
static int scx0, scx1, scy0, scy1;

/* This array contains problematic GPU drivers
 * that have problems when we draw outside the
 * bounds of the framebuffer */
static const struct
{
   const char *str;
   int len;
} scissor_device_strings[] = {
   { "ARM Mali-4xx", 10 },
   { 0, 0 }
};

static void scissor_set_rectangle(
      int x0, int x1, int y0, int y1, int sc)
{
   const int dx = sc ? 10 : 2;
   const int dy = dx;
   scx0         = x0 + dx;
   scx1         = x1 - dx;
   scy0         = y0 + dy;
   scy1         = y1 - dy;
}

static bool scissor_is_outside_rectangle(
      int x0, int x1, int y0, int y1)
{
   if (x1 < scx0)
      return true;
   if (scx1 < x0)
      return true;
   if (y1 < scy0)
      return true;
   if (scy1 < y0)
      return true;
   return false;
}

#define MALI_BUG
#endif

static const GLfloat gl2_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat gl2_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float *gfx_display_gl2_get_default_vertices(void)
{
   return &gl2_vertexes[0];
}

static const float *gfx_display_gl2_get_default_tex_coords(void)
{
   return &gl2_tex_coords[0];
}

static void *gfx_display_gl2_get_default_mvp(void *data)
{
   gl2_t *gl = (gl2_t*)data;

   if (!gl)
      return NULL;

   return &gl->mvp_no_rot;
}

static GLenum gfx_display_prim_to_gl_enum(
      enum gfx_display_prim_type type)
{
   switch (type)
   {
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return GL_TRIANGLE_STRIP;
      case GFX_DISPLAY_PRIM_TRIANGLES:
         return GL_TRIANGLES;
      case GFX_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   return 0;
}

static void gfx_display_gl2_blend_begin(void *data)
{
   gl2_t             *gl          = (gl2_t*)data;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   gl->shader->use(gl, gl->shader_data, VIDEO_SHADER_STOCK_BLEND,
         true);
}

static void gfx_display_gl2_blend_end(void *data)
{
   glDisable(GL_BLEND);
}

#ifdef MALI_BUG
static bool 
gfx_display_gl2_discard_draw_rectangle(gfx_display_ctx_draw_t *draw,
      unsigned width, unsigned height)
{
   static bool mali_4xx_detected     = false;
   static bool scissor_inited        = false;
   static unsigned last_video_width  = 0;
   static unsigned last_video_height = 0;

   if (!scissor_inited)
   {
      unsigned i;
      const char *gpu_device_string = NULL;
      scissor_inited                = true;

      scissor_set_rectangle(0,
            width - 1,
            0,
            height - 1,
            0);

      /* TODO/FIXME - This might be thread unsafe in the long run -
       * preferably call this once outside of the menu display driver
       * and then just pass this string as a parameter */
      gpu_device_string = video_driver_get_gpu_device_string();

      if (gpu_device_string)
      {
         for (i = 0; scissor_device_strings[i].len; ++i)
         {
            if (strncmp(gpu_device_string,
                     scissor_device_strings[i].str,
                     scissor_device_strings[i].len) == 0)
            {
               mali_4xx_detected = true;
               break;
            }
         }
      }

      last_video_width  = width;
      last_video_height = height;
   }

   /* Early out, to minimise performance impact on
    * non-mali_4xx devices */
   if (!mali_4xx_detected)
      return false;

   /* Have to update scissor_set_rectangle() if the
    * video dimensions change */
   if ((width  != last_video_width) ||
       (height != last_video_height))
   {
      scissor_set_rectangle(0,
            width - 1,
            0,
            height - 1,
            0);

      last_video_width  = width;
      last_video_height = height;
   }

   /* Discards not only out-of-bounds scissoring,
    * but also out-of-view draws.
    *
    * This is intentional.
    */
   return scissor_is_outside_rectangle(
         draw->x, draw->x + draw->width - 1,
         draw->y, draw->y + draw->height - 1);
}
#endif

static void gfx_display_gl2_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   gl2_t             *gl  = (gl2_t*)data;

   if (!gl || !draw)
      return;

#ifdef MALI_BUG
   if (gfx_display_gl2_discard_draw_rectangle(draw, video_width,
            video_height))
   {
      /*RARCH_WARN("[Menu]: discarded draw rect: %.4i %.4i %.4i %.4i\n",
        (int)draw->x, (int)draw->y, (int)draw->width, (int)draw->height);*/
      return;
   }
#endif

   if (!draw->coords->vertex)
      draw->coords->vertex        = &gl2_vertexes[0];
   if (!draw->coords->tex_coord)
      draw->coords->tex_coord     = &gl2_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = &gl2_tex_coords[0];

   glViewport(draw->x, draw->y, draw->width, draw->height);
   glBindTexture(GL_TEXTURE_2D, (GLuint)draw->texture);

   gl->shader->set_coords(gl->shader_data, draw->coords);
   gl->shader->set_mvp(gl->shader_data,
         draw->matrix_data ? (math_matrix_4x4*)draw->matrix_data
      : (math_matrix_4x4*)&gl->mvp_no_rot);


   glDrawArrays(gfx_display_prim_to_gl_enum(
            draw->prim_type), 0, draw->coords->vertices);

   gl->coords.color     = gl->white_color_ptr;
}

static void gfx_display_gl2_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
#ifdef HAVE_SHADERPIPELINE
   struct uniform_info uniform_param;
   gl2_t             *gl            = (gl2_t*)data;
   static float t                   = 0;
   video_coord_array_t *ca          = &p_disp->dispca;

   draw->x                          = 0;
   draw->y                          = 0;
   draw->coords                     = (struct video_coords*)(&ca->coords);
   draw->matrix_data                = NULL;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         glBlendFunc(GL_ONE, GL_ONE);
         break;
      default:
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         break;
   }

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         gl->shader->use(gl, gl->shader_data, draw->pipeline_id,
               true);

         t += 0.01;

         uniform_param.type              = UNIFORM_1F;
         uniform_param.enabled           = true;
         uniform_param.location          = 0;
         uniform_param.count             = 0;

         uniform_param.lookup.type       = SHADER_PROGRAM_VERTEX;
         uniform_param.lookup.ident      = "time";
         uniform_param.lookup.idx        = draw->pipeline_id;
         uniform_param.lookup.add_prefix = true;
         uniform_param.lookup.enable     = true;

         uniform_param.result.f.v0       = t;

         gl->shader->set_uniform_parameter(gl->shader_data,
               &uniform_param, NULL);
         break;
   }

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
#ifndef HAVE_PSGL
         uniform_param.type              = UNIFORM_2F;
         uniform_param.lookup.ident      = "OutputSize";
         uniform_param.result.f.v0       = draw->width;
         uniform_param.result.f.v1       = draw->height;

         gl->shader->set_uniform_parameter(gl->shader_data,
               &uniform_param, NULL);
#endif
         break;
   }
#endif
}

static bool gfx_display_gl2_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float menu_font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   if (!(*handle = font_driver_init_first(video_data,
         font_path, menu_font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_OPENGL_API)))
		 return false;
   return true;
}

static void gfx_display_gl2_scissor_begin(
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   glScissor(x, video_height - y - height, width, height);
   glEnable(GL_SCISSOR_TEST);
#ifdef MALI_BUG
   /* TODO/FIXME: If video width/height changes between
    * a call of gfx_display_gl2_scissor_begin() and the
    * next call of gfx_display_gl2_draw() (or if
    * gfx_display_gl2_scissor_begin() is called before the
    * first call of gfx_display_gl2_draw()), the scissor
    * rectangle set here will be overwritten by the initialisation
    * procedure inside gfx_display_gl2_discard_draw_rectangle(),
    * causing the next frame to render glitched content */
   scissor_set_rectangle(x, x + width - 1, y, y + height - 1, 1);
#endif
}

static void gfx_display_gl2_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   glScissor(0, 0, video_width, video_height);
   glDisable(GL_SCISSOR_TEST);
#ifdef MALI_BUG
   scissor_set_rectangle(0, video_width - 1, 0, video_height - 1, 0);
#endif
}

gfx_display_ctx_driver_t gfx_display_ctx_gl = {
   gfx_display_gl2_draw,
   gfx_display_gl2_draw_pipeline,
   gfx_display_gl2_blend_begin,
   gfx_display_gl2_blend_end,
   gfx_display_gl2_get_default_mvp,
   gfx_display_gl2_get_default_vertices,
   gfx_display_gl2_get_default_tex_coords,
   gfx_display_gl2_font_init_first,
   GFX_VIDEO_DRIVER_OPENGL,
   "gl",
   false,
   gfx_display_gl2_scissor_begin,
   gfx_display_gl2_scissor_end
};

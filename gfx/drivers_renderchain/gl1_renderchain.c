/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef _MSC_VER
#pragma comment(lib, "opengl32")
#endif

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <retro_common_api.h>
#include <libretro.h>

#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <string/stdstring.h>

#include <gfx/gl_capabilities.h>
#include <gfx/video_frame.h>

#include "../video_driver.h"
#include "../video_shader_parse.h"
#include "../common/gl_common.h"

#include "../../driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct gl1_renderchain
{
   void *empty;
} gl1_renderchain_t;

GLenum min_filter_to_mag(GLenum type);

void gl1_renderchain_free(void *data, void *chain_data)
{
   (void)chain_data;
   (void)data;
}

static void gl1_renderchain_bind_prev_texture(
      void *data,
      void *chain_data,
      const struct video_tex_info *tex_info)
{
   gl_t *gl = (gl_t*)data;

   memmove(gl->prev_info + 1, gl->prev_info,
         sizeof(*tex_info) * (gl->textures - 1));
   memcpy(&gl->prev_info[0], tex_info,
         sizeof(*tex_info));
}

static void gl1_renderchain_viewport_info(
      void *data, void *chain_data,
      struct video_viewport *vp)
{
   unsigned width, height;
   unsigned top_y, top_dist;
   gl_t *gl         = (gl_t*)data;

   video_driver_get_size(&width, &height);

   *vp             = gl->vp;
   vp->full_width  = width;
   vp->full_height = height;

   /* Adjust as GL viewport is bottom-up. */
   top_y           = vp->y + vp->height;
   top_dist        = height - top_y;
   vp->y           = top_dist;
}

static bool gl1_renderchain_read_viewport(
      void *data, void *chain_data,
      uint8_t *buffer, bool is_idle)
{
   unsigned                     num_pixels = 0;
   gl_t                                *gl = (gl_t*)data;

   if (!gl)
      return false;

   num_pixels = gl->vp.width * gl->vp.height;

   /* Use slow synchronous readbacks. Use this with plain screenshots
      as we don't really care about performance in this case. */

   /* GL1 only guarantees GL_RGBA/GL_UNSIGNED_BYTE
    * readbacks so do just that.
    * GL1 also doesn't support reading back data
    * from front buffer, so render a cached frame
    * and have gl_frame() do the readback while it's
    * in the back buffer.
    */
   gl->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));

   if (!gl->readback_buffer_screenshot)
      return false;

   if (!is_idle)
      video_driver_cached_frame();

   video_frame_convert_rgba_to_bgr(
         (const void*)gl->readback_buffer_screenshot,
         buffer,
         num_pixels);

   free(gl->readback_buffer_screenshot);
   gl->readback_buffer_screenshot = NULL;

   return true;
}

void gl1_renderchain_free_internal(void *data, void *chain_data)
{
   gl1_renderchain_t *cg_data = (gl1_renderchain_t*)chain_data;

   if (!cg_data)
      return;

   free(cg_data);
}

static void *gl1_renderchain_new(void)
{
   gl1_renderchain_t *renderchain = (gl1_renderchain_t*)
      calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   return renderchain;
}

static void gl1_renderchain_ff_vertex(const void *data)
{
   const struct video_coords *coords = (const struct video_coords*)data;
   /* Fall back to fixed function-style if needed and possible. */
   glClientActiveTexture(GL_TEXTURE1);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->lut_tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glClientActiveTexture(GL_TEXTURE0);
   glVertexPointer(2, GL_FLOAT, 0, coords->vertex);
   glEnableClientState(GL_VERTEX_ARRAY);
   glColorPointer(4, GL_FLOAT, 0, coords->color);
   glEnableClientState(GL_COLOR_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void gl1_renderchain_ff_matrix(const void *data)
{
   math_matrix_4x4 ident;
   const math_matrix_4x4 *mat = (const math_matrix_4x4*)data;

   /* Fall back to fixed function-style if needed and possible. */
   glMatrixMode(GL_PROJECTION);
   glLoadMatrixf(mat->data);
   glMatrixMode(GL_MODELVIEW);
   matrix_4x4_identity(ident);
   glLoadMatrixf(ident.data);
}

static void gl1_renderchain_disable_client_arrays(void *data,
      void *chain_data)
{
   if (gl_query_core_context_in_use())
      return;

   glClientActiveTexture(GL_TEXTURE1);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glClientActiveTexture(GL_TEXTURE0);
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void gl1_renderchain_restore_default_state(void *data,
      void *chain_data)
{
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;
   glEnable(GL_TEXTURE_2D);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DITHER);
}

static void gl1_renderchain_copy_frame(
      void *data,
      void *chain_data,
      video_frame_info_t *video_info,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   gl_t               *gl = (gl_t*)data;
   const GLvoid *data_buf = frame;
   glPixelStorei(GL_UNPACK_ALIGNMENT, video_pixel_get_alignment(pitch));

   if (gl->base_size == 2 && !gl->have_es2_compat)
   {
      /* Convert to 32-bit textures on desktop GL.
       *
       * It is *much* faster (order of magnitude on my setup)
       * to use a custom SIMD-optimized conversion routine
       * than letting GL do it. */
      video_frame_convert_rgb16_to_rgb32(
            &gl->scaler,
            gl->conv_buffer,
            frame,
            width,
            height,
            pitch);
      data_buf = gl->conv_buffer;
   }
   else
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);

   glTexSubImage2D(GL_TEXTURE_2D,
         0, 0, 0, width, height, gl->texture_type,
         gl->texture_fmt, data_buf);

   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

static void gl1_renderchain_readback(void *data,
      void *chain_data,
      unsigned alignment,
      unsigned fmt, unsigned type,
      void *src)
{
   gl_t *gl = (gl_t*)data;

   glPixelStorei(GL_PACK_ALIGNMENT, alignment);
   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glReadBuffer(GL_BACK);

   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         (GLenum)fmt, (GLenum)type, (GLvoid*)src);
}

static void gl1_renderchain_set_mvp(void *data,
      void *chain_data,
      void *shader_data, const void *mat_data)
{
   math_matrix_4x4 ident;
   const math_matrix_4x4 *mat = (const math_matrix_4x4*)mat_data;

   /* Fall back to fixed function-style if needed and possible. */
   glMatrixMode(GL_PROJECTION);
   glLoadMatrixf(mat->data);
   glMatrixMode(GL_MODELVIEW);
   matrix_4x4_identity(ident);
   glLoadMatrixf(ident.data);
}

static void gl1_renderchain_set_coords(void *handle_data,
      void *chain_data,
      void *shader_data, const struct video_coords *coords)
{
   /* Fall back to fixed function-style if needed and possible. */
   glClientActiveTexture(GL_TEXTURE1);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->lut_tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glClientActiveTexture(GL_TEXTURE0);
   glVertexPointer(2, GL_FLOAT, 0, coords->vertex);
   glEnableClientState(GL_VERTEX_ARRAY);
   glColorPointer(4, GL_FLOAT, 0, coords->color);
   glEnableClientState(GL_COLOR_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

gl_renderchain_driver_t gl2_renderchain = {
   gl1_renderchain_set_coords,
   gl1_renderchain_set_mvp,
   NULL,                                  /* init_textures_reference */
   NULL,                                  /* fence_iterate */
   NULL,                                  /* fence_free */
   gl1_renderchain_readback,
   NULL,                                  /* renderchain_init_pbo */
   NULL,                                  /* renderchain_bind_pbo */
   NULL,                                  /* renderchain_unbind_pbo */
   gl1_renderchain_copy_frame,
   gl1_renderchain_restore_default_state,
   NULL,                                  /* new_vao */
   NULL,                                  /* free_vao */
   NULL,                                  /* bind_vao */
   NULL,                                  /* unbind_vao */
   gl1_renderchain_disable_client_arrays, /* disable_client_arrays */
   gl1_renderchain_ff_vertex,             /* ff_vertex */
   gl1_renderchain_ff_matrix,
   NULL,                                  /* bind_backbuffer */
   NULL,                                  /* deinit_fbo */
   gl1_renderchain_viewport_info,
   gl1_renderchain_read_viewport,
   gl1_renderchain_bind_prev_texture,
   gl1_renderchain_free_internal,
   gl1_renderchain_new,
   NULL,                                  /* renderchain_init */
   NULL,                                  /* init_hw_render */
   gl1_renderchain_free,
   NULL,                                  /* deinit_hw_render     */
   NULL,                                  /* start_render         */
   NULL,                                  /* check_fbo_dimensions */
   NULL,                                  /* recompute_pass_sizes */
   NULL,                                  /* renderchain_render   */
   NULL,                                  /* resolve_extensions   */
   "gl1",
};

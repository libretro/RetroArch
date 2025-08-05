/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

/* Modern OpenGL driver.
 *
 * Minimum version (desktop): OpenGL   3.2+ (2009)
 * Minimum version (mobile) : OpenGLES 3.0+ (2012)
 */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdlib.h>

#include "../common/gl3_defines.h"

#include <encodings/utf.h>
#include <gfx/gl_capabilities.h>
#include <gfx/video_frame.h>
#include <glsym/glsym.h>
#include <string/stdstring.h>
#include <retro_math.h>

#include "../../configuration.h"
#include "../../dynamic.h"
#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif

#include "../../retroarch.h"
#include "../../verbosity.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../font_driver.h"
#include "../../record/record_driver.h"

#ifdef HAVE_GLSL
#include "../drivers_shader/shader_glsl.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#define GL3_SET_TEXTURE_COORDS(coords, xamt, yamt) \
   coords[2] = xamt; \
   coords[6] = xamt; \
   coords[5] = yamt; \
   coords[7] = yamt

struct gl3_streamed_texture
{
   GLuint tex;
   unsigned width;
   unsigned height;
};

typedef struct gl3
{
   const gfx_ctx_driver_t *ctx_driver;
   void *ctx_data;
   gl3_filter_chain_t *filter_chain;
   gl3_filter_chain_t *filter_chain_default;
   GLuint *overlay_tex;
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;
   GLsync fences[GL_CORE_NUM_FENCES];
   void *readback_buffer_screenshot;
   struct scaler_ctx pbo_readback_scaler;

   video_info_t video_info;
   video_viewport_t vp;
   struct gl3_viewport filter_chain_vp;
   struct gl3_streamed_texture textures[GL_CORE_NUM_TEXTURES];

   GLuint vao;
   GLuint menu_texture;
   GLuint pbo_readback[GL_CORE_NUM_PBOS];

   /* Render chain for non-Slang shaders only. */
   struct
   {
      const shader_backend_t *shader;
      void *shader_data;
      unsigned num_fbo_passes;
      unsigned num_prev_textures;
      GLuint fbo_feedback;
      unsigned fbo_feedback_pass;
      GLuint fbo_feedback_texture;
      unsigned last_width[GL_CORE_NUM_TEXTURES];
      unsigned last_height[GL_CORE_NUM_TEXTURES];
      unsigned hw_render_last_width;
      unsigned hw_render_last_height;
      GLuint fbo[GFX_MAX_SHADERS];
      GLuint fbo_texture[GFX_MAX_SHADERS];
      struct video_fbo_rect fbo_rect[GFX_MAX_SHADERS];
      struct gfx_fbo_scale fbo_scale[GFX_MAX_SHADERS];
      struct video_tex_info tex_info;
      struct video_tex_info prev_info[GFX_MAX_TEXTURES];
      struct video_coords coords;
      const float *vertex_ptr;
      bool active;
      bool mipmap_active;
   } chain;

#ifdef HAVE_SLANG
   struct
   {
      GLuint alpha_blend;
#ifdef HAVE_SHADERPIPELINE
      GLuint font;
      GLuint ribbon;
      GLuint ribbon_simple;
      GLuint snow_simple;
      GLuint snow;
      GLuint bokeh;
#endif /* HAVE_SHADERPIPELINE */
      struct gl3_buffer_locations alpha_blend_loc;
#ifdef HAVE_SHADERPIPELINE
      struct gl3_buffer_locations font_loc;
      struct gl3_buffer_locations ribbon_loc;
      struct gl3_buffer_locations ribbon_simple_loc;
      struct gl3_buffer_locations snow_simple_loc;
      struct gl3_buffer_locations snow_loc;
      struct gl3_buffer_locations bokeh_loc;
#endif /* HAVE_SHADERPIPELINE */
   } pipelines;
#endif /* HAVE_SLANG */

   unsigned video_width;
   unsigned video_height;
   unsigned overlays;
   unsigned version_major;
   unsigned version_minor;
   unsigned out_vp_width;
   unsigned out_vp_height;
   unsigned rotation;
   unsigned textures_index;
   unsigned scratch_vbo_index;
   unsigned fence_count;
   unsigned pbo_readback_index;
   unsigned hw_render_max_width;
   unsigned hw_render_max_height;
   GLuint scratch_vbos[GL_CORE_NUM_VBOS];
   GLuint hw_render_texture;
   GLuint hw_render_fbo;
   GLuint hw_render_rb_ds;

   float menu_texture_alpha;
   math_matrix_4x4 mvp;                /* float alignment */
   math_matrix_4x4 mvp_yflip;
   math_matrix_4x4 mvp_no_rot;
   math_matrix_4x4 mvp_no_rot_yflip;

   uint16_t flags;

   bool pbo_readback_valid[GL_CORE_NUM_PBOS];
} gl3_t;

typedef struct gl3_video_shader_ctx_init
{
   const char *path;
   const shader_backend_t *shader;
   void *data;
   void *shader_data;
   enum rarch_shader_type shader_type;
} gl3_video_shader_ctx_init_t;

static const struct video_ortho gl3_default_ortho = {0, 1, 0, 1, -1, 1};

static const float gl3_vertexes_flipped[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float gl3_vertexes[8]         = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float gl3_tex_coords[8]       = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float gl3_colors[16]          = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

/**
 * FORWARD DECLARATIONS
 */
static void gl3_set_viewport(gl3_t *gl,
      unsigned vp_width, unsigned vp_height,
      bool force_full,   bool allow_rotate);

/**
 * GL3 COMMON
 */

void gl3_framebuffer_copy(
      GLuint fb_id,
      GLuint quad_program,
      GLuint quad_vbo,
      GLint flat_ubo_vertex,
      struct Size2D size,
      GLuint image)
{
   glBindFramebuffer(GL_FRAMEBUFFER, fb_id);
   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, image);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glViewport(0, 0, size.width, size.height);
   glClear(GL_COLOR_BUFFER_BIT);

   glUseProgram(quad_program);
   if (flat_ubo_vertex >= 0)
   {
      static float mvp[16] = {
                                2.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 2.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 2.0f, 0.0f,
                               -1.0f,-1.0f, 0.0f, 1.0f
                             };
      glUniform4fv(flat_ubo_vertex, 4, mvp);
   }

   /* Draw quad */
   glDisable(GL_CULL_FACE);
   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void *)((uintptr_t)(0)));
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void *)((uintptr_t)(2 * sizeof(float))));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);

   glUseProgram(0);
   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void gl3_framebuffer_copy_partial(
      GLuint fb_id,
      GLuint quad_program,
      GLint flat_ubo_vertex,
      struct Size2D size,
      GLuint image,
      float rx, float ry)
{
   GLuint vbo;
   const float quad_data[16] = {
      0.0f, 0.0f, 0.0f, 0.0f,
      1.0f, 0.0f, rx,   0.0f,
      0.0f, 1.0f, 0.0f, ry,
      1.0f, 1.0f, rx,   ry,
   };

   glBindFramebuffer(GL_FRAMEBUFFER, fb_id);
   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, image);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glViewport(0, 0, size.width, size.height);
   glClear(GL_COLOR_BUFFER_BIT);

   glUseProgram(quad_program);
   if (flat_ubo_vertex >= 0)
   {
      static float mvp[16] = {
                                2.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 2.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 2.0f, 0.0f,
                               -1.0f,-1.0f, 0.0f, 1.0f
                             };
      glUniform4fv(flat_ubo_vertex, 4, mvp);
   }
   glDisable(GL_CULL_FACE);
   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);

   /* A bit crude, but heeeey. */
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);

   glBufferData(GL_ARRAY_BUFFER, sizeof(quad_data), quad_data, GL_STREAM_DRAW);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void *)((uintptr_t)(0)));
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         (void *)((uintptr_t)(2 * sizeof(float))));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDeleteBuffers(1, &vbo);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glUseProgram(0);
   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint gl3_compile_shader(GLenum stage, const char *source)
{
   GLint status;
   GLuint shader   = glCreateShader(stage);
   const char *ptr = source;

   glShaderSource(shader, 1, &ptr, NULL);
   glCompileShader(shader);

   glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

   if (!status)
   {
      GLint length;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
      if (length > 0)
      {
         char *info_log = (char*)malloc(length);

         if (info_log)
         {
            glGetShaderInfoLog(shader, length, &length, info_log);
            RARCH_ERR("[GLCore] Failed to compile shader: %s\n", info_log);
            free(info_log);
            glDeleteShader(shader);
            return 0;
         }
      }
   }

   return shader;
}

uint32_t gl3_get_cross_compiler_target_version(void)
{
   const char *version = (const char*)glGetString(GL_VERSION);
   unsigned major      = 0;
   unsigned minor      = 0;

#ifdef HAVE_OPENGLES3
   if (!version || sscanf(version, "OpenGL ES %u.%u", &major, &minor) != 2)
      return 300;

   if (major == 2 && minor == 0)
      return 100;
#else
   if (!version || sscanf(version, "%u.%u", &major, &minor) != 2)
      return 150;

   if (major == 3)
   {
      switch (minor)
      {
         case 2:
            return 150;
         case 1:
            return 140;
         case 0:
            return 130;
      }
   }
   else if (major == 2)
   {
      switch (minor)
      {
         case 1:
            return 120;
         case 0:
            return 110;
      }
   }
#endif

   return 100 * major + 10 * minor;
}

static void gl3_bind_scratch_vbo(gl3_t *gl, const void *data, size_t len)
{
   if (!gl->scratch_vbos[gl->scratch_vbo_index])
      glGenBuffers(1, &gl->scratch_vbos[gl->scratch_vbo_index]);
   glBindBuffer(GL_ARRAY_BUFFER, gl->scratch_vbos[gl->scratch_vbo_index]);
   glBufferData(GL_ARRAY_BUFFER, len, data, GL_STREAM_DRAW);
   gl->scratch_vbo_index++;
   if (gl->scratch_vbo_index >= GL_CORE_NUM_VBOS)
      gl->scratch_vbo_index = 0;
}


/**
 * DISPLAY DRIVER
 */

static void *gfx_display_gl3_get_default_mvp(void *data)
{
   gl3_t *gl3 = (gl3_t*)data;
   if (!gl3)
      return NULL;
   return &gl3->mvp_no_rot;
}

static const float *gfx_display_gl3_get_default_vertices(void)
{
   return &gl3_vertexes[0];
}

static const float *gfx_display_gl3_get_default_tex_coords(void)
{
   return &gl3_tex_coords[0];
}

static void gfx_display_gl3_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
#ifdef HAVE_SHADERPIPELINE
   static float t                = 0.0f;
   float yflip                   = 0.0f;
   video_coord_array_t *ca       = &p_disp->dispca;
   gl3_t *gl                 = (gl3_t*)data;

   if (!gl || !draw)
      return;

   draw->x                       = 0;
   draw->y                       = 0;
   draw->matrix_data             = NULL;

   if (gl->chain.active)
   {
      struct uniform_info uniform_param;

      draw->coords = (struct video_coords*)(&ca->coords);

      switch (draw->pipeline_id)
      {
         case VIDEO_SHADER_MENU:
         case VIDEO_SHADER_MENU_2:
            glBlendFunc(GL_DST_COLOR, GL_ONE);
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
            gl->chain.shader->use(gl, gl->chain.shader_data, draw->pipeline_id,
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

            gl->chain.shader->set_uniform_parameter(gl->chain.shader_data,
                  &uniform_param, NULL);
            break;
      }

      switch (draw->pipeline_id)
      {
         case VIDEO_SHADER_MENU_3:
         case VIDEO_SHADER_MENU_4:
         case VIDEO_SHADER_MENU_5:
         case VIDEO_SHADER_MENU_6:
            uniform_param.type              = UNIFORM_2F;
            uniform_param.lookup.ident      = "OutputSize";
            uniform_param.result.f.v0       = draw->width;
            uniform_param.result.f.v1       = draw->height;

            gl->chain.shader->set_uniform_parameter(gl->chain.shader_data,
                  &uniform_param, NULL);
            break;
      }
   }
#ifdef HAVE_SLANG
   else
   {
      static struct video_coords blank_coords;
      static uint8_t ubo_scratch_data[768];
      float output_size[2];
      output_size[0]                = (float)video_width;
      output_size[1]                = (float)video_height;

      switch (draw->pipeline_id)
      {
         /* Ribbon */
         default:
         case VIDEO_SHADER_MENU:
         case VIDEO_SHADER_MENU_2:
            draw->coords                     = (struct video_coords*)&ca->coords;
            draw->backend_data               = ubo_scratch_data;
            draw->backend_data_size          = 2 * sizeof(float);

            /* Match UBO layout in shader. */
            yflip = -1.0f;
            memcpy(ubo_scratch_data, &t, sizeof(t));
            memcpy(ubo_scratch_data + sizeof(float), &yflip, sizeof(yflip));
            break;

         /* Snow simple */
         case VIDEO_SHADER_MENU_3:
         case VIDEO_SHADER_MENU_4:
         case VIDEO_SHADER_MENU_5:
            draw->backend_data               = ubo_scratch_data;
            draw->backend_data_size          = sizeof(math_matrix_4x4)
               + 4 * sizeof(float);

            /* Match UBO layout in shader. */
            memcpy(ubo_scratch_data,
                  &gl->mvp_no_rot,
                  sizeof(math_matrix_4x4));
            memcpy(ubo_scratch_data + sizeof(math_matrix_4x4),
                  output_size,
                  sizeof(output_size));

            if (draw->pipeline_id == VIDEO_SHADER_MENU_5)
               yflip = 1.0f;

            memcpy(ubo_scratch_data + sizeof(math_matrix_4x4)
                  + 2 * sizeof(float), &t, sizeof(t));
            memcpy(ubo_scratch_data + sizeof(math_matrix_4x4)
                  + 3 * sizeof(float), &yflip, sizeof(yflip));
            draw->coords          = &blank_coords;
            blank_coords.vertices = 4;
            draw->prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
            break;
      }
   }
#endif

   t += 0.01;
#endif
}

static GLenum gfx_display_prim_to_gl3_enum(
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

static void gfx_display_gl3_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   gl3_t *gl                 = (gl3_t*)data;

   if (!gl || !draw)
      return;

   if (!draw->coords->vertex)
      draw->coords->vertex          = gfx_display_gl3_get_default_vertices();
   if (!draw->coords->tex_coord)
      draw->coords->tex_coord       = &gl3_tex_coords[0];
   if (!draw->coords->color)
      draw->coords->color           = &gl3_colors[0];

   glViewport(draw->x, draw->y, draw->width, draw->height);

   if (gl->chain.active)
   {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, draw->texture);

      gl->chain.shader->set_coords(gl->chain.shader_data, draw->coords);
      gl->chain.shader->set_mvp(gl->chain.shader_data,
            draw->matrix_data ? (math_matrix_4x4*)draw->matrix_data
         : (math_matrix_4x4*)&gl->mvp_no_rot);

      glDrawArrays(gfx_display_prim_to_gl3_enum(
               draw->prim_type), 0, draw->coords->vertices);
   }
#ifdef HAVE_SLANG
   else
   {
      const struct
         gl3_buffer_locations
         *loc                   = NULL;

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, draw->texture);

      switch (draw->pipeline_id)
      {
         case VIDEO_SHADER_MENU:
         case VIDEO_SHADER_MENU_2:
            glBlendFunc(GL_DST_COLOR, GL_ONE);
            break;
         default:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
      }

      switch (draw->pipeline_id)
      {
#ifdef HAVE_SHADERPIPELINE
         case VIDEO_SHADER_MENU:
            glUseProgram(gl->pipelines.ribbon);
            loc = &gl->pipelines.ribbon_loc;
            break;

         case VIDEO_SHADER_MENU_2:
            glUseProgram(gl->pipelines.ribbon_simple);
            loc = &gl->pipelines.ribbon_simple_loc;
            break;

         case VIDEO_SHADER_MENU_3:
            glUseProgram(gl->pipelines.snow_simple);
            loc = &gl->pipelines.snow_simple_loc;
            break;

         case VIDEO_SHADER_MENU_4:
            glUseProgram(gl->pipelines.snow);
            loc = &gl->pipelines.snow_loc;
            break;

         case VIDEO_SHADER_MENU_5:
            glUseProgram(gl->pipelines.bokeh);
            loc = &gl->pipelines.bokeh_loc;
            break;
#endif /* HAVE_SHADERPIPELINE */

         default:
            glUseProgram(gl->pipelines.alpha_blend);
            break;
      }

      if (loc)
      {
         if (loc->flat_ubo_vertex >= 0)
            glUniform4fv(loc->flat_ubo_vertex,
                  (GLsizei)((draw->backend_data_size + 15) / 16),
                  (const GLfloat*)draw->backend_data);

         if (loc->flat_ubo_fragment >= 0)
            glUniform4fv(loc->flat_ubo_fragment,
                  (GLsizei)((draw->backend_data_size + 15) / 16),
                  (const GLfloat*)draw->backend_data);
      }
      else
      {
         const math_matrix_4x4 *mat = draw->matrix_data
                        ? (const math_matrix_4x4*)draw->matrix_data
                        : (const math_matrix_4x4*)&gl->mvp_no_rot;
         if (gl->pipelines.alpha_blend_loc.flat_ubo_vertex >= 0)
            glUniform4fv(gl->pipelines.alpha_blend_loc.flat_ubo_vertex,
                         4, mat->data);
      }

      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glEnableVertexAttribArray(2);

      gl3_bind_scratch_vbo(gl, draw->coords->vertex,
            2 * sizeof(float) * draw->coords->vertices);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            2 * sizeof(float), (void *)(uintptr_t)0);
      gl3_bind_scratch_vbo(gl, draw->coords->tex_coord,
            2 * sizeof(float) * draw->coords->vertices);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            2 * sizeof(float), (void *)(uintptr_t)0);
      gl3_bind_scratch_vbo(gl, draw->coords->color,
            4 * sizeof(float) * draw->coords->vertices);
      glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE,
            4 * sizeof(float), (void *)(uintptr_t)0);

      switch (draw->prim_type)
      {
         case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
            glDrawArrays(GL_TRIANGLE_STRIP, 0, draw->coords->vertices);
            break;
         case GFX_DISPLAY_PRIM_TRIANGLES:
            glDrawArrays(GL_TRIANGLES, 0, draw->coords->vertices);
            break;
         case GFX_DISPLAY_PRIM_NONE:
         default:
            break;
      }

      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glDisableVertexAttribArray(2);
   }
#endif /* HAVE_SLANG */

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindTexture(GL_TEXTURE_2D, 0);
}

static void gfx_display_gl3_blend_begin(void *data)
{
   gl3_t *gl = (gl3_t*)data;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (gl->chain.active)
      gl->chain.shader->use(gl, gl->chain.shader_data, VIDEO_SHADER_STOCK_BLEND, true);
#ifdef HAVE_SLANG
   else
      glUseProgram(gl->pipelines.alpha_blend);
#endif
}

static void gfx_display_gl3_blend_end(void *data)
{
   glDisable(GL_BLEND);
}

static void gfx_display_gl3_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   glScissor(x, video_height - y - height, width, height);
   glEnable(GL_SCISSOR_TEST);
}

static void gfx_display_gl3_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   glDisable(GL_SCISSOR_TEST);
}

gfx_display_ctx_driver_t gfx_display_ctx_gl3 = {
   gfx_display_gl3_draw,
   gfx_display_gl3_draw_pipeline,
   gfx_display_gl3_blend_begin,
   gfx_display_gl3_blend_end,
   gfx_display_gl3_get_default_mvp,
   gfx_display_gl3_get_default_vertices,
   gfx_display_gl3_get_default_tex_coords,
   FONT_DRIVER_RENDER_OPENGL_CORE_API,
   GFX_VIDEO_DRIVER_OPENGL_CORE,
   "glcore",
   false,
   gfx_display_gl3_scissor_begin,
   gfx_display_gl3_scissor_end
};

/**
 * FONT DRIVER
 */

/* TODO: Move viewport side effects to the caller: it's a source of bugs. */

#define GL_CORE_RASTER_FONT_EMIT(c, vx, vy) \
   font_vertex[     2 * (6 * i + c) + 0] = (x + (delta_x + off_x + vx * width) * scale) * inv_win_width; \
   font_vertex[     2 * (6 * i + c) + 1] = (y + (delta_y - off_y - vy * height) * scale) * inv_win_height; \
   font_tex_coords[ 2 * (6 * i + c) + 0] = (tex_x + vx * width) * inv_tex_size_x; \
   font_tex_coords[ 2 * (6 * i + c) + 1] = (tex_y + vy * height) * inv_tex_size_y; \
   font_color[      4 * (6 * i + c) + 0] = color[0]; \
   font_color[      4 * (6 * i + c) + 1] = color[1]; \
   font_color[      4 * (6 * i + c) + 2] = color[2]; \
   font_color[      4 * (6 * i + c) + 3] = color[3]

#define MAX_MSG_LEN_CHUNK 64

typedef struct
{
   gl3_t *gl;
   GLuint tex;

   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;

   video_font_raster_block_t *block;
} gl3_raster_t;

static void gl3_raster_font_free(void *data,
      bool is_threaded)
{
   gl3_raster_t *font = (gl3_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (is_threaded)
      if (
            font->gl &&
            font->gl->ctx_driver &&
            font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(true);

   glDeleteTextures(1, &font->tex);

   free(font);
}

static void gl3_raster_font_upload_atlas(gl3_raster_t *font)
{
   if (font->tex)
      glDeleteTextures(1, &font->tex);
   glGenTextures(1, &font->tex);
   glBindTexture(GL_TEXTURE_2D, font->tex);

   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, font->atlas->width, font->atlas->height);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                   font->atlas->width, font->atlas->height, GL_RED, GL_UNSIGNED_BYTE, font->atlas->buffer);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glBindTexture(GL_TEXTURE_2D, 0);
}

static void *gl3_raster_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   gl3_raster_t *font = (gl3_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gl = (gl3_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   if (is_threaded)
      if (
            font->gl &&
            font->gl->ctx_driver &&
            font->gl->ctx_driver->make_current)
         font->gl->ctx_driver->make_current(false);

   font->atlas      = font->font_driver->get_atlas(font->font_data);

   gl3_raster_font_upload_atlas(font);

   font->atlas->dirty = false;
   return font;
}

static int gl3_raster_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   const struct font_glyph* glyph_q = NULL;
   gl3_raster_t *font   = (gl3_raster_t*)data;
   const char* msg_end = msg + msg_len;
   int delta_x         = 0;

   if (     !font
         || !font->font_driver
         || !font->font_data )
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      const struct font_glyph *glyph;
      unsigned code                  = utf8_walk(&msg);

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(
            font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void gl3_raster_font_draw_vertices(gl3_t *gl,
      gl3_raster_t *font,
      const video_coords_t *coords)
{
   if (font->atlas->dirty)
   {
      gl3_raster_font_upload_atlas(font);
      font->atlas->dirty   = false;
   }

   if (gl->chain.active)
   {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, font->tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

      gl->chain.shader->set_coords(gl->chain.shader_data, coords);
      gl->chain.shader->set_mvp(gl->chain.shader_data, &gl->mvp_no_rot);

      glDrawArrays(GL_TRIANGLES, 0, coords->vertices);
   }
#ifdef HAVE_SLANG
   else
   {
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, font->tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

      if (gl->pipelines.font_loc.flat_ubo_vertex >= 0)
         glUniform4fv(gl->pipelines.font_loc.flat_ubo_vertex,
                      4, gl->mvp_no_rot.data);

      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glEnableVertexAttribArray(2);

      gl3_bind_scratch_vbo(gl, coords->vertex,
            2 * sizeof(float)  * coords->vertices);
      glVertexAttribPointer(0, 2,
            GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)(uintptr_t)0);
      gl3_bind_scratch_vbo(gl, coords->tex_coord,
            2 * sizeof(float)  * coords->vertices);
      glVertexAttribPointer(1, 2,
            GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)(uintptr_t)0);
      gl3_bind_scratch_vbo(gl, coords->color,
            4 * sizeof(float) * coords->vertices);
      glVertexAttribPointer(2, 4,
            GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(uintptr_t)0);

      glDrawArrays(GL_TRIANGLES, 0, coords->vertices);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glDisableVertexAttribArray(2);
   }
#endif

   glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void gl3_raster_font_render_line(gl3_t *gl,
      gl3_raster_t *font,
      const struct font_glyph* glyph_q,
      const char *msg, size_t msg_len,
      GLfloat scale, const GLfloat color[4],
      GLfloat pos_x,
      GLfloat pos_y,
      int pre_x,
      float inv_tex_size_x,
      float inv_tex_size_y,
      float inv_win_width,
      float inv_win_height,
      unsigned text_align)
{
   int i;
   struct video_coords coords;
   GLfloat font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_vertex    [2 * 6 * MAX_MSG_LEN_CHUNK];
   GLfloat font_color     [4 * 6 * MAX_MSG_LEN_CHUNK];
   const char* msg_end  = msg + msg_len;
   int x                = pre_x;
   int y                = roundf(pos_y * gl->vp.height);
   int delta_x          = 0;
   int delta_y          = 0;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= gl3_raster_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= gl3_raster_font_get_message_width(font, msg, msg_len, scale) / 2.0;
         break;
   }

   while (msg < msg_end)
   {
      i = 0;
      while ((i < MAX_MSG_LEN_CHUNK) && (msg < msg_end))
      {
         const struct font_glyph *glyph;
         int off_x, off_y, tex_x, tex_y, width, height;
         unsigned code = utf8_walk(&msg);

         /* Do something smarter here ... */
         if (!(glyph = font->font_driver->get_glyph(
               font->font_data, code)))
            if (!(glyph = glyph_q))
               continue;

         off_x  = glyph->draw_offset_x;
         off_y  = glyph->draw_offset_y;
         tex_x  = glyph->atlas_offset_x;
         tex_y  = glyph->atlas_offset_y;
         width  = glyph->width;
         height = glyph->height;

         GL_CORE_RASTER_FONT_EMIT(0, 0, 1); /* Bottom-left */
         GL_CORE_RASTER_FONT_EMIT(1, 1, 1); /* Bottom-right */
         GL_CORE_RASTER_FONT_EMIT(2, 0, 0); /* Top-left */

         GL_CORE_RASTER_FONT_EMIT(3, 1, 0); /* Top-right */
         GL_CORE_RASTER_FONT_EMIT(4, 0, 0); /* Top-left */
         GL_CORE_RASTER_FONT_EMIT(5, 1, 1); /* Bottom-right */

         i++;

         delta_x += glyph->advance_x;
         delta_y -= glyph->advance_y;
      }

      coords.tex_coord     = font_tex_coords;
      coords.vertex        = font_vertex;
      coords.color         = font_color;
      coords.vertices      = i * 6;
      coords.lut_tex_coord = font_tex_coords;

      if (font->block)
         video_coord_array_append(&font->block->carr,
               &coords, coords.vertices);
      else
         gl3_raster_font_draw_vertices(gl, font, &coords);
   }
}

static void gl3_raster_font_render_message(
      gl3_t *gl,
      gl3_raster_t *font, const char *msg, GLfloat scale,
      const GLfloat color[4], GLfloat pos_x, GLfloat pos_y,
      unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   float inv_tex_size_x = 1.0f / font->atlas->width;
   float inv_tex_size_y = 1.0f / font->atlas->height;
   float inv_win_width  = 1.0f / gl->vp.width;
   float inv_win_height = 1.0f / gl->vp.height;
   int x                = roundf(pos_x * gl->vp.width);
   const struct font_glyph* glyph_q = font->font_driver->get_glyph(font->font_data, '?');
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / gl->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (size_t)(delim - msg) : strlen(msg);

      /* Draw the line */
      gl3_raster_font_render_line(gl, font,
            glyph_q,
            msg, msg_len, scale, color,
            pos_x,
            pos_y - (float)lines * line_height,
            x,
            inv_tex_size_x,
            inv_tex_size_y,
            inv_win_width,
            inv_win_height,
            text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void gl3_raster_font_setup_viewport(
      gl3_t *gl,
      unsigned width, unsigned height,
      gl3_raster_t *font, bool full_screen)
{
   gl3_set_viewport(gl, width, height, full_screen, false);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   if (gl->chain.active)
   {
      if (gl->chain.shader && gl->chain.shader->use)
         gl->chain.shader->use(gl, gl->chain.shader_data, VIDEO_SHADER_STOCK_BLEND, true);
   }
#ifdef HAVE_SLANG
   else
      glUseProgram(gl->pipelines.font);
#endif
}

static void gl3_raster_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   GLfloat color[4];
   int drop_x, drop_y;
   GLfloat x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align   = TEXT_ALIGN_LEFT;
   bool full_screen                 = false;
   gl3_raster_t           *font     = (gl3_raster_t*)data;
   gl3_t *gl                        = (gl3_t*)userdata;
   unsigned width                   = gl->video_width;
   unsigned height                  = gl->video_height;
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;

   if (!font || string_is_empty(msg) || !gl)
      return;

   if (params)
   {
      x           = params->x;
      y           = params->y;
      scale       = params->scale;
      full_screen = params->full_screen;
      text_align  = params->text_align;
      drop_x      = params->drop_x;
      drop_y      = params->drop_y;
      drop_mod    = params->drop_mod;
      drop_alpha  = params->drop_alpha;

      color[0]    = FONT_COLOR_GET_RED(params->color)   / 255.0f;
      color[1]    = FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      color[2]    = FONT_COLOR_GET_BLUE(params->color)  / 255.0f;
      color[3]    = FONT_COLOR_GET_ALPHA(params->color) / 255.0f;

      /* If alpha is 0.0f, turn it into default 1.0f */
      if (color[3] <= 0.0f)
         color[3] = 1.0f;
   }
   else
   {
      x                    = video_msg_pos_x;
      y                    = video_msg_pos_y;
      scale                = 1.0f;
      full_screen          = true;
      text_align           = TEXT_ALIGN_LEFT;

      color[0]             = video_msg_color_r;
      color[1]             = video_msg_color_g;
      color[2]             = video_msg_color_b;
      color[3]             = 1.0f;

      drop_x               = -2;
      drop_y               = -2;
      drop_mod             = 0.3f;
      drop_alpha           = 1.0f;
   }

   if (font->block)
      font->block->fullscreen = full_screen;
   else
      gl3_raster_font_setup_viewport(gl, width, height, font, full_screen);

   if (!string_is_empty(msg)
         && font->font_data  && font->font_driver)
   {
      if (drop_x || drop_y)
      {
         GLfloat color_dark[4];

         color_dark[0] = color[0] * drop_mod;
         color_dark[1] = color[1] * drop_mod;
         color_dark[2] = color[2] * drop_mod;
         color_dark[3] = color[3] * drop_alpha;

         gl3_raster_font_render_message(gl, font, msg, scale, color_dark,
               x + scale * drop_x / gl->vp.width,
               y + scale * drop_y / gl->vp.height, text_align);
      }

      gl3_raster_font_render_message(gl, font, msg, scale, color,
            x, y, text_align);
   }

   if (!font->block)
   {
      glDisable(GL_BLEND);
      gl3_set_viewport(gl, width, height, false, true);
   }
}

static const struct font_glyph *gl3_raster_font_get_glyph(
      void *data, uint32_t code)
{
   gl3_raster_t *font = (gl3_raster_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static void gl3_raster_font_flush_block(unsigned width, unsigned height,
      void *data)
{
   gl3_raster_t          *font       = (gl3_raster_t*)data;
   video_font_raster_block_t *block  = font ? font->block : NULL;
   gl3_t *gl                         = font ? font->gl    : NULL;

   if (!font || !block || !block->carr.coords.vertices || !gl)
      return;

   gl3_raster_font_setup_viewport(gl, width, height, font, block->fullscreen);
   gl3_raster_font_draw_vertices(gl, font, (video_coords_t*)&block->carr.coords);

   glDisable(GL_BLEND);
   gl3_set_viewport(gl, width, height, block->fullscreen, true);
}

static void gl3_raster_font_bind_block(void *data, void *userdata)
{
   gl3_raster_t                *font = (gl3_raster_t*)data;
   video_font_raster_block_t *block = (video_font_raster_block_t*)userdata;

   if (font)
      font->block = block;
}

static bool gl3_raster_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   gl3_raster_t *font   = (gl3_raster_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t gl3_raster_font = {
   gl3_raster_font_init,
   gl3_raster_font_free,
   gl3_raster_font_render_msg,
   "glcore",
   gl3_raster_font_get_glyph,
   gl3_raster_font_bind_block,
   gl3_raster_font_flush_block,
   gl3_raster_font_get_message_width,
   gl3_raster_font_get_line_metrics
};

/**
 * VIDEO DRIVER
 */

static void gl3_deinit_fences(gl3_t *gl)
{
   size_t i;
   for (i = 0; i < gl->fence_count; i++)
   {
      if (gl->fences[i])
         glDeleteSync(gl->fences[i]);
   }
   gl->fence_count = 0;
   memset(gl->fences, 0, sizeof(gl->fences));
}

static bool gl3_init_pbo_readback(gl3_t *gl)
{
   int i;
   struct scaler_ctx *scaler  = NULL;

   glGenBuffers(GL_CORE_NUM_PBOS, gl->pbo_readback);

   for (i = 0; i < GL_CORE_NUM_PBOS; i++)
   {
      glBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[i]);
      glBufferData(GL_PIXEL_PACK_BUFFER,
            gl->vp.width * gl->vp.height * sizeof(uint32_t),
            NULL, GL_STREAM_READ);
   }
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

   scaler                    = &gl->pbo_readback_scaler;

   scaler->in_width          = gl->vp.width;
   scaler->in_height         = gl->vp.height;
   scaler->out_width         = gl->vp.width;
   scaler->out_height        = gl->vp.height;
   scaler->in_stride         = gl->vp.width * sizeof(uint32_t);
   scaler->out_stride        = gl->vp.width * 3;
   scaler->in_fmt            = SCALER_FMT_ABGR8888;
   scaler->out_fmt           = SCALER_FMT_BGR24;
   scaler->scaler_type       = SCALER_TYPE_POINT;

   if (!scaler_ctx_gen_filter(scaler))
   {
      gl->flags &= ~GL3_FLAG_PBO_READBACK_ENABLE;
      RARCH_ERR("[GLCore] Failed to initialize pixel conversion for PBO.\n");
      glDeleteBuffers(4, gl->pbo_readback);
      memset(gl->pbo_readback, 0, sizeof(gl->pbo_readback));
      return false;
   }

   return true;
}

static void gl3_deinit_pbo_readback(gl3_t *gl)
{
   int i;
   for (i = 0; i < GL_CORE_NUM_PBOS; i++)
      if (gl->pbo_readback[i] != 0)
         glDeleteBuffers(1, &gl->pbo_readback[i]);
   memset(gl->pbo_readback, 0, sizeof(gl->pbo_readback));
   scaler_ctx_gen_reset(&gl->pbo_readback_scaler);
}

static void gl3_pbo_async_readback(gl3_t *gl)
{
   glBindBuffer(GL_PIXEL_PACK_BUFFER,
         gl->pbo_readback[gl->pbo_readback_index++]);
   glPixelStorei(GL_PACK_ALIGNMENT, 4);
   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
#ifndef HAVE_OPENGLES
   glReadBuffer(GL_BACK);
#endif
   if (gl->pbo_readback_index >= GL_CORE_NUM_PBOS)
      gl->pbo_readback_index = 0;
   gl->pbo_readback_valid[gl->pbo_readback_index] = true;

   glReadPixels(gl->vp.x, gl->vp.y,
                gl->vp.width, gl->vp.height,
                GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

static void gl3_fence_iterate(gl3_t *gl, unsigned hard_sync_frames)
{
   if (gl->fence_count < GL_CORE_NUM_FENCES)
   {
      /*
       * We need to do some work after the flip, or we risk fencing too early.
       * Do as little work as possible.
       */
      glEnable(GL_SCISSOR_TEST);
      glScissor(0, 0, 1, 1);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glClear(GL_COLOR_BUFFER_BIT);
      glDisable(GL_SCISSOR_TEST);

      gl->fences[gl->fence_count++] = glFenceSync(
            GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   }

   while (gl->fence_count > hard_sync_frames)
   {
      glClientWaitSync(gl->fences[0], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
      glDeleteSync(gl->fences[0]);
      gl->fence_count--;
      memmove(gl->fences, gl->fences + 1, gl->fence_count * sizeof(GLsync));
   }
}

#ifdef HAVE_OVERLAY
static void gl3_free_overlay(gl3_t *gl)
{
   if (gl->overlay_tex)
      glDeleteTextures(gl->overlays, gl->overlay_tex);

   free(gl->overlay_tex);
   free(gl->overlay_vertex_coord);
   free(gl->overlay_tex_coord);
   free(gl->overlay_color_coord);
   gl->overlay_tex          = NULL;
   gl->overlay_vertex_coord = NULL;
   gl->overlay_tex_coord    = NULL;
   gl->overlay_color_coord  = NULL;
   gl->overlays             = 0;
}

static void gl3_free_scratch_vbos(gl3_t *gl)
{
   int i;
   for (i = 0; i < GL_CORE_NUM_VBOS; i++)
      if (gl->scratch_vbos[i])
         glDeleteBuffers(1, &gl->scratch_vbos[i]);
}

static void gl3_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   GLfloat *vertex = NULL;
   gl3_t       *gl = (gl3_t*)data;

   if (!gl)
      return;

   if (image > gl->overlays)
      return;

   vertex          = (GLfloat*)&gl->overlay_vertex_coord[image * 8];

   /* Flipped, so we preserve top-down semantics. */
   y               = 1.0f - y;
   h               = -h;

   vertex[0]       = x;
   vertex[1]       = y;
   vertex[2]       = x + w;
   vertex[3]       = y;
   vertex[4]       = x;
   vertex[5]       = y + h;
   vertex[6]       = x + w;
   vertex[7]       = y + h;
}

static void gl3_overlay_tex_geom(void *data,
      unsigned image,
      GLfloat x, GLfloat y,
      GLfloat w, GLfloat h)
{
   GLfloat *tex = NULL;
   gl3_t *gl    = (gl3_t*)data;

   if (!gl)
      return;

   tex          = (GLfloat*)&gl->overlay_tex_coord[image * 8];

   tex[0]       = x;
   tex[1]       = y;
   tex[2]       = x + w;
   tex[3]       = y;
   tex[4]       = x;
   tex[5]       = y + h;
   tex[6]       = x + w;
   tex[7]       = y + h;
}

static void gl3_render_overlay(gl3_t *gl,
      unsigned width, unsigned height)
{
   size_t i;

   glEnable(GL_BLEND);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DEPTH_TEST);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   if (gl->flags & GL3_FLAG_OVERLAY_FULLSCREEN)
      glViewport(0, 0, width, height);

#ifdef HAVE_SLANG
   /* Ensure that we reset the attrib array. */
   glUseProgram(gl->pipelines.alpha_blend);
   if (gl->pipelines.alpha_blend_loc.flat_ubo_vertex >= 0)
      glUniform4fv(gl->pipelines.alpha_blend_loc.flat_ubo_vertex, 4, gl->mvp_no_rot.data);
#endif

   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glEnableVertexAttribArray(2);

   gl3_bind_scratch_vbo(gl, gl->overlay_vertex_coord,
         8 * sizeof(float) * gl->overlays);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
         2 * sizeof(float), (void *)(uintptr_t)0);
   gl3_bind_scratch_vbo(gl, gl->overlay_tex_coord,
         8 * sizeof(float) * gl->overlays);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
         2 * sizeof(float), (void *)(uintptr_t)0);
   gl3_bind_scratch_vbo(gl, gl->overlay_color_coord,
         16 * sizeof(float) * gl->overlays);
   glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE,
         4 * sizeof(float), (void *)(uintptr_t)0);

   for (i = 0; i < gl->overlays; i++)
   {
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, gl->overlay_tex[i]);
      glDrawArrays(GL_TRIANGLE_STRIP, (GLint)(4 * i), 4);
   }

   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glDisableVertexAttribArray(2);

   glDisable(GL_BLEND);
   glBindTexture(GL_TEXTURE_2D, 0);
   if (gl->flags & GL3_FLAG_OVERLAY_FULLSCREEN)
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
}
#endif

static void gl3_deinit_hw_render(gl3_t *gl)
{
   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);

   if (gl->hw_render_fbo)
      glDeleteFramebuffers(1, &gl->hw_render_fbo);
   if (gl->hw_render_rb_ds)
      glDeleteRenderbuffers(1, &gl->hw_render_rb_ds);
   if (gl->hw_render_texture)
      glDeleteTextures(1, &gl->hw_render_texture);

   gl->hw_render_fbo     = 0;
   gl->hw_render_rb_ds   = 0;
   gl->hw_render_texture = 0;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   gl->flags &= ~GL3_FLAG_HW_RENDER_ENABLE;
}

static void gl3_destroy_resources(gl3_t *gl)
{
   int i;
   if (!gl)
      return;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

#ifdef HAVE_SLANG
   if (gl->filter_chain)
      gl3_filter_chain_free(gl->filter_chain);
#endif
   gl->filter_chain = NULL;

#ifdef HAVE_SLANG
   if (gl->filter_chain_default)
      gl3_filter_chain_free(gl->filter_chain_default);
#endif
   gl->filter_chain_default = NULL;

   if (gl->chain.shader)
   {
      gl->chain.shader->deinit(gl->chain.shader_data);
      gl->chain.shader      = NULL;
      gl->chain.shader_data = NULL;
   }

   if (gl->chain.num_fbo_passes)
   {
      glDeleteFramebuffers(gl->chain.num_fbo_passes, gl->chain.fbo);
      glDeleteTextures(gl->chain.num_fbo_passes, gl->chain.fbo_texture);
      gl->chain.num_fbo_passes = 0;
   }

   if (gl->chain.fbo_feedback)
   {
      glDeleteFramebuffers(1, &gl->chain.fbo_feedback);
      gl->chain.fbo_feedback = 0;
   }

   if (gl->chain.fbo_feedback_texture)
   {
      glDeleteTextures(1, &gl->chain.fbo_feedback_texture);
      gl->chain.fbo_feedback_texture = 0;
   }

   glBindVertexArray(0);
   if (gl->vao != 0)
      glDeleteVertexArrays(1, &gl->vao);

   for (i = 0; i < GL_CORE_NUM_TEXTURES; i++)
   {
      if (gl->textures[i].tex != 0)
         glDeleteTextures(1, &gl->textures[i].tex);
   }
   memset(gl->textures, 0, sizeof(gl->textures));

   if (gl->menu_texture != 0)
      glDeleteTextures(1, &gl->menu_texture);

#ifdef HAVE_SLANG
   if (gl->pipelines.alpha_blend)
   {
      glDeleteProgram(gl->pipelines.alpha_blend);
      gl->pipelines.alpha_blend = 0;
   }
#ifdef HAVE_SHADERPIPELINE
   if (gl->pipelines.font)
   {
      glDeleteProgram(gl->pipelines.font);
      gl->pipelines.font = 0;
   }
   if (gl->pipelines.ribbon)
   {
      glDeleteProgram(gl->pipelines.ribbon);
      gl->pipelines.ribbon = 0;
   }
   if (gl->pipelines.ribbon_simple)
   {
      glDeleteProgram(gl->pipelines.ribbon_simple);
      gl->pipelines.ribbon_simple = 0;
   }
   if (gl->pipelines.snow_simple)
   {
      glDeleteProgram(gl->pipelines.snow_simple);
      gl->pipelines.snow_simple = 0;
   }
   if (gl->pipelines.snow)
   {
      glDeleteProgram(gl->pipelines.snow);
      gl->pipelines.snow = 0;
   }
   if (gl->pipelines.bokeh)
   {
      glDeleteProgram(gl->pipelines.bokeh);
      gl->pipelines.bokeh = 0;
   }
#endif /* HAVE_SHADERPIPELINE */
#endif /* HAVE_SLANG */

#ifdef HAVE_OVERLAY
   gl3_free_overlay(gl);
   gl3_free_scratch_vbos(gl);
#endif
   gl3_deinit_fences(gl);
   gl3_deinit_pbo_readback(gl);
   gl3_deinit_hw_render(gl);
}

static bool gl3_init_hw_render(gl3_t *gl, unsigned width, unsigned height)
{
   GLint max_fbo_size;
   GLint max_rb_size;
   GLenum status;
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);

   RARCH_LOG("[GLCore] Initializing HW render (%ux%u).\n", width, height);
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_fbo_size);
   glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_rb_size);
   RARCH_LOG("[GLCore] Max texture size: %d px, renderbuffer size: %d px.\n",
             max_fbo_size, max_rb_size);

   if (width > (unsigned)max_fbo_size)
       width = max_fbo_size;
   if (width > (unsigned)max_rb_size)
       width = max_rb_size;
   if (height > (unsigned)max_fbo_size)
       height = max_fbo_size;
   if (height > (unsigned)max_rb_size)
       height = max_rb_size;

   glGenFramebuffers(1, &gl->hw_render_fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, gl->hw_render_fbo);
   glGenTextures(1, &gl->hw_render_texture);
   glBindTexture(GL_TEXTURE_2D, gl->hw_render_texture);
   glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->hw_render_texture, 0);

   gl->hw_render_rb_ds = 0;
   if (hwr->bottom_left_origin)
      gl->flags |=  GL3_FLAG_HW_RENDER_BOTTOM_LEFT;
   else
      gl->flags &= ~GL3_FLAG_HW_RENDER_BOTTOM_LEFT;

   if (hwr->depth)
   {
      glGenRenderbuffers(1, &gl->hw_render_rb_ds);
      glBindRenderbuffer(GL_RENDERBUFFER, gl->hw_render_rb_ds);
      glRenderbufferStorage(GL_RENDERBUFFER, hwr->stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT16,
                            width, height);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);

      if (hwr->stencil)
         glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gl->hw_render_rb_ds);
      else
         glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl->hw_render_rb_ds);
   }

   status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   if (status != GL_FRAMEBUFFER_COMPLETE)
   {
      RARCH_ERR("[GLCore] Framebuffer is not complete.\n");
      if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
         gl->ctx_driver->bind_hw_render(gl->ctx_data, false);
      return false;
   }

   if (hwr->depth && hwr->stencil)
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
   else if (hwr->depth)
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   else
      glClear(GL_COLOR_BUFFER_BIT);

   gl->flags               |= GL3_FLAG_HW_RENDER_ENABLE;
   gl->hw_render_max_width  = width;
   gl->hw_render_max_height = height;
   glBindTexture(GL_TEXTURE_2D, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   return true;
}

static const gfx_ctx_driver_t *gl3_get_context(gl3_t *gl)
{
   unsigned major;
   unsigned minor;
   enum gfx_ctx_api api;
   gfx_ctx_flags_t flags;
   const gfx_ctx_driver_t *gfx_ctx      = NULL;
   void                      *ctx_data  = NULL;
   settings_t                 *settings = config_get_ptr();
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();

#ifdef HAVE_OPENGLES3
   api = GFX_CTX_OPENGL_ES_API;
   major = 3;
   minor = 0;
   if (hwr && hwr->context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION)
   {
      major = hwr->version_major;
      minor = hwr->version_minor;
   }
#else
   api   = GFX_CTX_OPENGL_API;
   if (hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE)
   {
      major = hwr->version_major;
      minor = hwr->version_minor;
      gl_query_core_context_set(hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE);
      if (hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE)
      {
         flags.flags = 0;
         BIT32_SET(flags.flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);
         video_context_driver_set_flags(&flags);
      }
   }
   else
   {
      major = 3;
      minor = 2;
      gl_query_core_context_set(true);
      flags.flags = 0;
      BIT32_SET(flags.flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);
      video_context_driver_set_flags(&flags);
   }
#endif

   /* Force shared context. */
   if (hwr)
   {
      if (hwr->context_type != RETRO_HW_CONTEXT_NONE)
         gl->flags |=  GL3_FLAG_USE_SHARED_CONTEXT;
      else
         gl->flags &= ~GL3_FLAG_USE_SHARED_CONTEXT;
   }

   gfx_ctx = video_context_driver_init_first(gl,
         settings->arrays.video_context_driver,
         api, major, minor,
         (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT) ? true : false,
         &ctx_data);

   if (ctx_data)
      gl->ctx_data = ctx_data;

   /* Need to force here since video_context_driver_init also checks for global option. */
   if (gfx_ctx->bind_hw_render)
      gfx_ctx->bind_hw_render(ctx_data, (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT) ? true : false);
   return gfx_ctx;
}

static bool gl3_recreate_fbo(
      struct video_fbo_rect *fbo_rect,
      GLuint fbo,
      GLuint* texture)
{
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glDeleteTextures(1, texture);
   glGenTextures(1, texture);
   glBindTexture(GL_TEXTURE_2D, *texture);
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGBA8,
         fbo_rect->width,
         fbo_rect->height,
         0, GL_RGBA,
         GL_UNSIGNED_BYTE, NULL);

   glFramebufferTexture2D(GL_FRAMEBUFFER,
         GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
         *texture, 0);

   if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
         == GL_FRAMEBUFFER_COMPLETE)
      return true;

   RARCH_WARN("[GLCore] Failed to reinitialize FBO texture.\n");
   return false;
}

static void gl3_set_projection(gl3_t *gl,
      const struct video_ortho *ortho, bool allow_rotate)
{
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   float radians, cosine, sine;

   /* Calculate projection. */
   matrix_4x4_ortho(gl->mvp_no_rot, ortho->left, ortho->right,
                    ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      gl->mvp = gl->mvp_no_rot;
      return;
   }

   radians                 = M_PI * gl->rotation / 180.0f;
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(gl->mvp, rot, gl->mvp_no_rot);

   memcpy(gl->mvp_no_rot_yflip.data, gl->mvp_no_rot.data, sizeof(gl->mvp_no_rot.data));
   MAT_ELEM_4X4(gl->mvp_no_rot_yflip, 1, 0) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_no_rot_yflip, 1, 1) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_no_rot_yflip, 1, 2) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_no_rot_yflip, 1, 3) *= -1.0f;

   memcpy(gl->mvp_yflip.data, gl->mvp.data, sizeof(gl->mvp.data));
   MAT_ELEM_4X4(gl->mvp_yflip, 1, 0) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_yflip, 1, 1) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_yflip, 1, 2) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_yflip, 1, 3) *= -1.0f;
}

static void gl3_set_viewport(gl3_t *gl,
      unsigned vp_width, unsigned vp_height,
      bool force_full, bool allow_rotate)
{
   settings_t *settings            = config_get_ptr();
   float device_aspect             = (float)vp_width / vp_height;
   bool video_scale_integer        = settings->bools.video_scale_integer;

   if (gl->ctx_driver->translate_aspect)
      device_aspect         = gl->ctx_driver->translate_aspect(
            gl->ctx_data, vp_width, vp_height);

   if (video_scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&gl->vp,
            vp_width, vp_height,
            video_driver_get_aspect_ratio(),
            (gl->flags & GL3_FLAG_KEEP_ASPECT) ? true : false,
            false);
      vp_width  = gl->vp.width;
      vp_height = gl->vp.height;
   }
   else if ((gl->flags & GL3_FLAG_KEEP_ASPECT) && !force_full)
   {
      gl->vp.full_height = gl->video_height;
      video_viewport_get_scaled_aspect2(&gl->vp, vp_width, vp_height, false, device_aspect, video_driver_get_aspect_ratio());
      vp_width           = gl->vp.width;
      vp_height          = gl->vp.height;
   }
   else
   {
      gl->vp.x           = gl->vp.y = 0;
      gl->vp.width       = vp_width;
      gl->vp.height      = vp_height;
   }

   glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   gl3_set_projection(gl, &gl3_default_ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      gl->out_vp_width  = vp_width;
      gl->out_vp_height = vp_height;
   }

   gl->filter_chain_vp.x = gl->vp.x;
   gl->filter_chain_vp.y = gl->vp.y;
   gl->filter_chain_vp.width = gl->vp.width;
   gl->filter_chain_vp.height = gl->vp.height;
}

#ifdef HAVE_SLANG
static bool gl3_init_pipelines(gl3_t *gl)
{
   static const uint32_t alpha_blend_vert[] =
#include "vulkan_shaders/alpha_blend.vert.inc"
      ;

   static const uint32_t alpha_blend_frag[] =
#include "vulkan_shaders/alpha_blend.frag.inc"
      ;

#ifdef HAVE_SHADERPIPELINE
   static const uint32_t font_frag[] =
#include "vulkan_shaders/font.frag.inc"
      ;

   static const uint32_t pipeline_ribbon_vert[] =
#include "vulkan_shaders/pipeline_ribbon.vert.inc"
      ;

   static const uint32_t pipeline_ribbon_frag[] =
#include "vulkan_shaders/pipeline_ribbon.frag.inc"
      ;

   static const uint32_t pipeline_ribbon_simple_vert[] =
#include "vulkan_shaders/pipeline_ribbon_simple.vert.inc"
      ;

   static const uint32_t pipeline_ribbon_simple_frag[] =
#include "vulkan_shaders/pipeline_ribbon_simple.frag.inc"
      ;

   static const uint32_t pipeline_snow_simple_frag[] =
#include "vulkan_shaders/pipeline_snow_simple.frag.inc"
      ;

   static const uint32_t pipeline_snow_frag[] =
#include "vulkan_shaders/pipeline_snow.frag.inc"
      ;

   static const uint32_t pipeline_bokeh_frag[] =
#include "vulkan_shaders/pipeline_bokeh.frag.inc"
      ;
#endif /* HAVE_SHADERPIPELINE */

   if (!gl->pipelines.alpha_blend)
      gl->pipelines.alpha_blend = gl3_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                             alpha_blend_frag, sizeof(alpha_blend_frag),
                                                             &gl->pipelines.alpha_blend_loc, true);
   if (!gl->pipelines.alpha_blend)
      return false;

#ifdef HAVE_SHADERPIPELINE
   if (!gl->pipelines.font)
      gl->pipelines.font = gl3_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                      font_frag, sizeof(font_frag),
                                                      &gl->pipelines.font_loc, true);
   if (!gl->pipelines.font)
      return false;

   if (!gl->pipelines.ribbon_simple)
      gl->pipelines.ribbon_simple = gl3_cross_compile_program(pipeline_ribbon_simple_vert, sizeof(pipeline_ribbon_simple_vert),
                                                               pipeline_ribbon_simple_frag, sizeof(pipeline_ribbon_simple_frag),
                                                               &gl->pipelines.ribbon_simple_loc, true);
   if (!gl->pipelines.ribbon_simple)
      return false;

   if (!gl->pipelines.ribbon)
      gl->pipelines.ribbon = gl3_cross_compile_program(pipeline_ribbon_vert, sizeof(pipeline_ribbon_vert),
                                                        pipeline_ribbon_frag, sizeof(pipeline_ribbon_frag),
                                                        &gl->pipelines.ribbon_loc, true);
   if (!gl->pipelines.ribbon)
      return false;

   if (!gl->pipelines.bokeh)
      gl->pipelines.bokeh = gl3_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                       pipeline_bokeh_frag, sizeof(pipeline_bokeh_frag),
                                                       &gl->pipelines.bokeh_loc, true);
   if (!gl->pipelines.bokeh)
      return false;

   if (!gl->pipelines.snow_simple)
      gl->pipelines.snow_simple = gl3_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                             pipeline_snow_simple_frag, sizeof(pipeline_snow_simple_frag),
                                                             &gl->pipelines.snow_simple_loc, true);
   if (!gl->pipelines.snow_simple)
      return false;

   if (!gl->pipelines.snow)
      gl->pipelines.snow = gl3_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                      pipeline_snow_frag, sizeof(pipeline_snow_frag),
                                                      &gl->pipelines.snow_loc, true);
   if (!gl->pipelines.snow)
      return false;
#endif /* HAVE_SHADERPIPELINE */

   return true;
}
#endif /* HAVE_SLANG */

/**
 * gl3_get_fallback_shader_type:
 * @type                      : shader type which should be used if possible
 *
 * Returns a supported fallback shader type in case the given one is not supported.
 * For gl3, shader support is completely defined by the context driver shader flags.
 *
 * gl3_get_fallback_shader_type(RARCH_SHADER_NONE) returns a default shader type.
 * if gl3_get_fallback_shader_type(type) != type, type was not supported.
 *
 * Returns: A supported shader type.
 *  If RARCH_SHADER_NONE is returned, no shader backend is supported.
 **/
static enum rarch_shader_type gl3_get_fallback_shader_type(enum rarch_shader_type type)
{
#if defined(HAVE_SLANG) || defined(HAVE_GLSL) || defined(HAVE_CG)
   int i;
   gfx_ctx_flags_t flags;
   flags.flags     = 0;
   video_context_driver_get_flags(&flags);

   if (type != RARCH_SHADER_CG && type != RARCH_SHADER_GLSL && type != RARCH_SHADER_SLANG)
   {
      type = DEFAULT_SHADER_TYPE;

      if (type != RARCH_SHADER_CG && type != RARCH_SHADER_GLSL && type != RARCH_SHADER_SLANG)
         type = RARCH_SHADER_SLANG;
   }

   for (i = 0; i < 3; i++)
   {
      switch (type)
      {
         case RARCH_SHADER_CG:
#ifdef HAVE_CG
            if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_CG))
               return type;
#endif
            type = RARCH_SHADER_SLANG;
            break;

         case RARCH_SHADER_GLSL:
#ifdef HAVE_GLSL
            if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_GLSL))
               return type;
#endif
            type = RARCH_SHADER_CG;
            break;

         case RARCH_SHADER_SLANG:
#ifdef HAVE_SLANG
            if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_SLANG))
               return type;
#endif
            type = RARCH_SHADER_GLSL;
            break;

         default:
            return RARCH_SHADER_NONE;
      }
   }
#endif
   return RARCH_SHADER_NONE;
}

#ifdef HAVE_SLANG
static bool gl3_init_default_filter_chain(gl3_t *gl)
{
   if (!gl->ctx_driver)
      return false;

   if (gl->filter_chain_default)
      return true;

   gl->filter_chain_default = gl3_filter_chain_create_default(
         gl->video_info.smooth
         ? GLSLANG_FILTER_CHAIN_LINEAR
         : GLSLANG_FILTER_CHAIN_NEAREST);

   if (!gl->filter_chain_default)
   {
      RARCH_ERR("[GLCore] Failed to create default filter chain.\n");
      return false;
   }

   return true;
}

static bool gl3_init_filter_chain_preset(gl3_t *gl, const char *shader_path)
{
   if (!gl->ctx_driver)
      return false;

   gl->filter_chain = gl3_filter_chain_create_from_preset(
         shader_path,
         gl->video_info.smooth
         ? GLSLANG_FILTER_CHAIN_LINEAR
         : GLSLANG_FILTER_CHAIN_NEAREST);

   if (!gl->filter_chain)
   {
      RARCH_ERR("[GLCore] Failed to create preset: \"%s\".\n", shader_path);
      return false;
   }

   return true;
}
#endif /* HAVE_SLANG */

static const shader_backend_t *gl3_shader_driver_set_backend(
      enum rarch_shader_type type)
{
   enum rarch_shader_type fallback = gl3_get_fallback_shader_type(type);
   if (fallback != type)
      RARCH_ERR("[GLCore] Shader backend %d not supported, falling back to %d.\n", type, fallback);

   switch (fallback)
   {
#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         RARCH_LOG("[GLCore] Using Cg shader backend.\n");
         return &gl_cg_backend;
#endif
#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         RARCH_LOG("[GLCore] Using GLSL shader backend.\n");
         return &gl_glsl_backend;
#endif
      default:
         RARCH_LOG("[GLCore] No supported shader backend.\n");
         return NULL;
   }
}

static bool gl3_shader_driver_init(gl3_video_shader_ctx_init_t *init)
{
   void            *tmp = NULL;
   settings_t *settings = config_get_ptr();

   if (!init->shader || !init->shader->init)
   {
      init->shader = gl3_shader_driver_set_backend(init->shader_type);

      if (!init->shader)
         return false;
   }

   tmp = init->shader->init(init->data, init->path);

   if (!tmp)
      return false;

   if (string_is_equal(settings->arrays.menu_driver, "xmb")
         && init->shader->init_menu_shaders)
   {
      RARCH_LOG("[GLCore] Setting up menu pipeline shaders for XMB...\n");
      init->shader->init_menu_shaders(tmp);
   }

   init->shader_data = tmp;

   return true;
}

static void gl3_renderchain_recompute_pass_sizes(
      gl3_t *gl,
      unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
   size_t i;
   bool size_modified       = false;
   GLint max_size           = 0;
   unsigned last_width      = width;
   unsigned last_height     = height;
   unsigned last_max_width  = RARCH_SCALE_BASE * gl->video_info.input_scale;
   unsigned last_max_height = RARCH_SCALE_BASE * gl->video_info.input_scale;

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

   /* Calculate viewports for FBOs. */
   for (i = 0; i < gl->chain.num_fbo_passes; i++)
   {
      struct video_fbo_rect *fbo_rect    = &gl->chain.fbo_rect[i];
      struct gfx_fbo_scale *fbo_scale    = &gl->chain.fbo_scale[i];

      switch (fbo_scale->type_x)
      {
         case RARCH_SCALE_INPUT:
            fbo_rect->img_width      = fbo_scale->scale_x * last_width;
            fbo_rect->max_img_width  = last_max_width     * fbo_scale->scale_x;
            break;

         case RARCH_SCALE_ABSOLUTE:
            fbo_rect->img_width      = fbo_rect->max_img_width =
               fbo_scale->abs_x;
            break;

         case RARCH_SCALE_VIEWPORT:
            if (gl->rotation % 180 == 90)
               fbo_rect->img_width = fbo_rect->max_img_width =
               fbo_scale->scale_x * vp_height;
            else
               fbo_rect->img_width = fbo_rect->max_img_width =
               fbo_scale->scale_x * vp_width;
            break;
      }

      switch (fbo_scale->type_y)
      {
         case RARCH_SCALE_INPUT:
            fbo_rect->img_height     = last_height * fbo_scale->scale_y;
            fbo_rect->max_img_height = last_max_height * fbo_scale->scale_y;
            break;

         case RARCH_SCALE_ABSOLUTE:
            fbo_rect->img_height     = fbo_scale->abs_y;
            fbo_rect->max_img_height = fbo_scale->abs_y;
            break;

         case RARCH_SCALE_VIEWPORT:
            if (gl->rotation % 180 == 90)
               fbo_rect->img_height = fbo_rect->max_img_height =
               fbo_scale->scale_y * vp_width;
            else
               fbo_rect->img_height = fbo_rect->max_img_height =
                  fbo_scale->scale_y * vp_height;
            break;
      }

      if (fbo_rect->img_width > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->img_width      = max_size;
      }

      if (fbo_rect->img_height > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->img_height     = max_size;
      }

      if (fbo_rect->max_img_width > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->max_img_width  = max_size;
      }

      if (fbo_rect->max_img_height > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->max_img_height = max_size;
      }

      if (size_modified)
         RARCH_WARN("[GLCore] FBO textures exceeded maximum size of GPU (%dx%d). Resizing to fit.\n", max_size, max_size);

      last_width      = fbo_rect->img_width;
      last_height     = fbo_rect->img_height;
      last_max_width  = fbo_rect->max_img_width;
      last_max_height = fbo_rect->max_img_height;
   }
}

static void gl3_create_fbo_texture(gl3_t *gl,
      unsigned pass, GLuint texture)
{
   GLint mag_filter;
   GLint min_filter;
   bool smooth;

   if (!gl->chain.shader->filter_type(gl->chain.shader_data, pass + 2, &smooth))
      smooth = gl->video_info.smooth;

   mag_filter = smooth ? GL_LINEAR : GL_NEAREST;
   min_filter = gl->chain.shader->mipmap_input(gl->chain.shader_data, pass + 2)
      ? (smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
      : mag_filter;

   glBindTexture(GL_TEXTURE_2D, texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, gl->chain.fbo_rect[pass].width, gl->chain.fbo_rect[pass].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glBindTexture(GL_TEXTURE_2D, 0);
}

static bool gl3_create_fbo_targets(gl3_t *gl)
{
   unsigned i;
   struct gfx_fbo_scale scale, scale_last;

   gl->chain.active               = true;
   gl->chain.mipmap_active        = gl->chain.shader->mipmap_input(gl->chain.shader_data, 1);
   gl->chain.fbo_feedback         = 0;
   gl->chain.fbo_feedback_pass    = 0;
   gl->chain.fbo_feedback_texture = 0;
   gl->chain.num_fbo_passes       = gl->chain.shader->num_shaders(gl->chain.shader_data);
   gl->chain.num_prev_textures    = gl->chain.shader->get_prev_textures(gl->chain.shader_data);
   if (gl->chain.num_prev_textures < 1)
      gl->chain.num_prev_textures = 1;

   memcpy(gl->chain.tex_info.coord, gl3_vertexes, sizeof(gl->chain.tex_info.coord));
   gl->chain.coords.vertex        = gl->chain.vertex_ptr;
   gl->chain.coords.tex_coord     = gl->chain.tex_info.coord;
   gl->chain.coords.color         = gl3_colors;
   gl->chain.coords.lut_tex_coord = gl3_vertexes;
   gl->chain.coords.vertices      = 4;

   for (i = 0; i < gl->chain.num_prev_textures; i++)
   {
      gl->chain.prev_info[i].tex           = gl->textures[0].tex;
      gl->chain.prev_info[i].input_size[0] = RARCH_SCALE_BASE * gl->video_info.input_scale;
      gl->chain.prev_info[i].input_size[1] = RARCH_SCALE_BASE * gl->video_info.input_scale;
      gl->chain.prev_info[i].tex_size[0]   = RARCH_SCALE_BASE * gl->video_info.input_scale;
      gl->chain.prev_info[i].tex_size[1]   = RARCH_SCALE_BASE * gl->video_info.input_scale;
      memcpy(gl->chain.prev_info[i].coord, gl3_vertexes, sizeof(gl->chain.prev_info[i].coord));
   }

   if (gl->flags & GL3_FLAG_HW_RENDER_ENABLE)
   {
      bool force_smooth;
      bool smooth = gl->chain.shader->filter_type(gl->chain.shader_data, 1, &force_smooth)
         ? force_smooth
         : gl->video_info.smooth;

      GLint mag_filter = smooth ? GL_LINEAR : GL_NEAREST;
      GLint min_filter = gl->chain.mipmap_active
         ? (smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
         : mag_filter;

      glBindTexture(GL_TEXTURE_2D, gl->hw_render_texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
   }

   if (gl->chain.num_fbo_passes == 0)
      return true;

   scale_last.flags = 0;
   gl->chain.shader->shader_scale(gl->chain.shader_data, gl->chain.num_fbo_passes, &scale_last);

   if (!(scale_last.flags & FBO_SCALE_FLAG_VALID) && --gl->chain.num_fbo_passes == 0)
      return true;

   scale.flags = 0;
   gl->chain.shader->shader_scale(gl->chain.shader_data, 1, &scale);

   if (!(scale.flags & FBO_SCALE_FLAG_VALID))
   {
      scale.scale_x    = 1.0f;
      scale.scale_y    = 1.0f;
      scale.type_x     = RARCH_SCALE_INPUT;
      scale.type_y     = RARCH_SCALE_INPUT;
      scale.flags     |= FBO_SCALE_FLAG_VALID;
   }

   gl->chain.fbo_scale[0] = scale;

   for (i = 1; i < gl->chain.num_fbo_passes; i++)
   {
      gl->chain.shader->shader_scale(gl->chain.shader_data, i + 1, &gl->chain.fbo_scale[i]);

      if (!(gl->chain.fbo_scale[i].flags & FBO_SCALE_FLAG_VALID))
      {
         gl->chain.fbo_scale[i].scale_x = gl->chain.fbo_scale[i].scale_y = 1.0f;
         gl->chain.fbo_scale[i].type_x  = gl->chain.fbo_scale[i].type_y  =
            RARCH_SCALE_INPUT;
         gl->chain.fbo_scale[i].flags  |= FBO_SCALE_FLAG_VALID;
      }
   }

   gl3_renderchain_recompute_pass_sizes(
         gl,
         RARCH_SCALE_BASE * gl->video_info.input_scale,
         RARCH_SCALE_BASE * gl->video_info.input_scale,
         gl->video_width,
         gl->video_height);

   glGenFramebuffers(gl->chain.num_fbo_passes, gl->chain.fbo);
   glGenTextures(gl->chain.num_fbo_passes, gl->chain.fbo_texture);

   for (i = 0; i < gl->chain.num_fbo_passes; i++)
   {
      gl->chain.fbo_rect[i].width  = next_pow2(gl->chain.fbo_rect[i].img_width);
      gl->chain.fbo_rect[i].height = next_pow2(gl->chain.fbo_rect[i].img_height);
      RARCH_LOG("[GLCore] Creating FBO %d @ %ux%u.\n", i,
            gl->chain.fbo_rect[i].width, gl->chain.fbo_rect[i].height);

      if (gl->chain.fbo[i] == 0 || gl->chain.fbo_texture[i] == 0)
         goto error;

      gl3_create_fbo_texture(gl, i, gl->chain.fbo_texture[i]);

      glBindFramebuffer(GL_FRAMEBUFFER, gl->chain.fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->chain.fbo_texture[i], 0);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   if (gl->chain.shader->get_feedback_pass(gl->chain.shader_data, &gl->chain.fbo_feedback_pass))
   {
      if (gl->chain.fbo_feedback_pass < gl->chain.num_fbo_passes)
      {
         RARCH_LOG("[GLCore] Creating feedback FBO %d @ %ux%u.\n", i,
               gl->chain.fbo_rect[gl->chain.fbo_feedback_pass].width,
               gl->chain.fbo_rect[gl->chain.fbo_feedback_pass].height);

         glGenFramebuffers(1, &gl->chain.fbo_feedback);
         if (!gl->chain.fbo_feedback)
            goto error;

         glGenTextures(1, &gl->chain.fbo_feedback_texture);
         if (!gl->chain.fbo_feedback_texture)
            goto error;

         gl3_create_fbo_texture(gl, gl->chain.fbo_feedback_pass, gl->chain.fbo_feedback_texture);

         glBindFramebuffer(GL_FRAMEBUFFER, gl->chain.fbo_feedback);
         glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->chain.fbo_feedback_texture, 0);
         if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            goto error;
      }
      else
         RARCH_WARN("[GLCore] Tried to create feedback FBO of pass #%u, but there are only %d FBO passes. Will use input texture as feedback texture.\n",
               gl->chain.fbo_feedback_pass, gl->chain.num_fbo_passes);
   }

   return true;

error:
   RARCH_ERR("[GLCore] Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
   if (gl->chain.fbo_feedback_texture)
      glDeleteTextures(1, &gl->chain.fbo_feedback_texture);
   if (gl->chain.fbo_feedback)
      glDeleteFramebuffers(1, &gl->chain.fbo_feedback);
   glDeleteTextures(gl->chain.num_fbo_passes, gl->chain.fbo_texture);
   glDeleteFramebuffers(gl->chain.num_fbo_passes, gl->chain.fbo);
   gl->chain.num_fbo_passes = 0;
   return false;
}

static bool gl3_init_filter_chain_with_path(gl3_t *gl, const char *shader_path)
{
   enum rarch_shader_type parse_type = video_shader_parse_type(shader_path);
   enum rarch_shader_type type       = gl3_get_fallback_shader_type(parse_type);

   gl->filter_chain            = NULL;
   gl->chain.shader            = NULL;
   gl->chain.shader_data       = NULL;
   gl->chain.num_fbo_passes    = 0;
   gl->chain.num_prev_textures = 0;
#ifdef HAVE_SLANG
   gl->chain.active = false;
#else
   gl->chain.active = true;
#endif
   gl->chain.mipmap_active        = false;
   gl->chain.fbo_feedback_texture = 0;

   if (type == RARCH_SHADER_NONE)
   {
      RARCH_ERR("[GLCore] Couldn't find any supported shader backend! Continuing without shaders.\n");
      return true;
   }

   if (type != parse_type)
   {
      if (!string_is_empty(shader_path))
         RARCH_WARN("[GLCore] Shader preset %s is using unsupported shader type %s, falling back to stock %s.\n",
            shader_path, video_shader_type_to_str(parse_type), video_shader_type_to_str(type));

      shader_path = NULL;
   }

#ifdef HAVE_GLSL
   if (type == RARCH_SHADER_GLSL)
      gl_glsl_set_context_type(true, gl->version_major, gl->version_minor);
#endif

#ifdef HAVE_SLANG
   if (type == RARCH_SHADER_SLANG)
   {
      RARCH_LOG("[GLCore] Using Slang shader backend.\n");

      gl->chain.active = false;

      if (!gl3_init_pipelines(gl))
      {
         RARCH_ERR("[GLCore] Failed to cross-compile menu pipelines.\n");
         return false;
      }

      if (string_is_empty(shader_path))
      {
         RARCH_LOG("[GLCore] Loading stock shader.\n");
         return gl3_init_default_filter_chain(gl);
      }
      else if (!gl3_init_filter_chain_preset(gl, shader_path))
         return gl3_init_default_filter_chain(gl);
      else
         return true;
   }
   else
#endif
   {
      gl3_video_shader_ctx_init_t init_data;
      bool ret = false;

      gl->chain.active = true;

      init_data.shader_type             = type;
      init_data.shader                  = NULL;
      init_data.shader_data             = NULL;
      init_data.data                    = gl;
      init_data.path                    = shader_path;

      if (gl3_shader_driver_init(&init_data))
      {
         gl->chain.shader               = init_data.shader;
         gl->chain.shader_data          = init_data.shader_data;

         return gl3_create_fbo_targets(gl);
      }

      RARCH_ERR("[GLCore] Failed to initialize shader, falling back to stock.\n");

      init_data.shader                  = NULL;
      init_data.shader_data             = NULL;
      init_data.path                    = NULL;

      ret                               = gl3_shader_driver_init(&init_data);

      gl->chain.shader                  = init_data.shader;
      gl->chain.shader_data             = init_data.shader_data;

      return ret && gl3_create_fbo_targets(gl);
   }
}

static bool gl3_init_filter_chain(gl3_t *gl)
{
   return gl3_init_filter_chain_with_path(gl, video_shader_get_current_shader_preset());
}

#ifdef GL_DEBUG
#ifdef HAVE_OPENGLES3
#define DEBUG_CALLBACK_TYPE GL_APIENTRY
#define GL_DEBUG_SOURCE_API GL_DEBUG_SOURCE_API_KHR
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR
#define GL_DEBUG_SOURCE_SHADER_COMPILER GL_DEBUG_SOURCE_SHADER_COMPILER_KHR
#define GL_DEBUG_SOURCE_THIRD_PARTY GL_DEBUG_SOURCE_THIRD_PARTY_KHR
#define GL_DEBUG_SOURCE_APPLICATION GL_DEBUG_SOURCE_APPLICATION_KHR
#define GL_DEBUG_SOURCE_OTHER GL_DEBUG_SOURCE_OTHER_KHR
#define GL_DEBUG_TYPE_ERROR GL_DEBUG_TYPE_ERROR_KHR
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR
#define GL_DEBUG_TYPE_PORTABILITY GL_DEBUG_TYPE_PORTABILITY_KHR
#define GL_DEBUG_TYPE_PERFORMANCE GL_DEBUG_TYPE_PERFORMANCE_KHR
#define GL_DEBUG_TYPE_MARKER GL_DEBUG_TYPE_MARKER_KHR
#define GL_DEBUG_TYPE_PUSH_GROUP GL_DEBUG_TYPE_PUSH_GROUP_KHR
#define GL_DEBUG_TYPE_POP_GROUP GL_DEBUG_TYPE_POP_GROUP_KHR
#define GL_DEBUG_TYPE_OTHER GL_DEBUG_TYPE_OTHER_KHR
#define GL_DEBUG_SEVERITY_HIGH GL_DEBUG_SEVERITY_HIGH_KHR
#define GL_DEBUG_SEVERITY_MEDIUM GL_DEBUG_SEVERITY_MEDIUM_KHR
#define GL_DEBUG_SEVERITY_LOW GL_DEBUG_SEVERITY_LOW_KHR
#else
#define DEBUG_CALLBACK_TYPE APIENTRY
#endif
static void DEBUG_CALLBACK_TYPE gl3_debug_cb(GLenum source, GLenum type,
      GLuint id, GLenum severity, GLsizei length,
      const GLchar *message, void *userParam)
{
   const char      *src = NULL;
   const char *typestr  = NULL;
   gl3_t *gl = (gl3_t*)userParam; /* Useful for debugger. */

   (void)gl;
   (void)id;
   (void)length;

   switch (source)
   {
      case GL_DEBUG_SOURCE_API:
         src = "API";
         break;
      case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
         src = "Window system";
         break;
      case GL_DEBUG_SOURCE_SHADER_COMPILER:
         src = "Shader compiler";
         break;
      case GL_DEBUG_SOURCE_THIRD_PARTY:
         src = "3rd party";
         break;
      case GL_DEBUG_SOURCE_APPLICATION:
         src = "Application";
         break;
      case GL_DEBUG_SOURCE_OTHER:
         src = "Other";
         break;
      default:
         src = "Unknown";
         break;
   }

   switch (type)
   {
      case GL_DEBUG_TYPE_ERROR:
         typestr = "Error";
         break;
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
         typestr = "Deprecated behavior";
         break;
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
         typestr = "Undefined behavior";
         break;
      case GL_DEBUG_TYPE_PORTABILITY:
         typestr = "Portability";
         break;
      case GL_DEBUG_TYPE_PERFORMANCE:
         typestr = "Performance";
         break;
      case GL_DEBUG_TYPE_MARKER:
         typestr = "Marker";
         break;
      case GL_DEBUG_TYPE_PUSH_GROUP:
         typestr = "Push group";
         break;
      case GL_DEBUG_TYPE_POP_GROUP:
        typestr = "Pop group";
        break;
      case GL_DEBUG_TYPE_OTHER:
        typestr = "Other";
        break;
      default:
        typestr = "Unknown";
        break;
   }

   switch (severity)
   {
      case GL_DEBUG_SEVERITY_HIGH:
         RARCH_ERR("[GL debug (High, %s, %s)] %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_MEDIUM:
         RARCH_WARN("[GL debug (Medium, %s, %s)] %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_LOW:
         RARCH_LOG("[GL debug (Low, %s, %s)] %s\n", src, typestr, message);
         break;
   }
}

static void gl3_begin_debug(gl3_t *gl)
{
   if (gl_check_capability(GL_CAPS_DEBUG))
   {
#ifdef HAVE_OPENGLES3
      glDebugMessageCallbackKHR(gl3_debug_cb, gl);
      glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
      glEnable(GL_DEBUG_OUTPUT_KHR);
#else
      glDebugMessageCallback(gl3_debug_cb, gl);
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
      glEnable(GL_DEBUG_OUTPUT);
#endif
   }
   else
      RARCH_ERR("[GLCore] Neither GL_KHR_debug nor GL_ARB_debug_output are implemented. Cannot start GL debugging.\n");
}
#endif

static void gl3_set_viewport_wrapper(void *data,
      unsigned vp_width, unsigned vp_height,
      bool force_full, bool allow_rotate)
{
   gl3_t *gl = (gl3_t*)data;
   gl3_set_viewport(gl, vp_width, vp_height,
         force_full, allow_rotate);
}


static void *gl3_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned full_x, full_y;
   settings_t *settings                 = config_get_ptr();
   bool video_gpu_record                = settings->bools.video_gpu_record;
   bool force_fullscreen                = false;
   int interval                         = 0;
   unsigned mode_width                  = 0;
   unsigned mode_height                 = 0;
   unsigned win_width                   = 0;
   unsigned win_height                  = 0;
   unsigned temp_width                  = 0;
   unsigned temp_height                 = 0;
   const char *vendor                   = NULL;
   const char *renderer                 = NULL;
   const char *version                  = NULL;
   char *err_string                     = NULL;
   gl3_t *gl                            = (gl3_t*)calloc(1, sizeof(gl3_t));
   const gfx_ctx_driver_t *ctx_driver   = gl3_get_context(gl);
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   unsigned i;

   if (!gl || !ctx_driver)
      goto error;

   video_context_driver_set(ctx_driver);

   gl->ctx_driver = ctx_driver;
   gl->video_info = *video;

   RARCH_LOG("[GLCore] Found GL context: \"%s\".\n", ctx_driver->ident);

   if (gl->ctx_driver->get_video_size)
      gl->ctx_driver->get_video_size(gl->ctx_data,
               &mode_width, &mode_height);

   if (!video->fullscreen && !gl->ctx_driver->has_windowed)
   {
      RARCH_DBG("[GLCore] Config requires windowed mode, but context driver does not support it. "
                "Forcing fullscreen for this session.\n");
      force_fullscreen = true;
   }

   full_x      = mode_width;
   full_y      = mode_height;
   mode_width  = 0;
   mode_height = 0;
   interval    = 0;

   RARCH_LOG("[GLCore] Detecting screen resolution: %ux%u.\n", full_x, full_y);

   if (video->vsync)
      interval = video->swap_interval;

   if (gl->ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled            = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      gl->ctx_driver->swap_interval(gl->ctx_data, interval);
   }

   win_width   = video->width;
   win_height  = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }
   /* If fullscreen had to be forced, video->width/height is incorrect */
   else if (force_fullscreen)
   {
      win_width  = settings->uints.video_fullscreen_x;
      win_height = settings->uints.video_fullscreen_y;
   }

   if (     !gl->ctx_driver->set_video_mode
         || !gl->ctx_driver->set_video_mode(gl->ctx_data,
            win_width, win_height, (video->fullscreen || force_fullscreen)))
      goto error;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   rglgen_resolve_symbols(ctx_driver->get_proc_address);

   if (hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE)
      gl3_init_hw_render(gl, RARCH_SCALE_BASE * video->input_scale, RARCH_SCALE_BASE * video->input_scale);

#ifdef GL_DEBUG
   gl3_begin_debug(gl);
   if (gl->flags & GL3_FLAG_HW_RENDER_ENABLE)
   {
      if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
         gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
      gl3_begin_debug(gl);
      if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
         gl->ctx_driver->bind_hw_render(gl->ctx_data, false);
   }
#endif

   /* Clear out potential error flags in case we use cached context. */
   glGetError();

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);

   RARCH_LOG("[GLCore] Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GLCore] Version: %s.\n", version);

   if (string_is_equal(ctx_driver->ident, "null"))
      goto error;

   if (!string_is_empty(version))
      sscanf(version, "%u.%u", &gl->version_major, &gl->version_minor);

   video_driver_set_gpu_api_version_string(version);

#ifdef _WIN32
   if (   string_is_equal(vendor,   "Microsoft Corporation"))
      if (string_is_equal(renderer, "GDI Generic"))
#ifdef HAVE_OPENGL1
         video_driver_force_fallback("gl1");
#else
         video_driver_force_fallback("gdi");
#endif
#endif

   if (video->vsync)
      gl->flags   |= GL3_FLAG_VSYNC;
   if (video->fullscreen || force_fullscreen)
      gl->flags   |= GL3_FLAG_FULLSCREEN;
   if (video->force_aspect)
      gl->flags   |= GL3_FLAG_KEEP_ASPECT;

   mode_width      = 0;
   mode_height     = 0;

   if (gl->ctx_driver->get_video_size)
      gl->ctx_driver->get_video_size(gl->ctx_data,
               &mode_width, &mode_height);

   temp_width     = mode_width;
   temp_height    = mode_height;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);
   video_driver_get_size(&temp_width, &temp_height);
   gl->video_width  = temp_width;
   gl->video_height = temp_height;

   RARCH_LOG("[GLCore] Using resolution %ux%u.\n", temp_width, temp_height);

   /* Set the viewport to fix recording, since it needs to know
    * the viewport sizes before we start running. */
   gl3_set_viewport_wrapper(gl, temp_width, temp_height, false, true);

   if (gl->ctx_driver->input_driver)
   {
      const char *joypad_name = settings->arrays.input_joypad_driver;
      gl->ctx_driver->input_driver(
            gl->ctx_data, joypad_name,
            input, input_data);
   }

   gl->chain.vertex_ptr = hwr && hwr->bottom_left_origin ? gl3_vertexes : gl3_vertexes_flipped;
   if (!gl3_init_filter_chain(gl))
   {
      RARCH_ERR("[GLCore] Failed to init filter chain.\n");
      goto error;
   }

   if (video->font_enable)
      font_driver_init_osd(gl,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_OPENGL_CORE_API);

   if (video_gpu_record
      && recording_state_get_ptr()->enable)
   {
      gl->flags |=  GL3_FLAG_PBO_READBACK_ENABLE;
      if (gl3_init_pbo_readback(gl))
      {
         RARCH_LOG("[GLCore] Async PBO readback enabled.\n");
      }
   }
   else
      gl->flags &= ~GL3_FLAG_PBO_READBACK_ENABLE;

   if (!gl_check_error(&err_string))
   {
      RARCH_ERR("[GLCore] %s\n", err_string);
      free(err_string);
      goto error;
   }

   glGenVertexArrays(1, &gl->vao);
   glBindVertexArray(gl->vao);
   glBindVertexArray(0);

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   return gl;

error:
   video_context_driver_free();
   gl3_destroy_resources(gl);
   if (gl)
      free(gl);
   return NULL;
}

static unsigned gl3_num_miplevels(unsigned width, unsigned height)
{
   unsigned levels = 1;
   if (width < height)
      width = height;
   while (width > 1)
   {
      levels++;
      width >>= 1;
   }
   return levels;
}

static void video_texture_load_gl3(
      const struct texture_image *ti,
      enum texture_filter_type filter_type,
      GLuint *idptr)
{
   /* Generate the OpenGL texture object */
   GLuint id;
   unsigned levels;
   GLenum mag_filter, min_filter;

   glGenTextures(1, &id);
   *idptr = id;
   glBindTexture(GL_TEXTURE_2D, id);

   levels = 1;
   if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR || filter_type == TEXTURE_FILTER_MIPMAP_NEAREST)
      levels = gl3_num_miplevels(ti->width, ti->height);

   glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA8, ti->width, ti->height);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   switch (filter_type)
   {
      case TEXTURE_FILTER_LINEAR:
         mag_filter = GL_LINEAR;
         min_filter = GL_LINEAR;
         break;

      case TEXTURE_FILTER_NEAREST:
         mag_filter = GL_NEAREST;
         min_filter = GL_NEAREST;
         break;

      case TEXTURE_FILTER_MIPMAP_NEAREST:
         mag_filter = GL_LINEAR;
         min_filter = GL_LINEAR_MIPMAP_NEAREST;
         break;

      case TEXTURE_FILTER_MIPMAP_LINEAR:
      default:
         mag_filter = GL_LINEAR;
         min_filter = GL_LINEAR_MIPMAP_LINEAR;
         break;
   }
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);

   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                   ti->width, ti->height, GL_RGBA, GL_UNSIGNED_BYTE, ti->pixels);

   if (levels > 1)
      glGenerateMipmap(GL_TEXTURE_2D);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
   glBindTexture(GL_TEXTURE_2D, 0);
}

#ifdef HAVE_OVERLAY
static bool gl3_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   size_t i;
   int j;
   GLuint id;
   gl3_t *gl = (gl3_t*)data;
   const struct texture_image *images =
      (const struct texture_image*)image_data;

   if (!gl)
      return false;

   gl3_free_overlay(gl);
   gl->overlay_tex = (GLuint*)
      calloc(num_images, sizeof(*gl->overlay_tex));

   if (!gl->overlay_tex)
      return false;

   gl->overlay_vertex_coord = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_tex_coord    = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_color_coord  = (GLfloat*)
      calloc(4 * 4 * num_images, sizeof(GLfloat));

   if (     !gl->overlay_vertex_coord
         || !gl->overlay_tex_coord
         || !gl->overlay_color_coord)
      return false;

   gl->overlays = num_images;
   glGenTextures(num_images, gl->overlay_tex);

   for (i = 0; i < num_images; i++)
   {
      video_texture_load_gl3(&images[i], TEXTURE_FILTER_LINEAR, &id);
      gl->overlay_tex[i] = id;

      /* Default. Stretch to whole screen. */
      gl3_overlay_tex_geom   (gl, (unsigned)i, 0, 0, 1, 1);
      gl3_overlay_vertex_geom(gl, (unsigned)i, 0, 0, 1, 1);

      for (j = 0; j < 16; j++)
         gl->overlay_color_coord[16 * i + j] = 1.0f;
   }

   return true;
}

static void gl3_overlay_enable(void *data, bool state)
{
   gl3_t *gl = (gl3_t*)data;

   if (!gl)
      return;

   if (state)
      gl->flags |=  GL3_FLAG_OVERLAY_ENABLE;
   else
      gl->flags &= ~GL3_FLAG_OVERLAY_ENABLE;

   if ((gl->flags & GL3_FLAG_FULLSCREEN) && gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl->ctx_data, state);
}

static void gl3_overlay_full_screen(void *data, bool enable)
{
   gl3_t *gl = (gl3_t*)data;
   if (gl)
   {
      if (enable)
         gl->flags |=  GL3_FLAG_OVERLAY_FULLSCREEN;
      else
         gl->flags &= ~GL3_FLAG_OVERLAY_FULLSCREEN;
   }
}

static void gl3_overlay_set_alpha(void *data, unsigned image, float mod)
{
   GLfloat *color = NULL;
   gl3_t *gl = (gl3_t*)data;
   if (!gl)
      return;

   color          = (GLfloat*)&gl->overlay_color_coord[image * 16];

   color[ 0 + 3]  = mod;
   color[ 4 + 3]  = mod;
   color[ 8 + 3]  = mod;
   color[12 + 3]  = mod;
}

static const video_overlay_interface_t gl3_overlay_interface = {
   gl3_overlay_enable,
   gl3_overlay_load,
   gl3_overlay_tex_geom,
   gl3_overlay_vertex_geom,
   gl3_overlay_full_screen,
   gl3_overlay_set_alpha,
};

static void gl3_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gl3_overlay_interface;
}
#endif

static void gl3_free(void *data)
{
   gl3_t *gl = (gl3_t*)data;
   if (!gl)
      return;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);
   font_driver_free_osd();
   gl3_destroy_resources(gl);
   if (gl->ctx_driver && gl->ctx_driver->destroy)
      gl->ctx_driver->destroy(gl->ctx_data);
   video_context_driver_free();
   free(gl);
}

static bool gl3_alive(void *data)
{
   bool ret             = false;
   bool quit            = false;
   bool resize          = false;
   gl3_t *gl        = (gl3_t*)data;
   unsigned temp_width  = gl->video_width;
   unsigned temp_height = gl->video_height;

   gl->ctx_driver->check_window(gl->ctx_data,
         &quit, &resize, &temp_width, &temp_height);

#ifdef __WINRT__
   if (is_running_on_xbox())
   {
      /* We can set it to 1920x1080 as xbox uwp windowsize is guaranteed
       * to be 1920x1080 and currently there is now way to set ANGLE to
       * use a variable resolution swapchain so regardless of the size
       * the window is always 1080p */
      temp_width  = 1920;
      temp_height = 1080;
   }
#endif

   if (quit)
      gl->flags        |= GL3_FLAG_QUITTING;
   else if (resize)
      gl->flags        |= GL3_FLAG_SHOULD_RESIZE;

   ret = !(gl->flags & GL3_FLAG_QUITTING);

   if (temp_width != 0 && temp_height != 0)
   {
      video_driver_set_size(temp_width, temp_height);
      gl->video_width  = temp_width;
      gl->video_height = temp_height;
   }

   return ret;
}

static void gl3_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   int interval            = 0;
   gl3_t         *gl       = (gl3_t*)data;

   if (!gl)
      return;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);
   if (!state)
      interval = swap_interval;

   if (gl->ctx_driver->swap_interval)
   {
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      gl->ctx_driver->swap_interval(gl->ctx_data, interval);
   }

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
}

static bool gl3_suppress_screensaver(void *data, bool enable)
{
   bool enabled                = enable;
   gl3_t         *gl       = (gl3_t*)data;
   if (gl->ctx_data && gl->ctx_driver->suppress_screensaver)
      return gl->ctx_driver->suppress_screensaver(gl->ctx_data, enabled);
   return false;
}

static bool gl3_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   gl3_t *gl = (gl3_t *)data;
   if (!gl)
      return false;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

#ifdef HAVE_SLANG
   if (gl->filter_chain)
      gl3_filter_chain_free(gl->filter_chain);
#endif

   if (gl->chain.shader)
      gl->chain.shader->deinit(gl->chain.shader_data);

   if (gl->chain.num_fbo_passes)
   {
      glDeleteFramebuffers(gl->chain.num_fbo_passes, gl->chain.fbo);
      glDeleteTextures(gl->chain.num_fbo_passes, gl->chain.fbo_texture);
   }

   if (gl->chain.fbo_feedback)
      glDeleteFramebuffers(1, &gl->chain.fbo_feedback);

   if (gl->chain.fbo_feedback_texture)
      glDeleteTextures(1, &gl->chain.fbo_feedback_texture);

   if (!gl3_init_filter_chain_with_path(gl, path))
      return false;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);

   return true;
}

static void gl3_set_rotation(void *data, unsigned rotation)
{
   gl3_t *gl = (gl3_t*)data;

   if (!gl)
      return;

   if (video_driver_is_hw_context() && (gl->flags & GL3_FLAG_HW_RENDER_BOTTOM_LEFT))
      gl->rotation = 90 * rotation;
   else
      gl->rotation = 270 * rotation;
   gl3_set_projection(gl, &gl3_default_ortho, true);
}

static void gl3_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned top_y, top_dist;
   gl3_t *gl       = (gl3_t*)data;
   unsigned width  = gl->video_width;
   unsigned height = gl->video_height;

   *vp             = gl->vp;
   vp->full_width  = width;
   vp->full_height = height;

   /* Adjust as GL viewport is bottom-up. */
   top_y           = vp->y + vp->height;
   top_dist        = height - top_y;
   vp->y           = top_dist;
}

static bool gl3_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   gl3_t *gl = (gl3_t*)data;
   unsigned num_pixels = 0;

   if (!gl)
      return false;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   num_pixels = gl->vp.width * gl->vp.height;

   if (gl->flags & GL3_FLAG_PBO_READBACK_ENABLE)
   {
      const void *ptr = NULL;
      struct scaler_ctx *ctx = &gl->pbo_readback_scaler;

      /* Don't readback if we're in menu mode.
       * We haven't buffered up enough frames yet, come back later. */
      if (!gl->pbo_readback_valid[gl->pbo_readback_index])
         goto error;

      gl->pbo_readback_valid[gl->pbo_readback_index] = false;
      glBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[gl->pbo_readback_index]);

      ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, num_pixels * sizeof(uint32_t), GL_MAP_READ_BIT);
      scaler_ctx_scale_direct(ctx, buffer, ptr);
      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   }
   else
   {
      /* Use slow synchronous readbacks. Use this with plain screenshots
         as we don't really care about performance in this case. */

      /* GLES only guarantees GL_RGBA/GL_UNSIGNED_BYTE
       * readbacks so do just that.
       * GLES also doesn't support reading back data
       * from front buffer, so render a cached frame
       * and have gl_frame() do the readback while it's
       * in the back buffer.
       *
       * Keep codepath similar for GLES and desktop GL.
       */
      gl->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));

      if (!gl->readback_buffer_screenshot)
         goto error;

      if (!is_idle)
         video_driver_cached_frame();

      video_frame_convert_rgba_to_bgr(
            (const void*)gl->readback_buffer_screenshot,
            buffer,
            num_pixels);

      free(gl->readback_buffer_screenshot);
      gl->readback_buffer_screenshot = NULL;
   }

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   return true;

error:
   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   return false;
}

static void gl3_update_cpu_texture(gl3_t *gl,
      struct gl3_streamed_texture *streamed,
      const void *frame, unsigned width, unsigned height, unsigned pitch)
{
   if (width != streamed->width || height != streamed->height)
   {
      if (streamed->tex != 0)
         glDeleteTextures(1, &streamed->tex);
      glGenTextures(1, &streamed->tex);
      glBindTexture(GL_TEXTURE_2D, streamed->tex);
      glTexStorage2D(GL_TEXTURE_2D, 1,
            gl->video_info.rgb32
            ? GL_RGBA8
            : GL_RGB565,
            width, height);
      streamed->width = width;
      streamed->height = height;

      if (gl->video_info.rgb32)
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
      }
   }
   else
      glBindTexture(GL_TEXTURE_2D, streamed->tex);

   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   if (gl->video_info.rgb32)
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> 2);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                      width, height, GL_RGBA, GL_UNSIGNED_BYTE, frame);
   }
   else
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> 1);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                      width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame);
   }
}

#if defined(HAVE_MENU)
static void gl3_draw_menu_texture(gl3_t *gl,
      unsigned width, unsigned height)
{
   const float vbo_data[32] = {
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
      1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
      0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
   };

   glEnable(GL_BLEND);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DEPTH_TEST);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   if (gl->flags & GL3_FLAG_MENU_TEXTURE_FULLSCREEN)
      glViewport(0, 0, width, height);
   else
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);

   glActiveTexture(GL_TEXTURE0 + 1);
   glBindTexture(GL_TEXTURE_2D, gl->menu_texture);

   if (gl->chain.active)
   {
      gl->chain.shader->use(gl,
            gl->chain.shader_data, VIDEO_SHADER_STOCK_BLEND, true);

      gl->chain.coords.vertices    = 4;

      gl->chain.shader->set_coords(gl->chain.shader_data, &gl->chain.coords);
      gl->chain.shader->set_mvp(gl->chain.shader_data, &gl->mvp_no_rot);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }
#ifdef HAVE_SLANG
   else
   {
      glUseProgram(gl->pipelines.alpha_blend);
      if (gl->pipelines.alpha_blend_loc.flat_ubo_vertex >= 0)
         glUniform4fv(gl->pipelines.alpha_blend_loc.flat_ubo_vertex, 4, gl->mvp_no_rot_yflip.data);

      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glEnableVertexAttribArray(2);
      gl3_bind_scratch_vbo(gl, vbo_data, sizeof(vbo_data));
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            8 * sizeof(float), (void *)(uintptr_t)0);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            8 * sizeof(float), (void *)(uintptr_t)(2 * sizeof(float)));
      glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE,
            8 * sizeof(float), (void *)(uintptr_t)(4 * sizeof(float)));
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glDisableVertexAttribArray(2);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
   }
#endif

   glDisable(GL_BLEND);
}
#endif

static void gl3_update_input_size(gl3_t *gl, unsigned width, unsigned height)
{
   float xamt = (float)width  / (RARCH_SCALE_BASE * gl->video_info.input_scale);
   float yamt = (float)height / (RARCH_SCALE_BASE * gl->video_info.input_scale);
   GL3_SET_TEXTURE_COORDS(gl->chain.tex_info.coord, xamt, yamt);
}

static void gl3_renderchain_start_render(
      gl3_t *gl)
{
   /* Used when rendering to an FBO.
    * Texture coords have to be aligned
    * with vertex coordinates. */
   static const GLfloat fbo_vertexes[] = {
      0, 0,
      1, 0,
      0, 1,
      1, 1
   };
   glBindFramebuffer(GL_FRAMEBUFFER, gl->chain.fbo[0]);

   gl3_set_viewport(gl,
         gl->chain.fbo_rect[0].img_width,
         gl->chain.fbo_rect[0].img_height, true, false);

   /* Need to preserve the "flipped" state when in FBO
    * as well to have consistent texture coordinates.
    *
    * We will "flip" it in place on last pass. */
   gl->chain.coords.vertex = fbo_vertexes;

   glEnable(GL_FRAMEBUFFER_SRGB);
   glBindTexture(GL_TEXTURE_2D, 0);
}

static void gl3_renderchain_render(
      gl3_t *gl,
      uint64_t frame_count,
      const struct video_tex_info *tex_info,
      const struct video_tex_info *feedback_info)
{
   unsigned i;
   video_shader_ctx_params_t params;
   static GLfloat fbo_tex_coords[8] = {0.0f};
   struct video_tex_info fbo_tex_info[GFX_MAX_SHADERS];
   struct video_tex_info *fbo_info        = NULL;
   const struct video_fbo_rect *prev_rect = NULL;
   GLfloat xamt                           = 0.0f;
   GLfloat yamt                           = 0.0f;
   unsigned mip_level                     = 0;
   unsigned fbo_tex_info_cnt              = 0;
   unsigned width                         = gl->video_width;
   unsigned height                        = gl->video_height;

   /* Render the rest of our passes. */
   gl->chain.coords.tex_coord = fbo_tex_coords;

   /* Calculate viewports, texture coordinates etc,
    * and render all passes from FBOs, to another FBO. */
   for (i = 1; i < gl->chain.num_fbo_passes; i++)
   {
      const struct video_fbo_rect *rect = &gl->chain.fbo_rect[i];

      prev_rect = &gl->chain.fbo_rect[i - 1];
      fbo_info  = &fbo_tex_info[i - 1];

      xamt      = (GLfloat)prev_rect->img_width / prev_rect->width;
      yamt      = (GLfloat)prev_rect->img_height / prev_rect->height;

      GL3_SET_TEXTURE_COORDS(fbo_tex_coords, xamt, yamt);

      fbo_info->tex           = gl->chain.fbo_texture[i - 1];
      fbo_info->input_size[0] = prev_rect->img_width;
      fbo_info->input_size[1] = prev_rect->img_height;
      fbo_info->tex_size[0]   = prev_rect->width;
      fbo_info->tex_size[1]   = prev_rect->height;
      memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
      fbo_tex_info_cnt++;

      glBindFramebuffer(GL_FRAMEBUFFER, gl->chain.fbo[i]);

      gl->chain.shader->use(gl, gl->chain.shader_data,
            i + 1, true);

      glBindTexture(GL_TEXTURE_2D, gl->chain.fbo_texture[i - 1]);

      mip_level = i + 1;

      if (gl->chain.shader->mipmap_input(gl->chain.shader_data, mip_level))
         glGenerateMipmap(GL_TEXTURE_2D);

      glClear(GL_COLOR_BUFFER_BIT);

      /* Render to FBO with certain size. */
      gl3_set_viewport(gl, rect->img_width, rect->img_height, true, false);

      params.vp_width      = gl->out_vp_width;
      params.vp_height     = gl->out_vp_height;
      params.width         = prev_rect->img_width;
      params.height        = prev_rect->img_height;
      params.tex_width     = prev_rect->width;
      params.tex_height    = prev_rect->height;
      params.out_width     = gl->vp.width;
      params.out_height    = gl->vp.height;
      params.frame_counter = (unsigned int)frame_count;
      params.info          = tex_info;
      params.prev_info     = gl->chain.prev_info;
      params.feedback_info = feedback_info;
      params.fbo_info      = fbo_tex_info;
      params.fbo_info_cnt  = fbo_tex_info_cnt;

      gl->chain.shader->set_params(&params, gl->chain.shader_data);

      gl->chain.coords.vertices = 4;

      gl->chain.shader->set_coords(gl->chain.shader_data, &gl->chain.coords);
      gl->chain.shader->set_mvp(gl->chain.shader_data, &gl->mvp);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }

   glDisable(GL_FRAMEBUFFER_SRGB);

   /* Render our last FBO texture directly to screen. */
   prev_rect = &gl->chain.fbo_rect[gl->chain.num_fbo_passes - 1];
   xamt      = (GLfloat)prev_rect->img_width / prev_rect->width;
   yamt      = (GLfloat)prev_rect->img_height / prev_rect->height;

   GL3_SET_TEXTURE_COORDS(fbo_tex_coords, xamt, yamt);

   /* Push final FBO to list. */
   fbo_info                = &fbo_tex_info[gl->chain.num_fbo_passes - 1];

   fbo_info->tex           = gl->chain.fbo_texture[gl->chain.num_fbo_passes - 1];
   fbo_info->input_size[0] = prev_rect->img_width;
   fbo_info->input_size[1] = prev_rect->img_height;
   fbo_info->tex_size[0]   = prev_rect->width;
   fbo_info->tex_size[1]   = prev_rect->height;
   memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
   fbo_tex_info_cnt++;

   /* Render our FBO texture to back buffer. */
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   gl->chain.shader->use(gl, gl->chain.shader_data,
         gl->chain.num_fbo_passes + 1, true);

   glBindTexture(GL_TEXTURE_2D, gl->chain.fbo_texture[gl->chain.num_fbo_passes - 1]);

   mip_level = gl->chain.num_fbo_passes + 1;

   if (gl->chain.shader->mipmap_input(gl->chain.shader_data, mip_level))
      glGenerateMipmap(GL_TEXTURE_2D);

   glClear(GL_COLOR_BUFFER_BIT);
   gl3_set_viewport(gl, width, height, false, true);

   params.vp_width      = gl->out_vp_width;
   params.vp_height     = gl->out_vp_height;
   params.width         = prev_rect->img_width;
   params.height        = prev_rect->img_height;
   params.tex_width     = prev_rect->width;
   params.tex_height    = prev_rect->height;
   params.out_width     = gl->vp.width;
   params.out_height    = gl->vp.height;
   params.frame_counter = (unsigned int)frame_count;
   params.info          = tex_info;
   params.prev_info     = gl->chain.prev_info;
   params.feedback_info = feedback_info;
   params.fbo_info      = fbo_tex_info;
   params.fbo_info_cnt  = fbo_tex_info_cnt;

   gl->chain.shader->set_params(&params, gl->chain.shader_data);

   gl->chain.coords.vertex    = gl->chain.vertex_ptr;

   gl->chain.coords.vertices  = 4;

   gl->chain.shader->set_coords(gl->chain.shader_data, &gl->chain.coords);
   gl->chain.shader->set_mvp(gl->chain.shader_data, &gl->mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->chain.coords.tex_coord = gl->chain.tex_info.coord;
}

static bool gl3_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count,
      unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   struct gl3_filter_chain_texture texture;
   struct gl3_streamed_texture *streamed   = NULL;
#ifdef HAVE_SLANG
   gl3_filter_chain_t *filter_chain        = NULL;
#endif
   gl3_t *gl                               = (gl3_t*)data;
   unsigned width                          = video_info->width;
   unsigned height                         = video_info->height;
   struct font_params *osd_params          = (struct font_params*)
      &video_info->osd_stat_params;
   const char *stat_text                   = video_info->stat_text;
   bool statistics_show                    = video_info->statistics_show;
#if 0
   bool msg_bgcolor_enable                 = video_info->msg_bgcolor_enable;
#endif
   int bfi_light_frames;
   unsigned i;
   unsigned n;
   unsigned hard_sync_frames               = video_info->hard_sync_frames;
   bool input_driver_nonblock_state        = video_info->input_driver_nonblock_state;
#ifdef HAVE_MENU
   bool menu_is_alive                      = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                     = video_info->widgets_active;
#endif
   bool hard_sync                          = video_info->hard_sync;
   bool overlay_behind_menu                = video_info->overlay_behind_menu;

   if (!gl)
      return false;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);
   glBindVertexArray(gl->vao);

   if (gl->chain.active)
      gl->chain.shader->use(gl, gl->chain.shader_data, 1, true);

#ifdef IOS
   /* Apparently the viewport is lost each frame, thanks Apple. */
   gl3_set_viewport(gl, width, height, false, true);
#endif

   if (frame)
      gl->textures_index = (gl->textures_index + 1)
         & (GL_CORE_NUM_TEXTURES - 1);

   streamed = &gl->textures[gl->textures_index];

   texture.image            = 0;
   texture.width            = streamed->width;
   texture.height           = streamed->height;
   texture.padded_width     = 0;
   texture.padded_height    = 0;
   texture.format           = 0;

   if (gl->flags & GL3_FLAG_HW_RENDER_ENABLE)
   {
      texture.image         = gl->hw_render_texture;
      texture.format        = GL_RGBA8;
      texture.padded_width  = gl->hw_render_max_width;
      texture.padded_height = gl->hw_render_max_height;

      if (texture.width == 0)
         texture.width      = 1;
      if (texture.height == 0)
         texture.height     = 1;
   }
   else
   {
      texture.image         = streamed->tex;
      texture.format        = gl->video_info.rgb32 ? GL_RGBA8 : GL_RGB565;
      texture.padded_width  = streamed->width;
      texture.padded_height = streamed->height;
   }

   /* Render to texture in first pass. */
   if (gl->chain.active && gl->chain.num_fbo_passes != 0)
   {
      gl3_renderchain_recompute_pass_sizes(gl,
            frame_width, frame_height,
            gl->out_vp_width, gl->out_vp_height);

      gl3_renderchain_start_render(gl);
   }

   if (frame)
   {
      if (gl->flags & GL3_FLAG_HW_RENDER_ENABLE)
      {
         streamed->width    = frame_width;
         streamed->height   = frame_height;
      }
      else
      {
         if (gl->chain.active)
            gl3_update_input_size(gl, frame_width, frame_height);

         gl3_update_cpu_texture(gl, streamed, frame,
               frame_width, frame_height, pitch);
      }

      /* No point regenerating mipmaps
       * if there are no new frames. */
      if (gl->chain.mipmap_active)
      {
         glBindTexture(GL_TEXTURE_2D, texture.image);
         glGenerateMipmap(GL_TEXTURE_2D);
         glBindTexture(GL_TEXTURE_2D, 0);
      }
   }

   if (gl->flags & GL3_FLAG_SHOULD_RESIZE)
   {
      if (gl->ctx_driver->set_resize)
         gl->ctx_driver->set_resize(gl->ctx_data,
               width, height);
      gl->flags            &= ~GL3_FLAG_SHOULD_RESIZE;

      if (gl->chain.active && gl->chain.num_fbo_passes != 0)
      {
         /* On resize, we might have to recreate our FBOs
          * due to "Viewport" scale, and set a new viewport. */

         /* Check if we have to recreate our FBO textures. */
         for (i = 0; i < gl->chain.num_fbo_passes; i++)
         {
            struct video_fbo_rect *fbo_rect = &gl->chain.fbo_rect[i];
            if (fbo_rect)
            {
               unsigned img_width   = fbo_rect->max_img_width;
               unsigned img_height  = fbo_rect->max_img_height;

               if (     (img_width  > fbo_rect->width)
                     || (img_height > fbo_rect->height))
               {
                  /* Check proactively since we might suddenly
                   * get sizes of tex_w width or tex_h height. */
                  unsigned max                    = img_width > img_height ? img_width : img_height;
                  unsigned pow2_size              = next_pow2(max);
                  bool update_feedback            = gl->chain.fbo_feedback && i == gl->chain.fbo_feedback_pass;

                  fbo_rect->width                 = pow2_size;
                  fbo_rect->height                = pow2_size;

                  gl3_recreate_fbo(fbo_rect, gl->chain.fbo[i], &gl->chain.fbo_texture[i]);

                  /* Update feedback texture in-place so we avoid having to
                   * juggle two different fbo_rect structs since they get updated here. */
                  if (update_feedback)
                  {
                     if (gl3_recreate_fbo(fbo_rect, gl->chain.fbo_feedback,
                              &gl->chain.fbo_feedback_texture))
                     {
                        /* Make sure the feedback textures are cleared
                         * so we don't feedback noise. */
                        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                     }
                  }

                  RARCH_LOG("[GLCore] Recreating FBO texture #%d: %ux%u.\n",
                        i, fbo_rect->width, fbo_rect->height);
               }
            }
         }

         /* Go back to what we're supposed to do,
          * render to FBO #0. */
         gl3_renderchain_start_render(gl);
      }
      else
         gl3_set_viewport(gl, width, height, false, true);
   }

   if (gl->chain.active)
   {
      unsigned i;
      video_shader_ctx_params_t params;
      struct video_tex_info feedback_info;

      gl3_update_input_size(gl, frame_width, frame_height);

      /* Have to reset rendering state which libretro core
       * could easily have overridden. */
      if (gl->flags & GL3_FLAG_HW_RENDER_ENABLE)
      {
         if (gl->chain.num_fbo_passes == 0)
         {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            gl3_set_viewport(gl, width, height, false, true);
         }

         glDisable(GL_DEPTH_TEST);
         glDisable(GL_CULL_FACE);
         glDisable(GL_DITHER);
         glDisable(GL_STENCIL_TEST);
         glDisable(GL_BLEND);
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         glBlendEquation(GL_FUNC_ADD);
      }

      gl->chain.tex_info.tex           = texture.image;
      gl->chain.tex_info.input_size[0] = frame_width;
      gl->chain.tex_info.input_size[1] = frame_height;
      gl->chain.tex_info.tex_size[0]   = RARCH_SCALE_BASE * gl->video_info.input_scale;
      gl->chain.tex_info.tex_size[1]   = RARCH_SCALE_BASE * gl->video_info.input_scale;

      feedback_info                    = gl->chain.tex_info;

      if (gl->chain.fbo_feedback)
      {
         const struct video_fbo_rect
            *rect                      = &gl->chain.fbo_rect[gl->chain.fbo_feedback_pass];
         GLfloat xamt                  = (GLfloat)rect->img_width / rect->width;
         GLfloat yamt                  = (GLfloat)rect->img_height / rect->height;

         feedback_info.tex             = gl->chain.fbo_feedback_texture;
         feedback_info.input_size[0]   = rect->img_width;
         feedback_info.input_size[1]   = rect->img_height;
         feedback_info.tex_size[0]     = rect->width;
         feedback_info.tex_size[1]     = rect->height;

         GL3_SET_TEXTURE_COORDS(feedback_info.coord, xamt, yamt);
      }

      params.vp_width      = gl->out_vp_width;
      params.vp_height     = gl->out_vp_height;
      params.width         = frame_width;
      params.height        = frame_height;
      params.tex_width     = RARCH_SCALE_BASE * gl->video_info.input_scale;
      params.tex_height    = RARCH_SCALE_BASE * gl->video_info.input_scale;
      params.out_width     = gl->vp.width;
      params.out_height    = gl->vp.height;
      params.frame_counter = (unsigned int)frame_count;
      params.info          = &gl->chain.tex_info;
      params.prev_info     = gl->chain.prev_info;
      params.feedback_info = &feedback_info;
      params.fbo_info      = NULL;
      params.fbo_info_cnt  = 0;

      glBindTexture(GL_TEXTURE_2D, texture.image);
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      gl->chain.shader->set_params(&params, gl->chain.shader_data);

      gl->chain.coords.vertices = 4;

      gl->chain.shader->set_coords(gl->chain.shader_data, &gl->chain.coords);
      gl->chain.shader->set_mvp(gl->chain.shader_data, &gl->mvp);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      if (gl->chain.num_fbo_passes != 0)
         gl3_renderchain_render(gl, frame_count, &gl->chain.tex_info, &feedback_info);

      memmove(gl->chain.prev_info + 1, gl->chain.prev_info, sizeof(*gl->chain.prev_info) * (gl->chain.num_prev_textures - 1));
      memcpy(&gl->chain.prev_info[0], &gl->chain.tex_info, sizeof(gl->chain.tex_info));

      /* Implement feedback by swapping out FBO/textures
       * for FBO pass #N and feedbacks. */
      if (gl->chain.fbo_feedback)
      {
         GLuint tmp_fbo                 = gl->chain.fbo_feedback;
         GLuint tmp_tex                 = gl->chain.fbo_feedback_texture;
         gl->chain.fbo_feedback         = gl->chain.fbo[gl->chain.fbo_feedback_pass];
         gl->chain.fbo_feedback_texture = gl->chain.fbo_texture[gl->chain.fbo_feedback_pass];
         gl->chain.fbo[gl->chain.fbo_feedback_pass]         = tmp_fbo;
         gl->chain.fbo_texture[gl->chain.fbo_feedback_pass] = tmp_tex;
      }

      glBindTexture(GL_TEXTURE_2D, 0);
   }
#ifdef HAVE_SLANG
   else
   {
      /* Fast toggle shader filter chain logic */
      filter_chain = gl->filter_chain;

      if (!video_info->shader_active && gl->filter_chain != gl->filter_chain_default)
      {
         if (!gl->filter_chain_default)
            gl3_init_default_filter_chain(gl);

         if (gl->filter_chain_default)
            filter_chain = gl->filter_chain_default;
         else
            return false;
      }

      if (!filter_chain && gl->filter_chain_default)
         filter_chain = gl->filter_chain_default;

      gl3_filter_chain_set_frame_count(filter_chain, frame_count);
#ifdef HAVE_REWIND
      gl3_filter_chain_set_frame_direction(filter_chain, state_manager_frame_is_reversed() ? -1 : 1);
#else
      gl3_filter_chain_set_frame_direction(filter_chain, 1);
#endif
      gl3_filter_chain_set_frame_time_delta(filter_chain, (uint32_t)video_driver_get_frame_time_delta_usec());

      gl3_filter_chain_set_original_fps(filter_chain, video_driver_get_original_fps());

      gl3_filter_chain_set_rotation(filter_chain, retroarch_get_rotation());

      gl3_filter_chain_set_core_aspect(filter_chain, video_driver_get_core_aspect());

      /* OriginalAspectRotated: return 1/aspect for 90 and 270 rotated content */
      uint32_t rot = retroarch_get_rotation();
      float core_aspect_rot = video_driver_get_core_aspect();
      if (rot == 1 || rot == 3)
         core_aspect_rot = 1/core_aspect_rot;
      gl3_filter_chain_set_core_aspect_rot(filter_chain, core_aspect_rot);

      /* Sub-frame info for multiframe shaders (per real content frame).
         Should always be 1 for non-use of subframes*/
      if (!(gl->flags & GL3_FLAG_FRAME_DUPE_LOCK))
      {
        if (     video_info->black_frame_insertion
              || video_info->input_driver_nonblock_state
              || video_info->runloop_is_slowmotion
              || video_info->runloop_is_paused
              || (gl->flags & GL3_FLAG_MENU_TEXTURE_ENABLE))
           gl3_filter_chain_set_shader_subframes(
              filter_chain, 1);
        else
           gl3_filter_chain_set_shader_subframes(
              filter_chain, video_info->shader_subframes);

        gl3_filter_chain_set_current_shader_subframe(
              filter_chain, 1);
      }

#ifdef GL3_ROLLING_SCANLINE_SIMULATION
      if (      (video_info->shader_subframes > 1)
            &&  (video_info->scan_subframes)
            &&  !video_info->black_frame_insertion
            &&  !video_info->input_driver_nonblock_state
            &&  !video_info->runloop_is_slowmotion
            &&  !video_info->runloop_is_paused
            &&  (!(gl->flags & GL3_FLAG_MENU_TEXTURE_ENABLE)))
         gl3_filter_chain_set_simulate_scanline(
               filter_chain, true);
      else
         gl3_filter_chain_set_simulate_scanline(
               filter_chain, false);
#endif /* GL3_ROLLING_SCANLINE_SIMULATION */

      gl3_filter_chain_set_input_texture(filter_chain, &texture);
      gl3_filter_chain_build_offscreen_passes(filter_chain,
            &gl->filter_chain_vp);

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      gl3_filter_chain_build_viewport_pass(filter_chain,
            &gl->filter_chain_vp,
            (gl->flags & GL3_FLAG_HW_RENDER_BOTTOM_LEFT)
            ? gl->mvp.data
            : gl->mvp_yflip.data);
      gl3_filter_chain_end_frame(filter_chain);
   }
#endif /* HAVE_SLANG */

#ifdef HAVE_OVERLAY
   if ((gl->flags & GL3_FLAG_OVERLAY_ENABLE) && overlay_behind_menu)
      gl3_render_overlay(gl, width, height);
#endif

#if defined(HAVE_MENU)
   if (gl->flags & GL3_FLAG_MENU_TEXTURE_ENABLE)
   {
      menu_driver_frame(menu_is_alive, video_info);
      if (gl->menu_texture)
         gl3_draw_menu_texture(gl, width, height);
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(gl, stat_text,
               (const struct font_params*)osd_params, NULL);
   }
#endif

#ifdef HAVE_OVERLAY
   if ((gl->flags & GL3_FLAG_OVERLAY_ENABLE) && !overlay_behind_menu)
      gl3_render_overlay(gl, width, height);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (!string_is_empty(msg))
   {
#if 0
      if (msg_bgcolor_enable)
         gl3_render_osd_background(gl, video_info, msg);
#endif
      font_driver_render_msg(gl, msg, NULL, NULL);
   }

   if (gl->ctx_driver->update_window_title)
      gl->ctx_driver->update_window_title(gl->ctx_data);

   if (gl->readback_buffer_screenshot)
   {
      /* For screenshots, just do the regular slow readback. */
      glPixelStorei(GL_PACK_ALIGNMENT, 4);
      glPixelStorei(GL_PACK_ROW_LENGTH, 0);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
#ifndef HAVE_OPENGLES
      glReadBuffer(GL_BACK);
#endif
      glReadPixels(
            (gl->vp.x > 0) ? gl->vp.x : 0,
            (gl->vp.y > 0) ? gl->vp.y : 0,
            (gl->vp.width  > gl->video_width)  ? gl->video_width  : gl->vp.width,
            (gl->vp.height > gl->video_height) ? gl->video_height : gl->vp.height,
            GL_RGBA, GL_UNSIGNED_BYTE,
            gl->readback_buffer_screenshot);
   }
   else if (gl->flags & GL3_FLAG_PBO_READBACK_ENABLE)
   {
#ifdef HAVE_MENU
      /* Don't readback if we're in menu mode. */
      if (!(gl->flags & GL3_FLAG_MENU_TEXTURE_ENABLE))
#endif
         gl3_pbo_async_readback(gl);
   }

   if (gl->ctx_driver->swap_buffers)
      gl->ctx_driver->swap_buffers(gl->ctx_data);

 /* Emscripten has to do black frame insertion in its main loop */
#ifndef EMSCRIPTEN
   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (
         video_info->black_frame_insertion
         && !video_info->input_driver_nonblock_state
         && !video_info->runloop_is_slowmotion
         && !video_info->runloop_is_paused
         && !(gl->flags & GL3_FLAG_MENU_TEXTURE_ENABLE))
   {

      if (video_info->bfi_dark_frames > video_info->black_frame_insertion)
      video_info->bfi_dark_frames = video_info->black_frame_insertion;

      /* BFI now handles variable strobe strength, like on-off-off, vs on-on-off for 180hz.
         This needs to be done with duping frames instead of increased swap intervals for
         a couple reasons. Swap interval caps out at 4 in most all apis as of coding,
         and seems to be flat ignored >1 at least in modern Windows for some older APIs. */
      bfi_light_frames = video_info->black_frame_insertion - video_info->bfi_dark_frames;
      if (bfi_light_frames > 0 && !(gl->flags & GL3_FLAG_FRAME_DUPE_LOCK))
      {
         gl->flags |= GL3_FLAG_FRAME_DUPE_LOCK;

         while (bfi_light_frames > 0)
         {
            if (!(gl3_frame(gl, NULL, 0, 0, frame_count, 0, msg, video_info)))
            {
               gl->flags &= ~GL3_FLAG_FRAME_DUPE_LOCK;
               return false;
            }
            --bfi_light_frames;
         }
         gl->flags &= ~GL3_FLAG_FRAME_DUPE_LOCK;
      }

      for (n = 0; n < video_info->bfi_dark_frames; ++n)
      {
         if (!(gl->flags & GL3_FLAG_FRAME_DUPE_LOCK))
         {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            if (gl->ctx_driver->swap_buffers)
               gl->ctx_driver->swap_buffers(gl->ctx_data);
         }
      }
   }
#endif

   /* Frame duping for Shader Subframes, don't combine with swap_interval > 1, BFI.
      Also, a major logical use of shader sub-frames will still be shader implemented BFI
      or even rolling scan bfi, so we need to protect the menu/ff/etc from bad flickering
      from improper settings, and unnecessary performance overhead for ff, screenshots etc. */
   if (      (video_info->shader_subframes > 1)
         &&  !video_info->black_frame_insertion
         &&  !video_info->input_driver_nonblock_state
         &&  !video_info->runloop_is_slowmotion
         &&  !video_info->runloop_is_paused
         &&  (!(gl->flags & GL3_FLAG_MENU_TEXTURE_ENABLE))
         &&  (!(gl->flags & GL3_FLAG_FRAME_DUPE_LOCK)))
   {
      gl->flags |= GL3_FLAG_FRAME_DUPE_LOCK;
      for (i = 1; i < video_info->shader_subframes; i++)
      {
#ifdef HAVE_SLANG
         if (!gl->chain.active)
         {
            gl3_filter_chain_set_shader_subframes(
               filter_chain, video_info->shader_subframes);
            gl3_filter_chain_set_current_shader_subframe(
               filter_chain, i+1);
         }
#endif

         if (!gl3_frame(gl, NULL, 0, 0, frame_count, 0, msg,
                  video_info))
         {
            gl->flags &= ~GL3_FLAG_FRAME_DUPE_LOCK;
            return false;
         }
      }
      gl->flags &= ~GL3_FLAG_FRAME_DUPE_LOCK;
   }

   if (    hard_sync
       && !input_driver_nonblock_state
       )
      gl3_fence_iterate(gl, hard_sync_frames);

   glBindVertexArray(0);
   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
   return true;
}

static uint32_t gl3_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_HARD_SYNC);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_SUBFRAME_SHADERS);
   BIT32_SET(flags, GFX_CTX_FLAGS_FAST_TOGGLE_SHADERS);

   return flags;
}

static float gl3_get_refresh_rate(void *data)
{
   float refresh_rate;
   if (video_context_driver_get_refresh_rate(&refresh_rate))
       return refresh_rate;
   return 0.0f;
}

static void gl3_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl3_t *gl     = (gl3_t*)data;
   if (gl)
      gl->flags |= (GL3_FLAG_KEEP_ASPECT | GL3_FLAG_SHOULD_RESIZE);
}

static void gl3_apply_state_changes(void *data)
{
   gl3_t *gl     = (gl3_t*)data;
   if (gl)
      gl->flags |= GL3_FLAG_SHOULD_RESIZE;
}

static struct video_shader *gl3_get_current_shader(void *data)
{
   gl3_t *gl = (gl3_t*)data;
#ifdef HAVE_SLANG
   if (gl && gl->filter_chain)
      return gl3_filter_chain_get_preset(gl->filter_chain);
#endif
   return NULL;
}

#ifdef HAVE_THREADS
static int video_texture_load_wrap_gl3_mipmap(void *data)
{
   GLuint id = 0;
   gl3_t *gl = (gl3_t*)video_driver_get_ptr();

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);

   if (data)
      video_texture_load_gl3((struct texture_image*)data,
            TEXTURE_FILTER_MIPMAP_LINEAR, &id);
   return (int)id;
}

static int video_texture_load_wrap_gl3(void *data)
{
   GLuint id = 0;
   gl3_t *gl = (gl3_t*)video_driver_get_ptr();

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);

   if (data)
      video_texture_load_gl3((struct texture_image*)data,
            TEXTURE_FILTER_LINEAR, &id);
   return (int)id;
}

static int video_texture_unload_wrap_gl3(void *data)
{
   GLuint  glid;
   uintptr_t id = (uintptr_t)data;
   gl3_t    *gl = (gl3_t*)video_driver_get_ptr();

   if (gl && gl->ctx_driver->make_current)
      gl->ctx_driver->make_current(false);

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
   return 0;
}
#endif

static uintptr_t gl3_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   GLuint id = 0;

#ifdef HAVE_THREADS
   if (threaded)
   {
      custom_command_method_t func = video_texture_load_wrap_gl3;
      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            func = video_texture_load_wrap_gl3_mipmap;
            break;
         default:
            break;
      }
      return video_thread_texture_handle(data, func);
   }
#endif

   video_texture_load_gl3((struct texture_image*)data, filter_type, &id);
   return id;
}

static void gl3_unload_texture(void *data, bool threaded,
      uintptr_t id)
{
   GLuint glid;
   if (!id)
      return;

#ifdef HAVE_THREADS
   if (threaded)
   {
      custom_command_method_t func = video_texture_unload_wrap_gl3;
      video_thread_texture_handle((void *)id, func);
      return;
   }
#endif

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
}

static void gl3_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gl3_t *gl = (gl3_t*)data;
   if (gl->ctx_driver->set_video_mode)
      gl->ctx_driver->set_video_mode(gl->ctx_data,
            width, height, fullscreen);
}

static void gl3_show_mouse(void *data, bool state)
{
   gl3_t *gl = (gl3_t*)data;
   if (gl && gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl->ctx_data, state);
}

static void gl3_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   settings_t *settings = config_get_ptr();
   GLenum menu_filter   = settings->bools.menu_linear_filter
      ? GL_LINEAR : GL_NEAREST;
   unsigned base_size   = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   gl3_t *gl            = (gl3_t*)data;
   if (!gl)
      return;

   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, false);

   if (gl->menu_texture)
      glDeleteTextures(1, &gl->menu_texture);
   glGenTextures(1, &gl->menu_texture);
   glBindTexture(GL_TEXTURE_2D, gl->menu_texture);
   glTexStorage2D(GL_TEXTURE_2D, 1, rgb32
         ? GL_RGBA8 : GL_RGBA4, width, height);

   glPixelStorei(GL_UNPACK_ALIGNMENT, base_size);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                   width, height, GL_RGBA, rgb32
                   ? GL_UNSIGNED_BYTE
                   : GL_UNSIGNED_SHORT_4_4_4_4, frame);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, menu_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, menu_filter);

   if (rgb32)
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   gl->menu_texture_alpha = alpha;
   if (gl->flags & GL3_FLAG_USE_SHARED_CONTEXT)
      gl->ctx_driver->bind_hw_render(gl->ctx_data, true);
}

static void gl3_set_texture_enable(void *data, bool state, bool fullscreen)
{
   gl3_t *gl = (gl3_t*)data;

   if (!gl)
      return;

   if (state)
      gl->flags |=  GL3_FLAG_MENU_TEXTURE_ENABLE;
   else
      gl->flags &= ~GL3_FLAG_MENU_TEXTURE_ENABLE;
   if (fullscreen)
      gl->flags |=  GL3_FLAG_MENU_TEXTURE_FULLSCREEN;
   else
      gl->flags &= ~GL3_FLAG_MENU_TEXTURE_FULLSCREEN;
}

static void gl3_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   gl3_t   *gl = (gl3_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->get_video_output_size)
      gl->ctx_driver->get_video_output_size(
            gl->ctx_data,
            width, height, desc, desc_len);
}

static void gl3_get_video_output_prev(void *data)
{
   gl3_t   *gl = (gl3_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->get_video_output_prev)
      gl->ctx_driver->get_video_output_prev(gl->ctx_data);
}

static void gl3_get_video_output_next(void *data)
{
   gl3_t   *gl = (gl3_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->get_video_output_next)
      gl->ctx_driver->get_video_output_next(gl->ctx_data);
}

static uintptr_t gl3_get_current_framebuffer(void *data)
{
   gl3_t *gl = (gl3_t*)data;
   if (gl && (gl->flags & GL3_FLAG_HW_RENDER_ENABLE))
      return gl->hw_render_fbo;
   return 0;
}

static retro_proc_address_t gl3_get_proc_address(
      void *data, const char *sym)
{
   gl3_t *gl = (gl3_t*)data;
   if (gl && gl->ctx_driver->get_proc_address)
      return gl->ctx_driver->get_proc_address(sym);
   return NULL;
}

static const video_poke_interface_t gl3_poke_interface = {
   gl3_get_flags,
   gl3_load_texture,
   gl3_unload_texture,
   gl3_set_video_mode,
   gl3_get_refresh_rate,
   NULL, /* set_filtering */
   gl3_get_video_output_size,
   gl3_get_video_output_prev,
   gl3_get_video_output_next,
   gl3_get_current_framebuffer,
   gl3_get_proc_address,
   gl3_set_aspect_ratio,
   gl3_apply_state_changes,
   gl3_set_texture_frame,
   gl3_set_texture_enable,
   font_driver_render_msg,
   gl3_show_mouse,
   NULL, /* grab_mouse_toggle */
   gl3_get_current_shader,
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void gl3_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gl3_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool gl3_gfx_widgets_enabled(void *data) { return true; }
#endif

static unsigned gl3_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
      case RARCH_WRAP_BORDER:
#ifdef HAVE_OPENGLES3
         /* GLES does not support CLAMP_TO_BORDER until GLES 3.2.
          * It is a deprecated feature in general. */
         return GL_CLAMP_TO_EDGE;
#else
         return GL_CLAMP_TO_BORDER;
#endif
      case RARCH_WRAP_EDGE:
         return GL_CLAMP_TO_EDGE;
      case RARCH_WRAP_REPEAT:
         return GL_REPEAT;
      case RARCH_WRAP_MIRRORED_REPEAT:
         return GL_MIRRORED_REPEAT;
      default:
         break;
   }

   return 0;
}

static bool gl3_has_windowed(void *data)
{
   gl3_t *gl        = (gl3_t*)data;
   if (gl && gl->ctx_driver)
      return gl->ctx_driver->has_windowed;
   return false;
}

static bool gl3_focus(void *data)
{
   gl3_t *gl        = (gl3_t*)data;
   if (gl && gl->ctx_driver && gl->ctx_driver->has_focus)
      return gl->ctx_driver->has_focus(gl->ctx_data);
   return true;
}

video_driver_t video_gl3 = {
   gl3_init,
   gl3_frame,
   gl3_set_nonblock_state,
   gl3_alive,
   gl3_focus,
   gl3_suppress_screensaver,
   gl3_has_windowed,
   gl3_set_shader,
   gl3_free,
   "glcore",
   gl3_set_viewport_wrapper,
   gl3_set_rotation,
   gl3_viewport_info,
   gl3_read_viewport,
#if defined(READ_RAW_GL_FRAME_TEST)
   gl3_read_frame_raw,
#else
   NULL, /* read_frame_raw */
#endif
#ifdef HAVE_OVERLAY
   gl3_get_overlay_interface,
#endif
   gl3_get_poke_interface,
   gl3_wrap_type_to_enum,
#ifdef HAVE_GFX_WIDGETS
   gl3_gfx_widgets_enabled
#endif
};

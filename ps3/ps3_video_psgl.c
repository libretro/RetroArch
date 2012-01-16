/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "../driver.h"

#include "ps3_video_psgl.h"

#include <stdint.h>
#include "../libsnes.hpp"
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include "../general.h"
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../gfx/gl_common.h"
#include "../gfx/gfx_common.h"
#include "../strl.h"

#include "../gfx/shader_cg.h"

#define BLUE		0xffff0000u
#define WHITE		0xffffffffu

// Used for the last pass when rendering to the back buffer.
static const GLfloat vertexes_flipped[] = {
   0, 0,
   0, 1,
   1, 1,
   1, 0
};

// Used when rendering to an FBO.
// Texture coords have to be aligned with vertex coordinates.
static const GLfloat vertexes[] = {
   0, 1,
   0, 0,
   1, 0,
   1, 1
};

static const GLfloat tex_coords[] = {
   0, 1,
   0, 0,
   1, 0,
   1, 1
};

static const GLfloat white_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};


static bool load_fbo_proc(void) { return true; }

#define MAX_SHADERS 16

#define TEXTURES 8
#define TEXTURES_MASK (TEXTURES - 1)

bool g_quitting;
unsigned g_frame_count;
void *g_gl;

typedef struct gl
{
   GLuint pbo;
   PSGLdevice* gl_device;
   PSGLcontext* gl_context;
   bool vsync;
   GLuint texture[TEXTURES];
   unsigned tex_index; // For use with PREV.
   struct gl_tex_info prev_info[TEXTURES];
   GLuint tex_filter;
   void *empty_buf;

   // Render-to-texture, multipass shaders
   GLuint fbo[MAX_SHADERS];
   GLuint fbo_texture[MAX_SHADERS];
   struct gl_fbo_rect fbo_rect[MAX_SHADERS];
   struct gl_fbo_scale fbo_scale[MAX_SHADERS];
   bool render_to_tex;
   int fbo_pass;
   bool fbo_inited;

   bool should_resize;
   bool keep_aspect;

   unsigned win_width;
   unsigned win_height;
   unsigned vp_width, vp_out_width;
   unsigned vp_height, vp_out_height;
   unsigned last_width[TEXTURES];
   unsigned last_height[TEXTURES];
   unsigned tex_w, tex_h;
   GLfloat tex_coords[8];
   GLfloat fbo_tex_coords[8];

   GLenum texture_type; // XBGR1555 or ARGB
   GLenum texture_fmt;
   unsigned base_size; // 2 or 4
} gl_t;

////////////////// Shaders
static bool gl_shader_init(void)
{
   switch (g_settings.video.shader_type)
   {
      case SSNES_SHADER_AUTO:
      {
         if (strlen(g_settings.video.cg_shader_path) > 0 && strlen(g_settings.video.bsnes_shader_path) > 0)
            SSNES_WARN("Both Cg and bSNES XML shader are defined in config file. Cg shader will be selected by default.\n");

         if (strlen(g_settings.video.cg_shader_path) > 0)
            return gl_cg_init(g_settings.video.cg_shader_path);
         break;
      }

      case SSNES_SHADER_CG:
      {
         if (strlen(g_settings.video.cg_shader_path) > 0)
		 return gl_cg_init(g_settings.video.cg_shader_path);
         break;
      }

      default:
         break;
   }

   return true;
}

static void gl_shader_use(unsigned index)
{
   gl_cg_use(index);
}

static void gl_shader_deinit(void)
{
   gl_cg_deinit();
}

static void gl_shader_set_proj_matrix(void)
{
   gl_cg_set_proj_matrix();
}

static void gl_shader_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info,
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info, unsigned fbo_info_cnt)
{
   gl_cg_set_params(width, height, 
         tex_width, tex_height, 
         out_width, out_height, 
         frame_count, info, prev_info, fbo_info, fbo_info_cnt);
}

static unsigned gl_shader_num(void)
{
   unsigned num = 0;
   unsigned cg_num = gl_cg_num();
   if (cg_num > num)
      num = cg_num;

   return num;
}

static bool gl_shader_filter_type(unsigned index, bool *smooth)
{
   bool valid = false;
   if (!valid)
      valid = gl_cg_filter_type(index, smooth);

   return valid;
}

static void gl_shader_scale(unsigned index, struct gl_fbo_scale *scale)
{
   scale->valid = false;
   if (!scale->valid)
      gl_cg_shader_scale(index, scale);
}

// Horribly long and complex FBO init :D
static void gl_init_fbo(gl_t *gl, unsigned width, unsigned height)
{
   if (!g_settings.video.render_to_texture && gl_shader_num() == 0)
      return;

   struct gl_fbo_scale scale, scale_last;
   gl_shader_scale(1, &scale);
   gl_shader_scale(gl_shader_num(), &scale_last);

   // No need to use FBOs.
   if (gl_shader_num() == 1 && !scale.valid && !g_settings.video.render_to_texture)
      return;

   if (!load_fbo_proc())
   {
      SSNES_ERR("Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   gl->fbo_pass = gl_shader_num() - 1;
   if (scale_last.valid)
      gl->fbo_pass++;

   if (gl->fbo_pass <= 0)
      gl->fbo_pass = 1;

   if (!scale.valid)
   {
      scale.scale_x = g_settings.video.fbo_scale_x;
      scale.scale_y = g_settings.video.fbo_scale_y;
      scale.type_x = scale.type_y = SSNES_SCALE_INPUT;
   }

   switch (scale.type_x)
   {
      case SSNES_SCALE_INPUT:
         gl->fbo_rect[0].width = width * next_pow2(ceil(scale.scale_x));
         break;

      case SSNES_SCALE_ABSOLUTE:
         gl->fbo_rect[0].width = next_pow2(scale.abs_x);
         break;

      case SSNES_SCALE_VIEWPORT:
         gl->fbo_rect[0].width = next_pow2(gl->win_width);
         break;

      default:
         break;
   }

   switch (scale.type_y)
   {
      case SSNES_SCALE_INPUT:
         gl->fbo_rect[0].height = height * next_pow2(ceil(scale.scale_y));
         break;

      case SSNES_SCALE_ABSOLUTE:
         gl->fbo_rect[0].height = next_pow2(scale.abs_y);
         break;

      case SSNES_SCALE_VIEWPORT:
         gl->fbo_rect[0].height = next_pow2(gl->win_height);
         break;

      default:
         break;
   }

   unsigned last_width = gl->fbo_rect[0].width, last_height = gl->fbo_rect[0].height;
   gl->fbo_scale[0] = scale;

   SSNES_LOG("Creating FBO 0 @ %ux%u\n", gl->fbo_rect[0].width, gl->fbo_rect[0].height);

   for (int i = 1; i < gl->fbo_pass; i++)
   {
      gl_shader_scale(i + 1, &gl->fbo_scale[i]);
      if (gl->fbo_scale[i].valid)
      {
         switch (gl->fbo_scale[i].type_x)
         {
            case SSNES_SCALE_INPUT:
               gl->fbo_rect[i].width = last_width * next_pow2(ceil(gl->fbo_scale[i].scale_x));
               break;

            case SSNES_SCALE_ABSOLUTE:
               gl->fbo_rect[i].width = next_pow2(gl->fbo_scale[i].abs_x);
               break;

            case SSNES_SCALE_VIEWPORT:
               gl->fbo_rect[i].width = next_pow2(gl->win_width);
               break;

            default:
               break;
         }

         switch (gl->fbo_scale[i].type_y)
         {
            case SSNES_SCALE_INPUT:
               gl->fbo_rect[i].height = last_height * next_pow2(ceil(gl->fbo_scale[i].scale_y));
               break;

            case SSNES_SCALE_ABSOLUTE:
               gl->fbo_rect[i].height = next_pow2(gl->fbo_scale[i].abs_y);
               break;

            case SSNES_SCALE_VIEWPORT:
               gl->fbo_rect[i].height = next_pow2(gl->win_height);
               break;

            default:
               break;
         }

         last_width = gl->fbo_rect[i].width;
         last_height = gl->fbo_rect[i].height;
      }
      else
      {
         // Use previous values, essentially a 1x scale compared to last shader in chain.
         gl->fbo_rect[i] = gl->fbo_rect[i - 1];
         gl->fbo_scale[i].scale_x = gl->fbo_scale[i].scale_y = 1.0;
         gl->fbo_scale[i].type_x = gl->fbo_scale[i].type_y = SSNES_SCALE_INPUT;
      }

      SSNES_LOG("Creating FBO %d @ %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   glGenTextures(gl->fbo_pass, gl->fbo_texture);

   GLuint base_filt = g_settings.video.second_pass_smooth ? GL_LINEAR : GL_NEAREST;
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

      GLuint filter_type = base_filt;
      bool smooth;
      if (gl_shader_filter_type(i + 2, &smooth))
         filter_type = smooth ? GL_LINEAR : GL_NEAREST;

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_type);

      glTexImage2D(GL_TEXTURE_2D,
            0, GL_RGBA, gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0, GL_BGRA,
            GL_UNSIGNED_INT_8_8_8_8, NULL);
   }

   glBindTexture(GL_TEXTURE_2D, 0);

   glGenFramebuffersOES(gl->fbo_pass, gl->fbo);
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[i]);
      glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      GLenum status = glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES);
      if (status != GL_FRAMEBUFFER_COMPLETE_OES)
         goto error;
   }

   gl->fbo_inited = true;
   return;

error:
   glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
   glDeleteFramebuffersOES(gl->fbo_pass, gl->fbo);
   SSNES_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
}

static inline unsigned get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

static void set_viewport(gl_t *gl, unsigned width, unsigned height, bool force_full)
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)width / height;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      // assume they are actually equal.
      if (fabs(device_aspect - desired_aspect) < 0.0001)
      {
         glViewport(0, 0, width, height);
      }
      else if (device_aspect > desired_aspect)
      {
         float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         glViewport((GLint)(width * (0.5 - delta)), 0,(GLint)(2.0 * width * delta), height);
         width = (unsigned)(2.0 * width * delta);
      }
      else
      {
         float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         glViewport(0, (GLint)(height * (0.5 - delta)), width,(GLint)(2.0 * height * delta));
         height = (unsigned)(2.0 * height * delta);
      }
   }
   else
      glViewport(0, 0, width, height);

   glOrthof(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   gl_shader_set_proj_matrix();

   gl->vp_width = width;
   gl->vp_height = height;

   // Set last backbuffer viewport.
   if (!force_full)
   {
      gl->vp_out_width = width;
      gl->vp_out_height = height;
   }

   //SSNES_LOG("Setting viewport @ %ux%u\n", width, height);
}

static inline void set_lut_texture_coords(const GLfloat *coords)
{
   // For texture images.
   pglClientActiveTexture(GL_TEXTURE1);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 0, coords);
   pglClientActiveTexture(GL_TEXTURE0);
}

static inline void set_texture_coords(GLfloat *coords, GLfloat xamt, GLfloat yamt)
{
   coords[1] = yamt;
   coords[4] = xamt;
   coords[6] = xamt;
   coords[7] = yamt;
}

static bool gl_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   gl_t *gl = data;

   gl_shader_use(1);
   g_frame_count++;

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   // Render to texture in first pass.
   if (gl->fbo_inited)
   {
      unsigned last_width = width;
      unsigned last_height = height;
      unsigned last_max_width = gl->tex_w;
      unsigned last_max_height = gl->tex_h;
      // Calculate viewports for FBOs.
      for (int i = 0; i < gl->fbo_pass; i++)
      {
         switch (gl->fbo_scale[i].type_x)
         {
            case SSNES_SCALE_INPUT:
               gl->fbo_rect[i].img_width = last_width * gl->fbo_scale[i].scale_x;
               gl->fbo_rect[i].max_img_width = last_max_width * gl->fbo_scale[i].scale_x;
               break;

            case SSNES_SCALE_ABSOLUTE:
               gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].abs_x;
               break;

            case SSNES_SCALE_VIEWPORT:
               gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].scale_x * gl->vp_out_width;
               break;

            default:
               break;
         }

         switch (gl->fbo_scale[i].type_y)
         {
            case SSNES_SCALE_INPUT:
               gl->fbo_rect[i].img_height = last_height * gl->fbo_scale[i].scale_y;
               gl->fbo_rect[i].max_img_height = last_max_height * gl->fbo_scale[i].scale_y;
               break;

            case SSNES_SCALE_ABSOLUTE:
               gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].abs_y;
               break;

            case SSNES_SCALE_VIEWPORT:
               gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].scale_y * gl->vp_out_height;
               break;

            default:
               break;
         }

         last_width = gl->fbo_rect[i].img_width;
         last_height = gl->fbo_rect[i].img_height;
         last_max_width = gl->fbo_rect[i].max_img_width;
         last_max_height = gl->fbo_rect[i].max_img_height;
      }

      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
      glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[0]);
      gl->render_to_tex = true;
      set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true);
   }

   if (gl->should_resize)
   {
      gl->should_resize = false;

      //sdlwrap_set_resize(gl->win_width, gl->win_height);

      if (!gl->render_to_tex)
         set_viewport(gl, gl->win_width, gl->win_height, false);
      else
      {
         // Check if we have to recreate our FBO textures.
         for (int i = 0; i < gl->fbo_pass; i++)
         {
            // Check proactively since we might suddently get sizes of tex_w width or tex_h height.
            if (gl->fbo_rect[i].max_img_width > gl->fbo_rect[i].width ||
                  gl->fbo_rect[i].max_img_height > gl->fbo_rect[i].height)
            {
               unsigned img_width = gl->fbo_rect[i].max_img_width;
               unsigned img_height = gl->fbo_rect[i].max_img_height;
               unsigned max = img_width > img_height ? img_width : img_height;
               unsigned pow2_size = next_pow2(max);
               gl->fbo_rect[i].width = gl->fbo_rect[i].height = pow2_size;

               glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[i]);
               glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);
               glTexImage2D(GL_TEXTURE_2D,
                     0, GL_RGBA, gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0, GL_BGRA,
                     GL_UNSIGNED_INT_8_8_8_8, NULL);

               glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

               GLenum status = glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES);
               if (status != GL_FRAMEBUFFER_COMPLETE_OES)
                  SSNES_WARN("Failed to reinit FBO texture!\n");

               SSNES_LOG("Recreating FBO texture #%d: %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
            }
         }

         // Go back to what we're supposed to do, render to FBO #0 :D
         glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
         glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[0]);
         set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true);
      }
   }

   if ((width != gl->last_width[gl->tex_index] || height != gl->last_height[gl->tex_index]) && gl->empty_buf) // Res change. need to clear out texture.
   {
      gl->last_width[gl->tex_index] = width;
      gl->last_height[gl->tex_index] = height;

      glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
            gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size,
            gl->tex_w * gl->tex_h * gl->base_size,
            gl->empty_buf);

      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;

      set_texture_coords(gl->tex_coords, xamt, yamt);
   }
   // We might have used different texture coordinates last frame. Edge case if resolution changes very rapidly.
   else if (width != gl->last_width[(gl->tex_index - 1) & TEXTURES_MASK] || height != gl->last_height[(gl->tex_index - 1) & TEXTURES_MASK])
   {
      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;
      set_texture_coords(gl->tex_coords, xamt, yamt);
   }

   // Need to preserve the "flipped" state when in FBO as well to have 
   // consistent texture coordinates.
   if (gl->render_to_tex)
      glVertexPointer(2, GL_FLOAT, 0, vertexes);

   {
      size_t buffer_addr = gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size;
      size_t buffer_stride = gl->tex_w * gl->base_size;
      const uint8_t *frame_copy = frame;
      size_t frame_copy_size = width * gl->base_size;
      for (unsigned h = 0; h < height; h++)
      {
         glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE, 
               buffer_addr,
               frame_copy_size,
               frame_copy);

         frame_copy += pitch;
         buffer_addr += buffer_stride;
      }
   }

   struct gl_tex_info tex_info = {
      .tex = gl->texture[gl->tex_index],
      .input_size = {width, height},
      .tex_size = {gl->tex_w, gl->tex_h}
   };
   struct gl_tex_info fbo_tex_info[MAX_SHADERS];
   unsigned fbo_tex_info_cnt = 0;
   memcpy(tex_info.coord, gl->tex_coords, sizeof(gl->tex_coords));

   glClear(GL_COLOR_BUFFER_BIT);
   gl_shader_set_params(width, height, gl->tex_w, gl->tex_h, gl->vp_width, gl->vp_height, g_frame_count, 
         &tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

   glDrawArrays(GL_QUADS, 0, 4);

   if (gl->fbo_inited)
   {
      // Render the rest of our passes.
      glTexCoordPointer(2, GL_FLOAT, 0, gl->fbo_tex_coords);

      // It's kinda handy ... :)
      const struct gl_fbo_rect *prev_rect;
      const struct gl_fbo_rect *rect;
      struct gl_tex_info *fbo_info;

      // Calculate viewports, texture coordinates etc, and render all passes from FBOs, to another FBO.
      for (int i = 1; i < gl->fbo_pass; i++)
      {
         prev_rect = &gl->fbo_rect[i - 1];
         rect = &gl->fbo_rect[i];
         fbo_info = &fbo_tex_info[i - 1];

         GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
         GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

         set_texture_coords(gl->fbo_tex_coords, xamt, yamt);

         fbo_info->tex = gl->fbo_texture[i - 1];
         fbo_info->input_size[0] = prev_rect->img_width;
         fbo_info->input_size[1] = prev_rect->img_height;
         fbo_info->tex_size[0] = prev_rect->width;
         fbo_info->tex_size[1] = prev_rect->height;
         memcpy(fbo_info->coord, gl->fbo_tex_coords, sizeof(gl->fbo_tex_coords));

         glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[i]);
         gl_shader_use(i + 1);
         glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

         glClear(GL_COLOR_BUFFER_BIT);

         // Render to FBO with certain size.
         set_viewport(gl, rect->img_width, rect->img_height, true);
         gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
               prev_rect->width, prev_rect->height, 
               gl->vp_width, gl->vp_height, g_frame_count, 
               &tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

         glDrawArrays(GL_QUADS, 0, 4);

         fbo_tex_info_cnt++;
      }

      // Render our last FBO texture directly to screen.
      prev_rect = &gl->fbo_rect[gl->fbo_pass - 1];
      GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
      GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

      set_texture_coords(gl->fbo_tex_coords, xamt, yamt);

      // Render our FBO texture to back buffer.
      glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
      gl_shader_use(gl->fbo_pass + 1);

      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

      glClear(GL_COLOR_BUFFER_BIT);
      gl->render_to_tex = false;
      set_viewport(gl, gl->win_width, gl->win_height, false);
      gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
            prev_rect->width, prev_rect->height, 
            gl->vp_width, gl->vp_height, g_frame_count, 
            &tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

      glVertexPointer(2, GL_FLOAT, 0, vertexes_flipped);
      glDrawArrays(GL_QUADS, 0, 4);

      glTexCoordPointer(2, GL_FLOAT, 0, gl->tex_coords);
   }

   memmove(gl->prev_info + 1, gl->prev_info, sizeof(tex_info) * (TEXTURES - 1));
   memcpy(&gl->prev_info[0], &tex_info, sizeof(tex_info));
   gl->tex_index = (gl->tex_index + 1) & TEXTURES_MASK;

   if (msg)
   {
      cellDbgFontPrintf(0.09f, 0.90f, 1.51f, BLUE,	msg);
      cellDbgFontPrintf(0.09f, 0.90f, 1.50f, WHITE, msg);
      cellDbgFontDraw();
   }

   psglSwap();
   return true;
}

static void psgl_deinit(gl_t *gl)
{
   glFinish();
   cellDbgFontExit();

   psglDestroyContext(gl->gl_context);
   psglDestroyDevice(gl->gl_device);

#if(CELL_SDK_VERSION > 0x340000)
   // FIXME: It will crash here for 1.92 - termination of the PSGL library - works fine for 3.41
   psglExit();
#else
   // For 1.92
   gl->min_width = 0;
   gl->min_height = 0;
   gl->gl_context = NULL;
   gl->gl_device = NULL;
#endif
}

static void gl_free(void *data)
{
   if (g_gl)
      return;

   gl_t *gl = data;

   gl_shader_deinit();
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);
   glDeleteTextures(TEXTURES, gl->texture);
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);

   if (gl->fbo_inited)
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      glDeleteFramebuffersOES(gl->fbo_pass, gl->fbo);
   }

   psgl_deinit(gl);

   if (gl->empty_buf)
      free(gl->empty_buf);

   free(gl);
}

static void gl_set_nonblock_state(void *data, bool state)
{
   gl_t *gl = data;
   if (gl->vsync)
   {
      SSNES_LOG("GL VSync => %s\n", state ? "off" : "on");
      state ? glDisable(GL_VSYNC_SCE) : glEnable(GL_VSYNC_SCE);
   }
}

static bool psgl_init_device(gl_t *gl, const video_info_t *video, uint32_t resolution_id)
{
   PSGLinitOptions options = {
      .enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS,
      .maxSPUs = 1,
      .initializeSPUs = GL_FALSE,
   };
#if CELL_SDK_VERSION > 0x340000
   options.enable |= PSGL_INIT_TRANSIENT_MEMORY_SIZE;
#else
   options.enable |=	PSGL_INIT_HOST_MEMORY_SIZE;
#endif
   
   psglInit(&options);
   
   PSGLdeviceParameters params = {
      .enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT |
         PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT |
         PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE |
         PSGL_DEVICE_PARAMETERS_BUFFERING_MODE,

      .colorFormat = GL_ARGB_SCE,
      .depthFormat = GL_NONE,
      .multisamplingMode = GL_MULTISAMPLING_NONE_SCE,
      .bufferingMode = PSGL_BUFFERING_MODE_TRIPLE,
   };
   
   gl->gl_device = psglCreateDeviceExtended(&params);
   psglGetDeviceDimensions(gl->gl_device, &gl->win_width, &gl->win_height); 
   gl->gl_context = psglCreateContext();
   psglMakeCurrent(gl->gl_context, gl->gl_device);
   psglResetCurrentContext();

   return true;
}

static void psgl_init_dbgfont(gl_t *gl)
{
   CellDbgFontConfig cfg;
   memset(&cfg, 0, sizeof(cfg));
   cfg.bufSize = 512;
   cfg.screenWidth = gl->win_width;
   cfg.screenHeight = gl->win_height;
   cellDbgFontInit(&cfg);
}

static void *gl_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (g_gl)
      return g_gl;

   gl_t *gl = calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

   if (!psgl_init_device(gl, video, 0))
      return NULL;

   SSNES_LOG("Detecting resolution %ux%u.\n", gl->win_width, gl->win_height);

   video->vsync ? glEnable(GL_VSYNC_SCE) : glDisable(GL_VSYNC_SCE);

   gl->vsync = video->vsync;
   
   SSNES_LOG("GL: Using resolution %ux%u\n", gl->win_width, gl->win_height);

   SSNES_LOG("GL: Initing debug fonts \n");
   psgl_init_dbgfont(gl);

   if (!gl_shader_init())
   {
      SSNES_ERR("Shader init failed.\n");
      psgl_deinit(gl);
      free(gl);
      return NULL;
   }

   SSNES_LOG("GL: Loaded %u program(s).\n", gl_shader_num());

   // Set up render to texture.
   gl_init_fbo(gl, SSNES_SCALE_BASE * video->input_scale,
         SSNES_SCALE_BASE * video->input_scale);

   
   gl->keep_aspect = video->force_aspect;

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_shader_use(0);
   set_viewport(gl, gl->win_width, gl->win_height, false);
   gl_shader_use(1);
   set_viewport(gl, gl->win_width, gl->win_height, false);

   bool force_smooth;
   if (gl_shader_filter_type(1, &force_smooth))
      gl->tex_filter = force_smooth ? GL_LINEAR : GL_NEAREST;
   else
      gl->tex_filter = video->smooth ? GL_LINEAR : GL_NEAREST;

   gl->texture_type = GL_BGRA;
   gl->texture_fmt = video->rgb32 ? GL_ARGB_SCE : GL_RGB5_A1;
   gl->base_size = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   glEnable(GL_TEXTURE_2D);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_DITHER);
   glClearColor(0, 0, 0, 1);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   gl->tex_w = SSNES_SCALE_BASE * video->input_scale;
   gl->tex_h = SSNES_SCALE_BASE * video->input_scale;
   glGenBuffers(1, &gl->pbo);
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->tex_w * gl->tex_h * gl->base_size * TEXTURES, NULL, GL_STREAM_DRAW);

   glGenTextures(TEXTURES, gl->texture);

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);
   }

   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glEnableClientState(GL_COLOR_ARRAY);
   glVertexPointer(2, GL_FLOAT, 0, vertexes_flipped);

   memcpy(gl->tex_coords, tex_coords, sizeof(tex_coords));
   glTexCoordPointer(2, GL_FLOAT, 0, gl->tex_coords);

   glColorPointer(4, GL_FLOAT, 0, white_color);

   set_lut_texture_coords(tex_coords);

   // Empty buffer that we use to clear out the texture with on res change.
   gl->empty_buf = calloc(gl->tex_w * gl->tex_h, gl->base_size);

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
      glTextureReferenceSCE(GL_TEXTURE_2D, 1,
            gl->tex_w, gl->tex_h, 0, 
            gl->texture_fmt,
            gl->tex_w * gl->base_size,
            gl->tex_w * gl->tex_h * i * gl->base_size);
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      gl->last_width[i] = gl->tex_w;
      gl->last_height[i] = gl->tex_h;
   }

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      gl->prev_info[i].tex = gl->texture[(gl->tex_index - (i + 1)) & TEXTURES_MASK];
      gl->prev_info[i].input_size[0] = gl->tex_w;
      gl->prev_info[i].tex_size[0] = gl->tex_w;
      gl->prev_info[i].input_size[1] = gl->tex_h;
      gl->prev_info[i].tex_size[1] = gl->tex_h;
      memcpy(gl->prev_info[i].coord, tex_coords, sizeof(tex_coords)); 
   }

   if (!gl_check_error())
   {
      psgl_deinit(gl);
      free(gl);
      return NULL;
   }

   if (input)
      *input = NULL;
   if (input_data)
      *input_data = NULL;

   return gl;
}

static bool gl_alive(void *data)
{
   gl_t *gl = data;
   cellSysutilCheckCallback();
   return !g_quitting;
}

static bool gl_focus(void *data)
{
   (void)data;
   return true;
}

const video_driver_t video_gl = {
   .init = gl_init,
   .frame = gl_frame,
   .alive = gl_alive,
   .set_nonblock_state = gl_set_nonblock_state,
   .focus = gl_focus,
   .free = gl_free,
   .ident = "gl"
};

// PS3 needs a working graphics stack before SSNES even starts.
// To deal with this main.c,
// the top level module owns the instance, and is created beforehand.
// When SSNES gets around to init it, it is already allocated.
// When SSNES wants to free it, it is ignored.
void ps3_video_init(void)
{
   video_info_t video_info = {0};
   // Might have to supply correct values here.
   video_info.vsync = true;
   video_info.force_aspect = false;
   video_info.smooth = true;
   video_info.input_scale = 2;
   g_gl = gl_init(&video_info, NULL, NULL);
}

void ps3_video_deinit(void)
{
   void *data = g_gl;
   g_gl = NULL;
   gl_free(data);
}


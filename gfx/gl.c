/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

/* Legacy FF OpenGL / OpenGL ES 1.1 driver */

#include "../driver.h"

#include <stdint.h>
#include "../libretro.h"
#include <stdio.h>
#include <string.h>
#include "../general.h"
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gl_common.h"
#include "gl_font.h"
#include "gfx_common.h"
#include "gfx_context.h"
#include "../compat/strl.h"

#ifdef HAVE_SDL
#define NO_SDL_GLEXT
#include "context/sdl_ctx.h"
#include "../input/rarch_sdl_input.h"
#endif

#ifdef HAVE_CG
#include "shader_cg.h"
#endif

#ifdef HAVE_XML
#include "shader_glsl.h"
#endif

extern const GLfloat vertexes_flipped[];
extern const GLfloat white_color[];

// Used for the last pass when rendering to the back buffer.
const GLfloat vertexes_flipped[] = {
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

const GLfloat white_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

const GLfloat *vertex_ptr = vertexes_flipped;
const GLfloat *default_vertex_ptr = vertexes_flipped;

#ifdef HAVE_SDL
#define LOAD_SYM(sym) if (!p##sym) { SDL_SYM_WRAP(p##sym, #sym) }
#endif

#ifdef HAVE_FBO
#if defined(_WIN32) && !defined(RARCH_CONSOLE)
static PFNGLGENFRAMEBUFFERSPROC pglGenFramebuffers = NULL;
static PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer = NULL;
static PFNGLFRAMEBUFFERTEXTURE2DPROC pglFramebufferTexture2D = NULL;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC pglCheckFramebufferStatus = NULL;
static PFNGLDELETEFRAMEBUFFERSPROC pglDeleteFramebuffers = NULL;

static bool load_fbo_proc(void)
{
   LOAD_SYM(glGenFramebuffers);
   LOAD_SYM(glBindFramebuffer);
   LOAD_SYM(glFramebufferTexture2D);
   LOAD_SYM(glCheckFramebufferStatus);
   LOAD_SYM(glDeleteFramebuffers);

   return pglGenFramebuffers && pglBindFramebuffer && pglFramebufferTexture2D && 
      pglCheckFramebufferStatus && pglDeleteFramebuffers;
}
#elif defined(HAVE_OPENGLES11)
#define pglGenFramebuffers glGenFramebuffersOES
#define pglBindFramebuffer glBindFramebufferOES
#define pglFramebufferTexture2D glFramebufferTexture2DOES
#define pglCheckFramebufferStatus glCheckFramebufferStatusOES
#define pglDeleteFramebuffers glDeleteFramebuffersOES
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define glOrtho glOrthof
static bool load_fbo_proc(void) { return true; }
#else
#define pglGenFramebuffers glGenFramebuffers
#define pglBindFramebuffer glBindFramebuffer
#define pglFramebufferTexture2D glFramebufferTexture2D
#define pglCheckFramebufferStatus glCheckFramebufferStatus
#define pglDeleteFramebuffers glDeleteFramebuffers
static bool load_fbo_proc(void) { return true; }
#endif
#endif

#if (defined(HAVE_XML) || defined(HAVE_CG)) && defined(_WIN32)
PFNGLCLIENTACTIVETEXTUREPROC pglClientActiveTexture = NULL;
PFNGLACTIVETEXTUREPROC pglActiveTexture = NULL;
static inline bool load_gl_proc(void)
{
   LOAD_SYM(glClientActiveTexture);
   LOAD_SYM(glActiveTexture);
   return pglClientActiveTexture && pglActiveTexture;
}
#else
static inline bool load_gl_proc(void) { return true; }
#endif

////////////////// Shaders
static bool gl_shader_init(void)
{
   switch (g_settings.video.shader_type)
   {
      case RARCH_SHADER_AUTO:
      {
         if (*g_settings.video.cg_shader_path && *g_settings.video.bsnes_shader_path)
            RARCH_WARN("Both Cg and bSNES XML shader are defined in config file. Cg shader will be selected by default.\n");

#ifdef HAVE_CG
         if (*g_settings.video.cg_shader_path)
            return gl_cg_init(g_settings.video.cg_shader_path);
#endif

#ifdef HAVE_XML
         if (*g_settings.video.bsnes_shader_path)
            return gl_glsl_init(g_settings.video.bsnes_shader_path);
#endif
         break;
      }

#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         return gl_cg_init(g_settings.video.cg_shader_path);
#endif

#ifdef HAVE_XML
      case RARCH_SHADER_BSNES:
         return gl_glsl_init(g_settings.video.bsnes_shader_path);
#endif

      default:
         break;
   }

   return true;
}

void gl_shader_use(unsigned index)
{
#ifdef HAVE_CG
   gl_cg_use(index);
#endif

#ifdef HAVE_XML
   gl_glsl_use(index);
#endif
}

static inline void gl_shader_deinit(void)
{
#ifdef HAVE_CG
   gl_cg_deinit();
#endif

#ifdef HAVE_XML
   gl_glsl_deinit();
#endif
}

static inline void gl_shader_set_proj_matrix(void)
{
#ifdef HAVE_CG
   gl_cg_set_proj_matrix();
#endif

#ifdef HAVE_XML
   gl_glsl_set_proj_matrix();
#endif
}

static inline void gl_shader_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info,
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info, unsigned fbo_info_cnt)
{
#ifdef HAVE_CG
   gl_cg_set_params(width, height, 
         tex_width, tex_height, 
         out_width, out_height, 
         frame_count, info, prev_info, fbo_info, fbo_info_cnt);
#endif

#ifdef HAVE_XML
   gl_glsl_set_params(width, height, 
         tex_width, tex_height, 
         out_width, out_height, 
         frame_count, info, prev_info, fbo_info, fbo_info_cnt);
#endif
}

static unsigned gl_shader_num(void)
{
#ifdef HAVE_CG
   unsigned cg_num = gl_cg_num();
   if (cg_num)
      return cg_num;
#endif

#ifdef HAVE_XML
   unsigned glsl_num = gl_glsl_num();
   if (glsl_num)
      return glsl_num;
#endif

   return 0;
}

static bool gl_shader_filter_type(unsigned index, bool *smooth)
{
   bool valid = false;

#ifdef HAVE_CG
   if (!valid)
      valid = gl_cg_filter_type(index, smooth);
#endif

#ifdef HAVE_XML
   if (!valid)
      valid = gl_glsl_filter_type(index, smooth);
#endif

   return valid;
}

#ifdef HAVE_FBO
static void gl_shader_scale(unsigned index, struct gl_fbo_scale *scale)
{
   scale->valid = false;

#ifdef HAVE_CG
   if (!scale->valid)
      gl_cg_shader_scale(index, scale);
#endif

#ifdef HAVE_XML
   if (!scale->valid)
      gl_glsl_shader_scale(index, scale);
#endif
}
#endif
///////////////////


#ifdef HAVE_FBO
static void gl_compute_fbo_geometry(gl_t *gl, unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height);

static void gl_create_fbo_textures(gl_t *gl)
{
   glGenTextures(gl->fbo_pass, gl->fbo_texture);

   GLuint base_filt = g_settings.video.second_pass_smooth ? GL_LINEAR : GL_NEAREST;
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

      GLuint filter_type = base_filt;
      bool smooth = false;
      if (gl_shader_filter_type(i + 2, &smooth))
         filter_type = smooth ? GL_LINEAR : GL_NEAREST;

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_type);

      glTexImage2D(GL_TEXTURE_2D,
            0, RARCH_GL_INTERNAL_FORMAT, gl->fbo_rect[i].width, gl->fbo_rect[i].height,
            0, RARCH_GL_TEXTURE_TYPE,
            RARCH_GL_FORMAT32, NULL);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

static bool gl_create_fbo_targets(gl_t *gl)
{
   pglGenFramebuffers(gl->fbo_pass, gl->fbo);
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
      pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   return true;

error:
   pglDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   RARCH_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
   return false;
}

void gl_deinit_fbo(gl_t *gl)
{
   if (gl->fbo_inited)
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      pglDeleteFramebuffers(gl->fbo_pass, gl->fbo);
      memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
      memset(gl->fbo, 0, sizeof(gl->fbo));
      gl->fbo_inited = false;
      gl->render_to_tex = false;
      gl->fbo_pass = 0;
   }
}

void gl_init_fbo(gl_t *gl, unsigned width, unsigned height)
{
   // No need to use FBOs.
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
      RARCH_ERR("Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
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
      scale.type_x  = scale.type_y = RARCH_SCALE_INPUT;
      scale.valid   = true;
   }

   gl->fbo_scale[0] = scale;

   for (int i = 1; i < gl->fbo_pass; i++)
   {
      gl_shader_scale(i + 1, &gl->fbo_scale[i]);

      if (!gl->fbo_scale[i].valid)
      {
         gl->fbo_scale[i].scale_x = gl->fbo_scale[i].scale_y = 1.0f;
         gl->fbo_scale[i].type_x  = gl->fbo_scale[i].type_y  = RARCH_SCALE_INPUT;
         gl->fbo_scale[i].valid   = true;
      }
   }

   gl_compute_fbo_geometry(gl, width, height, gl->win_width, gl->win_height);

   for (int i = 0; i < gl->fbo_pass; i++)
   {
      gl->fbo_rect[i].width  = next_pow2(gl->fbo_rect[i].img_width);
      gl->fbo_rect[i].height = next_pow2(gl->fbo_rect[i].img_height);
      RARCH_LOG("Creating FBO %d @ %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   gl_create_fbo_textures(gl);
   if (!gl_create_fbo_targets(gl))
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      return;
   }

   gl->fbo_inited = true;
}
#endif

////////////

void gl_set_projection(gl_t *gl, struct gl_ortho *ortho, bool allow_rotate)
{
#ifdef RARCH_CONSOLE
   if (g_console.overscan_enable)
   {
      ortho->left = -g_console.overscan_amount / 2;
      ortho->right = 1 + g_console.overscan_amount / 2;
      ortho->bottom = -g_console.overscan_amount / 2;
   }
#endif

   gfx_ctx_set_projection(gl, ortho, allow_rotate);
   gl_shader_set_proj_matrix();
}

void gl_set_viewport(gl_t *gl, unsigned width, unsigned height, bool force_full, bool allow_rotate)
{
   unsigned x = 0, y = 0;
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)width / height;
      float delta;

#ifdef RARCH_CONSOLE
      if (g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
      {
         x      = g_console.viewports.custom_vp.x;
         y      = g_console.viewports.custom_vp.y;
         width  = g_console.viewports.custom_vp.width;
         height = g_console.viewports.custom_vp.height;
      }
      else
#endif
      {
         if (fabs(device_aspect - desired_aspect) < 0.0001)
         {
            // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
            // assume they are actually equal.
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
            x     = (unsigned)(width * (0.5 - delta));
            width = (unsigned)(2.0 * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
            y      = (unsigned)(height * (0.5 - delta));
            height = (unsigned)(2.0 * height * delta);
         }
      }
   }

   glViewport(x, y, width, height);

   gl_set_projection(gl, &ortho, allow_rotate);

   gl->vp_width  = width;
   gl->vp_height = height;

   // Set last backbuffer viewport.
   if (!force_full)
   {
      gl->vp_out_width  = width;
      gl->vp_out_height = height;
   }

   //RARCH_LOG("Setting viewport @ %ux%u\n", width, height);
}

static void gl_set_rotation(void *data, unsigned rotation)
{
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};

   gl_t *gl = (gl_t*)driver.video_data;
   gl->rotation = 90 * rotation;
   gl_set_projection(gl, &ortho, true);
}

static inline void set_lut_texture_coords(const GLfloat *coords)
{
#if defined(HAVE_XML) || defined(HAVE_CG)
   // For texture images.
   pglClientActiveTexture(GL_TEXTURE1);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 0, coords);
   pglClientActiveTexture(GL_TEXTURE0);
#else
   (void)coords;
#endif
}

static inline void set_texture_coords(GLfloat *coords, GLfloat xamt, GLfloat yamt)
{
   coords[1] = yamt;
   coords[4] = xamt;
   coords[6] = xamt;
   coords[7] = yamt;
}

#ifdef HAVE_FBO
static void gl_compute_fbo_geometry(gl_t *gl, unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
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
         case RARCH_SCALE_INPUT:
            gl->fbo_rect[i].img_width = last_width * gl->fbo_scale[i].scale_x;
            gl->fbo_rect[i].max_img_width = last_max_width * gl->fbo_scale[i].scale_x;
            break;

         case RARCH_SCALE_ABSOLUTE:
            gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].abs_x;
            break;

         case RARCH_SCALE_VIEWPORT:
            gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].scale_x * vp_width;
            break;

         default:
            break;
      }

      switch (gl->fbo_scale[i].type_y)
      {
         case RARCH_SCALE_INPUT:
            gl->fbo_rect[i].img_height = last_height * gl->fbo_scale[i].scale_y;
            gl->fbo_rect[i].max_img_height = last_max_height * gl->fbo_scale[i].scale_y;
            break;

         case RARCH_SCALE_ABSOLUTE:
            gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].abs_y;
            break;

         case RARCH_SCALE_VIEWPORT:
            gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].scale_y * vp_height;
            break;

         default:
            break;
      }

      last_width = gl->fbo_rect[i].img_width;
      last_height = gl->fbo_rect[i].img_height;
      last_max_width = gl->fbo_rect[i].max_img_width;
      last_max_height = gl->fbo_rect[i].max_img_height;
   }
}

static inline void gl_start_frame_fbo(gl_t *gl)
{
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[0]);
   gl->render_to_tex = true;
   gl_set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true, false);

   // Need to preserve the "flipped" state when in FBO as well to have 
   // consistent texture coordinates.
   // We will "flip" it in place on last pass.
   if (gl->render_to_tex)
      glVertexPointer(2, GL_FLOAT, 0, vertexes);
}

static void gl_check_fbo_dimensions(gl_t *gl)
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

         pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
         glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);
         glTexImage2D(GL_TEXTURE_2D,
               0, RARCH_GL_INTERNAL_FORMAT, gl->fbo_rect[i].width, gl->fbo_rect[i].height,
               0, RARCH_GL_TEXTURE_TYPE,
               RARCH_GL_FORMAT32, NULL);

         pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

         GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
         if (status != GL_FRAMEBUFFER_COMPLETE)
            RARCH_WARN("Failed to reinit FBO texture.\n");

         RARCH_LOG("Recreating FBO texture #%d: %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
      }
   }
}

static void gl_frame_fbo(gl_t *gl, const struct gl_tex_info *tex_info)
{
   GLfloat fbo_tex_coords[8] = {0.0f};

   // Render the rest of our passes.
   glTexCoordPointer(2, GL_FLOAT, 0, fbo_tex_coords);

   // It's kinda handy ... :)
   const struct gl_fbo_rect *prev_rect;
   const struct gl_fbo_rect *rect;
   struct gl_tex_info *fbo_info;

   struct gl_tex_info fbo_tex_info[MAX_SHADERS];
   unsigned fbo_tex_info_cnt = 0;

   // Calculate viewports, texture coordinates etc, and render all passes from FBOs, to another FBO.
   for (int i = 1; i < gl->fbo_pass; i++)
   {
      prev_rect = &gl->fbo_rect[i - 1];
      rect = &gl->fbo_rect[i];
      fbo_info = &fbo_tex_info[i - 1];

      GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
      GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

      set_texture_coords(fbo_tex_coords, xamt, yamt);

      fbo_info->tex = gl->fbo_texture[i - 1];
      fbo_info->input_size[0] = prev_rect->img_width;
      fbo_info->input_size[1] = prev_rect->img_height;
      fbo_info->tex_size[0] = prev_rect->width;
      fbo_info->tex_size[1] = prev_rect->height;
      memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));

      pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
      gl_shader_use(i + 1);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

      glClear(GL_COLOR_BUFFER_BIT);

      // Render to FBO with certain size.
      gl_set_viewport(gl, rect->img_width, rect->img_height, true, false);
      gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
            prev_rect->width, prev_rect->height, 
            gl->vp_width, gl->vp_height, gl->frame_count, 
            tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

      glDrawArrays(GL_QUADS, 0, 4);

      fbo_tex_info_cnt++;
   }

   // Render our last FBO texture directly to screen.
   prev_rect = &gl->fbo_rect[gl->fbo_pass - 1];
   GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
   GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

   set_texture_coords(fbo_tex_coords, xamt, yamt);

   // Render our FBO texture to back buffer.
   pglBindFramebuffer(GL_FRAMEBUFFER, 0);
   gl_shader_use(gl->fbo_pass + 1);

   glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

   glClear(GL_COLOR_BUFFER_BIT);
   gl->render_to_tex = false;
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
         prev_rect->width, prev_rect->height, 
         gl->vp_width, gl->vp_height, gl->frame_count, 
         tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

   glVertexPointer(2, GL_FLOAT, 0, vertex_ptr);
   glDrawArrays(GL_QUADS, 0, 4);

   glTexCoordPointer(2, GL_FLOAT, 0, gl->tex_coords);
}
#endif

static void gl_update_resize(gl_t *gl)
{
#ifdef HAVE_FBO
   if (!gl->render_to_tex)
      gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   else
   {
      gl_check_fbo_dimensions(gl);

      // Go back to what we're supposed to do, render to FBO #0 :D
      gl_start_frame_fbo(gl);
   }
#else
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
#endif
}

static void gl_update_input_size(gl_t *gl, unsigned width, unsigned height, unsigned pitch)
{
   // Res change. Need to clear out texture.
   if ((width != gl->last_width[gl->tex_index] || height != gl->last_height[gl->tex_index]) && gl->empty_buf)
   {
      gl->last_width[gl->tex_index] = width;
      gl->last_height[gl->tex_index] = height;

#ifdef HAVE_OPENGL_TEXREF
      glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
		      gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size,
		      gl->tex_w * gl->tex_h * gl->base_size,
		      gl->empty_buf);
#else
      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(pitch));
      glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->tex_w);

      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, gl->tex_w, gl->tex_h, gl->texture_type,
            gl->texture_fmt, gl->empty_buf);
#endif

      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;

      set_texture_coords(gl->tex_coords, xamt, yamt);
   }
   // We might have used different texture coordinates last frame. Edge case if resolution changes very rapidly.
   else if (width != gl->last_width[(gl->tex_index - 1) & TEXTURES_MASK] ||
         height != gl->last_height[(gl->tex_index - 1) & TEXTURES_MASK])
   {
      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;
      set_texture_coords(gl->tex_coords, xamt, yamt);
   }
}

#ifdef __CELLOS_LV2__
static inline void gl_copy_frame(gl_t *gl, const void *frame, unsigned width, unsigned height, unsigned pitch)
{
   if (!gl->fbo_inited)
      gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);

   size_t buffer_addr        = gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size;
   size_t buffer_stride      = gl->tex_w * gl->base_size;
   const uint8_t *frame_copy = frame;
   size_t frame_copy_size    = width * gl->base_size;

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

static void gl_init_textures(gl_t *gl)
{
   glGenTextures(TEXTURES, gl->texture);

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);

      glTextureReferenceSCE(GL_TEXTURE_2D, 1,
            gl->tex_w, gl->tex_h, 0, 
            gl->texture_fmt,
            gl->tex_w * gl->base_size,
            gl->tex_w * gl->tex_h * i * gl->base_size);
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}
#else
static inline void gl_copy_frame(gl_t *gl, const void *frame, unsigned width, unsigned height, unsigned pitch)
{
   glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
   glTexSubImage2D(GL_TEXTURE_2D,
         0, 0, 0, width, height, gl->texture_type,
         gl->texture_fmt, frame);
}

static void gl_init_textures(gl_t *gl)
{
   glGenTextures(TEXTURES, gl->texture);
   for (unsigned i = 0; i < TEXTURES; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);

      glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->tex_w);
      glTexImage2D(GL_TEXTURE_2D,
            0, RARCH_GL_INTERNAL_FORMAT, gl->tex_w, gl->tex_h, 0, gl->texture_type,
            gl->texture_fmt, gl->empty_buf ? gl->empty_buf : NULL);
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}
#endif

static inline void gl_next_texture_index(gl_t *gl, const struct gl_tex_info *tex_info)
{
   memmove(gl->prev_info + 1, gl->prev_info, sizeof(*tex_info) * (TEXTURES - 1));
   memcpy(&gl->prev_info[0], tex_info, sizeof(*tex_info));
   gl->tex_index = (gl->tex_index + 1) & TEXTURES_MASK;
}

#ifdef HAVE_CG_MENU
static void gl_render_menu(gl_t *gl)
{
   gl_shader_use(RARCH_CG_MENU_SHADER_INDEX);

   gl_shader_set_params(gl->win_width, gl->win_height, gl->win_width, 
         gl->win_height, gl->win_width, gl->win_height, gl->frame_count,
         NULL, NULL, NULL, 0);

   gl_set_viewport(gl, gl->win_width, gl->win_height, true, false);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, gl->menu_texture_id);

   glVertexPointer(2, GL_FLOAT, 0, default_vertex_ptr);
   glDrawArrays(GL_QUADS, 0, 4); 
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}
#endif

static bool gl_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   gl_t *gl = (gl_t*)data;

   gl_shader_use(1);
   gl->frame_count++;

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

#ifdef HAVE_FBO
   // Render to texture in first pass.
   if (gl->fbo_inited)
   {
      // Recompute FBO geometry.
      // When width/height changes or window sizes change, we have to recalcuate geometry of our FBO.
      gl_compute_fbo_geometry(gl, width, height, gl->vp_out_width, gl->vp_out_height);
      gl_start_frame_fbo(gl);
   }
#endif

   if (gl->should_resize)
   {
      gl->should_resize = false;
      gfx_ctx_set_resize(gl->win_width, gl->win_height);

      // On resize, we might have to recreate our FBOs due to "Viewport" scale, and set a new viewport.
      gl_update_resize(gl);
   }

   if (frame) // Can be NULL for frame dupe / NULL render.
   {
      gl_update_input_size(gl, width, height, pitch);
      gl_copy_frame(gl, frame, width, height, pitch);
   }

   struct gl_tex_info tex_info = {0};
   tex_info.tex           = gl->texture[gl->tex_index];
   tex_info.input_size[0] = width;
   tex_info.input_size[1] = height;
   tex_info.tex_size[0]   = gl->tex_w;
   tex_info.tex_size[1]   = gl->tex_h;

   memcpy(tex_info.coord, gl->tex_coords, sizeof(gl->tex_coords));

   glClear(GL_COLOR_BUFFER_BIT);
   gl_shader_set_params(width, height,
         gl->tex_w, gl->tex_h,
         gl->vp_width, gl->vp_height,
         gl->frame_count, 
         &tex_info, gl->prev_info, NULL, 0);

   glDrawArrays(GL_QUADS, 0, 4);

#ifdef HAVE_FBO
   if (gl->fbo_inited)
      gl_frame_fbo(gl, &tex_info);
#endif

   gl_next_texture_index(gl, &tex_info);

   if (msg)
      gl_render_msg(gl, msg);

#ifndef RARCH_CONSOLE
   gfx_ctx_update_window_title(false);
#endif

#ifdef RARCH_CONSOLE
   if (!gl->block_swap)
#endif
      gfx_ctx_swap_buffers();

#ifdef HAVE_CG_MENU
   if (gl->menu_render)
      gl_render_menu(gl);
#endif

   return true;
}

static void gl_free(void *data)
{
#ifdef RARCH_CONSOLE
   if (driver.video_data)
      return;
#endif

   gl_t *gl = (gl_t*)data;

   gl_deinit_font(gl);
   gl_shader_deinit();
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);
   glDeleteTextures(TEXTURES, gl->texture);

#ifdef HAVE_OPENGL_TEXREF
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);
#endif

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
#endif

   gfx_ctx_destroy();

   if (gl->empty_buf)
      free(gl->empty_buf);

   free(gl);
}

static void gl_set_nonblock_state(void *data, bool state)
{
   gl_t *gl = (gl_t*)data;
   if (gl->vsync)
   {
      RARCH_LOG("GL VSync => %s\n", state ? "off" : "on");
      gfx_ctx_set_swap_interval(state ? 0 : 1, true);
   }
}

static void *gl_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
#ifdef _WIN32
   gfx_set_dwm();
#endif

#ifdef RARCH_CONSOLE
   if (driver.video_data)
      return driver.video_data;
#endif

   gl_t *gl = (gl_t*)calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

   if (!gfx_ctx_init())
   {
      free(gl);
      return NULL;
   }

   unsigned full_x = 0, full_y = 0;
   gfx_ctx_get_video_size(&full_x, &full_y);
   RARCH_LOG("Detecting resolution %ux%u.\n", full_x, full_y);

   gfx_ctx_set_swap_interval(video->vsync ? 1 : 0, false);

   unsigned win_width = video->width;
   unsigned win_height = video->height;
   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width = full_x;
      win_height = full_y;
   }

   if (!gfx_ctx_set_video_mode(win_width, win_height,
            g_settings.video.force_16bit ? 15 : 0, video->fullscreen))
   {
      free(gl);
      return NULL;
   }

#ifndef RARCH_CONSOLE
   gfx_ctx_update_window_title(true);

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

#if (defined(HAVE_XML) || defined(HAVE_CG)) && defined(_WIN32)
   // Win32 GL lib doesn't have some functions needed for XML shaders.
   // Need to load dynamically :(
   if (!load_gl_proc())
   {
      gfx_ctx_destroy();
      free(gl);
      return NULL;
   }
#endif

   gl->vsync = video->vsync;
   gl->fullscreen = video->fullscreen;
   
   gl->full_x = full_x;
   gl->full_y = full_y;
   gl->win_width = win_width;
   gl->win_height = win_height;

   RARCH_LOG("GL: Using resolution %ux%u\n", gl->win_width, gl->win_height);

#if defined(HAVE_CG_MENU) && defined(RARCH_CONSOLE)
   RARCH_LOG("Initializing menu shader ...\n");
   gl_cg_set_menu_shader(default_paths.menu_shader_file);
#endif

   if (!gl_shader_init())
   {
      RARCH_ERR("Shader init failed.\n");
      gfx_ctx_destroy();
      free(gl);
      return NULL;
   }

   RARCH_LOG("GL: Loaded %u program(s).\n", gl_shader_num());

#ifdef HAVE_FBO
   // Set up render to texture.
   gl_init_fbo(gl, RARCH_SCALE_BASE * video->input_scale,
         RARCH_SCALE_BASE * video->input_scale);
#endif
   
   gl->keep_aspect = video->force_aspect;

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_shader_use(0);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   gl_shader_use(1);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);

   bool force_smooth = false;
   if (gl_shader_filter_type(1, &force_smooth))
      gl->tex_filter = force_smooth ? GL_LINEAR : GL_NEAREST;
   else
      gl->tex_filter = video->smooth ? GL_LINEAR : GL_NEAREST;

   gl->texture_type = RARCH_GL_TEXTURE_TYPE;
   gl->texture_fmt = video->rgb32 ? RARCH_GL_FORMAT32 : RARCH_GL_FORMAT16;
   gl->base_size = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   glEnable(GL_TEXTURE_2D);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_DITHER);
   glClearColor(0, 0, 0, 1);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glEnableClientState(GL_COLOR_ARRAY);
   glVertexPointer(2, GL_FLOAT, 0, vertex_ptr);

   memcpy(gl->tex_coords, tex_coords, sizeof(tex_coords));
   glTexCoordPointer(2, GL_FLOAT, 0, gl->tex_coords);
   glColorPointer(4, GL_FLOAT, 0, white_color);

   set_lut_texture_coords(tex_coords);

   gl->tex_w = RARCH_SCALE_BASE * video->input_scale;
   gl->tex_h = RARCH_SCALE_BASE * video->input_scale;

#ifdef HAVE_OPENGL_TEXREF
   glGenBuffers(1, &gl->pbo);
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
         gl->tex_w * gl->tex_h * gl->base_size * TEXTURES, NULL, GL_STREAM_DRAW);
#endif

   // Empty buffer that we use to clear out the texture with on res change.
   gl->empty_buf = calloc(gl->tex_w * gl->tex_h, gl->base_size);
   gl_init_textures(gl);

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

   gfx_ctx_input_driver(input, input_data);
   gl_init_font(gl, g_settings.video.font_path, g_settings.video.font_size);
      
   if (!gl_check_error())
   {
      gfx_ctx_destroy();
      free(gl);
      return NULL;
   }

   return gl;
}

static bool gl_alive(void *data)
{
   gl_t *gl = (gl_t*)data;
   bool quit, resize;

   gfx_ctx_check_window(&quit,
         &resize, &gl->win_width, &gl->win_height,
         gl->frame_count);

   if (quit)
      gl->quitting = true;
   else if (resize)
      gl->should_resize = true;

   return !gl->quitting;
}

static bool gl_focus(void *data)
{
   (void)data;
   return gfx_ctx_window_has_focus();
}

#ifdef HAVE_XML
static bool gl_xml_shader(void *data, const char *path)
{
   gl_t *gl = (gl_t*)data;

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
#endif

   gl_shader_deinit();

   if (!gl_glsl_init(path))
      return false;

#ifdef HAVE_FBO
   // Set up render to texture again.
   gl_init_fbo(gl, gl->tex_w, gl->tex_h);
#endif

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_shader_use(0);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   gl_shader_use(1);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);

   return true;
}
#endif

#ifndef HAVE_RGL
static void gl_viewport_size(void *data, unsigned *width, unsigned *height)
{
   (void)data;

   GLint vp[4];
   glGetIntegerv(GL_VIEWPORT, vp);

   *width  = vp[2];
   *height = vp[3];
}

static bool gl_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;

   GLint vp[4];
   glGetIntegerv(GL_VIEWPORT, vp);

   glPixelStorei(GL_PACK_ALIGNMENT, get_alignment(vp[2]));
   glPixelStorei(GL_PACK_ROW_LENGTH, vp[2]);

   glReadPixels(vp[0], vp[1],
         vp[2], vp[3],
         GL_BGR, GL_UNSIGNED_BYTE, buffer);

   return true;
}
#endif

#ifdef RARCH_CONSOLE
static void gl_start(void)
{
   video_info_t video_info = {0};

   // Might have to supply correct values here.
   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;
   video_info.fullscreen = true;
   if (g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
   {
      video_info.width = g_console.viewports.custom_vp.width;
      video_info.height = g_console.viewports.custom_vp.height;
   }
   driver.video_data = gl_init(&video_info, NULL, NULL);

#ifdef HAVE_FBO
   gfx_ctx_set_fbo(g_console.fbo_enabled);
#endif

   gfx_ctx_get_available_resolutions();

#ifdef HAVE_CG_MENU
   gfx_ctx_menu_init();
#endif
}

static void gl_stop(void)
{
   void *data = driver.video_data;
   driver.video_data = NULL;
   gl_free(data);
}

static void gl_restart(void)
{
#ifdef HAVE_CG_MENU
   bool should_menu_render;
#endif
#ifdef RARCH_CONSOLE
   bool should_block_swap;
#endif
   gl_t *gl = driver.video_data;

   if (!gl)
	   return;

#ifdef RARCH_CONSOLE
   should_block_swap = gl->block_swap;
#endif
#ifdef HAVE_CG_MENU
   should_menu_render = gl->menu_render;
#endif

   gl_stop();
   gl_cg_invalidate_context();
   gl_start();

#ifdef HAVE_CG_MENU
   gl->menu_render = should_menu_render;
#endif

   gl->frame_count = 0;

#ifdef RARCH_CONSOLE
   gl->block_swap = should_block_swap;
   SET_TIMER_EXPIRATION(gl, 30);
#endif
}
#endif

const video_driver_t video_gl = {
   gl_init,
   gl_frame,
   gl_set_nonblock_state,
   gl_alive,
   gl_focus,

#ifdef HAVE_XML
   gl_xml_shader,
#else
   NULL,
#endif

   gl_free,
   "gl",

#ifdef RARCH_CONSOLE
   gl_start,
   gl_stop,
   gl_restart,
#endif

   gl_set_rotation,

#ifndef HAVE_RGL
   gl_viewport_size,
   gl_read_viewport,
#else
   NULL,
   NULL,
#endif
};


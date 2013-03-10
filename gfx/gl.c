/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "../driver.h"
#include "../performance.h"
#include "scaler/scaler.h"
#include "image.h"

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
#include "gfx_common.h"
#include "gfx_context.h"
#include "../compat/strl.h"

#ifdef HAVE_CG
#include "shader_cg.h"
#endif

#ifdef HAVE_GLSL
#include "shader_glsl.h"
#endif

#include "shader_common.h"

#ifdef __CELLOS_LV2__
#define FPS_COUNTER
#endif

#ifdef ANDROID
#include "../frontend/frontend_android.h"
#endif

#ifdef HAVE_RGUI
#include "../frontend/menu/rgui.h"
#endif

// Used for the last pass when rendering to the back buffer.
const GLfloat vertexes_flipped[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

// Used when rendering to an FBO.
// Texture coords have to be aligned with vertex coordinates.
static const GLfloat vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat tex_coords[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

#ifdef HAVE_OVERLAY
static void gl_render_overlay(void *data);
static void gl_overlay_vertex_geom(void *data,
      float x, float y, float w, float h);
static void gl_overlay_tex_geom(void *data,
      float x, float y, float w, float h);
#endif

static inline void set_texture_coords(GLfloat *coords, GLfloat xamt, GLfloat yamt)
{
   coords[2] = xamt;
   coords[6] = xamt;
   coords[5] = yamt;
   coords[7] = yamt;
}

const GLfloat white_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

const GLfloat *vertex_ptr = vertexes_flipped;
const GLfloat *default_vertex_ptr = vertexes_flipped;

#undef LOAD_GL_SYM
#define LOAD_GL_SYM(SYM) if (!pgl##SYM) { \
   gfx_ctx_proc_t sym = gl->ctx_driver->get_proc_address("gl" #SYM); \
   memcpy(&(pgl##SYM), &sym, sizeof(sym)); \
}

#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC pglEGLImageTargetTexture2DOES;

static bool load_eglimage_proc(gl_t *gl)
{
   LOAD_GL_SYM(EGLImageTargetTexture2DOES);
   return pglEGLImageTargetTexture2DOES;
}
#endif

#ifdef HAVE_FBO
#if defined(_WIN32) && !defined(RARCH_CONSOLE)
static PFNGLGENFRAMEBUFFERSPROC pglGenFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC pglFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC pglCheckFramebufferStatus;
static PFNGLDELETEFRAMEBUFFERSPROC pglDeleteFramebuffers;

static bool load_fbo_proc(gl_t *gl)
{
   LOAD_GL_SYM(GenFramebuffers);
   LOAD_GL_SYM(BindFramebuffer);
   LOAD_GL_SYM(FramebufferTexture2D);
   LOAD_GL_SYM(CheckFramebufferStatus);
   LOAD_GL_SYM(DeleteFramebuffers);

   return pglGenFramebuffers && pglBindFramebuffer && pglFramebufferTexture2D && 
      pglCheckFramebufferStatus && pglDeleteFramebuffers;
}
#elif defined(HAVE_OPENGLES2)
#define pglGenFramebuffers glGenFramebuffers
#define pglBindFramebuffer glBindFramebuffer
#define pglFramebufferTexture2D glFramebufferTexture2D
#define pglCheckFramebufferStatus glCheckFramebufferStatus
#define pglDeleteFramebuffers glDeleteFramebuffers
#define load_fbo_proc(gl) (true)
#elif defined(HAVE_OPENGLES)
#define pglGenFramebuffers glGenFramebuffersOES
#define pglBindFramebuffer glBindFramebufferOES
#define pglFramebufferTexture2D glFramebufferTexture2DOES
#define pglCheckFramebufferStatus glCheckFramebufferStatusOES
#define pglDeleteFramebuffers glDeleteFramebuffersOES
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define load_fbo_proc(gl) (true)
#else
#define pglGenFramebuffers glGenFramebuffers
#define pglBindFramebuffer glBindFramebuffer
#define pglFramebufferTexture2D glFramebufferTexture2D
#define pglCheckFramebufferStatus glCheckFramebufferStatus
#define pglDeleteFramebuffers glDeleteFramebuffers
#define load_fbo_proc(gl) (true)
#endif
#endif

#ifdef _WIN32
PFNGLCLIENTACTIVETEXTUREPROC pglClientActiveTexture;
PFNGLACTIVETEXTUREPROC pglActiveTexture;
static PFNGLGENBUFFERSPROC pglGenBuffers;
static PFNGLGENBUFFERSPROC pglDeleteBuffers;
static PFNGLBINDBUFFERPROC pglBindBuffer;
static PFNGLBUFFERSUBDATAPROC pglBufferSubData;
static PFNGLBUFFERDATAPROC pglBufferData;
static PFNGLMAPBUFFERPROC pglMapBuffer;
static PFNGLUNMAPBUFFERPROC pglUnmapBuffer;
static inline bool load_gl_proc_win32(gl_t *gl)
{
   LOAD_GL_SYM(ClientActiveTexture);
   LOAD_GL_SYM(ActiveTexture);
   LOAD_GL_SYM(GenBuffers);
   LOAD_GL_SYM(DeleteBuffers);
   LOAD_GL_SYM(BindBuffer);
   LOAD_GL_SYM(BufferSubData);
   LOAD_GL_SYM(BufferData);
   LOAD_GL_SYM(MapBuffer);
   LOAD_GL_SYM(UnmapBuffer);

   return pglClientActiveTexture && pglActiveTexture &&
      pglGenBuffers && pglDeleteBuffers &&
      pglBindBuffer && pglBufferSubData && pglBufferData &&
      pglMapBuffer && pglUnmapBuffer;
}
#else
#define pglGenBuffers glGenBuffers
#define pglDeleteBuffers glDeleteBuffers
#define pglBindBuffer glBindBuffer
#define pglBufferSubData glBufferSubData
#define pglBufferData glBufferData
#define pglMapBuffer glMapBuffer
#define pglUnmapBuffer glUnmapBuffer
#endif

////////////////// Shaders

#ifdef HAVE_OPENGLES2
static bool gl_shader_init(void *data) // We always need a shader alive in GLES2.
{
   gl_t *gl = (gl_t*)data;
   const char *shader_path = NULL;
   if ((g_settings.video.shader_type == RARCH_SHADER_AUTO || g_settings.video.shader_type == RARCH_SHADER_GLSL)
         && *g_settings.video.bsnes_shader_path)
      shader_path = g_settings.video.bsnes_shader_path;

   gl->shader = &gl_glsl_backend;
   return gl->shader->init(shader_path);
}
#else
static bool gl_shader_init(void *data)
{
   gl_t *gl = (gl_t*)data;
   const gl_shader_backend_t *backend = NULL;
   const char *shader_path            = NULL;

   switch (g_settings.video.shader_type)
   {
      case RARCH_SHADER_AUTO:
      {
         if (*g_settings.video.cg_shader_path && *g_settings.video.bsnes_shader_path)
            RARCH_WARN("Both Cg and GLSL XML shader are defined in config file. Cg shader will be selected by default.\n");

#ifdef HAVE_CG
         if (*g_settings.video.cg_shader_path)
         {
            backend     = &gl_cg_backend;
            shader_path = g_settings.video.cg_shader_path;
         }
#endif

#ifdef HAVE_GLSL
         if (*g_settings.video.bsnes_shader_path)
         {
            backend     = &gl_glsl_backend;
            shader_path = g_settings.video.bsnes_shader_path;
         }
#endif
         break;
      }

#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         backend     = &gl_cg_backend;
         shader_path = g_settings.video.cg_shader_path;
         break;
#endif

#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         backend     = &gl_glsl_backend;
         shader_path = g_settings.video.bsnes_shader_path;
         break;
#endif

      default:
         break;
   }

   if (!backend)
      return true;

   gl->shader = backend;
   return gl->shader->init(shader_path);
}
#endif

void gl_shader_use(void *data, unsigned index)
{
   gl_t *gl = (gl_t*)data;

   if (gl->shader)
      gl->shader->use(index);
}

static inline void gl_shader_deinit(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (gl->shader)
      gl->shader->deinit();
   gl->shader = NULL;
}

#ifndef NO_GL_FF_VERTEX
static void gl_set_coords(const struct gl_coords *coords)
{
   pglClientActiveTexture(GL_TEXTURE0);

   glVertexPointer(2, GL_FLOAT, 0, coords->vertex);
   glEnableClientState(GL_VERTEX_ARRAY);

   glColorPointer(4, GL_FLOAT, 0, coords->color);
   glEnableClientState(GL_COLOR_ARRAY);

   glTexCoordPointer(2, GL_FLOAT, 0, coords->tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   pglClientActiveTexture(GL_TEXTURE1);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->lut_tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   pglClientActiveTexture(GL_TEXTURE0);
}
#endif

#ifndef NO_GL_FF_MATRIX
static void gl_set_mvp(const void *data)
{
   const math_matrix *mat = (const math_matrix*)data;
   glMatrixMode(GL_PROJECTION);
   glLoadMatrixf(mat->data);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}
#endif

void gl_shader_set_coords(void *data, const struct gl_coords *coords, const math_matrix *mat)
{
   gl_t *gl = (gl_t*)data;

   bool ret_coords = false;
   bool ret_mvp    = false;

   (void)ret_coords;
   (void)ret_mvp;

   if (gl->shader)
      ret_coords = gl->shader->set_coords(coords);
   if (gl->shader)
      ret_mvp = gl->shader->set_mvp(mat);

   // Fall back to FF-style if needed and possible.
#ifndef NO_GL_FF_VERTEX
   if (!ret_coords)
      gl_set_coords(coords);
#endif

#ifndef NO_GL_FF_MATRIX
   if (!ret_mvp)
      gl_set_mvp(mat);
#endif
}

static inline void gl_shader_set_params(void *data, unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info,
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info, unsigned fbo_info_cnt)
{
   gl_t *gl = (gl_t*)data;

   if (gl->shader)
   {
      gl->shader->set_params(width, height, 
            tex_width, tex_height, 
            out_width, out_height, 
            frame_count, info, prev_info, fbo_info, fbo_info_cnt);
   }
}

static inline unsigned gl_shader_num(void *data)
{
   gl_t *gl = (gl_t*)data;
   
   if (gl->shader)
      return gl->shader->num_shaders();
   else
      return 0;
}

static bool gl_shader_filter_type(void *data, unsigned index, bool *smooth)
{
   gl_t *gl = (gl_t*)data;

   if (gl->shader)
      return gl->shader->filter_type(index, smooth);
   else
      return false;
}

#ifdef HAVE_FBO
static void gl_shader_scale(void *data, unsigned index, struct gl_fbo_scale *scale)
{
   gl_t *gl = (gl_t*)data;

   scale->valid = false;
   if (gl->shader)
      gl->shader->shader_scale(index, scale);
}

static void gl_compute_fbo_geometry(void *data, unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
   gl_t *gl = (gl_t*)data;
   unsigned last_width = width;
   unsigned last_height = height;
   unsigned last_max_width = gl->tex_w;
   unsigned last_max_height = gl->tex_h;

   GLint max_size = 0;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
   bool size_modified = false;

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

      if (gl->fbo_rect[i].img_width > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].img_width = max_size;
      }

      if (gl->fbo_rect[i].img_height > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].img_height = max_size;
      }

      if (gl->fbo_rect[i].max_img_width > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].max_img_width = max_size;
      }

      if (gl->fbo_rect[i].max_img_height > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].max_img_height = max_size;
      }

      if (size_modified)
         RARCH_WARN("FBO textures exceeded maximum size of GPU (%ux%u). Resizing to fit.\n", max_size, max_size);

      last_width = gl->fbo_rect[i].img_width;
      last_height = gl->fbo_rect[i].img_height;
      last_max_width = gl->fbo_rect[i].max_img_width;
      last_max_height = gl->fbo_rect[i].max_img_height;
   }
}

static void gl_create_fbo_textures(void *data)
{
   gl_t *gl = (gl_t*)data;

   glGenTextures(gl->fbo_pass, gl->fbo_texture);

   GLuint base_filt = g_settings.video.second_pass_smooth ? GL_LINEAR : GL_NEAREST;
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);

      GLuint filter_type = base_filt;
      bool smooth = false;
      if (gl_shader_filter_type(gl, i + 2, &smooth))
         filter_type = smooth ? GL_LINEAR : GL_NEAREST;

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_type);

      glTexImage2D(GL_TEXTURE_2D,
            0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32, gl->fbo_rect[i].width, gl->fbo_rect[i].height,
            0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
            RARCH_GL_FORMAT32, NULL);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

static bool gl_create_fbo_targets(void *data)
{
   gl_t *gl = (gl_t*)data;

   glBindTexture(GL_TEXTURE_2D, 0);
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

void gl_deinit_fbo(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (gl->fbo_inited)
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      pglDeleteFramebuffers(gl->fbo_pass, gl->fbo);
      memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
      memset(gl->fbo, 0, sizeof(gl->fbo));
      gl->fbo_inited = false;
      gl->fbo_pass = 0;
   }
}

static void gl_update_tex_filter_frame(gl_t *gl)
{
   bool smooth = false;
   if (!gl_shader_filter_type(gl, 1, &smooth))
      return;

   GLuint new_filt = smooth ? GL_LINEAR : GL_NEAREST;
   if (new_filt == gl->tex_filter)
      return;

   gl->tex_filter = new_filt;
   for (unsigned i = 0; i < TEXTURES; i++)
   {
      if (gl->texture[i])
      {
         glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);
      }
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

void gl_init_fbo(void *data, unsigned width, unsigned height)
{
   gl_t *gl = (gl_t*)data;

   gl_update_tex_filter_frame(gl);

   // No need to use FBOs.
#ifndef RARCH_CONSOLE
   /* we always want FBO to be at least initialized on startup for consoles */
   if (!g_settings.video.render_to_texture && gl_shader_num_func(gl) == 0)
      return;
#endif

   struct gl_fbo_scale scale, scale_last;
   gl_shader_scale(gl, 1, &scale);
   gl_shader_scale(gl, gl_shader_num_func(gl), &scale_last);

   // No need to use FBOs.
#ifndef RARCH_CONSOLE
   /* we always want FBO to be at least initialized on startup for consoles */
   if (gl_shader_num_func(gl) == 1 && !scale.valid && !g_settings.video.render_to_texture)
      return;
#endif

   if (!load_fbo_proc(gl))
   {
      RARCH_ERR("Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   gl->fbo_pass = gl_shader_num_func(gl) - 1;
   if (scale_last.valid)
      gl->fbo_pass++;

   if (gl->fbo_pass <= 0)
      gl->fbo_pass = 1;

   if (!scale.valid)
   {
      scale.scale_x = g_settings.video.fbo.scale_x;
      scale.scale_y = g_settings.video.fbo.scale_y;
      scale.type_x  = scale.type_y = RARCH_SCALE_INPUT;
      scale.valid   = true;
   }

   gl->fbo_scale[0] = scale;

   for (int i = 1; i < gl->fbo_pass; i++)
   {
      gl_shader_scale(gl, i + 1, &gl->fbo_scale[i]);

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
      RARCH_ERR("Failed to create FBO targets. Will continue without FBO.\n");
      return;
   }

   gl->fbo_inited = true;
}
#endif

void gl_set_projection(void *data, struct gl_ortho *ortho, bool allow_rotate)
{
   gl_t *gl = (gl_t*)data;
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_OVERSCAN_ENABLE))
   {
      ortho->left = -g_extern.console.screen.overscan_amount / 2;
      ortho->right = 1 + g_extern.console.screen.overscan_amount / 2;
      ortho->bottom = -g_extern.console.screen.overscan_amount / 2;
   }

   // Calculate projection.
   matrix_ortho(&gl->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (allow_rotate)
   {
      math_matrix rot;
      matrix_rotate_z(&rot, M_PI * gl->rotation / 180.0f);
      matrix_multiply(&gl->mvp, &rot, &gl->mvp_no_rot);
   }
   else
      gl->mvp = gl->mvp_no_rot;

   gl_shader_set_coords_func(gl, &gl->coords, &gl->mvp);
}

void gl_set_viewport(void *data, unsigned width, unsigned height, bool force_full, bool allow_rotate)
{
   gl_t *gl = (gl_t*)data;

   unsigned x = 0, y = 0;
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};

   float device_aspect = 0.0f;
   if (gl->ctx_driver->translate_aspect)
      device_aspect = context_translate_aspect_func(width, height);
   else
      device_aspect = (float)width / height;

   if (g_settings.video.scale_integer && !force_full)
   {
      gfx_scale_integer(&gl->vp, width, height, g_settings.video.aspect_ratio, gl->keep_aspect);
   }
   else if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float delta;

#ifdef HAVE_RGUI
      if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         x      = g_extern.console.screen.viewports.custom_vp.x;
         y      = g_extern.console.screen.viewports.custom_vp.y;
         width  = g_extern.console.screen.viewports.custom_vp.width;
         height = g_extern.console.screen.viewports.custom_vp.height;
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

      gl->vp.x      = x;
      gl->vp.y      = y;
      gl->vp.width  = width;
      gl->vp.height = height;
   }
   else
   {
      gl->vp.x = gl->vp.y = 0;
      gl->vp.width = width;
      gl->vp.height = height;
   }

#ifdef ANDROID
   // In portrait mode, we want viewport to gravitate to top of screen.
   if (device_aspect < 1.0f)
      gl->vp.y *= 2;
#endif

   glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   gl_set_projection(gl, &ortho, allow_rotate);

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

#ifdef HAVE_FBO

static inline void gl_start_frame_fbo(void *data)
{
   gl_t *gl = (gl_t*)data;

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[0]);
   gl_set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true, false);

   // Need to preserve the "flipped" state when in FBO as well to have 
   // consistent texture coordinates.
   // We will "flip" it in place on last pass.
   gl->coords.vertex = vertexes;
}

static void gl_check_fbo_dimensions(void *data)
{
   gl_t *gl = (gl_t*)data;

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
               0, RARCH_GL_INTERNAL_FORMAT32, gl->fbo_rect[i].width, gl->fbo_rect[i].height,
               0, RARCH_GL_TEXTURE_TYPE32,
               RARCH_GL_FORMAT32, NULL);

         pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

         GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
         if (status != GL_FRAMEBUFFER_COMPLETE)
            RARCH_WARN("Failed to reinit FBO texture.\n");

         RARCH_LOG("Recreating FBO texture #%d: %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
      }
   }
}

static void gl_frame_fbo(void *data, const struct gl_tex_info *tex_info)
{
   gl_t *gl = (gl_t*)data;
   GLfloat fbo_tex_coords[8] = {0.0f};

   // Render the rest of our passes.
   gl->coords.tex_coord = fbo_tex_coords;

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
      gl_shader_use_func(gl, i + 1);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

      glClear(GL_COLOR_BUFFER_BIT);

      // Render to FBO with certain size.
      gl_set_viewport(gl, rect->img_width, rect->img_height, true, false);
      gl_shader_set_params_func(gl, prev_rect->img_width, prev_rect->img_height, 
            prev_rect->width, prev_rect->height, 
            gl->vp.width, gl->vp.height, g_extern.frame_count, 
            tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

      gl_shader_set_coords_func(gl, &gl->coords, &gl->mvp);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      fbo_tex_info_cnt++;
   }

   // Render our last FBO texture directly to screen.
   prev_rect = &gl->fbo_rect[gl->fbo_pass - 1];
   GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
   GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

   set_texture_coords(fbo_tex_coords, xamt, yamt);

   // Render our FBO texture to back buffer.
   pglBindFramebuffer(GL_FRAMEBUFFER, 0);
   gl_shader_use_func(gl, gl->fbo_pass + 1);

   glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

   glClear(GL_COLOR_BUFFER_BIT);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   gl_shader_set_params_func(gl, prev_rect->img_width, prev_rect->img_height, 
         prev_rect->width, prev_rect->height, 
         gl->vp.width, gl->vp.height, g_extern.frame_count, 
         tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

   gl->coords.vertex = vertex_ptr;

   gl_shader_set_coords_func(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_coords;
}
#endif

static void gl_update_resize(void *data)
{
   gl_t *gl = (gl_t*)data;
#ifdef HAVE_FBO
   if (!gl->fbo_inited)
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

static void gl_update_input_size(void *data, unsigned width, unsigned height, unsigned pitch)
{
   gl_t *gl = (gl_t*)data;
   // Res change. Need to clear out texture.
   if ((width != gl->last_width[gl->tex_index] || height != gl->last_height[gl->tex_index]) && gl->empty_buf)
   {
      gl->last_width[gl->tex_index] = width;
      gl->last_height[gl->tex_index] = height;

#if defined(HAVE_PSGL)
      glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
		      gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size,
		      gl->tex_w * gl->tex_h * gl->base_size,
		      gl->empty_buf);
#else
      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(width * sizeof(uint32_t)));

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

// It is *much* faster (order of mangnitude on my setup) to use a custom SIMD-optimized conversion routine than letting GL do it :(
#if !defined(HAVE_PSGL) && !defined(HAVE_OPENGLES2)
static inline void gl_convert_frame_rgb16_32(void *data, void *output, const void *input, int width, int height, int in_pitch)
{
   gl_t *gl = (gl_t*)data;
   if (width != gl->scaler.in_width || height != gl->scaler.in_height)
   {
      gl->scaler.in_width    = width;
      gl->scaler.in_height   = height;
      gl->scaler.out_width   = width;
      gl->scaler.out_height  = height;
      gl->scaler.in_fmt      = SCALER_FMT_RGB565;
      gl->scaler.out_fmt     = SCALER_FMT_ARGB8888;
      gl->scaler.scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(&gl->scaler);
   }

   gl->scaler.in_stride  = in_pitch;
   gl->scaler.out_stride = width * sizeof(uint32_t);
   scaler_ctx_scale(&gl->scaler, output, input);
}
#endif

#ifdef HAVE_OPENGLES2
static inline void gl_convert_frame_argb8888_abgr8888(void *data, void *output, const void *input,
      int width, int height, int in_pitch)
{
   gl_t *gl = (gl_t*)data;
   if (width != gl->scaler.in_width || height != gl->scaler.in_height)
   {
      gl->scaler.in_width    = width;
      gl->scaler.in_height   = height;
      gl->scaler.out_width   = width;
      gl->scaler.out_height  = height;
      gl->scaler.in_fmt      = SCALER_FMT_ARGB8888;
      gl->scaler.out_fmt     = SCALER_FMT_ABGR8888;
      gl->scaler.scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(&gl->scaler);
   }

   gl->scaler.in_stride  = in_pitch;
   gl->scaler.out_stride = width * sizeof(uint32_t);
   scaler_ctx_scale(&gl->scaler, output, input);
}
#endif

static void gl_init_textures_data(void *data)
{
   gl_t *gl = (gl_t*)data;
   for (unsigned i = 0; i < TEXTURES; i++)
   {
      gl->last_width[i]  = gl->tex_w;
      gl->last_height[i] = gl->tex_h;
   }

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      gl->prev_info[i].tex           = gl->texture[0];
      gl->prev_info[i].input_size[0] = gl->tex_w;
      gl->prev_info[i].tex_size[0]   = gl->tex_w;
      gl->prev_info[i].input_size[1] = gl->tex_h;
      gl->prev_info[i].tex_size[1]   = gl->tex_h;
      memcpy(gl->prev_info[i].coord, tex_coords, sizeof(tex_coords)); 
   }
}

#if defined(HAVE_PSGL)
static inline void gl_copy_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch)
{
      gl_t *gl = (gl_t*)data;
   size_t buffer_addr        = gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size;
   size_t buffer_stride      = gl->tex_w * gl->base_size;
   const uint8_t *frame_copy = frame;
   size_t frame_copy_size    = width * gl->base_size;

   uint8_t *buffer = (uint8_t*)glMapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, GL_READ_WRITE) + buffer_addr;
   for (unsigned h = 0; h < height; h++, buffer += buffer_stride, frame_copy += pitch)
      memcpy(buffer, frame_copy, frame_copy_size);

   glUnmapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE);
}

static void gl_init_textures(void *data, const video_info_t *video)
{
   gl_t *gl = (gl_t*)data;
   
   if (!gl->pbo)
      glGenBuffers(1, &gl->pbo);

   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
         gl->tex_w * gl->tex_h * gl->base_size * TEXTURES, NULL, GL_STREAM_DRAW);

   glGenTextures(TEXTURES, gl->texture);

#ifdef HAVE_RGUI
   glGenTextures(1, &gl->rgui_texture);
   glBindTexture(GL_TEXTURE_2D, gl->rgui_texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);

      glTextureReferenceSCE(GL_TEXTURE_2D, 1,
            gl->tex_w, gl->tex_h, 0, 
            gl->internal_fmt,
            gl->tex_w * gl->base_size,
            gl->tex_w * gl->tex_h * i * gl->base_size);
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}
#else
static inline void gl_copy_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch)
{
   gl_t *gl = (gl_t*)data;
#ifdef HAVE_OPENGLES2
#ifdef HAVE_EGL
   if (gl->egl_images)
   {
      EGLImageKHR img = 0;
      bool new_egl = context_write_egl_image_func(frame, width, height, pitch, (gl->base_size == 4), gl->tex_index, &img);

      if (img == EGL_NO_IMAGE_KHR)
      {
         RARCH_ERR("[GL]: Failed to create EGL image.\n");
         return;
      }

      if (new_egl)
         pglEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
   }
   else
#endif
   {
      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(width * gl->base_size));

      // Fallback for GLES devices without GL_BGRA_EXT.
      if (gl->base_size == 4 && driver.gfx_use_rgba)
      {
         gl_convert_frame_argb8888_abgr8888(gl, gl->conv_buffer, frame, width, height, pitch);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, gl->conv_buffer);
      }
      else
      {
         // No GL_UNPACK_ROW_LENGTH ;(
         unsigned pitch_width = pitch / gl->base_size;
         if (width == pitch_width) // Happy path :D
         {
            glTexSubImage2D(GL_TEXTURE_2D,
                  0, 0, 0, width, height, gl->texture_type,
                  gl->texture_fmt, frame);
         }
         else // Slower path.
         {
            const unsigned line_bytes = width * gl->base_size;

            uint8_t *dst = (uint8_t*)gl->conv_buffer; // This buffer is preallocated for this purpose.
            const uint8_t *src = (const uint8_t*)frame;

            for (unsigned h = 0; h < height; h++, src += pitch, dst += line_bytes)
               memcpy(dst, src, line_bytes);

            glTexSubImage2D(GL_TEXTURE_2D,
                  0, 0, 0, width, height, gl->texture_type,
                  gl->texture_fmt, gl->conv_buffer);         
         }
      }
   }
#else
   glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(pitch));
   if (gl->base_size == 2)
   {
      // Always use 32-bit textures on desktop GL.
      gl_convert_frame_rgb16_32(gl, gl->conv_buffer, frame, width, height, pitch);
      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, gl->texture_type,
            gl->texture_fmt, gl->conv_buffer);
   }
   else
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, gl->texture_type,
            gl->texture_fmt, frame);

      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   }
#endif
}

static void gl_init_textures(void *data, const video_info_t *video)
{
   gl_t *gl = (gl_t*)data;
#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
   gl->egl_images = load_eglimage_proc(gl) && context_init_egl_image_buffer_func(video);
#else
   (void)video;
#endif

   glGenTextures(TEXTURES, gl->texture);

#ifdef HAVE_RGUI
   glGenTextures(1, &gl->rgui_texture);
   glBindTexture(GL_TEXTURE_2D, gl->rgui_texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);

      if (!gl->egl_images)
      {
         glTexImage2D(GL_TEXTURE_2D,
               0, gl->internal_fmt, gl->tex_w, gl->tex_h, 0, gl->texture_type,
               gl->texture_fmt, gl->empty_buf ? gl->empty_buf : NULL);
      }
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}
#endif

static inline void gl_set_prev_texture(void *data, const struct gl_tex_info *tex_info)
{
   gl_t *gl = (gl_t*)data;
   memmove(gl->prev_info + 1, gl->prev_info, sizeof(*tex_info) * (TEXTURES - 1));
   memcpy(&gl->prev_info[0], tex_info, sizeof(*tex_info));
}

static inline void gl_set_shader_viewport(void *data, unsigned shader)
{
   gl_t *gl = (gl_t*)data;
   gl_shader_use_func(gl, shader);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
}

#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
static void gl_pbo_async_readback(void *data)
{
   gl_t *gl = (gl_t*)data;
   pglBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[gl->pbo_readback_index++]);
   gl->pbo_readback_index &= 3;

   // If set, we 3 rendered frames already buffered up.
   gl->pbo_readback_valid |= gl->pbo_readback_index == 0;

   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glPixelStorei(GL_PACK_ALIGNMENT, get_alignment(gl->vp.width * sizeof(uint32_t)));

   // Read asynchronously into PBO buffer.
   RARCH_PERFORMANCE_INIT(async_readback);
   RARCH_PERFORMANCE_START(async_readback);
   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
   RARCH_PERFORMANCE_STOP(async_readback);

   pglBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}
#endif

#ifdef HAVE_RGUI
static inline void gl_draw_rgui(void *data)
{
   gl_t *gl = (gl_t*)data;
   gl->coords.tex_coord = tex_coords;

   glBindTexture(GL_TEXTURE_2D, gl->rgui_texture);

   glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(RGUI_WIDTH * 2));
   // RGUI is always packed so pitch = width * bpp
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGBA, RGUI_WIDTH, RGUI_HEIGHT, 0, GL_RGBA,
         GL_UNSIGNED_SHORT_4_4_4_4, gl->menu_data);

   gl_shader_use_func(gl, 0);
   gl_shader_set_coords_func(gl, &gl->coords, &gl->mvp_no_rot);

   glEnable(GL_BLEND);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);

   gl->coords.tex_coord = gl->tex_coords;
}
#endif

static bool gl_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   RARCH_PERFORMANCE_INIT(frame_run);
   RARCH_PERFORMANCE_START(frame_run);

   gl_t *gl = (gl_t*)data;
   uint64_t lifecycle_mode_state = g_extern.lifecycle_mode_state;
   (void)lifecycle_mode_state;

   gl_shader_use_func(gl, 1);

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
      context_set_resize_func(gl->win_width, gl->win_height);

      // On resize, we might have to recreate our FBOs due to "Viewport" scale, and set a new viewport.
      gl_update_resize(gl);
   }

   if (frame) // Can be NULL for frame dupe / NULL render.
   {
      gl->tex_index = (gl->tex_index + 1) & TEXTURES_MASK;
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

      gl_update_input_size(gl, width, height, pitch);

      RARCH_PERFORMANCE_INIT(copy_frame);
      RARCH_PERFORMANCE_START(copy_frame);
      gl_copy_frame(gl, frame, width, height, pitch);
      RARCH_PERFORMANCE_STOP(copy_frame);
   }
   else
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   struct gl_tex_info tex_info = {0};
   tex_info.tex           = gl->texture[gl->tex_index];
   tex_info.input_size[0] = width;
   tex_info.input_size[1] = height;
   tex_info.tex_size[0]   = gl->tex_w;
   tex_info.tex_size[1]   = gl->tex_h;

   memcpy(tex_info.coord, gl->tex_coords, sizeof(gl->tex_coords));

   glClear(GL_COLOR_BUFFER_BIT);
   gl_shader_set_params_func(gl, width, height,
         gl->tex_w, gl->tex_h,
         gl->vp.width, gl->vp.height,
         g_extern.frame_count, 
         &tex_info, gl->prev_info, NULL, 0);

   gl_shader_set_coords_func(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#ifdef HAVE_FBO
   if (gl->fbo_inited)
      gl_frame_fbo(gl, &tex_info);
#endif

   gl_set_prev_texture(gl, &tex_info);

#ifdef HAVE_RGUI
   if (lifecycle_mode_state & (1ULL << MODE_MENU_DRAW))
      gl_draw_rgui(gl);
#endif

#ifdef FPS_COUNTER
   if (lifecycle_mode_state & (1ULL << MODE_FPS_DRAW))
   {
      char fps_txt[128];
      gfx_get_fps(fps_txt, sizeof(fps_txt), true);

      if (gl->font_ctx)
         gl->font_ctx->render_msg_place(gl, g_settings.video.msg_pos_x, 0.56f, 1.04f, WHITE, fps_txt);
   }
#endif

   if (msg && gl->font_ctx)
      gl->font_ctx->render_msg(gl, msg);

   if (gl->ctx_driver->post_render)
      context_post_render_func(gl);
#ifdef HAVE_OVERLAY
   else if (gl->overlay_enable)
      gl_render_overlay(gl);
#endif

#if !defined(RARCH_CONSOLE)
   context_update_window_title_func(false);
#endif

   RARCH_PERFORMANCE_STOP(frame_run);

#if defined(HAVE_RMENU)
   if (lifecycle_mode_state & (1ULL << MODE_MENU_DRAW))
      context_rmenu_frame_func(gl);
   else
#endif
      context_swap_buffers_func();

#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
   if (gl->pbo_readback_enable)
      gl_pbo_async_readback(gl);
#endif
 
   return true;
}

#ifndef NO_GL_FF_VERTEX
static void gl_disable_client_arrays(void)
{
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);
}
#endif

static void gl_free(void *data)
{
#ifdef RARCH_CONSOLE
   if (driver.video_data)
      return;
#endif

   gl_t *gl = (gl_t*)data;

#ifdef HAVE_RMENU
   if (gl->ctx_driver->rmenu_free)
      context_rmenu_free_func();
#endif

   if (gl->font_ctx)
      gl->font_ctx->deinit(gl);
   gl_shader_deinit(gl);

#ifndef NO_GL_FF_VERTEX
   gl_disable_client_arrays();
#endif

   glDeleteTextures(TEXTURES, gl->texture);

#ifdef HAVE_RGUI
   glDeleteTextures(1, &gl->rgui_texture);
#endif

#ifdef HAVE_OVERLAY
   if (gl->tex_overlay)
      glDeleteTextures(1, &gl->tex_overlay);
#endif

#if defined(HAVE_PSGL)
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);
#endif

   scaler_ctx_gen_reset(&gl->scaler);

#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
   if (gl->pbo_readback_enable)
   {
      pglDeleteBuffers(4, gl->pbo_readback);
      scaler_ctx_gen_reset(&gl->pbo_readback_scaler);
   }
#endif

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
#endif

   context_destroy_func();

   free(gl->empty_buf);
   free(gl->conv_buffer);

   free(gl);
}

static void gl_set_nonblock_state(void *data, bool state)
{
   RARCH_LOG("GL VSync => %s\n", state ? "off" : "on");

   gl_t *gl = (gl_t*)data;
   (void)gl;
   context_swap_interval_func(state ? 0 : 1);
}

static bool resolve_extensions(gl_t *gl)
{
#ifdef _WIN32
   // Win32 GL lib doesn't have some elementary functions needed.
   // Need to load dynamically :(
   if (!load_gl_proc_win32(gl))
      return false;
#endif

#ifdef NO_GL_CLAMP_TO_BORDER
   // NOTE: This will be a serious problem for some shaders.
   gl->border_type = GL_CLAMP_TO_EDGE;
#else
   gl->border_type = GL_CLAMP_TO_BORDER;
#endif

   driver.gfx_use_rgba = false;
#ifdef HAVE_OPENGLES2
   if (gl_query_extension("BGRA8888"))
      RARCH_LOG("[GL]: BGRA8888 extension found for GLES.\n");
   else
   {
      driver.gfx_use_rgba = true;
      RARCH_WARN("[GL]: GLES implementation does not have BGRA8888 extension.\n"
                 "32-bit path will require conversion.\n");
   }
#endif

#if 0
   // Useful for debugging, but kinda obnoxious.
   const char *ext = (const char*)glGetString(GL_EXTENSIONS);
   if (ext)
      RARCH_LOG("[GL] Supported extensions: %s\n", ext);
#endif

   return true;
}

static inline void gl_set_texture_fmts(void *data, bool rgb32)
{
   gl_t *gl = (gl_t*)data;

   gl->internal_fmt = rgb32 ? RARCH_GL_INTERNAL_FORMAT32 : RARCH_GL_INTERNAL_FORMAT16;
   gl->texture_type = rgb32 ? RARCH_GL_TEXTURE_TYPE32 : RARCH_GL_TEXTURE_TYPE16;
   gl->texture_fmt  = rgb32 ? RARCH_GL_FORMAT32 : RARCH_GL_FORMAT16;
   gl->base_size    = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   if (driver.gfx_use_rgba && rgb32)
   {
      gl->internal_fmt = GL_RGBA;
      gl->texture_type = GL_RGBA;
   }
}

static inline void gl_reinit_textures(void *data, const video_info_t *video)
{
   gl_t *gl = (gl_t*)data;
   unsigned old_base_size = gl->base_size;
   unsigned old_width     = gl->tex_w;
   unsigned old_height    = gl->tex_h;

   gl_set_texture_fmts(gl, video->rgb32);
   gl->tex_w = gl->tex_h = RARCH_SCALE_BASE * video->input_scale;

   gl->empty_buf = realloc(gl->empty_buf, sizeof(uint32_t) * gl->tex_w * gl->tex_h);
   if (gl->empty_buf)
      memset(gl->empty_buf, 0, sizeof(uint32_t) * gl->tex_w * gl->tex_h);

   if (old_base_size != gl->base_size || old_width != gl->tex_w || old_height != gl->tex_h)
   {
      RARCH_LOG("Reinitializing textures (%u x %u @ %u bpp)\n", gl->tex_w, gl->tex_h, gl->base_size * CHAR_BIT);

#if defined(HAVE_PSGL)
      glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
      glDeleteBuffers(1, &gl->pbo);
      gl->pbo = 0;
#endif

      glBindTexture(GL_TEXTURE_2D, 0);
      glDeleteTextures(TEXTURES, gl->texture);

#ifdef HAVE_RGUI
      glDeleteTextures(1, &gl->rgui_texture);
#endif

      gl_init_textures(gl, video);
      gl_init_textures_data(gl);

#ifdef HAVE_FBO
      if (gl->tex_w > old_width || gl->tex_h > old_height)
      {
         RARCH_LOG("Reiniting FBO.\n");
         gl_deinit_fbo(gl);
         gl_init_fbo(gl, gl->tex_w, gl->tex_h);
      }
#endif
   }
   else
      RARCH_LOG("Reinitializing textures skipped.\n");

   if (!gl_check_error())
      RARCH_ERR("GL error reported while reinitializing textures. This should not happen ...\n");
}

static void gl_init_pbo_readback(void *data)
{
   gl_t *gl = (gl_t*)data;
#if !defined(HAVE_OPENGLES) && defined(HAVE_FFMPEG)
   // Only bother with this if we're doing FFmpeg GPU recording.
   gl->pbo_readback_enable = g_settings.video.gpu_record && g_extern.recording;
   if (!gl->pbo_readback_enable)
      return;

   RARCH_LOG("Async PBO readback enabled.\n");

   pglGenBuffers(4, gl->pbo_readback);
   for (unsigned i = 0; i < 4; i++)
   {
      pglBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[i]);
      pglBufferData(GL_PIXEL_PACK_BUFFER, gl->vp.width * gl->vp.height * sizeof(uint32_t),
            NULL, GL_STREAM_READ);
   }
   pglBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

   struct scaler_ctx *scaler = &gl->pbo_readback_scaler;
   scaler->in_width    = gl->vp.width;
   scaler->in_height   = gl->vp.height;
   scaler->out_width   = gl->vp.width;
   scaler->out_height  = gl->vp.height;
   scaler->in_stride   = gl->vp.width * sizeof(uint32_t);
   scaler->out_stride  = gl->vp.width * 3;
   scaler->in_fmt      = SCALER_FMT_ARGB8888;
   scaler->out_fmt     = SCALER_FMT_BGR24;
   scaler->scaler_type = SCALER_TYPE_POINT;

   if (!scaler_ctx_gen_filter(scaler))
   {
      gl->pbo_readback_enable = false;
      RARCH_ERR("Failed to init pixel conversion for PBO.\n");
      pglDeleteBuffers(4, gl->pbo_readback);
   }

#else
   (void)gl;
#endif
}

static const gfx_ctx_driver_t *gl_get_context(void)
{
#ifdef HAVE_OPENGLES
   enum gfx_ctx_api api = GFX_CTX_OPENGL_ES_API;
   const char *api_name = "OpenGL ES";
#else
   enum gfx_ctx_api api = GFX_CTX_OPENGL_API;
   const char *api_name = "OpenGL";
#endif

   if (*g_settings.video.gl_context)
   {
      const gfx_ctx_driver_t *ctx = gfx_ctx_find_driver(g_settings.video.gl_context);
      if (ctx)
      {
         if (!ctx->bind_api(api))
         {
            RARCH_ERR("Failed to bind API %s to context %s.\n", api_name, g_settings.video.gl_context);
            return NULL;
         }

         if (!ctx->init())
         {
            RARCH_ERR("Failed to init GL context: %s.\n", ctx->ident);
            return NULL;
         }
      }
      else
      {
         RARCH_ERR("Didn't find GL context: %s.\n", g_settings.video.gl_context);
         return NULL;
      }

      return ctx;
   }
   else
      return gfx_ctx_init_first(api);
}

static void *gl_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
#ifdef _WIN32
   gfx_set_dwm();
#endif

#ifdef RARCH_CONSOLE
   if (driver.video_data)
   {
      gl_t *gl = (gl_t*)driver.video_data;
      // Reinitialize textures as we might have changed pixel formats.
      gl_reinit_textures(gl, video); 
      return driver.video_data;
   }
#endif

   gl_t *gl = (gl_t*)calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

   gl->ctx_driver = gl_get_context();
   if (!gl->ctx_driver)
   {
      free(gl);
      return NULL;
   }

   RARCH_LOG("Found GL context: %s\n", gl->ctx_driver->ident);

   context_get_video_size_func(&gl->full_x, &gl->full_y);
   RARCH_LOG("Detecting screen resolution %ux%u.\n", gl->full_x, gl->full_y);

   context_swap_interval_func(video->vsync ? 1 : 0);

   unsigned win_width  = video->width;
   unsigned win_height = video->height;
   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = gl->full_x;
      win_height = gl->full_y;
   }

   if (!context_set_video_mode_func(win_width, win_height, video->fullscreen))
   {
      free(gl);
      return NULL;
   }

#ifndef RARCH_CONSOLE
   context_update_window_title_func(true);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

   if (!resolve_extensions(gl))
   {
      context_destroy_func();
      free(gl);
      return NULL;
   }

   gl->vsync      = video->vsync;
   gl->fullscreen = video->fullscreen;
   
   // Get real known video size, which might have been altered by context.
   context_get_video_size_func(&gl->win_width, &gl->win_height);
   RARCH_LOG("GL: Using resolution %ux%u\n", gl->win_width, gl->win_height);

   if (gl->full_x || gl->full_y) // We got bogus from gfx_ctx_get_video_size. Replace.
   {
      gl->full_x = gl->win_width;
      gl->full_y = gl->win_height;
   }

#if defined(HAVE_RMENU) && defined(HAVE_CG)
   RARCH_LOG("Initializing CG menu shader ...\n");
   gl_cg_set_menu_shader(default_paths.menu_shader_file);
#endif

#ifdef HAVE_GLSL
   gl_glsl_set_get_proc_address(gl->ctx_driver->get_proc_address);
#endif

   if (!gl_shader_init(gl))
   {
      RARCH_ERR("Shader init failed.\n");
      context_destroy_func();
      free(gl);
      return NULL;
   }

   RARCH_LOG("GL: Loaded %u program(s).\n", gl_shader_num_func(gl));

   gl->tex_w = RARCH_SCALE_BASE * video->input_scale;
   gl->tex_h = RARCH_SCALE_BASE * video->input_scale;

#ifdef HAVE_FBO
   // Set up render to texture.
   gl_init_fbo(gl, gl->tex_w, gl->tex_h);
#endif

   gl->keep_aspect = video->force_aspect;

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);

   bool force_smooth = false;
   if (gl_shader_filter_type(gl, 1, &force_smooth))
      gl->tex_filter = force_smooth ? GL_LINEAR : GL_NEAREST;
   else
      gl->tex_filter = video->smooth ? GL_LINEAR : GL_NEAREST;

   gl_set_texture_fmts(gl, video->rgb32);

#ifndef HAVE_OPENGLES
   glEnable(GL_TEXTURE_2D);
#endif

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_DITHER);

   memcpy(gl->tex_coords, tex_coords, sizeof(tex_coords));
   gl->coords.vertex         = vertex_ptr;
   gl->coords.tex_coord      = gl->tex_coords;
   gl->coords.color          = white_color;
   gl->coords.lut_tex_coord  = tex_coords;
   gl_shader_set_coords_func(gl, &gl->coords, &gl->mvp);

   // Empty buffer that we use to clear out the texture with on res change.
   gl->empty_buf = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

#if !defined(HAVE_PSGL)
   gl->conv_buffer = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);
   if (!gl->conv_buffer)
   {
      context_destroy_func();
      free(gl);
      return NULL;
   }
#endif

   gl_init_textures(gl, video);
   gl_init_textures_data(gl);

   if (input && input_data)
      context_input_driver_func(input, input_data);
   
#if !defined(HAVE_RMENU)
   // Comes too early for console - moved to gl_start
   if (g_settings.video.font_enable)
      gl->font_ctx = gl_font_init_first(gl, g_settings.video.font_path, g_settings.video.font_size);
#endif

   gl_init_pbo_readback(gl);

   if (!gl_check_error())
   {
      context_destroy_func();
      free(gl);
      return NULL;
   }

   return gl;
}

static bool gl_alive(void *data)
{
   gl_t *gl = (gl_t*)data;
   bool quit, resize;

   context_check_window_func(&quit,
         &resize, &gl->win_width, &gl->win_height,
         g_extern.frame_count);

   if (quit)
      gl->quitting = true;
   else if (resize)
      gl->should_resize = true;

   return !gl->quitting;
}

static bool gl_focus(void *data)
{
   gl_t *gl = (gl_t*)data;
   (void)gl;
   return context_has_focus_func();
}

#if defined(HAVE_GLSL) || defined(HAVE_CG)
static bool gl_set_shader(void *data, enum rarch_shader_type type, const char *path, unsigned index)
{
   gl_t *gl = (gl_t*)data;

   if (type == RARCH_SHADER_NONE)
      return false;

   if (index == RARCH_SHADER_INDEX_MULTIPASS && !path)
   {
      RARCH_ERR("[GL]: Cannot set stock shader to multipass.\n");
      return false;
   }

   if (!gl->shader && index != RARCH_SHADER_INDEX_MULTIPASS)
   {
      RARCH_ERR("[GL]: No shader core is init. Cannot set shader %s to pass %u.\n", path, index);
      return false;
   }

   if (gl->shader && gl->shader->type != type)
   {
      RARCH_ERR("[GL]: Trying to set a specific shader pass %u, but that particular shader core is not initialized.\n", index);
      return false;
   }

   // Need full teardown for multipass.
   if (index == RARCH_SHADER_INDEX_MULTIPASS)
   {
      gl_shader_deinit(gl);

      switch (type)
      {
#ifdef HAVE_GLSL
         case RARCH_SHADER_GLSL:
            gl->shader = &gl_glsl_backend;
            break;
#endif
#ifdef HAVE_CG
         case RARCH_SHADER_CG:
            gl->shader = &gl_cg_backend;
            break;
#endif

         default:
            gl->shader = NULL;
            break;
      }

      if (!gl->shader)
      {
         RARCH_ERR("[GL]: Cannot find shader core for path: %s.\n", path);
         return false;
      }

#ifdef HAVE_FBO
      gl_deinit_fbo(gl);
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
#endif

      bool ret = gl->shader->init(path);
      if (!ret)
      {
         RARCH_WARN("[GL]: Failed to set multipass shader. Falling back to stock.\n");
         bool ret = gl->shader->init(NULL);
         if (!ret)
            gl->shader = NULL;
         return ret;
      }

#ifdef HAVE_FBO
      // Set up render to texture again.
      gl_init_fbo(gl, gl->tex_w, gl->tex_h);
#endif

      // Apparently need to set viewport for passes when we aren't using FBOs.
      gl_set_shader_viewport(gl, 0);
      gl_set_shader_viewport(gl, 1);
      return true;
   }
   else // Replace a currently loaded shader directly.
   {
      if (index > gl->shader->num_shaders())
      {
         RARCH_ERR("Can only load shader for passes that already exist. "
                  "Attempted to set pass %u, but only %u passes exist.\n", index, gl->shader->num_shaders());
         return false;
      }

      return gl->shader->load_shader(index, path);
   }
}
#endif

#ifndef NO_GL_READ_VIEWPORT
static void gl_viewport_info(void *data, struct rarch_viewport *vp)
{
   gl_t *gl = (gl_t*)data;
   *vp = gl->vp;
   vp->full_width  = gl->win_width;
   vp->full_height = gl->win_height;

   // Adjust as GL viewport is bottom-up.
   unsigned top_y = vp->y + vp->height;
   unsigned top_dist = gl->win_height - top_y;
   vp->y = top_dist;
}

static bool gl_read_viewport(void *data, uint8_t *buffer)
{
   gl_t *gl = (gl_t*)data;

   RARCH_PERFORMANCE_INIT(read_viewport);
   RARCH_PERFORMANCE_START(read_viewport);

#ifdef HAVE_OPENGLES
   glPixelStorei(GL_PACK_ALIGNMENT, get_alignment(gl->vp.width * 3));
   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         GL_RGB, GL_UNSIGNED_BYTE, buffer);

   uint8_t *pixels = (uint8_t*)buffer;
   unsigned num_pixels = gl->vp.width * gl->vp.height;
   // Convert RGB to BGR. Formats are byte ordered, so just swap 1st and 3rd byte.
   for (unsigned i = 0; i <= num_pixels; pixels += 3, i++)
   {
      uint8_t tmp = pixels[2];
      pixels[2] = pixels[0];
      pixels[0] = tmp;
   }
#else
#ifdef HAVE_FFMPEG
   if (gl->pbo_readback_enable)
   {
      if (!gl->pbo_readback_valid) // We haven't buffered up enough frames yet, come back later.
         return false;

      pglBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[gl->pbo_readback_index]);
      const void *ptr = pglMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
      if (!ptr)
      {
         RARCH_ERR("Failed to map pixel unpack buffer.\n");
         return false;
      }

      scaler_ctx_scale(&gl->pbo_readback_scaler, buffer, ptr);
      pglUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      pglBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   }
   else // Use slow synchronous readbacks. Use this with plain screenshots as we don't really care about performance in this case.
#endif
   {
      glPixelStorei(GL_PACK_ROW_LENGTH, gl->vp.width);
      glPixelStorei(GL_PACK_ALIGNMENT, get_alignment(gl->vp.width * 3));

      glReadPixels(gl->vp.x, gl->vp.y,
            gl->vp.width, gl->vp.height,
            GL_BGR, GL_UNSIGNED_BYTE, buffer);
   }
#endif

   RARCH_PERFORMANCE_STOP(read_viewport);
   return true;
}
#endif

#ifdef HAVE_RMENU
static void gl_start(void)
{
   video_info_t video_info = {0};

   // Might have to supply correct values here.
   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;
   video_info.fullscreen = true;

   if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      video_info.width  = g_extern.console.screen.viewports.custom_vp.width;
      video_info.height = g_extern.console.screen.viewports.custom_vp.height;
   }

   driver.video_data = gl_init(&video_info, NULL, NULL);

   gl_t *gl = (gl_t*)driver.video_data;

#ifdef RARCH_CONSOLE
   // Comes too early for console - moved to gl_start
   gl->font_ctx = gl_font_init_first(gl, g_settings.video.font_path, g_settings.video.font_size);

   if (!g_settings.video.render_to_texture)
      gl_deinit_fbo(gl);
#endif

   context_get_available_resolutions_func();

#ifdef HAVE_RMENU
   if (gl->ctx_driver->rmenu_init)
      context_rmenu_init_func();
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
   gl_t *gl = driver.video_data;

   if (!gl)
	   return;

   gl_stop();
#ifdef HAVE_CG
   gl_cg_invalidate_context();
#endif
   gl_start();
}
#endif

#ifdef HAVE_OVERLAY
static bool gl_overlay_load(void *data, const uint32_t *image, unsigned width, unsigned height)
{
   gl_t *gl = (gl_t*)data;

   if (!gl->tex_overlay)
      glGenTextures(1, &gl->tex_overlay);

   glBindTexture(GL_TEXTURE_2D, gl->tex_overlay);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(width * sizeof(uint32_t)));
   glTexImage2D(GL_TEXTURE_2D, 0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
         width, height, 0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         RARCH_GL_FORMAT32, image);

   gl_overlay_tex_geom(gl, 0, 0, 1, 1); // Default. Stretch to whole screen.
   gl_overlay_vertex_geom(gl, 0, 0, 1, 1);

   return true;
}

static void gl_overlay_tex_geom(void *data,
      GLfloat x, GLfloat y,
      GLfloat w, GLfloat h)
{
   gl_t *gl = (gl_t*)data;

   gl->overlay_tex_coord[0] = x;     gl->overlay_tex_coord[1] = y;
   gl->overlay_tex_coord[2] = x + w; gl->overlay_tex_coord[3] = y;
   gl->overlay_tex_coord[4] = x;     gl->overlay_tex_coord[5] = y + h;
   gl->overlay_tex_coord[6] = x + w; gl->overlay_tex_coord[7] = y + h;
}

static void gl_overlay_vertex_geom(void *data,
      float x, float y,
      float w, float h)
{
   gl_t *gl = (gl_t*)data;

   // Flipped, so we preserve top-down semantics.
   y = 1.0f - y;
   h = -h;

   gl->overlay_vertex_coord[0] = x;     gl->overlay_vertex_coord[1] = y;
   gl->overlay_vertex_coord[2] = x + w; gl->overlay_vertex_coord[3] = y;
   gl->overlay_vertex_coord[4] = x;     gl->overlay_vertex_coord[5] = y + h;
   gl->overlay_vertex_coord[6] = x + w; gl->overlay_vertex_coord[7] = y + h;
}

static void gl_overlay_enable(void *data, bool state)
{
   gl_t *gl = (gl_t*)data;
   gl->overlay_enable = state;
}

static void gl_overlay_full_screen(void *data, bool enable)
{
   gl_t *gl = (gl_t*)data;
   gl->overlay_full_screen = enable;
}

static void gl_overlay_set_alpha(void *data, float mod)
{
   gl_t *gl = (gl_t*)data;
   gl->overlay_alpha_mod = mod;
}

static void gl_render_overlay(void *data)
{
   gl_t *gl = (gl_t*)data;

   glBindTexture(GL_TEXTURE_2D, gl->tex_overlay);

   const GLfloat white_color_mod[16] = {
      1.0f, 1.0f, 1.0f, gl->overlay_alpha_mod,
      1.0f, 1.0f, 1.0f, gl->overlay_alpha_mod,
      1.0f, 1.0f, 1.0f, gl->overlay_alpha_mod,
      1.0f, 1.0f, 1.0f, gl->overlay_alpha_mod,
   };

   gl_shader_use_func(gl, 0);
   glEnable(GL_BLEND);
   gl->coords.vertex    = gl->overlay_vertex_coord;
   gl->coords.tex_coord = gl->overlay_tex_coord;
   gl->coords.color     = white_color_mod;

   gl_shader_set_coords_func(gl, &gl->coords, &gl->mvp_no_rot);

   if (gl->overlay_full_screen)
   {
      glViewport(0, 0, gl->win_width, gl->win_height);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   }
   else
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisable(GL_BLEND);

   gl->coords.vertex    = vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color     = white_color;
}

static const video_overlay_interface_t gl_overlay_interface = {
   gl_overlay_enable,
   gl_overlay_load,
   gl_overlay_tex_geom,
   gl_overlay_vertex_geom,
   gl_overlay_full_screen,
   gl_overlay_set_alpha,
};

static void gl_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gl_overlay_interface;
}
#endif

static void gl_set_filtering(void *data, unsigned index, bool smooth)
{
   gl_t *gl = (gl_t*)data;

   GLuint filter = smooth ? GL_LINEAR : GL_NEAREST;
   if (index == 1)
   {
      gl->tex_filter = filter;
      // Apply to all PREV textures.
      for (unsigned i = 0; i < TEXTURES; i++)
      {
         glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
      }
   }
#ifdef HAVE_FBO
   else if (index >= 2 && gl->fbo_inited)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[index - 2]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
   }
#endif

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static void gl_set_fbo_state(void *data, unsigned mode)
{
#ifdef HAVE_FBO
   gl_t *gl = (gl_t*)data;

   switch (mode)
   {
      case FBO_DEINIT:
         gl_deinit_fbo(gl);
         break;
      case FBO_REINIT:
         gl_deinit_fbo(gl);
         // Fallthrough
      case FBO_INIT:
         gl_init_fbo(gl, gl->tex_w, gl->tex_h);
         break;
   }
#else
   (void)data;
   (void)mode;
#endif
}

static void gl_set_aspect_ratio(void *data, unsigned aspectratio_index)
{
   gl_t *gl = (gl_t*)data;

   if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_AUTO)
      gfx_set_auto_viewport(g_extern.frame_cache.width, g_extern.frame_cache.height);
   else if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CORE)
      gfx_set_core_viewport();

   g_settings.video.aspect_ratio = aspectratio_lut[g_settings.video.aspect_ratio_idx].value;
   g_settings.video.force_aspect = false;
   gl->keep_aspect = true;

   gl->should_resize = true;
}

static const video_poke_interface_t gl_poke_interface = {
   gl_set_filtering,
   gl_set_fbo_state,
   gl_set_aspect_ratio,
};

static void gl_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gl_poke_interface;
}

const video_driver_t video_gl = {
   gl_init,
   gl_frame,
   gl_set_nonblock_state,
   gl_alive,
   gl_focus,

#if defined(HAVE_GLSL) || defined(HAVE_CG)
   gl_set_shader,
#else
   NULL,
#endif

   gl_free,
   "gl",

#ifdef HAVE_RMENU
   gl_start,
   gl_stop,
   gl_restart,
#endif
   gl_set_rotation,

#ifndef NO_GL_READ_VIEWPORT
   gl_viewport_info,
   gl_read_viewport,
#else
   NULL,
   NULL,
#endif

#ifdef HAVE_OVERLAY
   gl_get_overlay_interface,
#endif
   gl_get_poke_interface,
};



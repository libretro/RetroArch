/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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


#include "../driver.h"

#include "ps3_video_psgl.h"

#include <stdint.h>
#include "../libretro.h"
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <math.h>

#include "../gfx/state_tracker.h"
#include "../gfx/shader_cg.h"
#include "../general.h"
#include "../compat/strl.h"
#include "shared.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../gfx/gl_font.h"
#include "../compat/strl.h"

#define BLUE		0xffff0000u
#define WHITE		0xffffffffu

// Used for the last pass when rendering to the back buffer.
static const GLfloat vertexes_flipped[] = {
   0, 0,
   0, 1,
   1, 1,
   1, 0
};

// Other vertex orientations
static const GLfloat vertexes_90[] = {
   0, 1,
   1, 1,
   1, 0,
   0, 0
};

static const GLfloat vertexes_180[] = {
   1, 1,
   1, 0,
   0, 0,
   0, 1
};

static const GLfloat vertexes_270[] = {
   1, 0,
   0, 0,
   0, 1,
   1, 1
};

static const GLfloat *vertex_ptr = vertexes_flipped;

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
#elif defined(HAVE_OPENGLES)
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

/*============================================================
	GL IMPLEMENTATION
============================================================ */

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
      {
         return gl_cg_init(g_settings.video.cg_shader_path);
         break;
      }
#endif

#ifdef HAVE_XML
      case RARCH_SHADER_BSNES:
      {
         return gl_glsl_init(g_settings.video.bsnes_shader_path);
         break;
      }
#endif

      default:
         break;
   }

   return true;
}

static void gl_shader_use(unsigned index)
{
#ifdef HAVE_CG
   gl_cg_use(index);
#endif

#ifdef HAVE_XML
   gl_glsl_use(index);
#endif
}

static void gl_shader_deinit(void)
{
#ifdef HAVE_CG
   gl_cg_deinit();
#endif

#ifdef HAVE_XML
   gl_glsl_deinit();
#endif
}

static void gl_shader_set_proj_matrix(void)
{
#ifdef HAVE_CG
   gl_cg_set_proj_matrix();
#endif

#ifdef HAVE_XML
   gl_glsl_set_proj_matrix();
#endif
}

static void gl_shader_set_params(unsigned width, unsigned height, 
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

void gl_set_fbo_enable (bool enable)
{
   gl_t *gl = driver.video_data;
   gl->fbo_inited = enable;
   gl->render_to_tex = false;
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
		      0, GL_ARGB_SCE, gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0, GL_ARGB_SCE,
		      GL_UNSIGNED_INT_8_8_8_8, NULL);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
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

// Horribly long and complex FBO init :D
void gl_init_fbo(gl_t *gl, unsigned width, unsigned height)
{
   struct gl_fbo_scale scale, scale_last;
   gl_shader_scale(1, &scale);
   gl_shader_scale(gl_shader_num(), &scale_last);

   gl->fbo_pass = gl_shader_num() - 1;
   if (scale_last.valid)
      gl->fbo_pass++;

   if (gl->fbo_pass <= 0)
      gl->fbo_pass = 1;

   if (!scale.valid)
   {
      scale.scale_x = g_settings.video.fbo_scale_x;
      scale.scale_y = g_settings.video.fbo_scale_y;
      scale.type_x = scale.type_y = RARCH_SCALE_INPUT;
   }

   switch (scale.type_x)
   {
      case RARCH_SCALE_INPUT:
         gl->fbo_rect[0].width = width * next_pow2(ceil(scale.scale_x));
	 break;
      case RARCH_SCALE_ABSOLUTE:
	 gl->fbo_rect[0].width = next_pow2(scale.abs_x);
	 break;
      case RARCH_SCALE_VIEWPORT:
	 gl->fbo_rect[0].width = next_pow2(gl->win_width);
	 break;
      default:
	 break;
   }

   switch (scale.type_y)
   {
      case RARCH_SCALE_INPUT:
         gl->fbo_rect[0].height = height * next_pow2(ceil(scale.scale_y));
	 break;
      case RARCH_SCALE_ABSOLUTE:
	 gl->fbo_rect[0].height = next_pow2(scale.abs_y);
	 break;
      case RARCH_SCALE_VIEWPORT:
	 gl->fbo_rect[0].height = next_pow2(gl->win_height);
	 break;
      default:
	 break;
   }

   unsigned last_width = gl->fbo_rect[0].width, last_height = gl->fbo_rect[0].height;
   gl->fbo_scale[0] = scale;

   RARCH_LOG("Creating FBO 0 @ %ux%u\n", gl->fbo_rect[0].width, gl->fbo_rect[0].height);

   for (int i = 1; i < gl->fbo_pass; i++)
   {
      gl_shader_scale(i + 1, &gl->fbo_scale[i]);
      if (gl->fbo_scale[i].valid)
      {
         switch (gl->fbo_scale[i].type_x)
	 {
            case RARCH_SCALE_INPUT:
               gl->fbo_rect[i].width = last_width * next_pow2(ceil(gl->fbo_scale[i].scale_x));
	       break;
	    case RARCH_SCALE_ABSOLUTE:
	       gl->fbo_rect[i].width = next_pow2(gl->fbo_scale[i].abs_x);
	       break;
	    case RARCH_SCALE_VIEWPORT:
	       gl->fbo_rect[i].width = next_pow2(gl->win_width);
	       break;
	    default:
	       break;
	 }

	 switch (gl->fbo_scale[i].type_y)
	 {
            case RARCH_SCALE_INPUT:
               gl->fbo_rect[i].height = last_height * next_pow2(ceil(gl->fbo_scale[i].scale_y));
	       break;
	    case RARCH_SCALE_ABSOLUTE:
	       gl->fbo_rect[i].height = next_pow2(gl->fbo_scale[i].abs_y);
	       break;
	    case RARCH_SCALE_VIEWPORT:
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
	 gl->fbo_scale[i].type_x = gl->fbo_scale[i].type_y = RARCH_SCALE_INPUT;
      }

      RARCH_LOG("Creating FBO %d @ %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   gl_create_fbo_textures(gl);

   pglGenFramebuffers(gl->fbo_pass, gl->fbo);
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
      pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
	      goto error;
   }

   pglBindFramebuffer(GL_FRAMEBUFFER, 0);

   gl->fbo_inited = true;
   return;

error:
   glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
   pglDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   RARCH_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
}

////////////

static void set_projection(gl_t *gl, bool allow_rotate)
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   if (allow_rotate)
      glRotatef(gl->rotation, 0, 0, 1);

   glOrtho(0, 1, 0, 1, -1, 1);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   gl_shader_set_proj_matrix();
}

static inline void gl_compute_fbo_geometry(gl_t *gl, unsigned width, unsigned height,
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
	    gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].scale_x * gl->vp_out_width;
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
}

static void set_viewport(gl_t *gl, unsigned width, unsigned height, bool force_full, bool allow_rotate)
{
   unsigned m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
   GLfloat m_left, m_right, m_bottom, m_top, m_zNear, m_zFar;

   m_viewport_x_temp = 0;
   m_viewport_y_temp = 0;
   m_viewport_width_temp = width;
   m_viewport_height_temp = height;

   m_left = 0.0f;
   m_right = 1.0f;
   m_bottom = 0.0f;
   m_top = 1.0f;
   m_zNear = -1.0f;
   m_zFar = 1.0f;

   if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)width / height;
      float delta;

      if(g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
	 m_viewport_x_temp = g_console.viewports.custom_vp.x;
	 m_viewport_y_temp = g_console.viewports.custom_vp.y;
	 m_viewport_width_temp = g_console.viewports.custom_vp.width;
	 m_viewport_height_temp = g_console.viewports.custom_vp.height;
      }
      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      // assume they are actually equal.
      else if (fabs(device_aspect - desired_aspect) < 0.0001)
      {
      }
      else if (device_aspect > desired_aspect)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
	 m_viewport_x_temp = (GLint)(width * (0.5 - delta));
	 m_viewport_width_temp = (GLint)(2.0 * width * delta);
	 width = (unsigned)(2.0 * width * delta);
      }
      else
      {
         delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
	 m_viewport_y_temp = (GLint)(height * (0.5 - delta));
	 m_viewport_height_temp = (GLint)(2.0 * height * delta);
	 height = (unsigned)(2.0 * height * delta);
      }
   }

   glViewport(m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp);

   if(g_console.overscan_enable)
   {
      m_left = -g_console.overscan_amount/2;
      m_right = 1 + g_console.overscan_amount/2;
      m_bottom = -g_console.overscan_amount/2;
   }

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   glOrtho(m_left, m_right, m_bottom, m_top, m_zNear, m_zFar);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   if (prg[active_index].mvp)
      cgGLSetStateMatrixParameter(prg[active_index].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);

   gl->vp_width = width;
   gl->vp_height = height;

   // Set last backbuffer viewport.
   gl->vp_out_width = width;
   gl->vp_out_height = height;
}

static void set_lut_texture_coords(const GLfloat *coords)
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

static void check_window(gl_t *gl)
{
   bool quit, resize;

   gfx_ctx_check_window(&quit,
         &resize, &gl->win_width, &gl->win_height,
         gl->frame_count);

   if (quit)
      gl->quitting = true;
   else if (resize)
      gl->should_resize = true;
}

static void ps3graphics_set_orientation(void * data, uint32_t orientation)
{
   (void)data;
   switch (orientation)
   {
      case ORIENTATION_NORMAL:
         vertex_ptr = vertexes_flipped;
	 break;
      case ORIENTATION_VERTICAL:
	 vertex_ptr = vertexes_90;
	 break;
      case ORIENTATION_FLIPPED:
	 vertex_ptr = vertexes_180;
	 break;
      case ORIENTATION_FLIPPED_ROTATED:
	 vertex_ptr = vertexes_270;
	 break;
   }

   glVertexPointer(2, GL_FLOAT, 0, vertex_ptr);
}

#ifdef __CELLOS_LV2__
static void gl_copy_frame(gl_t *gl, const void *frame, unsigned width, unsigned height, unsigned pitch)
{
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
static void gl_copy_frame(gl_t *gl, const void *frame, unsigned width, unsigned height, unsigned pitch)
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

void gl_old_render_path (gl_t *gl)
{
   // Go back to old rendering path.
   glTexCoordPointer(2, GL_FLOAT, 0, gl->tex_coords);
   glVertexPointer(2, GL_FLOAT, 0, vertexes_flipped);
   glColorPointer(4, GL_FLOAT, 0, white_color);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   glDisable(GL_BLEND);
   set_projection(gl, true);
}

static bool gl_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   gl_t *gl = data;

   gl_shader_use(1);
   gl->frame_count++;

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   // Render to texture in first pass.
   if (gl->fbo_inited)
   {
      gl_compute_fbo_geometry(gl, width, height, gl->vp_out_width, gl->vp_out_height);
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
      pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[0]);
      gl->render_to_tex = true;
      set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true, false);

      // Need to preserve the "flipped" state when in FBO as well to have 
      // consistent texture coordinates.
      // We will "flip" it in place on last pass.
      if (gl->render_to_tex)
	      glVertexPointer(2, GL_FLOAT, 0, vertexes);
   }


   if ((width != gl->last_width[gl->tex_index] || height != gl->last_height[gl->tex_index]))
   {
      //Resolution change, need to clear out texture.
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
   else if (width != gl->last_width[(gl->tex_index - 1) & TEXTURES_MASK] || height != gl->last_height[(gl->tex_index - 1) & TEXTURES_MASK])
   {
      // We might have used different texture coordinates last frame. Edge case if resolution changes very rapidly.
      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;
      set_texture_coords(gl->tex_coords, xamt, yamt);
   }

   // Need to preserve the "flipped" state when in FBO as well to have 
   // consistent texture coordinates.
   if (gl->fbo_inited)
      glVertexPointer(2, GL_FLOAT, 0, vertexes);

   gl_copy_frame(gl, frame, width, height, pitch);

   struct gl_tex_info tex_info = {
	   .tex = gl->texture[gl->tex_index],
	   .input_size = {width, height},
	   .tex_size = {gl->tex_w, gl->tex_h}
   };

   struct gl_tex_info fbo_tex_info[MAX_SHADERS];
   unsigned fbo_tex_info_cnt = 0;
   memcpy(tex_info.coord, gl->tex_coords, sizeof(gl->tex_coords));

   glClear(GL_COLOR_BUFFER_BIT);
   gl_shader_set_params(width, height, 
		   gl->tex_w, gl->tex_h, 
		   gl->vp_width, gl->vp_height, 
		   gl->frame_count, &tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

   glDrawArrays(GL_QUADS, 0, 4);

   if (gl->fbo_inited)
   {
      GLfloat fbo_tex_coords[8] = {0.0f};

      // Render the rest of our passes.
      glTexCoordPointer(2, GL_FLOAT, 0, fbo_tex_coords);

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
	 set_viewport(gl, rect->img_width, rect->img_height, true, false);
	 gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
			 prev_rect->width, prev_rect->height, 
			 gl->vp_width, gl->vp_height, gl->frame_count, 
			 &tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

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
      set_viewport(gl, gl->win_width, gl->win_height, false, true);
      gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
		      prev_rect->width, prev_rect->height, 
		      gl->vp_width, gl->vp_height, gl->frame_count, 
		      &tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

      glVertexPointer(2, GL_FLOAT, 0, vertex_ptr);
      glDrawArrays(GL_QUADS, 0, 4);

      glTexCoordPointer(2, GL_FLOAT, 0, gl->tex_coords);
   }

   memmove(gl->prev_info + 1, gl->prev_info, sizeof(tex_info) * (TEXTURES - 1));
   memcpy(&gl->prev_info[0], &tex_info, sizeof(tex_info));
   gl->tex_index = (gl->tex_index + 1) & TEXTURES_MASK;

   if (msg)
   {
      gl_render_msg(gl, msg);
      gl_render_msg_post(gl);
   }

   if(!gl->block_swap)
      gfx_ctx_swap_buffers();

#ifdef HAVE_CG_MENU
   if(gl->menu_render)
   {
      gl_shader_use(RARCH_CG_MENU_SHADER_INDEX);

      gl_shader_set_params(gl->win_width, gl->win_height, gl->win_width, 
		   gl->win_height, gl->win_width, gl->win_height, gl->frame_count,
		   NULL, NULL, NULL, 0);

      set_viewport(gl, gl->win_width, gl->win_height, true, false);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, gl->menu_texture_id);

      glDrawArrays(GL_QUADS, 0, 4); 
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   }
#endif

   return true;
}

static void gl_free(void *data)
{
   if (driver.video_data)
      return;

   gl_t *gl = data;

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
   if (driver.video_data)
      return driver.video_data;

   gl_t *gl = calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

   if (!gfx_ctx_init())
   {
      free(gl);
      return NULL;
   }

   unsigned full_x = 0, full_y = 0;
   gfx_ctx_get_video_size(&full_x, &full_y);
   RARCH_LOG("Detecting desktop resolution %ux%u.\n", full_x, full_y);

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

   RARCH_LOG("GL: Using resolution %ux%u.\n", gl->win_width, gl->win_height);

#ifdef HAVE_CG_MENU
   RARCH_LOG("Initializing menu shader...\n");
   gl_cg_set_menu_shader(DEFAULT_MENU_SHADER_FILE);
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
   set_viewport(gl, gl->win_width, gl->win_height, false, true);
   gl_shader_use(1);
   set_viewport(gl, gl->win_width, gl->win_height, false, true);

   bool force_smooth = false;
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
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->tex_w * gl->tex_h * gl->base_size * TEXTURES, NULL, GL_STREAM_DRAW);
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
   check_window(gl);
   return !gl->quitting;
}

static bool gl_focus(void *data)
{
   (void)data;
   return gfx_ctx_window_has_focus();
}

static void ps3graphics_set_swap_block_swap(void * data, bool enable)
{
   gl_t *gl = driver.video_data;

   gl->block_swap = enable;
}

static void ps3graphics_set_aspect_ratio(void * data, uint32_t aspectratio_index)
{
   (void)data;
   gl_t * gl = driver.video_data;

   if(g_console.aspect_ratio_index == ASPECT_RATIO_AUTO)
      rarch_set_auto_viewport(g_extern.frame_cache.width, g_extern.frame_cache.height);

   g_settings.video.aspect_ratio = aspectratio_lut[g_console.aspect_ratio_index].value;
   g_settings.video.force_aspect = false;
   gl->keep_aspect = true;


   set_viewport(gl, gl->win_width, gl->win_height, false, true);
}

const video_driver_t video_gl = 
{
   .init = gl_init,
   .frame = gl_frame,
   .alive = gl_alive,
   .set_nonblock_state = gl_set_nonblock_state,
   .focus = gl_focus,
   .free = gl_free,
   .ident = "gl",
   .set_swap_block_state = ps3graphics_set_swap_block_swap,
   .set_rotation = ps3graphics_set_orientation,
   .set_aspect_ratio = ps3graphics_set_aspect_ratio,
};

static void get_all_available_resolutions (void)
{
   bool defaultresolution;
   uint32_t i, resolution_count;
   uint16_t num_videomodes;

   defaultresolution = true;

   uint32_t videomode[] = {
	   CELL_VIDEO_OUT_RESOLUTION_480, CELL_VIDEO_OUT_RESOLUTION_576,
	   CELL_VIDEO_OUT_RESOLUTION_960x1080, CELL_VIDEO_OUT_RESOLUTION_720,
	   CELL_VIDEO_OUT_RESOLUTION_1280x1080, CELL_VIDEO_OUT_RESOLUTION_1440x1080,
	   CELL_VIDEO_OUT_RESOLUTION_1600x1080, CELL_VIDEO_OUT_RESOLUTION_1080};

   num_videomodes = sizeof(videomode)/sizeof(uint32_t);

   resolution_count = 0;
   for (i = 0; i < num_videomodes; i++)
      if (cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, videomode[i], CELL_VIDEO_OUT_ASPECT_AUTO,0))
         resolution_count++;
	
   g_console.supported_resolutions = (uint32_t*)malloc(resolution_count * sizeof(uint32_t));

   g_console.supported_resolutions_count = 0;

   for (i = 0; i < num_videomodes; i++)
   {
      if (cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, videomode[i], CELL_VIDEO_OUT_ASPECT_AUTO,0))
      {
         g_console.supported_resolutions[g_console.supported_resolutions_count++] = videomode[i];
	 g_console.initial_resolution_id = videomode[i];

	 if (g_console.current_resolution_id == videomode[i])
	 {
	    defaultresolution = false;
	    g_console.current_resolution_index = g_console.supported_resolutions_count-1;
	 }
      }
   }

   /* In case we didn't specify a resolution - make the last resolution
      that was added to the list (the highest resolution) the default resolution*/
   if (g_console.current_resolution_id > num_videomodes || defaultresolution)
      g_console.current_resolution_index = g_console.supported_resolutions_count-1;
}

void ps3_next_resolution (void)
{
   if(g_console.current_resolution_index+1 < g_console.supported_resolutions_count)
   {
      g_console.current_resolution_index++;
      g_console.current_resolution_id = g_console.supported_resolutions[g_console.current_resolution_index];
   }
}

void ps3_previous_resolution (void)
{
   if(g_console.current_resolution_index)
   {
      g_console.current_resolution_index--;
      g_console.current_resolution_id = g_console.supported_resolutions[g_console.current_resolution_index];
   }
}

int ps3_check_resolution(uint32_t resolution_id)
{
   return cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, resolution_id, CELL_VIDEO_OUT_ASPECT_AUTO,0);
}

const char * ps3_get_resolution_label(uint32_t resolution)
{
   switch(resolution)
   {
      case CELL_VIDEO_OUT_RESOLUTION_480:
	      return  "720x480 (480p)";
      case CELL_VIDEO_OUT_RESOLUTION_576:
	      return "720x576 (576p)"; 
      case CELL_VIDEO_OUT_RESOLUTION_720:
	      return "1280x720 (720p)";
      case CELL_VIDEO_OUT_RESOLUTION_960x1080:
	      return "960x1080";
      case CELL_VIDEO_OUT_RESOLUTION_1280x1080:
	      return "1280x1080";
      case CELL_VIDEO_OUT_RESOLUTION_1440x1080:
	      return "1440x1080";
      case CELL_VIDEO_OUT_RESOLUTION_1600x1080:
	      return "1600x1080";
      case CELL_VIDEO_OUT_RESOLUTION_1080:
	      return "1920x1080 (1080p)";
      default:
	      return "Unknown";
   }
}

void ps3_set_filtering(unsigned index, bool set_smooth)
{
   gl_t *gl = driver.video_data;

   if (!gl)
      return;

   if (index == 1)
   {
      // Apply to all PREV textures.
      for (unsigned i = 0; i < TEXTURES; i++)
      {
         glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
	 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
	 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
      }
   }
   else if (index >= 2 && gl->fbo_inited)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[index - 2]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

void ps3graphics_set_overscan(void)
{
   gl_t * gl = driver.video_data;
   if(!gl)
      return;

   set_viewport(gl, gl->win_width, gl->win_height, false, true);
}

void ps3graphics_video_init(bool get_all_resolutions)
{
   video_info_t video_info = {0};

   // Might have to supply correct values here.
   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;
   video_info.fullscreen = true;
   if(g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
   {
      video_info.width = g_console.viewports.custom_vp.width;
      video_info.height = g_console.viewports.custom_vp.height;
   }
   driver.video_data = gl_init(&video_info, NULL, NULL);
   gl_set_fbo_enable(g_console.fbo_enabled);

   if(get_all_resolutions)
      get_all_available_resolutions();

   CellVideoOutState g_video_state;
   cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &g_video_state);
#ifdef HAVE_CG_MENU
   gfx_ctx_menu_init();
#endif
}

void ps3graphics_video_reinit(void)
{
   gl_t * gl = driver.video_data;

   if(!gl)
	   return;

   ps3_video_deinit();
   gl_cg_invalidate_context();
   ps3graphics_video_init(false);
}

void ps3_video_deinit(void)
{
   void *data = driver.video_data;
   driver.video_data = NULL;
   gl_free(data);
}

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

#include <sys/spu_initialize.h>

#include "../gfx/state_tracker.h"
#include "../gfx/shader_cg.h"
#include "../general.h"
#include "../compat/strl.h"
#include "shared.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../compat/strl.h"

#define BLUE		0xffff0000u
#define WHITE		0xffffffffu

#define FORCE_16BIT_COLOR 1

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

bool g_quitting;
unsigned g_frame_count;
void *g_gl;

/*============================================================
	GL IMPLEMENTATION
============================================================ */

static bool gl_shader_init(void)
{
   switch (g_settings.video.shader_type)
   {
      case RARCH_SHADER_AUTO:
         if (strlen(g_settings.video.cg_shader_path) > 0 && strlen(g_settings.video.bsnes_shader_path) > 0)
            RARCH_WARN("Both Cg and bSNES XML shader are defined in config file. Cg shader will be selected by default.\n");
	 // fall-through
      case RARCH_SHADER_CG:
         if (strlen(g_settings.video.cg_shader_path) > 0)
            return gl_cg_init(g_settings.video.cg_shader_path);
	 break;
      default:
         break;
   }

   return true;
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

void gl_set_fbo_enable (bool enable)
{
   gl_t *gl = g_gl;

   gl->fbo_enabled = enable;
}

static void gl_shader_scale(unsigned index, struct gl_fbo_scale *scale)
{
   scale->valid = false;
   if (!scale->valid)
      gl_cg_shader_scale(index, scale);
}

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
   glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
   glDeleteFramebuffersOES(gl->fbo_pass, gl->fbo);
   memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
   memset(gl->fbo, 0, sizeof(gl->fbo));
   gl->fbo_pass = 0;
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

   glGenFramebuffersOES(gl->fbo_pass, gl->fbo);
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[i]);
      glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      GLenum status = glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES);
      if (status != GL_FRAMEBUFFER_COMPLETE_OES)
	      goto error;
   }

   glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
   return;

error:
   glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
   glDeleteFramebuffersOES(gl->fbo_pass, gl->fbo);
   RARCH_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
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

static void set_viewport(gl_t *gl, unsigned width, unsigned height)
{
   uint32_t m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
   GLfloat m_left, m_right, m_bottom, m_top, m_zNear, m_zFar;

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

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

   if (gl->keep_aspect)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)width / height;
      float delta;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      if(g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
	 m_viewport_x_temp = g_console.viewports.custom_vp.x;
	 m_viewport_y_temp = g_console.viewports.custom_vp.y;
	 m_viewport_width_temp = g_console.viewports.custom_vp.width;
	 m_viewport_height_temp = g_console.viewports.custom_vp.height;
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

   if(gl->overscan_enable)
   {
      m_left = -gl->overscan_amount/2;
      m_right = 1 + gl->overscan_amount/2;
      m_bottom = -gl->overscan_amount/2;
   }

   glOrthof(m_left, m_right, m_bottom, m_top, m_zNear, m_zFar);
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

#define set_viewport_force_full(gl, width, height) \
   glMatrixMode(GL_PROJECTION); \
   glLoadIdentity(); \
   glViewport(0, 0, width, height); \
   glOrthof(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f); \
   glMatrixMode(GL_MODELVIEW); \
   glLoadIdentity(); \
   if (prg[active_index].mvp) \
      cgGLSetStateMatrixParameter(prg[active_index].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY); \
   gl->vp_width = width; \
   gl->vp_height = height;

static void set_lut_texture_coords(const GLfloat *coords)
{
   // For texture images.
   pglClientActiveTexture(GL_TEXTURE1);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 0, coords);
   pglClientActiveTexture(GL_TEXTURE0);
}

#define set_texture_coords(coords, xamt, yamt) \
   coords[1] = yamt; \
   coords[4] = xamt; \
   coords[6] = xamt; \
   coords[7] = yamt;

void gl_frame_menu (void)
{
   gl_t *gl = g_gl;

   g_frame_count++;

   if(!gl)
	   return;

   gl_cg_use(RARCH_CG_MENU_SHADER_INDEX);

   gl_cg_set_params(gl->win_width, gl->win_height, gl->win_width, 
		   gl->win_height, gl->win_width, gl->win_height, g_frame_count,
		   NULL, NULL, NULL, 0);

   set_viewport_force_full(gl, gl->win_width, gl->win_height);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, gl->menu_texture_id);

   glDrawArrays(GL_QUADS, 0, 4); 

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
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

static bool gl_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   gl_t *gl = data;

   gl_cg_use(1);
   g_frame_count++;

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   // Render to texture in first pass.
   if (gl->fbo_enabled)
   {
      gl_compute_fbo_geometry(gl, width, height, gl->vp_out_width, gl->vp_out_height);
      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
      glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[0]);
      set_viewport_force_full(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height);
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
   if (gl->fbo_enabled)
      glVertexPointer(2, GL_FLOAT, 0, vertexes);

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

   struct gl_tex_info tex_info = {
	   .tex = gl->texture[gl->tex_index],
	   .input_size = {width, height},
	   .tex_size = {gl->tex_w, gl->tex_h}
   };

   struct gl_tex_info fbo_tex_info[MAX_SHADERS];
   unsigned fbo_tex_info_cnt = 0;
   memcpy(tex_info.coord, gl->tex_coords, sizeof(gl->tex_coords));

   glClear(GL_COLOR_BUFFER_BIT);
   gl_cg_set_params(width, height, 
		   gl->tex_w, gl->tex_h, 
		   gl->vp_width, gl->vp_height, 
		   g_frame_count, &tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

   glDrawArrays(GL_QUADS, 0, 4);

   if (gl->fbo_enabled)
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
	 gl_cg_use(i + 1);
	 glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

	 glClear(GL_COLOR_BUFFER_BIT);

	 // Render to FBO with certain size.
	 set_viewport_force_full(gl, rect->img_width, rect->img_height);
	 gl_cg_set_params(prev_rect->img_width, prev_rect->img_height, 
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
      gl_cg_use(gl->fbo_pass + 1);

      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

      glClear(GL_COLOR_BUFFER_BIT);
      set_viewport(gl, gl->win_width, gl->win_height);
      gl_cg_set_params(prev_rect->img_width, prev_rect->img_height, 
		      prev_rect->width, prev_rect->height, 
		      gl->vp_width, gl->vp_height, g_frame_count, 
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
      cellDbgFontPrintf(g_settings.video.msg_pos_x, g_settings.video.msg_pos_y, 1.11f, BLUE,	msg);
      cellDbgFontPrintf(g_settings.video.msg_pos_x, g_settings.video.msg_pos_y, 1.10f, WHITE, msg);
      cellDbgFontDraw();
   }

   if(!gl->block_swap)
      psglSwap();

   return true;
}

static void psgl_deinit(gl_t *gl)
{
   cellDbgFontExit();

   psglDestroyContext(gl->gl_context);
   psglDestroyDevice(gl->gl_device);

   psglExit();
}

static void gl_free(void *data)
{
   if (g_gl)
      return;

   gl_t *gl = data;

   gl_cg_deinit();

   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);
   glDeleteTextures(TEXTURES, gl->texture);
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);

   gl_deinit_fbo(gl);
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
      RARCH_LOG("GL VSync => %s\n", state ? "off" : "on");
      if(state)
         glDisable(GL_VSYNC_SCE);
      else
         glEnable(GL_VSYNC_SCE);
   }
}

static bool psgl_init_device(gl_t *gl, const video_info_t *video, uint32_t resolution_id)
{
   PSGLinitOptions options =
   {
	   .enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS,
	   .maxSPUs = 1,
	   .initializeSPUs = GL_FALSE,
   };
#if CELL_SDK_VERSION < 0x340000
   options.enable |=	PSGL_INIT_HOST_MEMORY_SIZE;
#endif

   // Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL
   sys_spu_initialize(6, 1);
   psglInit(&options);

   PSGLdeviceParameters params;

   params.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT | \
		   PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT | \
		   PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
   params.colorFormat = GL_ARGB_SCE;
   params.depthFormat = GL_NONE;
   params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

   if(g_console.triple_buffering_enable)
   {
      params.enable |= PSGL_DEVICE_PARAMETERS_BUFFERING_MODE;
      params.bufferingMode = PSGL_BUFFERING_MODE_TRIPLE;
   }

   if(resolution_id)
   {
      CellVideoOutResolution resolution;
      cellVideoOutGetResolution(resolution_id, &resolution);

      params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
      params.width = resolution.width;
      params.height = resolution.height;
   }

   gl->gl_device = psglCreateDeviceExtended(&params);
   psglGetDeviceDimensions(gl->gl_device, &gl->win_width, &gl->win_height); 

   if(g_console.viewports.custom_vp.width == 0)
      g_console.viewports.custom_vp.width = gl->win_width;

   if(g_console.viewports.custom_vp.height == 0)
      g_console.viewports.custom_vp.height = gl->win_height;

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

   if (!psgl_init_device(gl, video, g_console.current_resolution_id))
      return NULL;


   RARCH_LOG("Detecting resolution %ux%u.\n", gl->win_width, gl->win_height);

   video->vsync ? glEnable(GL_VSYNC_SCE) : glDisable(GL_VSYNC_SCE);

   gl->vsync = video->vsync;

   RARCH_LOG("GL: Using resolution %ux%u.\n", gl->win_width, gl->win_height);

   RARCH_LOG("GL: Initializing debug fonts...\n");
   psgl_init_dbgfont(gl);

   RARCH_LOG("Initializing menu shader...\n");
   gl_cg_set_menu_shader(DEFAULT_MENU_SHADER_FILE);

   if (!gl_shader_init())
   {
      RARCH_ERR("Menu shader initialization failed.\n");
      psgl_deinit(gl);
      free(gl);
      return NULL;
   }

   RARCH_LOG("GL: Loaded %u program(s).\n", gl_shader_num());

   // Set up render to texture.
   gl_init_fbo(gl, RARCH_SCALE_BASE * video->input_scale,
		   RARCH_SCALE_BASE * video->input_scale);


   gl->keep_aspect = video->force_aspect;

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_cg_use(0);
   set_viewport(gl, gl->win_width, gl->win_height);
   gl_cg_use(1);
   set_viewport(gl, gl->win_width, gl->win_height);

   bool force_smooth = false;
   if (gl_shader_filter_type(1, &force_smooth))
      gl->tex_filter = force_smooth ? GL_LINEAR : GL_NEAREST;
   else
      gl->tex_filter = video->smooth ? GL_LINEAR : GL_NEAREST;

   gl->texture_type = GL_BGRA;
   gl->texture_fmt = video->rgb32 ? GL_ARGB_SCE : GL_RGB5_A1;
   gl->base_size = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   glEnable(GL_TEXTURE_2D);
   glClearColor(0, 0, 0, 1);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   gl->tex_w = RARCH_SCALE_BASE * video->input_scale;
   gl->tex_h = RARCH_SCALE_BASE * video->input_scale;
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
   glVertexPointer(2, GL_FLOAT, 0, vertex_ptr);

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
   (void)data;
   cellSysutilCheckCallback();
   return !g_quitting;
}

static bool gl_focus(void *data)
{
   (void)data;
   return true;
}

static void ps3graphics_set_swap_block_swap(void * data, bool toggle)
{
   (void)data;
   gl_t *gl = g_gl;
   gl->block_swap = toggle;
}

static void ps3graphics_swap(void * data)
{
   (void)data;
   psglSwap();
   cellSysutilCheckCallback();
}

static void ps3graphics_set_aspect_ratio(void * data, uint32_t aspectratio_index)
{
   (void)data;
   gl_t * gl = g_gl;

   g_settings.video.aspect_ratio = aspectratio_lut[g_console.aspect_ratio_index].value;
   g_settings.video.force_aspect = false;
   gl->keep_aspect = true;
   set_viewport(gl, gl->win_width, gl->win_height);
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
   .swap = ps3graphics_swap
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

void ps3_set_resolution (void)
{
   gl_t *gl = g_gl;
   cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &gl->g_video_state);
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


void ps3graphics_set_vsync(uint32_t vsync)
{
   if(vsync)
      glEnable(GL_VSYNC_SCE);
   else
      glDisable(GL_VSYNC_SCE);
}

bool ps3_setup_texture(void)
{
   gl_t *gl = g_gl;

   if (!gl)
      return false;

   glGenTextures(1, &gl->menu_texture_id);

   RARCH_LOG("Loading texture image for menu...\n");
   if(!texture_image_load(DEFAULT_MENU_BORDER_FILE, &gl->menu_texture))
   {
      RARCH_ERR("Failed to load texture image for menu.\n");
      return false;
   }

   glBindTexture(GL_TEXTURE_2D, gl->menu_texture_id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB_SCE, gl->menu_texture.width, gl->menu_texture.height, 0,
		   GL_ARGB_SCE, GL_UNSIGNED_INT_8_8_8_8, gl->menu_texture.pixels);

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   free(gl->menu_texture.pixels);
	
   return true;
}

void ps3_set_filtering(unsigned index, bool set_smooth)
{
   gl_t *gl = g_gl;

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
   else if (index >= 2 && gl->fbo_enabled)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[index - 2]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

void ps3graphics_set_overscan(bool overscan_enable, float amount, bool recalculate_viewport)
{
   gl_t * gl = g_gl;
   if(!gl)
      return;

   gl->overscan_enable = overscan_enable;
   gl->overscan_amount = amount;

   if(recalculate_viewport)
   { 
      set_viewport(gl, gl->win_width, gl->win_height);
   }
}

void ps3graphics_video_init(bool get_all_resolutions)
{
   video_info_t video_info = {0};

   // Might have to supply correct values here.
   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;
   g_gl = gl_init(&video_info, NULL, NULL);
   gl_set_fbo_enable(g_console.fbo_enabled);

   gl_t * gl = g_gl;

   gl->overscan_enable = g_console.overscan_enable;
   gl->overscan_amount = g_console.overscan_amount;

   if(get_all_resolutions)
      get_all_available_resolutions();

   ps3_set_resolution();
   ps3_setup_texture();
   ps3graphics_set_overscan(gl->overscan_enable, gl->overscan_amount, 0);
}

void ps3graphics_video_reinit(void)
{
   gl_t * gl = g_gl;

   if(!gl)
	   return;

   ps3_video_deinit();
   gl_cg_invalidate_context();
   ps3graphics_video_init(false);
}

void ps3_video_deinit(void)
{
   void *data = g_gl;
   g_gl = NULL;
   gl_free(data);
}


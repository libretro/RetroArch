/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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


#include "driver.h"

#include <stdint.h>
#include "libsnes.hpp"
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include "general.h"
#include <assert.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gl_common.h"
#include "gfx_common.h"
#include "sdlwrap.h"
#include "strl.h"

#define NO_SDL_GLEXT
#include "SDL.h"
#include "SDL_opengl.h"
#include "input/ssnes_sdl_input.h"

#ifdef HAVE_CG
#include "shader_cg.h"
#endif

#ifdef HAVE_XML
#include "shader_glsl.h"
#endif


#ifdef HAVE_FREETYPE
#include "fonts.h"
#endif

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


#ifdef HAVE_FBO
#ifdef _WIN32
static PFNGLGENFRAMEBUFFERSPROC pglGenFramebuffers = NULL;
static PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer = NULL;
static PFNGLFRAMEBUFFERTEXTURE2DPROC pglFramebufferTexture2D = NULL;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC pglCheckFramebufferStatus = NULL;
static PFNGLDELETEFRAMEBUFFERSPROC pglDeleteFramebuffers = NULL;

#define LOAD_SYM(sym) if (!p##sym) { SDL_SYM_WRAP(p##sym, #sym) }
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

#define MAX_SHADERS 16

typedef struct gl
{
   bool vsync;
   GLuint texture[2];
   unsigned tex_index; // For use with PREV.
   struct gl_tex_info prev_info;
   GLuint tex_filter;

   void *empty_buf;

   unsigned frame_count;

#ifdef HAVE_FBO
   // Render-to-texture, multipass shaders
   GLuint fbo[MAX_SHADERS];
   GLuint fbo_texture[MAX_SHADERS];
   struct gl_fbo_rect fbo_rect[MAX_SHADERS];
   struct gl_fbo_scale fbo_scale[MAX_SHADERS];
   bool render_to_tex;
   int fbo_pass;
   bool fbo_inited;
#endif

   bool should_resize;
   bool quitting;
   bool fullscreen;
   bool keep_aspect;

   unsigned full_x, full_y;

   unsigned win_width;
   unsigned win_height;
   unsigned vp_width, vp_out_width;
   unsigned vp_height, vp_out_height;
   unsigned last_width[2];
   unsigned last_height[2];
   unsigned tex_w, tex_h;
   GLfloat tex_coords[8];
#ifdef HAVE_FBO
   GLfloat fbo_tex_coords[8];
#endif

   GLenum texture_type; // XBGR1555 or ARGB
   GLenum texture_fmt;
   unsigned base_size; // 2 or 4

#ifdef HAVE_FREETYPE
   font_renderer_t *font;
   GLuint font_tex;
   int font_tex_w, font_tex_h;
   void *font_tex_empty_buf;
   char font_last_msg[256];
   int font_last_width, font_last_height;
#endif

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

#ifdef HAVE_CG
         if (strlen(g_settings.video.cg_shader_path) > 0)
            return gl_cg_init(g_settings.video.cg_shader_path);
#endif

#ifdef HAVE_XML
         if (strlen(g_settings.video.bsnes_shader_path) > 0)
            return gl_glsl_init(g_settings.video.bsnes_shader_path);
#endif
         break;
      }

#ifdef HAVE_CG
      case SSNES_SHADER_CG:
      {
         return gl_cg_init(g_settings.video.cg_shader_path);
         break;
      }
#endif

#ifdef HAVE_XML
      case SSNES_SHADER_BSNES:
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
   unsigned num = 0;
#ifdef HAVE_CG
   unsigned cg_num = gl_cg_num();
   if (cg_num > num)
      num = cg_num;
#endif

#ifdef HAVE_XML
   unsigned glsl_num = gl_glsl_num();
   if (glsl_num > num)
      num = glsl_num;
#endif

   return num;
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

//////////////// Message rendering
static inline void gl_init_font(gl_t *gl, const char *font_path, unsigned font_size)
{
#ifdef HAVE_FREETYPE
   const char *path = font_path;
   if (!*path)
      path = font_renderer_get_default_font();

   if (path)
   {
      gl->font = font_renderer_new(path, font_size);
      if (gl->font)
      {
         glGenTextures(1, &gl->font_tex);
         glBindTexture(GL_TEXTURE_2D, gl->font_tex);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
      }
      else
         SSNES_WARN("Couldn't init font renderer with font \"%s\"...\n", font_path);
   }
   else
      SSNES_LOG("Did not find default font.\n");
#else
   (void)gl;
   (void)font_path;
   (void)font_size;
#endif
}

// Horribly long and complex FBO init :D
static void gl_init_fbo(gl_t *gl, unsigned width, unsigned height)
{
#ifdef HAVE_FBO
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

   pglGenFramebuffers(gl->fbo_pass, gl->fbo);
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
      pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   gl->fbo_inited = true;
   return;

error:
   glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
   pglDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   SSNES_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
#else
   (void)gl;
   (void)width;
   (void)height;
#endif
}

static inline void gl_deinit_font(gl_t *gl)
{
#ifdef HAVE_FREETYPE
   if (gl->font)
   {
      font_renderer_free(gl->font);
      glDeleteTextures(1, &gl->font_tex);

      if (gl->font_tex_empty_buf)
         free(gl->font_tex_empty_buf);
   }
#else
   (void)gl;
#endif
}
////////////

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
         glViewport(width * (0.5 - delta), 0, 2.0 * width * delta, height);
         width = 2.0 * width * delta;
      }
      else
      {
         float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         glViewport(0, height * (0.5 - delta), width, 2.0 * height * delta);
         height = 2.0 * height * delta;
      }
   }
   else
      glViewport(0, 0, width, height);

   glOrtho(0, 1, 0, 1, -1, 1);
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

#ifdef HAVE_FREETYPE

// Somewhat overwhelming code just to render some damn fonts.
// We aim to use NPOT textures for compatibility with old and shitty cards.
// Also, we want to avoid reallocating a texture for each glyph (performance dips), so we
// contruct the whole texture using one call, and copy straight to it with
// glTexSubImage.

struct font_rect
{
   int x, y;
   int width, height;
   int pot_width, pot_height;
};

static void calculate_msg_geometry(const struct font_output *head, struct font_rect *rect)
{
   int x_min = head->off_x;
   int x_max = head->off_x + head->width;
   int y_min = head->off_y;
   int y_max = head->off_y + head->height;

   while ((head = head->next))
   {
      int left = head->off_x;
      int right = head->off_x + head->width;
      int bottom = head->off_y;
      int top = head->off_y + head->height;

      if (left < x_min)
         x_min = left;
      if (right > x_max)
         x_max = right;

      if (bottom < y_min)
         y_min = bottom;
      if (top > y_max)
         y_max = top;
   }

   rect->x = x_min;
   rect->y = y_min;
   rect->width = x_max - x_min;
   rect->height = y_max - y_min;
}

static void adjust_power_of_two(gl_t *gl, struct font_rect *geom)
{
   // Some systems really hate NPOT textures.
   geom->pot_width = next_pow2(geom->width);
   geom->pot_height = next_pow2(geom->height);

   if ((geom->pot_width > gl->font_tex_w) || (geom->pot_height > gl->font_tex_h))
   {
      gl->font_tex_empty_buf = realloc(gl->font_tex_empty_buf, geom->pot_width * geom->pot_height);
      memset(gl->font_tex_empty_buf, 0, geom->pot_width * geom->pot_height);

      glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, geom->pot_width);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY8, geom->pot_width, geom->pot_height,
            0, GL_LUMINANCE, GL_UNSIGNED_BYTE, gl->font_tex_empty_buf);

      gl->font_tex_w = geom->pot_width;
      gl->font_tex_h = geom->pot_height;
   }
}

// Old style "blitting", so we can render all the fonts in one go.
// TODO: Is it possible that fonts could overlap if we blit without alpha blending?
static void blit_fonts(gl_t *gl, const struct font_output *head, const struct font_rect *geom)
{
   // Clear out earlier fonts.
   glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->font_tex_w);
   glTexSubImage2D(GL_TEXTURE_2D,
         0, 0, 0, gl->font_tex_w, gl->font_tex_h,
         GL_LUMINANCE, GL_UNSIGNED_BYTE, gl->font_tex_empty_buf);

   while (head)
   {
      // head has top-left oriented coords.
      int x = head->off_x - geom->x;
      int y = head->off_y - geom->y;
      y = gl->font_tex_h - head->height - y - 1;

      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(head->pitch));
      glPixelStorei(GL_UNPACK_ROW_LENGTH, head->pitch);
      glTexSubImage2D(GL_TEXTURE_2D,
            0, x, y, head->width, head->height,
            GL_LUMINANCE, GL_UNSIGNED_BYTE, head->output);

      head = head->next;
   }
}

static void calculate_font_coords(gl_t *gl,
      GLfloat font_vertex[8], GLfloat font_tex_coords[8])
{
   GLfloat lx = g_settings.video.msg_pos_x;
   GLfloat hx = (GLfloat)gl->font_last_width / gl->vp_width + lx;
   GLfloat ly = g_settings.video.msg_pos_y;
   GLfloat hy = (GLfloat)gl->font_last_height / gl->vp_height + ly;

   font_vertex[0] = lx;
   font_vertex[1] = ly;
   font_vertex[2] = lx;
   font_vertex[3] = hy;
   font_vertex[4] = hx;
   font_vertex[5] = hy;
   font_vertex[6] = hx;
   font_vertex[7] = ly;

   lx = 0.0f;
   hx = (GLfloat)gl->font_last_width / gl->font_tex_w;
   ly = 1.0f - (GLfloat)gl->font_last_height / gl->font_tex_h; 
   hy = 1.0f;

   font_tex_coords[0] = lx;
   font_tex_coords[1] = hy;
   font_tex_coords[2] = lx;
   font_tex_coords[3] = ly;
   font_tex_coords[4] = hx;
   font_tex_coords[5] = ly;
   font_tex_coords[6] = hx;
   font_tex_coords[7] = hy;
}

static void gl_render_msg(gl_t *gl, const char *msg)
{
   if (!gl->font)
      return;

   GLfloat font_vertex[8]; 
   GLfloat font_tex_coords[8];

   // Deactivate custom shaders. Enable the font texture.
   gl_shader_use(0);
   set_viewport(gl, gl->win_width, gl->win_height, false);
   glBindTexture(GL_TEXTURE_2D, gl->font_tex);
   glVertexPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), font_vertex);
   glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), font_tex_coords);

   // Need blending. 
   // Using fixed function pipeline here since we cannot guarantee presence of shaders (would be kinda overkill anyways).
   glEnable(GL_BLEND);
   glColor4f(g_settings.video.msg_color_r, g_settings.video.msg_color_g,
         g_settings.video.msg_color_b, 1);

   struct font_output_list out;

   // If we get the same message, there's obviously no need to render fonts again ...
   if (strcmp(gl->font_last_msg, msg) != 0)
   {
      font_renderer_msg(gl->font, msg, &out);
      struct font_output *head = out.head;

      struct font_rect geom;
      calculate_msg_geometry(head, &geom);
      adjust_power_of_two(gl, &geom);
      blit_fonts(gl, head, &geom);

      font_renderer_free_output(&out);
      strlcpy(gl->font_last_msg, msg, sizeof(gl->font_last_msg));

      gl->font_last_width = geom.width;
      gl->font_last_height = geom.height;
   }
   calculate_font_coords(gl, font_vertex, font_tex_coords);
   
   glDrawArrays(GL_QUADS, 0, 4);

   // Go back to old rendering path.
   glColor4f(1, 1, 1, 1);
   glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), gl->tex_coords);
   glVertexPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), vertexes_flipped);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   glDisable(GL_BLEND);
}
#else
static void gl_render_msg(gl_t *gl, const char *msg)
{
   (void)gl;
   (void)msg;
}
#endif

static inline void set_lut_texture_coords(const GLfloat *coords)
{
#if defined(HAVE_XML) || defined(HAVE_CG)
   // For texture images.
   pglClientActiveTexture(GL_TEXTURE1);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), coords);
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

static void check_window(gl_t *gl)
{
   bool quit, resize;

   sdlwrap_check_window(&quit,
         &resize, &gl->win_width, &gl->win_height,
         gl->frame_count);

   if (quit)
   {
      gl->quitting = true;
   }
   else if (resize)
   {
      gl->should_resize = true;
   }
}

static bool gl_frame(void *data, const void* frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   gl_t *gl = data;
   check_window(gl);

   gl_shader_use(1);
   gl->frame_count++;

#if defined(HAVE_XML) || defined(HAVE_CG)
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
#endif

#ifdef HAVE_FBO
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
      pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[0]);
      gl->render_to_tex = true;
      set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true);
   }
#endif

   if (gl->should_resize)
   {
      gl->should_resize = false;

      sdlwrap_set_resize(gl->win_width, gl->win_height);

#ifdef HAVE_FBO
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

               pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
               glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);
               glTexImage2D(GL_TEXTURE_2D,
                     0, GL_RGBA, gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0, GL_BGRA,
                     GL_UNSIGNED_INT_8_8_8_8, NULL);

               pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

               GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
               if (status != GL_FRAMEBUFFER_COMPLETE)
                  SSNES_WARN("Failed to reinit FBO texture!\n");

               SSNES_LOG("Recreating FBO texture #%d: %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
            }
         }

         // Go back to what we're supposed to do, render to FBO #0 :D
         glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
         pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[0]);
         set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true);
      }
#else
      set_viewport(gl, gl->win_width, gl->win_height, false);
#endif
   }

   if ((width != gl->last_width[gl->tex_index] || height != gl->last_height[gl->tex_index]) && gl->empty_buf) // Res change. need to clear out texture.
   {
      gl->last_width[gl->tex_index] = width;
      gl->last_height[gl->tex_index] = height;
      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(pitch));
      glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->tex_w);

      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, gl->tex_w, gl->tex_h, gl->texture_type,
            gl->texture_fmt, gl->empty_buf);

      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;

      set_texture_coords(gl->tex_coords, xamt, yamt);
   }
#if defined(HAVE_XML) || defined(HAVE_CG)
   else if (width != gl->last_width[1 - gl->tex_index] || height != gl->last_height[1 - gl->tex_index])
   {
      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;
      set_texture_coords(gl->tex_coords, xamt, yamt);
   }
#endif

#ifdef HAVE_FBO
   // Need to preserve the "flipped" state when in FBO as well to have 
   // consistent texture coordinates.
   if (gl->render_to_tex)
      glVertexPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), vertexes);
#endif

   glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
   glTexSubImage2D(GL_TEXTURE_2D,
         0, 0, 0, width, height, gl->texture_type,
         gl->texture_fmt, frame);

   struct gl_tex_info tex_info = {
      .tex = gl->texture[gl->tex_index],
      .input_size = {width, height},
      .tex_size = {gl->tex_w, gl->tex_h}
   };
   struct gl_tex_info fbo_tex_info[MAX_SHADERS];
   unsigned fbo_tex_info_cnt = 0;
   memcpy(tex_info.coord, gl->tex_coords, sizeof(gl->tex_coords));

   glClear(GL_COLOR_BUFFER_BIT);
   gl_shader_set_params(width, height, gl->tex_w, gl->tex_h, gl->vp_width, gl->vp_height, gl->frame_count, 
         &tex_info, &gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

   glDrawArrays(GL_QUADS, 0, 4);

#ifdef HAVE_FBO
   if (gl->fbo_inited)
   {
      // Render the rest of our passes.
      glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), gl->fbo_tex_coords);

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

         pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
         gl_shader_use(i + 1);
         glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

         glClear(GL_COLOR_BUFFER_BIT);

         // Render to FBO with certain size.
         set_viewport(gl, rect->img_width, rect->img_height, true);
         gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
               prev_rect->width, prev_rect->height, 
               gl->vp_width, gl->vp_height, gl->frame_count, 
               &tex_info, &gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

         glDrawArrays(GL_QUADS, 0, 4);

         fbo_tex_info_cnt++;
      }

      // Render our last FBO texture directly to screen.
      prev_rect = &gl->fbo_rect[gl->fbo_pass - 1];
      GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
      GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

      set_texture_coords(gl->fbo_tex_coords, xamt, yamt);

      // Render our FBO texture to back buffer.
      pglBindFramebuffer(GL_FRAMEBUFFER, 0);
      gl_shader_use(gl->fbo_pass + 1);

      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

      glClear(GL_COLOR_BUFFER_BIT);
      gl->render_to_tex = false;
      set_viewport(gl, gl->win_width, gl->win_height, false);
      gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
            prev_rect->width, prev_rect->height, 
            gl->vp_width, gl->vp_height, gl->frame_count, 
            &tex_info, &gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

      glVertexPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), vertexes_flipped);
      glDrawArrays(GL_QUADS, 0, 4);

      glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), gl->tex_coords);
   }
#endif

#if defined(HAVE_XML) || defined(HAVE_CG)
   memcpy(&gl->prev_info, &tex_info, sizeof(tex_info));
   gl->tex_index = 1 - gl->tex_index;
#endif

   if (msg)
      gl_render_msg(gl, msg);

   char buf[128];
   if (gfx_window_title(buf, sizeof(buf)))
      sdlwrap_wm_set_caption(buf);

   sdlwrap_swap_buffers();

   return true;
}

static void gl_free(void *data)
{
   gl_t *gl = data;

   gl_deinit_font(gl);
   gl_shader_deinit();
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDeleteTextures(2, gl->texture);

#ifdef HAVE_FBO
   if (gl->fbo_inited)
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      pglDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   }
#endif
   sdlwrap_destroy();

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
      sdlwrap_set_swap_interval(state ? 0 : 1, true);
   }
}

static void* gl_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
#ifdef _WIN32
   gfx_set_dwm();
#endif

   if (!sdlwrap_init())
      return NULL;

   const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
   assert(video_info);
   unsigned full_x = video_info->current_w;
   unsigned full_y = video_info->current_h;
   SSNES_LOG("Detecting desktop resolution %ux%u.\n", full_x, full_y);

   sdlwrap_set_swap_interval(video->vsync ? 1 : 0, false);

   unsigned win_width = video->width;
   unsigned win_height = video->height;
   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width = full_x;
      win_height = full_y;
   }

   if (!sdlwrap_set_video_mode(win_width, win_height,
            g_settings.video.force_16bit ? 15 : 0, video->fullscreen))
      return NULL;

   gfx_window_title_reset();
   char buf[128];
   if (gfx_window_title(buf, sizeof(buf)))
      sdlwrap_wm_set_caption(buf);

   // Remove that ugly mouse :D
   SDL_ShowCursor(SDL_DISABLE);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#if (defined(HAVE_XML) || defined(HAVE_CG)) && defined(_WIN32)
   // Win32 GL lib doesn't have some functions needed for XML shaders.
   // Need to load dynamically :(
   if (!load_gl_proc())
   {
      sdlwrap_destroy();
      return NULL;
   }
#endif

   gl_t *gl = calloc(1, sizeof(gl_t));
   if (!gl)
   {
      sdlwrap_destroy();
      return NULL;
   }

   gl->vsync = video->vsync;
   gl->fullscreen = video->fullscreen;
   
   gl->full_x = full_x;
   gl->full_y = full_y;
   gl->win_width = win_width;
   gl->win_height = win_height;

   SSNES_LOG("GL: Using resolution %ux%u\n", gl->win_width, gl->win_height);

   if (!gl_shader_init())
   {
      SSNES_ERR("Shader init failed.\n");
      sdlwrap_destroy();
      free(gl);
      return NULL;
   }

   SSNES_LOG("GL: Loaded %u program(s).\n", gl_shader_num());

   // Set up render to texture.
   gl_init_fbo(gl, 256 * video->input_scale, 256 * video->input_scale);
   
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
   gl->texture_fmt = video->rgb32 ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_SHORT_1_5_5_5_REV;
   gl->base_size = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   glEnable(GL_TEXTURE_2D);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_DITHER);
   glColor4f(1, 1, 1, 1);
   glClearColor(0, 0, 0, 1);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glGenTextures(2, gl->texture);

   for (unsigned i = 0; i < 2; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);
   }

   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glVertexPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), vertexes_flipped);

   memcpy(gl->tex_coords, tex_coords, sizeof(tex_coords));
   glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), gl->tex_coords);

   set_lut_texture_coords(tex_coords);

   gl->tex_w = 256 * video->input_scale;
   gl->tex_h = 256 * video->input_scale;

   // Empty buffer that we use to clear out the texture with on res change.
   gl->empty_buf = calloc(gl->tex_w * gl->tex_h, gl->base_size);

   for (unsigned i = 0; i < 2; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->tex_w);
      glTexImage2D(GL_TEXTURE_2D,
            0, GL_RGBA, gl->tex_w, gl->tex_h, 0, gl->texture_type,
            gl->texture_fmt, gl->empty_buf ? gl->empty_buf : NULL);
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   for (unsigned i = 0; i < 2; i++)
   {
      gl->last_width[i] = gl->tex_w;
      gl->last_height[i] = gl->tex_h;
   }

   gl->prev_info.tex = gl->texture[1 - gl->tex_index];
   gl->prev_info.input_size[0] = gl->tex_w;
   gl->prev_info.tex_size[0] = gl->tex_w;
   gl->prev_info.input_size[1] = gl->tex_h;
   gl->prev_info.tex_size[1] = gl->tex_h;
   memcpy(gl->prev_info.coord, tex_coords, sizeof(tex_coords)); 

   // Hook up SDL input driver to get SDL_QUIT events and RESIZE.
   sdl_input_t *sdl_input = input_sdl.init();
   if (sdl_input)
   {
      *input = &input_sdl;
      *input_data = sdl_input;
   }
   else
      *input = NULL;

   gl_init_font(gl, g_settings.video.font_path, g_settings.video.font_size);
      
   if (!gl_check_error())
   {
      sdlwrap_destroy();
      free(gl);
      return NULL;
   }

   return gl;
}

static bool gl_alive(void *data)
{
   gl_t *gl = data;
   return !gl->quitting;
}

static bool gl_focus(void *data)
{
   (void)data;
   return sdlwrap_window_has_focus();
}

#ifdef HAVE_XML
static bool gl_xml_shader(void *data, const char *path)
{
   gl_t *gl = data;

   //if (!gl_check_error())
   //   SSNES_WARN("Error happened before deinit!\n");


   //if (!gl_check_error())
   //   SSNES_WARN("Error happened in deinit!\n");

#ifdef HAVE_FBO
   if (gl->fbo_inited)
   {
      pglDeleteFramebuffers(gl->fbo_pass, gl->fbo);
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
      memset(gl->fbo, 0, sizeof(gl->fbo));
      gl->fbo_inited = false;
      gl->render_to_tex = false;
      gl->fbo_pass = 0;

      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   }
#endif

   gl_shader_deinit();

   //if (!gl_check_error())
   //   SSNES_WARN("Failed to deinit rendering path properly!\n");

   if (!gl_glsl_init(path))
      return false;

   // Set up render to texture.
   gl_init_fbo(gl, gl->tex_w, gl->tex_h);

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_shader_use(0);
   set_viewport(gl, gl->win_width, gl->win_height, false);
   gl_shader_use(1);
   set_viewport(gl, gl->win_width, gl->win_height, false);

   return true;
}
#endif

const video_driver_t video_gl = {
   .init = gl_init,
   .frame = gl_frame,
   .alive = gl_alive,
   .set_nonblock_state = gl_set_nonblock_state,
   .focus = gl_focus,
   .free = gl_free,
#ifdef HAVE_XML
   .xml_shader = gl_xml_shader,
#endif
   .ident = "gl"
};




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

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

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

#include "gl_common.h"

#ifdef HAVE_FREETYPE
#include "fonts.h"
#endif

static const GLfloat vertexes[] = {
   0, 0, 0,
   0, 1, 0,
   1, 1, 0,
   1, 0, 0
};

static const GLfloat tex_coords[] = {
   0, 1,
   0, 0,
   1, 0,
   1, 1
};

static const GLfloat fbo_tex_coords[] = {
   0, 0,
   0, 1,
   1, 1,
   1, 0
};

#ifdef _WIN32
static PFNGLGENFRAMEBUFFERSPROC pglGenFramebuffers = NULL;
static PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer = NULL;
static PFNGLFRAMEBUFFERTEXTURE2DPROC pglFramebufferTexture2D = NULL;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC pglCheckFramebufferStatus = NULL;
static PFNGLDELETEFRAMEBUFFERSPROC pglDeleteFramebuffers = NULL;

#define LOAD_SYM(sym) p##sym = ((void*)SDL_GL_GetProcAddress(#sym))
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

typedef struct gl
{
   bool vsync;
   GLuint texture;
   GLuint tex_filter;

   // Render-to-texture, multipass shaders
   GLuint fbo;
   GLuint fbo_texture;
   bool render_to_tex;
   unsigned fbo_width;
   unsigned fbo_height;
   bool fbo_inited;
   bool fbo_tex_filter;
   double fbo_scale_x;
   double fbo_scale_y;

   bool should_resize;
   bool quitting;
   bool keep_aspect;

   unsigned full_x, full_y;

   unsigned win_width;
   unsigned win_height;
   unsigned vp_width;
   unsigned vp_height;
   unsigned last_width;
   unsigned last_height;
   unsigned tex_w, tex_h;
   GLfloat tex_coords[8];
   GLfloat fbo_tex_coords[8];

   GLenum texture_type; // XBGR1555 or RGBA
   GLenum texture_fmt;
   unsigned base_size; // 2 or 4

#ifdef HAVE_FREETYPE
   font_renderer_t *font;
   GLuint font_tex;
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

static inline void gl_shader_use(unsigned index)
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
      unsigned out_width, unsigned out_height)
{
#ifdef HAVE_CG
   gl_cg_set_params(width, height, tex_width, tex_height, out_width, out_height);
#endif

#ifdef HAVE_XML
   gl_glsl_set_params(width, height, tex_width, tex_height, out_width, out_height);
#endif
}
///////////////////

//////////////// Message rendering
static inline void gl_init_font(gl_t *gl, const char *font_path, unsigned font_size)
{
#ifdef HAVE_FREETYPE
   if (strlen(font_path) > 0)
   {
      gl->font = font_renderer_new(font_path, font_size);
      if (gl->font)
      {
         glGenTextures(1, &gl->font_tex);
         glBindTexture(GL_TEXTURE_2D, gl->font_tex);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glBindTexture(GL_TEXTURE_2D, gl->texture);
      }
      else
         SSNES_WARN("Couldn't init font renderer with font \"%s\"...\n", font_path);
   }
#endif
}

static void gl_init_fbo(gl_t *gl, unsigned width, unsigned height)
{
   if (!g_settings.video.render_to_texture)
      return;

   if (!load_fbo_proc())
   {
      SSNES_ERR("Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   float scale_x = g_settings.video.fbo_scale_x;
   float scale_y = g_settings.video.fbo_scale_y;
   unsigned xscale = next_pow2(ceil(scale_x));
   unsigned yscale = next_pow2(ceil(scale_y));
   SSNES_LOG("Internal FBO scale: (%u, %u)\n", xscale, yscale);

   gl->fbo_width = width * xscale;
   gl->fbo_height = height * yscale;
   gl->fbo_scale_x = scale_x;
   gl->fbo_scale_y = scale_y;

   glGenTextures(1, &gl->fbo_texture);
   glBindTexture(GL_TEXTURE_2D, gl->fbo_texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, g_settings.video.second_pass_smooth ? GL_LINEAR : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, g_settings.video.second_pass_smooth ? GL_LINEAR : GL_NEAREST);

   void *tmp = calloc(gl->fbo_width * gl->fbo_height, sizeof(uint32_t));
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGBA, gl->fbo_width, gl->fbo_height, 0, GL_RGBA,
         GL_UNSIGNED_INT_8_8_8_8, tmp);
   free(tmp);
   glBindTexture(GL_TEXTURE_2D, 0);

   pglGenFramebuffers(1, &gl->fbo);
   pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo);
   pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture, 0);

   GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
   if (status == GL_FRAMEBUFFER_COMPLETE)
   {
      gl->fbo_inited = true;
      SSNES_LOG("Set up FBO @ %ux%u\n", gl->fbo_width, gl->fbo_height);
   }
   else
   {
      glDeleteTextures(1, &gl->fbo_texture);
      pglDeleteFramebuffers(1, &gl->fbo);
      SSNES_WARN("Failed to set up FBO. Two-pass shading will not work.\n");
   }
}

static inline void gl_deinit_font(gl_t *gl)
{
#ifdef HAVE_FREETYPE
   if (gl->font)
   {
      font_renderer_free(gl->font);
      glDeleteTextures(1, &gl->font_tex);
   }
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

static void gl_render_msg(gl_t *gl, const char *msg)
{
#ifdef HAVE_FREETYPE
   if (!gl->font)
      return;

   GLfloat font_vertex[12]; 

   // Deactivate custom shaders. Enable the font texture.
   gl_shader_use(0);
   glBindTexture(GL_TEXTURE_2D, gl->font_tex);
   glVertexPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), font_vertex);
   glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), tex_coords); // Use the static one (uses whole texture).

   // Need blending. 
   // Using fixed function pipeline here since we cannot guarantee presence of shaders (would be kinda overkill anyways).
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   struct font_output_list out;
   font_renderer_msg(gl->font, msg, &out);
   struct font_output *head = out.head;

   while (head != NULL)
   {
      GLfloat lx = (GLfloat)head->off_x / gl->vp_width + g_settings.video.msg_pos_x;
      GLfloat hx = (GLfloat)(head->off_x + head->width) / gl->vp_width + g_settings.video.msg_pos_x;
      GLfloat ly = (GLfloat)head->off_y / gl->vp_height + g_settings.video.msg_pos_y;
      GLfloat hy = (GLfloat)(head->off_y + head->height) / gl->vp_height + g_settings.video.msg_pos_y;

      font_vertex[0] = lx;
      font_vertex[1] = ly;
      font_vertex[3] = lx;
      font_vertex[4] = hy;
      font_vertex[6] = hx;
      font_vertex[7] = hy;
      font_vertex[9] = hx;
      font_vertex[10] = ly;

      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(head->pitch));
      glPixelStorei(GL_UNPACK_ROW_LENGTH, head->pitch);
      glTexImage2D(GL_TEXTURE_2D,
            0, GL_INTENSITY8, head->width, head->height, 0, GL_LUMINANCE,
            GL_UNSIGNED_BYTE, head->output);

      head = head->next;
      glFlush();
      glDrawArrays(GL_QUADS, 0, 4);
   }
   font_renderer_free_output(&out);

   // Go back to old rendering path.
   glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), gl->tex_coords);
   glVertexPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), vertexes);
   glBindTexture(GL_TEXTURE_2D, gl->texture);
   glDisable(GL_BLEND);
#endif
}

static void set_viewport(gl_t *gl, unsigned width, unsigned height, bool force_full)
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   GLuint out_width = width; 
   GLuint out_height = height;

   if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)gl->win_width / gl->win_height;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      // assume they are actually equal.
      if ( (int)(device_aspect*1000) > (int)(desired_aspect*1000) )
      {
         float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         glViewport(gl->win_width * (0.5 - delta), 0, 2.0 * gl->win_width * delta, gl->win_height);
         out_width = (int)(2.0 * gl->win_width * delta);
      }

      else if ( (int)(device_aspect*1000) < (int)(desired_aspect*1000) )
      {
         float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         glViewport(0, gl->win_height * (0.5 - delta), gl->win_width, 2.0 * gl->win_height * delta);
         out_height = (int)(2.0 * gl->win_height * delta);
      }
      else
         glViewport(0, 0, gl->win_width, gl->win_height);
   }
   else
      glViewport(0, 0, out_width, out_height);

   glOrtho(0, 1, 0, 1, -1, 1);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   gl_shader_set_proj_matrix();

   gl->vp_width = out_width;
   gl->vp_height = out_height;
}

static float tv_to_fps(const struct timeval *tv, const struct timeval *new_tv, int frames)
{
   float time = new_tv->tv_sec - tv->tv_sec + (new_tv->tv_usec - tv->tv_usec)/1000000.0;
   return frames/time;
}

static void show_fps(void)
{
   // Shows FPS in taskbar.
   static int frames = 0;
   static struct timeval tv;
   struct timeval new_tv;

   if (frames == 0)
      gettimeofday(&tv, NULL);

   if ((frames % 180) == 0 && frames > 0)
   {
      gettimeofday(&new_tv, NULL);
      struct timeval tmp_tv = tv;
      gettimeofday(&tv, NULL);
      char tmpstr[256] = {0};

      float fps = tv_to_fps(&tmp_tv, &new_tv, 180);

      snprintf(tmpstr, sizeof(tmpstr), "SSNES || FPS: %6.1f || Frames: %d", fps, frames);
      SDL_WM_SetCaption(tmpstr, NULL);
   }
   frames++;
}

static bool gl_frame(void *data, const void* frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   gl_t *gl = data;

   gl_shader_use(1);

   // Render to texture in first pass.
   if (gl->fbo_inited)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture);
      pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo);
      gl->render_to_tex = true;
      set_viewport(gl, width * gl->fbo_scale_x, height * gl->fbo_scale_y, true);
   }
   if (gl->should_resize)
   {
      gl->should_resize = false;
      SDL_SetVideoMode(gl->win_width, gl->win_height, 0, SDL_OPENGL | SDL_RESIZABLE | (g_settings.video.fullscreen ? SDL_FULLSCREEN : 0));

      if (!gl->render_to_tex)
         set_viewport(gl, gl->win_width, gl->win_height, false);
   }

   glClear(GL_COLOR_BUFFER_BIT);
   gl_shader_set_params(width, height, gl->tex_w, gl->tex_h, gl->vp_width, gl->vp_height);

   if (width != gl->last_width || height != gl->last_height) // res change. need to clear out texture.
   {
      gl->last_width = width;
      gl->last_height = height;
      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(pitch));
      glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->tex_w);

      void *tmp = calloc(1, gl->tex_w * gl->tex_h * gl->base_size);
      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, gl->tex_w, gl->tex_h, gl->texture_type,
            gl->texture_fmt, tmp);
      free(tmp);

      gl->tex_coords[0] = 0;
      gl->tex_coords[1] = (GLfloat)height / gl->tex_h;
      gl->tex_coords[2] = 0;
      gl->tex_coords[3] = 0;
      gl->tex_coords[4] = (GLfloat)width / gl->tex_w;
      gl->tex_coords[5] = 0;
      gl->tex_coords[6] = (GLfloat)width / gl->tex_w;
      gl->tex_coords[7] = (GLfloat)height / gl->tex_h;
   }


   glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
   glTexSubImage2D(GL_TEXTURE_2D,
         0, 0, 0, width, height, gl->texture_type,
         gl->texture_fmt, frame);

   glFlush();
   glDrawArrays(GL_QUADS, 0, 4);

   if (gl->fbo_inited)
   {
      // Render our FBO texture to back buffer.
      pglBindFramebuffer(GL_FRAMEBUFFER, 0);
      gl_shader_use(2);

      glClear(GL_COLOR_BUFFER_BIT);
      gl->render_to_tex = false;
      set_viewport(gl, gl->win_width, gl->win_height, false);
      gl_shader_set_params(width * gl->fbo_scale_x, height * gl->fbo_scale_y, gl->fbo_width, gl->fbo_height, gl->vp_width, gl->vp_height);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture);

      glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), gl->fbo_tex_coords);
      GLfloat xamt = (GLfloat)width * gl->fbo_scale_x / gl->fbo_width;
      GLfloat yamt = (GLfloat)height * gl->fbo_scale_y / gl->fbo_height;
      gl->fbo_tex_coords[3] = yamt;
      gl->fbo_tex_coords[4] = xamt;
      gl->fbo_tex_coords[5] = yamt;
      gl->fbo_tex_coords[6] = xamt;

      glDrawArrays(GL_QUADS, 0, 4);
      glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), gl->tex_coords);
   }

   if (msg)
      gl_render_msg(gl, msg);

   show_fps();
   SDL_GL_SwapBuffers();

   return true;
}

static void gl_free(void *data)
{
   gl_t *gl = data;

   gl_deinit_font(gl);
   gl_shader_deinit();
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDeleteTextures(1, &gl->texture);
   if (gl->fbo_inited)
   {
      glDeleteTextures(1, &gl->fbo_texture);
      pglDeleteFramebuffers(1, &gl->fbo);
   }
   SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void gl_set_nonblock_state(void *data, bool state)
{
   gl_t *gl = data;
   if (gl->vsync)
   {
      SSNES_LOG("GL VSync => %s\n", state ? "off" : "on");
#ifdef _WIN32
      static BOOL (APIENTRY *wgl_swap_interval)(int) = NULL;
      if (!wgl_swap_interval) wgl_swap_interval = (BOOL (APIENTRY*)(int)) SDL_GL_GetProcAddress("wglSwapIntervalEXT");
      if (wgl_swap_interval) wgl_swap_interval(state ? 0 : 1);
#else
      static int (*glx_swap_interval)(int) = NULL;
      if (!glx_swap_interval) glx_swap_interval = (int (*)(int))SDL_GL_GetProcAddress("glXSwapIntervalSGI");
      if (!glx_swap_interval) glx_swap_interval = (int (*)(int))SDL_GL_GetProcAddress("glXSwapIntervalMESA");
      if (glx_swap_interval) glx_swap_interval(state ? 0 : 1);
#endif
   }
}

static void* gl_init(video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (SDL_Init(SDL_INIT_VIDEO) < 0)
      return NULL;

   const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
   assert(video_info);
   unsigned full_x = video_info->current_w;
   unsigned full_y = video_info->current_h;
   SSNES_LOG("Detecting desktop resolution %ux%u.\n", full_x, full_y);

   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, video->vsync ? 1 : 0);

   if (!SDL_SetVideoMode(video->width, video->height, 0, SDL_OPENGL | SDL_RESIZABLE | (video->fullscreen ? SDL_FULLSCREEN : 0)))
      return NULL;

   int attr = 0;
   SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &attr);
   if (attr <= 0 && video->vsync)
      SSNES_WARN("GL VSync has not been enabled!\n");
   attr = 0;
   SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &attr);
   if (attr <= 0)
      SSNES_WARN("GL double buffer has not been enabled!\n");


   gl_t *gl = calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

   gl->full_x = full_x;
   gl->full_y = full_y;

   if (video->fullscreen)
   {
      gl->win_width = video->width ? video->width : gl->full_x;
      gl->win_height = video->height ? video->height : gl->full_y;
   }
   else
   {
      gl->win_width = video->width;
      gl->win_height = video->height;
   }

   SSNES_LOG("GL: Using resolution %ux%u\n", gl->win_width, gl->win_height);

   // Set up render to texture.
   gl_init_fbo(gl, 256 * video->input_scale, 256 * video->input_scale);

   gl->vsync = video->vsync;
   gl->keep_aspect = video->force_aspect;
   set_viewport(gl, gl->win_width, gl->win_height, false);

   if (!gl_shader_init())
   {
      SSNES_ERR("Shader init failed.\n");
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
      free(gl);
      return NULL;
   }

   // Remove that ugly mouse :D
   SDL_ShowCursor(SDL_DISABLE);

   if (video->smooth)
      gl->tex_filter = GL_LINEAR;
   else
      gl->tex_filter = GL_NEAREST;

   gl->texture_type = video->rgb32 ? GL_RGBA : GL_BGRA;
   gl->texture_fmt = video->rgb32 ? GL_UNSIGNED_INT_8_8_8_8 : GL_UNSIGNED_SHORT_1_5_5_5_REV;
   gl->base_size = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   glEnable(GL_TEXTURE_2D);
   glDisable(GL_DITHER);
   glDisable(GL_DEPTH_TEST);
   glColor4f(1, 1, 1, 1);
   glClearColor(0, 0, 0, 0);

   SDL_WM_SetCaption("SSNES", NULL);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glGenTextures(1, &gl->texture);

   glBindTexture(GL_TEXTURE_2D, gl->texture);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);

   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glVertexPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), vertexes);

   memcpy(gl->tex_coords, tex_coords, sizeof(tex_coords));
   glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), gl->tex_coords);

   gl->tex_w = 256 * video->input_scale;
   gl->tex_h = 256 * video->input_scale;

   void *tmp = calloc(1, gl->tex_w * gl->tex_h * gl->base_size);
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGBA, gl->tex_w, gl->tex_h, 0, gl->texture_type,
         gl->texture_fmt, tmp);
   free(tmp);

   gl->last_width = gl->tex_w;
   gl->last_height = gl->tex_h;

   // Hook up SDL input driver to get SDL_QUIT events and RESIZE.
   sdl_input_t *sdl_input = input_sdl.init();
   if (sdl_input)
   {
      sdl_input->quitting = &gl->quitting;
      sdl_input->should_resize = &gl->should_resize;
      sdl_input->new_width = &gl->win_width;
      sdl_input->new_height = &gl->win_height;
      *input = &input_sdl;
      *input_data = sdl_input;
   }
   else
      *input = NULL;

   gl_init_font(gl, g_settings.video.font_path, g_settings.video.font_size);
   
   if (!gl_check_error())
   {
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
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
   return (SDL_GetAppState() & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) == (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
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




/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define NO_SDL_GLEXT
#include "SDL.h"
#include "SDL_opengl.h"
#include "input/ssnes_sdl_input.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>

#ifndef _WIN32
#include <GL/glx.h>
#endif

#ifdef HAVE_CG
#include "shader_cg.h"
#endif

#ifdef HAVE_XML
#include "shader_glsl.h"
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

static bool keep_aspect = true;
static GLuint gl_width = 0, gl_height = 0;
typedef struct gl
{
   bool vsync;
   GLuint texture;
   GLuint tex_filter;

   bool should_resize;
   bool quitting;

   unsigned win_width;
   unsigned win_height;
   unsigned vp_width;
   unsigned vp_height;
   unsigned last_width;
   unsigned last_height;
   unsigned tex_w, tex_h;
   GLfloat tex_coords[8];
} gl_t;


static inline bool gl_shader_init(void)
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

   return true;
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

static void set_viewport(gl_t *gl)
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   GLuint out_width = gl->win_width, out_height = gl->win_height;

   if ( keep_aspect )
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
      glViewport(0, 0, gl->win_width, gl->win_height);

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

static bool gl_frame(void *data, const uint16_t* frame, int width, int height, int pitch)
{
   gl_t *gl = data;

   if (gl->should_resize)
   {
      gl->should_resize = false;
      SDL_SetVideoMode(gl->win_width, gl->win_height, 32, SDL_OPENGL | SDL_RESIZABLE | (g_settings.video.fullscreen ? SDL_FULLSCREEN : 0));
      set_viewport(gl);
   }

   glClear(GL_COLOR_BUFFER_BIT);

   gl_shader_set_params(width, height, gl->tex_w, gl->tex_h, gl_width, gl_height);

   if (width != gl->last_width || height != gl->last_height) // res change. need to clear out texture.
   {
      gl->last_width = width;
      gl->last_height = height;
      glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
      uint8_t *tmp = calloc(1, gl->tex_w * gl->tex_h * sizeof(uint16_t));
      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, gl->tex_w, gl->tex_h, GL_BGRA,
            GL_UNSIGNED_SHORT_1_5_5_5_REV, tmp);
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


   glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> 1);
   glTexSubImage2D(GL_TEXTURE_2D,
         0, 0, 0, width, height, GL_BGRA,
         GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);
   glDrawArrays(GL_QUADS, 0, 4);

   show_fps();
   glFlush();
   SDL_GL_SwapBuffers();

   return true;
}

static void gl_free(void *data)
{
   gl_t *gl = data;

   gl_shader_deinit();
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDeleteTextures(1, &gl->texture);
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
      if (!wgl_swap_interval)
         SSNES_WARN("SDL VSync toggling seems to be broken, attempting to use WGL VSync call directly instead.\n");
      if (!wgl_swap_interval) wgl_swap_interval = (BOOL (APIENTRY*)(int)) wglGetProcAddress("wglSwapIntervalEXT");
      if (wgl_swap_interval) wgl_swap_interval(state ? 0 : 1);
#else
      static int (*glx_swap_interval)(int) = NULL;
      if (!glx_swap_interval)
         SSNES_WARN("SDL VSync toggling seems to be broken, attempting to use GLX VSync call directly instead.\n");
      if (!glx_swap_interval) glx_swap_interval = (int (*)(int))glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalSGI");
      if (!glx_swap_interval) glx_swap_interval = (int (*)(int))glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalMESA");
      if (glx_swap_interval) glx_swap_interval(state ? 0 : 1);
#endif
   }
}

static void* gl_init(video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (SDL_Init(SDL_INIT_VIDEO) < 0)
      return NULL;

   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, video->vsync ? 1 : 0);

   if (!SDL_SetVideoMode(video->width, video->height, 32, SDL_OPENGL | SDL_RESIZABLE | (video->fullscreen ? SDL_FULLSCREEN : 0)))
      return NULL;
   
   int attr = 0;
   SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &attr);
   if (attr <= 0 && video->vsync)
      SSNES_WARN("GL VSync has not been enabled!\n");
   attr = 0;
   SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &attr);
   if (attr <= 0)
      SSNES_WARN("GL double buffer has not been enabled!\n");

   // Remove that ugly mouse :D
   SDL_ShowCursor(SDL_DISABLE);

   if (!gl_shader_init())
   {
      SSNES_ERR("Shader init failed.\n");
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
      return NULL;
   }

   gl_t *gl = calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

   gl->win_width = video->width;
   gl->win_height = video->height;
   gl->vsync = video->vsync;

   keep_aspect = video->force_aspect;
   if ( video->smooth )
      gl->tex_filter = GL_LINEAR;
   else
      gl->tex_filter = GL_NEAREST;

   set_viewport(gl);

   glEnable(GL_TEXTURE_2D);
   glDisable(GL_DITHER);
   glDisable(GL_DEPTH_TEST);
   glColor3f(1, 1, 1);
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
   uint8_t *tmp = calloc(1, gl->tex_w * gl->tex_h * sizeof(uint16_t));
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGBA, gl->tex_w, gl->tex_h, 0, GL_BGRA,
         GL_UNSIGNED_SHORT_1_5_5_5_REV, tmp);
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

   return gl;
}

static bool gl_alive(void *data)
{
   gl_t *gl = data;
   return !gl->quitting;
}

const video_driver_t video_gl = {
   .init = gl_init,
   .frame = gl_frame,
   .alive = gl_alive,
   .set_nonblock_state = gl_set_nonblock_state,
   .free = gl_free,
   .ident = "gl"
};




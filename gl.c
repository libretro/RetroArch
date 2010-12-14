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

#define GL_GLEXT_PROTOTYPES

#include "driver.h"
#include "config.h"
#include <GL/glfw.h>
#include <GL/glext.h>
#include <stdint.h>
#include "libsnes.hpp"
#include <stdio.h>
#include <sys/time.h>


#ifdef HAVE_CG
#include <Cg/cg.h>
#include <Cg/cgGL.h>
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
#ifdef HAVE_CG
static CGparameter cg_mvp_matrix;
#endif
static GLuint gl_width = 0, gl_height = 0;
typedef struct gl
{
   bool vsync;
#ifdef HAVE_CG
   CGcontext cgCtx;
   CGprogram cgFPrg;
   CGprogram cgVPrg;
   CGprofile cgFProf;
   CGprofile cgVProf;
   CGparameter cg_video_size, cg_texture_size, cg_output_size;
   CGparameter cg_Vvideo_size, cg_Vtexture_size, cg_Voutput_size; // Vertexes
#endif
   GLuint texture;
   GLuint tex_filter;
} gl_t;


static void glfw_input_poll(void *data)
{
   (void)data;
   glfwPollEvents();
}

#define BUTTONS_MAX 128
#define AXES_MAX 128

static int joypad_id[2];
static int joypad_buttons[2];
static int joypad_axes[2];
static bool joypad_inited = false;
static int joypad_count = 0;

static int init_joypads(int max_pads)
{
   // Finds the first (two) joypads that are alive
   int count = 0;
   for ( int i = GLFW_JOYSTICK_1; (i <= GLFW_JOYSTICK_LAST) && (count < max_pads); i++ )
   {
      if ( glfwGetJoystickParam(i, GLFW_PRESENT) == GL_TRUE )
      {
         joypad_id[count] = i;
         joypad_buttons[count] = glfwGetJoystickParam(i, GLFW_BUTTONS);
         if (joypad_buttons[count] > BUTTONS_MAX)
            joypad_buttons[count] = BUTTONS_MAX;
         joypad_axes[count] = glfwGetJoystickParam(i, GLFW_AXES);
         if (joypad_axes[count] > AXES_MAX)
            joypad_axes[count] = AXES_MAX;
         count++;
      }
   }
   joypad_inited = true;
   return count;
}

static bool glfw_is_pressed(int port_num, const struct snes_keybind *key, unsigned char *buttons, float *axes)
{
   if (glfwGetKey(key->key))
      return true;
   if (port_num >= joypad_count)
      return false;
   if (key->joykey >= 0)
      return (key->joykey < joypad_buttons[port_num] && buttons[key->joykey] == GLFW_PRESS);
   if (key->joyaxis >= 0)
      return (key->joyaxis < joypad_axes[port_num] && axes[key->joyaxis] >= AXIS_THRESHOLD);
   return (~key->joyaxis < joypad_axes[port_num] && axes[~key->joyaxis] <= -AXIS_THRESHOLD);
}

static int16_t glfw_input_state(void *data, const struct snes_keybind **binds, bool port, unsigned device, unsigned index, unsigned id)
{
   if ( device != SNES_DEVICE_JOYPAD )
      return 0;

   if ( !joypad_inited )
      joypad_count = init_joypads(2);

   int port_num = port ? 1 : 0;
   unsigned char buttons[BUTTONS_MAX];
   float axes[AXES_MAX];

   if ( joypad_count > port_num ) 
   {
      glfwGetJoystickButtons(joypad_id[port_num], buttons, joypad_buttons[port_num]);
      glfwGetJoystickPos(joypad_id[port_num], axes, joypad_axes[port_num]);
   }


   const struct snes_keybind *snes_keybinds;
   if (port == SNES_PORT_1)
      snes_keybinds = binds[0];
   else
      snes_keybinds = binds[1];

   // Checks if button is pressed, and sets fast-forwarding state
   bool pressed = false;
   for ( int i = 0; snes_keybinds[i].id != -1; i++ )
      if ( snes_keybinds[i].id == SNES_FAST_FORWARD_KEY )
         set_fast_forward_button(glfw_is_pressed(port_num, &snes_keybinds[i], buttons, axes));
      else if ( !pressed && snes_keybinds[i].id == (int)id )
         pressed = glfw_is_pressed(port_num, &snes_keybinds[i], buttons, axes);

   return pressed;
}

static void glfw_free_input(void *data)
{
   free(data);
}

static const input_driver_t input_glfw = {
   .poll = glfw_input_poll,
   .input_state = glfw_input_state,
   .free = glfw_free_input
};

static void GLFWCALL resize(int width, int height)
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   GLuint out_width = width, out_height = height;

   if ( keep_aspect )
   {
      float desired_aspect = 4.0/3;
      float device_aspect = (float)width / height;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      // assume they are actually equal.
      if ( (int)(device_aspect*1000) > (int)(desired_aspect*1000) )
      {
         float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         glViewport(width * (0.5 - delta), 0, 2.0 * width * delta, height);
         out_width = (int)(2.0 * width * delta);
      }

      else if ( (int)(device_aspect*1000) < (int)(desired_aspect*1000) )
      {
         float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         glViewport(0, height * (0.5 - delta), width, 2.0 * height * delta);
         out_height = (int)(2.0 * height * delta);
      }
      else
         glViewport(0, 0, width, height);
   }
   else
      glViewport(0, 0, width, height);

   glOrtho(0, 1, 0, 1, -1, 1);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
#ifdef HAVE_CG
   cgGLSetStateMatrixParameter(cg_mvp_matrix, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);
#endif
   gl_width = out_width;
   gl_height = out_height;
}

static float tv_to_fps(const struct timeval *tv, const struct timeval *new_tv, int frames)
{
   float time = new_tv->tv_sec - tv->tv_sec + (new_tv->tv_usec - tv->tv_usec)/1000000.0;
   return frames/time;
}

static inline void show_fps(void)
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
      struct timeval tmp_tv = {
         .tv_sec = tv.tv_sec,
         .tv_usec = tv.tv_usec
      };
      gettimeofday(&tv, NULL);
      char tmpstr[256] = {0};

      float fps = tv_to_fps(&tmp_tv, &new_tv, 180);

      snprintf(tmpstr, sizeof(tmpstr) - 1, "SSNES || FPS: %6.1f || Frames: %d", fps, frames);
      glfwSetWindowTitle(tmpstr);
   }
   frames++;
}

static bool gl_frame(void *data, const uint16_t* frame, int width, int height, int pitch)
{
   gl_t *gl = data;

   glClear(GL_COLOR_BUFFER_BIT);

#if HAVE_CG
   cgGLSetParameter2f(gl->cg_video_size, width, height);
   cgGLSetParameter2f(gl->cg_texture_size, width, height);
   cgGLSetParameter2f(gl->cg_output_size, gl_width, gl_height);

   cgGLSetParameter2f(gl->cg_Vvideo_size, width, height);
   cgGLSetParameter2f(gl->cg_Vtexture_size, width, height);
   cgGLSetParameter2f(gl->cg_Voutput_size, gl_width, gl_height);
#endif

   glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> 1);
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGBA, width, height, 0, GL_BGRA,
         GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);
   glDrawArrays(GL_QUADS, 0, 4);

   show_fps();
   glfwSwapBuffers();

   return true;
}

static void gl_free(void *data)
{
   gl_t *gl = data;
#ifdef HAVE_CG
   cgDestroyContext(gl->cgCtx);
#endif
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDeleteTextures(1, &gl->texture);
   glfwTerminate();
}

static void gl_set_nonblock_state(void *data, bool state)
{
   gl_t *gl = data;
   if (gl->vsync)
   {
      if (state)
         glfwSwapInterval(0);
      else
         glfwSwapInterval(1);
   }
}

static void* gl_init(video_info_t *video, const input_driver_t **input)
{
   gl_t *gl = malloc(sizeof(gl_t));
   if ( gl == NULL )
      return NULL;

   keep_aspect = video->force_aspect;
   if ( video->smooth )
      gl->tex_filter = GL_LINEAR;
   else
      gl->tex_filter = GL_NEAREST;

   glfwInit();

   int res;
   res = glfwOpenWindow(video->width, video->height, 0, 0, 0, 0, 0, 0, (video->fullscreen) ? GLFW_FULLSCREEN : GLFW_WINDOW);

   if (!res)
   {
      glfwTerminate();
      return NULL;
   }

   glfwSetWindowSizeCallback(resize);

   if ( video->vsync )
      glfwSwapInterval(1); // Force vsync
   else
      glfwSwapInterval(0);
   gl->vsync = video->vsync;

   glEnable(GL_TEXTURE_2D);
   glDisable(GL_DITHER);
   glDisable(GL_DEPTH_TEST);
   glColor3f(1, 1, 1);
   glClearColor(0, 0, 0, 0);

   glfwSetWindowTitle("SSNES");

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glGenTextures(1, &gl->texture);

   glBindTexture(GL_TEXTURE_2D, gl->texture);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);

   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glVertexPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), vertexes);
   glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), tex_coords);

#ifdef HAVE_CG
   gl->cgCtx = cgCreateContext();
   if (gl->cgCtx == NULL)
   {
      fprintf(stderr, "Failed to create Cg context\n");
      goto error;
   }
   gl->cgFProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);
   gl->cgVProf = cgGLGetLatestProfile(CG_GL_VERTEX);
   if (gl->cgFProf == CG_PROFILE_UNKNOWN || gl->cgVProf == CG_PROFILE_UNKNOWN)
   {
      fprintf(stderr, "Invalid profile type\n");
      goto error;
   }
   cgGLSetOptimalOptions(gl->cgFProf);
   cgGLSetOptimalOptions(gl->cgVProf);
   gl->cgFPrg = cgCreateProgramFromFile(gl->cgCtx, CG_SOURCE, cg_shader_path, gl->cgFProf, "main_fragment", 0);
   gl->cgVPrg = cgCreateProgramFromFile(gl->cgCtx, CG_SOURCE, cg_shader_path, gl->cgVProf, "main_vertex", 0);
   if (gl->cgFPrg == NULL || gl->cgVPrg == NULL)
   {
      CGerror err = cgGetError();
      fprintf(stderr, "CG error: %s\n", cgGetErrorString(err));
      goto error;
   }
   cgGLLoadProgram(gl->cgFPrg);
   cgGLLoadProgram(gl->cgVPrg);
   cgGLEnableProfile(gl->cgFProf);
   cgGLEnableProfile(gl->cgVProf);
   cgGLBindProgram(gl->cgFPrg);
   cgGLBindProgram(gl->cgVPrg);

   gl->cg_video_size = cgGetNamedParameter(gl->cgFPrg, "IN.video_size");
   gl->cg_texture_size = cgGetNamedParameter(gl->cgFPrg, "IN.texture_size");
   gl->cg_output_size = cgGetNamedParameter(gl->cgFPrg, "IN.output_size");
   gl->cg_Vvideo_size = cgGetNamedParameter(gl->cgVPrg, "IN.video_size");
   gl->cg_Vtexture_size = cgGetNamedParameter(gl->cgVPrg, "IN.texture_size");
   gl->cg_Voutput_size = cgGetNamedParameter(gl->cgVPrg, "IN.output_size");
   cg_mvp_matrix = cgGetNamedParameter(gl->cgVPrg, "modelViewProj");
   cgGLSetStateMatrixParameter(cg_mvp_matrix, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);
#endif

   *input = &input_glfw;
   return gl;
#ifdef HAVE_CG
error:
   free(gl);
   return NULL;
#endif
}

const video_driver_t video_gl = {
   .init = gl_init,
   .frame = gl_frame,
   .set_nonblock_state = gl_set_nonblock_state,
   .free = gl_free
};




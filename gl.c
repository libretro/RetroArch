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
#include <GL/glfw.h>
#include <stdint.h>
#include "libsnes.hpp"
#include <stdio.h>
#include <sys/time.h>

static GLuint texture;
static uint8_t *gl_buffer;
static bool keep_aspect = true;
static GLuint tex_filter;

static const GLfloat vertexes[] = {
   0, 0, 0,
   0, 1, 0,
   1, 1, 0,
   1, 0, 0
};

typedef struct gl
{
   bool vsync;
   unsigned real_x;
   unsigned real_y;
} gl_t;


static void glfw_input_poll(void *data)
{
   (void)data;
   glfwPollEvents();
}

#define BUTTONS_MAX 128

static int joypad_id[2];
static int joypad_buttons[2];
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
         count++;
      }
   }
   joypad_inited = true;
   return count;
}

static int16_t glfw_input_state(void *data, const struct snes_keybind **binds, bool port, unsigned device, unsigned index, unsigned id)
{
   if ( device != SNES_DEVICE_JOYPAD )
      return 0;

   if ( !joypad_inited )
      joypad_count = init_joypads(2);

   int port_num = port ? 1 : 0;
   unsigned char buttons[BUTTONS_MAX];

   if ( joypad_count > port_num )
      glfwGetJoystickButtons(joypad_id[port_num], buttons, joypad_buttons[port_num]);


   const struct snes_keybind *snes_keybinds;
   if (port == SNES_PORT_1)
      snes_keybinds = binds[0];
   else
      snes_keybinds = binds[1];

   // Finds fast forwarding state.
   for ( int i = 0; snes_keybinds[i].id != -1; i++ )
   {
      if ( snes_keybinds[i].id == SNES_FAST_FORWARD_KEY )
      {
         bool pressed = false;
         if ( glfwGetKey(snes_keybinds[i].key) )
            pressed = true;
         else if ( (joypad_count > port_num) && (snes_keybinds[i].joykey < joypad_buttons[port_num]) && (buttons[snes_keybinds[i].joykey] == GLFW_PRESS) )
            pressed = true;
         set_fast_forward_button(pressed);
         break;
      }
   }

   // Checks if button is pressed
   for ( int i = 0; snes_keybinds[i].id != -1; i++ )
   {
      if ( snes_keybinds[i].id == (int)id )
      {
         if ( glfwGetKey(snes_keybinds[i].key) )
            return 1;
         
         if ( (joypad_count > port_num) && (snes_keybinds[i].joykey < joypad_buttons[port_num]) && (buttons[snes_keybinds[i].joykey] == GLFW_PRESS) )
            return 1;
      }
   }

   return 0;
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
      }

      else if ( (int)(device_aspect*1000) < (int)(desired_aspect*1000) )
      {
         float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         glViewport(0, height * (0.5 - delta), width, 2.0 * height * delta);
      }
      else
         glViewport(0, 0, width, height);
   }
   else
      glViewport(0, 0, width, height);

   glOrtho(0, 1, 0, 1, -1, 1);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
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

   if ((frames % 60) == 0 && frames > 0)
   {
      gettimeofday(&new_tv, NULL);
      struct timeval tmp_tv = {
         .tv_sec = tv.tv_sec,
         .tv_usec = tv.tv_usec
      };
      gettimeofday(&tv, NULL);
      char tmpstr[256] = {0};

      float fps = tv_to_fps(&tmp_tv, &new_tv, 60);

      snprintf(tmpstr, sizeof(tmpstr) - 1, "SSNES || FPS: %6.1f || Frames: %d", fps, frames);
      glfwSetWindowTitle(tmpstr);
   }
   frames++;
}

static bool gl_frame(void *data, const uint16_t* frame, int width, int height, int pitch)
{
   gl_t *gl = data;

   glClear(GL_COLOR_BUFFER_BIT);

   GLfloat tex_coords[] = {
      0, (float)height/gl->real_y,
      0, 0,
      (float)width/gl->real_x, 0,
      (float)width/gl->real_x, (float)height/gl->real_y
   };
   glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), tex_coords);

   static int pitch_pixels = 1024;
   if (pitch_pixels != (pitch >> 1))
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> 1);
      pitch_pixels = pitch >> 1;
   }

   glBufferSubData(GL_ARRAY_BUFFER, 128, sizeof(tex_coords), tex_coords);
   glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, height * pitch, frame);

   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);
   glDrawArrays(GL_QUADS, 0, 4);
   
   show_fps();
   glfwSwapBuffers();

   return true;
}

static void gl_free(void *data)
{
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDeleteTextures(1, &texture);
   glfwTerminate();
   free(gl_buffer);
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
      tex_filter = GL_LINEAR;
   else
      tex_filter = GL_NEAREST;

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

   gl_buffer = calloc(1, 256 * 256 * 2 * video->input_scale * video->input_scale);
   if (!gl_buffer)
   {
      fprintf(stderr, "Couldn't allocate memory :<\n");
      exit(1);
   }

   gl->real_x = video->input_scale * 256;
   gl->real_y = video->input_scale * 256;

   glEnable(GL_TEXTURE_2D);
   glEnable(GL_DITHER);
   glDisable(GL_DEPTH_TEST);
   glColor3f(1, 1, 1);
   glClearColor(0, 0, 0, 0);

   glfwSetWindowTitle("SSNES");

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glGenTextures(1, &texture);

   glBindTexture(GL_TEXTURE_2D, texture);
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGB, 256 * video->input_scale, 256 * video->input_scale, 0, GL_BGRA,
         GL_UNSIGNED_SHORT_1_5_5_5_REV, gl_buffer);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_filter);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 1024);

   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glVertexPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), vertexes);

   *input = &input_glfw;
   return gl;
}

const video_driver_t video_gl = {
   .init = gl_init,
   .frame = gl_frame,
   .set_nonblock_state = gl_set_nonblock_state,
   .free = gl_free
};
   



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
#include <GL/glfw.h>
#include <stdint.h>
#include "libsnes.hpp"
#include <stdio.h>
#include <sys/time.h>

static GLuint texture;
static uint8_t *gl_buffer;
static bool keep_aspect = true;
static GLuint tex_filter;

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
      //float desired_aspect = 256.0/224.0;
      float desired_aspect = 296.0/224.0;
      float in_aspect = (float)width / height;

      if ( (int)(in_aspect*100) > (int)(desired_aspect*100) )
      {
         float delta = (in_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         glOrtho(0.5 - delta, 0.5 + delta, 0, 1, -1, 1);
      }

      else if ( (int)(in_aspect*100) < (int)(desired_aspect*100) )
      {
         float delta = (desired_aspect / in_aspect - 1.0) / 2.0 + 0.5;
         glOrtho(0, 1, 0.5 - delta, 0.5 + delta, -1, 1);
      }
      else
         glOrtho(0, 1, 0, 1, -1, 1);

   }
   else
      glOrtho(0, 1, 0, 1, -1, 1);

   glViewport(0, 0, width, height);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}

static float tv_to_fps(const struct timeval *tv, const struct timeval *new_tv, int frames)
{
   float time = new_tv->tv_sec - tv->tv_sec + (new_tv->tv_usec - tv->tv_usec)/1000000.0;
   return frames/time;
}

static bool gl_frame(void *data, const uint16_t* frame, int width, int height)
{
   gl_t *gl = data;

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_filter);

   glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);

   glLoadIdentity();
   glColor3f(1,1,1);

   glBegin(GL_QUADS);

   float h = 224.0/256.0;

   glTexCoord2f(0, h*height/gl->real_y);                       glVertex3i(0, 0, 0);
   glTexCoord2f(0, 0);                                         glVertex3i(0, 1, 0);
   glTexCoord2f((float)width/gl->real_x, 0);                   glVertex3i(1, 1, 0);
   glTexCoord2f((float)width/gl->real_x, h*height/gl->real_y); glVertex3i(1, 0, 0);

   glEnd();

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

   glfwSwapBuffers();

   return true;
}

static void gl_free(void *data)
{
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

   if ( !res )
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

   gl_buffer = malloc(256 * 256 * 2 * video->input_scale * video->input_scale);
   if ( !gl_buffer )
   {
      fprintf(stderr, "Couldn't allocate memory :<\n");
      exit(1);
   }

   gl->real_x = video->input_scale * 256;
   gl->real_y = video->input_scale * 224;

   glEnable(GL_TEXTURE_2D);
   glEnable(GL_DITHER);
   glEnable(GL_DEPTH_TEST);

   glfwSetWindowTitle("SSNES");

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 256 * video->input_scale);
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGB, 256 * video->input_scale, 256 * video->input_scale, 0, GL_BGRA,
         GL_UNSIGNED_SHORT_1_5_5_5_REV, gl_buffer);

   *input = &input_glfw;
   return gl;
}

const video_driver_t video_gl = {
   .init = gl_init,
   .frame = gl_frame,
   .set_nonblock_state = gl_set_nonblock_state,
   .free = gl_free
};
   



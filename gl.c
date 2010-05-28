#include "driver.h"
#include <GL/glfw.h>
#include <stdint.h>
#include "config.h"
#include "libsnes.hpp"
#include <stdio.h>

static GLuint texture;
static uint8_t *gl_buffer;

typedef struct gl
{
   int foo;
} gl_t;


static void glfw_input_poll(void *data)
{
   (void)data;
   glfwPollEvents();
}

static int16_t glfw_input_state(void *data, bool port, unsigned device, unsigned index, unsigned id)
{
   (void)data;

    if ( port != 0 || device != SNES_DEVICE_JOYPAD )
      return 0;

   int i;
   int joypad_id = -1;
   int joypad_buttons = -1;

   // Finds the first joypad that's alive
   for ( i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++ )
   {
      if ( glfwGetJoystickParam(i, GLFW_PRESENT) == GL_TRUE )
      {
         joypad_id = i;
         joypad_buttons = glfwGetJoystickParam(i, GLFW_BUTTONS);
         break;
      }
   }

   unsigned char buttons[128];
   if ( joypad_id != -1 )
   {
      glfwGetJoystickButtons(joypad_id, buttons, joypad_buttons);
   }

   for ( i = 0; snes_keybinds[i].id != -1; i++ )
   {
      if ( snes_keybinds[i].id == (int)id )
      {
         if ( glfwGetKey(snes_keybinds[i].key ))
            return 1;
         
         if ( snes_keybinds[i].joykey < joypad_buttons && buttons[snes_keybinds[i].joykey] == GLFW_PRESS )
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

   if ( force_aspect )
   {
      float desired_aspect = 256.0/224.0;
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

static bool gl_frame(void *data, const uint16_t* frame, int width, int height)
{
   (void)data;

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);

   glLoadIdentity();
   glColor3f(1,1,1);

   glBegin(GL_QUADS);

   float h = 224.0/256.0;

   glTexCoord2f(0, h); glVertex3i(0, 0, 0);
   glTexCoord2f(0, 0); glVertex3i(0, 1, 0);
   glTexCoord2f(1, 0); glVertex3i(1, 1, 0);
   glTexCoord2f(1, h); glVertex3i(1, 0, 0);

   glEnd();

   glfwSwapBuffers();

   return true;
}

static void gl_free(void *data)
{
   glfwTerminate();
   free(gl_buffer);
}

static void* gl_init(int width, int height, bool fullscreen, bool vsync, input_driver_t **input)
{
   gl_t *foo = malloc(sizeof(gl_t));
   if ( foo == NULL )
      return NULL;

   glfwInit();

   int res;
   res = glfwOpenWindow(width, height, 0, 0, 0, 0, 0, 0, (fullscreen) ? GLFW_FULLSCREEN : GLFW_WINDOW);

   if ( !res )
   {
      glfwTerminate();
      return NULL;
   }

   glfwSetWindowSizeCallback(resize);

   if ( vsync )
      glfwSwapInterval(1); // Force vsync
   else
      glfwSwapInterval(0);

   gl_buffer = malloc(256 * 256 * 2);
   if ( !gl_buffer )
   {
      fprintf(stderr, "Couldn't allocate memory :<\n");
      exit(1);
   }

   glEnable(GL_TEXTURE_2D);
   glEnable(GL_DITHER);
   glEnable(GL_DEPTH_TEST);

   glfwSetWindowTitle("SSNES");

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 256);
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGB, 256, 256, 0, GL_RGBA,
         GL_UNSIGNED_SHORT_1_5_5_5_REV, gl_buffer);

   *input = (input_driver_t*)&input_glfw;
   return foo;
}

const video_driver_t video_gl = {
   .init = gl_init,
   .frame = gl_frame,
   .free = gl_free
};
   



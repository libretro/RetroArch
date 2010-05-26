#include <GL/glfw.h>
#include <samplerate.h>
#include "libsnes.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <rsound.h>
#include <string.h>
#include "config.h"

///// RSound
static rsound_t *rd = NULL;
static void audio_write(const void *data, size_t size);


///// samplerate
static SRC_STATE* source = NULL;

///// GL
static GLuint texture;
static uint8_t* gl_buffer;
static void GLFWCALL resize(int width, int height);
static void init_gl(void);
static void uninit_gl(void);
static void uninit_audio(void);

static void uninit_gl(void)
{
   glfwTerminate();
   free(gl_buffer);
}

static void init_gl(void)
{
   glfwInit();

   int res;
   if ( fullscreen )
      res = glfwOpenWindow(fullscreen_x, fullscreen_y, 0, 0, 0, 0, 0, 0, GLFW_FULLSCREEN);

   else
      res = glfwOpenWindow(256 * xscale, 224 * yscale, 0, 0, 0, 0, 0, 0, GLFW_WINDOW);

   if ( !res )
   {
      glfwTerminate();
      exit(1);
   }

   glfwSetWindowSizeCallback(resize);

   if ( vsync )
      glfwSwapInterval(1); // Force vsync
   else
      glfwSwapInterval(0);

   gl_buffer = malloc(256 * 256 * 4);
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
         GL_UNSIGNED_INT_8_8_8_8, gl_buffer);
}

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

static void init_audio(void)
{
   rsd_init(&rd);
   int channels = 2;
   int rate = out_rate;
   int format = RSD_S16_LE;
   int latency = 80;
   rsd_set_param(rd, RSD_CHANNELS, &channels);
   rsd_set_param(rd, RSD_SAMPLERATE, &rate);
   rsd_set_param(rd, RSD_FORMAT, &format);
   rsd_set_param(rd, RSD_LATENCY, &latency);
   if ( rsd_start(rd) < 0 )
   {
      fprintf(stderr, "FAILED TO START RSD\n");
      exit(1);
   }

   int err;
   source = src_new(SRC_SINC_MEDIUM_QUALITY, 2, &err);
   if ( source == NULL )
   {
      fprintf(stderr, "Couldn't init libsamplerate\n");
      exit(1);
   }

   src_set_ratio(source, (double)out_rate / (double)in_rate);

}

static void uninit_audio(void)
{
   if ( rd )
   {
      rsd_stop(rd);
      rsd_free(rd);
      rd = NULL;
   }
   if ( source )
   {
      src_delete(source);
      source = NULL;
   }
}

static void video_refresh_GL(const uint16_t* data, unsigned width, unsigned height)
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

   uint32_t output[width*height];
   int y;
   for ( y = 0; y < height; y++ )
   {
      const uint16_t *src = data + y * 1024;
      uint32_t *dst = output + y * width;

      int x;
      for ( x = 0; x < width; x++ )
      {
         uint16_t pixel = *src++;
         int r = (pixel & 0x001f) << 3;
         int g = (pixel & 0x03e0) >> 2;
         int b = (pixel & 0x7c00) >> 7;
         *dst++ = (r << 24) | (g << 16) | (b << 8);
      }
   }

   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, output);

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
}

static void audio_refresh(uint16_t left, uint16_t right)
{
   static float data[256];
   static int data_ptr = 0;

   data[data_ptr++] = (float)(*(int16_t*)&left)/0x7FFF; 
   data[data_ptr++] = (float)(*(int16_t*)&right)/0x7FFF;

   if ( data_ptr == 256 )
   {
      float outsamples[2048];
      int16_t temp_outsamples[4096];

      SRC_DATA src_data;

      src_data.data_in = data;
      src_data.data_out = outsamples;
      src_data.input_frames = 128;
      src_data.output_frames = 1024;
      src_data.end_of_input = 0;
      src_data.src_ratio = (double)out_rate / (double)in_rate;

      src_process(source, &src_data);

      src_float_to_short_array(outsamples, temp_outsamples, src_data.output_frames_gen * 4);

      audio_write(temp_outsamples, src_data.output_frames_gen * 4);
      data_ptr = 0;
   }
}

static void audio_write(const void* data, size_t size)
{
   if ( size == 0 )
   {
      fprintf(stderr, "WTF, no data?!\n");
      return;
   }

   rsd_delay_wait(rd);
   if ( rsd_write(rd, data, size) == 0 )
      fprintf(stderr, "WTF!!\n");

   if ( rsd_delay_ms(rd) < 40 )
   {
      int ms = 30;
      size_t size = (ms * out_rate * 4) / 1000;
      void *temp = calloc(1, size);
      rsd_write(rd, temp, size);
      free(temp);
   }
}

static void snes_input_poll(void)
{
   glfwPollEvents();
}

static int16_t input_state(bool port, unsigned device, unsigned index, unsigned id)
{

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


int main(int argc, char *argv[])
{
   if ( argc != 2 )
   {
      fprintf(stderr, "Usage: %s file\n", argv[0]);
      exit(1);
   }
   char savefile_name[strlen(argv[1]+5)];
   strcpy(savefile_name, argv[1]);
   strcat(savefile_name, ".sav");

   init_gl();

   snes_init();

   snes_set_video_refresh(video_refresh_GL);
   snes_set_audio_sample(audio_refresh);
   snes_set_input_poll(snes_input_poll);
   snes_set_input_state(input_state);

   init_audio();

   FILE *file = fopen(argv[1], "rb");
   if ( file == NULL )
      exit(1);

   fseek(file, 0, SEEK_END);
   long length = ftell(file);
   rewind(file);
   if ((length & 0x7fff) == 512)
   {
      length -= 512;
      fseek(file, 512, SEEK_SET);
   }

   void *rom_buf = malloc(length);
   if ( rom_buf == NULL )
   {
      fprintf(stderr, "Couldn't allocate memory!\n");
      exit(1);
   }

   if ( fread(rom_buf, 1, length, file) < length )
   {
      fprintf(stderr, "Didn't read whole file.\n");
      exit(1);
   }

   fclose(file);

   snes_load_cartridge_normal(NULL, rom_buf, length);

   free(rom_buf);

   unsigned serial_size = snes_serialize_size();
   uint8_t *serial_data = malloc(serial_size);
   snes_serialize(serial_data, serial_size);

   file = fopen(savefile_name, "rb");
   if ( file != NULL )
   {
      fread(serial_data, 1, serial_size, file);
      fclose(file);
      snes_unserialize(serial_data, serial_size);
      snes_reset();
   }

   for(;;)
   {
      int quitting = glfwGetKey(GLFW_KEY_ESC) || !glfwGetWindowParam(GLFW_OPENED);
      
      if ( quitting )
         break;

      if ( glfwGetKey( SAVE_STATE_KEY ))
         snes_serialize(serial_data, serial_size);
      else if ( glfwGetKey( LOAD_STATE_KEY ) )
         snes_unserialize(serial_data, serial_size);
      else if ( glfwGetKey( TOGGLE_FULLSCREEN ) )
      {
         fullscreen = !fullscreen;
         uninit_gl();
         init_gl();
         uninit_audio();
         init_audio();
      }

      snes_run();
   }

   file = fopen(savefile_name, "wb");
   if ( file != NULL )
   {
      snes_serialize(serial_data, serial_size);
      fwrite(serial_data, 1, serial_size, file);
      fclose(file);
   }

   snes_unload();
   snes_term();

   uninit_gl();
   uninit_audio();

   return 0;
}

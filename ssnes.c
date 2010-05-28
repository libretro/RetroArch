#include <stdbool.h>
#include <GL/glfw.h>
#include <samplerate.h>
#include "libsnes.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <rsound.h>
#include <string.h>
#include "config.h"
#include "driver.h"

static bool video_active = true;
static bool audio_active = true;

static SRC_STATE* source = NULL;

//////////////////////////////////////////////// Backends
extern const audio_driver_t audio_rsound;
extern const video_driver_t video_gl;
////////////////////////////////////////////////

static driver_t driver = {
   .audio = &audio_rsound,
   .video = &video_gl
};

static void init_drivers(void);
static void uninit_drivers(void);

static void init_video_input(void);
static void uninit_video_input(void);
static void init_audio(void);
static void uninit_audio(void);

static void load_state(const char* path, uint8_t* data, size_t size);
static void write_state(const char* path, uint8_t* data, size_t size);

static void init_drivers(void)
{
   init_video_input();
   init_audio();
}

static void uninit_drivers(void)
{
   uninit_video_input();
   uninit_audio();
}

static void init_audio(void)
{
   driver.audio_data = driver.audio->init(audio_device, out_rate, out_latency);
   if ( driver.audio_data == NULL )
      audio_active = false;

   int err;
   source = src_new(SRC_SINC_MEDIUM_QUALITY, 2, &err);
}

static void uninit_audio(void)
{
   if ( driver.audio_data && driver.audio )
      driver.audio->free(driver.audio_data);

   if ( source )
      src_delete(source);
}

static void init_video_input(void)
{
   driver.video_data = driver.video->init((fullscreen) ? fullscreen_x : (256 * xscale), (fullscreen) ? fullscreen_y : (224 * yscale), fullscreen, vsync, (input_driver_t**)&(driver.input));

   if ( driver.video_data == NULL )
   {
      exit(1);
   }

   if ( driver.input != NULL )
   {
      driver.input_data = driver.video_data;
   }
   else
   {
      driver.input_data = driver.input->init();
      if ( driver.input_data == NULL )
         exit(1);
   }
}

static void uninit_video_input(void)
{
   if ( driver.video_data && driver.video )
      driver.video->free(driver.video_data);

   if ( driver.input_data != driver.video_data && driver.input )
      driver.input->free(driver.input_data);
}

static void video_frame(const uint16_t *data, unsigned width, unsigned height)
{
   if ( !video_active )
      return;

   uint16_t output[width * height];

   int y;
   for ( y = 0; y < height; y++ )
   {
      const uint16_t *src = data + y * 1024;
      uint16_t *dst = output + y * width;

      memcpy(dst, src, width * sizeof(uint16_t));
   }

   if ( !driver.video->frame(driver.video_data, output, width, height) )
      video_active = false;
}

static void audio_sample(uint16_t left, uint16_t right)
{
   if ( !audio_active )
      return;

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

      if ( driver.audio->write(driver.audio_data, temp_outsamples, src_data.output_frames_gen * 4) < 0 )
         audio_active = false;

      data_ptr = 0;
   }
}

static void input_poll(void)
{
   driver.input->poll(driver.input_data);
}

static int16_t input_state(bool port, unsigned device, unsigned index, unsigned id)
{
   return driver.input->input_state(driver.input_data, port, device, index, id);
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

   init_drivers();

   snes_init();

   snes_set_video_refresh(video_frame);
   snes_set_audio_sample(audio_sample);
   snes_set_input_poll(input_poll);
   snes_set_input_state(input_state);


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

   load_state(savefile_name, serial_data, serial_size);
   snes_reset();

   ///// TODO: Modular friendly!!!
   for(;;)
   {
      int quitting = glfwGetKey(GLFW_KEY_ESC) || !glfwGetWindowParam(GLFW_OPENED);
      
      if ( quitting )
         break;

      if ( glfwGetKey( SAVE_STATE_KEY ))
      {
         write_state(savefile_name, serial_data, serial_size);
      }

      else if ( glfwGetKey( LOAD_STATE_KEY ) )
         load_state(savefile_name, serial_data, serial_size);

      else if ( glfwGetKey( TOGGLE_FULLSCREEN ) )
      {
         fullscreen = !fullscreen;
         uninit_drivers();
         init_drivers();
      }

      snes_run();
   }

   write_state(savefile_name, serial_data, serial_size);

   snes_unload();
   snes_term();

   uninit_drivers();

   return 0;
}

static void write_state(const char* path, uint8_t* data, size_t size)
{
   FILE *file = fopen(path, "wb");
   if ( file != NULL )
   {
      fprintf(stderr, "SSNES: Saving state. Size: %d bytes.\n", (int)size);
      snes_serialize(data, size);
      if ( fwrite(data, 1, size, file) != size )
         fprintf(stderr, "SSNES [WARN]: Did not save state properly.");
      fclose(file);
   }
}

static void load_state(const char* path, uint8_t* data, size_t size)
{
   FILE *file = fopen(path, "rb");
   if ( file != NULL )
   {
      fprintf(stderr, "SSNES: Loading state. Size: %d bytes.\n", (int)size);
      if ( fread(data, 1, size, file) != size )
         fprintf(stderr, "SSNES [WARN]: Did not load state properly.");
      fclose(file);
      snes_unserialize(data, size);
   }
}

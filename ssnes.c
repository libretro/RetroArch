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


#include <stdbool.h>
#include <GL/glfw.h>
#include <samplerate.h>
#include <libsnes.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "driver.h"
#include "hqflt/pastlib.h"
#include "hqflt/grayscale.h"

static bool video_active = true;
static bool audio_active = true;

static SRC_STATE* source = NULL;

//////////////////////////////////////////////// Backends
extern const audio_driver_t audio_rsound;
extern const audio_driver_t audio_oss;
extern const audio_driver_t audio_alsa;
extern const audio_driver_t audio_roar;
extern const video_driver_t video_gl;
////////////////////////////////////////////////

static driver_t driver = {
#if VIDEO_DRIVER == VIDEO_GL
   .video = &video_gl,
#else
#error "Define a valid video driver in config.h"
#endif

#if AUDIO_DRIVER == AUDIO_RSOUND
   .audio = &audio_rsound,
#elif AUDIO_DRIVER == AUDIO_OSS
   .audio = &audio_oss,
#elif AUDIO_DRIVER == AUDIO_ALSA
   .audio = &audio_alsa,
#elif AUDIO_DRIVER == AUDIO_ROAR
   .audio = &audio_roar,
#else
#error "Define a valid audio driver in config.h"
#endif
};

static void init_drivers(void);
static void uninit_drivers(void);

static void init_video_input(void);
static void uninit_video_input(void);
static void init_audio(void);
static void uninit_audio(void);

static void load_state(const char* path, uint8_t* data, size_t size);
static void write_file(const char* path, uint8_t* data, size_t size);
static void load_save_file(const char* path, int type);
static void save_file(const char* path, int type);


// To avoid continous switching if we hold the button down, we require that the button must go from pressed, unpressed back to pressed to be able to toggle between then.

#define AUDIO_CHUNK_SIZE_BLOCKING 64
#define AUDIO_CHUNK_SIZE_NONBLOCKING 2048 // So we don't get complete line-noise when fast-forwarding audio.
static size_t audio_chunk_size = AUDIO_CHUNK_SIZE_BLOCKING;
void set_fast_forward_button(bool new_button_state)
{
   static bool old_button_state = false;
   static bool syncing_state = false;
   if (new_button_state && !old_button_state)
   {
      syncing_state = !syncing_state;
      if (video_active)
         driver.video->set_nonblock_state(driver.video_data, syncing_state);
      if (audio_active)
         driver.audio->set_nonblock_state(driver.audio_data, (audio_sync) ? syncing_state : true);
      if (syncing_state)
         audio_chunk_size = AUDIO_CHUNK_SIZE_NONBLOCKING;
      else
         audio_chunk_size = AUDIO_CHUNK_SIZE_BLOCKING;
   }
   old_button_state = new_button_state;
}

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
   if (!audio_enable)
   {
      audio_active = false;
      return;
   }

   driver.audio_data = driver.audio->init(audio_device, out_rate, out_latency);
   if ( driver.audio_data == NULL )
      audio_active = false;

   if (!audio_sync && audio_active)
      driver.audio->set_nonblock_state(driver.audio_data, true);

   int err;
   source = src_new(SAMPLERATE_QUALITY, 2, &err);
   if (!source)
      audio_active = false;
}

static void uninit_audio(void)
{
   if (!audio_enable)
   {
      audio_active = false;
      return;
   }

   if ( driver.audio_data && driver.audio )
      driver.audio->free(driver.audio_data);

   if ( source )
      src_delete(source);
}

static void init_video_input(void)
{
   int scale;

   // We multiply scales with 2 to allow for hi-res games.
#if VIDEO_FILTER == FILTER_NONE
   scale = 2;
#elif VIDEO_FILTER == FILTER_HQ2X
   scale = 4;
#elif VIDEO_FILTER == FILTER_HQ4X
   scale = 8;
#elif VIDEO_FILTER == FILTER_GRAYSCALE
   scale = 2;
#else
   scale = 2;
#endif

   video_info_t video = {
      .width = (fullscreen) ? fullscreen_x : (296 * xscale),
      .height = (fullscreen) ? fullscreen_y : (224 * yscale),
      .fullscreen = fullscreen,
      .vsync = vsync,
      .force_aspect = force_aspect,
      .smooth = video_smooth,
      .input_scale = scale,
   };

   driver.video_data = driver.video->init(&video, &(driver.input));

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

static inline void process_frame (uint16_t * restrict out, const uint16_t * restrict in, unsigned width, unsigned height)
{
   for ( int y = 0; y < height; y++ )
   {
      const uint16_t *src = in + y * 1024;
      uint16_t *dst = out + y * width;

      memcpy(dst, src, width * sizeof(uint16_t));
   }
}

// libsnes: 0.065
// Format received is 16-bit 0RRRRRGGGGGBBBBB
static void video_frame(const uint16_t *data, unsigned width, unsigned height)
{
   if ( !video_active )
      return;

#if VIDEO_FILTER == FILTER_HQ2X
   uint16_t outputHQ2x[width * height * 2 * 2];
#elif VIDEO_FILTER == FILTER_HQ4X
   uint16_t outputHQ4x[width * height * 4 * 4];
#endif
   uint16_t output[width * height];

   process_frame(output, data, width, height);

#if VIDEO_FILTER == FILTER_NONE
   if ( !driver.video->frame(driver.video_data, output, width, height) )
      video_active = false;
#elif VIDEO_FILTER == FILTER_HQ2X
   ProcessHQ2x(output, outputHQ2x);
   if ( !driver.video->frame(driver.video_data, outputHQ2x, width * 2, height * 2) )
      video_active = false;
#elif VIDEO_FILTER == FILTER_HQ4X
   ProcessHQ4x(output, outputHQ4x);
   if ( !driver.video->frame(driver.video_data, outputHQ4x, width * 4, height * 4) )
      video_active = false;
#elif VIDEO_FILTER == FILTER_GRAYSCALE
   grayscale_filter(output, width, height);
   if ( !driver.video->frame(driver.video_data, output, width, height) )
      video_active = false;
#else
   if ( !driver.video->frame(driver.video_data, output, width, height) )
      video_active = false;
#endif

}

static void audio_sample(uint16_t left, uint16_t right)
{
   if ( !audio_active )
      return;

   static float data[AUDIO_CHUNK_SIZE_NONBLOCKING];
   static int data_ptr = 0;

   data[data_ptr++] = (float)(*(int16_t*)&left)/0x7FFF; 
   data[data_ptr++] = (float)(*(int16_t*)&right)/0x7FFF;

   if ( data_ptr >= audio_chunk_size )
   {
      float outsamples[audio_chunk_size * 16];
      int16_t temp_outsamples[audio_chunk_size * 16];

      SRC_DATA src_data;

      src_data.data_in = data;
      src_data.data_out = outsamples;
      src_data.input_frames = audio_chunk_size / 2;
      src_data.output_frames = audio_chunk_size * 8;
      src_data.end_of_input = 0;
      src_data.src_ratio = (double)out_rate / (double)in_rate;

      src_process(source, &src_data);

      src_float_to_short_array(outsamples, temp_outsamples, src_data.output_frames_gen * 2);

      if ( driver.audio->write(driver.audio_data, temp_outsamples, src_data.output_frames_gen * 4) < 0 )
      {
         fprintf(stderr, "SSNES [ERROR]: Audio backend failed to write. Will continue without sound.\n");
         audio_active = false;
      }

      data_ptr = 0;
   }
}

static void input_poll(void)
{
   driver.input->poll(driver.input_data);
}

static int16_t input_state(bool port, unsigned device, unsigned index, unsigned id)
{
   return driver.input->input_state(driver.input_data, snes_keybinds, port, device, index, id);
}


int main(int argc, char *argv[])
{
   if ( argc != 2 )
   {
      fprintf(stderr, "Usage: %s file\n", argv[0]);
      exit(1);
   }

   fprintf(stderr, "SSNES: Opening file: \"%s\"\n", argv[1]);
   FILE *file = fopen(argv[1], "rb");
   if ( file == NULL )
   {
      fprintf(stderr, "SSNES [ERROR]: Could not open file: \"%s\"\n", argv[1]);
      exit(1);
   }

   const char *statefile_tok = NULL;
   char statefile_name[strlen(argv[1])+strlen("state")+1];
   char savefile_name_rtc[strlen(argv[1])+strlen("rtc")+1];
   char savefile_name_srm[strlen(argv[1])+strlen("srm")+1];

   statefile_tok = strtok(argv[1], ".");
   strcpy(statefile_name, statefile_tok);
   strcat(statefile_name, ".state");

   
   strcpy(savefile_name_rtc, statefile_tok);
   strcat(savefile_name_rtc, ".rtc");
   strcpy(savefile_name_srm, statefile_tok);
   strcat(savefile_name_srm, ".srm");

   init_drivers();

   snes_init();

   snes_set_video_refresh(video_frame);
   snes_set_audio_sample(audio_sample);
   snes_set_input_poll(input_poll);
   snes_set_input_state(input_state);


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
      fprintf(stderr, "SSNES [ERROR] :: Couldn't allocate memory!\n");
      goto error;
   }

   if ( fread(rom_buf, 1, length, file) < length )
   {
      fprintf(stderr, "SSNES [ERROR] :: Didn't read whole file.\n");
      goto error;
   }

   fclose(file);

   if (!snes_load_cartridge_normal(NULL, rom_buf, length))
   {
      fprintf(stderr, "SSNES [ERROR] :: ROM file \"%s\" is not valid!\n", argv[1]);;
      goto error;
   }

   free(rom_buf);

   unsigned serial_size = snes_serialize_size();
   uint8_t *serial_data = malloc(serial_size);

   load_save_file(savefile_name_srm, SNES_MEMORY_CARTRIDGE_RAM);
   load_save_file(savefile_name_rtc, SNES_MEMORY_CARTRIDGE_RTC);

   ///// TODO: Modular friendly!!!
   for(;;)
   {
      int quitting = glfwGetKey(GLFW_KEY_ESC) || !glfwGetWindowParam(GLFW_OPENED);
      
      if ( quitting )
         break;

      if ( glfwGetKey( SAVE_STATE_KEY ))
      {
         write_file(statefile_name, serial_data, serial_size);
      }

      else if ( glfwGetKey( LOAD_STATE_KEY ) )
         load_state(statefile_name, serial_data, serial_size);

      else if ( glfwGetKey( TOGGLE_FULLSCREEN ) )
      {
         fullscreen = !fullscreen;
         uninit_drivers();
         init_drivers();
      }

      snes_run();
   }

   save_file(savefile_name_srm, SNES_MEMORY_CARTRIDGE_RAM);
   save_file(savefile_name_rtc, SNES_MEMORY_CARTRIDGE_RTC);

   snes_unload_cartridge();
   snes_term();
   uninit_drivers();

   return 0;

error:
   snes_unload_cartridge();
   snes_term();
   uninit_drivers();

   return 1;
}

static void write_file(const char* path, uint8_t* data, size_t size)
{
   FILE *file = fopen(path, "wb");
   if ( file != NULL )
   {
      fprintf(stderr, "SSNES: Saving state \"%s\". Size: %d bytes.\n", path, (int)size);
      snes_serialize(data, size);
      if ( fwrite(data, 1, size, file) != size )
         fprintf(stderr, "SSNES [WARN]: Did not save state properly.");
      fclose(file);
   }
}

static void load_state(const char* path, uint8_t* data, size_t size)
{
   fprintf(stderr, "SSNES: Loading state: \"%s\".\n", path);
   FILE *file = fopen(path, "rb");
   if ( file != NULL )
   {
      //fprintf(stderr, "SSNES: Loading state. Size: %d bytes.\n", (int)size);
      if ( fread(data, 1, size, file) != size )
         fprintf(stderr, "SSNES [WARN]: Did not load state properly.");
      fclose(file);
      snes_unserialize(data, size);
   }
   else
   {
      fprintf(stderr, "SSNES: No state file found. Will create new.\n");
   }
}

static void load_save_file(const char* path, int type)
{
   FILE *file;

   file = fopen(path, "rb");
   if ( !file )
   {
      return;
   }

   size_t size = snes_get_memory_size(type);
   uint8_t *data = snes_get_memory_data(type);

   if (size == 0 || !data)
   {
      fclose(file);
      return;
   }

   int rc = fread(data, 1, size, file);
   if ( rc != size )
   {
      fprintf(stderr, "SSNES [ERROR]: Couldn't load save file.\n");
   }

   fprintf(stderr, "SSNES: Loaded save file: \"%s\"\n", path);

   fclose(file);
}

static void save_file(const char* path, int type)
{
   size_t size = snes_get_memory_size(type);
   uint8_t *data = snes_get_memory_data(type);

   if ( data && size > 0 )
      write_file(path, data, size);
}

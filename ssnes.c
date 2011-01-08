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
#include <libsnes.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "driver.h"
#include "file.h"
#include "hqflt/filters.h"
#include "general.h"
#include "dynamic.h"
#include "record/ffemu.h"
#include <assert.h>
#ifdef HAVE_SRC
#include <samplerate.h>
#endif

struct global g_extern = {
   .video_active = true,
   .audio_active = true,
};

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
      if (g_extern.video_active)
         driver.video->set_nonblock_state(driver.video_data, syncing_state);
      if (g_extern.audio_active)
         driver.audio->set_nonblock_state(driver.audio_data, (g_settings.audio.sync) ? syncing_state : true);
      if (syncing_state)
         audio_chunk_size = AUDIO_CHUNK_SIZE_NONBLOCKING;
      else
         audio_chunk_size = AUDIO_CHUNK_SIZE_BLOCKING;
   }
   old_button_state = new_button_state;
}

#ifdef HAVE_FILTER
static inline void process_frame (uint16_t * restrict out, const uint16_t * restrict in, unsigned width, unsigned height)
{
   int pitch = 1024;
   if ( height == 448 || height == 478 )
      pitch = 512;

   for ( int y = 0; y < height; y++ )
   {
      const uint16_t *src = in + y * pitch;
      uint16_t *dst = out + y * width;

      memcpy(dst, src, width * sizeof(uint16_t));
   }
}
#endif

// libsnes: 0.065
// Format received is 16-bit 0RRRRRGGGGGBBBBB
static void video_frame(const uint16_t *data, unsigned width, unsigned height)
{
   if ( !g_extern.video_active )
      return;

#ifdef HAVE_FFMPEG
   if (g_extern.recording)
   {
      struct ffemu_video_data ffemu_data = {
         .data = data,
         .pitch = height == 448 || height == 478 ? 1024 : 2048,
         .width = width,
         .height = height
      };
      ffemu_push_video(g_extern.rec, &ffemu_data);
   }
#endif

#ifdef HAVE_FILTER
   uint16_t output_filter[width * height * 4 * 4];
   uint16_t output[width * height];
   process_frame(output, data, width, height);

   switch (g_settings.video.filter)
   {
      case FILTER_HQ2X:
         ProcessHQ2x(output, output_filter);
         if ( !driver.video->frame(driver.video_data, output_filter, width << 1, height << 1, width << 2) )
            g_extern.video_active = false;
         break;
      case FILTER_HQ4X:
         ProcessHQ4x(output, output_filter);
         if ( !driver.video->frame(driver.video_data, output_filter, width << 2, height << 2, width << 3) )
            g_extern.video_active = false;
         break;
      case FILTER_GRAYSCALE:
         grayscale_filter(output, width, height);
         if ( !driver.video->frame(driver.video_data, output, width, height, width << 1) )
            g_extern.video_active = false;
         break;
      case FILTER_BLEED:
         bleed_filter(output, width, height);
         if ( !driver.video->frame(driver.video_data, output, width, height, width << 1) )
            g_extern.video_active = false;
         break;
      case FILTER_NTSC:
         ntsc_filter(output_filter, output, width, height);
         if ( !driver.video->frame(driver.video_data, output_filter, SNES_NTSC_OUT_WIDTH(width), height, SNES_NTSC_OUT_WIDTH(width) << 1) )
            g_extern.video_active = false;
         break;
      default:
         if ( !driver.video->frame(driver.video_data, data, width, height, (height == 448 || height == 478) ? 1024 : 2048) )
            g_extern.video_active = false;
   }
#else
   if ( !driver.video->frame(driver.video_data, data, width, height, (height == 448 || height == 478) ? 1024 : 2048) )
      g_extern.video_active = false;
#endif
}

static void audio_sample(uint16_t left, uint16_t right)
{
   if ( !g_extern.audio_active )
      return;

#ifdef HAVE_FFMPEG
   if (g_extern.recording)
   {
      static int16_t static_data[2];
      static_data[0] = left;
      static_data[1] = right;
      struct ffemu_audio_data ffemu_data = {
         .data = static_data,
         .frames = 1
      };
      ffemu_push_audio(g_extern.rec, &ffemu_data);
   }
#endif

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
      src_data.src_ratio = (double)g_settings.audio.out_rate / (double)g_settings.audio.in_rate;

      src_process(g_extern.source, &src_data);

      src_float_to_short_array(outsamples, temp_outsamples, src_data.output_frames_gen * 2);

      if ( driver.audio->write(driver.audio_data, temp_outsamples, src_data.output_frames_gen * 4) < 0 )
      {
         fprintf(stderr, "SSNES [ERROR]: Audio backend failed to write. Will continue without sound.\n");
         g_extern.audio_active = false;
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
   const struct snes_keybind *binds[] = { g_settings.input.binds[0], g_settings.input.binds[1] }; 
   return driver.input->input_state(driver.input_data, binds, port, device, index, id);
}

static void fill_pathname(char *out_path, char *in_path, const char *replace)
{
   char tmp_path[strlen(in_path) + 1];
   strcpy(tmp_path, in_path);
   char *tok = NULL;
   tok = strrchr(tmp_path, '.');
   if (tok != NULL)
      *tok = '\0';
   strcpy(out_path, tmp_path);
   strcat(out_path, replace);
}

#ifdef HAVE_FFMPEG
#define FFMPEG_HELP_QUARK " | -r/--record "
#else
#define FFMPEG_HELP_QUARK
#endif

#ifdef _WIN32
#define SSNES_DEFAULT_CONF_PATH_STR "\n\tDefaults to ssnes.cfg in same directory as ssnes.exe"
#else
#define SSNES_DEFAULT_CONF_PATH_STR " Defaults to $XDG_CONFIG_HOME/ssnes/ssnes.cfg"
#endif

static void print_help(void)
{
   puts("=================================================");
   puts("ssnes: Simple Super Nintendo Emulator (libsnes)");
   puts("=================================================");
   puts("Usage: ssnes [rom file] [-h/--help | -s/--save" FFMPEG_HELP_QUARK "]");
   puts("\t-h/--help: Show this help message");
   puts("\t-s/--save: Path for save file (*.srm). Required when rom is input from stdin");
   puts("\t-c/--config: Path for config file." SSNES_DEFAULT_CONF_PATH_STR);

#ifdef HAVE_FFMPEG
   puts("\t-r/--record: Path to record video file. Settings for video/audio codecs are found in config file.");
#endif
   puts("\t-v/--verbose: Verbose logging");
}

static void parse_input(int argc, char *argv[])
{
   if (argc < 2)
   {
      print_help();
      exit(1);
   }

   struct option opts[] = {
      { "help", 0, NULL, 'h' },
      { "save", 1, NULL, 's' },
#ifdef HAVE_FFMPEG
      { "record", 1, NULL, 'r' },
#endif
      { "verbose", 0, NULL, 'v' },
      { "config", 0, NULL, 'c' },
      { NULL, 0, NULL, 0 }
   };

   int option_index = 0;

#ifdef HAVE_FFMPEG
#define FFMPEG_RECORD_ARG "r:"
#else
#define FFMPEG_RECORD_ARG
#endif

   char optstring[] = "hs:vc:" FFMPEG_RECORD_ARG;
   for(;;)
   {
      int c = getopt_long(argc, argv, optstring, opts, &option_index);

      if (c == -1)
         break;

      switch (c)
      {
         case 'h':
            print_help();
            exit(0);

         case 's':
            strncpy(g_extern.savefile_name_srm, optarg, sizeof(g_extern.savefile_name_srm));
            g_extern.savefile_name_srm[sizeof(g_extern.savefile_name_srm)-1] = '\0';
            break;

         case 'v':
            g_extern.verbose = true;
            break;

         case 'c':
            strncpy(g_extern.config_path, optarg, sizeof(g_extern.config_path) - 1);
            break;

#ifdef HAVE_FFMPEG
         case 'r':
            strncpy(g_extern.record_path, optarg, sizeof(g_extern.record_path) - 1);
            g_extern.recording = true;
            break;
#endif

         case '?':
            print_help();
            exit(1);

         default:
            SSNES_ERR("Error parsing arguments.\n");
            exit(1);
      }
   }

   if (optind < argc)
   {
      char tmp[strlen(argv[optind]) + 1];
      strcpy(tmp, argv[optind]);
      char *dst = strrchr(tmp, '.');
      if (dst)
         *dst = '\0';
      strncpy(g_extern.basename, tmp, sizeof(g_extern.basename) - 1);

      SSNES_LOG("Opening file: \"%s\"\n", argv[optind]);
      g_extern.rom_file = fopen(argv[optind], "rb");
      if (g_extern.rom_file == NULL)
      {
         SSNES_ERR("Could not open file: \"%s\"\n", argv[optind]);
         exit(1);
      }
      if (strlen(g_extern.savefile_name_srm) == 0)
         fill_pathname(g_extern.savefile_name_srm, argv[optind], ".srm");
   }
   else if (strlen(g_extern.savefile_name_srm) == 0)
   {
      SSNES_ERR("Need savefile argument when reading rom from stdin.\n");
      print_help();
      exit(1);
   }
}

int main(int argc, char *argv[])
{
   parse_input(argc, argv);
   parse_config();
   init_dlsym();

   psnes_init();
   if (strlen(g_extern.basename) > 0)
      psnes_set_cartridge_basename(g_extern.basename);

   SSNES_LOG("Version of libsnes API: %u.%u\n", psnes_library_revision_major(), psnes_library_revision_minor());
   void *rom_buf;
   ssize_t rom_len = 0;
   if ((rom_len = read_file(g_extern.rom_file, &rom_buf)) == -1)
   {
      SSNES_ERR("Could not read ROM file.\n");
      exit(1);
   }
   SSNES_LOG("ROM size: %d bytes\n", (int)rom_len);

   if (g_extern.rom_file != NULL)
      fclose(g_extern.rom_file);

   char statefile_name[strlen(g_extern.savefile_name_srm)+strlen(".state")+1];
   char savefile_name_rtc[strlen(g_extern.savefile_name_srm)+strlen(".rtc")+1];

   fill_pathname(statefile_name, argv[1], ".state");
   fill_pathname(savefile_name_rtc, argv[1], ".rtc");

   init_drivers();

   psnes_set_video_refresh(video_frame);
   psnes_set_audio_sample(audio_sample);
   psnes_set_input_poll(input_poll);
   psnes_set_input_state(input_state);

   if (!psnes_load_cartridge_normal(NULL, rom_buf, rom_len))
   {
      SSNES_ERR("ROM file is not valid!\n");
      goto error;
   }

   free(rom_buf);

   unsigned serial_size = psnes_serialize_size();
   uint8_t *serial_data = malloc(serial_size);
   if (serial_data == NULL)
   {
      SSNES_ERR("Failed to allocate memory for states!\n");
      goto error;
   }

   load_save_file(g_extern.savefile_name_srm, SNES_MEMORY_CARTRIDGE_RAM);
   load_save_file(savefile_name_rtc, SNES_MEMORY_CARTRIDGE_RTC);

#ifdef HAVE_FFMPEG
   // Hardcode these options at the moment. Should be specificed in the config file later on.
   if (g_extern.recording)
   {
      struct ffemu_rational ntsc_fps = {60000, 1001};
      struct ffemu_rational pal_fps = {50000, 1001};
      struct ffemu_params params = {
         .vcodec = FFEMU_VIDEO_H264,
         .acodec = FFEMU_AUDIO_VORBIS,
         .rescaler = FFEMU_RESCALER_POINT,
         .out_width = 512,
         .out_height = 448,
         .channels = 2,
         .samplerate = 32040,
         .filename = g_extern.record_path,
         .fps = snes_get_region() == SNES_REGION_NTSC ? ntsc_fps : pal_fps,
         .aspect_ratio = 4.0/3
      };
      SSNES_LOG("Recording with FFmpeg to %s.\n", g_extern.record_path);
      g_extern.rec = ffemu_new(&params);
      if (!g_extern.rec)
      {
         SSNES_ERR("Failed to start FFmpeg recording.\n");
         g_extern.recording = false;
      }
   }
#endif

   for(;;)
   {
      if (driver.input->key_pressed(driver.input_data, g_settings.input.exit_emulator_key) ||
            !driver.video->alive(driver.video_data))
         break;

      if (driver.input->key_pressed(driver.input_data, g_settings.input.save_state_key))
      {
         write_file(statefile_name, serial_data, serial_size);
      }

      else if (driver.input->key_pressed(driver.input_data, g_settings.input.load_state_key))
         load_state(statefile_name, serial_data, serial_size);

      else if (driver.input->key_pressed(driver.input_data, g_settings.input.toggle_fullscreen_key))
      {
         g_settings.video.fullscreen = !g_settings.video.fullscreen;
         uninit_drivers();
         init_drivers();
      }

      psnes_run();
   }

#ifdef HAVE_FFMPEG
   if (g_extern.recording)
   {
      ffemu_finalize(g_extern.rec);
      ffemu_free(g_extern.rec);
   }
#endif

   save_file(g_extern.savefile_name_srm, SNES_MEMORY_CARTRIDGE_RAM);
   save_file(savefile_name_rtc, SNES_MEMORY_CARTRIDGE_RTC);

   psnes_unload_cartridge();
   psnes_term();
   uninit_drivers();
   free(serial_data);
   uninit_dlsym();

   return 0;

error:
   psnes_unload_cartridge();
   psnes_term();
   uninit_drivers();
   uninit_dlsym();

   return 1;
}

